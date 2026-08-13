#pragma once

#include "bitcoinkernel_wrapper.h"
#include "node/p2p.h"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace node {
[[nodiscard]] std::span<const std::string_view> dns_seeds(btck::ChainType chain);

[[nodiscard]] std::vector<PeerAddress> resolve_seeds(btck::ChainType chain, std::size_t max_addresses);

} // namespace node
