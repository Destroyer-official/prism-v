# P12 Digest: Next-Generation Android Video Compression — Evidence-Led Implementation Plan (2027–2030)

**Source File:** `research_implementation_plane12.md`  
**Total Lines:** 881 | **Bytes:** 93,010  
**Simulation Date:** November 14, 2026  
**Author / Attributed Identity:** Android Media & Systems Architecture Group  

---

## 1. Thesis
P12 provides an exceptionally pragmatic, evidence-based systems engineering plan. It rejects inventing a new general-purpose bitstream or requiring a neural decoder in v1:
- **Core Recommendation:** Build a **capability-probed, hardware-first Android transcoder with a separate archival-lossless pipeline**.
- **Three Distinct Lossless Contracts:**
  - *Contract A (Original-File / Bitstream Preservation):* Byte-for-byte SHA-256 preservation via file copy, or container remuxing without re-encoding (with explicit metadata auditing).
  - *Contract B (Lossless Pixel-Domain Transcode):* Full decode followed by bit-exact sample-plane preservation using **FFV1 v3 in Matroska (`.mkv`)** (RFC 9043).
  - *Contract C (Raw Sensor/Camera Archival):* Preserving un-demosaiced Bayer sensor data directly from the camera pipeline (deferred to a future acquisition feature; explicitly not an MP4 post-processing operation).
- **Lossy Mode (Hardware-First Capability Routing):**
  - Uses runtime capability discovery (`MediaCodecList`, `CodecCapabilities`, smoke tests) rather than hardcoding SoC assumptions.
  - AVC/H.264 as universal compatibility fallback; HEVC/H.265 as initial high-efficiency hardware default; AV1 only where a verified hardware encoder exists; software AV1 (rav1e/libaom/SVT-AV1) as an optional offline path.
  - Four explicit user targets: **Quality** (CQ/VBR), **File Size** (iterative rate calculation), **Speed** (low-complexity hardware), and **Battery** (energy-governed hardware).
- **Rejection of Neural Decoders & VVC Encoders:**
  - *No Mandatory Learned Decoder:* Quantization does not guarantee cross-device bit-identical reconstruction across mobile NPU/GPU/CPU drivers.
  - *No VVC Encoder:* Android 17 AOSP includes VVC framework plumbing and vendor Codec2 decoder integration, but explicitly provides **no software VVC decoder and zero VVC encoding support**.

---

## 2. Technical Decisions
- **Architecture & System Design:** Streaming pipeline with bounded-memory tiling/striping; atomic publication with crash recovery journals; runtime capability probing (`Codec Capability Fingerprint`).
- **Algorithms & Codecs:**
  - Lossless: FFV1 v3 (RFC 9043) in Matroska (`V_FFV1`), verifying decoded sample planes with cryptographic hashes.
  - Lossy: Hardware `MediaCodec` (HEVC/AVC/AV1); optional software AV1 (rav1e/libaom/SVT-AV1 arm64 builds).
  - Rejections: VVC encoding (unsupported in AOSP); end-to-end NVC decoders (non-deterministic).
- **Machine Learning (Encoder-Side Only):**
  - Research Track A: Content-Conditioned Encode Planner (CCEP), a lightweight 1–5M parameter model predicting scene class (screen, natural, grain, sports) and complexity to guide rate-control presets.
  - Runtime: LiteRT CPU/GPU baseline; standalone NPU integration only where verified. Avoids deprecated NNAPI.
- **Languages & Tools:**
  - Kotlin + Jetpack Compose: UI, lifecycle, WorkManager durable background jobs, Storage Access Framework (SAF).
  - C++20 (NDK r27+): Native media engine, explicit plane memory management, SIMD kernels.
  - FFmpeg: Pinned, minimal LGPLv2.1 build (libavcodec, libavformat, libswscale) stripped of all GPL components (strictly excludes x264/x265).
  - Media3 Transformer: Baseline integration and fallback.
- **Hardware Utilization:**
  - VPU: Hardware `MediaCodec` surface-to-surface zero-copy paths.
  - CPU: Multi-threaded ARM64 NEON/SVE2 for FFV1 and software fallbacks.
  - GPU: Vulkan/OpenGL compute strictly for measured pre-processing transforms (color conversions, rotation) avoiding CPU readbacks.
  - NPU: Probed and isolated; excluded from normative decoding.
- **Container Strategy:** MP4 for mainstream lossy (AVC/HEVC/AV1); Matroska (`.mkv`) for FFV1 archival. Custom containers are strictly rejected.
- **Quality Metrics:** PSNR, SSIM/MS-SSIM, VMAF v1 (NEG mode for codecs, phone/4K viewing condition models), BD-rate distributions, and ITU-T P.910 / BT.500 subjective testing.

---

## 3. Inventions & New Ideas
P12 frames its novel contributions as engineering combinations and testable hypotheses:
- **Codec Capability Fingerprint (CCF) (P12-I01):** Cached runtime fingerprint of device-specific encoder components, profile/level support, color formats, and live smoke-test performance, avoiding hardcoded SoC assumptions.
- **Content-Conditioned Encode Planner (CCEP) (P12-I02):** Short source scan using a compact (1–5M param) classifier to estimate motion, noise floor, and scene type to choose optimal rate-control parameters.
- **Bounded-Memory Tile/Stripe Processing (P12-I03):** Streaming slice-based memory architecture for FFV1 and software paths, capping peak RSS to ≤350 MiB on 1080p and ≤700 MiB on 4K.
- **Energy-Aware Job Governor (EAJG) (P12-I04):** Dynamic scheduler adjusting encoder complexity, resolution, and queue depth based on Android `PowerManager` thermal headroom and battery state.
- **Verification-First Archive Commit (P12-I05):** Multi-stage atomic publication writing to temporary storage, validating decoded-plane SHA-256 hashes, and committing via atomic rename with a persistent journal.
- **Dynamic Overhead Target-Size Formulator (P12-I06):** Exact bit-budgeting algorithm incorporating duration, audio tracks, and empirically calibrated container overhead fraction $h$.

---

## 4. Plan Content
- **Phases & Roadmap (9–12 Months):**
  - *Phase 0 (W1–2):* Scope, lossless contracts, feature matrix, device shortlist.
  - *Phase 1 (W3–6):* Capability probe tool, Media3 baseline, FFV1 host baseline (Day 30 Gate).
  - *Phase 2 (W7–14):* Core streaming pipeline, temp files, cancellation, memory bounds.
  - *Phase 3 (W15–20):* Lossless archive (FFV1 v3 + Matroska, sample-plane verification).
  - *Phase 4 (W21–28):* Lossy presets (AVC/HEVC/AV1 hardware, quality/size/battery controls).
  - *Phase 5 (W29–34):* Reliability, HDR10/PQ/HLG, subtitle preservation, crash recovery, fuzzing.
  - *Phase 6 (W35–40):* Beta testing, telemetry opt-in, support diagnostics.
  - *Phase 7 (W41–44):* Release hardening, SBOM, legal/privacy sign-off.
  - *Parallel ML Track (3–6 Months):* Encoder-adviser ablation; mobile neural decoder feasibility.
- **Team & Budget:** 4–5 core FTEs (Senior Android/Media Architect, 1–2 Native Codec Engineers, 1 Android Compose/WorkManager Engineer, 1 Perf/QA Engineer, 0.5 ML, 0.5 Security/Release, external legal). Budget: **₹60 lakh–₹2.3 crore** ($75k–$280k USD) for lean India-based engineering.
- **Testing & Lab Matrix:** 10-class test corpus (UVG 4K, MCL-JCV, open media, screen capture, animation, high motion, night/noise, HDR, synthetic stress). Device lab: entry-level 4GB, lower-midrange 6–8GB, upper-midrange 8–12GB, and flagship 12–16GB (Qualcomm, MediaTek, Tensor).
- **Legal & Licensing:** Extensive review of Access Advance HEVC/VVC pools (noting the Nov 11, 2026 MCBA discount phase-out announcement); AOMedia AV1 patent license terms; strict exclusion of GPL libraries (x264/x265) to maintain closed-source proprietary app status.

---

## 5. Numbers & Quantitative Claims
- **Memory Ceiling:** Peak RSS capped at $\le$350 MiB for 1080p on 4GB RAM devices; $\le$700 MiB for 4K.
- **Throughput Targets:** $\ge$30 fps for 1080p30 hardware transcoding on midrange/flagship; $\ge$1.0× real-time at 720p30 sustained on low-end entry devices.
- **Thermal Threshold:** Zero severe thermal throttle states sustained for >60 seconds in a 10-minute job.
- **Compression Efficiency Gate:** Target $\ge$10% lower bitrate than hardware AVC baseline at equal VMAF v1 quality.
- **MobileNVC Research Result:** 38.9 fps 1080p YUV420 neural decode on Snapdragon 8 Gen 2; up to 48% BD-rate gain over prior on-device baseline `[P-VERIFIED, WACV 2024]`.
- **AOSP AV1 CDD Requirement:** Only 480p30 at 1 Mbps required for supported Media Performance Classes `[P-VERIFIED]`.

---

## 6. Citations & Live Verification Status
- *ITU-T H.264 V16 (Jun 2026), H.265 V11 (Jan 2026), H.266 V4 (Jan 2026)*: Verified primary standards.
- *RFC 9043 (FFV1 v0, v1, v3, Aug 2021)*: Verified primary informational specification.
- *Android 17 AOSP VVC Documentation (Nov 2026)*: Verified primary platform status.
- *Android 16 CDD (§5.1) & Android 17 preview CDD*: Verified primary platform documents.
- *Access Advance MCBA Discount Announcement (11 Nov 2026)*: Verified primary legal document.
- *MobileNVC (WACV 2024, IEEE / arXiv:2310.01258)*: Verified primary paper.
- *DCVC-RT (CVPR 2025) & DCVC-UF (CVPR 2026)*: Verified primary papers/repos.

---

## 7. Strengths
- **Exemplary Pragmatism & Feasibility (§1.1, §4.6):** The most immediately shippable plan in the entire cohort, built on verified Android platform realities rather than speculative hardware roadmaps.
- **Clear Demarcation of Lossless (§2.1):** Solves the lossless ambiguity trap by dividing it into three strict contracts (File copy, Pixel-domain FFV1, Raw sensor).
- **In-Depth Android Platform Knowledge (§2.3, §4.4):** Highlights that `isHardwareAccelerated()` is an unverified OEM flag, exposes AOSP's lack of VVC encoding, and avoids deprecated NNAPI.
- **Strict Memory & Thermal Controls (§4.5, §5.5):** Bounded-memory streaming with explicit tile/stripe processing prevents OOM crashes on 4GB handsets.

---

## 8. Weaknesses
- **Modest Compression Ambition (§1.1, §3.2):** By entirely excluding learned neural compression and software AV2 from the product path, maximum compression gains lag behind advanced hybrid plans (P04, P08, P11).
- **FFmpeg Binary Footprint & LGPL Risk (§4.6, §7.2):** Bundling libavcodec/libavformat for FFV1 and Matroska adds 10–25 MB to the APK and requires strict dynamic linking compliance to satisfy LGPLv2.1.
- **Absence of Temporal Delta Coding for Lossless (§2.1, §4.2):** FFV1 is intra-only, missing the 2×–3× temporal compression gains possible on screen recordings and video bursts.

---

## 9. Scores
- **Depth:** 5/5 — Thorough coverage of Android media APIs, color spaces, buffer flows, and licensing terms.
- **Accuracy:** 5/5 — Highly accurate; primary sources cited for every platform constraint and standards milestone.
- **Originality:** 3/5 — Focuses intentionally on proven engineering combinations rather than novel algorithmic inventions.
- **Feasibility:** 5/5 — The highest feasibility score in the benchmark; could be implemented immediately by an experienced Android media team.
- **Completeness:** 5/5 — Detailed schedule, realistic budget, device lab, red-team analysis, and 10 prioritized experiments.
- **Evidence:** 5/5 — Exceptional evidence quality; relies strictly on primary standards, AOSP documentation, and verified papers.
