#include "prism/tile_pool.hpp"
#include <stdexcept>

namespace prism {

TilePool::TilePool() {
    for (size_t i = 0; i < NUM_SLOTS; ++i) {
        slots_[i] = std::make_unique<SpatialTile>();
        slots_[i]->in_use = false;
    }
}

SpatialTile* TilePool::acquire_slot(uint32_t tile_x, uint32_t tile_y) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Look for an unused slot
    for (size_t i = 0; i < NUM_SLOTS; ++i) {
        size_t idx = (round_robin_head_ + i) % NUM_SLOTS;
        if (!slots_[idx]->in_use) {
            slots_[idx]->in_use = true;
            slots_[idx]->tile_x = tile_x;
            slots_[idx]->tile_y = tile_y;
            round_robin_head_ = (idx + 1) % NUM_SLOTS;
            return slots_[idx].get();
        }
    }

    // If all in use, reuse the slot at head under ring backpressure
    size_t idx = round_robin_head_;
    slots_[idx]->in_use = true;
    slots_[idx]->tile_x = tile_x;
    slots_[idx]->tile_y = tile_y;
    round_robin_head_ = (idx + 1) % NUM_SLOTS;
    return slots_[idx].get();
}

void TilePool::release_slot(SpatialTile* tile) {
    if (!tile) return;
    std::lock_guard<std::mutex> lock(mutex_);
    tile->in_use = false;
}

size_t TilePool::get_total_allocated_bytes() const {
    return NUM_SLOTS * SpatialTile::PIXEL_COUNT * sizeof(int16_t);
}

void TilePool::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < NUM_SLOTS; ++i) {
        slots_[i]->in_use = false;
        slots_[i]->tile_x = 0;
        slots_[i]->tile_y = 0;
    }
    round_robin_head_ = 0;
}

} // namespace prism
