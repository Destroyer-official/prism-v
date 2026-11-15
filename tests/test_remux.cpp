#include "test_harness.hpp"
#include "prism/remux.hpp"
#include <vector>

using namespace prism;

PRISM_TEST(test_remux_savings_threshold_predicate) {
    // Mode L-Remux (Contract B) triggers when bitstream transcoding savings < 3% (0.03)
    PRISM_ASSERT(RemuxEngine::should_use_remux(0.012)); // 1.2% -> use remux
    PRISM_ASSERT(RemuxEngine::should_use_remux(0.029)); // 2.9% -> use remux
    PRISM_ASSERT(!RemuxEngine::should_use_remux(0.031)); // 3.1% -> use L-BBR
    PRISM_ASSERT(!RemuxEngine::should_use_remux(0.180)); // 18% -> use L-BBR
}

PRISM_TEST(test_remux_packet_preservation_and_sha256_parity) {
    RemuxEngine::RemuxPackage src_pkg;

    // Video NALs
    RemuxEngine::RemuxPacket v1;
    v1.type = RemuxEngine::MediaType::VIDEO_NAL;
    v1.pts_ms = 0;
    v1.dts_ms = 0;
    v1.is_keyframe = true;
    v1.payload = {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xC0, 0x1E, 0xD9};
    src_pkg.packets.push_back(v1);

    RemuxEngine::RemuxPacket v2;
    v2.type = RemuxEngine::MediaType::VIDEO_NAL;
    v2.pts_ms = 33;
    v2.dts_ms = 33;
    v2.is_keyframe = false;
    v2.payload = {0x00, 0x00, 0x00, 0x01, 0x41, 0x9A, 0x73, 0x22};
    src_pkg.packets.push_back(v2);

    // Audio Frame
    RemuxEngine::RemuxPacket a1;
    a1.type = RemuxEngine::MediaType::AUDIO_FRAME;
    a1.pts_ms = 0;
    a1.payload = {0xFF, 0xF1, 0x50, 0x80, 0x11, 0x22, 0x33};
    src_pkg.packets.push_back(a1);

    // Metadata
    src_pkg.metadata.has_colr = true;
    src_pkg.metadata.colr.colour_primaries = 1;
    src_pkg.metadata.colr.transfer_characteristics = 1;
    src_pkg.metadata.matrix = TrackMatrix::create_rotation(180);

    src_pkg.payload_hash = RemuxEngine::compute_payload_hash(src_pkg.packets);

    // Remux to clean ISOBMFF container
    std::vector<uint8_t> mp4_stream = RemuxEngine::remux_to_clean_isobmff(src_pkg);
    PRISM_ASSERT(!mp4_stream.empty());

    // Demux and verify Contract B (100% bit-exact packet payload preservation)
    RemuxEngine::RemuxPackage dst_pkg;
    bool ok = RemuxEngine::parse_and_verify_isobmff(mp4_stream.data(), mp4_stream.size(), dst_pkg);
    PRISM_ASSERT(ok);
    PRISM_ASSERT_EQ(dst_pkg.video_packet_count, 2);
    PRISM_ASSERT_EQ(dst_pkg.audio_packet_count, 1);

    // Verify SHA-256 payload identity and packet bit-exactness
    bool preserved = RemuxEngine::verify_packet_preservation(src_pkg, dst_pkg);
    PRISM_ASSERT(preserved);
    PRISM_ASSERT_EQ(dst_pkg.payload_hash, src_pkg.payload_hash);
    PRISM_ASSERT_EQ(dst_pkg.metadata.matrix.get_rotation_degrees(), 180);
}
