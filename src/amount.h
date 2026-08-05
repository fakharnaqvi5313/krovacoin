// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin developers
// Copyright (c) 2017-2020 The KROVACOIN Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef KROVACOIN_AMOUNT_H
#define KROVACOIN_AMOUNT_H

#include <stdint.h>
#include <limits>

/** Amount in KROV (Can be negative) */
typedef int64_t CAmount;

static const CAmount COIN = 100000000;
static const CAmount CENT = 1000000;

/**
 * Overflow-safe CAmount addition. Use anywhere multiple amounts (outputs, fees,
 * allocations) are summed outside the standard per-output-then-MoneyRange loop
 * pattern used in CheckTransaction -- see task 8 (int64 overflow audit) in the
 * launch plan. A single value near MAX_MONEY (72e9 * COIN =~ 7.2e18) still has
 * headroom under int64 (~9.22e18), but summing more than one such value, or
 * multiplying by an independent count/height/interval, can overflow -- this bit
 * a real budget-total calculation during development (multiplying the one-time
 * genesis premine value by a block-cycle count). Returns false on overflow
 * instead of silently wrapping.
 */
inline bool CheckedAdd(CAmount a, CAmount b, CAmount& out)
{
    if (b > 0 && a > std::numeric_limits<CAmount>::max() - b) return false;
    if (b < 0 && a < std::numeric_limits<CAmount>::min() - b) return false;
    out = a + b;
    return true;
}

#endif //  KROVACOIN_AMOUNT_H
