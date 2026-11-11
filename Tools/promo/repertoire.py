#!/usr/bin/env python3
"""The promo's three performed pieces, one lyre each, both hands on it.

    python3 Tools/promo/repertoire.py <out-dir>   # writes <out-dir>/<piece>/<piece>.mid

Played as a person plays, with Tools/write_preset_midi.py's Performance (a
breathing tempo, onsets a few milliseconds off the grid, velocities wandering
round the score's line, a fixed seed). The tunes are public domain and
transposed whole to fit each lyre; the accompaniments are written here.

- Minuet in G, BWV Anh. 114 (Christian Petzold, Notebook for Anna Magdalena
  Bach, 1725): the first strain twice, melody and Petzold's bass, on the
  nylon lyre harp. Its F sharps take the sharping levers (Levers on Auto).
- The Kesh, Irish jig (traditional; The Session, tune 55, setting 1), in G an
  octave down so it sits on the gut lyre's sixteen strings, D3 to E5, with
  low dyads under it.
- The Butterfly, Irish slip jig (traditional; The Session, tune 10, setting 1),
  a fourth up from E minor to A minor, so the steel lyre harp's C major strings
  play it without levers; a broken-chord left hand, and a rolled A minor close.
"""
import math
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
from write_preset_midi import Performance, breathe, n  # noqa: E402


def melody(p, start, notes, loud, accent, lean=0.012):
    """(name, length in beats) from `start`; the first of each group of
    `accent` beats a little stronger, as a player leans on the pulse."""
    at = start
    for name, length in notes:
        if name:
            strong = abs(at / accent - round(at / accent)) < 1e-6
            p.play(p.at(at, lean=lean), length * 0.96, at, n(name), loud + (6 if strong else 0))
        at += length
    return at


def minuet():
    # (name, quarters) per bar, the first strain; bars 9-14 repeat 1-6
    head = [[("D5", 1), ("G4", .5), ("A4", .5), ("B4", .5), ("C5", .5)], [("D5", 1), ("G4", 1), ("G4", 1)],
            [("E5", 1), ("C5", .5), ("D5", .5), ("E5", .5), ("F#5", .5)], [("G5", 1), ("G4", 1), ("G4", 1)],
            [("C5", 1), ("D5", .5), ("C5", .5), ("B4", .5), ("A4", .5)], [("B4", 1), ("C5", .5), ("B4", .5), ("A4", .5), ("G4", .5)]]
    tune = head + [[("F#4", 1), ("G4", .5), ("A4", .5), ("B4", .5), ("G4", .5)], [("A4", 3)]] \
        + head + [[("A4", 1), ("B4", .5), ("A4", .5), ("G4", .5), ("F#4", .5)], [("G4", 3)]]
    bass = [[("G3", 2), ("A3", 1)], [("B3", 3)], [("C4", 3)], [("B3", 3)], [("A3", 3)], [("G3", 3)],
            [("D4", 1), ("B3", 1), ("G3", 1)], [("D4", 1), ("D3", .5), ("C4", .5), ("B3", .5), ("A3", .5)],
            [("B3", 2), ("A3", 1)], [("G3", 1), ("B3", 1), ("G3", 1)], [("C4", 3)], [("B3", 1), ("C4", .5), ("B3", .5), ("A3", .5), ("G3", .5)],
            [("A3", 2), ("F#3", 1)], [("G3", 2), ("B3", 1)], [("C4", 1), ("D4", 1), ("D3", 1)], [None]]
    passes = [(0, 66), (48, 80)]
    p = Performance(breathe(120, [(s + 24 * h, 24) for s, _ in passes for h in range(2)][:-1] + [(72, 24)],
                            swell=0.03, settle=0.05, last=0.3), seed=1725, timing=0.008)
    for start, loud in passes:
        at = start
        for bar in tune:
            at = melody(p, at, bar, loud, 3)
        at = start
        for index, bar in enumerate(bass):
            if bar == [None]:   # the cadence: G rolled from the bass, the last pass held long
                last = start == passes[-1][0]
                for k, name in enumerate(["G3", "D4", "B4"] if not last else ["G2", "G3", "D4", "B4", "G5"]):
                    p.play(p.at(at, lean=-0.01 + (0.06 if last else 0.03) * k, scale=0.6), 6.0 if last else 2.9, at, n(name), loud - 10 - 2 * k)
                break
            at = melody(p, at, bar, loud - 14, 3, lean=-0.008)
    return p


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
    # one chord a dotted quarter; the low dyad on the bar, its upper note on the half bar
    chordsA = ["G G", "D D", "C C", "G D", "G G", "D D", "C C", "D G"]
    chordsB = ["G G", "C C", "G G", "D D", "G G", "C C", "G D", "D G"]
    dyad = {"G": ("D3", "G3"), "D": ("D3", "A3"), "C": ("E3", "G3"), "Em": ("E3", "B3")}
    form = [(A, chordsA, 74), (A, chordsA, 80), (B, chordsB, 84), (B, chordsB, 88)]
    p = Performance(breathe(312, [(48 * i, 48) for i in range(4)], swell=0.02, settle=0.03, last=0.12), seed=55, timing=0.007)
    for index, (part, chords, loud) in enumerate(form):
        start = 48 * index
        at = start
        for bar in part:
            at = melody(p, at, bar, loud, 3)
        for bar, pair in enumerate(chords):
            for half, chord in enumerate(pair.split()):
                beat = start + 6 * bar + 3 * half
                low, high = dyad[chord]
                final = index == len(form) - 1 and bar == 7 and half == 1
                if half == 0:
                    p.play(p.at(beat, lean=-0.01), 2.8, beat, n(low), loud - 16)
                p.play(p.at(beat, lean=-0.004), 5.0 if final else 2.8, beat, n(high), loud - 20)
    return p


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
    # The left hand breaks each chord root, fifth, tenth across its dotted quarter;
    # A and G are taken an octave up, as the harp's lowest string is B2.
    form = [("A", A, 78), ("A", A, 82), ("B", B, 86), ("B", B, 90), ("C", C, 88), ("C", C, 94)]
    bars = 4 * len(form)
    p = Performance(breathe(342, [(36 * i, 36) for i in range(len(form))] + [(36 * len(form), 9)],
                            swell=0.025, settle=0.03, last=0.5), seed=10, timing=0.006)
    for index, (name, part, loud) in enumerate(form):
        start = 36 * index
        at = start
        for bar in part:
            at = melody(p, at, bar, loud, 3)
        for bar, chords in enumerate(harmony[name]):
            for third, chord in enumerate(chords.split()):
                beat = start + 9 * bar + 3 * third
                r = n(root[chord]) + (12 if root[chord] in ("A2", "G2") else 0)
                tenth = r + (15 if chord in ("Am", "Em") else 16)
                for k, (pitch, quiet) in enumerate([(r, 18), (r + 7, 24), (tenth, 22)]):
                    p.play(p.at(beat + k, lean=-0.006), 2.9 - k, beat + k, pitch, loud - quiet)
    # The close: a glissando up the white strings from A3, then A minor rolled.
    end = 36 * len(form)
    gliss = [x for x in range(n("A3"), n("A5") + 1) if x % 12 in (0, 2, 4, 5, 7, 9, 11)]
    t0 = p.at(end - 0.6, scale=0.3)
    for k, pitch in enumerate(gliss):
        p.message(t0 + 0.022 * k, (0x90, pitch, max(40, 70 - k)))
        p.message(t0 + 0.022 * k + 1.2, (0x80, pitch, 0), 0)
    for k, name in enumerate(["A3", "E4", "A4", "C5", "E5", "A5", "C6", "E6"]):
        p.play(p.at(end, lean=0.33 + 0.07 * k, scale=0.4), 8.0, end, n(name), 92 - 3 * k)
    assert bars == 24
    return p


PIECES = {"minuet": minuet, "kesh": kesh, "butterfly": butterfly}

if __name__ == "__main__":
    out = pathlib.Path(sys.argv[1])
    for name, build in PIECES.items():
        (out / name).mkdir(parents=True, exist_ok=True)
        (out / name / f"{name}.mid").write_bytes(build().smf())
        print(out / name / f"{name}.mid")
