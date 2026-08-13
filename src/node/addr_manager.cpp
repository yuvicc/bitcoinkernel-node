#include "node/addr_manager.h"

#include "util/log.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <format>
#include <random>
#include <utility>

namespace node {

namespace {

std::string key_of(const PeerAddress& address)
{
    return std::format("{}:{}", address.host, address.port);
}

std::int64_t now_seconds()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

} // namespace

AddrManager::AddrManager(std::filesystem::path path) : m_path{std::move(path)} {}

void AddrManager::load()
{
    std::ifstream file{m_path};
    if (!file) return;

    std::string host;
    while (file >> host) {
        AddressRecord record;
        record.address.host = host;
        if (!(file >> record.address.port >> record.last_success >> record.failures)) break;
        if (!find(record.address)) m_records.push_back(std::move(record));
    }

    util::log(std::format("loaded {} known peer address(es) from {}", m_records.size(), m_path.string()));
}

void AddrManager::save() const
{
    std::error_code error;
    std::filesystem::create_directories(m_path.parent_path(), error);

    std::ofstream file{m_path, std::ios::trunc};
    if (!file) {
        util::log(std::format("could not write {}", m_path.string()));
        return;
    }

    for (const auto& record : m_records) {
        file << record.address.host << ' ' << record.address.port << ' '
             << record.last_success << ' ' << record.failures << '\n';
    }
}

AddressRecord* AddrManager::find(const PeerAddress& address)
{
    const auto match = std::ranges::find_if(m_records, [&](const AddressRecord& record) {
        return record.address.host == address.host && record.address.port == address.port;
    });
    return match == m_records.end() ? nullptr : &*match;
}

std::size_t AddrManager::add(std::span<const PeerAddress> addresses)
{
    std::size_t added = 0;
    for (const auto& address : addresses) {
        if (find(address)) continue;
        m_records.push_back(AddressRecord{.address = address});
        ++added;
    }
    return added;
}

std::optional<PeerAddress> AddrManager::select()
{
    std::vector<const AddressRecord*> candidates;
    for (const auto& record : m_records) {
        if (!m_attempted.contains(key_of(record.address))) candidates.push_back(&record);
    }
    if (candidates.empty()) return std::nullopt;

    static std::mt19937 generator{std::random_device{}()};
    std::ranges::shuffle(candidates, generator);
    std::ranges::stable_sort(candidates, [](const AddressRecord* left, const AddressRecord* right) {
        if (left->failures != right->failures) return left->failures < right->failures;
        return left->last_success > right->last_success;
    });

    const auto& chosen = candidates.front()->address;
    m_attempted.insert(key_of(chosen));
    return chosen;
}

void AddrManager::record_success(const PeerAddress& address)
{
    if (auto* record = find(address)) {
        record->last_success = now_seconds();
        record->failures = 0;
    }
    save();
}

void AddrManager::record_failure(const PeerAddress& address)
{
    if (auto* record = find(address)) ++record->failures;
    save();
}

void AddrManager::reset_cycle()
{
    m_attempted.clear();
}

} // namespace node
