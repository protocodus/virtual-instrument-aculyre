#!/usr/bin/env python3
"""Writes Aculyre's demo phrases as standard MIDI files.

    python3 Tools/write_demo_midi.py [output-directory]

MIDI map, as the phrases use it: a note plays the open string tuned to it (a
note no string is tuned to is ignored), velocity is the pluck's force, a
note-off is the hand damping that string, and CC64 holds released strings
ringing. Lyre players let strings ring and damp them by hand, so the phrases
hold the pedal through a harmony and lift it where the hand would damp.

Strings: the lyre harps (presets 0 and 1) are C major from G3 to A5; the gut
lyre (preset 2) is D3 E3 G3 A3 B3 D4. Every phrase is original.
"""

import os
import struct
import sys

PPQ = 480

NOTE = {}
for octave in range(1, 8):
    for name, offset in (("C", 0), ("D", 2), ("E", 4), ("F", 5), ("G", 7), ("A", 9), ("B", 11)):
        NOTE[f"{name}{octave}"] = 12 * (octave + 1) + offset


def n(name):
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

    def write(self, path):
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
        with open(path, "wb") as file:
            file.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, PPQ))
            file.write(b"MTrk" + struct.pack(">I", len(track)) + track)


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


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "Docs", "audio", "midi")
    for name, build in DEMOS.items():
        path = os.path.join(root, name)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        build().write(path)
        print(path)


if __name__ == "__main__":
    main()
