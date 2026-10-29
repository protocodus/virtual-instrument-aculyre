# The DR Plucked Lyre Harp as a benchmark, 2026-10-29

Two free sample libraries were checked as references for the steel lyre harp.
DeadRobot's DR Plucked Lyre Harp v1 is useful: it is the first recording of a
whole lyre harp the model has, every string plucked alone, over the same range
as the model's harp. The Radio Lyre is not. Measured note by note, the DR harp
supports moving the pluck point toward a quarter of the string, and it agrees
with the earlier lyre harp phrase (Freesound 649925) that the model is short of
treble and that the strings from A4 up are more inharmonic than the assumed
gauges make them. Where the model's body resonances fall, and how loud the
treble should be, it cannot settle: its microphones and pluck force are
unknown and the soundbox is assumed.

The measurements come from `Tools/compare_lyre_harp_samples.py`, whose head
states the protocol, on renders from `RenderAculyreNotes` (`Tools/RenderNotes.cpp`)
at engine version 0.6.0 (`8cacd07`).

## The DR Plucked Lyre Harp

Free from deadrobotmusic.com as a Decent Sampler instrument. Neither the
samples nor anything derived from them is stored here.

- **What it holds.** One harp's 16 strings, G3 to G#5, each plucked three
  times (`Lyre <note> RR<1-3>.wav`), 7.5 to 9.7 s long, 44.1 kHz 32-bit float
  stereo, peaks at -3 to -10 dBFS and nothing clipped. The harp is strung
  C major from G3 to A5 with its A, B and D strings tuned a semitone down
  (A3, B3, D4, A4, B4, D5 and A5 sound G#3, A#3, C#4, G#4, A#4, C#5 and
  G#5). The file names give the sounding pitch; the instrument's preset maps
  each sample unshifted to the key an octave above it.
- **Tuning.** Fitted over 0.1 to 1.6 s, the strings sit 1.7 cents flat of
  A = 440 Hz equal temperament on average, with a spread of 1.7 cents
  (-5.0 to +0.5).
- **Repeatability.** The three plucks of a note land close together: their
  fundamentals agree within 2 cents on 15 of the 16 notes (G5's within 2.7),
  half of all their partials within 0.4 cents, and their fitted pluck
  points within 0.04 on 10 of the 16 notes.
- **Tails.** Every tail falls to -128 to -138 dBFS with no steady noise
  floor: the recordings are denoised and faded. Decays past about a second
  are not used.
- **Low end.** Below 150 Hz the recordings carry about 40 dB less than just
  above it. The lowest string is 196 Hz, so this may be the instrument or a
  high-pass filter; nothing below 198 Hz is compared.
- **Microphones.** A stereo pair of unknown kind and placement: the two
  channels correlate 0.56 to 0.98 from note to note. Both are averaged.
- **Level.** The 0-300 ms level rises about 0.39 dB a semitone up the
  harp. With pluck force unknown and the samples possibly levelled, this is
  not a target.
- **What it cannot tell.** One velocity, single notes only, no
  articulations, no damping or chords, unknown pluck force, microphones and
  room, and no measurement of the body.
- **Sympathetic strings.** The harp's other strings were left free. On C4
  the 3rd, 4th, 11th, 13th and 14th partials sit within 2 cents of
  harmonic, and the 3rd, 4th and 11th decay at -8 to +12 dB/s, where its
  other partials follow a stiff string (+4 to +20 cents by the 12th): the
  G5 string's fundamental lies 3 cents from C4's 3rd partial and C5's
  octave 2 cents from its 4th. Its fitted B
  (-6e-6) is not a string's and is left out of the inharmonicity comparison
  below; its decays stay in the tool's averages. A#3's 3rd partial, on the
  F5 string, grows the same way (-10 dB/s). The model's other strings ring
  in sympathy too.

## The Radio Lyre

Not useful as a benchmark. It is a homemade six-string instrument on a tin
box, a different kind of instrument from any Aculyre plays. Only one plucked
note is recorded (E3, six takes), every take is normalised to the same peak,
so soft and hard plucks cannot be told apart, the recording is mono with
noise about 50 dB under the peak, and most of the library is bowed.

## How the model was rendered

```sh
build/RenderAculyreNotes 0 <renders> --rate 44100 --flatten 57,59,62,69,71,74,81 [--position p | --positions note:p,...]
python3 Tools/compare_lyre_harp_samples.py <Samples> <renders>
```

The steel lyre harp, every string plucked alone from a reset engine at
velocity 0.7, the preset's controls with the room off, at the recording's
rate, with the same seven strings slackened a semitone. Three pluck points
were compared: the shipping 0.47, each note at the point fitted to its
recording (`--positions` from the tool's `--positions` output), and 0.27,
the median over all 48 recorded plucks. The engine's pluck scatter moves
each pluck from the set point as it does in play.

## Results

### Note by note

Recording against the model at the shipping 0.47. Pluck points are the three
plucks' fitted points; H1 is the fundamental's decay and H2-6 the median of
partials 2 to 6, both from 0.08 to 0.6 s.

| Note | f0 Hz (cents) | B | Pluck points | H1 dB/s | H2-6 dB/s | Model B | Model H1 | Model H2-6 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| G3 | 195.46 (-4.8) | 5.3e-4 | 0.32 0.30 0.31 | 8.9 | 30.0 | 1.9e-4 | 7.2 | 18.4 |
| G#3 | 207.43 (-1.9) | 3.7e-4 | 0.12 0.35 0.34 | 7.3 | 23.5 | 6.1e-4 | 20.6 | 23.3 |
| A#3 | 232.99 (-0.7) | 2.7e-4 | 0.32 0.32 0.31 | 10.4 | 21.5 | 4.6e-4 | 5.5 | 14.5 |
| C4 | 261.69 (+0.4) | (see above) | 0.32 0.32 0.32 | 9.6 | 7.9 | 3.7e-4 | 6.1 | 19.0 |
| C#4 | 276.91 (-1.7) | 2.9e-4 | 0.28 0.27 0.29 | 7.9 | 18.5 | 3.4e-4 | 6.5 | 15.8 |
| E4 | 329.47 (-0.8) | 1.8e-4 | 0.48 0.08 0.24 | 7.2 | 20.2 | 2.5e-4 | 8.8 | 19.8 |
| F4 | 349.14 (-0.4) | 1.7e-4 | 0.25 0.25 0.25 | 11.6 | 19.4 | 2.4e-4 | 10.5 | 21.1 |
| G4 | 391.56 (-1.9) | 1.6e-4 | 0.25 0.25 0.25 | 7.6 | 38.3 | 1.5e-4 | 33.9 | 22.3 |
| G#4 | 414.57 (-3.1) | 4.3e-4 | 0.20 0.21 0.22 | 26.9 | 18.1 | 1.7e-4 | 20.8 | 23.4 |
| A#4 | 465.05 (-4.1) | 3.7e-4 | 0.27 0.27 0.27 | 15.3 | 25.4 | 9.8e-5 | 23.7 | 20.2 |
| C5 | 522.71 (-1.8) | 2.6e-4 | 0.30 0.26 0.26 | 46.0 | 13.7 | 6.8e-5 | 6.1 | 22.3 |
| C#5 | 554.51 (+0.5) | 2.7e-4 | 0.27 0.27 0.13 | 23.1 | 12.8 | 7.9e-5 | 13.0 | 25.8 |
| E5 | 657.37 (-5.0) | 1.7e-4 | 0.21 0.24 0.31 | 30.6 | 27.4 | 7.5e-5 | 11.4 | 32.1 |
| F5 | 697.51 (-2.3) | 1.8e-4 | 0.33 0.33 0.33 | 50.4 | 25.7 | 9.4e-5 | 16.5 | 36.0 |
| G5 | 783.82 (-0.4) | 2.1e-4 | 0.31 0.32 0.15 | 59.8 | 30.9 | 9.0e-5 | 35.4 | 33.9 |
| G#5 | 830.68 (+0.1) | 2.9e-4 | 0.34 0.34 0.16 | 15.5 | 35.8 | 1.1e-4 | 25.2 | 30.7 |

Partial frequencies and decay rates do not depend on the microphones, so
these columns are the firmest comparison the pack gives.

- **Pluck point.** Over all 48 plucks the median is 0.271, the middle half
  0.250 to 0.315. A few single plucks fit near 0.1 or 0.5, where the comb
  pattern is ambiguous (plucks at 0.125, 0.25 and 0.5 all leave out the 8th
  partial); the medians per note run from 0.21 (G#4) to 0.34 (G#3, G#5).
  On the model's own renders the fit lands within 0.03 of the drawn point
  on 11 of 16 notes.
- **Inharmonicity.** From E4 to G4 the model's strings match (1.5e-4 to
  2.5e-4 against 1.6e-4 to 1.8e-4). From G#4, the A4 string, up, the
  recording's are 1.9 to 3.8 times the model's, and they jump at that
  string: 1.6e-4 on G4, 4.3e-4 on G#4. The lyre harp phrase showed the same
  jump at A4. Its G3 reads as a plain string (5.3e-4, more than its G#3's
  3.7e-4), where the model's G3 is wound (1.9e-4). Since B goes as
  d^2 / (L^4 f^2), the jump can be a thicker gauge or a shorter string, and
  B alone cannot say which.
- **Decays.** Partials 2 to 6 decay broadly alike: the rms of
  log2(model / recording) over the notes is 0.57 (a typical factor of 1.5;
  0.49 without C4, whose sympathetic partials hold its 7.9 dB/s up).
  The fundamentals do not (1.22, a factor of 2.3), because the recording
  and the model drain different notes. The recording's fundamentals from
  C5 to G5 fall 23 to 60 dB/s; the model's there fall 6 to 35. The model's
  G4 fundamental falls 34 dB/s against the recording's 8. Where a body's
  modes sit sets these, and the model's body is assumed.

### Third-octave spectra

Each note at equal power, then averaged over the 16 notes (the recording's
three plucks each); model minus recording in dB, by band centre. Shape is
the rms from 200 Hz to 10 kHz after removing the mean difference.

| Band Hz | 0-0.3 s: 0.47 | fitted | 0.27 | 0.3-1.5 s: 0.47 | fitted | 0.27 |
| --- | --- | --- | --- | --- | --- | --- |
| 198 | +8.8 | +6.5 | +5.6 | +2.2 | +0.3 | -0.0 |
| 250 | +5.7 | +1.3 | +0.5 | +5.5 | +3.1 | +2.4 |
| 315 | +4.4 | +2.7 | +3.1 | +3.9 | +2.9 | +3.1 |
| 397 | +6.4 | +7.5 | +7.9 | +10.5 | +11.0 | +11.2 |
| 500 | +3.2 | +4.8 | +4.9 | +4.5 | +5.8 | +5.9 |
| 630 | +1.2 | +2.0 | +1.8 | -0.1 | +0.2 | +0.3 |
| 794 | -4.1 | -5.0 | -6.1 | -4.3 | -5.4 | -6.5 |
| 1000 | -8.5 | -5.8 | -7.0 | -17.7 | -9.2 | -9.5 |
| 1260 | -11.6 | -8.9 | -9.7 | -15.2 | -10.7 | -10.3 |
| 1587 | -7.5 | +0.5 | +0.7 | -4.0 | +4.7 | +5.2 |
| 2000 | +1.0 | -0.7 | +0.8 | -3.5 | -4.8 | -3.7 |
| 2520 | -0.0 | -7.2 | -3.1 | +7.8 | -1.7 | +4.1 |
| 3175 | -11.8 | -9.9 | -10.3 | -6.7 | -4.8 | -8.8 |
| 4000 | -3.7 | -2.1 | -3.7 | -4.1 | -4.1 | -5.8 |
| 5040 | -7.1 | -8.2 | -6.0 | -10.0 | -12.8 | -9.4 |
| 6350 | -12.4 | -8.1 | -10.4 | -11.7 | -8.0 | -13.2 |
| 8000 | -13.0 | -14.1 | -9.7 | -9.7 | -13.9 | -7.7 |
| 10079 | -16.9 | -14.0 | -13.4 | -15.9 | -18.1 | -11.7 |
| **Shape rms** | **7.6** | **6.5** | **6.2** | **8.2** | **7.4** | **7.0** |

The spectra depend on the microphones, so they compare shapes, not levels.
At every pluck point the model carries more from 200 to 500 Hz (up to 9 dB
in the first 0.3 s, and 11 dB at 400 Hz after it), less from 0.8 to 1.3 kHz,
where the recording peaks (4 to 11 dB at the fitted points and 0.27, up to
18 at 0.47), and 6 to 18 dB less from 5 to 10 kHz. Plucking at 0.27 lifts
the 1 to 1.6 kHz bands, which 0.47 leaves 4 to 18 dB short, by 1.5 to 9 dB,
and brings the shape 1.4 and 1.2 dB closer (the fitted points 1.1 and 0.8).
A uniform 0.25 scores as 0.27 does (6.1 and 7.0 dB). The partial decays do
not change with the pluck point (0.57, 0.56 and 0.54).

### Partial decays by band

Median decay of every partial in the band over 0.08 to 0.6 s, dB/s, model at
0.47 (number of partials in brackets).

| Band Hz | Recording | Model |
| --- | --- | --- |
| 198 | 7.6 (6) | 13.9 (2) |
| 250 | 9.6 (9) | 6.1 (3) |
| 315 | 9.3 (6) | 9.7 (2) |
| 397 | 26.5 (12) | 27.3 (4) |
| 500 | 15.1 (18) | 11.1 (6) |
| 630 | 5.1 (21) | 12.9 (7) |
| 794 | 16.8 (24) | 18.1 (8) |
| 1000 | 16.9 (30) | 29.2 (10) |
| 1260 | 21.3 (42) | 25.9 (13) |
| 1587 | 25.0 (42) | 18.5 (16) |
| 2000 | 29.4 (60) | 23.3 (18) |
| 2520 | 38.8 (78) | 29.5 (25) |
| 3175 | 58.2 (87) | 39.6 (31) |
| 4000 | 53.2 (102) | 46.7 (36) |
| 5040 | 48.6 (106) | 63.3 (46) |
| 6350 | 49.5 (84) | 71.3 (56) |
| 8000 | 65.2 (67) | 90.8 (61) |
| 10079 | 85.9 (68) | 98.3 (74) |
| 12699 | 90.4 (18) | 108.8 (20) |

Below 1.6 kHz the bands scatter both ways on few partials: the recording's
630 Hz partials ring far longer (5 against 13 dB/s) and its 1 kHz partials
too (17 against 29). From 1.6 to 4 kHz the model's partials decay 12 to 32%
slower than the recording's, and from 5 kHz up 14 to 44% faster.

### The attack

Energy within 40 Hz of a partial, and between the partials, each note at its
0-300 ms energy; model at the fitted points minus recording, dB, as
partials / between:

| From the onset | 1-2 kHz | 2-4 kHz | 4-8 kHz | 8-16 kHz |
| --- | --- | --- | --- | --- |
| 0-10 ms | -3.0 / -5.9 | -8.1 / -10.6 | -8.7 / -18.4 | -12.5 / -12.5 |
| 10-20 ms | -3.2 / -3.8 | -9.8 / -8.9 | -9.4 / -17.7 | -16.0 / -12.3 |
| 20-40 ms | -3.7 / -3.5 | -9.7 / -12.1 | -8.0 / -13.6 | -14.4 / -11.9 |
| 40-80 ms | -3.8 / -5.1 | -7.7 / -11.4 | -7.5 / -8.8 | -14.5 / -11.7 |
| 80-160 ms | -3.5 / -9.8 | -6.6 / -22.1 | -8.2 / -18.2 | -15.4 / -14.7 |
| 160-320 ms | -7.3 / -7.5 | -7.1 / -21.6 | -10.3 / -21.2 | -17.2 / -18.6 |

The treble shortfall is in the partials, from the first 10 ms to 320 ms,
not a missing click: the model's partials stand 7.5 to 10.3 dB lower from
4 to 8 kHz and 12.5 to 17.2 dB lower from 8 to 16 kHz in every window.
Between the partials the recording carries more still, as its room, its
denoising or contact noise could; the renders are dry.

## Candidates that the pack does not support

Each rendered at the fitted points; shape over 0-0.3 / 0.3-1.5 s and decay
rms for fundamentals / partials 2-6, against the fitted points' 6.5 / 7.4
and 1.22 / 0.56.

| Candidate | Shape | Decays | Why it does not ship |
| --- | --- | --- | --- |
| a nail's edge | 6.2 / 6.9 | 1.22 / 0.56 | about 1 dB more from 5 to 10 kHz (3 dB at 10 kHz after 0.3 s), as on the phrase; most of the gap stays, and nothing says this player used a nail |
| velocity 1.0 instead of 0.7 | 6.4 / 7.0 | 1.24 / 0.53 | under 1 dB more treble; the pluck force is unknown, so velocity is not a fit target |
| velocity 0.4 | 7.0 / 7.8 | 1.21 / 0.53 | further from the recording |
| top 4 / 5 / 6 mm instead of 3 | 5.2 / 6.9, 4.2 / 7.1, 4.5 / 5.8 | 1.01 / 0.79, 1.09 / 0.71, 1.03 / 0.63 | moves every body mode: the shape and the fundamentals come closer, the partials' decays move further off, and single bands swing by up to 13 dB; no lyre harp's top is published, so picking one would fit the body to one recording through unknown microphones |

## What the pack supports

- **The pluck point is the one change it supports**, and it disagrees with
  the phrase that set 0.47: this player plucks at 0.25 to 0.32, the phrase's
  player half its notes at 0.46 to 0.50, and concert harpists at 0.33 to
  0.385. Each is a defensible default, so it goes to a listening test
  (A the shipping 0.47, B 0.27, with the touch re-evened and the gains
  re-levelled at B), recorded in [decisions](../decisions.md) once heard. At
  0.27 the steel lyre harp's top octave at 1x falls 2.62 dB under 4x, past
  the 2.5 dB the engine tests allow, so shipping B first needs that gap
  closed, not the limit moved.
- **The treble shortfall is now seen in two recordings** of two players: 6
  to 18 dB from 5 to 10 kHz here, in the partials from the first 10 ms, and
  4.6 dB from 6 to 10 kHz over the phrase. Neither recording separates the
  body, the microphones and the room, so it stays a known gap.
- **The strings from A4 up are more inharmonic** than the assumed gauges
  make them, in both recordings. A string set's published gauges for this
  harp would settle it; fitting gauges to B alone would not, since B
  cannot pin both gauge and length.
- **The body's resonance region and the fundamentals' decays** differ, as
  the assumed soundbox would make them. One recording through unknown
  microphones cannot place the modes.
