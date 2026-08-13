#pragma once

#include "bitcoinkernel_wrapper.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace node {

[[nodiscard]] std::string_view describe(btck::ValidationMode mode);
[[nodiscard]] std::string_view describe(btck::BlockValidationResult result);

struct BlockFailure {
    std::string hash;
    btck::ValidationMode mode{};
    btck::BlockValidationResult result{};
};

// Callbacks for any notifications from kernel interface
class NodeNotifications : public btck::KernelNotifications
{
public:
    void BlockTipHandler(btck::SynchronizationState state, btck::BlockTreeEntry entry, double verification_progress) override;
    void HeaderTipHandler(btck::SynchronizationState state, std::int64_t height, std::int64_t timestamp, bool presync) override;
    void WarningSetHandler(btck::Warning warning, std::string_view message) override;
    void WarningUnsetHandler(btck::Warning warning) override;
    void FlushErrorHandler(std::string_view error) override;
    void FatalErrorHandler(std::string_view error) override;

    // Set once the kernel reports an unrecoverable error.
    [[nodiscard]] std::optional<std::string> fatal_error() const;

private:
    // Both handlers fire once per header/block, so log lines are thinned out.
    bool should_log(std::int64_t height, std::int64_t& last_logged, std::int64_t interval);

    mutable std::mutex m_mutex;
    std::optional<std::string> m_fatal_error;
    std::int64_t m_last_logged_tip{-1};
    std::int64_t m_last_logged_header{-1};
    // The kernel clears warnings after every block, so only warnings we saw
    // raised are worth reporting as cleared.
    std::set<std::int64_t> m_active_warnings;
};

// Validation verdicts
class NodeValidation : public btck::ValidationInterface
{
public:
    void BlockChecked(btck::Block block, btck::BlockValidationStateView state) override;

    [[nodiscard]] std::optional<BlockFailure> take_failure();

private:
    std::mutex m_mutex;
    std::optional<BlockFailure> m_failure;
};

} // namespace node
