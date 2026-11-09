# Exact CPU rewrites that did not pay, 2026-11-09

The target is under 10% of a 64-frame callback at 1x (133 µs at 48 kHz),
judged on the worst blocks. This round tried only rewrites that leave every
sample unchanged. None was kept. The code is in Git at `8f457af`.

The figures come from a development machine: a shared 4-vCPU Intel Xeon
cloud container at 2.10 GHz, GCC 13.3, Release `-O3 -DNDEBUG`. They are not
Reason, DAW or target-hardware CPU.

## Where the time goes

The steady cost of steel and nylon is about 130 µs per callback (9.5-10%);
gut is about 75 µs (5.6%). Each probe below removes one block from a
scratch copy of the engine. The copy is never shipped, and its sound is
not a candidate. Steel, all strings held, 1x, median calling-thread CPU per
callback:

| Removed | Median |
| --- | ---: |
| nothing | 132 µs |
| the soundbox's 289 modes | 68 µs |
| the string loops' filters (Thiran, dispersion, losses) | 111 µs |
| the resting hand's contacts | 135 µs |
| denormals (FTZ/DAZ set) | no change |
| nothing, whole program built `-mavx2` or `-mavx2 -mfma` | no change |

The mode bank is half the time but only 28% of the instructions (Callgrind).
A Cachegrind run with a 48 KB, 12-way L1D finds about 1,400 L1 read misses
per sample in the mode bank and 1,700 in the rest of `process`. The bank
alone is about 40 KB of double-precision coefficients and state, read every
sample. With the strings' state and delay-line taps it does not fit the
first-level cache, so each sample streams it again from L2.

## The rewrites

- The string filters' passes ran four loops at a time on AVX2, without
  FMA, selected in `prepare()` as the mode bank already is.
- `runModesAvx` took each group's pull on the bar in the same pass as its
  update, while the new state was still in registers. It also formed
  `drive` as b0 times the participation, which is the product
  `setInstrument` stores, instead of reading `drive`.

The output is byte-identical to `0b6909a` on all three presets, at 1x, 2x
and 4x, in all seven probe scenarios (12,096,000 bytes). Instructions fall
12% (265.9 M to 233.4 M for 0.3 s of dense steel). The mode bank's
simulated L1 read misses fall 25%.

## Timing

`AculyreCpuProbe --oversampling 1 --scenario all --seconds 2`, pinned to
one core. Five rounds alternate baseline, both rewrites, and the bank
rewrite alone. Each cell is the median of the five rounds.

| Preset, scenario | Mean CPU, base / both / bank only | p99 µs, base / both / bank only |
| --- | --- | --- |
| steel, dense | 9.53 / 9.96 / 10.11 % | 169 / 186 / 192 |
| steel, rapid re-plucks | 9.87 / 9.93 / 10.48 % | 163 / 183 / 203 |
| steel, all strings re-plucked at once | 10.44 / 10.06 / 10.54 % | 706 / 689 / 720 |
| nylon, dense | 9.54 / 9.86 / 10.17 % | 158 / 180 / 178 |
| gut, dense | 5.72 / 5.59 / 5.71 % | 107 / 119 / 116 |

Every difference is within the ±3% that the same binary moves from round to
round. Fewer instructions and fewer simulated misses did not shorten the
callback. Without hardware counters (this container has no `perf`), what
bounds it is not identified. Arithmetic width, FMA and denormals are ruled
out.

## What is left

- No exact rewrite here brings steel or nylon under 10%. Their mean is
  already at about 10%, and the p99 block is 160-190 µs (12-14%).
- An all-string re-pluck in one callback still costs 700 µs at p99 and up
  to 1 ms (53-75%). That is the worst block by far, and it is the
  plucks' setup ([re-pluck cost](cpu-repluck-2026-11-09.md)), not steady
  playing.
- Shrinking the mode bank's working set would need a change to the
  numbers: single-precision mode gains (participation, radiation), or fewer
  modes. Either changes the sound. It needs a level-matched A/B, and
  shipping it is the owner's decision.
