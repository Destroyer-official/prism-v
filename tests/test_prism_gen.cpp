#include "test_harness.hpp"
#include "prism/prism_gen.hpp"
#include <vector>
#include <filesystem>
#include <fstream>

using namespace prism;

PRISM_TEST(test_dense_affine_warper_deterministic) {
    SemanticFrameVector ref{};
    SemanticFrameVector curr{};

    // Reference keypoints placed symmetrically
    for (size_t k = 0; k < SemanticFrameVector::KEYPOINT_COUNT; ++k) {
        ref.keypoints_3d[k * 3] = static_cast<int16_t>((static_cast<int32_t>(k) - 16) * 1000);
        ref.keypoints_3d[k * 3 + 1] = static_cast<int16_t>((static_cast<int32_t>(k) - 16) * 1000);
    }

    // Shift current keypoints to simulate head turn
    for (size_t k = 0; k < SemanticFrameVector::KEYPOINT_COUNT; ++k) {
        curr.keypoints_3d[k * 3] = static_cast<int16_t>(ref.keypoints_3d[k * 3] + 2000);
        curr.keypoints_3d[k * 3 + 1] = ref.keypoints_3d[k * 3 + 1];
    }
    curr.pose_6dof[3] = 100; // Translation X

    size_t width = 64;
    size_t height = 64;

    DenseAffineWarper::WarpField warp = DenseAffineWarper::compute_warp_field(ref, curr, width, height, 256);
    PRISM_ASSERT_EQ(warp.width, width);
    PRISM_ASSERT_EQ(warp.height, height);
    PRISM_ASSERT_EQ(warp.flow_grid.size(), width * height * 2);

    // Apply warp to a uniform test pattern
    std::vector<uint8_t> in_frame(width * height, 128);
    // Draw a bright square in center
    for (size_t y = 24; y < 40; ++y) {
        for (size_t x = 24; x < 40; ++x) {
            in_frame[y * width + x] = 240;
        }
    }

    std::vector<uint8_t> out_warped(width * height, 0);
    DenseAffineWarper::apply_warp(in_frame.data(), width, height, width, warp, out_warped.data());

    // Check that warped frame has valid pixel intensities
    for (uint8_t px : out_warped) {
        PRISM_ASSERT(px >= 128 && px <= 240);
    }
}

PRISM_TEST(test_prism_detint_flow_generator_bounds) {
    PrismDetIntFlowGenerator flow_gen;
    size_t width = 32;
    size_t height = 32;

    std::vector<uint8_t> warped(width * height, 120);
    std::vector<uint8_t> base(width * height, 130);
    std::vector<uint8_t> recon(width * height, 0);

    SemanticFrameVector semantic{};
    semantic.blendshapes[0] = 128; // jawOpen

    flow_gen.synthesize_frame(warped.data(), base.data(), width, height, semantic, recon.data());

    for (size_t i = 0; i < width * height; ++i) {
        PRISM_ASSERT(recon[i] >= 100 && recon[i] <= 160);
    }
}

PRISM_TEST(test_audio_assisted_error_concealment) {
    AudioAssistedConcealment aec;
    SemanticFrameVector last_valid{};
    last_valid.pose_6dof = {500, 200, 0, 10, -5, 0};
    last_valid.blendshapes.fill(10);

    std::array<int8_t, 8> audio_tokens{64, 32, 0, 0, 0, 0, 0, 0};
    SemanticFrameVector concealed = aec.conceal_lost_packet(last_valid, audio_tokens);

    // Concealment flag must be set
    PRISM_ASSERT((concealed.flags & 0x04) != 0);

    // Mouth aperture must be synthesized from audio acoustic energy
    PRISM_ASSERT(concealed.blendshapes[0] > 100); // jawOpen increased
    PRISM_ASSERT(concealed.blendshapes[1] > 50);  // mouthPucker increased
}

PRISM_TEST(test_prism_gen_codec_roundtrip) {
    PrismGenConfig cfg;
    cfg.width = 64;
    cfg.height = 64;
    cfg.anchor_interval = 5;

    PrismGenCodec codec(cfg);

    std::vector<uint8_t> frame_anchor(cfg.width * cfg.height, 150);
    std::vector<uint8_t> frame_delta(cfg.width * cfg.height, 155);

    SemanticFrameVector sem_anchor{};
    SemanticFrameVector sem_delta{};
    sem_delta.pose_6dof[3] = 50;

    std::array<uint8_t, 128> payload{};
    bool is_anchor = false;

    // 1. Frame 0: Anchor frame
    size_t bytes0 = codec.encode_frame(frame_anchor.data(), 0, sem_anchor, payload.data(), payload.size(), is_anchor);
    PRISM_ASSERT(is_anchor);
    PRISM_ASSERT_EQ(bytes0, 1);

    std::vector<uint8_t> recon0(cfg.width * cfg.height, 0);
    bool ok0 = codec.decode_frame(payload.data(), bytes0, frame_anchor.data(), recon0.data());
    PRISM_ASSERT(ok0);
    PRISM_ASSERT_EQ(recon0[0], 150);

    // 2. Frame 1: Semantic delta frame (1.5 kbps budget)
    size_t bytes1 = codec.encode_frame(frame_delta.data(), 1, sem_delta, payload.data(), payload.size(), is_anchor);
    PRISM_ASSERT(!is_anchor);
    PRISM_ASSERT(bytes1 <= 55); // Strictly <= 55 bytes!

    std::vector<uint8_t> recon1(cfg.width * cfg.height, 0);
    bool ok1 = codec.decode_frame(payload.data(), bytes1, nullptr, recon1.data());
    PRISM_ASSERT(ok1);
    PRISM_ASSERT(recon1[0] > 100 && recon1[0] < 200);
}

PRISM_TEST(test_prism_detint_flow_gen_spirv_validity) {
    std::vector<std::string> candidate_paths = {
        "core/shaders/detint_flow_gen.spv",
        "../core/shaders/detint_flow_gen.spv",
        "../../core/shaders/detint_flow_gen.spv"
    };

    std::string spv_path;
    for (const auto& path : candidate_paths) {
        if (std::filesystem::exists(path)) {
            spv_path = path;
            break;
        }
    }

    PRISM_ASSERT(!spv_path.empty());
    auto file_size = std::filesystem::file_size(spv_path);
    PRISM_ASSERT(file_size >= 1024);
    PRISM_ASSERT(file_size % 4 == 0);

    // Verify SPIR-V magic number 0x07230203
    std::ifstream file(spv_path, std::ios::binary);
    PRISM_ASSERT(file.is_open());
    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    PRISM_ASSERT_EQ(magic, 0x07230203u);
}
