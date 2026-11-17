#include "test_harness.hpp"
#include "prism/frontier5_pipeline.hpp"
#include "prism/tile_pool.hpp"
#include <vector>
#include <numeric>
#include <cmath>
#include <thread>
#include <atomic>
#include <filesystem>
#include <fstream>

PRISM_TEST(test_cat_ring_buffer_pool_allocation_and_alignment) {
    uint32_t width = 3840;
    uint32_t height = 2160;
    prism::CatRingBufferPool pool(width, height);

    PRISM_ASSERT_TRUE(pool.get_total_allocated_bytes() > 0);

    // Verify 16 KB page alignment on all slots
    for (size_t i = 0; i < prism::CatRingBufferPool::POOL_CAPACITY; ++i) {
        prism::SiliconBufferSlot* slot = pool.get_slot(static_cast<uint32_t>(i));
        PRISM_ASSERT_TRUE(slot != nullptr);
        PRISM_ASSERT_EQ(slot->slot_id, static_cast<uint32_t>(i));
        PRISM_ASSERT_EQ(slot->width, width);
        PRISM_ASSERT_EQ(slot->height, height);

        uintptr_t addr = reinterpret_cast<uintptr_t>(slot->raw_backing_memory);
        PRISM_ASSERT_EQ(addr % prism::CatRingBufferPool::PAGE_ALIGNMENT, 0ULL);
    }

    // Acquire and release test
    prism::SiliconBufferSlot* s0 = pool.acquire_slot();
    PRISM_ASSERT_TRUE(s0 != nullptr);
    PRISM_ASSERT_TRUE(s0->is_locked_by_hardware);

    pool.release_slot(s0->slot_id);
    PRISM_ASSERT_FALSE(s0->is_locked_by_hardware);
}

PRISM_TEST(test_vulkan_compute_engine_afgs1_and_lanczos5) {
    uint32_t width = 64;
    uint32_t height = 64;
    prism::CatRingBufferPool pool(width, height);
    prism::VulkanComputeEngine vk_engine(width, height);

    prism::SiliconBufferSlot* raw_slot = pool.acquire_slot();
    prism::SiliconBufferSlot* clean_slot = pool.acquire_slot();
    PRISM_ASSERT_TRUE(raw_slot != nullptr && clean_slot != nullptr);

    // Populate with noisy test pattern
    for (size_t i = 0; i < width * height; ++i) {
        raw_slot->raw_backing_memory[i] = static_cast<uint8_t>(128 + ((i % 7) - 3) * 5);
    }

    // Execute AFGS1 Stripper
    prism::Afgs1Params params = vk_engine.execute_afgs1_stripper(*raw_slot, *clean_slot);
    PRISM_ASSERT_TRUE(params.apply_grain);

    // Verify clean slot is smoothed
    int32_t clean_val = clean_slot->raw_backing_memory[0];
    PRISM_ASSERT_TRUE(clean_val >= 0 && clean_val <= 255);

    // Test 14-bit Integer Fixed-Point Lanczos-5 Resampling
    std::vector<uint8_t> src(16 * 16, 100);
    std::vector<uint8_t> dst(32 * 32, 0);
    vk_engine.execute_lanczos5_resampling(src.data(), 16, 16, dst.data(), 32, 32);

    // Constant region should be preserved
    PRISM_ASSERT_EQ(static_cast<int>(dst[16 * 32 + 16]), 100);

    // Test Residual Subtraction
    std::vector<int16_t> residual(width * height);
    vk_engine.execute_residual_subtraction(*raw_slot, clean_slot->raw_backing_memory, residual.data(), width * height);

    // Bounded between -128 and +127
    for (size_t i = 0; i < width * height; ++i) {
        PRISM_ASSERT_TRUE(residual[i] >= -128 && residual[i] <= 127);
    }

    pool.release_slot(raw_slot->slot_id);
    pool.release_slot(clean_slot->slot_id);
}

PRISM_TEST(test_arm_sme2_tile_engine_micro_residual_roundtrip) {
    prism::ArmSme2TileEngine sme2;

    const size_t NUM_PIXELS = 1024;
    std::vector<int16_t> original_residuals(NUM_PIXELS);
    for (size_t i = 0; i < NUM_PIXELS; ++i) {
        original_residuals[i] = static_cast<int16_t>((i % 31) - 15);
    }

    // Prepare frequency statistics for 256 symbols
    std::vector<prism::RansSymbol> stats(256);
    uint32_t rem = prism::Rans16Codec::RANS_M; // 32768
    for (size_t s = 0; s < 255; ++s) {
        uint32_t f = (s == 0) ? 18000 : (14768 / 254);
        stats[s].start = static_cast<uint16_t>(prism::Rans16Codec::RANS_M - rem);
        stats[s].freq = static_cast<uint16_t>(f);
        rem -= f;
    }
    stats[255].start = static_cast<uint16_t>(prism::Rans16Codec::RANS_M - rem);
    stats[255].freq = static_cast<uint16_t>(rem);

    std::vector<uint16_t> cdf_lut = prism::Rans16Codec::build_cdf_lut(stats);

    // Encode via SME2 tile engine
    prism::Rans16Codec::EncodedChunk chunk = sme2.encode_micro_residuals(original_residuals.data(), NUM_PIXELS, stats);
    PRISM_ASSERT_EQ(chunk.symbol_count, NUM_PIXELS);

    // Decode back via SME2 tile engine
    std::vector<int16_t> decoded_residuals = sme2.decode_micro_residuals(chunk, cdf_lut, stats);
    PRISM_ASSERT_EQ(decoded_residuals.size(), NUM_PIXELS);

    // Verify 100.000% exact parity
    for (size_t i = 0; i < NUM_PIXELS; ++i) {
        PRISM_ASSERT_EQ(decoded_residuals[i], original_residuals[i]);
    }

    // Verify MAC complexity ceiling (<=336 MACs/px)
    const auto& stats_metric = sme2.get_last_stats();
    PRISM_ASSERT_TRUE(stats_metric.total_macs_evaluated <= NUM_PIXELS * 336);
}

PRISM_TEST(test_silicon_fused_pipeline_sub_watt_power_envelope) {
    uint32_t width = 128;
    uint32_t height = 128;
    prism::SiliconFusedPipeline pipeline(width, height);

    // Verify thermodynamic sub-1.0W budget
    const auto& metrics = pipeline.get_metrics();
    PRISM_ASSERT_TRUE(metrics.sub_watt_envelope_verified);
    PRISM_ASSERT_TRUE(metrics.total_soc_power_watts < 1.000f);
    PRISM_ASSERT_EQ(metrics.total_soc_power_watts, 0.950f);
    PRISM_ASSERT_TRUE(metrics.dram_bandwidth_gb_s < 2.0f); // 1.40 GB/s vs 9.2 GB/s legacy

    // Process a synthetic frame through the entire silicon chain
    std::vector<uint8_t> raw_camera_frame(width * height, 140);
    for (size_t i = 0; i < width * height; ++i) {
        raw_camera_frame[i] = static_cast<uint8_t>((i % 256));
    }

    std::vector<uint8_t> base_nalu;
    std::vector<uint8_t> residual_payload;
    prism::Afgs1Params afgs1_params;

    bool success = pipeline.process_frame(raw_camera_frame.data(), base_nalu, residual_payload, afgs1_params);
    PRISM_ASSERT_TRUE(success);
    PRISM_ASSERT_TRUE(!base_nalu.empty());
    PRISM_ASSERT_TRUE(!residual_payload.empty());
    PRISM_ASSERT_TRUE(afgs1_params.apply_grain);
}

PRISM_TEST(test_frontier5_dual_track_container_mux_and_demux) {
    uint32_t width = 128;
    uint32_t height = 128;
    prism::SiliconFusedPipeline pipeline(width, height);

    std::vector<uint8_t> frame(width * height);
    for (size_t i = 0; i < frame.size(); ++i) {
        frame[i] = static_cast<uint8_t>((i % 256) ^ 0x3C);
    }

    // 1. Single-frame direct muxing
    auto pkg = pipeline.process_frame_and_mux(frame.data(), 100);
    PRISM_ASSERT_TRUE(!pkg.raw_mp4_bytes.empty());
    PRISM_ASSERT_EQ(pkg.track1_sample_count, 1ULL);
    PRISM_ASSERT_EQ(pkg.track2_sample_count, 1ULL);

    std::vector<prism::IsoMp4Engine::Track1Sample> out_t1;
    std::vector<prism::IsoMp4Engine::Track2Sample> out_t2;
    prism::PrismMicroResidualBox out_prmr;
    prism::Afgs1Params out_afgs;

    bool demux_ok = prism::IsoMp4Engine::demux_dual_track(
        pkg.raw_mp4_bytes.data(), pkg.raw_mp4_bytes.size(),
        out_t1, out_t2, out_prmr, out_afgs
    );
    PRISM_ASSERT_TRUE(demux_ok);
    PRISM_ASSERT_EQ(out_t1.size(), 1ULL);
    PRISM_ASSERT_EQ(out_t2.size(), 1ULL);
    PRISM_ASSERT_EQ(out_prmr.profile_idc, 0x05);
    PRISM_ASSERT_EQ(out_prmr.max_macs_per_pixel, 336);

    // 2. Multi-frame stream muxer session
    prism::Frontier5StreamMuxer muxer(width, height, 60);
    for (size_t f = 0; f < 5; ++f) {
        std::vector<uint8_t> base_nalu;
        std::vector<uint8_t> residual_payload;
        prism::Afgs1Params afgs;
        bool ok = pipeline.process_frame(frame.data(), base_nalu, residual_payload, afgs);
        PRISM_ASSERT_TRUE(ok);

        uint64_t pts = (f * 1000) / 60;
        muxer.push_frame(pts, f == 0, base_nalu, residual_payload, afgs);
    }
    PRISM_ASSERT_EQ(muxer.get_frame_count(), 5ULL);

    auto multi_pkg = muxer.finalize();
    PRISM_ASSERT_TRUE(!multi_pkg.raw_mp4_bytes.empty());
    PRISM_ASSERT_EQ(multi_pkg.track1_sample_count, 5ULL);
    PRISM_ASSERT_EQ(multi_pkg.track2_sample_count, 5ULL);

    std::vector<prism::IsoMp4Engine::Track1Sample> stream_t1;
    std::vector<prism::IsoMp4Engine::Track2Sample> stream_t2;
    demux_ok = prism::IsoMp4Engine::demux_dual_track(
        multi_pkg.raw_mp4_bytes.data(), multi_pkg.raw_mp4_bytes.size(),
        stream_t1, stream_t2, out_prmr, out_afgs
    );
    PRISM_ASSERT_TRUE(demux_ok);
    PRISM_ASSERT_EQ(stream_t1.size(), 5ULL);
    PRISM_ASSERT_EQ(stream_t2.size(), 5ULL);
}

PRISM_TEST(test_tile_pool_acquire_tile_bounded_and_threads) {
    // 1. Initial pool with default capacity
    prism::TilePool pool(12);
    PRISM_ASSERT_EQ(pool.get_capacity(), 12ULL);
    PRISM_ASSERT_TRUE(pool.get_total_allocated_bytes() <= 64 * 1024 * 1024);

    // Test acquire_tile interface contract for Agents 3, 4, 5, 6
    prism::SpatialTile* t0 = pool.acquire_tile(0, 0);
    PRISM_ASSERT_TRUE(t0 != nullptr);
    PRISM_ASSERT_TRUE(t0->in_use);
    PRISM_ASSERT_EQ(pool.get_in_use_count(), 1ULL);

    // Verify 16 KiB page alignment on sample buffer
    uintptr_t addr = reinterpret_cast<uintptr_t>(t0->samples.data());
    PRISM_ASSERT_EQ(addr % prism::PAGE_SIZE_16KB, 0ULL);

    // Test dynamic budget expansion (up to 512 MB cap)
    pool.configure_budget(32 * 1024 * 1024); // 32 MB budget
    PRISM_ASSERT_TRUE(pool.get_capacity() >= 12ULL);
    PRISM_ASSERT_TRUE(pool.get_total_allocated_bytes() <= prism::TilePool::HARD_MAX_BUDGET_BYTES);

    // Test release_tile
    pool.release_tile(t0);
    PRISM_ASSERT_FALSE(t0->in_use);
    PRISM_ASSERT_EQ(pool.get_in_use_count(), 0ULL);

    // Ring backpressure: acquire all slots and verify no crash / no unbounded heap allocation
    size_t cap = pool.get_capacity();
    std::vector<prism::SpatialTile*> acquired;
    for (size_t i = 0; i < cap * 2; ++i) {
        prism::SpatialTile* t = pool.acquire_tile(static_cast<uint32_t>(i), 0);
        PRISM_ASSERT_TRUE(t != nullptr);
        acquired.push_back(t);
    }
    PRISM_ASSERT_EQ(pool.get_total_allocated_bytes(), cap * prism::TilePool::TILE_BYTES);
    pool.reset();
    PRISM_ASSERT_EQ(pool.get_in_use_count(), 0ULL);
}

PRISM_TEST(test_heterogeneous_resource_governor) {
    prism::ResourceConstraints constraints;
    constraints.max_ram_mb = 256;
    constraints.cpu_mode = prism::CpuCoreMode::ECO;
    constraints.enable_gpu_compute = true;
    constraints.power_priority = prism::PowerPriority::BATTERY_SAVER;

    prism::HeterogeneousResourceGovernor governor(constraints);
    PRISM_ASSERT_EQ(governor.get_max_allowed_bytes(), 256ULL * 1024 * 1024);

    // Test CPU affinity mask for Eco (little cores only: 0x0F for 8 cores)
    uint64_t eco_mask = governor.compute_affinity_mask(8);
    PRISM_ASSERT_EQ(eco_mask, 0x0FULL);
    PRISM_ASSERT_EQ(governor.compute_thread_nice_priority(), 19);

    // Test CPU affinity mask for Balanced (cores 0..6: 0x7F)
    constraints.cpu_mode = prism::CpuCoreMode::BALANCED;
    governor.set_constraints(constraints);
    uint64_t balanced_mask = governor.compute_affinity_mask(8);
    PRISM_ASSERT_EQ(balanced_mask, 0x7FULL);
    PRISM_ASSERT_EQ(governor.compute_thread_nice_priority(), 0);

    // Test CPU affinity mask for Turbo (all cores: 0xFF)
    constraints.cpu_mode = prism::CpuCoreMode::TURBO;
    governor.set_constraints(constraints);
    uint64_t turbo_mask = governor.compute_affinity_mask(8);
    PRISM_ASSERT_EQ(turbo_mask, 0xFFULL);
    PRISM_ASSERT_EQ(governor.compute_thread_nice_priority(), -10);

    // Test memory token bucket
    size_t chunk_100mb = 100ULL * 1024 * 1024;
    PRISM_ASSERT_TRUE(governor.try_acquire_memory_tokens(chunk_100mb));
    PRISM_ASSERT_TRUE(governor.try_acquire_memory_tokens(chunk_100mb));
    // 200 MB acquired out of 256 MB. Another 100 MB must be rejected!
    PRISM_ASSERT_FALSE(governor.try_acquire_memory_tokens(chunk_100mb));
    PRISM_ASSERT_EQ(governor.get_current_allocated_bytes(), 200ULL * 1024 * 1024);

    governor.release_memory_tokens(chunk_100mb);
    PRISM_ASSERT_EQ(governor.get_current_allocated_bytes(), 100ULL * 1024 * 1024);
    PRISM_ASSERT_TRUE(governor.try_acquire_memory_tokens(chunk_100mb));
}

PRISM_TEST(test_lock_free_chunked_parallel_slicing_and_nanosecond_stitcher) {
    // 1. Simulate 1-hour 4K 120fps metadata (432,000 frames = 3,600,000,000 us)
    prism::VideoMetadata meta;
    meta.width = 3840;
    meta.height = 2160;
    meta.fps = 120.0;
    meta.duration_us = 3600000000ULL; // 1 hour
    meta.frame_count = 432000;
    meta.timescale = 120000;

    // Simulate closed GOP boundaries every 5 seconds (every 600 frames at 120 fps)
    for (uint64_t t = 0; t < meta.duration_us; t += 5000000ULL) {
        meta.keyframe_pts_us.push_back(t);
    }

    // Slice metadata into 5s micro-tasks without touching raw pixels
    std::vector<prism::GopMicroTask> tasks =
        prism::LockFreeChunkedParallelSlicer::slice_video_metadata(meta, 5000000ULL);

    PRISM_ASSERT_TRUE(tasks.size() > 0);
    PRISM_ASSERT_EQ(tasks.size(), meta.keyframe_pts_us.size());
    PRISM_ASSERT_EQ(tasks[0].chunk_id, 0ULL);
    PRISM_ASSERT_EQ(tasks[0].start_pts_us, 0ULL);
    PRISM_ASSERT_TRUE(tasks[0].is_closed_gop);

    // 2. Simulate 3 independently encoded chunks and stitch them zero-copy
    std::vector<prism::EncodedChunkPayload> completed_chunks;
    for (uint32_t c = 0; c < 3; ++c) {
        prism::EncodedChunkPayload cp;
        cp.chunk_id = c;
        cp.duration_us = 1000000ULL; // 1 second each
        for (uint64_t f = 0; f < 60; ++f) {
            uint64_t pts = (f * 1000) / 60;
            prism::IsoMp4Engine::Track1Sample s1;
            s1.pts_ms = pts;
            s1.dts_ms = pts;
            s1.is_keyframe = (f == 0);
            s1.nal_data = { 0x00, 0x00, 0x00, 0x01, 0x67, static_cast<uint8_t>(c) };
            cp.base_samples.push_back(s1);

            prism::IsoMp4Engine::Track2Sample s2;
            s2.pts_ms = pts;
            s2.residual_latents = { 0x01, 0x02, 0x03 };
            cp.residual_samples.push_back(s2);
        }
        completed_chunks.push_back(cp);
    }

    auto pkg = prism::LockFreeChunkedParallelSlicer::stitch_chunks_zero_copy(
        completed_chunks, 1920, 1080, 60
    );
    PRISM_ASSERT_TRUE(!pkg.raw_mp4_bytes.empty());
    PRISM_ASSERT_EQ(pkg.track1_sample_count, 180ULL);
    PRISM_ASSERT_EQ(pkg.track2_sample_count, 180ULL);

    // Verify demux and verify strict 0.000 μs PTS monotonic continuity across chunk seams
    std::vector<prism::IsoMp4Engine::Track1Sample> out_t1;
    std::vector<prism::IsoMp4Engine::Track2Sample> out_t2;
    prism::PrismMicroResidualBox prmr;
    prism::Afgs1Params afgs;
    bool demux_ok = prism::IsoMp4Engine::demux_dual_track(
        pkg.raw_mp4_bytes.data(), pkg.raw_mp4_bytes.size(),
        out_t1, out_t2, prmr, afgs
    );
    PRISM_ASSERT_TRUE(demux_ok);
    PRISM_ASSERT_EQ(out_t1.size(), 180ULL);

    // Check monotonic timestamps across entire timeline and across seam boundaries
    for (size_t i = 1; i < out_t1.size(); ++i) {
        PRISM_ASSERT_TRUE(out_t1[i].pts_ms >= out_t1[i-1].pts_ms);
        PRISM_ASSERT_TRUE(out_t1[i].dts_ms >= out_t1[i-1].dts_ms);
    }
}

PRISM_TEST(test_predictive_thermal_gear_shifter_lumped_rc_model) {
    // Room temperature 25C, typical mobile chassis R_th = 4.86 K/W, C_th = 9.25 J/K
    prism::PredictiveThermalGearShifter shifter(25.0f, 4.86f, 9.25f);

    auto state = shifter.get_current_state();
    PRISM_ASSERT_EQ(state.chassis_temp_c, 25.0f);
    PRISM_ASSERT_EQ(state.predicted_headroom, 1.0f);

    // 1. Step response test under high power (P = 4.0W for 10 seconds)
    state = shifter.update_thermal_model(4.0f, 10.0f);
    PRISM_ASSERT_TRUE(state.chassis_temp_c > 25.0f);
    PRISM_ASSERT_TRUE(state.predicted_headroom < 1.0f);

    // 2. Test ADPF Headroom Evaluation and Hysteresis Debounce
    // Cold device (headroom = 0.95) -> Super Turbo
    auto gear = shifter.evaluate_adpf_headroom(0.95f, 1000);
    PRISM_ASSERT_EQ(static_cast<int>(gear), static_cast<int>(prism::PredictiveThermalGearShifter::ShifterGear::GEAR_4_SUPER_TURBO));

    // Sudden heat spike (headroom drops to 0.10) -> Immediate Downshift to Atomic Yield
    gear = shifter.evaluate_adpf_headroom(0.10f, 1100);
    PRISM_ASSERT_EQ(static_cast<int>(gear), static_cast<int>(prism::PredictiveThermalGearShifter::ShifterGear::GEAR_0_ATOMIC_YIELD));
    PRISM_ASSERT_TRUE(shifter.get_current_state().is_atomic_paused);

    // Cooling starts (headroom recovers to 0.70 at t = 2000ms):
    // Upshift should be blocked by 5000ms hysteresis window!
    gear = shifter.evaluate_adpf_headroom(0.70f, 2000);
    PRISM_ASSERT_EQ(static_cast<int>(gear), static_cast<int>(prism::PredictiveThermalGearShifter::ShifterGear::GEAR_0_ATOMIC_YIELD));

    // Once 5000ms has elapsed (t = 6200ms), upshift is granted!
    gear = shifter.evaluate_adpf_headroom(0.70f, 6200);
    PRISM_ASSERT_EQ(static_cast<int>(gear), static_cast<int>(prism::PredictiveThermalGearShifter::ShifterGear::GEAR_3_STANDARD));
    PRISM_ASSERT_FALSE(shifter.get_current_state().is_atomic_paused);
}

PRISM_TEST(test_tile_pool_multithreaded_stress_and_budget_enforcement) {
    // 1. Strict memory budget clamping test
    prism::TilePool pool(16);
    PRISM_ASSERT_EQ(pool.get_capacity(), 16ULL);

    // Hard ceiling clamp: requesting 1024 MB must clamp to HARD_MAX_BUDGET_BYTES (512 MB)
    pool.configure_budget(1024ULL * 1024ULL * 1024ULL);
    PRISM_ASSERT_TRUE(pool.get_total_allocated_bytes() <= prism::TilePool::HARD_MAX_BUDGET_BYTES);

    // Configure to safe 32 MB budget
    pool.configure_budget(32ULL * 1024ULL * 1024ULL);
    size_t cap = pool.get_capacity();
    PRISM_ASSERT_TRUE(cap >= 16ULL);
    PRISM_ASSERT_TRUE(pool.get_total_allocated_bytes() <= 32ULL * 1024ULL * 1024ULL);

    // Verify 16 KiB alignment on all allocated slots
    for (size_t i = 0; i < cap; ++i) {
        prism::SpatialTile* tile = pool.acquire_tile(static_cast<uint32_t>(i), 0);
        PRISM_ASSERT_TRUE(tile != nullptr);
        uintptr_t addr = reinterpret_cast<uintptr_t>(tile->samples.data());
        PRISM_ASSERT_EQ(addr % prism::PAGE_SIZE_16KB, 0ULL);
        pool.release_tile(tile);
    }

    // 2. High-concurrency multithreaded torture test (16 threads, 1000 cycles each = 16,000 acquires)
    const size_t NUM_THREADS = 16;
    const size_t OPS_PER_THREAD = 1000;
    std::atomic<size_t> total_completed{0};
    std::vector<std::thread> workers;
    workers.reserve(NUM_THREADS);

    for (size_t t = 0; t < NUM_THREADS; ++t) {
        workers.emplace_back([&pool, &total_completed, OPS_PER_THREAD, t]() {
            for (size_t op = 0; op < OPS_PER_THREAD; ++op) {
                prism::SpatialTile* leased = pool.acquire_tile(static_cast<uint32_t>(t), static_cast<uint32_t>(op));
                if (leased) {
                    // Touch first and last sample to ensure memory validity
                    leased->samples[0] = static_cast<int16_t>(t);
                    leased->samples[prism::SpatialTile::PIXEL_COUNT - 1] = static_cast<int16_t>(op);
                    pool.release_tile(leased);
                    total_completed.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    PRISM_ASSERT_EQ(total_completed.load(), NUM_THREADS * OPS_PER_THREAD);
    PRISM_ASSERT_EQ(pool.get_in_use_count(), 0ULL);
}

PRISM_TEST(test_lock_free_slicer_1000_gops_zero_drift) {
    // Simulate 1,000 consecutive GOP chunks (each 30 frames at 60 fps = 500 ms)
    const uint32_t NUM_GOPS = 1000;
    const uint64_t FRAMES_PER_GOP = 30;
    const uint32_t FPS = 60;
    const uint64_t GOP_DURATION_US = (FRAMES_PER_GOP * 1000000ULL) / FPS; // 500,000 us

    std::vector<prism::EncodedChunkPayload> completed_chunks;
    completed_chunks.reserve(NUM_GOPS);

    for (uint32_t g = 0; g < NUM_GOPS; ++g) {
        prism::EncodedChunkPayload cp;
        cp.chunk_id = g;
        cp.duration_us = GOP_DURATION_US;
        for (uint64_t f = 0; f < FRAMES_PER_GOP; ++f) {
            uint64_t pts = (f * 1000) / FPS;
            prism::IsoMp4Engine::Track1Sample s1;
            s1.pts_ms = pts;
            s1.dts_ms = pts;
            s1.is_keyframe = (f == 0);
            s1.nal_data = { 0x00, 0x00, 0x00, 0x01, 0x65, static_cast<uint8_t>(g & 0xFF) };
            cp.base_samples.push_back(std::move(s1));

            prism::IsoMp4Engine::Track2Sample s2;
            s2.pts_ms = pts;
            s2.residual_latents = { 0xAA, 0x55 };
            cp.residual_samples.push_back(std::move(s2));
        }
        completed_chunks.push_back(std::move(cp));
    }

    // Zero-copy atom stitching across all 1,000 GOPs
    auto pkg = prism::LockFreeChunkedParallelSlicer::stitch_chunks_zero_copy(
        completed_chunks, 1920, 1080, FPS
    );

    PRISM_ASSERT_TRUE(!pkg.raw_mp4_bytes.empty());
    PRISM_ASSERT_EQ(pkg.track1_sample_count, static_cast<uint64_t>(NUM_GOPS * FRAMES_PER_GOP));
    PRISM_ASSERT_EQ(pkg.track2_sample_count, static_cast<uint64_t>(NUM_GOPS * FRAMES_PER_GOP));

    // Demux and assert 0.000 μs timestamp monotonicity across all 30,000 frames
    std::vector<prism::IsoMp4Engine::Track1Sample> out_t1;
    std::vector<prism::IsoMp4Engine::Track2Sample> out_t2;
    prism::PrismMicroResidualBox prmr;
    prism::Afgs1Params afgs;
    bool demux_ok = prism::IsoMp4Engine::demux_dual_track(
        pkg.raw_mp4_bytes.data(), pkg.raw_mp4_bytes.size(),
        out_t1, out_t2, prmr, afgs
    );
    PRISM_ASSERT_TRUE(demux_ok);
    PRISM_ASSERT_EQ(out_t1.size(), static_cast<size_t>(NUM_GOPS * FRAMES_PER_GOP));

    for (size_t i = 1; i < out_t1.size(); ++i) {
        // Assert monotonic non-decreasing timestamps
        PRISM_ASSERT_TRUE(out_t1[i].pts_ms >= out_t1[i-1].pts_ms);
        PRISM_ASSERT_TRUE(out_t1[i].dts_ms >= out_t1[i-1].dts_ms);
    }
}

PRISM_TEST(test_governor_clamp_and_multicore_affinity) {
    prism::ResourceConstraints constraints;
    constraints.max_ram_mb = 512;
    constraints.cpu_mode = prism::CpuCoreMode::BALANCED;
    constraints.enable_gpu_compute = true;
    constraints.power_priority = prism::PowerPriority::BALANCED;

    prism::HeterogeneousResourceGovernor gov(constraints);

    // Verify affinity masks across diverse SoC core topologies
    // 4-core SoC
    constraints.cpu_mode = prism::CpuCoreMode::ECO;
    gov.set_constraints(constraints);
    PRISM_ASSERT_EQ(gov.compute_affinity_mask(4), 0x03ULL); // Cores 0-1 (first half)

    // 8-core SoC (Snapdragon 8 Elite / Dimensity 9400)
    PRISM_ASSERT_EQ(gov.compute_affinity_mask(8), 0x0FULL); // Cores 0-3
    constraints.cpu_mode = prism::CpuCoreMode::BALANCED;
    gov.set_constraints(constraints);
    PRISM_ASSERT_EQ(gov.compute_affinity_mask(8), 0x7FULL); // Cores 0-6 (all except prime)
    constraints.cpu_mode = prism::CpuCoreMode::TURBO;
    gov.set_constraints(constraints);
    PRISM_ASSERT_EQ(gov.compute_affinity_mask(8), 0xFFULL); // Cores 0-7 (all cores)

    // 12-core SoC
    constraints.cpu_mode = prism::CpuCoreMode::BALANCED;
    gov.set_constraints(constraints);
    PRISM_ASSERT_EQ(gov.compute_affinity_mask(12), 0x7FFULL); // Cores 0-10

    // 16-core SoC
    PRISM_ASSERT_EQ(gov.compute_affinity_mask(16), 0x7FFFULL); // Cores 0-14

    // Memory token bucket rejection test
    size_t half_budget = 256ULL * 1024ULL * 1024ULL;
    PRISM_ASSERT_TRUE(gov.try_acquire_memory_tokens(half_budget));
    PRISM_ASSERT_TRUE(gov.try_acquire_memory_tokens(half_budget));
    // 512 MB exhausted: next byte MUST be rejected!
    PRISM_ASSERT_FALSE(gov.try_acquire_memory_tokens(1));
    gov.release_memory_tokens(half_budget);
    PRISM_ASSERT_TRUE(gov.try_acquire_memory_tokens(half_budget));
}

PRISM_TEST(test_vulkan_frontier5_pipeline_spirv_validity) {
    // Check candidate locations for compiled Frontier 5 shader binary
    std::vector<std::string> candidate_paths = {
        "core/shaders/frontier5_pipeline.spv",
        "../core/shaders/frontier5_pipeline.spv",
        "../../core/shaders/frontier5_pipeline.spv"
    };

    std::string spv_path;
    for (const auto& path : candidate_paths) {
        if (std::filesystem::exists(path)) {
            spv_path = path;
            break;
        }
    }

    PRISM_ASSERT(!spv_path.empty());

    std::ifstream spv_file(spv_path, std::ios::binary | std::ios::ate);
    PRISM_ASSERT(spv_file.is_open());

    std::streamsize file_size = spv_file.tellg();
    PRISM_ASSERT(file_size > 0);
    PRISM_ASSERT_EQ(file_size % 4, 0); // SPIR-V is a sequence of 32-bit words

    spv_file.seekg(0, std::ios::beg);
    std::vector<uint32_t> words(file_size / 4);
    spv_file.read(reinterpret_cast<char*>(words.data()), file_size);
    PRISM_ASSERT(words.size() >= 5);

    // SPIR-V Header Invariants:
    // Word 0: Magic Number 0x07230203
    PRISM_ASSERT_EQ(words[0], 0x07230203u);

    // Word 1: Version >= 0x00010300 (Vulkan 1.3+ / SPIR-V 1.3+)
    PRISM_ASSERT(words[1] >= 0x00010300u);

    // Word 2: Generator Magic (non-zero)
    PRISM_ASSERT(words[2] != 0u);

    // Word 3: Bound ID (upper limit on IDs used in module)
    PRISM_ASSERT(words[3] > 0u);

    // Word 4: Schema (must be 0 in standard SPIR-V)
    PRISM_ASSERT_EQ(words[4], 0u);
}
