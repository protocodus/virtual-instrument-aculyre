# Aculyre

Aculyre is an experimental physically modelled lyre, forked in method (not in
code history) from [Acustra](https://github.com/protocodus/virtual-instrument-acustra):
a row of open strings, each a pair of stiff-string waveguides, all ending on
one bridge bar that bends with a modal soundbox, so idle strings ring in
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

- **0.3.0** (prototype, not released): the bridge bar bends with the
  soundboard, so each string drives the soundbox at its own point and the
  treble reaches the air; the pluck's held force unloads exponentially as the
  string slides off the finger. Against a lyre harp recording the steel harp's
  shortfall from 1.25 to 5 kHz falls from 10.0 to 5.4 dB. With every string
  ringing it costs about twice the CPU of 0.2.0.
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
90 (2004)). The loop carries Acustra 1.9.0's string loss laws unchanged: a
constant loss angle (dislocation damping in steel, viscoelastic loss in nylon
and gut), Valette's and Woodhouse's bending loss, and Valette's air damping.
Dispersion is a cascade of up to eight identical first-order allpass sections
whose count and coefficient are searched so the 2nd to 12th partials (the range
Acustra fits) land within 6 cents of the stiff-string law
f_n = n f0 sqrt((1 + B n^2) / (1 + B)); above the 12th the partials fall short
of the law, by about a fifth of the stretch at the 16th on the bass strings.
A second-order Thiran section sets the fractional delay, solved with the
bridge's reflection phase so the coupled string sounds its note.

**Pluck.** The released shape is a triangle rounded by the fingertip's contact
width (a harpist's finger touches at most 2 cm of string: Chadefaux et al.,
JASA 131 (2012) 844), and the held force unloads exponentially over the
release time as the string slides off, as Acustra's slip does. The wave that
reaches the bridge over the first round trip is loaded as one full period of
the loop (cell-averaged slopes, so the release is band-limited). The soundbox starts deflected under the held
force, so only the release reaches it and the top springs back. Plucks vary
a little in position, force and direction. A finger meeting a sounding string
stops most of it first. Tension modulation raises the pitch of a hard pluck
for its first moments.

**Bridge and body.** All strings end on one bridge bar glued across the top
of a modal soundbox: an orthotropic plate (Warburton's clamped-plate
frequencies, simply-supported shapes), coupled through the cavity's stiffness
to a Helmholtz air plug when the box has a soundhole. The bar is no rigid
body: the lyre harp's 24 cm, 6 g maple bar would bend freely at about 785 Hz,
and above a kilohertz the top's modes change sign along it. It is an
Euler-Bernoulli beam whose mass, tipping inertia and bending stiffness join
the plate's in the basis of the plate's own shapes (Rayleigh-Ritz), so the
combined modes carry the bar's stiffening along its line. Each string drives
them at its own point with its motion normal to the top; the motion parallel
to the top tips the bar about its foot, one port that every string shares.
At the lyre harp's middle string the bridge's mobility runs from 0.003 to
0.06 m/s/N between 1 and 5 kHz, mode by mode, where the rigid bar of 0.2.0
averaged the top's modes away to 0.002-0.007 m/s/N from 1.6 to 5 kHz. Mode
Qs are authored from the spread of Acustra's measured guitar bridge (16-75
below 1.5 kHz, 5-36 above 2 kHz): 40 at 300 Hz falling to 12 at 10 kHz, and 20
for the air mode. The junction, one port per string and the tipping, is
solved implicitly every sample, so it is passive and every string, played or
idle, is coupled to every other.

**Radiation and room.** Each mode radiates its net volume change as a
monopole, plus an equivalent volume for its edge and corner radiation, from a
radiation efficiency, with a seeded sign at each of the two microphones; the
far-field pressure at 0.5 m is rho/(4 pi r) times the volume acceleration. A
small feedback-delay room follows.

**Presets.** The steel lyre harp has 16 strings G3-A5 in C major, 0.44-0.22 m,
gauges 0.022-0.010 inch, 54-63 N, over a 20 x 30 cm spruce top on a 5 cm box
with a 6 cm soundhole: its air mode is 212 Hz and its first top modes 455 and
483 Hz.
The nylon harp uses the same frame with 1.30-0.47 mm monofilament at 29-44 N.
The gut lyre has six 50 cm strings D3 E3 G3 A3 B3 D4 at about 37 N over a
closed maple box whose first mode is 347 Hz. Dimensions are typical of each
kind of instrument, not measurements of one.

**Measured against recordings.** On the A4 of a CC0 lyre harp note (Freesound
722897), the model at the reference's quarter-length pluck point is close in
spectral centroid over the first 250 ms (482 Hz against 465 Hz), and both lack
the 4th harmonic. Its apparent inharmonicity over partials 2-12 is higher
(B = 4.8e-4 against 3.9e-4, the string's own 2.4e-4 plus the body's pull), and
it decays faster over 80-450 ms (50 against 30 dB/s; 43 in 0.2.0): A4 sits
beside the assumed body's 455 Hz top mode, which the bending bar couples to
the A4 string's point more strongly than the rigid bar did. Replaying a lyre
harp phrase note for note (Freesound 649925, CC BY), the model's 1/3-octave
long-term spectrum falls short of the recording's by 5.4 dB on average from
1.25 to 5 kHz (10.0 dB in 0.2.0). Over a CC0 Iron Age lyre recording
(Freesound 191883, another instrument and tuning), the gut lyre's notes are
about as bright (centroid 527-548 Hz against 557 Hz; 429-490 Hz in 0.2.0) but
decay faster (median 23-27 against 18 dB/s over 80-550 ms; 18-22 dB/s in
0.2.0). These are order-of-magnitude checks, not a calibration.

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
front end (the Rack adapter now, a plug-in later) plays through it.

**Cost.** The steel lyre harp (32 waveguides, 217 modes, 17 bridge ports and
the room) renders about 7 times faster than real time on one core of a cloud
container with every string ringing, half the speed of 0.2.0's rigid bar:
every mode meets every string's port each sample.

## Known gaps

- No body is measured: every soundbox is a plate-and-air model with assumed
  dimensions, so where its modes fall against the strings is a guess, and that
  sets which notes are loud, which are dead and how fast they decay.
- The nylon and gut loss angles and bending losses are estimates; only steel
  carries Acustra's fitted values. Acustra retired its own nylon voice.
- The parallel polarisation drives one port for all strings, the bar's mean
  tipping; a bar that twists along its length would let each string tip it
  differently. On the 6 g bar the body pulls the bass string's partials up
  to about 14 cents from the stiff-string law.
- The bar's dimensions and wood are assumed, and the top under it adds no
  stiffness of its own. Its free ends are its most mobile points, so the
  outer strings lose energy fastest (the steel A5's normal polarisation fades
  at about 45 dB/s); no lyre harp has been measured string by string to
  check it.
- The dispersion fit stops at the 12th partial.
- No plucking-hand model beyond a contact width and a release time, no
  buzzing, no string-to-string contact, no hand damping gestures beyond
  note-off.
- Designing an instrument is not real-time safe; the Performer designs all
  three at startup, which a front end must allow for.
- The engine never idles: every string, the body and the room run whether or
  not anything sounds, so a silent instance costs as much as a playing one.
- No plug-in yet. The Rack Extension adapter has passed its authored host
  simulation, not a Jukebox SDK build or Reason.

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
