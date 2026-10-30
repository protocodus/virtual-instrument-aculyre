#!/usr/bin/env python3
"""Replays the gut lyre recording's melody and compares its notes' partials.

    python3 Tools/compare_gut_lyre_phrase.py --midi melody.mid
    build/RenderAculyreDemo 2 melody.mid model.wav
    python3 Tools/compare_gut_lyre_phrase.py <recording.wav> <model.wav> ...

The recording is Freesound 191883 (a gut-strung lyre, CC0), decoded to WAV;
it is fetched for analysis and never committed. MELODY is its notes as
transcribed on 2026-10-30: onsets from the spectral flux's peaks (4096-point
frames, hop 512, a peak over 0.12 of the largest, merged within 0.12 s;
one onset at 13.82 s with no clear pitch is left out) and each fundamental
from a harmonic sum over its first five partials, 30-280 ms after the onset.
The lyre is tuned about 3.3 semitones below the model's G major strings, so
the melody runs E4 D4 C4 B3 A3 G3 F#3 and B4 on them: each note is written
3.3 semitones up, rounded, at velocity 90, held to the next onset.

Protocol, per note: the onsets are aligned at the first sample within 30 dB
of the file's peak. Partials: over 30-250 ms after the onset, the channel
mean, Hann window, 65536-point FFT; partial k is the strongest bin within
-3 % / +4 % of k f0 (stretch room for gut's inharmonicity), k = 1 to 12, in
dB against partial 1. Decays: 40 ms Hann frames hopped by 20 ms from 80 ms
after the onset to the next onset or 700 ms, whichever is first; the slope
of a line through each partial's level, partial 1 alone and the median of
partials 2-6. Reported: the median over notes of each partial's level, the
rms of model minus recording over partials 2-12, and the median decays.
"""
import argparse
import struct
import sys

import numpy as np
from scipy.io import wavfile

# (seconds, fundamental Hz as recorded).
MELODY = [(0.38, 272.0), (1.18, 242.5), (1.89, 271.5), (2.58, 242.0), (3.29, 216.0), (4.01, 407.5),
          (4.69, 242.0), (5.39, 216.0), (6.06, 204.0), (6.75, 271.5), (7.46, 242.0), (8.17, 216.0),
          (8.89, 204.0), (9.57, 182.0), (10.23, 159.0), (10.92, 271.5), (11.64, 242.5), (12.35, 216.0),
          (13.09, 204.5), (14.61, 160.0)]
SHIFT = 3.3  # semitones from the recording's tuning to the model's strings
PARTIALS = 12


def model_note(f0):
    return int(round(69 + 12 * np.log2(f0 / 440.0) + SHIFT))


def write_midi(path):
    ticks = 960  # per second at 120 bpm and 480 per quarter
    events = []
    for i, (t, f0) in enumerate(MELODY):
        end = MELODY[i + 1][0] if i + 1 < len(MELODY) else t + 1.2
        events += [(round(t * ticks), 0x90, model_note(f0), 90), (round(end * ticks), 0x80, model_note(f0), 0)]
    events.sort()

    def vlq(value):
        out = [value & 0x7F]
        value >>= 7
        while value:
            out.insert(0, (value & 0x7F) | 0x80)
            value >>= 7
        return bytes(out)

    track, last = b"\x00\xff\x51\x03" + (500000).to_bytes(3, "big"), 0
    for tick, status, note, velocity in events:
        track += vlq(tick - last) + bytes([status, note, velocity])
        last = tick
    track += b"\x00\xff\x2f\x00"
    with open(path, "wb") as f:
        f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, 480) + b"MTrk" + struct.pack(">I", len(track)) + track)


def peak_db(power, freqs, f):
    band = (freqs > 0.97 * f) & (freqs < 1.04 * f)
    return 10 * np.log10(power[band].max() + 1e-30)


def measure(path, model):
    rate, x = wavfile.read(path)
    if x.dtype.kind == "i":
        x = x / float(np.iinfo(x.dtype).max)
    x = np.asarray(x, dtype=float)
    x = x if x.ndim == 1 else x.mean(axis=1)
    offset = np.argmax(np.abs(x) > np.abs(x).max() * 10 ** (-30 / 20)) / rate - MELODY[0][0]
    levels, first, rest = [], [], []
    for i, (t, f0) in enumerate(MELODY):
        if model:
            f0 = 440.0 * 2 ** ((model_note(f0) - 69) / 12)
        start = t + offset
        stop = min(MELODY[i + 1][0] + offset if i + 1 < len(MELODY) else start + 1.2, start + 0.7)
        seg = x[int((start + 0.03) * rate):int((start + 0.25) * rate)]
        power = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), 65536)) ** 2
        freqs = np.fft.rfftfreq(65536, 1 / rate)
        p = np.array([peak_db(power, freqs, k * f0) for k in range(1, PARTIALS + 1)])
        levels.append(p - p[0])
        width, hop = int(0.04 * rate), int(0.02 * rate)
        freqs = np.fft.rfftfreq(8192, 1 / rate)
        track = []
        for a in range(int((start + 0.08) * rate), int(stop * rate) - width, hop):
            power = np.abs(np.fft.rfft(x[a:a + width] * np.hanning(width), 8192)) ** 2
            track.append([peak_db(power, freqs, k * f0) for k in range(1, 7)])
        if len(track) > 8:
            track = np.array(track)
            times = np.arange(len(track)) * 0.02
            slopes = [-np.polyfit(times, track[:, k], 1)[0] for k in range(6)]
            first.append(slopes[0])
            rest.append(np.median(slopes[1:]))
    return np.median(np.array(levels), axis=0), np.median(first), np.median(rest)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("recording", nargs="?")
    parser.add_argument("models", nargs="*")
    parser.add_argument("--midi", help="write the melody as a MIDI file and stop")
    args = parser.parse_args()
    if args.midi:
        write_midi(args.midi)
        return 0
    if not args.recording or not args.models:
        parser.error("a recording and at least one model render are required unless --midi")
    reference, ref_first, ref_rest = measure(args.recording, False)
    print("partial:             " + " ".join(f"{k:6d}" for k in range(2, PARTIALS + 1)))
    print("recording, dB re 1:  " + " ".join(f"{v:+6.1f}" for v in reference[1:]))
    print(f"  decays: partial 1 {ref_first:.1f} dB/s, partials 2-6 {ref_rest:.1f} dB/s")
    for path in args.models:
        levels, first, rest = measure(path, True)
        d = levels[1:] - reference[1:]
        print(f"{path}: partials 2-{PARTIALS} rms {np.sqrt(np.mean(d ** 2)):.1f} dB")
        print("  dB re 1:           " + " ".join(f"{v:+6.1f}" for v in levels[1:]))
        print(f"  decays: partial 1 {first:.1f} dB/s, partials 2-6 {rest:.1f} dB/s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
