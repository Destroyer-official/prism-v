#include "test_harness.hpp"
#include "prism/afgs1.hpp"
#include <vector>

using namespace prism;

PRISM_TEST(test_afgs1_noise_extraction_and_synthesis) {
    constexpr uint32_t W = 64;
    constexpr uint32_t H = 64;

    std::vector<uint8_t> noisy_frame(W * H);
    std::vector<uint8_t> clean_frame(W * H);
    std::vector<uint8_t> synthesized_frame(W * H);

    for (size_t i = 0; i < W * H; ++i) {
        noisy_frame[i] = static_cast<uint8_t>((128 + ((i % 17) - 8)));
    }

    // Extract noise & get clean frame
    Afgs1Params params = Afgs1Engine::extract_noise_and_denoise(noisy_frame.data(),
                                                               clean_frame.data(),
                                                               W, H);
    PRISM_ASSERT(params.apply_grain);

    // Synthesize grain on clean frame
    Afgs1Engine::synthesize_grain(clean_frame.data(), synthesized_frame.data(), W, H, params);

    // Serialization & deserialization check
    std::vector<uint8_t> bytes = Afgs1Engine::serialize_params(params);
    Afgs1Params restored_params = Afgs1Engine::deserialize_params(bytes.data(), bytes.size());
    PRISM_ASSERT_EQ(restored_params.grain_scale, params.grain_scale);
    PRISM_ASSERT_EQ(restored_params.random_seed, params.random_seed);
}
