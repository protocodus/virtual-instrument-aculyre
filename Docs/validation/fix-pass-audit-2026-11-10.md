# Fix pass, 2026-11-10 (0.13.5)

A second reading of the engine and the Performer from end to end, for
defects only: wrong arithmetic, rate or block dependence, state that is not
reset, a control that does not reach, clicks, stuck or lost notes, and
strings, keys or instruments that stand out where the design does not mean
them to. No control, mechanism or preset was added and nothing was
re-voiced. The three instruments are unchanged: their dimensions are
typical of each kind, not measurements, and the nylon and gut losses are
estimates.

Two defects were found and fixed. Both are audible, so each has a
level-matched pair for the ear (A is 0.13.4, B the fix); the fixes are in
0.13.5 unless the owner says otherwise. CPU work of the same day is in
[cpu-fix-pass-2026-11-10.md](../benchmarks/cpu-fix-pass-2026-11-10.md).

## What changed

| Commit | Version | Change | Evidence | Test |
| --- | --- | --- | --- | --- |
| `07653da` | 0.13.5 | A key above the top string keeps that string's own split between its two planes. | Planes up to 28 cents apart before, the string's 3.0 to 5.1 after; the first 200 ms of the nylon C8 read 14.1 cents sharp, now 5.7. | `Aculyre.RangeSafety`: every key above the top string, three instruments; fails on 0.13.4 (7.8, 14.1 and 28.3 cents). |
| `d19e227` | 0.13.5 | Turning the instrument control through a second instrument inside the 20 ms fade no longer cuts the first one's tail off in one sample. | Sample steps of 0.11 to 0.12 on a ringing string whose own largest step is 0.030; none above it after. | `Aculyre.InstrumentSwitch`: twelve double switches and a single one; eight fail on 0.13.4. |

## Keys above the top string

A string's two planes end at slightly different places: the plane along
the top is one string diameter longer than the plane normal to it, which
is what makes a string's decay two-stage. A key above the top string is
that string played at the key's pitch, designed as a shorter string of the
same wire. The diameter was added to that imagined length, so the split
between the planes grew as the key rose: one diameter is 0.17% of the
steel harp's 132 mm top string and 0.47% of the 49 mm its C8 is designed
at.

Designed pitch of the plane normal to the top against the key's pitch
(`contactReport`, tension glide off), the worst key above the top string:

| | Top string's own split | 0.13.4 | 0.13.5 |
| --- | --- | --- | --- |
| Steel lyre harp (G6, 0.23 mm) | 3.0 cents | 7.8 | 2.8 |
| Nylon lyre harp (G6, 0.39 mm) | 5.1 | 14.1 | 5.6 |
| Gut lyre (E5, 0.58 mm) | 4.5 | 28.3 | 4.5 |

The normal plane radiates first and dies first, so the key opened sharp
and then settled. Pitch of the first 200 ms (30 to 230 ms after the pluck,
velocity 80, microphones, 48 kHz), keys above the top string:

| | 0.13.4 | 0.13.5 |
| --- | --- | --- |
| Steel lyre harp | -1.7 to +5.4 cents | -3.6 to +4.0 |
| Nylon lyre harp | -1.0 to +14.1 | -1.0 to +5.7 |
| Gut lyre | -0.2 to +15.0 | -0.5 to +7.0 |

Every key up to the top string reads the same before and after to 0.001
cent, and the keys below the lowest string are untouched: a longer string
of the same wire has a smaller split than the string's, which is the
instrument's. What remains above the top string is the string's own split
and the soundbox's pull where a key falls on one of its modes (the gut
lyre's F#5 and G5 at +6.6 and +7.0, the steel harp's A6 at -3.6), which the
strings of the instrument show as well.

From 0.13.5 the correction added to a key above the top string is the
string's diameter scaled by the key's length over the string's, so the two
planes keep the ratio the string has.

Listening set `treble`: the gut lyre, then the nylon harp, single notes
rising from the top string by 3, 7, 10, 12, 15 and 17 semitones at velocity
0.75, then the same keys with strings below them; 14.8 s. B is trimmed by
+0.063 dB to A's whole-file RMS (-26.74 dBFS).

## The instrument switch

The Performer fades the old instrument out over 20 ms while the new one
starts. It kept one fading instrument: a second switch inside the fade
(the control turned from the first position to the third through the
second, or there and back) dropped the first tail where it stood and
reset that instrument. With a steel B2 ringing at full velocity the output
stepped by 0.11 to 0.12 in one sample, four times the string's own largest
step (0.030). A hand turning a three-position control through the middle
one does this whenever it passes in under 20 ms.

From 0.13.5 each instrument has its own place in the fade. A silent one
starts at once; one still fading comes back from where its fade stands;
whatever else is fading goes on fading. A single switch renders the same
bytes as before at 1x, 2x and 4x.

Listening set `switch`: a steel chord (B2 F#3 B3 D4 F#4, velocity 0.9)
with the instrument turned 0, 1, 2 or 0, 1, 0 and 4 to 16 ms on the middle
position, four rounds; 5.0 s. B is trimmed by +0.003 dB.

## Surveyed and found sound

- **Sample rate.** Every key of the three instruments at 44.1 and 48 kHz:
  tuning differs by 0.01 to 0.02 cents rms and the attack level by 0.01 to
  0.02 dB rms.
- **Reset and sleep.** `reset()` and the sleep path were read against every
  piece of state a block moves; nothing is left over, and a faded
  instrument is reset when its fade ends.
- **Notes.** Overlapping presses, sustain, the levers and the keys past the
  strings release as documented (`Aculyre.Ownership`, `Aculyre.RangeSafety`).
- **Subnormals.** `Aculyre.Denormals` passes with both fixes in.

## Found and left

None of these is a defect of the model as documented; each would be a new
mechanism, a re-voicing or a change to a file this repository only copies.

- A raised lever shortens the string, and the pluck stays where the hand
  is, so a lever note is plucked 5.6% nearer the bridge as a fraction of
  its length. That is what a lever does; the lever notes' touch has no
  generator of its own (2026-11-09 audit).
- Each end of the artificial range plays one key at a time (Known gaps).
- The keys A7 and A#7 stand 4 to 6 dB above their neighbours where they
  fall on soundbox modes, and A4 and G#4 open a few cents off
  beside the 434 Hz mode. This is the assumed soundbox, not an error in it.
- With Sympathy at 1 the engine sleeps only when every open string has died
  away, seconds after the last note.
- Soft plucks on the gut lyre are uneven from string to string (Known gaps).
- The shared output stage squares its detector input in single precision;
  the engine sends it zeros below 1e-15 (0.13.3). It is the portfolio's
  file and changes there.
- There is no `perf` tier and no `Aculyre.AudioDemos` yet (the prototype's
  listening set is rendered, not committed), and `Tests/Fixtures/fuzz/`
  holds no crash input: none has been found.

## Tests run

GCC 13.3 and Clang 18.1, Release, 48 kHz unless a suite sets its own:
all 17 CTest tests (14 `unit`, 3 `audio`) and `AculyreDSPCxx17`.
