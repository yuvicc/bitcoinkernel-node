#pragma once

#include "block_header.h"
#include "net/peer.h"
#include "node/kernel_node.h"
#include "node/p2p.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace node {

enum class FailureKind {
    peer,  // which peer misbeheved
    fatal, // fata error
};

struct SyncError {
    FailureKind kind{FailureKind::peer};
    std::string message;
};

// Locator over the most-work header chain
[[nodiscard]] std::vector<std::array<std::byte, 32>> build_locator(const btck::ChainMan& chainman);

[[nodiscard]] std::array<std::byte, 80> serialize_block_header(const BlockHeader& header);

struct HeaderSyncResult {
    std::size_t headers_received{};
    std::int32_t best_header_height{};
    bool interrupted{};
};

[[nodiscard]] std::expected<HeaderSyncResult, SyncError> sync_headers(KernelNode& node, Peer& peer);

struct BlockSyncResult {
    std::size_t blocks_processed{};
    std::int32_t tip_height{};
    bool interrupted{};
};

// Download every block between the validated tip and the most-work header, one at a time
[[nodiscard]] std::expected<BlockSyncResult, SyncError> download_blocks(KernelNode& node, Peer& peer);

[[nodiscard]] std::expected<void, SyncError> run_live_loop(KernelNode& node, PeerConnection& connection);

} // namespace node
