# Verification notes

This file records what was actually built and run in this working tree, with
the commands used. It is not a benchmark report. Numbers that were not measured
are not listed.

**Date:** 2026-11-15
**Host:** Windows x64
**HEAD:** `7c8b219` plus uncommitted working-tree changes

## Toolchain

| Tool | Version |
|---|---|
| CMake | 3.31.6 (MSVC build) |
| Compiler | MSVC 19.44.35229 (x64) |
| Cargo | 1.99.1 |
| JDK | 25 (sources compiled with `--release 17`) |
| Apktool | 2.9.3 |
| D8 / uber-apk-signer | from `r8lib.jar` and `uber-apk-signer.jar` |

## Native build and tests

Configure and build:

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Result: all native targets built with no reported errors.

Run CTest:

```powershell
ctest --test-dir build --output-on-failure -C Release
```

Result:

```text
1/3 Test #1: AllPrismTests .................... Passed
2/3 Test #2: Exp1A_BBR ........................ Passed
3/3 Test #3: Exp1B_Lossy ...................... Passed

100% tests passed, 0 tests failed out of 3
```

Run the unit-test executable directly:

```powershell
build\prism_tests.exe
```

Result:

```text
Total Tests: 39 | Passed: 39 | Failed: 0
```

The 39 tests cover the properties listed in
[MATHEMATICAL_INVARIANTS.md](MATHEMATICAL_INVARIANTS.md). `exp_1a_bbr` and
`exp_1b_lossy` run on synthetic input; they are smoke tests, not codec
benchmarks.

## CLI checks

Audit:

```powershell
build\prism.exe audit
```

Result: all four checks passed (max dot product below `INT32_MAX`, 16 KiB page
size, tile-ring budget, monotonic CDF lookup). Exit code 0.

Round-trip on a 256-byte binary file:

```powershell
build\prism.exe compress input.bin out.pvloss --contract bbr
build\prism.exe decompress out.pvloss restored.bin
```

Result: compress and decompress exited 0; the printed SHA-256 values matched,
and `Get-FileHash -Algorithm SHA256` returned the same hash for both files.
The container was 368 bytes for a 256-byte input (-43.75% savings), which is
expected for small inputs because of header and index overhead.

Error paths:

- `compress ... --contract psc` prints an unsupported-contract error and exits
  1.
- An unknown command prints usage and exits 1.

## Rust workspace

```powershell
cd rust
cargo test --workspace
```

Result (exit code 0):

```text
test result: ok. 2 passed; 0 failed; 0 ignored
```

The two tests are `test_detint_overflow_free` and `test_ycocgr_roundtrip`.

## Standalone Android APK

The APK is built without Gradle by `scripts/build_apk.ps1`. The build resolves
`android.jar`, `apktool.jar`, `r8lib.jar`, and `uber-apk-signer.jar` from
environment variables or `tools/vendor/`.

Command used (paths abbreviated):

```powershell
$env:JAVA_HOME = "<jdk>"
$env:PRISM_ANDROID_JAR = "<android.jar>"
$env:PRISM_APKTOOL_JAR = "<apktool.jar>"
$env:PRISM_R8_JAR = "<r8lib.jar>"
$env:PRISM_SIGNER_JAR = "<uber-apk-signer.jar>"
.\scripts\build_apk.ps1
```

Result: javac, D8, apktool packaging, zipalign, and signing all completed. The
signer verified the APK with v2 and v3 signatures. Output:

```text
android_app/signed/unsigned-aligned-debugSigned.apk
```

The APK is signed with a debug keystore located next to the signer jar. It is
not suitable for distribution as-is.

## Device-side self-checks (Android APK)

The APK includes `com.prismv.test.TestRunnerReceiver`, triggered by

```text
adb shell am broadcast -a com.prismv.ACTION_RUN_TESTS
```

It runs six on-device checks (BBR round-trip, DetInt determinism, thermal gear
table bound, GOP payload round-trip, VFS proxy seek, verified replacement) and
writes `prism_test_results.json` to the app's files dir. These are local
behavior checks only. They do not measure video quality, bitrate, throughput,
or energy, and they were not run in this environment because no device or
emulator was attached. The thermal check validates the declared estimate table
against a 0.050 J/frame ceiling; it is not a power measurement.

A separate standalone Java harness (rANS and BBR round-trip plus truncation
checks) was run from `%TEMP%\prism_java_test` against the hardened
`Rans8Codec` and `BbrCodec` and passed; it is not committed to the repository.

## Not verified

These items could not be checked in this environment:

- **Kotlin/Compose module (`android/`)**: no Gradle wrapper is committed and no
  Gradle installation or Android SDK is available on this host. The module was
  not compiled.
- **Android runtime**: no device or emulator was attached. MediaCodec encoding,
  foreground-service timeouts, storage permissions, and UI behavior were not
  exercised. The Java app compiles and packages, but runtime behavior is
  unknown.
- **Cross-architecture determinism**: tests ran only on Windows x64 with MSVC.
- **Performance, quality, and energy**: no VMAF, PSNR, bitrate, throughput, or
  power measurement on real video is included. The `coolchic` VMAF and energy
  values are heuristic model outputs.
- **Fuzzing and sanitizers**: no fuzz run or AddressSanitizer/UBSan run is
  recorded here.
- **Real-world MP4 coverage**: remux tests use the project's own box layout.
  Third-party MP4 files, fragmented MP4, and edit lists were not tested.
