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
            info.estimated_bbr_savings_ratio = 0.12; // heuristic default for HEVC
        } else if (contains_fourcc(header_data, header_len, "av01")) {
            info.family = CodecFamily::AV1;
            info.codec_id = SourceCodecId::CODEC_H264;
            info.estimated_bbr_savings_ratio = 0.02; // heuristic: AV1 usually favours remux
        } else {
            // Default H.264 / AVC
            info.family = CodecFamily::H264_AVC;
            info.codec_id = SourceCodecId::CODEC_H264;
            info.estimated_bbr_savings_ratio = 0.18; // heuristic default for H.264
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
        decision.rationale = "Hardware MediaCodec base layer plus Cool-chic micro-residual and AFGS1 parametric film grain synthesis";
        return decision;
    }

    decision.is_lossless = true;

    // Lossless Tri-Contract Evaluation per Section 3.1
    if (info.is_raw_sensor || info.family == CodecFamily::RAW_BAYER_CFA) {
        if (info.prefer_psc2 || info.is_quad_bayer || info.codec_id == SourceCodecId::CODEC_QUAD_BAYER_RAW) {
            // Contract C: Mode L-PSC 2.0 (Quad-Bayer CFA)
            decision.contract = LosslessContract::CONTRACT_C_PSC2;
            decision.recommended_codec_id = SourceCodecId::CODEC_QUAD_BAYER_RAW;
            decision.mode_name = "Mode L-PSC 2.0 (Quad-Bayer CFA)";
            decision.rationale = "Contract C (Frontier 2): Camera2 Quad-Bayer RAW sensor capture. Adaptive Directional Integer Wavelet (ADIW 5/3 & 9/7), unit-triangular A-IKLT spectral decorrelation, and lock-free wavefront SIMD rANS8 entropy coding.";
        } else {
            // Contract C: Mode L-PSC (Bayer CFA 4-plane)
            decision.contract = LosslessContract::CONTRACT_C_PSC;
            decision.recommended_codec_id = SourceCodecId::CODEC_BAYER_RAW10;
            decision.mode_name = "Mode L-PSC (Bayer CFA)";
            decision.rationale = "Contract C: Camera2 RAW sensor capture. Splits 4 Bayer sub-planes with a reversible LeGall 5/3 integer wavelet and rANS8 entropy coding.";
        }
    } else if (info.family == CodecFamily::RAW_YUV) {
        if (info.prefer_psc2) {
            // Contract C: Mode L-PSC 2.0 (Raw YUV)
            decision.contract = LosslessContract::CONTRACT_C_PSC2;
            decision.recommended_codec_id = SourceCodecId::CODEC_YUV420_8;
            decision.mode_name = "Mode L-PSC 2.0 (YUV)";
            decision.rationale = "Contract C (Frontier 2): Uncompressed camera capture with ADIW integer lifting, YCoCg-R2 shears, and lock-free wavefront SIMD.";
        } else {
            // Contract C: Mode L-PSC (Raw YUV)
            decision.contract = LosslessContract::CONTRACT_C_PSC;
            decision.recommended_codec_id = SourceCodecId::CODEC_YUV420_8;
            decision.mode_name = "Mode L-PSC (YUV)";
            decision.rationale = "Contract C: Uncompressed camera capture. Reversible YCoCg-R, LeGall 5/3 lifting wavelet, MED prediction, and rANS8 entropy coding.";
        }
    } else if (info.family == CodecFamily::AUDIO_ONLY || !info.is_container_standard || info.estimated_bbr_savings_ratio < 0.03) {
        // Contract B: Mode L-Remux
        decision.contract = LosslessContract::CONTRACT_B_REMUX;
        decision.recommended_codec_id = info.codec_id;
        decision.mode_name = "Mode L-Remux";
        decision.rationale = "Contract B: Container repacking without re-encoding; packet payloads are verified with a SHA-256 digest.";
    } else {
        // Contract A: Mode L-BBR 2.0 (Frontier 1)
        decision.contract = LosslessContract::CONTRACT_A_BBR2;
        decision.recommended_codec_id = (info.family == CodecFamily::H265_HEVC) ? SourceCodecId::CODEC_HEVC : SourceCodecId::CODEC_H264;
        decision.mode_name = "Mode L-BBR 2.0";
        decision.rationale = "Contract A (Frontier 1): Pre-existing compressed video. Bitstream recompression via Attentive Spatial Syntax Graph (SSG-CR4), C-MAVQ 64-symbol VCTs, and 16-way SIMD rANS with SHA-256 bit-exact contract.";
    }

    return decision;
}

} // namespace prism
