#pragma once

#include "bitcoinkernel_wrapper.h"
#include "net/handshake.h"
#include "net/header_sync.h"
#include "net/peer.h"
#include "net/tcp_connection.h"
#include "version_message.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace node {

inline constexpr std::uint32_t protocol_version = 70016;
inline constexpr std::uint64_t node_witness_service = 1 << 3;

struct PeerAddress {
    std::string host;
    std::uint16_t port{};
};

struct PeerConnection {
    Peer peer;
    VersionMessage version;
    int socket{-1};
};

[[nodiscard]] std::array<std::byte, 4> network_magic(btck::ChainType chain);
[[nodiscard]] std::uint16_t default_port(btck::ChainType chain);
[[nodiscard]] std::optional<PeerAddress> parse_peer_address(std::string_view text, std::uint16_t fallback_port);

inline constexpr std::chrono::milliseconds default_connect_timeout{5000};

// A peer that sends nothing for this long during a sync has stalled.
inline constexpr std::chrono::seconds receive_timeout{30};

// Returns false if the socket option could not be set.
bool set_receive_timeout(int socket, std::chrono::seconds timeout);

// True when the last socket error was a receive timeout rather than a failure.
[[nodiscard]] bool last_error_was_timeout();

[[nodiscard]] std::expected<PeerConnection, std::string>
connect_and_handshake(const PeerAddress& address,
                      btck::ChainType chain,
                      std::int32_t start_height,
                      std::chrono::milliseconds timeout = default_connect_timeout);

[[nodiscard]] std::string_view describe(TcpErrorCode error);
[[nodiscard]] std::string_view describe(PeerErrorCode error);
[[nodiscard]] std::string_view describe(HandshakeErrorCode error);
[[nodiscard]] std::string_view describe(SyncErrorCode error);

} // namespace node
