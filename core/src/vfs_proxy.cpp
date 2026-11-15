#include "prism/vfs_proxy.hpp"
#include <cstring>
#include <chrono>
#include <algorithm>

namespace prism {

PrismVfsProxy::PrismVfsProxy() = default;

PrismVfsProxy::~PrismVfsProxy() {
    unmount();
}

bool PrismVfsProxy::mount(const std::string& pvloss_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    unmount();

    if (!reader_.open(pvloss_path)) {
        return false;
    }

    pvloss_path_ = pvloss_path;
    is_mounted_ = true;
    stats_ = CacheStats{};
    return true;
}

void PrismVfsProxy::unmount() {
    if (is_mounted_) {
        reader_.close();
        lru_list_.clear();
        lru_map_.clear();
        is_mounted_ = false;
    }
}

uint64_t PrismVfsProxy::on_get_size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_mounted_) return 0;
    return reader_.header().orig_file_size;
}

const std::vector<uint8_t>& PrismVfsProxy::get_or_decompress_chunk(const PidxEntry& entry) {
    auto it = lru_map_.find(entry.virtual_byte_offset);
    if (it != lru_map_.end()) {
        stats_.cache_hits++;
        // Move to front of LRU
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
        return it->second->decompressed_bytes;
    }

    stats_.cache_misses++;

    // Decompress 256 KB chunk in RAM
    std::vector<uint8_t> compressed_payload = reader_.read_chunk_payload(entry);
    BbrChunk bchunk;
    bchunk.uncompressed_size = entry.uncompressed_size;
    bchunk.rans_payload = std::move(compressed_payload);

    std::vector<uint8_t> decomp = bbr_engine_.decompress(bchunk);

    // Evict oldest if full (64 chunks = 16 MB)
    if (lru_list_.size() >= MAX_CACHE_CHUNKS) {
        auto last = lru_list_.end();
        --last;
        lru_map_.erase(last->virtual_offset);
        lru_list_.pop_back();
    }

    // Insert new chunk at front
    lru_list_.push_front(CachedChunk{entry.virtual_byte_offset, std::move(decomp)});
    lru_map_[entry.virtual_byte_offset] = lru_list_.begin();

    return lru_list_.front().decompressed_bytes;
}

int32_t PrismVfsProxy::on_read(uint64_t offset, uint32_t size, uint8_t* out_data) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_mounted_ || !out_data || size == 0) return 0;

    auto start_time = std::chrono::high_resolution_clock::now();

    uint64_t total_size = reader_.header().orig_file_size;
    if (offset >= total_size) return 0;

    uint32_t bytes_to_read = size;
    if (offset + bytes_to_read > total_size) {
        bytes_to_read = static_cast<uint32_t>(total_size - offset);
    }

    uint32_t bytes_copied = 0;
    uint64_t current_offset = offset;

    while (bytes_copied < bytes_to_read) {
        const PidxEntry* entry = reader_.find_entry_for_offset(current_offset);
        if (!entry) break;

        const auto& chunk_bytes = get_or_decompress_chunk(*entry);
        uint64_t chunk_start = entry->virtual_byte_offset;
        uint64_t offset_in_chunk = current_offset - chunk_start;

        if (offset_in_chunk >= chunk_bytes.size()) break;

        uint32_t avail_in_chunk = static_cast<uint32_t>(chunk_bytes.size() - offset_in_chunk);
        uint32_t copy_now = std::min(bytes_to_read - bytes_copied, avail_in_chunk);

        std::memcpy(out_data + bytes_copied, chunk_bytes.data() + offset_in_chunk, copy_now);

        bytes_copied += copy_now;
        current_offset += copy_now;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed_us = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    stats_.avg_read_latency_us = (stats_.avg_read_latency_us * 0.9) + (elapsed_us * 0.1);

    return static_cast<int32_t>(bytes_copied);
}

PrismVfsProxy::CacheStats PrismVfsProxy::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

} // namespace prism
