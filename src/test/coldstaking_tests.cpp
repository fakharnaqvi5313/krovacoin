// Copyright (c) 2026 The KrovaCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Direct unit tests for CheckColdStakeFreeOutput (validation.cpp). This is the
// function task 10 rewrote after discovering it used to fall back to
// masternode/budget-payment checks for a coinstake's trailing "free" output --
// a fallback that turned out to be live code (UPGRADE_V6_0 never activates on
// any KrovaCoin network) referencing subsystems that no longer exist. KrovaCoin
// has no masternode reward and no coldstake-embedded budget payment, so the
// rewritten behavior requires any free output to duplicate the one before it,
// which in practice means the "free output" slot can never legitimately be used
// for anything.

#include "validation.h"

#include "amount.h"
#include "key.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "script/standard.h"
#include "test/test_krova.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(coldstaking_consensus_tests, BasicTestingSetup)

namespace {

CMutableTransaction MakeCoinStakeSkeleton(const CScript& p2csScript)
{
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].prevout.n = 0;
    tx.vin[0].prevout.hash = uint256S("1111111111111111111111111111111111111111111111111111111111111111");
    tx.vout.resize(2);
    tx.vout[0].nValue = 0;
    tx.vout[0].scriptPubKey.clear();
    tx.vout[1].nValue = 100 * COIN;
    tx.vout[1].scriptPubKey = p2csScript;
    return tx;
}

} // namespace

BOOST_AUTO_TEST_CASE(non_lof_coldstake_always_passes)
{
    CKey stakerKey, ownerKey;
    stakerKey.MakeNewKey(true);
    ownerKey.MakeNewKey(true);
    CScript plainP2CS = GetScriptForStakeDelegation(stakerKey.GetPubKey().GetID(), ownerKey.GetPubKey().GetID());

    CMutableTransaction tx = MakeCoinStakeSkeleton(plainP2CS);
    // A mismatched trailing output would matter for the LOF variant, but this
    // is a plain (non-LOF) cold-stake script, so it's irrelevant here.
    tx.vout.emplace_back(5 * COIN, CScript() << OP_TRUE);
    BOOST_REQUIRE(CTransaction(tx).IsCoinStake());
    BOOST_CHECK(CheckColdStakeFreeOutput(CTransaction(tx), 1));
}

BOOST_AUTO_TEST_CASE(lof_coldstake_with_no_free_output_passes)
{
    CKey stakerKey, ownerKey;
    stakerKey.MakeNewKey(true);
    ownerKey.MakeNewKey(true);
    CScript lofP2CS = GetScriptForStakeDelegationLOF(stakerKey.GetPubKey().GetID(), ownerKey.GetPubKey().GetID());

    CMutableTransaction tx = MakeCoinStakeSkeleton(lofP2CS);
    BOOST_REQUIRE_EQUAL(tx.vout.size(), 2);
    BOOST_REQUIRE(CTransaction(tx).IsCoinStake());
    BOOST_CHECK(CheckColdStakeFreeOutput(CTransaction(tx), 1));
}

BOOST_AUTO_TEST_CASE(lof_coldstake_with_mismatched_free_output_fails)
{
    CKey stakerKey, ownerKey;
    stakerKey.MakeNewKey(true);
    ownerKey.MakeNewKey(true);
    CScript lofP2CS = GetScriptForStakeDelegationLOF(stakerKey.GetPubKey().GetID(), ownerKey.GetPubKey().GetID());

    CMutableTransaction tx = MakeCoinStakeSkeleton(lofP2CS);
    // A third output that does not match vout[1] (the P2CS-LOF script) --
    // this is exactly the shape a masternode/budget payment would have taken
    // pre-task-10, and must now be rejected unconditionally.
    tx.vout.emplace_back(5 * COIN, CScript() << OP_TRUE);
    BOOST_REQUIRE(CTransaction(tx).IsCoinStake());
    BOOST_CHECK(!CheckColdStakeFreeOutput(CTransaction(tx), 1));
}

BOOST_AUTO_TEST_CASE(lof_coldstake_with_duplicated_free_output_passes)
{
    CKey stakerKey, ownerKey;
    stakerKey.MakeNewKey(true);
    ownerKey.MakeNewKey(true);
    CScript lofP2CS = GetScriptForStakeDelegationLOF(stakerKey.GetPubKey().GetID(), ownerKey.GetPubKey().GetID());

    CMutableTransaction tx = MakeCoinStakeSkeleton(lofP2CS);
    // Only the very last output is checked against the one before it, so with
    // 3 outputs the "free" output (vout[2]) is compared against vout[1] --
    // the P2CS-LOF script itself. Duplicating it is the only way to pass.
    tx.vout.emplace_back(5 * COIN, lofP2CS);
    BOOST_REQUIRE(CTransaction(tx).IsCoinStake());
    BOOST_CHECK(CheckColdStakeFreeOutput(CTransaction(tx), 1));
}

BOOST_AUTO_TEST_CASE(lof_coldstake_checks_only_the_last_pair)
{
    CKey stakerKey, ownerKey;
    stakerKey.MakeNewKey(true);
    ownerKey.MakeNewKey(true);
    CScript lofP2CS = GetScriptForStakeDelegationLOF(stakerKey.GetPubKey().GetID(), ownerKey.GetPubKey().GetID());
    CScript dummy = CScript() << OP_TRUE;

    CMutableTransaction tx = MakeCoinStakeSkeleton(lofP2CS);
    // 4 outputs: vout[2] and vout[3] must match each other -- vout[1] is not
    // part of the comparison once there's more than one trailing output.
    tx.vout.emplace_back(5 * COIN, dummy);
    tx.vout.emplace_back(5 * COIN, dummy);
    BOOST_REQUIRE(CTransaction(tx).IsCoinStake());
    BOOST_CHECK(CheckColdStakeFreeOutput(CTransaction(tx), 1));

    // Now make the last one diverge from the third.
    tx.vout.back().scriptPubKey = CScript() << OP_FALSE;
    BOOST_CHECK(!CheckColdStakeFreeOutput(CTransaction(tx), 1));
}

BOOST_AUTO_TEST_SUITE_END()
