# Research ledger — verified facts with dates (compiled 2026-11-14)

Tag key: VP=verified primary, VS=verified secondary, AC=author claim, EST=estimate, SPEC=speculation.

## A. Standards / codecs

| # | Fact | Date | Tag | Source |
| --- | --- | --- | --- | --- |
| A1 | AV2 spec v1.0.0 + AVM reference software v1.0.0 published; "coding tools complete", future versions add profiles/metadata only | 2026-05-28 (spec), 2026-06-09 (press) | VP | <https://av2.aomedia.org/> ; <http://aomedia.org/press%20releases/Alliance-for-Open-Media-Releases-AV2-Codec/> |
| A2 | AV2 ≈30% better compression than AV1 on PSNR/VMAF; "visual gains closer to 40%" (AOM chairs Norkin/Grange) | 2026-07-27 blog | AC | <http://aomedia.org/blog%20posts/AV2-What-the-Final-Specification-Means-for-Implementers/> |
| A3 | AV2 adds multi-stream/multi-view in one bitstream (texture+depth, fg+bg+alpha), screen content, wider quality range, AR/VR | 2026 | VP | <https://av2.aomedia.org/> |
| A4 | Working draft v13 dated 2026-01-05 superseded by v1.0.0 | 2026-01-05 | VP | <https://av2.aomedia.org/> |
| A5 | YouTube: >90% of catalog (watch-time weighted) available in AV1 as of 2026-03-19; YouTube AV2 web streams "as soon as early 2027" | 2026-10-22 | AC (Des Keane, YouTube) | <http://aomedia.org/press%20releases/AOMedia-Rapid-AV2-Adoption-Across-the-Video-Ecosystem/> |
| A6 | Intel developing SVT-AV2 (Moccagatta quote); Meta commits AV2 productization | 2026-10-22 | AC | same as A5 |
| A7 | No shipping AV2 hardware decode as of Oct 2026; VeriSilicon decode IP core ~2 weeks after spec | 2026-10 | VS | <https://overclock3d.net/news/misc/av2-video-is-coming-to-youtube-in-early-2027/> ; <https://www.linkedin.com/pulse/how-fast-av2-deploy-ask-joe-sixpack-jan-ozer-qtsdc> |
| A8 | Analyst forecast: first AV2-capable SoCs 2027, midrange phones/TVs 2028 (12–24 mo IP→silicon) | 2026-10 | VS/EST | <https://www.forasoft.com/learn/video-encoding/articles/av2-what-to-expect-2026-2031> |
| A9 | Android 17 adds VVC/H.266 platform support (video/vvc MIME, MP4 parsing) but does NOT bundle a decoder; OEM/SoC must supply | 2026-02..2026-10 | VP | <https://source.android.com/docs/core/media/vvc> ; <https://streaminglearningcenter.com/articles/vvc-on-the-rise.html> |
| A10 | MediaTek Dimensity 9600 Pro = first smartphone SoC with hardware VVC decode (+MV-HEVC spatial video), 2nm | 2026-10-01 | VS | <https://counterpointresearch.com/en/insights/mediatek-dimensity-9600-pro> |
| A11 | ECM (JVET exploration beyond VVC): ~17% AI / ~28% RA / ~24% LD max BD-rate reduction vs VVC (Wein, IBC 2026 paper) | 2026-09 | VS | <https://www.ibc.org/files/live/sites/ibc/files/2026/9-September/Tech%20Papers/Next%20generation%20video%20compression%20standards/%2344303%2C%20Mathias%20Wein.pdf> |
| A12 | APV (Advanced Professional Video, Samsung) published as IETF RFC 9924 | 2026-02 | VP | <https://www.rfc-editor.org/info/rfc9924/> |
| A13 | APV = intra-only, visually/near-lossless professional mezzanine; openapv ref impl under Academy Software Foundation; APV-ISOBMFF + RTP payload drafts | 2026 | VP | <https://github.com/academysoftwarefoundation/openapv> |
| A14 | Snapdragon 8 Elite Gen 5 = first mobile chip with APV support; Galaxy S26 Ultra ships APV capture up to 8K30, ~10% less storage vs alternatives (Samsung claim) | 2025-09-24 / 2026-03-23 | VS + AC | <https://www.androidauthority.com/qualcomm-snapdragon-8-elite-gen-5-3599341/> ; <https://www.samsungmobilepress.com/articles/samsungs-pro-grade-apv-codec-on-galaxy-s26-ultra> |
| A15 | Snapdragon 8 Elite Gen 5 product brief lists HW accelerated H.265/VP9/AV1 **decoder**; HDR10+/HDR10/HLG/Dolby Vision playback | 2025-09 | VP | <https://www.qualcomm.com/content/dam/qcomm-martech/dm-assets/documents/Snapdragon-8-Elite-Gen-5-product-brief.pdf> |
| A16 | FFV1 standardized as IETF RFC 9043 (versions 0,1,3), lossless intra-frame | 2021-08 | VP | <https://www.rfc-editor.org/info/rfc9043/> |
| A17 | JPEG XL: lossless WebP 9–17% larger for photos, lossless AVIF 30–120% larger (Archibald test) | 2026-10-15 | VS | <https://jakearchibald.com/2026/jpeg-xl-vs-avif/> |

## B. Neural codecs

| # | Fact | Date | Tag | Source |
| --- | --- | --- | --- | --- |
| B1 | DCVC-RT (CVPR 2025): 125.2 fps enc / 112.8 fps dec 1080p on A100; 39.5/34.1 fps on RTX 2080Ti; ~21% BD-rate saving vs H.266/VTM | 2025-02-27 arXiv 2502.20762 | VP (paper) | <https://arxiv.org/abs/2502.20762> ; <https://streaminglearningcenter.com/articles/evaluating-dcvc-rt-a-real-time-neural-video-codec-that-delivers-on-speed-and-compression.html> |
| B2 | DCVC-UF (CVPR 2026, arXiv 2606.04410): chunk-based coding, multiple frames simultaneously, no explicit MV coding, parallel decode | 2026-06-03 | VP | <https://arxiv.org/abs/2606.04410> ; <https://openaccess.thecvf.com/content/CVPR2026/papers/Li_Ultra-Fast_Neural_Video_Compression_CVPR_2026_paper.pdf> |
| B3 | DCVC-RT model sizes FP32: I-frame model 174 MB, P-frame model 79 MB (as reported by MLVC paper) | 2026-07 | VS (3rd-party reporting of DCVC) | <https://arxiv.org/html/2606.28027v2> §3.3 |
| B4 | **MLVC** (Microsoft, ECCV 2026, arXiv 2606.28027v2 2026-07-29): cross-platform NVC. >70% BD-rate(MOS) improvement vs hardware HEVC (HEVC-QSV) on VCD conferencing benchmark; enc+dec ~100 FPS avg on Apple/Intel/Qualcomm NPUs | 2026-07-29 | VP | <https://arxiv.org/html/2606.28027v2> |
| B5 | MLVC key trick = **scale sending**: derive entropy-model scale indices from the *quantized hyperlatent* ẑ (coded with fixed factorized model ⇒ identical both sides). Structured sharing reduces scale param count by s²·r ≈128×. Removes catastrophic entropy desync **without** bit-exact arithmetic | 2026-07 | VP | arXiv 2606.28027v2 §3.1 |
| B6 | MLVC also: ReLU/LeakyReLU only (ANE sweep found only these are exact); gated memory; LTR recovery frames; periodic I-frames to resync FP drift; unified I/P model (I = P with gray 0.5 reference) | 2026-07 | VP | arXiv 2606.28027v2 §3.2–3.4 |
| B7 | MLVC open-sourced MIT: model code, weights, training scripts, data-collection docs, platform conversion scripts (github.com/microsoft/mlvc); C++ runtime libmlvc first published 2026-09-11, active as of 2026-11-04 | 2026-07-24 blog | VP | <https://techcommunity.microsoft.com/blog/linuxandopensourceblog/announcing-the-open-source-release-of-ml-video-codec-mlvc/4539875> ; <https://github.com/microsoft/libmlvc> |
| B8 | MLVC Microsoft blog bitrate savings (P.910 subjective, VCD dataset, equal inference compute): 360p 87.8% vs H.264 / 75.5% vs H.265; 540p 82.7% / 65.4% | 2026-07-24 | AC | same blog |
| B9 | MLVC "NPU-first, ≤50% NPU utilization", demonstrated 540p30 on Apple/Intel/Qualcomm; rolling out in Microsoft Teams with fallback to conventional codecs | 2026-07-24 | AC | same blog |
| B10 | Integer-Centric NVC (Jia et al., ICLR 2026 submission 1445): integer-from-scratch training, "multiply-twice integerization", memorized temporal modeling; claims 20% BD-rate saving vs VTM, 153.0/137.3 fps 1080p. **REJECTED at ICLR 2026 (decision 2026-01-26)**; AC: quantization reduces performance significantly esp. at high bitrate; gains mostly from LSTM memory | 2026-01-26 | VP (reviews) | <https://openreview.net/forum?id=KCQo0fXtFH> |
| B11 | Streamable NVC / mixed precision (Arunruangsirilert, Sun, Katto; arXiv 2608.00483, 2026-08-01): INT8 can cost **up to 80% BD-rate penalty** (MobileNVC); INT16 avoids drift but up to **5× slowdown** on DCVC-RT because accelerators are FP16/INT8-optimized. Proposes FP16 P-frames + IEEE-754 FP32 I-frames/resets; tested cross-encode/decode across 12 GPUs / 4 generations | 2026-08-01 | VP | <https://arxiv.org/html/2608.00483> |
| B12 | MobileNVC (van Rozendaal et al., Qualcomm AI Research, WACV 2024, arXiv 2310.01258): first neural codec decoding 1080p YUV420 in real time (30+ fps) on a mobile device NPU | 2023-10 / 2024-01 | VP | <https://arxiv.org/abs/2310.01258> |
| B13 | Qualcomm AI Research 2021: first software neural video decoder, 1280x704 @30+fps on Snapdragon 888 | 2021-06-16 | AC | <https://www.qualcomm.com/news/onq/2021/06/worlds-first-software-based-neural-video-decoder-running-hd-format-real-time> |
| B14 | NeuralLVC — "Neural Lossless Video Compression via Masked Diffusion" (arXiv 2604.03353, 2026-04-03): beats H.264/H.265 lossless on 9 Xiph CIF sequences; claims exact reconstruction verification | 2026-04-03 | AC (preprint) | <https://arxiv.org/html/2604.03353v1> |

## C. Platform / hardware

| # | Fact | Date | Tag | Source |
| --- | --- | --- | --- | --- |
| C1 | NNAPI deprecated in Android 15 (API 35); migration guide points to LiteRT / vendor SDKs | doc updated 2026-03-06 | VP | <https://developer.android.com/ndk/guides/neuralnetworks/migration-guide> |
| C2 | LiteRT NPU delegates supplied by chip vendors (Qualcomm, MediaTek, Samsung…); Google-published NPU overview | 2026-06-02 | VP | <https://developers.google.com/edge/litert/android/npu/overview> |
| C3 | LiteRT + Qualcomm QNN NPU path: FastVLM TTFT 0.12 s on Snapdragon 8 Elite Gen 5 NPU | 2025-11-24 | AC (Google) | <https://developers.googleblog.com/unlocking-peak-performance-on-qualcomm-npu-with-litert/> |
| C4 | Snapdragon 8 Elite Gen 5 NPU supports INT2/INT4/INT8/INT16/FP8/FP16 | 2025-12-02 | VS (Qualcomm dev blog) | <https://www.qualcomm.com/developer/blog/2025/11/nexa-ai-for-android-simple-way-to-bring-on-device-ai-to-smartphones-with-snapdragon> |
| C5 | VK_KHR_cooperative_matrix is part of Vulkan 1.4; vendor-neutral tensor-matmul abstraction; Vulkan tensor/datagraph (ML) extensions exist and are documented by Arm | 2026 | VP | <https://docs.vulkan.org/tutorial/latest/Advanced_Vulkan_Compute/09_Specialized_Math/01_introduction.html> ; <https://support.arm.com/documentation/101897/0400/Machine-learning> |
| C6 | Google Play: new apps/updates must target API 36 (Android 16) from 2026-08-31 | 2026 | VP | <https://support.google.com/googleplay/android-developer/answer/11926878> |
| C7 | Google Play 16 KB page-size requirement for apps targeting Android 15+: new apps/updates from 2025-11-01; existing-app updates from 2026-05-01 | 2026-11-04 doc | VP | <https://developer.android.com/guide/practices/page-sizes> |
| C8 | Same INT8 model produces different results across Snapdragon generations (Hexagon INT8 accumulation/rounding changes) — community report | 2026 | VS (weak) | <https://www.reddit.com/r/MachineLearning/comments/1r7ruu8/> |

# Research ledger part 2 (compiled 2026-11-14)

## D. Independent benchmarks & critiques (most decision-relevant)

| # | Fact | Date | Tag | Source |
| --- | --- | --- | --- | --- |
| D1 | **MSU Neural Video Codecs Evaluation**: NVCs (MLVC, DCVC-RT, DCVC-UF) run on RTX 4070 Ti, conventional encoders on i7-12700K, MSU FullHD 50+ video set, VOD 5+ FPS use case. DCVC-RT ≈ **10–20% better BSQ-rate than reference x265**; **MLVC needs >30% MORE bitrate** than x265 on PSNR 6:1:1 / SSIM 6:1:1 / VMAF-Y | 2026-08-31 | VS (independent lab) | <https://www.compression.ru/video/codec_comparison/2025/neural.html> |
| D2 | MSU subjective (8 videos, >20,000 pairwise assessments, Bradley–Terry): **DCVC-RT beaten only by leading proprietary implementations; MLVC ≈ VVenC and SVT-AV1**. Neural codecs "substantially underperform leading proprietary implementations of VVC, AV1, HEVC" on most objective metrics | 2026-08-31 | VS | same |
| D3 | MSU conclusion: NVC compression ≈ open-source HEVC/AV1 implementations, NOT clearly better than best proprietary encoders at FullHD VOD | 2026-08-31 | VS | same |
| D4 | Ozer analysis: MLVC is **hybrid CPU+NPU** — C++ entropy coder on CPU must be bit-exact, NN on NPU. Not a fixed-function decoder | 2026-08-02 | VS | <https://streaminglearningcenter.com/articles/microsoft_mlvc.html> |
| D5 | Ozer: MLVC-S = smaller model tuned for 150 FPS @540p. Model sizes **18.3M params (MLVC) + 5.4M params (MLVC-S)**. "MLVC-multi" stitches 360p MLVC (−75.5% MOS, 103/99 fps) + 540p MLVC-S (−65.4% MOS, 121/114 fps) → −71.8% MOS headline; needs 2 resident models | 2026-08-02 | VS | same |
| D6 | Ozer: single-model reality — MLVC @540p = 49/48 fps avg; MLVC-S @1080p = 39.5/34.6 fps Apple, 24.0/25.8 Intel, 26.5/19.6 Qualcomm ⇒ **real-time 1080p only on Apple NPU today** | 2026-08-02 | VS | same |
| D7 | Ozer: MLVC-S @720p ~900 kbps ≈ 3.83 MOS vs HEVC-QSV needing ~2.5–2.8 Mbps for same MOS (≈1/3 bitrate); MLVC-S real-time 720p on all tested NPUs | 2026-08-02 | VS | same |
| D8 | Ozer power estimate: "50% of an NPU plus some percentage of the CPU probably uses about **5× the power** of a dedicated AV1/HEVC decoder in a SoC" — explicitly the author's guess, no measured numbers published | 2026-08-02 | **EST (author's own guess)** | same |
| D9 | MLVC limits: checkpoints trained 360p–720p (VCD + OpenVidHD); **fixed-size NPU graphs**, must re-export/re-quantize per resolution per backend (CoreML/OpenVINO/QNN); **YUV420 8-bit only**; RTC rate control not 2-pass VBR | 2026-08-02 | VS | same |
| D10 | MLVC paper itself: "Power consumption is likely to become the next optimization frontier now that real-time performance has been achieved" | 2026-07 | VP (paper quote) | arXiv 2606.28027v2 |
| D11 | **dav2d**: VideoLAN fast AV2 decoder announced; feature-complete AVM v15 decoder, 8-bit + 10-bit; BSD-style license; NEON/AVX2 work started. **"AV2 decoding is roughly five times more complex than AV1 decoding"** | 2026-05-31 | VP (project lead) | <https://jbkempf.com/blog/2026/dav2d/> |
| D12 | Kempf: AV2 gains "around 25% compared to AV1 commonly seen" — lower than AOM's 30% headline | 2026-05-31 | VS | same |

## E. Legal / IP

| # | Fact | Date | Tag | Source |
| --- | --- | --- | --- | --- |
| E1 | **Microsoft patent application US 2026/0254962 A1** (filed 2025-02-25, published 2026-08-27; inventors Pärnamaa, Indenbom, Lumiste, Loot, Saabas) claims losslessly-encoded entropy distribution parameters + one hyperlatent set serving multiple latent groups = the MLVC scale-sending mechanism. 20 claims; examiner objections noted | 2026-08-27 | VS (patent summary site; primary doc = USPTO) | <https://patentlyze.com/patent/microsoft-cross-platform-neural-video-codec-explained/> |
| E2 | MIT license grants copyright only — **no express patent license, no patent retaliation clause** (Ozer's reading of the MLVC release) | 2026-08-02 | VS (legal opinion) | <https://streaminglearningcenter.com/articles/microsoft_mlvc.html> |
| E3 | Qualcomm **US 11,388,416 B2** granted, AI-codec related, in force | cited 2026-08-02 | VS | <https://patents.google.com/patent/US11388416B2/en> |
| E4 | GreyB "Beyond VVC (H.267)" landscape analyzed **3,916 patents + 1,357 JVET contributions**; commercial licensors dominate ECM + NNVC filings | 2026 | VS | <https://greyb.com/blog/exclusive-video-codec-landscape/> |
| E5 | No patent pool exists for neural video coding as of Aug 2026 ⇒ bilateral licensing | 2026-08-02 | VS | Ozer |
| E6 | AOMedia Patent License 1.0 includes **defensive termination** clause | current | VP | <http://aomedia.org/license/patent-license/> |
| E7 | Sisvel operates VP9 + AV1 pools; claims to have licensed "overwhelming majority of VP9 market and a substantial share of AV1"; ~1,050+ AV1 patents offered | 2026-09-09 | AC (licensor) | <https://www.sisvel.com/insights/the-video-codec-patent-licensing-mistake-and-how-to-avoid-it/> |
| E8 | AOMedia RF policy cannot prevent third-party assertion against AV2 | 2026 | VS | <https://www.iam-media.com/article/how-strategically-navigate-the-video-coding-space-minimise-licensing-and-litigation-expense-best-ip-management-practices-av2> |
| E9 | Access Advance: HEVC Advance + VVC Advance rates held through 2030; MCBA discounts up to 50% for joining both pools by 2026-12-31, effective 2027-01-01 | 2026-11-11 | VP | <https://accessadvance.com/2026-11-11/access-advance-announces-new-effective-discount-rates-for-its-multi-codec-bridging-agreement-starting-on-january-1-2027/> |
| E10 | Via LA HEVC/VVC: first 100,000 units $0.00; units 100,001+ $0.30 each | current | VP | <https://www.via-la.com/licensing-programs/hevc-vvc/> |
| E11 | InterDigital **acquired Deep Render** (AI-native codec startup) | 2025-10/11 | VS + VP (InterDigital post) | <https://www.interdigital.com/post/accelerating-foundational-research-through-ai> ; <https://streaminglearningcenter.com/articles/interdigitals-deep-render-bet-ai-native-codec-or-strategic-ip-hedge.html> |

## F. Metrics / datasets / platform extras

| # | Fact | Date | Tag | Source |
| --- | --- | --- | --- | --- |
| F1 | **VMAF v1** released: adds AIM (additive impairment) component alongside DLM; phone model reworked; **NEG enabled by default as of v1** | 2026-06-19 | VP (Netflix blog) / VS (Wikipedia for NEG-default) | <https://netflixtechblog.com/vmaf-v1-good-is-not-good-enough-60d7e4244ea8> ; <https://en.wikipedia.org/wiki/Video_Multimethod_Assessment_Fusion> |
| F2 | VMAF v0 1080p model assumes 3H viewing distance ≈ 60 px/degree; phone viewing ≈ 4–5H | 2026-06-19 | VP | same |
| F3 | "On Optimizing Image Codecs for VMAF NEG" — VMAF-NEG is still attackable (e.g. unsharpening can raise score while lowering true quality) | 2026-02-24 | VP (preprint) | <https://arxiv.org/html/2602.21336v1> |
| F4 | Netflix AV1 film grain synthesis at scale: **36% average bitrate reduction at ≥1080p** when FGS enabled | 2025-07-02 | AC (Netflix) | <https://netflixtechblog.com/av1-scale-film-grain-synthesis-the-awakening-ee09cfdff40b> |
| F5 | ColorVideoVDP (CVVDP) — full-reference metric modelling luminance+colour, space+time, with explicit display spec; open source | 2024-07 (TOG) | VP | <https://github.com/gfxdisp/colorvideovdp> ; <https://www.cl.cam.ac.uk/research/rainbow/projects/colorvideovdp/> |
| F6 | MSU NVC quality-assessment dataset/benchmark, arXiv 2608.29331 (Safonov, Gornostaev, Dubonos, Vatolin) | 2026-08-29 | VP | <https://arxiv.org/abs/2608.29331> |
| F7 | BVI-AOM: 956 uncompressed sequences, 270p–2160p, for deep video coding training | 2024-08 | VP | <https://arxiv.org/html/2408.03265v3> |
| F8 | AOM Common Test Conditions v2.0 (CWG-B075o) defines subjective+objective evaluation for AOM codecs | 2021 | VP | <https://aomedia.org/docs/CWG-B075o_AV2_CTC_v2.pdf> |
| F9 | Meta VCAT (Video Codec Acid Test) — open Android app benchmarking decode battery drain + system health; beta 4.0 adds VVC decode | 2025-07 / 2026-02 | VS + VP (repo) | <https://github.com/Video-Codec-Acid-Test-VCAT/vcat-d> ; <https://streaminglearningcenter.com/articles/david-ronca-details-vcat-beta-4-0-with-vvc-decode.html> |
| F10 | Meta/Vodafone/YouTube white paper: Moto e13 (Unisoc T606, 2GB RAM, 12nm, A75+A55) 720p30 local playback 16h: VP9 SW ≈2.6%/h battery drain, AV1 fast-decode-2 ≈3.2%/h. Unisoc ships `c2.unisoc.av1.decoder` which is actually dav1d SW | 2025-09 | VP (joint white paper) | <https://engineering.fb.com/wp-content/uploads/2025/09/Meta-AV1-White-Paper-FINAL.pdf> |
| F11 | Same white paper: Android CDD requires AV1 encode+decode on handhelds, HW **or SW**; recommends dav1d; tier levels Low=3.1, Mid=4.0, High=4.1 | 2025-09 | VP | same |
| F12 | Android 17 CDD MPC section exists with explicit AV1 HW decoder / 4K60 HW encoder / AV1 encoder requirement IDs | 2026-06-24 | VP | <https://source.android.com/docs/compatibility/17/mpc> |
| F13 | Perfetto ODPM (on-device power rails monitor): Android 10+, needs dedicated HW; Power Profiler available Pixel 6+ | 2026-03-06 | VP | <https://perfetto.dev/docs/data-sources/battery-counters> ; <https://developer.android.com/studio/profile/power-profiler> |
| F14 | Android 17 introduces **per-app memory limits** based on device total RAM, starting with Pixel; can compress/swap then kill process | 2026-06-02 / 2026-08-19 | VS (Google blog content not retrievable; multiple secondary) | <https://android-developers.googleblog.com/2026/08/app-broader-memory-limits.html> ; <https://www.highcircl.com/en/blog/android-17-per-app-memory-limits> ; <https://www.heise.de/en/news/Android-17-Google-introduces-stricter-memory-limits-for-apps-11420492.html> |
| F15 | Android 15+: `dataSync` and `mediaProcessing` foreground services capped at **6 hours per 24h**, tracked separately | current | VP | <https://developer.android.com/develop/background-work/services/fgs/timeout> |
| F16 | LiteRT NPU vendor list includes Qualcomm, MediaTek NeuroPilot, Intel OpenVINO, **Samsung Exynos AI LiteCore** | 2026-10-12 | VP | <https://developers.google.com/edge/litert/next/npu> |
| F17 | MediaTek LiteRT **Neuron Delegate** built on LiteRT Stable delegate API | 2026-09-07 | VP | <https://genio.mediatek.com/doc/tao/npu_acceleration.html> |
| F18 | LiteRT + NPU: Epic games case, up to 30 FPS on-device acceleration on supported Android | 2026-04-23 | AC (Google) | <https://developers.googleblog.com/building-real-world-on-device-ai-with-litert-and-npu/> |
| F19 | ExecuTorch supports selective backend delegation to Qualcomm Hexagon / Apple ANE (MLSys 2026 paper) | 2026 | VP | <https://proceedings.mlsys.org/paper_files/paper/2026/file/236f915dd02af4f11927f67330b21d4b-Paper-Conference.pdf> |
| F20 | Samsung UFS 5.0 announced: up to 10.8 GB/s seq read, 9.5 GB/s seq write; UFS 4.0 was 4.2 / 2.8 GB/s | 2026-06-23 | VP (Samsung newsroom) | <https://news.samsung.com/global/samsung-unveils-industrys-fastest-ufs-5-0-solution-for-next-gen-on-device-ai-applications> |
| F21 | JPEG AI became an International Standard (JPEG 106th meeting); Part 5 file format still pre-draft in 2026 | 2025-02-19 | VP | <https://jpeg.org/items/20250219_press.html> |
| F22 | Google Play base module compressed download ≤200 MB; asset packs for larger content (install-time / fast-follow / on-demand) | 2026-10-06 | VP | <https://developer.android.com/guide/playcore/asset-delivery> |
| F23 | Google: Rust in Android → ~1000× reduction in memory-safety vulnerability density vs C/C++ | 2025-11-13 | AC (Google) | <https://blog.google/security/rust-in-android-move-fast-fix-things/> |
| F24 | Integer Discrete Flows (Hoogeboom et al., NeurIPS 2019), IDF++ (ICLR 2021), iFlow (numerically invertible flows) — lossless flow lineage | 2019–2021 | VP | <https://proceedings.neurips.cc/paper/9383-integer-discrete-flows-and-lossless-compression> ; <https://iclr.cc/virtual/2021/poster/2779> |
| F25 | JPEG XL lossless JPEG transcoding ≈20% average saving, bit-exact reversible; Brunsli 2.5× faster than Lepton but ~1% worse | 2020–2026 | VP (jpegxl.info) + VS | <https://jpegxl.info/> ; <https://news.ycombinator.com/item?id=22456764> |
| F26 | Lossless neural image codecs LC-FDNet, ArIB-BPS, CALLIC, FNLIC, HPAC "now compete with or surpass JPEG-XL on standard benchmarks" (NeuralLVC's characterization) | 2026-09-08 | AC (paper's literature claim) | <https://arxiv.org/html/2604.03353v2> |
| F27 | NeuralLVC (Uricchio & Bertini, ACM MM 2026, DOI 10.1145/3767308.3835727): masked-diffusion (LLaDA) entropy model + I/P architecture + bijective linear tokenization; P-model adds only 1.3% params; beats H.264/H.265 lossless on 9 Xiph CIF + 24 HD (720p/1080p, Xiph/UVG/BVI-AOM); exact reconstruction verified end-to-end with arithmetic coding | 2026-09-08 | VP (peer-reviewed ACM MM) | <https://arxiv.org/html/2604.03353v2> |
| F28 | APV: Snapdragon 8 Elite Gen 5 first mobile SoC with APV; Samsung Galaxy S26 Ultra APV capture up to 8K30 | 2025-09 / 2026-03 | VS | AndroidAuthority / SamsungMobilePress |
