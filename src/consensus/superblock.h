// Copyright (c) 2026 The KrovaCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef KROVACOIN_CONSENSUS_SUPERBLOCK_H
#define KROVACOIN_CONSENSUS_SUPERBLOCK_H

#include "amount.h"
#include "primitives/transaction.h"
#include "script/script.h"

#include <map>
#include <vector>

class CBlockIndex;
class CCoinsViewCache;

/**
 * Staking Rewards Pool superblock payouts (task 7 of the launch plan; flagged
 * there as the highest-risk, most-modified consensus path).
 *
 * The pool's 25.2B KROV allocation (see consensus/premine.h) is a REAL,
 * spendable UTXO, not a schedule-capped mint -- it must actually deplete over
 * the 20-year schedule. Minting new coins on a schedule while leaving the
 * pool's premine output untouched would double-count supply (pool balance +
 * newly-minted rewards could exceed 72B), breaking the zero-inflation
 * guarantee. So every superblock cycle spends the pool's current UTXO,
 * paying stakers pro-rata by blocks staked in the preceding cycle, with
 * change back to the pool's own scriptPubKey for the next cycle -- a linear
 * UTXO chain starting at the block-1 premine's pool output.
 *
 * Cadence reuses Consensus::Params::nBudgetCycleBlocks (repurposed from
 * PIVX's masternode-budget-voting cycle length, which is otherwise dead code
 * in KrovaCoin -- see GetTotalBudget() in budgetmanager.cpp).
 */

// True if nHeight is a superblock (pool-payout) height: the first superblock
// occurs at the PoS activation height (UPGRADE_POS) for the active network,
// then every nBudgetCycleBlocks blocks after that.
bool IsSuperblockHeight(int nHeight);

// 0-indexed cycle number for a superblock height (undefined if !IsSuperblockHeight).
int64_t GetSuperblockCycleIndex(int nHeight);

// Total KROV that should have left the pool by the end of cycle `cycleIndex`
// (0-indexed, inclusive), per the 20-year declining schedule:
//   cycles 0..1824    (years 1-5):   2,520,000,000 KROV/yr  (12.6B total)
//   cycles 1825..3649 (years 6-10):  1,260,000,000 KROV/yr  ( 6.3B total)
//   cycles 3650..7299 (years 11-20):   630,000,000 KROV/yr  ( 6.3B total)
//   cycles >= 7300: capped at 25,200,000,000 KROV (pool exhausted; stakers
//   then live on fees only, matching GetBlockValue's zero-inflation design).
// Uses a cumulative-target formula (not a per-cycle rate multiplied out) so
// the running total is always exact -- no rounding drift can accumulate
// across cycles or years. One cycle == one day (nBudgetCycleBlocks blocks
// at the network's block-time).
CAmount GetSuperblockCumulativeTarget(int64_t cycleIndex);

// The amount that should leave the pool in this specific cycle: the marginal
// difference between consecutive cumulative targets.
CAmount GetSuperblockPayout(int nHeight);

// Tally of blocks staked per destination script across
// [cycleStartHeight, cycleEndHeight] (inclusive), walking the active chain's
// block index and each block's coinstake. Returns an empty map if the range
// contains no coinstakes (e.g. the first superblock's lookback window falls
// entirely inside the PoW bootstrap period -- the "zero stakers" edge case).
std::map<CScript, int> TallyStakersInCycle(int cycleStartHeight, int cycleEndHeight);

// Given the pool's current coin value and the per-staker block tally, compute
// the payout outputs: one per staker (floor(poolPayout * theirBlocks / totalBlocks)),
// plus a final change output returning the remainder to the pool's own
// scriptPubKey. Deterministic given the same tally and cycle -- every
// validating node computes the identical result. Returns false (no payout
// this cycle) if the tally is empty.
bool BuildSuperblockPayoutOutputs(
        int nHeight,
        const CScript& poolScriptPubKey,
        CAmount poolInputValue,
        std::vector<CTxOut>& outputsOut);

// Consensus check: does `tx` exactly match the required superblock payout at
// `nHeight`, spending the pool's actual current UTXO (identified via `view`)?
// Called from ConnectBlock at every superblock height; the block is invalid
// if no transaction in it satisfies this (see validation.cpp).
bool CheckSuperblockPayoutTx(const CTransaction& tx, int nHeight, const CCoinsViewCache& view);

// True if the cycle ending just before `nHeight` had at least one staker --
// i.e. whether ConnectBlock should require a payout transaction at all. The
// very first superblock's lookback window falls entirely inside the PoW
// bootstrap period (no coinstakes yet), so it legitimately has none; without
// this check, ConnectBlock would wrongly demand a payout that can never
// exist and permanently reject every block from the first superblock on.
bool CycleHasStakers(int nHeight);

#endif // KROVACOIN_CONSENSUS_SUPERBLOCK_H
