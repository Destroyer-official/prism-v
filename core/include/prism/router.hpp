#pragma once
#ifndef PRISM_ROUTER_HPP
#define PRISM_ROUTER_HPP

#include "prism/types.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace prism {

/**
 * @brief Detected Codec and Media Family.
 */
enum class CodecFamily : uint8_t {
    UNKNOWN = 0x00,
    H264_AVC = 0x01,
    H265_HEVC = 0x02,
    AV1 = 0x03,
    RAW_YUV = 0x04,
    RAW_BAYER_CFA = 0x05,
    AUDIO_ONLY = 0x06
};

/**
 * @brief Ingest Media Stream Metadata.
 */
struct IngestMediaInfo {
    SourceCodecId codec_id = SourceCodecId::CODEC_H264;
    CodecFamily family = CodecFamily::UNKNOWN;
    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t bit_depth = 8;
    uint64_t file_size = 0;
    uint64_t duration_us = 0;              // Microsecond duration from container parser
    double   source_bitrate_bps = 0.0;     // Ground-truth source bitrate in bits/sec
    double estimated_bbr_savings_ratio = 0.18; // Default ~18% for H.264
    bool is_raw_sensor = false;
    bool is_container_standard = true;
    bool prefer_psc2 = false;
    bool is_quad_bayer = false;
};

/**
 * @brief Operational Routing Decision per Section 3.1 & 11.1.
 */
struct RoutingDecision {
    LosslessContract contract = LosslessContract::CONTRACT_A_BBR;
    SourceCodecId recommended_codec_id = SourceCodecId::CODEC_H264;
    std::string mode_name; // "Mode L-BBR", "Mode L-Remux", "Mode L-PSC", "Mode Y-Hybrid"
    std::string rationale;
    bool is_lossless = true;
};

/**
 * @brief Ingest Content Router.
 *
 * Implements automated tri-contract lossless routing and lossy hybrid dispatch:
 * 1. Compressed H.264/HEVC -> Contract A: Mode L-BBR.
 * 2. Incompatible container / audio-only / savings < 3% -> Contract B: Mode L-Remux.
 * 3. Raw Camera2 (YUV/RAW10/Bayer) -> Contract C: Mode L-PSC.
 * 4. User Lossy mode -> Mode Y-Hybrid (HW MediaCodec + Cool-chic + AFGS1).
 */
class IngestRouter {
public:
    /**
     * @brief Inspect input file/stream header and extract media info.
     */
    static IngestMediaInfo probe_media(const uint8_t* header_data, size_t header_len, uint64_t file_size);

    /**
     * @brief Route ingested media to the optimal operational contract.
     */
    static RoutingDecision route_media(const IngestMediaInfo& info, bool user_requested_lossless = true);
};

} // namespace prism

#endif // PRISM_ROUTER_HPP
