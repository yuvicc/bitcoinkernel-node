#include "util/log.h"

#include <iostream>
#include <mutex>

namespace util {

namespace {

std::mutex g_output_mutex;

void write_line(std::string_view prefix, std::string_view message)
{
    const std::lock_guard lock{g_output_mutex};
    std::cout << prefix << message;
    if (!message.ends_with('\n')) std::cout << '\n';
    std::cout.flush();
}

} // namespace

void log(std::string_view message)
{
    write_line("node   | ", message);
}

void log_kernel(std::string_view message)
{
    write_line("kernel | ", message);
}

} // namespace util
