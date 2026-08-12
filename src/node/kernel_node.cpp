#include "node/kernel_node.h"

#include "util/log.h"

#include <utility>

namespace node {

namespace {

btck::Context make_context(const KernelConfig& config,
                           std::shared_ptr<NodeNotifications> notifications,
                           std::shared_ptr<NodeValidation> validation)
{
    btck::ChainParams chain_params{config.chain};

    btck::ContextOptions options;
    options.SetChainParams(chain_params);
    options.SetNotifications(std::move(notifications));
    options.SetValidationInterface(std::move(validation));

    return btck::Context{options};
}

btck::ChainstateManagerOptions make_chainman_options(const btck::Context& context, const KernelConfig& config)
{
    btck::ChainstateManagerOptions options{context, config.data_dir, config.blocks_dir};
    options.SetWorkerThreads(config.worker_threads);
    return options;
}

} // namespace

std::optional<btck::ChainType> parse_chain_type(std::string_view name)
{
    if (name == "mainnet") return btck::ChainType::MAINNET;
    if (name == "testnet") return btck::ChainType::TESTNET;
    if (name == "testnet4") return btck::ChainType::TESTNET_4;
    if (name == "signet") return btck::ChainType::SIGNET;
    if (name == "regtest") return btck::ChainType::REGTEST;
    return std::nullopt;
}

std::string_view chain_name(btck::ChainType chain)
{
    switch (chain) {
    case btck::ChainType::MAINNET: return "mainnet";
    case btck::ChainType::TESTNET: return "testnet";
    case btck::ChainType::TESTNET_4: return "testnet4";
    case btck::ChainType::SIGNET: return "signet";
    case btck::ChainType::REGTEST: return "regtest";
    }
    return "unknown";
}

void KernelLog::LogMessage(std::string_view message)
{
    util::log_kernel(message);
}

KernelNode::KernelNode(KernelConfig config)
    : m_config{std::move(config)},
      m_logger{std::make_unique<KernelLog>()},
      m_notifications{std::make_shared<NodeNotifications>()},
      m_validation{std::make_shared<NodeValidation>()},
      m_context{make_context(m_config, m_notifications, m_validation)},
      m_chainman_options{make_chainman_options(m_context, m_config)},
      m_chainman{m_context, m_chainman_options}
{
}

std::int32_t KernelNode::tip_height() const
{
    return m_chainman.GetChain().Height();
}

std::int32_t KernelNode::best_header_height() const
{
    return m_chainman.GetBestEntry().GetHeight();
}

} // namespace node
