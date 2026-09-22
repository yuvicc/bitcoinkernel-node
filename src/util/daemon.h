#pragma once

#include <filesystem>

namespace util {

// Detach from the terminal. Only the child returns (true on success); the
// parent waits until the child calls notify_daemon_started() or exits, reports
// the outcome on the terminal and exits. Call before any threads are started.
[[nodiscard]] bool daemonize(const std::filesystem::path& log_path);

// Tell the waiting parent that startup succeeded. No-op when not daemonized.
void notify_daemon_started();

} // namespace util
