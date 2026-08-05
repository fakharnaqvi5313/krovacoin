// Copyright (c) 2026 The KrovaCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Genesis block validity across all three networks. chainparams.cpp already
// asserts each genesis hash/merkle root against a hardcoded value at
// CChainParams construction time, so a silent hash drift would crash the
// whole process on startup -- these tests independently re-derive the same
// invariants (PoW, merkle root, single unspendable coinbase output, and that
// the premine does NOT live in genesis) as a normal, gracefully-failing part
// of the test suite, and check all three networks in one run.

#include "chainparams.h"
#include "chainparamsbase.h"
#include "consensus/merkle.h"
#include "consensus/premine.h"
#include "pow.h"
#include "test/test_krova.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(genesis_tests, BasicTestingSetup)

static void CheckGenesisForNetwork(const std::string& network)
{
    SelectParams(network);
    const CChainParams& params = Params();
    const CBlock& genesis = params.GenesisBlock();

    BOOST_CHECK_EQUAL(genesis.GetHash().ToString(), params.GetConsensus().hashGenesisBlock.ToString());

    bool mutated = false;
    uint256 computedMerkleRoot = BlockMerkleRoot(genesis, &mutated);
    BOOST_CHECK(!mutated);
    BOOST_CHECK_EQUAL(computedMerkleRoot.ToString(), genesis.hashMerkleRoot.ToString());

    BOOST_CHECK(CheckProofOfWork(genesis.GetHash(), genesis.nBits));

    BOOST_CHECK(genesis.hashPrevBlock.IsNull());

    // Genesis has exactly one transaction (the coinbase), with a single,
    // zero-value, unspendable-in-practice output -- the 72B premine is
    // enforced in block 1's coinbase instead (see consensus/premine.h).
    BOOST_REQUIRE_EQUAL(genesis.vtx.size(), 1);
    BOOST_REQUIRE_EQUAL(genesis.vtx[0]->vout.size(), 1);
    BOOST_CHECK_EQUAL(genesis.vtx[0]->vout[0].nValue, 0);
    BOOST_CHECK(!CheckPremineCoinbase(*genesis.vtx[0]));
}

BOOST_AUTO_TEST_CASE(mainnet_genesis_is_valid)
{
    CheckGenesisForNetwork(CBaseChainParams::MAIN);
}

BOOST_AUTO_TEST_CASE(testnet_genesis_is_valid)
{
    CheckGenesisForNetwork(CBaseChainParams::TESTNET);
}

BOOST_AUTO_TEST_CASE(regtest_genesis_is_valid)
{
    CheckGenesisForNetwork(CBaseChainParams::REGTEST);
}

BOOST_AUTO_TEST_CASE(networks_have_distinct_genesis_blocks)
{
    SelectParams(CBaseChainParams::MAIN);
    uint256 mainHash = Params().GenesisBlock().GetHash();
    SelectParams(CBaseChainParams::TESTNET);
    uint256 testHash = Params().GenesisBlock().GetHash();
    SelectParams(CBaseChainParams::REGTEST);
    uint256 regHash = Params().GenesisBlock().GetHash();

    BOOST_CHECK(mainHash != testHash);
    BOOST_CHECK(mainHash != regHash);
    BOOST_CHECK(testHash != regHash);
}

BOOST_AUTO_TEST_SUITE_END()
