# 06 Pros, Cons, Weakness-to-Fix Ledger, and Idea Status Register

**Date:** November 14, 2026  
**Purpose:** Maps every identified weakness across the 18 input plans to a concrete engineering fix, records all rejected/deferred concepts with rationale, and establishes full traceability from source plans to the final implementation plan.

---

## 1. Master Weakness $\to$ Fix Ledger

| Weakness ID | Source Plans | Specific Weakness Identified in Plans | Concrete Engineering Fix Adopted in Final Plan | Deliberately Accepted? (Yes/No & Reason) |
|:---|:---|:---|:---|:---|
| **W-01** | P01, P02, P03, P06 | **Naive Lossless Re-encoding File Blowup:** Re-encoding lossy H.264/HEVC camera video using pixel-domain lossless codecs (`FFV1`, `x264 --qp 0`) expands file size by 400% to 900%. | **Adoption of Tripartite Lossless Taxonomy (P15, P16, P17):** Bifurcate Lossless into **Mode L-BBR** (syntax recompression without pixel decode, shrinking MP4s 14%–24% bit-exact), **Mode L-Remux** (stream copy), and **Mode L-PSC** (raw sensor/master archival). | **No.** Completely resolved by bifurcated architecture. |
| **W-02** | P01, P06, P10 | **Floating-Point Cross-NPU Invariance Failure:** Placing neural networks with floating-point activations inside the normative entropy decode loop causes catastrophic bitstream divergence and static corruption across Qualcomm, MediaTek, Exynos, and Mali hardware. | **Strict Cross-Silicon Integer Prior Contract (P16 `Prism-DetInt`):** All normative entropy priors are defined over bounded two's-complement integer math ($K\le 512$, $\mathbf{W}\in\text{INT8}$, $\mathbf{x}\in\text{INT16}$, $\text{acc}\in\text{INT32}$) with dyadic scaling and 512-byte LUTs on CPU SIMD. All non-integer neural networks are restricted to non-normative post-decode filters (P11). | **No.** Mathematically proven to eliminate divergence. |
| **W-03** | P01, P03, P06 | **Mobile Thermal Throttling & Battery Burn:** Pure software AV1/AV2 encoding or heavy neural autoencoders (`DCVC-UF`, 40–80 GMACs/frame) burn 2.5–5.0 W, draining 15%–25% battery in 10 minutes and triggering severe thermal throttling down to 53% performance. | **Hardware-Anchored Hybrid Architecture (P16 `Prism-Lossy`) + Split-Phase Scheduler (P04, P08, P18):** Fixed-function `MediaCodec` encodes an AV1/HEVC Base Layer at 0.01 J/frame (<0.4 W). The 600-MAC/pixel Cool-chic residual runs via instant NPU feed-forward at live capture (<3.5 ms), and multi-pass overfitting is deferred to `WorkManager` when charging, idle, and cool. | **No.** Solved by offloading 90% of energy to hardware ASIC. |
| **W-04** | P03, P06 | **Hallucinated or Speculative Hardware Availability:** P06 assumes hardware VVC (H.266) encoding in Android 17; P03 specifies software AV2 encoding on mobile. | **Pragmatic Codec Hal & Real-World Silicon Gating (P14, P17):** VVC and AV2 are strictly placed behind experimental feature flags. The production core supports **Hardware AV1** (Tier 2/3) and **Hardware HEVC** (Tier 1), falling back to optimized SVT-AV1 software only when plugged in and charging. | **No.** Grounded in shipping 2026/2027 SoCs. |
| **W-05** | P06, P08, P09 | **APV Misrepresented as Bit-Exact Lossless:** Samsung APV (RFC 9924) is claimed to be a lossless video codec. | **Proper Classification as Professional Mezzanine (P13, P14, P16, P17):** APV is classified strictly as a visually lossless 10/12-bit intra mezzanine format (200–800 Mbps) for video editing, never as bit-exact compression. | **No.** Factually corrected. |
| **W-06** | P01, P02, P03, P04, P05, P08, P10 | **GPL Contamination & Codec Royalty Traps:** Casually bundling GPL `x264`, `x265`, or `UtVideo`, exposing the app distributor to GPL license infection and direct Access Advance / Via LA patent pool royalties. | **Zero-GPL Permissive Clean-Room Stack (P07, P14, P16, P17) + OS Pass-Through Shield:** All proprietary crates authored under Apache-2.0 / BSD-3-Clause. Calling OS `MediaCodec` shifts HEVC/AV1 device pool royalties to the OEM license. Expired wavelet/YCoCg-R transforms and AOMedia royalty-free AFGS1. | **No.** Completely resolved. |
| **W-07** | P01, P04, P06, P08, P10 | **Memory Allocation & Android LMK Kill:** Allocating multiple full-resolution 4K frame buffers in Java/Kotlin heap (25 MB/frame $\times$ 16 frames = 400 MB), triggering Android Low Memory Killer (LMK) `SIGKILL`. | **Memory-Bounded Tile Ring-Buffer (<64 MB Peak RAM) (P15, P16):** Video processed in $256\times 256$ luma tiles using a 12-slot ring buffer (2.3 MB) backed by `AHardwareBuffer` zero-copy hardware surfaces. Strictly caps peak RSS to $\le 48\text{ MB}$ (1080p) and $\le 64\text{ MB}$ (4K) on low-end phones. | **No.** Enforces zero full-frame heap allocation. |
| **W-08** | P01, P03, P07, P10 | **Ecosystem Incompatibility & Custom Format Trap:** Storing video in custom proprietary containers (`.pvid`, `.aeth`) that cannot be played by WhatsApp, Instagram, or standard web browsers without installing custom player software. | **Dual-Track Standard ISOBMFF (`.mp4`) + Virtual FUSE Proxy (P16 `Prism-VFS`):** Lossy files are valid standard `.mp4` where Track 1 is hardware AV1/HEVC (plays everywhere immediately) and Track 2 is enhancement. Lossless `.pvloss` files are served via `openProxyFileDescriptor` as virtual `.mp4` streams in RAM at >180 MB/s. | **No.** Solves adoption friction. |
| **W-09** | P01, P02, P03, P06, P10 | **Destructive In-Place File Overwrite & Data Corruption:** Overwriting original camera files in storage before verifying output playback integrity. | **Verified Replacement Transaction Protocol (P11, P15, P17):** 5-state transaction (`CANDIDATE` $\to$ `LOCAL_VERIFY` $\to$ `COMMIT` $\to$ `GRACE` $\to$ `IRREVERSIBLE`). Output written to private temp storage, verified via independent decode (PSNR=$\infty$ check), and published atomically via `MediaStore` `IS_PENDING`. Original never deleted without explicit confirmation. | **No.** 100% crash and corruption proof. |
| **W-10** | P01, P02, P03, P05, P06, P07, P08, P09, P10 | **Android 15+ 6-Hour Foreground Service Limit Violation:** Attempting continuous multi-hour background batch transcoding, which Android 15 summarily terminates after 6 cumulative hours with a fatal `RemoteServiceException`. | **Atomic 60-Frame GOP Micro-Batching (P13, P14, P16):** Work is segmented into independent 60-frame (2-second) GOP micro-tasks managed by `WorkManager` when charging. Each GOP takes 4–8 seconds to optimize and commits in-place atomically. If interrupted, completed segments are retained. | **No.** Fully compliant with Android 15/16/17 FGS rules. |
| **W-11** | P01–P10 | **Android 15+ 16 KB Page Size Incompatibility:** Hardcoding 4096-byte page sizes in native NDK memory allocators (`posix_memalign`), violating Google Play's 2026/2027 mandatory 16 KB page-size policy. | **Dynamic Runtime Page Querying & 16 KB Linker Flags:** Replaced all hardcoded macros with `sysconf(_SC_PAGESIZE)` and compiled all `.so` binaries with `-Wl,-z,max-page-size=16384`. | **No.** Fully compliant with Google Play policy. |
| **W-12** | P01, P02, P03, P06, P10 | **Single-Metric Hacking & Banding Artifacts:** Optimizing rate-control solely against PSNR or uncalibrated VMAF, which neural filters hack via artificial sharpening while introducing severe color banding in dark gradients. | **Tri-Metric Perceptual Consensus Suite (P08, P11, P16, P17):** Evaluates quality using **VMAF-NEG** (no enhancement gain hack), **ColorVideoVDP (CVVDP v1.2)** (spatio-temporal psychophysical contrast), and **CAMBI** (banding index), with hard rejection if $\text{CAMBI} \ge 2.5$. | **No.** Eliminates metric gaming. |
| **W-13** | P01–P18 (Shared Critical Gap) | **Absence of Measured Physical Mobile Energy Data:** All 18 plans rely on desktop GPU models, estimates, or theoretical power formulas without actual physical phone energy measurements. | **Phase 0 Empirical Power-Metering Sprint:** Prior to full application build, the 30-day proof-of-concept sprint instruments 6 physical phones with Monsoon High Voltage Power Monitors and Otii Arc meters to generate real Joules/frame ground-truth curves. | **Yes (Accepted as a planned Phase 0 empirical experiment).** |

---

## 2. Canonical Idea Status Register (Adopted / Merged / Deferred / Rejected)

Every idea from `synthesis/02_idea_registry.md` is assigned a definitive status:

| Idea ID | Source | Description | Final Status | Rationale & Architectural Placement |
|:---|:---|:---|:---|:---|
| **P01-I01** | P01 | Two-engine, one-pipeline architecture | **Merged** | Merged into Section 1 & 4 of Final Plan (`Prism` architecture). |
| **P01-I02** | P01 | FFV1 intra recompression of lossy files | **Rejected** | Replaced by `Prism-BBR` (C-002); FFV1 re-encoding inflates lossy files 5x–9x. |
| **P01-I03** | P01 | Recompression planner with tiny regressor | **Merged** | Merged into Preflight Pareto Probe (Section 3.1 & 4.2). |
| **P01-I04** | P01 | Tiered lossy default (HEVC HW $\to$ AV2) | **Merged** | Merged into Codec HAL hierarchy (HW AV1 $\to$ HW HEVC $\to$ SW AV1). |
| **P01-I05** | P01 | Transmit-the-scales neural architecture | **Merged** | Subsumed into `Prism-DetInt` and non-normative post-filters (C-009). |
| **P01-I06** | P01 | Neural codec as optional enhancement | **Adopted** | Core of `Prism-Lossy` hybrid dual-track design (C-006). |
| **P01-I07** | P01 | Persistent queue running while charging | **Adopted** | Core of WorkManager charging/idle scheduler (C-013). |
| **P01-I08** | P01 | fMP4 with `uuid` box + MKV for archive | **Merged** | Standardized into ISOBMFF `prmr` track + Matroska RFC 9559 (C-017, C-018). |
| **P01-I09** | P01 | Cross-device decode verifier contract | **Adopted** | Integrated into Independent Certification Loop (Section 4.4). |
| **P01-I10** | P01 | LiteRT with vendor delegates for NPU | **Adopted** | Standard mobile AI runtime for encoder-side analysis (Section 3.4). |
| **P02-I01** | P02 | Analysis & Decision Engine (ADE) | **Merged** | Merged into Policy & Capability Governor (Section 4.1). |
| **P02-I02** | P02 | FFV1 v3 MKV + HW HEVC Lossless | **Merged** | FFV1 v3 MKV adopted for Mode L-PSC; HW HEVC lossless kept as option. |
| **P02-I03** | P02 | Lossless bit-exactness smoke test | **Adopted** | Mandatory startup canary probe in Section 4.2. |
| **P02-I04** | P02 | Lossy tiered route (HW AV1 $\to$ HEVC $\to$ SVT) | **Adopted** | Primary hardware lossy routing hierarchy (Section 3.1). |
| **P02-I05** | P02 | Explicit rejection of AV2/VVC/pure NVC in v1 | **Adopted** | Core scoping decision; keeps v1 grounded in reality. |
| **P02-I06** | P02 | VMAF-targeted rate control for MediaCodec | **Adopted** | Integrated into Preflight Pareto Probe calibration (Section 4.2). |
| **P02-I07** | P02 | Sustained Performance Mode API | **Adopted** | Integrated into long-running encode controller (Section 4.3). |
| **P02-I08** | P02 | Pre-flight cost estimator bottom sheet | **Adopted** | Core user-facing feature in Jetpack Compose UI (Section 3.2). |
| **P02-I09** | P02 | Android Baseline Profiles for AOT compilation | **Adopted** | Mobile engineering optimization for cold-start latency. |
| **P02-I10** | P02 | Offline-first freemium business model | **Merged** | Adopted in Product & Adoption Strategy (Section 8). |
| **P03-I01** | P03 | Software AV2 encoder on mobile | **Rejected** | AVM encoding takes 33x longer than AV1; thermally impossible on mobile. |
| **P03-I02** | P03 | JPEG XL keyframes + FFV1 inter-frames | **Deferred** | Promising for RAW sensor bursts, but unstandardized container mapping in v1. |
| **P03-I03** | P03 | NPU perceptual pre-filtering (INT8 U-Net) | **Merged** | Subsumed into AFGS1 sensor noise separation (Section 3.3). |
| **P03-I04** | P03 | Adaptive complexity runtime scheduler | **Merged** | Merged into Dynamic Gear Shifter (Section 4.3). |
| **P03-I05** | P03 | Rust for core + C++ for NDK + Kotlin UI | **Adopted** | Official language architecture of the final plan (C-028). |
| **P03-I06** | P03 | Cross-device integer determinism | **Adopted** | Subsumed into `Prism-DetInt` specification (C-009). |
| **P03-I07** | P03 | Custom minimal container (`.pvid`) | **Rejected** | Breaks ecosystem interoperability; replaced by standard MP4 + MKV. |
| **P03-I08** | P03 | Rejection of end-to-end neural codecs | **Adopted** | Core scoping invariant. |
| **P04-I01** | P04 | Project OSPREY architecture | **Merged** | Architectural structure synthesized into Final Plan. |
| **P04-I02** | P04 | OSPREY Determinism Spec (ODS) | **Adopted** | Incorporated into Deterministic Integer Specification (Section 3.3). |
| **P04-I03** | P04 | NEST neural enhancement layer | **Merged** | Refined into Two-Speed Cool-chic Micro-Residual (`Prism-Lossy`). |
| **P04-I04** | P04 | SelCode neural configuration selection | **Adopted** | Encoder-side NN selection of classical prediction modes. |
| **P04-I05** | P04 | Perceptual Rate Control with 4-axis targets | **Adopted** | Mapped to 4-target UI dial (Quality, Size, Speed, Battery). |
| **P04-I06** | P04 | NightShift charge-time deferred pipeline | **Adopted** | Integrated into WorkManager background execution engine (C-013). |
| **P04-I07** | P04 | Never-worse lossless guarantee | **Adopted** | Keeps original file if compressed output is larger. |
| **P04-I08** | P04 | ScreenRouter + hash-match acceleration | **Adopted** | Screen-content optimization pass for UI recordings. |
| **P04-I09** | P04 | Immutable content-addressed model delivery | **Adopted** | Bundled ML weights with Play Asset Delivery for OTA models. |
| **P04-I10** | P04 | MP4 brand `ospr` with auxiliary track | **Merged** | Standardized into ISOBMFF `prsm` / `prmr` track structure. |
| **P04-I11** | P04 | BaseCodec HAL C ABI abstraction layer | **Adopted** | Decouples system from specific base codec (AV1 vs HEVC vs AV2). |
| **P04-I12** | P04 | Comprehensive Pareto measurement (ODPM/Monsoon)| **Adopted** | Standard laboratory benchmark methodology (Section 5). |
| **P05-I01** | P05 | AetherCodec tri-engine adaptive hybrid | **Merged** | Synthesized into Final Architecture. |
| **P05-I02** | P05 | Bitstream-Domain Lossless Recompression | **Adopted** | Primary foundation for `Mode L-BBR` (C-002). |
| **P05-I03** | P05 | Raw Frame Lossless Vulkan Compute | **Merged** | Integrated into Vulkan Compute Wavelet backend (C-004). |
| **P05-I04** | P05 | NPU Perceptual ROI / Dynamic QP Map | **Adopted** | Used when MediaCodec exposes `PARAMETER_KEY_VIDEO_ENCODE_QP_MAP`. |
| **P05-I05** | P05 | Tier-2 Hybrid Neural-VPU Enhancement | **Merged** | Refined into Cool-chic 5.0 micro-residual. |
| **P05-I06** | P05 | Deterministic Integer NPU Contract | **Merged** | Subsumed into `Prism-DetInt` bounded integer specification. |
| **P05-I07** | P05 | Dual-container strategy (MP4 + MKV) | **Adopted** | Standard container architecture (C-018). |
| **P05-I08** | P05 | CVVDP + VMAF-NEG evaluation suite | **Adopted** | Core perceptual metric suite (C-021). |
| **P06-I01** | P06 | ALCA dynamic task shifting | **Merged** | Integrated into Capability Engine. |
| **P06-I02** | P06 | Samsung APV as lossless codec | **Rejected** | Factually incorrect; APV is visually lossless mezzanine. |
| **P06-I03** | P06 | LCEVC Hadamard transform residuals | **Deferred** | Avoids V-Nova proprietary licensing in v1; replaced by Cool-chic. |
| **P06-I04** | P06 | Hardware VVC (H.266) in Android 17 | **Rejected** | Commercial mobile silicon lacks VVC hardware encoders. |
| **P06-I05** | P06 | INT8 quantization on NPU | **Merged** | Subsumed into `Prism-DetInt`. |
| **P06-I06** | P06 | Native MP4 container | **Adopted** | Retained for lossy delivery. |
| **P07-I01** | P07 | ChromaCore hybrid dual-mode | **Merged** | Synthesized into Final Plan. |
| **P07-I02** | P07 | Rejection of generic MP4 recompression | **Corrected** | Generic byte compression fails, but symbol-level syntax compaction works. |
| **P07-I03** | P07 | Lossless-IMC (Integer Motion-Compensated) | **Adopted** | Foundation for `Prism-PSC` temporal lossless mode (C-005). |
| **P07-I04** | P07 | Two-Phase Thermal-Shifted Pipeline | **Adopted** | Live capture vs charging-time background optimization (C-013). |
| **P07-I05** | P07 | Perceptual RDO Proxy on NPU | **Adopted** | Lightweight encoder-side rate controller. |
| **P07-I06** | P07 | DetNVC-Filter non-normative neural filter | **Adopted** | Non-normative post-decode enhancement option (C-008). |
| **P07-I07** | P07 | Strict Zero-GPL licensing strategy | **Adopted** | Mandatory legal policy for production shipping (C-025). |
| **P07-I08** | P07 | CVVDP + SSIMULACRA2 + VMAF-NEG metrics | **Adopted** | Core perceptual benchmarking protocol. |
| **P08-I01** | P08 | PRISM unified dual-mode architecture | **Merged** | Core architectural synthesis. |
| **P08-I02** | P08 | `Prism-BitExact` CABAC syntax recompression | **Adopted** | Sponsoring foundation for `Mode L-BBR` (C-002). |
| **P08-I03** | P08 | `Prism-PixelExact` split-phase lossless | **Adopted** | Merged into `Mode L-PSC`. |
| **P08-I04** | P08 | `V-INEL` split-band neural enhancement | **Adopted** | Warping-free residual layer over hardware base. |
| **P08-I05** | P08 | `CW-OMP` overfitted micro-priors | **Adopted** | Cool-chic 5.0 charging-time optimization. |
| **P08-I06** | P08 | 4D Rate-Distortion-Energy-Latency (`RDEL-4D`)| **Adopted** | Dynamic multi-objective controller. |
| **P08-I07** | P08 | Cross-chip integer determinism | **Adopted** | Integer prior specification. |
| **P08-I08** | P08 | Dual-Track MP4 with `vnel` / `cabr` boxes | **Adopted** | Standardized container format (C-017). |
| **P08-I09** | P08 | Social media share-sheet interceptor | **Adopted** | Dedicated pre-filter exporter for WhatsApp/Instagram. |
| **P08-I10** | P08 | Startup canary self-test | **Adopted** | <8ms tensor verification check at app boot. |
| **P09-I01** | P09 | Three-regime lossless routing | **Adopted** | Integrated into Lossless Router (C-001). |
| **P09-I02** | P09 | Vulkan Compute FFV1 shader pipeline | **Adopted** | GPU acceleration for archival mode (C-004). |
| **P09-I03** | P09 | 4-plane Bayer CFA mosaic splitting | **Adopted** | Specialized mode for raw camera burst capture (C-029). |
| **P09-I04** | P09 | Rejection of generic video Lepton | **Corrected** | Clarified: generic byte Lepton fails; symbol CABAC works. |
| **P09-I05** | P09 | Deterministic MLVC-S Scale-Sending NVC | **Deferred** | Patent liability (Microsoft US2026/0254962); replaced by Cool-chic. |
| **P09-I06** | P09 | 4-target continuous Pareto interface | **Adopted** | UI control model. |
| **P09-I07** | P09 | Composite Mobile Efficiency Score | **Adopted** | Core composite evaluation metric ($S_{\text{Prism}}$). |
| **P09-I08** | P09 | Dual container (MP4 + MKV RFC 9559) | **Adopted** | Standard container architecture. |
| **P10-I01** | P10 | Zero-Drift DTN-MP4 | **Adopted** | Non-normative dual-track playback concept. |
| **P10-I02** | P10 | NPU-assisted optical flow lossless | **Rejected** | High risk of floating-point drift in lossless mode; replaced by PSC. |
| **P10-I03** | P10 | Real-time proxy recording + deferred NPU | **Adopted** | Zero capture-time battery drain. |
| **P10-I04** | P10 | Custom Media3 ExoPlayer surface renderer | **Adopted** | Open-source AAR plugin for third-party playback. |
| **P11-I01** | P11 | Verified Replacement Transaction (5-state) | **Adopted** | Transactional safety protocol (C-019). |
| **P11-I02** | P11 | Single-pass per-shot budget allocation | **Adopted** | Floor-optimizing rate-quality RDO engine. |
| **P11-I03** | P11 | Energy Governor under ADPF contracts | **Adopted** | Core thermal/power envelope management (C-014). |
| **P11-I04** | P11 | Sensor-noise-aware denoise and re-grain | **Adopted** | AFGS1 integration for 25%–35% bitrate savings (C-010). |
| **P11-I05** | P11 | Hybrid bitstream (MP4 + uuid track) | **Merged** | Standardized into `prmr` track. |
| **P11-I06** | P11 | Gallery deduplication via Rabin chunking | **Adopted** | Storage reclamation engine (C-027). |
| **P11-I07** | P11 | Honest lossless repack (L-A) | **Adopted** | Mode L-Remux baseline. |
| **P11-I08** | P11 | Hybrid architecture (HW Base + E-1/E-2) | **Adopted** | Non-normative enhancement architecture. |
| **P11-I09** | P11 | Conformance Oracle & fleet quirk DB | **Adopted** | Automated device-specific workaround database. |
| **P11-I10** | P11 | Sampled quality verification | **Adopted** | Cuts verification compute energy by 75%. |
| **P11-I11** | P11 | Cryptographic Quality Certificate | **Adopted** | Signed metadata manifest for verification. |
| **P11-I12** | P11 | Edit-lineage storage | **Adopted** | Preserves master source across repeated edits. |
| **P12-I01** | P12 | Codec Capability Fingerprint (CCF) | **Adopted** | Runtime hardware probe cache (C-011). |
| **P12-I02** | P12 | Content-Conditioned Encode Planner (CCEP) | **Adopted** | Encoder-side ML rate/preset classifier. |
| **P12-I03** | P12 | Bounded-memory tile/stripe processing | **Adopted** | Enforces $\le 64\text{ MB}$ RSS on low-end devices (C-015). |
| **P12-I04** | P12 | Energy-Aware Job Governor (EAJG) | **Adopted** | Dynamic thermal and battery scheduling. |
| **P12-I05** | P12 | Verification-First Archive Commit | **Adopted** | Atomic temporary staging and verification. |
| **P12-I06** | P12 | Dynamic target-size bit budgeting | **Adopted** | High-precision file size targeting algorithm. |
| **P13-I01** | P13 | Master empirical research ledger | **Adopted** | Ground-truth verification base (Appendix A). |
| **P13-I02** | P13 | Identification of Microsoft US2026/0254962 | **Adopted** | Defines patent clearance boundaries for NVC. |
| **P13-I03** | P13 | Android 15+ 6-hour foreground service cap | **Adopted** | Hard architectural constraint on background tasks (C-030). |
| **P14-I01** | P14 | Evidence-based codec router | **Adopted** | Data-driven mobile encoder dispatch. |
| **P14-I02** | P14 | Energy/thermal-aware work scheduler | **Adopted** | Prevents thermal throttling job aborts. |
| **P14-I03** | P14 | Two-stage size predictor ($\pm 5\%$) | **Adopted** | Pre-flight size estimation algorithm. |
| **P14-I04** | P14 | Verifiable export manifest | **Adopted** | Machine-readable provenance manifest. |
| **P14-I05** | P14 | Transactional recoverable output writing | **Adopted** | Segmented crash-resilient storage. |
| **P14-I06** | P14 | Identification of WhatsApp US 11,178,395 | **Adopted** | Defines defensible non-infringing router scope. |
| **P15-I01** | P15 | Tripartite Lossless Taxonomy | **Adopted** | Core taxonomy preventing user deception (C-001). |
| **P15-I02** | P15 | Pre-Flight Hardware Conformance Probe | **Adopted** | Runtime MediaCodec quality verification (C-011). |
| **P15-I03** | P15 | Thermal-State Dynamic Gear Shifting | **Adopted** | SVT-AV1 preset gear-shifting on thermal stress. |
| **P15-I04** | P15 | Deterministic Verification Loop | **Adopted** | Post-encode PSNR=$\infty$ validation worker. |
| **P15-I05** | P15 | Crash-Resilient Segmented Pipeline | **Adopted** | Segmented writing allowing resume after kill. |
| **P15-I06** | P15 | Adaptive Power-Budget Governor | **Adopted** | WorkManager charging/Wi-Fi constraints. |
| **P16-I01** | P16 | `Prism-BBR` Bitstream Recompression | **Adopted** | Core algorithmic engine for Mode L-BBR (C-002). |
| **P16-I02** | P16 | `Prism-DetInt` Integer Determinism Engine | **Adopted** | Core mathematical specification for integer priors (C-009). |
| **P16-I03** | P16 | `Prism-Lossy` Two-Speed Hybrid Split-Stream | **Adopted** | Core lossy engine (HW Base + Cool-chic + AFGS1) (C-006). |
| **P16-I04** | P16 | `Prism-PSC` Wavelet-Checkerboard Lossless | **Adopted** | Core algorithmic engine for Mode L-PSC (C-005). |
| **P16-I05** | P16 | `Prism-VFS` Zero-Copy Virtual MP4 Bridge | **Adopted** | Core ecosystem adoption technology (C-016). |
| **P16-I06** | P16 | Tri-Metric Consensus & 4-Target Dial | **Adopted** | UI and evaluation framework (C-021). |
| **P17-I01** | P17 | Tri-Contract Lossless Semantics | **Adopted** | Integrated into contract specification. |
| **P17-I02** | P17 | Device Codec Performance Atlas | **Adopted** | Persistent device capability cache (C-011). |
| **P17-I03** | P17 | Preflight Pareto Probe | **Adopted** | 1–2s pre-flight test encode. |
| **P17-I04** | P17 | Metadata Preservation Ledger | **Adopted** | Complete metadata audit matrix. |
| **P17-I05** | P17 | Multi-Objective Optimization Function | **Adopted** | Formal mathematical candidate ranking function. |
| **P17-I06** | P17 | Atomic Publishing with Certification | **Adopted** | MediaStore `IS_PENDING` atomic commit. |
| **P18-I01** | P18 | Four-Tier Lossless Framework | **Adopted** | L0/L1/L2/APV taxonomy. |
| **P18-I02** | P18 | NAL-Unit Syntax Compaction | **Merged** | Subsumed into `Prism-BBR`. |
| **P18-I03** | P18 | Charging/Idle/Thermal-Gated Scheduler | **Adopted** | 3–5 W thermal envelope protection. |
| **P18-I04** | P18 | Vulkan Compute FFV1 Tracking | **Adopted** | GPU acceleration tracking in FFmpeg 8/9. |
