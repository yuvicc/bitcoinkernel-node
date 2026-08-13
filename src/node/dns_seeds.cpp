#include "node/dns_seeds.h"

#include "util/log.h"

#include <algorithm>
#include <array>
#include <format>
#include <memory>
#include <netdb.h>
#include <random>
#include <sys/socket.h>

namespace node {

namespace {

constexpr std::array<std::string_view, 7> mainnet_seeds{
    "dnsseed.bluematt.me.",
    "seed.bitcoin.jonasschnelli.ch.",
    "seed.btc.petertodd.net.",
    "seed.bitcoin.sprovoost.nl.",
    "dnsseed.emzy.de.",
    "seed.bitcoin.wiz.biz.",
    "seed.mainnet.achownodes.xyz.",
};

constexpr std::array<std::string_view, 4> testnet_seeds{
    "testnet-seed.bitcoin.jonasschnelli.ch.",
    "seed.tbtc.petertodd.net.",
    "testnet-seed.bluematt.me.",
    "seed.testnet.achownodes.xyz.",
};

constexpr std::array<std::string_view, 2> testnet4_seeds{
    "seed.testnet4.bitcoin.sprovoost.nl.",
    "seed.testnet4.wiz.biz.",
};

constexpr std::array<std::string_view, 2> signet_seeds{
    "seed.signet.bitcoin.sprovoost.nl.",
    "seed.signet.achownodes.xyz.",
};

struct AddrinfoDeleter {
    void operator()(addrinfo* info) const noexcept { freeaddrinfo(info); }
};

void resolve_one(std::string_view seed,
                 std::uint16_t port,
                 std::vector<PeerAddress>& ipv4,
                 std::vector<PeerAddress>& ipv6)
{
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    const std::string host{seed};
    addrinfo* resolved{};
    if (const int error = getaddrinfo(host.c_str(), nullptr, &hints, &resolved); error != 0) {
        util::log(std::format("seed {} did not resolve: {}", seed, gai_strerror(error)));
        return;
    }
    const std::unique_ptr<addrinfo, AddrinfoDeleter> guard{resolved};

    for (const addrinfo* entry = resolved; entry != nullptr; entry = entry->ai_next) {
        char address[NI_MAXHOST];
        if (getnameinfo(entry->ai_addr, entry->ai_addrlen, address, sizeof(address),
                        nullptr, 0, NI_NUMERICHOST) != 0) {
            continue;
        }
        auto& bucket = entry->ai_family == AF_INET6 ? ipv6 : ipv4;
        bucket.push_back(PeerAddress{.host = address, .port = port});
    }
}

} // namespace

std::span<const std::string_view> dns_seeds(btck::ChainType chain)
{
    switch (chain) {
    case btck::ChainType::MAINNET: return mainnet_seeds;
    case btck::ChainType::TESTNET: return testnet_seeds;
    case btck::ChainType::TESTNET_4: return testnet4_seeds;
    case btck::ChainType::SIGNET: return signet_seeds;
    case btck::ChainType::REGTEST: return {};
    }
    return {};
}

std::vector<PeerAddress> resolve_seeds(btck::ChainType chain, std::size_t max_addresses)
{
    const auto port = default_port(chain);

    std::vector<PeerAddress> ipv4;
    std::vector<PeerAddress> ipv6;
    for (const auto seed : dns_seeds(chain)) {
        resolve_one(seed, port, ipv4, ipv6);
        if (ipv4.size() >= max_addresses) break;
    }

    std::mt19937 generator{std::random_device{}()};
    std::ranges::shuffle(ipv4, generator);
    std::ranges::shuffle(ipv6, generator);

    std::vector<PeerAddress> addresses = std::move(ipv4);
    addresses.insert(addresses.end(), ipv6.begin(), ipv6.end());
    if (addresses.size() > max_addresses) addresses.resize(max_addresses);
    return addresses;
}

} // namespace node
