#!/usr/bin/env python3
"""The promo's three performed pieces, each arranged for a consort of lyres.

    python3 Tools/promo/repertoire.py <out-dir>   # writes <out-dir>/<piece>/<part>.mid

Every part is one Aculyre set to a factory patch; `pan` (-1 left .. +1 right)
and `level` (dB against the melody, on matched loudness) are the part's place
in the mix promo_audio.py makes. Played as people play, on
Tools/write_preset_midi.py's Performance, with fixed seeds:

- tempo: the players share one clock that breathes with the phrases, holds
  back at the cadences, wanders a little from bar to bar, and in the jigs
  pushes on as a session does;
- timing: each player has a hand of its own (onsets a few milliseconds off
  the grid, a slow drift ahead of and behind the others), the accompanists
  sit a touch behind the tune, and the jigs lilt: the middle quaver of each
  three comes late and light;
- velocity: every part follows the phrase's rise and fall, the tune leans on
  the pulse and plays its high notes stronger, and no two notes land alike.

The tunes are
public domain and transposed whole to fit the lyres; the other parts are
written here.

- Minuet in G, BWV Anh. 114 (Christian Petzold, Notebook for Anna Magdalena
  Bach, 1725), the first strain twice: the melody on the nylon lyre harp (its
  F sharps take the sharping levers), Petzold's bass on the gut lyre, the
  harmony broken on the steel lyre harp, and the second time the melody an
  octave higher on the nail-plucked harp.
- The Kesh, Irish jig (traditional; The Session, tune 55, setting 1), in G:
  the tune an octave down on the gut lyre, alone the first time; then a
  damped, picked steel harp keeps the jig's bass and off-beat chords, the
  nylon harp joins the tune at its own octave, and the nail harp rings chord
  notes above the last time through.
- The Butterfly, Irish slip jig (traditional; The Session, tune 10, setting 1),
  a fourth up from E minor to A minor, so the lyre harps' C major strings play
  it without levers: the tune on the steel lyre harp, broken chords on the
  nylon harp, a picked gut lyre marking the pulse, and on each part's repeat
  the nail harp a chord note under the tune.
"""
import collections
import math
import pathlib
import random
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
from write_preset_midi import Performance, breathe, n  # noqa: E402

Part = collections.namedtuple("Part", "name patch pan level p")


class Player(Performance):
    """One player of the consort: Performance's hand, plus what playing with
    others adds. `phrase` is the written phrase length in beats, whose rise
    and fall every part's velocity follows (`swell` steps); `lilt` delays the
    middle note of each three beats by that share of a beat and plays it
    lighter (the jigs' swing); `behind` seats the player that many seconds
    after the clock; the drift is a slow wander either side of it."""

    def __init__(self, tempo, seed, timing, phrase, swell, lilt=0.0, behind=0.0):
        super().__init__(tempo, seed=seed, timing=timing)
        self.phrase, self.swell, self.lilt, self.behind = phrase, swell, lilt, behind
        own = random.Random(seed + 7)
        # two slow sines, about 5 and 3 ms, periods of several seconds
        self.wander = [(amount * own.uniform(0.7, 1.3), own.uniform(*period), own.uniform(0, 2 * math.pi))
                       for amount, period in ((0.005, (7.0, 11.0)), (0.003, (2.5, 4.0)))]

    def middle(self, beat):
        return self.lilt and abs(beat % 3 - 1) < 1e-6

    def at(self, beat, lean=0.0, scale=1.0):
        clock = self.seconds(beat)
        drift = sum(a * math.sin(2 * math.pi * clock / period + phase) for a, period, phase in self.wander)
        late = self.lilt * 60.0 / self.bpm(beat) if self.middle(beat) else 0.0
        return max(0.0, clock + lean + self.behind + drift + late + self.scatter(scale))

    def play(self, onset, beats_held, beat, note, velocity, channel=0):
        x = (beat % self.phrase) / self.phrase
        # up to the phrase's peak past its middle, then away to its end
        rise = math.sin(math.pi * x ** 0.85) - 0.55 - 0.45 * max(0.0, (x - 0.85) / 0.15)
        super().play(onset, beats_held, beat, note, velocity + self.swell * rise - (5 if self.middle(beat) else 0), channel)


def wobble(points, step, amount, seed):
    """The tempo curve with a little wander from one `step` of beats to the next."""
    curve, rng = Performance(points, seed=0), random.Random(seed)
    first, last = points[0][0], points[-1][0]
    marks = sorted({b for b, _ in points} | {first + step * k for k in range(1, int((last - first) / step))})
    return [(b, curve.bpm(b) * (1.0 if b in (first, last) else 1.0 + max(-2, min(2, rng.gauss(0, 1))) * amount)) for b in marks]


def push(points, by):
    """The tempo curve pushed on by the share `by` from its first beat to its last."""
    first, last = points[0][0], points[-1][0]
    return [(b, bpm * (1.0 + by * (b - first) / (last - first))) for b, bpm in points]


def players(tempo, timing, seed, phrase, swell, lilt, *specs):
    """One Player a part, on the piece's shared tempo: (name, patch, pan, level).
    The first carries the tune; the others sit 4 to 9 ms behind it."""
    return [Part(*spec, Player(tempo, seed + 101 * k, timing, phrase, swell, lilt, behind=0.0 if k == 0 else 0.004 + 0.0025 * (k % 3)))
            for k, spec in enumerate(specs)]


def melody(p, start, notes, loud, accent, lean=0.012, shift=0):
    """(name, length in beats) from `start`; the first of each group of
    `accent` beats a little stronger, as a player leans on the pulse, and the
    bar's high notes stronger than its low ones."""
    pitches = [n(name) for name, _ in notes if name]
    centre = sum(pitches) / max(1, len(pitches))
    at = start
    for name, length in notes:
        if name:
            strong = abs(at / accent - round(at / accent)) < 1e-6
            contour = max(-4.0, min(5.0, 0.5 * (n(name) - centre)))
            p.play(p.at(at, lean=lean), length * (0.96 if length > 1 or not p.lilt else 0.9), at, n(name) + shift,
                   loud + (6 if strong else 0) + contour)
        at += length
    return at


def roll(p, beat, names, held, loud, spread=0.03, lean=-0.01):
    for k, name in enumerate(names):
        p.play(p.at(beat, lean=lean + spread * k, scale=0.6), held, beat, n(name) if isinstance(name, str) else name, loud - 2 * k)


def minuet():
    # (name, quarters) per bar, the first strain; bars 9-14 repeat 1-6
    head = [[("D5", 1), ("G4", .5), ("A4", .5), ("B4", .5), ("C5", .5)], [("D5", 1), ("G4", 1), ("G4", 1)],
            [("E5", 1), ("C5", .5), ("D5", .5), ("E5", .5), ("F#5", .5)], [("G5", 1), ("G4", 1), ("G4", 1)],
            [("C5", 1), ("D5", .5), ("C5", .5), ("B4", .5), ("A4", .5)], [("B4", 1), ("C5", .5), ("B4", .5), ("A4", .5), ("G4", .5)]]
    tune = head + [[("F#4", 1), ("G4", .5), ("A4", .5), ("B4", .5), ("G4", .5)], [("A4", 3)]] \
        + head + [[("A4", 1), ("B4", .5), ("A4", .5), ("G4", .5), ("F#4", .5)]]
    bass = [[("G3", 2), ("A3", 1)], [("B3", 3)], [("C4", 3)], [("B3", 3)], [("A3", 3)], [("G3", 3)],
            [("D4", 1), ("B3", 1), ("G3", 1)], [("D4", 1), ("D3", .5), ("C4", .5), ("B3", .5), ("A3", .5)],
            [("B3", 2), ("A3", 1)], [("G3", 1), ("B3", 1), ("G3", 1)], [("C4", 3)], [("B3", 1), ("C4", .5), ("B3", .5), ("A3", .5), ("G3", .5)],
            [("A3", 2), ("F#3", 1)], [("G3", 2), ("B3", 1)], [("C4", 1), ("D4", 1), ("D3", 1)]]
    # the harmony a beat at a time, between the bass and the melody; where the
    # bass moves under one bar's chord (7, 13, 15) the notes follow it
    G, C, Am, D = ("B3", "D4", "G4"), ("C4", "E4", "G4"), ("A3", "C4", "E4"), ("A3", "D4", "F#4")
    inner = [G, G, C, G, Am, G, ("A3", "D4", "B3"), D, G, G, C, G, ("A3", "C4", "D4"), G, ("E4", "D4", "C4")]
    passes = [(0, 66), (48, 80)]
    # a dance at an easy walk: each eight bars swell and give way at their cadence, the last broadly
    tempo = wobble(breathe(116, [(s + 24 * h, 24) for s, _ in passes for h in range(2)][:-1] + [(72, 24)], swell=0.05, settle=0.11, last=0.34),
                   3, 0.012, 1725)
    parts = players(tempo, 0.009, 1725, 24, 9.0, 0.0, ("melody", "Nylon Lyre Harp", -0.12, 0.0), ("bass", "Gut Lyre", 0.55, -4.0),
                    ("harmony", "Lyre Harp", -0.60, -8.0), ("octave", "Bright Nail Harp", 0.25, -10.0))
    lead, low, mid, high = (part.p for part in parts)
    for start, loud in passes:
        last = start == passes[-1][0]
        held = 6.0 if last else 2.9   # the cadence, the last one held long
        at = start
        for bar in tune:
            at = melody(lead, at, bar, loud, 3)
        lead.play(lead.at(at, lean=0.012), held, at, n("G4"), loud + 6)
        at = start
        for bar in bass:
            at = melody(low, at, bar, loud - 6, 3, lean=-0.008)
        roll(low, at, ["G2", "G3", "D4"] if last else ["G3", "D4"], held, loud - 8, 0.06 if last else 0.03)
        for bar, notes in enumerate(inner):
            beat = start + 3 * bar
            if last:   # the second time the chord is broken across the bar and left to ring
                for k, name in enumerate(notes):
                    mid.play(mid.at(beat + k, lean=-0.004), 2.9 - k, beat + k, n(name), loud - 14 + 2 * k)
            else:
                roll(mid, beat, notes[:2], 2.8, loud - 12)
        roll(mid, at, G, held, loud - 12, 0.07 if last else 0.03, lean=0.02)
        if last:
            at = start
            for bar in tune:
                at = melody(high, at, bar, loud - 6, 3, lean=0.016, shift=12)
            roll(high, at, ["G5", "B5", "D6"], held, loud - 8, 0.07, lean=0.2)
    return parts


def kesh():
    # (name, eighths); ABC (The Session 55, setting 1) an octave down
    A = [[("G3", 3), ("G3", 1), ("A3", 1), ("B3", 1)], [("A3", 3), ("A3", 1), ("B3", 1), ("D4", 1)],
         [("E4", 1), ("D4", 1), ("D4", 1), ("G4", 1), ("D4", 1), ("D4", 1)], [("E4", 1), ("D4", 1), ("B3", 1), ("D4", 1), ("B3", 1), ("A3", 1)],
         [("G3", 1), ("A3", 1), ("G3", 1), ("G3", 1), ("A3", 1), ("B3", 1)], [("A3", 3), ("A3", 1), ("B3", 1), ("D4", 1)],
         [("E4", 1), ("D4", 1), ("D4", 1), ("G4", 1), ("D4", 1), ("B3", 1)], [("A3", 1), ("G3", 1), ("F#3", 1), ("G3", 3)]]
    B = [[("B3", 1), ("A3", 1), ("B3", 1), ("D4", 1), ("B3", 1), ("D4", 1)], [("E4", 1), ("G4", 1), ("E4", 1), ("D4", 1), ("B3", 1), ("D4", 1)],
         [("B3", 1), ("A3", 1), ("B3", 1), ("D4", 1), ("B3", 1), ("G3", 1)], [("A3", 1), ("B3", 1), ("A3", 1), ("A3", 1), ("G3", 1), ("A3", 1)],
         [("B3", 1), ("A3", 1), ("B3", 1), ("D4", 1), ("B3", 1), ("D4", 1)], [("E4", 1), ("G4", 1), ("E4", 1), ("D4", 1), ("B3", 1), ("D4", 1)],
         [("G4", 1), ("F#4", 1), ("G4", 1), ("A4", 1), ("G4", 1), ("A4", 1)], [("B4", 1), ("G4", 1), ("F#4", 1), ("G4", 3)]]
    # one chord a dotted quarter
    chordsA = ["G G", "D D", "C C", "G D", "G G", "D D", "C C", "D G"]
    chordsB = ["G G", "C C", "G G", "D D", "G G", "C C", "G D", "D G"]
    dyad = {"G": ("D3", "G3"), "D": ("D3", "A3"), "C": ("E3", "G3")}   # the gut lyre's own, when it plays alone
    # the backing's bass, on the bar and on the half bar, and its off-beat chord;
    # G2 lies below the harp's B2 string, which a key past the strings retunes
    boom = {"G": ("G2", "D3"), "D": ("D3", "A3"), "C": ("C3", "G3")}
    chop = {"G": ("B3", "D4", "G4"), "D": ("A3", "D4", "F#4"), "C": ("C4", "E4", "G4")}
    bell = {"G": ("D6", "B5"), "D": ("D6", "A5"), "C": ("E6", "C6")}
    form = [(A, chordsA, 74), (A, chordsA, 80), (B, chordsB, 84), (B, chordsB, 88)]
    # a session's jig: steady in the bar, 4 % faster by the end, a breath at each turn of the tune
    tempo = wobble(push(breathe(306, [(48 * i, 48) for i in range(4)], swell=0.025, settle=0.03, last=0.16), 0.04), 6, 0.008, 55)
    parts = players(tempo, 0.007, 55, 24, 7.0, 0.09, ("melody", "Gut Lyre", 0.05, 0.0), ("backing", "Damped Steel Pick", -0.58, -5.0),
                    ("octave", "Nylon Lyre Harp", 0.55, -4.0), ("bells", "Bright Nail Harp", 0.30, -11.0))
    lead, back, upper, bells = (part.p for part in parts)
    for index, (part, chords, loud) in enumerate(form):
        start = 48 * index
        at = start
        for bar in part:
            at = melody(lead, at, bar, loud, 3)
        if index >= 2:
            at = start
            for bar in part:
                at = melody(upper, at, bar, loud - 4, 3, lean=0.014, shift=12)
        for bar, pair in enumerate(chords):
            for half, chord in enumerate(pair.split()):
                beat = start + 6 * bar + 3 * half
                final = index == len(form) - 1 and bar == 7 and half == 1
                if index == 0:
                    low, high = dyad[chord]
                    if half == 0:
                        lead.play(lead.at(beat, lean=-0.01), 2.8, beat, n(low), loud - 16)
                    lead.play(lead.at(beat, lean=-0.004), 2.8, beat, n(high), loud - 20)
                    continue
                if final:   # the close: G rolled up from the bass and left to ring
                    roll(back, beat, ["G2", "D3", "G3", "B3", "D4"], 5.0, loud - 4, 0.035)
                    roll(bells, beat, ["G5", "D6"], 5.0, loud - 10, 0.08, lean=0.15)
                    continue
                back.play(back.at(beat, lean=-0.008), 1.6, beat, n(boom[chord][half]), loud + (2 if half == 0 else -3))
                roll(back, beat + 2, chop[chord], 0.7, loud - 10, 0.008, lean=0.0)
                if index == 3:
                    bells.play(bells.at(beat, lean=0.004), 2.9, beat, n(bell[chord][half]), loud - 12)
    return parts


def butterfly():
    # (name, eighths); ABC (The Session 10, setting 1) up a fourth, E minor to A minor
    A = [[("E5", 2), ("A4", 1), ("C5", 2), ("A4", 1), ("B4", 3)], [("E5", 2), ("A4", 1), ("C5", 2), ("A4", 1), ("B4", 1), ("A4", 1), ("G4", 1)],
         [("E5", 2), ("A4", 1), ("C5", 2), ("A4", 1), ("B4", 3)], [("E5", 2), ("G5", 1), ("G5", 2), ("E5", 1), ("D5", 1), ("B4", 1), ("G4", 1)]]
    B = [[("E5", 2), ("G5", 1), ("A5", 2), ("B5", 1), ("C6", 3)], [("E5", 2), ("G5", 1), ("C6", 2), ("A5", 1), ("G5", 1), ("E5", 1), ("D5", 1)],
         [("E5", 2), ("G5", 1), ("A5", 2), ("B5", 1), ("C6", 2), ("D6", 1)], [("E6", 2), ("D6", 1), ("C6", 2), ("A5", 1), ("G5", 1), ("E5", 1), ("D5", 1)]]
    C = [[("E5", 3), ("E5", 2), ("D5", 1), ("C5", 2), ("D5", 1)], [("E5", 3), ("E5", 1), ("D5", 1), ("E5", 1), ("G5", 1), ("E5", 1), ("D5", 1)],
         [("E5", 3), ("E5", 2), ("D5", 1), ("C5", 2), ("D5", 1)], [("E5", 2), ("G5", 1), ("C6", 2), ("A5", 1), ("G5", 1), ("E5", 1), ("D5", 1)]]
    harmony = {"A": ["Am Am Em", "Am Am G", "Am Am Em", "C C G"],
               "B": ["C Am C", "C F G", "C Am F", "Am F G"],
               "C": ["Am Am C", "Am Am G", "Am Am C", "C F G"]}
    root = {"Am": "A2", "C": "C3", "G": "G2", "Em": "E3", "F": "F3"}
    tones = {"Am": (9, 0, 4), "C": (0, 4, 7), "G": (7, 11, 2), "Em": (4, 7, 11), "F": (5, 9, 0)}
    # the gut lyre's pulse, a low note on the beat and one above it before the
    # next; F's are its third and fifth, the lyre's strings having F sharp
    pulse = {"Am": ("A3", "E4"), "C": ("C4", "G4"), "G": ("G3", "D4"), "Em": ("E3", "B3"), "F": ("A3", "C4")}
    form = [("A", A, 78), ("A", A, 82), ("B", B, 86), ("B", B, 90), ("C", C, 88), ("C", C, 94)]
    # a slip jig that lifts through its second part and broadens into the close
    tempo = wobble(push(breathe(336, [(36 * i, 36) for i in range(len(form))] + [(36 * len(form), 9)], swell=0.03, settle=0.04, last=0.5), 0.03),
                   9, 0.008, 10)
    parts = players(tempo, 0.006, 10, 18, 7.0, 0.07, ("melody", "Lyre Harp", -0.10, 0.0), ("chords", "Nylon Lyre Harp", 0.55, -5.0),
                    ("pulse", "Picked Gut Lyre", -0.60, -7.0), ("under", "Bright Nail Harp", 0.25, -8.0))
    lead, harp, gut, under = (part.p for part in parts)
    for index, (name, part, loud) in enumerate(form):
        start = 36 * index
        at = start
        for bar, notes in enumerate(part):
            at = melody(lead, at, notes, loud, 3)
            if index % 2:   # the repeat: under each note of the tune, the bar's nearest chord note a third or more below
                beat = start + 9 * bar
                for pitch, length in notes:
                    chord = tones[harmony[name][bar].split()[min(2, int(beat - start - 9 * bar) // 3)]]
                    below = max(x for x in range(n(pitch) - 9, n(pitch) - 2) if x % 12 in chord)
                    under.play(under.at(beat, lean=0.014), length * 0.96, beat, below, loud - 8)
                    beat += length
        for bar, chords in enumerate(harmony[name]):
            for third, chord in enumerate(chords.split()):
                beat = start + 9 * bar + 3 * third
                # The harp breaks each chord root, fifth, tenth across its dotted quarter;
                # A and G are taken an octave up, as its lowest string is B2.
                r = n(root[chord]) + (12 if root[chord] in ("A2", "G2") else 0)
                tenth = r + (15 if chord in ("Am", "Em") else 16)
                for k, (pitch, quiet) in enumerate([(r, 10), (r + 7, 16), (tenth, 14)]):
                    harp.play(harp.at(beat + k, lean=-0.006), 2.9 - k, beat + k, pitch, loud - quiet)
                if index:
                    low, high = pulse[chord]
                    gut.play(gut.at(beat, lean=-0.008), 1.8, beat, n(low), loud - 6)
                    gut.play(gut.at(beat + 2, lean=0.0), 0.9, beat + 2, n(high), loud - 14)
    # The close: a glissando up the white strings from A3, then A minor rolled by everyone.
    end = 36 * len(form)
    gliss = [x for x in range(n("A3"), n("A5") + 1) if x % 12 in (0, 2, 4, 5, 7, 9, 11)]
    t0 = lead.at(end - 0.6, scale=0.3)
    for k, pitch in enumerate(gliss):
        lead.message(t0 + 0.022 * k, (0x90, pitch, max(40, 70 - k)))
        lead.message(t0 + 0.022 * k + 1.2, (0x80, pitch, 0), 0)
    for k, name in enumerate(["A3", "E4", "A4", "C5", "E5", "A5", "C6", "E6"]):
        lead.play(lead.at(end, lean=0.33 + 0.07 * k, scale=0.4), 8.0, end, n(name), 92 - 3 * k)
    roll(harp, end, ["A3", "E4", "C5"], 8.0, 80, 0.09, lean=0.3)
    roll(gut, end, ["A3", "E4"], 8.0, 80, 0.09, lean=0.34)
    roll(under, end, ["A5", "E6"], 8.0, 74, 0.1, lean=0.7)
    return parts


PIECES = {"minuet": minuet, "kesh": kesh, "butterfly": butterfly}

if __name__ == "__main__":
    out = pathlib.Path(sys.argv[1])
    for name, build in PIECES.items():
        (out / name).mkdir(parents=True, exist_ok=True)
        for part in build():
            (out / name / f"{part.name}.mid").write_bytes(part.p.smf())
            print(f"{out / name / part.name}.mid\t{part.patch}")
