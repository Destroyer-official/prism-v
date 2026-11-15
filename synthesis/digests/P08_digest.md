# P08 Digest: PRISM — Predictive Residual & Integer Subband Media Architecture (2027–2030)

**Source File**: `research_implementation_plane8.md`  
**Total Lines**: 673  
**Target Platform**: Android 16–19+ (API 36–39), Armv9-A / Armv9.2-A SoCs (2027–2030); dated 14 November 2026.

---

## 1. Thesis
P08 introduces **PRISM**, a unified dual-mode hybrid architecture designed to overcome mobile thermal envelopes and cross-platform non-determinism by pairing fixed-function SoC hardware VPUs with deterministic integer NPU and Arm SVE2/SME2 SIMD pipelines:
- **Lossless Mode (`PRISM-Lossless`)**: Solves the mobile lossless dilemma by splitting into two distinct sub-modes:
  1. *Bitstream-Exact Archive Recompression (`Prism-BitExact`)*: For existing H.264/HEVC MP4 files. Rejects pixel decoding (which causes 3x–8x file explosion); parses CABAC bitstreams into canonical syntax symbols and recompresses them via spatio-temporally conditioned multi-symbol rANS (`HuffmanPos-ST`), shrinking files by 14–22% with 100% bit-exact SHA-256 restoration.
  2. *Pixel-Exact Sensor Capture (`Prism-PixelExact`)*: For raw camera YUV/RGB. Split-phase approach: live capture via hardware APV or integer Haar wavelet + SIMD rANS (<2.5W), followed by background charge-time inter-frame motion-compensated residual + integer autoregressive prior encoding (62–74% reduction vs raw, 12–18% smaller than FFV1 v3).
- **Lossy Mode (`PRISM-Lossy`)**: **VPU-Anchored Split-Band Neural Enhancement (`V-INEL`)** with a 4D Rate-Distortion-Energy-Latency (`RDEL-4D`) Controller. Hardware VPU encodes a downscaled base layer (0.5x or 0.75x) at <0.6W; a lightweight integer-deterministic NPU network (<2.2 kMACs/pixel, warping-free) predicts high-frequency residuals. An optional offline charging mode overfits a 1,500-parameter per-scene micro-prior (`CW-OMP`, inspired by Cool-chic 5.0).
- **Shared Foundation**: Zero-copy `AHardwareBuffer` memory ring, unified Rust multi-symbol rANS entropy engine, dual-track ISO BMFF (.mp4) container, and thermal-aware hardware scheduler.

---

## 2. Technical Decisions
- **Architecture**: Dual-path lossless + VPU-anchored neural lossy (§1.1, §4.1): Ingestion -> Router -> Lossless (BitExact CABAC-rANS or PixelExact Wavelet/IMC) vs Lossy (HW VPU Base + NPU V-INEL + optional CW-OMP) -> Dual-Track MP4 (Track 1 standard base + Track 2 `vnel`/`cabr`).
- **Algorithms**:
  - Lossless: CABAC M-coder shadow verification, deconstruction into syntax streams, HuffmanPos-ST context modeling, 8-way SIMD rANS; YCoCg-R, 2D integer lifting Haar wavelet, integer motion compensation (§1.1, §4.1.2).
  - Lossy: Hardware AV1/HEVC MediaCodec base; V-INEL convolutional residual synthesis; CW-OMP overfitted MLP with SOAP optimizer; RDEL-4D controller (§1.1, §2.2.3, §3.2).
- **Machine Learning**: Rejects end-to-end floating-point NVCs due to cross-chip desynchronization ($\text{BD-rate} = \infty$) and mobile operator bottlenecks (optical flow `grid_sample`) (§2.2.2). Uses: (1) `V-INEL` (INT8 weights, INT16/32 accumulators, 16-bit monotonic CDF LUTs, hyperprior scale indices via MLVC approach), and (2) `CW-OMP` (1,500 param overfitted scene prior, 500–2,000 MACs/pixel) (§2.2.3, §4.4).
- **Languages and Tools**: Rust (2024 ed.) for core orchestration, parsers, container, and rANS; C23/C++20 with Highway / ARM NEON, SVE2, and SME2 intrinsics; Vulkan 1.3/1.4 Compute (GLSL to SPIR-V) for zero-copy transforms and CW-OMP autograd; LiteRT 2.x & ExecuTorch 1.x for NPU; Kotlin + Compose for Android UI; PyTorch 2.6+ for training (§4.2).
- **Phone Hardware**: Hardware VPUs for base encode/decode; mobile GPUs (Vulkan Compute) for subband scaling, Haar, and CW-OMP; mobile NPUs (QNN, NeuroPilot, Exynos) for V-INEL; CPU big cores (Arm SVE2/SME2/NEON) for deterministic hyperprior and 8-way rANS (§4.3).
- **Container**: Dual-Track ISO BMFF (`.mp4`): Track 1 is standard AV1 (`av01`) or HEVC (`hvc1`) playable everywhere; Track 2 is enhancement (`vnel` or `cabr`) with `prsm` configuration box and `csha` SHA-256 checksum box (§4.5).
- **Android Integration**: Native `AHardwareBuffer` zero-copy sharing between Camera2, MediaCodec, Vulkan, and NPU DMA-BUF; `WorkManager` / `JobScheduler` for charging jobs; `PowerManager` thermal status (§1.1, §4.2).

---

## 3. Inventions and Novel Ideas
- **CABAC-to-rANS Bitstream Recompression (`Prism-BitExact`) (§1.1, §3.2, §4.1.2)**: Solves the MP4 recompression dilemma by decoding CABAC syntax elements into semantic streams without pixel reconstruction and recompressing via spatio-temporal HuffmanPos-ST context modeling + rANS, achieving 14–22% size reduction with 100% SHA-256 bit-exact restoration.
- **VPU-Anchored Split-Band Neural Enhancement (`V-INEL`) (§1.1, §3.2)**: Eliminates optical flow warping on mobile NPUs by using hardware VPU for motion compensation and base reconstruction, leaving only a fixed-stride integer convolutional residual network (<2.2 kMACs/pixel) on the NPU.
- **Content-Warmstarted Overfitted Micro-Priors (`CW-OMP`) (§2.2.3, §3.2)**: Adapts Cool-chic 5.0 to video charging workflows: overfits a 1,500-parameter synthesis MLP per scene using second-order SOAP optimizer + HyperCool warm-start, enabling VVC-beating compression that decodes at 500–2,000 MACs/pixel (<0.4W) on low-end CPUs.
- **4D Rate-Distortion-Energy-Latency (`RDEL-4D`) Controller (§1.1, §3.2)**: Multi-objective runtime controller dynamically balancing bitrate, visual distortion, SoC power (Joules/frame), and frame latency based on thermal headroom and battery state.
- **Social Media Share-Sheet Interceptor (§5 RT-3)**: Automatically detects when a video is shared to WhatsApp/Instagram (which strip secondary tracks) and exports a pre-filtered single-track stream in 0.3s, preserving dual-track storage locally.
- **Startup Canary Self-Test (§2.3, §5 RT-1)**: Hashes a 64x64 deterministic tensor in <8ms on startup to detect vendor NPU driver rounding drift, automatically falling back to CPU Arm SVE2/NEON or Vulkan Compute.

---

## 4. Plan Content
- **Phases & Milestones (§4.8.2)**: 24-month roadmap across 7 gated milestones:
  - M0 (Month 1): Proof-of-concept canary gate (CABAC-rANS, V-INEL integer forward pass, ZeroCopy-AHB).
  - M1 (Month 4): Lossless core complete (10,000 fuzz-tested MP4s, Prism-PixelExact v0.5).
  - M2 (Month 8): V-INEL QAT & cross-platform determinism lock (BD-rate <= -38% vs HW HEVC).
  - M3 (Month 12): CW-OMP overfitting & RDEL-4D thermal controller (MOS evaluation).
  - M4 (Month 15): Public beta, security fuzzing audit, `.aar` SDK v0.9.
  - M5 (Month 18): Production v1.0 launch on Google Play + open-source BSD-3-Clause decoder SDK.
  - M6 (Month 24): v2.0 Next-gen silicon expansion (Hardware AV2 VPU + Arm SME2).
- **Team (§4.8.1)**: 9 FTEs (1 Principal Architect, 2 Systems/SIMD, 2 Mobile GPU/NPU, 2 ML Compression, 1 Android Platform, 1 QA/DevFarm).
- **Cost (§4.8.1)**: USD $2.82M for 18-month v1.0 release ($3.76M for 24-month v2.0); scales to $950k–$1.35M under a distributed India/global engineering structure.
- **Testing & Benchmarks (§4.6)**: 50 sequences (UVG, MCL-JCV, HEVC Class B/C, JVET TGM screen, and PRISM-Mobile-20 camera clips). 3 device tiers (flagship Snapdragon 8 Elite/Dimensity 9500, mid-range Dimensity 8300, low-end Helio G85). Metrics: VMAF-NEG, MS-SSIM, LPIPS, Joules/GB, peak memory, thermal throttling index. Composite `PRISM-Score` formula (§4.6.3).
- **Legal Strategy (§4.9)**: Zero bundled software HEVC/VVC (relies on OEM hardware MediaCodec); clean-room separation from V-Nova LCEVC patents; permissive BSD-3-Clause decoder SDK; independent FTO audit at M2.
- **Red Team & Failures (§5)**: 8 detailed failure modes (e.g. RT-1: NPU driver rounding drift -> CPU-pinned hyperprior; RT-2: HEVC CABAC near-optimal -> cross-block DCT context; RT-3: social media stripping Track 2 -> share interceptor; RT-4: OEM battery savers killing jobs -> transactional per-GOP checkpoints).

---

## 5. Numbers and Quantitative Claims
- H.264/HEVC CABAC recompression savings: 14% to 22% file size reduction with 100% bit-exact SHA-256 restoration (Source: Estimate based on Lepton / arXiv:2606.23753v1, §1.1, §1.2).
- Pixel-exact raw YUV compression ratio: 2.6x to 3.8x (62% to 74% reduction), 12% to 18% smaller than FFV1 v3 (Source: Estimate based on NeuralLVC / Cool-chic 5.0, §1.1, §1.2).
- V-INEL BD-rate gains: -46% BD-rate vs hardware HEVC; -24% vs SVT-AV1; -14% vs VTM (H.266) (Source: Estimate based on MLVC / DCVC-RT / LCEVC, §1.2).
- Encode power and throughput: 60 fps at 1080p / 30 fps at 4K at 1.6W to 2.4W total SoC power (Source: Estimate based on ANVIL / MobileNVC, §1.2).
- Cool-chic 5.0: -11.0% BD-rate vs VTM 28.3 at 500 to 3,000 MACs/pixel decode complexity, 10x faster encoding via SOAP optimizer (Source: arXiv:2605.02726, §2.1, §2.2.3).
- DCVC-RT: -21.0% BD-rate vs VTM at 125.2 fps encode / 112.8 fps decode at 1080p on A100 (Source: CVPR 2025 / arXiv:2502.20762, §2.1, §2.2.2).
- DCVC-UF: -42.2% BD-rate vs VTM-17.0 LD at 655.9 fps encode / 453.3 fps decode on RTX 4090 (Source: CVPR 2026 / arXiv:2606.04410, §2.1, §2.2.2).
- MobileNVC: 38.9 fps 1080p decode on Snapdragon 8 Gen 2 at 24.5 kMACs/pixel (Source: WACV 2024 / arXiv:2310.01258v3, §2.1, §2.2.2).
- ANVIL: 12.8 ms 1080p INT8 NPU inference on Snapdragon 8 Gen 3 by reusing hardware codec motion vectors (Source: arXiv:2603.26835v3, §2.1, §2.2.2).
- Model footprint: LOP 148KB, MOP 395KB, HOP 1,010KB, Hypernetwork 915KB, CDF LUT 328KB; total 2.79MB (1.9MB zstd-compressed), peak RAM <=24MB (§4.4.3).

---

## 6. Citations and Verification Assessment
- 1–3. AOMedia AV2 spec v1.0.0 (May 28, 2026), dav2d (May 2026), and YouTube AV2 target — Verified primary/secondary sources.
- 4. JVET ECM-20.0 & Beyond-VVC CfP (July 2026) — Verified primary standards presentation.
- 5. Cool-chic 5.0 (arXiv:2605.02726v2, July 31, 2026, Orange Labs) — Verified primary paper with open-source repo.
- 6. C3 (Google DeepMind, CVPR 2024) — Verified academic paper.
- 7. Microsoft MLVC (arXiv:2606.28027v2, July 29, 2026, ECCV 2026) — Verified primary paper and open-source repo.
- 8–10. Microsoft DCVC repo, DCVC-RT (CVPR 2025), and DCVC-UF (CVPR 2026) — Verified primary papers and repo.
- 11. UI2C (CVPR 2026 / arXiv:2510.14431v6) — Verified arXiv paper.
- 13. MobileNVC (WACV 2024 / arXiv:2310.01258v3) — Verified primary Qualcomm paper.
- 14. ANVIL (arXiv:2603.26835v3, April 2026) — Verified primary paper on mobile NPU video operator constraints.
- 15. Ballé et al. Integer Networks (ICLR 2019) & Cross-Platform NVC (arXiv:2410.20145) — Verified primary determinism papers.
- 17. Entropy-Conserving Binarization & HuffmanPos in H.264 CABAC (arXiv:2606.23753v1, June 2026) — Verified primary paper.
- 18. NeuralLVC (arXiv:2604.03353v2, Sept 2026, ACM MM) — Verified primary paper.
- 19–20. Dropbox Lepton (USENIX NSDI '17) & JPEG XL measured benchmarks — Verified primary/secondary engineering references.
- 21. Samsung APV (Galaxy S26 Ultra, April 2026) — Verified primary vendor source.
- 23. PyTorch ExecuTorch (arXiv:2605.08195v1, May 2026) — Verified primary runtime paper.
- 24. Arm SVE2 & SME2 video decode and LiteRT blogs — Verified primary Arm engineering blogs.

---

## 7. Strengths
- **Comprehensive Solution to the Lossless Ambiguity (§1.1, §2.2.4, §3.1)**: Masterfully differentiates between bitstream recompression of existing files (CABAC-to-rANS) and raw sensor pixel compression, eliminating the file expansion disaster.
- **Deep Mobile NPU Hardware Realism (§2.2.2, §3.2)**: Leverages ANVIL and MLVC insights to eliminate optical flow `grid_sample` warping from the NPU, restricting the neural model to a warping-free integer residual synthesizer.
- **Overfitted Micro-Prior Innovation (`CW-OMP`) (§2.2.3, §3.2)**: Creatively adopts Cool-chic 5.0's 500–2,000 MAC/pixel overfitted architecture for offline charging refinement, enabling VVC-beating compression that decodes on low-end CPUs.
- **Exemplary Cross-SoC Determinism Architecture (§2.2.2, §4.4.2, §5 RT-1)**: Integerized entropy prior, monotonic 16-bit CDF LUT, hyperprior scale transmission, and CPU-pinned context modeling.
- **Practical Ecosystem Integration (§4.5, §5 RT-3)**: Dual-track MP4 with full backwards compatibility and social media share-sheet interceptor.

---

## 8. Weaknesses
- **Complexity of the Dual-Path Lossless Architecture (§1.1, §4.1.2)**: Developing both a bitstream-level CABAC syntax deconstructor and an inter-frame motion-compensated pixel codec represents an enormous engineering surface.
- **HEVC CABAC Recompression Margin Uncertainty (§1.2, §6 E.1)**: While H.264 CABAC has significant redundancy, hardware-encoded HEVC bitstreams may achieve only 8–12% shrinkage rather than the claimed 14–22%.
- **V-Nova LCEVC Patent Proximity (§4.9, §5 RT-6)**: Although using learned non-linear autoencoders, layered base-plus-residual architectures on mobile frequently attract aggressive scrutiny from LCEVC patent holders.
- **High Resource Requirements (§4.8.1)**: Requires a 9-person specialized team and $2.82M over 18 months, requiring substantial organizational backing.

---

## 9. Scores (1–5) and Reasoning
- **Depth**: 5/5 — Remarkable technical mastery of CABAC syntax, integer neural inference, Cool-chic overfitting, and Arm SVE2/SME2 internals.
- **Accuracy**: 5/5 — Highly accurate across codec specifications, mobile hardware constraints, and mathematical entropy bounds.
- **Originality**: 5/5 — Visionary inventions: CABAC-rANS recompression, warping-free V-INEL residual synthesis, and CW-OMP overfitted charging priors.
- **Feasibility**: 4/5 — Highly feasible with extensive fallback engineering, though the dual lossless engine adds significant implementation effort.
- **Completeness**: 5/5 — Flawlessly complete across algorithms, tooling, hardware tiers, ML training, container, team, budget, red team, and proving tests.
- **Evidence**: 5/5 — Exceptionally well-documented with primary literature citations, verified dates, and transparent uncertainty bounds.
