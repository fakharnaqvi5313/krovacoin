// Copyright (c) 2026 The KrovaCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/vesting.h"

#include "amount.h"
#include "chainparams.h"
#include "consensus/premine.h"
#include "script/script.h"
#include "test/test_krova.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(vesting_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(vesting_output_count_and_total)
{
    std::vector<CTxOut> outputs = GetTeamVestingOutputs();
    BOOST_CHECK_EQUAL(outputs.size(), 36);

    CAmount total = 0;
    for (const CTxOut& out : outputs) {
        total += out.nValue;
    }
    BOOST_CHECK_EQUAL(total, GetTeamFoundersAllocation().amount);
    BOOST_CHECK_EQUAL(total, 7200000000LL * COIN);
}

BOOST_AUTO_TEST_CASE(vesting_tranches_are_equal)
{
    std::vector<CTxOut> outputs = GetTeamVestingOutputs();
    CAmount expectedTranche = (7200000000LL * COIN) / 36;
    for (const CTxOut& out : outputs) {
        BOOST_CHECK_EQUAL(out.nValue, expectedTranche);
    }
}

BOOST_AUTO_TEST_CASE(vesting_locktimes_are_monthly_and_increasing)
{
    std::vector<CTxOut> outputs = GetTeamVestingOutputs();
    BOOST_REQUIRE_EQUAL(outputs.size(), 36);

    // Each script is: <locktime> OP_CHECKLOCKTIMEVERIFY OP_DROP OP_DUP
    // OP_HASH160 <20-byte hash> OP_EQUALVERIFY OP_CHECKSIG -- pull the
    // locktime back out of the first push to check the schedule.
    // CScriptNum has no operator<<(ostream&), which BOOST_CHECK_GT needs for
    // its failure message -- extract via getint() instead. Safe here since
    // every locktime is a real-world UNIX timestamp, comfortably under
    // INT32_MAX (~2^31), not because CScriptNum itself is 32-bit (it isn't).
    std::vector<int> locktimes;
    for (const CTxOut& out : outputs) {
        CScript::const_iterator pc = out.scriptPubKey.begin();
        opcodetype opcode;
        std::vector<unsigned char> vch;
        BOOST_REQUIRE(out.scriptPubKey.GetOp(pc, opcode, vch));
        locktimes.push_back(CScriptNum(vch, false, 5).getint());
    }

    int64_t genesisTime = Params().GenesisBlock().nTime;
    // First tranche unlocks at genesis + 12 months (the cliff); every
    // following tranche is exactly one calendar month after the previous,
    // and every locktime is a real-world UNIX timestamp (well above BIP65's
    // LOCKTIME_THRESHOLD, i.e. this is a time-lock, not a height-lock).
    BOOST_CHECK_GT(locktimes[0], genesisTime);
    for (size_t i = 0; i < locktimes.size(); i++) {
        BOOST_CHECK_GT(locktimes[i], 500000000); // LOCKTIME_THRESHOLD
        if (i > 0) {
            BOOST_CHECK_GT(locktimes[i], locktimes[i - 1]);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
