# Aculyre

Aculyre is an experimental physically modelled lyre, forked in method (not in
code history) from [Acustra](https://github.com/protocodus/virtual-instrument-acustra):
a row of open strings, each a pair of stiff-string waveguides, all ending on
one bridge bar that stands on a modal soundbox, so idle strings ring in
sympathy. It plays three instruments: a modern 16-string steel lyre harp, the
same harp strung in nylon, and a gut lyre after the Anglo-Saxon
reconstructions, extended from their six strings to sixteen. It is a feasibility prototype: there is no plug-in and no
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

- **0.7.4** (prototype, not released): every MIDI note sounds. A note
  above or below the strings plays the string an octave or more away that
  sounds its pitch class (C2 plays the lyre harp's C4), where it was
  silent; with the levers off, notes between the strings stay silent.
- **0.7.3** (prototype, not released): the gut lyre is plucked at 0.11 of
  the string's length, where it was 0.27, the lyre harp's point. Its
  recording (Freesound 191883), replayed note by note, puts its 2nd to 12th
  partials 3.5 dB rms from the model's at 0.11 against 10.4 at 0.27: the
  4th partial, which the old point all but cancelled, comes back by 19 dB,
  and the gut lyre sounds brighter and more open. Its strings are re-evened
  and the preset re-levelled to -9 VU; the lyre harps do not change.
- **0.7.2** (prototype, not released): the lyre harps' spruce top is 6 mm
  thick, where it was 3. Over four draws of the top's irregularities, the
  DR lyre harp's notes fit it best among 3 to 8 mm: the third-octave shape
  comes from 5.6 / 6.7 to 4.6 / 5.2 dB rms (first 0.3 s / after) and the
  fundamentals' decays from 1.41 to 1.07 (log2 rms). The top's first mode
  moves from 467 to 835 Hz, so the boxy 400 Hz is gone (-17 dB on a steel
  phrase) and 0.8 to 4 kHz comes forward (+4 to +12 dB); the bridge's
  admittance falls into a lever harp's measured range. The heavier top
  radiates 8 to 10 dB less, made up in the lyre harps' gains, and has half
  the modes below the limit, so the lyre harps render a little faster. The
  piezo's even touch is re-measured with the pickup blend settled (it had
  been measured while the blend still glided from the microphones, which
  left the gut lyre's piezo up to 3.5 dB uneven); the gut lyre's piezo is
  1.5 dB louder to match.
- **0.7.1** (prototype, not released): the two microphones are a 17 cm
  pair. Each soundbox mode's sign at the two had been drawn apart, so from
  0.8 to 1.6 kHz the lyre harp's sides correlated 0.73 (and from -0.3 to
  0.7 with other draws of the body) where the DR lyre harp's read 0.95;
  now they are alike as often as a pair that close hears the top as one
  source, and the sides correlate 1.00, 0.98, 0.64 and 0.44 by octave from
  0.8 to 12.8 kHz (the recording 0.95, 0.64, 0.42 and 0.19), and differ by
  0.3 to 2.1 dB (1.6 to 2.0; 0.9 to 3.6 before). The image is steadier and
  narrower in the mids; WIDTH widens it. Against the DR harp the shape
  over the first 0.3 s comes 0.4 dB rms closer (5.1) and 0.2 dB further
  after (6.6). The lyre harps' touch is re-evened and their gains
  re-levelled to -9 VU; the CPU is unchanged.
- **0.7.0** (prototype, not released): each soundbox mode radiates by its
  own efficiency, worked out for its own shape, where a band average for the
  whole top had stood in. The spruce top's modes across the grain radiate
  far better than that average said, so the lyre harps gain presence around
  1 kHz: on the microphones a steel phrase stands 5.4 dB higher at 1 kHz,
  2.2 lower at 2 kHz and 1.7 higher at 4 kHz, the gut lyre 2 dB higher at
  500 Hz and 2 lower at 1 kHz. Against the DR lyre harp's notes the 1 kHz
  third-octave over the first 0.3 s comes from 6.8 to 1.3 dB short and the
  shape over that time 0.4 dB rms closer (5.5); after 0.3 s it is 0.2 dB
  further (6.4), as the 5 to 6.3 kHz bands fall 3 to 6 dB further short.
  The low modes, which radiate mostly their net volume, now sound alike at
  both microphones: from 400 to 800 Hz the two sides differ by 0.9 dB on
  the median note where they differed by 5.3 (the recording 0.7), and from
  1.6 to 6.4 kHz the sides correlate 0.51 to 0.77 where they did 0.90 (the
  recording 0.42 to 0.64). At 1x, the back panel's default, each mode keeps
  the energy the sampled design's narrowed band had cost it near Nyquist,
  so the top octave sits within 1.1 dB of the 4x sound (1.65 before). The
  touch is re-evened and the gains re-levelled to -9 VU; the CPU is
  unchanged, and designing an instrument takes about 10% longer.
- **0.6.5** (prototype, not released): three steps towards the sampled lyre
  harp. The finger lets go of the string as fast as its measured slip says:
  the release had used the slip's whole length as its time constant, which
  leaves a third of the band, so plucks are brighter in their first
  moments, about 2 dB more from 4 to 8 kHz over the first 40 ms on the DR
  lyre harp's notes. A soft
  pluck slips further than before (0.8 mm at the softest, inside harpists'
  measured slips), so velocity still moves the tone: a steel E4's 6th and
  10th partials gain 4.9 and 5.2 dB from velocity 64 to 127. The steel
  strings lose less to bending, at Acustra's fitted value rather than its
  guitar's by-ear one, so their treble rings longer. With the release,
  from 0.3 to 1.5 s, 3 to 10 kHz stands 3.4 to 4.2 dB higher on the DR
  harp's notes, which leaves it 4.6 to 10.9 dB short of the recording. The hand stops a string by
  touching it where it plucked, so the partials with a node there ring on
  briefly as a real touch leaves them, instead of every partial fading
  alike. Against the DR lyre harp the third-octave shape comes 0.6 and
  1.2 dB rms closer over the first 0.3 s and from 0.3 to 1.5 s (5.9 and
  6.2 against 6.5 and 7.4); the CPU is unchanged.
- **0.6.4** (prototype, not released): the engine sleeps once its tail is
  below -140 dBFS for half a second, under a 24-bit output's last bit,
  instead of -170 dBFS: a ringing steel chord stops costing CPU about 9 s
  sooner (28 s after release instead of 37). Against 0.6.3 the output
  differs only in that tail, by at most -143 dBFS on every preset.
- **0.6.3** (prototype, not released): the same sound for about half the
  CPU. Every change is bit-identical on every preset, phrase and
  oversampling factor. The three preset demos at 48 kHz, built as the Rack
  SDK builds, fell from 47% to 24% of one core of a cloud container summed
  (the 4x nylon demo from 68% to 39%): the strings' filters run stage by
  stage across all 32 waveguides, and the modes, the filters and the bridge
  solve run two lanes per instruction with explicit vector types, which the
  SDK compiler keeps.
- **0.6.2** (prototype, not released): velocity moves the tone as well as
  the level, and each pluck sounds a little different. A softer pluck
  slips further along the finger before it lets go, from the lab
  fingertip's 0.04 mm at the hardest towards 0.2 mm, the shortest slip of
  harpists playing music, so medium plucks are softer in their attack and
  the hardest keep their snap: from velocity 64 to 127 a steel E4's 6th and
  10th partials gain 6 dB against the fundamental, where they gained 2.5.
  Each pluck's slip follows its own drawn force and scatters twice as widely
  as before, so a note played again changes brightness by 1.9 dB from pluck
  to pluck, where a sampled lyre harp's takes change by 1.0. Velocity's
  loudness holds within 0.3 dB from velocity 64 up; softer plucks read up
  to 2.3 dB quieter, as a slower release leaves less of the fundamental.
- **0.6.1** (prototype, not released): the techniques sound apart. A nail
  or a plectrum lets go of the string at once, and a plectrum meets it at
  0.40 of the finger's distance from the bridge, as Acustra's fitted
  plectrum does: at velocity 90 the pick's 3rd to 6th partials stand 11 dB
  higher on the fundamental than the finger's and its top octaves 11 dB,
  the nail's top octaves 2.7 dB (in 0.6.0 all within 1.2 dB). The gut lyre
  has sixteen strings, D3 to E5 in G major, so with the sharping levers
  every note from D3 to F5 plays. The six it had keep their pitch and
  length; the touch, evened again over all sixteen, gives the full force to
  the weakest string, D5, so the six are plucked at 0.4 to 0.6 of their
  0.6.0 force (their hard plucks glide less in pitch) and the output gain
  rises 3.4 dB to keep typical playing at -9 VU. A new `piezoInverted`
  control wires the piezo the other way round.
- **0.6.0** (prototype, not released): the piezo strip flexes under the
  bridge bar, so the piezo rings up near 9 kHz (6.5 kHz on the gut lyre) and
  rolls off above, as an under-saddle pickup does. The bridge bar bends
  with the top, so the strings hear the top's upper modes and their colour
  runs more evenly from string to string, at 1.4 to 1.8 times the CPU. The
  fingertip slips off the string as quickly as harpists' were measured to,
  quicker for a hard pluck and slower for a soft one, so soft playing is
  darker and hard playing brighter. Plucked a quarter of the way along, as
  0.5.0 was, the two with the rest of 0.6.0 bring the lyre harp's treble
  from 1.25 to 5 kHz to about 1 dB of a recording's on average, where 0.5.0
  fell 8.0 dB short. The finger now plucks 0.27 of the string's length
  from the bridge (0.5.0 plucked at a quarter, 0.22 on the gut lyre), where
  the player of a sampled lyre harp plucks, chosen by ear over 0.47, where
  the lyre harp phrase's player plucks half its notes. On the sampled harp's
  notes the steel lyre harp keeps nearer the recording's shape (6.3 dB
  rms over the third-octaves from 200 Hz to 10 kHz in the first 0.3 s,
  against 7.6 at 0.47).
  The steel lyre
  harp's lowest string, G3, is nickel-wound, as string sets for these harps
  wind it: far less inharmonic than the plain wire it replaces, darker, and
  its octave rings in tune with the G4 string. The body's modes
  are more damped, taking the edge off a ring near 2 kHz and evening the
  sustain of the strings beside the main mode. A hard pluck on a steel bass string decays as a soft one
  does: the tension glide's energy no longer pumps the string, which held
  it ringing and sharp for seconds. The bass strings' upper partials follow
  the stiff-string law to 5 kHz at every host rate, where they had fallen up
  to 95 cents flat,
  and the treble strings' to 12 kHz at 1x, held bends included, where they
  had run up to 49 cents sharp and only 4x had kept them in tune.
  At 1x the top octave keeps more of its air: from 10 to 16 kHz each
  instrument sits 1.1 to 2.2 dB below its 4x sound, where 0.5.0's fell 3.1
  to 3.6 dB below. The soundbox radiates its upper modes by a published plate
  formula instead of an authored curve: up to 3 dB more air around 6 kHz
  (5 kHz on the gut lyre), where the old curve dipped before a step, and
  under 1 dB less from 1 to 3 kHz. The finger plucks at a point, as
  harp-plucking measurements take it, where the pluck had been rounded
  over the widest a fingertip touches: 1 to 2.4 dB more air from 5 to
  12 kHz. A finger re-plucking a ringing string
  stops it without a click. Pitch
  bends stay in tune over their full twelve semitones, where an octave had
  fallen up to 59 cents flat. The room's early reflections keep their timing
  whenever the strings run above 96 kHz (4x on a 44.1 or 48 kHz host), where
  they had arrived after 0.6 to 2.5 ms instead of 7.3. A held key or pedal
  no longer keeps a silent instrument running: once its strings have died
  away it sleeps, as it does after they are released. Every string is
  plucked with an even touch, so the level runs smoothly from bass to
  treble: single strings had stood up to 11.5 dB out of line (the nylon
  A4), and the level spread across the strings at one velocity falls from
  13 to 16 dB to 3.5 to 9, the register's own slope. The piezo, which hears
  the bridge rather than the soundbox, keeps its own even touch, and the
  PIEZO blend moves between the two. The room's tail is smooth from its
  start instead of fluttering with separate echoes after each pluck.
  Harder chords read louder all the way to full velocity, where the output
  stage had held them level or even quieter.
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
loss, and Valette's air damping. A wound steel string takes its maker's
unit weight for its mass and a smaller core for its stiffness, with
Acustra's wound losses: friction between the turns, a constant loss angle
on the share of each partial's energy its tension carries, and a bending
loss of its own (after Paté, Le Carrou and Fabre, JASA 135 (2014) 3045).
Nylon and gut carry Woodhouse and
Lynch-Aird's fit to measured harp strings (Acta Acustica 105 (2019) 516):
nylon stiffens under tension, bending loses by each material's loss modulus,
and air damping rises with the square root of frequency, with no constant
loss term.
Dispersion is a cascade of up to eight identical first-order allpass sections
whose count and coefficient are searched so the 2nd to 12th partials (the range
Acustra fits) land within 6.3 cents of the stiff-string law
f_n = n f0 sqrt((1 + B n^2) / (1 + B)). Where partials past the 12th fall below
5 kHz (strings under about 417 Hz), second-order sections are fitted with
the first-order coefficient to all of them, by least squares in cents: two,
and a third and a fourth, each fit resuming from the last, while a partial
lands more than 5 cents off, and where four still leave one so, again over
eight first-order sections. The first-order coefficient stays negative,
since a positive one flattens the upper partials a stiff string sharpens.
Every string of the three instruments, lever up or down, lands within 4.2
cents up to 5 kHz at every rate from 44.1 to 192 kHz, no more than 0.6
cents further out than at 48 kHz. First-order sections alone left the steel
G3 up to 95 cents flat, and two sections the gut D3 25 cents off.
A second-order Thiran section sets the fractional delay, solved with the
bridge's reflection phase so the coupled string sounds its note. The strings
are tuned by hand: each instrument's sit a few cents off their notes, the
same ones every time, spread by 4.1 cents about the instrument's pitch as the
six strings of a lyre harp phrase are (Freesound 649925).

**Pluck.** The released shape is a triangle. A finger meets the string at a
point: a harpist's finger touches at most 2 cm of string, too little to
filter it (Chadefaux et al., JASA 131 (2012) 844, who take the pluck as a
point), and the string leaves the fingertip's edge wrapped round it only
over its kink angle, a few hundredths of a radian. A nail's or a plectrum's
hard edge has no flesh for the string to slip along, so it lets go at once,
the ideal pluck, and a plectrum meets the string nearer the bridge than the
hand's place: at 0.40 of the finger's distance, Acustra's plectrum fitted on
picked archtop recordings. A finger's held force unloads exponentially over
the release time as the string slides off, as Acustra's slip does. A harpist's
string slips 0.04 mm along the fingertip and leaves at about 1.5 m/s
(Chadefaux, Le Carrou and Fabre, JASA 133 (2013) 2444), a slip lasting
0.053 ms. The exponential takes that slip's band rather than its length:
a force let go evenly over a time T is 3 dB down at 0.443/T, so the time
constant is T/2.78, 0.019 ms (one as long as the slip would have unloaded
only 63% by its end). A harder pluck slips off faster: the release velocity rises with
the string's displacement, about as its 1.2th power, from 0.2 m/s at
0.5 mm to 5 m/s at 7 mm (Chadefaux et al. 2012), so the release time runs
as the velocity to the -1.68 about the middle of its range. A softer pluck
also slips further, from the lab's 0.04 mm at the hardest to 0.8 mm, the
middle of the 0.5 to 1.2 mm most of ten harpists' slips in chords and
arpeggios fell in (the same paper, Table V), geometrically with velocity: an authored law between two measured
slips, at the owner's request that the tone follow velocity more. The wave that
reaches the bridge over the first round trip is loaded as one full period of
the loop (cell-averaged slopes, so the release is band-limited). The soundbox starts deflected under the held
force, so only the release reaches it and the top springs back. Plucks
scatter as harpists' do (Chadefaux et al.): the pluck point by 0.012 of the
length on a short string to 0.022 on a long one, the force by 14%, the
angle the string leaves at by 4 degrees, each a normal spread about the
control. The release follows each pluck's own displacement through the
1.2th-power law, and scatters by 40% on its own besides, twice what the
harpist's spread leaves once the displacement is taken out (25% against
14%), at the owner's request that each pluck sound different. A finger meeting a sounding string
stops it first, as a harpist's finger holds the string 56 to 431 ms before
letting go (Chadefaux et al., Table II); a plectrum glances off and stops most
of it. A finger's contact silences a string in about 10 ms (Laurson et al.,
CMJ 25(3) 2001), slow against the body's periods, so the body takes the
force the stopped wave leaves as it takes the held force, without the click
of a step. Tension modulation raises the pitch of a hard pluck
for its first moments. The touch is even, a voicing rather than a
measurement: as a player evens out a scale by ear, each string is plucked
with the share of the full force (1 on the quietest string, down to 0.15
on the nylon A4 and 0.13 on the gut E3, which the strongest body modes
favour) that puts its first 300 ms on the
straight line through every string's level, so the register keeps its
slope (about -0.3 dB per semitone on the lyre harps) and no string stands
out of it. The
piezo hears the force on the bar, not what the soundbox radiates, so it
favours other strings (the microphones' touch would leave one string 7 to
8 dB out of the piezo's line); it has a touch of its own, evened the same
way, and the touch blends geometrically between the two as PIEZO does.

**Bridge and body.** All strings end on one bar: heave and rock carry the
strings' motion normal to the top, sway the motion parallel to it, through
the bar's height (a stand-in path, see Known gaps), and the bar bends as
a free maple beam (from 790 Hz to 19.6 kHz in seven bends on the lyre
harps' 24 cm bar, 6.8 and 18.6 kHz on the gut lyre's 9 cm one). On a top this light the top under the bar bends
with it, so each string drives the modes beneath its own place instead of
an average along the bar. The bar stands on a modal
soundbox: an orthotropic plate (Warburton's clamped-plate frequencies,
simply-supported shapes for the bar's participation), coupled through the
cavity's stiffness to a Helmholtz air plug when the box has a soundhole. Mode
Qs are authored from the spread of Acustra's measured guitar bridge (16-75
below 1.5 kHz, 5-36 above 2 kHz): 23 to 1 kHz, its lower quartile, falling
to 10 at 2 kHz and 9 at 10 kHz, its median, and 20 for the air mode. The
junction is solved implicitly every sample, so it is passive and every string, played or idle, is coupled to every other.

**Radiation and room.** Each mode radiates its net volume change as a
monopole, plus an equivalent volume for its edge and corner radiation, from
its own radiation efficiency, with a seeded sign and weight at each of the
two microphones. The microphones are a pair 17 cm apart, ORTF's spacing,
0.5 m from the top, so a mode's two signs are alike as often as such a pair
hears the top's width as one source: the van Cittert-Zernike transform of
a top radiating as sin^2 across its width, 0.93 at 1.1 kHz on the lyre
harp and 0.27 at 4.5 kHz. Until 0.7.1 the two signs were drawn apart, and
the two sides' correlation at 1 kHz ran from -0.3 to 0.7 depending on the
draw. The efficiency is Wallace's integral for the mode's own
shape in a baffle (JASA 51 (1972) 946), less the net volume's share; below
the mode's own coincidence, where its bending wavenumber exceeds the air's,
that edge and corner share is doubled for edges glued to the box, as
Maidanik gives a clamped panel (JSV 9 (1969) 469). An orthotropic top has no
single coincidence frequency: the spruce's modes across the grain reach the
air's wavenumber from 4.2 kHz, along it from 15 kHz, so a band's modes
radiate very differently, and taken one by one they average 2.5 to 6 dB
more from 0.6 to 4.8 kHz than Maidanik's band average, which had stood in
until 0.7.0. The far-field pressure at 0.5 m is rho/(4 pi r) times the
volume acceleration, each mode's taken as its bilinear design takes s times
its velocity, k (1 - z^-1)/(1 + z^-1), so the tails the transform compresses
toward Nyquist keep their acceleration. The transform also narrows each
mode's band near Nyquist, by sin(2x)/2x at x = w/2fs (2 dB of a 12 kHz
mode's energy at 48 kHz), so each mode's radiation is scaled to give back
the energy its continuous design has below Nyquist. A small feedback-delay room follows: six early reflections, then an
eight-line network fed through Dattorro's four input diffusers, so its tail
is dense from about 70 ms, as a room's is, rather than a train of separate
echoes for its first 300 ms.

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
the steel lyre harp phrase. `PlayingControls::piezoInverted` wires it the
other way round: alone it sounds the same, with its sign changed, and in a
blend it cancels the microphones where it otherwise adds to them. Against the
microphones, the piezo carries more of the strings' fundamentals and of the
top two octaves.

**Presets.** The steel lyre harp has 16 strings G3-A5 in C major, 0.44-0.22 m,
plain gauges 0.020-0.010 inch over a nickel-wound 0.024 inch G3, 54-63 N,
over a 20 x 30 cm spruce top 6 mm thick on a 5 cm box
with a 6 cm soundhole: its air mode is 237 Hz and its first top mode 835 Hz.
The nylon harp uses the same frame with 1.30-0.47 mm monofilament at 29-44 N.
The gut lyre has sixteen strings, D3 to E5 in G major, over a closed maple
box whose first mode is 343 Hz: the reconstructions' six 50 cm strings D3 E3
G3 A3 B3 D4 at about 37 N, with F#3 and C4 between them alike, and from E4 up
a slanted yoke shortens each string to hold D4's stress (114 MPa), 22 cm and
30 N at E5. All sixteen share the six's 9 cm bridge bar, 6 mm apart. Dimensions are typical of each
kind of instrument, not measurements of one.

**Measured against recordings.** On the A4 of a CC0 lyre harp note
(Freesound 722897), with the model at the reference's quarter-length pluck
point, 0.3.0 was close in spectral centroid over the first 250 ms (482 Hz
against 465 Hz) and in the apparent inharmonicity of partials 2-12
(B = 3.86e-4 against 3.93e-4), both lacked the 4th harmonic, and the model's
note decayed faster over 80-450 ms (43 against 30 dB/s): A4 sits beside the
assumed body's 467 Hz top mode. In 0.6.0 it decays at 34 dB/s with a
centroid of 518 Hz. Over its first 45 ms the 3rd, 5th, 6th, 7th and 10th
partials sit on average 4.6 dB further under the fundamental than the
recording's, where they sat 14.5 dB under before the bending bar and the
measured slip; by 100-200 ms they stand 6.7 dB over. Partials 3-12 decay
alike (33 against 36 dB/s on average from 60 to 400 ms, the recording's
length), but the fundamental falls away faster (42 against 32 dB/s), drained
by the assumed body's top mode beside it. The model's partials follow its
string's own stiffness (B = 2.4e-4 against the recording's 4.0e-4 by the same
fit).
Replaying a lyre harp phrase note for note (Freesound 649925, CC BY), the
model's 1/3-octave long-term spectrum fell short of the recording's by
7.6 dB on average from 1.25 to 5 kHz in 0.3.0 (10.0 dB in 0.2.0), and by
8.0 dB over eight seeds of 0.4.0's scattered plucks (7.1-8.9). In 0.6.0,
plucked at a quarter as before, it falls 0.8 dB short over the same seeds
(0.2 to 1.6 short), both microphones summed. The wound G3 cost 0.4 dB of
that when it came in: the plain string, ringing in sympathy, had filled the
1.25 kHz band 2.3 dB more. Plucked at 0.47, it falls
3.9 dB short (3.0 to 4.7), but its shape over the third-octaves from 200 Hz
to 10 kHz sits 5.3 dB rms from the recording's on every seed (5.2 to 5.5),
where a quarter's sits 7.2 (6.8 to 7.5): the quarter's 630 Hz band, the
D4's and E4's second partials, stood 22 dB over the recording's, and at
0.47 stands 10 over. With the mean over those bands removed, the rest runs
from 7 dB under at 5 kHz to 11 dB over at 1 kHz. 0.6.0 now plucks at 0.27,
chosen by ear on the sampled lyre harp below, a point nearer the quarter
than 0.47 at which the phrase has not been measured. Its partials decay alike
to the 4th: from 0.12 s to the next note, medians over the eight notes of
13, 11, 21 and 15 dB/s against the recording's 13, 12, 17 and 18. The 5th
and 6th decay at 25 and 27 against 37 and 48, but on most notes strings
still ringing share those partials, and on the one note played alone,
the first D4, they go the other way (38 and 13 against 15 and 2). Over a CC0
Iron Age lyre recording (Freesound 191883, another instrument and tuning),
0.3.0's gut lyre decayed at a similar rate (median 18-22 against 18 dB/s
over 80-550 ms) and was a little darker (centroid 448-496 Hz against
557 Hz). In 0.6.0 the shared phrase and the recording are measured alike:
onsets from the spectral flux, each note's centroid over its first 250 ms
and its decay from 80 to 550 ms or to the next note, medians over the
notes. Plucked at 0.47, the gut lyre's notes decay at 19 against the
recording's 22 dB/s (27 plucked at 0.22), and centre at 526 Hz at the left
microphone and 761 Hz at the right, where the recording's channels read 582
and 611 Hz: the body's drawn radiation signs part its two microphones
further than the recording's channels. Measured the same way on the gut
lyre's README demo (onsets over the spectral flux's peaks), 0.6.5 decays
at 25 dB/s and centres at 492 and 650 Hz; 0.7.2, with each mode's own
radiation and the microphone pair, at 21 dB/s and 521 and 531 Hz, where
the recording reads 20 dB/s and 577 and 607 Hz. Replaying the recording's own
melody instead (`Tools/compare_gut_lyre_phrase.py`; the lyre is tuned 3.3
semitones below the model's strings, so the model plays it that much
higher, and a spectrum that follows the pitch centres 21% higher), 0.7.2
centres at 548 and 539 Hz against the recording's 577 and 607 (698 and 734
at the model's pitch), and 0.7.3, plucked at 0.11, at 709 and 709; its notes
decay at 22 against 20 dB/s.
A sampled lyre harp (DeadRobot's free DR Plucked Lyre Harp: 16 strings G3 to
G#5, each plucked alone three times) is measured note by note against the
steel lyre harp strung the same way
([benchmark](Docs/validation/dr-lyre-harp-benchmark-2026-10-29.md)). Its
player plucks at 0.25 to 0.32 of the string (median 0.27), where the model
now plucks. Partials 2 to 6 decay alike (rms log2 of model over recording
0.50) and the fundamentals do not (1.23). Each note at equal power, the
model's third-octave shape over its first 0.3 s sits 5.9 dB rms from the
recording's, and 6.2 from 0.3 to 1.5 s (0.6.4: 6.5 and 7.4; plucked at
0.47, before the bar's bends were corrected, 7.6 and 8.2). From 3 to
10 kHz after 0.3 s it still falls 4.6 to 10.9 dB short. These are
order-of-magnitude checks, not a calibration.

**Playing.** `PlayingControls` holds what a player and the room change while
the instrument sounds; the plucking fields apply at the next pluck, the rest at
once. Release sets how a let-go string sounds on, from the playing hand
stopping it (a 30 ms amplitude time constant on average over its partials)
to ringing until it dies away. The hand touches the string where it plucked
it, a dashpot at that point: partial n is damped as sin^2(n pi x / L), so
the partials with a node near the touch ring on for a moment, the
flageolet a light touch makes, where a fade had taken every partial alike;
mute rests a palm across every string; sympathy rests a hand on the strings
nobody plucked. With the sharping levers on, a note a semitone above a
string's own raises that string's lever, which shortens its speaking length
by a semitone, and the string's own note lowers it again, as a lyre harp
player sets the levers by hand. A note above or below the strings plays
the string an octave or more away that sounds its pitch class, so every
MIDI note sounds with the levers on. `AculyrePerformer` designs all three
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
every new instrument) with its guard for the plucked instruments
(`kVuGuard`): a compressor on the louder channel's power averaged over the
meter's 300 ms (10 ms attack, 300 ms release, ratio 10 over a 16 dB knee
from -11 to +5 VU), then a peak limiter that looks 2 ms ahead and holds
every sample under -1.5 dBFS. The portfolio's earlier guard read each
sample's power with an 80 ms attack and its knee centred at -2 VU, so a
chord's attack set its gain: the median four-note chord read 0.2-0.4 dB
quieter at velocity 127 than at 110 on the microphones, and the gut lyre's
0.8 dB quieter than at 90. Through this one it rises 1.7-2.4 dB from velocity 90 to 127 on the lyre harps and
0.6-1.9 dB on the gut lyre, whose plucks gain only 1.8 dB there unguarded.
Typical playing passes it unchanged, single notes read within 0.3 dB of
their own level but for the loudest strings at full velocity (up to 2.1 dB
lower), and the hardest chords read -1.6 VU or less. The look-ahead delays
the output by 2 ms rounded down to 4 samples (88 at 44.1 kHz, 96 at
48 kHz).

**Cost.** The steel lyre harp (32 waveguides, 234 modes, seven bar bends
and the room) playing its demo uses about 9% of one core of a cloud
container at 1x and 48 kHz, built as the Rack SDK builds (clang -O3
-fno-builtin, no auto-vectorization), the nylon lyre harp alike and the gut
lyre 7%, where 0.6.2 used 16% and 15%. The strings' dispersion stages run
stage by stage across all the waveguides, so their chains overlap, and the
modes, the strings' filters and the bridge solve are written with two-lane
vector types, which the SDK compiler keeps where it vectorizes nothing by
itself. On a bar centred on the top most modes are even or odd about its
middle, and their passes skip the bar motions they cannot meet. Once everything has died away below -140 dBFS for
half a second the engine sleeps, and a sleeping instance costs next to
nothing until the next pluck.

## Known gaps

- No body is measured: every soundbox is a plate-and-air model with assumed
  dimensions, so where its modes fall against the strings is a guess, and that
  sets which notes are loud, which are dead and how fast they decay. The
  lyre harps' first top mode, 835 Hz on the 6 mm top, drains the treble:
  plucked alone at velocity 0.7, the steel A5's fundamental falls 38 dB
  from 0.12 to 0.7 s and G5's 23, where D5 to F5 fall 8 to 15 and the
  strings below 3 to 8. The DR lyre harp's G5 falls as fast (60 dB/s), but
  its G#5 does not; until a lyre harp is measured, which notes its top
  drains is a guess. Before 0.7.2 the 3 mm top's mode at 879 Hz did the
  same to A5 (27 dB). Published small soundboxes only bound it. A
  12-string kokle on a 3 mm spruce top (G3 to D5) rings at Q 18 to 29 from
  290 to 700 Hz (Peekna and Rossing, Forum Acusticum 2005, Fig. 3), where
  the model's modes below 1 kHz sit at Q 20 to 23. A lever harp's bridge
  admittance, scaled by its string's impedance, peaks near -46 dB and
  mostly runs -50 to -70 dB (Woodhouse, J. Sound Vib. 523 (2021), Fig. 4);
  the steel lyre harp's, scaled by its A4's, runs a median -53 to -66 dB
  by octave from 200 Hz to 6.4 kHz and peaks at -36 dB at 811 Hz, in the
  harp's range; the 3 mm top before 0.7.2 ran -44 to -53 and peaked at
  -25 dB at 455 Hz. How much a lyre harp's small top couples, no
  measurement says.
- Nylon and gut damping follow one published fit to measured harp strings,
  not strings from these instruments; gut varied most from string to string
  in that study.
- The bridge bar bends as a free beam of an assumed maple section; no
  lyre's bar is measured, and the bends cost 1.4 to 1.8 times the CPU of a
  rigid bar. The body's mode damping is a point in a guitar bridge's
  measured spread, chosen by ear ([decisions](Docs/decisions.md)).
- Against a recorded lyre harp A4, the model's fundamental decays slower
  (9 against 32 dB/s over 80-450 ms; 42, drained by the 3 mm top's 467 Hz
  mode, before 0.7.2), and its partials are less inharmonic (B = 2.4e-4 against 4.0e-4). The lyre
  harp phrase's other strings do not follow: fitted over each note's
  partials to 6 kHz, its D4, E4 and G4 read 1.5e-4 to 3.0e-4 where the
  model's read 2.3e-4 to 4.0e-4, and its A4 and B4 3.6e-4 to 4.0e-4 where
  the model's read 1.6e-4 to 1.9e-4, a jump at A4 that no smooth run of
  gauges makes. The sampled DR lyre harp, another harp and player, jumps at
  its A4 string too (1.6e-4 on G4, 4.3e-4 on the A4 string), and from there
  up its strings are 1.9 to 3.8 times the model's. The body and the string
  gauges are both assumed. Only G3
  is wound: the string set it follows is for a harp tuned up from D3 and
  winds its four lowest strings, and no set for a G3-A5 harp lists its
  gauges. The DR harp's G3 reads as plain wire (5.3e-4 against the model's
  1.7e-4).
- Against the DR lyre harp, note by note, the model's mid and treble
  fundamentals ring on: the recording's G#4 to F5 fall 15 to 50 dB/s from
  0.08 to 0.6 s where the model's fall 9 to 25 (G5 alike, 61 against 60),
  and from G3 to F4 partials 2 to 6 fall 11 to 16 dB/s against 8 to 30
  (most 18 to 30). With each note at equal power it carries 4 to 11 dB less from 300 to
  500 Hz over the first 0.3 s and 6 more at 400 Hz after, and 8 to 10 dB
  less from 5 to 8 kHz throughout. The 6 mm top (0.7.2) was chosen on
  these notes among 3 to 8 mm; it put the 0.8 to 1.3 kHz peak, where the
  recording's is, within 2.3 dB, which the 3 mm top had left 1 to 8 dB short
  ([benchmark](Docs/validation/dr-lyre-harp-benchmark-2026-10-29.md), [2026-10-30](Docs/validation/lyre-recordings-benchmark-2026-10-30.md),
  [decisions](Docs/decisions.md)). The other lyre harp recording disagrees:
  replaying its phrase (Freesound 649925, D4 to B4), the 6 mm top leaves
  the fundamentals 8 to 15 dB short and the shape 11.2 dB rms from the
  recording's, where the 3 mm top's sat 8.1.
- The parallel polarisation reaches the body by a stand-in path: it tips
  the bar toward the yoke, which is what the string's pull along its own
  length does. A sideways pull at the bar's top is a moment about the
  string's axis, which rocks and bends the bar instead, as Acustra derives
  it for a guitar saddle. That path was built and measured on the replayed
  phrase over eight seeds. It couples the plane so weakly at its
  fundamental that the G4's fundamental decays at about 1 dB/s against the
  recording's 17 (the stand-in's 10), and the D4's and E4's at 4 and 6
  against 7 and 8 (the stand-in's 8). Near the bar's ends the bends' slopes
  couple the treble strings strongly instead: A5's partials land 10 cents
  off the stiff-string law and the top strings 3.6 dB off the even touch.
  The recordings point to a loss the model lacks rather than to that path,
  so the stand-in stays. With the 6 g bar it moves some partials by up to
  about 11 cents.
- No axial force. A plucked string's stretch pulls on its end at twice each
  partial's frequency, growing as the square of the pluck. A harp's strings
  meet its soundboard at 35 degrees, and there the octave this adds matches
  the fundamental's height even for gentle plucks (Woodhouse, J. Sound Vib.
  523 (2021), Sec. 4.2). Here the strings run along the top to a tie bar
  1 cm high, where that pull only tips the bar. Estimated from the
  published formulas, it reaches the top's lower modes about 27 dB under
  the fundamental's drive on G5 plucked at full strength, and some 40 dB
  under on A4 at its touch. The strings' lengthwise stiffness, which raises
  a strung harp's soundboard modes, is left out too.
- The dispersion fit stops at 5 kHz, and above it the bass strings'
  partials fall flat of the law: at 48 kHz the design leaves them up to 33
  cents flat by 8 kHz on the wound steel G3, 69 on the steel A3 and 104 on
  the gut D3. Holding them to 8 kHz would take up to eight second-order
  sections a string, where two to four hold them to 5 kHz, for partials
  that sound 56 to 70 dB below a steel A3's strongest over its first
  0.1 s at velocity 0.7 (64 to 80 on the wound G3, 87 to 100 on the gut
  D3).
- At 1x, the back panel's default, the top octave from 10 to 16 kHz sits
  0.6 to 1.1 dB below the 4x sound on the lyre harps and 0.2 dB above it on
  the gut lyre, playing every string in turn, dry. Until 0.7.0 it sat 1.5
  to 1.65 dB below on the lyre harps: the bilinear transform narrowed each
  mode's band near Nyquist, which each mode's radiation now makes up for.
  The rest is not measured apart. Near the pluck's comb notches single
  partials also land up to 16 dB apart between rates (the steel A5's 13th
  and 14th).
- No plucking-hand model beyond a release time and a plectrum's reach, no
  buzzing, no string-to-string contact, no hand damping
  gestures beyond note-off. The release is a stand-in for the slip
  Chadefaux, Le Carrou and Fabre model (JASA 133 (2013) 2444): its 0.053 ms
  is one harpist's plucks near the fingertip in their lab, while ten
  harpists playing chords and arpeggios slipped 0.2 to 1.5 mm, mostly 0.5
  to 1.2 (their Table V). How the slip lengthens with a softer pluck, to
  0.8 mm at the softest, the slip's 40% scatter, and where the hand
  touches a string to stop it (where it plucked), are authored, not
  measured.
- Replaying the lyre harp phrase (Freesound 649925) with each note at the
  pluck point fitted to it, the steel lyre harp falls 3.6 dB short of the
  recording from 1.25 to 5 kHz and 4.6 dB from 6 to 10 kHz, where a
  quarter-length pluck leaves 1 dB and 0.47 about 4 (0.6.0 plucks at
  0.27, near the quarter). That
  long-term spectrum also weighs each note's loudness, which the recording
  does not give (every note is replayed at velocity 0.7). Note by note,
  each partial's level under its fundamental, on the phrase's five cleanest
  notes, matches the recording's to 4 kHz (band medians within 3.4 dB,
  either way) and falls short above it: 7 to 9 dB from 4 to 6 kHz and 15 to
  21 dB from 6 to 9 kHz, alike from 45 to 300 ms after the pluck, so the
  strings lose their treble no faster than the recording's. 0.6.0's nail,
  its apex rounded over a millimetre, raised it about 1 dB, and an instant
  release, as a nail's now is, would still leave 2.3 dB from 1.25 to 5 kHz, so the rest lies in the assumed body, the microphones or the
  recording, which one unmeasured recording cannot separate. The DR lyre
  harp's single notes, another player and microphones, show the same
  shortfall: 6 to 18 dB from 5 to 10 kHz with each note at equal power, in
  the partials from the first 10 ms on, at any of the pluck points tried.
- The top's mode frequencies are a clamped plate's (Warburton), but its
  mode shapes and the volumes they move are a simply supported plate's.
  Clamped beam shapes were tried and left the dead notes deader without
  adding treble.
- The nail and the plectrum are authored: no nail or plectrum pluck on a
  lyre is measured, and the plectrum's reach is fitted on an archtop
  guitar. Against the finger at velocity 90 the nail is only a little
  brighter, because the fingertip already lets go in 0.03 ms.
- The gut lyre's ten added strings are no reconstruction's: their gauges,
  the slanted yoke and the 6 mm spacing are assumed, and the box and bridge
  bar are still the six-string lyre's.
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

`RenderAculyreNotes` plucks every string of a preset alone, one WAV per note,
and `Tools/compare_lyre_harp_samples.py` (NumPy and SciPy) measures those
against the DR Plucked Lyre Harp's samples, fetched separately; its head
gives the commands and the protocol.

## Licensing

Aculyre's source is available under the [PolyForm Noncommercial License
1.0.0](LICENSE). The string loss sections and the Thiran design are copied from
Acustra, under the same licence and owner. The body's mode Q statistics are
summarised from Acustra's measured bridge, adapted from Robert Mores' archive
for the acoustical documentation of guitars (CC BY 4.0). The comparison
recordings, Freesound 722897 ("Lyre harp single note", Biggertigger2006) and
191883 ("Lyre.flac", Hedmarking), are CC0 1.0 and are not stored here, and
neither are DeadRobot's DR Plucked Lyre Harp samples (deadrobotmusic.com) or
anything rendered from them.
