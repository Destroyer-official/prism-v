# P03 Digest: Next-Gen Android Video Compression System: Implementation Plan for 2027+

**Source File**: `research_implementation_plane3.md`  
**Total Lines**: 581  
**Author/Context**: @shantanu's AI Research Assistant; dated 14 November 2026.

---

## 1. Thesis
P03 proposes a dual-mode video compression system that bets aggressively on **software AV2 encoding combined with NPU-accelerated preprocessing for lossy mode**, and a **novel hybrid of FFv1 and JPEG XL for lossless mode**:
- **Lossless Mode**: Rejects pure FFv1 in favor of a custom hybrid: scene segmentation where keyframes are encoded using JPEG XL lossless, while subsequent inter-frames are temporal delta-encoded using FFv1's range coder.
- **Lossy Mode**: AV2 software encoding (libavm ported/optimized in Rust) as the primary engine, paired with an INT8 U-Net running on the NPU for perceptual pre-filtering (denoising/artifact reduction), falling back to hardware AV1/HEVC only on low-end devices.
- **Shared Foundation**: Common NDK/JNI abstraction, an adaptive runtime complexity scheduler that samples hardware during the first 5 seconds, and Matroska (MKV) container formatting.

---

## 2. Technical Decisions
- **Architecture**: Multi-tier pipeline (§4.1): Kotlin UI -> Compression Manager -> Lossy path (NPU Pre-filter -> AV2 Encoder in Rust -> AV2 Entropy Coder) vs Lossless path (Scene Detector -> JPEG XL Keyframe Encoder -> FFv1 Temporal Delta) -> MKV Muxer -> NDK/JNI -> Hardware fallback.
- **Algorithms**:
  - Lossless: Temporal scene detection, JPEG XL (libjxl) intra-coding, FFv1 temporal delta range coding (§3.2).
  - Lossy: AV2 software encoding (libavm) with rate-distortion tools; NPU U-Net pre-filter; hardware AV1/HEVC fallback (§3.1).
- **Machine Learning**: Rejects end-to-end neural codecs (e.g., DCVC-UF) due to 100–500MB model sizes, high NPU complexity, and non-determinism (§2.2). ML is strictly confined to: (1) Scene classifier (<5MB MobileNetV3-Small), (2) Perceptual pre-filter (10–15MB INT8 U-Net), and (3) Content complexity analyzer (<2MB CNN) (§4.4).
- **Languages and Tools**: Rust for new codec components and FFv1 port; C (libavm) for AV2 core; C++ for libjxl, TFLite/QNN delegates, and JNI bridge; Kotlin for Android app; Bazel/CMake build system; Python/PyTorch for ML research (§4.2).
- **Phone Hardware**: Multi-core ARMv9 CPU (NEON, SVE, SVE2) for AV2 and FFv1; NPU (Hexagon, APU, TPU) for pre-filtering; GPU (Vulkan Compute) for JPEG XL keyframes and DCT transforms; RAM management limiting reference frames (§4.3).
- **Container**: Matroska (MKV) with a custom TrackEntry (`V_MKV_LOSSLESS_HYBRID`) for the JPEG XL + FFv1 stream (§4.5).
- **Android Integration**: NDK/JNI layer, Qualcomm AI Engine Direct SDK / MediaTek APU SDK (due to NNAPI deprecation in Android 15), MediaCodec hardware fallback (§2.4, §2.5).

---

## 3. Inventions and Novel Ideas
- **FFv1 + JPEG XL Lossless Hybrid (§3.2)**: Uses JPEG XL for scene keyframes (claiming 20–40% better intra-compression than FFv1) and FFv1 range-coded deltas for inter-frames, claiming 15–25% overall size reduction over standard FFv1.
- **Adaptive Complexity Runtime Scheduler (§3.3)**: Probes CPU/GPU/NPU during the first 5 seconds of encoding and dynamically adjusts motion search range, reference frame count, and transform sizes to match real-time throughput and predicted battery drain.
- **NPU-Accelerated Perceptual Pre-filtering (§3.4)**: 10–15MB INT8 U-Net running on mobile NPU at 30+ FPS (1080p) to remove camera sensor noise and compression artifacts prior to encoding, claiming 5–12% bitrate savings.
- **Decoupled Cross-Device Determinism (§3.5)**: Enforces determinism by keeping all ML models strictly on the encoder input (pre-filtering), ensuring the codec core and entropy coders remain 100% integer-deterministic across devices.

---

## 4. Plan Content
- **Phases & Milestones (§4.8)**:
  - Phase 1: Research & Prototyping (Months 1–6): M1.1 AV2 mobile feasibility, M1.2 Lossless hybrid prototype, M1.3 NPU pre-filter, M1.4 Container, M1.5 Integrated prototype.
  - Phase 2: Core Development (Months 7–18): M2.1 AV2 encoder optimization, M2.2 Production lossless codec, M2.3 NPU integration, M2.4 App alpha, M2.5 Multi-device support, M2.6 Beta.
  - Phase 3: Optimization & Release (Months 19–24): M3.1 Performance, M3.2 Battery optimization, M3.3 Play Store release.
- **Team (§4.9)**: 5.5 FTE over 24 months (2 Video Codec Engineers, 1 ML Engineer, 1 Android Engineer, 1 Systems Engineer, 0.5 QA, 0.5 Project Lead).
- **Cost (§4.10)**: USD $1.31M–$2.06M total ($1.2M–$1.8M engineering salaries, $50k–$100k test lab of 50 devices, $20k–$50k cloud compute, $30k–$80k legal/patent).
- **Testing & Benchmarks (§4.6)**: 100 clips (30–60s, 1080p, 1440p, 4K; camera, UGC, screen, gaming, animation; SDR/HDR). 3 device tiers (flagship, mid-range, low-end). Metrics: VMAF, PSNR-HVS, SSIM, CAMBI, BD-rate vs AV1, FPS, mWh/min, peak RSS, battery % drain. Explicit multi-metric scoring function (§4.6).
- **Legal Strategy (§4.11)**: Relies on royalty-free status of AV2, FFv1, and JPEG XL; reserves $0.50–$1.50/device if forced to fall back to VVC/HEVC pools (Access Advance MCBA); mandates FTO analysis for scheduler and hybrid lossless.
- **Go/No-Go Gates**: M1.1 gate: if AV2 encoding is >10x slower than real-time on mobile, pivot to VVC or AV1; Phase 2 gate: if 4K encoding on mid-range takes >5 min, cap mid-range at 1080p.

---

## 5. Numbers and Quantitative Claims
- AV2 BD-rate vs AV1: -29.81% PSNR-YUV, -33.79% VMAF under Random Access (RA) Common Test Conditions (Source: arXiv 2605.15800v1, §2.1, S502).
- AV2 complexity target: <=5x encoder, <=2x decoder vs AV1 (Source: AOMedia CTC targets, §2.1).
- VideoLAN decode complexity claim: AV2 is roughly 5x more complex than AV1 (Source: VideoLAN President May 2025, §2.1).
- DCVC-UF: 371 FPS encode, 273 FPS decode at 1080p on RTX 4090; 42.2% bitrate saving vs VTM (Source: arXiv 2606.04410v1, §2.2).
- Access Advance MCBA VVC mobile royalty: $0.50–$0.60 per device (Source: Access Advance Nov 2026 announcement, §4.11, S495).
- FFv1 + JPEG XL hybrid compression: 15–25% smaller than pure FFv1 baseline (Source: Author estimate, §1, §3.2).
- NPU pre-filtering bitrate saving: 5–12% at matched VMAF (Source: Author estimate from academic literature, §3.4).
- Target encoding speeds: 30–60 FPS 1080p on flagship, 10–15 FPS on mid-range (Source: Author estimate, §1).
- Model sizes: Scene classifier <5MB, U-Net pre-filter 10–15MB, Complexity analyzer <2MB; total <30MB (Source: Author design, §4.4).

---

## 6. Citations and Verification Assessment
- AOMedia AV2 press release / spec finalized (June 9, 2026) — Verified primary date (note: P01/P02 cite May 28-29, 2026; June 9 may be official press release date).
- arXiv 2605.15800v1 (AV2 quality evaluation) — Specific arXiv identifier; needs verification.
- arXiv 2606.04410v1 (DCVC-UF paper, June 2026) — Specific arXiv identifier; needs verification.
- Access Advance MCBA VVC pricing ($0.50–$0.60) — Verified primary licensing announcement.
- VideoLAN President quote on AV2 5x complexity — Verified secondary industry quote.
- Qualcomm AI Engine Direct SDK — Verified primary Qualcomm SDK.

---

## 7. Strengths
- **Creative Lossless Concept (§3.2)**: Pairing JPEG XL (which excels at photographic still compression) for intra keyframes with FFv1 range-coded temporal residuals is a technically plausible, innovative approach to lossless video.
- **Sound Architectural Determinism (§3.5)**: By restricting ML to pre-codec filtering, it eliminates the cross-device floating-point NPU determinism trap that plagues neural codecs.
- **Comprehensive Benchmarking Scoring Function (§4.6)**: Explicit mathematical formula balancing BD-rate, VMAF, FPS, battery drain, and memory.
- **Thoughtful Language Choice (§4.2)**: Justifies Rust for safety and SIMD performance in custom codec components while retaining C/C++ for library FFI.

---

## 8. Weaknesses
- **Severe AV2 Software Encoding Speed Illusion (§1, §3.1, §4.3)**: Believing that software AV2 encoding can achieve 15–30 FPS at 1080p on a mobile CPU in 2027 is wildly unrealistic. The reference encoder (libavm) runs at 1–3 fps on desktop x86 CPUs; even with heavy optimization, mobile ARM chips will be severely throttled and unable to sustain interactive AV2 software encoding.
- **Custom Container Player Breakage (§4.5)**: Adding a custom TrackEntry (`V_MKV_LOSSLESS_HYBRID`) to MKV means that no standard video player (ExoPlayer, VLC, browser) will be able to play the resulting lossless file; it becomes a proprietary container locked to this specific app.
- **NPU Continuous Video Power Drain (§3.4, §5.5)**: Running a 15MB U-Net continuously on the NPU at 30 FPS for 1080p video while simultaneously running a heavy software video encoder on the CPU will quickly cause severe thermal throttling (>45°C) and battery depletion.
- **Unrealistic Porting Scope (§4.2)**: Porting libavm and FFv1 to Rust within 24 months with a team of only 2 codec engineers is an immense engineering overhead that distracts from core compression optimization.

---

## 9. Scores (1–5) and Reasoning
- **Depth**: 4/5 — Solid technical exploration of AV2 CTC data, DCVC-UF benchmarks, and Android NPU SDK realities.
- **Accuracy**: 3/5 — Severely underestimates AV2 mobile software encoding complexity, though accurate on standards and neural limitations.
- **Originality**: 4/5 — Highly inventive hybrid lossless (JPEG XL + FFv1) and adaptive 3D scheduler concepts.
- **Feasibility**: 2/5 — Shipping full software AV2 encoding and porting codec stacks to Rust on mobile in 2027 is nearly unachievable without hardware acceleration.
- **Completeness**: 4/5 — Comprehensive plan covering phases, team, cost, metrics, and architecture.
- **Evidence**: 4/5 — Good primary citations (arXiv, AOMedia, Access Advance), with honest self-audit acknowledging weak claims.
