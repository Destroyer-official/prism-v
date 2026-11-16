#pragma once
#ifndef PRISM_TYPES_HPP
#define PRISM_TYPES_HPP

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace prism {

// Version identifier: Prism-V v1.0 (2026/2027)
constexpr uint32_t PRISM_VERSION_MAJOR = 1;
constexpr uint32_t PRISM_VERSION_MINOR = 0;
constexpr uint32_t PRISM_VERSION_PATCH = 0;

// Magic headers
constexpr std::array<uint8_t, 8> PVLOSS_MAGIC = {'P', 'R', 'I', 'S',
                                                 'M', 'V', '0', '1'};
constexpr uint32_t BOX_TYPE_PRSM = 0x7072736D; // 'prsm'
constexpr uint32_t BOX_TYPE_PRMR = 0x70726D72; // 'prmr'
constexpr uint32_t BOX_TYPE_AFGS = 0x61666773; // 'afgs'
constexpr uint32_t BOX_TYPE_4DGS =
    0x34646773; // '4dgs' (Frontier 4: Dynamic 4D-GS Video)
constexpr uint32_t BOX_TYPE_4DGC =
    0x34646763; // '4dgc' (Prism-Spatial Configuration)
constexpr uint32_t BOX_TYPE_PGEN =
    0x7067656E; // 'pgen' (Frontier 3: Prism-Gen Semantic Box)
constexpr uint32_t BOX_TYPE_PSC2 =
    0x70736332; // 'psc2' (Frontier 2: PSC 2.0 Directional Lifting & Coupling)
constexpr uint32_t BOX_TYPE_PVMH =
    0x70766D68; // 'pvmh' (Frontier 6: Prism Virtual Mesh Vault Box)
constexpr uint32_t BOX_TYPE_PVMD =
    0x70766D64; // 'pvmd' (Frontier 6: Prism Virtual Mesh Delta Manifest Box)

// Memory & Page Alignment Constants
constexpr size_t PAGE_SIZE_16KB = 16384; // Android 15+ mandated 16 KB page size
constexpr size_t TILE_SIZE_LUMA = 256;   // 256x256 luma spatial tile
constexpr size_t TILE_SIZE_CHROMA = 128; // 128x128 chroma tile
constexpr size_t TILE_HALO_PIXELS = 16;  // 16-pixel overlap halo
constexpr size_t MAX_TILE_SLOTS = 12;    // 12-slot ring buffer pool

// Lossless Operational Contracts
enum class LosslessContract : uint8_t {
  CONTRACT_A_BBR = 0x01,   // Mode L-BBR: Bitstream-Domain Syntax Recompression
  CONTRACT_B_REMUX = 0x02, // Mode L-Remux: Packet-Preserving Remux
  CONTRACT_C_PSC = 0x03,   // Mode L-PSC: Bijective Wavelet-Checkerboard Codec
  CONTRACT_A_BBR2 = 0x04,  // Mode L-BBR 2.0: Frontier 1 Graph-Syntax & SIMD-16
  CONTRACT_C_PSC2 = 0x05,  // Mode L-PSC 2.0: Frontier 2 ADIW & Spectral Coupling
  CONTRACT_D_MESH = 0x06   // Mode L-Mesh: Frontier 6 Rabin-Mesh Global Deduplication Vault
};

// Source Codec Identifiers
enum class SourceCodecId : uint8_t {
  CODEC_H264 = 0x01,
  CODEC_HEVC = 0x02,
  CODEC_YUV420_8 = 0x03,
  CODEC_P010_10 = 0x04,
  CODEC_BAYER_RAW10 = 0x05,
  CODEC_AV1 = 0x06,
  CODEC_QUAD_BAYER_RAW = 0x07
};

// Thermal Operating Gears (Android PowerManager / ADPF)
enum class ThermalGear : uint8_t {
  GEAR_1_HIGH_QUALITY = 1, // THERMAL_STATUS_NONE (<38°C)
  GEAR_2_BALANCED = 2,     // THERMAL_STATUS_MODERATE (>38°C)
  GEAR_3_POWER_SAVER = 3,  // THERMAL_STATUS_SEVERE (>42°C)
  GEAR_4_THERMAL_PAUSE = 4 // THERMAL_STATUS_CRITICAL (>45°C)
};

// 5-Stage Verified Replacement Transaction States
enum class TransactionState : uint8_t {
  CANDIDATE = 0,    // Initial staging in private sandbox
  LOCAL_VERIFY = 1, // Full decode & cryptographic/VMAF verification
  COMMIT = 2,       // Passed verification; staged for atomic publish
  PUBLISH = 3,      // Registered in MediaStore with IS_PENDING=1, copied, then
                    // IS_PENDING=0
  GRACE = 4, // Original retained in 7-day vault trash; awaiting user clearance
  FAILED = 5 // Verification failed, rolled back, temp files purged
};

// 4-Target Pareto Dial Weights
struct ParetoTarget {
  float weight_quality = 0.5f;    // VMAF / CVVDP target
  float weight_file_size = 0.25f; // Strict MB cap
  float weight_speed = 0.15f;     // 60-120 fps throughput
  float weight_battery = 0.10f;   // Joules per frame ceiling (<0.05 J/frame)
  uint64_t explicit_size_cap_bytes = 0; // 0 = automatic
  float min_vmaf_floor = 93.0f;
};

// Cryptographic Checksum (SHA-256: 32 bytes)
using Sha256Digest = std::array<uint8_t, 32>;

// Cryptographic Checksum (BLAKE3: 32 bytes)
using Blake3Digest = std::array<uint8_t, 32>;

} // namespace prism

#endif // PRISM_TYPES_HPP
