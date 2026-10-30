# Aculyre against the lyre recordings, 2026-10-30

Where 0.7.2 (`00c20d7`) stands against the three recordings the model is
checked against, after the day's three engine steps (0.7.0 per-mode
radiation, 0.7.1 the microphone pair, 0.7.2 the lyre harps' 6 mm top), and
what was measured but not shipped. The decisions behind each step are in
[decisions](../decisions.md); the earlier benchmark is
[dr-lyre-harp-benchmark-2026-10-29](dr-lyre-harp-benchmark-2026-10-29.md).

## DR Plucked Lyre Harp (16 strings, three plucks each)

`Tools/compare_lyre_harp_samples.py`, whose head states the protocol, on
`RenderAculyreNotes 0 <dir> --rate 44100 --flatten 57,59,62,69,71,74,81`.
Shape is the third-octave rms from 200 Hz to 10 kHz, each note at equal
power, over the first 0.3 s / after.

| | 0.6.5 | 0.7.2 |
| --- | --- | --- |
| shape (dB rms) | 5.9 / 6.2 | 4.9 / 5.1 (4.6 / 5.2 over four draws of the body) |
| decay rms log2, fundamentals / partials 2-6 | 1.23 / 0.50 | 1.03 / 0.68 |
| channel correlation by octave from 200 Hz (recording 0.99, 0.97, 0.95, 0.64, 0.42, 0.19) | 0.87-0.91 from 0.8 kHz up | 1.00, 1.00, 1.00, 0.90, 0.34, 0.53 |

The channel rows are the tool's new `channels` section (same protocol as
the 0.7.1 decision).

**Treble and air above 4 kHz.** Taking each note's 4-11 kHz against its
own 1-4 kHz, the model's C4 to G#4 stand 3 to 22 dB brighter than the
recording's, its A#4 to G#5 mostly darker from 4 to 8 kHz, by up to 19 dB
(one band of F5 and of G#5 level or brighter); the wound G3 is 9 to 42
dB darker (the recording's G3 reads as plain wire). In the attack, the
recording carries 11 to 24 dB more between the partials from 4 to 8 kHz
than the model does, for the first 320 ms: noise, other strings or the
library's processing that the model's strings do not make. A search for
the strings' lengthwise modes among the recording's strongest 3-12 kHz
peaks found none that stand clear of the stretched partials.

**Where the 5-8 kHz shortfall sits.** Not in the strings' decay: from 5
to 8 kHz the model's partials decay at 45, 55 and 69 dB/s (median, 0.08-0.6
s) against the recording's 49, 50 and 65. It is there from the first 10 ms,
in every one of four draws of the body (5 to 15 dB from 5 to 8 kHz, both
windows), and on the 4 mm top too, while 4 kHz sits within 2 dB and 10
kHz within 1 dB on the shipping body (7 over the four draws).
A release with no slip at all (the nail's) lifts 4-8 kHz 2 to 3 dB, but it
leaves the 5, 6.3 and 8 kHz bands 5 to 7 dB short. The model's microphones
against its own piezo fall 10 to 20 dB from 4 to 8 kHz relative to 2 kHz
and come back by 10 kHz, so the shortfall is in the body, not in the
strings' force on the bar. The bar's bends do not place it: a bar 1.5
or 0.7 times as tall moves the bands by 2 dB at most. The radiation does not place
it either: every top mode from 5 to 8 kHz but two is past its own
coincidence, radiating at an efficiency of 1. The coupling does: the bar
runs across the top, so it averages away the modes with several half-waves
across it, and from 4.2 to 10 kHz only the (4,1) mode at 7.3 kHz meets it
with a heave participation above 0.1 (the bar's bends, above, add little) (the (3,1) at 4.1 kHz and the (5,1)
at 10.2 kHz bracket the dip). A real top's outline, bars and grain
scatter would spread that coupling; the rectangular plate cannot say how
much. Nothing was shipped for it.

**Decays by register.** The fundamentals from G3 to A#4 decay at about half
the recording's rate (4.3 to 11 dB/s against 7 to 27), and partials 2-6
from G3 to F4 mostly at 0.5 to 0.8 of it (C4's at 1.7); from G4 up the
partials scatter from 0.7 to 2.4 times the recording's, with no trend. The
strings' own losses set most of it: G3's intrinsic T60 is 14 s, 4.3
dB/s, and the shipping body adds 0.5 dB/s, where the recording's 8.9 dB/s
needs the bar's real mobility near 0.004 m/s/N at 196 Hz (more than twice the 6 mm
top's |Y|). The top's thickness moves it, against the spectrum: as the
mean log2 of model over recording, G3 to F4,

| top | fundamentals G3-F4 | partials 2-6 G3-F4 / G4-G#5 | DR shape |
| --- | --- | --- | --- |
| 6 mm (0.7.2) | -0.77 | -0.49 / +0.31 | 4.9 / 5.1 |
| 5 mm | -0.72 | -0.19 / +0.15 | 4.7 / 6.5 |
| 3 mm | -0.06 | -0.08 / +0.03 | 5.1 / 6.7 |

The 3 mm top's treble fundamentals scatter further (rms 1.85 against
1.17), and its late spectrum sits further from this recording.
Like the lyre harp phrase below, the bass decays favour a thinner top than
the DR spectrum does. Nothing shipped for it either.

**Thicker treble strings, measured again.** The strings from A4 up 1.6
times thicker at the same length and stress match the recording's
inharmonicity note by note (A#4 3.4e-4 against 3.7e-4, C5 3.0 against 2.6,
E5 1.7 against 1.7, G5 2.0 against 2.1) and lift the treble's 4-11 kHz 1
to 7 dB, but the shape after 0.3 s goes from 5.1 to 7.0 dB rms, as on
2026-10-29 with the 3 mm top. Not shipped; the gauges stay assumed.

## Lyre harp phrase (Freesound 649925)

`Tools/compare_lyre_harp_phrase.py`, its eight notes transcribed by onset
and pitch, rendered with `RenderAculyreDemo 0` at the preset's defaults:
long-term shape 7.5 dB rms on 0.6.5, 8.1 on 0.7.1, 11.2 on 0.7.2. The
6 mm top leaves the phrase's fundamentals (294-494 Hz) 8 to 15 dB short.
Nine bodies (4 to 6 mm, 1.0 to 1.3 times the size, four draws each) trade
the two recordings against each other: none fits both.

| top | DR shape | phrase shape |
| --- | --- | --- |
| 4 mm, 1.0 | 5.7 / 6.9 | 8.0 |
| 4 mm, 1.3 | 6.0 / 8.0 | 7.7 |
| 5 mm, 1.0 | 5.0 / 6.1 | 9.4 |
| 6 mm, 1.0 (0.7.2) | 4.6 / 5.2 | 10.8 |
| 6 mm, 1.3 | 6.0 / 6.0 | 9.1 |

The phrase renders here play every string at a touch of 1, so their levels
are the body's alone.

## Gut lyre recording (Freesound 191883)

The gut lyre's README demo against the recording, onsets at the spectral
flux's peaks, each note's centroid over its first 250 ms by channel and
decay from 80 to 550 ms: 0.6.5 centres at 492 / 650 Hz and decays at 25
dB/s, 0.7.2 at 521 / 531 Hz and 21 dB/s; the recording reads 577 / 607 Hz
and 20 dB/s. The gut lyre's body did not change; the radiation and the
microphone pair moved it.

**After 0.7.3 (the gut lyre plucked at 0.11).** Replaying the recording's
melody (`Tools/compare_gut_lyre_phrase.py`), partials 2-12 sit 3.5 dB rms
from the recording's, against 10.4 at 0.27. Partial 1 decays at 15.2 dB/s
against 15.9; partials 2-6 at 27.5 against 37.5. Gut's loss modulus would
have to be over three times the published fit's 0.24 GPa to close that
(1.5 times: 28.7, twice: 31.9, three times: 34.2, with the partials' rms
4.7), and the recording's partials share their bands with the strings still
ringing from earlier notes; not changed.
