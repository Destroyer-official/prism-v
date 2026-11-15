# Next-Generation Android Video Compression (Lossless + Lossy), 2027–2030

## A Complete Implementation Plan

**Version:** 1.0 · **Date:** 2026-11-14 · **Status:** Planning document (no application code, no APK)
**Working project name:** *Prism* (placeholder; rename freely)
**Author:** Research synthesis from ~580 live web searches (12 parallel research agents + direct verification), November 2026.

> This document is written so that an engineering team that has never seen the originating conversation can start work the next day. Every major decision lists the real options, the choice, the reason, and the evidence that would change our mind. Every factual claim is tagged and dated; every technology is labeled by maturity. Numbers are given as ranges with their baseline, test set, and hardware. Where we could not verify something, it says **UNVERIFIED**.

---

## How to read this document

### Claim tags (applied inline)

| Tag | Meaning |
| --- | --- |
| **[VP]** | Verified against a **primary** source (standard, official repo/doc, paper, vendor spec). |
| **[VS]** | Verified against a **secondary** source (reputable press, independent benchmark, community measurement). |
| **[AC]** | **Author/vendor claim** — published by the technology's author or vendor, not independently reproduced. |
| **[EST]** | **Estimate** — our reasoned range, not a measurement. Baseline stated. |
| **[SPEC]** | **Speculation** — forward projection, explicitly uncertain. |
| **[UNV]** | **Unverified** — we could not confirm; do not build on it without checking. |

### Maturity labels (applied to every technology)

`[SHIPPING]` in products · `[STD]` standardized · `[OSS]` open-source implementation · `[PROTO]` research prototype with code · `[PAPER]` paper only · `[THEORY]` theory only.

### Source citations

Inline `[S#]` refers to the numbered source table in **Appendix A** (URL + date + what it supports). Facts are dated either by publication date or "accessed 2026-11-14".

### A note on honesty

Three framing corrections up front, because they change the whole design:

1. **"Lossless, as small as possible" is bounded by information theory for already-compressed video.** You cannot meaningfully shrink a well-encoded H.264/HEVC/AV1 bitstream without changing the pixels. The best *lossless* answer for a compressed input is to **keep its bitstream** (remux, trim, strip metadata) — typically 0–3% savings [EST]. Re-encoding decoded frames "losslessly" makes files **5–20× larger** [EST], not smaller. We therefore split "lossless" into three honest sub-modes (§3, D1).
2. **The best quality-per-bit on a phone in 2027 is a hybrid, not an end-to-end neural codec.** Hardware HEVC/AV1 plus small ML for analysis/preprocessing beats any published mobile neural codec on efficiency-per-watt today (§2.3, §2.7). Neural codecs are a *gated future option*, not the backbone.
3. **Battery and heat, not peak speed, are the real constraints.** A phone sustains ~3–5 W; flagships hold only ~53–58% of peak in 20-minute stress tests [VS S94]. Heavy encoding must run **while charging, idle, and cool**, and be resumable. This is a first-class architectural requirement, not an afterthought.

---

# 1. Executive Summary (one page)

**What we are building.** An Android app + open-source core, *Prism*, with two user-selectable modes:

- **Lossless** — bit-exact, verified by cryptographic hash of reconstructed frames. Three sub-modes: **L0 Preserve** (keep the original bitstream; remux/trim/metadata-diet; the default for already-compressed files), **L1 Archive** (true lossless re-encode of RAW/uncompressed/YUV sources to FFV1/JPEG XL), and **L2 Near-lossless** (opt-in, clearly labeled *not* bit-exact, bounded ±1–2 LSB error). A fourth capture path uses **APV** (Android 16's visually-lossless pro codec) where hardware exists — labeled "visually lossless," never "lossless."
- **Lossy** — best quality per bit, with the user picking the target: **Quality**, **Size**, **Speed**, or **Battery**. A hardware-first hybrid engine: MediaCodec HEVC Main10 / AV1 for realtime, SVT-AV1 software for offline quality, per-shot content-adaptive decisions, film-grain synthesis, optional NPU denoising and quality-proxy rate control. AV2 is integrated when a production encoder and mobile hardware exist (gated, ~2028).

**What the two modes share:** ingest/probe, a content analyzer, a charging/idle/thermal-aware scheduler, a container writer, a verification + metrics harness, and the UI. **No machine learning sits in any decoder that must be bit-exact.** ML is encode-side only in v1.

**Expected results (with uncertainty).** Versus a blind hardware-HEVC re-encode baseline on a diverse 60-clip set:

- Lossy quality mode: **20–40% smaller at matched perceptual quality** [EST], driven by AV1 (+~25–30% vs HEVC [VS S31,S32]), per-shot adaptation (+~10–22% [AC S162]), and grain synthesis (+~30–36% on grainy content [AC S153]). These gains *stack imperfectly*; treat 20–40% as the realistic combined range, not a sum.
- Lossless L0: **0–3%** on compressed inputs [EST]; L1 Archive: **~2–3× smaller than uncompressed** YUV/RAW [VS S7,EST]; L2: **~30–50% smaller than L1** [EST from S16].
- Energy: hardware 1080p30 encode **~0.002–0.006 Wh per content-minute** [EST]; software AV1 **~0.1–0.5 Wh/min** [EST] — a ~50–100× gap that dictates the scheduler. **No public phone-encode energy measurements exist; we must generate them (Experiment E2).**

**Top decisions (detail in §3).** (D1) Three honest lossless sub-modes. (D2) Hybrid hardware-first lossy, ML encode-side only. (D4) Kotlin/Compose + C++20 core + Rust container/parser + Python research. (D5) CPU NEON/SVE2 baseline, Vulkan compute for FFV1/SR, NPU via LiteRT vendor accelerators, hardware codecs via MediaCodec. (D7) MP4/ISOBMFF for lossy, Matroska for archival, JSON sidecar manifest. (D10) Permissive-codec core (AV1/AV2/FFV1/libjxl/dav1d/SVT-AV1), MediaCodec-only HEVC, no GPL encoders in the shipped app, patent-risk budget. (D9) 12–14 people, 24 months to 1.0, **$3.5–6.5M** [EST].

**First 30 days.** Run the 10 P0 experiments (§7): measure FFV1/JXL/SVT-AV1-lossless on 6 phones; measure MediaCodec HEVC/AV1 quality **and energy** (ODPM + Otii); sweep SVT-AV1 presets; prototype the analyzer, the bitstream-diet, the NPU denoiser, the quality proxy, grain synthesis, the scheduler, and the conformance harness. **Go/no-go gate G0** (§4.6) decides whether the core numbers hold before we commit to the full build.

**The single biggest risk:** we cannot verify on-phone energy or FFV1-on-ARM speed from public sources — both are load-bearing. P0 exists to measure them in week 1–4. If hardware AV1 encode stays Pixel-only and software AV1 is too slow/hot on mid-range, the lossy mode degrades gracefully to hardware HEVC + grain + per-shot (still a strong product), and the neural/AV2 ambitions slip rather than break the plan.

---

# 2. Research Findings — The Landscape

This section maps what ships, what is standardized, what is open source, what is a research prototype, what is paper-only, and what is theory — and what each would cost on a phone. It is the evidence base for every decision in §3.

## 2.1 Lossless and near-lossless video compression

### 2.1.1 The information-theoretic reality (read this first)

A compressed video bitstream is already near its entropy limit. "Lossless recompression" of it can only recover **redundancy the original encoder left** (container overhead, metadata, suboptimal entropy coding). The only quantified academic precedent is **Wang, Hou, Hu & Xiao, "A Lossless Recompression Approach for Video Streaming Transmission," IEEE Access 7:35162–35172, 2019-03** [VP S20]: it re-codes H.264/HEVC NAL-unit bytes via Fibonacci-position mapping + symbol aggregation + run-length compaction, with a deterministic inverse. Conditions: JM 19.0 / HM 16.6, QP 22/27/32/37, NAL ≤ 1000 B, i7-6700HQ, Foreman 300 frames. It reports **average data reduction (ADR) of 67–318 per frame for H.264** (less for H.265), at **+7.32% (H.264) / +2.86% (H.265) encode time**. **It reports no percentage savings** — the % is UNVERIFIED [S20]. The image-domain analogue is **Dropbox Lepton** (2016): **22% average lossless JPEG savings at 15 MB/s**, bit-exact, deployed at scale [VP S21]; JPEG XL's lossless JPEG transcoding gives **~20%** [VP S12].

**Implication for the plan:** L0 "Preserve" is the honest default for compressed inputs. A "deep diet" (entropy re-coding à la Wang) is an *optional experiment* with realistic single-digit-% upside [EST], gated on Experiment E5. We will not promise large lossless gains on already-compressed video — anyone who does is selling near-lossless or lossy.

### 2.1.2 FFV1 — the archival workhorse [STD][OSS][SHIPPING]

- **Standard:** FFV1 v0/1/3 = **IETF RFC 9043 (2021-08)**, *Informational*, lossless **intra-frame**, range coder, slices, per-slice CRC fixity, up to 16-bit, 4:4:4/RGB [VP S1]. Adopted by the **Austrian Mediathek** (since 2010) and the **Rockefeller Archive Center**; Library of Congress maintains a format description [VS S169,S170].
- **v4 status (corrected):** The v4 IETF draft (`draft-ietf-cellar-ffv1-v4`, rev 22) **expired 2024-07-20** and is intra-only [VP]. The v4 *spec* (`ffv1.md`) adds **float samples (IEEE754 16/32/64-bit), remap tables, cleanup — NOT inter-frame** [VP S3]. The v4 *goals* doc lists **BAYER/RAW support**, better error resilience, arbitrary color spaces, worst-case-size limiting; "previous frames with motion estimation" and "neural-network compression" are **ideas only, proposals welcome** [VP S2].
- **What actually shipped:** FFmpeg **8.0 "Huffman" (2025-08-22)** added **Vulkan compute FFV1** encode/decode [VP S4,S6]; **8.1 (2026-03-16)** added Vulkan ProRes; **9.0 (2026-08-04)** ships **Vulkan Bayer FFV1** (merged 2026-05/06, sponsored by Sovereign Tech Fund) [VP S4]. An **experimental inter-frame mode exists only in the Vulkan encoder** (commit `898b8aa`, 2026-07-12): it references the previous frame's slice state — **no motion estimation/compensation** [VP S8]. The software encoder remains intra-only.
- **Ratios (content-dependent):** doom9 "Lossless Encoding Comparison 2024" [VS S7]: on **SD Digital Betacam 10-bit** (v210 source 332 Mb/s), FFV1 → **37 Mb/s**, UtVideo 54, x264-lossless 64, x265-lossless 72, HuffYUV 99 (FFV1 best, slowest). On **Alexa natural footage**, x264-lossless **445 Mb/s** *beat* FFV1 **462** (x265 454, AV1 447). Archival rule of thumb: **~30% of uncompressed size** [VS S170]. **No modern 1080p/4K natural-video lossless benchmark with named test sets exists beyond this thread** [UNV].
- **Speed:** i9-11900K: FFV1 **62.5 fps**, UtVideo **832 fps**, MagicYUV **1434 fps** (different clips) [VS S7]. **No published FFV1 encode/decode benchmarks on any phone SoC, ARM SBC, or Raspberry Pi exist** [UNV] — a critical gap (Experiment E1). Vulkan FFV1 has known defects (trac #11352: Intel/NVIDIA throughput divergence; #11613: CRC mismatches, Vulkan emits v4.3 vs SW v3.4) [VP S5]. **No mobile-GPU (Adreno/Mali) validation exists** [UNV].
- **Phone cost [EST]:** CPU-only FFV1 1080p30 encode likely **0.3–1× realtime on a 2024 mid-range**, 1–3× on a flagship (single-core ~60 fps desktop → ARM NEON + slices + threads). Vulkan path could be 3–10× faster *if* it ports cleanly — unproven. Memory: slice-based, **<200 MB** for 1080p [EST].

### 2.1.3 Other lossless codecs [OSS][SHIPPING]

| Codec | License | Speed/ratio (doom9 2024, SD Betacam) | Notes |
| --- | --- | --- | --- |
| **Ut Video** | GPL-2+ | 832 fps, 54 Mb/s | Best speed/ratio tradeoff; intra-only [VS S7] |
| **MagicYUV** | proprietary/commercial | 1434 fps, 103 Mb/s | Fastest; 10-bit encode failed in FFmpeg [VS S7] |
| **HuffYUV** | GPL | fast, 99 Mb/s | Weak ratio [VS S7] |
| **Lagarith** | GPL | slower decode, better ratio than HuffYUV | Desktop only [VS] |

None have Android hardware paths; all CPU. **Ut Video's GPL** makes it awkward for a closed app; FFV1 (LGPL via FFmpeg) is cleaner.

### 2.1.4 Lossless modes of lossy codecs [SHIPPING]

- **x264 `--lossless`** requires **High 4:4:4 Predictive** (`-qp 0`); High 10 does **not** support lossless [VP]. Sometimes beats FFV1 on natural content [VS S7]. **GPL** — cannot ship in closed app without commercial license.
- **x265 `--lossless`** (HEVC RExt: transform-skip + BDPCM) [VP]; very slow (medium preset took **2 days on i7-5930K** for the Betacam clip) [VS S7]. **GPL**.
