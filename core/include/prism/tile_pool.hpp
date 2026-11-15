#pragma once
#ifndef PRISM_TILE_POOL_HPP
#define PRISM_TILE_POOL_HPP

#include "prism/types.hpp"
#include "prism/detint.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <array>
#include <mutex>

namespace prism {

/**
 * @brief Memory-Bounded Spatial Tile Buffer.
 * Contains 256x256 Luma (or 128x128 Chroma) samples plus 16-pixel halo margin.
 * Pre-allocated on 16 KB page boundaries to comply with Android 15+.
 */
struct SpatialTile {
    static constexpr size_t CORE_DIM = TILE_SIZE_LUMA; // 256
    static constexpr size_t HALO = TILE_HALO_PIXELS;   // 16
    static constexpr size_t TOTAL_DIM = CORE_DIM + (2 * HALO); // 288
    static constexpr size_t PIXEL_COUNT = TOTAL_DIM * TOTAL_DIM; // 82,944 samples

    uint32_t tile_x = 0;
    uint32_t tile_y = 0;
    bool in_use = false;

    // Buffer aligned to 16 KB page boundary
    AlignedBuffer<int16_t> samples{PIXEL_COUNT, PAGE_SIZE_16KB};
};

/**
 * @brief Pre-allocated 12-Slot Ring Buffer Pool for 4K video processing.
 *
 * Enforces the Zero-Full-Frame-Heap Policy:
 * 12 slots x 82,944 samples x 2 bytes = 1.99 MB working RAM.
 * Other process allocations are not included in this bound.
 */
class TilePool {
public:
    static constexpr size_t NUM_SLOTS = MAX_TILE_SLOTS; // 12

    TilePool();
    ~TilePool() = default;

    TilePool(const TilePool&) = delete;
    TilePool& operator=(const TilePool&) = delete;

    /**
     * @brief Acquire a tile slot, reusing the oldest slot when all are in use.
     */
    SpatialTile* acquire_slot(uint32_t tile_x, uint32_t tile_y);

    /**
     * @brief Release tile back to the pool.
     */
    void release_slot(SpatialTile* tile);

    /**
     * @brief Calculate total memory footprint in bytes.
     */
    size_t get_total_allocated_bytes() const;

    /**
     * @brief Reset all slots.
     */
    void reset();

private:
    std::array<std::unique_ptr<SpatialTile>, NUM_SLOTS> slots_;
    mutable std::mutex mutex_;
    size_t round_robin_head_ = 0;
};

} // namespace prism

#endif // PRISM_TILE_POOL_HPP
