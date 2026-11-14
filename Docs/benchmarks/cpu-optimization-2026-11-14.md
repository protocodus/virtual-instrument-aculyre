# CPU optimization and audit pass, 2026-11-14 (0.13.6)

A pass over the engine, modal bank, string loop filters, room FDN and piezo
circuit for bit-exact CPU reductions and physical/edge-case correctness,
following [the 2026-11-10 fix pass](cpu-fix-pass-2026-11-10.md). Every default
72-scenario render in `AculyreCpuProbe` (three instruments, four oversampling
settings `off/1x/2x/4x`, six scenarios `idle/single/dense/repluck/stress/burst`)
is 100% bit-identical (`cmp` zero-byte difference over 6,912,000 `float`
samples) in both the Rack-like and native builds.

All figures below come from a shared 10-core Apple Silicon macOS development
machine (Apple Clang 17) while three sibling instrument builds and test suites
ran concurrently. Per repository rules, these are development-machine
measurements (`proc_pid_rusage` hardware retired instructions and alternating
`CLOCK_THREAD_CPUTIME_ID` calling-thread callback times), **not** Reason, DAW
or target-hardware CPU.

## Builds

- **Rack-like** (`clang++ -O2 -std=c++17 -fno-vectorize -fno-slp-vectorize
  -fno-builtin -ffp-contract=off -fno-exceptions -DACULYRE_DISABLE_AVX2`):
  models the Reason Rack Extension `phdsp64` compilation flags (no auto- or SLP
  vectorization, no compiler math builtins, no FP contraction, 8-byte heap
  alignment ceiling).
- **Native** (`clang++ -O3 -DNDEBUG -std=c++17`): the repository's Release
  build on ARM64 NEON.

## Hardware retired instructions (1.0 s `dense` at 48 kHz, 1x)

Exact retired instructions over 48,000 rendered frames (`dense` scenario,
64-frame blocks after warmup), steel harp / nylon harp / gut lyre, in millions
of instructions:

| Build | 0.13.5 (`0fb36e6`) | 0.13.6 | Change |
| --- | --- | --- | --- |
| **Rack-like**, `dense`, 1.0 s | 1106.80 / 1120.45 / 676.75 M | 1004.58 / 1015.70 / 587.67 M | **-9.24% / -9.35% / -13.16%** |
| **Native**, `dense`, 1.0 s | 904.28 / 916.17 / 555.24 M | 841.49 / 850.64 / 498.12 M | **-6.94% / -7.15% / -10.29%** |

## Calling-thread CPU (`CLOCK_THREAD_CPUTIME_ID`, alternating rounds)

`AculyreCpuProbe` at 48 kHz in 64-frame callbacks (`1333.33 µs` deadline),
alternating `A/B, B/A, A/B...` rounds (`5` rounds for `1x dense`, `3` rounds
for `1x stress` and `4x dense`). Figures report the median across rounds of
`thread_cpu.median_us` (and overall calling-thread `cpu_percent`), steel harp /
nylon harp / gut lyre:

| Build & scenario | 0.13.5 median callback (µs) | 0.13.6 median callback (µs) | Callback change | 0.13.5 `cpu_percent` | 0.13.6 `cpu_percent` |
| --- | --- | --- | --- | --- | --- |
| **Rack-like**, `1x dense` | 87.29 / 88.46 / 55.29 | 80.19 / 81.21 / 46.92 | **-8.14% / -8.20% / -15.15%** | 6.70 / 6.87 / 4.25% | 6.13 / 6.23 / 3.65% |
| **Rack-like**, `1x stress` | 87.08 / 87.96 / 55.17 | 79.92 / 81.08 / 47.00 | **-8.23% / -7.82% / -14.80%** | 6.75 / 6.80 / 4.31% | 6.15 / 6.27 / 3.68% |
| **Rack-like**, `4x dense` | 355.67 / 361.25 / 231.54 | 324.83 / 330.79 / 197.08 | **-8.67% / -8.43% / -14.88%** | 27.05 / 27.61 / 17.64% | 24.99 / 25.58 / 15.11% |
| **Native**, `1x dense` | 77.83 / 79.12 / 46.42 | 74.04 / 75.08 / 41.88 | **-4.87% / -5.11% / -9.78%** | 5.98 / 6.08 / 3.58% | 5.70 / 5.80 / 3.23% |
| **Native**, `1x stress` | 77.96 / 79.38 / 46.54 | 74.25 / 75.25 / 41.96 | **-4.76% / -5.20% / -9.85%** | 6.08 / 6.22 / 3.66% | 5.80 / 5.89 / 3.38% |
| **Native**, `4x dense` | 326.04 / 329.62 / 198.62 | 307.71 / 313.08 / 180.08 | **-5.62% / -5.02% / -9.34%** | 25.10 / 25.43 / 15.38% | 23.59 / 23.91 / 13.82% |

## What changed (all 100% bit-exact)

1. **`ModeBank` group packing, specialized `stepPart<0..5>`, and direct
   symmetric bar pull (`ModeBank.h`).**
   - Each 8-mode group's `(a1, a2, s1, s2, left, right)` arrays are packed into
     `ModeBank::Group` (384 bytes, well inside ARM64's 1008-byte `ldp/stp q`
     signed immediate offset range) so one group pointer addresses all six
     fields without extra induction variables.
   - `stepPart<0..5>` hoists each parity part's active generalized-force splats
     and shape-table pointers into registers across all groups of eight modes.
   - For symmetric soundboxes (`oddEnd == count`, true of all three instruments),
     `pullOnBarDirect<1..5>` accumulates each parity's distinct coordinates in
     vector registers and reduces straight into `pull`, eliminating the 640-byte
     `lanePull` stack buffer and its per-sample zeroing; mixed-parity banks keep
     `runModesMixedPull` (`noinline`) so its stack frame never touches the hot
     symmetric path.
2. **4-lane (`2 x Pair`) unrolling across `LoopFilters`, `Room`, and output
   smoothing (`AculyreEngine.cpp`).**
   - `LoopFilters::Losses::process`, `LoopFilters::read` (Thiran 2nd-order,
     1st-order dispersion, and 2nd-order dispersion stages), `Room::process`,
     and `s.output` parameter smoothing step two `Pair`s (4 `double` lanes) per
     iteration so `-fno-vectorize` emits paired 128-bit `ldp/stp q` loads and
     stores and overlaps biquad dependency chains.
   - `noteOn` soundbox modal preload uses `Pair` vector loads/stores across
     `ModeBank::Group`.
   - `AculyrePerformer::downsample` unrolls its symmetric Kaiser-sinc inner tap
     pairs (`#pragma unroll 4`).
3. **Transposed bridge matrices, precomputed loop pointers/couplings, and
   staggered ring-buffer strides (`AculyreEngine.cpp`).**
   - `State` precomputes transposed `immediateMobilityColumns`,
     `stringImpedanceColumns`, `solveColumns`, `loops`, `coupling`, `port`, and
     `saddleRotation` once per instrument design instead of rebuilding ~5 KB of
     stack tables on every audio block; `coupling` and `port` copy into compact
     16-byte stack buffers per block for paired loads.
   - Delay lines (`loopLines`), pending excitation buffers (`loopPending`),
     mean-square rings (`stringSquares`), and pitch tables (`stringPitch`) live
     outside `StringVoice` so all 27 `StringVoice` instances (`~2.4 KB` each)
     sit contiguously at offset `0` of `State`.
   - Because `delayCapacity` is `16,384` floats (`65,536 = 2^16` bytes) and all
     54 waveguides advance `write` in lockstep, contiguous `64 KB` rows alias
     onto a single L1D/L2 cache set (`81` accesses to one cache set per sample
     on the 27-string harps). Padding `ringStride = delayCapacity + 32` (`+128`
     bytes, one cache line) staggers every waveguide and square ring across
     distinct cache sets, cutting `1x dense` median callback time on the
     27-string harps by ~18 µs relative to unpadded `64 KB` rows.
4. **Control-rate Thiran caching, `PiezoCircuit` quiet fast path, and `-fno-builtin`
   intrinsics (`AculyreEngine.cpp`, `PiezoCircuit.h`, `DspMath.h`, `FingerContact.h`, `StringContact.h`).**
   - `Loop::setLineDelay` and `LoopFilters::setLineDelay` skip divisions and
     scatter/gather updates when `d == thiranD` and `thiranOrder == nextOrder`.
   - `PiezoCircuit::processSample` bypasses the nonlinear BJT/diode solver once
     its states have decayed to zero and bridge force is zero (`piezoQuiet_`).
   - `DspMath.h` adds `floorValue` (`__builtin_elementwise_floor` / `__builtin_floor`)
     and `isFinite` (`__builtin_isfinite`), eliminating the remaining `-fno-builtin`
     C library calls in `FingerContact.h`, `PiezoCircuit.h`, and `ModeBank.h`.
   - `StringContact::configure` skips tap-geometry setup when `rate == 0.0`,
     computing geometry lazily only inside `contactReport()`.
