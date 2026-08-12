#pragma once

#include "bitcoinkernel_wrapper.h"

#include <cstdint>
#include <mutex>
#include <optional>
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
    mutable std::mutex m_mutex;
    std::optional<std::string> m_fatal_error;
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
