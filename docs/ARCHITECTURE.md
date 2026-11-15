# Architecture

This document describes the code as it exists in this repository. Where a
component is a prototype or a heuristic, that is stated explicitly.

## Layers

```
┌──────────────────────────────────────────────────────────────┐
│ Android UI                                                   │
│  android_app/  Java single-activity app (packaged via script)│
│  android/      Kotlin Compose preview (not connected)        │
└──────────────────────────────┬───────────────────────────────┘
                               │ JNI (core/src/prism_jni.cpp)
┌──────────────────────────────▼───────────────────────────────┐
│ prism_core (C++20 static library)                            │
│  rans8, bbr, psc, cont+pvloss, isobmff, remux, vfs_proxy,    │
│  transaction, dedup_rabin, router, detint, coolchic, afgs1   │
└──────────────────────────────┬───────────────────────────────┘
                               │
┌──────────────────────────────▼───────────────────────────────┐
│ Host OS: filesystem, page size, optional MediaCodec/NDK      │
└──────────────────────────────────────────────────────────────┘
```

The CLI (`tools/prism_cli.cpp`) links against `prism_core` directly and is the
most reliable way to exercise the native code.

## Native modules

| Module | Source | Behavior |
|---|---|---|
| `detint` | `detint.cpp` | Integer dot products and a 512-entry sigmoid CDF lookup table. Used by the probability model. |
| `rans8` | `rans8.cpp` | 8-lane rANS with 16-bit renormalization. Encoder validates contiguous symbol statistics that sum to 32,768. Decoder validates stream lengths, symbol indexes, and renormalization bounds. |
| `bbr` | `bbr.cpp` | Splits each input byte into two nibbles, builds symbol statistics from a small integer model, rANS-codes them, and stores the raw bytes instead when coding would expand the payload. |
| `psc` | `psc.cpp` | Reversible YCoCg-R + 2D LeGall 5/3 wavelet + MED predictor for 8-bit RGB frames, plus 4-plane Bayer CFA split/merge with temporal delta. Requires even dimensions of at least 2×2. |
| `container_pvloss` | `container_pvloss.cpp` | `.pvloss` writer/reader: 72-byte header, CRC32C-framed chunks, and a 28-byte-per-entry seek index. The reader validates the index offset, entry count, and chunk ranges against the file size. |
| `isobmff` | `isobmff.cpp` | Dual-track MP4 mux/demux for the project's own box layout and metadata ledger. |
| `remux` | `remux.cpp` | Writes a minimal `ftyp`/`moov`/`mdat` file and parses it back. Box and sample ranges are bounded against the input buffer; malformed or trailing data returns `false`. |
| `vfs_proxy` | `vfs_proxy.cpp` | Mounts a `.pvloss` file and serves reads from cached chunks. |
| `transaction` | `transaction.cpp` | Stages a replacement, verifies it, then swaps it into place with a rollback journal. |
| `dedup_rabin` | `dedup_rabin.cpp` | Content-defined chunking with a Rabin rolling hash and a hash-addressed block vault. Restore verifies each block's length and SHA-256 before writing. |
| `router` | `router.cpp` | Heuristic media probe based on header magic (`ftyp`, `hvc1`, `av01`, TIFF/DNG) and a routing decision. The savings ratios are fixed defaults, not measurements. |
| `coolchic` | `coolchic.cpp` | Prototype integer residual MLP plus heuristic Pareto target selection. The `predicted_vmaf` and energy fields are estimates. |
| `afgs1` | `afgs1.cpp` | Parametric grain parameter container and synthesis helper. |
| `tile_pool` | `tile_pool.cpp` | Fixed-size tile ring buffer used for memory-bounded processing. |

## Lossless paths

### `.pvloss` + BBR

1. The CLI reads the input in 256 KiB chunks.
2. `BbrEngine::compress` produces a coded payload or a raw bypass payload.
3. `PvlossWriter` records each chunk with its CRC32C and virtual offset.
4. `PvlossWriter::close` writes the seek index and finalizes the header.
5. `prism decompress` reads each chunk, checks CRC32C, decompresses it, and
   compares the result with the header's SHA-256.

### PSC image/Bayer

The PSC path is a library API. It is not exposed as a CLI file compressor.
Decode verifies the reconstructed SHA-256 and throws on mismatch. Odd
dimensions and invalid bit depths are rejected before any buffer access.

### Remux

`remux_to_clean_isobmff` preserves packet payload bytes and metadata boxes.
`parse_and_verify_isobmff` is intentionally strict: invalid box sizes, sample
lengths that exceed the `mdat` box, and trailing bytes all return `false`.

## Android application

`android_app/` is a Java application compiled with `javac`, dexed with D8, and
packaged/signed by `scripts/build_apk.ps1`. Its packages include `engine`,
`worker`, `storage`, `vfs`, `provider`, `thermal`, `history`, `scheduler`,
`notification`, `ai`, and `ui`. The UI is a single activity with custom views
and dialogs.

The app's runtime behavior on a device is not verified in this repository.
Features described in UI text (for example hardware transcode throughput,
VMAF values, or deduplication savings) must be treated as unimplemented or
unmeasured until reproduced.

`android/` is a Kotlin/Compose preview module. It declares an Android
application and native build, but there is no Gradle wrapper in the repository,
and its button actions only show a Toast.

## Build and platform notes

- CMake sets `-Wall -Wextra -Wpedantic` for non-MSVC builds.
- The JNI target is built when `JNI_FOUND` is set or when targeting Android.
- Android 16 KiB page-size linker flags are added for Android and Linux.
- `sample_video/` and media files are ignored by Git; the repository does not
  ship test videos.
