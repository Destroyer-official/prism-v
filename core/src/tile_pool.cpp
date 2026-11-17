#include "prism/tile_pool.hpp"
#include <algorithm>
#include <stdexcept>

namespace prism {

TilePool::TilePool(size_t num_slots) {
    size_t target_slots = std::max<size_t>(1, num_slots);
    size_t max_allowed_slots = HARD_MAX_BUDGET_BYTES / TILE_BYTES;
    target_slots = std::min(target_slots, max_allowed_slots);

    slots_.resize(target_slots);
    for (size_t i = 0; i < target_slots; ++i) {
        slots_[i] = std::make_unique<SpatialTile>();
        slots_[i]->in_use = false;
    }
}

void TilePool::configure_budget(size_t max_budget_bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t safe_budget = std::min(max_budget_bytes, HARD_MAX_BUDGET_BYTES);
    size_t target_slots = std::max<size_t>(4, safe_budget / TILE_BYTES);

    if (target_slots == slots_.size()) {
        return;
    }

    if (target_slots > slots_.size()) {
        size_t current_size = slots_.size();
        slots_.resize(target_slots);
        for (size_t i = current_size; i < target_slots; ++i) {
            slots_[i] = std::make_unique<SpatialTile>();
            slots_[i]->in_use = false;
        }
    } else {
        // Shrink safely only if slots beyond target are not in use
        size_t new_size = target_slots;
        while (new_size < slots_.size() && slots_.back() && !slots_.back()->in_use) {
            slots_.pop_back();
        }
    }

    if (round_robin_head_ >= slots_.size()) {
        round_robin_head_ = 0;
    }
}

SpatialTile* TilePool::acquire_tile(uint32_t tile_x, uint32_t tile_y) {
    std::lock_guard<std::mutex> lock(mutex_);
    const size_t num_slots = slots_.size();
    if (num_slots == 0) return nullptr;

    // Fast-path: Look for an unused slot starting from head
    for (size_t i = 0; i < num_slots; ++i) {
        size_t idx = (round_robin_head_ + i) % num_slots;
        if (!slots_[idx]->in_use) {
            slots_[idx]->in_use = true;
            slots_[idx]->tile_x = tile_x;
            slots_[idx]->tile_y = tile_y;
            round_robin_head_ = (idx + 1) % num_slots;
            return slots_[idx].get();
        }
    }

    // Under ring backpressure: reuse the slot at head to prevent unbounded heap allocations
    size_t idx = round_robin_head_;
    slots_[idx]->in_use = true;
    slots_[idx]->tile_x = tile_x;
    slots_[idx]->tile_y = tile_y;
    round_robin_head_ = (idx + 1) % num_slots;
    return slots_[idx].get();
}

void TilePool::release_tile(SpatialTile* tile) {
    if (!tile) return;
    std::lock_guard<std::mutex> lock(mutex_);
    tile->in_use = false;
}

size_t TilePool::get_total_allocated_bytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return slots_.size() * TILE_BYTES;
}

size_t TilePool::get_capacity() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return slots_.size();
}

size_t TilePool::get_in_use_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& slot : slots_) {
        if (slot && slot->in_use) {
            count++;
        }
    }
    return count;
}

void TilePool::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < slots_.size(); ++i) {
        if (slots_[i]) {
            slots_[i]->in_use = false;
            slots_[i]->tile_x = 0;
            slots_[i]->tile_y = 0;
        }
    }
    round_robin_head_ = 0;
}

} // namespace prism
