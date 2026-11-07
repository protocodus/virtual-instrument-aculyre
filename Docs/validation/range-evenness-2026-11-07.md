# Range evenness at 0.12.0 (2026-11-07)

Every key A0-C8 of each preset, levers up, rendered alone at 48 kHz at
velocities 64 and 127, through the shipping signal path with the room off.
Tools: the pitch sweep (strongest DFT peak within 60 cents of the note over
0.05-0.35 s and 0.35-1.35 s, idle strings held), the loudness sweep (stereo
RMS over 20-500 ms and 0.5-1.5 s, idle strings ringing and held) and the
decay probe (the fundamental's level 0.3-0.6 s against 1.3-1.6 s, at 44.1,
48 and 96 kHz). Figures below are over the strings and lever notes (B2-G#6
on the harps, D3-F5 on the gut lyre); the stretched keys past them roll off
as the design intends.

## Tuning (0.35-1.35 s, velocity 64)

| Preset | rms | worst | where |
| --- | --- | --- | --- |
| Lyre Harp | 0.80 c | 3.0 c | F#4 lever |
| Nylon Lyre Harp | 1.44 c | 4.9 c | G#3 lever, on the 205 Hz air mode |
| Gut Lyre | 1.03 c | 2.3 c | E4 |

At velocity 127 the wound bass strings open 5 to 11 cents sharp and settle
within a second (the tension glide; the Lyre Lyre library's loud takes open
5 to 14 cents sharp). Late in the decay (2.5 s) the plane the body drains
least rings on alone, up to 4.5 cents from the mix (steel B2).

## Loudness

Attack RMS (20-500 ms) falls smoothly with pitch, about 20 dB from B2 to
G6 on the harps and 6 dB over the gut lyre; against the median of the
neighbouring keys every string is within ±2 dB except the lever notes that
land on a body mode: G#3 (+4 dB, the 205 Hz air mode), G#4 (+6, between
the 396 and 434 Hz modes), C#5 (+4, the 533 Hz mode) and A#5 (+4), and G#5
(-4). Sustain RMS (0.5-1.5 s) is within ±4 dB except nylon B4 and C5 (-8
and -6 dB with the idle strings held, drained by the 488 and 533 Hz modes)
and the notes above. Velocity 127 sits 9 to 10 dB above 64 on every key.

The DR Plucked Lyre Harp samples (16 notes, round robins averaged) are far
less even: attack RMS against the neighbours' median -7.4 (E5) to +5.7 dB
(C4), sustain -7.6 (C5) to +10.4 dB (C4), with a 4 dB spread between round
robins of one note. The model's bumps are the body's modes doing what a
real lyre harp's do, and smaller; nothing was changed.

## Fundamental decay

Identical at 44.1, 48 and 96 kHz (within 0.3 dB/s, the two-plane beat
aside). The strings' own losses give 2.7 (B2) to 22.5 dB/s (G6) on the
steel harp, 3.3 to 37.9 on the nylon, 6.9 to 18.1 on the gut lyre; the body
adds up to 21 dB/s where a string sits on a strong mode (steel G3 on the air
mode, nylon B4-D5 on the 488-533 Hz modes), 0 to 7 elsewhere.

## Tone

Spectral centroid over the attack (50-400 ms, idle strings held), in
semitones above the note: on the harps it falls from about 24 at B2 (the
body radiates nothing at 123 Hz, so partials 2 and 3 carry the note; the
4th partial is missing, the pluck point) to about 1 at G6, and swings
up to ±5 semitones against the neighbouring keys where a fundamental lands
on or between body modes (steel G4 -6, G#4 and A4 +5, C5 -4); the gut
lyre, with three modes below 600 Hz, swings ±8 (E4 on the 337 Hz mode -7,
F#4 between modes +8). The DR lyre harp's 16 notes swing -6 to +6 (and
-13 at G#5) by the same measure, so the model is no less even than the
recording. The recording's centroid in the 4th and 5th octaves is 10
semitones higher than the model's (partials 2 and 3 dominate where the
model's fundamental does); its pluck point is unknown and the Lyre Lyre
library disagrees with it, so this was not pursued. Velocity 127 raises
the centroid 1 to 5 semitones over 64, most where the hard pluck's tension
glide carries partials across a mode.
