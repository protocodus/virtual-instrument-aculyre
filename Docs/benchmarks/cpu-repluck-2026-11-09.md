# Re-pluck cost and per-sample profile, 2026-11-09

These figures compare the shipping 0.13.0 (`main`, `7a18016`) with the 0.13.1
candidate (`claude/project-thread-l5u0le`). Three changes separate them:

- the cheaper re-pluck contact (`91c8b86`);
- the removed stick-slip micro-transient (`5e7a32b`);
- the causal release unloading (`3df201e`).

The figures come from a development machine: a shared 4-vCPU Intel Xeon
cloud container at 2.10 GHz, GCC 13.3, Release `-O3 -DNDEBUG`. They are not
Reason, DAW or target-hardware CPU. The container's neighbours were not
idle, so a single maximum can move by a few hundred microseconds between
runs. The event-cost change below is far larger than that.

## Event cost

`Tools/MeasureCpu.cpp` (`AculyreCpuProbe --oversampling 1 --scenario all
--seconds 2`) times each case for two seconds through the public
Performer, at 48 kHz with 64-frame callbacks. The budget for one callback
is 1333 µs. The values are calling-thread CPU per callback, in µs. The
baseline ran first, then the candidate.

| Preset, scenario | 0.13.0 median / p99 / max | 0.13.1 median / p99 / max |
| --- | --- | --- |
| steel, all 27 strings re-plucked in one callback | 124 / 3759 / 4474 | 125 / 677 / 898 |
| nylon, all 27 strings re-plucked in one callback | 127 / 3887 / 4397 | 126 / 730 / 958 |
| gut, all 16 strings re-plucked in one callback | 74 / 2498 / 2823 | 75 / 471 / 712 |
| steel, factory single notes and chords | 124 / 177 / 767 | 128 / 200 / 276 |
| steel, all strings held (dense) | 124 / 264 / 502 | 125 / 214 / 367 |
| gut, all strings held (dense) | 78 / 243 / 413 | 74 / 141 / 208 |

Before 0.13.1, each re-plucked string cost about 160 µs. About eight
strings re-plucked in one callback filled it, and all 27 overran it by a
factor of 3.4. Three costs explain it:

- The finger contact evaluated a Catmull-Rom interpolation per sample, for
  each of up to about 30 Bessel-weighted shifts of the period.
- Finding the period copied each plane's 128 KB loop to run it freely.
- Stopping the wave cleared both 64 KB rings of each plane.

The period's samples are whole, so all of a shift's samples share one
interpolation fraction. The contact is therefore one circular convolution
with four taps per shift (`Source/DSP/FingerContact.h`).
`Tests/ContactPhysicsTests.cpp` checks it against the direct sum and the
exp(-hold (1 - cos 2 pi n x)) partial law. Stopping now clears only the
history any of the string's designs can read (`Loop::reach`), and the free
run uses the loop itself, saving and restoring its state. The bounded
clear is byte-identical. The convolution keeps every probe scenario's
output within -142 dB of its peak.

Steady playing is unchanged. The median stays at about 125 µs for 27
strings and 74 µs for 16, which is 9.4% and 5.6% of a 64-frame callback
at 1x. Steel at 2x and 4x costs 261 and 514 µs: oversampling runs the
engine at that multiple of the rate, at about that multiple of the cost.

## Where a sample's time goes

This profile is from Callgrind on the candidate (`-O3 -g`, the probe built
with `-DACULYRE_PROFILE_CALLGRIND=ON`): steel, all strings held, 1x, half
a second. The figures are instruction counts, not time. Each sample costs
18.4 k instructions.

| Share | Work |
| --- | --- |
| 62% | the 54 string loops (Thiran, dispersion, loss sections, delay lines) and the bridge-bar junction, all inlined in `AculyreEngine::process` |
| 28% | the soundbox's 289 modes (AVX2 bank: each mode's drive from 10 bar coordinates, its recursion, and its pull back on the bar) |
| 3.0% | the saddle rotation of each string's two planes |
| 1.7% | the resting hand's contact on the idle strings |
| 1.3% | the piezo circuit |
| 0.9% | the output stage |

Both large shares are per-mode and per-loop arithmetic, already paired
or quadrupled into vectors. No single line exceeds 4.1% of the total.
Two small savings were considered and not taken:

- Skipping the saddle rotation on strings with no lever engaged would save
  under 3%, at the cost of a second code path through the junction.
- Skipping the piezo circuit while its blend is zero would save 1.3%, but
  the circuit's capacitors would then hold stale charge when the blend
  rises.

A pluck costs 234 k instructions per string, which is about 13 samples of
steady playing. The finger contact's convolution takes 134 k of that. The
causal release's unloading adds two branches per string per sample, too
small to measure.
