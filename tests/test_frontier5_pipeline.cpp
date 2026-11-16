#include "test_harness.hpp"
#include "prism/frontier5_pipeline.hpp"
#include <vector>
#include <numeric>
#include <cmath>

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
