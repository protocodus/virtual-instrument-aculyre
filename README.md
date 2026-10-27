# Aculyre

Aculyre is an experimental physically modelled lyre, forked in method (not in
code history) from [Acustra](https://github.com/protocodus/virtual-instrument-acustra):
a row of open strings, each a pair of stiff-string waveguides, all ending on
one bridge bar that stands on a modal soundbox, so idle strings ring in
sympathy. It plays three instruments: a modern 16-string steel lyre harp, the
same harp strung in nylon, and a six-string gut lyre after the Anglo-Saxon
reconstructions. It is a DSP-only feasibility prototype: there is no plug-in,
no interface and no release.

## Audio demos

No audio is committed. `Tools/render_demo_set.sh <build> <out>` renders the
listening set from the phrases `Tools/write_preset_midi.py` writes (committed in
`Docs/audio/midi/`): one demo per
preset, single notes with and without sympathetic strings, a glissando, a
dynamics ladder on A4, one phrase on all three presets, and the A4 of the
reference recording below.

## Version history

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
width and the release time, and the wave that reaches the bridge over the first
round trip is loaded as one full period of the loop (cell-averaged slopes, so
the release is band-limited). The soundbox starts deflected under the held
force, so only the release reaches it and the top springs back. Plucks vary
a little in position, force and direction. A finger meeting a sounding string
stops most of it first. Tension modulation raises the pitch of a hard pluck
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
radiation efficiency, with a seeded sign at each of the two microphones; the
far-field pressure at 0.5 m is rho/(4 pi r) times the volume acceleration. A
small feedback-delay room follows.

**Presets.** The steel lyre harp has 16 strings G3-A5 in C major, 0.44-0.22 m,
gauges 0.022-0.010 inch, 54-63 N, over a 20 x 30 cm spruce top on a 5 cm box
with a 6 cm soundhole: its air mode is 212 Hz and its first top mode 467 Hz.
The nylon harp uses the same frame with 1.30-0.47 mm monofilament at 29-44 N.
The gut lyre has six 50 cm strings D3 E3 G3 A3 B3 D4 at about 37 N over a
closed maple box whose first mode is 343 Hz. Dimensions are typical of each
kind of instrument, not measurements of one.

**Measured against recordings.** On the A4 of a CC0 lyre harp note (Freesound
722897), the model at the reference's quarter-length pluck point matches the
spectral centroid of the first 250 ms (473 Hz against 465 Hz) and the
apparent inharmonicity of partials 2-12 (B = 3.84e-4 against 3.93e-4, the
string's own 2.36e-4 plus the body's pull), and both lack the 4th harmonic. The
model's note decays faster over 80-450 ms (43 against 30 dB/s): A4 sits beside
the assumed body's 467 Hz top mode. Over a CC0 Iron Age lyre recording
(Freesound 191883, another instrument and tuning), the gut lyre's notes decay
at a similar rate (median 16.5 against 17.8 dB/s over 80-550 ms) and are a
little darker (centroid 503 against 557 Hz). These are order-of-magnitude
checks, not a calibration.

**Cost.** The steel lyre harp (32 waveguides, 234 modes and the room) renders
about 12 times faster than real time on one core of a cloud container.

## Known gaps

- No body is measured: every soundbox is a plate-and-air model with assumed
  dimensions, so where its modes fall against the strings is a guess, and that
  sets which notes are loud, which are dead and how fast they decay.
- The nylon and gut loss angles and bending losses are estimates; only steel
  carries Acustra's fitted values. Acustra retired its own nylon voice.
- The sway coupling of the parallel polarisation through the bar's height is
  simple geometry; with the 6 g bar it moves some partials by up to about
  11 cents.
- The dispersion fit stops at the 12th partial.
- No plucking-hand model beyond a contact width and a release time, no
  buzzing, no string-to-string contact, no hand damping gestures beyond
  note-off.
- Setup is not real-time safe; the engine is not yet a plug-in.

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
