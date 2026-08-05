// Copyright (c) 2026 The KrovaCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/vesting.h"

#include "chainparams.h"
#include "consensus/premine.h"
#include "key.h"
#include "script/script.h"
#include "script/standard.h"
#include "utilstrencodings.h"

#include <cassert>
#include <ctime>

static const int VESTING_CLIFF_MONTHS = 12;
static const int VESTING_TRANCHE_COUNT = 36;

// Adds `months` calendar months to a UTC UNIX timestamp. Day-of-month is not
// explicitly clamped: if the genesis day-of-month doesn't exist in a target
// month (e.g. the 31st, added to a 30-day month), timegm()'s standard
// out-of-range struct tm normalization rolls the date forward into the
// following month. That's deterministic and fine for a vesting schedule
// (never produces an earlier or duplicate unlock date), and is moot for
// KrovaCoin's current placeholder genesis timestamps (all day 5), but is
// documented here since it becomes relevant once the real launch date is
// chosen (see premine.h's key-ceremony TODO).
static int64_t AddMonthsUTC(int64_t unixTime, int months)
{
    time_t t = (time_t)unixTime;
    struct tm tmVal;
    gmtime_r(&t, &tmVal);
    tmVal.tm_mon += months;
    return (int64_t)timegm(&tmVal);
}

static CScript GetCLTVPayToPubKeyHash(int64_t nLockTime, const CKeyID& keyID)
{
    CScript script;
    script << nLockTime << OP_CHECKLOCKTIMEVERIFY << OP_DROP;
    script << OP_DUP << OP_HASH160 << ToByteVector(keyID) << OP_EQUALVERIFY << OP_CHECKSIG;
    return script;
}

std::vector<CTxOut> GetTeamVestingOutputs()
{
    const PremineAllocation& team = GetTeamFoundersAllocation();
    // 7,200,000,000 KROV / 36 tranches = 200,000,000 KROV exactly -- no
    // remainder to carry or drop, unlike the superblock pool's pro-rata
    // splits (task 7), which do need a remainder/change output.
    assert(team.amount % VESTING_TRANCHE_COUNT == 0);
    CAmount trancheAmount = team.amount / VESTING_TRANCHE_COUNT;

    std::vector<unsigned char> scriptBytes = ParseHex(team.scriptPubKeyHex);
    CScript teamP2PKH(scriptBytes.begin(), scriptBytes.end());
    CTxDestination dest;
    bool ok = ExtractDestination(teamP2PKH, dest);
    assert(ok);
    const CKeyID* keyID = boost::get<CKeyID>(&dest);
    assert(keyID != nullptr); // TeamFounders allocation must be a plain P2PKH address

    int64_t genesisTime = Params().GenesisBlock().nTime;

    std::vector<CTxOut> outputs;
    outputs.reserve(VESTING_TRANCHE_COUNT);
    for (int i = 0; i < VESTING_TRANCHE_COUNT; i++) {
        int64_t unlockTime = AddMonthsUTC(genesisTime, VESTING_CLIFF_MONTHS + i);
        outputs.emplace_back(trancheAmount, GetCLTVPayToPubKeyHash(unlockTime, *keyID));
    }
    return outputs;
}
