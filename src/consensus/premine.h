// Copyright (c) 2026 The KrovaCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef KROVACOIN_CONSENSUS_PREMINE_H
#define KROVACOIN_CONSENSUS_PREMINE_H

#include "amount.h"
#include "primitives/transaction.h"

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

// Sum of all allocations. Asserts on overflow -- this must always equal exactly
// 72,000,000,000 * COIN; a mismatch here means the allocation table itself is broken
// and the node must refuse to run rather than silently mint the wrong supply.
CAmount GetPremineTotal();

// The block-1 coinbase outputs corresponding to vPremineAllocations, in table order.
std::vector<CTxOut> GetPremineOutputs();

// Consensus check: does this coinbase transaction exactly match the required
// block-1 premine structure (same outputs, same order, same amounts, nothing else)?
bool CheckPremineCoinbase(const CTransaction& coinbaseTx);

#endif // KROVACOIN_CONSENSUS_PREMINE_H
