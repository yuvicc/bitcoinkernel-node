#include "util/signal.h"

#include <atomic>
#include <csignal>
#include <sys/socket.h>

namespace util {

namespace {

std::atomic<bool> g_shutdown{false};
std::atomic<int> g_socket{-1};
static_assert(std::atomic<bool>::is_always_lock_free);
static_assert(std::atomic<int>::is_always_lock_free);

extern "C" void handle_signal(int)
{
    g_shutdown.store(true, std::memory_order_relaxed);

    // shutdown() is async-signal-safe; this makes the blocking read return.
    const int fd = g_socket.load(std::memory_order_relaxed);
    if (fd >= 0) ::shutdown(fd, SHUT_RDWR);
}

} // namespace

void install_signal_handlers()
{
    struct sigaction action{};
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);
}

bool shutdown_requested()
{
    return g_shutdown.load(std::memory_order_relaxed);
}

void set_interrupt_socket(int fd)
{
    g_socket.store(fd, std::memory_order_relaxed);
}

} // namespace util
