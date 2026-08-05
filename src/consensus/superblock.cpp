// Copyright (c) 2026 The KrovaCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/superblock.h"

#include "chain.h"
#include "chainparams.h"
#include "coins.h"
#include "consensus/premine.h"
#include "consensus/upgrades.h"
#include "primitives/block.h"
#include "utilstrencodings.h"
#include "../validation.h"

#include <cassert>
#include <limits>

static const CAmount PHASE_A_ANNUAL = 2520000000LL * COIN; // years 1-5
static const CAmount PHASE_B_ANNUAL = 1260000000LL * COIN; // years 6-10
static const CAmount PHASE_C_ANNUAL =  630000000LL * COIN; // years 11-20
static const int64_t CYCLES_PER_YEAR = 365; // one cycle == one day, by design

static const int64_t PHASE_A_CYCLES = 5 * CYCLES_PER_YEAR;  // 1825
static const int64_t PHASE_B_CYCLES = 5 * CYCLES_PER_YEAR;  // 1825
static const int64_t PHASE_C_CYCLES = 10 * CYCLES_PER_YEAR; // 3650

static const CAmount PHASE_A_TOTAL = 5 * PHASE_A_ANNUAL;  // 12.6B
static const CAmount PHASE_B_TOTAL = 5 * PHASE_B_ANNUAL;  //  6.3B
static const CAmount PHASE_C_TOTAL = 10 * PHASE_C_ANNUAL; //  6.3B
static const CAmount POOL_TOTAL = PHASE_A_TOTAL + PHASE_B_TOTAL + PHASE_C_TOTAL; // 25.2B

// a*b can overflow int64_t well before the division brings it back into range
// (e.g. 7300 cycles * 2.52e17 ~= 1.8e21) -- widen to __int128 for the
// multiply, then divide back down. See task 8: this is exactly the shape of
// bug that hit GetTotalBudget() for real.
static CAmount MulDiv(int64_t a, CAmount b, int64_t c)
{
    assert(a >= 0 && b >= 0 && c > 0);
    __int128 result = (__int128)a * (__int128)b / (__int128)c;
    assert(result >= 0 && result <= (__int128)std::numeric_limits<CAmount>::max());
    return (CAmount)result;
}

static int GetFirstSuperblockHeight()
{
    const Consensus::Params& consensus = Params().GetConsensus();
    int nFirst = consensus.vUpgrades[Consensus::UPGRADE_POS].nActivationHeight;
    if (nFirst == Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT ||
        nFirst == Consensus::NetworkUpgrade::ALWAYS_ACTIVE) {
        return -1; // not a valid configuration for this feature
    }
    return nFirst;
}

bool IsSuperblockHeight(int nHeight)
{
    int nFirst = GetFirstSuperblockHeight();
    if (nFirst < 0 || nHeight < nFirst) return false;
    int nCycle = Params().GetConsensus().nBudgetCycleBlocks;
    if (nCycle <= 0) return false;
    return (nHeight - nFirst) % nCycle == 0;
}

int64_t GetSuperblockCycleIndex(int nHeight)
{
    int nFirst = GetFirstSuperblockHeight();
    int nCycle = Params().GetConsensus().nBudgetCycleBlocks;
    return (nHeight - nFirst) / nCycle;
}

CAmount GetSuperblockCumulativeTarget(int64_t cycleIndex)
{
    int64_t n = cycleIndex + 1; // number of cycles completed by (and including) cycleIndex
    if (n <= 0) return 0;

    if (n <= PHASE_A_CYCLES) {
        return MulDiv(n, PHASE_A_ANNUAL, CYCLES_PER_YEAR);
    }
    if (n <= PHASE_A_CYCLES + PHASE_B_CYCLES) {
        return PHASE_A_TOTAL + MulDiv(n - PHASE_A_CYCLES, PHASE_B_ANNUAL, CYCLES_PER_YEAR);
    }
    if (n <= PHASE_A_CYCLES + PHASE_B_CYCLES + PHASE_C_CYCLES) {
        return PHASE_A_TOTAL + PHASE_B_TOTAL + MulDiv(n - PHASE_A_CYCLES - PHASE_B_CYCLES, PHASE_C_ANNUAL, CYCLES_PER_YEAR);
    }
    return POOL_TOTAL; // pool exhausted
}

CAmount GetSuperblockPayout(int nHeight)
{
    if (!IsSuperblockHeight(nHeight)) return 0;
    int64_t idx = GetSuperblockCycleIndex(nHeight);
    CAmount cur = GetSuperblockCumulativeTarget(idx);
    CAmount prev = GetSuperblockCumulativeTarget(idx - 1);
    assert(cur >= prev); // cumulative target is non-decreasing by construction
    return cur - prev;
}

std::map<CScript, int> TallyStakersInCycle(int cycleStartHeight, int cycleEndHeight)
{
    std::map<CScript, int> tally;
    if (cycleStartHeight < 1) cycleStartHeight = 1; // height 0 (genesis) is never a coinstake

    for (int h = cycleStartHeight; h <= cycleEndHeight; h++) {
        CBlockIndex* pindex = chainActive[h];
        if (!pindex) continue; // past the current tip; nothing more to tally
        CBlock block;
        if (!ReadBlockFromDisk(block, pindex)) continue;
        if (!block.IsProofOfStake()) continue; // PoW bootstrap block, no staker
        // vout[0] is the coinstake marker (value 0, empty script); the real
        // payout starts at vout[1], and any stake-split outputs after it
        // share the same scriptPubKey -- see CWallet::CreateCoinstakeOuts.
        if (block.vtx[1]->vout.size() < 2) continue;
        const CScript& staker = block.vtx[1]->vout[1].scriptPubKey;
        tally[staker]++;
    }
    return tally;
}

bool BuildSuperblockPayoutOutputs(
        int nHeight,
        const CScript& poolScriptPubKey,
        CAmount poolInputValue,
        std::vector<CTxOut>& outputsOut)
{
    outputsOut.clear();
    int nCycle = Params().GetConsensus().nBudgetCycleBlocks;
    int cycleEndHeight = nHeight - 1;
    int cycleStartHeight = cycleEndHeight - nCycle + 1;

    std::map<CScript, int> tally = TallyStakersInCycle(cycleStartHeight, cycleEndHeight);
    if (tally.empty()) return false; // zero stakers this cycle -- no payout, pool untouched

    int totalBlocks = 0;
    for (const auto& entry : tally) totalBlocks += entry.second;

    CAmount targetPayout = GetSuperblockPayout(nHeight);
    if (targetPayout <= 0) return false; // pool exhausted or misconfigured

    CAmount distributed = 0;
    for (const auto& entry : tally) {
        // Floor division: the sum of shares is <= targetPayout by construction,
        // so it can never overpay the pool. Any remainder (a few base units at
        // most, from rounding) simply stays in the pool's change output below
        // -- it isn't lost, just carried into future cycles' distributions.
        CAmount share = MulDiv(entry.second, targetPayout, totalBlocks);
        if (share <= 0) continue;
        outputsOut.emplace_back(share, entry.first);
        distributed += share;
    }

    assert(distributed <= targetPayout);
    assert(distributed <= poolInputValue);
    CAmount change = poolInputValue - distributed;
    if (change > 0) {
        outputsOut.emplace_back(change, poolScriptPubKey);
    }
    return !outputsOut.empty();
}

bool CycleHasStakers(int nHeight)
{
    int nCycle = Params().GetConsensus().nBudgetCycleBlocks;
    int cycleEndHeight = nHeight - 1;
    int cycleStartHeight = cycleEndHeight - nCycle + 1;
    return !TallyStakersInCycle(cycleStartHeight, cycleEndHeight).empty();
}

bool CheckSuperblockPayoutTx(const CTransaction& tx, int nHeight, const CCoinsViewCache& view)
{
    if (tx.vin.size() != 1) return false;

    const Coin& coin = view.AccessCoin(tx.vin[0].prevout);
    if (coin.IsSpent()) return false;

    const CScript poolScriptPubKey = coin.out.scriptPubKey;
    // Only a transaction actually spending the pool's own address can be the
    // superblock payout -- anything else is just an ordinary transaction that
    // happens to be included in the same block.
    std::vector<unsigned char> poolScriptBytes = ParseHex(GetStakingRewardsPoolAllocation().scriptPubKeyHex);
    CScript expectedPoolScript(poolScriptBytes.begin(), poolScriptBytes.end());
    if (poolScriptPubKey != expectedPoolScript) return false;

    std::vector<CTxOut> expected;
    if (!BuildSuperblockPayoutOutputs(nHeight, poolScriptPubKey, coin.out.nValue, expected)) {
        return false;
    }

    if (tx.vout.size() != expected.size()) return false;
    for (size_t i = 0; i < expected.size(); i++) {
        if (tx.vout[i].nValue != expected[i].nValue) return false;
        if (tx.vout[i].scriptPubKey != expected[i].scriptPubKey) return false;
    }
    return true;
}
