# bitcoinkernel-node

A minimal Bitcoin full node that connects to a single peer, downloads the blockchain, and validates each block. The project demonstrates how to build a minimal full-validation node using [libbitcoinkernel](https://github.com/bitcoin/bitcoin), Bitcoin Core's validation engine library for external applications and a [P2P library](https://github.com/yuvicc/binary_p2p) for serializing / deserializing p2p messages.


## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

[binary_p2p](https://github.com/yuvicc/binary_p2p) is fetched automatically by CMake. To build against a local
checkout instead, add `-DFETCHCONTENT_SOURCE_DIR_BINARY_P2P=/path/to/binary_p2p`.
libbitcoinkernel is looked up under `/usr/local`; override with
`-DBITCOINKERNEL_ROOT=<prefix>`.

## Run

```bash
# signet, peers found via DNS seeds
./build/bitcoinkernel_node --chain signet --datadir ~/.bitcoinkernel-node/signet

# same, but in the background; stop it with `kill <pid>` (SIGTERM shuts down cleanly)
./build/bitcoinkernel_node --chain signet --datadir ~/.bitcoinkernel-node/signet --daemon
tail -f ~/.bitcoinkernel-node/signet/debug.log
```

Every run appends timestamped output to `<datadir>/debug.log`; with `--daemon`
that file is the only place it goes. See [doc/daemon.md](doc/daemon.md) for
running it in the background.

## What it does

- Connects to one peer found via DNS seeds and performs the version handshake
- Syncs headers from connected peer
- validation is done through `libbitcoinkernel` C++ wrapper.
- Currently it has been tested only on signet and regtest.

The data directory holds the kernel's `blocks/` and `chainstate/` — the same
layout Bitcoin Core uses plus `peers.txt` for stroing addresses.

[binary_p2p]: https://github.com/yuvicc/binary_p2p
[kernel]: https://github.com/bitcoin/bitcoin/blob/master/src/kernel/bitcoinkernel.h
