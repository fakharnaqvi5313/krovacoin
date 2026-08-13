# Testnet / regtest quickstart

## Status

**There is no live, joinable KrovaCoin testnet yet.** Testnet chainparams (`src/chainparams.cpp`,
`CTestNetParams`) are complete and genesis is grinded and fixed (see below), but the two DNS
seeds testnet nodes bootstrap from -- `testnet-seed1.krovacoin.com`, `testnet-seed2.krovacoin.com`
-- don't resolve to anything real yet (still an explicit `// TODO: stand these up before public
testnet` in the source). Standing up real seed nodes is covered by the `krovacoin-infra` repo
(`ansible/roles/dnsseed`, `ansible/roles/krovad_node`, `terraform/contabo`) -- written and
validated, not yet applied to real infrastructure. Until that happens, `-testnet` runs a
single-node chain with no peers to sync from, no different from regtest except for its fixed
chainparams and genesis. This doc's regtest section below works today; the sections after it
describe what running `-testnet` will look like once seed nodes are live.

## Testnet network parameters

| | |
|---|---|
| P2P port | 52872 |
| RPC port | 52873 |
| Genesis hash | `00000ef56932bb65700cb706c8eef59a39c3f279cc6062cdf7fc4a3b0d533f38` |
| Address prefix | `x`/`y...` (pubkey-hash, base58 prefix 139) |
| Cold-staking prefix | `W...` (base58 prefix 73) |
| PoS activation | height 101 (blocks 1-100 are PoW, matching `nStakeMinDepth=100`) |
| Staking Rewards Pool superblock cycle | every 50 blocks (`nBudgetCycleBlocks`, shortened from mainnet's 1440 for faster iteration) |
| DNS seeds | `testnet-seed1.krovacoin.com`, `testnet-seed2.krovacoin.com` (not live yet) |

## Premine test key

On every non-mainnet network (testnet, regtest), `GetEffectiveScriptPubKeyHex()`
(`src/consensus/premine.cpp`) redirects every premine allocation's coinbase output to a single,
publicly-known test keypair, instead of the real (still-placeholder, pending key ceremony)
mainnet addresses. This makes non-mainnet chains actually spendable/testable without needing
real key material.

**Do not use this key for anything real.** It is intentionally public.

```
Private key (WIF, non-mainnet): cPpxACHhg7cHUTSEjx2J7Le8YNmnnirJC3CGNxf1Zepf5nPYXn9E
Address (testnet/regtest):      yANqJzM49zNofkFM9V8KmptrR2jjvSfhz4
scriptPubKey (hex):              76a91492f2bd80a027f45a50c1a0b44b0754e5e976238288ac
```

Import it into a node's wallet to spend premine funds on any non-mainnet chain:

```bash
src/krova-cli -regtest -datadir=<datadir> importprivkey cPpxACHhg7cHUTSEjx2J7Le8YNmnnirJC3CGNxf1Zepf5nPYXn9E
src/krova-cli -regtest -datadir=<datadir> getbalance
```

## Regtest quickstart

```bash
./autogen.sh && ./configure && make -j$(nproc)
src/krovad -regtest -daemon -datadir=/tmp/krova-regtest
src/krova-cli -regtest -datadir=/tmp/krova-regtest importprivkey cPpxACHhg7cHUTSEjx2J7Le8YNmnnirJC3CGNxf1Zepf5nPYXn9E
src/krova-cli -regtest -datadir=/tmp/krova-regtest getblockchaininfo
src/krova-cli -regtest -datadir=/tmp/krova-regtest getbalance
```

Regtest is pure PoS (no PoW fallback) — new blocks only come from staking, which needs an
existing balance. `importprivkey` on a fresh chain immediately gives the node's wallet the
premine funds (see above), which is enough balance to start staking from.

## Running `-testnet` today (standalone, pre-seed-nodes)

Unlike regtest, testnet has blocks 1-100 as PoW (`nStakeMinDepth=100` needs the premine coinbase
to actually mature before the pool/vesting outputs can be spent), so `-gen=1` is needed to get
the chain started before staking can take over:

```bash
src/krovad -testnet -daemon -datadir=/tmp/krova-testnet -gen=1
src/krova-cli -testnet -datadir=/tmp/krova-testnet importprivkey cPpxACHhg7cHUTSEjx2J7Le8YNmnnirJC3CGNxf1Zepf5nPYXn9E
src/krova-cli -testnet -datadir=/tmp/krova-testnet getblockchaininfo
```

With no seed nodes live, this is a single-node chain with no peers — useful for exercising the
real testnet genesis/params/PoW-to-PoS transition end to end, not for anything multi-node.

CPU-mining those first 100 PoW blocks with a single-threaded `-gen=1` can take a while — genesis
itself needed a dedicated external grinder tool (`contrib/devtools/genesis_grinder.c`) rather than
the built-in miner, which is a sign the built-in miner isn't fast. `-genproclimit=<n>` raises the
thread count if it's taking too long.

## Joining the public testnet (once seed nodes are live)

Once `krovacoin-infra`'s `dnsseed` and `krovad_node` roles have been applied to real
infrastructure and `testnet-seed1/2.krovacoin.com` resolve, joining is just:

```bash
src/krovad -testnet -daemon -datadir=<datadir>
src/krova-cli -testnet -datadir=<datadir> getblockchaininfo   # confirm the node is syncing (blocks climbing, peer count > 0)
```

`-testnet` picks up the DNS seeds automatically (`vSeeds` in `CTestNetParams`, `src/chainparams.cpp`)
-- no `-connect`/`-addnode` needed. If the DNS seeds aren't resolving yet but a specific seed
node's IP is known, `-addnode=<ip>:52872` connects directly as a stopgap.
