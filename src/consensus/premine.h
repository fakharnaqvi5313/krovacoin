// Copyright (c) 2026 The KrovaCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef KROVACOIN_CONSENSUS_PREMINE_H
#define KROVACOIN_CONSENSUS_PREMINE_H

#include "amount.h"
#include "primitives/transaction.h"

#include <cassert>
#include <cstring>
#include <string>
#include <vector>

/**
 * The 72,000,000,000 KROV genesis allocation. Because a genesis-block coinbase
 * output is unspendable by protocol design, the real premine is paid out in the
 * block-1 coinbase instead, under consensus rules enforced by CheckPremineCoinbase()
 * (see validation.cpp's ConnectBlock, height == 1 special case).
 *
 * TODO before any public testnet/mainnet launch: every scriptPubKeyHex below is a
 * placeholder P2PKH address generated for development only (see KrovaCoin_Launch_Plan.md
 * §9 "key-ceremony"). Replace all six with real 3-of-5 multisig addresses from the
 * key ceremony before freezing chain parameters -- changing them after block 1 has
 * been mined is impossible without a new genesis.
 */
struct PremineAllocation {
    const char* name;
    CAmount amount;
    const char* scriptPubKeyHex; // P2PKH script: OP_DUP OP_HASH160 <20-byte hash> OP_EQUALVERIFY OP_CHECKSIG
};

static const PremineAllocation vPremineAllocations[] = {
    { "StakingRewardsPool",  25200000000LL * COIN, "76a914d5799b311c94188c9c4b5271f9735666dba138e188ac" }, // 35%
    { "EcosystemTreasury",   14400000000LL * COIN, "76a9147a5acc292eaee02a8ff3192c3875ccd960e0896388ac" }, // 20%
    { "TeamFounders",         7200000000LL * COIN, "76a914a6d1abf6015c6291b3a4d988a8f07401fe6c490388ac" }, // 10%
    { "Community",           10800000000LL * COIN, "76a914e6f6a25cc877f18211e2c546e6618857b3eab0f188ac" }, // 15%
    { "PublicSaleLiquidity", 10800000000LL * COIN, "76a914611f57de4233c92f95a4310c57ffef8347b9ba6c88ac" }, // 15%
    { "StrategicReserve",     3600000000LL * COIN, "76a9144fafdae3b2a0bc9fdc2f9c5ccb3f08838084b57e88ac" }, // 5%
};
static const size_t NUM_PREMINE_ALLOCATIONS = sizeof(vPremineAllocations) / sizeof(vPremineAllocations[0]);

// The Staking Rewards Pool is always allocation index 0 -- exposed by name so
// dependent code (e.g. consensus/superblock.cpp, task 7) doesn't hardcode the
// index and silently break if the table is ever reordered.
inline const PremineAllocation& GetStakingRewardsPoolAllocation()
{
    assert(strcmp(vPremineAllocations[0].name, "StakingRewardsPool") == 0);
    return vPremineAllocations[0];
}

// The Team/Founders allocation is always allocation index 2 -- exposed by name
// for the same reason as GetStakingRewardsPoolAllocation() above. Its
// scriptPubKeyHex here is used only as the destination pubkey hash for the
// vesting schedule; the block-1 coinbase does not actually pay this address
// directly (see consensus/vesting.h, task 9).
inline const PremineAllocation& GetTeamFoundersAllocation()
{
    assert(strcmp(vPremineAllocations[2].name, "TeamFounders") == 0);
    return vPremineAllocations[2];
}

// Returns the destination script that should actually be used for `allocation`
// on the currently-selected network: the real placeholder on mainnet, or a
// single, real, team-known test key (privkey recorded in TESTNET.md; see
// premine.cpp) everywhere else. The mainnet placeholders are real "TBD at key
// ceremony" addresses -- nobody, including this team, holds their private
// keys. Without this override, testnet/regtest premine would be permanently
// unspendable: once PoS activation requires a stake, no node anywhere could
// ever produce one, and the chain would halt forever at that height. Every
// consumer of an allocation's scriptPubKeyHex (GetPremineOutputs,
// GetTeamVestingOutputs, the superblock payout builder) must go through this
// instead of reading scriptPubKeyHex directly.
std::string GetEffectiveScriptPubKeyHex(const PremineAllocation& allocation);

// Sum of all allocations. Asserts on overflow -- this must always equal exactly
// 72,000,000,000 * COIN; a mismatch here means the allocation table itself is broken
// and the node must refuse to run rather than silently mint the wrong supply.
CAmount GetPremineTotal();

// The block-1 coinbase outputs. Five of the six vPremineAllocations entries
// (all but TeamFounders) become one plain output each, in table order.
// TeamFounders is instead split into 36 CLTV-locked vesting tranche outputs
// (see consensus/vesting.h, task 9), appended after the five plain outputs --
// so the coinbase has 41 outputs total, not 6.
std::vector<CTxOut> GetPremineOutputs();

// Consensus check: does this coinbase transaction exactly match the required
// block-1 premine structure (same outputs, same order, same amounts, nothing else)?
bool CheckPremineCoinbase(const CTransaction& coinbaseTx);

#endif // KROVACOIN_CONSENSUS_PREMINE_H
