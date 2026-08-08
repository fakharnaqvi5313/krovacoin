// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2022 The KROVACOIN Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"

#include "chainparamsseeds.h"
#include "sapling/incrementalmerkletree.h"
#include "consensus/merkle.h"
#include "tinyformat.h"
#include "utilstrencodings.h"

#include <assert.h>

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.vtx.push_back(std::make_shared<const CTransaction>(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.nVersion = nVersion;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

void CChainParams::UpdateNetworkUpgradeParameters(Consensus::UpgradeIndex idx, int nActivationHeight)
{
    assert(IsRegTestNet()); // only available for regtest
    assert(idx > Consensus::BASE_NETWORK && idx < Consensus::MAX_NETWORK_UPGRADES);
    consensus.vUpgrades[idx].nActivationHeight = nActivationHeight;
}

void CChainParams::UpdateBudgetCycleBlocks(int nCycleBlocks)
{
    assert(IsRegTestNet()); // only available for regtest
    consensus.nBudgetCycleBlocks = nCycleBlocks;
}

/**
 * Build the genesis block. Note that the output of the genesis coinbase cannot
 * be spent as it did not originally exist in the database -- the real 72B KrovaCoin
 * premine lives in block 1, not genesis (see the block-1 premine consensus logic).
 *
 * TODO before any public testnet/mainnet launch: the timestamp strings below are
 * placeholders. Replace each with a real, independently verifiable August 2026
 * headline (the whole point of the genesis timestamp is proof the chain wasn't
 * created earlier) and re-grind the corresponding genesis block with
 * contrib/genesis_grinder before publishing chain parameters.
 */
static const char* GENESIS_PUBKEY_HEX = "04c10e83b2703ccf322f7dbd62dd5855ac7c10bd055814ce121ba32607d573b8810c02c0582aed05b4deb9c4b77b26d92428c61256cd42774babea0a073b2ed0c9";

static CBlock CreateGenesisBlock(const char* pszTimestamp, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const CScript genesisOutputScript = CScript() << ParseHex(GENESIS_PUBKEY_HEX) << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}


/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */
static MapCheckpoints mapCheckpoints = {
    { 259201, uint256S("1c9121bf9329a6234bfd1ea2d91515f19cd96990725265253f4b164283ade5dd")},
    { 424998, uint256S("f31e381eedb0ed3ed65fcc98cc71f36012bee32e8efd017c4f9fb0620fd35f6b")},
    { 616764, uint256S("29dd0bd1c59484f290896687b4ffb6a49afa5c498caf61967c69a541f8191557")}, //!< First block to use new modifierV1
    { 623933, uint256S("c7aafa648a0f1450157dc93bd4d7448913a85b7448f803b4ab970d91fc2a7da7")},
    { 791150, uint256S("8e76f462e4e82d1bd21cb72e1ce1567d4ddda2390f26074ffd1f5d9c270e5e50")},
    { 795000, uint256S("4423cceeb9fd574137a18733416275a70fdf95283cc79ad976ca399aa424a443")},
    { 863787, uint256S("5b2482eca24caf2a46bb22e0545db7b7037282733faa3a42ec20542509999a64")},
    { 863795, uint256S("2ad866818c4866e0d555181daccc628056216c0db431f88a825e84ed4f469067")},
    { 863805, uint256S("a755bd9a22b63c70d3db474f4b2b61a1f86c835b290a081bb3ec1ba2103eb4cb")},
    { 867733, uint256S("03b26296bf693de5782c76843d2fb649cb66d4b05550c6a79c047ff7e1c3ae15")},
    { 879650, uint256S("227e1d2b738b6cd83c46d1d64617934ec899d77cee34336a56e61b71acd10bb2")},
    { 895400, uint256S("7796a0274a608fac12d400198174e50beda992c1d522e52e5b95b884bc1beac6")}, //!< Block that serial# range is enforced
    { 895991, uint256S("d53013ed7ea5c325b9696c95e07667d6858f8ff7ee13fecfa90827bf3c9ae316")}, //!< Network split here
    { 908000, uint256S("202708f8c289b676fceb832a079ff6b308a28608339acbf7584de533619d014d")},
    {1142400, uint256S("98aff9d605bf123247f98b1e3a02567eb5799d208d78ec30fb89737b1c1f79c5")},
    {1679090, uint256S("f747ce055ba1b12e1f2e842bd480bc647210799359cb2e553ab292065e3419d6")}, //!< First block with a "wrapped" serial spend
    {1686229, uint256S("bb42bf1e886a7c23474634c90893dd3d68a6ccbfea4ac92a98da5cad0c6a6cb7")}, //!< Last block in the "wrapped" serial attack range
    {1778954, uint256S("0d3241268264a2908d6babf00d9cd1ffb83d93d7bb4e428820127fe227c2029c")}, //!< Network split here
    {1788528, uint256S("ea9243ff8fc079fdd7a04f11fac415de4d98e1bb0dc38db6f79f8f8bbfdbe496")}, //!< Network split here
    {2153200, uint256S("14e477e597d24549cac5e59d97d32155e6ec2861c1003b42d0566f9bf39b65d5")}, //!< First v7 block
    {2356049, uint256S("62e80d8e193bca84655fb78893b20f54a79f2d71124c4ea37b7ef51a0d5451c4")}, //!< Network split here
    {2365700, uint256S("b5d0beead57735539abc2db2b0b08cd65db3e5928efd3c3bf3182d5bf013f36c")}, //!< KROVACOIN v4.1.1 enforced
    {2678402, uint256S("580a26ff0a45177a7a6f387f009c5b26140ea48b4790a857d9a796f8b3c25899")}, //!< Network split here
    {3014000, uint256S("78ad99b7225f73c42238bd7ca841ff700542b92bba75a0ef2ed351caa560f87f")}, //!< KROVACOIN v5.3.0 enforced
    {3024000, uint256S("be4bc75afcfb9136924810f7483b2695089a366cc4ee27fd6dc3ecd5396e1f0f")}, //!< Superblock
    {3715200, uint256S("a676b9a598c393c82b949c37dd35013aeda55f5d18ab062349db6a8235972aaa")}, //!< Superblock for 5.5.0 mainnet rewards changeover
};

static const CCheckpointData data = {
    &mapCheckpoints,
    1591401645, // * UNIX timestamp of last checkpoint block
    5607713,    // * total number of transactions between genesis and last checkpoint
                //   (the tx=... number in the UpdateTip debug.log lines)
    3000        // * estimated number of transactions per day after checkpoint
};

static MapCheckpoints mapCheckpointsTestnet = {
    {0, uint256S("0x001")},
    //{    201, uint256S("6ae7d52092fd918c8ac8d9b1334400387d3057997e6e927a88e57186dc395231")},     // v5 activation (PoS/Sapling)
};

static const CCheckpointData dataTestnet = {
    &mapCheckpointsTestnet,
    1454124731,
    0,
    3000};

static MapCheckpoints mapCheckpointsRegtest = {{0, uint256S("0x001")}};
static const CCheckpointData dataRegtest = {
    &mapCheckpointsRegtest,
    1454124731,
    0,
    100};

class CMainParams : public CChainParams
{
public:
    CMainParams()
    {
        strNetworkID = "main";

        // TODO: placeholder timestamp -- replace with a real Aug 2026 headline and
        // re-grind before any public launch (see GENESIS_PUBKEY_HEX comment above).
        genesis = CreateGenesisBlock("KrovaCoin genesis - TODO insert verifiable Aug 2026 headline before public launch (see launch plan)",
                                      1785931200, 231101, 0x1e0ffff0, 8, 0);
        genesis.hashFinalSaplingRoot = uint256S("0x3e49b5f954aa9d3545bc6c37744661eea48d7c34e3000d82b7f0010c30f4c2fb");
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x000001c2f0bad388f63e3b81b05dce775b458cb99841abd82e91bba56ac261b4"));
        assert(genesis.hashMerkleRoot == uint256S("0xb26beffd3e4735aa77e7ce94a9ff49902ad46b39a7e0f0ab50b13787a4381008"));

        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.powLimit   = uint256S("0x00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.posLimitV1 = uint256S("0x000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.posLimitV2 = uint256S("0x00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nBudgetCycleBlocks = 1440;        // Staking Rewards Pool superblock cycle: ~daily at 60s blocks (task 7)
        consensus.nCoinbaseMaturity = 100;
        consensus.nFutureTimeDriftPoW = 7200;
        consensus.nFutureTimeDriftPoS = 180;
        consensus.nMaxMoneyOut = 72000000000LL * COIN;
        consensus.nStakeMinAge = 60 * 60;
        consensus.nStakeMinDepth = 600;
        consensus.nTargetTimespan = 40 * 60;
        consensus.nTargetTimespanV2 = 30 * 60;
        consensus.nTargetSpacing = 1 * 60;
        consensus.nTimeSlotLength = 15;

        // spork keys
        consensus.strSporkPubKey = "0410050aa740d280b134b40b40658781fc1116ba7700764e0ce27af3e1737586b3257d19232e0cb5084947f5107e44bcd577f126c9eb4a30ea2807b271d2145298";
        consensus.strSporkPubKeyOld = "040F129DE6546FE405995329A887329BED4321325B1A73B0A257423C05C1FCFE9E40EF0678AEF59036A22C42E61DFD29DF7EFB09F56CC73CADF64E05741880E3E7";
        consensus.nTime_EnforceNewSporkKey = 1608512400;    //!> December 21, 2020 01:00:00 AM GMT
        consensus.nTime_RejectOldSporkKey = 1614560400;     //!> March 1, 2021 01:00:00 AM GMT

        // height-based activations
        consensus.height_last_invalid_UTXO = 894538;

        // validation by-pass
        consensus.nKrovacoinBadBlockTime = 1471401614;    // Skip nBit validation of Block 259201 per PR #915
        consensus.nKrovacoinBadBlockBits = 0x1c056dac;    // Skip nBit validation of Block 259201 per PR #915

        // Network upgrades — fresh chain, no history to replay: every current PIVX protocol
        // improvement is active from genesis EXCEPT Zerocoin, which we never activate (stripped
        // per KrovaCoin_Launch_Plan.md's audit-surface reduction goal; see task 10).
        // hashActivationBlock guards are removed below since they assert against PIVX's real
        // historical chain, which is meaningless for our independent genesis.
        consensus.vUpgrades[Consensus::BASE_NETWORK].nActivationHeight =
                Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_TESTDUMMY].nActivationHeight =
                Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;
        // Blocks 1-600 stay PoW (block-1 premine + maturity window for nStakeMinDepth=600); PoS from 601.
        consensus.vUpgrades[Consensus::UPGRADE_POS].nActivationHeight           = 601;
        consensus.vUpgrades[Consensus::UPGRADE_POS_V2].nActivationHeight        = 601;
        consensus.vUpgrades[Consensus::UPGRADE_ZC].nActivationHeight            = Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;
        consensus.vUpgrades[Consensus::UPGRADE_ZC_V2].nActivationHeight         = Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;
        consensus.vUpgrades[Consensus::UPGRADE_BIP65].nActivationHeight         = Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_ZC_PUBLIC].nActivationHeight     = Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;
        consensus.vUpgrades[Consensus::UPGRADE_V3_4].nActivationHeight          = 601; // must not precede UPGRADE_POS: AddToBlockIndex assumes vtx[1] is a coinstake once active
        consensus.vUpgrades[Consensus::UPGRADE_V4_0].nActivationHeight          = Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_V5_0].nActivationHeight          = Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_V5_2].nActivationHeight          = Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_V5_3].nActivationHeight          = Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_V5_5].nActivationHeight          = Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_V5_6].nActivationHeight          = Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_V6_0].nActivationHeight =
                Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 4-byte int at any alignment.
         */
        // Network magic: 'K','R','O','V'
        pchMessageStart[0] = 0x4b;
        pchMessageStart[1] = 0x52;
        pchMessageStart[2] = 0x4f;
        pchMessageStart[3] = 0x56;
        nDefaultPort = 42872;

        // TODO: stand these up before mainnet launch (see KrovaCoin_Launch_Plan.md Phase 4/7)
        vSeeds.emplace_back("seed1.krovacoin.com", true);
        vSeeds.emplace_back("seed2.krovacoin.com", true);
        vSeeds.emplace_back("seed3.krovacoin.com", true);
        vSeeds.emplace_back("dnsseed.krovacoin.com", true);

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 45);   // 'K...'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 105);  // 'k...'
        base58Prefixes[STAKING_ADDRESS] = std::vector<unsigned char>(1, 63);     // starting with 'S'
        base58Prefixes[EXCHANGE_ADDRESS] = {0x01, 0xb9, 0xa2};   // starts with EXM
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 173);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x02, 0x2D, 0x25, 0x33};
        base58Prefixes[EXT_SECRET_KEY] = {0x02, 0x21, 0x31, 0x2B};
        // TODO: coin type is a placeholder pending the real SLIP-0044 PR (see launch plan §3.2)
        base58Prefixes[EXT_COIN_TYPE] = {0x80, 0x00, 0x03, 0x39};

        vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_main), std::end(chainparams_seed_main));

        // Reject non-standard transactions by default
        fRequireStandard = true;

        // Sapling
        bech32HRPs[SAPLING_PAYMENT_ADDRESS]      = "ps";
        bech32HRPs[SAPLING_FULL_VIEWING_KEY]     = "pviews";
        bech32HRPs[SAPLING_INCOMING_VIEWING_KEY] = "pivks";
        bech32HRPs[SAPLING_EXTENDED_SPEND_KEY]   = "p-secret-spending-key-main";
        bech32HRPs[SAPLING_EXTENDED_FVK]         = "pxviews";

        bech32HRPs[BLS_SECRET_KEY]               = "bls-sk";
        bech32HRPs[BLS_PUBLIC_KEY]               = "bls-pk";

        // long living quorum params

        // Tier two
        nFulfilledRequestExpireTime = 60 * 60; // fulfilled requests expire in 1 hour
    }

    const CCheckpointData& Checkpoints() const
    {
        return data;
    }

};

/**
 * Testnet (v5)
 */
class CTestNetParams : public CChainParams
{
public:
    CTestNetParams()
    {
        strNetworkID = "test";

        // Genesis timestamp is a real, dated, independently-verifiable headline
        // (NPR, 06/Aug/2026), the same anti-premine-backdating convention Bitcoin's
        // own genesis block uses -- proves this chain wasn't started before the
        // date shown. Re-grind (see contrib/devtools/genesis_grinder.c) only if
        // the public testnet needs to be reset from scratch.
        genesis = CreateGenesisBlock("NPR 06/Aug/2026 Trump signs new orders targeting birthright citizenship, weeks after Supreme Court ruling",
                                      1786170027, 987529, 0x1e0ffff0, 8, 0);
        genesis.hashFinalSaplingRoot = uint256S("0x3e49b5f954aa9d3545bc6c37744661eea48d7c34e3000d82b7f0010c30f4c2fb");
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x00000ef56932bb65700cb706c8eef59a39c3f279cc6062cdf7fc4a3b0d533f38"));
        assert(genesis.hashMerkleRoot == uint256S("0x58bf5810460b66efe8d63401963dfe68d2ddbe6e1e9ba05ec0bcd6db19a9e606"));

        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.powLimit   = uint256S("0x00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.posLimitV1 = uint256S("0x000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.posLimitV2 = uint256S("0x00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nBudgetCycleBlocks = 50;          // shortened for testnet iteration speed (task 7 superblock cycle)
        consensus.nCoinbaseMaturity = 15;
        consensus.nFutureTimeDriftPoW = 7200;
        consensus.nFutureTimeDriftPoS = 180;
        consensus.nMaxMoneyOut = 72000000000LL * COIN;
        consensus.nStakeMinAge = 60 * 60;
        consensus.nStakeMinDepth = 100;
        consensus.nTargetTimespan = 40 * 60;
        consensus.nTargetTimespanV2 = 30 * 60;
        consensus.nTargetSpacing = 1 * 60;
        consensus.nTimeSlotLength = 15;

        // spork keys
        consensus.strSporkPubKey = "04677c34726c491117265f4b1c83cef085684f36c8df5a97a3a42fc499316d0c4e63959c9eca0dba239d9aaaf72011afffeb3ef9f51b9017811dec686e412eb504";
        consensus.strSporkPubKeyOld = "04E88BB455E2A04E65FCC41D88CD367E9CCE1F5A409BE94D8C2B4B35D223DED9C8E2F4E061349BA3A38839282508066B6DC4DB72DD432AC4067991E6BF20176127";
        consensus.nTime_EnforceNewSporkKey = 1608512400;    //!> December 21, 2020 01:00:00 AM GMT
        consensus.nTime_RejectOldSporkKey = 1614560400;     //!> March 1, 2021 01:00:00 AM GMT

        // height based activations
        consensus.height_last_invalid_UTXO = -1;

        // Network upgrades — same rationale as mainnet: fresh chain, everything active from
        // genesis except Zerocoin (never activated, stripped per launch plan task 10).
        consensus.vUpgrades[Consensus::BASE_NETWORK].nActivationHeight =
                Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_TESTDUMMY].nActivationHeight =
                Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;
        // Blocks 1-100 stay PoW (block-1 premine + maturity window for nStakeMinDepth=100); PoS from 101.
        consensus.vUpgrades[Consensus::UPGRADE_POS].nActivationHeight           = 101;
        consensus.vUpgrades[Consensus::UPGRADE_POS_V2].nActivationHeight        = 101;
        consensus.vUpgrades[Consensus::UPGRADE_ZC].nActivationHeight            = Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;
        consensus.vUpgrades[Consensus::UPGRADE_ZC_V2].nActivationHeight         = Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;
        consensus.vUpgrades[Consensus::UPGRADE_BIP65].nActivationHeight         = Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_ZC_PUBLIC].nActivationHeight     = Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;
        consensus.vUpgrades[Consensus::UPGRADE_V3_4].nActivationHeight          = 101; // must not precede UPGRADE_POS: AddToBlockIndex assumes vtx[1] is a coinstake once active
        consensus.vUpgrades[Consensus::UPGRADE_V4_0].nActivationHeight          = Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_V5_0].nActivationHeight          = Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_V5_2].nActivationHeight          = Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_V5_3].nActivationHeight          = Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_V5_5].nActivationHeight          = Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_V5_6].nActivationHeight          = Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_V6_0].nActivationHeight =
                Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 4-byte int at any alignment.
         */
        pchMessageStart[0] = 0x74;
        pchMessageStart[1] = 0x6b;
        pchMessageStart[2] = 0x72;
        pchMessageStart[3] = 0x76;
        nDefaultPort = 52872;

        // TODO: stand these up before public testnet (see KrovaCoin_Launch_Plan.md Phase 2)
        vSeeds.emplace_back("testnet-seed1.krovacoin.com", true);
        vSeeds.emplace_back("testnet-seed2.krovacoin.com", true);

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 139); // Testnet krovacoin addresses start with 'x' or 'y'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 19);  // Testnet krovacoin script addresses start with '8' or '9'
        base58Prefixes[STAKING_ADDRESS] = std::vector<unsigned char>(1, 73);     // starting with 'W'
        base58Prefixes[EXCHANGE_ADDRESS] = {0x01, 0xb9, 0xb1};   // EXT prefix for the address
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 239);     // Testnet private keys start with '9' or 'c' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = {0x3a, 0x80, 0x61, 0xa0};
        base58Prefixes[EXT_SECRET_KEY] = {0x3a, 0x80, 0x58, 0x37};
        // Testnet BIP44 coin type is '1' (all coins' testnet default)
        base58Prefixes[EXT_COIN_TYPE] = {0x80, 0x00, 0x00, 0x01};

        vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_test), std::end(chainparams_seed_test));

        fRequireStandard = false;

        // Sapling
        bech32HRPs[SAPLING_PAYMENT_ADDRESS]      = "ptestsapling";
        bech32HRPs[SAPLING_FULL_VIEWING_KEY]     = "pviewtestsapling";
        bech32HRPs[SAPLING_INCOMING_VIEWING_KEY] = "pivktestsapling";
        bech32HRPs[SAPLING_EXTENDED_SPEND_KEY]   = "p-secret-spending-key-test";
        bech32HRPs[SAPLING_EXTENDED_FVK]         = "pxviewtestsapling";

        bech32HRPs[BLS_SECRET_KEY]               = "bls-sk-test";
        bech32HRPs[BLS_PUBLIC_KEY]               = "bls-pk-test";

        // long living quorum params

        // Tier two
        nFulfilledRequestExpireTime = 60 * 60; // fulfilled requests expire in 1 hour
    }

    const CCheckpointData& Checkpoints() const
    {
        return dataTestnet;
    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams
{
public:
    CRegTestParams()
    {
        strNetworkID = "regtest";

        genesis = CreateGenesisBlock("KrovaCoin regtest genesis", 1785920000, 2, 0x207fffff, 8, 0);
        genesis.hashFinalSaplingRoot = uint256S("0x3e49b5f954aa9d3545bc6c37744661eea48d7c34e3000d82b7f0010c30f4c2fb");
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x266d0526e0cf1f45c2783092808755f26cabb7bca1aa90835810cb8cd4cf585c"));
        assert(genesis.hashMerkleRoot == uint256S("0xa2fe36fb9bd6393bc2ab6c66aa108388497abacb1f53bad6d16d7691e8a3a56f"));

        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.powLimit   = uint256S("0x7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.posLimitV1 = uint256S("0x000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.posLimitV2 = uint256S("0x00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nBudgetCycleBlocks = 10;          // shortened for fast regtest iteration (task 7 superblock cycle)
        consensus.nCoinbaseMaturity = 2;
        consensus.nFutureTimeDriftPoW = 7200;
        consensus.nFutureTimeDriftPoS = 180;
        consensus.nMaxMoneyOut = 72000000000LL * COIN;
        consensus.nStakeMinAge = 0;
        consensus.nStakeMinDepth = 3;
        consensus.nTargetTimespan = 40 * 60;
        consensus.nTargetTimespanV2 = 30 * 60;
        consensus.nTargetSpacing = 1 * 60;
        consensus.nTimeSlotLength = 15;

        /* Spork Key for RegTest:
        WIF private key: 932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi
        private key hex: bd4960dcbd9e7f2223f24e7164ecb6f1fe96fc3a416f5d3a830ba5720c84b8ca
        Address: yCvUVd72w7xpimf981m114FSFbmAmne7j9
        */
        consensus.strSporkPubKey = "043969b1b0e6f327de37f297a015d37e2235eaaeeb3933deecd8162c075cee0207b13537618bde640879606001a8136091c62ec272dd0133424a178704e6e75bb7";
        consensus.strSporkPubKeyOld = "";
        consensus.nTime_EnforceNewSporkKey = 0;
        consensus.nTime_RejectOldSporkKey = 0;

        // height based activations
        consensus.height_last_invalid_UTXO = -1;

        // Network upgrades — default to ALWAYS_ACTIVE (except Zerocoin, disabled) like main/test;
        // individual tests can still override via UpdateNetworkUpgradeParameters() to exercise
        // pre/post-activation behavior.
        consensus.vUpgrades[Consensus::BASE_NETWORK].nActivationHeight =
                Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_TESTDUMMY].nActivationHeight =
                Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;
        // Blocks 1-3 stay PoW (block-1 premine + coinbase-maturity/stake-depth window); PoS from 4.
        consensus.vUpgrades[Consensus::UPGRADE_POS].nActivationHeight           = 4; // max(nCoinbaseMaturity=2, nStakeMinDepth=3)+1, shortened for regtest superblock testing
        consensus.vUpgrades[Consensus::UPGRADE_POS_V2].nActivationHeight        = 4;
        consensus.vUpgrades[Consensus::UPGRADE_ZC].nActivationHeight            = Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;
        consensus.vUpgrades[Consensus::UPGRADE_ZC_V2].nActivationHeight         = Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;
        consensus.vUpgrades[Consensus::UPGRADE_BIP65].nActivationHeight         =
                Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_ZC_PUBLIC].nActivationHeight     = Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;
        consensus.vUpgrades[Consensus::UPGRADE_V3_4].nActivationHeight          = 4;
        consensus.vUpgrades[Consensus::UPGRADE_V4_0].nActivationHeight          =
                Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_V5_0].nActivationHeight          = Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_V5_2].nActivationHeight          = Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_V5_3].nActivationHeight          = Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_V5_5].nActivationHeight          = Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_V5_6].nActivationHeight          = Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_V6_0].nActivationHeight =
                Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 4-byte int at any alignment.
         */
        pchMessageStart[0] = 0x72;
        pchMessageStart[1] = 0x65;
        pchMessageStart[2] = 0x67;
        pchMessageStart[3] = 0x74;
        nDefaultPort = 42876;

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 139); // Testnet krovacoin addresses start with 'x' or 'y'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 19);  // Testnet krovacoin script addresses start with '8' or '9'
        base58Prefixes[STAKING_ADDRESS] = std::vector<unsigned char>(1, 73);     // starting with 'W'
        base58Prefixes[EXCHANGE_ADDRESS] = {0x01, 0xb9, 0xb1};   // EXT prefix for the address
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 239);     // Testnet private keys start with '9' or 'c' (Bitcoin defaults)
        // Testnet krovacoin BIP32 pubkeys start with 'DRKV'
        base58Prefixes[EXT_PUBLIC_KEY] = {0x3a, 0x80, 0x61, 0xa0};
        // Testnet krovacoin BIP32 prvkeys start with 'DRKP'
        base58Prefixes[EXT_SECRET_KEY] = {0x3a, 0x80, 0x58, 0x37};
        // Testnet krovacoin BIP44 coin type is '1' (All coin's testnet default)
        base58Prefixes[EXT_COIN_TYPE] = {0x80, 0x00, 0x00, 0x01};

        // Reject non-standard transactions by default
        fRequireStandard = true;

        // Sapling
        bech32HRPs[SAPLING_PAYMENT_ADDRESS]      = "ptestsapling";
        bech32HRPs[SAPLING_FULL_VIEWING_KEY]     = "pviewtestsapling";
        bech32HRPs[SAPLING_INCOMING_VIEWING_KEY] = "pivktestsapling";
        bech32HRPs[SAPLING_EXTENDED_SPEND_KEY]   = "p-secret-spending-key-test";
        bech32HRPs[SAPLING_EXTENDED_FVK]         = "pxviewtestsapling";

        bech32HRPs[BLS_SECRET_KEY]               = "bls-sk-test";
        bech32HRPs[BLS_PUBLIC_KEY]               = "bls-pk-test";

        // long living quorum params

        // Tier two
        nFulfilledRequestExpireTime = 60 * 60; // fulfilled requests expire in 1 hour
    }

    const CCheckpointData& Checkpoints() const
    {
        return dataRegtest;
    }
};

static std::unique_ptr<CChainParams> globalChainParams;

const CChainParams &Params()
{
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams());
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}

void UpdateNetworkUpgradeParameters(Consensus::UpgradeIndex idx, int nActivationHeight)
{
    globalChainParams->UpdateNetworkUpgradeParameters(idx, nActivationHeight);
}

void UpdateBudgetCycleBlocks(int nCycleBlocks)
{
    globalChainParams->UpdateBudgetCycleBlocks(nCycleBlocks);
}
