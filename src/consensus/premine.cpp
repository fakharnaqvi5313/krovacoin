// Copyright (c) 2026 The KrovaCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/premine.h"

#include "script/script.h"
#include "utilstrencodings.h"

#include <cassert>

CAmount GetPremineTotal()
{
    CAmount total = 0;
    for (size_t i = 0; i < NUM_PREMINE_ALLOCATIONS; i++) {
        bool ok = CheckedAdd(total, vPremineAllocations[i].amount, total);
        assert(ok && "premine allocation table overflow");
    }
    assert(total == 72000000000LL * COIN && "premine allocation table must sum to exactly 72,000,000,000 KROV");
    return total;
}

std::vector<CTxOut> GetPremineOutputs()
{
    std::vector<CTxOut> outputs;
    outputs.reserve(NUM_PREMINE_ALLOCATIONS);
    for (size_t i = 0; i < NUM_PREMINE_ALLOCATIONS; i++) {
        std::vector<unsigned char> scriptBytes = ParseHex(vPremineAllocations[i].scriptPubKeyHex);
        CScript scriptPubKey(scriptBytes.begin(), scriptBytes.end());
        outputs.emplace_back(vPremineAllocations[i].amount, scriptPubKey);
    }
    return outputs;
}

bool CheckPremineCoinbase(const CTransaction& coinbaseTx)
{
    const std::vector<CTxOut> expected = GetPremineOutputs();
    if (coinbaseTx.vout.size() != expected.size()) return false;
    for (size_t i = 0; i < expected.size(); i++) {
        if (coinbaseTx.vout[i].nValue != expected[i].nValue) return false;
        if (coinbaseTx.vout[i].scriptPubKey != expected[i].scriptPubKey) return false;
    }
    return true;
}
