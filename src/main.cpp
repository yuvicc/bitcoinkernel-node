#include "bitcoinkernel_wrapper.h"

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace {

std::string to_display_hex(std::span<const std::byte> bytes)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(bytes.size() * 2);
    for (auto it = bytes.rbegin(); it != bytes.rend(); ++it) {
        const auto value = static_cast<unsigned char>(*it);
        hex.push_back(digits[value >> 4]);
        hex.push_back(digits[value & 0x0f]);
    }
    return hex;
}

class StdoutLog
{
public:
    void LogMessage(std::string_view message)
    {
        std::cout << "[kernel] " << message;
        if (!message.ends_with('\n')) std::cout << '\n';
    }
};

} // namespace

int main(int argc, char* argv[])
{
    const std::string data_dir{argc > 1 ? argv[1] : "./node_data"};
    const std::string blocks_dir{(std::filesystem::path{data_dir} / "blocks").string()};

    try {
        btck::Logger<StdoutLog> logger{std::make_unique<StdoutLog>()};

        btck::ChainParams chain_params{btck::ChainType::REGTEST};

        btck::ContextOptions context_options;
        context_options.SetChainParams(chain_params);
        btck::Context context{context_options};

        btck::ChainstateManagerOptions chainman_options{context, data_dir, blocks_dir};
        chainman_options.SetWorkerThreads(4);

        btck::ChainMan chainman{context, chainman_options};

        const auto chain = chainman.GetChain();
        const auto tip = chain.GetByHeight(chain.Height());

        std::cout << "data dir: " << data_dir << '\n'
                  << "tip height: " << chain.Height() << '\n'
                  << "tip hash: " << to_display_hex(tip.GetHash().ToBytes()) << '\n';
    } catch (const std::exception& error) {
        std::cerr << "fatal: " << error.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
