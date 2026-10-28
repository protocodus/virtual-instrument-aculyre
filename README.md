# Aculyre

Aculyre is an experimental physically modelled lyre, forked in method (not in
code history) from [Acustra](https://github.com/protocodus/virtual-instrument-acustra):
a row of open strings, each a pair of stiff-string waveguides, all ending on
one bridge bar that stands on a modal soundbox, so idle strings ring in
sympathy. It plays three instruments: a modern 16-string steel lyre harp, the
same harp strung in nylon, and a six-string gut lyre after the Anglo-Saxon
reconstructions. It is a feasibility prototype: there is no plug-in and no
release. A Reason Rack Extension adapter plays it from the private
`protocodus/reason-rack-extensions` repository (source only, not yet built
against the SDK).

## Audio demos

No audio is committed. `Tools/render_demo_set.sh <build> <out>` renders the
listening set from the phrases `Tools/write_preset_midi.py` writes (committed in
`Docs/audio/midi/`): one demo per
preset, single notes with and without sympathetic strings, a glissando, a
dynamics ladder on A4, one phrase on all three presets, and the A4 of the
reference recording below.

## Version history

- **0.6.0** (prototype, not released): the piezo strip flexes under the
  bridge bar, so the piezo rings up near 9 kHz (6.5 kHz on the gut lyre) and
  rolls off above, as an under-saddle pickup does. The microphones hear the
  soundbox as a symmetric pair does, so each string sounds from its place
  along the bridge bar, bass to the left, instead of every string sitting
  slightly left. A hard pluck on a steel bass string decays as a soft one
  does: the tension glide's energy no longer pumps the string, which held
  it ringing and sharp for seconds. The bass strings' upper partials follow
  the stiff-string law to 5 kHz, where they had fallen up to 95 cents flat.
  A finger re-plucking a ringing string stops it without a click. Pitch
  bends stay in tune over their full twelve semitones, where an octave had
  fallen up to 59 cents flat. The room's early reflections keep their timing
  whenever the strings run above 96 kHz (4x on a 44.1 or 48 kHz host), where
  they had arrived after 0.6 to 2.5 ms instead of 7.3.
- **0.5.0** (prototype, not released): a piezo pickup under the bridge bar,
  blended with the microphones from none to the piezo alone. Levels follow
  Reason's VU meter: typical playing reads about -9 VU on every instrument,
  on the microphones and the piezo alike, and a new output stage keeps the
  hardest chords out of the red and every sample under -1.5 dBFS. The output
  is 2 ms later for the stage's look-ahead.
- **0.4.0** (prototype, not released): the instruments run at 1x, 2x or 4x
  the host rate (1x by default). Nylon and gut strings are damped as
  measured harp strings are. Plucks scatter in point, force, angle and slip
  as harpists' do. A finger stops a sounding string before re-plucking it.
  Each string is tuned by hand, a few cents off its note. The soundbox runs
  about 30% faster, and a silent instance sleeps.
- **0.3.0** (prototype, not released): the pluck's held force unloads
  exponentially as the string slides off the finger, so the steel lyre harp's
  treble comes 2.4 dB closer to a lyre harp recording from 1.25 to 5 kHz.
- **0.2.0** (prototype, not released): playing controls that change while
  the instrument sounds (technique, pluck position and angle, how long a
  let-go string rings, a palm mute, sympathetic strings, room, stereo width,
  volume, pitch bend), sharping levers that follow the notes played, a
  Performer that switches between the three instruments with a 20 ms
  crossfade, and each string's activity for a front end to draw.
- **0.1.0** (prototype, not released): the engine, three presets, the demo
  renderer and the engine contracts.

## Downloads

None: this is a source-only prototype.

---

## How it works

**Strings.** Each string is two digital waveguides, one per polarisation, each
holding the round trip from the bridge to the yoke and back. The plane parallel
to the soundboard is longer by one string diameter (Woodhouse, Acta Acustica
90 (2004)). Steel carries Acustra 1.9.0's string loss laws unchanged: a
constant loss angle for dislocation damping, Valette's and Woodhouse's bending
loss, and Valette's air damping. Nylon and gut carry Woodhouse and
Lynch-Aird's fit to measured harp strings (Acta Acustica 105 (2019) 516):
nylon stiffens under tension, bending loses by each material's loss modulus,
and air damping rises with the square root of frequency, with no constant
loss term.
Dispersion is a cascade of up to eight identical first-order allpass sections
whose count and coefficient are searched so the 2nd to 12th partials (the range
Acustra fits) land within 6.3 cents of the stiff-string law
f_n = n f0 sqrt((1 + B n^2) / (1 + B)). Where partials past the 12th fall below
5 kHz (strings under about 417 Hz), two second-order sections are fitted with
the first-order coefficient to all of them, by least squares in cents: the
steel and nylon G3 land within 6.3 cents up to their 21st partial at 48 kHz
(8 at 192 kHz), which first-order sections alone left up to 95 cents flat.
A second-order Thiran section sets the fractional delay, solved with the
bridge's reflection phase so the coupled string sounds its note. The strings
are tuned by hand: each instrument's sit a few cents off their notes, the
same ones every time, spread by 4.1 cents about the instrument's pitch as the
six strings of a lyre harp phrase are (Freesound 649925).

**Pluck.** The released shape is a triangle rounded by the fingertip's contact
width (a harpist's finger touches at most 2 cm of string: Chadefaux et al.,
JASA 131 (2012) 844), and the held force unloads exponentially over the
release time as the string slides off, as Acustra's slip does. The wave that
reaches the bridge over the first round trip is loaded as one full period of
the loop (cell-averaged slopes, so the release is band-limited). The soundbox starts deflected under the held
force, so only the release reaches it and the top springs back. Plucks
scatter as harpists' do (Chadefaux et al.): the pluck point by 0.012 of the
length on a short string to 0.022 on a long one, the force by 14%, the
angle the string leaves at by 4 degrees and the release time, so the
brightness, by 20%, each a normal spread about the control. A finger meeting a sounding string
stops it first, as a harpist's finger holds the string 56 to 431 ms before
letting go (Chadefaux et al., Table II); a plectrum glances off and stops most
of it. A finger's contact silences a string in about 10 ms (Laurson et al.,
CMJ 25(3) 2001), slow against the body's periods, so the body takes the
force the stopped wave leaves as it takes the held force, without the click
of a step. Tension modulation raises the pitch of a hard pluck
for its first moments.

**Bridge and body.** All strings end on one rigid bar with three degrees of
freedom: heave and rock carry the strings' motion normal to the top, sway the
motion parallel to it, through the bar's height. The bar stands on a modal
soundbox: an orthotropic plate (Warburton's clamped-plate frequencies,
simply-supported shapes for the bar's participation), coupled through the
cavity's stiffness to a Helmholtz air plug when the box has a soundhole. Mode
Qs are authored from the spread of Acustra's measured guitar bridge (16-75
below 1.5 kHz, 5-36 above 2 kHz): 40 at 300 Hz falling to 12 at 10 kHz, and 20
for the air mode. The junction is solved implicitly every sample, so it
is passive and every string, played or idle, is coupled to every other.

**Radiation and room.** Each mode radiates its net volume change as a
monopole, plus an equivalent volume for its edge and corner radiation, from a
radiation efficiency. The two microphones stand symmetrically about the top's
long axis, so a mode antisymmetric across the width reaches them in opposite
phase and a symmetric one alike, with a seeded sign for which way its lobe
faces them: each string sounds from where it sits along the bar, the steel
lyre harp's G3 9 dB to the left and its A5 8 dB to the right. The far-field
pressure at 0.5 m is rho/(4 pi r) times the volume acceleration. A
small feedback-delay room follows.

**Piezo.** A pickup strip under the bridge bar presses with the bar's whole
force on the top, so it hears the bar's heave and not its rock, and it hears no
room. Its element, cable, preamp and DI are Aculele's, which are Acustra's
documented chain: an element of 0.2 V/N at 1.45 nF, the ESP Project 202 preamp
and a 1 MOhm DI. At a lyre's bridge forces that chain stays linear. A
glissando at full velocity presses at most 7.6 N on any factory patch, and the
preamp's op-amps limit at about 13 N. So the chain reduces to its 31 Hz input
coupling. The strip itself is not rigid: the bar's mass rides on the
element's stiffness, Acustra's element (a 3.8 g saddle on it rings at 6 kHz)
taken per metre of its length along the whole bar. On the lyre harp that
resonance sits at 8.7 kHz, damped by the top under the bar, lifting the piezo
up to 9 dB there and falling 11 dB short at 20 kHz; on the gut lyre's shorter
bar, 6.5 kHz, 5 dB up and 13 dB down at 20 kHz. `PlayingControls::piezo` blends from the microphones to the piezo
alone. The element is wired so that a blend adds in the low mids instead of
cancelling there; half and half is about 2 dB quieter than either alone on
the steel lyre harp phrase. Against the microphones, the piezo carries more of
the strings' fundamentals and of the top two octaves.

**Presets.** The steel lyre harp has 16 strings G3-A5 in C major, 0.44-0.22 m,
gauges 0.022-0.010 inch, 54-63 N, over a 20 x 30 cm spruce top on a 5 cm box
with a 6 cm soundhole: its air mode is 212 Hz and its first top mode 467 Hz.
The nylon harp uses the same frame with 1.30-0.47 mm monofilament at 29-44 N.
The gut lyre has six 50 cm strings D3 E3 G3 A3 B3 D4 at about 37 N over a
closed maple box whose first mode is 343 Hz. Dimensions are typical of each
kind of instrument, not measurements of one.

**Measured against recordings** (0.3.0 unless noted). On the A4 of a CC0
lyre harp note (Freesound 722897), the model at the reference's quarter-length pluck point is close in
spectral centroid over the first 250 ms (482 Hz against 465 Hz) and in the
apparent inharmonicity of partials 2-12 (B = 3.86e-4 against 3.93e-4, the
string's own 2.36e-4 plus the body's pull), and both lack the 4th harmonic.
The model's note decays faster over 80-450 ms (43 against 30 dB/s): A4 sits
beside the assumed body's 467 Hz top mode. Replaying a lyre harp phrase note
for note (Freesound 649925, CC BY), the model's 1/3-octave long-term spectrum
falls short of the recording's by 7.6 dB on average from 1.25 to 5 kHz
(10.0 dB in 0.2.0). With 0.4.0's scattered plucks the shortfall depends on
the draws: 8.0 dB on average over eight seeds (7.1-8.9), against 7.4
(7.1-7.9) for 0.3.0's authored spreads. Over a CC0 Iron Age lyre recording (Freesound 191883,
another instrument and tuning), the gut lyre's notes decay at a similar rate
(median 18-22 against 18 dB/s over 80-550 ms) and are a little darker
(centroid 448-496 Hz against 557 Hz); 0.4.0's measured gut damping
brightens them (D4's centroid 794 to 975 Hz over its first 250 ms), not yet
compared with that recording again. These are order-of-magnitude checks, not
a calibration.

**Playing.** `PlayingControls` holds what a player and the room change while
the instrument sounds; the plucking fields apply at the next pluck, the rest at
once. Release sets how a let-go string sounds on, from the playing hand
stopping it (a 30 ms amplitude time constant) to ringing until it dies away;
mute rests a palm across every string; sympathy rests a hand on the strings
nobody plucked. With the sharping levers on, a note a semitone above a
string's own raises that string's lever, which shortens its speaking length
by a semitone, and the string's own note lowers it again, as a lyre harp
player sets the levers by hand. `AculyrePerformer` designs all three
instruments once, outside real time, so choosing one is a real-time switch:
the old instrument fades out over 20 ms while the new one starts, and every
front end (the Rack adapter now, a plug-in later) plays through it. The
Performer can run the instruments at 2x or 4x the host rate (at most
192 kHz) and bring them down through a linear-phase Kaiser-windowed sinc,
31 host samples late. Replaying the lyre harp phrase dry, 4x moves the
long-term spectrum within about 1 dB of 1x below 6 kHz (2.4 dB at
1.25 kHz) and lifts the top octave 4-11 dB; 2x lands within 0.5 dB of 4x
below 12.5 kHz and 2 dB short at 20 kHz.

**Level.** Levels are set on Reason's VU meter, which reads the RMS over
300 ms with 0 VU at -12 dBFS (the Big Meter's default offset). `outputGain`
puts each instrument's typical playing, each string alone and three- and
four-note chords at velocities 70-110, at a median of -9 VU at its own
controls; `piezoGain` puts the piezo alone within 1 dB of that. Unchecked,
the hardest playing, chords of up to six strings at full velocity with a
plectrum, would read up to +8 VU and peak at +10 dBFS. So the Performer
ends in the portfolio's shared output stage (`OutputStage.h`, the same in
every new instrument) with its VU guard: a compressor on the louder
channel's power with ballistics near the meter's (80 ms attack, 300 ms
release, ratio 10, knee centred at -2 VU), then a peak limiter that looks
2 ms ahead and holds every sample under -1.5 dBFS. Single notes pass it
untouched, typical chords read 0.1-1.4 dB lower, mostly from their attacks,
and the hardest chords read -1 VU or less. The look-ahead delays the output
by 2 ms rounded down to 4 samples (88 at 44.1 kHz, 96 at 48 kHz).

**Cost.** The steel lyre harp (32 waveguides, 234 modes and the room),
plucked every half second, renders about 12 times faster than real time on
one core of a cloud container at 1x (104-110 us per 64-sample block at
48 kHz, GCC -O3) and 3 times at 4x. The modes run in one pass a compiler can vectorize. Once
everything has died away below -170 dBFS for half a second the engine
sleeps, and a sleeping instance costs next to nothing until the next pluck.

## Known gaps

- No body is measured: every soundbox is a plate-and-air model with assumed
  dimensions, so where its modes fall against the strings is a guess, and that
  sets which notes are loud, which are dead and how fast they decay.
- Nylon and gut damping follow one published fit to measured harp strings,
  not strings from these instruments; gut varied most from string to string
  in that study.
- The bridge bar is rigid, so above a kilohertz it averages the top's modes
  away and the steel lyre harp stays 7.6 dB darker than a lyre harp recording
  from 1.25 to 5 kHz. A bar that bends with the top (commit f209037) closed
  more of that gap, but by ear it sounded much like the release change alone
  and it doubled the CPU ([decisions](Docs/decisions.md)).
- The sway coupling of the parallel polarisation through the bar's height is
  simple geometry; with the 6 g bar it moves some partials by up to about
  11 cents.
- The dispersion fit stops at 5 kHz, and two second-order sections cannot
  follow all 24 of the gut lyre's D3 partials below it: the worst lands 25
  cents from the law (9 on E3).
- No plucking-hand model beyond a contact width and a release time, no
  buzzing, no string-to-string contact, no hand damping gestures beyond
  note-off.
- The piezo's element stiffness is Acustra's, scaled to the bar's length,
  and its preamp is taken as linear, with no overload at settings past the
  factory patches. The pickup system is a guitar's; no lyre pickup is
  measured.
- Designing an instrument is not real-time safe; the Performer designs all
  three at startup, which a front end must allow for.
- No plug-in yet. The Rack Extension's 0.4.0d1 has been played in Recon
  from a local build; it has not been through Reason Studios' acceptance.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
Tools/render_demo_set.sh build <output-directory>
build/RenderAculyreDemo --report 0
```

`AculyreDSPCxx17` builds the engine as C++17 without exceptions, as the Rack
toolchain would. `Aculyre.Engine` checks tuning, the bass string's partials,
the pluck point's missing harmonic, stability, determinism, block-size
invariance and sympathetic coupling; `Aculyre.DemoMidiSources` checks that the
committed phrases are what `Tools/write_preset_midi.py` writes.

## Licensing

Aculyre's source is available under the [PolyForm Noncommercial License
1.0.0](LICENSE). The string loss sections and the Thiran design are copied from
Acustra, under the same licence and owner. The body's mode Q statistics are
summarised from Acustra's measured bridge, adapted from Robert Mores' archive
for the acoustical documentation of guitars (CC BY 4.0). The comparison
recordings, Freesound 722897 ("Lyre harp single note", Biggertigger2006) and
191883 ("Lyre.flac", Hedmarking), are CC0 1.0 and are not stored here.
