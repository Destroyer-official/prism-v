# Next-Generation Android Video Compression

## Evidence-led implementation plan for 2027–2030

**Research snapshot:** 14 November 2026  
**Document type:** Architecture and implementation plan; no application source code and no APK  
**Intended audience:** Android media engineers, codec engineers, ML systems engineers, QA/performance engineers, product and legal teams  
**Evidence labels:** **[P-VERIFIED]** primary-source fact; **[S-VERIFIED]** secondary-source fact; **[AUTHOR ANALYSIS]** engineering conclusion derived from evidence; **[ESTIMATE]** planning estimate; **[SPECULATION]** forward-looking assumption requiring validation.  
**Maturity labels:** **Shipping**, **Standardized**, **Open-source implementation**, **Research code**, **Paper only**, **Theory / unverified**.

---

# 1. Executive summary

## 1.1 Recommendation

Build a **capability-probed, hardware-first Android transcoder with a separate archival-lossless pipeline**. Do not attempt to invent a new general-purpose video bitstream in the first product release.

The product should expose two top-level choices, while defining the meaning of *lossless* precisely:

1. **Lossless** — a selector for three distinct operations:
   - **Preserve original file/bitstream:** byte-copy the source file where possible. This is exact preservation, not compression.
   - **Lossless pixel-domain transcode:** decode the source, preserve its decoded sample values exactly, and re-encode them using a reversible codec. The initial archival target is FFV1 in Matroska, with all relevant stream and color metadata preserved or explicitly declared.
   - **Raw capture archival:** accept only an actually retained sensor/raw stream. This is not a feature that can reconstruct raw sensor values from an ordinary processed MP4.
2. **Lossy** — choose a quality, size, speed, or energy target. Use Android `MediaCodec` encoders that are actually available on the current handset and have passed runtime format checks and product validation. Use AVC as the broad compatibility fallback, HEVC as the initial high-efficiency hardware default, and AV1 only when a suitable encoder exists and passes capability/quality/power validation. Offer slower software encoding as an optional offline path, not as the default for every phone.

**Do not make a neural decoder mandatory.** A conventional standardized codec should carry the video. Machine learning may be evaluated for encoder-side analysis, optional preprocessing, or rate-control assistance, but a learned model must not be required to decode the file in v1. Quantization alone does not guarantee bit-identical inference across CPU, GPU, DSP, NPU, drivers, compiler versions, or vendor implementations.

**Do not choose VVC as the primary Android encoder.** ITU-T H.266 is standardized and current Android 17 AOSP documentation describes platform VVC decoding support, but that same documentation explicitly says AOSP supplies no VVC software decoder and no VVC encoding support; the device vendor must provide a Codec2 decoder. This is a decode-path statement, not a promise that a handset exposes a usable VVC encoder. [P-VERIFIED; Android AOSP page, accessed 14 Nov 2026]

## 1.2 Why this architecture is the strongest starting point

- Android's media stack is device-specific. The platform exposes codec enumeration, supported profiles, formats, ranges, and performance information. The app must inspect a concrete codec component rather than infer features from a chipset family or Android version. [P-VERIFIED]
- Media3 Transformer already provides a supported high-level transcoding/editing path built on `MediaCodec`; it is useful for a fast prototype and baseline, while the production engine can replace lower-level components where more control is needed. [P-VERIFIED]
- Hardware codec blocks are usually the best starting point for battery-conscious mobile encode/decode. Their actual efficiency and output quality still vary by phone, firmware, codec component, profile, rate-control mode, and thermal state. [AUTHOR ANALYSIS]
- FFV1 has a documented lossless intra-frame bitstream and Matroska mapping. It is a more defensible archival baseline than a proprietary learned-lossless scheme. [P-VERIFIED]
- Neural video coding has advanced substantially, including MobileNVC and 2025–2026 DCVC research, but published results are not a substitute for a repeatable Android handset benchmark under sustained thermal and power constraints. [P-VERIFIED for paper/repository existence; AUTHOR ANALYSIS for product readiness]

## 1.3 Expected outcomes and uncertainty

| Area | Expected v1 outcome | Confidence |
| --- | --- | --- |
| Original-file preservation | Exact byte copy when requested; zero decode/re-encode loss | High; simple file I/O and hashing |
| Lossless pixel transcode | Bit-identical decoded sample planes after decode of the output, for explicitly supported pixel formats; FFV1/Matroska | High for correctness if tested; compression ratio content-dependent and not promised |
| Lossy transcode | Hardware-first AVC/HEVC; AV1 only when proven available and efficient; software fallback where feasible | Medium; handset fragmentation is substantial |
| Exact target size | Approximation by default; more accurate target using analysis / iterative encode where time and encoder support permit | Medium |
| Neural codec | Research-only until Android performance, energy, memory, deterministic decoding and compatibility are independently demonstrated | High confidence in this conservative gating decision |
| 2027–2030 SoC support | Increasing accelerator capability is plausible, but no family-wide codec guarantee is made | Low-to-medium; future device configurations are unknown |

No universal compression ratio is promised. A codec's bitrate saving depends on the source, profile, resolution, frame rate, color format, quality metric, encoder version, preset, reference implementation and hardware. FFV1 can compress natural or computer-generated content well but cannot guarantee a 2:1 or 4:1 ratio for every input; already compressed, noisy, grainy or high-entropy frames may compress poorly.

## 1.4 Top decisions

| Decision | Selection | Rejected alternative | Evidence that would change the decision |
| --- | --- | --- | --- |
| Product strategy | Standard codecs and runtime capability discovery | New proprietary codec as v1 | A new codec proves a large reproducible gain, low decode cost, deterministic portability and manageable patent position |
| Lossless baseline | FFV1 v3 + Matroska | ML-based lossless format as initial format | A reversible alternative is smaller on a broad corpus, faster on low-end ARM, and has a stable documented decoder and favorable legal review |
| Lossy baseline | AVC fallback; HEVC default candidate; AV1 optional | Universal AV1/VVC assumptions | Device lab results show another codec offers better equal-quality energy/file-size on the supported fleet |
| Android pipeline | Kotlin orchestration + NDK/C++ media engine + `MediaCodec` | Pure Java frame-copy pipeline | Profiling shows a simpler Media3 path meets every quality, memory and metadata requirement |
| ML | Encoder-side experiments only; no mandatory learned decoder | NPU-specific normative decoder | Integer-only inference is specified end-to-end and bit-exact across all supported implementations, not merely one reference implementation |
| Container | MP4 for mainstream lossy; Matroska for FFV1 archival | Custom container | A specific missing requirement cannot be represented safely by MP4/Matroska and a public specification is justified |

## 1.5 First 30 days

**Days 1–5 — scope and instrumentation:** write the lossless contract; create device capability dumper; build source-file inventory and checksum tools; define sample corpus and metadata checklist.

**Days 6–10 — reference paths:** implement no-codec byte-copy/remux tests; build Media3 Transformer baseline; create `MediaCodec` enumeration and short encode/decode smoke tests for AVC/HEVC/AV1; build FFV1/Matroska host baseline using FFmpeg.

**Days 11–20 — minimal proof:** run pixel-exact FFV1 round trips, hardware AVC/HEVC/AV1 experiments where exposed, measure wall-clock time, frame drops, peak RSS, thermal status and energy. Compare equal-quality rate-distortion curves, not raw bitrate alone.

**Days 21–30 — architecture gate:** choose first supported device tiers, establish codec fallback policy, decide whether target-size iterative encoding is affordable, and publish the first reproducible benchmark report. Do not start a neural codec port before this gate.

**Day-30 go/no-go:** proceed to product implementation only if (a) lossless sample-plane exactness passes on every declared pixel format; (b) at least one viable lossy encoder exists on each target device tier; (c) metadata loss is either prevented or clearly surfaced; and (d) no severe thermal or memory failure occurs in the defined 10-minute sustained test. These are proposed program gates, not published industry thresholds.

---

# 2. Research findings: codec and platform landscape

## 2.1 What “lossless” means in this product

The word must be tied to a specific object and comparison boundary.

### Contract A — original-file / elementary-bitstream preservation

- **File preservation:** output bytes equal input bytes. Verify SHA-256 equality. The correct operation is copy, not decode and encode.
- **Elementary-stream preservation:** a remux may change container bytes while copying compressed video access units. This is *not* whole-file byte equality. It must be documented as “video bitstream copied; container rewritten.”
- Audio, subtitles, chapters, metadata, attachments, edit lists, timestamps, rotation, HDR metadata and stream ordering must be audited separately. A remux cannot be called whole-file lossless if one of these is dropped or rewritten.

### Contract B — lossless pixel-domain transcode

The input is decoded to a declared sequence of digital sample planes. The encoder must produce a bitstream whose decoder reconstructs those sample values exactly. The comparison key is not the source MP4's original compressed bytes; it is the decoded sample arrays after a controlled, documented input decode.

The contract must declare:

- width, height, frame order, frame rate/time base and presentation timestamps;
- pixel format, chroma sampling (4:2:0, 4:2:2, 4:4:4), bit depth, plane layout and alpha handling;
- range (limited/video or full), matrix coefficients, primaries and transfer function;
- HDR10 static metadata, HDR10+ dynamic metadata, Dolby Vision configuration/metadata where present, and whether dynamic metadata can be retained without alteration;
- rotation/display matrix, sample aspect ratio, clean aperture and crop;
- audio bitstream handling and delay, subtitles, chapters and attachments;
- whether decoder-side dithering, chroma resampling, tone mapping or color conversion is disabled.

A pixel-domain lossless encode after a lossy source decode cannot recover information already discarded by the original source codec. It preserves the decoded representation, not the pre-encode camera scene.

### Contract C — raw sensor/camera archival

This means retaining raw sensor samples or a documented camera-raw stream before image signal processing (ISP) transforms such as demosaic, denoise, white balance, sharpening, color correction and tone mapping. An ordinary phone video file is generally already a processed output. It is invalid to label an MP4-to-FFV1 conversion as raw-sensor preservation.

**v1 scope:** A and B. C is an explicitly separate future acquisition feature, limited to devices and camera pipelines that expose a genuine raw-video stream with documented sample semantics. It is not a general gallery-file operation.

## 2.2 Codec taxonomy and maturity

“Standardized” means a published normative or standards-track specification exists. “Shipping” means commercial devices, services or applications expose the technology; this does not imply universal Android support. “Open-source implementation” describes code availability and does not imply patent clearance.

| Codec / family | Maturity | Core idea | Android reality and product position | Cost / caveat |
| --- | --- | --- | --- | --- |
| AVC / H.264 | **Standardized; Shipping; Open-source implementations** | Block prediction, motion compensation, transform/quantization and entropy coding; mature hybrid codec | Broadest compatibility; treat as universal fallback and baseline. Probe exact profile/level and encoder. | Lower compression efficiency than newer codecs at comparable settings in many tests; output quality strongly depends on vendor encoder and settings. Patent/royalty review required for commercial product. |
| HEVC / H.265 | **Standardized; Shipping; Open-source implementations** | Larger coding tree units, improved prediction, transforms and in-loop filters | Common hardware decode/encode path on many mid/high devices, but not guaranteed by SoC name alone. Initial high-efficiency hardware default candidate. | Vendor encoder quality and supported profiles vary. Patent pools and separate legal obligations. |
| VP9 | **Standardized in WebM/related specifications; Shipping; Open-source** | Block-based hybrid coding with flexible partitions and transforms | Playback support is common in Android ecosystems; encode support is less consistent. Keep mainly for input compatibility, not first-choice output. | Profile/bit-depth support varies; software encoders may be costly. |
| AV1 | **AOM specification; Shipping; Open-source implementations** | Modern block-based codec with richer partitioning, prediction, transforms, filters and entropy tools | Attractive decode and compression candidate; Android CDD defines requirements conditional on a device supporting AV1. Do not assume every phone can hardware-decode or encode every AV1 profile. Runtime-probe. | Hardware encoder availability is narrower and more fragmented than codec marketing can imply. Software high-quality encode may be too slow/hot for low-end phones. AOMedia patent license exists but does not eliminate every third-party patent issue. |
| VVC / H.266 | **Standardized; early/limited Shipping; Open-source implementations** | More flexible partitioning and prediction, transforms, filters and coding tools than HEVC | Android 17 AOSP adds VVC media type/parser/Codec2 integration and MP4 track support, but AOSP has no VVC software decoder and no VVC encoding. Vendor must supply decoder. Optional future input/playback path only after device validation. | Complexity, sparse fleet exposure and patent-pool costs make it unsuitable as v1 default. No assumption of VVC encoder support. |
| EVC / MPEG-5 Part 1 | **Standardized; Open-source implementations; limited Shipping evidence** | Baseline profile intended to provide a simpler licensing position; Main profile adds tools for higher compression | Research and compatibility candidate; no assumption of Android-native hardware encode/decode. Prototype only if the product needs it and licensing is cleared. | “Baseline royalty-free” language must not be treated as proof that every implementation or profile is patent-free. Verify actual patent declarations, implementations and contractual terms. |
| LCEVC / MPEG-5 Part 2 | **Standardized; commercial implementations; open-source/reference material** | Adds a low-complexity enhancement layer to a base codec; enhancement is intended to improve quality or reduce bitrate for a given system design | Not a native universal Android codec. Would require integrated base codec + enhancement decoder and a defined packaging path. Research only. | Claimed gains are configuration-specific. Always name base codec, encoder, decoder, content, metric and bitrate ladder. Patent and licensing review is essential. |
| FFV1 | **RFC-documented; Open-source implementation; Shipping in archival workflows** | Intra-frame reversible prediction and entropy coding; no inter-frame motion prediction | Excellent first lossless pixel-domain archival baseline using software encode/decode and Matroska. Not assumed to be exposed through Android `MediaCodec`. | Compression ratio is content-dependent. Software CPU and storage bandwidth costs may be meaningful on phones. RFC 9043 is informational, not an Internet Standards Track specification. |
| JPEG XS | **Standardized; professional shipping** | Low-complexity, low-latency, visually-lossless or mathematically lossless image/video coding, optimized for parallel hardware | Relevant to professional camera, transport and sensor pipelines; not a practical general Android gallery transcode default without device/SDK support. | High implementation ecosystem specialization; visually lossless modes must not be confused with mathematically lossless modes. |
| JPEG 2000 / Motion JPEG 2000 | **Standardized; open/commercial implementations; specialist shipping** | Wavelet image coding; supports both lossless and lossy image coding | Useful for frame-based scientific/archival workflows, not general Android distribution. | Large decode/encode complexity and limited device-native ecosystem. |
| ProRes | **Commercial/proprietary ecosystem; Shipping** | Intra-frame production codec family | Preserve as input if possible; do not assume native Android encoder/decoder. | Apple specification, licensing and implementation conditions need separate review. Not a v1 target codec. |
| DNxHR | **Commercial/proprietary ecosystem; Shipping** | Intra-frame production codec family | Preserve/remux when the existing stack can do so; otherwise document limitations. | Avid ecosystem and licensing; not a general consumer-phone target. |
| Neural video codecs (NVC) | **Research code and papers; selected prototypes** | Learned motion/context representations, latent quantization, entropy models and learned synthesis | Strong research direction, but not a drop-in Android codec. Keep outside the interoperable v1 bitstream. | Model weights, entropy coding, compute, memory, power, reproducibility, deterministic decoding and format standardization are unresolved product risks. |

### Primary standard and implementation evidence

- ITU-T H.264 V16 was approved June 2026; ITU-T H.265 V11 was approved January 2026. [P-VERIFIED]
- ITU-T H.266 V4 was approved January 2026; the ITU summary describes UHD, HDR/wide-color and immersive use cases. [P-VERIFIED]
- ISO/IEC catalogue lists EVC as ISO/IEC 23094-1:2020, LCEVC as ISO/IEC 23094-2:2021, and conformance/reference-software parts for both. [P-VERIFIED]
- FFV1 RFC 9043 describes a lossless intra-frame format, its pixel formats, versioning, Matroska `V_FFV1` mapping and frame-fixity mechanisms. The RFC explicitly says it is Informational, not Standards Track. [P-VERIFIED]
- JPEG XS describes very-low-latency, low-complexity encoding and lossless image coding options; the JPEG committee describes typical visually-lossless ratios up to 10:1 in selected professional contexts. This is not a universal promise for arbitrary mobile video. [P-VERIFIED]

## 2.3 Android media platform: what the app can and cannot infer

### `MediaCodec` is the runtime authority

The app must enumerate codecs using `MediaCodecList`, inspect each component's supported MIME types, encoder/decoder direction, profile/level list, `CodecCapabilities`, `VideoCapabilities`, color formats, bitrate ranges, frame-rate ranges, performance points and encoder capabilities. It should prefer a concrete component chosen by format, rather than assuming that `createEncoderByType()` yields the best encoder for the desired profile.

Android 10 / API 29 added `MediaCodecInfo` methods including `isSoftwareOnly()`, `isHardwareAccelerated()`, `isVendor()` and aliases. The Android documentation warns that `isHardwareAccelerated()` is manufacturer-provided metadata and cannot itself be tested for correctness. Therefore, the app must run a real short encode/decode smoke test and benchmark the actual output path. [P-VERIFIED]

**Probe matrix per device:**

1. OS build, API level, manufacturer/model, SoC identifier where available, RAM class, storage type and thermal status.
2. For each desired MIME type: encoder/decoder existence, component name, alias/canonical name, vendor/software/hardware flags, profile/level, color formats, size/rate coverage, performance points, bitrate modes and quality/complexity ranges.
3. Configure and run a short encode with actual input dimensions, frame rate, bit depth, chroma and color metadata.
4. Parse the output bitstream and container, decode it on the same phone and on at least one independent reference decoder.
5. Measure sustained throughput, dropped frames, CPU time, peak memory, thermal status and energy.
6. Cache the result by device build fingerprint + component name + codec configuration; invalidate after OS/firmware changes.

Do not hardcode “Snapdragon 8 series supports AV1 encode” or “all Tensor devices support VVC.” A chip vendor's listed media block and an Android handset's exposed `MediaCodec` component are separate facts. A phone OEM can disable or omit a vendor component, use a different SoC configuration, constrain encoder profiles, or ship different firmware by region.

### Android 17 and VVC

The Android Open Source Project's VVC support page states that Android 17 introduces a VVC MIME type, `MediaCodec`/Codec2 integration, VVC tracks in MP4 parsing/extraction, 4:2:0 profiles with HDR10/HDR10+ metadata and up to 10-bit content. It also states:

- AOSP does not provide a VVC software decoder.
- There is no VVC encoding support.
- A SoC vendor must provide a Codec2 VVC decoder and expose it through vendor `media_codecs.xml`.

Therefore, VVC support in AOSP is **framework plumbing and vendor opt-in**, not universal VVC decoding and not VVC encoding. The app must discover `video/vvc` and test the vendor component on the particular device. [P-VERIFIED; Android AOSP VVC documentation, accessed 14 Nov 2026]

### AV1 platform requirements

Android 16 CDD states conditional requirements for devices that implement AV1. It requires Main Profile support including 8-bit and 10-bit content, plus performance reporting and HDR-related behavior. This is not the same as requiring every Android 16 device to implement AV1. A separate Android 17 preview supplemental CDD table publishes AV1 encoder requirements for selected media-performance-class values at 480p/30fps/1Mbps. That narrow requirement must not be extrapolated into 1080p/4K AV1 encoding across the Android market. [P-VERIFIED; Android CDD and preview, accessed 14 Nov 2026]

### `Codec2`, NDK and Media3

- Use `MediaCodec` for mainstream encode/decode and the NDK media APIs where native processing, C/C++ integration or data ownership requires it. `Codec2` is the lower-level Android media component framework used by platform/vendor codec implementations; ordinary apps should normally use the public media APIs, not bind directly to private vendor internals.
- Use Media3 Transformer as a benchmark and possibly as the first implementation of conventional transforms. It uses `MediaCodec` for hardware decode/encode and OpenGL for graphical changes; it supports Android API 23+. Replace parts only after the benchmark demonstrates a concrete limitation.
- Use `MediaExtractor`/`MediaMuxer` for supported workflows. Validate each codec/container pair and metadata path, because API surface availability is not a guarantee that every vendor codec, track feature or metadata field can be muxed correctly.

### Buffer transport and GPU

`AHardwareBuffer` can be shared between hardware components and imported into EGL/OpenGL/Vulkan paths; Android describes cross-process `AHardwareBuffer` transfer as zero-copy. Its support is nevertheless constrained by the buffer usage flags, format, gralloc implementation, protected-content rules, fences and producer/consumer compatibility. Use it when end-to-end tests confirm the path; do not describe the whole pipeline as zero-copy merely because one buffer is shared.

Recommended policy:

- **Hardware codec surface path first:** use surface input to hardware encoder where compatible; avoid CPU readback.
- **CPU path second:** use CPU pixel planes for FFV1 and software fallbacks, with explicit stride/plane handling.
- **Vulkan/OpenGL compute only for measured transforms:** color conversion, scaling, rotation or optional pre-processing may use GPU if it avoids readback and beats the CPU. A GPU stage can become slower if it forces format conversion, synchronization or memory copies.
- **NPU only after a targeted experiment:** no generic NPU workload is included in the critical encode/decode loop for v1.

### Neural APIs: NNAPI is deprecated

Google's NNAPI migration guide states that NNAPI was deprecated in Android 15. AOSP says the NNAPI HAL interface continues to be supported, but app developers should use newer ML deployment routes. The current Android developer guidance recommends LiteRT for custom on-device ML; GPU acceleration is available, and NPU acceleration is available in standalone LiteRT, with Google Play services NPU support described as forthcoming in the page accessed on 14 Nov 2026. [P-VERIFIED]

Do not couple the core video codec to NNAPI. If optional ML is added, wrap it behind a runtime abstraction and test CPU/GPU/NPU backend separately. Never assume one model will be mapped to the same accelerator on all vendors.

## 2.4 SoC evidence: examples, not generalizations

Vendor specifications are useful for constructing candidate test devices, but the actual handset must still pass the Android runtime probe.

| Vendor/device-class example found in official vendor documentation | Vendor-published media claims | What may be concluded | What may not be concluded |
| --- | --- | --- | --- |
| MediaTek Dimensity 9500 | 8K60 10-bit decode for HEVC/AVC/VP9/AV1; 8K30 10-bit encode for HEVC/AVC; 8K60 8-bit HEVC encode | This specific published platform claims those media capabilities. | It does not prove the phone OEM exposes every mode/profile through Android `MediaCodec`, nor that AV1 encoding is present. |
| MediaTek Dimensity 9400 | 8K60 10-bit decode for HEVC/AVC/VP9/AV1; 8K30 10-bit encode for HEVC/AVC; 8K60 8-bit HEVC encode | Similar published decode breadth and HEVC/AVC encode breadth on that chip. | Not evidence of an AV1 encoder or universal handset performance. |
| MediaTek Dimensity 8400 | Vendor page lists H.264/HEVC encode, 4K60 encoding, and H.264/HEVC/VP9/AV1 playback. | Shows that an AV1 decode capability can coexist with AVC/HEVC encoders on a particular chip. | No AV1 encode claim. |
| MediaTek Dimensity 7300 | Vendor page lists H.264/HEVC encoding and H.264/HEVC/VP9 playback. | A candidate midrange device where AVC/HEVC may be primary. | No AV1 or VVC support claim. |
| Google Pixel 10 Pro | Google product specs list HEVC, AVC, AV1 and VP9 as video formats. | Useful candidate for codec/container smoke tests; product page also describes camera capture modes. | The generic format list does not specify MediaCodec encoder/decoder profile, hardware/software status or sustained transcode performance. |
| Qualcomm Snapdragon 8 Gen 3 | Qualcomm page and product brief identify the platform, but the public page reviewed here did not establish the full codec profile/encode matrix in a machine-readable manner. | Candidate flagship test hardware. | No unverified AV1/VVC hardware claim should be made on this evidence alone. |
| Samsung Exynos family | Public information varies by product and device; this review did not verify an authoritative complete matrix for the selected current models. | Include representative retail devices in the lab after inspecting their actual codec components. | Do not infer codecs or NPU access from Exynos family names. |

**Required device lab:**

- Low-end: 4 GB RAM class, entry CPU/GPU, limited storage bandwidth, AVC hardware path; at least two independently sourced devices.
- Lower-midrange: 6–8 GB RAM, AVC/HEVC hardware encode/decode; two vendors.
- Upper-midrange: 8–12 GB RAM, HEVC plus any verified AV1 decoding; two SoC vendors.
- Flagship: 12–16 GB RAM class, modern CPU/GPU/NPU and tested AVC/HEVC/AV1 paths; at least Qualcomm and MediaTek, plus a Google Tensor handset where available.
- Android versions: API 29 baseline where product requirements permit, plus currently supported Android versions and Android 17 preview/final device(s) for VVC integration experiments.
- At least one device with no Google Play services if the app claims offline/alternative distribution support.

The precise retail models are a procurement decision after the device capability probe; a chipset matrix alone is not a sufficient qualification list.

---

# 3. Neural codecs and design choices

## 3.1 Research results and what they actually prove

### MobileNVC (WACV 2024)

The MobileNVC paper reports real-time 1080p YUV420 neural decoding on a mobile device. The abstract describes a pipeline splitting neural-network inference to a neural signal processor, entropy coding to the mobile GPU, and motion warping to a hardware warping core; it reports up to 48% BD-rate savings relative to its selected prior on-device neural codec and a 10× reduction in receiver-side MAC count. It was published in WACV 2024. [P-VERIFIED for paper and abstract]

**Interpretation:** this is a strong proof that a carefully co-designed neural decoder can run on a specific mobile accelerator configuration. It is not proof that it runs on generic Android phones, nor that the same model is available on Qualcomm, MediaTek, Exynos and Tensor with equivalent numeric results, power, memory and driver behavior. The 48% result is a comparison to the paper's stated baseline, not a claim of 48% bitrate savings over every modern hardware HEVC/AV1 implementation.

### DCVC family (2021–2026)

Microsoft's public DCVC repository lists successive research implementations including DCVC, DCVC-TCM, DCVC-HEM, DCVC-DC, DCVC-FM, DCVC-RT and DCVC-UF. The 2025 CVPR DCVC-RT paper reports real-time 1080p average encoding/decoding rates on its reported evaluation setup and bitrate reduction against VVC/VTM. The official repository's later DCVC-UF entry describes chunk-based coding, 64 QP levels and unified YUV/RGB support; its repository instructions require modern PyTorch, CUDA and custom C++/CUDA extensions for the listed implementation. [P-VERIFIED for publication and repo contents]

**Interpretation:** this is strong research progress, but CUDA-based research code and datacenter GPU results do not directly imply a deployable Android arm64 encoder/decoder. A mobile port would need entropy coding, tensor kernels, motion/warping, quantization, memory reuse and model execution reworked for Android accelerators. The actual receiver-side energy and sustained thermal behavior are the product-critical unknowns.

### ELF-VC and earlier learned codecs

ELF-VC (ICCV 2021) provides flexible-rate learned video coding and reports results on UVG and MCL-JCV, including GPU timings on an NVIDIA Titan V. That is useful historical evidence for rate-control and model-efficiency ideas, not mobile performance evidence. Later work has progressed beyond this particular GPU baseline.

### VVC neural-network post-filter syntax

The January 2026 H.266 V4 summary references two neural-network post-filter SEI messages and new SEI material, including AI-related metadata. This shows that standardized video ecosystems are beginning to accommodate neural-network-related signaling. It does **not** establish a universal neural decoder or mean that a mobile device implements these tools. [P-VERIFIED]

## 3.2 ML deployment decision

**v1: no mandatory ML codec.** Keep bitstream decoding conventional and deterministic through standardized codec implementations.

**Research track A — content-aware encoder adviser:**

- Input: low-resolution, sampled frames and source metadata.
- Output: scene class (animation, screen/text, natural, grain/noise, sports/high motion), motion/complexity estimates, and suggested bitrate/quality preset.
- Model target: start with a compact model in the approximate 1–5 million parameter range. This is a proposed experiment envelope, not a literature-proven optimum.
- Deployment: LiteRT CPU/GPU baseline; NPU backend only through a tested supported vendor/runtime path. Avoid NNAPI because it is deprecated from Android 15.
- Decoder consequence: none. The encoded file remains an ordinary AVC/HEVC/AV1 stream.
- Falsification: reject if quality-at-size is not measurably better than a simple content/complexity heuristic, or if inference overhead adds more than 2% of total encode energy/time in the target workflow. The 2% is a proposed gate.

**Research track B — optional prefilter:**

- Evaluate denoise/deband only as an explicit user-selectable lossy transform. Never silently apply to “preserve quality” output.
- Keep original decoded frames as the reference; account for prefilter-induced detail loss separately from codec loss.
- Do not use generative frame synthesis, generative upscaling, or content hallucination in a normal compression preset.
- Measure temporal stability; a per-frame denoiser that flickers is unacceptable even when single-frame VMAF improves.

**Research track C — learned end-to-end codec:**

- Keep in a research branch until mobile decoder energy, RAM, latency, file compatibility and deterministic output pass all gates.
- Begin with an encoder-only trial using a neural rate/quality estimator or motion-model analysis. A full NVC has a new bitstream and decoder model lifecycle; do not merge it with the interoperable product path prematurely.

## 3.3 Deterministic decoder rules

A bitstream decoder must produce the same normative output samples regardless of supported device. “INT8 quantized” does not establish this. Differences can arise from integer overflow rules, rounding, saturation, accumulator width, operation ordering, fused operators, memory layout, compiler transformations, backend kernel selection and driver behavior.

If a learned decoder is ever considered:

1. Define every operation, tensor layout, scale, zero point, rounding rule, saturation rule and accumulator width in a normative specification.
2. Use exact integer operators only; prohibit unspecified floating-point execution in normative reconstruction.
3. Fix entropy decoder behavior, CDF precision, symbol order, padding and end-of-stream handling.
4. Produce reference vectors and cryptographic hashes for decoded frames.
5. Run the same vectors on every CPU/GPU/NPU backend and compare decoded sample buffers bit-for-bit.
6. Verify across compiler versions and runtime/library updates.
7. If a backend differs, force a conforming software decoder or disable that backend; never quietly accept a different decoded result.

**Decision:** no learned decoder in v1. This provides cross-device compatibility and avoids dependence on proprietary NPU kernels.

## 3.4 Invented/combined ideas — hypotheses, not claims of novelty

| Proposed idea | Status | Expected gain hypothesis | Costs/risks | Test that proves or kills it | Prior-art position |
| --- | --- | --- | --- | --- | --- |
| Codec Capability Fingerprint (CCF): a cached runtime fingerprint of codec component, profiles, pixel formats, performance points, color handling and measured smoke tests | Engineering combination, not claimed novel | Fewer failed jobs and more accurate path selection | Cache invalidation, OEM firmware variation, probe time | Same handset/build produces repeatable capability report; firmware update changes cache key; predicted success agrees with actual jobs | Uses documented Android capability APIs; no patent novelty assertion made |
| Content-Conditioned Encode Planner (CCEP): short source scan, motion/noise/screen-content classifier and quality/size preset planner | Research hypothesis | Better quality/size selection and fewer wasteful trial encodes | Inference cost, misclassification, training data bias | Compare against fixed heuristic at equal output size and quality over content-stratified corpus | Content-aware encoding and learned rate control are established research themes; not claimed as new |
| Bounded-memory tile/stripe processing for archival FFV1 and software paths | Engineering design | Avoid full-frame or full-video buffers; lower peak RSS | Codec slice constraints, stride and plane handling | Peak RSS remains below budget on 4K/10-bit test; exact round-trip maintained | Tiling/slicing and FFV1 slices are established; no novelty claim |
| Energy-Aware Job Governor (EAJG): adjust codec/preset/resolution/queue depth based on thermal status, battery and measured energy | Engineering combination | Avoid severe throttling and battery spikes | Thermal API differences; performance variability | 20-minute sustained test has fewer severe-throttle minutes at similar quality/job completion | Adaptive mobile workload scheduling is established; no novelty claim |
| Verification-first archive commit: stream hash + decoded-plane fixity + metadata manifest, then atomic output publication | Engineering combination | Detect corruption and make lossless claims auditable | Additional reads and CPU; storage space for temp output | Inject crash and data corruption; no partial output is presented as complete | Fixity/CRC/checksum practice is established; no novelty claim |

**Patent warning:** this is an engineering prior-art scan, not a freedom-to-operate opinion. Search relevant standards essential patent declarations, published patents and pool terms before any novel bitstream or encoder tool is commercialized. A new codec's most important legal risk is not solved by writing original code.

---

# 4. Full implementation plan

## 4.1 User-facing product modes and control model

### Lossless screen

First ask the app to identify the source and report what is possible:

- “Preserve original file (exact copy)” — no codec conversion; output file hash must equal source.
- “Preserve compressed video stream (remux)” — video access units copied, container may change; audio/subtitles/metadata preservation status shown explicitly.
- “Lossless pixel transcode (FFV1)” — decode, losslessly encode pixels, archive in Matroska; decoded planes verified exactly.
- “Unsupported source feature” — stop or offer exact-copy preservation; do not silently drop Dolby Vision, alpha, 4:2:2/4:4:4, 12-bit samples or side data.

### Lossy screen

User chooses one primary objective:

| User target | Internal strategy | UI promise |
| --- | --- | --- |
| Quality | Constant-quality / CQ / quality parameter where truly supported; otherwise map a quality target to calibrated bitrate and test encodes | Target quality band, not a universal exact visual score |
| File size | Compute target video bitrate from size, duration, audio and mux overhead; use multi-pass/iterative encode where supported | Estimated size range; exact size only after a verified iterative pass |
| Speed | Hardware encoder, lower complexity, short GOP and fewer analysis steps | Fastest validated path on this device |
| Battery | Hardware codec, low CPU processing, no neural scan, conservative frame queue and thermal governor | Lowest measured energy path on this device, not “zero drain” |

The app should also expose advanced controls: output codec, resolution, frame rate policy, HDR policy, audio handling, bitrate cap, maximum output size, keyframe interval and preserve-metadata option. Keep defaults safe; do not expose unsupported rate-control modes as if universally effective.

### Target-size calculation

For target file size `S` bytes and duration `T` seconds:

`R_total = 8 × S / T` bits/s

`R_video = R_total × (1 − h) − R_audio − R_other`

where `h` is an estimated container/overhead fraction, and `R_other` includes subtitle, data and metadata tracks if they materially contribute. `h` is calibrated from measured output for the chosen container and codec; do not use a fixed universal overhead value.

The initial rate estimate is only a starting point. The app must account for encoder buffering, bitrate variability, B-frame structures, variable frame rate, audio packets and container overhead. A constant-quality hardware encoder may not honor exact target size. An exact-size workflow can require a first analysis pass or trial encode followed by a corrected encode; this costs time and energy and must be described to the user.

## 4.2 Lossless implementation details

### Recommended baseline

- Input parse/demux: FFmpeg libraries or Android platform extractor where metadata fidelity and input support are adequate.
- Decode: source's platform hardware decoder where it returns the needed pixel format without hidden transforms; otherwise a trusted software decoder for strict sample control.
- Transform: identity by default. Scaling, denoise, tone mapping, frame interpolation, chroma conversion and bit-depth reduction are forbidden in lossless pixel mode.
- Encode: FFV1 version 3, preserving the declared source sample format where supported; evaluate range coder and slice settings.
- Container: Matroska (`.mkv`), using the documented `V_FFV1` mapping and configuration record.
- Verify: decode output with independent FFV1 decoder, compare sample-plane hashes per frame, verify frame count/PTS and sidecar/container metadata manifest.

FFV1 is intra-frame; no temporal prediction means each frame is independently decodable. Its RFC documents slice structure and CRC support for fixity. This is attractive for archival recovery and frame-level verification.

### Compression ratio policy

Do not promise 2:1–4:1. The ratio is:

`compression_ratio = uncompressed_payload_bytes / compressed_payload_bytes`

It depends on bit depth, chroma, noise, grain, motion-independent spatial texture, overlays, screen content and input processing. Test each class separately. For already compressed video, the decoded samples may still contain complex quantization noise that limits lossless compression. Some sources may produce a ratio near 1:1 or even grow after muxing and metadata overhead.

### Optional lossless selector research

Compare FFV1, other mature reversible intra-frame codecs and lossless image-sequence approaches on exactly the same decoded frames and format. Optimize for all of:

- minimum output size;
- encode time and decode time;
- peak RSS;
- energy per megapixel-frame;
- error detection / frame fixity;
- ecosystem and long-term recoverability.

Choose a format by source-content class only if the metadata manifest makes the selection explicit and all outputs remain independently decodable. Do not create a proprietary archival format just to shave a few percent from one small benchmark.

## 4.3 Lossy implementation details

### Codec selection policy

1. Enumerate all encoder components and check requested size/rate/profile/color configuration.
2. Try a short bounded smoke encode and decode.
3. Score candidates for the user's chosen objective using a per-device calibration table.
4. Default to validated HEVC hardware encoding where available and compatible with the user's playback requirement; otherwise AVC.
5. Use AV1 only if a real AV1 encoder exists, the desired profile is supported, the output passes parse/decode tests, and sustained quality/energy tests justify it. A device that only decodes AV1 must not be shown an AV1 output choice.
6. Add software AV1 (e.g. rav1e/libaom-class implementations) as a high-compression optional route only after ARM64 performance, ABI, license and energy tests. Do not present a desktop preset score as a handset result.
7. Do not offer VVC encoding in v1. VVC decoding is a separately discovered optional Android 17+ input path only when the vendor component is present and proven.

### Quality and rate control

Use capability discovery to determine if the encoder supports constant-quality, variable bitrate (VBR), constant bitrate (CBR), quality range or complexity range. These settings are implementation-specific; their existence does not imply identical behavior between vendors. Calibrate presets independently per codec component.

For “quality” mode, prefer a CQ/quality mode if present and stable. If not, run a short analysis to estimate bitrate and choose a calibrated VBR target. For “speed,” lower the encoder's complexity and avoid software analysis. For “battery,” favor validated hardware, avoid GPU/NPU prefilters, limit CPU copies and stop/reduce work when thermal headroom becomes poor. For “size,” choose the best validated compression path with sufficient encode-time budget.

### Frame rate, GOP and audio

- Preserve source timestamps and variable frame rate unless the user explicitly requests constant frame rate. Never silently duplicate/drop frames to satisfy an assumed frame rate.
- Set GOP/keyframe interval based on seekability and bitrate needs; test short, medium and long GOPs per codec.
- Preserve audio bitstream when compatible; otherwise transcode separately and report it. Keep A/V sync under test after remuxing, frame-rate conversion or segmented outputs.
- Preserve subtitles and timed metadata when container/codec supports them; if not, offer a clear incompatibility warning or sidecar export.

## 4.4 HDR, color and pixel-format handling

HDR and color correctness must be treated as first-class, not a late polish item.

- Read source color standard, transfer, range, primaries and matrix coefficient metadata.
- Preserve HDR10 static mastering display and content-light metadata where supported.
- Preserve HDR10+ dynamic metadata only when the pipeline can carry it correctly; do not claim HDR10+ preservation merely because the base HEVC stream remains HDR.
- Dolby Vision profiles and RPU metadata need a dedicated compatibility matrix and legal review. Do not silently strip dynamic metadata.
- Keep P010/10-bit when the encoder supports it; never downgrade to 8-bit without explicit user consent.
- Avoid implicit RGB↔YUV round trips when exact sample identity is required.
- Document chroma siting and resampling filters where conversion is unavoidable.
- Maintain alpha only when input codec/container and output format both support it. For archival formats lacking expected alpha support in the Android path, fail closed or preserve original file.

Color standards reference: ITU-R BT.2020 specifies UHDTV colorimetry; ITU-R BT.2100 defines HDR television parameters including PQ/HLG. These values should drive metadata interpretation rather than ad-hoc display assumptions.

## 4.5 Container strategy

### MP4 / ISO Base Media File Format

Use MP4 for mainstream AVC/HEVC/AV1 output where the muxer and downstream apps support the specific codec mapping, sample entry, profiles and metadata. It has the broadest consumer ecosystem and good seekability. Use fragmented MP4 only when the recovery/streaming workflow needs it and the player compatibility matrix passes.

### Matroska

Use Matroska for FFV1 archival output and for workflows requiring broader preservation-oriented track and metadata support. FFV1's RFC explicitly identifies Matroska's `V_FFV1` Codec ID and configuration record behavior.

### Custom container

Do not create one in v1. A custom format immediately creates decoder distribution, parser security, compatibility, seekability, recovery, metadata, documentation, patent and long-term support burdens. A sidecar JSON manifest may record source hash, decoded-plane hashes, metadata audit, app/encoder version, codec version and verification result; it must not be required to decode the media.

### Recovery and atomic publication

- Write to a temporary file in the destination filesystem.
- Maintain a compact journal (input URI/identity, plan, current segment/frame, codec config, temp outputs, last verified boundary, checksums).
- Flush and fsync where supported before committing metadata and renaming.
- Publish only after mux finalization, full decode verification or the defined verification policy, and all metadata audits pass.
- Keep the original untouched until the new output is committed.
- For long jobs, checkpoint at independently decodable segment boundaries; do not pretend a hardware encoder's internal reference state can be resumed after process death.
- If using segmenting, each segment begins with an independently decodable keyframe; account for bitrate overhead and timestamp stitching. A segment concat test must pass on more than one demuxer/player.

## 4.6 Software stack

| Layer | Recommended technology | Rationale |
| --- | --- | --- |
| UI | Kotlin + Jetpack Compose | Android-native UI, lifecycle and accessibility |
| Job orchestration | Kotlin coroutines + WorkManager for durable background work | Structured cancellation, constraints and persistent jobs; foreground service only for user-visible long-running work where platform policy requires it |
| Media pipeline | Android `MediaExtractor`, `MediaCodec`, `MediaMuxer` where sufficient; Media3 Transformer as baseline/optional component | Uses platform codec components and reduces custom pipeline burden |
| Native engine | C++20 with Android NDK + CMake | Native codec integration, SIMD, FFmpeg libraries, explicit memory control |
| FFmpeg | Pinned, minimal build of libavformat/libavcodec/libavutil/libswscale only as required | Demux/mux, FFV1 and software codec support; minimize enabled codecs and license surface |
| Lossless mux | Matroska through libavformat or another conformance-tested muxer | Documented FFV1 mapping and archival ecosystem |
| Optional AV1 software path | Benchmark rav1e/libaom/SVT-AV1 arm64 builds before selecting one | No library should be chosen on desktop speed alone |
| GPU | Vulkan compute or OpenGL only for measured preprocessing | Avoid forced CPU round trips |
| ML experiments | Python, PyTorch, ONNX/LiteRT export experiments | Research and offline training; not part of required decoder |
| Product ML runtime | LiteRT CPU/GPU first; standalone NPU integration only where supported | NNAPI is deprecated in Android 15; avoid vendor lock-in in core path |
| Benchmark analysis | Python + NumPy/Pandas + libvmaf + plotting scripts | Reproducible analysis pipelines |
| Build/test | Gradle, CMake, GitHub Actions or self-hosted runners, Android CTS-style instrumentation tests | Deterministic CI and device matrix |

**License gate:** FFmpeg is LGPLv2.1-or-later by default but optional GPL components can change the combined binary's licensing obligations. FFmpeg's own legal page specifically calls out GPL libraries such as x264/x265. Audit the actual build configuration and every shipped binary. Do not assume “FFmpeg is LGPL” if GPL components are enabled.

## 4.7 Repository organization

Use a monorepo initially to keep app, native engine, codec manifests and test vectors versioned together. Split only when independent release cadence or ownership justifies it.

Suggested structure:

```text
android-video-compressor/
  app/                       # Compose UI, permissions, job UX
  core-job/                  # job state, scheduling, recovery journal
  core-capability/            # MediaCodec probe, device fingerprint, calibration
  media-android/              # MediaExtractor/Codec/Muxer/Media3 adapters
  media-native/               # C++ pipeline, FFmpeg wrappers, pixel formats
  codecs/
    ffv1/                     # lossless encoder/decoder integration
    av1/                      # optional encoder builds and manifests
    vendor-mediacodec/        # Android codec adapter; no private HAL deps
  containers/                 # MP4/Matroska policies and metadata mapping
  ml-research/                # separate Python experiments and model cards
  benchmark/                  # corpus manifest, scripts, metric configs
  test-vectors/               # bitstream/pixel/metadata vectors and hashes
  docs/                       # architecture, claims, licenses, SBOM, threat model
  third_party/                # pinned source, patches, license notices
```

Each codec adapter declares: accepted input formats, output profiles, determinism level, supported metadata, rate-control modes, known issues, license obligations, ABI and exact version/commit. A release is blocked if the manifest and binary SBOM disagree.

## 4.8 Security, privacy and reliability

- Treat all input media as untrusted. Fuzz container parsers, codec parsers, metadata parsers and resume journals.
- Decode in bounded processes where possible; use Android's public sandboxed codec components where they are exposed. Do not load arbitrary downloaded codec plugins.
- Cap dimensions, frame rate, duration, number of tracks, metadata size, recursion and decompressed frame allocation before allocation.
- Validate integer arithmetic for width × height × bytes-per-sample and stride calculations; guard overflow.
- Avoid path traversal and unsafe URI-to-filesystem assumptions. Use the Storage Access Framework/content URIs and persist URI permissions where appropriate.
- Do not upload video, thumbnails, model features or diagnostics by default. Make all cloud features opt-in and separate from offline compression.
- Provide a clear output manifest and error code; distinguish codec unsupported, corrupted source, out of storage, thermal stop, permission revoked and verification failure.
- Fuzz parsers and use adversarial malformed files, truncated streams, huge metadata blocks, timestamp discontinuities and repeated codec resets.
- Crash recovery must never report a partial output as complete.

---

# 5. Benchmark protocol and success scorecard

## 5.1 Test corpus

Use multiple independent content classes. No single dataset is enough.

1. **UVG 4K sequences:** high-quality high-resolution natural video, commonly used in learned-video-compression research.
2. **MCL-JCV:** natural video content with varied motion and texture; useful for learned-codec comparison.
3. **Netflix/VMAF internal-style content:** representative film/TV clips where permitted; do not redistribute copyrighted material without rights.
4. **Open test media:** Xiph/other permissively distributed sequences, with source license recorded.
5. **Screen capture:** text, code editor, scrolling UI, thin lines and static areas.
6. **Animation:** flat gradients, sharp edges, pans, fine line art.
7. **High motion:** sports, camera pan, handheld movement.
8. **Night/noise/grain:** low-light sensor noise and film grain.
9. **HDR:** HDR10/PQ and HLG content with documented color metadata.
10. **Synthetic stress:** impulses, checkerboards, exact gradients, alpha, chroma edge patterns and extreme dimensions for correctness, not perceptual scoring.

Record each clip's checksum, license, dimensions, frame rate, source codec, pixel format, bit depth, color metadata and duration. Build a fixed train/validation/test split for ML; avoid test-set leakage.

## 5.2 Quality metrics

Use multiple metrics because each has failure modes:

- **PSNR:** pixel-level squared-error metric; useful for exact fidelity/debugging, weak as a standalone perceptual quality measure.
- **SSIM / MS-SSIM:** structural similarity metrics; complement PSNR.
- **VMAF:** learned perceptual quality metric from Netflix. Use the current model appropriate to viewing condition and codec evaluation. VMAF v1 was released in 2026, including phone and 4K viewing-condition models; for encoder comparisons, consider the No Enhancement Gain (NEG) mode to avoid rewarding preprocessing enhancement as codec compression gain.
- **BD-rate / BD-PSNR:** compare rate-distortion curves over matched quality ranges. Report exact anchor codec, encoder version, presets, QP/rate points, metric, and content set.
- **Subjective study:** use a controlled double-stimulus or ACR-style protocol following current ITU-T P.910 / ITU-R BT.500 guidance; include mobile-screen viewing and HDR review where relevant.

Do not compare scores across incompatible VMAF models or use a VMAF number without the model/version and viewing assumptions. For archival lossless, do not use perceptual scores as the success criterion: compare decoded samples bit-for-bit.

## 5.3 Device and test conditions

For every device test, record:

- device model, SoC and RAM/storage SKU;
- Android version, build fingerprint, security patch and codec component name/version;
- battery level, battery temperature, thermal status/headroom, screen state/brightness, background processes and power mode;
- codec profile, resolution, bit depth, chroma, frame rate, GOP, bitrate/rate-control mode and encoder parameters;
- source clip checksum and starting point;
- wall time, CPU/GPU usage if available, dropped frames, encoded bytes, peak RSS, Java/native allocation, power/energy and temperature.

Perform at least three warm runs after a documented warm-up; randomize codec order to reduce thermal/order bias. Repeat under a cool baseline and a sustained thermal state. Publish medians and 95% confidence intervals or bootstrap intervals, not only best runs.

## 5.4 Power and thermal method

Use a controlled lab setup where possible: external power monitor / battery fuel gauge telemetry, identical display and radios state, fixed ambient temperature, screen off during background jobs unless required. Android Battery Historian/system tracing can assist diagnosis but app-level battery percentage deltas are too coarse for small jobs. Use Android thermal APIs to react to device state; do not treat the thermal headroom estimate as a precise temperature or guaranteed throttle predictor.

Calculate:

- energy per encoded minute (mWh/min);
- energy per megapixel-frame (mJ/Mpix-frame);
- energy per output GB and energy per VMAF point at a fixed rate/quality target;
- time-to-completion and sustained throughput;
- fraction of job spent in thermal throttling states.

Every reported energy number must identify the device, test clip, codec path, run count and measurement tool.

## 5.5 Proposed go/no-go targets

The following are **engineering targets**, not published device guarantees. Confirm or adjust them after day-30 baselines.

| Area | Proposed v1 gate | Baseline and test conditions |
| --- | --- | --- |
| Lossless sample correctness | 100% exact decoded-plane match, zero mismatched frames | All supported formats; synthetic patterns + real clips; FFV1 output decoded by independent FFmpeg build |
| Original file preserve | SHA-256 equality for byte-copy operation | At least 100 source files with diverse containers/tracks |
| Metadata | 100% preservation of declared required metadata or explicit blocked status | HDR, color, timestamps, rotation, audio, subtitles, chapters and side data corpus |
| Job robustness | 0 corrupt final outputs across 100 forced interruption/crash tests | Low-end and flagship devices; storage pressure and permission revocation included |
| 1080p30 hardware transcode | ≥30 encoded frames/s on the selected midrange/flagship class under cool conditions | 1080p30 corpus; actual vendor hardware encoder; 3+ runs; target, not universal promise |
| Low-end sustained transcode | ≥1.0× real-time at 720p30 for at least one validated hardware path | Entry device; 10-minute clip; battery/thermal measurements; proposed gate |
| Peak memory | ≤350 MiB incremental native+managed RSS on a 4 GB RAM target for 1080p; ≤700 MiB for 4K path on supported high-end devices | Long synthetic/real input; streaming pipeline; proposed limits to be revised with Android process kill behavior |
| Thermal stability | No severe thermal state sustained for >60 s in 10-minute standard job | Fixed ambient, screen off, documented device and battery state; proposed gate |
| Quality | At equal output quality, target ≥10% lower bitrate than AVC hardware baseline on aggregate if selecting a more efficient codec path | Matched VMAF v1/PSNR curves; at least 20 clips in 5 content classes; one device per tier; aspirational gate, not guaranteed |
| Energy | Chosen battery preset must not exceed AVC hardware baseline energy per megapixel-frame by >10% at matched quality | Same device and content; 3+ runs; proposed product threshold |
| Crash recovery | Job resumes or cleanly restarts without publishing partial/corrupt output | Forced process death at 10 randomized stages per long job |

A codec can win on average BD-rate but lose on animation or screen capture. Publish per-class curves and a weighted overall score. Do not optimize a single aggregate score if it hides unacceptable regressions for text, HDR or noisy content.

## 5.6 Composite “best” score

Use a two-stage decision rather than a single opaque score:

1. **Hard constraints:** correctness, decode compatibility, memory ceiling, no metadata corruption, supported profile, and no unacceptable thermal behavior.
2. **Pareto ranking:** among candidates meeting hard constraints, rank quality/bitrate, energy, speed and memory. Allow the user to choose the priority weights.

A default aggregate score can be:

`Score = wq × Q_norm + ws × S_norm + wt × T_norm + we × E_norm + wm × M_norm`

where `Q_norm` is normalized perceptual quality, `S_norm` file-size efficiency, `T_norm` throughput, `E_norm` energy efficiency and `M_norm` memory efficiency. Normalize within the tested device/content cohort; do not compare normalized scores across devices without calibration. Weights are preset-specific and disclosed. Hard constraints cannot be compensated by a high quality score.

---

# 6. Team, schedule, cost and milestones

## 6.1 Team

A credible first production release requires multiple disciplines:

- 1 senior Android/media architect (MediaCodec, containers, Android lifecycle);
- 1–2 native codec engineers (C/C++, FFmpeg, pixel formats, bitstream and muxing);
- 1 Android application engineer (Kotlin, Compose, WorkManager, SAF);
- 1 performance/QA engineer (device matrix, thermal/power, automated media tests);
- 0.5–1 ML/perception engineer for the optional research track only;
- 0.25–0.5 security/release engineer (fuzzing, SBOM, signing, CI);
- external counsel / patent counsel on a scoped legal review.

Smallest credible core team: 4–5 full-time equivalents, with security/legal support shared. Neural codec R&D increases staffing and schedule materially.

## 6.2 Schedule (9–12 months)

| Phase | Duration | Deliverables | Go/no-go |
| --- | ---: | --- | --- |
| 0. Scope and evidence baseline | 2 weeks | Lossless contracts, feature matrix, threat model, device shortlist | No-go if required source features cannot be retained or scope remains ambiguous |
| 1. Capability probe + baseline | 4 weeks | MediaCodec report tool, Media3 baseline, FFV1/Matroska host baseline, first device results | Proceed only if each target tier has a reliable lossy path and lossless path |
| 2. Core pipeline | 6–8 weeks | Streaming ingest/encode/mux, temp files, cancellation and progress | No memory growth with video duration; no data loss under cancellation |
| 3. Lossless archive | 4–6 weeks | FFV1 v3, Matroska, pixel hashes, metadata audit, corruption tests | Zero pixel mismatches across supported formats |
| 4. Lossy presets | 6–8 weeks | AVC/HEVC, optional AV1, quality/size/speed/battery controls | Must meet quality/energy trade-off gates on target devices |
| 5. Reliability + HDR | 4–6 weeks | HDR/color, subtitles/audio, crash recovery, parser fuzzing | No silent metadata loss; fuzz and interruption suite pass |
| 6. Beta and tuning | 4–6 weeks | Device matrix, telemetry opt-in, support diagnostics, user study | Performance and crash targets met across device tiers |
| 7. Release hardening | 2–4 weeks | SBOM, notices, privacy/data-safety review, store assets, signing and rollback | Legal, privacy, security and compatibility sign-off |
| Optional ML/NVC research | Parallel 3–6+ months | Encoder adviser benchmark; separate mobile neural decoder proof | Stay out of product if it fails any energy, reproducibility or interoperability gate |

## 6.3 Cost envelope

**[ESTIMATE; planning assumptions, not a vendor quote]** For an India-based engineering team with 4–6 FTE average over 9–12 months, assume fully loaded engineering cost of roughly ₹1.2–2.5 lakh per FTE-month, depending on seniority, employment model and overhead. This produces an engineering-only envelope of approximately ₹45–180 lakh. Add device lab procurement, storage, test automation, security review, legal/patent counsel and release overhead: approximately ₹15–50 lakh. A sensible planning envelope is therefore **₹60 lakh–₹2.3 crore**, excluding major commercial codec royalties, paid model/data licenses, large external patent settlements and marketing. These assumptions must be replaced by actual staffing and counsel quotes before budget approval.

A neural codec research program beyond the encoder-adviser experiment could add 2–4 FTE and 3–9 months, with GPU training/benchmark infrastructure and additional patent review. This is not included in the v1 budget.

---

# 7. Legal, licensing, patents and release

## 7.1 Codec patent position

- **AV1:** AOMedia publishes a patent license that grants a worldwide, perpetual, non-exclusive, no-charge, royalty-free patent license to defined Necessary Claims, subject to terms and defensive-termination provisions. This is a meaningful advantage, but not a guarantee against every third-party patent claim outside the license's scope. Include the license and comply with distribution conditions. [P-VERIFIED]
- **HEVC:** Access Advance operates an HEVC Advance patent pool and publishes royalty schedules; the current published schedule is dated effective 1 July 2026. Another pool or unpooled patent owners may exist. Product counsel must establish which activities and products trigger a license. [P-VERIFIED for pool publication; LEGAL REVIEW REQUIRED for applicability]
- **VVC:** Access Advance operates a VVC Advance pool and says its license covers patents it identifies as essential to VVC. Its terms are provided through a request process. The pool's existence means VVC should not be treated as royalty-free merely because an open-source encoder exists. [P-VERIFIED]
- **2027 licensing change:** On 11 November 2026, Access Advance announced that effective discounts under its Multi-Codec Bridging Agreement for HEVC/VVC pool licensees signing from 1 January 2027 will be reduced in phases compared with qualifying licensees joining before 31 December 2026. The actual impact depends on the licensee's products and agreement; this is a reason to obtain counsel's cost model before choosing HEVC+VVC. [P-VERIFIED for announcement]
- **EVC:** ISO/IEC standardization and profile design do not automatically resolve every patent or implementation claim. Require a written patent analysis for the selected profile and territories. The “baseline” licensing position remains unverified for this specific commercial implementation until counsel checks patent declarations and agreements.
- **LCEVC:** Standardized and has commercial licensing activity; no assumption of free use. Obtain actual license terms for the planned app/service distribution model.
- **FFV1:** RFC 9043 is an IETF Informational specification; code license obligations depend on the chosen FFmpeg build and any other libraries. FFmpeg has its own LGPL/GPL configuration distinction.
- **Research code:** paper code may have source licenses, checkpoint terms, dataset restrictions and patent rights independent of the paper. Audit all four.

## 7.2 Open-source compliance

Maintain per-release:

- SPDX license identifiers and a machine-generated SBOM (CycloneDX or SPDX);
- source URL, commit hash, build flags, patches and compiler version for every third-party library;
- notices and license texts in the app's legal screen and distribution package;
- LGPL relinking / replacement / source offer compliance where relevant;
- GPL exclusion unless the product is intentionally prepared to comply with the combined GPL terms;
- model cards, training-data rights and weight licenses for ML assets;
- a patent-risk register separate from the software license register.

FFmpeg's own legal page states it is LGPLv2.1-or-later by default but that enabling GPL components, including x264/x265 in its examples, changes the licensing consequences. Do not ship a build whose configure flags or transitive dependencies have not been reviewed.

## 7.3 Release and adoption strategy

1. Private engineering preview for device/corpus validation.
2. Closed beta on explicitly supported device tiers; publish codec capability diagnostics.
3. Public release with clear labels: preserve source, lossless pixel archive, lossy; explain metadata and unsupported-input behavior.
4. Release notes list changed codec library versions and known device-specific issues.
5. Optional benchmark page publishes methodology, not cherry-picked marketing percentages.
6. No remote upload by default. Make privacy and offline operation a differentiator.
7. Provide reproducible test clips and output verification tools for users who need archival confidence.
8. Maintain an LTS path for old output files; an archival decoder should remain available even if the app UI evolves.

---

# 8. Red-team analysis

| Failure mode | Why it happens | Detection | Mitigation / fallback |
| --- | --- | --- | --- |
| Codec advertised but unusable at requested size/profile | Vendor capability metadata may be incomplete, device policy or firmware differences | Real short encode/decode smoke test; profile and output parser | Re-probe; fallback to next validated component or AVC |
| Hardware flag is wrong | Android docs say the flag is vendor-provided and cannot be tested for correctness | Compare observed CPU/GPU load and sustained throughput | Treat flag as hint, not truth |
| AV1 decode mistaken for AV1 encode | Product specs often list “AV1” without separating direction | Enumerate encoder components independently | UI exposes only verified encode path |
| VVC Android 17 assumed universal | AOSP VVC support still depends on vendor Codec2 decoder; no AOSP encoder | Query `video/vvc`; run test stream | VVC input only when present; never promise VVC output in v1 |
| Lossless claim hides pixel conversion | YUV/RGB conversion, 8-bit downshift, chroma resampling, tone mapping or decoder behavior | Per-frame plane hashes and metadata audit | Identity-only transform; fail closed for unsupported format |
| FFV1 output grows | Source has noise/grain or high entropy; decoded lossy artifacts reduce redundancy | Compression ratio by content class | Warn, allow exact copy, test alternative mature reversible codec |
| HDR dynamic metadata lost | Muxer or encoder does not preserve side data | HDR test corpus + metadata parser/player checks | Preserve original stream or block unsupported transcode |
| Phone overheats midway | Long software encode, NPU/GPU contention, ambient heat | Thermal API, throughput trend, energy telemetry | Pause/reduce complexity; let user resume; favor hardware path |
| Memory spikes on 4K | Full-frame copies, multiple buffers, stride mistakes | Peak RSS and allocation tracing | Bounded queues, surface path, tiled processing, reject unsupported dimensions |
| Neural decoder output differs by vendor | Backend kernels/rounding/quantization differ | Cross-backend hash vectors | No learned decoder; conforming CPU implementation fallback |
| ML filter improves metric but destroys texture | Metric or model over-optimizes training domain | Grain/text/animation subjective review | Keep optional; separate prefilter gain from codec gain; off by default |
| Exact target size misses | CBR/VBR/CQ is implementation-specific; variable content and container overhead | Actual output file measurement | Iterative re-encode or report estimated size range |
| Crash leaves plausible-looking broken output | Muxer not finalized or partial write committed | Inject process death at all pipeline states | Temp files, journal, checksum, atomic commit, final decode verification |
| File parser exploit / resource exhaustion | Media files are untrusted and complex | Fuzzers, sanitizer builds, corpus regression | Bounds checks, resource caps, update dependencies, fail closed |
| Licensing violation | Unreviewed GPL component, patent pool, model weights or patent claims | SBOM and legal audits | Release gate; remove component or obtain license |
| 2027–2030 hardware assumptions fail | SoC roadmaps change; handset OEMs expose different blocks | Quarterly device survey and runtime telemetry (opt-in) | Capability-driven abstraction; no chipset hardcoding |

---

# 9. Immediate experiments: smallest proof and top 10 next experiments

## 9.1 Smallest experiment that proves the core numbers

Build one bench harness, not a full app:

- Input: 10 representative 1080p/4K clips, each with source hash and metadata manifest.
- Devices: one entry-level phone, one midrange phone, one flagship from a second SoC vendor.
- Lossless: decode each supported input to the declared sample format, encode FFV1 v3 into Matroska, decode with an independent decoder, compare every plane and frame hash, measure output ratio/time/RSS/energy.
- Lossy: test AVC and HEVC hardware encoders; test AV1 only where a real encoder component exists. Use four bitrate/quality points per codec, decode output, measure VMAF v1 appropriate model, PSNR and SSIM. Record device build and encoder component.
- Stress: run the longest clip at 4K, cancel, kill process, revoke permission and run low-storage scenario.

This is the smallest useful experiment because it tests the two critical assumptions: that lossless output is truly reversible and that the actual mobile encoders deliver a useful quality/energy trade-off.

## 9.2 Ten best next experiments, ranked

1. **Pixel-exact FFV1 round trip:** compare all decoded samples on 8-bit and 10-bit 4:2:0 sources and synthetic edge/gradient patterns. Kill the FFV1 choice if exactness fails or format restrictions make the product claim too narrow.
2. **MediaCodec capability survey:** collect complete encoder/decoder component/profile/color/rate-control data from 12–20 real phones across tiers. This resolves codec availability better than chipset marketing pages.
3. **HEVC vs AVC hardware encode:** matched VMAF v1 / PSNR curves at multiple rate points on low, mid and flagship devices; measure battery and thermals.
4. **AV1 encoder existence and reality test:** separate decode from encode capability; validate output profiles, actual speed, quality and power on every candidate handset.
5. **Zero-copy vs copied-buffer pipeline:** benchmark surface input, `AHardwareBuffer` import and CPU conversion paths; measure memory bandwidth, time and energy.
6. **HDR metadata preservation:** create an HDR10/HDR10+/HLG input battery; inspect bitstream/container metadata and verify on independent playback devices.
7. **FFV1 vs other lossless codecs:** test archival ratio, speed, decode time, energy and long-term interoperability on screen content, natural footage, noise/grain, 10-bit and 4:4:4.
8. **Thermal governor experiment:** compare one aggressive long encode against a thermal-aware policy for 20 minutes; measure completed work, total energy and user-visible throttle states.
9. **Target file-size iteration:** test first-pass/second-pass and trial-encode control versus one-pass VBR/CQ; quantify size error, elapsed time and energy.
10. **Encoder-adviser ML ablation:** test compact scene/motion/noise classification against a hand-built heuristic. Require better equal-size quality or reduced energy, and reject it if inference overhead erases the benefit.

---

# 10. Appendices

## Appendix A — source register (primary sources prioritized)

**Access date for live pages:** 14 November 2026 unless a publication/approval date is shown. URLs are provided so the team can pin exact versions/commits before implementation.

### Standards and codec specifications

1. ITU-T H.264 V16 (approved 13 June 2026): <https://www.itu.int/rec/T-REC-H.264-202606-I/en>
2. ITU-T H.265 V11 (approved 13 January 2026): <https://www.itu.int/rec/T-REC-H.265-202601-I/en>
3. ITU-T H.266 V4 (approved 13 January 2026): <https://www.itu.int/rec/T-REC-H.266-202601-I>
4. ITU-T H.266.1 V3 conformance spec (approved 13 January 2026): <https://www.itu.int/rec/T-REC-H.266.1-202601-I>
5. ISO/IEC JTC 1/SC 29 catalogue, including EVC/LCEVC and related parts: <https://www.iso.org/committee/45316/x/catalogue/>
6. IETF RFC 9043, FFV1 Video Coding Format Versions 0, 1, and 3 (August 2021): <https://www.rfc-editor.org/rfc/rfc9043.html>
7. Matroska codec mappings (`V_FFV1`): <https://www.matroska.org/technical/codec_specs.html>
8. Matroska v4 element specification: <https://www.matroska.org/technical/elements.html>
9. JPEG XS overview / ISO/IEC 21122: <https://jpeg.org/jpegxs/>
10. JPEG 2000 overview / ISO/IEC 15444: <https://jpeg.org/jpeg2000/>

### Android platform and APIs

 1. Android `MediaCodec` API: <https://developer.android.com/reference/android/media/MediaCodec>
 2. Android `MediaCodecInfo` API: <https://developer.android.com/reference/android/media/MediaCodecInfo>
 3. Android media codec performance documentation: <https://developer.android.com/media/optimize/performance/codec>
 4. Android `MediaExtractor` API: <https://developer.android.com/reference/android/media/MediaExtractor>
 5. Android NDK media APIs: <https://developer.android.com/ndk/reference/group/media>
 6. Android `AHardwareBuffer` NDK API: <https://developer.android.com/ndk/reference/group/a-hardware-buffer>
 7. Android Media3 Transformer docs: <https://developer.android.com/media/media3/transformer>
 8. Android 16 Compatibility Definition Document, video codec section: <https://source.android.com/docs/compatibility/16/android-16-cdd>
 9. Android 17 VVC support (AOSP): <https://source.android.com/docs/core/media/vvc>
10. Android 17 supplemental media performance class CDD information: <https://source.android.com/docs/compatibility/preview/mpc>
11. Android PowerManager thermal APIs: <https://developer.android.com/reference/android/os/PowerManager>
12. Android NNAPI migration guide (NNAPI deprecated in Android 15): <https://developer.android.com/ndk/guides/neuralnetworks/migration-guide>
13. Android NNAPI platform status: <https://source.android.com/docs/core/ota/modular-system/nnapi>
14. Android LiteRT deployment: <https://developer.android.com/ai/custom>

### Vendor specifications

 1. MediaTek Dimensity 9500 official product specifications: <https://www.mediatek.com/products/smartphones/mediatek-dimensity-9500>
 2. MediaTek Dimensity 9400 official product specifications: <https://www.mediatek.com/products/smartphones/mediatek-dimensity-9400>
 3. MediaTek Dimensity 8400 official product specifications: <https://www.mediatek.com/products/smartphones/mediatek-dimensity-8400>
 4. MediaTek Dimensity 7300 official product specifications: <https://www.mediatek.com/products/smartphones/mediatek-dimensity-7300>
 5. Qualcomm Snapdragon 8 Gen 3 product page: <https://www.qualcomm.com/smartphones/products/8-series/snapdragon-8-gen-3-mobile-platform>
 6. Google Pixel 10 Pro specifications (video formats and capture capabilities): <https://store.google.com/product/pixel_10_pro_specs?hl=en-US>

### Neural compression research

 1. MobileNVC, IEEE Xplore paper record (WACV 2024): <https://ieeexplore.ieee.org/document/10484439>
 2. MobileNVC arXiv full text: <https://arxiv.org/abs/2310.01258>
 3. Microsoft DCVC research code repository: <https://github.com/microsoft/DCVC>
 4. DCVC-RT, CVPR 2025 proceedings page: <https://openaccess.thecvf.com/content/CVPR2025/html/Jia_Towards_Practical_Real-Time_Neural_Video_Compression_CVPR_2025_paper.html>
 5. DCVC context modulation, CVPR 2025: <https://openaccess.thecvf.com/content/CVPR2025/html/Tang_Neural_Video_Compression_with_Context_Modulation_CVPR_2025_paper.html>
 6. DCVC-UF, CVPR 2026 proceedings page: <https://openaccess.thecvf.com/content/CVPR2026/html/Li_Ultra-Fast_Neural_Video_Compression_CVPR_2026_paper.html>
 7. ELF-VC, ICCV 2021 paper: <https://openaccess.thecvf.com/content/ICCV2021/html/Rippel_ELF-VC_Efficient_Learned_Flexible-Rate_Video_Coding_ICCV_2021_paper.html>

### Quality measurement and test methodology

 1. Netflix VMAF repository and release notes: <https://github.com/Netflix/vmaf>
 2. Netflix VMAF v1 model guidance: <https://github.com/Netflix/vmaf/blob/master/resource/doc/models_v1.md>
 3. Netflix VMAF v0 model guidance, including mobile viewing model: <https://github.com/Netflix/vmaf/blob/master/resource/doc/models_v0.md>
 4. ITU-T P.910 (July 2026 edition): <https://www.itu.int/rec/T-REC-P.910-202607-P/en>
 5. ITU-R BT.500-15 subjective television-image assessment (May 2023): <https://www.itu.int/rec/R-REC-BT.500>
 6. ITU-R BT.2020 UHDTV colorimetry: <https://www.itu.int/rec/R-REC-BT.2020>

### Licensing and legal

 1. AOMedia Patent License 1.0: <https://aomedia.org/license/patent-license/>
 2. AOMedia legal/licensing overview: <https://aomedia.org/about/legal/>
 3. Access Advance HEVC Advance pool: <https://accessadvance.com/licensing-programs/hevc-advance/>
 4. Access Advance HEVC royalty schedule effective 1 July 2026: <https://accessadvance.com/hevc-advance-patent-pool-detailed-royalty-rates/>
 5. Access Advance VVC Advance pool: <https://accessadvance.com/licensing-programs/vvc-advance/>
 6. Access Advance MCBA discount announcement (11 November 2026): <https://accessadvance.com/2026/11/11/access-advance-announces-new-effective-discount-rates-for-its-multi-codec-bridging-agreement-starting-on-january-1-2027/>
 7. Access Advance VVC Advance Software Policy (10 August 2021 PDF): <https://accessadvance.com/wp-content/uploads/2023/08/VVC-Advance-Software-Policy.pdf>
 8. FFmpeg license and legal considerations: <https://www.ffmpeg.org/legal.html>
 9. FFmpeg license file: <https://github.com/FFmpeg/FFmpeg/blob/master/LICENSE.md>
10. VVenC license text (BSD-3-Clause; explicitly no patent license granted by that software license): <https://github.com/fraunhoferhhi/vvenc/blob/master/LICENSE.txt>
11. rav1e manifest and license metadata: <https://github.com/xiph/rav1e/blob/master/Cargo.toml>

## Appendix B — claim register and confidence

| Claim | Label | Confidence / qualification |
| --- | --- | --- |
| Android 17 AOSP documents VVC decoding integration but no AOSP VVC software decoder and no VVC encoding | P-VERIFIED | High; use official AOSP page; vendor decoder still required |
| NNAPI NDK API is deprecated from Android 15 | P-VERIFIED | High; Android NDK migration guide and AOSP page |
| Android `isHardwareAccelerated()` is vendor-provided and cannot be tested for correctness by the API | P-VERIFIED | High; Android media codec docs |
| MediaTek Dimensity 9500 advertises AV1 10-bit decode and AVC/HEVC encode capabilities at listed resolutions | P-VERIFIED | Applies to vendor-published 9500 platform only; not every handset implementation |
| Pixel 10 Pro public specs list HEVC, AVC, AV1 and VP9 formats | P-VERIFIED | Does not identify exact MediaCodec profile, encode/decode direction per component or sustained performance |
| MobileNVC reports real-time 1080p YUV420 mobile neural decoding and up to 48% BD-rate improvement vs its stated prior on-device baseline | P-VERIFIED | Paper result; benchmark device and baseline in the paper; not a universal Android saving |
| DCVC-RT paper reports 1080p speed and bitrate results against VTM | P-VERIFIED | Research-paper result; not proof of Android arm64 performance |
| FFV1 RFC defines lossless intra-frame coding and Matroska mapping | P-VERIFIED | High; RFC is Informational, not Standards Track |
| VMAF v1 model set was released in 2026 and includes phone/4K viewing-condition guidance | P-VERIFIED | High; Netflix repository docs; metric still requires subjective cross-check |
| AV1 is royalty-free under AOMedia's defined patent license subject to its terms | P-VERIFIED | High as to the license text; not a universal FTO guarantee against all third-party patents |
| HEVC/VVC patent-pool royalties may apply to commercial deployment | P-VERIFIED | High that pools and terms exist; applicability and cost depend on counsel/contract |
| HEVC is the best default hardware codec on every Android handset | **Not claimed** | Unsupported; the plan instead probes and benchmarks each device |
| FFV1 always compresses video 2:1–4:1 | **Rejected** | Unsupported generalization; ratio is content-dependent |
| 8-bit quantized neural models decode identically on all vendors | **Rejected** | False as a general guarantee; operation semantics and backend results must be verified |
| 2027–2030 SoCs will all implement hardware VVC/AV1 encoders | **Speculation; not used as a plan assumption** | Must be re-evaluated against released device documentation and runtime probes |
| Proposed schedule, cost and gate thresholds | ESTIMATE / AUTHOR TARGET | Planning assumptions; must be replaced by empirical results and bids |

## Appendix C — glossary

- **AOMedia:** Alliance for Open Media, the organization responsible for AV1 and related specifications/licensing.
- **BD-rate:** Bjøntegaard delta bitrate; an estimate of the bitrate difference between two rate-distortion curves over a matched quality range.
- **Bitstream:** the encoded elementary video data, excluding or distinguished from its container.
- **Codec2:** Android's lower-level codec component framework used by platform/vendor media codecs.
- **CQ:** constant-quality rate-control mode; implementation-specific and not universally supported.
- **CSD:** codec-specific data required to initialize a decoder, such as parameter sets.
- **FFV1:** FFmpeg Video Codec 1, an intra-frame lossless codec documented in RFC 9043.
- **GOP:** group of pictures; a sequence of frames organized around keyframes and inter-predicted frames.
- **HDR10 / HDR10+ / HLG / PQ:** HDR signaling/format families and transfer functions; metadata must be preserved separately from pixel values.
- **ISP:** image signal processor, which transforms raw camera sensor data into processed image/video samples.
- **MediaCodec:** Android public API for accessing media codec components.
- **NPU:** neural processing unit, a hardware accelerator for machine-learning inference.
- **P010:** a common 10-bit 4:2:0 semiplanar storage format using 16-bit containers per sample.
- **Rate-distortion:** relationship between coded bitrate and reconstruction quality.
- **SEI:** supplemental enhancement information in video bitstreams, carrying metadata and other side information.
- **SoC:** system-on-chip; includes CPU/GPU/media/AI and other blocks, but public SoC claims do not prove handset API exposure.
- **VMAF:** Video Multi-Method Assessment Fusion, a perceptual video-quality metric developed by Netflix.

## Appendix D — open questions to resolve before production

1. Which retail handsets and firmware versions define the launch support matrix?
2. Which exact FFmpeg build and license configuration can ship without GPL components?
3. Which Android `MediaMuxer`/Media3 paths preserve required HDR, subtitle and timed metadata for each output container?
4. Which AV1 hardware encoders are exposed on actual target handsets, and which profiles/rate-control modes are usable?
5. Does FFV1 software encode meet time/energy budgets on entry-tier devices, or should lossless pixel transcode be deferred to charging/idle windows?
6. Which commercial patent pools and unpooled claims apply to the intended territories, app distribution model and codec functionality?
7. What objective VMAF model and subjective protocol best predict the target user's phone-screen viewing conditions?
8. Is there a compelling product use case for LCEVC/EVC after real license and device-stack costs are included?
9. Can the product preserve Dolby Vision and HDR10+ metadata across chosen paths, or must those sources be limited to exact-copy/remux?
10. What is the minimum supported Android API level that balances market coverage with modern codec, thermal and background-job APIs?

---

# 11. Final self-audit

## 11.1 Weakest claims and verification limits

1. **Future hardware:** 2027–2030 mobile SoC capability predictions remain speculative. This plan intentionally avoids depending on them. Actual product selection must be updated as retail devices ship.
2. **Per-device codec performance:** vendor SoC specifications do not expose all phone OEM firmware configurations, Android component registrations or sustained thermal behavior. The complete answer can only come from physical-device tests.
3. **Neural codec mobile readiness:** MobileNVC demonstrates a specialized mobile pipeline, but its results cannot be extrapolated to generic current/future phones. DCVC research is highly promising, yet its published GPU training/inference stack is not evidence of production Android arm64 performance.
4. **Codec compression superiority:** there is no universal best codec. The result changes with content, profile, rate control, encoder build, device, power state and metric.
5. **FFV1 compression ratios:** no universal ratio is verified. A content-stratified test is mandatory.
6. **Patent freedom to operate:** the patent pool pages establish that relevant pools exist and publish/licence terms; they do not provide an exhaustive freedom-to-operate opinion for this product, every jurisdiction, every codec profile or every app distribution model.
7. **EVC baseline and LCEVC commercial terms:** official standard catalogues establish standardization, but this research snapshot does not establish a complete patent/licensing position for a commercial Android compressor. Mark unverified until legal review.
8. **Full standard text access:** this report uses official standards registry pages, summaries, API documentation, RFC text and public research papers. It does not claim that every paywalled or restricted full ISO/ITU standards document was obtained and read in entirety. Obtain the normative texts and conformance suites before implementing a decoder or claiming standards conformance.
9. **Cost estimate:** the ₹60 lakh–₹2.3 crore planning envelope depends on India-based staffing assumptions and can change substantially with seniority, device procurement, patent counsel, royalties and scope.
10. **Novelty and patent search:** the proposed capability fingerprint, job governor and verification-first workflow are engineering combinations, not asserted inventions. A complete patentability/FTO search has not been performed.

## 11.2 What would invalidate the recommendation

- FFV1 fails exact plane round-trip for a required format or cannot preserve required metadata in the chosen packaging workflow.
- The target handset set exposes no reliable hardware lossy encoder and software fallback cannot meet acceptable energy/time limits.
- Licensing analysis shows the selected output codecs are not commercially viable for the intended markets.
- A standard, interoperable codec with a demonstrated cross-vendor mobile decoder appears and achieves a major measured gain with lower energy and no significant legal risk.
- A learned decoder becomes standardized or fully specified with proven bit-exact output and broad low-power hardware support, and independent tests confirm it.

## 11.3 Bottom line

The strongest product is not necessarily the one with the newest codec or the most elaborate neural network. It is the one that preserves exactly what it promises, selects codecs from measured device capabilities, produces predictable quality/size/power trade-offs, preserves HDR/audio/metadata correctly, survives interruptions, and can decode its archival outputs years later.

**Recommended sequence:** ship a robust conventional Android transcoder and FFV1 archival path first; add AV1/VVC inputs and software high-efficiency options only as device capability and licensing evidence justify them; keep learned video coding as a separately gated research program.
