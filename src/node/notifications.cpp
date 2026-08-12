#include "node/notifications.h"

#include "util/hex.h"
#include "util/log.h"

#include <format>
#include <utility>

namespace node {

std::string_view describe(btck::ValidationMode mode)
{
    switch (mode) {
    case btck::ValidationMode::VALID: return "valid";
    case btck::ValidationMode::INVALID: return "invalid";
    case btck::ValidationMode::INTERNAL_ERROR: return "internal error";
    }
    return "unknown";
}

std::string_view describe(btck::BlockValidationResult result)
{
    switch (result) {
    case btck::BlockValidationResult::UNSET: return "unset";
    case btck::BlockValidationResult::CONSENSUS: return "consensus rule violation";
    case btck::BlockValidationResult::CACHED_INVALID: return "cached invalid";
    case btck::BlockValidationResult::INVALID_HEADER: return "invalid header";
    case btck::BlockValidationResult::MUTATED: return "mutated";
    case btck::BlockValidationResult::MISSING_PREV: return "missing previous block";
    case btck::BlockValidationResult::INVALID_PREV: return "invalid previous block";
    case btck::BlockValidationResult::TIME_FUTURE: return "timestamp too far in the future";
    case btck::BlockValidationResult::HEADER_LOW_WORK: return "header has too little work";
    }
    return "unknown";
}

namespace {

std::string_view describe(btck::SynchronizationState state)
{
    switch (state) {
    case btck::SynchronizationState::INIT_REINDEX: return "reindex";
    case btck::SynchronizationState::INIT_DOWNLOAD: return "ibd";
    case btck::SynchronizationState::POST_INIT: return "synced";
    }
    return "unknown";
}

std::string_view describe(btck::Warning warning)
{
    switch (warning) {
    case btck::Warning::UNKNOWN_NEW_RULES_ACTIVATED: return "unknown new rules activated";
    case btck::Warning::LARGE_WORK_INVALID_CHAIN: return "large work invalid chain";
    }
    return "unknown";
}

} // namespace

void NodeNotifications::BlockTipHandler(btck::SynchronizationState state, btck::BlockTreeEntry entry, double verification_progress)
{
    util::log(std::format("tip {} {} [{}] progress {:.2f}%",
                          entry.GetHeight(),
                          util::to_display_hex(entry.GetHash().ToBytes()),
                          describe(state),
                          verification_progress * 100.0));
}

void NodeNotifications::HeaderTipHandler(btck::SynchronizationState state, std::int64_t height, std::int64_t timestamp, bool presync)
{
    util::log(std::format("headers {} [{}]{} timestamp {}",
                          height,
                          describe(state),
                          presync ? " presync" : "",
                          timestamp));
}

void NodeNotifications::WarningSetHandler(btck::Warning warning, std::string_view message)
{
    util::log(std::format("warning: {}: {}", describe(warning), message));
}

void NodeNotifications::WarningUnsetHandler(btck::Warning warning)
{
    util::log(std::format("warning cleared: {}", describe(warning)));
}

void NodeNotifications::FlushErrorHandler(std::string_view error)
{
    util::log(std::format("flush error: {}", error));
}

void NodeNotifications::FatalErrorHandler(std::string_view error)
{
    util::log(std::format("fatal error: {}", error));
    const std::lock_guard lock{m_mutex};
    if (!m_fatal_error) m_fatal_error = std::string{error};
}

std::optional<std::string> NodeNotifications::fatal_error() const
{
    const std::lock_guard lock{m_mutex};
    return m_fatal_error;
}

void NodeValidation::BlockChecked(btck::Block block, btck::BlockValidationStateView state)
{
    const auto mode = state.GetValidationMode();
    if (mode == btck::ValidationMode::VALID) return;

    BlockFailure failure{
        .hash = util::to_display_hex(block.GetHash().ToBytes()),
        .mode = mode,
        .result = state.GetBlockValidationResult(),
    };
    util::log(std::format("block {} rejected: {} ({})",
                          failure.hash,
                          describe(failure.result),
                          describe(failure.mode)));

    const std::lock_guard lock{m_mutex};
    m_failure = std::move(failure);
}

std::optional<BlockFailure> NodeValidation::take_failure()
{
    const std::lock_guard lock{m_mutex};
    return std::exchange(m_failure, std::nullopt);
}

} // namespace node
