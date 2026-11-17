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
#include <atomic>

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
    std::atomic<bool> in_use{false};

    SpatialTile() = default;
    SpatialTile(const SpatialTile&) = delete;
    SpatialTile& operator=(const SpatialTile&) = delete;

    // Buffer aligned to 16 KB page boundary
    AlignedBuffer<int16_t> samples{PIXEL_COUNT, PAGE_SIZE_16KB};
};

/**
 * @brief Pre-allocated Memory-Bounded Ring Buffer Pool for 4K video processing.
 *
 * Enforces the Zero-Full-Frame-Heap Policy and Strict User Memory Budget:
 * Default 12 slots x 82,944 samples x 2 bytes = 1.99 MB working RAM.
 * Maximum user cap strictly bounded to < 512 MB to prevent Android Low Memory Killer (LMK) aborts.
 * Provides thread-safe acquire_tile / acquire_slot and release_tile / release_slot.
 */
class TilePool {
public:
    static constexpr size_t DEFAULT_SLOTS = MAX_TILE_SLOTS; // 12
    static constexpr size_t HARD_MAX_BUDGET_BYTES = 512 * 1024 * 1024; // 512 MB Hard Ceiling
    static constexpr size_t TILE_BYTES = SpatialTile::PIXEL_COUNT * sizeof(int16_t);

    static TilePool& instance() {
        static TilePool s_instance;
        return s_instance;
    }

    explicit TilePool(size_t num_slots = DEFAULT_SLOTS);
    ~TilePool() = default;

    TilePool(const TilePool&) = delete;
    TilePool& operator=(const TilePool&) = delete;
    TilePool(TilePool&&) = delete;
    TilePool& operator=(TilePool&&) = delete;

    /**
     * @brief Configure user memory budget in bytes (clamped to HARD_MAX_BUDGET_BYTES).
     */
    void configure_budget(size_t max_budget_bytes);

    /**
     * @brief Acquire a tile slot, reusing the oldest slot under ring backpressure when all are in use.
     * Complies with interface contract for Agents 3, 4, 5, 6.
     */
    SpatialTile* acquire_tile(uint32_t tile_x = 0, uint32_t tile_y = 0);

    /**
     * @brief Backward-compatible alias for acquire_tile.
     */
    SpatialTile* acquire_slot(uint32_t tile_x, uint32_t tile_y) {
        return acquire_tile(tile_x, tile_y);
    }

    /**
     * @brief Release tile back to the pool.
     */
    void release_tile(SpatialTile* tile);

    /**
     * @brief Backward-compatible alias for release_tile.
     */
    void release_slot(SpatialTile* tile) {
        release_tile(tile);
    }

    /**
     * @brief Calculate total memory footprint of the pool in bytes.
     */
    size_t get_total_allocated_bytes() const;

    /**
     * @brief Query number of available slots in pool.
     */
    size_t get_capacity() const;

    /**
     * @brief Query count of currently leased/in-use tiles.
     */
    size_t get_in_use_count() const;

    /**
     * @brief Reset all slots to free state.
     */
    void reset();

private:
    std::vector<std::unique_ptr<SpatialTile>> slots_;
    mutable std::mutex mutex_;
    size_t round_robin_head_ = 0;
};

} // namespace prism

#endif // PRISM_TILE_POOL_HPP
