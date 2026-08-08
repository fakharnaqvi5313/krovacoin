/* Standalone genesis-block nonce grinder for the KrovaCoin (PIVX-lineage) fork.
 * Replicates CreateGenesisBlock()'s exact tx/header serialization independently
 * of the full build, so it can run immediately with only OpenSSL as a dependency.
 *
 * Usage: ./genesis_grinder <timestamp-string> <pubkey-hex> <nTime> <nBits-hex> <reward-satoshis>
 * Prints: nNonce, block hash (display order), merkle root (display order)
 *
 * Build: gcc -O2 -o genesis_grinder genesis_grinder.c -lcrypto
 *
 * CAUTION: this tool previously had a confirmed bug (fixed 2026-08-08: the
 * coinbase tx's nVersion was wrongly set to the block's nVersion argument
 * instead of the hardcoded 1 CreateGenesisBlock() actually uses, corrupting
 * the merkle root and making the nonce search meaningless). The merkle root
 * now matches a real build's output for the case that surfaced this, but the
 * nonce/hash still hasn't been proven to match bit-for-bit against the real
 * compiled CreateGenesisBlock()/GetHash() path in every case. Do not trust
 * this tool's output for a real launch (mainnet or public testnet) without
 * cross-checking against an actual daemon build first -- e.g. grind directly
 * in the CChainParams constructor (loop nNonce, compare against the target
 * decompressed from nBits via arith_uint256, NOT CheckProofOfWork() itself --
 * that calls Params() internally, which isn't valid yet mid-construction),
 * print the result, then hardcode it and remove the temporary loop. That's
 * the real code path, so it's authoritative by construction.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <openssl/sha.h>

static uint8_t *buf;
static size_t buf_len, buf_cap;

static void buf_reset(void) { buf_len = 0; }

static void buf_push(const uint8_t *data, size_t len) {
    if (buf_len + len > buf_cap) {
        buf_cap = (buf_len + len) * 2 + 64;
        buf = realloc(buf, buf_cap);
    }
    memcpy(buf + buf_len, data, len);
    buf_len += len;
}

static void push_u8(uint8_t v) { buf_push(&v, 1); }

static void push_u32le(uint32_t v) {
    uint8_t b[4] = { v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff, (v >> 24) & 0xff };
    buf_push(b, 4);
}

static void push_u64le(uint64_t v) {
    uint8_t b[8];
    for (int i = 0; i < 8; i++) b[i] = (v >> (8 * i)) & 0xff;
    buf_push(b, 8);
}

static void push_varint(uint64_t v) {
    if (v < 0xfd) {
        push_u8((uint8_t)v);
    } else if (v <= 0xffff) {
        push_u8(0xfd);
        push_u8(v & 0xff); push_u8((v >> 8) & 0xff);
    } else if (v <= 0xffffffffULL) {
        push_u8(0xfe);
        push_u32le((uint32_t)v);
    } else {
        push_u8(0xff);
        push_u64le(v);
    }
}

/* Standard Bitcoin-lineage script data push (minimal encoding). */
static void push_script_data(const uint8_t *data, size_t len) {
    if (len <= 75) {
        push_u8((uint8_t)len);
    } else if (len <= 255) {
        push_u8(0x4c); push_u8((uint8_t)len);
    } else if (len <= 65535) {
        push_u8(0x4d);
        push_u8(len & 0xff); push_u8((len >> 8) & 0xff);
    } else {
        fprintf(stderr, "push_script_data: too long\n"); exit(1);
    }
    buf_push(data, len);
}

/* Minimal little-endian CScriptNum byte encoding (no opcode shortcut), matching
 * CScriptNum::getvch(). Used for `<< CScriptNum(n)`, which CScript::operator<< always
 * pushes as plain data (length-prefix + bytes) -- unlike a bare `<< (int64_t)n`, which
 * goes through push_int64() and gets the OP_1NEGATE/OP_0/OP_1..OP_16 opcode shortcut. */
static void push_scriptnum_as_data(int64_t n) {
    if (n == 0) { push_script_data(NULL, 0); return; }
    uint8_t tmp[9];
    int len = 0;
    int neg = n < 0;
    uint64_t abs_n = neg ? (uint64_t)(-n) : (uint64_t)n;
    while (abs_n) {
        tmp[len++] = abs_n & 0xff;
        abs_n >>= 8;
    }
    if (tmp[len - 1] & 0x80) {
        tmp[len++] = neg ? 0x80 : 0x00;
    } else if (neg) {
        tmp[len - 1] |= 0x80;
    }
    push_script_data(tmp, len);
}

/* CScriptNum minimal encoding (little-endian, sign-extended if needed), matching
 * CScript::push_int64: OP_1NEGATE/OP_0/OP_1..OP_16 for -1 and 0..16, else minimal push.
 * This is for a bare `<< (int64_t)n` push (e.g. the literal 486604799 above), which IS
 * eligible for the opcode shortcut -- unlike `<< CScriptNum(n)`, see above. */
static void push_int64_script(int64_t n) {
    if (n == -1) { push_u8(0x4f); return; }               /* OP_1NEGATE */
    if (n == 0)  { push_u8(0x00); return; }                /* OP_0 */
    if (n >= 1 && n <= 16) { push_u8((uint8_t)(0x50 + n)); return; } /* OP_1..OP_16 */

    uint8_t tmp[9];
    int len = 0;
    int neg = n < 0;
    uint64_t abs_n = neg ? (uint64_t)(-n) : (uint64_t)n;
    while (abs_n) {
        tmp[len++] = abs_n & 0xff;
        abs_n >>= 8;
    }
    if (tmp[len - 1] & 0x80) {
        tmp[len++] = neg ? 0x80 : 0x00;
    } else if (neg) {
        tmp[len - 1] |= 0x80;
    }
    push_script_data(tmp, len);
}

static void sha256d(const uint8_t *data, size_t len, uint8_t out[32]) {
    uint8_t h1[32];
    SHA256(data, len, h1);
    SHA256(h1, 32, out);
}

int main(int argc, char **argv) {
    if (argc != 7 && argc != 8) {
        fprintf(stderr, "usage: %s <timestamp> <pubkey-hex> <nTime> <nBits-hex> <reward-sat> <nVersion> [forced-nonce]\n", argv[0]);
        return 1;
    }
    const char *pszTimestamp = argv[1];
    const char *pubkeyHex = argv[2];
    uint32_t nTime = (uint32_t)strtoul(argv[3], NULL, 10);
    uint32_t nBits = (uint32_t)strtoul(argv[4], NULL, 16);
    int64_t reward = strtoll(argv[5], NULL, 10);
    uint32_t nVersion = (uint32_t)strtoul(argv[6], NULL, 10);
    int have_forced_nonce = argc == 8;
    uint64_t forced_nonce = have_forced_nonce ? strtoull(argv[7], NULL, 10) : 0;
    /* GetHash() only uses standard SerializeHash (double-SHA256) for nVersion >= 4;
     * below that it uses HashQuark (a different algorithm entirely, not implemented here). */
    if (nVersion < 4) {
        fprintf(stderr, "error: nVersion < 4 uses HashQuark in this codebase, not double-SHA256 -- unsupported by this tool\n");
        return 1;
    }

    size_t pkLen = strlen(pubkeyHex) / 2;
    uint8_t *pubkey = malloc(pkLen);
    for (size_t i = 0; i < pkLen; i++) {
        sscanf(pubkeyHex + 2 * i, "%2hhx", &pubkey[i]);
    }

    /* --- Build coinbase tx --- */
    uint8_t zero32[32] = {0};

    /* Build scriptSig into its own buffer first. */
    uint8_t *saved_buf = buf; size_t saved_len = buf_len, saved_cap = buf_cap;
    buf = NULL; buf_len = 0; buf_cap = 0;
    push_int64_script(486604799);          /* plain int push: eligible for opcode shortcut */
    push_scriptnum_as_data(4);              /* CScriptNum(4) push: always plain data push */
    push_script_data((const uint8_t*)pszTimestamp, strlen(pszTimestamp));
    uint8_t *scriptSig = buf; size_t scriptSigLen = buf_len;
    buf = saved_buf; buf_len = saved_len; buf_cap = saved_cap;

    /* Build scriptPubKey = <pubkey> OP_CHECKSIG */
    uint8_t *saved_buf2 = buf; size_t saved_len2 = buf_len, saved_cap2 = buf_cap;
    buf = NULL; buf_len = 0; buf_cap = 0;
    push_script_data(pubkey, pkLen);
    push_u8(0xac); /* OP_CHECKSIG */
    uint8_t *scriptPubKey = buf; size_t scriptPubKeyLen = buf_len;
    buf = saved_buf2; buf_len = saved_len2; buf_cap = saved_cap2;

    /* Now assemble the full tx. CreateGenesisBlock() hardcodes the coinbase
     * tx's own nVersion to 1 regardless of the block's nVersion -- reusing
     * the block-level nVersion here (as an earlier version of this tool did)
     * silently produces a different txid/merkle root and searches for a
     * nonce against the wrong header entirely. */
    buf_reset();
    push_u32le(1);
    push_varint(1);                       /* vin count */
    buf_push(zero32, 32);                 /* prevout hash */
    push_u32le(0xFFFFFFFF);               /* prevout n */
    push_varint(scriptSigLen);
    buf_push(scriptSig, scriptSigLen);
    push_u32le(0xFFFFFFFF);               /* nSequence */
    push_varint(1);                       /* vout count */
    push_u64le((uint64_t)reward);
    push_varint(scriptPubKeyLen);
    buf_push(scriptPubKey, scriptPubKeyLen);
    push_u32le(0);                        /* nLockTime */

    uint8_t txid[32];
    sha256d(buf, buf_len, txid);
    /* merkle root of a single-tx block == that tx's hash (internal byte order) */
    uint8_t merkleRoot[32];
    memcpy(merkleRoot, txid, 32);

    /* --- Grind header --- */
    /* decompress nBits -> big-endian target[32] */
    uint8_t target_be[32] = {0};
    {
        uint32_t exponent = nBits >> 24;
        uint32_t mantissa = nBits & 0x00ffffff;
        if (exponent <= 32 && exponent >= 3) {
            int pos = 32 - (int)exponent;
            target_be[pos]     = (mantissa >> 16) & 0xff;
            target_be[pos + 1] = (mantissa >> 8) & 0xff;
            target_be[pos + 2] = mantissa & 0xff;
        }
    }

    uint8_t header[80];
    header[0] = nVersion & 0xff; header[1] = (nVersion>>8)&0xff; header[2]=(nVersion>>16)&0xff; header[3]=(nVersion>>24)&0xff;
    memset(header + 4, 0, 32); /* hashPrevBlock */
    memcpy(header + 36, merkleRoot, 32);
    header[68] = nTime & 0xff; header[69]=(nTime>>8)&0xff; header[70]=(nTime>>16)&0xff; header[71]=(nTime>>24)&0xff;
    header[72] = nBits & 0xff; header[73]=(nBits>>8)&0xff; header[74]=(nBits>>16)&0xff; header[75]=(nBits>>24)&0xff;

    uint64_t nonce;
    uint8_t hash[32], hash_be[32];
    if (have_forced_nonce) {
        nonce = forced_nonce;
        header[76] = nonce & 0xff; header[77]=(nonce>>8)&0xff; header[78]=(nonce>>16)&0xff; header[79]=(nonce>>24)&0xff;
        sha256d(header, 80, hash);
        for (int i = 0; i < 32; i++) hash_be[i] = hash[31 - i];
        int meets_target = memcmp(hash_be, target_be, 32) <= 0;
        fprintf(stderr, "forced nonce meets target: %s\n", meets_target ? "yes" : "no");
    } else {
        for (nonce = 0; nonce <= 0xFFFFFFFFULL; nonce++) {
            header[76] = nonce & 0xff; header[77]=(nonce>>8)&0xff; header[78]=(nonce>>16)&0xff; header[79]=(nonce>>24)&0xff;
            sha256d(header, 80, hash);
            for (int i = 0; i < 32; i++) hash_be[i] = hash[31 - i];
            if (memcmp(hash_be, target_be, 32) <= 0) break;
        }
        if (nonce > 0xFFFFFFFFULL) {
            fprintf(stderr, "No nonce found in uint32 range\n");
            return 1;
        }
    }

    printf("nTime=%u\n", nTime);
    printf("nNonce=%llu\n", (unsigned long long)nonce);
    printf("hash="); for (int i = 0; i < 32; i++) printf("%02x", hash_be[i]); printf("\n");
    printf("merkleRoot="); for (int i = 0; i < 32; i++) printf("%02x", merkleRoot[31 - i]); printf("\n");
    return 0;
}
