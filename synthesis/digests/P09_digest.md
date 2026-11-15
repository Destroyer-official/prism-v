# P09 Digest: Next-Generation Android Video Compression System (Lossless + Lossy, 2027–2030)

**Source File:** `research_implementation_plane9.md`  
**Total Lines:** 317 | **Bytes:** 50,077  
**Simulation Date:** November 14, 2026  
**Author / Attributed Identity:** Architecture & Systems Planning Team  

---

## 1. Thesis
P09 rejects the notion of a monolithic single-codec or purely neural solution for mobile video compression. Instead, it proposes a **Dual-Mode Adaptive Hybrid Architecture**:
- **Lossless Mode (Three-Regime Routing):**
  - *Regime A (Raw Camera Sensor Bursts RAW10/12/14):* 4-plane Bayer CFA mosaic split ($R, G_1, G_2, B$) with integer block-aligned temporal delta coding, compressed via **JPEG XL Modular** (`jxl-encoder`/`libjxl`) inside DNG 1.7 / MKV (~2.5×–3.5× reduction).
  - *Regime B (Uncompressed / Decoded Frame Sequences YUV/RGB, screen captures):* **FFV1 v3/v4** (RFC 9043) accelerated via **Vulkan Compute shaders** (following FFmpeg 8.0, circumventing Android's blocked `VK_KHR_video_queue`), with fallback to multi-threaded ARM64 NEON/SVE2 FFV1. JXL Modular used for RGB screen recordings.
  - *Regime C (Already-Compressed Video MP4/H.264/HEVC/AV1):* Explicitly **rejects** "Lepton-style" entropy recompression (CABAC/MSAC repacking yields only 0%–9% savings at 5×–10× slower decode and breaks hardware playback). Offers container optimization (`moov` fast-start, metadata stripping) or explicit opt-in lossy transcoding.
- **Lossy Mode (Three-Tier Target-Driven Engine):**
  - Mapped to four intuitive user targets: **Speed**, **Battery**, **File Size**, and **Quality**.
  - *Tier 1 (Speed/Battery/Live Capture):* Hardware `MediaCodec` (10-bit HEVC / AV1 HW) controlled by Media3 `CodecDB`, ROI/perceptual QP, and `PowerManager.getThermalHeadroom()`.
  - *Tier 2 (Quality/Size — Charging-Deferred):* `SVT-AV1 4.2+` (NEON/SVE2/SME2, `--tune vq`/VMAF) and `SVT-AV2` / `dav2d` scheduled via Jetpack `WorkManager` (`setRequiresCharging(true)`).
  - *Tier 3 (Ultra-Low-Bitrate & Flagship NPU):* Deterministic Neural Video Codec (`MLVC-S` / `DCVC-RT` architecture) utilizing **Scale-Sending** (quantized scale indices in hyperlatent $\hat{z}$), ReGLU/LeakyReLU activations, and 30-frame LTR / FP32 resets to guarantee cross-SoC determinism.
- **Shared Infrastructure:** Monorepo Rust core (`aetherstream`), C++20 NDK bridge, Vulkan Compute shaders, LiteRT 2.3+ runtime, zero-copy `AHardwareBuffer` memory pipelines, and ISOBMFF/MKV containers.

---

## 2. Technical Decisions
- **Architecture & System Design:** Clean separation into live hardware paths (Tier 1) and charging-deferred high-compute paths (Tier 2/3), orchestrated by a Rust policy engine.
- **Algorithms & Codecs:**
  - Lossless: FFV1 v3/v4 (RFC 9043), JPEG XL Modular (`libjxl 0.12`), temporal integer delta.
  - Lossy: OEM `AMediaCodec` (HEVC/AV1), SVT-AV1 4.2, dav1d 1.5+, dav2d, MLVC-S neural codec.
- **Machine Learning & Determinism:**
  - Framework: LiteRT 2.3+ (Google Play Services `CompiledModel` API, saving 8 MB APK size) with ExecuTorch Vulkan fallback.
  - Determinism Guarantee: Scale-Sending mechanism (factorized hyperlatent entropy model indexes pre-computed discrete scales $\boldsymbol{\sigma}$), strictly eliminating floating-point CDF evaluation on NPU.
  - Activations: `ReLU`, `LeakyReLU`, and `ReGLU` (avoiding piecewise polynomial approximations of SiLU/GELU across vendors).
  - rANS Engine: Executed strictly in 32-bit integer arithmetic on CPU NEON or Vulkan Compute shaders.
- **Languages & Tools:**
  - Rust: Containers (ISOBMFF, MKV RFC 9559), bitstream parsing, rANS entropy coding, `jxl-encoder`, scheduling state machines.
  - C++20 (NDK r27+): `AMediaCodec`, `AImageReader`, Vulkan Compute SPIR-V pipeline, `dav1d`, `dav2d`, `SVT-AV1`, slim FFmpeg FFV1 build.
  - Kotlin + Jetpack Compose: UI, Media3 1.12 ExoPlayer, `WorkManager`, Thermal API.
- **Hardware Utilization:**
  - GPU: Vulkan Compute Shaders (SPIR-V) for FFV1 and color conversions (explicitly avoiding forbidden `VK_KHR_video_queue`).
  - CPU: ARMv9.3-A SME2 (Dimensity 9500/9600 Pro, Exynos 2600, Tensor G6) and Oryon SVE2/QMX (Snapdragon 8 Elite Gen 5/6).
  - NPU: Qualcomm QNN, MediaTek NeuroPilot, Samsung Exynos, Google Tensor TPU via LiteRT.
- **Container & File Format:**
  - Lossy Standards: ISOBMFF (`.mp4`) with `moov`-before-`mdat`, `stss` sync index, HDR metadata (`colr`/`clli`/`mdcv`), and `MP4-AT` auxiliary tracks.
  - Lossless & Neural: Matroska (`.mkv`, RFC 9559) with EBML `Cues` and custom CodecIDs (`V_FFV1`, `V_JXL_MODULAR`, `V_AETHER_MLVC1`).
- **Android Integration:** Jetpack Media3 `CodecDB`, `PowerManager.getThermalHeadroom()`, `WorkManager`, `AHardwareBuffer` zero-copy sharing via `CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_FD_KHR`.

---

## 3. Inventions & New Ideas
- **Three-Regime Lossless Routing (P09-I01):** Distinct pipelines for RAW sensor bursts (4-plane Bayer JXL), decoded/uncompressed frames (Vulkan FFV1), and already-compressed MP4s (container optimization only, rejecting entropy recompression).
- **Vulkan Compute FFV1 Shader Pipeline (P09-I02):** Adapts FFmpeg 8.0 SPIR-V compute shaders to Android mobile GPUs (Adreno/Mali), bypassing Android CDD's block on `VK_KHR_video_queue`.
- **4-Plane Bayer JXL with Temporal Delta Coding (P09-I03):** Splits $2\times2$ Bayer CFA into planar sub-channels, applies integer translation motion compensation, and encodes via JXL Modular inside MKV.
- **Anti-Pattern Proof: Rejection of Video Lepton (P09-I04):** Formally demonstrates that CABAC/MSAC recompression on modern video yields negligible compression (0–9%) at 5×–10× decode slowdowns and breaks hardware decoders.
- **Deterministic MLVC-S Scale-Sending Architecture (P09-I05):** Encodes discrete scale indices inside the hyperlatent $\hat{z}$, keeping NPU in fast FP16 with ReGLU while integer rANS runs deterministically on CPU/Vulkan.
- **Four-Target Continuous Pareto Interface (P09-I06):** Abstracting technical codecs into four user targets (Speed, Battery, File Size, Quality) deterministically mapped to live HW or charging-deferred SW/NPU.
- **Composite Mobile Efficiency Score $S_{\text{mobile}}$ (P09-I07):** Evaluates codecs by weighting BD-rate gain and perceptual score against energy per frame (mJ) and square root of peak RAM.

---

## 4. Plan Content
- **Phased Roadmap (12 Months):**
  - *Phase 0 (M1–2):* Validation lab, CVVDP/VMAF-NEG harness, Vulkan FFV1 & MLVC-S cross-chip determinism smoke tests (Gate 0).
  - *Phase 1 (M3–5):* Lossless core (Bayer JXL + FFV1 Vulkan/NEON), MKV container, Camera2 RAW burst capture (Gate 1).
  - *Phase 2 (M6–8):* Adaptive lossy engine (Tier 1 HW + Tier 2 SVT-AV1 WorkManager charging + Thermal API) (Gate 2).
  - *Phase 3 (M9–10):* Neural Tier 3 LiteRT NPU integration, Play Feature Delivery, dav2d integration (Gate 3).
  - *Phase 4 (M11–12):* Fuzzing (`cargo-fuzz`), battery/thermal regression, Play Store beta (Gate 4).
- **Team & Budget:** 6 dedicated engineers (Tech Lead, Android NDK, GPU/Vulkan, ML Compression, UI, QA/Benchmark). Budget: $1.1M–$1.6M (or lean 2–3 person OSS team for $20K–$50K compute/lab).
- **Testing & Verification:** UVG, MCL-JCV, YouTube-UGC, BVI-DVC, Camera2 RAW10/12, 120Hz screen recording. Fuzzing with `cargo-fuzz`.
- **Legal & IP Strategy:** Pure royalty-free software core (FFV1, libjxl, SVT-AV1, dav1d, dav2d, Apache/BSD/MIT) + OEM-licensed `MediaCodec` for HEVC/VVC. Strict avoidance of GPLv2 `x264`/`x265`. Tracks Sisvel AV2 pool and Access Advance Dolby v. Snap litigation.

---

## 5. Numbers & Quantitative Claims
- **AV2 Performance:** −29.81% PSNR-YUV, −33.79% VMAF (RA); −31.34% PSNR-YUV, −35.77% VMAF (AS) vs. AV1 anchor `[verified (primary source)]` (arXiv:2605.15800).
- **AV2 Speed:** AVM encoder 33× slower than AV1; `dav2d` decoder 3.86×–5× slower than `dav1d` `[verified (primary source)]`.
- **VVC Performance & Cost:** −40% to −50% vs. HEVC; SW decode 1.5×–1.8× HEVC CPU overhead (~3× faster battery drain vs. `dav1d`) `[verified (secondary source)]`.
- **FFV1 v3/v4:** 8.1× compression ratio on 10-bit 4:2:2 `v210` (37 vs. 332 Mbit/s); Vulkan Compute yields +35% to 3× speedup in FFmpeg 8.0 `[verified (primary source)]`.
- **JPEG XL Modular:** Beats lossless WebP by 9–17% and AVIF by 33–122%; 3.06× ratio on CLIC2021 RGB `[verified (primary source)]`.
- **MLVC-S Neural Codec:** 5.4M parameters, 567–587 GFLOPs at 1080p; −75.5% MOS at 360p; 80.0/77.5 fps on Snapdragon X Elite NPU; 39.5/34.6 fps on Apple M3 Pro `[verified (primary source)]`.
- **NeuralLVC Failure Baseline:** 0.06 fps, 4.5 GB VRAM on NVIDIA GH200 `[verified (primary source)]` (unusable on mobile).
- **Video Lepton Recompression:** Max −9.10% on DCT residuals at 4.7×–10.1× slower decode `[verified (primary source)]`.
- **App Size:** Base APK < 22 MB; LiteRT in Play Services saves ~8 MB; on-demand neural model pack ~15–25 MB.

---

## 6. Citations & Live Verification Status
- *AOMedia Releases AV2 (Jun 9, 2026)*: Confirmed primary standard milestone.
- *arXiv:2605.15800 (AV2 / dav2d Evaluation, May 2026)*: Verified primary paper.
- *FFmpeg 8.0/8.1 & Khronos Vulkan Compute Blog (Mar 2026)*: Verified primary technical documentation.
- *arXiv:2606.28027 (MLVC, Jul 2026)*: Verified primary paper with code.
- *arXiv:2502.20762 (DCVC-RT, Mar 2025)*: Verified primary paper.
- *Google IssueTracker #293419320 (VK_KHR_video_queue blocked on Android)*: Verified primary platform issue.
- *Access Advance Dolby v. Snap litigation (Mar 2026)*: Verified legal development.
- *ISO/IEC 14496-12:2026 (ISOBMFF 2026 edition)* & *RFC 9559 (Matroska)*: Verified standards.

---

## 7. Strengths
- **Exemplary Lossless Framing (§1.1, §3.2):** Formally divides lossless into 3 distinct regimes, correctly identifying that generic CABAC recompression is an anti-pattern for mobile video.
- **Pragmatic GPU Strategy (§2.4, §3.2):** Uncovers Android CDD's block on `VK_KHR_video_queue` and provides a production-ready Vulkan Compute shader alternative based on FFmpeg 8.0.
- **Architectural Solution to Cross-NPU Invariance (§2.3, §3.4):** Adopts Scale-Sending + ReGLU + integer rANS on CPU/Vulkan, directly resolving cross-SoC divergence.
- **Disciplined Legal Strategy (§2.6, §4.2):** Explicitly analyzes Dolby v. Snap, Sisvel AV2 pool, and avoids GPLv2/patent pool exposure.

---

## 8. Weaknesses
- **Unverified Mobile NPU Throughput for MLVC-S (§5.3):** While verified on desktop M3 Pro and laptop Snapdragon X Elite, smartphone thermal performance under 6–8 W is unmeasured.
- **Vulkan Compute FFV1 Shader Overhead on Mobile (§5.3):** Mobile Mali and Adreno GPU driver quirks could reduce compute shader efficiency compared to desktop.
- **Complex Engineering Stack (§4.1):** Mixing Rust, C++20, SPIR-V Vulkan shaders, LiteRT, and Kotlin across 6 engineers poses high orchestration risk.

---

## 9. Scores
- **Depth:** 5/5 — Covers every architectural layer from Bayer mosaic to SPIR-V shaders, NPU determinism, and CDD driver quirks.
- **Accuracy:** 5/5 — Rigorous distinction between primary, secondary, and author estimates; highly accurate hardware and standards data.
- **Originality:** 5/5 — Three-regime lossless routing, Vulkan Compute FFV1 adaptation, and continuous Pareto target mapping.
- **Feasibility:** 4/5 — Sophisticated multi-language stack requires elite engineering talent, but individual components are well grounded.
- **Completeness:** 5/5 — Contains complete schedule, team, budgets, go/no-go gates, red-team analysis, and 10 prioritized experiments.
- **Evidence:** 5/5 — Every claim is tagged with epistemic status and backed by verifiable 2025–2026 primary literature.
