# Design invariants and test coverage

This document lists properties the native code is designed to preserve and the
tests that exercise them. It is a map of the test suite, not a formal proof.

All results below come from running the test executable on one development
machine (Windows, MSVC). Cross-architecture, device, and performance claims are
out of scope and are not asserted by the suite. See
[VERIFICATION.md](VERIFICATION.md) for the commands used.

## 1. Deterministic integer arithmetic (`detint`)

`core/src/detint.cpp` uses 16-bit signed activations, 8-bit signed weights, and
32-bit accumulation with a maximum of 512 products.

- The largest possible dot product is
  `512 * 127 * 32767 = 2,130,641,408`, which is below `INT32_MAX`. The test
  computes this value exactly for all-max and all-min inputs.
- `dyadic_rescale` rounds with a power-of-two shift and clamps to the 16-bit
  activation range. The test checks a normal case, upper clamp, and lower clamp.
- `CDF_SIGMOID_LUT` has 512 entries; the test checks that it starts at 0, ends
  at 32768, and that the lookup is non-decreasing for the sampled inputs.
- `allocate_page_aligned` returns an address aligned to at least 16 KiB.

The suite does not compare results across CPU architectures. Bit-identical
cross-architecture output is a design intent, not a tested property here.

## 2. rANS entropy coding (`rans8`)

`core/src/rans8.cpp` is an 8-lane rANS coder with a 15-bit normalization mass
(`RANS_M = 32768`) and 16-bit renormalization.

Properties checked by tests:

- Symbol frequencies must be non-zero and must describe contiguous, ordered
  intervals. A zero-frequency symbol is rejected with an exception.
- A stream encoded from valid statistics decodes to the original symbols
  exactly. The checked cases are a 16-symbol non-uniform distribution over 4096
  symbols and a single-symbol alphabet with full frequency.
- Serialization and deserialization preserve the symbol count.
- A stream truncated by two bytes is rejected instead of reading past the
  buffer.
- The full-frequency single-symbol case encodes and decodes without stalling or
  overflowing the renormalization bound.

The suite does not measure achieved bitrate, so no entropy-bound statement is
made.

## 3. BBR lossless path (`bbr`)

`core/src/bbr.cpp` splits bytes into nibbles, derives symbol statistics from a
small integer model, and runs rANS coding. When coding would expand the payload,
the raw bytes are stored instead.

Properties checked by tests:

- An 8192-byte synthetic pattern round-trips byte-for-byte, and the SHA-256 of
  the restored buffer equals the original digest.
- The micro-MLP produces a valid CDF: every frequency is non-zero and the
  frequencies sum to 32768. Two runs with the same input produce identical
  symbol intervals.
- The `HuffmanPos` bucket function maps the DC position to 0, the bottom-right
  position to 15, and an interior position to a value in between.

## 4. Image and Bayer paths (`psc`)

`core/src/psc.cpp` implements a reversible YCoCg-R color transform, a 2D LeGall
5/3 integer wavelet, MED prediction, and rANS coding for 8-bit RGB frames. Bayer
CFA frames are split into four planes and can be coded intra or as a temporal
delta against a reference frame.

Properties checked by tests:

- YCoCg-R round-trips every sampled RGB triple in steps of 17 per channel.
- The 1D and 2D LeGall 5/3 transforms reconstruct their input exactly for the
  tested lengths and sizes.
- A 64x64 RGB frame compresses and decompresses with exact sample parity.
- 10-bit Bayer splice/merge preserves every sample; intra and delta frames both
  reconstruct exactly.
- Odd width or height is rejected before buffer access.
- A 2x2 frame round-trips, which covers the smallest supported geometry.
- A compressed frame whose geometry is altered is rejected on decode.
- A Bayer bit depth of 0 is rejected.

Only even dimensions of at least 2x2 are supported. 8-bit RGB and 10-bit Bayer
are the tested depth/format combinations.

## 5. `.pvloss` container (`container_pvloss`)

The writer emits a fixed header, CRC32C-framed chunks, and a seek index. The
reader bounds the index and chunk ranges against the actual file size.

Properties checked by tests:

- Two chunks can be written, reopened, located by virtual offset, and read back
  with their payload bytes intact and their CRC32C checked.
- A file that is too short to hold a header is rejected.
- A header whose index offset lies past the end of the file is rejected.
- A chunk truncated at the end of the file makes `open` fail.
- Reading a chunk through the index returns exactly the bytes that were
  written.

## 6. ISOBMFF remux (`remux`)

`remux_to_clean_isobmff` writes a minimal `ftyp`/`moov`/`mdat` layout and keeps
packet payloads unchanged. `parse_and_verify_isobmff` is deliberately strict.

Properties checked by tests:

- Video and audio packet payloads survive a write/parse cycle, and the payload
  SHA-256 matches.
- Colour metadata, HDR metadata, pixel aspect ratio, and rotation are preserved
  through the project's own mux/demux path.
- A stream truncated by one byte is rejected.
- A stream with one trailing byte is rejected.
- A null or empty input is rejected.
- The remux threshold predicate selects remux below 3% estimated savings and
  BBR above it.

This covers the project's own box layout, not arbitrary real-world MP4 files.

## 7. Deduplication and restore (`dedup_rabin`)

The content-defined chunker writes hash-addressed blocks to a vault. Restore
recomputes each block's length and SHA-256 before writing.

Properties checked by tests:

- A 1 MiB patterned stream is chunked and restored byte-for-byte.
- The manifest contains at least one chunk and reports newly written bytes.
- Restore reports failure if a block is missing or fails verification.

## 8. Transactional replacement (`transaction`)

`VerifiedTransaction` stages output in a sandbox, runs a verification callback,
and only then publishes it and moves the original into a trash directory.

Properties checked by tests:

- On success the receipt reports `GRACE`, the published file exists, and the
  original is in the trash directory.
- On verification failure the receipt reports `FAILED`, the original is still
  present and unchanged, and the temporary file is removed.

The test covers the process-level sequence. It does not simulate a power loss or
process kill during the swap.

## 9. Virtual file proxy (`vfs_proxy`)

`PrismVfsProxy` serves random-access reads from a mounted `.pvloss` file using a
chunk cache.

Properties checked by tests:

- A read that starts before a chunk boundary and ends after it returns the
  correct bytes.
- A repeated read is served from the cache and increments the cache-hit count.
- The reported virtual size matches the original size.

## 10. Grain synthesis (`afgs1`)

`Afgs1Engine` extracts a grain estimate, produces a denoised frame, synthesizes
grain from parameters, and serializes those parameters.

Properties checked by tests:

- The parameter set reports that grain should be applied.
- Synthesized output has the same dimensions as the input.
- Serialized parameters deserialize with the same grain scale and random seed.

The test does not assert filter stability, spectral radius, or perceptual
quality.

## 11. Residual model and Pareto solver (`coolchic`)

`CoolchicResidualModel` is a hand-initialized integer MLP. `ParetoDialSolver`
selects heuristic settings; its VMAF and energy fields are estimates.

Properties checked by tests:

- Synthesized residuals stay within the 8-bit range.
- The blend function returns the base value at full weight and local average
  plus residual at zero weight.
- The solver returns settings inside its own configured bounds for a
  battery-weighted and a quality-weighted target, and enables the residual path
  only in the quality case.
- The preflight probe returns a predicted VMAF in `(0, 100]` for exact, wide,
  tiny, and null frame inputs.

The asserted quality and energy values are properties of the heuristic model,
not measurements of encoded video.

## 12. Ingest routing (`router`)

`IngestRouter` inspects header magic and file size, then selects a lossless
contract or a lossy mode.

Properties checked by tests:

- An `ftyp`/`avc1` header is classified as H.264 and routed to BBR.
- An `ftyp`/`av01` header with estimated savings below 3% is routed to remux.
- A TIFF/RAW header is classified as Bayer and routed to the PSC Bayer path.
- A lossy request on H.264 selects the hybrid mode.

Savings ratios used by the router are fixed heuristic defaults, not measured
values.

## Not covered

- Cross-architecture or cross-compiler bit-identity.
- Entropy-coding efficiency versus Shannon entropy.
- Any VMAF, BD-rate, PSNR, throughput, memory, or energy measurement on real
  video.
- Android runtime behavior, MediaCodec, foreground-service timeouts, and the
  Java/Kotlin UI.
- Thread safety, fuzzing, and sanitizer runs.
- Arbitrary third-party MP4 layouts, fragmented MP4, and edit lists.
