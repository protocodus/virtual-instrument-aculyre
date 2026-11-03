# What designing the instruments costs, 2026-11-03

`AculyreEngine::setInstrument` designs an instrument: the strings'
dispersion sections, the soundbox's coupled modes, and the tuning of every
string (and every lever and extension key) to the bridge bar. The Performer
designs all three instruments in `prepare()`, so a front end waits for it at
startup (README, Known gaps). This round took the savings that leave every
sample unchanged; the playing path was profiled and left alone.

Tool: `Tools/MeasureDesign.cpp` (`MeasureAculyreDesign`), whose head states
the protocol. Machine: a cloud container, 4 cores of an Intel Xeon at
2.80 GHz, GCC, Release (`-O3`), one thread. Development-machine figures,
never Reason or DAW CPU.

## Instructions per `setInstrument` (callgrind, 48 kHz)

Deterministic, so the figure to compare.

| Instrument | Before (0.7.7) | After | Change |
| --- | --- | --- | --- |
| Lyre Harp (steel) | 4.223 G | 3.612 G | -14.5 % |
| Nylon Lyre Harp | 4.513 G | 3.897 G | -13.7 % |
| Gut Lyre | 4.807 G | 3.433 G | -28.6 % |
| the three | 13.543 G | 10.942 G | -19.2 % |

## `Performer::prepare` at 48 kHz, wall clock

Median of seven calls, the two builds alternated three times on a machine
that moves 15 % between runs: 1.96, 1.67 and 1.68 s before; 1.31, 1.28 and
1.62 s after. The whole demo set (twelve `RenderAculyreDemo` runs, each
designing all three instruments) went from 37 s to 29.5 s.

## What changed

- **Dispersion design.** The sections' poles and the unit-circle point were
  recomputed for every partial of every candidate; they are computed once.
  The worst-partial scan stops once a candidate is already worse than the
  best so far. Each stage count is searched only when the choice needs it:
  the fewest sections within a cent wins, and the largest count is searched
  only for the retry of a poor fit.
- **Bar-bend tuning (`reflectionLead`).** The string line's filter lag is
  computed once per frequency instead of once per bend, and the plate
  mobility sum runs in real and imaginary parts and skips the coordinates a
  mode does not drive: with the bar centred across the top, half of every
  mode's participations are exactly zero.

## It is the same sound

- At the commit that made these savings (before 0.7.8's touch), the twelve
  demo WAVs rendered by the new build are `cmp`-identical to the 0.7.7
  build's.
- A checksum of rendered notes and of each string's design report matches
  the 0.7.7 sources at 44.1, 48, 88.2 and 192 kHz on all three instruments.
- `Aculyre.Engine` and `Aculyre.DemoMidiSources` pass.

## Left alone

The playing path (`process()`) was profiled on the steel demo. The bend
loop's complex products could be rewritten as precomputed pairs for about 2 %
and bit-identically; not done. Nothing else stood out in the profile.
