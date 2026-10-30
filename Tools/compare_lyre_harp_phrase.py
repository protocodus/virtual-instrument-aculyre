#!/usr/bin/env python3
"""Replays a recorded lyre harp phrase and compares its long-term spectrum.

    python3 Tools/compare_lyre_harp_phrase.py --midi phrase.mid
    build/RenderAculyreDemo 0 phrase.mid model.wav
    python3 Tools/compare_lyre_harp_phrase.py <recording.wav> <model.wav> ...

The recording is Freesound 649925 (a lyre harp phrase, CC BY), decoded to
WAV; it is fetched for analysis and never committed. PHRASE is its notes as
transcribed on 2026-10-30: onsets from the spectral flux's peaks (4096-point
frames, hop 512, a peak over 0.12 of the largest, merged within 0.15 s) and
each pitch from a harmonic product spectrum over 50-350 ms. Every note is
written at velocity 90 and held 2 s, so the player's dynamics are not
replayed.

Protocol: from the first sample within 30 dB of the peak, 7 s of the channel
mean, 8192-point Hann frames with hop 2048, power summed into third-octaves
from 198 Hz to 10 kHz; shape = the rms of model minus recording after
removing the mean, so level, microphone gain and loudness drop out.
"""
import argparse
import struct
import sys

import numpy as np
from scipy.io import wavfile

# (seconds, MIDI note): A4 G4 A4 B4 A4 D4 E4 E4.
PHRASE = [(0.81, 69), (1.35, 67), (2.98, 69), (3.54, 71), (4.10, 69), (4.70, 62), (5.89, 64), (6.34, 64)]
BANDS = 1000.0 * 2.0 ** (np.arange(-7, 11) / 3.0)


def write_midi(path):
    ticks = 960  # per second at 120 bpm and 480 per quarter
    events = sorted([(round(t * ticks), 0x90, n, 90) for t, n in PHRASE]
                    + [(round((t + 2.0) * ticks), 0x80, n, 0) for t, n in PHRASE])

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


def spectrum(path):
    rate, x = wavfile.read(path)
    if x.dtype.kind == "i":
        x = x / float(np.iinfo(x.dtype).max)
    x = np.asarray(x, dtype=float)
    x = x if x.ndim == 1 else x.mean(axis=1)
    start = int(np.argmax(np.abs(x) > np.abs(x).max() * 10 ** (-30 / 20)))
    x = x[start:start + int(7.0 * rate)]
    power = np.zeros(4097)
    for i in range(0, len(x) - 8192, 2048):
        power += np.abs(np.fft.rfft(x[i:i + 8192] * np.hanning(8192))) ** 2
    f = np.fft.rfftfreq(8192, 1 / rate)
    return np.array([10 * np.log10(power[(f >= b * 2 ** (-1 / 6)) & (f < b * 2 ** (1 / 6))].sum() + 1e-20)
                     for b in BANDS])


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("recording", nargs="?")
    parser.add_argument("models", nargs="*")
    parser.add_argument("--midi", help="write the phrase as a MIDI file and stop")
    args = parser.parse_args()
    if args.midi:
        write_midi(args.midi)
        return 0
    if not args.recording or not args.models:
        parser.error("a recording and at least one model render are required unless --midi")
    reference = spectrum(args.recording)
    print("band (Hz): " + " ".join(f"{b:6.0f}" for b in BANDS))
    for path in args.models:
        d = spectrum(path) - reference
        d -= d.mean()
        print(f"{path}: shape rms {np.sqrt(np.mean(d ** 2)):.1f} dB")
        print("  model - recording: " + " ".join(f"{v:+6.1f}" for v in d))
    return 0


if __name__ == "__main__":
    sys.exit(main())
