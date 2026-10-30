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
