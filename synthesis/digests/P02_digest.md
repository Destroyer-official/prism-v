# P02 Digest: Next-Gen Android Video Compression (Lossless + Lossy), 2027+

**Source File**: `research_implementation_plane2.md`  
**Total Lines**: 655  
**Audience/Context**: Founding engineering team, technical investors; dated 14 November 2026.

---

## 1. Thesis
P02 advocates an ultra-pragmatic, production-first Android compression system that pairs two specialized codec engines behind an **Analysis & Decision Engine (ADE)**:
- **Lossless Mode**: **FFV1 v3 in Matroska (.mkv)** for archival, screen content, and guaranteed bit-exact software execution; opportunistic **hardware HEVC lossless** (via `FEATURE_Lossless` in `MediaCodecInfo`) for real-time capture only if it passes a runtime bit-exactness smoke test.
- **Lossy Mode**: **AV1 hardware encode** as primary on supported flagships; **HEVC hardware encode** as universal low-power fallback; **SVT-AV1 software encode** (preset 6–8) strictly for deferred-while-charging execution. Container: **CMAF / fragmented MP4**. Deliberately rejects AV2, VVC encoding, LCEVC, and neural codecs as primary engines in v1.
- **Shared Foundation**: The ADE profiles input characteristics (resolution, motion, noise, screen vs camera), hardware capabilities, and battery/thermal state to translate a user trade-off slider (Quality ↔ Size ↔ Speed ↔ Battery) into codec parameters and execution timing.

---

## 2. Technical Decisions
- **Architecture**: Four-layer structure (§4.1): UI (Kotlin Compose), Orchestration (ADE, WorkManager, Thermal), Native Media (C++17, `AMediaCodec`, `AMediaMuxer`, FFmpeg 7.x, SVT-AV1, libvmaf), and an optional lightweight ML Content Classifier (§4.4).
- **Algorithms**:
  - Lossless: FFV1 v3 intra/inter with range coding and context modeling; HEVC transform-skip/PCM lossless.
  - Lossy: Hardware AV1/HEVC via MediaCodec; software SVT-AV1 with presets 6–8; VMAF-targeted rate control mapping (§3.2).
- **Machine Learning**: Strictly relegated to **pre-analysis and decision support** (optional v1.1 <2MB int8 TFLite classifier for screen vs camera, motion score), NOT the codec bitstream. Neural decoders are rejected due to lack of mobile encoders and non-deterministic cross-NPU output (§2.3, §4.4).
- **Languages and Tools**: Kotlin with Jetpack Compose & Baseline Profiles (`androidx.profileinstaller`); C++17 via NDK; FFmpeg 7.x (libavcodec, libavformat, libavfilter); SVT-AV1; libvmaf; Python for test harness; Gradle + CMake (§4.2).
- **Phone Hardware**: Hardware video codec blocks (`MediaCodec`) are primary; CPU (NEON, SVE2) for FFV1 and deferred SVT-AV1; GPU reserved for VMAF in lab and UI rendering; NPU used only for optional classification (§4.3).
- **Container**: **CMAF / fMP4** for lossy (ISO/IEC 23000-19:2024); **Matroska (.mkv)** for FFV1 lossless (§2.5, §3.5).
- **Android Integration**: Native `AMediaCodec`, `Window.setSustainedPerformanceMode(true)` for jobs >60s, `PowerManager.getCurrentThermalStatus()`, and `WorkManager` for charging+idle execution (§2.4, §3.3).

---

## 3. Inventions and Novel Ideas
- **Analysis & Decision Engine (ADE) (§3.1)**: Device-capability-aware, VMAF-target-driven router that dynamically inspects device hardware and input video samples (2–5s) to choose the optimal engine, preset, and execution schedule.
- **Runtime Bit-Exactness Smoke Test (§3.4)**: Solves buggy OEM hardware lossless implementations by encoding and decoding a 1-second YUV sample to verify SHA-256 match before executing a full job.
- **VMAF-Targeted Rate Control for MediaCodec (§3.2)**: Overcomes MediaCodec's lack of Constant Quality (CQ/CRF) by using an empirical rate-quality lookup model calibrated across device classes, with an optional two-pass prefix correction.
- **Pre-Flight Cost Estimator Bottom Sheet (§3.3)**: Transparent UX showing predicted wall-clock time and battery drain upfront (e.g., "Compress now: ~4 min, 12% battery vs when charging: ~6 min, plugged in").

---

## 4. Plan Content
- **Phases & Milestones (§4.5)**:
  - Phase 0 (Weeks 1–4): Baseline & Feasibility (M0.1 Benchmark harness, M0.2 MediaCodec capability matrix, M0.3 End-to-end slice, M0.4 Fallback decision).
  - Phase 1 (Weeks 5–12): Product v1 (M1.1 ADE v1, M1.2 Thermal scheduler + Sustained Performance, M1.3 Container correctness, M1.4 Legal review).
  - Phase 2 (Weeks 13–20): Hardening & Beta (M2.1 50-device beta, M2.2 VMAF user validation, M2.3 Thermal profiling).
  - Phase 3 (Weeks 21–26): Launch & Research Track (M3.1 Production Play Store release, M3.2 AV2 watch report, M3.3 Neural decoder feasibility).
- **Team (§4.7)**: 4.35 FTE over 26 weeks (1 Tech Lead/Architect, 1 Kotlin Engineer, 1.5 C++ Native Media, 0.5 Benchmark/Quality, 0.25 UX Designer, 0.1 Legal/IP Consultant).
- **Cost (§4.7)**: USD $440k–$710k for 26 weeks (Salaries $350k-$550k, test devices $5k-$15k, legal $10k-$25k, contingency 20%).
- **Testing & Benchmarks (§4.6)**: 20 curated clips (30s each, 1080p and 4K; daylight, low-light, screen text, screen video, sports, animation, HDR 10-bit). 3 physical devices (low-end, mid-range, flagship). Metrics: SHA-256 bit-exactness, VMAF v0.6.1, PSNR-YUV, SSIM, bitrate, wall-clock, RSS, battery mAh, skin temp.
- **Legal Strategy (§4.8)**: Relies on SoC vendor hardware licensing; models $0.02–$0.05 per-download patent reserve for AV1/AV2 pool exposure (Access Advance); flags FFmpeg LGPL dynamic linking compliance.
- **Go/No-Go Gates**: Concrete kill criteria at each phase (e.g., M0.4: if SVT-AV1 <1x real-time on low-end, demote to deferred-only; M2.1: if lossless bit-exactness fails >1%, disable offending hardware path; M2.3: skin temp >45°C blocks device support).

---

## 5. Numbers and Quantitative Claims
- FFV1 v3 compression ratio: 2x–4x vs raw YUV on screen content; 1.2x–2x on camera content (Source: author estimate / codec literature, §1.1).
- AV1 vs HEVC/H.264: 35%–50% bitrate reduction vs HEVC at VMAF 93–95; 50%–65% vs H.264 (Source: published comparisons, §1.1).
- AV2 vs AV1: 28.6% PSNR-YUV, 32.6% VMAF, >40% screen content (Source: AOMedia numbers / Nikodev1 Aug 2026, §2.2.2, S3).
- AV2 reference encoder speed: 1–3 fps at 1080p on modern CPU (Source: Fosslinux June 2026, S4).
- VVC playback power: Snapdragon 8 Elite Extreme claims up to 55% longer battery life vs HEVC (Source: Qualcomm Sept 2026, S6).
- LCEVC royalty: Starts at US$0.20 per consumer device (Source: LCEVC.org Nov 2025, S9).
- HybridPrompt: >150 FPS 1080p decode on commercial smartphone at ~200 kbps (Source: ACM DL April 2026, S16).
- MobileNVC: >30 fps 1080p neural decoding on mobile (Source: arXiv, S17).
- Warping-based neural codec patent (US12501050B2): Up to 40% BD-rate savings, 9x lower complexity (Source: Patsnap Dec 2025, S19).
- GreyB AV2 patents: 126 patents from 21 innovators covering AV2 (Source: GreyB July 2026, S21).
- Thermal / time thresholds: Jobs >60s use Sustained Performance Mode; deferred if battery <30% and time >60s; skin temp limit 45°C (§3.1, §4.5).

---

## 6. Citations and Verification Assessment
- S1–S5 (AOMedia, Tweaktown, Nikodev1, Fosslinux, Forasoft): AV2 1.0.0 finalization, 30% claims, and 1-3 fps reference encoder speed — Consistent with industry reality in mid-2026.
- S6/S7: Snapdragon 8 Elite Extreme VVC playback — Qualcomm Snapdragon Summit coverage; needs confirmation of exact marketing claim.
- S8/S9: LCEVC royalties ($0.20/device) — Verified primary licensing terms from LCEVC.org.
- S10/S11: Android Baseline Profiles & Sustained Performance Mode — Verified primary AOSP documentation.
- S14/S15: CMAF packaging & AV2 ISO BMFF binding — Real technical standard status.
- S16–S18: HybridPrompt, MobileNVC, MobileCodec — Verified academic papers on mobile neural video decode.
- S19: US Patent US12501050B2 — Needs verification of patent grant date and assignee.
- S20/S21: Access Advance AV1/AV2 pool exploration & GreyB 126 patents — Real patent pool landscape events in 2026.
- S26: FOSDEM FFV1 talk — Verified open-source community reference.

---

## 7. Strengths
- **Superior Realism & Restraint (§1.2, §3.6)**: Refuses to jump on unready technologies (AV2, neural primary codecs, LCEVC) in v1, correctly identifying that shipping a codec that cannot decode on receiving devices destroys product value.
- **Runtime Hardware Verification (§3.4)**: The 1-second bit-exactness smoke test for `FEATURE_Lossless` is an essential real-world safeguard against broken OEM hardware encoders.
- **Practical Rate Control for MediaCodec (§3.2)**: Direct solution for MediaCodec's lack of CRF/CQ modes via empirical rate-quality lookup tables.
- **Pragmatic Platform Optimization (§1.2, §3.3, §4.1)**: Uses `Window.setSustainedPerformanceMode(true)`, Baseline Profiles, and thermal status monitoring.
- **Transparent Budget & Team Sizing (§4.7)**: Realistic 26-week, 4.35 FTE, $440k–$710k scope for a native Android media app.

---

## 8. Weaknesses
- **Lack of True Innovation in Compression Core (§2.2, §3.1)**: Relies entirely on standard off-the-shelf codecs (FFmpeg FFV1, SVT-AV1, MediaCodec). It is essentially a smart UI and router over existing open-source libraries rather than an advanced compression system.
- **SVT-AV1 Mobile Viability (§2.2.1, §4.3)**: Even at preset 8, running SVT-AV1 in software on ARM mid-range SoCs is notoriously slow and power-hungry; may take 10–20x real-time on complex scenes, making deferred jobs take hours.
- **Matroska (.mkv) Android Native Incompatibility (§2.5)**: While Matroska is standard for FFV1, native Android MediaExtractor / MediaMuxer lacks first-class MKV muxing support, requiring heavy custom C++ muxers and limiting player compatibility in generic gallery apps.
- **Overly Conservative Neural Stance (§2.3)**: Completely dismisses neural codecs from the bitstream, missing opportunities for lightweight enhancement layers or neural pre/post-filters that could provide competitive edges.

---

## 9. Scores (1–5) and Reasoning
- **Depth**: 4/5 — Excellent analysis of real-world Android platform constraints, thermal hal APIs, and MediaCodec quirks.
- **Accuracy**: 5/5 — Highly accurate depiction of 2026 codec ecosystem (AV2 reference speeds, LCEVC royalties, NPU non-determinism).
- **Originality**: 3/5 — High product utility (ADE router, smoke test), but low algorithmic originality (uses existing codecs as-is).
- **Feasibility**: 5/5 — The most directly implementable plan; avoids all high-risk research traps.
- **Completeness**: 4/5 — Covers engineering, testing, legal, and team thoroughly, though light on algorithmic innovations for 2027+.
- **Evidence**: 5/5 — Transparent citations, realistic estimates, and comprehensive self-audit.
