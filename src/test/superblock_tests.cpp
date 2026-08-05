// Copyright (c) 2026 The KrovaCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/superblock.h"
#include "amount.h"
#include "test/test_krova.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(superblock_tests, BasicTestingSetup)

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

BOOST_AUTO_TEST_SUITE_END()
