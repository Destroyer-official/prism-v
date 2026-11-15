# P04 Digest: Project OSPREY — Next-Generation Video Compression System for Android, 2027 and Beyond

**Source File**: `research_implementation_plane4.md`  
**Total Lines**: 921  
**Author/Context**: Version 1.0, dated 14 November 2026; comprehensive ~15,000-word engineering handoff document.

---

## 1. Thesis
Project OSPREY proposes a dual-mode video compression architecture built on the premise that classical codecs must anchor normative correctness while neural networks are restricted to encoder-side intelligence and strictly bounded, hash-verified fixed-point enhancement:
- **Lossless Mode (OSPREY-L)**: A new, custom, memory-safe Rust-based codec core utilizing reversible color transforms (YCoCg-R), adaptive spatial/temporal prediction, intra-block copy (IBC) / palette for screen content, and an interleaved rANS context-mixing entropy coder. Neural networks guide configuration *selection* on the encode side, while decode remains 100% classical and deterministic.
- **Lossy Mode (OSPREY-V)**: Combines a standard base layer (AV1 in v1.0, migrating to AV2 via a BaseCodec HAL in v2.0) with an opt-in deterministic Neural Enhancement Spatio-temporal Track (NEST), AV1 Film Grain Synthesis (FGS), and a 4-axis perceptual rate controller, scheduled overnight via "NightShift".
- **Shared Foundation**: Monorepo architecture, MP4-family container (`ospr`) with backwards-compatible base playback, content-addressed model management via Play Asset Delivery, per-GOP Merkle integrity hashing, and an energy governor tied to Android On-Device Power Monitors (ODPM).

---

## 2. Technical Decisions
- **Architecture**: Modular layered structure (§4.1): Kotlin UI -> NightShift Scheduler / Device Capability Profiler -> JNI with zero-copy `AHardwareBuffer` -> `osprey-core` in Rust (`base-hal`, `lossless`, `enhance`, `container`, `conformance`, `governor`) -> `osprey-nn` (custom fixed-point INT8/INT16 runtime) -> `osprey-bench` (12-device farm with ODPM rails).
- **Algorithms**:
  - Lossless: Reversible YCoCg-R transform; Median Edge Detection (MED) / gradient predictors; hash-table-accelerated IBC and palette coding for screen content; interleaved rANS entropy coder (§3.1, §3.5).
  - Lossy: AV1 software encoding (SVT-AV1 class) or hardware fast-path; NEST latent residual refinement; AV1 FGS; 4-axis Perceptual Rate Control (PRC) (§3.3, §3.5).
- **Machine Learning**: End-to-end neural codecs (e.g. DCVC) are rejected as normative decoders due to cross-NPU floating-point divergence (§2.2). ML is strictly partitioned: (1) Encoder-only decision models (SelCode, router, saliency), and (2) Fixed-point NEST enhancement decoder running on an in-house INT8/INT16 runtime with strict rounding and per-GOP Merkle hash fallbacks (§3.4, §4.3).
- **Languages and Tools**: Rust for core codecs, container, and fixed-point inference; Kotlin + Jetpack Compose for app; C FFI islands only for external libraries (libaom, SVT-AV1, libvmaf); PyTorch for offline training; Cargo workspaces + Gradle (§4.2).
- **Phone Hardware**: Hardware video blocks for capture and base decode; multi-core CPU (NEON baseline, SVE2 runtime dispatch) for software encode/decode; NPU (QNN, NeuroPilot) or GPU (Vulkan Compute INT8) for fixed-point NEST enhancement (§4.5).
- **Container**: ISO Base Media File Format (ISOBMFF / MP4) brand `ospr` containing: standard `av01` base track, auxiliary `osen` enhancement track, `omdl` model manifest box, and `ohsh` Merkle hash box (§4.4).
- **Android Integration**: Native `AHardwareBuffer`, `WorkManager` with charging/idle/thermal constraints, `IPowerStats` HAL (ODPM rail counters), 16 KB page alignment from day one (§2.4, §4.7).

---

## 3. Inventions and Novel Ideas
- **OSPREY Determinism Spec (ODS) (§3.4)**: Solves NPU platform divergence through four pillars: (1) Classical normative base decode, (2) Fixed-point integer-only enhancement runtime, (3) Explicitly-signaled configuration selection (SelCode), and (4) Per-GOP Merkle hashes enabling automatic fallback (enhanced -> base -> error).
- **NEST (Neural Enhancement Spatio-temporal Track) (§3.5, I1)**: Encodes a spatially-reduced AV1 base (1/2, 5/8, 3/4) plus a compact fixed-point latent residual; decouples noise/grain to standard AV1 FGS, focusing the neural net purely on structural reconstruction. Claimed gain: +8–15% BD-VMAF at equal energy.
- **SelCode Neural-Guided Selection (§3.5, I2)**: Encoder-side NPU model analyzes 64x64 chunks to select the optimal predictor/context/refresh configuration from a 1024-entry menu and signals the choice index in the bitstream (10–40 bits/chunk), preserving classical, zero-risk decode. Claimed gain: 10–25% size reduction over FFV1.
- **Perceptual Rate Control (PRC) with 4-Axis Targets (§3.5, I3)**: Dynamic rate allocation supporting Quality (CRF proxy), Size (two-pass solver hitting within +-2%), Energy (budgeted presets), and Time (hardware realtime).
- **NightShift Deferred Optimization (§3.5, I4)**: Charge-time, thermal-budgeted, GOP-resumable re-encoding pipeline running slow multi-pass software encode and neural enhancement overnight.
- **Never-Worse Lossless Recompression (§3.5, I5)**: Guarantees output is never larger than the input file; keeps original stream if recompression does not shrink the file.

---

## 4. Plan Content
- **Phases & Milestones (§4.10)**: 30-month program across 7 gated milestones:
  - M1 (Month 3): Spikes E1/E2 validated; AOMedia membership filed.
  - M2 (Month 6): Base+NEST bit-exact playback demo; Subjective Lab #1 (+0.10 MOS gain).
  - M3 (Month 10): Alpha app on 5 devices (>=12% BD-VMAF vs HW encode).
  - M4 (Month 15): Closed beta (200 users, crash-free >=99.5%, S-mode +-2% accuracy).
  - M5 (Month 18): v1.0 Play launch; public conformance corpus; FTO legal opinion.
  - M6 (Month 24): AV2 integration gate.
  - M7 (Month 30): Ecosystem expansion (WASM player, desktop CLI, OEM SDK pilot).
- **Team (§4.10)**: 12 FTE + 2 Part-Time (2 Codec Research, 3 Rust Systems, 2 ML, 2 Android, 1 NPU/Inference, 1 QA/Bench, 0.5 UX, 0.5 Legal, 1 Tech Lead).
- **Cost (§4.10)**: USD ~$13.5M US-equivalent (~$6.5–$7.5M distributed EU/Asia); pre-scoped fallback to a v1.0-only 8-FTE path at ~$4.5–$5.5M US (~$2.8M distributed).
- **Testing & Benchmarks (§4.6)**: 300-clip PhonePack-2027 corpus + AOM CTC. 12-phone physical device farm (4 SoC tiers) equipped with ODPM rail monitoring and Monsoon meters. Metrics: VMAF v1-2026, XPSNR, PSNR-HVS, CAMBI, CIEDE2000, J/clip, wall-clock, BT.500-15 ACR subjective labs.
- **Legal Strategy (§4.8)**: Joins AOMedia ($500/yr); budgets $150k–$300k for specialist codec counsel and FTO analysis; avoids VVC/HEVC as normative choices; monitors Access Advance AV1/AV2 pool exploration; files 8–12 defensive patents ($80k–$150k).
- **Red Team & Failures (§5)**: Documents 12 concrete failure modes (e.g. R1: determinism tax too high, R2: Access Advance hostile pool, R5: neural enhancement looks synthetic, R6: NightShift killed by OEM battery savers) with explicit tripwires and mitigation responses.

---

## 5. Numbers and Quantitative Claims
- AV2 BD-rate vs AV1: -29.8% PSNR-YUV, -33.8% VMAF (Random Access), -35.8% VMAF (Adaptive Streaming) (Source: AOM CTC / arXiv 2605.15800v1, §2.1, P3).
- AV2 decode complexity: ~5x that of AV1 on CPU (Source: J.-B. Kempf, dav2d lead / VideoLAN, §2.1).
- DCVC-RT: 125.2 fps enc / 112.8 fps dec at 1080p, ~21% bitrate saving vs VTM (Source: arXiv 2502.20762, §2.2, P10).
- MLVC: 100 fps on commodity NPUs, >70% BD-MOS vs HW HEVC on conferencing content (Source: arXiv 2606.28027, §2.2, P8).
- Netflix Film Grain Synthesis: 36% average bitrate reduction at >=1080p (Source: Netflix Tech Blog May 2025, §2.6, P13).
- JPEG AI: 13.1% (Main) / 19.2% (High) BD-rate gain vs VVC-intra (Source: arXiv 2503.16288 / IEEE 2025, §2.1, P12).
- Bit-exact quantization cost: ~0.5% BD at 16-bit, ~7.9% at 8-bit weights + 16-bit activations (Source: arXiv 2312.11209 cited in MLVC, §2.2).
- Snapdragon 8 Elite Gen 5 / Gen 6 NPU: 80 TOPS (Gen 5), +20% perf/watt (Gen 6) (Source: Counterpoint / Qualcomm product briefs, §2.4, P6).
- Sisvel AV1 pool royalties: €0.32/€0.24 display consumer device, €0.11/€0.08 non-display (Source: Sisvel rate card, §2.7, P17).
- AOMedia membership: $500/year (Source: aomedia.org/join, §2.7, P19).
- OSPREY-V target: 20–35% bitrate reduction vs phone default HW encode; 5–15% vs SVT-AV1 preset 6 (Source: Author estimate, §1).
- OSPREY-L target: 10–20% smaller than FFV1; 5–15% smaller than AV1 lossless on screen content at 3–10x faster speed (Source: Author estimate, §1).
- Memory limits: Capture/fast <=350MB, NightShift <=900MB, NEST decode <=220MB, OSPREY-L <=600MB (Source: Author design, §4.1).

---

## 6. Citations and Verification Assessment
- P1–P3 (AOMedia AV2 spec & CTC paper arXiv 2605.15800v1) — Verified primary standards documents.
- P4 (MPEG 156th meeting beyond-VVC CfP) — Verified primary standards body solicitation.
- P5–P6 (MediaTek Dimensity & Qualcomm Snapdragon product briefs) — Primary SoC documentation.
- P7–P10 (DCVC repo, MLVC arXiv 2606.28027, NeuralLVC arXiv 2604.03353, DCVC-RT arXiv 2502.20762) — Verified academic papers with code.
- P11–P12 (JPEG AI IS July 2025 & arXiv 2503.16288) — Verified primary standardized neural image codec.
- P13–P15 (Netflix FGS tech blog, Meta AV1 white paper, Netflix vmaf v1 model set June 2026) — Verified primary engineering sources.
- P17–P19 (Sisvel AV1 rate card, Access Advance exploration, AOMedia join page) — Primary legal/licensing sources.
- P20–P22 (LiteRT repo, Qualcomm AI Engine SDK, Android Perfetto ODPM docs) — Verified primary platform documentation.
- P23 (ITU-R BT.500-15) — Standardized subjective methodology.

---

## 7. Strengths
- **Exemplary Epistemic & Methodological Rigor (§0, §8)**: Comprehensive tagging ([V1], [V2], [PK], [AC], [E], [SP]), thorough source tracking, and a self-audit explicitly acknowledging its 8 weakest claims and 10 spend-ordered proving experiments.
- **Robust Cross-Device Determinism Solution (§3.4)**: The ODS protocol (classical normative base, custom fixed-point INT8/INT16 runtime, SelCode signaled indices, per-GOP Merkle hash fallbacks) directly solves the NPU non-determinism crisis.
- **Architectural Modularity & Future-Proofing (§3.3 D2, §4.1)**: BaseCodec HAL cleanly isolates the base codec, allowing seamless migration from AV1 to AV2 once mobile hardware decoders emerge.
- **Energy-First Systems Engineering (§2.4, §4.5, §4.7)**: Real-world battery/thermal integration utilizing Android ODPM rail monitors, PID energy governors, and resumable NightShift WorkManager scheduling.
- **Comprehensive Red Team Analysis (§5)**: 12 detailed failure modes (patent pools, thermal throttling, NPU quantization taxes, brand risks) paired with concrete tripwires and pre-designed pivot responses.

---

## 8. Weaknesses
- **High Financial and Operational Scope (§4.10)**: 12 FTE over 30 months costing $13.5M (US) is by far the most expensive and resource-intensive plan among all reviewed proposals.
- **Complexity of Writing a Custom Codec from Scratch in Rust (§3.3 D3, §4.1)**: Building a new lossless video codec core (reversible transforms, predictors, rANS context mixing) and a custom fixed-point neural runtime represents an immense engineering surface compared to using stabilized libraries like FFmpeg.
- **Uncertainty of SelCode Compression Gains (§3.5 I2, §8.1)**: The claimed 10–25% gain over FFV1 relies on the unproven assumption that neural chunk-level configuration selection can match the efficiency of full neural context mixing.
- **OEM Task-Killer Vulnerability (§4.7, §5 R6)**: Sustained background processing on Android (even with WorkManager foreground mediaProcessing) faces aggressive OEM background process termination on aggressive third-party skins (Xiaomi, Transsion, Samsung).

---

## 9. Scores (1–5) and Reasoning
- **Depth**: 5/5 — Exhaustive technical analysis across standards, hardware architectures, numerical determinism, and Android internals.
- **Accuracy**: 5/5 — Meticulously verified facts, highly accurate representation of 2026 hardware and codec ecosystems.
- **Originality**: 5/5 — Highly inventive ODS determinism protocol, SelCode neural selection, and NEST decoupled grain architecture.
- **Feasibility**: 4/5 — Technically sound, but carries significant execution risk due to large scope and custom Rust engine development.
- **Completeness**: 5/5 — Thoroughly addresses all required lifecycle phases, legal analysis, budgets, red teams, and proving experiments.
- **Evidence**: 5/5 — Pristine evidence standards with primary URL citations, claim tags, and transparent self-audits.
