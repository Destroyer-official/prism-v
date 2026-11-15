# P01 Digest: Next-Gen Android Video Compression (Lossless + Lossy), 2027–2030

**Source File**: `research_implementation_plane1.md`  
**Total Lines**: 584  
**Author/Prompt**: @shantanu (planning prompt), dated 14 November 2026.

---

## 1. Thesis
P01 proposes a "two-engine, one-pipeline" Android compression architecture where lossless and lossy are treated as distinct user intents over a shared container, capability probe, and background scheduling layer:
- **Lossless Mode**: Primary product is bit-exact recompression of already-encoded files (e.g., reclaiming phone storage from HEVC videos) plus near-lossless screen recording. It explicitly rejects raw-camera lossless at scale as thermally/economically unfeasible. Core engine is FFV1 (RFC 9043) with modern range coder, NEON vectorization, and slice-level parallelism, with opportunistic fallback to HEVC lossless hardware if supported.
- **Lossy Mode**: Codec-agnostic container layer with a tiered encoder backend: HEVC hardware encode as default (2027–2028), AV1 software encode on flagship devices as a bridge, and AV2 hardware encode as the 2029+ target. VVC is treated strictly as a decode/transcode target due to lack of mobile hardware encoders.
- **Shared Foundation**: Fragmented MP4 (fMP4) with custom `uuid` metadata box, device hardware probe (`MediaCodecInfo`), thermal/battery-gated background queue, and a strict cross-device determinism contract.

---

## 2. Technical Decisions
- **Architecture**: Modular pipeline comprising `probe` (device capabilities), `policy` (intent/tier mapping), `lossless` (FFV1 + planner), `lossy-base` (HEVC/AV1/AV2 backends), `neural` (enhancement layer), `container` (fMP4/MKV), `queue` (WorkManager scheduling), and `benchmark` (lab VMAF/power tools) (§4.1).
- **Algorithms**:
  - Lossless: FFV1 intra-frame coding (RFC 9043) with range coding and slice parallelism (§3.1).
  - Lossy: Hardware HEVC baseline; software AV1 (libaom / SVT-AV1); AV2 reference AVM; neural hybrid enhancement.
- **Machine Learning**: MLVC (ECCV 2026) inspired neural enhancement layer. Solves cross-device NPU floating-point non-determinism via "transmit-the-scales" through the hyperprior rather than enforcing bit-exact integer math. Neural model is strictly an enhancement layer over a standard base stream (§3.3).
- **Languages and Tools**: Kotlin for Android UI/orchestration/WorkManager; C/C++ (NDK) with NEON for codecs, FFV1, and container muxing; C++ with LiteRT 2.3 + vendor delegates (Hexagon QNN, NeuroPilot, Tensor) for ML; Python for offline VMAF/BD-rate benchmarks; Gradle + CMake (§4.2).
- **Phone Hardware**: SoC hardware video encoders (MediaCodec) for primary lossy encode; NPU via LiteRT vendor delegates for opt-in enhancement; GPU reserved for pre-processing/color conversion; CPU (NEON) for FFV1 and fallback (§2.3).
- **Container**: Fragmented MP4 (fMP4) with a custom `uuid` box storing codec info, model version, determinism hash, and original file hash; optional MKV export for FFV1 (§3.4).
- **Android Integration**: Native `MediaCodec` NDK/Java APIs, `WorkManager`, `BatteryManager`, `PowerManager` for thermal throttling monitoring (§3.5, §4.1).

---

## 3. Inventions and Novel Ideas
- **Recompression Planner (§3.1)**: Uses a fast sample of 10–20 frames and a lightweight regressor (<1 MB) to predict FFV1 output size and decode+re-encode energy against a device power model. Skips recompression if estimated saving <3% or energy exceeds threshold. Claimed gain: prevents wasted battery on incompressible media; 5–15% size reduction on compressible HEVC.
- **Transmit-the-Scales Neural Determinism (§3.3)**: Solves NPU platform divergence by transmitting entropy-coding scale parameters via the hyperprior, decoupling entropy decode consistency from device-specific floating-point arithmetic. Claimed gain: 30–50% bitrate reduction at equal perceptual quality over low-bitrate streams while preserving cross-device playback.
- **Compression Queue as First-Class Product (§3.5)**: System-level thermal and power gating (only runs while charging, battery >40%, temp <38°C; pauses at >42°C).

---

## 4. Plan Content
- **Phases & Milestones (§4.3)**:
  - Phase 0 (M1–M2): Foundations (Device probe, FFV1 baseline, determinism harness, patent first pass).
  - Phase 1 (M3–M5): Lossless MVP (Recompression planner, queue, fMP4 uuid, bit-exact verifier). Release: "Storage Saver".
  - Phase 2 (M6–M10): Lossy HEVC + AV1 bridge (HEVC backend, AV1 software flagship, policy engine, benchmark suite). Release: "Smart Compress".
  - Phase 3 (M11–M15): Neural enhancement layer (MLVC reproduction, NPU deployment, model delivery 10-50MB). Release: "Neural Enhance (Beta)".
  - Phase 4 (M16–M24): AV2 readiness (Software decoder, hardware stub, transcode path).
- **Team (§4.5)**: 6.7 FTEs over 24 months (2 Video Codec Engineers, 1 ML Engineer, 1 Android Platform, 1 C++/NDK, 1 QA/Benchmark, 0.5 Product/UX, 0.2 Legal/IP).
- **Cost (§4.5)**: USD 2.5–4.0M engineering fully loaded over 24 months; speculative USD 0.5–2.0M additional patent pool licensing reserve.
- **Testing & Benchmarks (§4.4)**: 50 clips (10s, 1080p and 4K; camera, screen, animation, conferencing). Metrics: VMAF-phone, PSNR-YUV, BD-rate, bitrate, encode/decode time, battery/thermal deltas, frame SHA-256 determinism.
- **Legal Strategy (§4.6)**: Anticipates patent pools for AV1/AV2 (Access Advance VDP, Sisvel, Avanci); enforces codec substitutability via `CodecBackend`.
- **Go/No-Go Gates**: Clear quantitative criteria at every milestone (e.g., M0.1: 8/10 devices support HEVC HW encode; M1.1: planner error <15%; M2.2: AV1 1080p30 <3min without >50% throttle).

---

## 5. Numbers and Quantitative Claims
- HEVC vs H.264: Median ~33% smaller at equal quality (Source: VideoRecompress Sept 2026, S24).
- Hardware HEVC energy savings: 16% to >92% vs software (Source: IEEE study Jan 2026, S27).
- VVC vs HEVC: ~40% BD-rate saving, ~40% on mobile screen content (Source: Springer Dec 2024, S23).
- AV1 vs HEVC: 15–25% bitrate saving at equal VMAF-phone in software (Source: EST based on secondary benchmarks, §1.2).
- AV2 vs AV1: 28.6% PSNR-YUV / 32.6% VMAF BD-rate reduction (Source: Netflix / Andrey Norkin Oct 2025 presentation, S4).
- AV2 decode complexity: ~5× more computationally demanding than AV1 (Source: VideoLAN estimate, S4).
- DCVC-RT: 1080p real-time on A100 with ~21% bitrate saving over H.266/VTM (Source: arXiv 2502.20762, S9).
- MLVC: >70% BD-rate (MOS) over hardware HEVC on VCD benchmark, ~100 FPS on commodity NPUs at 360p-540p (Source: arXiv 2606.28027, S8).
- HybridPrompt: >150 FPS 1080p decode on iPhone 16 Pro Max, ~8% LPIPS gain at 200 kbps (Source: ACM NOSSDAV 2026, S10).
- SlimFit-Gens: 480p real-time calling at 30 kbps (Source: IEEE TMC 2026, S11).
- Lossless screen content string matching: ~35.67% fewer bytes with <1% runtime overhead (Source: paper claim, §2.2).
- FFV1 recompression of HEVC files: 5–15% size reduction (Source: EST, §1.2).
- Screen recording vs raw: 40–60% smaller (Source: V-S / AC, §1.2).
- Queue thermal thresholds: Battery >40%, start temp <38°C, pause temp >42°C (Source: Author design, §3.5).

---

## 6. Citations and Verification Assessment
- S1: AOMedia AV2 spec v1.0.0 (28 May 2026) — Appears real and relevant (AOMedia GitHub).
- S2: AVM v1.0.0 tag (30 May 2026) — Appears real (reference repo).
- S3: AOSP VVC support docs (18 Jun 2026) — Appears real (Android 17 MediaCodec VVC addition).
- S6/S7: Dimensity 9600 Pro / Snapdragon 8 Elite Extreme Gen 6 VVC decode — Needs verification; likely accurate representation of late 2026 mobile silicon rumors/releases.
- S8: MLVC (arXiv 2606.28027, 26 Jun 2026) — Specific arXiv identifier; critical to verify paper title and scale-transmission concept.
- S9: DCVC-RT (arXiv 2502.20762, CVPR 2025) — Specific arXiv identifier; to verify.
- S10: HybridPrompt (ACM NOSSDAV 2026) — To verify.
- S11: SlimFit-Gens (IEEE TMC 2026) — To verify.
- S12/S13/S14: GreyB, Access Advance VDP, Avanci pools — Highly relevant and factual regarding patent pool pressure on AV1/VVC.
- S15: RFC 9043 (FFV1) — Verified primary IETF RFC.
- S18/S19: LiteRT 2.3 Maven runtimes & NNAPI deprecation in Android 15 — Accurate primary Android facts.
- S20: VMAF phone model — Verified Netflix libvmaf model.

---

## 7. Strengths
- **Rigorous Epistemic Discipline (§0, Appendix E)**: Every claim is labeled (V-P, V-S, AC, EST, SPEC, UNV). Appendix E self-audits the weakest claims honestly.
- **Realistic Problem Formulation (§2.4, D1)**: Correctly identifies that raw-camera lossless is unworkable on mobile and defines lossless as bit-exact recompression of existing files.
- **Concrete Thermal & Energy Awareness (§3.5)**: Introduces the persistent compression queue with strict thermal/battery gating as a core product feature.
- **Sound NPU & Determinism Handling (§3.3, D6)**: Recognizes cross-platform NPU divergence and uses "transmit-the-scales" enhancement layers rather than relying on bit-exact integer quantization across fragmented SoCs.
- **Comprehensive Go/No-Go Milestone Gates (§4.3)**: Measurable, testable failure criteria for each milestone.

---

## 8. Weaknesses
- **Lossless Recompression Feasibility (§1.2, §3.1)**: Claiming 5–15% lossless reduction by re-encoding already heavily quantized/compressed HEVC bitstreams into FFV1 is technically dubious unless the original HEVC file had massive container/stuffing overhead or was intra-only. Decoding lossy HEVC into raw YUV and re-encoding with FFV1 typically causes a 5× to 10× file size *expansion*, not a 5–15% *reduction*! Recompressing bitstreams losslessly without decoding pixels (like packJPG / Lepton for JPEG) requires specialized bitstream entropy re-coding, which FFV1 does not do. This is a critical technical flaw in P01's lossless thesis.
- **AV1 Software Encode Viability on Flagship (§3.2, §4.3)**: Even on flagship SoCs, SVT-AV1 / libaom software encoding at 1080p30 in <3 minutes is extremely battery-draining and prone to thermal throttling.
- **Neural Enhancement Overhead (§3.3)**: Transmitting enhancement latents adds bitrate overhead; if base stream is already high quality, enhancement layers risk diminishing returns.
- **VVC Encode Dismissal (§2.2)**: Completely defers VVC encode due to absence of mobile hardware encoders, ignoring potential cloud-assisted or selective offline paths.

---

## 9. Scores (1–5) and Reasoning
- **Depth**: 5/5 — Deep technical analysis covering codecs, NPU runtimes, thermal management, metrics, and patent pools.
- **Accuracy**: 4/5 — Very high standards of evidence, but conflates bitstream re-entropy coding with raw frame re-encoding in FFV1.
- **Originality**: 4/5 — Creative recompression planner, transmit-the-scales determinism layer, and queue-as-product concept.
- **Feasibility**: 4/5 — Highly pragmatic Android platform choices (LiteRT, MediaCodec, WorkManager), though FFV1 size reduction claim is problematic.
- **Completeness**: 5/5 — Thoroughly addresses all required lifecycle components including team, cost, legal, metrics, and risk gates.
- **Evidence**: 5/5 — Transparent tagging system, rigorous source citations, and thorough self-audit.
