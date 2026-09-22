# Running as a daemon

`bitcoinkernel_node --daemon` detaches from the terminal and keeps syncing in
the background, similar to `bitcoind -daemon`. All output goes to
`<datadir>/debug.log`.

## Start

```bash
./build/build/Release/bitcoinkernel_node \
    --chain mainnet \
    --datadir /data/bitcoinkernel-node/mainnet \
    --daemon
```

The command waits until the node has opened its data directory and loaded the
chainstate, then returns:

```
bitcoinkernel_node started (pid 45544), logging to /data/bitcoinkernel-node/mainnet/debug.log
```

If startup fails (for example a corrupt chainstate, or an invalid `--peer`), it prints
`bitcoinkernel_node failed to start, see <datadir>/debug.log` and exits with
status 1. If the log file itself can't be opened, the error is printed and
nothing is forked.

`--daemon` combines with every other option (`--chain`, `--datadir`, `--peer`).

Use an absolute `--datadir`. The default, `./node_data/<chain>`, is relative to
the directory you start the node from.

## Watch the logs

```bash
tail -f /data/bitcoinkernel-node/mainnet/debug.log
```

Each line starts with its source and a UTC timestamp:

```
node   | 2026-09-22T05:10:11Z connected to 136.57.54.14:38333 — /Satoshi:31.99.0/ protocol 70016 height 323186
node   | 2026-09-22T05:10:14Z headers 2000 (+2000)
kernel | 2026-09-22T05:10:16Z Opened LevelDB successfully
```

Useful filters:

```bash
grep 'node   | .* tip ' debug.log      # validation progress, every 1000 blocks
grep -v '^kernel' debug.log           # node messages only
```

`debug.log` is written on every run, with or without `--daemon`, and is
appended to across restarts.

## Stop

Send `SIGTERM` (or `SIGINT`). The node interrupts the peer connection, saves
`peers.txt`, flushes the chainstate to disk and exits:

```bash
kill <pid>
# or, without the pid:
pkill -TERM -x bitcoinkernel_node
```

Wait for it to exit before you restart it or unmount the disk:

```bash
while kill -0 <pid> 2>/dev/null; do sleep 1; done
```

The last node line in the log is `shutting down at tip <height>`.

Don't use `kill -9`. It skips the chainstate flush, and on the next start the
kernel has to replay the unflushed blocks, or in the worst case the datadir
needs a reindex.

## Restart and resume

Start it again with the same `--datadir`. Headers and blocks sync from the
current tip, so stopping and starting loses no progress.

## Status

There is no RPC or `bitcoin-cli` equivalent, so use the process list and the log:

```bash
pgrep -a bitcoinkernel_node                          # is it running, and with what args
grep 'node   |' debug.log | tail -n 5                 # what it did last
```

## Exit conditions

The daemon exits by itself, with `EXIT_FAILURE`, when:

- it was started with `--peer` and that peer can't be reached or drops out
  (a pinned peer is never replaced by another one);
- it has no peer addresses at all (`peers.txt` is empty and the DNS seeds
  return nothing);
- sync hits a fatal error, such as the kernel rejecting a block.

The reason is the last node line in `debug.log`. Without `--peer`, a peer that
drops out is replaced by the next address, and once every address has failed
the node starts over from the top of the list, so it keeps running.

## Log rotation

The node keeps `debug.log` open and does not reopen it on `SIGHUP`. Because it
writes in append mode, `logrotate` with `copytruncate` works:

```
/data/bitcoinkernel-node/mainnet/debug.log {
    weekly
    rotate 4
    compress
    copytruncate
    missingok
}
```

## Running under systemd

systemd does its own backgrounding, so run the node **without** `--daemon`
under it. Output goes to the journal as well as to `debug.log`:

```ini
# /etc/systemd/system/bitcoinkernel-node.service
[Unit]
Description=bitcoinkernel-node
After=network-online.target
Wants=network-online.target

[Service]
User=bitcoin
ExecStart=/opt/bitcoinkernel-node/bitcoinkernel_node --chain mainnet --datadir /data/bitcoinkernel-node/mainnet
Restart=on-failure
KillSignal=SIGTERM
# Give the chainstate flush time to finish before systemd sends SIGKILL.
TimeoutStopSec=600

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable --now bitcoinkernel-node
sudo systemctl stop bitcoinkernel-node
```

## Limitations

- **No datadir lock.** Nothing stops a second instance from starting on the
  same `--datadir`, and that corrupts the chainstate. Check `pgrep -a
  bitcoinkernel_node` before starting.
- **No pid file.** Note the pid printed at startup, or use `pgrep`.
- **No RPC.** You can only observe the node through `debug.log`.
