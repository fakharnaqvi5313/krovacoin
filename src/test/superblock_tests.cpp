// Copyright (c) 2026 The KrovaCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/superblock.h"
#include "amount.h"
#include "script/script.h"
#include "test/test_krova.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(superblock_tests, BasicTestingSetup)

static CScript DummyScript(unsigned char tag)
{
    return CScript() << OP_DUP << OP_HASH160 << std::vector<unsigned char>(20, tag) << OP_EQUALVERIFY << OP_CHECKSIG;
}

BOOST_AUTO_TEST_CASE(superblock_cumulative_target_year_boundaries)
{
    // Year boundaries (one cycle == one day, 365 days/year):
    //   cycles 0..1824    (years 1-5):  12.6B total
    //   cycles 1825..3649 (years 6-10):  6.3B total (18.9B cumulative)
    //   cycles 3650..7299 (years 11-20): 6.3B total (25.2B cumulative, final)
    BOOST_CHECK_EQUAL(GetSuperblockCumulativeTarget(-1), 0);
    BOOST_CHECK_EQUAL(GetSuperblockCumulativeTarget(0), 2520000000LL * COIN / 365);

    // End of year 5 (cycle index 1824 = the 1825th cycle): exactly 12.6B, no
    // rounding remainder, since 1825 is an exact multiple of 365.
    BOOST_CHECK_EQUAL(GetSuperblockCumulativeTarget(1824), 12600000000LL * COIN);

    // End of year 10 (cycle index 3649 = 3650th cycle): 12.6B + 6.3B.
    BOOST_CHECK_EQUAL(GetSuperblockCumulativeTarget(3649), 18900000000LL * COIN);

    // End of year 20 (cycle index 7299 = 7300th cycle): the full 25.2B.
    BOOST_CHECK_EQUAL(GetSuperblockCumulativeTarget(7299), 25200000000LL * COIN);

    // Pool exhausted: capped at 25.2B forever after, no overflow.
    BOOST_CHECK_EQUAL(GetSuperblockCumulativeTarget(7300), 25200000000LL * COIN);
    BOOST_CHECK_EQUAL(GetSuperblockCumulativeTarget(1000000), 25200000000LL * COIN);
}

BOOST_AUTO_TEST_CASE(superblock_cumulative_target_monotonic)
{
    // The running total must never decrease -- GetSuperblockPayout subtracts
    // consecutive cumulative targets and assert()s this internally, so a
    // violation here would abort rather than silently misbehave, but this
    // test catches it long before that ever runs against a real chain.
    CAmount prev = 0;
    for (int64_t i = -1; i < 7310; i++) {
        CAmount cur = GetSuperblockCumulativeTarget(i);
        BOOST_CHECK(cur >= prev);
        prev = cur;
    }
}

BOOST_AUTO_TEST_CASE(superblock_payout_sums_to_exact_total)
{
    // The whole point of the cumulative-target formula: no matter how the
    // daily amounts round, the 20-year sum must equal exactly 25.2B KROV --
    // the pool's premine allocation, to the base unit. A drift here would
    // mean the pool either runs dry early or never fully pays out.
    CAmount total = 0;
    for (int64_t cycleIndex = 0; cycleIndex < 7300; cycleIndex++) {
        CAmount cur = GetSuperblockCumulativeTarget(cycleIndex);
        CAmount prev = GetSuperblockCumulativeTarget(cycleIndex - 1);
        CAmount payout = cur - prev;
        BOOST_CHECK(payout >= 0);
        total += payout;
    }
    BOOST_CHECK_EQUAL(total, 25200000000LL * COIN);

    // And nothing pays out after the schedule ends.
    BOOST_CHECK_EQUAL(GetSuperblockCumulativeTarget(7300) - GetSuperblockCumulativeTarget(7299), 0);
}

BOOST_AUTO_TEST_CASE(superblock_distribution_normal_case)
{
    // Pool has plenty: distribution matches floor-division shares exactly, with
    // the remainder landing in the change output back to the pool.
    CScript pool = DummyScript(0xAA);
    CScript stakerA = DummyScript(0x01);
    CScript stakerB = DummyScript(0x02);
    std::map<CScript, int> tally = {{stakerA, 3}, {stakerB, 1}};

    std::vector<CTxOut> outputs;
    ComputeSuperblockDistribution(tally, /*targetPayout=*/1000, /*poolInputValue=*/1'000'000, pool, outputs);

    BOOST_REQUIRE_EQUAL(outputs.size(), 3u); // stakerA, stakerB, change
    CAmount distributed = 0;
    bool sawChange = false;
    for (const CTxOut& out : outputs) {
        if (out.scriptPubKey == pool) {
            sawChange = true;
            BOOST_CHECK_EQUAL(out.nValue, 1'000'000 - 1000); // pool keeps everything but the target payout
        } else {
            distributed += out.nValue;
        }
    }
    BOOST_CHECK(sawChange);
    BOOST_CHECK_EQUAL(distributed, 1000); // 750 + 250, exact (3:1 split of 1000)
}

BOOST_AUTO_TEST_CASE(superblock_distribution_clamps_to_short_pool_balance)
{
    // The exact bug this test locks in: reproduced live for the first time this
    // path was ever exercised end-to-end (see the comment on
    // ComputeSuperblockDistribution) -- a pool coin worth LESS than the
    // schedule's target must degrade to "pay out everything available,"
    // never assert/crash. Previously this asserted and took the whole node
    // down; any peer's block reaching this code path could do the same.
    CScript pool = DummyScript(0xAA);
    CScript staker = DummyScript(0x01);
    std::map<CScript, int> tally = {{staker, 1}};

    std::vector<CTxOut> outputs;
    // targetPayout (1000) > poolInputValue (7) -- the exact shape of the crash.
    ComputeSuperblockDistribution(tally, /*targetPayout=*/1000, /*poolInputValue=*/7, pool, outputs);

    BOOST_REQUIRE_EQUAL(outputs.size(), 1u); // staker gets everything, nothing left for change
    BOOST_CHECK(outputs[0].scriptPubKey == staker);
    BOOST_CHECK_EQUAL(outputs[0].nValue, 7);
}

BOOST_AUTO_TEST_CASE(superblock_distribution_pool_exactly_covers_target)
{
    // Boundary: poolInputValue == targetPayout exactly -- no change output,
    // pool coin fully consumed, still no assert.
    CScript pool = DummyScript(0xAA);
    CScript staker = DummyScript(0x01);
    std::map<CScript, int> tally = {{staker, 1}};

    std::vector<CTxOut> outputs;
    ComputeSuperblockDistribution(tally, /*targetPayout=*/500, /*poolInputValue=*/500, pool, outputs);

    BOOST_REQUIRE_EQUAL(outputs.size(), 1u);
    BOOST_CHECK_EQUAL(outputs[0].nValue, 500);
}

BOOST_AUTO_TEST_CASE(superblock_distribution_empty_tally_produces_no_outputs)
{
    CScript pool = DummyScript(0xAA);
    std::vector<CTxOut> outputs;
    ComputeSuperblockDistribution({}, /*targetPayout=*/1000, /*poolInputValue=*/1'000'000, pool, outputs);
    BOOST_CHECK(outputs.empty());
}

BOOST_AUTO_TEST_CASE(superblock_distribution_zero_target_produces_no_outputs)
{
    CScript pool = DummyScript(0xAA);
    CScript staker = DummyScript(0x01);
    std::map<CScript, int> tally = {{staker, 1}};
    std::vector<CTxOut> outputs;
    ComputeSuperblockDistribution(tally, /*targetPayout=*/0, /*poolInputValue=*/1'000'000, pool, outputs);
    BOOST_CHECK(outputs.empty());
}

BOOST_AUTO_TEST_SUITE_END()
