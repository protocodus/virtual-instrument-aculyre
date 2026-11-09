# CPU in the consolidation pass, 2026-11-09 (0.13.1 to 0.13.4)

What the pass changed and what it measured, the build Reason makes first.
Every change here leaves the sound alone: bit for bit, or (the mode bank)
142 dB under the signal.

The figures come from a development machine: a shared 4-vCPU Intel Xeon
cloud container at 2.10 GHz, GCC 13.3 and Clang 18.1. Four other sessions
were building and rendering on it throughout (load average 4 to 11 on four
cores), so the timings are loose and the decisions rest on instruction
counts and cache simulation (Callgrind, 32 KB 8-way L1D, 2 MB last level).
They are not Reason, DAW or target-hardware CPU.

## The two builds

- **Rack-like.** Reason's SDK compiles the engine with its own Clang
  (`-fno-builtin`, no loop or SLP vectorisation, no contraction) to
  baseline x86-64 with SSE3, or to arm64 with NEON. It never has AVX2 or
  x86 FMA, and `CpuFeatures.h`'s x86 branch is not compiled. The stand-in
  here: `clang++ -O2 -std=c++17 -fno-vectorize -fno-slp-vectorize
  -fno-builtin -ffp-contract=off -fno-exceptions -DACULYRE_DISABLE_AVX2`.
  It differs from Reason's in one way that matters: its maths library is
  glibc's, where Reason's own `sqrt`, `floor` and `lround` are far slower.
- **Native.** GCC `-O3`, as the repository's Release build, with the
  run-time dispatch to AVX2 and FMA3. This is what a future plug-in and CI
  would run. Not counted toward the Rack release.

`AculyreCpuProbe` plays each scenario through the Performer at 48 kHz, 1x,
in 64-frame callbacks. **dense** holds notes on every string;
**stress** is the portfolio's phrase (U12): a chord of eight at full
velocity under the sustain pedal, re-struck note by note, a second chord
on top, all sixteen re-struck in turn, and the whole phrase again every
second, so the first chord lands on eight strings still sounding;
**burst** re-plucks all 27 strings in one callback, which no player does.

## Instruction counts

Steel harp / nylon harp / gut lyre, millions of instructions, and
first-level cache read misses.

| | 0.13.1 | 0.13.4 | Change |
| --- | --- | --- | --- |
| Rack-like, dense, 0.3 s | 334.9 / 338.9 / 205.9 | 337.2 / 341.2 / 206.0 | +0.7% / +0.7% / +0.1% |
| its L1 read misses | 20.8 / 20.9 / 12.4 | 17.8 / 17.9 / 11.0 | -14% / -14% / -12% |
| Rack-like, stress, 1 s | 1123.4 / 1136.9 / 696.6 | 1128.9 / 1142.4 / 693.0 | +0.5% / +0.5% / -0.5% |
| its L1 read misses | 69.5 / 69.8 / 41.4 | 59.4 / 59.7 / 36.6 | -15% / -14% / -12% |
| Native, dense, 0.3 s | 265.9 / 270.1 / 165.9 | 223.7 / 227.9 / 141.8 | -15.9% / -15.6% / -14.5% |
| Native, stress, 1 s | 891.4 / 905.4 / 560.1 | 750.1 / 764.1 / 478.4 | -15.9% / -15.6% / -14.6% |

In the Rack-like build the pass is a trade inside the mode bank (1.5% more
instructions for a seventh fewer cache misses, below) and a cut where the
blocks are worst (a note-on, below). The steady instruction count there is
where it was: the bank is already two lanes of double precision, near the
fewest operations its sums allow (a multiply and an add per mode and bar
coordinate, twice: once for the force, once for the pull).

## What each change did

**One shape table for the modes (`097f01c`, 0.13.2).** Each mode's force is
`b0 sum(P F)` and its pull on the bar `sum(P s1)`. The bank kept both
`b0 P` and `P`: with the coefficients and state, 40 KB read every sample on
the harps, more than the first-level cache holds beside the strings.
Scaling each mode's state by `1 / sqrt(b0)` makes both sums read one table,
`sqrt(b0) P`: 28 KB. Largest difference from 0.13.1: 142.3 dB under the
signal (18 probe cases). Rack-like dense: 334.9 to 340.0 M instructions
(steel), L1 read misses 20.8 to 17.9 M, and over seven alternating rounds
the median callback fell to 0.95 / 0.96 / 0.92 of 0.13.1's. Native, with
the fused kernels the same commit adds: 265.9 to 224.3 M, median callback
0.84 / 0.89 / 0.85.

Why an FMA build showed no gain in
[cpu-exact-rewrites](cpu-exact-rewrites-2026-11-09.md): the AVX2 kernel was
compiled `no-fma` to stay bit-exact with the portable one, so no flag could
reach it. Acukoto gained because it wrote separate fused kernels; Aculyre
now has them too, and its native output is no longer bit-exact with the
portable build's (about 160 dB under the signal).

**The re-pluck's contact convolution (`2f0cb10`, `30a1ea4`).** A re-pluck
convolves one period of each plane with a kernel of up to 129 taps. Tap by
tap in scalars, a vectorising compiler made lanes of it and Reason's did
not. Eight taps a pass in the two-lane `Pair` type, each output adding its
taps in the old order: the same bits. Rack-like, over the stress phrase's
38 re-plucks: the contact 11.03 M to 3.94 M instructions (290 k to 104 k a
re-pluck), a whole note-on 363 k to 212 k on average. (The two commits'
messages divide by 24 and 30 re-plucks; 38 is right, and the totals they
give are right.)

**No library call while playing (`61889b1`).** With `-fno-builtin`,
`std::sqrt`, `std::floor` and `std::lround` are calls into Reason's library
(its square root is a software routine). The engine made 20 k, 324 k and
81 k of them a second on a harp: each string's saddle rotation every
callback, and the hand's four tap ages and the round trip's length every
control period. They are in-line operations now (`DspMath.h`, after
Respira's). The same bits. In the stand-in build, whose library is the
cheap one, 1136.3 to 1129.8 M instructions over the stress second; what it
saves in Reason was not measured and should be larger.

**Subnormals (`56d5cf1`, 0.13.3).** A fully palm-muted chord's callbacks
cost 86 µs against 87 before the fix on this CPU: no speed-up here. The fix
is a safeguard for a host that leaves flush-to-zero off on a CPU that
charges for subnormals (see the audit).

## Worst blocks

The ceiling is 10% of one core for any 256-frame block (5.333 ms). Each
block's fastest of five alternating rounds, pinned to one core, 0.13.1
against 0.13.4, steel / nylon / gut:

| | 0.13.1 | 0.13.4 |
| --- | --- | --- |
| Rack-like, stress, worst 256-frame block | 11.3 / 11.6 / 9.2% | 11.2 / 11.3 / 8.7% |
| its worst callback | 326 / 325 / 304 µs | 263 / 255 / 260 µs |
| Rack-like, burst, worst 256-frame block | 21.7 / 22.3 / 13.5% | 17.5 / 17.0 / 10.4% |
| its worst callback | 811 / 823 / 525 µs | 601 / 587 / 344 µs |
| Native, stress, worst 256-frame block | 10.7 / 11.0 / 8.8% | 9.8 / 9.7 / 6.8% |
| its worst callback | 284 / 306 / 300 µs | 227 / 236 / 208 µs |
| Native, burst, worst 256-frame block | 20.3 / 19.1 / 12.0% | 16.5 / 17.3 / 10.4% |

So on this machine, under load, the harps' stress phrase is still above
the ceiling in the Rack-like build (11.2 and 11.3%) and just under it
natively. The steady state leaves little room: the harps' mean callback
through the stress phrase was 117 and 124 µs in these runs (8.8 and 9.3%;
97 and 106 µs, 7.3 and 7.9%, taking each callback's fastest round), and
eight strings re-plucked in one callback add about 160 µs to one of a
block's four. These need repeating on a quiet machine:

```sh
python3 pair.py 7 4 base=<0.13.1 probe> now=<0.13.4 probe> -- --scenario stress
```

(`pair.py` is the pass's paired-timing script; it runs
`AculyreCpuProbe --blocks-csv` pinned and alternating and takes each
block's fastest round.)

## Tried and not kept, or measured and not done

- **A single-precision mode bank** (Acukoto's table). Rounding only the
  table leaves 131 dB; running the sums in single precision, four lanes to
  an instruction, leaves 81 to 96 dB, which is no longer a neutral change.
  In the Rack-like build it took 6.4% fewer instructions than 0.13.1 and
  ran at 0.94 / 0.95 / 1.00 of its median callback, no better than the
  double-precision single table (0.95 / 0.96 / 0.92). Not shipped, and no
  listening set made for a change that buys nothing here. On a CPU with a
  larger first-level cache the balance may differ.
- **Leaving out modes nobody can hear.** Of the three instruments' 797
  modes, 6 have a peak mobility at the bar 120 dB under the strongest
  mode's and 2 a radiated level that low; 31 and 13 are 100 dB under, 153
  and 157 are 80 dB under. Nothing to gain within a neutral change; a cut
  at 80 dB would save a fifth of the bank and would be an audible-grade
  change needing a listening set.
- **Skipping the control period's work when nothing moved.** Every
  sounding and every sympathetically ringing string's tension glide
  changes its delay each period; only a string at exact rest would skip,
  and then the engine is nearly asleep.
- **A cheaper fresh pluck.** `releasedPluck` already takes one `expm1` per
  boundary, not per cell (11 k instructions a plane).

## What would bring the worst block down further

Not done, because each changes numbers by more than a neutral change may,
or is a new mechanism:

- the 80 dB mode cut above (about a tenth of the steady cost);
- single-precision force and pull sums (6% of the instructions in the
  Rack-like build, perhaps more time on arm64, where four lanes of float
  are one NEON instruction);
- a re-pluck contact with fewer taps (the Bessel series is cut at 1e-9;
  at 1e-7 it has an eighth fewer terms and its error would be 145 dB
  under the wave).
