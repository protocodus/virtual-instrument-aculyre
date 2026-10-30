# Decisions

Accepted choices and listening verdicts, newest first. A direction chosen by
ear is recorded as chosen by ear.

## 2026-10-30: each mode radiates by its own efficiency (0.7.0), adopted unheard

**What was asked.** An eight-hour pass on sound quality, authenticity,
microphones and behaviour, calibrated and benchmarked against the
recordings the owner shared (the DR Plucked Lyre Harp pack).

**What ships.**

- **Each mode's own radiation efficiency.** Below coincidence every mode had
  radiated Maidanik's average over a band (Crocker and Price, JSV 9 (1969)
  469, Eq. 38) for one coincidence frequency of the whole top. Each now
  takes Wallace's integral for its own simply supported shape in a baffle
  (JASA 51 (1972) 946), less its net volume's share, which stays a
  free-field monopole as before; the edge and corner share of a mode below
  its own coincidence keeps Maidanik's doubling for clamped edges, the form
  chosen on 2026-10-28. An orthotropic top's modes across the grain reach
  the air's wavenumber from 4.2 kHz on the spruce, so a band's modes
  radiate very differently: taken one by one they average 2.5 to 6 dB more
  from 0.6 to 4.8 kHz than the band average did. Supersonic modes are not
  capped at 1: capping them (and all) lost the 3 to 6 kHz gain and left the
  shape after 0.3 s 0.3 dB further from the recording than 0.6.5.
- **Each mode keeps its energy at 1x.** The bilinear transform, prewarped to
  a mode's frequency, keeps its peak but narrows its band by sin(2x)/2x at
  x = w/2fs: 2 dB of a 12 kHz mode's energy at 48 kHz, 3.8 dB at 16 kHz. Each
  mode's radiation is scaled so the sampled mode radiates the continuous
  one's energy below Nyquist. 10 to 16 kHz at 1x against 4x: steel -0.62,
  nylon -1.05, gut +0.24 dB (0.6.5: -1.65, -1.50, -0.91; with the first
  change alone -2.40 and -2.69 on the lyre harps, past the 2.5 dB the tests
  allow). Not the strings' loss filters (at 4x they lose more from 10 to
  16 kHz, not less) and not the bar's bends (a rigid bar keeps the gap).

**What the measurements say** (DR lyre harp, every string alone at velocity
0.7, 44.1 kHz, `Tools/compare_lyre_harp_samples.py`; band correlation and
side difference from both channels over each note's first 1.5 s):

| | 0.6.5 | 0.7.0 | recording |
| --- | --- | --- | --- |
| shape rms, 0-0.3 s / 0.3-1.5 s | 5.9 / 6.2 dB | 5.5 / 6.4 dB | |
| 1 kHz third-octave, 0-0.3 s, from the recording | -6.8 dB | -1.3 dB | |
| 5 and 6.3 kHz, 0.3-1.5 s, from the recording | -7.8, -10.9 dB | -13.6, -14.0 dB | |
| sides' difference 400-800 Hz, median note | 5.3 dB | 0.9 dB | 0.7 dB |
| sides' correlation 0.8-1.6 / 1.6-3.2 / 3.2-6.4 kHz | 0.87 / 0.91 / 0.90 | 0.73 / 0.77 / 0.51 | 0.95 / 0.64 / 0.42 |
| decay rms log2, fundamentals / partials 2-6 | 1.23 / 0.50 | 1.42 / 0.64 | |

The partials' decays by band are unchanged (the strings are), but at the
microphones a note's partial is the sum of several modes' paths, and the
reweighting moves single notes' fits: C5's and C#5's partials 2-6 now read
27 and 28 dB/s against the recording's 14 and 13 (0.6.5: 19 and 23).

**Tried and not shipped.**

- **A spaced pair that hears each mode by its shape.** Two microphones
  0.3 m apart, 0.5 m from the top, each mode's pressure there from the
  Rayleigh integral over its shape and the soundhole as a piston, its sign
  from the phase: on the symmetry line, at two offsets and at 0.4 m apart,
  1 to 6 kHz fell 4 to 14 dB further short of the recording and the shape
  went to 7.6 to 9.6 dB rms. A point in front of an ideal rectangular top
  sits in the nulls of its edge radiation, which a real top's irregular
  shape fills.
- **Another top.** Fifteen tops from 2.5 to 5 mm and 80 to 100% of the
  size, a beech top (Thomann's TLH-16 listing) and a 3 or 12 g bar: the
  fundamentals' decay rms scattered from 0.81 to 1.95 with no trend in
  either dimension, so the DR harp cannot pick a body.
- **An instant release.** 2 to 3 dB more from 4 to 10 kHz on the DR harp's
  notes, but nothing says its player plucked harder than velocity 0.7.

Also checked: the first three partials' slow fluctuation (the polarisations
beating) matches the recording's (residual 0.86, 1.5 and 1.1 dB against
0.5, 1.4 and 1.7). Roosebeck's published 16-string set (D3 to E5: .010 to
.018 plain, .024 and .028 wound) is thinner than the model's gauges where
they share notes, which would leave the treble strings further from the DR
harp's inharmonicity, not nearer; the gauges stay.

**Listening test.** Each instrument plays every string up the scale, a
slow tune and a rolled chord, at the Rack's defaults (room 100 %, piezo
50 %) and with the microphones alone; 48 kHz, 1x, matched on whole-file RMS
(trims -0.10 to +0.53 dB). In the project files
(`aculyre/listening-tests/2026-10-30-radiation/`).

| Letter | Engine |
| --- | --- |
| A | 0.6.5 as shipped (756a714) |
| B | each mode's own efficiency, touch re-evened and gains re-levelled |
| C | each mode's energy at 1x |
| D | both, as 0.7.0 ships |

**Not heard before shipping.** D ships under the owner's standing rule to
take the candidate; the owner's pick replaces it.

**What it licenses.** Each mode's radiation by Wallace's integral for its
shape, with the clamped doubling below its own coincidence, and the energy
correction at every rate. It does not license reading the microphones' seeded
signs as a placement, or the 1 kHz agreement as a measured body.

## 2026-10-30: three realism steps (0.6.5), adopted unheard

**What was asked.** One more pass of smart, audible realism improvements,
about three, benchmarked against the DR lyre harp, without costing CPU.

**What ships.**

- **The finger's release takes its slip's band.** The 0.053 ms a fingertip
  slip lasts (0.04 mm accelerating evenly to 1.5 m/s) had been used as the
  exponential's time constant, which has unloaded only 63% when the slip
  ends and passes a third of the band of a force let go over that time. A
  linear unloading over T is 3 dB down at 0.443 / T, so the exponential
  takes T / 2.78, 0.019 ms. The faster release moves velocity's effect up
  the spectrum, so the softest slip moves from 0.2 mm (the shortest of ten
  harpists' slips in music, held there because longer ones left the
  strings darker than any recording) to 0.8 mm, the middle of the 0.5 to
  1.2 mm most of their slips fell in (Chadefaux, Le Carrou and Fabre 2013,
  Table V). From velocity 64 to 127 a steel E4's 6th and 10th partials gain
  4.9 and 5.2 dB against the fundamental (0.6.4: 6.3 and 5.9).
- **The hand stops a string at a point.** Release had stopped a let-go
  string with a loss spread over the loop, fading every partial alike. The
  playing hand is now a dashpot where it last plucked, damping partial n as
  sin^2(n pi x / L), at the same rate on average over the partials; the
  partials with a node near the touch ring on briefly. Where the hand
  touches is authored. The palm (mute) and the resting hand (sympathy)
  keep the spread loss.
- **Steel's bending loss at Acustra's fitted value.** Acustra's joint refit
  to guitar recordings found 7.8e-4 (its candidate C); Acustra ships 2.3e-3,
  70% of the way to 0.003, chosen by ear on a guitar. Aculyre's plain steel
  takes C. The wound G3 keeps its own loss.

**What the measurements say** (DR lyre harp, every string alone at velocity
0.7, `Tools/compare_lyre_harp_samples.py`; 0.6.4 against 0.6.5):

| | 0.6.4 | release alone | release + loss (0.6.5) |
| --- | --- | --- | --- |
| shape rms, 0-0.3 s | 6.5 dB | 6.2 | 5.9 |
| shape rms, 0.3-1.5 s | 7.4 dB | 6.9 | 6.2 |
| 4-8 kHz partials, 0-10 ms, from the recording | -10.2 dB | | -8.4 |
| 3-10 kHz, 0.3-1.5 s, from the recording | -8.0 to -14.5 dB | | -4.6 to -10.9 |
| decay rms log2, fundamentals / partials 2-6 | 1.22 / 0.49 | | 1.23 / 0.50 |

The hand leaves the DR comparison untouched (its notes are never stopped).
On the listening phrase below, after the rolled chord is stopped, the steel
lyre harp's 1-4 kHz stands about 9 dB over 0.6.4's for half a second: the
partials the touch leaves. The CPU is unchanged: the three preset demos
render in the same time within a few percent. Tests, levels (-8.8 to -8.9
VU) and the even touch hold; the 1x top octave stays 1.0 to 1.6 dB under
4x.

**Tried and not shipped.** Both recordings show the strings from A4 up 1.9
to 3.8 times as inharmonic as the model's. Shortening them by 15% and
thickening them by 17% at the same tension matched the DR harp's B, but
the stiffer strings lost 4 to 7 dB from 3 to 10 kHz after 0.3 s, the gap
above, and B alone cannot tell length from gauge. It stays a known gap.

**Listening test.** One phrase on the steel lyre harp and the gut lyre at
the Rack's defaults (room 100 %, piezo 50 %): eight short plucks the hand
stops, a D4 at velocity 40 to 127, and a rolled G chord rung two bars and
stopped; 48 kHz, matched on whole-file RMS (every trim within 0.08 dB). In
the project files (`aculyre/listening-tests/2026-10-30-realism/`).

| Letter | Engine |
| --- | --- |
| A | 0.6.4 as shipped (5ad3d46) |
| B | the release alone, with the 0.8 mm softest slip |
| C | the hand at a point alone |
| D | steel's bending loss alone (on the gut lyre identical to A) |
| E | all three, as 0.6.5 ships |

**Not heard before shipping.** E ships under the owner's standing rule to
take the candidate; the owner's pick among A to E replaces it.

**What it licenses.** The slip's band as the release's time constant, the
0.8 mm softest slip and the touch at the pluck point as shipping behaviour
until the owner hears the set, and Acustra's fitted bending loss for plain
steel. It does not license refitting the bending loss to the DR harp, or
reading the softest slip or the hand's place as measured.

## 2026-10-29: velocity and pluck variety (0.6.2), adopted unheard

**What was asked.** Keep velocity's loudness but make the model react to
it more, and make the initial pluck softer and different on every hit.

**What the measurements said.** From velocity 64 to 127 the steel E4's 6th
and 10th partials gained only 2.5 dB against the fundamental: the measured
release law reaches the harpists' fastest slip near velocity 110, and at
medium strength the fingertip already lets go in 0.03 ms. On the
microphones the attack is not harder than the DR lyre harp's: over the
first 10 ms its partials sit 3 to 11 dB further under each note's 0-300 ms
energy than the recording's. With the Rack's new 50 % piezo the 8-16 kHz
partials of the first 10 ms stand 1.4 dB over the recording's (piezo
alone, 4.6). A note's three takes varied about as the DR harp's do: level
0.73 against 0.99 dB, brightness over 2.5 kHz 0.91 against 1.04, the comb
the pluck point leaves 3.2 against 1.5 (`compare_lyre_harp_samples.py
--takes`), so the measurement does not call for more variety; the owner
did.

**What ships.** Three changes, the last two at the owner's request:

- The release follows each pluck's drawn force through the D^1.2 law. The
  code's own reasoning (one harpist's release velocity scatters 25%, the
  displacement 14%) had left that share out.
- A softer pluck slips further, from the lab's 0.04 mm at velocity 127 to
  0.2 mm at velocity 0, the shortest of ten harpists' slips in music
  (Chadefaux, Le Carrou and Fabre 2013, Table V), geometrically with
  velocity: authored between two measured slips. Velocity 64 to 127 now
  moves those partials 6.3 and 5.9 dB; velocity 127 is unchanged.
- The slip's own scatter doubles to 40%: a note's takes now vary 1.9 dB in
  brightness, between the DR harp's 1.0 and two sampled ukuleles' 3.6 to
  4.0.

The piezo's attack is left as it is: the strip's resonance was chosen by
ear (2026-10-28) and the blend is the owner's default. Touch and gains were
re-evened and re-levelled to -9 VU. Not heard before shipping: it ships
under the owner's standing rule to take the candidate, with a listening set
(A 0.6.1, B this, C a softest slip of 0.5 mm, D this with the 20% scatter)
for the owner to hear: the steel lyre harp at the Rack's defaults (room
100 %, piezo 50 %), six A4s at velocity 90, a D4 ladder from 30 to 127 and
an arpeggio, trimmed to A's whole-file RMS by +0.02, +0.07 and +0.02 dB.
Through the room and the piezo the variety is smaller than on the engine
alone: the six A4s' first 50 ms vary 0.75 dB in brightness over 2.5 kHz
in B against 0.63 in A.

**What it licenses.** The lengthening slip and the 40% scatter as shipping
behaviour until the owner hears the set; neither is measured, and neither
licenses moving the release outside the harpists' measured slips.

## 2026-10-29: owner feedback on 0.6.0, adopted unheard

**What was asked.** The owner heard the Rack build and reported that the
picking styles had no audible impact, that the piezo had no polarity
choice, and asked for the gut lyre's range to be extended artificially to
a complete one, and for Room 100 % and Piezo 50 % by default.

**Techniques.** At velocity 90, 0.6.0's nail and plectrum (releases of
0.035 and 0.025 ms, apexes rounded over 1.2 and 0.8 mm, all authored) sat
within 2 dB of the finger in every third-octave band. A nail's or a
plectrum's hard edge has no flesh to slip along, so both now let go at
once, and the plectrum meets the string at 0.40 of the finger's distance
from the bridge, the reach Acustra fitted on picked archtop recordings
(its FittedPhysicalData.h, 2026-09-25). The finger, chosen by ear, is
unchanged. This was not heard as an A-B: it ships under the owner's
standing rule to take the candidate over the baseline. The engine tests
now hold the pick's 3rd to 6th partials 6 dB or more up on the finger's
against the fundamental (10.9 dB) and both edges' top octaves 1.5 dB or
more up (nail 2.7, pick 10.8). The nail stays near the finger: no
physical difference between them is left to draw on but the release,
and a fingertip already lets go in 0.03 ms at that velocity.

**Gut lyre.** An owner request, not a reconstruction: sixteen strings, D3
to E5 in G major, so with the levers every note from D3 to F5 plays. The
six strings keep their pitch, length and gauge; F#3 and C4 take the same
50 cm and 37 N, and from E4 up the lengths shorten to hold D4's stress.
The touch is evened again on both pickups and the gains re-levelled to
-9 VU.

**Piezo polarity and defaults.** `PlayingControls::piezoInverted` flips the
piezo's sign; the engine's default stays as wired. Room 100 % and Piezo
50 % are the Rack device's defaults, product configuration in the
adapter; the engine's own controls and its -9 VU calibration are
unchanged.

**What it licenses.** The instant release for both hard edges and the
plectrum's reach as shipping behaviour, until a listening test or a
measured lyre pluck says otherwise. It does not license reading the nail
or the plectrum as measured.

## 2026-10-29: the finger plucks at 0.27, chosen by ear

**What was decided.** Where the finger plucks by default. Two recorded
players disagree. The lyre harp phrase's player (Freesound 649925) plucks
four of its eight notes at 0.46 to 0.50 of the string from the bridge,
which set 0.47 (the entry below). DeadRobot's DR Plucked Lyre Harp plucks
each of its 16 strings three times at 0.25 to 0.32, median 0.27 over the 48
plucks ([benchmark](validation/dr-lyre-harp-benchmark-2026-10-29.md)).
Each point is defensible, so the steel lyre harp was heard at both: seven
single notes, G3 to G5 at velocity 90, then the lyre harp demo phrase, A
the shipping 0.47 and B 0.27 with the touch re-evened and the gains
re-levelled, B trimmed -0.49 dB to A's whole-file RMS, with the DR harp
playing the same seven notes beside them.

**Chosen by ear: B.** The owner judged B better. All three instruments now
pluck at 0.27, each with its touch re-evened on both pickups and its gains
re-levelled to -9 VU.

**What the measurements say.** On the DR harp's notes, each at equal power,
0.27 keeps the steel lyre harp's third-octave shape nearer the
recording's: 6.2 against 7.6 dB rms over the first 0.3 s, and 7.0 against
8.2 after it, with the partials' decays about the same (rms log2 of model
over recording 0.54 against 0.57). On the phrase, 0.47 had kept nearer
(5.3 dB rms against a quarter's 7.2). The measurements split, so the ear
chose. As shipped, with the bar's bends corrected (below), 0.27 sits 6.3
and 7.0 dB rms from the DR harp's shape and its partials 0.51.

**What it took.** At 0.27 the steel lyre harp's 10-16 kHz at 1x fell
2.62 dB under 4x, past the 2.5 dB the engine tests allow. The bar's bends
now hold their stiffness at every rate (`AculyreEngine::setInstrument`),
where the highest had erred soft below their resonance; that leaves it
2.20 dB under, and the limit is unchanged.

**What it licenses.** 0.27 as the finger's default. The nylon harp and the
gut lyre were not heard at it: they follow by the standing assumption that
they are played as the steel harp is, which a listening test of their own
could overturn. This replaces the open pick of question 9 below (a quarter,
0.35 or 0.47); 0.27 lies next to its quarter. It does not license treating
0.27 as one player's measured habit on a nylon harp or a gut lyre, or
moving the Rack device's nail and plectrum patches.

## 2026-10-29: the finger plucks near the middle of the string

**What was decided.** The default pluck point (the POSITION control) was a
quarter of the way from the bridge (0.22 on the gut lyre), taken from one
CC0 lyre harp A4 (Freesound 722897) that lacks its 4th and 8th partials.
Two other measurements put players further along: nine concert harpists
pluck at 0.33 to 0.385 of the string (Chadefaux et al., JASA 131 (2012)
844, Fig. 7), and the player of the lyre harp phrase the model is checked
against (Freesound 649925) plucks four of its eight notes at 0.46 to 0.50.
The pluck point is now 0.47 on all three instruments, the phrase's player's;
the touch is re-evened on both pickups and the gains re-levelled to -9 VU.

**What the measurements say** (eight seeds, both microphones summed):

| | quarter (0.25) | 0.47 |
| --- | --- | --- |
| phrase's long-term spectrum, 200 Hz-10 kHz, rms from the recording's shape | 7.2 dB (6.8-7.5) | 5.3 dB (5.2-5.5) |
| its 630 Hz band (the D4's and E4's second partials), over the recording's | 22 dB | 10 dB |
| 1.25-5 kHz against the recording | 0.8 dB short | 3.9 dB short |
| partials 2-10 on the phrase's five cleanest notes, mean from the recording's | 10.9 dB | 8.7 dB |
| 10-16 kHz at 1x from 4x, steel / nylon / gut | -1.85 / -1.49 / -1.02 dB | -2.25 / -1.42 / -0.60 dB |

At 0.47 the even partials a quarter kept, and the recorded player's plucks
leave out, come down: the shape matches better on every seed, and the
partials on the cleanest notes. The quarter's closer 1.25-5 kHz level had
come from those partials, so 0.47 leaves the rest of the model 3.9 dB short
there, the shortfall Known gaps puts in the assumed body, the microphones
or the recording. Before the bar's bends were matched in z (same day), 0.47
left the steel lyre harp's 10-16 kHz 4.9 dB under 4x at 1x, past the 2.5 dB
the engine tests allow; it now passes every test unchanged.

**Not yet by ear.** Three readings were defensible (question 9 on the
listening page: A the quarter, B 0.35, C 0.47). C went to main under the
owner's standing instruction to accept candidates, as the recommended
option once it passed the tests; the owner's pick is still open, and
choosing A or B replaces it. The gut lyre has no recording of its own
pluck point: it is assumed to be played as the lyre harps are.

**What it licenses.** A mid-string default for the finger. It does not
license moving the Rack device's nail and plectrum patches, which keep
their own points, or treating 0.47 as measured on a gut lyre.

## 2026-10-28: the finger plucks at a point

**What was decided.** The finger's pluck had been rounded by a Gaussian 5 mm
wide, the 2 cm a harpist's finger touches at most (Chadefaux et al., JASA 131
(2012) 844, Table V). The paper gives the 2 cm as an upper bound, finds the
low-pass it implies negligible and takes the pluck as a point
(Sec. IV.A.5); the string leaves the fingertip's edge wrapped round it only
over its kink angle, a few hundredths of a radian. The finger is now a point.
The nail's and the plectrum's authored edges stay.

**What the measurements say.**

- **Lyre harp phrase** (Freesound 649925, eight seeds): the shortfall from
  1.25 to 5 kHz falls from 1.34 to 0.96 dB at the shipping quarter pluck.
  With each note at the pluck point fitted to it, it falls from 3.7 to
  3.2 dB, and from 6.3 to 10 kHz from 5.5 to 4.0 dB.
- **Diagnostic, not shipped:** an instant release as well would still leave
  that per-note shortfall at 2.3 dB from 1.25 to 5 kHz, so most of it is not
  in the pluck.
- **Listening phrase**, RMS-matched: +0.3 dB at 2 kHz, rising to +1.4 dB
  (steel), +2.0 (nylon) and +2.4 (gut) by 10 kHz.
- **Contracts:** the even touch, the levels (within 0.1 VU of -9) and the 1x
  top octave hold unchanged; the gut lyre's 10-16 kHz at 1x comes from
  -1.78 to -1.18 dB of its 4x sound.

**Not yet by ear.** Two readings were defensible: the finger's widest
contact and the paper's point. The point went to main under the owner's
standing instruction to accept candidates and push improvements to main. The
pair is on the listening page (A the 5 mm contact, B the point); choosing A
undoes it.

**What it licenses.** A point contact for the finger. It does not license
narrowing the nail's or the plectrum's edges, which stay authored.

## 2026-10-28: the soundbox radiates by Maidanik's clamped-panel efficiency

**What was decided.** Below the top's coincidence frequency each mode's edge
and corner radiation had come from an efficiency of 0.28 sqrt(f / fc), a
coefficient nobody had sourced, stepping to 1 at coincidence. It is replaced
by Maidanik's average over a baffled plate's corner and edge modes, as
Crocker and Price correct it (JSV 9 (1969) 469, Eq. 38), doubled for
clamped edges as Maidanik gives a clamped panel (ibid., p. 484), capped at 1.

**Not by ear.** Two published forms were open: simply supported edges, and
the clamped doubling. The model already takes its plate's frequencies as
clamped (Warburton), a top glued to its box, so the clamped form is the one
consistent with it. The measurements agreed, against the simply supported
form, with the steel lyre harp phrase over eight seeds (Freesound 649925):

| | today's curve | simply supported | clamped |
| --- | --- | --- | --- |
| 1.25-5 kHz against the recording | 0.8 dB short | 1.9 dB short | 1.0 dB short |
| 6.3-10 kHz against the recording | 1.8 dB short | 1.4 dB short | 1.0 dB short |
| worst string off the register's line, steel / nylon / gut (mics) | 0.45 / 0.17 / 0.18 dB | 1.70 / 1.63 / 0.97 dB | 0.46 / 0.37 / 0.18 dB |

Through the shipping path, matched on whole-file RMS, the clamped form moves
each instrument's phrase by under 1 dB below 5 kHz and adds 2.2 to 2.7 dB
around 6.3 kHz on the lyre harps and 2.4 dB around 5 kHz on the gut lyre,
where the old curve sat up to 6 dB under it before its step. The output
gains were re-levelled to -9 VU (+0.1, +0.2 and -0.3 dB). No listening test
was run.

**What it licenses.** The published clamped-panel efficiency below
coincidence and 1 at and above it. It does not license a factor other than
Maidanik's two, or Maidanik's peak at coincidence, which an orthotropic top
spreads between its two directions' coincidences (4.2 and 15 kHz on the
spruce).

## 2026-10-28: the steel lyre harp's G3 is wound, and the 0.6.0 round is heard as a whole

**What was decided.** Two more questions on the listening page, 48 kHz
24-bit, matched on whole-file RMS:

| Question | Material | Letters | Trims on the candidates (dB) |
| --- | --- | --- | --- |
| Should the steel lyre harp's lowest string be wound? | steel, room on: G3 alone medium then hard, a G3 B3 D4 G4 arpeggio, then G3, D4 and G4 together; only G3 differs | A plain steel 0.022 inch; B nickel-wound 0.024 inch, as Roosebeck's 16-string set winds its G3, with D'Addario's unit weight for that gauge (NW024) and Acustra's wound core (0.38 mm) and losses (friction 2.25e-4, bending 0.0175) | B +0.21 |
| Is this round better as a whole? | each lyre's demo phrase as a freshly loaded instrument plays it, room on | A main (0.5.0); B the 0.6.0 branch with the six verdicts above and this round's fixes | steel +1.21, nylon -1.93, gut +0.68 |

**What the measurements said** (against A, before choosing).

- **Wound G3:** B = 1.8e-4 against 8.5e-4, so the 10th partial sits 16
  cents sharp of harmonic instead of 71 and the 20th 61 instead of 254.
  B has 9 dB less from 3 to 8 kHz over the first quarter second and, from
  0.5 s on, 7 dB less from 1 to 3 kHz and 25 dB less above 8 kHz. Its
  octave, in tune with the G4 string ringing along, holds 8 dB more at
  392 Hz from 0.5 to 2 s; A's, 3 cents sharp of G4, beats against it.
  Tension 57.6 N against 57.2, and every string's touch stays within 0.5 dB
  of the register's line by either pickup.
- **The round:** against the lyre harp recording's phrase (Freesound
  649925), the steel lyre harp's treble from 1.25 to 5 kHz falls 0.5 dB
  short in B and 8.0 dB short in A.

**Verdict, by ear.** The owner chose B on both, with no note on what still
sounds off.

**After choosing.** With G3 wound, the lyre harp phrase falls 0.9 dB short
of the recording from 1.25 to 5 kHz over the eight seeds (0.6 to 1.4),
where it fell 0.5 short with the plain G3: G3 is not played in the phrase,
but the plain string, ringing in sympathy, had filled the 1.25 kHz band
2.3 dB more than the wound one does. The recorded A4's partials read as
before (4.5 dB under over the first 45 ms, 6.7 over by 100-200 ms).

**What it licenses.** A wound G3 on the steel lyre harp with a published
unit weight and Acustra's wound-string losses and authored core. The set it
follows is for a harp tuned from D3, which winds its four lowest strings;
winding more than G3 here, or a different gauge, needs its own source. The
round's B confirms the six verdicts above heard together; it is not a
verdict on any one of this round's fixes.

## 2026-10-28: six listening verdicts for 0.6.0 (piezo, stereo, finger, body, bar, dynamics)

**What was decided.** Six open questions about the 0.6.0 sound, each heard on
its own against today's engine (A) on a listening page, one mechanism
changed per question, 48 kHz 24-bit, matched on whole-file RMS:

| Question | Material | Letters | Trims on the candidates (dB) |
| --- | --- | --- | --- |
| Piezo strip flexes under the bar? | steel, piezo alone: six plucks G3-A5, a rolled chord, a fast figure, a loud octave | A rigid force sensor; B the strip flexing, Acustra's element scaled to the bar's length | B -0.07 |
| Each string from its place along the bar? | steel, microphones: all sixteen strings up and back, G3 and A5 in turn, a rolled chord | A each mode's sign and weight drawn per microphone; B a symmetric microphone pair | B +0.17 |
| How quickly the string leaves the finger? | each lyre harp: repeated A4s, chords, an arpeggio, repeated G3s | A 0.10 ms (authored); B 0.053 ms (harder fingertip); C 0.27 ms (pad) | steel B -0.10, C +0.75; nylon B -0.10, C +0.68 |
| How long the body's modes ring? | each instrument, velocity 90: every string held a second, then a chord | A Q 40 at 300 Hz, 32 at 1 kHz, 18 at 4 kHz, 12 at 10 kHz; B 10 at 2 kHz and 9 at 10 kHz; C 23 at 300 Hz and 1 kHz; D both | B/C/D steel +0.06/+0.57/+0.72, nylon +0.04/+0.40/+0.47, gut +0.08/+0.16/+0.33 |
| Bridge bar bends or stays rigid? | each instrument: a rising scale at velocity 90, a chord | A rigid; B a free-free maple beam, seven bends on the harps, two on the gut lyre, touches re-evened | steel +2.42, nylon +2.96, gut +0.42 |
| How much tone follows velocity? | each instrument: D4 at velocity 20 to 127, soft and loud arpeggios and chords | A an authored law, less than twice as quick from softest to hardest; B release velocity as harpists' (Chadefaux et al., JASA 131 (2012) 844, Sec. III.B.1), a 25-fold range | steel -0.09, nylon -0.07, gut -0.11 |

**What the measurements said** (each against A, before choosing).

- **Piezo B:** +1.7 dB at 4 kHz, +4.9 at 6.3, +8.1 at 8, +7.0 at 10, then
  -3.2 at 12.5 and -12.7 at 16 kHz; unchanged below 3 kHz.
- **Stereo:** A leans 3.5 dB left over the file; B balances to 0.1 dB with
  G3 9 dB left and A5 8 dB right, but is thinner in mono and 2.6 dB darker
  against the lyre harp recording's treble.
- **Finger:** from 2 to 5 kHz B is 1.2-2.2 dB brighter and C 1.6-3.6 dB
  darker. On the recorded A4 (Freesound 722897) the 3rd, 5th, 6th, 7th and
  10th partials over the first 45 ms sat 14.5 dB further under the
  fundamental than the recording's with A, 11.9 with B.
- **Body:** B lowers the worst partial standing over its neighbours from
  16.6 to 14.3 dB on steel and 16.0 to 12.4 on nylon; C brings the steel A4's
  decay from 38 to 26 dB/s (a recorded A4: 30).
- **Bar:** the spread of spectral tilt across strings falls from 1.8 to
  1.5 dB on steel and 1.6 to 1.1 on nylon; the recorded A4's partials come to
  10.9 dB under (from 14.5); about twice the CPU.
- **Dynamics:** B is 2-6 dB darker above 2 kHz at velocity 30-40 and 1-2 dB
  brighter at 100-127, nearly the same at 60-80. At the softest plucks its
  slip reaches a millisecond, longer than the release model was built for.

**Verdict, by ear.** The owner chose piezo B, stereo A, finger B, body D,
bar B and dynamics B. The bar is a second bending bar. 0.3.0's (f209037)
folded the bar into the plate's modes by Rayleigh-Ritz, each string driving
them at its own point, and sounded much like the rigid bar; this one keeps
the plate's modes and adds the bar's own free bends below the top's highest
mode as junction coordinates beside heave, rock and sway.

**After combining.** The six were heard one at a time; together, with every
string's touch re-evened by each pickup and the output gains re-levelled to
-9 VU, the steel lyre harp phrase (Freesound 649925) falls 0.5 dB short of
the recording from 1.25 to 5 kHz over eight seeds (5.0 dB before), and the
recorded A4's partials sit 4.5 dB under over the first 45 ms but 6.7 dB over
by 100-200 ms. The steel lyre harp renders at 162-172 us per 64-sample block
at 1x, against 81-84 before.

**What it licenses.** The flexing piezo strip with Acustra's element
stiffness per metre of bar; per-microphone drawn signs and weights (the
symmetric pair stays out); the fingertip's 0.053 ms release and the measured
velocity law for every technique's release, within the model's 0.4-10 times
clamp; body Qs at the guitar bridge's lower quartile below 1 kHz and median
above 2 kHz; and the bending maple bar. It does not license moving the body
Qs outside that measured spread, retuning the piezo resonance by ear, or
calling the nail's and plectrum's release times measured.

## 2026-10-27: five realism changes ship, with the lyre harp phrase's 4.1-cent tuning (0.4.0)

**What was decided.** The owner asked for five audible steps towards the real
instrument, including natural randomness grounded in reality. Each of the
five rests on a published measurement or a recording:

- **Nylon and gut damping.** Woodhouse and Lynch-Aird's fit to measured harp
  strings (Acta Acustica 105 (2019) 516) replaces estimated loss values.
- **Re-pluck stop.** A finger or nail re-plucking a sounding string stops
  it first: a harpist's finger holds the string 56-431 ms (Chadefaux et al.,
  JASA 131 (2012) 844, Table II).
- **Pluck scatter.** Plucks scatter in point, force and angle as nine
  harpists' did (same paper, Fig. 7, Fig. 8 and Table VI).
- **Slip scatter.** The release time scatters by the share of the release
  velocity's spread that the displacement does not explain (Fig. 8).
- **Hand tuning.** Strings sit off their notes by the spread measured between
  the six strings of a lyre harp phrase (Freesound 649925), 4.1 cents. That
  spread is a pinning, and the kanklės tune's 1 cent is the other defensible
  one.

**What the measurements say.**

- **Nylon and gut damping:**
  - The nylon bass is darker and decays faster: G3's centroid falls from 585
    to 488 Hz, and its fundamental decays at 12.8 dB/s instead of 8.1.
  - The nylon treble is brighter: G5's centroid rises from 1336 to 1493 Hz.
  - The gut lyre is brighter: D4's centroid rises from 794 to 975 Hz.
- **Re-pluck stop:** a soft finger re-pluck of a loud A4 leaves 1.1 times a
  fresh pluck's string energy 25 ms later, against 32 times before.
- **Pluck scatter:** 2000 drawn plucks match the published spreads within 2%.
- **Lyre harp phrase:** replaying the phrase (Freesound 649925), the scatter
  and the re-pluck stop leave its 1.25-5 kHz shortfall 0.6 dB larger on
  average over eight seeds (8.0 against 7.4 dB). That is within one seed's
  range of the other.

**Listening test.** All three instruments play one phrase: repeated notes, a
chord, an octave, an arpeggio and repeated bass notes. The renders are 48 kHz
and matched on whole-file RMS. The set and its key are in the project files
(`aculyre/listening-2026-10-27-realism/`).

| Letter | Engine |
| --- | --- |
| A | 0.4.0 at 1x as shipped (117b9b5) |
| B | the re-pluck stop alone |
| C | all five, 4.1-cent tuning spread (81d56a4) |
| D | the nylon and gut damping alone (not on steel, which it leaves unchanged) |
| E | all five, every string exactly in tune |
| F | the pluck and slip scatter alone |
| G | all five, 1-cent tuning spread |

**Verdict, by ear.** The owner heard C, E, F and G as much alike, and chose
C, which already shipped. All four carry the pluck scatter. C, E and G differ
only in the tuning spread, so the ear did not separate 0, 1 and 4.1 cents on
this phrase. C keeps the spread measured on the lyre harp phrase.

**What it licenses.** All five changes as measured, with the 4.1-cent tuning
spread. It does not license tuning any spread by ear beyond measured values.

## 2026-10-27: the exponential pluck release ships; the bending bridge bar does not (0.3.0)

**What was decided.** Which of two changes aimed at the steel lyre harp
sounding too dark against recordings Aculyre keeps: the pluck's held force
unloading exponentially as the string slides off the finger (instead of a
wider Gaussian rounding the apex), and a bridge bar that bends with the top,
each string driving it at its own point (instead of one rigid bar that
averages the top's modes away above a kilohertz).

**What the measurements said.** Replaying a lyre harp phrase (Freesound
649925) note for note, the model's mean shortfall from 1.25 to 5 kHz in
1/3-octave long-term spectra was 10.0 dB for 0.2.0, 7.6 dB with the release
alone, 7.9 dB with the bar alone and 5.4 dB with both. The bar also made the
steel A4 decay faster (50 against the recording's 30 dB/s; 43 without it) and
the gut lyre (23-27 against 18 dB/s), and it doubled the CPU with every string
ringing. One unmeasured recording ranks the candidates; it does not
calibrate them.

**Listening test.** Steel lyre harp (the recording's phrase, then a sweep
over all 16 strings) and gut lyre, 48 kHz, matched on whole-file RMS:

| Letter | Engine |
| --- | --- |
| A | 0.2.0 as shipped (35e6f44) |
| B | both changes (f209037) |
| C | the bending bar alone |
| D | the exponential release alone (d3b9481) |

**Verdict, by ear.** The owner chose C or D, which sounded much alike to
them, over A and B. Between the two, D ships: it matches C's treble
measurement, keeps the decay rates closer to the recordings and costs no
CPU. The bar is kept in Git history at f209037 for when a measured lyre
body can tell whether its coupling is right.

**What it licenses.** The exponential release as the pluck model for every
technique. It does not license the release times, which stay authored.
