# P06 Digest: Implementation Plan: Next-Gen Android Video Compression (2027+)

**Source File**: `research_implementation_plane6.md`  
**Total Lines**: 131  
**Author/Context**: Undated/November 2026; compact 131-line overview.

---

## 1. Thesis
P06 introduces an **Adaptive Layered Codec Architecture (ALCA)** that dynamically shifts compression workloads between CPU, GPU, NPU, and dedicated hardware media accelerators based on user priority (Quality, Size, Speed, or Battery):
- **Lossless Mode**: Claims true mathematically bit-exact preservation using Samsung's **APV (Advanced Professional Video, RFC 9924)** codec in Android 16, falling back to hardware HEVC Main 10 Lossless profile.
- **Lossy Mode (Battery/Speed)**: Hybrid combination of hardware AV1 or HEVC base layer enhanced by **MPEG-5 Part 2 LCEVC** calculated on CPU via Hadamard transforms.
- **Lossy Mode (Quality/Size)**: Hardware **VVC (H.266)** via Android 17 MediaCodec, with an **NPU-Gated Neural Residual Layer** (adapted from Microsoft MLVC) running in INT8 on the NPU as an enhancement layer or fallback.
- **Shared Foundation**: Standard MP4 container across all paths, C++20 core via Android NDK `MediaCodec` C-API, and common tradeoff sliders.

---

## 2. Technical Decisions
- **Architecture**: 4-tier workload routing (§1, §3): Input -> User Priority Selector -> Lossless (APV / HEVC Main 10 Lossless) vs Lossy Battery (GPU downscale + HW HEVC base + LCEVC software residuals) vs Lossy Max Quality (HW VVC + NPU INT8 neural residual) -> MP4 container.
- **Algorithms**:
  - Lossless: APV (RFC 9924) intra-frame tiling; HEVC Main 10 lossless.
  - Lossy: Hardware HEVC/AV1 base; MPEG-5 LCEVC (Hadamard transforms); hardware VVC; INT8 learned residual synthesis (§3).
- **Machine Learning**: Custom INT8 Neural Residual Codec derived from MobileNVC / Microsoft MLVC. Runs on NPU to predict residuals between reconstructed VPU base and original frame. Uses strict INT8 quantization to avoid floating-point drift, with CPU XNNPACK fallback (§3 Choice 2, §5 A).
- **Languages and Tools**: Kotlin for UI and background orchestration; C++20 for core encoding pipeline; Python/PyTorch for offline residual training; Android NDK, `MediaCodec` C-API, OpenAPV, libLCEVC, Android NNAPI / TFLite, Vulkan Compute (§4).
- **Phone Hardware**: Hardware VPUs for base encode (AV1, HEVC, VVC); GPU via Vulkan for color conversion and 2x downscaling; NPU via NNAPI for neural residuals (capped at <=1.5W); CPU for LCEVC Hadamard residuals (§3, §4).
- **Container**: Standard **MP4 (ISOBMFF)** across all modes (§3 Choice 3).
- **Android Integration**: Native `MediaCodec` C-API, Android 16 APV support, Android 17 `MIMETYPE_VIDEO_VVC`, Camera2 YUV directly into Vulkan memory, `dumpsys batterystats` (§1, §2, §4).

---

## 3. Inventions and Novel Ideas
- **NPU-Gated Neural Residual Layer (§3 Choice 2)**: Replaces LCEVC's mathematical Hadamard transforms with a lightweight INT8 neural network running on the NPU to compress high-frequency residuals over a hardware VPU base.
- **INT8 NPU Determinism Contract with CPU XNNPACK Fallback (§3 Choice 2, §5 A)**: Enforces integer-only quantization on the NPU to eliminate floating-point divergence, with an automatic fallback to XNNPACK on CPU if NPU drivers drift.

---

## 4. Plan Content
- **Phases & Milestones (§4)**: 10-month roadmap across 4 phases:
  - Phase 1 (M1–M2): Foundation (C++ pipeline, Camera2 to Vulkan, HW HEVC + LCEVC real-time; gate: 1080p60 at <15% battery/hr).
  - Phase 2 (M3–M4): Lossless Core (OpenAPV integration; gate: sustain 4K30 on flagship without frame drops).
  - Phase 3 (M5–M7): Neural Enhancement Layer (INT8 residual model on NNAPI; gate: beat VVC VMAF at equal bitrate).
  - Phase 4 (M8–M10): Optimization & Release (Tradeoff sliders, monorepo packaging, AAB with dynamic delivery).
- **Team**: Not specified.
- **Cost**: Not specified.
- **Testing & Benchmarks (§4)**: VMAF (>95 for high quality), PSNR, battery depletion rate (mW via `dumpsys batterystats`), encoding FPS; 10-minute 4K60 thermal stress test (§3 Choice 1, §4).
- **Legal Strategy (§4)**: Relies on SoC vendor hardware licenses for VVC/HEVC `MediaCodec`; acknowledges software licensing requirement for LCEVC; claims custom NPU neural codec will be proprietary and royalty-free (§4).
- **Go/No-Go Gates**: Concrete gates per phase (e.g. Phase 1: 1080p60 <15% battery/hr; Phase 3: neural mode must beat VVC VMAF, else pivot to hardware VVC).

---

## 5. Numbers and Quantitative Claims
- VVC efficiency: Cuts HEVC bitrates by ~50% (Source: Verified primary source / text claim, §2).
- AV2 efficiency: ~30% fewer bits than AV1 (Source: Verified secondary source, §2).
- LCEVC efficiency: Cuts bitrates by 10–25% over HEVC and saves battery vs software encoding (Source: Verified secondary source, §2).
- NPU power cap: If NPU power draw exceeds 1.5W for 1080p60 stream, kill neural path and default to LCEVC (Source: Author design, §3 Choice 2).
- Battery target: 1080p60 encoding with <15% battery drain per hour (Source: Author gate, §4).
- Quality target: VMAF > 95 for high quality (Source: Author target, §4).

---

## 6. Citations and Verification Assessment
- Samsung APV (RFC 9924, Feb 2026) — Verified primary RFC.
- AV2 finalized May 28, 2026 — Verified primary/secondary date.
- LCEVC (MPEG-5 Part 2) — Verified primary ISO standard.
- Real-time NVC on mobile (arXiv:2310.01258v3, Nov 2023) — Verified arXiv preprint.
- Microsoft MLVC open-sourced July 2026 — Verified open-source release.
- Android 17 VVC support via Codec2 — Verified primary AOSP documentation.
- Qualcomm/MediaTek hardware VVC decoders (Sept 2026) — Verified trade press announcements.
- VVC decoding energy demands (arXiv:2402.09001v1, Feb 2024) — Verified arXiv paper.

---

## 7. Strengths
- **Compact & Practical Core Concept (§1, §3)**: The division between Battery Mode (HEVC + LCEVC) and Quality Mode (VVC / Neural residual) is a logical, low-complexity framework.
- **Zero-Copy Vulkan Memory Pipeline (§4 Phase 1)**: Capturing Camera2 YUV directly into Vulkan memory minimizes CPU-GPU memory copies.
- **Clear Power Gate on NPU (§3 Choice 2)**: Explicit 1.5W ceiling on NPU execution prevents runaway thermal degradation.

---

## 8. Weaknesses
- **Severe Factual Error Regarding APV Lossless Status (§1, §3 Choice 1)**: P06 explicitly claims APV provides "true mathematically bit-exact preservation of YUV 4:2:0/4:2:2 camera frames." This is completely false. Samsung's APV (RFC 9924) is a *visually lossless, high-bitrate intra-frame editing codec* (analogous to Apple ProRes or Avid DNxHR). It is NOT mathematically bit-exact lossless!
- **Reliance on Deprecated NNAPI (§1, §4, §5)**: P06 relies heavily on Android NNAPI for deploying its neural models. NNAPI was deprecated in Android 15 and is completely obsolete in Android 16/17; modern Android NPU deployment requires Google AI Edge LiteRT and vendor-specific delegates.
- **Severe Lack of Depth & Completeness**: Only 131 lines total. Completely omits team composition, engineering budget, risk analysis, detailed hardware usage matrices, container syntax specifics, and test clip corpora.
- **VVC Encode Assumption (§1, §3 Choice 2)**: Assumes hardware VVC encoding is available on Android 17 devices; late 2026 SoCs (Snapdragon 8 Elite Gen 6, Dimensity 9600 Pro) only include VVC *hardware decoders*, not encoders.

---

## 9. Scores (1–5) and Reasoning
- **Depth**: 2/5 — Very brief high-level sketch with minimal technical elaboration.
- **Accuracy**: 2/5 — Critical errors: misidentifies lossy APV as bit-exact lossless, relies on deprecated NNAPI, and assumes mobile hardware VVC encoders exist.
- **Originality**: 3/5 — Good concept of NPU-gated neural residual layer over VPU base, but largely derivative of LCEVC.
- **Feasibility**: 2/5 — Flawed foundational assumptions (APV lossless, NNAPI, VVC encode) make it unexecutable as written.
- **Completeness**: 2/5 — Missing team, budget, detailed architecture, risk register, and comprehensive metrics.
- **Evidence**: 3/5 — Cites real 2026 events (RFC 9924, AV2 finalization, MLVC), but misapplies them.
