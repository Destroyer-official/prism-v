# P14 Digest: Next-Gen Android Video Compression — Engineering Master Plan (2027–2030)

**Source File:** `research_implementation_plane14.md`  
**Total Lines:** 1,171 | **Bytes:** 108,312  
**Simulation Date:** November 14, 2026  
**Author / Attributed Identity:** Systems & Codec Planning Team  

---

## 1. Thesis
P14 advocates an evidence-grounded, dual-engine architecture over a shared local Android media pipeline. It explicitly rejects creating a proprietary video bitstream or container in v1, and rejects universal codec generalizations:
- **Lossless Mode (Two Strictly Separated Operations):**
  - *Stream Preservation:* Copies encoded audio/video packet payloads unchanged without decoding or re-encoding. Bit-identical packet payloads; container metadata may be updated.
  - *Pixel-Domain Exact Transcode:* Decodes to a canonical sample format and re-encodes using **FFV1 v3 in Matroska (`.mkv`)** as the archival baseline, benchmarking SVT-AV1 lossless as the high-compression candidate. Every decoded sample must match the canonical source frame bit-for-bit.
- **Lossy Mode (Pareto Objective-Driven):**
  - Runtime-selected Android hardware encoder (`MediaCodec` AV1, HEVC, AVC, or APV) plus a native **SVT-AV1 software backend** for high-efficiency jobs on capable devices.
  - Four user objectives: **Best Quality for Size**, **Smallest File for Quality**, **Fastest Completion**, and **Lowest Battery Use**.
- **Rejection of Neural Decoders in v1:**
  - Machine learning models are strictly excluded from normative decoding. Quantization does not guarantee cross-device bit-exactness across Qualcomm, MediaTek, Exynos, and Tensor NPUs/GPUs. Learned models are restricted to an optional encoder-side complexity/rate estimator.
- **Rejection of VVC as Primary Encoder:**
  - Notes that while Android 17 AOSP adds VVC playback plumbing, it provides zero software VVC decoder and zero encoding support.

---

## 2. Technical Decisions
- **Architecture & System Design:** Dual-engine architecture with a shared native C++20 pipeline; runtime device capability probing; bounded memory queues; transactional atomic output commit.
- **Algorithms & Codecs:**
  - Lossless: FFV1 v3 (RFC 9043) in Matroska (`V_FFV1`); SVT-AV1 lossless candidate (leveraging v4.3.0 lossless improvements).
  - Lossy: Hardware `MediaCodec` (HEVC/AVC/AV1/APV) and software `SVT-AV1` (pinned to v4.3.0, released 1 Nov 2026).
  - Creator/Mezzanine: APV (IETF RFC 9924) supported via Android 16 APIs.
- **Machine Learning (Encoder-Side Analysis Only):**
  - Content-Conditioned Encode Planner (CCEP): Small (1–5M param) linear/GBDT model predicting scene class, motion magnitude, and noise floor to select presets. Evaluated against deterministic rule heuristics.
  - Runtime: LiteRT CPU/GPU baseline; standalone NPU integration only where verified. Avoids deprecated NNAPI.
- **Languages & Tools:**
  - Kotlin: Android UI (Jetpack Compose), lifecycle, WorkManager, Storage Access Framework (SAF).
  - C++20 (NDK r27+): Native media core, FFmpeg integration, SVT-AV1 wrapper, explicit plane memory management. Strictly rejects Rust in v1 to avoid FFI/build complexity with FFmpeg and Android NDK.
  - FFmpeg: Pinned minimal LGPLv2.1 build (libavcodec, libavformat, libswscale) stripped of all GPL components (`--disable-gpl`, `--disable-nonfree`).
- **Hardware Utilization:**
  - VPU: Hardware `MediaCodec` surface-to-surface zero-copy paths.
  - CPU: Multi-threaded ARM64 NEON/SVE2 dispatch; bounded thread pool to prevent SoC oversubscription.
  - GPU: Vulkan/OpenGL compute strictly for measured lossy pre-processing (scaling, color conversion); strictly forbidden for lossless pixel-exact paths.
  - NPU: Completely excluded from normative decoding.
- **Memory Management:** Bounded queues and reusable frame pools; direct buffers; peak RSS budgets: $\le$128 MiB PSS (1080p SDR) and $\le$256 MiB PSS (4K 10-bit).
- **Container Strategy:** MP4 for mainstream lossy (AVC/HEVC/AV1); Matroska (`.mkv`) for FFV1 archival. Custom containers are strictly banned.
- **Quality Metrics:** Primary: **VMAF v1** (June 2026, default NEG mode) and **HDR VMAF-v1** (26 Oct 2026 update); PSNR, SSIM/MS-SSIM, CAMBI (banding), and ITU-R BT.500 subjective testing.

---

## 3. Inventions & Key Contributions
P14 documents five engineering innovations and uncovers a critical prior-art patent:
- **Evidence-Based Codec Router (P14-I01):** Per-device calibration cache storing tested encoder profiles, throughput, and energy per minute, selecting Pareto-optimal backends.
- **Energy- and Thermal-Aware Work Scheduler (P14-I02):** Dynamic scheduler modulating encoder concurrency, resolution, and safe fragment pauses based on Android thermal headroom.
- **Two-Stage Size Predictor (P14-I03):** Low-cost sampled analysis + optional short probe encode + one correction pass to hit tight file-size targets within $\pm$5%.
- **Verifiable Export Manifest (P14-I04):** Machine-readable manifest containing input/output SHA-256 hashes, canonical pixel format, frame count, timestamps, codec version, and verification level.
- **Transactional, Recoverable Output (P14-I05):** Private temporary output writing, segment-boundary checkpoints, and atomic commit/rename preventing corrupted media output.
- **Critical Prior-Art Identification — US 11,178,395 B1 (P14-I06):** Unearths WhatsApp's granted US patent (filed 2020, published Nov 2021) claiming dynamic codec selection based on expected quality improvement and power consumption, warning that broad "quality-per-joule router" claims are anticipated and cannot be patented as novel inventions.

---

## 4. Plan Content
- **Phases & Roadmap (36–44 Weeks / 9–12 Months):**
  - *P0 (W1–4):* Contract & baseline (lossless semantics, corpus manifest, host harness).
  - *P1 (W5–8):* Android pipeline skeleton (SAF, MediaCodec enumeration, temp output, cancellation).
  - *P2 (W9–13):* Lossless foundation (packet-copy, FFV1, pixel exactness, metadata matrix).
  - *P3 (W14–20):* Lossy engines (hardware AV1/HEVC/AVC, SVT-AV1 software, MP4 muxing).
  - *P4 (W21–25):* Adaptive controller (size predictor, quality objectives, device profile, thermal policy).
  - *P5 (W26–32):* Hardening (fuzzing, low-memory limits, HDR/VFR, recovery, playback matrix).
  - *P6 (W33–40):* Beta & release (closed beta, legal review, release gates).
  - *Contingency (W41–44):* Vendor/OS regressions.
- **Team & Budget:** 6.25–6.5 FTE (Principal Media Architect, 2 Codec/Perf Engineers, 1 Android Platform, 1 QA/Perf, 0.75–1 Product/UI, 0.5 Security/Build, external patent counsel). Budget: **₹2.2–4.5 crore** ($260k–$540k USD) for India-based engineering.
- **Testing & Lab Matrix:** 10-class test corpus (natural low/high motion, night/noise, fine texture, screen/UI, animation, HDR, HFR, VFR, rare formats). Physical lab covering Entry 4GB, Budget 6GB, Midrange 8GB, Flagship Qualcomm, Flagship MediaTek, Tensor, and Exynos.
- **Legal & Licensing Strategy:** Thorough review of AOMedia Patent License 1.0 (defensive termination), ITU patent declarations disclaimer, and FFmpeg LGPLv2.1 compliance (strictly excluding GPL x264/x265). Cites WhatsApp patent US 11,178,395 B1 to establish novelty boundaries.

---

## 5. Numbers & Quantitative Claims
- **SVT-AV1 Release Date:** Version 4.3.0 released on **1 November 2026** with lossless fixes, multi-threaded memory reductions, and low-delay parallelism `[VERIFIED—PRIMARY]`.
- **HDR VMAF-v1 Update Date:** Updated on **26 October 2026** by Netflix `[VERIFIED—PRIMARY]`.
- **Memory Ceiling:** App-owned incremental PSS capped at $\le$128 MiB (1080p SDR) and $\le$256 MiB (4K 10-bit). 8-frame 4K P010 uncompressed ring buffer alone equals ~199 MB.
- **Target Size Convergence:** Within $\pm$5% of target size on $\ge$95% of test clips.
- **Lossy Compression Goal:** Target $\ge$5%–10% aggregate BD-rate reduction vs hardware baseline at matched VMAF.
- **Budget:** ₹2.2–4.5 crore ($260k–$540k USD) for 9–12 months.

---

## 6. Citations & Live Verification Status
- *SVT-AV1 Changelog (v4.3.0, 1 Nov 2026)*: Verified primary software release.
- *Netflix VMAF v1 & HDR VMAF-v1 (Oct 26, 2026)*: Verified primary metric update.
- *US Patent 11,178,395 B1 (WhatsApp LLC, Nov 2021)*: Verified primary patent document.
- *RFC 9043 (FFV1 v0, v1, v3, Aug 2021)*: Verified primary specification.
- *ITU-T H.264 V16 (Jun 2026), H.265 V11 (Jan 2026), H.266 V4 (Jan 2026)*: Verified primary standards.
- *AOMedia AV2 Specification v1.0.0 (28 May 2026)*: Verified primary standard.
- *DCVC-RT (CVPR 2025) & DCVC-UF (CVPR 2026)*: Verified primary research repos/papers.

---

## 7. Strengths
- **Extraordinary Legal & Patent Diligence (§3.4, §9.2):** Uncovers WhatsApp's granted US patent US 11,178,395 B1, preventing the team from making unsupportable patentability claims over quality/power-aware codec selection.
- **Pinpoint Current Software Currency (§2.3, §2.5):** Leverages SVT-AV1 4.3.0 (1 Nov 2026) and Netflix HDR VMAF-v1 (26 Oct 2026), ensuring state-of-the-art tools.
- **Realistic Memory & Buffer Architecture (§4.4):** Explicitly calculates 4K P010 memory footprints and enforces strict PSS ceilings to avoid Android low-memory killer (LMK) aborts.
- **Rigorous Lossless Semantics (§1.2, §3.2):** Clearly demarcates packet-copy stream preservation from pixel-domain FFV1 transcode.

---

## 8. Weaknesses
- **Conservative Compression Ceiling (§1.1, §3.2):** Rejects all learned neural codecs and post-filters in v1, yielding less maximum compression than hybrid neural approaches (P04, P08, P11).
- **Exclusion of Rust (§4.2):** Relying on C++20 for all native media parsing increases exposure to memory-safety CVEs in untrusted media handling.
- **No Sensor-Level Temporal Delta Coding (§3.2):** Misses temporal compression opportunities for camera burst sequences, relying on intra-only FFV1.

---

## 9. Scores
- **Depth:** 5/5 — Rigorous examination of Android buffer flows, memory PSS math, SVT-AV1 compiler flags, and prior art.
- **Accuracy:** 5/5 — Highest factual fidelity; catches SVT-AV1 4.3.0 and HDR VMAF-v1 dates perfectly.
- **Originality:** 4/5 — While intentionally conservative in codec design, its patent analysis and objective budgeting are first-rate.
- **Feasibility:** 5/5 — Exceptional engineering feasibility; perfectly aligned with Android platform reality.
- **Completeness:** 5/5 — Full schedule, realistic budget, red-team analysis, and prioritized experiments.
- **Evidence:** 5/5 — Primary source references throughout, with strict evidence tagging.
