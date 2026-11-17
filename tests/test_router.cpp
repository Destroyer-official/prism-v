#include "test_harness.hpp"
#include "prism/router.hpp"
#include <vector>

using namespace prism;

PRISM_TEST(test_router_lossless_tri_contract_and_hybrid_dispatch) {
    // 1. Test Ingest Probe: H.264 Camera MP4 -> Contract A (Mode L-BBR)
    std::vector<uint8_t> h264_header = {
        0x00, 0x00, 0x00, 0x18, 'f', 't', 'y', 'p',
        'm', 'p', '4', '2', 0x00, 0x00, 0x00, 0x00,
        'i', 's', 'o', 'm', 'a', 'v', 'c', '1'
    };
    auto info_h264 = IngestRouter::probe_media(h264_header.data(), h264_header.size(), 50 * 1024 * 1024);
    PRISM_ASSERT_EQ(info_h264.family, CodecFamily::H264_AVC);
    PRISM_ASSERT(info_h264.is_container_standard);

    auto dec_h264 = IngestRouter::route_media(info_h264, true);
    PRISM_ASSERT_EQ(dec_h264.contract, LosslessContract::CONTRACT_A_BBR2);
    PRISM_ASSERT_EQ(dec_h264.mode_name, "Mode L-BBR 2.0");
    PRISM_ASSERT(dec_h264.is_lossless);

    // 2. Test Ingest Probe: Tight AV1 Stream (<3% estimated BBR savings) -> Contract B (Mode L-Remux)
    std::vector<uint8_t> av1_header = {
        0x00, 0x00, 0x00, 0x18, 'f', 't', 'y', 'p',
        'm', 'p', '4', '2', 0x00, 0x00, 0x00, 0x00,
        'i', 's', 'o', 'm', 'a', 'v', '0', '1'
    };
    auto info_av1 = IngestRouter::probe_media(av1_header.data(), av1_header.size(), 20 * 1024 * 1024);
    PRISM_ASSERT_EQ(info_av1.family, CodecFamily::AV1);
    PRISM_ASSERT(info_av1.estimated_bbr_savings_ratio < 0.03);

    auto dec_av1 = IngestRouter::route_media(info_av1, true);
    PRISM_ASSERT_EQ(dec_av1.contract, LosslessContract::CONTRACT_B_REMUX);
    PRISM_ASSERT_EQ(dec_av1.mode_name, "Mode L-Remux");

    // 3. Test Ingest Probe: Camera2 Bayer RAW Sensor Capture -> Contract C (Mode L-PSC Bayer)
    std::vector<uint8_t> bayer_header = {
        'I', 'I', '*', 0x00, 0x08, 0x00, 0x00, 0x00,
        0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    auto info_bayer = IngestRouter::probe_media(bayer_header.data(), bayer_header.size(), 40 * 1024 * 1024);
    PRISM_ASSERT(info_bayer.is_raw_sensor);
    PRISM_ASSERT_EQ(info_bayer.family, CodecFamily::RAW_BAYER_CFA);

    auto dec_bayer = IngestRouter::route_media(info_bayer, true);
    PRISM_ASSERT_EQ(dec_bayer.contract, LosslessContract::CONTRACT_C_PSC);
    PRISM_ASSERT_EQ(dec_bayer.recommended_codec_id, SourceCodecId::CODEC_BAYER_RAW10);
    PRISM_ASSERT_EQ(dec_bayer.mode_name, "Mode L-PSC (Bayer CFA)");

    // 4. Test Ingest Probe: Camera2 Quad-Bayer RAW Sensor Capture -> Contract C (Mode L-PSC 2.0)
    info_bayer.is_quad_bayer = true;
    auto dec_psc2_bayer = IngestRouter::route_media(info_bayer, true);
    PRISM_ASSERT_EQ(dec_psc2_bayer.contract, LosslessContract::CONTRACT_C_PSC2);
    PRISM_ASSERT_EQ(dec_psc2_bayer.recommended_codec_id, SourceCodecId::CODEC_QUAD_BAYER_RAW);
    PRISM_ASSERT_EQ(dec_psc2_bayer.mode_name, "Mode L-PSC 2.0 (Quad-Bayer CFA)");

    // 5. Test Raw YUV with prefer_psc2 -> Mode L-PSC 2.0 (YUV)
    IngestMediaInfo info_yuv;
    info_yuv.family = CodecFamily::RAW_YUV;
    info_yuv.prefer_psc2 = true;
    auto dec_psc2_yuv = IngestRouter::route_media(info_yuv, true);
    PRISM_ASSERT_EQ(dec_psc2_yuv.contract, LosslessContract::CONTRACT_C_PSC2);
    PRISM_ASSERT_EQ(dec_psc2_yuv.mode_name, "Mode L-PSC 2.0 (YUV)");

    // 6. Test Lossy Request: Mode Y-Hybrid
    auto dec_lossy = IngestRouter::route_media(info_h264, false); // user_requested_lossless = false
    PRISM_ASSERT(!dec_lossy.is_lossless);
    PRISM_ASSERT_EQ(dec_lossy.mode_name, "Mode Y-Hybrid");

    // 7. Test Ingest Probe: Matroska / WebM EBML Header -> Mode L-BBR 2.0 (Lossless) / Mode Y-Hybrid (Lossy)
    std::vector<uint8_t> mkv_header = {
        0x1A, 0x45, 0xDF, 0xA3, 0x85, 0x42, 0x82, 0x81, 'm',
        0x18, 0x53, 0x80, 0x67, 0x01, 0x00, 0x00
    };
    auto info_mkv = IngestRouter::probe_media(mkv_header.data(), mkv_header.size(), 234ULL * 1024ULL * 1024ULL);
    PRISM_ASSERT(info_mkv.is_container_standard);
    PRISM_ASSERT_EQ(info_mkv.family, CodecFamily::H264_AVC);

    auto dec_mkv_lossy = IngestRouter::route_media(info_mkv, false);
    PRISM_ASSERT(!dec_mkv_lossy.is_lossless);
    PRISM_ASSERT_EQ(dec_mkv_lossy.mode_name, "Mode Y-Hybrid");

    // 8. Test Hyper-Efficient Ingest Redirection (2027+ Never-Worse Invariant)
    // When a video has native bitrate < 1.5 Mbps for 1080p, lossy mode automatically
    // redirects to Mode L-BBR 2.0 to eliminate file inflation and preserve master quality.
    IngestMediaInfo info_hyper;
    info_hyper.width = 1920;
    info_hyper.height = 1080;
    info_hyper.duration_us = 1200000000ULL; // 20m 00s
    info_hyper.source_bitrate_bps = 1250000.0; // 1.25 Mbps (hyper-efficient for 1080p)
    info_hyper.family = CodecFamily::H264_AVC;
    info_hyper.is_container_standard = true;

    auto dec_hyper = IngestRouter::route_media(info_hyper, false); // user requested lossy
    PRISM_ASSERT(dec_hyper.is_lossless); // Must be redirected to lossless!
    PRISM_ASSERT_EQ(dec_hyper.contract, LosslessContract::CONTRACT_A_BBR2);
    PRISM_ASSERT_EQ(dec_hyper.mode_name, "Mode L-BBR 2.0 (Hyper-Efficient Redirection)");
}
