#pragma once

#include <filesystem>
#include <string_view>

namespace util {

void log(std::string_view message);
void log_kernel(std::string_view message);

// Append every subsequent line, timestamped, to this file as well.
[[nodiscard]] bool open_log_file(const std::filesystem::path& path);
void set_print_to_console(bool enabled);

} // namespace util
