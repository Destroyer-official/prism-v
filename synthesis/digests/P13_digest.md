# P13 Digest: Master Research Ledger — Verified Facts with Dates (2026–2030)

**Source File:** `research_implementation_plane13.md`  
**Total Lines:** 125 | **Bytes:** 23,488  
**Simulation Date:** November 14, 2026  
**Author / Attributed Identity:** Unspecified Research Lead / Fact Verification Group  

---

## 1. Thesis
P13 is not an architectural design or application implementation plan; it is a **Master Research Ledger of 87 rigorously tagged and dated empirical facts**. It serves as the foundational ground-truth reference for validating codec standards, neural network realities, mobile hardware capabilities, independent benchmark critiques, and legal/patent exposure across the 2026–2030 Android landscape.

---

## 2. Technical Findings Summary by Domain

### A. Standards & Hybrid Codecs (A1–A17)
- **AV2:** Bitstream specification v1.0.0 and AVM reference software v1.0.0 published on May 28, 2026 (announced June 9, 2026), superseding Draft 13 (Jan 2026). Yields ~30%–40% compression gain over AV1. YouTube commits to early 2027 web streams. However, consumer silicon decoders are absent in late 2026; first SoCs expected 2027–2028.
- **VVC (H.266):** Android 17 adds `video/vvc` MIME and MP4 extraction, but AOSP supplies **zero software decoder and zero encoding support**. MediaTek Dimensity 9600 Pro (Oct 2026) is the first smartphone SoC with hardware VVC decode.
- **APV (Advanced Professional Video):** Standardized as IETF RFC 9924 in Feb 2026. Intra-only, visually lossless mezzanine up to 2 Gbps. Snapdragon 8 Elite Gen 5 is the first mobile chip with hardware APV; ships in Galaxy S26 Ultra up to 8K30.
- **FFV1 & JPEG XL:** FFV1 RFC 9043 (versions 0, 1, 3) is informational; JPEG XL outperforms lossless WebP by 9%–17% and AVIF by 30%–120% on photo sets (Archibald Oct 2026 benchmark).

### B. Neural Video Compression (B1–B14)
- **DCVC-RT & DCVC-UF:** DCVC-RT (CVPR 2025) delivers 125.2 fps encode / 112.8 fps decode at 1080p on A100 (~21% BD-rate vs VTM). DCVC-UF (CVPR 2026) introduces chunk-based parallel multi-frame coding.
- **MLVC (Microsoft, ECCV 2026):** Solves cross-device NPU floating-point drift via **Scale-Sending** (derives entropy scale indices from quantized hyperlatent $\hat{z}$ via fixed factorized model). Replaces complex activations with pure ReLU/LeakyReLU. Open-sourced under MIT.
- **Quantization Catastrophe:** Integer-Centric NVC (Jia et al.) was **rejected at ICLR 2026** (Jan 2026) because strict quantization degraded RD curves significantly at high bitrates. Streamable NVC (Arunruangsirilert et al., Aug 2026) showed INT8 can cause an **80% BD-rate penalty** and INT16 incurs a **5× slowdown** on mobile accelerators; proposes FP16 P-frames + FP32 anchor resets.
- **Mobile Real-Time Throughput:** MobileNVC (WACV 2024) achieved 38.9 fps 1080p decode on Snapdragon 8 Gen 2, but encode was ~3 fps. MLVC-S reaches 1080p real-time (39.5 fps) **only on Apple ANE**; Qualcomm Hexagon runs 1080p at 26.5/19.6 fps, meaning real-time 1080p NVC remains unachieved on current Android NPUs.

### C. Android Platform & Hardware (C1–C8, F12–F20)
- **Runtime Deprecations:** NNAPI was deprecated in Android 15 (API 35); LiteRT 2.3+ is Google’s official NPU runtime.
- **Google Play Requirements:** Target API 36 (Android 16) mandatory from Aug 2026; 16 KB page-size compatibility mandatory from Nov 2025 (new) / May 2026 (updates). Base module download size capped at $\le$200 MB.
- **Background Execution Limits:** Android 15+ caps `dataSync` and `mediaProcessing` foreground services at **6 hours per 24-hour period**.
- **Storage Speeds:** Samsung UFS 5.0 (June 2026) delivers up to 10.8 GB/s sequential read and 9.5 GB/s sequential write (vs 4.2 / 2.8 GB/s on UFS 4.0).

### D. Independent Benchmarks & Critiques (D1–D12)
- **MSU 2025/2026 NVC Evaluation:** On objective metrics (PSNR, SSIM, VMAF), MLVC needs **>30% MORE bitrate** than reference x265, while DCVC-RT is 10%–20% better. In subjective tests, MLVC matches SVT-AV1/VVenC. NVCs are competitive with open-source encoders, but lag leading proprietary implementations.
- **Decoder Power Gap (Jan Ozer Analysis):** Running 50% NPU + CPU for neural decoding is estimated to consume **~5× more power** than fixed-function SoC hardware decoders.
- **dav2d Complexity:** VideoLAN's `dav2d` is roughly **5× more computationally complex** than `dav1d`.

### E. Legal, Patents & Licensing (E1–E11)
- **Microsoft Scale-Sending Patent:** US Patent Application **US 2026/0254962 A1** (filed Feb 2025, published Aug 27, 2026) specifically claims transmitting entropy scale parameters from hyperlatents.
- **No Patent Grant in MIT:** MLVC's MIT license covers copyright only, providing zero patent immunity.
- **Access Advance MCBA Discount Sunset:** Announced on Nov 11, 2026 that 50% dual-codec bridging discounts for HEVC + VVC will phase down starting Jan 1, 2027.

### F. Quality Metrics & Evaluation (F1–F11)
- **VMAF v1:** Released June 2026; adds AIM (additive impairment) component; enables **VMAF-NEG by default**. Phone viewing distance is modeled at 4–5H (vs 3H for 1080p desktop).
- **FGS Bitrate Reductions:** Netflix confirmed a **36% average bitrate reduction** at $\ge$1080p using AV1 film-grain synthesis in production.
- **ColorVideoVDP (CVVDP):** Full-reference spatio-temporal metric modeling luminance, color, and display specifications.

---

## 3. Inventions & Key Contributions
While P13 presents no architectural inventions, it contributes high-value empirical insights:
- **Primary Source Ledger (P13-I01):** Exhaustive catalog of 87 primary-sourced facts spanning standards, MLVC patents, MSU benchmarks, and Android 17 media stack constraints.
- **Identification of US 2026/0254962 A1 (P13-I02):** Uncovers Microsoft's active patent application over Scale-Sending, proving that copying MLVC's entropy design incurs patent infringement risk.
- **Forewarning of 6-Hour Foreground Service Limit (P13-I03):** Identifies Android 15+ platform timeout capping `mediaProcessing` services at 6h/24h, requiring persistent `WorkManager` chunking.

---

## 4. Scores
- **Depth:** 5/5 — Unmatched factual concentration covering every niche from UFS 5.0 bus speeds to USPTO patent numbers.
- **Accuracy:** 5/5 — Flawless epistemic rigor; every single claim carries exact dates, sources, and epistemic tags.
- **Originality:** 3/5 — Reference ledger format; does not attempt to design a novel codec architecture.
- **Feasibility:** 5/5 — Fact base ensures any plan built on it is grounded in mobile reality.
- **Completeness:** 4/5 — Exhaustive as a fact reference; lacks operational schedule or budgeting.
- **Evidence:** 5/5 — The gold standard of evidence in the entire 18-plan corpus.
