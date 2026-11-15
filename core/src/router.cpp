#include "prism/router.hpp"
#include <cstring>

namespace prism {

namespace {

bool contains_fourcc(const uint8_t* data, size_t len, const char* fourcc) {
    if (len < 4) return false;
    for (size_t i = 0; i + 4 <= len; ++i) {
        if (data[i] == fourcc[0] &&
            data[i+1] == fourcc[1] &&
            data[i+2] == fourcc[2] &&
            data[i+3] == fourcc[3]) {
            return true;
        }
    }
    return false;
}

} // anonymous namespace

IngestMediaInfo IngestRouter::probe_media(const uint8_t* header_data, size_t header_len, uint64_t file_size) {
    IngestMediaInfo info;
    info.file_size = file_size;

    if (!header_data || header_len < 16) {
        return info;
    }

    // Check for standard ISOBMFF 'ftyp'
    if (contains_fourcc(header_data, header_len, "ftyp")) {
        info.is_container_standard = true;
        if (contains_fourcc(header_data, header_len, "hvc1") ||
            contains_fourcc(header_data, header_len, "hev1")) {
            info.family = CodecFamily::H265_HEVC;
            info.codec_id = SourceCodecId::CODEC_HEVC;
            info.estimated_bbr_savings_ratio = 0.12; // 12% average for HEVC
        } else if (contains_fourcc(header_data, header_len, "av01")) {
            info.family = CodecFamily::AV1;
            info.codec_id = SourceCodecId::CODEC_H264;
            info.estimated_bbr_savings_ratio = 0.02; // AV1 bitstream already tight -> candidate for remux
        } else {
            // Default H.264 / AVC
            info.family = CodecFamily::H264_AVC;
            info.codec_id = SourceCodecId::CODEC_H264;
            info.estimated_bbr_savings_ratio = 0.18; // 18% average for H.264
        }
    } else if (header_len >= 8 && std::memcmp(header_data, "PRISMV01", 8) == 0) {
        // .pvloss container
        info.is_container_standard = false;
        info.family = CodecFamily::H264_AVC;
        info.estimated_bbr_savings_ratio = 0.0;
    } else if (header_len >= 4 && (std::memcmp(header_data, "II*\0", 4) == 0 || std::memcmp(header_data, "MM\0*", 4) == 0)) {
        // TIFF / DNG Bayer RAW Sensor header
        info.family = CodecFamily::RAW_BAYER_CFA;
        info.codec_id = SourceCodecId::CODEC_BAYER_RAW10;
        info.is_raw_sensor = true;
        info.bit_depth = 10;
    } else {
        // Unknown container or raw stream
        info.family = CodecFamily::RAW_YUV;
        info.codec_id = SourceCodecId::CODEC_YUV420_8;
    }

    return info;
}

RoutingDecision IngestRouter::route_media(const IngestMediaInfo& info, bool user_requested_lossless) {
    RoutingDecision decision;

    if (!user_requested_lossless) {
        // Mode Y-Hybrid (Lossy Hardware-Anchored Hybrid Split-Stream)
        decision.contract = LosslessContract::CONTRACT_A_BBR;
        decision.is_lossless = false;
        decision.mode_name = "Mode Y-Hybrid";
        decision.recommended_codec_id = (info.family == CodecFamily::H265_HEVC) ? SourceCodecId::CODEC_HEVC : SourceCodecId::CODEC_H264;
        decision.rationale = "Hardware MediaCodec Base (85% energy) + Cool-chic 5.0 Micro-Residual (<600 MAC/px) + AFGS1 Parametric Film Grain Synthesis";
        return decision;
    }

    decision.is_lossless = true;

    // Lossless Tri-Contract Evaluation per Section 3.1
    if (info.is_raw_sensor || info.family == CodecFamily::RAW_BAYER_CFA) {
        // Contract C: Mode L-PSC (Bayer CFA 4-plane)
        decision.contract = LosslessContract::CONTRACT_C_PSC;
        decision.recommended_codec_id = SourceCodecId::CODEC_BAYER_RAW10;
        decision.mode_name = "Mode L-PSC (Bayer CFA)";
        decision.rationale = "Contract C: Camera2 RAW sensor capture. Splices 4 Bayer sub-planes with LeGall 5/3 integer wavelet + rANS8 (100% sample parity, PSNR=infinity).";
    } else if (info.family == CodecFamily::RAW_YUV) {
        // Contract C: Mode L-PSC (Raw YUV)
        decision.contract = LosslessContract::CONTRACT_C_PSC;
        decision.recommended_codec_id = SourceCodecId::CODEC_YUV420_8;
        decision.mode_name = "Mode L-PSC (YUV)";
        decision.rationale = "Contract C: Uncompressed camera sensor capture. Reversible YCoCg-R + LeGall 5/3 lifting wavelet + 4-pass checkerboard ConvNet (PSNR=infinity).";
    } else if (info.family == CodecFamily::AUDIO_ONLY || !info.is_container_standard || info.estimated_bbr_savings_ratio < 0.03) {
        // Contract B: Mode L-Remux
        decision.contract = LosslessContract::CONTRACT_B_REMUX;
        decision.recommended_codec_id = info.codec_id;
        decision.mode_name = "Mode L-Remux";
        decision.rationale = "Contract B: Bitstream syntax savings < 3% or container repacking. 100% encoded packet preservation without dequantization, validated by SHA-256 payload digest.";
    } else {
        // Contract A: Mode L-BBR
        decision.contract = LosslessContract::CONTRACT_A_BBR;
        decision.recommended_codec_id = (info.family == CodecFamily::H265_HEVC) ? SourceCodecId::CODEC_HEVC : SourceCodecId::CODEC_H264;
        decision.mode_name = "Mode L-BBR";
        decision.rationale = "Contract A: Pre-existing compressed camera video. Bitstream syntax recompression via HuffmanPos/ECB + 1.1 KB Micro-MLP + 8-way rANS (100.000% SHA-256 bit-exact).";
    }

    return decision;
}

} // namespace prism
