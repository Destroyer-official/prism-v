# Master Implementation Plan: Prism-V Next-Generation Android Video Compression System (Lossless + Lossy, 2027–2030)

**Document Version:** 1.0.0-FINAL  
**Date:** November 14, 2026  
**Target Platform:** Android 14–18+ (API Levels 34–38+), Armv8.2-A through Armv9.2-A (NEON, SVE2, SME2), Vulkan 1.3/1.4, MediaCodec Hardware ASICs, and Mobile NPUs (LiteRT / QNN / NeuroPilot)

## 1. Executive Summary

### 1.1 Reframing the Problem and Recommended Architecture ("Prism-V")

Building the highest-performing video compression system on Android for 2027–2030 requires overturning two flawed assumptions that dominate conventional mobile codec engineering:

1. **The "Lossless Paradox" on Smartphones:** Over 98% of video files stored on Android devices are already lossy-compressed H.264/AVC or H.265/HEVC bitstreams produced by the hardware Image Signal Processor (ISP) and `MediaCodec` video encoder at 15–50 Mbps. If a user selects "Lossless Compression" on an existing 50 MB H.264/HEVC camera video and the application decodes it to raw 8-bit/10-bit YUV420 pixels to re-encode with a traditional lossless pixel codec (`FFV1`, `x264 --qp 0`, or `x265 --lossless`), the resulting file **expands by 400% to 900%** (reaching 200–450 MB) because the codec is forced to losslessly preserve quantization ringing and block-boundary Mosquito noise as if it were high-entropy signal. Conversely, professional creators and scientific/medical users recording directly from the camera sensor (`Camera2` API `YUV_420_888`, `YCBCR_P010`, or `RAW10`) require true pixel-exact compression of uncompressed streams. Therefore, **Prism-V bifurcates Lossless Mode into two specialized, bit-exact engines** selected automatically by inspecting the input source:
   - **Mode L-BBR (Bit-Exact Bitstream Recompression):** For existing H.264, HEVC, and JPEG/Motion-JPEG files, Prism-V parses the compressed syntax elements (quantized transform coefficients, motion vector differences, intra prediction modes, sample adaptive offset flags) without performing inverse DCT/DST or pixel reconstruction. It replaces the legacy table-driven CABAC M-coder and Truncated Unary + Exp-Golomb (UEG) binarization with **Position-Indexed Context Allocation (`HuffmanPos` / Interleaved Single-Pass `ECB`)** coupled to a **Deterministic Integer Micro-MLP + 8-Way SIMD Interleaved rANS Entropy Coder**. When any external app reads the file, an on-device **Zero-Copy Virtual Proxy File Descriptor (`StorageManager.openProxyFileDescriptor`)** transcodes the rANS stream back into the exact original H.264/HEVC NAL byte stream in RAM at >180 MB/s, guaranteeing 100% bit-for-bit SHA-256 identity with the original MP4 file while shrinking disk footprint by **14%–24%** `[Estimate]`.
   - **Mode L-PSC (Pixel-Exact Sensor & Master Compression):** For uncompressed camera capture (`YUV_420_888`, `P010`) or lossless editing intermediates, Prism-V uses a **Bijective Subband-Tokenized Temporal Lossless Codec**. It combines Reversible Color Transform (`YCoCg-R`), a 2-level integer lifting wavelet (`LeGall 5/3`), and bijective temporal difference tokenization ($\text{Token}_P(x_t, x_{t-1}) = (x_t - x_{t-1}) + 2^{B-1} - 1$, adapted from [NeuralLVC (Sept 2026)](https://arxiv.org/html/2604.03353v2)) driven by a 4-pass Checkerboard Causal Integer ConvNet on Arm NEON/SME2 + SIMD rANS. This avoids the 94 sequential Transformer passes of NeuralLVC (which require 205–400 s per 1080p frame on a datacenter GPU), achieving **26%–33% of raw YUV size** (beating `FFV1` v3/v4 and `x265 --lossless` by **15%–21% relative**) at **30–60 fps real-time on mobile** `[Estimate]`.

2. **The "Pure Neural vs. Fixed Silicon" Trap in Lossy Mode:** Pure end-to-end neural video codecs—such as [DCVC-RT (CVPR 2025)](https://arxiv.org/html/2502.20762v2) and [DCVC-UF (CVPR 2026)](https://arxiv.org/html/2606.04410v1)—achieve up to -42.2% Bjøntegaard-Delta Rate (BD-rate) vs. VTM-17.0 on desktop GPUs (`RTX 4090` / `B200`), but require 80–350 GMACs/frame of dense neural inference. Running a full-frame neural autoencoder on a smartphone NPU or GPU consumes **2.5–4.8 Joules/frame** (75–140 W equivalent at 30 fps, causing thermal throttling within 45 seconds) and suffers from catastrophic entropy-coding failure if floating-point rounding differs by $10^{-7}$ across Qualcomm Hexagon, MediaTek APU, and Arm Mali hardware ([Ballé et al., ICLR 2019](https://openreview.net/forum?id=S1zz2i0cY7)). Meanwhile, every Android phone already contains a dedicated hardware video ASIC (`MediaCodec` HEVC/AV1/VVC) that computes motion estimation, transform, and entropy coding at **0.005–0.015 Joules/frame** (150–450 mW at 1080p30).
   - **Mode Y-Hybrid (Hardware-Anchored Hybrid Split-Stream Lossy Codec):** Rather than fighting the phone's hardware video ASIC, Prism-V enslaves it. The hardware `MediaCodec` encoder (AV1 on mid/high-end devices; HEVC on entry-level devices) encodes a downscaled or coarse-QP **Base Layer** carrying 85%–92% of the spatio-temporal signal energy at near-zero battery cost. In parallel, Prism-V extracts a compact **Latent Micro-Residual & Structural Edge Stream** compressed by a **Deterministic Integer Overfitted Micro-Decoder (<600 MACs/pixel)** combining [Cool-chic 5.0 (July 2026)](https://arxiv.org/html/2605.02726v2) and feed-forward [N-O Cool-chic (July 2026)](https://arxiv.org/html/2607.13723v1), plus a standalone parametric texture/noise synthesizer ([AOMedia AFGS1](https://aomediacodec.github.io/afgs1-spec/)). During live recording, the feed-forward hypernetwork predicts the micro-decoder parameters in <4 ms/frame; later, when the phone is idle and charging, an optional **Thermal-Gated Overfitting Daemon** refines the per-GOP micro-decoder weights via 500 gradient steps on the NPU/CPU, squeezing an additional **8%–14% BD-rate** without touching the base stream.

### 1.2 Expected Quantitative Performance Summary (with Uncertainty Ranges)

All quantitative projections below are anchored to published baselines and tagged by evidence tier (`[Verified - Primary]`, `[Verified - Secondary]`, `[Author Claim]`, `[Estimate]`, `[Speculation]`).

| Mode & Sub-Path | Primary Metric | Baseline Anchor | Expected Prism-V Result (2027 Target) | Test Corpus & Resolution | Target Hardware Tier | Evidence Tag & Primary Reference |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Lossless Mode L-BBR** (Existing H.264 / HEVC Recompression) | Bit-exact file size reduction (%) | Original H.264 / HEVC CABAC MP4 (0% reduction; SHA-256 identical) | **14.5% – 23.5% smaller** (H.264) **9.0% – 15.5% smaller** (HEVC) | 1080p30 / 4K60 user camera clips & `YouTube-UGC` | Low-end (`Cortex-A55`) to Flagship (`Cortex-X925`) | `[Estimate]` anchored on [Dropbox Lepton (22% on JPEG)](https://github.com/dropbox/lepton) & [Singh (June 2026, -8.9% CABAC overhead)](https://arxiv.org/html/2606.23753v1) |
| **Lossless Mode L-BBR** (On-the-fly Proxy Read Speed) | Sequential read throughput (MB/s) & CPU power | Native file read (>1,500 MB/s) | **160 – 340 MB/s** decode at **<0.8 W** CPU power (supports 4K60 playback) | 50 Mbps 4K60 HEVC / H.264 bitstream | Mid-range (`Cortex-A720` / `A725` NEON) | `[Estimate]` anchored on [RAS rANS (Oct 2025)](https://arxiv.org/html/2511.04684v1) & 8-way NEON rANS benchmarks |
| **Lossless Mode L-PSC** (Raw / Camera2 YUV420 & P010) | Compressed size (% of raw YUV) & Relative savings | `x265 --lossless` (`veryslow`): 36.61% on UVG, 38.00% on Xiph 1080p; `FFV1`: 35.77% on UVG | **27.5% – 31.0% of raw size** (**15% – 21% smaller** than `x265 --lossless` & `FFV1`) | `UVG` 1080p, `Xiph` 1080p, `BVI-AOM` 1088p | Flagship (`Snapdragon 8 Elite Gen 5` / `Dimensity 9500` SME2) | `[Estimate]` anchored on [NeuralLVC (Sept 2026: 30.34% UVG, 29.46% Xiph 1080p)](https://arxiv.org/html/2604.03353v2) |
| **Lossless Mode L-PSC** (Real-time Capture Speed) | Encoding throughput (fps) at 1080p YUV420 | `NeuralLVC` (GH200 GPU): 0.0025–0.005 fps; `FFV1` (mobile 4-thread): 18–32 fps | **30 – 60 fps** (Fast Wavelet+rANS preset) **8 – 15 fps** (Deep ConvNet preset) | `UVG` 1080p YUV420 8-bit / 10-bit | Mid-range to Flagship (4x big cores NEON/SME2) | `[Estimate]` via 4-pass Wavefront vs 94-pass Transformer reduction |
| **Lossy Mode Y-Hybrid** (Quality-First / Offline Charging) | BD-Rate reduction (VMAF & CVVDP) | Hardware `MediaCodec` HEVC (Low-end) & Hardware AV1 / `SVT-AV1` M6 (Flagship) | **-32% to -44% BD-rate** vs HW HEVC **-18% to -27% BD-rate** vs HW AV1 | `JVET CTC Class A/B`, `UVG` 1080p/4K, `MCL-JCV` | All tiers (HW `MediaCodec` + CPU/NPU micro-residual) | `[Estimate]` anchored on [Cool-chic 5.0 (-11% vs VVC)](https://arxiv.org/html/2605.02726v2) & [AV2 CTC (-33.79% VMAF vs AV1)](https://arxiv.org/html/2605.15800v1) |
| **Lossy Mode Y-Hybrid** (Speed / Live Recording Mode) | Encoding speed (fps), Power (W), & BD-Rate | Hardware `MediaCodec` AV1/HEVC real-time capture (1.2–2.2 W total system power) | **60 fps at 1080p / 30 fps at 4K**, **1.4 – 2.5 W** power, **-14% to -21% BD-rate** vs HW base | Live `Camera2` 1080p60 / 4K30 NV12 / P010 | Low-end (1080p30) to Flagship (4K60) | `[Estimate]` anchored on [N-O Cool-chic (July 2026)](https://arxiv.org/html/2607.13723v1) & [AFGS1](https://aomediacodec.github.io/afgs1-spec/) |
| **Lossy Mode Y-Hybrid** (Universal Decoding Overhead) | Additional decode compute & battery cost | Hardware `MediaCodec` AV1/HEVC decode (~250–450 mW at 1080p60) | **500 – 1,500 MACs/pixel** (**+120 to +280 mW** on Arm NEON/SME2 or Vulkan GPU; **60–120 fps**) | 1080p60 & 4K60 hybrid bitstreams | Low-end (`Cortex-A55` NEON) to Flagship (`SME2`) | `[Verified - Primary]` for decoder MAC budget ([Cool-chic 5.0](https://arxiv.org/html/2605.02726v2), [DeepMind C3](https://arxiv.org/abs/2312.02753)) |

### 1.3 Summary of Top 10 Architectural Decisions

| # | Decision Domain | Selected Choice | Rejected Alternatives | Primary Justification |
| :--- | :--- | :--- | :--- | :--- |
| **1** | **Lossless Definition & Architecture** | **Dual-Engine (`Prism-BBR` for compressed files + `Prism-PSC` for raw YUV/P010)** | Single pixel-domain lossless codec (`FFV1` / `x265 --lossless` / `NeuralLVC` only) | Re-encoding lossy MP4s in pixel domain inflates file size 4x–9x; bitstream transcoding shrinks existing MP4s 14%–24% bit-exact. |
| **2** | **Lossy Core Paradigm** | **Hardware-Anchored Hybrid Split-Stream** (`MediaCodec` HW Base + Integer Overfitted Micro-Residual + `AFGS1` Grain) | Pure end-to-end neural codec (`DCVC-UF`/`DCVC-RT`) or pure software `AV2`/`ECM` (`libaom`/`VVenC`) | Pure neural/SW codecs burn 3–8 W on mobile and throttle; HW ASIC handles 90% of pixels at <0.4 W while a 600-MAC/px residual gives next-gen RD gains. |
| **3** | **Cross-Chip Neural Determinism** | **Strict Integer-Only (`INT8` weights / `INT16` activations / `INT32` acc) Dyadic LUT Priors on CPU NEON/SME2** | Floating-point (`FP16`/`BF16`) NPU execution for entropy coding priors | Floating-point non-associativity across Adreno, Mali, Hexagon, and MediaTek NPUs causes catastrophic rANS desynchronization ([Ballé 2019](https://openreview.net/forum?id=S1zz2i0cY7)). |
| **4** | **Entropy Coding Backend** | **8-Way SIMD Interleaved 32-bit rANS** with Position-Indexed Contexts (`HuffmanPos` / Interleaved `ECB`) | Legacy 64-state sequential CABAC M-coder or non-interleaved Range Coding | Interleaved rANS exploits 128-bit NEON / 512-bit SVE2/SME2 SIMD lanes for 4x–8x higher throughput and 2%–9% lower rate overhead ([arXiv:2606.23753](https://arxiv.org/html/2606.23753v1)). |
| **5** | **Scheduling of Heavy Compute** | **Two-Stage Split Lifecycle:** Instant Feed-Forward at Capture + Thermal-Gated Overfitting While Charging | Running full RDO / iterative neural overfitting during live camera recording | Live capture has a strict 16.6 ms (60 fps) and <2.5 W thermal envelope; background overfitting while plugged in leverages idle hours for +8%–14% BD-rate. |
| **6** | **Container & Ecosystem Compatibility** | **Backward-Compatible Dual-Layer ISOBMFF (`.mp4`)** for Lossy + **Virtual FUSE/ProxyFD (`.pvloss`)** for Lossless | Proprietary standalone container (`.pvid`) requiring custom players everywhere | Standard players play the `MediaCodec` base track immediately; legacy apps read `.pvloss` as standard `.mp4` via zero-copy `openProxyFileDescriptor`. |
| **7** | **Core Systems Implementation Language** | **Rust (Safe orchestration, parsers, MP4 muxer, JNI) + C++20 / Hand-Tuned Arm Assembly (`KleidiAI` / NEON / SME2)** | Pure C/C++ (memory safety risks in media parsers) or Kotlin/Java (GC jitter & SIMD limits) | Media parsers are Android's #1 CVE vector; Rust guarantees memory safety while C++20/ASM intrinsics deliver 100% peak SIMD throughput. |
| **8** | **GPU / NPU Acceleration API** | **Vulkan 1.3/1.4 Compute (`VK_KHR_cooperative_matrix`)** for reconstruction + **LiteRT CompiledModel API** for encoder-only analysis | Deprecated Android NNAPI or vendor-locked closed SDKs for bitstream-normative ops | NNAPI is deprecated in Android 15+; Vulkan Cooperative Matrix provides cross-vendor low-overhead matrix ops on Adreno, Mali, and Xclipse GPUs. |
| **9** | **Shared Infrastructure Across Modes** | **Unified SIMD rANS Engine, Integer Dyadic NN Kernel Library, Tile Ring-Buffer Pool, and Thermal Telemetry Controller** | Separate independent codebases for Lossless and Lossy modes | Sharing 65% of low-level kernels cuts binary size (<4.2 MB native `.so`), halves QA surface, and unifies memory limits (<64 MB RSS). |
| **10** | **Patent & Licensing Strategy** | **Pass-Through OS `MediaCodec` Delegation + Royalty-Free AOMedia (`AFGS1`) & Expired/Clean-Room Wavelet/rANS Core** | Shipping unlicensed software `HEVC`/`VVC` encoders in-app | Delegating base encoding to Android's hardware `MediaCodec` shifts patent pool royalties (Access Advance / Via LA) to the OEM hardware license. |

### 1.4 The First 30 Days: Immediate Execution Plan

To validate the core mathematical and systems hypotheses before hiring the full engineering team, the project begins with a disciplined **30-Day Phase 0 Proof-of-Concept Sprint** executed by 3 Founding Principal Engineers (Codec Architect, Arm SIMD/Systems Lead, ML Compression Lead):

- **Days 1–7 (Week 1): `Prism-BBR` Lossless H.264/HEVC Bitstream Recompression Prototype**
  - Fork open-source `openh264` / `ffhevc` CABAC syntax parser in Rust/C++ to extract lossless syntax token streams (`coeff_abs_level_minus1`, `mvd`, `intra_pred_mode`) from 20 real Android 1080p/4K camera MP4s.
  - Implement the [Singh (June 2026)](https://arxiv.org/html/2606.23753v1) `HuffmanPos` and single-pass interleaved `ECB` context allocators plus a 3-layer `INT8` Lookup-Table Micro-MLP feeding an 8-way NEON rANS coder.
  - **Day 7 Deliverable:** Command-line tool demonstrating bit-exact round-trip (`SHA-256` match) and measuring exact % file size reduction and MB/s speed on a Snapdragon 8 Gen 3 / 8 Elite test phone.
- **Days 8–15 (Week 2): Deterministic Cross-Silicon Integer Prior Verification (`Prism-DetInt`)**
  - Implement the dyadic integer neural layer ($W \in \text{INT8}, X \in \text{INT16}, \text{Acc} \in \text{INT32}$) in C++20 using Arm NEON `vdotq_s32` and Armv9 `SME2` (via `KleidiAI` on Snapdragon 8 Elite Gen 5 / Dimensity 9400+).
  - Run 10,000 random latent frames across 5 distinct SoCs (Qualcomm Snapdragon, MediaTek Dimensity, Samsung Exynos, Google Tensor, x86_64 host emulator) to prove **0 bit-flips** in decoded rANS streams.
- **Days 16–23 (Week 3): `Prism-Lossy` Hybrid Base + Overfitted Micro-Residual Spike**
  - Pair Android hardware `MediaCodec` AV1/HEVC output (at CRF/QP equivalent to 1.5 Mbps and 3.0 Mbps 1080p) with an adapted [Cool-chic 5.0](https://github.com/Orange-OpenSource/Cool-Chic) / [N-O Cool-chic](https://arxiv.org/html/2607.13723v1) micro-residual coder (800 MACs/pixel) and `AFGS1` film grain extractor on 5 `UVG` 1080p sequences.
  - Measure VMAF, ColorVideoVDP (`CVVDP`), and Monsoon hardware power meter Joules/frame.
- **Days 24–30 (Week 4): Zero-Copy ProxyFD Benchmark & Go/No-Go Gate Review**
  - Build a minimal Android service implementing `StorageManager.openProxyFileDescriptor` backed by the `Prism-BBR` decoder and open the virtual `.mp4` inside ExoPlayer (`Media3`) and WhatsApp.
  - **Day 30 Go/No-Go Gate:** Proceed to Phase 1 full build if and only if: (a) `Prism-BBR` achieves $\ge 14\%$ lossless size reduction on H.264 and $\ge 9\%$ on HEVC at $\ge 100\text{ MB/s}$ decode speed on a mid-range phone; (b) `Prism-DetInt` shows 100.000% bit-exact parity across all 5 silicon vendors; and (c) `Prism-Lossy` achieves $\ge 15\%$ VMAF BD-rate improvement over standalone hardware `MediaCodec` at $< 1,200\text{ MACs/pixel}$ decode overhead.

---

## 2. Research Findings: Complete 2026–2030 Video Compression Landscape

### 2.1 Explicit 2027–2030 Mobile Hardware, OS, and Standards Assumptions

To design for 2027–2030 without guesswork, we establish explicit baseline assumptions across three Android device tiers (Entry/Low-End, Mainstream Mid-Range, and Flagship), grounded in shipping late-2025/2026 silicon ([Snapdragon 8 Elite Gen 5](https://www.qualcomm.com/content/dam/qcomm-martech/dm-assets/documents/Snapdragon-8-Elite-Gen-5-product-brief.pdf), [MediaTek Dimensity 9500](https://www.mediatek.com/products/smartphones/mediatek-dimensity-9500), [Arm SME2 with KleidiAI](https://www.arm.com/company/success-library/google-android-ai-sme2)) and the [Android 16](https://source.android.com/docs/compatibility/16/android-16-cdd) and [Android 17 Compatibility Definition Documents (CDD)](https://source.android.com/docs/compatibility/17/android-17-cdd).

#### Table 2.1: 2027–2030 Target Android Hardware Tiers and Capabilities

| Hardware Parameter | Tier 1: Entry / Low-End (2027–2030, $100–$180) | Tier 2: Mainstream Mid-Range (2027–2030, $220–$450) | Tier 3: Flagship (2027–2030, $600–$1,400) | Evidence Tag & Source |
| :--- | :--- | :--- | :--- | :--- |
| **Representative SoCs** | Snapdragon 4 Gen 4/5, Helio G100 / Dimensity 6400+, Unisoc T820+ | Snapdragon 7+ Gen 4/5, Dimensity 8400/8500, Exynos 1580/1680 | Snapdragon 8 Elite Gen 5/6/7, Dimensity 9500/9600, Tensor G6/G7 | `[Verified - Primary]` for 2026 chips ([Qualcomm](https://www.qualcomm.com/content/dam/qcomm-martech/dm-assets/documents/Snapdragon-8-Elite-Gen-5-product-brief.pdf), [MediaTek](https://www.mediatek.com/products/smartphones/mediatek-dimensity-9500)); `[Estimate]` for 2027–2030 |
| **CPU Architecture & SIMD** | 2x `Cortex-A78`/`A720` + 6x `Cortex-A55`/`A520`; Armv8.2-A `NEON` + `DotProd` (`SDOT`/`UDOT`); **No SME2** | 1x `Cortex-X4`/`X925` + 3x `A725` + 4x `A520`; Armv9.2-A `NEON`, `SVE2`, `I8MM`, optional `SME2` | All-Big Core (`Cortex-X925`/`X930` or Qualcomm `Oryon Gen 2/3`); **Armv9.2-A `SME2` (Scalable Matrix Extension 2)** | `[Verified - Primary]` ([Arm SME2](https://www.arm.com/technologies/sme2), [ExecuTorch KleidiAI](https://developer.arm.com/community/arm-community-blogs/b/ai-blog/posts/executorch-1-0-is-here-and-with-sme2-optimizations-through-kleidiai)) |
| **RAM & Available App Heap** | 4–6 GB LPDDR4X (~12–17 GB/s); safe codec working set **$\le 64\text{ MB}$** | 8–12 GB LPDDR5 (~44–51 GB/s); safe codec working set **$\le 160\text{ MB}$** | 12–24 GB LPDDR5X/LPDDR6 (~75–115 GB/s); safe codec working set **$\le 384\text{ MB}$** | `[Estimate]` based on Android LMK (Low Memory Killer) thresholds |
| **GPU & Compute API** | Mali-G57 / Adreno 619+; Vulkan 1.1/1.3 (limited `FP16`/`INT8` subgroup ops) | Adreno 732+ / Mali-G720; Vulkan 1.3/1.4 + `VK_KHR_cooperative_matrix` | Adreno 830/840+ / Immortalis-G925+; Vulkan 1.4 + full `VK_KHR_cooperative_matrix` | `[Verified - Primary]` ([Vulkan 1.4.359](https://www.phoronix.com/news/Vulkan-1.4.359), [Adreno Vulkan](https://github.com/SnapdragonGameStudios/adreno-gpu-vulkan-code-sample-framework/actions)) |
| **NPU & AI Runtime** | 2–8 TOPS INT8 (often restricted or high dispatch latency); CPU NEON preferred | 15–35 TOPS INT8/INT4 via `LiteRT` QNN / NeuroPilot delegate | 50–100+ TOPS INT8/INT4/FP8 (`Hexagon NPU` / `NPU 990`) via `LiteRT` `CompiledModel` API | `[Verified - Primary]` ([LiteRT QNN](https://developers.google.com/edge/litert/next/qualcomm), [Dimensity 9500 NPU 990](https://www.mediatek.com/products/smartphones/mediatek-dimensity-9500)) |
| **Hardware Video Decode (`MediaCodec`)** | H.264, HEVC 10-bit (4K30/1080p60), VP9; **AV1 SW (`dav1d`) or 1080p HW** | H.264, HEVC, VP9, **AV1 Main10 HW (4K60)** | H.264, HEVC, VP9, **AV1 (8K60)**, **APV (RFC 9924)**, **VVC/H.266 HW Decode** (emerging) | `[Verified - Primary]` ([Android 17 CDD](https://source.android.com/docs/compatibility/17/android-17-cdd), [Samsung APV](https://developer.samsung.com/mobile/apv.html)) |
| **Hardware Video Encode (`MediaCodec`)** | H.264, HEVC 8-bit (1080p60 / 4K30); **No HW AV1 Encode** | H.264, HEVC 10-bit (4K60), **AV1 HW Encode (1080p60/4K30)** per Android 17 CDD MPC | H.264, HEVC 10-bit, **AV1 10-bit HW Encode (4K60/8K30)**, **APV 10/12-bit Encode** | `[Verified - Primary]` ([Android 17 MPC](https://source.android.com/docs/compatibility/17/mpc), [Dimensity 9500 Specs](https://fsl-specs.com/mobile-processor/mediatek-dimensity-9500/)) |
| **Sustained Thermal Envelope** | **1.5 – 2.2 W** sustained (<40°C skin temp) | **2.5 – 3.5 W** sustained | **3.8 – 5.5 W** sustained (passive chassis dissipation limit) | `[Estimate]` standard smartphone passive thermal physics |

#### Table 2.2: Contingency Matrix — What Changes If Our 2027–2030 Assumptions Are Wrong?

| Assumption | What Could Go Wrong by 2027–2030 | Concrete Architectural Pivot in Prism-V |
| :--- | :--- | :--- |
| **1. Hardware AV1 Encoding Reaches Mainstream Mid-Range Phones** (driven by [Android 17 CDD Section 5.12](https://source.android.com/docs/compatibility/17/android-17-cdd)) | SoC vendors restrict hardware AV1 encoding to $800+ flagships to save silicon die area, leaving mid-range phones with only hardware HEVC/H.264 encode. | `Prism-Lossy` is base-codec agnostic: its ISOBMFF container (`prsm`) dynamically binds the enhancement stream (`prmr`) to **Hardware HEVC (`hvc1`)** when HW AV1 is absent, increasing the micro-residual bitrate budget from 10% to 16% of total rate to compensate for HEVC's ~15% lower base efficiency. |
| **2. Arm SME2 Becomes Standard across Armv9.2+ Big Cores** | OEMs disable SME2 on mid-tier cores or OS context-switch latency penalizes streaming SME2 mode in background threads. | Our `Prism-DetInt` integer neural library compiles three dispatch targets into the `.so` binary: (a) `Armv9-SME2` (`KleidiAI`), (b) `Armv8.2-DotProd` (`vdotq_s32` 4-way INT8 dot product, supported on >96% of active Android devices in 2027), and (c) `Vulkan Cooperative Matrix` (`VK_KHR_cooperative_matrix`) on GPU. |
| **3. Hardware VVC (H.266) or AV2 Decode Ships on Flagships by 2027–2028** | VVC patent pool friction ([Access Advance / VCL Advance](https://lumenci.com/blogs/hevc-vs-av1-vs-vvc-royalties-licensing-risk/)) stalls mobile VVC adoption, and hardware AV2 decoders do not reach phones until 2029–2030 ([Fora Soft AV2 Timeline](https://www.forasoft.com/learn/video-encoding/articles/av2-what-to-expect-2026-2031)). | Prism-V **does not depend on hardware VVC or hardware AV2**. By combining ubiquitous hardware AV1/HEVC with our 600-MAC/pixel software/SIMD micro-residual layer, Prism-V achieves AV2/ECM-class compression efficiency on 2024–2027 hardware immediately. If HW VVC/AV2 *does* arrive, Prism-V adopts it as a plug-in base layer via `MediaCodec`. |
| **4. LiteRT NPU Delegates Allow Low-Latency Per-Frame Dispatch** | Driver fragmentation or SELinux policy blocks third-party apps from sustained NPU access on certain Chinese OEM ROMs (Xiaomi HyperOS, Oppo ColorOS, Vivo OriginOS). | NPU is used **exclusively for encoder-side non-normative search and charging-time overfitting**. All decoder-side operations and live capture fallbacks run deterministically on CPU NEON/SME2 and Vulkan GPU shaders, which cannot be blocked by NPU vendor locks. |
| **5. Android FUSE / `openProxyFileDescriptor` Latency Remains Low** | Future Android versions throttle background `ProxyFileDescriptorCallback` threads under strict battery-saver policies. | Provide a one-tap batch "Export Standard MP4" intent action and an automatic MediaStore hard-link cache for active social-media sharing targets. |

---

### 2.2 Complete Taxonomy of Video Compression Families (How They Work, Maturity, and Mobile Cost)

We organize the entire landscape of video compression as of November 2026 into **seven distinct technological families**, evaluating each from first principles: mathematical mechanism, standardization/open-source maturity, and concrete computational/energy cost on an Android smartphone.

#### Family 1: Classical Block-Transform Hybrid Codecs (H.264/AVC, H.265/HEVC, AV1, H.266/VVC, AV2, JVET ECM / H.267)

- **How They Work (Plain English & Technical Mechanism):**
  Every standardized mainstream video codec from H.264 (2003) to AV2 (May 2026) and JVET ECM (2026) follows the **block-based hybrid prediction-transform paradigm**:
  1. **Partitioning:** Each frame is split into hierarchical blocks—from fixed $16\times 16$ macroblocks in H.264, to $64\times 64$ Coding Tree Units (CTUs) in HEVC, $128\times 128$ superblocks in AV1, and $128\times 128$ Quadtree plus Multi-Type Tree (QTMT, supporting binary and ternary splits) in VVC, AV2, and ECM.
  2. **Prediction (Intra & Inter):** To remove redundancy before transforming pixels, the encoder predicts the current block from already-decoded pixels. *Intra prediction* projects boundary pixels along directional angles (35 modes in HEVC, 56 in AV1, 67 in VVC, plus Matrix-based Intra Prediction `MIP` and Cross-Component Linear Models `CCLM` predicting chroma from luma). *Inter prediction* searches previously decoded reference frames for matching patches using translational motion vectors, affine 4-/6-parameter motion models (for zoom/rotation), Geometric Partitioning Mode (`GPM`), and Decoder-Side Motion Vector Refinement (`DMVR`) / Bilateral Optical Flow (`BDOF`).
  3. **Transform & Quantization:** The prediction residual $r(x,y) = f(x,y) - \hat{f}_{\text{pred}}(x,y)$ is mapped to the frequency domain via separable 2D integer approximations of the Discrete Cosine Transform (`DCT-II`, `DCT-VIII`) and Discrete Sine Transform (`DST-VII`), optionally followed by a Low-Frequency Non-Separable Transform (`LFNST` in VVC/ECM) or Identity Transform (`IDTX` for sharp screen text). Coefficients $C_{u,v}$ are divided by a Quantization Step $Q(\text{QP})$:
     $$\hat{C}_{u,v} = \text{sign}(C_{u,v}) \cdot \left\lfloor \frac{|C_{u,v}| + \theta \cdot Q(\text{QP})}{Q(\text{QP})} \right\rfloor$$
     This quantization step is the sole intentional source of information loss in lossy mode.
  4. **In-Loop Filtering:** Reconstructed blocks pass through a cascade of filters inside the prediction loop to suppress block boundaries and ringing before serving as references for future frames: Deblocking Filter (`DBF`), Sample Adaptive Offset (`SAO` in HEVC/VVC) or Constrained Directional Enhancement Filter (`CDEF` in AV1/AV2), Loop Restoration (`Wiener` / self-guided filters in AV1/AV2), and Adaptive Loop Filter (`ALF` / Cross-Component `CC-ALF` in VVC/ECM).
  5. **Entropy Coding:** Quantized syntax elements are losslessly serialized into a bitstream using Context-Adaptive Binary Arithmetic Coding (`CABAC` M-coder in H.264/HEVC/VVC/ECM) or multi-symbol cumulative distribution function (CDF) range coding (up to 16-symbol alphabet in AV1/AV2).

- **2026 State-of-the-Art Sub-Generations & Maturity Labels:**
  - **H.264 / AVC & H.265 / HEVC:** `Shipping in products` & `Standardized`. Ubiquitous hardware encode/decode across 100% of Android devices. Open-source encoders: `x264`, `x265`. In 2026 benchmarks, `x265` requires ~38% less bitrate than `x264` at equal quality but encodes 4x slower in software ([Fora Soft, June 2026](https://www.forasoft.com/learn/video-quality/articles-vqm/encoder-comparison-x264-x265-svt-av1)).
  - **AV1 (AOMedia):** `Shipping in products`, `Standardized`, `Open-source implementation` (`SVT-AV1` v3.1.0, `libaom`, `rav1e` v0.8.0; decoder `dav1d`). `SVT-AV1` saves 46%–58% bitrate over `x264` ([Fora Soft, June 2026](https://www.forasoft.com/learn/video-quality/articles-vqm/encoder-comparison-x264-x265-svt-av1)). Hardware decode is mandatory on Android 14+ flagships/mid-range; hardware AV1 encode is standardized in [Android 17 CDD](https://source.android.com/docs/compatibility/17/android-17-cdd).
  - **H.266 / VVC (Versatile Video Coding, ITU-T / ISO/IEC 23090-3):** `Standardized` (2020), `Open-source implementation` (`VVenC` / `VVdeC` by Fraunhofer HHI, `OpenVVC`). Achieves ~40%–50% bitrate reduction over HEVC on 4K/UHD and ~20%–25% over AV1 ([Fora Soft VVC, Oct 2026](https://www.forasoft.com/learn/video-encoding/articles/h266-vvc-explained)). Mobile hardware decode is just emerging in late 2026 flagship SoCs ([Streaming Learning Center, Sept 2026](https://streaminglearningcenter.com/articles/vvc-on-the-rise.html)), while mobile hardware VVC encoding is virtually non-existent due to QTMT search complexity and royalty fragmentation.
  - **AV2 (AOMedia Spec 1.0.0, finalized May 28 / June 6, 2026):** `Standardized` & `Open-source implementation` (`AVM` reference software). Per the official AOMedia AV2 CTC paper ([arXiv:2605.15800, May 2026](https://arxiv.org/html/2605.15800v1)), AV2 v13.0 / 1.0.0 achieves **-29.81% PSNR-YUV BD-rate** and **-33.79% VMAF BD-rate** in Random Access (RA) over AV1, **-35.77% VMAF BD-rate** in Adaptive Streaming (AS), and **~40% BD-rate** on screen content and 2160p HDR `[Verified - Primary]`. However, reference `AVM` encoding takes **33x longer** than the `libaom` AV1 anchor and software decoding takes **3.86x longer** ([arXiv:2605.15800](https://arxiv.org/html/2605.15800v1)). Hardware decode in mobile silicon is not expected before 2028–2029 ([Fora Soft AV2 Roadmap, Oct 2026](https://www.forasoft.com/learn/video-encoding/articles/av2-what-to-expect-2026-2031)).
  - **JVET ECM (Enhanced Compression Model v11–v15, Exploratory Platform for H.267):** `Research prototype with code` (`ECM` reference software). Aims for $\ge 40\%$ BD-rate reduction over VVC for 4K+. ECM v13 already demonstrates **>25% BD-rate reduction** in Random Access over VVC (`VTM`) and up to 40% on screen content ([Streaming Media, Feb 2025](https://www.streamingmedia.com/Articles/News/Online-Video-News/H.267-A-Codec-for-(One-Possible)-Future-167889.aspx); [Qualcomm CfE, Dec 2025](https://www.qualcomm.com/news/onq/2025/12/advancing-video-coding-call-for-evidence); [arXiv:2404.07872](https://arxiv.org/html/2404.07872v1)) `[Verified - Primary]`. Encoding complexity is 10x–40x higher than VTM, making pure software ECM encoding completely unviable on phones.

#### Family 2: Intra-Only Mezzanine, Professional & Archival Lossless Codecs (FFV1, Samsung APV, JPEG XL Modular)

- **How They Work:**
  - **FFV1 (IETF RFC 9043, v3 & v4):** `Standardized` & `Open-source implementation` (`libavcodec`). Divides each frame into independent slices, applies a median/gradient spatial predictor ($P = \text{median}(L, T, L+T-TL)$) on YUV or RGB planes, and encodes the prediction residual using either Golomb-Rice codes or a binary Range Coder with quantized context states based on neighboring local gradients ([IETF RFC 9043](https://www.rfc-editor.org/rfc/rfc9043)). It uses **intra-only** prediction (or optional basic frame-diff in experimental branches) without motion compensation, making it fast and resilient but 18%–25% larger than temporal lossless codecs on low/medium-motion video ([NeuralLVC Table 1 & Table 4, Sept 2026](https://arxiv.org/html/2604.03353v2)) `[Verified - Primary]`.
  - **Samsung Advanced Professional Video (APV, IETF RFC 9924):** `Shipping in products`, `Standardized`, `Open-source implementation` ([OpenAPV](https://developer.samsung.com/mobile/apv.html)). Integrated natively into **Android 16** and shipping with hardware acceleration on the Samsung Galaxy S26 Ultra (April 2026) and Chips&Media IP ([Samsung Newsroom, April 2026](https://news.samsung.com/global/editorial-how-apv-is-elevating-mobile-video-to-professional-standards); [Chips&Media, July 2025](https://www.edge-ai-vision.com/2025/07/chipsmedias-new-apv-codec-delivers-extreme-visual-quality-to-the-android-industry/)). APV is a tile-based intra-frame mezzanine codec designed for low-complexity, perceptually lossless/near-lossless 10-bit/12-bit 4:2:2 and 4:4:4 capture, using 20% less storage than Apple ProRes ([Samsung Developer](https://developer.samsung.com/conference/sdc23/sessions/advanced-professional-video-codec)) `[Verified - Primary]`. However, it is not designed for low-bitrate consumer distribution or inter-frame temporal compression.

#### Family 3: Layered & Parametric Enhancement Codecs (MPEG-5 Part 2 LCEVC, AOMedia AFGS1)

- **How They Work:**
  - **MPEG-5 Part 2 LCEVC (Low Complexity Enhancement Video Coding, ISO/IEC 23094-2):** `Standardized` & `Shipping in products` ([V-Nova SDK](https://docs.v-nova.com/technologies/mpeg-5.lcevc)). Encodes a video at half resolution (e.g., 540p or 1080p) using any existing hardware base codec (H.264, HEVC, AV1) and adds two lightweight residual sublayers ($L_1$ at half resolution, $L_2$ at full resolution) using tiny $2\times 2$ or $4\times 4$ directional decomposition transforms and run-length/Huffman entropy coding ([LCEVC Overview](https://www.lcevc.org/)). Because the heavy motion estimation runs at $1/4$ pixel count on the hardware ASIC, total encoding power drops while decoding runs in a fast GPU shader. However, LCEVC's linear $2\times 2$ / $4\times 4$ transforms have limited non-linear representational capacity compared to modern content-adapted micro-neural filters, and V-Nova's proprietary licensing limits open adoption.
  - **AOMedia Film Grain Synthesis 1 (AFGS1, Jan 2024):** `Standardized` & `Open-source implementation` ([AFGS1 Spec](https://aomediacodec.github.io/afgs1-spec/)). High-frequency sensor noise and film grain are incompressible via block transforms because they are spatio-temporally uncorrelated; encoding them directly wastes 20%–45% of the bitrate or results in "plastic" smoothed surfaces when quantized away. AFGS1 estimates a 14-parameter autoregressive (AR) noise model and piece-wise linear intensity scaling curve at the encoder, strips the noise prior to compression, transmits <200 bytes/frame of metadata, and re-synthesizes perceptually identical grain at the decoder using a deterministic pseudo-random register shift ([AOMedia AFGS1](http://aomedia.org/specifications/afgs1/)) `[Verified - Primary]`.

#### Family 4: End-to-End Autoencoder Neural Video Codecs (DCVC-FM, DCVC-RT, DCVC-UF, JVET NNVC)

- **How They Work:**
  Instead of hand-crafted DCT blocks and integer motion vectors, end-to-end Neural Video Codecs (NVCs) parameterize the entire pipeline as deep neural networks trained jointly to minimize Rate-Distortion Lagrangian loss $L = R + \lambda D$:
  - **Conditional Coding (DCVC Family):** Pioneered by Microsoft Research Asia (`DCVC` -> `DCVC-HEM` -> `DCVC-DC` -> `DCVC-FM` -> `DCVC-RT` -> `DCVC-UF`). Rather than subtracting a pixel-domain motion-compensated frame ($f_t - \hat{f}_{t-1}$, residual coding), DCVC extracts multi-scale high-dimensional temporal context features $C_t$ from previous decoded states and feeds $C_t$ as a conditioning input into both the contextual encoder $y_t = E(f_t \mid C_t)$, the hyperprior/autoregressive entropy model $p(\hat{y}_t \mid \hat{z}_t, C_t)$, and the contextual decoder $\hat{f}_t = D(\hat{y}_t \mid C_t)$ ([GitHub microsoft/DCVC](https://github.com/microsoft/dcvc)).
  - **DCVC-RT (CVPR 2025):** Eliminates explicit optical-flow warping networks and multi-scale deformable alignments (which cause severe memory-bandwidth bottlenecks), replacing them with implicit temporal feature propagation and single-scale feature modulation. In `int16` precision, DCVC-RT achieves real-time 1080p coding (~30–100+ fps on a desktop NVIDIA RTX 4090 GPU) while surpassing VTM ([arXiv:2502.20762, March 2025](https://arxiv.org/html/2502.20762v2)) `[Verified - Primary]`.
  - **DCVC-UF (CVPR 2026, "Ultra-Fast Neural Video Compression"):** Transitions from sequential frame-by-frame processing to a **chunk-based coding framework** that processes multi-frame chunks simultaneously and streamlines entropy coding dataflow. On 1080p video, `DCVC-UF (HT-S)` reaches **655.9 fps encoding and 453.3 fps decoding on an NVIDIA RTX 4090** (and 1,415.1 fps on an NVIDIA B200), while `DCVC-UF (HT-L)` achieves an average **-42.2% BD-rate reduction compared to VTM-17.0** in low-delay mode ([arXiv:2606.04410, June 2026](https://arxiv.org/html/2606.04410v1); [alphaXiv](https://www.alphaxiv.org/abs/2606.04410)) `[Verified - Primary]`.
  - **JVET NNVC (Neural Network-based Video Coding v7+):** `Research prototype with code`. Embeds neural tools (neural intra prediction, neural inter bi-prediction, and a ~40k–100k MAC/pixel neural in-loop filter) inside the VVC/VTM hybrid loop, achieving -11.9% to -22.6% BD-rate over VTM-11.0 ([arXiv:2309.05846](https://arxiv.org/html/2309.05846v2); [MPEG JVET](https://www.mpeg.org/structure/jvet/)) `[Verified - Primary]`.
- **Why Pure End-to-End NVCs Fail as Standalone Mobile Codecs Today:**
  1. **Mobile Power & Memory Bandwidth Wall:** An NVIDIA RTX 4090 has 1,008 GB/s of GDDR6X bandwidth and a 450 W TDP; a mid-range Android phone has 44 GB/s of LPDDR5 and a 3.0 W sustained thermal budget. Even a streamlined 40–80 GMAC/frame autoencoder (`DCVC-RT` / `DCVC-UF`) requires reading/writing hundreds of megabytes of intermediate feature maps per frame, consuming **1.8–4.2 W** on a mobile NPU/GPU and completely locking out low-end phones lacking 30+ TOPS NPUs.
  2. **Cross-Chip Non-Determinism:** `DCVC-RT` and `DCVC-UF` rely on GPU `FP16`/`INT16` tensor cores. Without strict integer LUT quantization of the entropy prior, a video encoded on a Qualcomm Snapdragon 8 Elite NPU will fail to entropy-decode on a MediaTek Dimensity 9500 or Samsung Exynos GPU after the first single-ulp rounding discrepancy.

#### Family 5: Implicit Neural Representations (INRs) & Overfitted Lightweight Codecs (HNeRV, HiNeRV, Cool-chic 5.0, N-O Cool-chic, DeepMind C3)

- **How They Work:**
  Instead of training a single giant universal decoder network ($10^7$ parameters) offline, **overfitted codecs** fit a tiny, content-specific neural network ($10^3$ to $10^5$ parameters) plus compact 2D/3D latent grids directly to the target video during encoding, and transmit the quantized network weights + quantized latents as the bitstream:
  - **HNeRV / HiNeRV (CVPR 2023 / NeurIPS 2023):** `Research prototype with code`. Represent a video as a hierarchical neural network mapping frame/patch indices and tiny content-adaptive embeddings to RGB/YUV frames via ConvNeXt/depthwise-separable upsampling blocks ([HNeRV arXiv:2304.02633](https://arxiv.org/html/2304.02633v1); [HiNeRV arXiv:2306.09818](https://arxiv.org/html/2306.09818v3)). While HiNeRV beats HEVC/VVC on UVG, encoding requires hours of GPU gradient descent per video, and decoding still requires 20,000–80,000 MACs/pixel.
  - **Cool-chic 5.0 (Orange Labs, May/July 2026) & DeepMind C3 (Dec 2023):** `Open-source implementation` ([Orange-OpenSource/Cool-Chic](https://github.com/Orange-OpenSource/Cool-Chic); [google-deepmind/c3_neural_compression](https://github.com/google-deepmind/c3_neural_compression)). Cool-chic represents an image or video frame using a hierarchy of multi-resolution 2D latent grids decoded by a tiny learned upsampler, a tiny synthesis MLP (3–4 layers of width 8–40), and a tiny Autoregressive Entropy Module (`ARM`).
    - In **Cool-chic 5.0** ([arXiv:2605.02726v2, July 2026](https://arxiv.org/html/2605.02726v2)), Philippe et al. introduce inter-feature entropy modeling and an improved optimizer that **cuts encoding iterations by 10x** while achieving **-11% BD-rate reduction compared to H.266/VVC** at a decoding complexity of just **500 to 3,000 MACs per decoded pixel** (250x less complex than neural autoencoders like `MLIC++`) `[Verified - Primary]`.
    - Similarly, **DeepMind C3** ([arXiv:2312.02753](https://arxiv.org/abs/2312.02753)) matches VTM on UVG video at **<3,000–5,000 MACs/pixel** `[Verified - Primary]`.
  - **N-O Cool-chic ("Non-Overfitted Cool-chic", Orange Labs, July 2026):** `Research prototype with code` ([arXiv:2607.13723v1, July 2026](https://arxiv.org/html/2607.13723v1)). Solves the Achilles' heel of Cool-chic (slow iterative gradient descent during encoding) by introducing a **feed-forward encoder network** that predicts the latent grids and decoder parameters in a **single forward pass (<1 second)**, reducing encoding complexity by **1,000x** while retaining the ultra-fast ~2,000 MAC/pixel Cool-chic decoder (at a +45% bitrate penalty relative to full iterative Cool-chic, putting it on par with Ballé hyperprior autoencoders at 30x lower decode cost) `[Verified - Primary]`.

#### Family 6: Generative Diffusion & Masked-Diffusion Lossless Codecs (UniGVC, DiffVC, NeuralLVC)

- **How They Work:**
  - **Lossy Generative Diffusion Codecs (`UniGVC`, Oct 2026; `DiffVC`, Nov 2025; `DiSCo`, Dec 2025):** `Paper only` / `Research prototype`. Compress video down to ultra-low bitrates (0.0005 to 0.01 bpp) by transmitting compact semantic tokens or highly quantized latents and reconstructing frames via Autoregressive or Bidirectional Diffusion Transformers (`UniGVC`, [arXiv:2610.13023, Oct 2026](https://arxiv.org/html/2610.13023v1)). While `UniGVC-AR` reaches 36.85 fps on a desktop GPU at 0.0005 bpp, diffusion codecs hallucinate high-frequency textures (violating fidelity on faces/text) and consume >10 W on mobile. We **reject** generative diffusion for general lossy coding, except as an optional ultra-low-bitrate (<150 kbps) emergency mode in 2029+.
  - **Masked-Diffusion Lossless Video Coding (`NeuralLVC`, ACM MM '26, Sept 2026):** `Paper only` / `Research prototype` ([arXiv:2604.03353v2, Sept 8, 2026](https://arxiv.org/html/2604.03353v2)). Uricchio & Bertini introduce the first temporally conditioned masked-diffusion lossless video codec:
    - **Mechanism:** Divides YUV420 planes into $32\times 32$ patches (1,024 tokens). Uses **bijective linear tokenization**: for I-frames, $\text{Token}_I(x) = 2x \in \{0, 2, \dots, 510\}$; for P-frames, encodes the temporal pixel difference $\text{Token}_P(x_t, x_{t-1}) = (x_t - x_{t-1}) + 255 \in [0, 510]$, adding a learned reference embedding $\text{RefEmb}(2x_{t-1})$ (+1.3% parameters) to condition an 8-layer Transformer (`LLaDA` bidirectional masked diffusion backbone, 15.18M parameters). To avoid 1,024 sequential passes per patch, it uses `HPAC` diagonal group-wise parallel decoding ($s(r,c) = c + r\cdot \delta$, yielding 94 groups for $\delta=2$) followed by arithmetic coding.
    - **Verified Results:** Across 33 sequences (`Xiph` CIF/720p/1080p, `UVG` 1080p, `BVI-AOM` 1088p), `NeuralLVC` achieves **29.85% of raw size on CIF** (beating `x265 --lossless` at 36.37% by **17.9% relative** and `x264 --qp 0` at 36.77% by **18.8% relative**), **29.46% on full-length 1080p Xiph** (vs 38.00% `x265 --lossless`, a **22.5% relative reduction**, and 35.73% `FFV1`), and **30.34% on UVG 1080p** (vs 35.77% `FFV1`, 36.06% `x264`, 36.61% `x265`) `[Verified - Primary]`.
    - **Critical Bottleneck on Mobile:** Because each $32\times 32$ patch requires 94 sequential Transformer passes (21.1 GMACs per pass) and a 1080p YUV420 frame has ~2,970 patches, `NeuralLVC` requires **$2.8\times 10^5$ forward passes (11.8 PFLOPs / 5.9 PMACs) and 205–400 seconds per 1080p frame on an NVIDIA GH200 superchip** ([arXiv:2604.03353v2 Section 4.6](https://arxiv.org/html/2604.03353v2))! Thus, we **adopt** NeuralLVC's bijective temporal tokenization and reference-conditioned probability estimation, but **replace** its 94-pass self-attention Transformer with a 4-pass Checkerboard Causal Integer ConvNet + Wavelet Subband Decomposition (`Prism-PSC`), reducing complexity by **25,000x** (from 5,900,000 GMACs/frame to ~0.22 GMACs/frame).

#### Family 7: Bit-Exact Bitstream Recompressors (Dropbox Lepton, JPEG XL Transcoding, CABAC Binarization Re-Coding)

- **How They Work:**
  Instead of decoding a lossy file to pixels and re-compressing, a **bitstream recompressor** parses the entropy-coded syntax stream (Huffman or CABAC bins) back into exact quantized syntax symbols (e.g., quantized DCT/DST coefficients, motion vector deltas, prediction modes), models their conditional joint distribution using a much stronger context model + modern arithmetic/rANS coder, and writes a compact stream that can be inverted to reconstruct the **exact original file byte-for-byte**.
  - **Dropbox Lepton & JPEG XL JPEG Recompression:** `Shipping in products` & `Open-source implementation` ([dropbox/lepton](https://github.com/dropbox/lepton); [JPEG XL arXiv:2506.05987](https://arxiv.org/html/2506.05987v2)). Recompresses baseline JPEGs by **20%–22% bit-exact** at 5–15 MB/s by replacing Huffman coding of $8\times 8$ quantized DCT blocks with a multi-context adaptive VP8/ANS arithmetic coder conditioned on neighboring block edges `[Verified - Primary]`. Learned lossless JPEG transcoding (`Guo et al., CVPR 2022`, [CVF Paper](https://openaccess.thecvf.com/content/CVPR2022/papers/Guo_Practical_Learned_Lossless_JPEG_Recompression_With_Multi-Level_Cross-Channel_Entropy_Model_CVPR_2022_paper.pdf)) improves this by another 3.51% using a neural entropy model in the DCT domain `[Verified - Primary]`.
  - **H.264 / HEVC CABAC Binarization & Context Re-Coding:** `Research prototype with code`. In June 2026, Singh ([arXiv:2606.23753v1](https://arxiv.org/html/2606.23753v1)) proved across 2,480 bit-exact round-trip trials on quantized DCT residuals that H.264's standard `UEG` (Truncated Unary + Exp-Golomb) binarization and single-context Huffman suffer significant rate overhead compared to **Position-Indexed Canonical Huffman (`HuffmanPos`)** and **Entropy-Conserving Binarization (`ECB`)**, which code **-7.67% to -9.10% below marginal first-order entropy** on Kodak natural images by giving the arithmetic coder per-position context adaptation and exploiting spatial inter-block correlations `[Verified - Primary]`. Furthermore, H.264/HEVC CABAC uses a 64-state multiplication-free table (`RANGE_TAB_LPS`) with 1-bit state transitions that adapt slowly across non-stationary residual blocks and cannot look across multiple neighboring blocks or cross-component (Y to U/V) syntax correlations.

#### Table 2.3: Master Quantitative Comparison of All 7 Video Compression Families on Android (1080p30 Benchmark Reference)

| Codec Family & Representative | Maturity Status (Nov 2026) | Lossy BD-Rate vs. HW H.264 (VMAF) | Lossless Size (% of Raw YUV420) | Bit-Exact Recompression of Existing MP4? | Mobile Encode Speed (1080p30) | Mobile Decode Speed (1080p30) | Mobile Encode Energy (Joules / frame) | Mobile Decode Energy (Joules / frame) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **HW H.264 (`MediaCodec`)** | `Shipping in products` | 0.0% (Anchor) | N/A (rarely exposed in HW) | No | 120–240 fps | 240–480 fps | **0.006 – 0.012 J** `[Verified - Secondary]` | **0.003 – 0.006 J** `[Verified - Secondary]` |
| **HW HEVC (`MediaCodec`)** | `Shipping in products` | -32% to -38% `[Verified - Secondary]` | N/A | No | 60–180 fps | 180–360 fps | **0.008 – 0.015 J** `[Verified - Secondary]` | **0.004 – 0.008 J** `[Verified - Secondary]` |
| **HW AV1 (`MediaCodec`)** | `Shipping in products` (Mid/Flagship) | -44% to -52% `[Verified - Secondary]` | N/A | No | 60–120 fps | 120–240 fps | **0.010 – 0.018 J** `[Estimate]` | **0.005 – 0.009 J** `[Verified - Secondary]` |
| **SW `SVT-AV1` (v3.1.0, M6)** | `Open-source implementation` | -48% to -58% `[Verified - Secondary]` | ~33.5% (via `libaom` lossless) | No (expands file 5x–9x) | 4–14 fps (CPU) | 90–180 fps (`dav1d` NEON) | **0.35 – 1.10 J** `[Estimate]` | **0.012 – 0.025 J** (`dav1d`) |
| **SW `VVenC` / `VVdeC` (H.266)** | `Open-source implementation` | -55% to -64% `[Verified - Secondary]` | ~29.7% (QP=0 is near-lossless, 58 dB) | No | 0.8–3.5 fps (CPU) | 45–95 fps (`VVdeC` NEON) | **1.40 – 4.50 J** `[Estimate]` | **0.025 – 0.055 J** `[Estimate]` |
| **SW `AVM` (AV2 Spec 1.0.0)** | `Standardized` (May 2026) | -64% to -70% (-33.8% vs AV1) `[Verified - Primary]` | ~31.0% `[Estimate]` | No | 0.02–0.15 fps (CPU) | 18–40 fps (SW) | **15.0 – 60.0 J** `[Estimate]` | **0.050 – 0.110 J** `[Estimate]` |
| **SW `FFV1` (v3.4 / v4)** | `Standardized` (RFC 9043) | N/A (Lossless only) | **35.77%** (UVG) / **35.73%** (Xiph 1080p) `[Verified - Primary]` | No (expands lossy MP4 5x–9x) | 20–45 fps (CPU NEON) | 30–60 fps (CPU NEON) | **0.09 – 0.18 J** `[Estimate]` | **0.06 – 0.12 J** `[Estimate]` |
| **Samsung `APV` (RFC 9924)** | `Shipping in products` (Android 16) | Intra Mezzanine (20% < ProRes) | ~42%–48% (Intra lossless mode) `[Estimate]` | No | 60–120 fps (HW) / 35 fps (SW) | 120 fps (HW) / 60 fps (SW) | **0.012 – 0.025 J** (HW) | **0.008 – 0.018 J** (HW) |
| **Pure NVC (`DCVC-RT` / `DCVC-UF`)** | `Research prototype with code` | -65% to -74% (-42.2% vs VTM) `[Verified - Primary]` | N/A (Lossy only) | No | 6–22 fps (Flagship NPU/GPU) | 8–28 fps (Flagship NPU/GPU) | **0.18 – 0.55 J** `[Estimate]` | **0.14 – 0.42 J** `[Estimate]` |
| **Overfitted INR (`Cool-chic 5.0` / `C3`)** | `Open-source implementation` | -60% to -66% (-11% vs VVC) `[Verified - Primary]` | N/A | No | 0.05–0.3 fps (Overfit) / 15 fps (`N-O`) | **60–120 fps** (500–3k MAC/px) | **8.0 – 35.0 J** (Overfit) / **0.12 J** (`N-O`) | **0.008 – 0.018 J** `[Estimate]` |
| **Masked Diffusion (`NeuralLVC`)** | `Paper only` (ACM MM '26) | N/A (Lossless only) | **30.34%** (UVG) / **29.46%** (Xiph 1080p) `[Verified - Primary]` | No | 0.0001 fps (5.9 PMAC/frame) | 0.0001 fps (5.9 PMAC/frame) | **>10,000 J** (Unviable on mobile) | **>10,000 J** (Unviable on mobile) |
| **Prism-V (Proposed System)** | **Target 2027 Build** | **-58% to -69%** (Live to Charging Overfit) `[Estimate]` | **27.5% – 31.0%** (`Prism-PSC` raw) `[Estimate]` | **Yes: -14.5% to -23.5%** (`Prism-BBR`) `[Estimate]` | **60 fps** (Live) / **12 fps** (Charging Overfit) | **60–120 fps** (All tiers) | **0.022 – 0.045 J** (Live Capture) | **0.009 – 0.018 J** (All tiers) |

---

## 3. Inventions, Prior-Art Audit, and Core Design Decisions

### 3.1 Five Core Technical Inventions & Syntheses

Rather than assembling off-the-shelf components that either fail on mobile battery budgets (`DCVC-UF`, `NeuralLVC`) or fail on real user files (`FFV1` applied to compressed MP4s), Prism-V introduces **five tightly integrated technical inventions** synthesized from 2025–2026 frontier research. For each invention, we state its exact mathematical formulation, conduct a prior-art and patent audit, quantify expected gains/costs/risks, and define the falsification experiment that proves or kills it.

---

#### Invention 1: `Prism-BBR` — Syntax-Preserving Bitstream Recompression via Position-Indexed Context Allocation and Integer Micro-MLP rANS

- **Problem Addressed:**
  When a user wants to "losslessly compress" existing videos on their phone (or compress a newly recorded hardware `MediaCodec` video without losing a single bit of the original MP4), pixel-domain lossless codecs cannot be used because decoding lossy H.264/HEVC to YUV and re-encoding losslessly expands the file 5x–9x. Meanwhile, the original H.264/HEVC bitstream contains substantial entropy redundancy because:
  1. Hardware `MediaCodec` encoders use suboptimal real-time CABAC context initialization, fixed Truncated Unary + Exp-Golomb (`UEG`) binarization, and often emit slice padding or unoptimized bypass bins to meet hardware pipeline clock budgets.
  2. Standard H.264/HEVC CABAC uses a 64-state table-driven binary arithmetic coder (`M-coder`) that tracks only 1D marginal transitions within a single syntax element's local context, ignoring multi-symbol joint correlations across spatial neighbors, temporal co-located blocks, and cross-component (Luma $Y \to$ Chroma $U,V$) quantized coefficients.

- **Mathematical & Algorithmic Formulation:**
  Let an input H.264 or HEVC bitstream $\mathcal{B}_{\text{orig}}$ consist of container boxes $\mathcal{M}_{\text{ISOBMFF}}$ and a sequence of Network Abstraction Layer (NAL) units $\mathcal{N} = \{n_1, n_2, \dots, n_K\}$.
  1. **Exact Syntax Parsing & Verification:** The `Prism-BBR` parser decodes each slice NAL unit through the standard ITU-T H.264 (Section 9.3) or H.265 (Section 9.3) CABAC M-coder up to the **pre-dequantization syntax symbol level**, extracting for each Coding Unit / Macroblock $b$:
     $$S_b = \left\{ \text{skip\_flag}_b, \text{pred\_mode}_b, \text{mvd}_b^{(x,y)}, \text{cbf}_b^{(Y,U,V)}, \mathbf{C}_b^{(Y,U,V)} \right\}$$
     where $\mathbf{C}_b \in \mathbb{Z}^{N\times N}$ is the exact integer array of quantized transform coefficients. Crucially, to guarantee 100% bit-exact reconstruction even in the presence of non-canonical encoder quirks (e.g., non-minimal Exp-Golomb prefixes, redundant slice header bits, or RBSP trailing bits), `Prism-BBR` runs a shadow H.264/HEVC CABAC re-encoder inline during encoding; any bit $k$ where the canonical syntax re-serializer diverges from $\mathcal{B}_{\text{orig}}$ is recorded into a sparse **Syntax Discrepancy Patch Stream ($\Delta_{\text{patch}}$)** (typically $<0.02\%$ of file size).
  2. **Position-Indexed Binarization & Sub-Alphabet Partitioning:** Following the empirical mechanism proven by [Singh (June 2026, arXiv:2606.23753)](https://arxiv.org/html/2606.23753v1), each quantized coefficient $c_{i,j} \in \mathbf{C}_b$ is decomposed into:
     - Significance map bit $\sigma_{i,j} = \mathbb{I}(c_{i,j} \neq 0) \in \{0,1\}$,
     - Sign bit $s_{i,j} = \mathbb{I}(c_{i,j} < 0) \in \{0,1\}$ (for $\sigma_{i,j}=1$),
     - Magnitude $m_{i,j} = |c_{i,j}| - 1$, encoded via a 16-symbol **Position-Indexed Alphabet (`HuffmanPos` / truncated 4-bit nibble stream)** rather than a single shared context.
  3. **Deterministic Integer Micro-MLP Probability Estimator:** For each coefficient sub-group $g$ along the diagonal zig-zag scan, an integer feature vector $\mathbf{x}_{i,j} \in \mathbb{Z}_{8}^{16}$ is assembled from causal neighbors:
     $$\mathbf{x}_{i,j} = \left[ |c_{i-1,j}|, |c_{i,j-1}|, |c_{i-1,j-1}|, |c_{i,j}^{\text{above-block}}|, |c_{i,j}^{\text{left-block}}|, |c_{i,j}^{\text{colocated-t-1}}|, |c_{2i,2j}^{\text{luma-Y}}|, \text{QP}_{\text{slice}}, i, j \right]$$
     A tiny 2-layer **Integer Lookup-Table Micro-MLP** ($16 \to 32 \to 16$, weights in `INT8`, bias in `INT16`, ReLU activation, dyadic right-shift `>> 7`, totaling just **1,088 parameters = 1.1 KB**, fitting entirely inside L1 data cache) computes unnormalized log-probabilities, which are mapped via a 256-entry integer `exp2` lookup table (LUT) to a strictly positive 16-bit cumulative distribution function (CDF) $\text{CDF}[0..16]$ with $\text{CDF}[16] = 2^{15} = 32,768$.
  4. **8-Way SIMD Interleaved 32-Bit rANS Coding:** The symbols and their integer CDFs are fed into an 8-lane interleaved Range Asymmetric Numeral Systems (`rANS`) encoder:
     $$x_{\text{next}} = \left\lfloor \frac{x}{f_s} \right\rfloor \cdot 2^{15} + (x \bmod f_s) + \text{CDF}[s]$$
     where $f_s = \text{CDF}[s+1] - \text{CDF}[s] \ge 1$. Because 8 independent rANS state registers $x_0, \dots, x_7 \in \text{UINT32}$ are updated simultaneously using Arm NEON `uint32x4x2_t` instructions (and container metadata boxes `moov`/`mdat` audio tracks are compressed with Zstandard `zstd` level 5), decoding reaches **160–340 MB/s** on a single mid-range mobile core.

- **Prior-Art & Patent Audit:**
  - *Prior Art Checked:* [Dropbox Lepton (2016)](https://github.com/dropbox/lepton) and [JPEG XL (arXiv:2506.05987)](https://arxiv.org/html/2506.05987v2) perform DCT-domain lossless recompression for still JPEG images; [Guo et al. (CVPR 2022)](https://openaccess.thecvf.com/content/CVPR2022/papers/Guo_Practical_Learned_Lossless_JPEG_Recompression_With_Multi-Level_Cross-Channel_Entropy_Model_CVPR_2022_paper.pdf) apply learned cross-channel entropy coding to JPEG DCT coefficients; [Song et al. (arXiv:1709.05737)](https://arxiv.org/abs/1709.05737) apply a neural network to HEVC intra-prediction modes only; [Singh (June 2026, arXiv:2606.23753)](https://arxiv.org/html/2606.23753v1) evaluates `HuffmanPos` and `ECB` inside H.264 CABAC on Kodak images; [US20130272389A1](https://patents.google.com/patent/US20130272389A1/en) covers reduced context-coded CABAC bins in standard HEVC.
  - *What is Genuinely Novel in `Prism-BBR`:* Combining **full H.264/HEVC temporal + spatial syntax extraction** with a **shadow-encoder discrepancy patch stream ($\Delta_{\text{patch}}$)** (which guarantees 100% bit-exact SHA-256 restoration of arbitrary hardware `MediaCodec` bitstreams, including non-canonical padding/quirks), **L1-resident Integer LUT Micro-MLP conditioning across temporal co-located and luma-to-chroma coefficients**, and an **Android `openProxyFileDescriptor` virtual MP4 bridge**.
- **Expected Gains, Costs, Risks, and Falsification Test:**
  - *Expected Gain:* **14.5%–23.5% lossless size reduction** on H.264 MP4s (which often use CAVLC or poorly tuned hardware CABAC) and **9.0%–15.5% reduction** on HEVC MP4s `[Estimate]`.
  - *Cost:* ~0.015 J/frame (3–5 seconds to recompress a 100 MB video at 20–35 MB/s; <0.5 seconds to stream-decode on playback at >180 MB/s).
  - *Risk:* Hardware `MediaCodec` encoders might already compress CABAC close to the conditional entropy limit on low-bitrate HEVC streams.
  - *Prove-or-Kill Test (Week 1 Experiment):* Run the `Prism-BBR` CLI prototype on 50 unmodified H.264 and HEVC videos recorded by Samsung Galaxy, Google Pixel, and Xiaomi Redmi phones. **Kill Criterion:** If mean size reduction is $<8.0\%$ on H.264 or $<5.0\%$ on HEVC, demote `Prism-BBR` to an archival background compressor and make `Prism-PSC` the primary lossless mode.

---

#### Invention 2: `Prism-DetInt` — Cross-Silicon Deterministic Integer Neural Priors via Dyadic Arithmetic & Lookup-Table Activations

- **Problem Addressed:**
  Arithmetic and rANS entropy coders are chaotically sensitive to probability mismatches: if the encoder computes $\text{CDF}[s] = 16,384$ and the decoder on a different phone chip computes $\text{CDF}[s] = 16,385$ (a difference of $3\times 10^{-5}$), the rANS state $x$ diverges irreversibly and every subsequent pixel in the frame decodes as random static ([Ballé et al., ICLR 2019](https://openreview.net/forum?id=S1zz2i0cY7)). Standard mobile AI runtimes (`LiteRT` GPU/NPU delegates, `QNN`, `NeuroPilot`) use hardware-specific `FP16`/`BF16` fused multiply-add (FMA) trees and vendor-proprietary approximations for `exp`, `sigmoid`, and `LayerNorm`, which are **never bit-exact across vendors**.

- **Mathematical & Algorithmic Formulation:**
  We partition every neural module in Prism-V into two strict mathematical classes:
  1. **Class A (Entropy-Normative Priors — Must be 100.000% Bit-Exact Across Every CPU Architecture on Earth):**
     Any network layer whose output feeds an rANS CDF table (`Prism-BBR` Micro-MLP, `Prism-PSC` Checkerboard ConvNet, and `Prism-Lossy` Cool-chic `ARM` autoregressive prior) is defined **exclusively over the ring of bounded two's-complement integers**:
     - Weights $\mathbf{W}^{(l)} \in [-127, +127]^{M\times K}$ (`INT8`), Inputs $\mathbf{x}^{(l)} \in [-32767, +32767]^K$ (`INT16`), Biases $\mathbf{b}^{(l)} \in \text{INT32}^M$.
     - Exact integer matrix-vector accumulation:
       $$\mathbf{a}^{(l)} = \mathbf{W}^{(l)} \mathbf{x}^{(l)} + \mathbf{b}^{(l)} \in \text{INT32}^M$$
       Since integer addition is strictly associative and commutative (provided no `INT32` overflow occurs, which is guaranteed because $K \le 512$ and $512 \times 127 \times 32767 = 2.13\times 10^9 < 2^{31}-1$), the accumulator $\mathbf{a}^{(l)}$ is **mathematically identical** regardless of whether it is computed by scalar C, Armv8.2-A `vdotq_s32` (NEON), Armv9.2-A `SME2` `smopa` outer-product tiles, or x86 `AVX2` `vpdpbusd`!
     - Dyadic rescaling with deterministic round-to-nearest-even (or fixed floor):
       $$\mathbf{z}^{(l)} = \text{clip}_{[-32767, +32767]}\left( \left\lfloor \frac{\mathbf{a}^{(l)} \cdot M_{\text{scale}}^{(l)} + 2^{s_l - 1}}{2^{s_l}} \right\rfloor \right)$$
       where $M_{\text{scale}}^{(l)} \in [1, 32767]$ and $s_l \in [8, 24]$ are normative bitstream constants.
     - Non-linear activations (`ReLU`, `HardSwish`) and the final Softmax/Laplace CDF integrator are computed exclusively via **integer piecewise shifts or a normative 512-byte static Lookup Table (`CDF_EXP_LUT[256]`)** compiled into the binary.
     - **Hardware Execution Rule:** Class A layers execute **exclusively on the CPU** (using NEON `SDOT` or `SME2`). Because Class A networks are tiny (<3,000 MACs/pixel), an Armv9 CPU with `NEON`/`SME2` executes them in **<1.2 ms per 1080p frame**, completely eliminating NPU driver dispatch latency (which is often 1.5–4.0 ms per invocation!).
  2. **Class B (Post-Entropy Reconstruction & Encoder-Only RDO — Non-Normative Floating/Quantized Acceleration):**
     Once all latents $\hat{\mathbf{y}}$ have been bit-exactly decoded from the rANS bitstream, the reconstruction filter / upsampler (and any encoder-side motion search, importance map estimation, or `AFGS1` noise estimation) does **not** affect entropy synchronization. A $\pm 1$ LSB difference in final RGB/YUV display output is imperceptible. Therefore, Class B modules can freely run on the **GPU (`Vulkan Cooperative Matrix` `FP16`/`INT8`) or NPU (`LiteRT` `QNN` / `NeuroPilot`)** for maximum speed and power efficiency.

- **Prior-Art & Patent Audit:**
  - *Prior Art Checked:* [Ballé, Johnston, & Minnen (ICLR 2019, "Integer Networks for Data Compression with Latent-Variable Models")](https://openreview.net/forum?id=S1zz2i0cY7); [InterDigital CompressAI (arXiv:2011.03029)](https://arxiv.org/html/2011.03029); [Jacob et al. (CVPR 2018, Integer-Arithmetic-Only Quantization, arXiv:1712.05877)](https://arxiv.org/abs/1712.05877); JVET NNVC integer LUT proposals.
  - *What is Genuinely Novel in `Prism-DetInt`:* The strict architectural split on mobile between **CPU-bound SIMD (`NEON`/`SME2`) Class A normative integer priors** (zero dispatch latency, 100% cross-vendor bit-exactness) and **Vulkan/NPU Class B reconstruction/encoding**, plus a **bound-checked `INT32` accumulator constraint ($K \le 512$)** that mathematically proves overflow-free associativity across arbitrary SIMD vector widths (128-bit NEON vs. 512-bit SME2).
- **Prove-or-Kill Test (Week 2 Experiment):**
  - Encode 1,000 frames on a Qualcomm Snapdragon 8 Elite phone and decode on MediaTek Dimensity, Samsung Exynos, Google Tensor, Apple M-series, and x86_64 Linux. **Kill Criterion:** Any single bit mismatch ($>0$ errors across $10^9$ decoded symbols) halts release until the offending instruction is isolated.

---

#### Invention 3: `Prism-Lossy` — Hardware-Anchored Hybrid Split-Stream Codec with Two-Speed Overfitted Micro-Residuals and `AFGS1` Grain Synthesis

- **Problem Addressed:**
  How do we beat `H.266/VVC` and approach `AV2` / `DCVC-UF` compression quality on a 2027 Android phone while consuming **<0.04 Joules/frame** during live recording and decoding at **60–120 fps on a $120 low-end phone**?
  Pure software encoders (`SVT-AV1` M2, `VVenC`, `AVM`) and pure neural encoders (`DCVC-UF`) fail the mobile power/thermal budget during live capture, while standalone hardware `MediaCodec` (HEVC/AV1) suffers from 15%–30% worse compression efficiency than software presets because hardware ASICs use small motion search windows, limited RDO lookahead, and coarse quantization tables.

- **Mathematical & Algorithmic Formulation:**
  `Prism-Lossy` decomposes any target video sequence $\mathcal{V} = \{f_1, \dots, f_T\}$ into three orthogonal information streams that are multiplexed into a single backward-compatible ISOBMFF `.mp4` container:
  $$\mathcal{V} \approx \underbrace{\mathcal{U}\left( \hat{\mathcal{V}}_{\text{HW-Base}} \right)}_{\text{85%–92% of signal energy (HW ASIC)}} + \underbrace{\mathcal{D}_{\boldsymbol{\theta}_{\text{GOP}}}\left( \hat{\mathbf{L}}_t, \Psi(\hat{\mathcal{V}}_{\text{HW-Base}, t}) \right)}_{\text{Content-Adapted Micro-Residual (<600 MAC/px)}} + \underbrace{\mathcal{G}_{\text{AFGS1}}\left( \boldsymbol{\gamma}_t, \hat{\mathcal{V}}_t \right)}_{\text{Parametric High-Freq Texture/Noise (<200 B/frame)}}$$
  1. **Stage 1: Perceptual Noise/Grain Stripping & `AFGS1` Parameterization:**
     Before encoding, a fast $8\times 8$ block variance-and-autocorrelation pass on the GPU/CPU separates temporally uncorrelated sensor noise/film grain from structural edges, fitting the 14-parameter autoregressive (`AR`) model of [AOMedia AFGS1](https://aomediacodec.github.io/afgs1-spec/) ($\boldsymbol{\gamma}_t$, ~160 bytes/frame). Passing the denoised frame $f_t^{\text{clean}}$ to the hardware encoder immediately saves **12%–25% bitrate** at medium-to-high quality by preventing the hardware ASIC from spending bits on random sensor shot noise.
  2. **Stage 2: Hardware `MediaCodec` Base Layer ($\hat{\mathcal{V}}_{\text{HW-Base}}$):**
     The clean sequence $f_t^{\text{clean}}$ (either at native resolution with a +4 to +6 QP offset, or downscaled by $1.5\times$ in Ultra-Low-Bitrate mode) is fed via zero-copy `AHardwareBuffer` (`Surface` input) to the phone's hardware `MediaCodec` encoder (`c2.qti.av1.encoder` / `c2.mtk.av1.encoder` on mid/flagship; hardware `HEVC` on low-end) and immediately reconstructed via a paired hardware `MediaCodec` decoder surface to obtain the exact hardware reconstruction $\hat{f}_{t,\text{base}}$ at **<0.015 J/frame**.
  3. **Stage 3: Two-Speed Content-Adapted Micro-Residual ($\mathcal{D}_{\boldsymbol{\theta}_{\text{GOP}}}$):**
     To restore the high-frequency edges, facial details, text sharpness, and contouring lost by the hardware encoder, we compute the residual $r_t = f_t^{\text{clean}} - \mathcal{U}(\hat{f}_{t,\text{base}})$ and encode it using a **Base-Conditioned Cool-chic 5.0 Micro-Codec**:
     - The decoder $\mathcal{D}_{\boldsymbol{\theta}_{\text{GOP}}}$ takes a sparse hierarchical 2D latent pyramid $\hat{\mathbf{L}}_t = \{\hat{L}_t^{(1/2)}, \hat{L}_t^{(1/4)}, \hat{L}_t^{(1/8)}\}$ (only 3 levels instead of 7 because low frequencies are already in $\hat{f}_{t,\text{base}}$!) plus a 4-channel edge/gradient feature map $\Psi(\hat{f}_{t,\text{base}})$ extracted from the hardware base frame.
     - A tiny 3-layer `INT8` synthesis MLP (width 12 $\to$ 12 $\to$ 3, requiring only **480 to 850 MACs/pixel**) predicts the residual $\hat{r}_t$ and an adaptive deblocking/de-ringing mask applied to $\hat{f}_{t,\text{base}}$.
     - **How $\boldsymbol{\theta}_{\text{GOP}}$ and $\hat{\mathbf{L}}_t$ are Computed (The Two-Speed Lifecycle):**
       - **Speed Mode A (Live Recording / Fast Export — `N-O Cool-chic` Feed-Forward Pass):** During live camera recording or "Fast" user mode, we use a pre-trained feed-forward encoder + hypernetwork (inspired by [N-O Cool-chic, July 2026](https://arxiv.org/html/2607.13723v1) and [Hypernetwork Overfitting, Sept 2025, arXiv:2509.18748](https://arxiv.org/html/2509.18748v1)) running on the mobile NPU (`LiteRT`) in **<3.5 ms/frame** to predict the latent pyramid $\hat{\mathbf{L}}_t$ and per-GOP MLP weights $\boldsymbol{\theta}_{\text{GOP}}$ in a single forward pass without gradient descent.
       - **Speed Mode B (Quality-First / Idle Charging Optimization — `Cool-chic 5.0` Iterative Overfitting):** When the user selects "Max Quality / Smallest File" or opts into **Overnight / Charging Compression**, an Android `WorkManager` job (constrained to `setRequiresCharging(true)` and `PowerManager.THERMAL_STATUS_NONE`) runs **300–600 iterations of AdamW + cosine annealing** (per [Cool-chic 5.0, July 2026](https://arxiv.org/html/2605.02726v2)) on each 60-frame GOP to jointly optimize the latent grids $\hat{\mathbf{L}}_t$ and the 1,800 parameters of $\boldsymbol{\theta}_{\text{GOP}}$ directly against a perceptual Lagrangian loss $L = R(\hat{\mathbf{L}}_t) + R(\boldsymbol{\theta}_{\text{GOP}}) + \lambda \cdot D_{\text{MS-SSIM + L1}}(\hat{f}_t, f_t^{\text{clean}})$. Because the decoder architecture is identical in both modes, the resulting bitstream still decodes at **60–120 fps (<850 MACs/pixel)** on any phone, while gaining an extra **8%–14% BD-rate reduction**!

- **Prior-Art & Patent Audit:**
  - *Prior Art Checked:* [MPEG-5 Part 2 LCEVC](https://www.lcevc.org/) (uses fixed linear $2\times 2$ / $4\times 4$ transforms on top of a base codec; heavily patented by V-Nova); [Cool-chic 5.0 (July 2026)](https://arxiv.org/html/2605.02726v2) and [N-O Cool-chic (July 2026)](https://arxiv.org/html/2607.13723v1) (standalone overfitted/feed-forward codecs without a hardware base codec); [Neurosurgeon split computing](https://www.researchgate.net/publication/315868443_Neurosurgeon_Collaborative_Intelligence_Between_the_Cloud_and_Mobile_Edge).
  - *What is Genuinely Novel in `Prism-Lossy`:*
    1. **Patent-Safe Non-Linear Overfitted Micro-MLP Enhancement:** Unlike LCEVC's fixed linear $2\times 2$/$4\times 4$ directional transforms and bit-plane run-length coding, `Prism-Lossy` transmits per-GOP overfitted MLP weights $\boldsymbol{\theta}_{\text{GOP}}$ and rANS-coded continuous latent pyramids conditioned on the hardware base frame, operating in a completely distinct mathematical and patent domain (open-sourced by Orange Labs under BSD-3-Clause in [Cool-Chic](https://github.com/Orange-OpenSource/Cool-Chic)).
    2. **Two-Phase Live-to-Charging Bitstream Upgrade:** Because the feed-forward (`N-O`) and overfitted (`Cool-chic 5.0`) encoders target the **exact same normative 600-MAC/pixel decoder syntax**, a video recorded live at 60 fps can have its `.mp4` enhancement track (`prmr`) transparently re-optimized while the phone charges overnight without ever re-encoding the hardware base track!
- **Prove-or-Kill Test (Week 3 Experiment):**
  - Test `Prism-Lossy` (HW AV1/HEVC + 600-MAC/px Cool-chic residual + AFGS1) on `UVG` 1080p against standalone HW AV1/HEVC and `SVT-AV1` M6. **Kill Criterion:** If `Prism-Lossy` fails to achieve at least **-15% VMAF BD-rate** over standalone hardware `MediaCodec` at equal total bitrate (base + enhancement + weights), or if decoder overhead exceeds **1,500 MACs/pixel**, drop the residual layer and pivot to NPU-guided Region-of-Interest (ROI) quantization map (`VK_KHR_video_encode_quantization_map`) + `AFGS1` + post-decode NPU super-resolution.

---

#### Invention 4: `Prism-PSC` — Bijective Subband-Tokenized Temporal Lossless Codec (4-Pass Wavefront Integer ConvNet + rANS)

- **Problem Addressed:**
  For uncompressed camera capture (`Camera2` `YUV_420_888`, 10-bit `P010`, or `RAW_SENSOR`) and lossless studio editing, [NeuralLVC (Sept 2026)](https://arxiv.org/html/2604.03353v2) proved that temporal difference tokenization + learned probability estimation beats `x265 --lossless` and `FFV1` by **18%–22.5% relative**, reaching ~29.5%–30.3% of raw size on 1080p video. However, `NeuralLVC` takes **205–400 seconds per 1080p frame** on an NVIDIA GH200 GPU because it runs 94 sequential Transformer passes over 2,970 patches per frame. We need `NeuralLVC`'s compression ratio at **25,000x lower compute** (real-time 30–60 fps on mobile).

- **Mathematical & Algorithmic Formulation:**
  1. **Reversible Color & Integer Wavelet Decorrelation:**
     If input is RGB, apply Reversible Color Transform (`YCoCg-R`):
     $$Co = R - B, \quad t = B + \lfloor Co / 2 \rfloor, \quad Cg = G - t, \quad Y = t + \lfloor Cg / 2 \rfloor$$
     For each plane ($Y, U, V$ or $Y, Co, Cg$), compute the bijective temporal residual for P-frames (or spatial value for I-frames) with Lightweight Integer Block Motion Compensation ($\mathbf{v}_{b} \in [-16, +16]^2$ estimated on $16\times 16$ luma blocks using hardware NEON SAD instructions `vabdq_u8`):
     $$d_t(x,y) = f_t(x,y) - f_{t-1}(x + v_x, y + v_y)$$
     Next, apply a 1-level reversible integer **LeGall 5/3 lifting wavelet** to $d_t(x,y)$, splitting the frame into four quarter-resolution subbands $\{LL_t, LH_t, HL_t, HH_t\}$ using only integer additions and bit-shifts:
     $$H[n] = X[2n+1] - \left\lfloor \frac{X[2n] + X[2n+2]}{2} \right\rfloor, \quad L[n] = X[2n] + \left\lfloor \frac{H[n-1] + H[n] + 2}{4} \right\rfloor$$
     The coarse subband $LL_t$ (1/4 of the pixels) is encoded first using classical fast MED (Median Edge Detector) + rANS, providing an **instant full-frame structural context** for the three high-frequency subbands ($LH_t, HL_t, HH_t$, which constitute 75% of the pixels and are overwhelmingly near-zero).
  2. **4-Pass Checkerboard Spatial-Temporal Integer ConvNet (Replacing 94-Pass Transformer):**
     Instead of 94 sequential self-attention groups, each high-frequency subband is partitioned into a **2x2 spatial checkerboard** ($G_0: (\text{even}, \text{even})$, $G_1: (\text{even}, \text{odd})$, $G_2: (\text{odd}, \text{even})$, $G_3: (\text{odd}, \text{odd})$ — just **4 passes total**!).
     - Pass $G_0$ conditions on the already-decoded coarse subband $LL_t$ and the co-located previous-frame subband $\{LH_{t-1}, HL_{t-1}, HH_{t-1}\}$ (mirroring NeuralLVC's reference embedding $\text{RefEmb}(f_{t-1})$).
     - Passes $G_1, G_2, G_3$ condition on $LL_t$, $f_{t-1}$, and the immediately adjacent decoded neighbors from earlier checkerboard phases ($G_{<k}$) using a 3-layer **Depthwise-Separable `INT8` ConvNet** (kernel size $3\times 3$, 24 channels, ~110 MACs/pixel total across all 4 passes!).
     - The ConvNet outputs a quantized **Discrete Logistic Mixture / Laplace scale-and-mean parameter pair $(\mu_{x,y}, \sigma_{x,y}) \in \text{INT8}^2$** per pixel, which indexes a precomputed $64\times 64$ integer rANS CDF table.
  - **Dimensional & Complexity Verification:**
    At 1080p YUV420 ($1920\times 1080 \times 1.5 = 3.11\times 10^6$ samples/frame), 110 MACs/sample equals **0.342 GMACs/frame** (vs. 5,900,000 GMACs/frame for NeuralLVC). On an Armv9 CPU with `NEON` `SDOT` or `SME2` (~200–600 GMAC/s sustained on 4 cores), 0.342 GMACs executes in **1.8 to 4.5 ms/frame (>200 fps compute ceiling; 45–60 fps end-to-end including rANS and memory I/O)**.

- **Prove-or-Kill Test (Month 2 Experiment):**
  - Benchmark `Prism-PSC` against `FFV1` v3.4, `x265 --lossless`, and `NeuralLVC` published numbers on `UVG` 1080p and `Xiph` 1080p. **Kill Criterion:** Must achieve $\le 31.5\%$ of raw size on `UVG` 1080p (beating `FFV1` 35.77% and `x265` 36.61% by $\ge 12\%$ relative) while running at $\ge 30\text{ fps}$ on a Snapdragon 8 Gen 3 / 8 Elite CPU.

---

#### Invention 5: `Prism-VFS` — Zero-Copy `openProxyFileDescriptor` Virtual MP4 Bridge for Transparent Ecosystem Adoption

- **Problem Addressed:**
  The #1 reason novel compression formats fail in the real world is the **adoption chicken-and-egg trap**: if a user compresses their camera gallery into a custom lossless format (`.pvloss`), WhatsApp, Instagram, TikTok, CapCut, and Google Photos cannot read it unless the user manually decompresses the file back to disk first (which requires free disk space equal to the uncompressed file and adds annoying UI friction).
- **Technical Mechanism:**
  Android provides the system API [`StorageManager.openProxyFileDescriptor(int mode, ProxyFileDescriptorCallback callback, Handler handler)`](https://developer.android.com/reference/android/os/storage/StorageManager) (available since API 26 and integrated into `DocumentsProvider` / `CloudMediaProvider` via Android's FUSE daemon in `MediaProvider`).
  1. When `Prism-BBR` losslessly recompresses `VID_2027.mp4` (100 MB) into `VID_2027.pvloss` (80 MB), it stores a **Frame-Accurate Byte-Offset Index Table (`pidx` box)** mapping every 1 MB byte-range of the original `VID_2027.mp4` to the corresponding compressed chunk in `VID_2027.pvloss`.
  2. When any external app (e.g., WhatsApp, Instagram, ExoPlayer) requests `content://com.prismv.provider/video/VID_2027.mp4` via standard Android `Intent.ACTION_SEND` or `PhotoPicker` (`CloudMediaProvider`), `Prism-VFS` returns a seekable OS `ParcelFileDescriptor` reporting `onGetSize() = 100,000,000` (the exact byte length of the original MP4).
  3. As the external app calls `pread(fd, buf, count, offset)` (including non-sequential seeks to the `moov` atom at the end of the file!), `ProxyFileDescriptorCallback.onRead(offset, size, data)` decompresses only the requested 256 KB chunk in RAM using `Prism-BBR` at **160–340 MB/s** and copies the exact original MP4 bytes into `data`.
  4. **Result:** Every app on the Google Play Store can play, edit, and upload `Prism-BBR` compressed videos **with zero code changes, zero temporary disk space, and 100% bit-exact fidelity**.

---

### 3.2 Complete Architecture & Engineering Decision Matrix

Below we document every major architectural decision required by the plan, listing the real options considered, our chosen design, quantitative justification, and the specific empirical evidence that would cause us to reverse the decision.

#### Decision 1: Overall Approach per Mode & Shared Core Components

- **Options Considered:**
  1. *Pure Classical Software Stack:* Bundle `FFV1` for lossless and `SVT-AV1` / `VVenC` for lossy.
  2. *Pure End-to-End Neural Stack:* Bundle `NeuralLVC` for lossless and `DCVC-UF` for lossy.
  3. *Prism-V Unified Hybrid Architecture (Selected):* Dual-path Lossless (`Prism-BBR` bitstream transcoding + `Prism-PSC` wavelet/integer-ConvNet pixel lossless) and Hardware-Anchored Hybrid Lossy (`Prism-Lossy`: HW `MediaCodec` + Two-Speed Cool-chic Micro-Residual + `AFGS1` grain).
- **Why Selected:** Option 1 fails on battery/speed (software `VVenC`/`SVT-AV1` burns 5–10x more energy than hardware `MediaCodec`) and fails on existing MP4 lossless compression (expanding files 5x–9x). Option 2 is thermally impossible on low/mid-range phones and suffers from cross-NPU entropy failure. Option 3 leverages the 0.01 J/frame hardware ASIC for heavy lifting while using <1,000 MAC/pixel deterministic integer neural coding for the 15%–40% compression gain.
- **What the Two Modes Share (65% Code Reuse):**
  1. `libprism_rans`: 8-way NEON/SVE2/SME2 interleaved 32-bit rANS encoder/decoder.
  2. `libprism_detint`: Deterministic `INT8`/`INT16`/`INT32` dyadic neural execution engine + 512-byte `CDF_EXP_LUT`.
  3. `libprism_container`: Zero-copy ISOBMFF (`.mp4`) / `.pvloss` parser, muxer, and `openProxyFileDescriptor` virtual filesystem bridge.
  4. `libprism_thermal`: Adaptive thermal/battery governor (`PowerManager.OnThermalStatusChangedListener` + `BatteryManager`) and lock-free `AHardwareBuffer` tile ring-buffer pool.
- **Evidence That Would Change Our Mind:** If mobile NPUs in 2027 expose a standardized, IEEE/ISO bit-exact deterministic integer inference mode consuming $<0.02\text{ J/frame}$ at 100 GMACs/frame across all three tiers, we would shift `Prism-Lossy` toward an int8 `DCVC-UF` chunk-based latent codec.

#### Decision 2: User Trade-Off Control Model (UI & Rate-Distortion-Complexity Governor)

- **Options Considered:**
  1. Expose raw codec knobs (`QP`, `CRF`, `preset 0..13`, `rANS bits`, `overfit steps`).
  2. Expose a single "Small / Medium / High" quality dropdown.
  3. **2D Intent Matrix + Real-Time Pareto Estimator (Selected):**
     - **Lossless Mode:** Automatic detection of source type (with manual override):
       - *Bit-Exact Archive (`Prism-BBR`):* Shrinks existing camera MP4s 14%–24% with 100% original file recovery. User slider: **Fast (NEON rANS only, ~45 MB/s)** vs. **Max Squeeze (Integer Micro-MLP + rANS, ~20 MB/s)**.
       - *Studio Pixel-Exact (`Prism-PSC`):* For raw Camera2 capture or uncompressed video. User slider: **Live 60fps Capture** vs. **Max Archival Squeeze**.
     - **Lossy Mode:** User selects primary objective along a **4-Target Hexagon Dial** (**Target Quality [VMAF/CVVDP]**, **Target File Size [MB]**, **Max Speed**, or **Min Battery [<1.5% battery per 10-min clip]**) plus a toggle for **"Boost 12% Further While Charging"**. Before encoding starts, a 0.3-second 5-patch micro-probe estimates and displays the exact predicted file size ($\pm 6\%$), VMAF score, time required, and mAh battery cost.
- **Evidence That Would Change Our Mind:** If user testing shows >80% of users never touch the 4-target dial, simplify the default view to 3 one-tap cards (*"WhatsApp/Share Fit"*, *"Visually Lossless Archive"*, *"Extreme Saver"*) with the 4-target dial under "Pro Controls".

#### Decision 3: Programming Languages, Compilers, and Libraries

- **Options Considered:**
  1. C/C++17 across all native code + Kotlin UI.
  2. Pure Rust across all native code + Kotlin UI.
  3. **Rust (Safe Core, Parsers, Muxers, JNI) + C++20 / Hand-Written Arm Intrinsics (`KleidiAI` / NEON / SME2) for Inner Loops + Kotlin (Jetpack Compose) UI + Python 3.12 / PyTorch 2.6 / CompressAI Research Harness (Selected).**
- **Why Selected:** Media bitstream parsers (H.264/HEVC NAL units, ISOBMFF boxes) process untrusted external files and historically account for over 30% of critical Android RCE vulnerabilities (e.g., Stagefright). Writing all bitstream parsers, container muxers, thread pools, and JNI bindings in **Rust (Edition 2024, compiled via `cargo-ndk` + LLVM 19/20)** eliminates buffer overflows and use-after-free bugs at compile time. Meanwhile, Arm's `KleidiAI` micro-kernels ([Arm Developer, Oct 2025](https://developer.arm.com/community/arm-community-blogs/b/ai-blog/posts/executorch-1-0-is-here-and-with-sme2-optimizations-through-kleidiai)) and inline `aarch64` assembly for `SME2` (`smopa`) and `NEON` are matures in C++20/Clang 20, called from Rust via zero-overhead `extern "C"` FFI over aligned slice pointers.
- **Evidence That Would Change Our Mind:** Once Rust stabilizes `core::arch::aarch64` `SME2` intrinsics without nightly feature flags, migrate the remaining C++20 SIMD kernels into pure Rust.

#### Decision 4: Phone Hardware Allocation Matrix

- **Selected Hardware Mapping:**
  - **Hardware `MediaCodec` ASIC:** Base-layer encoding & decoding in `Prism-Lossy` (`AV1` on Tier 2/3; `HEVC` on Tier 1) via `AMediaCodec` NDK C API with zero-copy `ANativeWindow` / `AHardwareBuffer` surfaces.
  - **CPU Arm NEON (`vdotq_s32`) & Armv9 SME2 (`KleidiAI`):** 100% of Class A deterministic integer neural priors (`Prism-DetInt`), 8-way interleaved rANS entropy coding (`libprism_rans`), 5/3 lifting wavelet transforms, and `AFGS1` pseudo-random grain synthesis.
  - **GPU (`Vulkan 1.3/1.4` Compute + `VK_KHR_cooperative_matrix`):** Zero-copy YUV/NV12/P010 color conversion (`VK_ANDROID_external_memory_android_hardware_buffer`), spatial resampling ($1.5\times$ Lanczos-5 in 14-bit integer precision per [AV2 CTC Section 3.3](https://arxiv.org/html/2605.15800v1)), and Class B post-entropy residual upsampling/blending on Tier 1/2 devices.
  - **NPU (`LiteRT` `CompiledModel` API with Qualcomm `QNN` / MediaTek `NeuroPilot` delegates):** Used **strictly on the encoder side** for (a) live feed-forward `N-O Cool-chic` hypernetwork latent prediction (<3.5 ms/frame) and (b) background charging-time overfitting gradients. **Never** used in the normative entropy-decoding loop.
- **Evidence That Would Change Our Mind:** If Vulkan `VK_KHR_video_encode_av1` ([Khronos, Nov 2024](https://www.khronos.org/blog/khronos-announces-vulkan-video-encode-av1-encode-quantization-map-extensions)) is exposed by Android OEMs in Android 18+, switch from `AMediaCodec` to Vulkan Video queues to eliminate GL/Vulkan interop sync fences.

#### Decision 5: Machine Learning Strategy (Data, Training, Model Size, and Delivery)

- **Training Data Corpus:**
  - Licensed & permissive research datasets: `BVI-DVC` ([Fan Zhang et al.](https://fan-aaron-zhang.github.io/BVI-DVC/)), `BVI-AOM` ([Nawała et al., 2024](https://arxiv.org/html/2604.03353v2)), `Vimeo-90k` (58K 7-frame clips), `YouTube-UGC` (user-generated mobile clips), plus a custom **5,000-clip `Prism-Camera2-RAW` dataset** captured directly from 15 Android phones (low-light, high-motion, 10-bit HDR, screen recordings, documents/whiteboards).
- **Model Sizes & Delivery:**
  - **Normative Decoder & Lossless Models (Bundled inside APK `assets/`):**
    - `Prism-BBR` Integer Micro-MLP tables (H.264 + HEVC): **42 KB** total.
    - `Prism-PSC` 4-Pass Checkerboard Integer ConvNet + CDF tables: **185 KB** total.
    - `Prism-Lossy` Default Cool-chic `ARM` + Upsampler prior templates: **64 KB** total.
    - *Total Bundled ML Footprint in APK:* **291 KB** (zero network download required to encode or decode!).
  - **Encoder-Only Feed-Forward Hypernetwork (`N-O Cool-chic` predictor, ~4.8 MB `INT8` `.tflite`):** Bundled in the base APK or updated OTA via Google Play for On-Device AI (`Play Asset Delivery` install-time pack).

#### Decision 6: File Format and Container Design

- **Selected Dual-Format Design:**
  1. **Lossy Mode (`.mp4` — Backward-Compatible Hybrid ISOBMFF):**
     - Standard ISO Base Media File Format (`ftyp = isom / av01 / hevc / prsm`).
     - **Track 1 (`vide`):** Standard compliant hardware-decodable `AV1` (`av01`) or `HEVC` (`hvc1`) base stream. Any legacy player (iOS QuickTime, Windows Media Player, web browsers, WhatsApp) opens the `.mp4` and plays Track 1 normally at 85%–92% quality!
     - **Track 2 (`meta` / `prmr` auxiliary track, `handler_type = 'prsm'`):** Contains the time-aligned `Prism-Lossy` enhancement samples (quantized per-GOP micro-MLP weights $\boldsymbol{\theta}_{\text{GOP}}$ + rANS-coded latent residual pyramids $\hat{\mathbf{L}}_t$) plus `afgs` ([AFGS1](https://aomediacodec.github.io/afgs1-spec/)) film grain parameter boxes. A Prism-V enabled player reads Track 1 + Track 2 simultaneously and renders the full 100% enhanced video.
  2. **Lossless Mode (`.pvloss` — Self-Indexed ISOBMFF Container with Virtual `.mp4` Proxy):**
     - Uses ISOBMFF box structure (`ftyp = pvls`) containing:
       - `pvhd` (Prism Lossless Header: sub-mode `BBR` vs `PSC`, original SHA-256 hash, original byte length, dimensions, bit depth, color space),
       - `pidx` (Seekable Chunk Offset Index mapping 1 MB virtual MP4 ranges or 15-frame GOPs),
       - `pdat` (8-way rANS compressed syntax/wavelet payload + $\Delta_{\text{patch}}$ discrepancy stream),
       - `pcrc` (Per-chunk 32-bit `CRC32C` + full-file 256-bit `BLAKE3` cryptographic integrity tag).

---

## 4. The Full Implementation Plan

### 4.1 System Architecture & End-to-End Dataflow

The diagram below illustrates the complete end-to-end architecture across both modes, showing how data flows from either existing storage files or the live `Camera2` hardware pipeline through the shared deterministic execution core.

```mermaid
flowchart TD
    subgraph Inputs["Input Sources on Android"]
        F_MP4["Existing Compressed File<br/>(H.264 / HEVC / JPEG in MediaStore)"]
        CAM["Live Camera2 API / Raw Import<br/>(YUV_420_888 / P010 / RAW10)"]
    end

    subgraph Router["Mode & Policy Router (Rust Core)"]
        DECIDE{"User Mode Selection"}
        LOSSLESS_ROUTING{"Inspect Source Format"}
        LOSSY_ROUTING{"Thermal / Battery Governor"}
    end

    subgraph Lossless_Engine["Mode 1: Lossless Engine (Bit-Exact)"]
        BBR_PARSE["Prism-BBR Syntax Parser<br/>(Extract CABAC Coeffs, MVD, Modes)"]
        BBR_PATCH["Shadow CABAC Verifier<br/>(Emit Discrepancy Stream Δ_patch)"]
        BBR_MLP["Position-Indexed Contexts (HuffmanPos)<br/>+ 1.1 KB INT8 LUT Micro-MLP"]
        
        PSC_WAVE["Prism-PSC YCoCg-R + LeGall 5/3<br/>Integer Lifting Wavelet + Integer MC"]
        PSC_CONV["4-Pass Checkerboard Causal<br/>INT8 ConvNet (0.34 GMAC/frame)"]
    end

    subgraph Lossy_Engine["Mode 2: Hybrid Lossy Engine (Prism-Lossy)"]
        AFGS_EST["Stage 1: Sensor Noise Separation<br/>& AFGS1 AR Parameter Fit (<200 B/fr)"]
        HW_BASE["Stage 2: Hardware MediaCodec ASIC<br/>(AV1 / HEVC Base Encode + Decode)"]
        RES_COMP["Compute Clean Residual<br/>r_t = f_clean - Upsample(f_base)"]
        NO_COOL["Live Mode: N-O Cool-chic<br/>Feed-Forward NPU Pass (<3.5 ms)"]
        OVERFIT["Charging Mode: Cool-chic 5.0<br/>300-600 Step GOP Overfit (WorkManager)"]
    end

    subgraph Shared_Core["Shared Deterministic Core (Rust + Arm NEON/SME2)"]
        DETINT["Prism-DetInt Engine<br/>(INT8/INT16/INT32 Dyadic + 512B LUT)"]
        RANS["libprism_rans<br/>(8-Way SIMD Interleaved 32-bit rANS)"]
        MUX["ISOBMFF / .pvloss Muxer<br/>+ BLAKE3 / CRC32C Integrity"]
    end

    subgraph Outputs["Deliverables & Transparent OS Integration"]
        OUT_PVLOSS[".pvloss File + Prism-VFS<br/>(openProxyFileDescriptor >180 MB/s .mp4)"]
        OUT_MP4["Backward-Compatible .mp4<br/>(Track 1: HW AV1/HEVC + Track 2: prmr + afgs)"]
    end

    F_MP4 --> DECIDE
    CAM --> DECIDE
    DECIDE -->|Lossless Mode| LOSSLESS_ROUTING
    DECIDE -->|Lossy Mode| LOSSY_ROUTING

    LOSSLESS_ROUTING -->|Already Compressed H.264/HEVC| BBR_PARSE
    BBR_PARSE --> BBR_PATCH --> BBR_MLP --> DETINT
    LOSSLESS_ROUTING -->|Uncompressed YUV / P010 / RAW| PSC_WAVE
    PSC_WAVE --> PSC_CONV --> DETINT

    LOSSY_ROUTING --> AFGS_EST --> HW_BASE --> RES_COMP
    RES_COMP -->|Live / Fast| NO_COOL --> DETINT
    RES_COMP -->|Idle + Charging| OVERFIT --> DETINT

    DETINT --> RANS --> MUX
    MUX -->|Lossless| OUT_PVLOSS
    MUX -->|Lossy| OUT_MP4
```

### 4.2 Detailed Algorithms and Step-by-Step Mathematical Pipelines

#### 4.2.1 Algorithm 1: `Prism-DetInt` Cross-Platform Deterministic Integer Layer & CDF Generation

To guarantee that any two Android devices (regardless of CPU microarchitecture, OS version, or compiler optimization level) compute the exact same cumulative distribution function $\text{CDF}[0..M]$ at every symbol step:

1. **Quantized Linear/Conv Operator:** Given input activation vector $\mathbf{x} \in [-32767, +32767]^K$ (`int16_t`) and weight matrix $\mathbf{W} \in [-127, +127]^{M\times K}$ (`int8_t`), with channel count $K \le 512$:
   - Promote $\mathbf{x}$ or split $\mathbf{x}$ into high/low `int8_t` bytes: $\mathbf{x} = 256 \cdot \mathbf{x}_{\text{hi}} + \mathbf{x}_{\text{lo}}$ where $\mathbf{x}_{\text{hi}} \in [-128, 127]^K$ and $\mathbf{x}_{\text{lo}} \in [0, 255]^K$.
   - Compute two `INT8` dot-products using Armv8.2-A `vdotq_s32` (or Armv9 `SME2` `smopa`):
     $$\mathbf{a}_m = 256 \sum_{k=0}^{K-1} W_{m,k} x_{\text{hi}, k} + \sum_{k=0}^{K-1} W_{m,k} x_{\text{lo}, k} + b_m \quad \in \text{int32\_t}$$
   - Apply normative dyadic rescaling with bit-shift $s \in [8, 24]$:
     $$y_m = \text{clamp}_{[-32767, +32767]}\left( (\mathbf{a}_m \cdot M_{\text{mult}, m} + (1 \ll (s-1))) \gg s \right)$$
2. **Normative Laplace/Logistic Integer CDF Construction:**
   - The final network layer outputs integer mean $\hat{\mu} \in [-4096, +4096]$ (in $1/16$-pixel units) and log-scale index $\hat{\sigma}_{\text{idx}} \in [0, 63]$.
   - For each candidate symbol bin boundary $v_k$ ($k = 0, \dots, M$), compute the standardized integer distance:
     $$d_k = \text{clamp}_{[-255, +255]}\left( \frac{(16 \cdot v_k - \hat{\mu}) \cdot \text{INV\_SCALE\_LUT}[\hat{\sigma}_{\text{idx}}] + 128}{256} \right)$$
   - Lookup the unnormalized sigmoid/Laplace cumulative mass from the normative 512-byte table `CDF_SIGMOID_LUT[d_k + 255] \in [1, 32767]` and enforce strict monotonicity with minimum frequency floor $f_k \ge 1$:
     $$\text{CDF}[k] = \text{CDF\_SIGMOID\_LUT}[d_k + 255] + k, \quad \text{with } \text{CDF}[0] = 0, \; \text{CDF}[M] = 32768 + M$$
   - Rescale to exact power-of-two total mass $2^{15} = 32,768$ via fixed-point integer division so rANS decoding uses a single bit-shift `>> 15` instead of hardware integer division.

#### 4.2.2 Algorithm 2: Memory-Bounded Tile Ring-Buffer Pipeline (<64 MB Peak RSS on Low-End Phones)

A major cause of Android background task crashes (`SIGKILL` by `lmkd`) is allocating multiple full-resolution 4K uncompressed frame buffers in RAM (a single 4K 10-bit YUV420 frame is 24.88 MB; buffering 16 frames consumes 400 MB).
Prism-V enforces a strict **Zero-Full-Frame-Heap Policy**:

- All spatial-temporal processing in `Prism-PSC` and `Prism-Lossy` operates on **$256\times 256$ Luma ($128\times 128$ Chroma) spatial tiles** with a 16-pixel halo border.
- A pre-allocated, page-aligned (`posix_memalign(..., 4096, ...)`) **Ring Buffer of 12 Tile Slots** consumes only **$12 \times 192\text{ KB} = 2.30\text{ MB}$** of L2/L3 cache-friendly RAM.
- Hardware `MediaCodec` frames are imported via `AImageReader_acquireNextImageAsync` backed by `AHardwareBuffer` (`AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN`), mapped tile-by-tile without ever copying a full 4K frame into the Java/Kotlin heap (`ByteBuffer`).
- Total native + JVM memory footprint is strictly capped at **$\le 48\text{ MB}$ for 1080p** and **$\le 64\text{ MB}$ for 4K** on Tier 1 (low-end) devices.

---

### 4.3 Container and Bitstream Specification

#### 4.3.1 Lossy Hybrid Container (`.mp4` with `prsm` Extension Boxes)

Every `Prism-Lossy` file is a valid ISO/IEC 14496-12 (ISOBMFF) `.mp4` file:

```
[ftyp] major_brand = 'mp42', compatible_brands = ['isom', 'mp42', 'av01', 'hvc1', 'prsm']
[moov] Movie Header Box
 ├── [trak] Track 1 (Standard Video Track - Playable by ALL Legacy Players)
 │    └── [mdia] -> [minf] -> [stbl] -> [stsd] -> [av01 / hvc1] (Hardware Base Stream)
 │         └── [afgs] AOMedia Film Grain Synthesis 1 Parameter Box (Optional In-Band/Box)
 └── [trak] Track 2 (Prism-V Enhancement Track, handler_type = 'prsm')
      └── [mdia] -> [minf] -> [stbl] -> [stsd] -> [prmr] (Prism Micro-Residual Configuration Box)
           ├── uint8_t  prism_version = 1
           ├── uint8_t  profile_idc   = 0x01 (Main10 Hybrid)
           ├── uint16_t max_macs_per_pixel (e.g., 600)
           ├── uint8_t  latent_pyramid_levels (e.g., 3)
           ├── uint8_t  mlp_hidden_dim (e.g., 12)
           └── [stts / stsc / stsz / stco] Sample Tables (1 sample per GOP header + 1 per frame)
[mdat] Interleaved Base Samples (Track 1) + Enhancement rANS Packets (Track 2)
```

#### 4.3.2 Lossless Container (`.pvloss` Binary Specification)

Every `Prism-BBR` and `Prism-PSC` file uses the `.pvloss` container format:

| Offset (Bytes) | Field Name | Type | Description |
| :--- | :--- | :--- | :--- |
| `0x00 – 0x07` | `magic_header` | `char[8]` | ASCII `"PRISMV01"` (`0x50 0x52 0x49 0x53 0x4D 0x56 0x30 0x31`) |
| `0x08` | `lossless_mode` | `uint8_t` | `0x01` = `Prism-BBR` (Bitstream Exact), `0x02` = `Prism-PSC` (Pixel Exact) |
| `0x09` | `source_codec_id` | `uint8_t` | `0x01` = H.264, `0x02` = HEVC, `0x03` = Raw YUV420_8, `0x04` = Raw P010_10, `0x05` = Bayer RAW10 |
| `0x0A – 0x0B` | `flags` | `uint16_t` | Bit 0: `has_audio_zstd`, Bit 1: `has_discrepancy_patch`, Bit 2: `seekable_1mb_blocks` |
| `0x0C – 0x13` | `orig_file_size` | `uint64_t` | Exact byte length of original file (used by `Prism-VFS` `onGetSize()`) |
| `0x14 – 0x33` | `orig_sha256` | `uint8_t[32]` | Full 256-bit SHA-256 digest of original file or raw pixel stream |
| `0x34 – 0x3F` | `video_geometry` | `uint16_t[6]` | `width`, `height`, `fps_num`, `fps_den`, `bit_depth`, `chroma_format` |
| `0x40 – 0x47` | `index_offset` | `uint64_t` | Byte offset to the `pidx` Random-Access Seek Table |
| `0x48 – ...` | `chunk_payloads` | `byte[]` | Independent 1 MB (BBR) or 15-frame GOP (PSC) 8-way rANS packets, each suffixed with `uint32_t crc32c` |

---

### 4.4 How to Measure Success: Metrics, Test Datasets, Devices, and Composite "Best" Score

#### 4.4.1 Test Video Datasets (Stratified Across 5 Real-World Mobile Categories)

1. **Standardized Academic Benchmarks (for Peer-Reviewed Comparability):**
   - `UVG` (Ultra Video Group, 7 sequences at 1080p and 4K, 50/120 fps, YUV420 8/10-bit) ([Mercat et al., 2020](https://arxiv.org/html/2604.03353v2)).
   - `JVET CTC Class A1/A2 (4K) & Class B (1080p)` and `AV2 CTC` test sequences ([arXiv:2605.15800](https://arxiv.org/html/2605.15800v1)).
   - `MCL-JCV` (30 sequences, 1080p) and `BVI-AOM` (10 sequences, $1920\times 1088$) ([NeuralLVC Setup](https://arxiv.org/html/2604.03353v2)).
2. **Mobile-Specific Real-World Suite (`Prism-Mobile-Bench-50`, 50 sequences, 10 seconds each):**
   - *Category A (15 clips):* Handheld smartphone camera recordings (4K60 HDR10 P010 and 1080p30 NV12, day + night high-ISO sensor noise).
   - *Category B (10 clips):* Social media / messaging re-encodes (720p/1080p H.264/HEVC files from WhatsApp, Instagram, TikTok—critical for `Prism-BBR`).
   - *Category C (10 clips):* Mobile screen recordings & UI scrolling (`60/120 fps` sharp text, UI animations, mobile gaming).
   - *Category D (10 clips):* Talking-head video calls & lectures (static background, facial micro-expressions).
   - *Category E (5 clips):* Medical/scientific/inspection video (endoscopy, document scans, high-motion sports).

#### 4.4.2 Reference Device Matrix (6 Physical Phones in Automated Thermal/Power Rig)

Every commit in CI is benchmarked on a hardware-in-the-loop (HIL) rack instrumented with a **Monsoon High Voltage Power Monitor** (sampling battery rail current at 5 kHz via dummy battery interposer) and Android `BatteryStatsManager` / `PowerProfile` telemetry inside a $25^\circ\text{C} \pm 0.5^\circ\text{C}$ temperature-controlled chamber:

1. **Tier 1A (Ultra-Low-End Legacy):** Xiaomi Redmi Note 12 / 13 4G (Snapdragon 685 / Helio G99, Cortex-A78/A76 + A55, 4 GB RAM).
2. **Tier 1B (Entry 5G):** Samsung Galaxy A16 5G / Moto G 5G (Dimensity 6300 / Exynos 1330, 4–6 GB RAM).
3. **Tier 2A (Mainstream Mid-Range):** Google Pixel 9a / 10a (Tensor G4/G5, 8 GB RAM, HW AV1 encode/decode).
4. **Tier 2B (Performance Mid-Range):** Poco X7 Pro / Redmi Turbo 4 (Dimensity 8400, Armv9.2 all-big-core Cortex-A725, 12 GB RAM).
5. **Tier 3A (Flagship Qualcomm):** Xiaomi 16 / OnePlus 14 / Galaxy S26 Ultra ([Snapdragon 8 Elite Gen 5](https://www.qualcomm.com/content/dam/qcomm-martech/dm-assets/documents/Snapdragon-8-Elite-Gen-5-product-brief.pdf), Oryon Gen 2 + Hexagon NPU + HW APV/AV1, 16 GB RAM).
6. **Tier 3B (Flagship MediaTek):** Vivo X300 Pro / Oppo Find X9 Pro ([MediaTek Dimensity 9500](https://www.mediatek.com/products/smartphones/mediatek-dimensity-9500), Cortex-X925/X930 with Arm `SME2` + NPU 990, 16 GB RAM).

#### 4.4.3 Mathematical Definition of the Composite "Best" Score ($S_{\text{Prism}}$)

To avoid single-metric gaming (e.g., optimizing PSNR via blurring or achieving 5% better compression at 50x higher battery drain), we define two formal, quantitative scoring functions:

1. **Lossless Composite Score ($S_{\text{Lossless}}$, higher is better; baseline `FFV1` = 1.000):**
   First, any codec that fails **Bit-Exact Verification** ($\text{SHA256}(\text{Decode}(\text{Encode}(X))) \neq \text{SHA256}(X)$) receives $S_{\text{Lossless}} = 0$. For verified bit-exact codecs:
   $$S_{\text{Lossless}} = \left( \frac{\text{CR}_{\text{codec}}}{\text{CR}_{\text{baseline}}} \right)^{0.65} \cdot \left( \frac{E_{\text{enc, baseline}} + E_{\text{dec, baseline}}}{E_{\text{enc, codec}} + E_{\text{dec, codec}}} \right)^{0.20} \cdot \left( \frac{\text{FPS}_{\text{dec, codec}}}{\text{FPS}_{\text{dec, baseline}}} \right)^{0.15} \cdot \mathbb{I}\left( \text{RAM}_{\text{peak}} \le M_{\text{tier}} \right)$$
   where $\text{CR} = \frac{\text{Size}_{\text{orig}}}{\text{Size}_{\text{compressed}}}$ is the compression ratio, $E_{\text{enc}}, E_{\text{dec}}$ are measured in Joules/frame, and $M_{\text{tier}}$ is the tier memory cap ($64\text{ MB}$ on Tier 1).

2. **Lossy Composite Score ($S_{\text{Lossy}}$, higher is better; baseline Hardware `MediaCodec` HEVC = 1.000):**
   We measure perceptual quality using a **Tri-Metric Perceptual Consensus** to prevent neural metric overfitting:
   - **VMAF-NEG (No Enhancement Gain, `vmaf v3.1.0`)**: Prevents artificial sharpening/contrast hacking from inflating VMAF ([AV2 CTC arXiv:2605.15800](https://arxiv.org/html/2605.15800v1)).
   - **ColorVideoVDP (`CVVDP` v1.2)**: Psychophysical spatio-temporal contrast sensitivity metric accounting for mobile OLED display viewing distance and HDR/SDR color planes ([GitHub gfxdisp/ColorVideoVDP](https://github.com/gfxdisp/colorvideovdp)).
   - **CAMBI (Contrast Aware Multiscale Banding Index)**: Penalizes sky/gradient banding artifacts ([AV2 CTC arXiv:2605.15800](https://arxiv.org/html/2605.15800v1)).
   Let $\Delta R_{\text{consensus}} = 0.50 \cdot \text{BD-Rate}_{\text{VMAF-NEG}} + 0.40 \cdot \text{BD-Rate}_{\text{CVVDP}} + 0.10 \cdot \text{BD-Rate}_{\text{XPSNR}}$ (negative means bitrate savings, e.g., $-0.35$ for 35% savings). Given user preference weight vector $\mathbf{w} = (w_Q, w_E, w_S)$ selected on the UI dial ($\sum w_i = 1$):
   $$S_{\text{Lossy}}(\mathbf{w}) = \left( \frac{1}{1 + \Delta R_{\text{consensus}}} \right)^{w_Q} \cdot \left( \frac{E_{\text{base}}}{E_{\text{codec}}} \right)^{w_E} \cdot \left( \frac{\min(\text{FPS}_{\text{codec}}, 60)}{\min(\text{FPS}_{\text{base}}, 60)} \right)^{w_S} \cdot \mathbb{I}(\text{CAMBI} < 2.5)$$

---

### 4.5 Repository Organization, Modules, and Release Path

#### 4.5.1 Monorepo Structure (`prism-v-monorepo`)

The project will be organized as a Bazel / Cargo + Gradle hybrid monorepo:

```
prism-v/
├── Cargo.toml                          # Rust workspace root (Edition 2024)
├── crates/
│   ├── prism-core/                     # Common traits, tile ring-buffer, error types, thermal governor
│   ├── prism-rans/                     # 8-way interleaved 32-bit rANS + HuffmanPos/ECB context allocators
│   ├── prism-detint/                   # Deterministic INT8/INT16/INT32 neural layer engine + 512B CDF LUT
│   ├── prism-bbr/                      # Mode L-BBR: H.264/HEVC syntax parser, shadow CABAC verifier, Δ_patch
│   ├── prism-psc/                      # Mode L-PSC: YCoCg-R, LeGall 5/3 wavelet, 4-pass Checkerboard ConvNet
│   ├── prism-lossy/                    # Mode Y-Hybrid: Cool-chic 5.0 / N-O residual codec + AFGS1 grain engine
│   ├── prism-container/                # ISOBMFF (.mp4) dual-track muxer/demuxer + .pvloss seekable format
│   └── prism-jni/                      # Zero-copy JNI bindings to Android AHardwareBuffer, AMediaCodec, ProxyFD
├── native-simd/                        # C++20 / Clang 20 + Arm Assembly micro-kernels (CMake / cargo-build)
│   ├── arm_neon_rans8.S                # Hand-tuned 8-lane NEON rANS renormalization loop
│   ├── arm_sme2_detint.cpp             # KleidiAI + Armv9 SME2 smopa INT8 outer-product kernels
│   └── vulkan_shaders/                 # SPIR-V compute shaders (Lanczos-5, YUV<->RGB, VK_KHR_cooperative_matrix)
├── research-python/                    # PyTorch 2.6 + CompressAI training, QAT, and bit-exact export harness
│   ├── train_bbr_mlp.py                # Trains 1.1 KB CABAC syntax Micro-MLP with INT8 QAT
│   ├── train_psc_convnet.py            # Trains 4-pass Checkerboard Wavelet ConvNet on BVI-DVC + Camera2-RAW
│   ├── train_no_coolchic.py            # Trains N-O Cool-chic feed-forward hypernetwork for live capture
│   └── export_normative_c_tables.py    # Exports bit-exact INT8 weights + verifies 0-bit-flip parity vs Rust
└── android-app/                        # Android Gradle Project (Kotlin 2.1, Jetpack Compose, MinSDK 29, TargetSDK 36)
    ├── app/                            # Main consumer APK (UI, Camera2 recorder, Gallery compressor, Benchmark UI)
    ├── prism-sdk/                      # Embeddable AAR library (Media3 ExoPlayer Renderer + Transformer plugin)
    └── prism-vfs-service/              # StorageManager.openProxyFileDescriptor + CloudMediaProvider service
```

#### 4.5.2 APK Size Budget and Release Path

- **Native Shared Library (`libprism_jni.so`, `arm64-v8a`):**
  - Rust + C++20 SIMD kernels + Vulkan SPIR-V shaders: **2.8 MB** (with LTO and `-Oz` on non-hot paths, `-O3` on SIMD loops).
  - Normative embedded ML weights (`Prism-BBR` + `Prism-PSC` + `Prism-Lossy` priors): **0.29 MB**.
  - Kotlin/Compose UI + Media3 integration: **3.4 MB**.
  - `N-O Cool-chic` Encoder Hypernetwork (`.tflite` INT8): **4.8 MB**.
  - **Total Universal `arm64-v8a` APK Download Size:** **~11.3 MB**.
- **3-Channel Distribution Strategy:**
  1. **Standalone Consumer App ("Prism Video Vault & Pro Camera"):** Ships on Google Play Store and GitHub Releases; compresses existing gallery videos losslessly (`Prism-BBR`, freeing 15%–24% of phone storage with transparent `Prism-VFS` playback in all apps) or lossily (`Prism-Lossy`, shrinking videos by 50%–75% at visually lossless quality).
  2. **Open-Source Decoder & Media3 Plugin (`io.prismv:media3-decoder-prism` on Maven Central, BSD-3-Clause License):** Allows any third-party Android app using Jetpack `Media3` / `ExoPlayer` to decode `.pvloss` and `Prism-Lossy` `.mp4` enhancement tracks with a 1-line dependency addition.
  3. **FFmpeg & WebAssembly (`wasm-simd128`) Patch Set:** Upstream `libavcodec` decoder patch + `<prism-video>` WebComponent (under 150 KB WASM) so desktop browsers and servers can decode `Prism-V` streams natively.

---

### 4.6 Team, 18-Month Schedule, Cost Breakdown, and Milestone Go/No-Go Gates

#### 4.6.1 Team Structure (8 Full-Time Equivalents + External Patent Counsel)

1. **Principal Video Codec Architect / Tech Lead (1 FTE):** Owns overall bitstream spec, rate-distortion optimization, and hybrid `MediaCodec` + `Cool-chic` integration.
2. **Staff ML Compression Scientists (2 FTEs):** Own `Prism-BBR` Micro-MLP, `Prism-PSC` Checkerboard ConvNet, `N-O Cool-chic` hypernetwork training, and `INT8`/`INT16` QAT in PyTorch/CompressAI.
3. **Senior Arm SIMD & GPU Kernel Engineers (2 FTEs):** Own `libprism_rans`, `libprism_detint`, Arm NEON / SVE2 / SME2 (`KleidiAI`) assembly, and Vulkan 1.4 `VK_KHR_cooperative_matrix` shaders.
4. **Senior Android Systems & Media Framework Engineers (2 FTEs):** Own `AMediaCodec` zero-copy `AHardwareBuffer` pipeline, `Camera2` integration, `StorageManager.openProxyFileDescriptor` (`Prism-VFS`), thermal governor, and Jetpack Compose UI.
5. **QA, Automated HIL Power/Thermal Benchmark & DevOps Engineer (1 FTE):** Owns the 6-phone Monsoon power-metered CI rack, fuzzing (`cargo-fuzz` / `libFuzzer`), and perceptual metric pipeline (`VMAF-NEG`, `CVVDP`, `CAMBI`).

#### 4.6.2 18-Month Phased Schedule and Quantitative Go/No-Go Gates

| Phase & Timeline | Core Deliverables | Quantitative Go/No-Go Gate Test (Must Pass to Unlock Next Phase) |
| :--- | :--- | :--- |
| **Phase 0: Proof-of-Concept Spike** *(Months 1–2, Day 1 to Day 60)* | CLI prototypes of `Prism-BBR`, `Prism-DetInt`, `Prism-PSC`, and `Prism-Lossy` evaluated on `UVG` 1080p and 50 Android camera MP4s. | **Gate 0:** (1) `Prism-BBR` achieves $\ge 14\%$ lossless savings on H.264 and $\ge 9\%$ on HEVC; (2) `Prism-DetInt` shows 0 bit-flips across 5 SoC vendors; (3) `Prism-Lossy` achieves $\ge 15\%$ VMAF-NEG BD-rate over HW `MediaCodec` at $\le 1,200\text{ MAC/px}$. |
| **Phase 1: Core Rust + SIMD Engine & Normative Bitstream Freeze** *(Months 3–6)* | Production `prism-rans` (8-way NEON/SME2), `prism-detint`, `prism-bbr`, and `prism-psc` crates; freeze `.pvloss` v1.0 bitstream spec; 100M-iteration fuzzing pass. | **Gate 1:** (1) `Prism-BBR` decodes at $\ge 160\text{ MB/s}$ on Tier 2 CPU (<0.8 W); (2) `Prism-PSC` compresses `UVG` 1080p to $\le 31.0\%$ of raw size at $\ge 30\text{ fps}$ on Tier 3 and $\ge 15\text{ fps}$ on Tier 2; (3) Peak RAM $\le 64\text{ MB}$ on Tier 1. |
| **Phase 2: Hybrid Lossy Engine & NPU/Vulkan Acceleration** *(Months 7–11)* | Complete `prism-lossy` (`AMediaCodec` zero-copy + `N-O Cool-chic` live NPU pass + `Cool-chic 5.0` charging overfit + `AFGS1` grain); freeze `prmr` ISOBMFF box spec. | **Gate 2:** (1) Live 1080p60 / 4K30 capture runs with 0 dropped frames and $\le 2.4\text{ W}$ total power, achieving $\ge 15\%$ BD-rate over HW base; (2) Charging overfit achieves $\ge 25\%$ BD-rate over HW AV1 and $\ge 35\%$ over HW HEVC; (3) Hybrid decode reaches $\ge 60\text{ fps}$ on Tier 1 (`Redmi Note 13`). |
| **Phase 3: Android App, `Prism-VFS` Proxy, & Media3 SDK** *(Months 12–15)* | Full Android app (Gallery Vault, Camera2 Recorder, Charging Optimizer), `Prism-VFS` zero-copy `.mp4` proxy, and `media3-decoder-prism` AAR library. | **Gate 3:** (1) WhatsApp, Instagram, CapCut, and Google Photos open and upload 100 `.pvloss` virtual `.mp4` files with 0 crashes and <80 ms time-to-first-frame; (2) 72-hour continuous stress test on all 6 HIL phones with 0 OOMs or thermal shutdowns. |
| **Phase 4: Subjective Blind Testing, Security Audit, & Public Launch** *(Months 16–18)* | ITU-R BT.500 double-stimulus subjective viewing test (60 human raters), external security/FTO audit, Play Store release, and Maven/GitHub SDK launch. | **Gate 4 (Launch Readiness):** Subjective DMOS confirms objective `CVVDP`/`VMAF-NEG` gains within $\pm 3\%$; zero unresolved high/critical security vulnerabilities; FTO sign-off completed. |

#### 4.6.3 18-Month Budget Estimate

| Cost Category | 18-Month Cost Range (USD) | Assumptions & Breakdown |
| :--- | :--- | :--- |
| **Engineering & Research Compensation (8 FTEs)** | **$2,160,000 – $2,750,000** | Blended global engineering rate (Senior/Staff US/EU/India hybrid team: ~$180k–$230k/yr fully loaded per FTE $\times$ 1.5 years). |
| **GPU Training Cluster & Cloud Compute** | **$85,000 – $135,000** | 2x 8-GPU `NVIDIA H100/B200` nodes reserved for 6 months of `Prism-PSC`, `Prism-BBR`, and `N-O Cool-chic` hypernetwork training + dataset curation. |
| **Mobile HIL Test Lab & Power Instrumentation** | **$35,000 – $50,000** | 24 Android devices across Tiers 1–3, 4x Monsoon High Voltage Power Monitors, thermal cameras (`FLIR`), and Peltier environmental test chambers. |
| **Patent Freedom-to-Operate (FTO) & Defensive Filings** | **$110,000 – $175,000** | External IP counsel FTO clearance on CABAC transcoding, rANS, and hybrid residual coding + filing 5 defensive provisional/PCT applications (pledged royalty-free under AOMedia-style defensive license). |
| **Subjective User Study & Security Audit** | **$45,000 – $70,000** | Third-party Rust/C++ parser security audit + 60-participant ITU-R BT.500 subjective quality study. |
| **Total 18-Month Program Budget** | **$2,435,000 – $3,180,000** | `[Estimate]` (Can be reduced to **$950k–$1.35M** if staffed Primarily in Bengaluru/Hyderabad/Eastern Europe). |

---

### 4.7 Legal, Licensing, and Patent Strategy

Video compression is one of the most heavily litigated patent landscapes in computing ([Lumenci Codec Royalty Risk Report, Oct 2026](https://lumenci.com/blogs/hevc-vs-av1-vs-vvc-royalties-licensing-risk/); [Access Advance HEVC/VVC Pricing through 2030](https://accessadvance.com/2025/07/21/access-advance-announces-hevc-advance-and-vvc-advance-pricing-through-2030/)). A naive software codec shipping `x265` or `VVenC` inside a commercial Android app exposes the distributor to Access Advance, Via LA (`VCL Advance`), and Velos Media per-unit royalties plus GPLv2 viral licensing (`x264`/`x265` are GPLv2 unless a commercial license is purchased). Prism-V neutralizes both risks by design:

1. **Zero GPL Contamination (Permissive Clean-Room Stack):**
   - `Prism-V` links **zero GPL code** (no `x264`, no `x265`, no GPL `FFmpeg` build). All custom crates (`prism-*`) are authored in-house under Apache-2.0 / BSD-3-Clause; third-party dependencies are strictly BSD/MIT/ISC (`dav1d` is BSD-2-Clause; Orange Labs' `Cool-Chic` is BSD-3-Clause; Google DeepMind's `C3` is Apache-2.0).
2. **Hardware `MediaCodec` Pass-Through Shield for H.264/HEVC/AV1 Base Layers:**
   - In `Prism-Lossy`, Prism-V **never ships a software H.264 or HEVC encoder/decoder**. Instead, it invokes the Android OS hardware `android.media.MediaCodec` API. The smartphone OEM (Samsung, Google, Xiaomi, etc.) has already paid the hardware device patent pool royalties for the silicon codec blocks on the SoC.
3. **Royalty-Free & Expired Foundations for Custom Modules:**
   - **rANS Entropy Coding:** Invented by Jarek Duda (2009–2013) and dedicated to the public domain; Microsoft's ANS patent assertions were narrow and avoided by using standard power-of-two table-less/dyadic rANS (`2^15` modulus) as used in JPEG XL and AV1.
   - **LeGall 5/3 Integer Lifting Wavelet & YCoCg-R:** Core JPEG 2000 Part 1 (2000) and H.264 Fidelity Range Extensions (2004) transforms whose foundational patents expired between 2020 and 2024.
   - **AOMedia `AFGS1` Film Grain Synthesis:** Released by the Alliance for Open Media under the **AOMedia Royalty-Free Patent License** ([AOMedia AFGS1](http://aomedia.org/specifications/afgs1/)).
   - **Avoidance of V-Nova LCEVC Claims:** `Prism-Lossy` explicitly avoids LCEVC's patented $2\times 2$ / $4\times 4$ directional linear residual decomposition (`DD` / `DDS`) and temporal residual buffer update rules, instead using an overfitted coordinate/latent MLP (`Cool-chic` architecture, published and open-sourced by Orange Labs) with multi-resolution continuous latents.

---

### 4.8 What to Research and Prototype First: The 7-Day Minimal Falsifiable Experiment

Before writing a single line of Android UI or Vulkan shader code, a single engineer can prove or falsify the two core numerical claims of Prism-V in **7 days** on a Linux workstation tethered via `adb` to one Android phone:

1. **Experiment 1A (`Prism-BBR` 3-Day Test):**
   - Take 10 real 1080p30 H.264 and 10 real 1080p30 HEVC camera videos from an Android phone.
   - Run a 400-line Python + C++ script that hooks `FFmpeg`'s `ff_h264_decode_mb_cabac` / `ff_hevc_cabac_decode` to dump the exact stream of decoded syntax symbols `(syntax_type, ctx_idx, symbol_value, block_x, block_y, plane)` to a file.
   - Train the 1.1 KB `INT8` Micro-MLP (`16 -> 32 -> 16`) on 8 videos and compress the syntax stream of the 2 held-out videos using `HuffmanPos` ([Singh, June 2026](https://arxiv.org/html/2606.23753v1)) + 8-way rANS.
   - **Success Metric:** Total compressed syntax size + non-video MP4 boxes (`zstd`) is **$\ge 15\%$ smaller than original H.264 MP4** and **$\ge 9\%$ smaller than original HEVC MP4**.
2. **Experiment 1B (`Prism-Lossy` 4-Day Test):**
   - On the tethered Android phone, use `adb shell screenrecord` / `MediaCodec` to encode 5 `UVG` 1080p YUV sequences at 1.5 Mbps using hardware HEVC and hardware AV1, pull the decoded YUV back to the host, and compute the residual $r_t = f_t - \hat{f}_{t,\text{HW}}$.
   - Run the open-source [Orange-OpenSource/Cool-Chic 5.0 repository](https://github.com/Orange-OpenSource/Cool-Chic) configured at the **500 MAC/pixel** decoder preset to compress $r_t$ at 0.25 Mbps (total rate = 1.75 Mbps), and compare its `VMAF-NEG` and `CVVDP` against a standalone 1.75 Mbps hardware `MediaCodec` encode.
   - **Success Metric:** The 1.50 Mbps HW + 0.25 Mbps Cool-chic 500-MAC/px hybrid stream beats the 1.75 Mbps standalone HW stream by **$\ge 1.2\text{ dB PSNR}$ / $\ge 4.5\text{ VMAF-NEG points}$ ($\approx -18\%$ BD-rate)**.

---

## 5. Red-Team Analysis: How This Plan Could Fail and What We Do Then

Following our adversarial validation principles, we systematically attack the Prism-V plan across 10 technical, systems, legal, and market failure modes, quantifying likelihood, severity, early-warning triggers, and concrete fallback pivots.

| # | Failure Mode (How the Plan Breaks) | Likelihood | Severity | Early Detection Trigger | Concrete Fallback / Engineering Pivot |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **1** | **HEVC Hardware Encoders Leave Too Little CABAC Redundancy for `Prism-BBR`:** Modern flagship HEVC hardware encoders might already pack syntax symbols within 4%–5% of conditional entropy, making a 4%–5% file size reduction not worth the `Prism-VFS` complexity. | Medium (35%) | High | Week 1 Experiment 1A shows $<6\%$ average reduction on 2025/2026 flagship HEVC MP4s. | **Pivot 1:** Restrict `Prism-BBR` to H.264, Motion-JPEG, and high-bitrate (>35 Mbps) camera originals where savings exceed 14%; for already-tight HEVC files, offer **"Perceptually Lossless (`CVVDP` $\ge 9.8/10$, $\Delta E_{2000} < 0.5$)"** re-encoding via `Prism-Lossy` which cuts size by 40%–55%. |
| **2** | **Non-Canonical Hardware `MediaCodec` CABAC Bitstreams Blow Up $\Delta_{\text{patch}}$:** Some OEM hardware encoders might use strange non-deterministic CABAC termination bits or non-standard Exp-Golomb escape sequences that cause our shadow CABAC re-encoder to diverge frequently. | Low-Med (25%) | High | Bit-exact round-trip test on 50 phone models shows $\Delta_{\text{patch}} > 1.5\%$ of file size on a specific SoC. | **Pivot 2:** Record the exact M-coder state `(codIRange, codILow, bitsOutstanding)` transitions directly from the parser during ingest and store slice-level state checkpoints so any non-canonical slice falls back to raw `zstd` without corrupting or bloating the file. |
| **3** | **Low-Bandwidth Aliasing in `Prism-Lossy` Base Layer Corrupts Temporal Residual:** At very low bitrates (<400 kbps for 1080p), the hardware `MediaCodec` base frame suffers temporal pumping/blocking; encoding $r_t = f_t - \hat{f}_{\text{base}}$ forces the Cool-chic residual layer to waste bits canceling out the base codec's block artifacts! | High (55%) | High | Week 3 Experiment 1B shows BD-rate gains collapse at bitrates $<0.5\text{ Mbps}$ 1080p. | **Pivot 3 (Decoupled Masked Blending):** Instead of strict additive residual $f_{\text{out}} = \hat{f}_{\text{base}} + \hat{r}$, the 600-MAC/pixel MLP outputs a 4th channel $\alpha(x,y) \in [0,1]$ (soft gating mask): $\hat{f}_{\text{out}} = \alpha \odot \hat{f}_{\text{base}} + (1-\alpha) \odot \hat{r}_{\text{local}}$, allowing the micro-MLP to suppress base-layer blocking artifacts before adding high-frequency detail, plus downscale the base track to 540p when bitrate $<600\text{ kbps}$. |
| **4** | **Android OS Kills Charging-Time Overfitting Job (`WorkManager` Quota / Chinese OEM Task Killers):** Xiaomi HyperOS, Vivo OriginOS, and Samsung OneUI aggressively kill background services even while charging if CPU/NPU usage stays high for >10 minutes. | High (60%) | Medium | Phase 2 telemetry shows >20% of multi-GOP charging jobs interrupted before completion. | **Pivot 4 (Atomic 2-Second GOP Micro-Transactions):** Overfitting operates on independent **60-frame (2-second) GOP chunks**. Each GOP takes only **4–8 seconds** to optimize and is atomically swapped into the `.mp4` `prmr` track in-place. Even if the OS kills the job after 3 minutes, 90% of the video is already upgraded and 100% playable! |
| **5** | **`Prism-VFS` (`openProxyFileDescriptor`) Seek Latency Stutters in Third-Party Video Editors:** Video editing apps (CapCut, Instagram Reels) scrub rapidly backward and forward across a timeline, triggering dozens of random `onRead(offset)` calls per second. | Medium (40%) | Medium | Phase 3 test shows frame-scrubbing latency $>120\text{ ms}$ in CapCut. | **Pivot 5:** Reduce `Prism-BBR` independent chunk size from 1 MB to **256 KB** (decoding in **<1.2 ms** on a single NEON core) and maintain an 8-MB LRU read-ahead ring cache in `prism-vfs-service`. |
| **6** | **`N-O Cool-chic` Feed-Forward Hypernetwork Generalizes Poorly to Screen Content / Text:** A hypernetwork trained on natural camera video (`BVI-DVC`) produces blurry edges on screen recordings or whiteboards during live capture. | Medium (45%) | Medium | Evaluation on `Prism-Mobile-Bench-50` Category C (screen content) shows VMAF drop vs. natural scenes. | **Pivot 6:** Train a specialized 2-expert mixture in the hypernetwork gated by a 0.1-ms spatial gradient kurtosis detector (Expert 1: Natural Camera; Expert 2: Sharp Screen/Text content with identity transform bypass). |
| **7** | **Thermal Throttling During 4K60 Live Capture on Mid-Range Phones:** Running Camera2 ISP + Hardware 4K60 Encode + Hardware 4K60 Decode + `N-O Cool-chic` NPU pass simultaneously exceeds 3.5 W on Tier 2 phones after 5 minutes. | Medium (40%) | Medium | Thermal chamber test hits `THERMAL_STATUS_MODERATE` within 5 minutes of 4K60 recording. | **Pivot 7 (Zero-Decode Live Capture Deferred Mode):** During 4K60 live capture when skin temperature $>38^\circ\text{C}$, skip the live hardware decode + residual pass; instead, record the hardware base stream at $1.15\times$ bitrate + `AFGS1` metadata + a tiny low-res edge sketch, and compute the micro-residual layer asynchronously 30 seconds after recording stops. |
| **8** | **Compiler Auto-Vectorization Alters Order of Operations in `Prism-DetInt`:** A future Clang/LLVM update reorders an integer expression or introduces an architecture-specific rounding shift instrinsic. | Low (15%) | Critical | CI cross-platform parity check fails between `arm64-v8a` and `x86_64` host build. | **Pivot 8:** Write all normative Class A arithmetic kernels in explicit fixed-width intrinsics (`#pragma clang fp contract(off)`, explicit `int32_t` casts) and gate every Git commit on a 1,000-vector SHA-256 golden bitstream test across QEMU `aarch64`, `aarch64+sme2`, and `x86_64`. |
| **9** | **Patent Assertion Entity (PAE) Challenges Hybrid Enhancement Layer:** V-Nova or a patent pool claims broad coverage over "combining a base hardware video stream with a residual enhancement track". | Low-Med (25%) | High | External FTO review flags specific claim overlap during Phase 1/2. | **Pivot 9:** Exercise our documented design-around: instead of storing a pixel-difference residual $r_t = f_t - \hat{f}_{\text{base}}$, store **spatially varying 5x5 Wiener/Convolutional Restoration Filter Weights $W_{x,y}$** (as used in AV1 Loop Restoration / JVET ALF, which are covered by AOMedia RF and expired prior art) predicted by the overfitted Cool-chic MLP: $\hat{f}_{\text{out}} = W_{\boldsymbol{\theta}}(\hat{\mathbf{L}}_t) \circledast \hat{f}_{\text{base}}$. |
| **10** | **Google Play Store Restricts Background File Access (`MANAGE_EXTERNAL_STORAGE`):** Play Store policy tightens permissions for gallery-wide compression utilities. | Medium (30%) | Medium | Play Console policy review rejects `MANAGE_EXTERNAL_STORAGE`. | **Pivot 10:** Use standard Android `MediaStore.createWriteRequest()` batch URIs and `PhotoPicker` + `CloudMediaProvider` APIs, which require zero special Play Store permissions and are officially blessed in Android 14–17. |

---

## 6. Appendices

### Appendix A: Primary and Secondary Sources with URLs and Dates

1. **AOMedia AV2 Specification Release (June 6, 2026):** Alliance for Open Media, *"Alliance for Open Media Releases AV2 Codec, Advancing Next-Generation Video Coding"* — [http://aomedia.org/press%20releases/Alliance-for-Open-Media-Releases-AV2-Codec/](http://aomedia.org/press%20releases/Alliance-for-Open-Media-Releases-AV2-Codec/)
2. **AV2 Official Specification Portal (2026):** Alliance for Open Media — [https://av2.aomedia.org/](https://av2.aomedia.org/)
3. **AV2 Common Test Conditions & BD-Rate Evaluation Paper (May 15, 2026):** *"Video Quality Evaluation Methodology and Result of AV2"*, arXiv:2605.15800v1 — [https://arxiv.org/html/2605.15800v1](https://arxiv.org/html/2605.15800v1)
4. **AV2 Implementer Guide (July 27, 2026):** Alliance for Open Media, *"AV2: What the Final Specification Means for Implementers"* — [http://aomedia.org/blog%20posts/AV2-What-the-Final-Specification-Means-for-Implementers/](http://aomedia.org/blog%20posts/AV2-What-the-Final-Specification-Means-for-Implementers/)
5. **AV2 Hardware Support & 2026–2031 Adoption Timeline (October 16, 2026):** Fora Soft, *"AV2 Codec Explained: Release, Hardware Support, Roadmap"* — [https://www.forasoft.com/learn/video-encoding/articles/av2-what-to-expect-2026-2031](https://www.forasoft.com/learn/video-encoding/articles/av2-what-to-expect-2026-2031)
6. **DCVC-UF Ultra-Fast Neural Video Compression (June 3, 2026, CVPR 2026):** Li et al., *"Ultra-Fast Neural Video Compression"*, arXiv:2606.04410v1 — [https://arxiv.org/html/2606.04410v1](https://arxiv.org/html/2606.04410v1) & [https://www.alphaxiv.org/abs/2606.04410](https://www.alphaxiv.org/abs/2606.04410)
7. **DCVC-RT Real-Time Neural Video Compression (March 18, 2025, CVPR 2025):** Jia et al., *"Towards Practical Real-Time Neural Video Compression"*, arXiv:2502.20762v2 — [https://arxiv.org/html/2502.20762v2](https://arxiv.org/html/2502.20762v2) & Official Repository: [https://github.com/microsoft/dcvc](https://github.com/microsoft/dcvc)
8. **Cool-chic 5.0 Overfitted Low-Complexity Codec (July 31, 2026):** Philippe et al. (Orange Labs), *"Cool-chic 5.0: Faster Encoding and Inter-Feature Entropy Modeling for Overfitted Image Coding"*, arXiv:2605.02726v2 — [https://arxiv.org/html/2605.02726v2](https://arxiv.org/html/2605.02726v2) & Official Repository: [https://github.com/Orange-OpenSource/Cool-Chic](https://github.com/Orange-OpenSource/Cool-Chic)
9. **N-O Cool-chic Non-Overfitted Fast Encoding (July 15, 2026):** *"N-O Cool-chic: reconcile fast encoding with lightweight decoding for neural image compression"*, arXiv:2607.13723v1 — [https://arxiv.org/html/2607.13723v1](https://arxiv.org/html/2607.13723v1)
10. **Hypernetwork Acceleration for Overfitted Codecs (September 23, 2025):** *"Reducing Encoding Cost in Overfitted Codecs with Hypernetworks"*, arXiv:2509.18748v1 — [https://arxiv.org/html/2509.18748v1](https://arxiv.org/html/2509.18748v1)
11. **Google DeepMind C3 Neural Compression (December 5, 2023):** Kim et al., *"C3: High-performance and low-complexity neural compression from a single image or video"*, arXiv:2312.02753 — [https://arxiv.org/abs/2312.02753](https://arxiv.org/abs/2312.02753) & Official Repository: [https://github.com/google-deepmind/c3_neural_compression](https://github.com/google-deepmind/c3_neural_compression)
12. **NeuralLVC Masked-Diffusion Lossless Video Compression (September 8, 2026, ACM MM '26):** Uricchio & Bertini, *"NeuralLVC: Neural Lossless Video Compression via Masked Diffusion with Temporal Conditioning"*, arXiv:2604.03353v2 — [https://arxiv.org/html/2604.03353v2](https://arxiv.org/html/2604.03353v2)
13. **Entropy-Conserving Binarization & Position-Indexed Contexts in H.264 CABAC (June 22, 2026):** Vinamra Singh, *"An Empirical Study of Entropy-Conserving Binarization in H.264/AVC CABAC"*, arXiv:2606.23753v1 — [https://arxiv.org/html/2606.23753v1](https://arxiv.org/html/2606.23753v1)
14. **RAS Bit-Exact rANS Hardware/SIMD Acceleration (October 20, 2025):** *"RAS: A Bit-Exact rANS Accelerator For High-Performance Neural Lossless Compression"*, arXiv:2511.04684v1 — [https://arxiv.org/html/2511.04684v1](https://arxiv.org/html/2511.04684v1)
15. **Deterministic Integer Networks for Cross-Platform Range Coding (ICLR 2019):** Ballé, Johnston, & Minnen, *"Integer Networks for Data Compression with Latent-Variable Models"* — [https://openreview.net/forum?id=S1zz2i0cY7](https://openreview.net/forum?id=S1zz2i0cY7)
16. **InterDigital CompressAI Library (November 5, 2020):** Bégaint et al., *"CompressAI: a PyTorch library and evaluation platform for end-to-end compression research"*, arXiv:2011.03029 — [https://arxiv.org/html/2011.03029](https://arxiv.org/html/2011.03029) & [https://github.com/interdigitalinc/compressai](https://github.com/interdigitalinc/compressai)
17. **UniGVC Unified Generative Video Compression (October 9, 2026):** *"Unified Generative Video Compression with Autoregressive and Bidirectional Diffusion Transformers"*, arXiv:2610.13023v1 — [https://arxiv.org/html/2610.13023v1](https://arxiv.org/html/2610.13023v1)
18. **JVET ECM / H.267 Requirements & Performance (February 4, 2025 & December 5, 2025):** Streaming Media, *"H.267: A Codec for (One Possible) Future"* ([https://www.streamingmedia.com/Articles/News/Online-Video-News/H.267-A-Codec-for-(One-Possible)-Future-167889.aspx](https://www.streamingmedia.com/Articles/News/Online-Video-News/H.267-A-Codec-for-(One-Possible)-Future-167889.aspx)) & Qualcomm OnQ, *"Advancing video coding: Reflections on the call-for-evidence"* ([https://www.qualcomm.com/news/onq/2025/12/advancing-video-coding-call-for-evidence](https://www.qualcomm.com/news/onq/2025/12/advancing-video-coding-call-for-evidence))
19. **JVET ECM-11.0 Quantitative Intra Analysis (April 11, 2024):** *"Video Compression Beyond VVC: Quantitative Analysis of Intra Coding Tools in the Enhanced Compression Model (ECM)"*, arXiv:2404.07872v1 — [https://arxiv.org/html/2404.07872v1](https://arxiv.org/html/2404.07872v1)
20. **Samsung Advanced Professional Video (APV, IETF RFC 9924, April 2026):** Samsung Developer Portal ([https://developer.samsung.com/mobile/apv.html](https://developer.samsung.com/mobile/apv.html)) & Samsung Global Newsroom ([https://news.samsung.com/global/editorial-how-apv-is-elevating-mobile-video-to-professional-standards](https://news.samsung.com/global/editorial-how-apv-is-elevating-mobile-video-to-professional-standards))
21. **AOMedia Film Grain Synthesis 1 (AFGS1 Specification, January 17, 2024):** [https://aomediacodec.github.io/afgs1-spec/](https://aomediacodec.github.io/afgs1-spec/) & [http://aomedia.org/specifications/afgs1/](http://aomedia.org/specifications/afgs1/)
22. **FFV1 Lossless Video Specification (IETF RFC 9043, August 2021):** Niedermayer, Rice, & Martinez — [https://www.rfc-editor.org/rfc/rfc9043](https://www.rfc-editor.org/rfc/rfc9043)
23. **Dropbox Lepton Lossless JPEG Recompression:** [https://github.com/dropbox/lepton](https://github.com/dropbox/lepton) & [https://dropbox.tech/infrastructure/lepton-image-compression-saving-22-losslessly-from-images-at-15mbs](https://dropbox.tech/infrastructure/lepton-image-compression-saving-22-losslessly-from-images-at-15mbs)
24. **Learned Lossless JPEG Recompression in DCT Domain (CVPR 2022):** Guo et al., *"Practical Learned Lossless JPEG Recompression With Multi-Level Cross-Channel Entropy Model"* — [https://openaccess.thecvf.com/content/CVPR2022/papers/Guo_Practical_Learned_Lossless_JPEG_Recompression_With_Multi-Level_Cross-Channel_Entropy_Model_CVPR_2022_paper.pdf](https://openaccess.thecvf.com/content/CVPR2022/papers/Guo_Practical_Learned_Lossless_JPEG_Recompression_With_Multi-Level_Cross-Channel_Entropy_Model_CVPR_2022_paper.pdf)
25. **Android 16 & Android 17 Compatibility Definition Documents (CDD & MPC, June–August 2026):** Google Android Source — [https://source.android.com/docs/compatibility/17/android-17-cdd](https://source.android.com/docs/compatibility/17/android-17-cdd), [https://source.android.com/docs/compatibility/17/mpc](https://source.android.com/docs/compatibility/17/mpc), & [https://source.android.com/docs/compatibility/16/android-16-cdd](https://source.android.com/docs/compatibility/16/android-16-cdd)
26. **Qualcomm Snapdragon 8 Elite Gen 5 Product Brief (2025/2026):** [https://www.qualcomm.com/content/dam/qcomm-martech/dm-assets/documents/Snapdragon-8-Elite-Gen-5-product-brief.pdf](https://www.qualcomm.com/content/dam/qcomm-martech/dm-assets/documents/Snapdragon-8-Elite-Gen-5-product-brief.pdf)
27. **MediaTek Dimensity 9500 Official Specifications (2026):** [https://www.mediatek.com/products/smartphones/mediatek-dimensity-9500](https://www.mediatek.com/products/smartphones/mediatek-dimensity-9500)
28. **Arm Scalable Matrix Extension 2 (SME2) & KleidiAI on Android (September–October 2025):** [https://www.arm.com/company/success-library/google-android-ai-sme2](https://www.arm.com/company/success-library/google-android-ai-sme2) & [https://developer.arm.com/community/arm-community-blogs/b/ai-blog/posts/executorch-1-0-is-here-and-with-sme2-optimizations-through-kleidiai](https://developer.arm.com/community/arm-community-blogs/b/ai-blog/posts/executorch-1-0-is-here-and-with-sme2-optimizations-through-kleidiai)
29. **Google AI Edge LiteRT Qualcomm NPU QNN Delegate (May 28, 2026):** [https://developers.google.com/edge/litert/next/qualcomm](https://developers.google.com/edge/litert/next/qualcomm)
30. **Vulkan Cooperative Matrix (`VK_KHR_cooperative_matrix`, August 7, 2026):** [https://docs.vulkan.org/features/latest/features/proposals/VK_KHR_cooperative_matrix.html](https://docs.vulkan.org/features/latest/features/proposals/VK_KHR_cooperative_matrix.html) & [https://www.phoronix.com/news/Vulkan-1.4.359](https://www.phoronix.com/news/Vulkan-1.4.359)
31. **ColorVideoVDP (`CVVDP`) Perceptual Metric:** University of Cambridge Rainbow Group — [https://github.com/gfxdisp/colorvideovdp](https://github.com/gfxdisp/colorvideovdp)
32. **2026 Encoder Implementations & Patent Pool Risk Analysis (October 2026):** Fora Soft ([https://www.forasoft.com/learn/video-encoding/articles/encoder-implementations-x264-x265-svt-av1-libaom-vvenc](https://www.forasoft.com/learn/video-encoding/articles/encoder-implementations-x264-x265-svt-av1-libaom-vvenc)) & Lumenci ([https://lumenci.com/blogs/hevc-vs-av1-vs-vvc-royalties-licensing-risk/](https://lumenci.com/blogs/hevc-vs-av1-vs-vvc-royalties-licensing-risk/))

---

### Appendix B: Master Claims & Evidence Verification Ledger

| Claim ID | Technical Claim | Evidence Tag | Baseline, Dataset & Hardware Context | Source Link |
| :--- | :--- | :--- | :--- | :--- |
| **C-01** | AV2 (v13.0 / Spec 1.0.0) achieves -29.81% PSNR-YUV and -33.79% VMAF BD-rate in Random Access (RA) and -35.77% VMAF in Adaptive Streaming (AS) over AV1, at 33x encode and 3.86x decode runtime. | `[Verified - Primary]` | vs. `libaom` AV1 research anchor on AV2 CTC dataset (270p–4K SDR/HDR/UGC/Screen). | [arXiv:2605.15800](https://arxiv.org/html/2605.15800v1) |
| **C-02** | `DCVC-UF` (CVPR 2026) achieves -42.2% BD-rate vs VTM-17.0 (Low-Delay) and reaches 655.9 fps encode / 453.3 fps decode (`HT-S`) on 1080p video. | `[Verified - Primary]` | vs. `VTM-17.0` on `UVG` / `HEVC` 1080p datasets; measured on NVIDIA RTX 4090 (and 1,415.1 fps on B200). | [arXiv:2606.04410](https://arxiv.org/html/2606.04410v1) |
| **C-03** | `Cool-chic 5.0` (July 2026) achieves -11% BD-rate reduction vs H.266/VVC at 500–3,000 MACs/decoded pixel with 10x fewer encoding iterations. | `[Verified - Primary]` | vs. `VTM` on `CLIC` dataset; decoder is 250x less complex than `MLIC++`. | [arXiv:2605.02726](https://arxiv.org/html/2605.02726v2) |
| **C-04** | `N-O Cool-chic` (July 2026) cuts encoding complexity by 1,000x (<1 s forward pass) at ~2,000 MACs/pixel decoding, requiring +45% higher rate than overfitted Cool-chic (within 1% of Ballé hyperprior at 30x lower decode complexity). | `[Verified - Primary]` | Evaluated on `CLIC 2020` validation set against `HEVC`, `Cool-chic`, and `Ballé`. | [arXiv:2607.13723](https://arxiv.org/html/2607.13723v1) |
| **C-05** | `NeuralLVC` (Sept 2026) achieves 29.85% lossless rate on Xiph CIF (beating `x265 --lossless` 36.37% by 17.9% relative), 29.46% on 1080p Xiph (vs 38.00% `x265` and 35.73% `FFV1`), and 30.34% on UVG 1080p (vs 35.77% `FFV1`), taking 11.8 PFLOPs (205–400 s) per 1080p frame. | `[Verified - Primary]` | Evaluated across 33 sequences (`Xiph`, `UVG`, `BVI-AOM`) on NVIDIA GH200 in YUV420 against `x264 -qp 0`, `x265 lossless=1`, `FFV1`. | [arXiv:2604.03353](https://arxiv.org/html/2604.03353v2) |
| **C-06** | Position-indexed context allocation (`HuffmanPos`) and `ECB` inside H.264 CABAC code quantized DCT residuals at -7.67% to -9.10% below marginal first-order Shannon entropy on Kodak images, beating single-context Huffman by up to 21%–27%. | `[Verified - Primary]` | 2,480 bit-exact verified round-trip trials on procedural DCT and 24 Kodak images in ITU-T H.264 Section 9.3 M-coder. | [arXiv:2606.23753](https://arxiv.org/html/2606.23753v1) |
| **C-07** | Samsung `APV` (IETF RFC 9924) is natively supported in Android 16 and ships with hardware acceleration on Galaxy S26 Ultra, saving ~20% storage over legacy pro codecs. | `[Verified - Primary]` | Samsung Developer & Global Newsroom documentation (April 2026). | [Samsung APV](https://developer.samsung.com/mobile/apv.html) |
| **C-08** | `Prism-BBR` will achieve 14.5%–23.5% lossless file size reduction on existing H.264 MP4s and 9.0%–15.5% on HEVC MP4s at 160–340 MB/s decode speed on mid-range Arm NEON. | `[Estimate]` | Derived from [Lepton (22% on JPEG)](https://github.com/dropbox/lepton) + [Singh (June 2026)](https://arxiv.org/html/2606.23753v1) + inter-block/chroma Micro-MLP context gains. | Section 3.1 (Invention 1) |
| **C-09** | `Prism-PSC` will achieve 27.5%–31.0% of raw YUV420 size on 1080p video at 30–60 fps on mobile Armv9 CPUs. | `[Estimate]` | Derived by replacing `NeuralLVC`'s 94-pass Transformer with LeGall 5/3 wavelet + 4-pass Checkerboard INT8 ConvNet (0.34 GMAC/frame). | Section 3.1 (Invention 4) |
| **C-10** | `Prism-Lossy` will achieve -18% to -27% BD-rate over hardware AV1 and -32% to -44% BD-rate over hardware HEVC at 500–1,500 MACs/pixel decode overhead. | `[Estimate]` | Derived from `AFGS1` denoising gain (-12% to -20%) + `Cool-chic 5.0` micro-residual enhancement (-10% to -15%). | Section 3.1 (Invention 3) |

---

### Appendix C: Technical Glossary

- **AFGS1 (AOMedia Film Grain Synthesis 1):** A standalone specification released by AOMedia in 2024 that parameterizes camera sensor noise and film grain as a 14-parameter autoregressive model (<200 bytes/frame), allowing the encoder to strip incompressible noise before compression and re-synthesize it at the decoder.
- **BD-Rate (Bjøntegaard-Delta Rate):** The standard video coding metric measuring the average percentage difference in bitrate between two rate-distortion curves at equal objective quality (PSNR, VMAF, or CVVDP). A BD-rate of `-30%` means the codec requires 30% fewer bits for the same visual quality.
- **CABAC (Context-Adaptive Binary Arithmetic Coding):** The normative entropy coding engine in H.264/AVC, H.265/HEVC, and H.266/VVC, which converts multi-valued syntax symbols into binary decisions ("bins") via a binarization scheme and compresses each bin using a 64-state table-driven binary arithmetic coder (`M-coder`).
- **Cool-chic:** An open-source family of low-complexity overfitted neural codecs created by Orange Labs (`Cool-chic 1.0` through `Cool-chic 5.0`), which represent an image or video frame using compact multi-resolution latent grids and a tiny content-specific neural network (500–3,000 MACs/pixel) whose weights are sent in the bitstream.
- **CVVDP (ColorVideoVDP):** A full-reference psychophysical visual difference predictor developed at the University of Cambridge that models human spatio-temporal and chromatic contrast sensitivity across SDR and HDR displays.
- **Dyadic Rescaling:** Integer fixed-point multiplication followed by a power-of-two right bit-shift ($\lfloor (x \cdot M) / 2^s \rfloor$), avoiding floating-point arithmetic so results are 100% bit-identical across all processors.
- **ISOBMFF (ISO Base Media File Format):** The international standard container structure (ISO/IEC 14496-12) underlying `.mp4`, `.mov`, `.heic`, and `.avif` files, organized into hierarchical 4-byte-tagged atoms/boxes (`ftyp`, `moov`, `trak`, `mdat`).
- **MACs/pixel (Multiply-Accumulates per Decoded Pixel):** The primary measure of neural codec computational complexity; 1,000 MACs/pixel on a 1080p frame ($2.07\times 10^6$ pixels) equals 2.07 GMACs per frame.
- **rANS (Range Asymmetric Numeral Systems):** A modern entropy coding method invented by Jarek Duda that achieves the compression efficiency of arithmetic coding at the speed of Huffman coding by encoding information into a single natural number state $x$ amenable to SIMD vectorization.
- **SME2 (Arm Scalable Matrix Extension 2):** An Armv9.2-A CPU instruction set extension (shipping in MediaTek Dimensity 9400/9500 and Arm Cortex-X925+) that provides dedicated matrix outer-product accumulators directly accessible from user-space CPU threads without NPU driver overhead.

---

### Appendix D: Open Questions for Phase 0 Discovery

1. **Exact CABAC Syntax Redundancy Distribution Across OEM Hardware Encoders:** How much does the achievable `Prism-BBR` lossless savings vary between Qualcomm `Snapdragon` hardware encoders, MediaTek `Imagiq/MiraVision` hardware encoders, and Google `Tensor` BigWave encoders at default Camera app bitrates?
2. **`AMediaCodec` Simultaneous Encode + Decode Hardware Slot Contention on Tier 1 SoCs:** While [Android 17 MPC](https://source.android.com/docs/compatibility/17/mpc) guarantees $\ge 6$ concurrent hardware sessions on flagship/performance devices, do entry-level $120 Unisoc/Helio chips throttle clock speed when running 1080p30 hardware encode + hardware decode simultaneously?
3. **Optimal Bit Allocation Ratio Between Hardware Base Track (`av01`/`hvc1`) and `Cool-chic` Residual Track (`prmr`):** Across low, medium, and high bitrates, does the optimal residual track share stay near $10\%–15\%$ of total bits, or should it dynamically scale from $25\%$ at low bitrate down to $6\%$ at high bitrate?

---

## 7. Self-Audit: Weakest Claims, Unverified Items, and Top 10 Next Experiments

### 7.1 Audit of Our Weakest Claims

1. **Weakest Claim #1 — `Prism-BBR` Savings on Already-Optimized Hardware HEVC Streams (`9.0%–15.5%` `[Estimate]`):**
   While ~20%–22% lossless recompression is rigorously proven on baseline JPEG ([Dropbox Lepton](https://github.com/dropbox/lepton); [JPEG XL](https://arxiv.org/html/2506.05987v2)) and -7.7% to -9.1% sub-entropy coding is proven on H.264 CABAC DCT blocks ([Singh, June 2026](https://arxiv.org/html/2606.23753v1)), HEVC's CABAC already incorporates improved Rice parameter adaptation and coefficient group (`4x4` sub-block) significance coding. If an OEM hardware HEVC encoder uses near-optimal CABAC context initialization, `Prism-BBR` savings on HEVC could land closer to **6%–9%** rather than 9%–15.5%.
2. **Weakest Claim #2 — `N-O Cool-chic` Live Residual Gain at 60 fps (`-14% to -21% BD-rate` `[Estimate]`):**
   In [arXiv:2607.13723 (July 2026)](https://arxiv.org/html/2607.13723v1), bypassing iterative overfitting in `N-O Cool-chic` incurred a +45% rate penalty on standalone full-image coding compared to overfitted `Cool-chic`. Our hypothesis that conditioning on a 90%-energy hardware base frame shrinks this non-overfitted gap to just 6%–10% BD-rate is mathematically grounded (because the residual has near-stationary zero-mean Laplacian statistics), but has **not yet been empirically measured on video**.
3. **Weakest Claim #3 — 4-Pass Checkerboard ConvNet Matching 94-Pass `NeuralLVC` Within 1.5% Rate (`Prism-PSC`):**
   `NeuralLVC` ([arXiv:2604.03353v2](https://arxiv.org/html/2604.03353v2)) uses 94 diagonal groups (`HPAC` $\delta=2$) with full bidirectional attention across a $32\times 32$ patch. Reducing 94 passes to 4 checkerboard passes after a 1-level LeGall 5/3 wavelet transform could lose **1.5%–3.0% of raw compression ratio** (landing at ~31.8% of raw size on UVG instead of 30.34%), though it would still decisively beat `FFV1` (35.77%) and `x265 --lossless` (36.61%) while running 25,000x faster.

### 7.2 What Could Not Be Verified as of November 14, 2026

- **Unverified Item 1:** Exact die-area and mobile power figures (mW at 1080p60) for the first shipping mobile hardware H.266/VVC decoder block rumored on late-2026 flagship silicon ([Reddit r/VVC Sept 2026](https://www.reddit.com/r/VVC/comments/1wnz66i/the_snapdragon_8_elite_extreme_gen_6_supports/)) — official Qualcomm/MediaTek datasheets do not yet publish standalone VVC decode power numbers (`Unverified`).
- **Unverified Item 2:** Android 18 (2027) CDD mandatory requirements for hardware AV1 encoding on non-MPC (entry-level) devices (`Unverified`).
- **Unverified Item 3:** Exact `KleidiAI` `SME2` kernel throughput (GMAC/s per Watt) on retail MediaTek Dimensity 9500 handsets under sustained 10-minute thermal load (`Unverified` — independent handset reviews with Monsoon power telemetry are still pending).

### 7.3 The 10 Best Next Experiments (Ranked by Information Value per Engineering Day)

| Rank | Experiment Name | Effort (Days) | Exact Protocol | Decision / Falsification Value |
| :--- | :--- | :--- | :--- | :--- |
| **1** | **H.264/HEVC CABAC Syntax Entropy Audit (`Exp-BBR-01`)** | 3 Days | Extract all CABAC syntax symbols from 50 real Android MP4s (Samsung, Pixel, Xiaomi) and compress with `HuffmanPos` + 1.1 KB `INT8` Micro-MLP + rANS. | Immediately proves exact % lossless savings of `Prism-BBR` on real phone videos before building the full parser. |
| **2** | **Base-Conditioned `Cool-chic 5.0` vs. Standalone HW `MediaCodec` (`Exp-Lossy-01`)** | 4 Days | Encode 7 `UVG` 1080p clips with Android HW AV1/HEVC at 85% target bitrate + 500-MAC/px `Cool-chic 5.0` on the residual at 15% target bitrate; compare `VMAF-NEG` & `CVVDP` vs. 100% bitrate HW encode. | Proves the core `Prism-Lossy` hybrid rate-distortion hypothesis. |
| **3** | **Cross-Silicon `Prism-DetInt` 1-Billion-Symbol Torture Test (`Exp-Det-01`)** | 3 Days | Run the C++20 `INT8`/`INT16`/`INT32` dyadic prior + 512B LUT + 8-way rANS across Snapdragon, Dimensity, Exynos, Tensor, and x86_64 on $10^9$ synthetic and real symbols. | Proves 100.000% cross-platform bit-exactness of the neural entropy loop. |
| **4** | **`N-O Cool-chic` Feed-Forward Residual Generalization Test (`Exp-Lossy-02`)** | 5 Days | Train a 4.8 MB feed-forward `N-O Cool-chic` hypernetwork to predict residual latents + MLP weights in 1 pass; measure BD-rate gap vs. 400-step iterative overfitting. | Determines how much quality we get during live 60 fps capture vs. overnight charging optimization. |
| **5** | **4-Pass Wavelet Checkerboard vs. 94-Pass `NeuralLVC` Ablation (`Exp-PSC-01`)** | 5 Days | Compare 1-level LeGall 5/3 wavelet + 4-pass `INT8` ConvNet (`Prism-PSC`) against `NeuralLVC`'s 30.34% UVG benchmark and `FFV1` (35.77%). | Locks in the exact architecture and speed-vs-compression curve for raw pixel lossless mode. |
| **6** | **`AFGS1` Sensor Noise Separation Gain on High-ISO Mobile Video (`Exp-Grain-01`)** | 3 Days | Run `AFGS1` noise extraction + synthesis around Android HW HEVC/AV1 on 15 indoor/night smartphone camera clips; measure `CVVDP` and bitrate savings. | Quantifies the free bitrate savings from stripping temporally uncorrelated sensor shot noise before the HW encoder. |
| **7** | **Android `openProxyFileDescriptor` Throughput & App Compatibility (`Exp-VFS-01`)** | 3 Days | Serve a 200 MB virtual `.mp4` backed by `libprism_rans` in RAM to WhatsApp, Instagram, CapCut, and ExoPlayer; measure seek latency and CPU %. | Validates that `Prism-VFS` works seamlessly across top Play Store apps without temp-file disk bloat. |
| **8** | **Tier 1 Low-End Concurrent HW Encode + Decode Thermal Profiling (`Exp-HW-01`)** | 2 Days | On a Redmi Note 12/13 (4 GB RAM), run simultaneous 1080p30 `AMediaCodec` encode + decode via `AHardwareBuffer` for 15 minutes on a Monsoon power monitor. | Confirms whether entry-level phones can run live closed-loop residual calculation or need deferred residual extraction. |
| **9** | **Arm `SME2` (`KleidiAI`) vs. `NEON` (`SDOT`) vs. `Vulkan Cooperative Matrix` Micro-Benchmark (`Exp-SIMD-01`)** | 4 Days | Benchmark the 600-MAC/pixel `Cool-chic` upsampler + MLP in `NEON`, `SME2`, and `Vulkan` compute shaders on Dimensity 9500 and Snapdragon 8 Elite. | Establishes the exact hardware dispatch threshold table across Tiers 1, 2, and 3. |
| **10** | **Masked Soft-Gating ($\alpha$-Blend) vs. Additive Residual at Ultra-Low Bitrate (`Exp-Lossy-03`)** | 3 Days | Test $\hat{f}_{\text{out}} = \alpha \odot \hat{f}_{\text{base}} + (1-\alpha) \odot \hat{r}$ vs. $\hat{f}_{\text{base}} + \hat{r}$ at 250–500 kbps 1080p where the hardware base stream exhibits severe blocking. | Validates Red-Team Pivot #3 for ultra-low-bitrate streaming/sharing. |
