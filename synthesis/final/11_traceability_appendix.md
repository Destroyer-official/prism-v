# Section 11: Traceability Matrix, Rejected Ideas & Technical Glossary

---

## 11.1 Master Traceability Matrix (Decisions $\to$ Source Plans $\to$ Primary Evidence)

The table below maps every architectural decision in Prism-V back to its sponsoring input plans and primary literature citations:

| Decision Domain | Synthesis Choice in Prism-V | Source Plan Lineage | Primary Literature & Standards Citations |
|:---|:---|:---|:---|
| **Lossless Definition** | Tri-Contract Framework: Mode L-BBR (Syntax Transcode), Mode L-Remux (Stream Copy), Mode L-PSC (Wavelet-Checkerboard). | **[P05, P08, P11, P15, P16, P17, P18]** | Singh (arXiv:2606.23753v1, June 2026), Dropbox Lepton (2016), IETF RFC 9043 (FFV1). |
| **Bitstream Recompression** | Position-Indexed Contexts (`HuffmanPos`/`ECB`) + 1.1 KB `INT8` Micro-MLP + 8-way SIMD rANS with patch stream ($\Delta_{\text{patch}}$). | **[P05, P08, P16, P18]** | Singh (arXiv:2606.23753v1), Jarek Duda rANS (2013), RAS Accelerator (arXiv:2511.04684v1). |
| **Sensor Lossless Codec** | Reversible YCoCg-R + LeGall 5/3 lifting wavelet + 4-pass Checkerboard `INT8` ConvNet (0.34 GMAC/frame). | **[P07, P08, P16]** | Prunes NeuralLVC (ACM MM '26, arXiv:2604.03353v2) by 25,000x; JPEG 2000 5/3 wavelet. |
| **Lossy Core Paradigm** | Hardware-Anchored Hybrid Split-Stream: Fixed-Function `MediaCodec` Base + 600-MAC/pixel Cool-chic residual + AFGS1 grain. | **[P01, P04, P08, P10, P11, P16]** | Cool-chic 5.0 (arXiv:2605.02726v2), N-O Cool-chic (arXiv:2607.13723v1), AOMedia AFGS1 Spec (2024). |
| **Neural Determinism** | Class A/B split: Class A (entropy priors) run on CPU NEON/SME2 via bounded integer math ($K\le 512$, dyadic shift, 512B LUT). | **[P01, P04, P08, P09, P16]** | Ballé et al. (ICLR 2019), Arm KleidiAI SME2, InterDigital CompressAI (arXiv:2011.03029). |
| **Ecosystem Adoption** | `Prism-VFS`: `StorageManager.openProxyFileDescriptor` virtual MP4 bridge decompressing in RAM at >180 MB/s. | **[P16]** | Android `StorageManager` API, Android FUSE `MediaProvider` architecture. |
| **Container Architecture** | Backward-Compatible Dual-Track ISOBMFF (`.mp4`) with `prsm`/`prmr`/`afgs` boxes for lossy; `.pvloss` for lossless. | **[P01, P04, P08, P09, P11, P16, P17]** | ISO/IEC 14496-12 (ISOBMFF), IETF RFC 9559 (Matroska, Oct 2024). |
| **Systems Language** | Safe Rust (Edition 2024) for parsers/muxers/JNI + C++20 / Assembly (`KleidiAI`, NEON, SME2) for hot vector loops + Kotlin UI. | **[P03, P04, P16, P18]** | Android NDK r27+, LLVM 19/20 Clang, Android Memory Safety Standards. |
| **Thermal & Power Scheduling** | Two-speed lifecycle: live feed-forward (<3.5ms) at capture + `WorkManager` multi-GOP overfitting while charging, idle, and cool. | **[P01, P02, P04, P08, P11, P16, P18]** | Android ADPF `PerformanceHintManager`, Android `PowerManager` Thermal Status. |
| **Memory Architecture** | Bounded Tile Ring-Buffer (<64 MB peak RAM on low-end 4K) using $256\times 256$ luma tiles backed by `AHardwareBuffer`. | **[P05, P11, P12, P15, P16, P17]** | Android LMK thresholds, Android 15+ 16 KB memory page size requirements. |
| **Perceptual Metrics** | Tri-Metric Perceptual Consensus: VMAF-NEG v3.1.0 + ColorVideoVDP (CVVDP v1.2) + CAMBI banding index (<2.5). | **[P04, P05, P07, P08, P11, P14, P15, P16, P17]** | Netflix VMAF v3.1.0 NEG, Univ. of Cambridge CVVDP, AV2 CTC (arXiv:2605.15800v1). |
| **Gallery Storage Reclamation**| Content-Defined Chunk Deduplication using rolling Rabin fingerprinting, downcoding duplicate video chunks. | **[P11]** | Rabin Fingerprinting (1981), Content-Defined Chunking in Storage Systems. |
| **Legal & Royalty Shield** | Pass-through OS `MediaCodec` delegation + Zero-GPL clean-room binary (Apache/BSD) + defensive router boundaries. | **[P07, P12, P14, P15, P16, P17, P18]** | Distinguishes WhatsApp US 11,178,395 B1 and Microsoft US 2026/0254962 A1. |

---

## 11.2 Exhaustive Rejected Ideas Register

Every rejected concept across the 18 input plans is logged below with technical and economic rationale:

| Rejected Concept | Sponsoring Plans | Definitive Technical, Economic, or Legal Reason for Rejection |
|:---|:---|:---|
| **Re-encoding Compressed MP4s with Lossless Pixel Codecs (`FFV1`, `x264 --qp 0`)** | P01, P02, P03, P06 | **Information-Theoretic Impossibility:** Unrolling lossy H.264/HEVC into raw YUV pixels forces the lossless encoder to treat high-entropy quantization noise and DCT ringing as true signal, expanding file size by 400% to 900%. Replaced by `Mode L-BBR` (C-002). |
| **Pure End-to-End Neural Autoencoders (`DCVC-RT`, `DCVC-UF`) on Mobile Silicon** | P01, P06 | **Mobile Thermal & Energy Physics:** Consumes 2.5–4.8 J/frame (75–140 W equivalent at 30 fps), inducing severe thermal throttling within 45s. Floating-point non-associativity across mobile NPUs causes catastrophic rANS desynchronization. Replaced by `Prism-Lossy` (C-006). |
| **Software VVC (`VVenC`) & Software AV2 (`AVM`) Encoding on Android Handsets** | P03, P06 | **Computational Complexity:** Reference AVM encoding is 33x slower than AV1 libaom; VVenC takes >1.4 J/frame. Burns 15%–25% battery in minutes and throttles mobile CPUs to 53% clock speed. Commercial mobile silicon in 2027 lacks hardware VVC encoding blocks. |
| **Samsung APV (RFC 9924) Misrepresented as Bit-Exact Lossless** | P06 | **Factual Misrepresentation:** APV is an intra-frame mezzanine codec (200–800 Mbps) for video editing, comparable to Apple ProRes. It is visually lossless, NOT mathematically bit-exact. Relegated strictly to professional editing capture paths. |
| **Bundling GPL Codecs (`x264`, `x265`, `UtVideo`) in Commercial Mobile Binary** | P01, P02, P03, P04, P05, P08, P10 | **Copyright & Licensing Infection:** Bundling GPL-licensed binaries forces proprietary application code to be released under GPL or triggers expensive commercial license fees ($0.20–$1.20/unit). Replaced by Zero-GPL clean-room permissive stack (C-025). |
| **Proprietary Custom Container Formats (`.pvid`, `.aeth`)** | P03, P05 | **Ecosystem Isolation Trap:** Files stored in proprietary containers cannot be played, shared, or edited by WhatsApp, Instagram, TikTok, or ExoPlayer without installing custom software. Replaced by Dual-Track ISOBMFF (`.mp4`) + `.pvloss` with `Prism-VFS` (C-016, C-017). |
| **Destructive In-Place Original File Overwrite** | P01, P02, P03, P06, P10 | **Data Loss Liability:** Overwriting original media files in storage prior to independent output decode and visual verification risks permanent user data loss during crashes. Replaced by Verified Replacement 5-State Transaction Protocol (C-019). |
| **Unbounded Full 4K Frame Buffer Allocation in Heap** | P01, P04, P06, P08, P10 | **Android LMK Process Termination:** Buffering multiple 4K 10-bit frames in RAM consumes >400 MB, triggering immediate `SIGKILL` by Android Low Memory Killer on 4 GB budget phones. Replaced by Bounded Tile Ring-Buffer (<64 MB Peak RAM) (C-015). |
| **Scale-Sending Hyperlatent Parameter Transmission in Normative Decoder** | P09 (`MLVC-S`) | **Patent Infringement Risk:** Directly claimed by Microsoft Technology Licensing in Patent Application US 2026/0254962 A1 (published Aug 27, 2026). Replaced by Orange Labs' open-source Cool-chic continuous coordinate latents. |
| **Continuous Multi-Hour Background Foreground Services** | P01–P10 | **Android 15 Platform Violation:** Android 15+ strictly terminates `mediaProcessing` foreground services after 6 cumulative hours in a rolling 24-hour window. Replaced by atomic 60-frame GOP micro-tasks (C-030). |

---

## 11.3 Technical Glossary

- **ADPF (Android Dynamic Performance Framework):** Android platform APIs (`PerformanceHintManager`, `ThermalManager`) enabling applications to communicate workload hints and monitor thermal headroom to prevent thermal throttling.
- **AFGS1 (AOMedia Film Grain Synthesis 1):** A standalone specification finalized by AOMedia in January 2024 that models camera sensor noise as a 14-parameter autoregressive model (<200 bytes/frame), stripping noise before compression and re-synthesizing it deterministically at decode.
- **AMediaCodec:** The low-overhead, native C API exposed by Android NDK for interacting directly with the device's hardware video encoding and decoding ASICs.
- **BD-Rate (Bjøntegaard-Delta Rate):** The standard metric measuring the average percentage difference in bitrate between two rate-distortion curves at equal objective visual quality (PSNR, VMAF, or CVVDP). Negative values indicate bitrate savings.
- **CABAC (Context-Adaptive Binary Arithmetic Coding):** The normative entropy coding engine in H.264/AVC and H.265/HEVC, converting multi-valued syntax symbols into binary bins compressed by a 64-state table-driven binary arithmetic coder (`M-coder`).
- **CAMBI (Contrast Aware Multiscale Banding Index):** A perceptual banding visibility metric developed by Netflix that detects and penalizes false contouring and color steps in smooth gradients.
- **ColorVideoVDP (CVVDP):** A full-reference psychophysical visual difference predictor developed at the University of Cambridge modeling human spatial, temporal, and chromatic contrast sensitivity across SDR and HDR mobile displays.
- **Cool-chic:** An open-source family of low-complexity overfitted neural codecs created by Orange Labs, parameterizing video frames via compact multi-resolution 2D latent grids and a tiny synthesis MLP (500–3,000 MACs/pixel).
- **Dyadic Rescaling:** Fixed-point integer multiplication followed by a power-of-two right bit-shift ($\lfloor (x \cdot M) / 2^s \rfloor$), guaranteeing mathematically bit-identical results across all processor architectures.
- **ISOBMFF (ISO Base Media File Format):** The international standard container structure (ISO/IEC 14496-12) underlying `.mp4` files, organized into 4-byte tagged hierarchical boxes (`ftyp`, `moov`, `trak`, `mdat`).
- **KleidiAI:** Arm's optimized open-source micro-kernel library providing hand-tuned vector kernels for Armv8.2-A DotProd and Armv9.2-A Scalable Matrix Extension 2 (SME2).
- **MACs/pixel (Multiply-Accumulates per Decoded Pixel):** The primary metric of neural codec computational complexity; 1,000 MACs/pixel on a 1080p frame equals 2.07 GMACs per frame.
- **ProxyFileDescriptor:** An Android system API (`StorageManager.openProxyFileDescriptor`) enabling applications to intercept standard OS file read/seek calls and serve data generated on-the-fly in RAM.
- **rANS (Range Asymmetric Numeral Systems):** An entropy coding method invented by Jarek Duda combining the compression efficiency of arithmetic coding with the vectorizable throughput of table-driven coders.
- **SME2 (Scalable Matrix Extension 2):** An Armv9.2-A instruction set architecture extension providing hardware matrix outer-product accumulator tiles (`smopa`) accessible directly from user-space CPU threads.
- **VMAF-NEG:** Netflix's Video Multi-Method Assessment Fusion running under the No Enhancement Gain model, preventing artificial contrast and edge sharpening from hacking quality scores.
