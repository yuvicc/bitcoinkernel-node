#pragma once

#include "node/p2p.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace node {

struct AddressRecord {
    PeerAddress address;
    std::int64_t last_success{};
    int failures{};
};

// Class that represents how addresses are stored in data dir
class AddrManager
{
public:
    explicit AddrManager(std::filesystem::path path);

    void load();
    void save() const;

    std::size_t add(std::span<const PeerAddress> addresses);

    // Next address to try to fetch
    [[nodiscard]] std::optional<PeerAddress> select();

    void record_success(const PeerAddress& address);
    void record_failure(const PeerAddress& address);

    // Forget which addresses were tried, so exhausted lists can be retried.
    void reset_cycle();

    [[nodiscard]] bool empty() const { return m_records.empty(); }
    [[nodiscard]] std::size_t size() const { return m_records.size(); }

private:
    AddressRecord* find(const PeerAddress& address);

    std::filesystem::path m_path;
    std::vector<AddressRecord> m_records;
    std::set<std::string> m_attempted;
};

} // namespace node
