# Contributing to Prism-V

## License and source policy

Prism-V is licensed under Apache-2.0. Contributions must be original work or
derived from permissively licensed sources (for example ISO/IEC 14496-12,
ITU-T H.264/H.265, or AOM specifications). Do not copy GPL or LGPL code,
including FFmpeg, libav, x264, or x265.

Keep numeric paths integer-only where the existing code already does so
(`detint.hpp`). Avoid introducing floating-point behavior into entropy coding,
quantization, or rate-control code unless the change includes tests that pin
the expected output.

## Prerequisites

- C++20 compiler: Clang 16+, GCC 13+, or MSVC 2022+
- CMake 3.20 or newer
- Android NDK r26b or newer for Android native builds
- JDK 17+ for the Java app
- For the APK script: `android.jar`, `apktool.jar`, `r8lib.jar`, and
  `uber-apk-signer.jar`

## Build and test the native core

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

`ctest` runs three targets: the 39-case unit test executable, the synthetic
BBR experiment, and the synthetic lossy experiment. Run `build/prism_tests`
directly to see per-test results.

## Build the Android APK

The packaged Java app is built without Gradle:

```powershell
$env:JAVA_HOME = "<jdk>"
$env:PRISM_ANDROID_JAR = "<android.jar>"
$env:PRISM_APKTOOL_JAR = "<apktool.jar>"
$env:PRISM_R8_JAR = "<r8lib.jar>"
$env:PRISM_SIGNER_JAR = "<uber-apk-signer.jar>"
.\scripts\build_apk.ps1
```

The script writes a debug-signed APK to `android_app/signed/`. The Kotlin
module in `android/` has no Gradle wrapper in this repository; building it
requires an Android SDK and a locally installed Gradle.

## Coding conventions

- C++: C++20, standard containers, fixed-width integer types, no raw owning
  pointers. Keep allocations out of per-frame loops.
- Java/Kotlin: Material 3 widgets and existing app patterns. Background work
  should use the foreground-service and worker classes already in the project.
- Comments should explain a non-obvious constraint or invariant. Do not add
  decorative banners, emoji, or restatements of the code.
- Commit messages use the Conventional Commits form, for example
  `fix(core): reject truncated pvloss index` or
  `docs(readme): correct APK build path`.

## Before submitting a pull request

- [ ] Native build succeeds and `ctest` passes.
- [ ] New behavior has a test, especially for bounds checks and malformed input.
- [ ] No GPL/LGPL code or dependencies are introduced.
- [ ] Documentation matches the code; do not add unmeasured performance or
      quality claims.
- [ ] The Java app still compiles with `javac --release 17` against the Android
      platform jar.
