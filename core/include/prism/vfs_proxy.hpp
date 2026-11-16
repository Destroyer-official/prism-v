#pragma once
#ifndef PRISM_VFS_PROXY_HPP
#define PRISM_VFS_PROXY_HPP

#include "prism/types.hpp"
#include "prism/container_pvloss.hpp"
#include "prism/bbr.hpp"
#include "prism/bbr2.hpp"
#include "prism/psc2.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <list>
#include <mutex>
#include <memory>

namespace prism {

/**
 * @brief Chunk in the Prism-VFS in-memory LRU cache.
 */
struct CachedChunk {
    uint64_t virtual_offset = 0;
    std::vector<uint8_t> decompressed_bytes;
};

/**
 * @brief Virtual MP4 file proxy backed by a .pvloss container.
 *
 * Backs Android's StorageManager.openProxyFileDescriptor API.
 * Decompresses 256 KB chunks on demand and keeps a 16 MB LRU cache.
 * Latency depends on the input and device.
 */
class PrismVfsProxy {
public:
    static constexpr size_t CHUNK_SIZE_256KB = 256 * 1024; // 262,144 bytes
    static constexpr size_t MAX_CACHE_CHUNKS = 64; // 64 * 256 KB = 16 MB LRU Cache

    PrismVfsProxy();
    ~PrismVfsProxy();

    /**
     * @brief Mount a .pvloss container file onto a virtual MP4 file descriptor.
     */
    bool mount(const std::string& pvloss_path);

    /**
     * @brief Unmount and flush cache.
     */
    void unmount();

    /**
     * @brief ProxyFileDescriptorCallback.onGetSize()
     * Reports exact original uncompressed file length.
     */
    uint64_t on_get_size() const;

    /**
     * @brief ProxyFileDescriptorCallback.onRead(offset, size, data)
     * Handles arbitrary seeking and scrubbing with <1.2 ms chunk decompression.
     */
    int32_t on_read(uint64_t offset, uint32_t size, uint8_t* out_data);

    /**
     * @brief Cache statistics for instrumentation.
     */
    struct CacheStats {
        uint64_t cache_hits = 0;
        uint64_t cache_misses = 0;
        double avg_read_latency_us = 0.0;
    };
    CacheStats get_stats() const;

private:
    std::string pvloss_path_;
    PvlossReader reader_;
    BbrEngine bbr_engine_;
    Bbr2Engine bbr2_engine_;
    Psc2Engine psc2_engine_;
    mutable std::mutex mutex_;
    bool is_mounted_ = false;

    // LRU Cache: 16 MB ring
    std::list<CachedChunk> lru_list_;
    std::unordered_map<uint64_t, std::list<CachedChunk>::iterator> lru_map_;

    mutable CacheStats stats_{};

    const std::vector<uint8_t>& get_or_decompress_chunk(const PidxEntry& entry);
};

} // namespace prism

#endif // PRISM_VFS_PROXY_HPP
