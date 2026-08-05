// Copyright (c) 2011-2014 The Bitcoin Core developers
// Copyright (c) 2016-2021 The KROVACOIN Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "wallet/test/wallet_test_fixture.h"

#include "blockassembler.h"
#include "checkpoints.h"
#include "coins.h"
#include "consensus/merkle.h"
#include "miner.h"
#include "pow.h"
#include "pubkey.h"
#include "uint256.h"
#include "util/blockstatecatcher.h"
#include "util/system.h"
#include "validation.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>


// future: this should be MAINNET.
BOOST_FIXTURE_TEST_SUITE(miner_tests, WalletRegTestingSetup)

// Number of PoW bootstrap blocks to load before the main test body runs.
static const unsigned int NUM_BOOTSTRAP_BLOCKS = 100;

// Test suite for ancestor feerate transaction selection.
// Implemented as an additional function, rather than a separate test case,
// to allow reusing the blockchain created in CreateNewBlock_validity.
void TestPackageSelection(const CChainParams& chainparams, CScript scriptPubKey, std::vector<CTransactionRef>& txFirst)
{
    // Test the ancestor feerate transaction selection.
    TestMemPoolEntryHelper entry;

    // Test that a medium fee transaction will be selected after a higher fee
    // rate package with a low fee rate parent.
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].scriptSig = CScript() << OP_1;
    tx.vin[0].prevout.hash = txFirst[0]->GetHash();
    tx.vin[0].prevout.n = 0;
    tx.vout.resize(1);
    tx.vout[0].nValue = 5000000000LL - 1000;
    // This tx has a low fee: 1000 satoshis
    uint256 hashParentTx = tx.GetHash(); // save this txid for later use
    mempool.addUnchecked(hashParentTx, entry.Fee(1000).Time(GetTime()).SpendsCoinbaseOrCoinstake(true).FromTx(tx));

    // This tx has a medium fee: 10000 satoshis
    tx.vin[0].prevout.hash = txFirst[1]->GetHash();
    tx.vout[0].nValue = 5000000000LL - 10000;
    uint256 hashMediumFeeTx = tx.GetHash();
    mempool.addUnchecked(hashMediumFeeTx, entry.Fee(10000).Time(GetTime()).SpendsCoinbaseOrCoinstake(true).FromTx(tx));

    // This tx has a high fee, but depends on the first transaction
    tx.vin[0].prevout.hash = hashParentTx;
    tx.vout[0].nValue = 5000000000LL - 1000 - 50000; // 50k satoshi fee
    uint256 hashHighFeeTx = tx.GetHash();
    mempool.addUnchecked(hashHighFeeTx, entry.Fee(50000).Time(GetTime()).SpendsCoinbaseOrCoinstake(false).FromTx(tx));

    std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(chainparams, DEFAULT_PRINTPRIORITY).CreateNewBlock(scriptPubKey);

    BOOST_CHECK(pblocktemplate->block.vtx[1]->GetHash() == hashParentTx);
    BOOST_CHECK(pblocktemplate->block.vtx[2]->GetHash() == hashHighFeeTx);
    BOOST_CHECK(pblocktemplate->block.vtx[3]->GetHash() == hashMediumFeeTx);

    // Test that a package below the min relay fee doesn't get included
    tx.vin[0].prevout.hash = hashHighFeeTx;
    tx.vout[0].nValue = 5000000000LL - 1000 - 50000; // 0 fee
    uint256 hashFreeTx = tx.GetHash();
    mempool.addUnchecked(hashFreeTx, entry.Fee(0).FromTx(tx));
    size_t freeTxSize = ::GetSerializeSize(tx, PROTOCOL_VERSION);

    // Calculate a fee on child transaction that will put the package just
    // below the min relay fee (assuming 1 child tx of the same size).
    CAmount feeToUse = minRelayTxFee.GetFee(2*freeTxSize) - 1;

    tx.vin[0].prevout.hash = hashFreeTx;
    tx.vout[0].nValue = 5000000000LL - 1000 - 50000 - feeToUse;
    uint256 hashLowFeeTx = tx.GetHash();
    mempool.addUnchecked(hashLowFeeTx, entry.Fee(feeToUse).FromTx(tx));
    pblocktemplate = BlockAssembler(chainparams, DEFAULT_PRINTPRIORITY).CreateNewBlock(scriptPubKey);
    // Verify that the free tx and the low fee tx didn't get selected
    for (size_t i=0; i<pblocktemplate->block.vtx.size(); ++i) {
        BOOST_CHECK(pblocktemplate->block.vtx[i]->GetHash() != hashFreeTx);
        BOOST_CHECK(pblocktemplate->block.vtx[i]->GetHash() != hashLowFeeTx);
    }

    // Test that packages above the min relay fee do get included, even if one
    // of the transactions is below the min relay fee
    // Remove the low fee transaction and replace with a higher fee transaction
    mempool.removeRecursive(tx);
    tx.vout[0].nValue -= 2; // Now we should be just over the min relay fee
    hashLowFeeTx = tx.GetHash();
    mempool.addUnchecked(hashLowFeeTx, entry.Fee(feeToUse+2).FromTx(tx));
    pblocktemplate = BlockAssembler(chainparams, DEFAULT_PRINTPRIORITY).CreateNewBlock(scriptPubKey);
    BOOST_CHECK(pblocktemplate->block.vtx[4]->GetHash() == hashFreeTx);
    BOOST_CHECK(pblocktemplate->block.vtx[5]->GetHash() == hashLowFeeTx);

    // Test that transaction selection properly updates ancestor fee
    // calculations as ancestor transactions get included in a block.
    // Add a 0-fee transaction that has 2 outputs.
    tx.vin[0].prevout.hash = txFirst[2]->GetHash();
    tx.vout.resize(2);
    tx.vout[0].nValue = 5000000000LL - 100000000;
    tx.vout[1].nValue = 100000000; // 1BTC output
    uint256 hashFreeTx2 = tx.GetHash();
    mempool.addUnchecked(hashFreeTx2, entry.Fee(0).SpendsCoinbaseOrCoinstake(true).FromTx(tx));

    // This tx can't be mined by itself
    tx.vin[0].prevout.hash = hashFreeTx2;
    tx.vout.resize(1);
    feeToUse = minRelayTxFee.GetFee(freeTxSize);
    tx.vout[0].nValue = 5000000000LL - 100000000 - feeToUse;
    uint256 hashLowFeeTx2 = tx.GetHash();
    mempool.addUnchecked(hashLowFeeTx2, entry.Fee(feeToUse).SpendsCoinbaseOrCoinstake(false).FromTx(tx));
    pblocktemplate = BlockAssembler(chainparams, DEFAULT_PRINTPRIORITY).CreateNewBlock(scriptPubKey);

    // Verify that this tx isn't selected.
    for (size_t i=0; i<pblocktemplate->block.vtx.size(); ++i) {
        BOOST_CHECK(pblocktemplate->block.vtx[i]->GetHash() != hashFreeTx2);
        BOOST_CHECK(pblocktemplate->block.vtx[i]->GetHash() != hashLowFeeTx2);
    }

    // This tx will be mineable, and should cause hashLowFeeTx2 to be selected
    // as well.
    tx.vin[0].prevout.n = 1;
    tx.vout[0].nValue = 100000000 - 10000; // 10k satoshi fee
    mempool.addUnchecked(tx.GetHash(), entry.Fee(10000).FromTx(tx));
    pblocktemplate = BlockAssembler(chainparams, DEFAULT_PRINTPRIORITY).CreateNewBlock(scriptPubKey);
    BOOST_CHECK(pblocktemplate->block.vtx[8]->GetHash() == hashLowFeeTx2);
}

// NOTE: These tests rely on CreateNewBlock doing its own self-validation!
BOOST_AUTO_TEST_CASE(CreateNewBlock_validity)
{
    // Note that by default, these tests run with size accounting enabled.
    const CChainParams& chainparams = Params();
    CScript scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ParseHex("8d5b4f83212214d6ef693e02e6d71969fddad976") << OP_EQUALVERIFY << OP_CHECKSIG;

    std::unique_ptr<CBlockTemplate> pblocktemplate;
    CMutableTransaction tx,tx2;
    CScript script;
    uint256 hash;
    TestMemPoolEntryHelper entry;
    entry.nFee = 11;
    entry.nHeight = 11;

    // This test builds a NUM_BOOTSTRAP_BLOCKS-long pure-PoW chain to exercise
    // mempool/mining selection (sigops limits, size limits, package selection).
    // On regtest, UPGRADE_POS activates at height 4 by default (shortened for
    // fast staking/superblock tests elsewhere) -- delay it well past the
    // bootstrap range, same as TestChainSetup does, so this test isn't forced
    // into producing proof-of-stake blocks it was never designed to build.
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_POS, NUM_BOOTSTRAP_BLOCKS + 100);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_V3_4, NUM_BOOTSTRAP_BLOCKS + 101);

    Checkpoints::fEnabled = false;

    // Simple block creation, nothing special yet:
    BOOST_CHECK(pblocktemplate = BlockAssembler(Params(), DEFAULT_PRINTPRIORITY).CreateNewBlock(scriptPubKey, &m_wallet, false));
    // Set genesis block
    pblocktemplate->block.hashPrevBlock = chainparams.GetConsensus().hashGenesisBlock;

    // Advance the chain 100 blocks so height/time-dependent logic elsewhere
    // in this test (maturity, median-time-past locktimes, etc.) has a
    // realistic tip to work against.
    //
    // These blocks are NOT used as this test's source of spendable funds:
    // KrovaCoin mints no ordinary per-block reward (task 6 -- GetBlockValue()
    // is 0 past height 1), and height 1's own coinbase is the real 72B
    // premine (task 5) with genuine cryptographically-locked P2PKH/CLTV
    // outputs, not the placeholder-spendable coinbase upstream tests expect.
    // Instead, txFirst below is populated with synthetic, test-controlled
    // "coinbases" injected directly into the UTXO set further down.
    std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>(pblocktemplate->block); // pointer for convenience
    for (unsigned int i = 0; i < NUM_BOOTSTRAP_BLOCKS; ++i) {
        CBlockIndex* pindexPrev = WITH_LOCK(cs_main, return chainActive.Tip());
        assert(pindexPrev);
        pblock->nTime = pindexPrev->GetMedianTimePast() + 60;
        pblock->vtx.clear(); // Update coinbase input height manually
        CreateCoinbaseTx(pblock.get(), CScript(), pindexPrev);
        pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
        // Mine the nonce live against the network's actual PoW target, rather
        // than relying on a hardcoded table -- the premine/genesis content
        // (and therefore every block hash in this chain) is KrovaCoin-specific
        // and won't match nonces mined for any other chain's block 1.
        pblock->nNonce = 0;
        while (!CheckProofOfWork(pblock->GetHash(), pblock->nBits)) {
            ++pblock->nNonce;
        }
        BlockStateCatcher stateCatcher(pblock->GetHash());
        stateCatcher.registerEvent();
        BOOST_CHECK(ProcessNewBlock(pblock, nullptr));
        SyncWithValidationInterfaceQueue();
        BOOST_CHECK(stateCatcher.found && stateCatcher.state.IsValid());
        pblock->hashPrevBlock = pblock->GetHash();
    }

    // Seed 4 synthetic, already-mature "coinbase" outputs directly into the
    // UTXO set for this test's transactions to spend -- an empty scriptPubKey
    // (anyone-can-spend, matching the placeholder OP_1-style scriptSigs used
    // throughout this file) and a real value, since no real block in this
    // chain can offer both at once anymore.
    std::vector<CTransactionRef> txFirst;
    for (unsigned int i = 0; i < 4; ++i) {
        CMutableTransaction fakeCoinbase;
        fakeCoinbase.vin.resize(1);
        fakeCoinbase.vin[0].prevout.SetNull();
        fakeCoinbase.vin[0].scriptSig = CScript() << CScriptNum(i) << OP_0;
        fakeCoinbase.vout.resize(1);
        fakeCoinbase.vout[0].nValue = 5000000000LL; // 50 COIN
        fakeCoinbase.vout[0].scriptPubKey = CScript();
        CTransactionRef txRef = MakeTransactionRef(fakeCoinbase);
        WITH_LOCK(cs_main, pcoinsTip->AddCoin(COutPoint(txRef->GetHash(), 0), Coin(txRef->vout[0], 1, /*fCoinBase=*/true, /*fCoinStake=*/false), false));
        txFirst.emplace_back(txRef);
    }

    // Just to make sure we can still make simple blocks
    BOOST_CHECK(pblocktemplate = BlockAssembler(Params(), DEFAULT_PRINTPRIORITY).CreateNewBlock(scriptPubKey, &m_wallet, false));

    // block sigops > limit: 2000 CHECKMULTISIG + 1
    tx.vin.resize(1);
    // NOTE: OP_NOP is used to force 20 SigOps for the CHECKMULTISIG
    tx.vin[0].scriptSig = CScript() << OP_0 << OP_0 << OP_0 << OP_NOP << OP_CHECKMULTISIG << OP_1;
    tx.vin[0].prevout.hash = txFirst[0]->GetHash();
    tx.vin[0].prevout.n = 0;
    tx.vout.resize(1);
    tx.vout[0].nValue = 5000000000LL;
    for (unsigned int i = 0; i < 2001; ++i) {
        tx.vout[0].nValue -= 1000000;
        hash = tx.GetHash();
        bool spendsCoinbase = (i == 0) ? true : false; // only first tx spends coinbase
        // If we don't set the # of sig ops in the CTxMemPoolEntry, template creation fails
        mempool.addUnchecked(hash, entry.Fee(1000000).Time(GetTime()).SpendsCoinbaseOrCoinstake(spendsCoinbase).FromTx(tx));
        tx.vin[0].prevout.hash = hash;
    }
    BOOST_CHECK_EXCEPTION(pblocktemplate = BlockAssembler(Params(), DEFAULT_PRINTPRIORITY).CreateNewBlock(scriptPubKey, &m_wallet, false), std::runtime_error, HasReason("bad-blk-sigops"));
    mempool.clear();

    tx.vin[0].prevout.hash = txFirst[0]->GetHash();
    tx.vout[0].nValue = 5000000000LL;
    for (unsigned int i = 0; i < 1001; ++i) {
        tx.vout[0].nValue -= 1000000;
        hash = tx.GetHash();
        bool spendsCoinbase = (i == 0) ? true : false; // only first tx spends coinbase
        // If we do set the # of sig ops in the CTxMemPoolEntry, template creation passes
        mempool.addUnchecked(hash, entry.Fee(1000000).Time(GetTime()).SpendsCoinbaseOrCoinstake(spendsCoinbase).SigOps(20).FromTx(tx));
        tx.vin[0].prevout.hash = hash;
    }
    BOOST_CHECK(pblocktemplate = BlockAssembler(Params(), DEFAULT_PRINTPRIORITY).CreateNewBlock(scriptPubKey, &m_wallet, false));
    mempool.clear();

    // block size > limit
    tx.vin[0].scriptSig = CScript();
    // 18 * (520char + DROP) + OP_1 = 9433 bytes
    std::vector<unsigned char> vchData(520);
    for (unsigned int i = 0; i < 18; ++i)
        tx.vin[0].scriptSig << vchData << OP_DROP;
    tx.vin[0].scriptSig << OP_1;
    tx.vin[0].prevout.hash = txFirst[0]->GetHash();
    tx.vin[0].prevout.n = 0;
    tx.vout[0].nValue = 5000000000LL;
    for (unsigned int i = 0; i < 215; ++i) {
        tx.vout[0].nValue -= 10000000;
        hash = tx.GetHash();
        bool spendsCoinbase = i == 0; // only first tx spends coinbase
        mempool.addUnchecked(hash, entry.Fee(1000000).Time(GetTime()).SpendsCoinbaseOrCoinstake(spendsCoinbase).FromTx(tx));
        tx.vin[0].prevout.hash = hash;
    }
    BOOST_CHECK(pblocktemplate = BlockAssembler(Params(), DEFAULT_PRINTPRIORITY).CreateNewBlock(scriptPubKey, &m_wallet, false));
    mempool.clear();

    // orphan in mempool, template creation fails
    hash = tx.GetHash();
    mempool.addUnchecked(hash, entry.Fee(1000000).Time(GetTime()).FromTx(tx));
    BOOST_CHECK_EXCEPTION(pblocktemplate = BlockAssembler(Params(), DEFAULT_PRINTPRIORITY).CreateNewBlock(scriptPubKey, &m_wallet, false), std::runtime_error, HasReason("bad-txns-inputs-missingorspent"));
    mempool.clear();

    // child with higher feerate than parent
    tx.vin[0].scriptSig = CScript() << OP_1;
    tx.vin[0].prevout.hash = txFirst[1]->GetHash();
    tx.vout[0].nValue = 4900000000LL;
    hash = tx.GetHash();
    mempool.addUnchecked(hash, entry.Fee(100000000LL).Time(GetTime()).SpendsCoinbaseOrCoinstake(true).FromTx(tx));
    tx.vin[0].prevout.hash = hash;
    tx.vin.resize(2);
    tx.vin[1].scriptSig = CScript() << OP_1;
    tx.vin[1].prevout.hash = txFirst[0]->GetHash();
    tx.vin[1].prevout.n = 0;
    tx.vout[0].nValue = 5900000000LL;
    hash = tx.GetHash();
    mempool.addUnchecked(hash, entry.Fee(400000000LL).Time(GetTime()).SpendsCoinbaseOrCoinstake(true).FromTx(tx));
    BOOST_CHECK(pblocktemplate = BlockAssembler(Params(), DEFAULT_PRINTPRIORITY).CreateNewBlock(scriptPubKey, &m_wallet, false));
    mempool.clear();

    // coinbase in mempool, template creation fails
    tx.vin.resize(1);
    tx.vin[0].prevout.SetNull();
    tx.vin[0].scriptSig = CScript() << OP_0 << OP_1;
    tx.vout[0].nValue = 0;
    hash = tx.GetHash();
    // give it a fee so it'll get mined
    mempool.addUnchecked(hash, entry.Fee(100000).Time(GetTime()).SpendsCoinbaseOrCoinstake(false).FromTx(tx));
    BOOST_CHECK_EXCEPTION(pblocktemplate = BlockAssembler(Params(), DEFAULT_PRINTPRIORITY).CreateNewBlock(scriptPubKey, &m_wallet, false), std::runtime_error, HasReason("bad-cb-multiple"));
    mempool.clear();

    // invalid (pre-p2sh) txn in mempool, template creation fails
    tx.vin[0].prevout.hash = txFirst[0]->GetHash();
    tx.vin[0].prevout.n = 0;
    tx.vin[0].scriptSig = CScript() << OP_1;
    tx.vout[0].nValue = 4900000000LL;
    script = CScript() << OP_0;
    tx.vout[0].scriptPubKey = GetScriptForDestination(CScriptID(script));
    hash = tx.GetHash();
    mempool.addUnchecked(hash, entry.Fee(10000000L).Time(GetTime()).SpendsCoinbaseOrCoinstake(true).FromTx(tx));
    tx.vin[0].prevout.hash = hash;
    tx.vin[0].scriptSig = CScript() << std::vector<unsigned char>(script.begin(), script.end());
    tx.vout[0].nValue -= 1000000;
    hash = tx.GetHash();
    mempool.addUnchecked(hash, entry.Fee(1000000).Time(GetTime()).SpendsCoinbaseOrCoinstake(false).FromTx(tx));
     // Should throw block-validation-failed
    BOOST_CHECK_EXCEPTION(pblocktemplate = BlockAssembler(Params(), DEFAULT_PRINTPRIORITY).CreateNewBlock(scriptPubKey, &m_wallet, false), std::runtime_error, HasReason("block-validation-failed"));
    mempool.clear();

    // double spend txn pair in mempool, template creation fails
    tx.vin[0].prevout.hash = txFirst[0]->GetHash();
    tx.vin[0].scriptSig = CScript() << OP_1;
    tx.vout[0].nValue = 4900000000LL;
    tx.vout[0].scriptPubKey = CScript() << OP_1;
    hash = tx.GetHash();
    mempool.addUnchecked(hash, entry.Fee(100000000L).Time(GetTime()).SpendsCoinbaseOrCoinstake(true).FromTx(tx));
    tx.vout[0].scriptPubKey = CScript() << OP_2;
    hash = tx.GetHash();
    mempool.addUnchecked(hash, entry.Fee(100000000L).Time(GetTime()).SpendsCoinbaseOrCoinstake(true).FromTx(tx));
    BOOST_CHECK_EXCEPTION(pblocktemplate = BlockAssembler(Params(), DEFAULT_PRINTPRIORITY).CreateNewBlock(scriptPubKey, &m_wallet, false), std::runtime_error, HasReason("bad-txns-inputs-missingorspent"));
    mempool.clear();

    // non-final txs in mempool
    SetMockTime(WITH_LOCK(cs_main, return chainActive.Tip()->GetMedianTimePast()+1));

    // height locked
    tx.vin[0].prevout.hash = txFirst[0]->GetHash();
    tx.vin[0].scriptSig = CScript() << OP_1;
    tx.vin[0].nSequence = 0;
    tx.vout[0].nValue = 4900000000LL;
    tx.vout[0].scriptPubKey = CScript() << OP_1;
    tx.nLockTime = WITH_LOCK(cs_main, return chainActive.Tip()->nHeight+1);
    hash = tx.GetHash();
    mempool.addUnchecked(hash, entry.Fee(100000000L).Time(GetTime()).SpendsCoinbaseOrCoinstake(true).FromTx(tx));
    { LOCK(cs_main); BOOST_CHECK(!CheckFinalTx(MakeTransactionRef(tx), LOCKTIME_MEDIAN_TIME_PAST)); }

    // time locked
    tx2.vin.resize(1);
    tx2.vin[0].prevout.hash = txFirst[1]->GetHash();
    tx2.vin[0].prevout.n = 0;
    tx2.vin[0].scriptSig = CScript() << OP_1;
    tx2.vin[0].nSequence = 0;
    tx2.vout.resize(1);
    tx2.vout[0].nValue = 4900000000LL;
    tx2.vout[0].scriptPubKey = CScript() << OP_1;
    tx2.nLockTime = WITH_LOCK(cs_main, return chainActive.Tip()->GetMedianTimePast()+1);
    hash = tx2.GetHash();
    mempool.addUnchecked(hash, entry.Fee(100000000L).Time(GetTime()).SpendsCoinbaseOrCoinstake(true).FromTx(tx2));
    { LOCK(cs_main); BOOST_CHECK(!CheckFinalTx(MakeTransactionRef(tx2), LOCKTIME_MEDIAN_TIME_PAST)); }

    BOOST_CHECK(pblocktemplate = BlockAssembler(Params(), DEFAULT_PRINTPRIORITY).CreateNewBlock(scriptPubKey, &m_wallet, false));

    // Neither tx should have make it into the template.
    BOOST_CHECK_EQUAL(pblocktemplate->block.vtx.size(), 1);

    {
        LOCK(cs_main);
        // However if we advance height and time by one, both will.
        chainActive.Tip()->nHeight++;
        SetMockTime(chainActive.Tip()->GetMedianTimePast() + 2);
    }

    // FIXME: we should *actually* create a new block so the following test
    //        works; CheckFinalTx() isn't fooled by monkey-patching nHeight.
    //BOOST_CHECK(CheckFinalTx(tx));
    //BOOST_CHECK(CheckFinalTx(tx2));

    BOOST_CHECK(pblocktemplate = BlockAssembler(Params(), DEFAULT_PRINTPRIORITY).CreateNewBlock(scriptPubKey, &m_wallet, false));
    BOOST_CHECK_EQUAL(pblocktemplate->block.vtx.size(), 3);

    WITH_LOCK(cs_main, chainActive.Tip()->nHeight--);
    SetMockTime(0);
    mempool.clear();

    TestPackageSelection(chainparams, scriptPubKey, txFirst);

    Checkpoints::fEnabled = true;
}

BOOST_AUTO_TEST_SUITE_END()
