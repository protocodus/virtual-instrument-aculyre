# CPU optimization and audit passes, 2026-11-14 (0.13.6 and 0.13.7)

Two passes on 2026-11-14 following [the 2026-11-10 fix pass](cpu-fix-pass-2026-11-10.md):

1. **0.13.6 (`599fa5c`):** bit-exact CPU reductions and physical/edge-case
   correctness across the engine, modal bank, string loop filters, room FDN and
   piezo circuit. Every default 72-scenario render in `AculyreCpuProbe` is 100%
   bit-identical (`cmp` zero-byte difference over 6,912,000 `float` samples) in
   both the Rack-like and native builds.
2. **0.13.7:** owner-selected soundbox simplifications ([decisions](../decisions.md))
   combining an `-80 dB` (`< 1e-4`) weak-mode cut on the real-time bridge
   junction and `ModeBank` (`232 / 232 / 216` padded mode slots kept out of
   `289 / 289 / 219` designed modes) with single-precision `Float4` modal shape
   storage (`16,960 B` vs `33,920 B`) and 4-lane bar-force / bar-pull dot
   products around 64-bit `double` modal resonators (plus touch-scaled
   soft-pluck release in `noteOn`, `0.0%` per-sample CPU cost).

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

---

## Pass 2: 0.13.7 owner-selected soundbox simplifications

Following the 2026-11-14 A/B listening sets ([decisions](../decisions.md)),
`0.13.7` integrates the `-80 dB` weak-mode cut (`01`) and the single-precision
`Float4` modal bank (`02`), plus touch-scaled soft-pluck release (`03`, `0.0%`
per-sample CPU cost), on top of `0.13.6` (`599fa5c`).

### Hardware retired instructions: 0.13.6 to 0.13.7 (1.0 s `dense` at 48 kHz, 1x)

Exact retired instructions over 48,000 rendered frames (`dense` scenario,
64-frame blocks after warmup), steel harp / nylon harp / gut lyre, in millions
of instructions:

| Build | 0.13.6 (`599fa5c`) | 0.13.7 | 0.13.6 -> 0.13.7 | Cumulative vs 0.13.5 (`0fb36e6`) |
| --- | --- | --- | --- | --- |
| **Rack-like**, `dense`, 1.0 s | 1003.01 / 1013.62 / 585.16 M | 840.85 / 851.93 / 564.12 M | **-16.17% / -15.95% / -3.59%** | **-24.03% / -23.97% / -16.64%** |
| **Native**, `dense`, 1.0 s | 843.31 / 852.02 / 498.46 M | 743.42 / 752.11 / 490.26 M | **-11.85% / -11.73% / -1.64%** | **-17.79% / -17.91% / -11.70%** |

### Calling-thread CPU: 0.13.6 to 0.13.7 (`CLOCK_THREAD_CPUTIME_ID`, alternating rounds)

`AculyreCpuProbe` at 48 kHz in 64-frame callbacks (`1333.33 µs` deadline),
alternating `A/B, B/A, A/B...` rounds (`5` rounds for `1x dense`, `3` rounds
for `1x stress` and `4x dense`), steel harp / nylon harp / gut lyre:

| Build & scenario | 0.13.6 median callback (µs) | 0.13.7 median callback (µs) | Callback change | 0.13.6 `cpu_percent` | 0.13.7 `cpu_percent` |
| --- | --- | --- | --- | --- | --- |
| **Rack-like**, `1x dense` | 80.06 / 80.96 / 47.00 | 65.83 / 65.71 / 45.29 | **-17.77% / -18.84% / -3.63%** | 6.15 / 6.18 / 3.61% | 5.01 / 5.04 / 3.45% |
| **Rack-like**, `1x stress` | 80.04 / 81.10 / 46.98 | 65.83 / 65.79 / 44.96 | **-17.75% / -18.88% / -4.30%** | 6.16 / 6.22 / 3.64% | 5.07 / 5.05 / 3.48% |
| **Rack-like**, `4x dense` | 325.48 / 332.31 / 196.96 | 271.83 / 272.02 / 192.04 | **-16.48% / -18.14% / -2.50%** | 24.64 / 25.19 / 14.94% | 20.79 / 20.90 / 14.86% |
| **Native**, `1x dense` | 73.71 / 74.88 / 42.12 | 61.60 / 61.67 / 39.25 | **-16.42% / -17.64% / -6.82%** | 5.67 / 5.81 / 3.29% | 4.72 / 4.69 / 2.99% |
| **Native**, `1x stress` | 73.17 / 72.96 / 40.50 | 61.96 / 62.08 / 40.38 | **-15.32% / -14.91% / -0.31%** | 5.61 / 5.68 / 3.24% | 4.75 / 4.78 / 3.10% |
| **Native**, `4x dense` | 301.69 / 305.21 / 179.46 | 255.19 / 261.10 / 173.79 | **-15.41% / -14.45% / -3.16%** | 22.99 / 23.16 / 13.63% | 19.36 / 20.14 / 13.40% |

On the portfolio `stress` phrase (`1x stress`, standard U12 fastest-of-3
`block256_percent.max`), the worst 256-frame block drops from `8.53 / 8.74 /
6.82%` to **`7.22 / 7.37 / 6.59%`** in the Rack-like build (`7.52 / 7.60 /
5.57%` to **`6.56 / 6.74 / 5.44%`** natively; round medians `9.33 / 8.76 /
7.00%` to **`7.47 / 7.81 / 7.30%`** Rack-like and `7.64 / 8.01 / 6.35%` to
**`7.18 / 7.32 / 5.79%`** natively).

### What changed in 0.13.7

1. **80 dB weak soundbox mode cut (`AculyreEngine.cpp`).**
   - `setInstrument` prunes from `immediateMobility` (`y0m`) and `ModeBank`
     (`s.bank`) those soundbox modes whose peak resonant bar mobility
     `(Q / omega) * ||P||^2` or peak resonant radiated pressure per unit bar
     force `Q * ||P|| * max(|rL|, |rR|)` sits more than 80 dB (`< 1e-4`) below
     the strongest mode (`232 / 232 / 216` padded mode slots kept out of `289 /
     289 / 219` designed modes; null `-77.39 dBFS` RMS / `-57.52 dB` relative
     to `0.13.6` on the 9.6 s three-instrument phrase).
   - The full `s.modes` array remains intact for setup string reflection/ear
     tuning (`portLoad` / `tuneByEar`), piezo mount conductance, and
     `modeReport()` / `bridgeMobilityMagnitude()`.
2. **Single-precision `Float4` modal shape table and bar-force / bar-pull
   accumulation (`ModeBank.h`, `Pair.h`, `AculyreEngine.cpp`).**
   - `ModeBank::shape` (`424 x 10` floats = `16,960 B`, down from `33,920 B`)
     and `Group::s1Float` store scaled modal participations and `s1` copies in
     32-bit `float` (`Float4`, `aligned(4), may_alias`, `alignof(ModeBank) == 8`,
     `alignof(State) == 8`), cutting L1D modal bandwidth in half and running
     both the generalized bar-force dot product (`stepPart<0..10>`) and the
     bar-pull dot product (`pullOnBarDirect<1..6>`, `pullOnBar<1..5>`) four
     modes per 128-bit vector.
   - Every mode's recursive 2nd-order resonator `(a1, a2, s1, s2)` and
     Nyquist-zero radiation sum `(left, right)` remain in 64-bit `double` (solo
     null `-111.38 dBFS` RMS / `-91.51 dB` relative to `0.13.6`).
   - Once all strings decay below `idleEnergy` (`-130 dB`), a `2.5 Hz`
     sub-audible bleed relaxes only the Nyquist poles (`z = -1 -> -bleedPole`)
     on the bridge mass/bends and radiation integrator while keeping the bar
     bends' DC spring pole exact at `z = +1`, draining any 24-bit `Float4`
     `(-1)^n` deadband without discharging silent static preload.
