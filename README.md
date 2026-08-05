KROVACOIN Core integration/staging repository
=====================================

[![master Actions Status](https://github.com/KROVACOIN-Project/KROVACOIN/workflows/CI%20Actions%20for%20KROVACOIN/badge.svg)](https://github.com/KROVACOIN-Project/KROVACOIN/actions)
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/KROVACOIN-Project/krovacoin?color=%235c4b7d&cacheSeconds=3600)](https://github.com/KROVACOIN-Project/KROVACOIN/releases)
[![GitHub Release Date](https://img.shields.io/github/release-date/KROVACOIN-Project/krovacoin?color=%235c4b7d&cacheSeconds=3600)](https://github.com/KROVACOIN-Project/KROVACOIN/releases)

## What is KROVACOIN?

KROVACOIN is an open source community-driven cryptocurrency, focused on five main aspects:

(1) User Data Protection: Through the use of SHIELD, a zk-SNARKs based privacy protocol.

(2) Low environmental footprint and network participation equality: Through the use of a highly developed Proof of Stake protocol.

(3) Decentralized Governance System: A DAO built on top of the tier two Masternodes network, enabling a monthly community treasury, proposals submission and decentralized voting.

(4) Fast Transactions: Through the use of fast block times and the tier two network, KROVACOIN is committed to continue researching new and better instant transactions mechanisms.

(5) Ease of Use: KROVACOIN is determined to offer the best possible graphical interface for a core node/wallet. A full featured graphical product for new and advanced users.

A lot more information and specs at [KROVACOIN.org](https://www.krovacoin.org/). Join the community at [KROVACOIN Discord](https://discordapp.com/invite/jzqVsJd).

## License
KROVACOIN Core is released under the terms of the MIT license. See [COPYING](https://github.com/KROVACOIN-Project/KROVACOIN/blob/master/COPYING) for more information or see https://opensource.org/licenses/MIT.

## Development Process

The master branch is regularly built (see doc/build-*.md for instructions) and tested, but it is not guaranteed to be completely stable. [Tags](https://github.com/KROVACOIN-Project/KROVACOIN/tags) are created regularly from release branches to indicate new official, stable release versions of KROVACOIN Core.

The contribution workflow is described in [CONTRIBUTING.md](https://github.com/KROVACOIN-Project/KROVACOIN/blob/master/CONTRIBUTING.md) and useful hints for developers can be found in [doc/developer-notes.md](https://github.com/KROVACOIN-Project/KROVACOIN/blob/master/doc/developer-notes.md).

## Testing

Testing and code review is the bottleneck for development; we get more pull requests than we can review and test on short notice. Please be patient and help out by testing other people's pull requests, and remember this is a security-critical project where any mistake might cost people a lot of money.

## Automated Testing

Developers are strongly encouraged to write [unit tests](https://github.com/KROVACOIN-Project/KROVACOIN/blob/master/src/test/README.md) for new code, and to submit new unit tests for old code. Unit tests can be compiled and run (assuming they weren't disabled in configure) with: make check. Further details on running and extending unit tests can be found in [/src/test/README.md](https://github.com/KROVACOIN-Project/KROVACOIN/blob/master/src/test/README.md).

There are also regression and integration tests, written in Python. These tests can be run (if the test dependencies are installed) with: test/functional/test_runner.py`

The CI (Continuous Integration) systems make sure that every pull request is built for Windows, Linux, and macOS, and that unit/sanity tests are run automatically.

## Regtest Quickstart

This walks through building the daemon and exercising KrovaCoin's core
consensus rules (the 72B premine, staking, the Staking Rewards Pool superblock
payout, team vesting, cold staking) on `-regtest`, a private local chain with
adjustable difficulty and no need for a peer network.

### Build and run the unit tests

```bash
./autogen.sh
./configure
make -j$(nproc)
make check
```

`make check` runs the full `test/test_krova` Boost suite, including
[src/test/premine_tests.cpp](src/test/premine_tests.cpp) (the 72B allocation
table and block-1 coinbase structure), [src/test/vesting_tests.cpp](src/test/vesting_tests.cpp)
(the 36 CLTV-locked team tranches), [src/test/superblock_tests.cpp](src/test/superblock_tests.cpp)
(the Staking Rewards Pool's 20-year payout schedule), [src/test/coldstaking_tests.cpp](src/test/coldstaking_tests.cpp)
(cold-stake free-output consensus rules), and [src/test/genesis_tests.cpp](src/test/genesis_tests.cpp)
(genesis block validity on all three networks). To run just one suite:

```bash
src/test/test_krova --run_test=premine_tests
```

### Start a regtest node

```bash
src/krovad -regtest -daemon -datadir=/tmp/krova-regtest
src/krova-cli -regtest -datadir=/tmp/krova-regtest getblockchaininfo
```

By default regtest listens for RPC on port 42876 and stores its data under
whatever `-datadir` you pass (nothing is written to `~/.krovacoin` unless you
omit it). Stop the node when you're done with:

```bash
src/krova-cli -regtest -datadir=/tmp/krova-regtest stop
```

### Bootstrap the chain and inspect the premine

Block 1's coinbase carries the entire 72,000,000,000 KROV genesis allocation
(genesis's own coinbase is unspendable by design, see [src/consensus/premine.h](src/consensus/premine.h)).
Regtest requires a short proof-of-work bootstrap before Proof-of-Stake takes
over at height 4 (`UPGRADE_POS`, see `nActivationHeight` in the regtest block
in [src/chainparams.cpp](src/chainparams.cpp)):

```bash
ADDR=$(src/krova-cli -regtest -datadir=/tmp/krova-regtest getnewaddress)
src/krova-cli -regtest -datadir=/tmp/krova-regtest generatetoaddress 10 $ADDR
src/krova-cli -regtest -datadir=/tmp/krova-regtest getblock $(src/krova-cli -regtest -datadir=/tmp/krova-regtest getblockhash 1)
```

The block-1 output shows 41 `vout` entries: 5 plain allocations (Staking
Rewards Pool, Ecosystem Treasury, Community, Public Sale Liquidity, Strategic
Reserve) followed by 36 CLTV-locked team vesting tranches.

### Staking Rewards Pool superblock payouts

On regtest, `nBudgetCycleBlocks = 10` (repurposed from PIVX's budget cycle for
the superblock schedule -- see [src/consensus/superblock.h](src/consensus/superblock.h)),
so every 10th block after PoS activation is a superblock. Once you're staking
(see below) or have generated far enough, `getblock` on a superblock height
will show an extra transaction spending from the Staking Rewards Pool's UTXO
rather than minting new coins.

### Cold staking and vesting

Cold-staking scripts (`OP_CHECKCOLDSTAKEVERIFY`, see [src/test/script_P2CS_tests.cpp](src/test/script_P2CS_tests.cpp)
and [src/test/coldstaking_tests.cpp](src/test/coldstaking_tests.cpp)) can be
exercised with the wallet's `delegatestake`/`rawdelegatestake` RPCs once you
have a staking-age balance (`nStakeMinAge` is 1 hour on regtest, same as
mainnet/testnet).

The team vesting tranches are locked to real UNIX timestamps 12+ months after
genesis time, not block heights -- generating blocks alone will never unlock
them on regtest. To test an unlock, advance the node's clock instead:

```bash
src/krova-cli -regtest -datadir=/tmp/krova-regtest setmocktime $(($(date +%s) + 400*24*60*60))
```

then generate a new block past that mocked time and attempt to spend the
relevant vesting output.

## Manual Quality Assurance (QA) Testing

Changes should be tested by somebody other than the developer who wrote the code. This is especially important for large or high-risk changes. It is useful to add a test plan to the pull request description if testing the changes is not straightforward.

## Translations

Changes to translations as well as new translations can be submitted to KROVACOIN Core's Transifex page.

Translations are periodically pulled from Transifex and merged into the git repository. See the [translation process](https://github.com/KROVACOIN-Project/KROVACOIN/blob/master/doc/translation_process.md) for details on how this works.

Important: We do not accept translation changes as GitHub pull requests because the next pull from Transifex would automatically overwrite them again.
