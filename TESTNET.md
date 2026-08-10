# Testnet / regtest quickstart

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
