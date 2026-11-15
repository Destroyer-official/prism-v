# P16 Plan Digest: Prism-V Next-Generation Android Video Compression System (Lossless + Lossy, 2027–2030)

**Source File:** `research_implementation_plane16.md`  
**Total Lines:** 835  
**Reading Range:** Lines 1–835 (Chunks: 1–400 [Bytes 0–46080, 46080–88638], 401–835 [Bytes 0–46080, 46080–64635])  
**First Heading:** `# Master Implementation Plan: Prism-V Next-Generation Android Video Compression System (Lossless + Lossy, 2027–2030)`  
**Last Heading:** `### 7.3 The 10 Best Next Experiments (Ranked by Information Value per Engineering Day)`  
**Final Sentence:** `| **10** | **Masked Soft-Gating ($\alpha$-Blend) vs. Additive Residual at Ultra-Low Bitrate (Exp-Lossy-03)** | 3 Days | Test $\hat{f}_{\text{out}} = \alpha \odot \hat{f}_{\text{base}} + (1-\alpha) \odot \hat{r}$ vs. $\hat{f}_{\text{base}} + \hat{r}$ at 250–500 kbps 1080p where the hardware base stream exhibits severe blocking. | Validates Red-Team Pivot #3 for ultra-low-bitrate streaming/sharing. |`

---

## 1. Thesis
Prism-V tackles the two foundational mobile video traps:
1. **The "Lossless Paradox":** Decoding compressed H.264/HEVC MP4s to pixels and re-encoding with lossless pixel codecs (`FFV1`, `x264 --qp 0`) expands files by 400%–900%. Prism-V bifurcates Lossless into:
   - **Mode L-BBR (Bit-Exact Bitstream Recompression):** Parses H.264/HEVC CABAC syntax without dequantization/reconstruction, re-encoding syntax tokens using Position-Indexed Contexts (`HuffmanPos` / `ECB`) + a 1.1 KB `INT8` Micro-MLP and 8-way SIMD rANS, coupled to an on-device virtual proxy (`StorageManager.openProxyFileDescriptor`) for transparent 100% SHA-256 bit-exact reproduction at >180 MB/s, saving 14%–24% disk space.
   - **Mode L-PSC (Pixel-Exact Sensor Compression):** For raw Camera2 capture (`YUV_420_888`, `P010`), uses Reversible Color Transform (`YCoCg-R`) + LeGall 5/3 integer lifting wavelet + 4-pass Checkerboard Causal `INT8` ConvNet (0.34 GMACs/frame) + rANS, compressing raw YUV to 27.5%–31% (15%–21% smaller than `FFV1`) at 30–60 fps on mobile.
2. **The "Pure Neural vs. Fixed Silicon" Trap in Lossy Mode:** Rejects end-to-end neural autoencoders (which burn 2.5–4.8 J/frame and suffer cross-silicon rounding drift). Instead, uses a **Hardware-Anchored Hybrid Split-Stream (`Mode Y-Hybrid`)**: standard hardware `MediaCodec` encodes an AV1/HEVC Base Layer (85%–92% energy at 0.01 J/frame), while a non-normative/overfitted micro-residual (<600 MACs/pixel, based on Cool-chic 5.0 and N-O Cool-chic) plus AOMedia `AFGS1` grain synthesis restores fine details. It operates at two speeds: instant feed-forward hypernetwork at live capture (<3.5 ms/frame), followed by optional background charging-time overfitting (+8%–14% BD-rate).

---

## 2. Technical Decisions
- **Architecture:** Dual-track ISOBMFF (`.mp4`) for lossy (Track 1: standard HW AV1/HEVC playable everywhere; Track 2: `prmr` enhancement track + `afgs` grain box). For lossless, `.pvloss` container with `openProxyFileDescriptor` FUSE bridge for zero-copy transparent virtual `.mp4` playback in WhatsApp, Instagram, and ExoPlayer.
- **Cross-Silicon Neural Determinism (`Prism-DetInt`):** Strict division between **Class A (Entropy-Normative Priors)** and **Class B (Post-Entropy Reconstruction & RDO)**. Class A runs exclusively on CPU (Arm NEON `vdotq_s32` / SME2 `smopa`) using bounded two's-complement integer math ($\mathbf{W}\in\text{INT8}, \mathbf{x}\in\text{INT16}, \text{acc}\in\text{INT32}, K\le 512$) with dyadic rescaling and a 512-byte LUT, mathematically guaranteeing 0 bit-flips across Qualcomm, MediaTek, Exynos, Tensor, and x86. Class B runs on Vulkan GPU (`VK_KHR_cooperative_matrix`) or NPU (`LiteRT`).
- **Memory & Pipeline:** Memory-bounded tile ring-buffer pipeline (<64 MB peak RSS on 4K low-end devices) using $256\times 256$ luma tiles with zero-copy `AHardwareBuffer` / `AImageReader`.
- **Languages & Tooling:** Rust (Edition 2024 via `cargo-ndk`) for safe orchestration, bitstream parsers, container muxers, and JNI. C++20 and hand-tuned Arm assembly (`KleidiAI` / NEON / SME2) for inner loops and SIMD rANS. Kotlin + Jetpack Compose for UI. PyTorch 2.6 + CompressAI for training/QAT.
- **Hardware Tiering:**
  - *Tier 1 ($100–$180, A55/A78):* Hardware HEVC base, CPU NEON rANS, max 64 MB RSS.
  - *Tier 2 ($220–$450, A725/X4):* Hardware AV1 base, Vulkan 1.3/1.4 compute, 160 MB RSS.
  - *Tier 3 ($600–$1,400, Oryon/X925):* Hardware AV1/APV, Armv9.2 SME2, LiteRT QNN/NPU 990, 384 MB RSS.

---

## 3. Inventions & Differentiating Concepts
- **P16-I01 (`Prism-BBR` Bitstream Recompressor):** Recompresses existing H.264/HEVC MP4s by parsing CABAC syntax without dequantizing, re-encoding via `HuffmanPos`/`ECB` and a 1.1 KB `INT8` Micro-MLP into 8-way rANS, with a shadow-CABAC discrepancy patch stream ($\Delta_{\text{patch}}$) for 100% bit-exact SHA-256 recovery.
- **P16-I02 (`Prism-DetInt` Integer Determinism Engine):** Rigorously proves overflow-free integer matrix accumulation ($K \le 512$, $512 \times 127 \times 32767 < 2^{31}-1$) across SIMD widths (128-bit NEON vs 512-bit SME2) with dyadic shifts and static 512-byte LUT for entropy priors.
- **P16-I03 (`Prism-Lossy` Two-Speed Hybrid Split-Stream):** Combines hardware `MediaCodec` base layer with a 600-MAC/pixel Cool-chic residual and `AFGS1` grain synthesis. Features instant feed-forward hypernetwork (<3.5 ms/frame) during live capture and optional background charging-time overfitting (+8%–14% BD-rate).
- **P16-I04 (`Prism-PSC` Wavelet-Checkerboard Lossless Codec):** Replaces NeuralLVC's 94-pass Transformer (5.9 PMACs/frame) with 1-level LeGall 5/3 lifting wavelet + 4-pass Checkerboard `INT8` ConvNet (0.34 GMACs/frame), cutting compute 25,000x while beating `FFV1` by 15%–21% relative.
- **P16-I05 (`Prism-VFS` Zero-Copy Virtual MP4 Bridge):** Implements `StorageManager.openProxyFileDescriptor` to expose `.pvloss` archives as virtual `.mp4` streams decompressed on-the-fly in RAM at >180 MB/s, allowing instant playback in WhatsApp, Instagram, and ExoPlayer without disk bloat.
- **P16-I06 (Tri-Metric Perceptual Consensus & Dial):** Composite scoring using VMAF-NEG, ColorVideoVDP (CVVDP v1.2), and CAMBI banding index, driven by a 4-target user hexagon dial with a 0.3-second pre-flight micro-probe.

---

## 4. Plan Content
- **Phases & Timeline:** 18-month plan across 5 phases:
  - Phase 0 (Months 1–2): Proof-of-concept CLI spikes for BBR, DetInt, PSC, Lossy, and Go/No-Go Gate 0.
  - Phase 1 (Months 3–6): Production Rust + SIMD crates, bitstream freeze, 100M-iteration fuzzing, Gate 1.
  - Phase 2 (Months 7–11): Hybrid lossy engine, NPU/Vulkan acceleration, Gate 2.
  - Phase 3 (Months 12–15): Android app, `Prism-VFS` proxy, Media3 SDK, Gate 3.
  - Phase 4 (Months 16–18): ITU-R BT.500 subjective testing (60 raters), security audit, Play Store launch, Gate 4.
- **First 30 Days & 7-Day Sprint:** Explicit Day 1–7 (BBR), Days 8–15 (DetInt), Days 16–23 (Lossy spike), Days 24–30 (ProxyFD) sprint with exact CLI deliverables and pass/fail criteria.
- **Team & Budget:** 8 FTEs (1 Principal Codec Architect, 2 ML Compression Scientists, 2 SIMD/GPU Engineers, 2 Android Systems Engineers, 1 QA/HIL Benchmark Engineer) + external patent counsel. Total budget: $2.43M–$3.18M USD ($950k–$1.35M if staffed in hybrid low-cost geos).
- **Testing & Benchmarks:** 6 physical phones in an automated thermal chamber with Monsoon High Voltage Power Monitors. Test datasets: UVG, JVET CTC, MCL-JCV, BVI-AOM, and 50-clip `Prism-Mobile-Bench-50`.
- **Legal & IP:** Zero GPL code (no x264/x265, no GPL FFmpeg). Permissive Apache-2.0 / BSD-3-Clause stack. Hardware `MediaCodec` pass-through shifts HEVC/AV1 patent royalties to OEM licenses. Expired wavelet/YCoCg-R patents, AOMedia royalty-free AFGS1, clean-room rANS. Explicit design-around for V-Nova LCEVC.

---

## 5. Numbers & Quantitative Claims
- `Prism-BBR` lossless file reduction: 14.5%–23.5% on H.264, 9.0%–15.5% on HEVC `[Estimate]`.
- `Prism-BBR` proxy decode throughput: 160–340 MB/s at <0.8 W CPU power `[Estimate]`.
- `Prism-PSC` lossless raw YUV size: 27.5%–31.0% of raw size (15%–21% smaller than `FFV1` and `x265 --lossless`) `[Estimate]`.
- `Prism-PSC` complexity: 0.342 GMACs/frame at 1080p, running at 30–60 fps on mobile CPU (vs 5.9 PMACs and 205–400 s/frame for NeuralLVC) `[Estimate]`.
- `Prism-Lossy` BD-rate reduction: -32% to -44% vs HW HEVC, -18% to -27% vs HW AV1 `[Estimate]`.
- `Prism-Lossy` decode overhead: 500–1,500 MACs/pixel (+120 to +280 mW, 60–120 fps) `[Verified - Primary]`.
- Bundled APK ML weights: 291 KB total (BBR 42 KB, PSC 185 KB, Lossy prior 64 KB); total universal APK ~11.3 MB.
- Mobile encode energy: 0.022–0.045 J/frame (live capture); decode energy: 0.009–0.018 J/frame.

---

## 6. Citations
- **Key 2025–2026 Primary Papers:**
  - NeuralLVC (ACM MM '26, Sept 2026, arXiv:2604.03353v2)
  - DCVC-UF (CVPR 2026, June 2026, arXiv:2606.04410v1)
  - DCVC-RT (CVPR 2025, March 2025, arXiv:2502.20762v2)
  - Cool-chic 5.0 (Orange Labs, July 2026, arXiv:2605.02726v2)
  - N-O Cool-chic (Orange Labs, July 2026, arXiv:2607.13723v1)
  - Singh (June 2026, arXiv:2606.23753v1, HuffmanPos/ECB in H.264 CABAC)
  - AV2 CTC & Evaluation (AOMedia, May 2026, arXiv:2605.15800v1)
  - Ballé et al. (ICLR 2019, Integer Networks for Data Compression)
  - DeepMind C3 (Dec 2023, arXiv:2312.02753)
  - UniGVC (Oct 2026, arXiv:2610.13023v1)
  - RAS rANS accelerator (Oct 2025, arXiv:2511.04684v1)
- **Standards & Platform:** AOMedia AV2 Spec 1.0.0 (May/June 2026), AFGS1 Spec (Jan 2024), Samsung APV (IETF RFC 9924, April 2026), FFV1 (RFC 9043), Android 16/17 CDD & MPC, Arm SME2 KleidiAI, Vulkan 1.4 `VK_KHR_cooperative_matrix`.

---

## 7. Strengths
- **Brilliant Resolution of the Lossless Paradox:** The first plan to fully realize that users want to recompress existing lossy MP4s, inventing `Prism-BBR` (bitstream recompression + proxy FD) to achieve 14%–24% lossless gains without 5x–9x pixel-domain inflation.
- **Spectacular Algorithmic Pruning:** Distills the state-of-the-art NeuralLVC from 5.9 PMACs/frame (205 s/frame) down to 0.34 GMACs/frame (30–60 fps) via LeGall 5/3 wavelets and 4-pass checkerboard ConvNets while retaining temporal difference tokenization.
- **Mathematical Rigor on Cross-Chip Determinism:** Provides explicit bounded two's-complement integer equations ($K\le 512$) guaranteeing that accumulator overflow cannot occur and integer associativity holds across 128-bit NEON, 512-bit SME2, and x86 AVX2.
- **Pragmatic Hybrid Lossy Engine:** Blends hardware ASIC low power with an overfitted micro-residual (<600 MAC/pixel) and royalty-free AFGS1 grain, with a dual-speed lifecycle (instant NPU feed-forward at capture, iterative refinement while charging).
- **Exemplary Software Engineering & Risk Mitigation:** Safe Rust core for media parsers (mitigating CVEs), 10 detailed red-team failure modes with concrete engineering pivots, and a 7-day proof-of-concept sprint.

---

## 8. Weaknesses
- **Uncertainty of HEVC CABAC Recompression Gains:** Flagged by the author in self-audit: modern hardware HEVC encoders already optimize CABAC contexts well; actual savings on tight HEVC streams might be 6%–9% rather than 9%–15.5%.
- **N-O Cool-chic Feed-Forward Generalization:** The live feed-forward hypernetwork has only been demonstrated on still images in published literature; extending it to base-conditioned video residuals at 60 fps without temporal flicker is an empirical hypothesis needing proof.
- **Android FUSE Proxy Latency Under Rapid Timeline Scrubbing:** Video editors making dozens of random seeks per second through `openProxyFileDescriptor` may experience cache misses, requiring careful 256 KB chunking and read-ahead tuning.

---

## 9. Scores
- **Depth: 5/5** — Exceptional mathematical formulations, instruction-level SIMD details, bitstream layouts, and memory topologies.
- **Accuracy: 5/5** — Deeply grounded in verified 2025–2026 literature, Android 16/17 CDDs, and SoC hardware physics.
- **Originality: 5/5** — Outstanding synthesis of `Prism-BBR`, `Prism-DetInt`, `Prism-PSC`, and `Prism-VFS`.
- **Feasibility: 4.8/5** — All neural modules are strictly bounded to <1,000 MACs/pixel and run deterministically on CPU SIMD or GPU shaders.
- **Completeness: 5/5** — End-to-end plan covering architecture, team, budget, 30-day sprint, legal strategy, red-team analysis, and open questions.
- **Evidence: 5/5** — Flawless citation apparatus with primary URLs, arXiv identifiers, and clear epistemic classification tags.
- **Overall Score: 4.96 / 5.0** (Premier blueprint for technical architecture and algorithmic synthesis).
