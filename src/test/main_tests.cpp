// Copyright (c) 2014 The Bitcoin Core developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2021 The KROVACOIN Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_krova.h"

#include "blocksignature.h"
#include "net.h"
#include "primitives/transaction.h"
#include "script/sign.h"
#include "validation.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(main_tests, TestingSetup)

enum BlockSignatureType{
    P2PK,
    P2PKH,
    P2CS
};

CScript GetScriptForType(CPubKey pubKey, BlockSignatureType type)
{
    switch(type){
        case P2PK:
            return CScript() << pubKey << OP_CHECKSIG;
        default:
            return GetScriptForDestination(pubKey.GetID());
    }
}

std::vector<unsigned char> CreateDummyScriptSigWithKey(CPubKey pubKey)
{
    std::vector<unsigned char> vchSig;
    const CScript scriptCode;
    DummySignatureCreator(nullptr).CreateSig(vchSig, pubKey.GetID(), scriptCode, SIGVERSION_BASE);
    return vchSig;
}

CScript GetDummyScriptSigByType(CPubKey pubKey, bool isP2PK)
{
    CScript script = CScript() << CreateDummyScriptSigWithKey(pubKey);
    if (!isP2PK)
        script << ToByteVector(pubKey);
    return script;
}

CBlock CreateDummyBlockWithSignature(CKey stakingKey, BlockSignatureType type, bool useInputP2PK)
{
    CMutableTransaction txCoinStake;
    // Dummy input
    CTxIn input(uint256(), 0);
    // P2PKH input
    input.scriptSig = GetDummyScriptSigByType(stakingKey.GetPubKey(), useInputP2PK);
    // Add dummy input
    txCoinStake.vin.emplace_back(input);
    // Empty first output
    txCoinStake.vout.emplace_back(0, CScript());
    // P2PK staking output
    CScript scriptPubKey = GetScriptForType(stakingKey.GetPubKey(), type);
    txCoinStake.vout.emplace_back(0, scriptPubKey);

    // Now the block.
    CBlock block;
    block.vtx.emplace_back(std::make_shared<const CTransaction>(CTransaction())); // dummy first tx
    block.vtx.emplace_back(std::make_shared<const CTransaction>(txCoinStake));
    SignBlockWithKey(block, stakingKey);

    return block;
}

bool TestBlockSignature(const CBlock& block)
{
    return CheckBlockSignature(block);
}

BOOST_AUTO_TEST_CASE(block_signature_test)
{
    for (int i = 0; i < 20; ++i) {
        CKey stakingKey;
        stakingKey.MakeNewKey(true);
        bool useInputP2PK = i % 2 == 0;

        // Test P2PK block signature
        CBlock block = CreateDummyBlockWithSignature(stakingKey, BlockSignatureType::P2PK, useInputP2PK);
        BOOST_CHECK(TestBlockSignature(block));

        // Test P2PKH block signature
        block = CreateDummyBlockWithSignature(stakingKey, BlockSignatureType::P2PKH, useInputP2PK);
        if (useInputP2PK) {
            // If it's using a P2PK scriptsig as input and a P2PKH output
            // The block doesn't contain the public key to verify the sig anywhere.
            // Must fail.
            BOOST_CHECK(!TestBlockSignature(block));
        } else {
            BOOST_CHECK(TestBlockSignature(block));
        }
    }
}

BOOST_AUTO_TEST_CASE(subsidy_limit_test)
{
    // Zero protocol inflation (see GetBlockValue() in validation.cpp): the entire
    // 72B supply is minted exactly once, in the block-1 premine. Every other
    // height -- including genesis and the PoW bootstrap window before PoS
    // activation -- mints nothing. This replaces the old PIVX declining-subsidy
    // curve this test used to check, which no longer describes this chain at all.
    const CAmount nExpectedPremine = Params().GetConsensus().nMaxMoneyOut;
    BOOST_CHECK(GetBlockValue(1) == nExpectedPremine);
    BOOST_CHECK(Params().GetConsensus().MoneyRange(GetBlockValue(1)));

    BOOST_CHECK(GetBlockValue(0) == 0);
    for (int nHeight = 2; nHeight < 300000; nHeight += 997) {
        BOOST_CHECK(GetBlockValue(nHeight) == 0);
    }

    // Total ever minted, summed over a full 20-year-plus span of blocks at the
    // 60s target block time (a little over 10.5M blocks), must equal exactly
    // the premine and never exceed nMaxMoneyOut -- the same invariant
    // AssertPremineTotal()-style consensus checks enforce at chain-init time.
    CAmount nSum = 0;
    for (int nHeight = 0; nHeight <= 10512000; nHeight += 1) {
        nSum += GetBlockValue(nHeight);
        BOOST_CHECK(nSum <= nExpectedPremine);
    }
    BOOST_CHECK(nSum == nExpectedPremine);
}

bool ReturnFalse() { return false; }
bool ReturnTrue() { return true; }

BOOST_AUTO_TEST_CASE(test_combiner_all)
{
    boost::signals2::signal<bool(), CombinerAll> Test;
    BOOST_CHECK(Test());
    Test.connect(&ReturnFalse);
    BOOST_CHECK(!Test());
    Test.connect(&ReturnTrue);
    BOOST_CHECK(!Test());
    Test.disconnect(&ReturnFalse);
    BOOST_CHECK(Test());
    Test.disconnect(&ReturnTrue);
    BOOST_CHECK(Test());
}

BOOST_AUTO_TEST_SUITE_END()
