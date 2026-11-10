# CPU after the fix pass, 2026-11-10 (0.13.5)

A second look for CPU that can be taken without an audible change, after
[the consolidation pass](cpu-consolidation-2026-11-09.md). It found
little: one small bit-exact change shipped in 0.13.5, two ideas were
measured and not shipped, and the steady state is confirmed where that
pass left it.

The machine is the same shared 4-vCPU cloud container (GCC 13.3, Clang
18.1, Callgrind 3.22), with five other sessions building and rendering on
it throughout (load average 8 to 29 on four cores). Nothing here was
accepted or rejected on time: every figure is an instruction count, and
no timing was taken that is worth printing. They are not Reason, DAW or
target-hardware CPU.

The two builds are that document's: **Rack-like** (`clang++ -O2
-std=c++17 -fno-vectorize -fno-slp-vectorize -fno-builtin
-ffp-contract=off -fno-exceptions -DACULYRE_DISABLE_AVX2`), the stand-in
for Reason's, and **native** (GCC `-O3` with the AVX2 and FMA3 dispatch).
`AculyreCpuProbe` plays each scenario through the Performer at 48 kHz, 1x,
in 64-frame callbacks.

## The worst block, counted

The stress phrase's dearest 256-frame block is the one where a chord of
eight is re-struck under the pedal in one callback. The probe can now
count that block alone:

```sh
valgrind --tool=callgrind --instr-atstart=no \
  AculyreCpuProbe --preset 0 --oversampling 1 --scenario stress \
  --seconds 1.1 --callgrind-blocks 748:4
```

(`--callgrind-blocks first:count` counts `count` callbacks from callback
`first`; 748:4 is that block. Build the probe with
`-DACULYRE_USE_CALLGRIND`.)

In the Rack-like build at 0.13.4 the steel harp's block is 7.95 million
instructions: 2.72 M the soundbox's modes, 3.27 M the rest of the sample
loop (the strings, the bridge, the room and the output) and 1.95 M the
eight re-plucks (0.24 M each). A re-pluck is the contact convolution
(0.85 M for the eight), the two planes' free period before it (about
0.40 M) and the new plucks and their preload.

## Instruction counts

Steel harp / nylon harp / gut lyre, millions of instructions.

| | 0.13.4 | 0.13.5 | Change |
| --- | --- | --- | --- |
| Rack-like, dense, 0.3 s | 337.2 / 341.2 / 206.0 | 336.1 / 340.1 / 205.4 | -0.32% / -0.31% / -0.30% |
| Rack-like, stress, 1 s | 1128.9 / 1142.4 / 693.0 | 1125.2 / 1138.7 / 690.8 | -0.33% / -0.32% / -0.31% |
| Rack-like, stress, worst 256-frame block | 7.950 / 8.056 / 5.604 | 7.929 / 8.035 / 5.591 | -0.26% / -0.26% / -0.23% |
| Native, dense, 0.3 s | 223.7 / 227.9 / 141.8 | 222.6 / 226.8 / 141.1 | -0.49% / -0.50% / -0.49% |
| Native, stress, 1 s | 750.1 / 764.1 / 478.4 | 746.4 / 760.4 / 476.2 | -0.50% / -0.49% / -0.46% |
| Native, stress, worst 256-frame block | 5.802 / 5.908 / 4.338 | 5.780 / 5.886 / 4.325 | -0.38% / -0.37% / -0.30% |

The 0.13.4 dense and stress counts are the 2026-11-09 document's (the
steel harp's were taken again and agree). The two fixes of 0.13.5 count
the same as 0.13.4 (7.950 M for the steel harp's worst block in the
Rack-like build); the change is the one below.

The ceiling (10% of a core for any 256-frame block) was measured on
2026-11-09 at 11.2 / 11.3 / 8.7% in the Rack-like build under load. A
third of a per cent fewer instructions (half a per cent natively) does not
move the harps under it; the cost stays as the README states it.

## What shipped

**The hand's contact taps are computed only under a hand** (bit-exact).
Every control period (16 samples) each of the 54 string loops recomputes
where a damping hand would touch it, since the tension glide moves its
period. With no hand on the string (`share` 0) nothing reads the four
taps and their weights: 24 of the call's 67 instructions, 162,000 calls a
second. Every sample is the same: 30 renders (five scenarios of 2.5 s,
three instruments, both builds) are byte-identical with and without it.
The contact's report (its ages and its period) is still kept on every
loop.

## Measured and not shipped

- **The re-pluck's contact series cut at 1e-7** in place of 1e-9. A
  re-plucking finger's contact is a sum of shifts of the string's period
  weighted by e^-a I_k(a); cut two decades earlier it is 14 shifts for 16
  at the finger's longest hold and 9 for 11 under a plectrum (113 taps for
  129, 73 for 89), and the shifts left out sum to under 2.5e-7 of the wave
  at any hold. In the Rack-like build the eight re-plucks of the worst
  block took 75,000 fewer instructions (7.950 M to 7.875 M, 0.94%). The
  output differed by 133.8 dB under the signal or less in single
  precision (five scenarios, three instruments, both builds; largest
  residual sample -133.6 dBFS), single least significant bits in the
  listening set's 24-bit files. Not shipped: `Aculyre.ContactPhysics`
  holds the convolution to its definition within 1e-8 of the wave, and
  the cut needs that guard loosened to 4e-7. A test's tolerance is not
  loosened for one per cent of one block.
- **A re-pluck's two planes run their free period together through the
  block's own filters**, two lanes to an instruction, in place of one plane
  after the other in scalars. Bit-exact, and slower: the block's filters
  are laid out for 54 loops (sorted by stage count, gathered and
  scattered), and for two loops that costs more than it saves. The steel
  harp's worst block rose from 7.950 to 8.323 M (+4.7%) in the Rack-like
  build. The steady count rose too (dense +0.18%, stress +0.24%), because
  with a second caller the compiler stopped inlining the filters into the
  block's loop.

## What is left

- A free run written for exactly two loops, in two lanes, could at best
  halve its 0.40 M: 2.5% of the worst block, nothing off the mean. It
  needs its own bit-exactness test against the scalar loop and was not
  started.
- The contact convolution itself (0.85 M) is already eight taps a pass
  and two outputs at a time.
- The steady state is the mode bank and the string loops, both already
  two lanes of double precision at close to the fewest operations their
  sums allow. The remaining steps there are the ones the 2026-11-09
  document lists (an 80 dB mode cut, single-precision sums); each is an
  audible-grade change that needs a listening decision, and none was
  taken or rendered again here.
