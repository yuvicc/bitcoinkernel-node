#pragma once

#include "bitcoinkernel_wrapper.h"
#include "node/notifications.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace node {

// configure the context before doing validation
struct KernelConfig {
    btck::ChainType chain{btck::ChainType::REGTEST};
    std::string data_dir;
    std::string blocks_dir;
    int worker_threads{4};
};

[[nodiscard]] std::optional<btck::ChainType> parse_chain_type(std::string_view name);
[[nodiscard]] std::string_view chain_name(btck::ChainType chain);

// for kernel logs
class KernelLog
{
public:
    void LogMessage(std::string_view message);
};

// does the kernel stuff separately so that we can be smarter on how
// kernel is working under the hood.
class KernelNode
{
public:
    explicit KernelNode(KernelConfig config);

    btck::ChainMan& chainman() { return m_chainman; }
    const btck::ChainMan& chainman() const { return m_chainman; }
    btck::Context& context() { return m_context; }

    NodeNotifications& notifications() { return *m_notifications; }
    NodeValidation& validation() { return *m_validation; }
    const KernelConfig& config() const { return m_config; }

    [[nodiscard]] std::int32_t tip_height() const;
    [[nodiscard]] std::int32_t best_header_height() const;

private:
    KernelConfig m_config;
    btck::Logger<KernelLog> m_logger;
    std::shared_ptr<NodeNotifications> m_notifications;
    std::shared_ptr<NodeValidation> m_validation;
    btck::Context m_context;
    btck::ChainstateManagerOptions m_chainman_options;
    btck::ChainMan m_chainman;
};

} // namespace node
