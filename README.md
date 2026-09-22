# bitcoinkernel-node

A minimal Bitcoin full node that connects to a single peer, downloads the blockchain, and validates each block. The project demonstrates how to build a minimal full-validation node using [libbitcoinkernel](https://github.com/bitcoin/bitcoin), Bitcoin Core's validation engine library for external applications.
```
peer ──request_headers──▶ BlockHeader ─────▶ ProcessBlockHeader
peer ──request_block ───▶ BlockMessage ────▶ ProcessBlock
                                              └─ kernel validates, stores blocks
```

## Build

```bash
conan install . --output-folder=build --build=missing -s build_type=Release -s compiler.cppstd=23
cmake --preset conan-release
cmake --build --preset conan-release
```

## Run

```bash
# signet, peers found via DNS seeds
./build/build/Release/bitcoinkernel_node --chain signet --datadir ~/.bitcoinkernel-node/signet

# same, but in the background; stop it with `kill <pid>` (SIGTERM shuts down cleanly)
./build/build/Release/bitcoinkernel_node --chain signet --datadir ~/.bitcoinkernel-node/signet --daemon
tail -f ~/.bitcoinkernel-node/signet/debug.log
```

Every run appends timestamped output to `<datadir>/debug.log`; with `--daemon`
that file is the only place it goes. See [doc/daemon.md](doc/daemon.md) for
running it in the background.

## What it does

- Connects to one peer found via DNS seeds and performs the version handshake
- Syncs headers from connected peer
- validation is done through `libbitcoinkernel` C++ wrapper.

The data directory holds the kernel's `blocks/` and `chainstate/` — the same
layout Bitcoin Core uses plus `peers.txt` for stroing addresses.

[binary_p2p]: https://github.com/yuvicc/binary_p2p
[kernel]: https://github.com/bitcoin/bitcoin/blob/master/src/kernel/bitcoinkernel.h
[conan]: https://conan.io
