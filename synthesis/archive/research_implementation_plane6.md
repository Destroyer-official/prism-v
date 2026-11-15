# Implementation Plan: Next-Gen Android Video Compression (2027+)

## 1. Executive Summary

This document outlines the architecture and execution plan for a state-of-the-art video compression application targeting Android devices from 2027 to 2030. The system introduces an **Adaptive Layered Codec Architecture (ALCA)** that dynamically shifts workloads between the CPU, GPU, NPU, and dedicated hardware media accelerators based on the user's priority (Quality, Size, Speed, or Battery).

**The Recommended Approach:**

* **Lossless Mode:** True mathematically bit-exact preservation of YUV 4:2:0/4:2:2 camera frames. We will leverage Samsung's **APV (Advanced Professional Video)** codec natively supported in Android 16. For older hardware, the system will fall back to hardware-accelerated HEVC Main 10 Lossless profile.
* **Lossy Mode (Battery/Speed Focus):** A hybrid combination of a hardware AV1 or HEVC base layer enhanced by **MPEG-5 Part 2 LCEVC (Low Complexity Enhancement Video Codec)**. This runs in pure software, minimizing power draw while improving sharpness.
* **Lossy Mode (Max Quality/Size Focus):** Hardware **VVC (H.266)**, natively supported in Android 17. As a fallback (or for devices lacking VVC hardware), we will deploy a custom **Neural Residual Codec** running on the Android NPU, adapted from Microsoft's MLVC framework.

**First 30 Days:**

1. Stand up the Android NDK C++ pipeline orchestrating `MediaCodec` (C-API) and `NNAPI`.
2. Integrate the OpenAPV reference library and measure YUV lossless memory bandwidth on flagship vs. mid-tier devices.
3. Establish the continuous benchmarking harness (VMAF, PSNR, Battery metrics via `dumpsys batterystats`).

---

## 2. Research Findings: The 2026 Video Codec Landscape

The field has shifted dramatically in 2026. Below is the state of the art, categorized by maturity.

* **APV (Advanced Professional Video) [Standardized, Shipping in Products]:** Samsung published RFC 9924 in Feb 2026. Android 16 made APV encoding/decoding mandatory. It is optimized for lossless/high-bitrate editing with low-complexity frame tiling for multi-core CPUs. *(Verified secondary source)*
* **VVC (H.266) [Standardized, Shipping in Products]:** Android 17 introduces native support (`MIMETYPE_VIDEO_VVC`) via vendor Codec2 implementations. Late 2026 Snapdragon and MediaTek Dimensity chips feature hardware VVC decoders. It cuts HEVC bitrates by ~50% but heavily taxes software decoding. *(Verified primary source)*
* **AV2 [Standardized, No Hardware Yet]:** AOMedia finalized the 1.0 specification on May 28, 2026. It yields ~30% fewer bits than AV1, but hardware decoders will not hit Android SoCs until late 2027/2028. *(Verified secondary source)*
* **MPEG-5 Part 2 LCEVC [Standardized, Open-Source Implementation]:** A thin software layer that scales up a low-res base stream (encoded by existing HEVC/AV1 hardware). It cuts bitrates by 10-25% over HEVC and drastically saves battery compared to full software encoding. *(Verified secondary source)*
* **Neural Video Coding (NVC) [Open-Source Implementation, Research Prototype]:** AI codecs now match or beat VVC. Microsoft's MLVC was open-sourced in July 2026 and runs in real-time on Qualcomm NPUs. *(Verified secondary source)*

---

## 3. Inventions and Design Choices

### Choice 1: The Lossless Strategy

*What does lossless mean here?* We define it as mathematically bit-exact reconstruction of the YUV frames output by the camera ISP (Image Signal Processor) before conventional chroma-subsampling degradation.

* **Decision:** Use the **APV Codec** (via OpenAPV) for Android 16+, falling back to **HEVC Main 10 Lossless** via Android `MediaCodec`.
* **Why:** APV is explicitly designed to replace heavy ProRes pipelines on mobile, offering parallel encoding and lossless preservation. High-end 2027 mobile memory bandwidth can sustain APV's requirements.
* **Risk & Test:** If APV software encoding chokes thermal limits on mid-range devices, we will fail over to hardware HEVC. Test: 10 minutes of 4K60 lossless capture; monitor thermal throttling and frame drops.

### Choice 2: The Lossy Engine & NPU-Gated Residual Coding

To offer user-selectable tradeoffs (Quality vs. Battery), we cannot rely on a single codec.

* **Decision:** Implement a **Base + Enhancement** architecture.
* **Battery Mode:** GPU downscales video 2x. Hardware HEVC encodes the base. CPU calculates LCEVC residuals (Hadamard transforms).
* **Max Quality Mode (Invention):** We will invent an **NPU-Gated Neural Residual Layer**. Using a hardware HEVC/VVC base encoder, we subtract the reconstructed base from the original frame. Instead of LCEVC's math transforms, we pass this residual to a lightweight neural network (derived from MobileNVC/MLVC) running on the phone's NPU via NNAPI.

* **Addressing the Decoder Problem:** If the decoder uses machine learning, how do we guarantee bit-exact playback across different phone NPUs (which have different floating-point drift)?
* **Solution:** The Neural Residual Layer operates entirely in INT8 quantization. We will enforce deterministic integer-only math on the NPU, bypassing floating-point divergence.

* **Risk & Test:** NPU memory bandwidth overhead. Test: Encode a 1080p60 stream using the NNAPI residual model. If NPU power draw exceeds 1.5W, kill the neural path and default to LCEVC.

### Choice 3: Container Format

* **Decision:** **MP4 (ISO Base Media File Format).**
* **Why:** Android 16 natively supports APV in MP4, and Android 17 supports VVC in MP4. By standardizing on MP4, we guarantee compatibility with Android's `MediaExtractor` API and system sharing intents.

---

## 4. Implementation Plan

### Architecture & Tools

* **Languages:** Kotlin (UI and background service orchestration), C++20 (Core encoding pipeline), Python/PyTorch (for training the neural residual model off-device).
* **Libraries:** Android NDK, `MediaCodec` C-API (for zero-copy hardware access), OpenAPV, libLCEVC, Android NNAPI / TensorFlow Lite (for NPU execution), Vulkan Compute (for color space conversions and downscaling).
* **Metrics for Success:** VMAF (Visual Multi-Method Assessment Fusion) > 95 for high quality; Battery depletion rate (mW) during a 30-minute encode; encoding FPS.

### Phases & Milestones

**Phase 1: Foundation (Months 1–2)**

* *Goal:* C++ pipeline capturing Camera2 API YUV frames directly into Vulkan memory.
* *Milestone 1:* Hardware HEVC base + LCEVC software layer working in real-time.
* *Go/No-Go:* Must achieve 1080p60 encoding with < 15% battery drain per hour.

**Phase 2: The Lossless Core (Months 3–4)**

* *Goal:* Integrate OpenAPV for bit-exact capture.
* *Milestone 2:* Lossless APV capture and decoding verified against a byte-level YUV diff script.
* *Go/No-Go:* APV must sustain 4K30 on a 2026/2027 flagship without dropping frames.

**Phase 3: Neural Enhancement Layer (Months 5–7)**

* *Goal:* Train an INT8 residual compression model and deploy it via NNAPI.
* *Milestone 3:* NPU-Gated Neural Residuals functioning on a target Qualcomm NPU.
* *Go/No-Go:* The Neural mode must beat VVC's VMAF score at equivalent bitrates. If it fails, we pivot to hardware VVC entirely.

**Phase 4: Optimization & Release (Months 8–10)**

* *Goal:* User-facing tradeoff sliders, battery heuristics, and release packaging.
* *Organization:* Monorepo architecture. `core-codec` (C++), `app-ui` (Kotlin), `ml-models` (TFLite). Shipped as a standard Android App Bundle (AAB) with dynamic delivery for ML models.

### Legal and Licensing Strategy

* **Patent Risks:** VVC is encumbered by patent pools (Access Advance). AV2's royalty-free status is currently under legal scrutiny.
* **Strategy:** By utilizing the Android OS's built-in `MediaCodec` for VVC/HEVC, the SoC vendor (Qualcomm/MediaTek) and device OEM hold the hardware decoder licenses. LCEVC has a software licensing model that we must license for commercial deployment. Our custom NPU neural codec will be entirely proprietary and royalty-free.

---

## 5. Appendices

### A. Self-Audit & Weakest Claims

* **Weakest Claim:** Running neural residuals on the NPU deterministically across all Android vendors (Qualcomm, Google Tensor, MediaTek) using INT8 is notoriously difficult due to NNAPI driver fragmentation. *Mitigation:* We may need to bundle a CPU fallback (XNNPACK) to guarantee deterministic decoding if the NPU drifts.
* **Unverified:** While APV is mandatory in Android 16, the efficiency of Samsung's OpenAPV software implementation on non-Samsung ARM chips remains unproven.
* **Top Next Experiments:**

1. Port Microsoft MLVC to Android NNAPI and benchmark execution time on a Snapdragon Hexagon NPU.
2. Run a byte-diff on OpenAPV output vs raw camera YUV 4:2:0.
3. Measure the baseline power draw (in mW) of Vulkan image downscaling vs CPU downscaling on a mid-tier Android phone.

### B. Claims & Sources

* **** *Samsung APV RFC 9924 and Android 16 support.* Verified (primary/secondary source). Date: April 15, 2026. URL: `[https://news.samsung.com/global/editorial-how-apv-is-elevating-mobile-video-to-professional-standards](https://news.samsung.com/global/editorial-how-apv-is-elevating-mobile-video-to-professional-standards)`
* **** *AV2 finalized May 2026, 30% fewer bits.* Verified (secondary source). Date: October 12, 2026. URL: `[https://www.forasoft.com/learn/video-encoding/articles/codec-history-h120-to-av2](https://www.forasoft.com/learn/video-encoding/articles/codec-history-h120-to-av2)`
* **** *LCEVC (MPEG-5 Part 2) architecture and gains.* Verified (primary/secondary source). Date: October 26, 2026. URL: `[https://www.forasoft.com/learn/video-encoding/articles/lcevc-explained](https://www.forasoft.com/learn/video-encoding/articles/lcevc-explained)`
* **** *Real-time Neural Video Compression on Mobile.* Verified (research paper). Date: November 15, 2023. URL: `[https://arxiv.org/html/2310.01258v3](https://arxiv.org/html/2310.01258v3)`
* **** *Microsoft MLVC open-sourced July 2026.* Verified (secondary source). Date: October 16, 2026. URL: `[https://www.forasoft.com/learn/video-encoding/articles/future-codecs-av2-neural-end-to-end](https://www.forasoft.com/learn/video-encoding/articles/future-codecs-av2-neural-end-to-end)`
* **** *VVC support in Android 17 via Codec2.* Verified (primary source). Date: October 09, 2026. URL: `[https://source.android.com/docs/core/media/vvc](https://source.android.com/docs/core/media/vvc)`
* **** *Hardware VVC decoders in Qualcomm/MediaTek (2026).* Verified (secondary source). Date: September 23, 2026. URL: `[https://www.decodetv.com/2026/09/23/vvc-gets-a-boost-with-qualcomm-and-mediatek-announcements/](https://www.decodetv.com/2026/09/23/vvc-gets-a-boost-with-qualcomm-and-mediatek-announcements/)`
* **** *VVC decoding energy demands.* Verified (research paper). Date: February 14, 2024. URL: `[https://arxiv.org/html/2402.09001v1](https://arxiv.org/html/2402.09001v1)`

### C. Glossary

* **LCEVC:** Low Complexity Enhancement Video Codec. Uses a low-res base stream + high-res residual data to save compute.
* **NPU:** Neural Processing Unit. Dedicated AI silicon on mobile SoCs.
* **VMAF:** Video Multimethod Assessment Fusion. A perceptual video quality metric developed by Netflix.
* **APV:** Advanced Professional Video. A new codec optimized for visually lossless, high-bitrate video editing rather than streaming.
