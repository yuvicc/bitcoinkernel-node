#include "util/hex.h"

namespace util {

namespace {

constexpr char digits[] = "0123456789abcdef";

void append_byte(std::string& hex, std::byte value)
{
    const auto raw = static_cast<unsigned char>(value);
    hex.push_back(digits[raw >> 4]);
    hex.push_back(digits[raw & 0x0f]);
}

} // namespace

std::string to_hex(std::span<const std::byte> bytes)
{
    std::string hex;
    hex.reserve(bytes.size() * 2);
    for (const auto byte : bytes) append_byte(hex, byte);
    return hex;
}

std::string to_display_hex(std::span<const std::byte> bytes)
{
    std::string hex;
    hex.reserve(bytes.size() * 2);
    for (auto it = bytes.rbegin(); it != bytes.rend(); ++it) append_byte(hex, *it);
    return hex;
}

} // namespace util
