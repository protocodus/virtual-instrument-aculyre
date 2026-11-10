# Consolidation audit, 2026-11-09 (0.13.1 to 0.13.4)

An end-to-end reading of the engine and its tests before a release, with
the fixes it led to. The three instruments, their dimensions, materials and
voicing are unchanged: the soundboxes are still assumed geometries and the
nylon and gut losses still estimates. Nothing here was chosen by ear, and
nothing in this pass moves the sound by more than 142 dB under the signal
(the three subnormal sites aside, which now give exact zeros below 300 dB
under a pluck), so the earlier comparisons with recordings stand and were
not repeated.

CPU evidence is in
[cpu-consolidation-2026-11-09.md](../benchmarks/cpu-consolidation-2026-11-09.md).

## What changed

| Commit | Version | Change | Evidence |
| --- | --- | --- | --- |
| `097f01c` | 0.13.2 | The soundbox's modes keep one table of their shapes at the bridge, scaled by the root of each mode's gain, where they kept two; fused kernels on AVX2+FMA. | Largest difference from 0.13.1 over 18 probe cases: 142.3 dB under the signal. `Aculyre.ModeBank` holds both kernels to a mode-by-mode sum in `long double` (error under 1e-11 of the signal, six layouts). |
| `56d5cf1` | 0.13.3 | No subnormal arithmetic as a sound dies away, whether or not the host sets flush-to-zero. | `Aculyre.Denormals`: 36 cases, sticky denormal-operand and underflow flags clear; 27 of them fail on the commit before. |
| `2f0cb10`, `30a1ea4` | 0.13.4 | A re-pluck's contact convolution runs eight taps a pass, two outputs at a time. | The same bits in a native build and in one with the Rack's flags (24 probe cases each). |
| `61889b1` | 0.13.4 | No library call for a square root, a floor or a rounding while playing. | The same bits in both builds. |
| `e7406c2` | (tool) | `AculyreCpuProbe --scenario stress`: the portfolio's stress phrase, and each case's worst 256-frame block. | |
| `e7310fb` | (test) | `Aculyre.DspFuzzReplay`: the fuzz target runs in the unit tier. | 200 seeded inputs, three instruments, 22.05, 48 and 192 kHz. |

## Subnormals

Reason's documents do not say whether it calls with flush-to-zero set, so
the engine is held to needing neither it nor denormals-are-zero. Every case
was played through the Performer with both off and the CPU's sticky flags
read after each callback; a trap on the first flagged operation named each
site.

| Site | When | Fix |
| --- | --- | --- |
| The ring of each string's squared velocity (single precision), which feeds the tension glide and the sleep test | A fully palm-muted chord under held keys: subnormal for about 1.6 s from 1.0 s after the pluck. The first samples an idle string hears with Sympathy under 1. The softest pluck. | A square under 1e-30 (300 dB under a pluck's) is stored as zero. |
| The same ring, scaled by a re-pluck's kept energy | A re-pluck of a nearly dead string | The same floor. |
| The engine's output, squared in single precision by the output stage's detector | The softest muted pluck, and any tail under about 1e-19 | Samples under 1e-15 leave the engine as zero. |

Nothing else was flagged. In particular one string damped beside ringing
ones never reaches a double-precision subnormal, because every string and
every mode hears the bridge on every sample: forty seconds of it ran clean
before the fix. On the development machine the subnormal blocks cost no
measurable time (86 against 87 µs a callback), so this is a safeguard
against a host or CPU where they do, not a speed-up measured here.

The shared output stage (`Source/DSP/OutputStage.h`, a copy of the
portfolio master) squares its input in single precision
(`power = max(power, sample * sample)`), which is subnormal for inputs
under about 1e-19. The engine now keeps such inputs from it; the master
would be safer with its own floor. Written to the portfolio as an offer,
not changed here.

## The physics shared with Acustra and Aculele

`stretchedPartial`, `bendingLossRate`, `insideRoot`, `bendingLossSection`
and `constantLossSection` were compared, text against text, with Acustra
1.9.0 (their source), Acustra today and Aculele: all identical, and
`thiranCoefficients` is the standard closed form in all four. Neither
sibling has corrected the shared loss sections or the Thiran design since
the copy, so there was nothing to take.

## The lever

A lever note is the string at 2^(-1/12) of its length under the same
tension. The design follows from that alone: the frequency from c / 2L,
the inharmonicity from 1 / L^2 (12% more), the air and internal losses at
the new frequency, the tension glide with the frame's stiffness in series
with the shorter wire's, the parallel plane's end correction, and the
dispersion and the bridge reflection's phase designed again for it. That is
what a lever does to a string. Two things a real lever also does are not
modelled, and no measurement in hand gives either: its fork is a lossier
end than the tuning pin, and it deflects the string slightly. One small
inconsistency was found and left: Pluck position is a fraction of the
speaking length, so with the lever engaged the hand moves 5.6% nearer the
bridge, where a harpist's hand would stay put. At the default 27% that is
1.6% of the length, the size of the per-pluck spread the model already
draws (1.2 to 2.2%).

## Key range

`AculyreKeyboardSurvey` at velocities 32, 64 and 110 on each instrument
(each key from a reset engine, held 1.5 s, room off, levers on), measured by
`Tools/analyze_keyboard_survey.py`. The figures are for the strings' own
notes and their lever notes (B2 to G#6 on the harps, D3 to F5 on the gut
lyre): each note against the median of its six neighbours, in its level
from 20 to 300 ms (attack), from 0.5 to 1 s (sustain) and in its spectral
centroid.

| Instrument | Velocity | Attack, rms / worst | Sustain, rms / worst | Centroid, rms / worst |
| --- | ---: | --- | --- | --- |
| Steel harp | 32 | 0.92 / 2.90 dB (G6) | 3.06 / 6.76 dB | 2.25 / 6.41 semitones |
| | 64 | 0.63 / 2.54 dB (G6) | 2.98 / 6.96 dB | 2.47 / 6.36 |
| | 110 | 0.43 / 1.50 dB (G6) | 2.72 / 7.05 dB | 3.06 / 7.63 |
| Nylon harp | 32 | 0.95 / 2.76 dB (G#6) | 2.80 / 8.07 dB | 2.03 / 7.19 |
| | 64 | 0.72 / 2.39 dB (G#6) | 2.54 / 7.79 dB | 2.34 / 6.69 |
| | 110 | 0.42 / 1.17 dB (G#4) | 2.49 / 7.34 dB | 2.79 / 8.54 |
| Gut lyre | 32 | 2.42 / 6.15 dB (F5) | 3.22 / 6.94 dB | 4.57 / 11.95 |
| | 64 | 0.93 / 2.96 dB (F5) | 2.88 / 6.14 dB | 5.23 / 12.61 |
| | 110 | 0.30 / 0.70 dB (F#4) | 2.66 / 5.89 dB | 4.89 / 11.75 |

The survey was run on 0.13.1 before the pass and on 0.13.4 after it. Every
figure is the same: no note's attack, sustain or late level moved by 0.001
dB, no centroid by 0.0001 semitone and no pitch reading at all. That is
what changes 142 dB under the signal should do; the table is therefore both
the before and the after.

What stands out, and whose doing it is:

- **Soft plucks on the gut lyre are uneven** (rms 2.4 dB, F5 6.2 dB off
  its neighbours at velocity 32, against 0.3 dB at velocity 110). The model
  does this, and not by an error in one place: since 0.13.1 a soft pluck
  slips off slower and is darker, so its level rides on its fundamental
  alone, and each fundamental meets the three lowest soundbox modes
  differently. The per-string touch that evens the scale
  (`StringSpec::touch`) is fitted at velocity 90, where the upper partials
  average the body out. A touch that follows velocity would even it; that
  is a new mechanism and a new fit, so it is written down here and in the
  README's Known gaps, not built. Acukoto's remedy for the same symptom, a
  release that follows the touch, was measured afterwards (below).
- **Sustain, brightness and decay vary note to note** (sustain 2.5 to 3.2
  dB rms against the neighbours, centroid 2 to 5 semitones). The
  instrument does this: a note whose fundamental sits on a soundbox mode
  radiates more and dies sooner, and the two planes' beat moves the level
  at any fixed time. `Docs/validation/range-evenness-2026-11-07.md` has
  the notes and the modes; the survey has not moved since.
- **Pitch.** The survey reads pitch as the highest bin of a 0.3 s
  window's spectrum, and by it the harps' B4 is 6.8 cents sharp from 1.0
  to 1.3 s and the steel E5 8.5 cents sharp from 0.2 to 0.5 s, alike at
  every velocity. Neither is a tuning error. Resolved finely over 0.3 to
  2 s, the steel B4 is two components, 1.1 cents under and 2.7 cents over
  its pitch (the string's two planes), beating about once a second; the
  survey's window falls in the beat's trough, 25 dB down, where the sum's
  phase runs fast. Over 0.35 to 1.35 s the same note reads 1.3 cents
  sharp. Resolved the same way, the strongest component of every steel
  string and lever note is within 3.5 cents of its pitch (1.2 rms; the
  G#3 lever note is the furthest), which is what the README's Known gaps
  already say of strings under a strong soundbox mode. `Aculyre.Engine`
  holds the strings' modes to 3 cents by modal extraction for the same
  reason: a beating spectrum's maximum is not a pitch.

No outlier was found whose cause is a wrong formula, so no note was
changed.

## Soft plucks and a release that follows the touch (added 2026-11-10)

Acukoto 0.25.11 (`93aae28`) made a pluck's release follow the string's
touch as well as the velocity: a touch is force, the release law is the
displacement's, so the clamped term became `(v / 0.5)^-1.68 touch^-1.2`.
There the soft spread between keys fell from 7.6 and 8.4 dB to 3.4.
Aculyre's law is the same line, so the change was measured here before any
port, in scratch builds of 0.13.4 with that one line changed. It was not
ported, and the engine is unchanged.

**The diagnostic.** Played at velocity 32 with the release held at the
value velocity 64 gives it, each instrument's attack spread is its
velocity 64 figure: 0.63 / 0.72 / 0.94 dB rms (steel / nylon / gut)
against 0.63 / 0.72 / 0.93. Held at velocity 90's value: 0.48 / 0.50 /
0.39. So the release's slowing is what makes soft plucks uneven on all
three, as said above: at velocity 32 the finger lets go over 0.57 ms, a
corner near 280 Hz, under most fundamentals. That names the release, not
the touch; whether the touch is the remedy is what the survey says.

**The survey.** Attack level (20 to 300 ms) against the median of six
neighbours, dB rms at velocities 32 / 64 / 110:

| Release law | Steel harp | Nylon harp | Gut lyre |
| --- | --- | --- | --- |
| 0.13.4: the velocity alone | 0.92 / 0.63 / 0.43 | 0.95 / 0.72 / 0.42 | 2.42 / 0.93 / 0.30 |
| Acukoto's line, the touches as they are | 1.31 / 2.44 / 0.86 | 1.60 / 2.01 / 0.93 | 1.47 / 0.79 / 0.28 |
| Acukoto's line, the touches refitted | 1.16 / 1.01 / 0.62 | 1.32 / 1.22 / 0.84 | 1.25 / 0.65 / 0.32 |
| The touch against the instrument's mean touch, refitted | 2.10 / 0.93 / 0.37 | 2.19 / 0.93 / 0.47 | 1.20 / 0.44 / 0.31 |
| The touch against the register's slope of touch, refitted | 2.49 / 0.80 / 0.42 | 2.57 / 0.80 / 0.43 | 1.17 / 0.54 / 0.31 |

"Refitted" puts the touches and lever touches back on the velocity 90 line
as `Aculyre.Engine` measures it (six rounds, to the two decimals the
presets hold). Under Acukoto's line the harps' did not land: 1.35 and 2.43
dB off the line after six rounds, against the test's 1 dB and today's 0.42
and 0.74, because a string's level then goes as more than its touch. The
last two rows are not proposals. They separate the law's effect on
evenness from its overall slowing: Acukoto's touches lie on both sides of
1 (0.58 and 1.85 on the two keys its test names), Aculyre's are at most 1
(each instrument's strongest string), so the plain line slows every other
string, and with the touches refitted the harps' mean attack level falls
8.3 and 8.1 dB at velocity 32, 6.6 and 7.0 at 64 and 1.2 at 110 (the gut
lyre's 4.2, 1.1 and none).

- **The gut lyre: yes.** Its soft spread falls by two fifths with the
  touches as they are and halves once they are refitted, under every form of
  the law (2.42 to 1.47, then 1.17-1.25 dB rms; the worst note from F5's
  6.15 dB to about 3, D3 or F5). Its touches run from 0.35 to 1.00 with no
  fall down the register (they rise 0.2 dB a semitone), and its
  fundamentals, 147 to 698 Hz, lie on the three lowest soundbox modes. The
  reading that fits, not measured on its own: a string the soundbox favours
  is favoured more at its fundamental than across its partials, and the
  slower release its lighter touch earns takes that off.
- **The harps: no.** Their soft plucks are less even under every form of the
  law, and under Acukoto's line at every velocity. Their touches fall 0.25
  and 0.30 dB a semitone, from 1 in the bass to 0.14 and 0.11 in the treble
  (lever notes 0.11 and 0.10): read as force, the treble is plucked at about
  an eighth of the bass's, and the law slows its release by up to the
  clamp's whole factor of 10. Taking the register's slope out of the touch
  (the last row) does not help either. The reading that fits, again not
  measured on its own: above the lowest modes a string's fundamental is not
  favoured as its touch says, and where the release's corner lies under the
  fundamental the level goes as the touch to the 2.2, which over-applies
  each string's evening.

One release law serves the three instruments, and the same finger plays
them. The line that halves the gut lyre's soft spread un-evens both harps,
and a law switched on for one instrument is a mechanism the physics does
not give, so nothing was ported and the gut lyre's gap stays in the
README. What the harps' result says about their touches (that they carry
the soundbox's treble balance as well as each string's evening, and so are
not all force) is a question for a measured soundbox, not for this pass.

## Other findings, not fixed

- **The engine is slow to sleep at Sympathy 1.** After a released chord
  the steel, nylon and gut instruments take 29.5, 24.6 and 12.3 s to pass
  the sleep test (every string under 1e-9 of energy, output under -140
  dBFS for half a second), because the idle strings ring on in sympathy,
  as they should. Until then the instrument costs what it costs playing.
  A lighter "nobody can hear this" state is a design question, not a bug.
- **The hand-set touches have no generator.** Aculele's `AculeleLevels`
  and Acukoto's `--write-keys` regenerate their level tables; Aculyre's
  touches were fitted once and typed in. `Aculyre.Engine` holds every
  string within 1 dB of the line at velocity 90 and reports the touch each
  would need, so drift is caught, but a change to the body means refitting
  by hand.
- **The native path is no longer bit-exact with the portable one.** From
  0.13.2 a CPU with AVX2 and FMA3 runs fused kernels; their output differs
  from the portable build's by about 160 dB under the signal. Reason's
  build is always the portable one. A test that compares renders across
  machines must allow for it.
- **Every Known gap in the README was read against the engine**: all still
  hold, and none is cheap to close, because each waits on a measured
  instrument (the soundbox, the string losses, the nail and plectrum) or
  is an authored stand-in the README already names as one.

## What separates this from a release

Engine and tests:

- No `perf` tier. `Aculyre.CpuBudget` needs the portfolio's
  `Tools/PresetDemoRenderer.h` and a preset-demo renderer, which Aculyre
  does not have yet; the shared header is being changed by other work
  today, so it was not added in this pass. Until then the 10% ceiling is
  checked by hand with `AculyreCpuProbe --scenario stress`.
- No `audio` tier demos: `Aculyre.AudioDemos` needs the same renderer. The
  three `Aculyre.Keyboard*` suites are the audio tier today.
- The worst 256-frame block of the stress phrase is above 10% of the
  development machine's core for the two harps in a build with the Rack's
  flags (see the benchmark document). The exact rewrites that pay there
  are done; what remains would change numbers by more than the 120 dB a
  neutral change is allowed.
- `Tests/Fixtures/fuzz/` is empty: the libFuzzer target has not been run
  for long in this pass (it needs Clang's fuzzer runtime and hours).
- `CLAUDE.md` still calls the instrument a feasibility prototype.

Outside the engine, not looked at in this pass: the JUCE plug-in, CI, and
the build-written README parts (demo table, downloads, screenshot).
