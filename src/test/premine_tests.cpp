// Copyright (c) 2026 The KrovaCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/premine.h"

#include "amount.h"
#include "consensus/tx_verify.h"
#include "consensus/validation.h"
#include "consensus/vesting.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "test/test_krova.h"
#include "utilstrencodings.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(premine_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(premine_total_is_exactly_72_billion)
{
    BOOST_CHECK_EQUAL(GetPremineTotal(), 72000000000LL * COIN);
}

BOOST_AUTO_TEST_CASE(premine_allocation_table_percentages)
{
    // 35 + 20 + 10 + 15 + 15 + 5 = 100%, each against the 72B total.
    BOOST_CHECK_EQUAL(GetStakingRewardsPoolAllocation().amount, 25200000000LL * COIN);
    BOOST_CHECK_EQUAL(GetTeamFoundersAllocation().amount, 7200000000LL * COIN);

    CAmount total = 0;
    for (size_t i = 0; i < NUM_PREMINE_ALLOCATIONS; i++) {
        total += vPremineAllocations[i].amount;
    }
    BOOST_CHECK_EQUAL(total, 72000000000LL * COIN);
}

BOOST_AUTO_TEST_CASE(premine_outputs_structure)
{
    std::vector<CTxOut> outputs = GetPremineOutputs();
    // 5 plain allocations (all but TeamFounders) + 36 vesting tranches.
    BOOST_CHECK_EQUAL(outputs.size(), NUM_PREMINE_ALLOCATIONS - 1 + 36);

    CAmount total = 0;
    for (const CTxOut& out : outputs) {
        total += out.nValue;
    }
    BOOST_CHECK_EQUAL(total, GetPremineTotal());

    // The vesting tranches are appended last, in the same order GetTeamVestingOutputs() returns.
    std::vector<CTxOut> vesting = GetTeamVestingOutputs();
    BOOST_REQUIRE_EQUAL(vesting.size(), 36);
    for (size_t i = 0; i < vesting.size(); i++) {
        const CTxOut& out = outputs[outputs.size() - 36 + i];
        BOOST_CHECK_EQUAL(out.nValue, vesting[i].nValue);
        BOOST_CHECK(out.scriptPubKey == vesting[i].scriptPubKey);
    }

    // None of the 5 plain outputs pay the (unused, vesting-only) TeamFounders scriptPubKeyHex directly.
    std::vector<unsigned char> teamScriptBytes = ParseHex(GetTeamFoundersAllocation().scriptPubKeyHex);
    CScript teamScript(teamScriptBytes.begin(), teamScriptBytes.end());
    for (size_t i = 0; i + 36 < outputs.size(); i++) {
        BOOST_CHECK(outputs[i].scriptPubKey != teamScript);
    }
}

BOOST_AUTO_TEST_CASE(premine_outputs_deterministic)
{
    // Calling it twice must produce byte-identical results -- this is consensus
    // data, not something that can vary run to run.
    std::vector<CTxOut> a = GetPremineOutputs();
    std::vector<CTxOut> b = GetPremineOutputs();
    BOOST_REQUIRE_EQUAL(a.size(), b.size());
    for (size_t i = 0; i < a.size(); i++) {
        BOOST_CHECK_EQUAL(a[i].nValue, b[i].nValue);
        BOOST_CHECK(a[i].scriptPubKey == b[i].scriptPubKey);
    }
}

BOOST_AUTO_TEST_CASE(check_premine_coinbase_accepts_canonical)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    for (const CTxOut& out : GetPremineOutputs()) {
        mtx.vout.push_back(out);
    }
    BOOST_CHECK(CheckPremineCoinbase(CTransaction(mtx)));
}

BOOST_AUTO_TEST_CASE(check_premine_coinbase_rejects_tampering)
{
    const std::vector<CTxOut> canonical = GetPremineOutputs();

    // Wrong amount on one output.
    {
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        for (const CTxOut& out : canonical) mtx.vout.push_back(out);
        mtx.vout[0].nValue += 1;
        BOOST_CHECK(!CheckPremineCoinbase(CTransaction(mtx)));
    }
    // Missing an output.
    {
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        for (size_t i = 0; i + 1 < canonical.size(); i++) mtx.vout.push_back(canonical[i]);
        BOOST_CHECK(!CheckPremineCoinbase(CTransaction(mtx)));
    }
    // Extra output appended.
    {
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        for (const CTxOut& out : canonical) mtx.vout.push_back(out);
        mtx.vout.push_back(canonical.back());
        BOOST_CHECK(!CheckPremineCoinbase(CTransaction(mtx)));
    }
    // Outputs reordered.
    {
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        for (auto it = canonical.rbegin(); it != canonical.rend(); ++it) mtx.vout.push_back(*it);
        BOOST_CHECK(!CheckPremineCoinbase(CTransaction(mtx)));
    }
    // Wrong scriptPubKey on one output.
    {
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        for (const CTxOut& out : canonical) mtx.vout.push_back(out);
        mtx.vout[1].scriptPubKey = CScript() << OP_TRUE;
        BOOST_CHECK(!CheckPremineCoinbase(CTransaction(mtx)));
    }
    // Completely empty transaction.
    {
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        BOOST_CHECK(!CheckPremineCoinbase(CTransaction(mtx)));
    }
}

BOOST_AUTO_TEST_CASE(premine_coinbase_passes_full_consensus_check)
{
    // Diagnostic: build the exact block-1 coinbase shape CreateCoinbaseTx()
    // (blockassembler.cpp) produces, and run it through the real consensus
    // CheckTransaction() -- not just the structural CheckPremineCoinbase()
    // comparison above.
    CMutableTransaction txPremine;
    txPremine.vin.emplace_back();
    txPremine.vin[0].scriptSig = CScript() << 1 << OP_0;
    txPremine.vout = GetPremineOutputs();

    CValidationState state;
    bool ok = CheckTransaction(CTransaction(txPremine), state, true);
    BOOST_TEST_MESSAGE("CheckTransaction result: " << ok << " reject reason: " << state.GetRejectReason());
    BOOST_CHECK(ok);
}

BOOST_AUTO_TEST_SUITE_END()
