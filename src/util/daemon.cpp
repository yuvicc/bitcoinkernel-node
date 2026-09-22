#include "util/daemon.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

namespace util {

namespace {

int g_ready_fd{-1};

// The parent blocks here until the child reports in. If the child dies during
// startup its end of the pipe closes and read() returns 0.
[[noreturn]] void wait_for_child(int read_fd, pid_t child, const std::filesystem::path& log_path)
{
    char status{0};
    ssize_t n;
    do {
        n = ::read(read_fd, &status, 1);
    } while (n < 0 && errno == EINTR);

    if (n == 1 && status == 1) {
        std::cout << "bitcoinkernel_node started (pid " << child << "), logging to " << log_path.string() << '\n';
        std::cout.flush();
        std::_Exit(EXIT_SUCCESS);
    }
    std::cerr << "bitcoinkernel_node failed to start, see " << log_path.string() << '\n';
    std::_Exit(EXIT_FAILURE);
}

bool redirect_stdio_to_null()
{
    const int null_fd = ::open("/dev/null", O_RDWR);
    if (null_fd < 0) return false;
    for (const int fd : {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO}) {
        if (::dup2(null_fd, fd) < 0) return false;
    }
    if (null_fd > STDERR_FILENO) ::close(null_fd);
    return true;
}

} // namespace

bool daemonize(const std::filesystem::path& log_path)
{
    // Anything still buffered would otherwise be written twice after fork().
    std::cout.flush();
    std::cerr.flush();
    std::fflush(nullptr);

    int fds[2];
    if (::pipe(fds) != 0) return false;

    const pid_t pid = ::fork();
    if (pid < 0) {
        ::close(fds[0]);
        ::close(fds[1]);
        return false;
    }
    if (pid > 0) {
        ::close(fds[1]);
        wait_for_child(fds[0], pid, log_path);
    }

    ::close(fds[0]);
    g_ready_fd = fds[1];
    return ::setsid() >= 0 && redirect_stdio_to_null();
}

void notify_daemon_started()
{
    if (g_ready_fd < 0) return;
    const char status{1};
    [[maybe_unused]] const auto written = ::write(g_ready_fd, &status, 1);
    ::close(g_ready_fd);
    g_ready_fd = -1;
}

} // namespace util
