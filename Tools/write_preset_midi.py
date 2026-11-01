#!/usr/bin/env python3
"""Writes Aculyre's demo phrases as standard MIDI files.

    python3 Tools/write_preset_midi.py [output-directory]
    python3 Tools/write_preset_midi.py --check   # the committed files match

MIDI map, as the phrases use it: a note plays the open string tuned to it (a
note no string is tuned to is ignored), velocity is the pluck's force, a
note-off is the hand damping that string, and CC64 holds released strings
ringing. Lyre players let strings ring and damp them by hand, so the phrases
hold the pedal through a harmony and lift it where the hand would damp.

Strings: the lyre harps (presets 0 and 1) are C major from G3 to A5; the gut
lyre (preset 2) is D3 E3 G3 A3 B3 D4. Every phrase is original.

songs/ holds longer public-domain pieces played with a person's timing
(Performance, below); SONGS names the preset each is written for:

    build/RenderAculyreDemo 0 Docs/audio/midi/songs/canon-in-c.mid canon-in-c.wav
"""

import argparse
import math
import os
import random
import struct
import sys

PPQ = 480

NOTE = {}
for octave in range(1, 8):
    for name, offset in (("C", 0), ("D", 2), ("E", 4), ("F", 5), ("G", 7), ("A", 9), ("B", 11)):
        NOTE[f"{name}{octave}"] = 12 * (octave + 1) + offset


def n(name):
    if "#" in name:
        return NOTE[name.replace("#", "")] + 1
    return NOTE[name]


class Phrase:
    def __init__(self, bpm):
        self.bpm = bpm
        self.events = []  # (beat, order, bytes)

    def note(self, beat, name, length, velocity):
        key = n(name) if isinstance(name, str) else name
        self.events.append((beat, 1, bytes([0x90, key, velocity])))
        self.events.append((beat + length, 0, bytes([0x80, key, 0])))

    def pedal(self, beat, down):
        self.events.append((beat, 0 if not down else 2, bytes([0xB0, 64, 127 if down else 0])))

    def roll(self, beat, names, length, velocity, spread=0.06, upward=True):
        """A chord rolled across the strings, `spread` beats between strings."""
        order = names if upward else list(reversed(names))
        for index, name in enumerate(order):
            self.note(beat + index * spread, name, length - index * spread, velocity - 3 * index)

    def smf(self):
        tempo = int(round(60_000_000 / self.bpm))
        track = bytearray()
        track += b"\x00\xff\x51\x03" + tempo.to_bytes(3, "big")
        last = 0
        for beat, _, data in sorted(self.events, key=lambda e: (e[0], e[1])):
            tick = int(round(beat * PPQ))
            delta = tick - last
            last = tick
            track += variable(delta) + data
        track += b"\x00\xff\x2f\x00"
        return (b"MThd" + struct.pack(">IHHH", 6, 0, 1, PPQ)
                + b"MTrk" + struct.pack(">I", len(track)) + track)


def variable(value):
    out = [value & 0x7F]
    value >>= 7
    while value:
        out.append(0x80 | (value & 0x7F))
        value >>= 7
    return bytes(reversed(out))


def lyre_harp_steel():
    """Broken chords in A minor, the harp-like wash the lyre harp is bought for."""
    p = Phrase(84)
    bars = [
        ["A3", "E4", "A4", "C5", "E5", "C5", "A4", "E4"],
        ["C4", "F4", "A4", "C5", "F5", "C5", "A4", "F4"],
        ["G3", "D4", "G4", "B4", "D5", "G5", "D5", "B4"],
    ]
    for bar, notes in enumerate(bars):
        start = 4 * bar
        p.pedal(start, True)
        for step, name in enumerate(notes):
            accent = 84 if step == 0 else 70 if step % 2 == 0 else 60
            p.note(start + 0.5 * step, name, 0.45, accent)
        p.pedal(start + 3.98, False)
    # The last chord rings until the hand damps it.
    p.pedal(12, True)
    p.roll(12, ["A3", "E4", "A4", "C5", "E5", "A5"], 3.0, 82, spread=0.07)
    p.pedal(15.8, False)
    return p


def lyre_harp_nylon():
    """A slow 3/4 song over open fifths: melody and drone on one hand each."""
    p = Phrase(96)
    melody = [
        (0, "E5", 2), (2, "D5", 1), (3, "C5", 1), (4, "D5", 1), (5, "E5", 1),
        (6, "G5", 2), (8, "E5", 1), (9, "D5", 3), (12, "C5", 3),
    ]
    drones = [(0, ["C4", "G4"]), (3, ["A3", "E4"]), (6, ["F4", "C5"]), (9, ["G3", "D4"]), (12, ["C4", "G4"])]
    for beat, names in drones:
        p.pedal(beat, True)
        for index, name in enumerate(names):
            p.note(beat + 0.04 * index, name, 2.9, 62)
        p.pedal(beat + 2.95, False)
    for beat, name, length in melody:
        p.note(beat + 0.02, name, length - 0.05, 78)
    p.note(14.9, "C5", 0.05, 1)  # end marker, inaudible
    return p


def gut_lyre():
    """Block and strum: every stroke crosses all six strings while the left
    hand rests on those the chord leaves out, so only the open ones sound."""
    p = Phrase(92)
    strings = ["D3", "E3", "G3", "A3", "B3", "D4"]
    chords = [
        ["D3", "A3", "D4"], ["D3", "A3", "D4"], ["E3", "G3", "B3"], ["D3", "G3", "B3", "D4"],
        ["E3", "A3", "D4"], ["D3", "A3", "D4"], ["G3", "B3", "D4"], ["D3", "A3", "D4"],
    ]
    for beat, chord in enumerate(chords):
        strong = beat % 2 == 0
        for index, name in enumerate(strings):
            if name in chord:
                p.note(beat + index * 0.03, name, 0.9 if beat < len(chords) - 1 else 4.0, 88 if strong else 72)
        # An off-beat flick of the upper strings, lighter.
        if beat < len(chords) - 1:
            upper = [s for s in chord if s in ("A3", "B3", "D4")]
            for index, name in enumerate(reversed(upper)):
                p.note(beat + 0.5 + index * 0.025, name, 0.45, 56)
    return p


def single_notes():
    """C4, G4, C5 and G5 left to ring: decay, two-stage sustain, sympathy."""
    p = Phrase(60)
    p.pedal(0, True)
    for index, name in enumerate(["C4", "G4", "C5", "G5"]):
        p.note(2.5 * index, name, 0.3, 90)
    p.pedal(11.5, False)
    return p


def glissando():
    """A sweep up and down all sixteen strings, then a rolled chord."""
    p = Phrase(60)
    names = ["G3", "A3", "B3", "C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5", "D5", "E5", "F5", "G5", "A5"]
    p.pedal(0, True)
    for index, name in enumerate(names):
        p.note(0.05 * index, name, 0.2, 60 + 2 * index)
    for index, name in enumerate(reversed(names)):
        p.note(2.0 + 0.045 * index, name, 0.2, 90 - 2 * index)
    p.pedal(4.2, False)
    p.pedal(4.3, True)
    p.roll(4.3, ["C4", "E4", "G4", "C5", "E5", "G5"], 3.2, 80, spread=0.05)
    p.pedal(7.6, False)
    return p


def dynamics():
    """One string, A4, from the softest to the hardest pluck, damped between."""
    p = Phrase(60)
    for index, velocity in enumerate([25, 50, 75, 100, 127]):
        p.note(1.6 * index, "A4", 1.45, velocity)
    return p


def shared_phrase():
    """A pentatonic phrase on G3 A3 B3 D4, which every preset has: the three
    stringings and bodies side by side."""
    p = Phrase(80)
    phrase = [(0, "G3", 1), (1, "B3", 0.5), (1.5, "D4", 0.5), (2, "A3", 1), (3, "B3", 1),
              (4, "D4", 0.5), (4.5, "B3", 0.5), (5, "A3", 1), (6, "G3", 2)]
    p.pedal(0, True)
    for beat, name, length in phrase:
        p.note(beat, name, length * 0.9, 80)
    p.pedal(7.9, False)
    return p


def reference_note():
    """A4 once at mezzo-forte, the note of the CC0 lyre harp reference."""
    p = Phrase(60)
    p.note(0.0, "A4", 1.9, 80)
    return p


# ---- Songs ---------------------------------------------------------------
#
# Longer public-domain pieces, played as a person plays them rather than as
# a sequencer does. The score is written in beats; Performance plays it:
# the tempo breathes along a curve of (beat, bpm) points, every onset lands
# a few milliseconds off the grid, a chord's notes are struck as one gesture
# and velocities wander a little around the line the score shapes. A fixed
# seed makes every run write the same bytes.


LEAD_IN = 0.3  # seconds


class Performance:
    def __init__(self, tempo, seed, timing=0.007, dynamics=4.0):
        self.tempo = sorted(tempo)  # [(beat, bpm)], linear between, held past the ends
        self.random = random.Random(seed)
        self.timing = timing        # seconds, the onset scatter's standard deviation
        self.dynamics = dynamics    # velocity steps, likewise
        self.events = []            # (seconds, order, bytes); note-offs sort first

    def bpm(self, beat):
        points = self.tempo
        if beat <= points[0][0]:
            return points[0][1]
        for (b0, t0), (b1, t1) in zip(points, points[1:]):
            if beat <= b1:
                return t0 + (t1 - t0) * (beat - b0) / (b1 - b0)
        return points[-1][1]

    def seconds(self, beat):
        """Clock time of a written beat: the integral of 60 / bpm, after a
        moment's silence before the first note."""
        total, previous = LEAD_IN, 0.0
        for b, _ in self.tempo[1:] + [(math.inf, 0)]:
            end = min(beat, b)
            if end > previous:
                t0, t1 = self.bpm(previous), self.bpm(end)
                slope = (t1 - t0) / (end - previous)
                total += (60.0 * (end - previous) / t0 if abs(slope) < 1e-9
                          else 60.0 / slope * math.log(t1 / t0))
                previous = end
            if beat <= b:
                break
        return total

    def scatter(self, scale=1.0):
        deviation = self.timing * scale
        return max(-2.5 * deviation, min(2.5 * deviation, self.random.gauss(0.0, deviation)))

    def velocity(self, value):
        return max(1, min(127, round(value + self.random.gauss(0.0, self.dynamics))))

    def at(self, beat, lean=0.0, scale=1.0):
        """A played onset: the written beat on the breathing clock, plus a
        deliberate lean in seconds and the hand's scatter."""
        return max(0.0, self.seconds(beat) + lean + self.scatter(scale))

    def message(self, seconds, data, order=1):
        self.events.append((seconds, order, bytes(data)))

    def play(self, onset, beats_held, beat, note, velocity, channel=0):
        """One note struck at `onset` seconds and let go `beats_held` written
        beats after `beat`, with the release scattered too."""
        release = max(onset + 0.04, self.seconds(beat + beats_held) + self.scatter(0.6))
        self.message(onset, (0x90 | channel, note, self.velocity(velocity)))
        self.message(release, (0x80 | channel, note, 0), 0)

    def smf(self):
        """A format 0 file at the piece's opening tempo, so it sits roughly on
        a DAW's bars; the performance's own time is in the ticks."""
        bpm = self.tempo[0][1]
        events = sorted(self.events, key=lambda e: (e[0], e[1]))
        # A key struck again before its last release lets go just before.
        struck = {}
        trimmed = []
        for index in range(len(events) - 1, -1, -1):
            seconds, order, data = events[index]
            key = (data[0] & 0x0F, data[1])
            if data[0] & 0xF0 == 0x90:
                struck[key] = seconds
            elif data[0] & 0xF0 == 0x80 and key in struck and seconds >= struck[key]:
                seconds = struck[key] - 0.004
            trimmed.append((seconds, order, data))
        events = sorted(trimmed, key=lambda e: (e[0], e[1]))
        ticks_per_second = PPQ * bpm / 60.0
        tempo = round(60_000_000 / bpm)
        track = bytearray(b"\x00\xff\x51\x03" + tempo.to_bytes(3, "big"))
        last = 0
        for seconds, _, data in events:
            tick = max(last, round(seconds * ticks_per_second))
            track += variable(tick - last) + data
            last = tick
        track += b"\x00\xff\x2f\x00"
        return (b"MThd" + struct.pack(">IHHH", 6, 0, 1, PPQ)
                + b"MTrk" + struct.pack(">I", len(track)) + bytes(track))


def breathe(bpm, phrases, swell=0.03, settle=0.07, last=0.22):
    """A tempo curve over phrases of (first beat, length in beats): each
    pushes on a little towards its middle and settles at its end; the last
    one broadens into the close."""
    points = []
    for index, (start, length) in enumerate(phrases):
        final = index == len(phrases) - 1
        # After a breath the hand takes the tempo up again within a beat.
        points += [(start if index == 0 else start + 0.75, bpm), (start + 0.55 * length, bpm * (1 + swell)),
                   (start + 0.85 * length, bpm), (start + length, bpm * (1 - (last if final else settle)))]
    return points


def canon_in_c():
    """Pachelbel's Canon (c. 1680), set for the steel lyre harp in C: the
    ground bass and its broken chords in the left hand, and over them the
    canon's first three entries, each richer than the last: a bare
    statement of the ground, the long notes, the walking quarters, and the
    long notes again an octave up, before a slow rolled C."""
    ground = [  # (bass, the three strings broken above it)
        ("C3", "G3", "C4", "E4"), ("G3", "B3", "D4", "G4"), ("A3", "C4", "E4", "A4"), ("E3", "G3", "B3", "E4"),
        ("F3", "A3", "C4", "F4"), ("C3", "G3", "C4", "E4"), ("F3", "A3", "C4", "F4"), ("G3", "B3", "D4", "G4"),
    ]
    long_notes = ["E5", "D5", "C5", "B4", "A4", "G4", "A4", "B4"]
    walking = ["C5", "E5", "G5", "F5", "E5", "C5", "E5", "D5", "C5", "A4", "C5", "G4", "F4", "A4", "G4", "F4"]
    cycles = 4
    p = Performance(breathe(60, [(16 * c, 16) for c in range(cycles)] + [(64, 4)],
                            swell=0.035, settle=0.05, last=0.45), seed=1680, timing=0.009)
    for cycle, loud in enumerate((58, 68, 78, 86)):
        for chord, (bass, *broken) in enumerate(ground):
            at = 16 * cycle + 2 * chord
            rise = 5 * math.sin(math.pi * (chord + 0.5) / 8)
            # The bass leads the hand by a hair, as a harpist's thumb does.
            p.play(p.at(at, lean=-0.01), 1.97, at, n(bass), loud + rise)
            if cycle == 0:
                for index, name in enumerate(broken):
                    p.play(p.at(at, lean=0.07 + 0.06 * index), 1.9, at, n(name), loud - 10 + rise - 2 * index)
            else:
                for index, name in enumerate(broken):
                    beat = at + 0.5 * (index + 1)
                    p.play(p.at(beat), 2 - 0.5 * (index + 1) - 0.03, beat, n(name),
                           loud - 12 + rise + (2 if index == 1 else 0))
            if cycle in (1, 3):
                name = long_notes[chord]
                if cycle == 3:
                    name = name[:-1] + str(int(name[-1]) + 1)
                p.play(p.at(at, lean=0.008), 1.95, at, n(name), loud + 8 + rise)
            elif cycle == 2:
                for half in range(2):
                    beat = at + half
                    p.play(p.at(beat, lean=0.008), 0.95, beat, n(walking[2 * chord + half]),
                           loud + 6 + rise + (3 if half == 0 else -2))
    # The close: C rolled up the harp from the bass, let ring.
    for index, name in enumerate(["C3", "G3", "C4", "E4", "G4", "C5", "E5", "G5", "C6"]):
        p.play(p.at(64, lean=0.075 * index, scale=0.6), 5.5, 64, n(name), 80 - 2 * index)
    return p


def scarborough_fair():
    """Scarborough Fair, the English ballad, on the gut lyre in A dorian:
    the hand strums the chord across the strings on each bar's first beat
    while the other picks the tune, damping at the bar. An Am strum to set
    the mode, the verse with the chords bare, then again with a flick of
    the upper strings on the third beat, and a slow last strum."""
    tune = [
        [(0, "A3", 2), (2, "A3", 1)], [(0, "E4", 2), (2, "E4", 1)],
        [(0, "B3", 1.5), (1.5, "C4", 0.5), (2, "B3", 1)], [(0, "A3", 3)],
        [(1, "E4", 1), (2, "G4", 1)], [(0, "A4", 2), (2, "G4", 1)],
        [(0, "E4", 1), (1, "F#4", 1), (2, "D4", 1)], [(0, "E4", 3)],
        [(0, "A4", 2), (2, "A4", 1)], [(0, "G4", 2), (2, "E4", 1)],
        [(0, "E4", 1), (1, "D4", 1), (2, "C4", 1)], [(0, "B3", 1), (1, "G3", 2)],
        [(0, "A3", 2), (2, "E4", 1)], [(0, "D4", 2), (2, "C4", 1)],
        [(0, "B3", 1), (1, "A3", 1), (2, "G3", 1)], [(0, "A3", 3)],
    ]
    chords = {"Am": ["E3", "A3", "C4", "E4"], "G": ["D3", "G3", "B3", "D4"], "C": ["E3", "G3", "C4", "E4"],
              "D": ["D3", "F#3", "A3", "D4"], "Em": ["E3", "G3", "B3", "E4"]}
    harmony = ["Am", "Am", "G", "Am", "C", "D", "G", "Am", "Am", "C", "G", "Em", "Am", "D", "G", "Am"]
    intro = 6
    verses = [(intro, 92, 74, False), (intro + 48, 96, 82, True)]
    tempo = breathe(88, [(0, intro)], last=0.08) + breathe(92, [(intro + 12 * k, 12) for k in range(4)]) \
        + breathe(96, [(intro + 48 + 12 * k, 12) for k in range(4)], last=0.4)
    p = Performance(tempo, seed=1600, timing=0.01)

    def strum(beat, names, velocity, held, spread=0.022, skip=()):
        onset = p.at(beat)
        for index, name in enumerate(x for x in names if x not in skip):
            p.play(onset + spread * index, held, beat, n(name), velocity - 2 * index)

    def flick(beat, names, velocity, held):
        onset = p.at(beat, scale=1.2)
        for index, name in enumerate(reversed(names[1:])):
            p.play(onset + 0.018 * index, held, beat, n(name), velocity - 2 * index)

    for bar in range(2):
        strum(3 * bar, chords["Am"], 66 + 6 * bar, 2.9)
        flick(3 * bar + 2, chords["Am"], 48, 0.9)
    for start, _, loud, flicks in verses:
        for bar, notes in enumerate(tune):
            at = start + 3 * bar
            rise = 6 * math.sin(math.pi * ((bar % 4) + 0.5) / 4)
            melody = {name for beat, name, _ in notes if beat == 0}
            final = flicks and bar == 15
            if final:
                strum(at, chords["Am"] + ["A4"], loud, 6.0, spread=0.06)
                continue
            strum(at, chords[harmony[bar]], loud - 14 + rise, 2.95, skip=melody)
            if flicks:
                flick(at + 2, chords[harmony[bar]], loud - 30 + rise, 0.95)
            for beat, name, length in notes:
                p.play(p.at(at + beat, lean=0.012), length * 0.97, at + beat, n(name),
                       loud + rise + (4 if beat == 0 else 0))
    return p


SONGS = {
    # file in Docs/audio/midi/songs/: (phrase, the preset it is written for)
    "canon-in-c.mid": (canon_in_c, 0),
    "scarborough-fair.mid": (scarborough_fair, 2),
}


DEMOS = {
    "00-lyre-harp.mid": lyre_harp_steel,
    "01-nylon-lyre-harp.mid": lyre_harp_nylon,
    "02-gut-lyre.mid": gut_lyre,
    "more/01-single-notes.mid": single_notes,
    "more/02-glissando.mid": glissando,
    "more/03-dynamics.mid": dynamics,
    "more/04-shared-phrase.mid": shared_phrase,
    "more/05-reference-a4.mid": reference_note,
}
DEMOS.update({"songs/" + name: build for name, (build, _) in SONGS.items()})


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("root", nargs="?",
                        default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Docs", "audio", "midi"))
    parser.add_argument("--check", action="store_true", help="fail unless the committed files match")
    args = parser.parse_args()
    if args.check:
        stale = []
        for name, build in DEMOS.items():
            path = os.path.join(args.root, name)
            if not os.path.isfile(path) or open(path, "rb").read() != build().smf():
                stale.append(name)
        if stale:
            sys.exit(f"Docs/audio/midi differs from {os.path.basename(__file__)} in: {', '.join(stale)}")
        print(f"{len(DEMOS)} demo phrases match {os.path.basename(__file__)}")
        return
    for name, build in DEMOS.items():
        path = os.path.join(args.root, name)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as file:
            file.write(build().smf())
        print(path)


if __name__ == "__main__":
    main()
