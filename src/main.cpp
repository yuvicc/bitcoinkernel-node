#include "node/kernel_node.h"
#include "util/hex.h"
#include "util/log.h"

#include <cstdlib>
#include <filesystem>
#include <format>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

namespace {

void print_usage()
{
    std::cerr << "usage: bitcoinkernel_node [options]\n"
                 "  --chain <name>    mainnet | testnet | testnet4 | signet | regtest (default: regtest)\n"
                 "  --datadir <path>  data directory (default: ./node_data/<chain>)\n"
                 "  --help\n";
}

bool parse_args(int argc, char* argv[], node::KernelConfig& config)
{
    const std::span args{argv, static_cast<std::size_t>(argc)};
    std::string data_dir;

    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string_view arg{args[i]};

        if (arg == "--help" || arg == "-h") return false;

        const bool has_value = i + 1 < args.size();
        if (arg == "--chain" && has_value) {
            const auto chain = node::parse_chain_type(args[++i]);
            if (!chain) {
                std::cerr << "unknown chain: " << args[i] << '\n';
                return false;
            }
            config.chain = *chain;
        } else if (arg == "--datadir" && has_value) {
            data_dir = args[++i];
        } else {
            std::cerr << "unrecognised argument: " << arg << '\n';
            return false;
        }
    }

    if (data_dir.empty()) {
        data_dir = (std::filesystem::path{"./node_data"} / node::chain_name(config.chain)).string();
    }
    config.data_dir = data_dir;
    config.blocks_dir = (std::filesystem::path{data_dir} / "blocks").string();
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    node::KernelConfig config;
    if (!parse_args(argc, argv, config)) {
        print_usage();
        return EXIT_FAILURE;
    }

    try {
        node::KernelNode node{std::move(config)};

        const auto chain = node.chainman().GetChain();
        const auto tip = chain.GetByHeight(chain.Height());

        util::log(std::format("chain {} data dir {}",
                              node::chain_name(node.config().chain),
                              node.config().data_dir));
        util::log(std::format("tip {} {}",
                              node.tip_height(),
                              util::to_display_hex(tip.GetHash().ToBytes())));
        util::log(std::format("best header {}", node.best_header_height()));
    } catch (const std::exception& error) {
        std::cerr << "fatal: " << error.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
