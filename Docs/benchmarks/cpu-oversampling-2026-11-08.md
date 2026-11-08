# Aculyre CPU and oversampling — 2026-11-08

Local version 0.12.6; canonical DSP and Performer only. The modal bank uses
AVX2 on supporting CPUs/operating systems; the original portable modal path is
retained. Oversampled output filters compute independent left/right channels
in 128-bit vectors, retaining each channel's complete tap order and rounding.
No mode, voice, control update, physical coefficient, sensor gain or output
threshold was removed or retuned.

## Measured complete Performer

AMD EPYC 9V74, native Linux/GCC 14.2, C++17 Release (-O3 -DNDEBUG;
DSP -fno-exceptions), 48 kHz host, 64-frame callbacks. Every preset, dense held
playing, 0.25 s untimed warm-up, 1 s timed render, three alternating A/B rounds.
Setup, export and reporting excluded; builds/tests/renders idle. CPU is callback
thread CPU time; wall median/p99/max are retained separately. The timer adds
small common overhead. Figures are development-machine results, not Reason,
SDK or DAW measurements.

| Setting | Before, % of one core | After | Reduction |
| --- | ---: | ---: | ---: |
| 1× | 8.44 | 7.17 | 15.12% |
| 2× | 17.42 | 14.40 | 17.33% |
| 4× | 34.94 | 28.84 | 17.44% |

The aggregate averages each preset's median CPU factor. All three lyre presets
and both koto presets were evaluated. At a 48 kHz host these are true 1×/2×/4×.
The 192 kHz engine ceiling caps the 4× setting to 2× at a 96 kHz host and to 1×
at 192 kHz; the tests check actual factor and engine rate.

## Correctness and limits

The complete native pipeline matrix covered 92 cases across four instruments:
every preset at every setting, attacks, bends and high-rate caps. All before/after
samples and recorded string observers matched byte for byte; all output was
finite and below full scale. A separate C++17 Clang build with automatic
vectorization disabled matched all 31 selected lyre/koto pipeline cases.

The engine's independent mechanics/circuit tests and C++17 builds pass. The
first koto Clang prototype exposed an alignment mistake in a using-alias; the
qualified code uses attributed typedefs with static alignment checks. Its
one-sample-block reproducer, subsequent exact comparisons and ASan/UBSan probe
pass. No failed tolerance or CPU budget was raised.

AVX2 savings do not establish ARM/native-SDK savings. Some adversarial high-rate
callbacks still exceed their wall deadline in this native harness (also in the
baseline); average CPU reduction is not a host deadline guarantee. The sample
sequence, latency and previously calibrated output levels remain unchanged.

The original physical-model listening fixtures remain frozen. CPU changes
have no new listening-preference claim. Acustra/Aculele CPU trials failed to
show a dependable aggregate benefit and were not selected; their accepted
engines remain unchanged.
