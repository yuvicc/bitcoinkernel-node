#include "node/p2p.h"

#include "network_magic.h"
#include "util/log.h"

#include <cerrno>
#include <charconv>
#include <chrono>
#include <fcntl.h>
#include <format>
#include <memory>
#include <netdb.h>
#include <poll.h>
#include <random>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace node {

namespace {

constexpr std::array<std::byte, 4> testnet3_magic{
    std::byte{0x0b}, std::byte{0x11}, std::byte{0x09}, std::byte{0x07},
};

constexpr std::array<std::byte, 4> testnet4_magic{
    std::byte{0x1c}, std::byte{0x16}, std::byte{0x3f}, std::byte{0x28},
};

std::uint64_t random_nonce()
{
    std::random_device device;
    std::mt19937_64 generator{device()};
    return generator();
}

struct AddrinfoDeleter {
    void operator()(addrinfo* info) const noexcept { freeaddrinfo(info); }
};

// TcpConnection::connect blocks on the kernel's TCP timeout, which is far too
// long when we are working through a list of seed addresses. Do the connect
// non-blocking with our own deadline and hand the ready socket over.
std::expected<TcpConnection, std::string> open_connection(const PeerAddress& address,
                                                          std::chrono::milliseconds timeout)
{
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    const auto port = std::to_string(address.port);
    addrinfo* resolved{};
    if (const int error = getaddrinfo(address.host.c_str(), port.c_str(), &hints, &resolved); error != 0) {
        return std::unexpected(std::format("could not resolve {}: {}", address.host, gai_strerror(error)));
    }
    const std::unique_ptr<addrinfo, AddrinfoDeleter> guard{resolved};

    for (const addrinfo* entry = resolved; entry != nullptr; entry = entry->ai_next) {
        const int fd = ::socket(entry->ai_family, entry->ai_socktype | SOCK_NONBLOCK, entry->ai_protocol);
        if (fd < 0) continue;

        int status = ::connect(fd, entry->ai_addr, entry->ai_addrlen);
        if (status != 0 && errno == EINPROGRESS) {
            pollfd waiter{.fd = fd, .events = POLLOUT, .revents = 0};
            if (::poll(&waiter, 1, static_cast<int>(timeout.count())) == 1) {
                int error{};
                socklen_t length = sizeof(error);
                if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) == 0 && error == 0) status = 0;
            }
        }

        if (status == 0) {
            const int flags = ::fcntl(fd, F_GETFL, 0);
            if (flags >= 0 && ::fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) == 0) {
                return TcpConnection{fd};
            }
        }
        ::close(fd);
    }

    return std::unexpected(std::format("could not connect to {}:{}", address.host, address.port));
}

VersionMessage local_version(const PeerAddress& address, std::int32_t start_height)
{
    VersionMessage version;
    version.protocol_version = static_cast<std::int32_t>(protocol_version);
    version.services = node_witness_service;
    version.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    version.receiver_address.services = node_witness_service;
    version.receiver_address.port = address.port;
    version.sender_address.services = node_witness_service;
    version.nonce = random_nonce();
    version.user_agent = "/bitcoinkernel-node:0.1.0/";
    version.start_height = start_height;
    // Block-only node: we have no mempool to fill.
    version.relay = false;
    return version;
}

} // namespace

std::array<std::byte, 4> network_magic(btck::ChainType chain)
{
    switch (chain) {
    case btck::ChainType::MAINNET: return NetworkMagic::mainnet;
    case btck::ChainType::TESTNET: return testnet3_magic;
    case btck::ChainType::TESTNET_4: return testnet4_magic;
    case btck::ChainType::SIGNET: return NetworkMagic::signet;
    case btck::ChainType::REGTEST: return NetworkMagic::regtest;
    }
    return NetworkMagic::regtest;
}

std::uint16_t default_port(btck::ChainType chain)
{
    switch (chain) {
    case btck::ChainType::MAINNET: return 8333;
    case btck::ChainType::TESTNET: return 18333;
    case btck::ChainType::TESTNET_4: return 48333;
    case btck::ChainType::SIGNET: return 38333;
    case btck::ChainType::REGTEST: return 18444;
    }
    return 18444;
}

std::optional<PeerAddress> parse_peer_address(std::string_view text, std::uint16_t fallback_port)
{
    if (text.empty()) return std::nullopt;

    // [v6]:port, [v6], host:port or host.
    std::string_view host = text;
    std::string_view port_text;

    if (text.front() == '[') {
        const auto close = text.find(']');
        if (close == std::string_view::npos) return std::nullopt;
        host = text.substr(1, close - 1);
        const auto rest = text.substr(close + 1);
        if (!rest.empty()) {
            if (rest.front() != ':') return std::nullopt;
            port_text = rest.substr(1);
        }
    } else if (const auto colon = text.rfind(':');
               colon != std::string_view::npos && text.find(':') == colon) {
        host = text.substr(0, colon);
        port_text = text.substr(colon + 1);
    }

    if (host.empty()) return std::nullopt;

    PeerAddress address{.host = std::string{host}, .port = fallback_port};
    if (!port_text.empty()) {
        std::uint16_t port{};
        const auto* end = port_text.data() + port_text.size();
        const auto [ptr, error] = std::from_chars(port_text.data(), end, port);
        if (error != std::errc{} || ptr != end || port == 0) return std::nullopt;
        address.port = port;
    }
    return address;
}

std::expected<PeerConnection, std::string>
connect_and_handshake(const PeerAddress& address,
                      btck::ChainType chain,
                      std::int32_t start_height,
                      std::chrono::milliseconds timeout)
{
    auto connection = open_connection(address, timeout);
    if (!connection) return std::unexpected(connection.error());

    const int socket = connection->native_handle();
    set_receive_timeout(socket, receive_timeout);
    Peer peer{std::move(*connection), network_magic(chain)};

    auto version = perform_handshake(peer, local_version(address, start_height));
    if (!version) {
        return std::unexpected(std::format("handshake with {}:{} failed: {}",
                                           address.host, address.port, describe(version.error())));
    }

    return PeerConnection{.peer = std::move(peer), .version = std::move(*version), .socket = socket};
}

bool set_receive_timeout(int socket, std::chrono::seconds timeout)
{
    timeval value{.tv_sec = static_cast<time_t>(timeout.count()), .tv_usec = 0};
    return ::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &value, sizeof(value)) == 0;
}

bool last_error_was_timeout()
{
    return errno == EAGAIN || errno == EWOULDBLOCK;
}

std::string_view describe(TcpErrorCode error)
{
    switch (error) {
    case TcpErrorCode::resolve_failed: return "could not resolve host";
    case TcpErrorCode::connect_failed: return "connection refused";
    case TcpErrorCode::send_failed: return "send failed";
    case TcpErrorCode::receive_failed: return "receive failed";
    }
    return "unknown error";
}

std::string_view describe(PeerErrorCode error)
{
    switch (error) {
    case PeerErrorCode::connection_closed: return "peer closed the connection";
    case PeerErrorCode::io_error: return "socket error";
    case PeerErrorCode::protocol_error: return "protocol error";
    }
    return "unknown error";
}

std::string_view describe(HandshakeErrorCode error)
{
    switch (error) {
    case HandshakeErrorCode::connection_closed: return "peer closed the connection";
    case HandshakeErrorCode::io_error: return "socket error";
    case HandshakeErrorCode::protocol_error: return "protocol error";
    case HandshakeErrorCode::unexpected_message: return "peer violated handshake ordering";
    }
    return "unknown error";
}

std::string_view describe(SyncErrorCode error)
{
    switch (error) {
    case SyncErrorCode::connection_closed: return "peer closed the connection";
    case SyncErrorCode::io_error: return "socket error";
    case SyncErrorCode::protocol_error: return "protocol error";
    }
    return "unknown error";
}

} // namespace node
