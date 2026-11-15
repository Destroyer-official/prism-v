<div align="center">

# Prism-V

**Lossless media compression experiments, a small C++20 engine, and Android UI prototypes.**

<img src="docs/assets/logo.svg" width="96" alt="Prism-V logo" />

</div>

---

## What this repository contains

Prism-V is an experimental video/media toolchain. It is not a replacement for
FFmpeg or a hardware video encoder. The repository has four parts:

| Part | Location | What it is |
|---|---|---|
| Native core + CLI | `core/`, `tools/` | C++20 library and `prism` command-line tool for lossless bitstream/container operations and image coding experiments. |
| Standalone Android app | `android_app/` | Java single-activity Android application built without Gradle (javac + D8 + apktool). This is the packaged app. |
| Compose UI shell | `android/` | Kotlin/Jetpack Compose UI preview. It has no Gradle wrapper and its compression actions are not connected to the native core. |
| Rust workspace | `rust/` | Small independent Rust crate used for parser experiments. |

### Implemented and tested

- **rANS entropy coder** (`core/src/rans8.cpp`): 8-lane 16-bit rANS with
  symbol-statistics validation and bounds checks.
- **BBR bitstream recompression** (`core/src/bbr.cpp`): nibble symbolization,
  a small integer probability model, rANS coding, and an uncompressed fallback
  when the coded payload would be larger than the input.
- **`.pvloss` container** (`core/src/container_pvloss.cpp`): fixed header,
  CRC32C-checked chunks, and a seek index. Reader rejects out-of-range indexes
  and truncated payloads.
- **ISOBMFF remux** (`core/src/remux.cpp`): packet-preserving MP4 repack with
  bounded box parsing and SHA-256 payload verification.
- **PSC image path** (`core/src/psc.cpp`): reversible YCoCg-R color transform,
  LeGall 5/3 integer wavelet, MED prediction, and rANS coding for 8-bit RGB or
  Bayer CFA frames with even dimensions.
- **Virtual file proxy** (`core/src/vfs_proxy.cpp`), **content-defined
  chunking / vault** (`core/src/dedup_rabin.cpp`), **atomic file replacement**
  (`core/src/transaction.cpp`), and **ingest routing** (`core/src/router.cpp`).
- **CLI** (`tools/prism_cli.cpp`): `compress`, `decompress`, `vfs-mount`,
  `dedup`, `restore`, `route`, `remux`, `audit`.

The C++ test suite currently contains **39 tests** and passes locally on
Windows with CMake/MSVC. The CLI round-trip was checked on a small binary
input. See [docs/VERIFICATION.md](docs/VERIFICATION.md) for the exact commands
and results.

### Prototype / heuristic code

The following are design experiments, not trained production models:

- `core/src/coolchic.cpp` uses hand-initialized integer weights and heuristic
  VMAF/energy estimates. `predicted_vmaf` is an estimate, not a measurement.
- `tools/exp_1a_bbr.cpp` and `tools/exp_1b_lossy.cpp` operate on synthetic
  data. Their output describes those synthetic inputs only; it is not a codec
  benchmark.
- The Android UI includes preview values and controls. Runtime behavior on a
  device has not been verified in this repository snapshot.

---

## Build the native core and tests

Requirements: CMake 3.20+, a C++20 compiler (MSVC, GCC, or Clang).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

Targets produced:

- `prism_core` — static library
- `prism` — command-line tool
- `prism_tests` — unit test executable
- `exp_1a_bbr`, `exp_1b_lossy`, `verify_real_video` — small utilities

### CLI examples

```bash
# Losslessly recompress a file into a .pvloss container
prism compress input.mp4 output.pvloss --contract bbr

# Restore it and verify SHA-256
prism decompress output.pvloss restored.mp4

# Check local build invariants (integer bound, page size, tile budget, CDF LUT)
prism audit

# Repack a supported MP4 box layout
prism remux input.mp4 repacked.mp4

# Content-defined chunking into a vault directory
prism dedup input.mp4 vault/ manifest.pvref
prism restore manifest.pvref vault/ restored.mp4
```

Only the `bbr` file-compression contract is wired in the CLI. `psc`, lossy,
and hybrid contracts are available as library code but not as CLI file
compressors.

---

## Build the standalone Android APK

The script is `scripts/build_apk.ps1`. It does not require Gradle. Set these
environment variables (or place the jars in `tools/vendor/`):

| Variable | File |
|---|---|
| `JAVA_HOME` | JDK 17+ with `bin/javac.exe` and `bin/java.exe` |
| `PRISM_ANDROID_JAR` | Android platform `android.jar` |
| `PRISM_APKTOOL_JAR` | `apktool.jar` |
| `PRISM_R8_JAR` | `r8lib.jar` (provides D8) |
| `PRISM_SIGNER_JAR` | `uber-apk-signer.jar` |

```powershell
$env:PRISM_ANDROID_JAR = "C:\path\to\android.jar"
$env:PRISM_APKTOOL_JAR = "C:\path\to\apktool.jar"
$env:PRISM_R8_JAR = "C:\path\to\r8lib.jar"
$env:PRISM_SIGNER_JAR = "C:\path\to\uber-apk-signer.jar"
.\scripts\build_apk.ps1
```

The signed APK is written to `android_app/signed/`. It is signed with the
debug keystore only; replace the signer configuration before distribution.

The Kotlin module in `android/` needs an Android SDK and a Gradle wrapper,
neither of which is committed. It cannot be built with the commands above.

---

## Repository layout

```
core/include/prism/    public C++ headers
core/src/              C++ implementation and JNI bridge
tests/                 C++ unit tests
tools/                 CLI and developer utilities
android_app/           standalone Java Android app + apktool root
android/               Kotlin/Compose UI module (no Gradle wrapper)
rust/                  Rust workspace
docs/                  architecture, invariants, verification notes
scripts/               build tooling
artifacts/             captured screenshots, UI dumps, logs
```

---

## Limitations

- No benchmark results are committed. The performance, size-reduction, VMAF,
  synchronization, and energy numbers that appeared in earlier revisions of
  this repository were not reproducible and have been removed.
- The native core is lossless for the paths covered by tests. The image/Bayer
  paths require even dimensions; odd widths/heights are rejected.
- `prism compress` can produce a larger file than the input for small inputs
  because of container and index overhead. The CLI reports the real ratio,
  which may be negative.
- MP4 support is limited to the box layout produced by `remux_to_clean_isobmff`.
  More complex real-world MP4s (64-bit box sizes, fragmented MP4, edit lists)
  are not fully handled.
- The Android applications are prototypes. Device runtime behavior, hardware
  MediaCodec encoding, foreground-service timeouts, and UI claims have not
  been verified in this working tree.
- The `.pvloss` header stores only the fields listed in
  `core/include/prism/container_pvloss.hpp`; it does not carry a full codec
  configuration record.

---

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Design invariants and what the tests cover](docs/MATHEMATICAL_INVARIANTS.md)
- [Verification notes and reproducibility](docs/VERIFICATION.md)
- [Point-in-time gap audit](docs/GAP_AUDIT_REPORT.md)

## License

Apache License 2.0. See [LICENSE](LICENSE).
