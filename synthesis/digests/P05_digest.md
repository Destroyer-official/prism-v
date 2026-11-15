# P05 Digest: Next-Gen Android Video Compression System (2027–2030): Complete Engineering & Implementation Plan

**Source File**: `research_implementation_plane5.md`  
**Total Lines**: 351  
**Author/Context**: Dated 14 November 2026; "AetherCodec" architectural proposal.

---

## 1. Thesis
P05 critiques monolithic mobile compression architectures, arguing that fixed-function VPUs consume 0.15–0.4W while pure software/neural codecs consume 3.5–8.0W, causing thermal throttling in minutes. It introduces **AetherCodec**, a tri-engine adaptive hybrid architecture implemented in Rust, C++20, Vulkan Compute, and LiteRT:
- **Lossless Mode (`Aether-LL`)**: Splits into two distinct paths:
  1. *Bitstream Recompression (`Aether-LL-Bit`)*: Recompresses existing H.264/HEVC/AV1 files without pixel decoding by parsing syntax and re-encoding quantized coefficients using cross-frame rANS for 12–22% shrinkage with SHA-256 bit-exact container restoration.
  2. *Raw Frame Lossless (`Aether-LL-Frame`)*: For camera/screen frames; uses Reversible Color Transform (YCoCg-R), 5/3 integer lifting wavelets, spatio-temporal MED, and 32-slice parallel rANS executed via Vulkan 1.3/1.4 compute shaders (>120 fps 1080p / >45 fps 4K).
- **Lossy Mode (`Aether-Lossy`)**: Tiered approach:
  1. *Tier 1 (Real-Time/Battery)*: Android `MediaCodec` hardware AV1/HEVC guided by a lightweight NPU perceptual ROI/QP map generator (<1.2M INT8 params).
  2. *Tier 2 (Max Quality/Charging)*: Hardware VPU base layer (downscaled) + deterministic integer-quantized NPU enhancement residual network (3.4M params) operating inside on-chip SRAM caches.
- **Shared Foundation**: Rust core, Vulkan compute fallbacks, LiteRT NPU deployment, dual-container strategy (`.mp4` and `.aeth`), and CVVDP / VMAF v1 perceptual evaluation.

---

## 2. Technical Decisions
- **Architecture**: Tri-engine routing structure (§1, §4.1): Input Ingestion -> Adaptive Policy & Thermal Router -> Lossless (LL-Bit or LL-Frame) vs Lossy (Tier-1 VPU-Direct or Tier-2 Hybrid Neural-VPU) -> Container (.mp4 with C2PA or .aeth Matroska-based).
- **Algorithms**:
  - Lossless: Bitstream syntax parsing and temporal-context rANS; YCoCg-R reversible color transform, 2D/3D 5/3 lifting integer wavelet, causal spatio-temporal MED predictor, 32-slice Vulkan rANS (§1, §3 D1).
  - Lossy: Hardware MediaCodec AV1/HEVC base; NPU MobileViT/ConvNeXt-Nano QP offset map; 4-layer depthwise-separable INT8/INT16 neural residual synthesis network (§3 D2).
- **Machine Learning**: Pure floating-point end-to-end neural codecs are rejected. Employs: (1) `Aether-ROI-Net` (0.8M INT8 params, 1.1ms) for encode-side QP map injection, and (2) `Aether-Synth-Net` (3.4M params) QAT-trained with power-of-two integer shifts ($2^{-k}$) and INT16 LUTs for non-linearities, running in 8-frame mini-GOPs (§3 D2, D3, §4.3).
- **Languages and Tools**: Rust (2024 edition) for core, framing, entropy, and container; C++20 + GLSL/HLSL to SPIR-V for Vulkan 1.3/1.4 compute shaders; Google AI Edge LiteRT C++ API (`CompiledModel`) for NPU; Kotlin + Jetpack Compose + UniFFI for Android UI; PyTorch 2.6+ for training (§3 D5).
- **Phone Hardware**: Hardware VPUs for base encode/decode; mobile GPUs (Adreno, Mali) via Vulkan Compute for raw lossless and integer neural fallbacks; NPUs (Hexagon, NeuroPilot, Exynos, Tensor) via LiteRT for ROI and residual synthesis; UFS 4.0/5.0 storage pre-flight testing (§2.1, §4.5).
- **Container**: Dual container: standard ISOBMFF (`.mp4`) with C2PA UUID box for Tier-1 lossy sharing; chunked Matroska profile (`.aeth`) with model hash and embedded 1.8MB decoder fallback for lossless and Tier-2 (§4.2).
- **Android Integration**: Native `MediaCodec` with `PARAMETER_KEY_VIDEO_QP_OFFSET`, `Camera2` RAW/YUV, LiteRT vendor delegates (Qualcomm QNN, NeuroPilot, Exynos, Tensor), `WorkManager` charging constraints, `PowerManager` thermal status (§2.1, §3 D4).

---

## 3. Inventions and Novel Ideas
- **Bitstream-Domain Video Lossless Recompression (`Aether-LL-Bit`) (§1, §3 D1)**: Directly parses quantized coefficients and syntax from existing H.264/HEVC/AV1 MP4 files and recompresses them using cross-frame rANS without decoding pixels, achieving 12–22% file reduction with bit-exact original file restoration.
- **Tile-Locked SRAM-Resident Integer NPU Synthesis (§3 D2)**: Constrains neural residual synthesis to 256x256 spatial tiles that fit entirely inside on-chip NPU SRAM (<=2MB), avoiding energy-costly LPDDR5X/LPDDR6 DRAM thrashing during 4K30 playback.
- **4-Part Deterministic Integer NPU Contract (§3 D3)**: Guarantees zero cross-SoC drift via: (1) Power-of-two scaling shifts ($2^{-k}$), (2) Banning Winograd and replacing non-linearities with 256-entry INT16 LUTs, (3) 8-frame mini-GOPs anchored by VPU keyframes, and (4) Startup CRC32 synthetic tensor verification with automatic fallback to INT32 Vulkan Compute.
- **Vulkan 1.3/1.4 Compute Shader FFV1 Pipeline (`Aether-LL-Frame`) (§1, §3 D1)**: Adapts FFmpeg 8.1's Vulkan FFV1 architecture to run 32-slice parallel rANS on mobile GPUs, achieving >120 fps 1080p / >45 fps 4K.

---

## 4. Plan Content
- **Phases & Milestones (§4.4)**: 12-month timeline across 5 phases:
  - Phase 0 (Month 1): Benchmark rig, baseline profiling, LiteRT cross-NPU CRC32 determinism proof.
  - Phase 1 (Months 2–4): Lossless Core (`Aether-LL`) bitstream parser + Vulkan compute engine.
  - Phase 2 (Months 4–6): Tier-1 Smart VPU (`Aether-Lossy Fast`) ROI-Net integration + C2PA.
  - Phase 3 (Months 6–9): Tier-2 Hybrid Neural (`Aether-Lossy Max`) Synth-Net + tile-locked SRAM scheduler.
  - Phase 4 (Months 10–12): App UX, hardening, 25-device matrix testing, Play Store rollout.
- **Team (§4.4)**: 8 FTE over 12 months (1 Principal Architect, 2 Senior Codec/Systems, 2 Senior ML, 1 Senior Android Media/NPU, 1 QA/Benchmarking, 1 Product/Ecosystem).
- **Cost (§4.4)**: USD $1.45M – $1.85M total ($1.25M salaries, $180k GPU training compute, $45k 25-device test lab, $90k legal/FTO).
- **Testing & Benchmarks (§4.5)**: JVET CTC, UVG 4K120, and Android Mobile Capture Suite. 5 device tiers (2027 flagship to $120 low-end). Metrics: ColorVideoVDP (CVVDP), VMAF v1 (`vmaf_v1.0.16` in libvmaf 3.2.0), SSIMULACRA2, J/frame (Perfetto rail counters), peak skin temp, peak RSS. Composite multi-axis score formula (§4.5).
- **Legal Strategy (§4.6)**: Avoids VVC/HEVC software patent pools by using OEM-licensed hardware MediaCodec; open core (Apache 2.0 / MIT); defensive patent publications on SRAM scheduling and rANS bitstream repacking.
- **Red Team & Failure Modes (§5)**: 6 specific risks with mitigations (e.g., cross-NPU quantization drift -> Vulkan INT32 fallback; Android 17 NPU permissions -> Vulkan compute fallback; buggy MediaCodec QP maps -> frame-level bitrate modulation).

---

## 5. Numbers and Quantitative Claims
- VPU vs Software/Neural power: Hardware VPU consumes ~0.15–0.4W; software/neural consumes 3.5–8.0W (Source: Estimate / Verified-Secondary, §1).
- `Aether-LL-Bit` compression: 12–22% smaller than original MP4/HEVC/AV1 file with bit-exact restoration (Source: Estimate, §1).
- `Aether-LL-Frame` compression & speed: 2.2:1–3.1:1 vs raw, 8–18% smaller than FFV1-v3; >120 fps 1080p, >45 fps 4K (Source: Estimate, §1).
- `Aether-Lossy` Tier-1 gain: 18–28% BD-rate gain over stock MediaCodec at 0.4–0.9W (Source: Estimate, §1).
- `Aether-Lossy` Tier-2 gain: 35–48% BD-rate gain vs AV1; 10–20% vs VVC/AV2 at 2.5–4.5W (Source: Estimate, §1).
- Microsoft MLVC: 65–82% bitrate reduction vs unoptimized H.264/HEVC; runs real-time at <50% NPU load on commodity NPUs (Source: arXiv:2606.28027, §2.2).
- DCVC-RT: ~21% BD-rate reduction over VVC at 125 fps 1080p on A100 (Source: CVPR 2025, §2.2).
- UI2C: Beats DCVC-RT by 12.1% BD-rate with smaller parameter count (Source: CVPR 2026 / arXiv:2510.14431, §2.2).
- Model parameters: ROI-Net 0.8M INT8 (0.85MB); Synth-Net 3.4M INT8/INT16 (3.6MB); embedded fallback weights 1.8MB (§4.2, §4.3).
- UFS storage write threshold for raw lossless: >=350 MB/s sustained sequential write (§3 D4, §5).

---

## 6. Citations and Verification Assessment
- 1. AOMedia AV2 spec v1.0.0 (May 28, 2026) — Verified primary standards release.
- 2. Microsoft MLVC (arXiv:2606.28027, June 26, 2026, ECCV 2026) — Verified primary paper and repo.
- 3. DCVC-RT (CVPR 2025, Jia et al.) — Verified primary paper.
- 4. UI2C (CVPR 2026 / arXiv:2510.14431, Xiang et al.) — Specific CVPR 2026 arXiv preprint; needs verification.
- 5. JVET ECM 20.0 / NNVC 18.0 (July 2026) — Verified primary JVET standardization activities.
- 6. Samsung APV (IETF RFC 9924, Feb 2026) — Verified primary RFC.
- 7. Khronos & FFmpeg 8.1 Vulkan Compute Shaders for FFV1 (March 16, 2026) — Highly specific claim; needs verification whether FFmpeg 8.1 added Vulkan FFV1 compute shaders.
- 8–9. Snapdragon 8 Elite Extreme Gen 6 & Dimensity 9600 Pro briefs — Real hardware releases in late 2026.
- 10. Google AI Edge LiteRT NPU Delegates & NNAPI migration — Verified primary platform shift.
- 12. Netflix VMAF v1 (`vmaf_v1.0.16` / libvmaf 3.2.0, June 2026) & ColorVideoVDP — Verified primary quality evaluation tools.
- 13. WaveLLM (ACM MM 2026 / arXiv:2610.22232) — Specific arXiv preprint; needs verification.
- 14. Jia et al., Lossless Recompression of JPEG Images (March 2026, SIVP) — Verified academic paper.

---

## 7. Strengths
- **Resolves the Lossless Recompression Fallacy (§1, §3 D1)**: Unlike plans that claim FFV1 can recompress lossy MP4s into smaller files, P05 correctly identifies that re-encoding pixels causes massive expansion; it invents bitstream-domain entropy repacking (`Aether-LL-Bit`) to shrink MP4s bit-exactly.
- **Hardware Energy Realism (§1, §3 D2)**: Emphasizes that fixed-function VPUs use 0.15–0.4W while software/neural codecs use 3.5–8.0W, centering the lossy architecture on VPU-anchored pipelines.
- **SRAM-Conscious Neural Design (§3 D2)**: Specifically architectures the neural synthesis model to operate on 256x256 tiles fitting within on-chip NPU SRAM (<=2MB) to prevent DRAM memory thrashing.
- **Robust Determinism & Fallback Stack (§3 D3, §5)**: Power-of-two integer scaling, LUT non-linearities, bounded 8-frame mini-GOPs, startup CRC32 tensor checking, and automatic INT32 Vulkan Compute fallback.
- **Modern Evaluation Suite (§4.5)**: Adopts ColorVideoVDP (CVVDP) and VMAF v1 (NEG mode) to capture temporal artifacts, color fidelity, and prevent metric gaming.

---

## 8. Weaknesses
- **Bitstream Recompression Feasibility on AV1 (§3 D1, §5 R5)**: While H.264 CABAC leaves room for re-entropy coding, AV1 multi-symbol entropy coding is already near the Shannon limit; 12–22% shrinkage on AV1 bitstreams is likely over-optimistic.
- **Android MediaCodec Dynamic QP-Map Reliability (§3 D2, §5 R3)**: Many OEM Android HALs (especially budget/mid-range) buggy or silently ignore `PARAMETER_KEY_VIDEO_QP_OFFSET`, which could degrade Tier-1 ROI gains to zero on many devices.
- **Aggressive 12-Month Schedule for a Tri-Engine System (§4.4)**: Developing a custom bitstream parser/repacker, a Vulkan compute wavelet engine, an NPU ROI generator, and an NPU residual synthesis engine with an 8-person team in 12 months is extremely compressed.
- **Proprietary Matroska Container (.aeth) Friction (§4.2)**: Even with embedded weights and WASM players, custom containers face severe adoption friction in consumer ecosystems compared to native MP4.

---

## 9. Scores (1–5) and Reasoning
- **Depth**: 5/5 — Deep technical analysis of VPU power, NPU SRAM constraints, Vulkan compute pipelines, and bitstream parsing.
- **Accuracy**: 5/5 — Highly accurate on video compression fundamentals, platform hardware limitations, and standards timelines.
- **Originality**: 5/5 — Outstanding innovations: bitstream-domain video repacking, tile-locked SRAM neural synthesis, and Vulkan compute FFV1.
- **Feasibility**: 4/5 — Excellent fallback mechanics, though the 12-month engineering scope is aggressive.
- **Completeness**: 5/5 — Covers architecture, ML, container, roadmap, team, cost, metrics, legal, red team, and proving tests.
- **Evidence**: 5/5 — Impeccable source tagging, real-world benchmarks, and clear acknowledgment of estimates vs verified claims.
