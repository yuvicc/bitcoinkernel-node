#include "node/dns_seeds.h"
#include "node/addr_manager.h"
#include "node/kernel_node.h"
#include "node/p2p.h"
#include "node/sync.h"
#include "util/daemon.h"
#include "util/hex.h"
#include "util/log.h"
#include "util/signal.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <iostream>
#include <optional>
#include <vector>
#include <span>
#include <string>
#include <string_view>
#include <thread>

namespace {

struct Options {
    node::KernelConfig kernel;
    std::string peer;
    bool daemon{false};
};

void print_usage()
{
    std::cerr << "usage: bitcoinkernel_node [options]\n"
                 "  --chain <name>      mainnet | testnet | testnet4 | signet | regtest (default: regtest)\n"
                 "  --datadir <path>    data directory (default: ./node_data/<chain>)\n"
                 "  --peer <host:port>  peer to sync from (default: DNS seeds, or 127.0.0.1 on regtest)\n"
                 "  --daemon            run in the background; output goes to <datadir>/debug.log only\n"
                 "  --help\n";
}

bool parse_args(int argc, char* argv[], Options& options)
{
    const std::span args{argv, static_cast<std::size_t>(argc)};
    std::string data_dir;

    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string_view arg{args[i]};

        if (arg == "--help" || arg == "-h") return false;
        if (arg == "--daemon") {
            options.daemon = true;
            continue;
        }

        const bool has_value = i + 1 < args.size();
        if (arg == "--chain" && has_value) {
            const auto chain = node::parse_chain_type(args[++i]);
            if (!chain) {
                std::cerr << "unknown chain: " << args[i] << '\n';
                return false;
            }
            options.kernel.chain = *chain;
        } else if (arg == "--datadir" && has_value) {
            data_dir = args[++i];
        } else if (arg == "--peer" && has_value) {
            options.peer = args[++i];
        } else {
            std::cerr << "unrecognised argument: " << arg << '\n';
            return false;
        }
    }

    if (data_dir.empty()) {
        data_dir = (std::filesystem::path{"./node_data"} / node::chain_name(options.kernel.chain)).string();
    }
    options.kernel.data_dir = data_dir;
    options.kernel.blocks_dir = (std::filesystem::path{data_dir} / "blocks").string();
    return true;
}

// Regtest has no seeds worth speaking of, so localhost is the sensible default
// there; everywhere else we top the store up from the chain's DNS seeds.
void refill_addresses(node::AddrManager& addresses, btck::ChainType chain)
{
    const auto port = node::default_port(chain);

    if (chain == btck::ChainType::REGTEST) {
        const std::array local{node::PeerAddress{.host = "127.0.0.1", .port = port}};
        addresses.add(local);
        return;
    }

    util::log(std::format("resolving {} DNS seeds", node::dns_seeds(chain).size()));
    const auto resolved = node::resolve_seeds(chain, 25);
    const auto added = addresses.add(resolved);
    util::log(std::format("{} address(es) from seeds, {} new, {} known in total",
                          resolved.size(), added, addresses.size()));
}

// Known addresses first; only fall back to DNS once they are exhausted, and
// only then start the cycle over.
std::optional<node::PeerAddress> next_peer(node::AddrManager& addresses, btck::ChainType chain)
{
    if (auto candidate = addresses.select()) return candidate;

    refill_addresses(addresses, chain);
    if (auto candidate = addresses.select()) return candidate;

    addresses.reset_cycle();
    return addresses.select();
}

// A refused connection comes back immediately, so without this the rotation
// would spin through the address list as fast as the CPU allows.
void backoff()
{
    for (int elapsed = 0; elapsed < 20 && !util::shutdown_requested(); ++elapsed) {
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
}

std::expected<void, node::SyncError> sync_with_peer(node::KernelNode& node, node::PeerConnection& connection)
{
    const auto headers = node::sync_headers(node, connection.peer);
    if (!headers) return std::unexpected(headers.error());
    util::log(std::format("header sync done: {} received, best header {}",
                          headers->headers_received,
                          headers->best_header_height));

    const auto blocks = node::download_blocks(node, connection.peer);
    if (!blocks) return std::unexpected(blocks.error());
    util::log(std::format("block sync done: {} processed, tip {}",
                          blocks->blocks_processed,
                          blocks->tip_height));

    if (headers->interrupted || blocks->interrupted) return {};
    return node::run_live_loop(node, connection);
}

} // namespace

int main(int argc, char* argv[])
{
    Options options;
    if (!parse_args(argc, argv, options)) {
        print_usage();
        return EXIT_FAILURE;
    }

    // debug.log is opened before forking so a bad datadir is reported on the
    // terminal, and the fork happens before the kernel starts any threads.
    const std::filesystem::path log_path = std::filesystem::path{options.kernel.data_dir} / "debug.log";
    std::error_code error;
    std::filesystem::create_directories(options.kernel.data_dir, error);
    if (error || !util::open_log_file(log_path)) {
        std::cerr << "cannot open log file " << log_path.string() << '\n';
        return EXIT_FAILURE;
    }

    if (options.daemon) {
        util::set_print_to_console(false);
        if (!util::daemonize(log_path)) {
            util::log("failed to daemonize");
            return EXIT_FAILURE;
        }
    }

    util::install_signal_handlers();

    try {
        node::KernelNode node{options.kernel};

        const auto chain = node.chainman().GetChain();
        util::log(std::format("chain {} data dir {}",
                              node::chain_name(node.config().chain),
                              node.config().data_dir));
        util::log(std::format("tip {} {}",
                              node.tip_height(),
                              util::to_display_hex(chain.GetByHeight(chain.Height()).GetHash().ToBytes())));

        node::AddrManager addresses{std::filesystem::path{node.config().data_dir} / "peers.txt"};
        addresses.load();

        // An explicit --peer means that peer and no other.
        const bool pinned = !options.peer.empty();
        if (pinned) {
            const auto address = node::parse_peer_address(options.peer, node::default_port(node.config().chain));
            if (!address) {
                util::log(std::format("invalid peer address: {}", options.peer));
                return EXIT_FAILURE;
            }
            const std::array pinned_address{*address};
            addresses.add(pinned_address);
        }

        util::notify_daemon_started();

        while (!util::shutdown_requested()) {
            const auto candidate = pinned
                                       ? node::parse_peer_address(options.peer, node::default_port(node.config().chain))
                                       : next_peer(addresses, node.config().chain);
            if (!candidate) {
                util::log("no peer addresses left to try");
                return EXIT_FAILURE;
            }

            util::log(std::format("connecting to {}:{}", candidate->host, candidate->port));
            auto connection = node::connect_and_handshake(*candidate, node.config().chain, node.tip_height());
            if (!connection) {
                addresses.record_failure(*candidate);
                util::log(connection.error());
                if (pinned) return EXIT_FAILURE;
                backoff();
                continue;
            }

            addresses.record_success(*candidate);
            util::set_interrupt_socket(connection->socket);
            util::log(std::format("connected to {}:{} — {} protocol {} height {}",
                                  candidate->host,
                                  candidate->port,
                                  connection->version.user_agent,
                                  connection->version.protocol_version,
                                  connection->version.start_height));

            const auto outcome = sync_with_peer(node, *connection);
            if (outcome) break;

            if (outcome.error().kind == node::FailureKind::fatal) {
                util::log(outcome.error().message);
                return EXIT_FAILURE;
            }

            addresses.record_failure(*candidate);
            util::log(std::format("peer {}:{} dropped out: {}",
                                  candidate->host, candidate->port, outcome.error().message));
            if (pinned) return EXIT_FAILURE;
            util::log("switching to another peer");
            backoff();
        }

        addresses.save();
        util::log(std::format("shutting down at tip {}", node.tip_height()));
    } catch (const std::exception& error) {
        util::log(std::format("fatal: {}", error.what()));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
