// Copyright (c) 2021 The KROVACOIN Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "wallet/test/pos_test_fixture.h"
#include "blockassembler.h"
#include "chainparams.h"
#include "coins.h"
#include "utiltime.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

// Fund `count` distinct, equally-sized outputs to scriptPubKeyOut in a single
// block (same seed-UTXO trick as TestChainSetup::FundOutput, just batched so
// many stakeable coins can be seeded within the few PoW blocks regtest allows
// before PoS activation -- see the constructor below for why more than one
// or two coins are needed).
static std::vector<CTransactionRef> FundManyOutputs(TestChainSetup& self, const CScript& scriptPubKeyOut, CAmount amountEach, int count)
{
    static unsigned int nSeedCounter = 0;
    std::vector<CMutableTransaction> fundTxs;
    for (int i = 0; i < count; i++) {
        CMutableTransaction seedTx;
        seedTx.vin.resize(1);
        seedTx.vin[0].prevout.SetNull();
        seedTx.vin[0].scriptSig = CScript() << CScriptNum(++nSeedCounter) << OP_0;
        seedTx.vout.resize(1);
        seedTx.vout[0].nValue = amountEach;
        seedTx.vout[0].scriptPubKey = CScript(); // anyone-can-spend seed, redeemed below
        CTransactionRef seedRef = MakeTransactionRef(seedTx);
        WITH_LOCK(cs_main, pcoinsTip->AddCoin(COutPoint(seedRef->GetHash(), 0), Coin(seedRef->vout[0], 1, /*fCoinBase=*/false, /*fCoinStake=*/false), false));

        CMutableTransaction fundTx;
        fundTx.vin.resize(1);
        fundTx.vin[0].prevout = COutPoint(seedRef->GetHash(), 0);
        fundTx.vin[0].scriptSig = CScript() << OP_1;
        fundTx.vout.resize(1);
        fundTx.vout[0].nValue = amountEach;
        fundTx.vout[0].scriptPubKey = scriptPubKeyOut;
        fundTxs.push_back(fundTx);
    }
    CBlock block = self.CreateAndProcessBlock(fundTxs, CScript());
    return std::vector<CTransactionRef>(block.vtx.begin() + 1, block.vtx.end());
}

TestPoSChainSetup::TestPoSChainSetup() : TestChainSetup(0)
{
    initZKSNARKS(); // init zk-snarks lib

    bool fFirstRun;
    pwalletMain = std::make_unique<CWallet>("testWallet", WalletDatabase::CreateMock());
    pwalletMain->LoadWallet(fFirstRun);
    RegisterValidationInterface(pwalletMain.get());

    {
        LOCK(pwalletMain->cs_wallet);
        pwalletMain->SetMinVersion(FEATURE_SAPLING);
        gArgs.ForceSetArg("-keypool", "5");
        pwalletMain->SetupSPKM(true);

        // import the key used to fund/stake the preloaded chain below
        BOOST_CHECK(pwalletMain->AddKeyPubKey(coinbaseKey, coinbaseKey.GetPubKey()));
    }

    // Superblock heights (see consensus/superblock.h, task 7) require a
    // payout transaction that spends the Staking Rewards Pool's premine
    // output -- a placeholder P2PKH address whose private key isn't (and, by
    // design, can't be) known here; it's a real-world "TBD at key ceremony"
    // address, not a test fixture key (see consensus/premine.h). Push the
    // superblock cycle length well past the height this fixture mines to, so
    // none of the many staked blocks below land on a cycle boundary that
    // would demand a payout this fixture has no way to produce. The one
    // superblock height that can't be avoided this way (POS activation
    // height itself, which is always also the first superblock height) is
    // fine as-is: its cycle covers only the pre-PoS PoW bootstrap blocks, so
    // CycleHasStakers() is false and no payout is required there.
    UpdateBudgetCycleBlocks(100000);

    // Ordinary blocks mint 0 KROV (zero-inflation design -- see GetBlockValue()
    // in validation.cpp), so mining PoW blocks to coinbaseKey (the old PIVX
    // assumption this fixture used to rely on) gives the wallet nothing to
    // stake with, and block 1's coinbase is always the real premine
    // regardless of which key is passed in (see blockassembler.cpp). Seed the
    // wallet with real, spendable coin via FundOutput instead, as early as
    // possible so it has enough depth to be stakeable once PoS activates a
    // couple of blocks later.
    //
    // A single coin isn't enough: each successful stake consumes its input
    // and pays the wallet back with a *new* output that itself needs
    // nStakeMinDepth confirmations before it can stake again, so a small
    // rotating set of coins runs dry for a few blocks while everything it
    // has cycled through is still cooling down -- and once nothing is
    // stakeable, the chain can never advance again to let anything mature.
    // Seed a large enough pool up front that later blocks' recycled outputs
    // mature well before the initial pool is exhausted.
    //
    // The pool also needs to be bigger than the number of blocks mined below
    // (250 - posActivation, ~247): downstream tests (created_on_fork_tests)
    // need coins with 120+ confirmations available immediately, but a pool
    // that's fully recycled every block never ages past a few confirmations.
    // Funding more coins than there are blocks to consume them guarantees,
    // by pigeonhole, that some stay untouched and age all the way to 250.
    CScript fundScript = CScript() << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
    FundManyOutputs(*this, fundScript, 1000 * COIN, 300);
    SyncWithValidationInterfaceQueue();

    int posActivation = Params().GetConsensus().vUpgrades[Consensus::UPGRADE_POS].nActivationHeight - 1;
    while (WITH_LOCK(cs_main, return chainActive.Height()) < posActivation) {
        CBlock b = CreateAndProcessBlock({}, coinbaseKey);
        coinbaseTxns.emplace_back(*b.vtx[0]);
    }
    SyncWithValidationInterfaceQueue();

    // From here on, PoW is no longer consensus-valid (PoS has activated).
    // Keep advancing the chain with real staked blocks -- using the coin(s)
    // funded above -- up to height 250, matching this fixture's documented
    // contract ("a preloaded 250-blocks regtest chain running on PoS") so
    // downstream tests inherit a mature chain whose coins have plenty of
    // confirmations to spend/stake with.
    while (WITH_LOCK(cs_main, return chainActive.Height()) < 250) {
        std::vector<CStakeableOutput> availableCoins;
        BOOST_CHECK(pwalletMain->StakeableCoins(&availableCoins));

        // The kernel hash check is probabilistic (weighted by stake value
        // against a target that's already re-derived every block), so a
        // single attempt at a single timestamp isn't guaranteed to find a
        // valid kernel. Real staking software just keeps retrying every
        // second; mock the clock forward and retry here instead of waiting
        // on real wall-clock time.
        std::unique_ptr<CBlockTemplate> pblocktemplate;
        for (int attempt = 0; attempt < 100000 && !pblocktemplate; attempt++) {
            SetMockTime(GetAdjustedTime() + 1);
            pblocktemplate = BlockAssembler(
                    Params(), false).CreateNewBlock(CScript(), pwalletMain.get(), true, &availableCoins, true);
        }
        BOOST_CHECK(pblocktemplate);
        std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>(pblocktemplate->block);
        BOOST_CHECK(ProcessNewBlock(pblock, nullptr));
        SyncWithValidationInterfaceQueue();
    }
}

TestPoSChainSetup::~TestPoSChainSetup()
{
    SyncWithValidationInterfaceQueue();
    UnregisterValidationInterface(pwalletMain.get());
}
