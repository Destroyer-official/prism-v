# P10 Digest: NexusCodec — Next-Generation Android Video Compression System (2027–2030)

**Source File:** `research_implementation_plane10.md`  
**Total Lines:** 165 | **Bytes:** 15,790  
**Simulation Date:** November 14, 2026  
**Author / Attributed Identity:** Unspecified Architecture Plan  

---

## 1. Thesis
P10 proposes **NexusCodec**, a hybrid compression architecture that merges deterministic hardware video encoding with edge Neural Video Compression (NVC):
- **Lossy Mode (Symbiotic Layered Neural Compression / Zero-Drift DTN-MP4):**
  - Pure neural video codecs are rejected due to cross-chip floating-point drift and battery drain.
  - Instead, the device's hardware `MediaCodec` encodes a 720p/1080p AV1 "Base Track".
  - The mobile NPU computes a "Neural Residual" (missing high-frequency details between the base track and the original 4K frame), compressed into a compact latent stream stored in a custom metadata track (`'nres'`) inside a standard ISOBMFF `.mp4` container.
  - During playback, legacy devices play the standard AV1 base track smoothly. NexusCodec devices decode the `'nres'` track via NPU to reconstruct 4K details in real-time. Because enhancement residuals are strictly feed-forward and never looped back into the motion-compensation reference buffer, **cross-device pixel drift is completely eradicated**.
- **Lossless Mode (Strictly-Quantized NPU Predictive Coding):**
  - Restricted to raw Camera2 API capture (YUV 4:2:0/4:2:2 10-bit/12-bit) or imported ProRes.
  - An NPU-based optical flow + frame synthesis model predicts Frame N from Frame N-1.
  - The integer difference (residual) between the predicted frame and the raw sensor frame is calculated on CPU and compressed via Zstd (Finite State Entropy).
  - Requires strict INT8 quantization of the NPU model to ensure bit-exact identical predictions across devices.
- **Shared Infrastructure:** Rust core engine (`nexus-core`), LiteRT C++ API for NPU inference, Vulkan GPU shaders for surface fusion, and a WorkManager deferred compute daemon.

---

## 2. Technical Decisions
- **Architecture & System Design:** Two-layer hierarchical lossy encoding (hardware AV1 base + neural latent enhancement) and predictive lossless residual coding.
- **Algorithms & Codecs:**
  - Lossy: Hardware MediaCodec AV1 (`av01`), DCVC-style latent feature residual encoder.
  - Lossless: NPU INT8 optical flow prediction + CPU Zstd entropy coding; JPEG XL Part 3 for lossless keyframes.
- **Machine Learning & Runtimes:**
  - Framework: LiteRT C++ API (targeting Qualcomm QNN and MediaTek Neuron delegates).
  - Training: PyTorch / Python on Vimeo-90K, BVI-DVC, and open 4K mobile datasets.
  - Delivery: Model weights (<15 MB) delivered dynamically via Google Play Feature Delivery.
- **Languages & Tools:**
  - Rust: Container parsing/muxing (`mp4` crate), Zstd entropy coding, pipeline orchestration via JNI.
  - C++ (NDK): LiteRT delegate integration.
  - Kotlin + Jetpack Compose: Android application, WorkManager daemon, Media3 ExoPlayer custom `Renderer`.
  - Vulkan: Shaders for fusing the hardware AV1 base surface with the NPU latent enhancement surface.
- **Hardware Utilization:**
  - SoC VPU: Hardware AV1 encode/decode for base track.
  - NPU: Latent decoding and INT8 predictive frame synthesis.
  - GPU: Vulkan compute/composition shaders.
  - CPU: Rust orchestration, Zstd compression/decompression.
- **Container Design:** Dual-Track MP4 (`DTN-MP4`): ISOBMFF container containing standard `av01` video (`trak 1`), `mp4a` audio (`trak 2`), and a custom `'nres'` metadata track (`trak 3`) holding chunk-based latent byte streams.
- **Android Integration:** Camera2 API, `AMediaCodec`, Jetpack Media3 (ExoPlayer), `WorkManager` (deferred compute on charging + idle).

---

## 3. Inventions & New Ideas
- **Zero-Drift DTN-MP4 (P10-I01):** Decouples neural enhancement from the temporal reference loop by storing NPU latent residuals in a secondary MP4 track (`'nres'`), guaranteeing backward compatibility on standard players and mathematically preventing cross-device drift.
- **NPU-Assisted INT8 Predictive Lossless (P10-I02):** Uses an INT8 optical flow network on the NPU to predict adjacent frames, storing only the raw pixel residual compressed with Zstd.
- **Deferred Compute Daemon (P10-I03):** Captures high-bitrate AV1 proxy in real-time, deferring heavy NPU latent extraction or lossless packing to idle charging states via Android WorkManager.
- **Dual-Surface ExoPlayer Vulkan Fusion (P10-I04):** Custom Media3 `Renderer` combining decoded hardware AV1 frames and NPU-upscaled latent frames in a single Vulkan composition pass.

---

## 4. Plan Content
- **Phases & Roadmap (18 Months):**
  - *Phase 1 (M1–2):* Research & Desktop Prototyping (Python prototype; kill if VMAF < AV2).
  - *Phase 2 (M3–6):* Android Core & ML Porting (Rust/LiteRT; kill if playback < 24 fps or device > 45°C).
  - *Phase 3 (M7–10):* Lossless Engine & Daemon (INT8 prediction; kill if Pixel vs Galaxy prediction differs by 1 byte).
  - *Phase 4 (M11–14):* Client UI, Media3 & Beta (ExoPlayer custom renderer, Vulkan fusion).
  - *Phase 5 (M15–18):* Optimization & Launch (QA, battery profiling, Play Store release).
- **Team & Budget:** 6 engineers (2 ML Video Researchers, 2 Rust Systems, 2 Android Native/Vulkan). Budget: $3.5M–$4.5M (including compute).
- **Testing & Success Metrics:**
  - Lossy: VMAF > 95 at 20% lower bitrate than AV2; playback ≥ 24 fps on Snapdragon 8 Gen 5.
  - Lossless: Compression ratio > 4.0:1 on raw 10-bit YUV.
  - System: Battery drain < 1% per minute of deferred encoding.
- **Legal & IP Strategy:** Rejects VVC/HEVC patent pools entirely. Relies on royalty-free AV1 base. Open-sources DTN-MP4 architecture under Apache 2.0; retains proprietary rights over trained model weights.

---

## 5. Numbers & Quantitative Claims
- **Lossy Gain:** 30–45% smaller file size than AV1 at equivalent VMAF `[estimate]`; 60% lower encoding battery drain than pure software AV2.
- **Lossless Ratio:** 4.5:1 to 5:1 on raw 10-bit YUV (compared to ~2.5:1 for standard FFV1) `[estimate]`.
- **NPU Performance:** Snapdragon 8 Elite Gen 5/6 NPU delivers 100–120+ TOPS with 16MB FlexCache `[verified (primary source)]`.
- **Latency Target:** < 33 ms per frame for 4K NPU enhancement on Snapdragon 8 Gen 5.
- **Model Size:** Latent enhancement model < 15 MB.

---

## 6. Citations & Live Verification Status
- *AOMedia AV2 Bitstream Specification (Jun 2026)*: Verified primary standard milestone.
- *DCVC-UF (CVPR Jun 2026, Microsoft Research)*: Verified primary paper.
- *Qualcomm Snapdragon 8 Elite Gen 5/Gen 6 Tech Briefs (Late 2026)*: Verified primary hardware specs.
- *S-NVRC (arXiv, Nov 2026)*: Verified primary literature.

---

## 7. Strengths
- **Practical Ecosystem Bridge (§1, §3.1, §4.6):** The DTN-MP4 architecture completely solves the codec adoption cold-start problem by playing universally on legacy devices (as standard AV1) while unlocking 4K neural enhancement on supported hardware.
- **Decoupled Drift Elimination (§3.1):** By keeping the neural network strictly on a feed-forward enhancement path outside the motion compensation loop, decoder divergence across varying NPU hardware cannot corrupt subsequent frames.
- **Explicit Self-Audit (§5.3):** Accurately highlights the fragile nature of INT8 cross-vendor determinism as its weakest claim.

---

## 8. Weaknesses
- **Near-Certain Failure of INT8 NPU Lossless Determinism (§3.2, §5.3):** Cross-vendor NPU runtimes (Qualcomm QNN vs MediaTek NeuroPilot vs Google TPU) reorder, fuse, and quantize integer kernels with differing rounding behaviors, almost certainly causing 1-bit discrepancies that break lossless reconstruction.
- **Inflated Lossless Ratio Claims (§1, §3.2):** Claiming a 4.5:1 to 5:1 lossless ratio on natural camera video with simple Zstd residual compression contradicts raw sensor noise entropy floors (typically 2.2:1–3.5:1).
- **High Budget & Slow Schedule (§4.5):** $3.5M–$4.5M over 18 months is disproportionate for a 6-person team building on existing AV1 hardware encoders and LiteRT delegates.
- **Brief Specification (§1–§5):** At 165 lines, lacks detailed zero-copy buffer layouts, thermal mitigation curves, and battery measurement data.

---

## 9. Scores
- **Depth:** 3/5 — High-level architecture is conceptually clear, but lacks implementation-level buffer management and shader details.
- **Accuracy:** 4/5 — Sound understanding of AV1/AV2 tradeoffs and NVC drift risks, though lossless ratio estimates are unrealistic.
- **Originality:** 4/5 — The DTN-MP4 dual-track backward-compatible approach is an excellent practical innovation.
- **Feasibility:** 3/5 — Lossy layered track is highly feasible, but INT8 NPU lossless determinism is virtually impossible across mobile SoCs.
- **Completeness:** 3/5 — Covers phases, team, and metrics, but lacks depth in memory and device matrix specifications.
- **Evidence:** 3/5 — Cites key 2026 developments, but relies heavily on optimistic theoretical estimates for compression ratios.
