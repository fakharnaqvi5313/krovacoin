// Copyright (c) 2026 The KrovaCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef KROVACOIN_CONSENSUS_VESTING_H
#define KROVACOIN_CONSENSUS_VESTING_H

#include "primitives/transaction.h"

#include <vector>

/**
 * The Team/Founders allocation (7,200,000,000 KROV, see consensus/premine.h)
 * vests as a 12-month cliff followed by 36 equal monthly tranches (4 years
 * total). Each tranche is its own block-1 coinbase output, individually
 * CLTV-locked (OP_CHECKLOCKTIMEVERIFY, BIP65 -- always-active consensus rule
 * on every KrovaCoin network, see chainparams.cpp) to a UNIX timestamp
 * computed relative to this network's own genesis block time, so testnet and
 * regtest vest on the same relative schedule as mainnet even though their
 * genesis timestamps differ. This set of outputs replaces the single plain
 * P2PKH TeamFounders entry that vPremineAllocations would otherwise produce
 * (task 9).
 */
std::vector<CTxOut> GetTeamVestingOutputs();

#endif // KROVACOIN_CONSENSUS_VESTING_H
