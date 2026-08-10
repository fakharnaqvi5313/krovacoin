// Copyright (c) 2026 The KrovaCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/premine.h"

#include "chainparams.h"
#include "consensus/vesting.h"
#include "script/script.h"
#include "utilstrencodings.h"

#include <cassert>

// Real, team-known keypair (see TESTNET.md for the WIF privkey) used for
// every allocation on every non-mainnet network -- see GetEffectiveScriptPubKeyHex()'s
// doc comment in premine.h for why this exists.
//
// Regenerated 2026-08-10: the previous key's WIF was never actually recorded anywhere
// (TESTNET.md didn't exist despite this comment referencing it), making every prior
// regtest/testnet chain's premine permanently unspendable by anyone. This one's WIF is
// committed to TESTNET.md at the same time as this change, so that doesn't happen again.
static const char* TEST_PREMINE_SCRIPTPUBKEY_HEX = "76a91492f2bd80a027f45a50c1a0b44b0754e5e976238288ac";

std::string GetEffectiveScriptPubKeyHex(const PremineAllocation& allocation)
{
    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
        return allocation.scriptPubKeyHex;
    }
    return TEST_PREMINE_SCRIPTPUBKEY_HEX;
}

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
    outputs.reserve(NUM_PREMINE_ALLOCATIONS - 1 + 36); // 5 plain + 36 vesting tranches
    for (size_t i = 0; i < NUM_PREMINE_ALLOCATIONS; i++) {
        // Compare by name, not address: vPremineAllocations is a `static
        // const` array declared in a header, so it has internal linkage --
        // every translation unit that includes premine.h gets its OWN
        // separate copy of the array. GetTeamFoundersAllocation() is inline
        // in that same header, so whether "&vPremineAllocations[i] ==
        // &GetTeamFoundersAllocation()" actually holds depends on whether
        // the compiler inlines the call using THIS TU's copy of the array,
        // or resolves it to some other TU's instantiation via linkonce
        // symbol deduplication -- implementation-defined, and NOT
        // guaranteed to agree with what a naive reading suggests. This was
        // silently broken under Apple Clang (the pointer comparison never
        // matched, so TeamFounders got paid BOTH as a plain output here AND
        // via its 36 vesting tranches below -- 42 outputs instead of 41,
        // 7.92B KROV instead of 7.2B for this allocation alone) while
        // happening to hold under GCC. Comparing the name string's content
        // is well-defined regardless of which TU's copy either side refers
        // to.
        if (strcmp(vPremineAllocations[i].name, GetTeamFoundersAllocation().name) == 0) {
            continue; // paid out via the vesting schedule below instead
        }
        std::vector<unsigned char> scriptBytes = ParseHex(GetEffectiveScriptPubKeyHex(vPremineAllocations[i]));
        CScript scriptPubKey(scriptBytes.begin(), scriptBytes.end());
        outputs.emplace_back(vPremineAllocations[i].amount, scriptPubKey);
    }
    std::vector<CTxOut> vesting = GetTeamVestingOutputs();
    outputs.insert(outputs.end(), vesting.begin(), vesting.end());
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
