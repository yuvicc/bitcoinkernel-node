#include "node/sync.h"

#include "block_hash.h"
#include "block_serializer.h"
#include "inventory.h"
#include "net/block_download.h"
#include "net/header_sync.h"
#include "node/p2p.h"
#include "payload_parser.h"
#include "util/hex.h"
#include "util/log.h"
#include "util/signal.h"
#include "util/writer.h"

#include <algorithm>
#include <format>
#include <utility>
#include <variant>

namespace node {

namespace {

constexpr std::size_t max_headers_per_message = 2000;

SyncError peer_failure(std::string message)
{
    return SyncError{.kind = FailureKind::peer, .message = std::move(message)};
}

SyncError fatal_failure(std::string message)
{
    return SyncError{.kind = FailureKind::fatal, .message = std::move(message)};
}

// The kernel and binary_p2p must agree on hash byte order before the locator
// means anything. Checked once against a header the kernel has just accepted.
bool hashes_agree(const btck::ChainMan& chainman, const BlockHeader& header)
{
    const btck::BlockHash hash{block_hash(header)};
    return chainman.GetBlockTreeEntry(hash).has_value();
}

} // namespace

std::array<std::byte, 80> serialize_block_header(const BlockHeader& header)
{
    ByteWriter writer{80};
    writer.write_i32_le(header.version);
    writer.write_array(header.previous_block_hash);
    writer.write_array(header.merkle_root);
    writer.write_u32_le(header.timestamp);
    writer.write_u32_le(header.bits);
    writer.write_u32_le(header.nonce);

    const auto bytes = writer.take();
    std::array<std::byte, 80> serialized{};
    std::copy(bytes.begin(), bytes.end(), serialized.begin());
    return serialized;
}

std::vector<std::array<std::byte, 32>> build_locator(const btck::ChainMan& chainman)
{
    const auto best = chainman.GetBestEntry();
    const auto height = best.GetHeight();

    std::vector<std::array<std::byte, 32>> locator;
    std::int32_t step = 1;
    for (std::int32_t next = height; next > 0; next -= step) {
        locator.push_back(best.GetAncestor(next).GetHash().ToBytes());
        if (locator.size() > 10) step *= 2;
    }
    locator.push_back(best.GetAncestor(0).GetHash().ToBytes());
    return locator;
}

std::expected<HeaderSyncResult, SyncError> sync_headers(KernelNode& node, Peer& peer)
{
    HeaderSyncResult result{};
    bool byte_order_checked = false;

    while (true) {
        if (util::shutdown_requested()) {
            result.interrupted = true;
            break;
        }

        const auto headers = request_headers(peer, protocol_version, build_locator(node.chainman()));
        if (!headers) {
            if (util::shutdown_requested()) {
                result.interrupted = true;
                break;
            }
            return std::unexpected(peer_failure(std::format("header sync failed: {}", describe(headers.error()))));
        }
        if (headers->empty()) break;

        for (const auto& header : *headers) {
            const auto serialized = serialize_block_header(header);
            btck::BlockHeader kernel_header{serialized};

            const auto state = node.chainman().ProcessBlockHeader(kernel_header);
            if (state.GetValidationMode() != btck::ValidationMode::VALID) {
                return std::unexpected(peer_failure(std::format(
                    "header {} rejected: {} ({})",
                    util::to_display_hex(block_hash(header)),
                    describe(state.GetBlockValidationResult()),
                    describe(state.GetValidationMode()))));
            }
        }
        result.headers_received += headers->size();

        if (!byte_order_checked) {
            if (!hashes_agree(node.chainman(), headers->front())) {
                return std::unexpected(fatal_failure("hash byte order mismatch between the kernel and the p2p layer"));
            }
            byte_order_checked = true;
        }

        if (const auto fatal = node.notifications().fatal_error()) {
            return std::unexpected(fatal_failure(*fatal));
        }

        util::log(std::format("headers {} (+{})", node.best_header_height(), headers->size()));
        if (headers->size() < max_headers_per_message) break;
    }

    result.best_header_height = node.best_header_height();
    return result;
}

std::expected<BlockSyncResult, SyncError> download_blocks(KernelNode& node, Peer& peer)
{
    BlockSyncResult result{};

    const auto best_header = node.chainman().GetBestEntry();
    const auto target = best_header.GetHeight();

    for (std::int32_t height = node.tip_height() + 1; height <= target; ++height) {
        if (util::shutdown_requested()) {
            result.interrupted = true;
            break;
        }

        const auto entry = best_header.GetAncestor(height);
        const auto hash = entry.GetHash().ToBytes();

        const auto message = request_block(peer, hash, InventoryType::witness_block);
        if (!message) {
            if (util::shutdown_requested()) {
                result.interrupted = true;
                break;
            }
            return std::unexpected(peer_failure(std::format("block download failed at height {}: {}",
                                                            height, describe(message.error()))));
        }
        if (block_hash(message->header) != hash) {
            return std::unexpected(peer_failure(std::format("peer sent the wrong block at height {}: asked for {}, got {}",
                                                            height,
                                                            util::to_display_hex(hash),
                                                            util::to_display_hex(block_hash(message->header)))));
        }

        ByteWriter writer;
        serialize_block_payload(writer, *message);
        const auto serialized = writer.take();

        btck::Block block{serialized};
        bool new_block = false;
        if (!node.chainman().ProcessBlock(block, &new_block)) {
            return std::unexpected(fatal_failure(std::format("kernel could not process block {} at height {}",
                                                              util::to_display_hex(hash), height)));
        }
        // ProcessBlock succeeding only means processing ran; the verdict lands
        // in the validation interface.
        if (const auto failure = node.validation().take_failure()) {
            return std::unexpected(peer_failure(std::format("block {} at height {} is invalid: {} ({})",
                                                            failure->hash,
                                                            height,
                                                            describe(failure->result),
                                                            describe(failure->mode))));
        }
        if (const auto fatal = node.notifications().fatal_error()) {
            return std::unexpected(fatal_failure(*fatal));
        }

        ++result.blocks_processed;
    }

    result.tip_height = node.tip_height();
    return result;
}

std::expected<void, SyncError> run_live_loop(KernelNode& node, PeerConnection& connection)
{
    util::log("listening for new blocks (ctrl-c to stop)");

    auto& peer = connection.peer;

    while (!util::shutdown_requested()) {
        auto raw = peer.receive();
        if (!raw) {
            if (util::shutdown_requested()) break;
            // An idle chain is normal here: no traffic for the receive timeout
            // is not the same as a stalled peer. Any partial message stays
            // buffered in the Peer, so resuming the read is safe.
            if (raw.error() == PeerErrorCode::io_error && last_error_was_timeout()) continue;
            return std::unexpected(peer_failure(std::format("connection lost: {}", describe(raw.error()))));
        }

        const auto message = parse_payload(std::move(*raw));
        if (!message) return std::unexpected(peer_failure("peer sent a message we could not parse"));

        bool announced_a_block = false;

        if (const auto* ping = std::get_if<PingMessage>(&message->payload)) {
            if (const auto sent = peer.send("pong", PongMessage{ping->nonce}); !sent) {
                return std::unexpected(peer_failure(std::format("could not answer ping: {}", describe(sent.error()))));
            }
        } else if (const auto* inv = std::get_if<InvMessage>(&message->payload)) {
            announced_a_block = std::ranges::any_of(inv->inventory, [](const InventoryVector& item) {
                return item.type == InventoryType::block || item.type == InventoryType::witness_block;
            });
        } else if (std::holds_alternative<HeadersMessage>(message->payload)) {
            announced_a_block = true;
        }

        if (!announced_a_block) continue;

        // Let the kernel's header chain stay authoritative: ask for headers
        // from our locator rather than trusting the announced hash directly.
        const auto headers = sync_headers(node, peer);
        if (!headers) return std::unexpected(headers.error());
        if (headers->interrupted) break;

        const auto blocks = download_blocks(node, peer);
        if (!blocks) return std::unexpected(blocks.error());
        if (blocks->interrupted) break;


        if (blocks->blocks_processed > 0) {
            util::log(std::format("connected {} new block(s), tip {}",
                                  blocks->blocks_processed,
                                  blocks->tip_height));
        }
    }

    return {};
}

} // namespace node
