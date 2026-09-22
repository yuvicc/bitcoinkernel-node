#include "util/log.h"

#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>

namespace util {

namespace {

std::mutex g_output_mutex;
std::ofstream g_log_file;
bool g_print_to_console{true};

void write_to(std::ostream& out, std::string_view prefix, std::string_view timestamp, std::string_view message)
{
    out << prefix << timestamp << message;
    if (!message.ends_with('\n')) out << '\n';
    out.flush();
}

// The kernel stamps its own messages, so only our lines get one added, and
// only in the log file.
void write_line(std::string_view prefix, std::string_view message, bool add_timestamp)
{
    const std::lock_guard lock{g_output_mutex};
    if (g_print_to_console) write_to(std::cout, prefix, "", message);
    if (g_log_file.is_open()) {
        const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
        const auto timestamp = add_timestamp ? std::format("{:%Y-%m-%dT%H:%M:%SZ} ", now) : std::string{};
        write_to(g_log_file, prefix, timestamp, message);
    }
}

} // namespace

void log(std::string_view message)
{
    write_line("node   | ", message, true);
}

void log_kernel(std::string_view message)
{
    write_line("kernel | ", message, false);
}

bool open_log_file(const std::filesystem::path& path)
{
    const std::lock_guard lock{g_output_mutex};
    g_log_file.open(path, std::ios::app);
    return g_log_file.is_open();
}

void set_print_to_console(bool enabled)
{
    const std::lock_guard lock{g_output_mutex};
    g_print_to_console = enabled;
}

} // namespace util
