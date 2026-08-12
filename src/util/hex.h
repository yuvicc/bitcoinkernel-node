#pragma once

#include <cstddef>
#include <span>
#include <string>

namespace util {

[[nodiscard]] std::string to_hex(std::span<const std::byte> bytes);
[[nodiscard]] std::string to_display_hex(std::span<const std::byte> bytes);

} // namespace util
