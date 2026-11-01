#!/usr/bin/env python3
"""Render the songs in Docs/audio/midi/songs/ and mix their parts.

Each part plays on the lyre Tools/write_preset_midi.py's SONGS gives it,
through RenderAculyreDemo; the parts are panned and trimmed as SONGS says,
summed, and the mix is scaled so its peak sits at -1 dBFS. Output is 48 kHz
24-bit stereo, <out-dir>/<song>.wav, never committed.

    python3 Tools/render_songs.py build <out-dir>
"""

import pathlib
import subprocess
import sys
import tempfile

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from write_preset_midi import SONGS  # noqa: E402

SONG_DIR = pathlib.Path(__file__).resolve().parent.parent / "Docs/audio/midi/songs"

def read_wav24(path):
    data = pathlib.Path(path).read_bytes()
    offset = 12
    while data[offset:offset + 4] != b"data":
        offset += 8 + int.from_bytes(data[offset + 4:offset + 8], "little")
    size = int.from_bytes(data[offset + 4:offset + 8], "little")
    raw = np.frombuffer(data[offset + 8:offset + 8 + size], dtype=np.uint8).reshape(-1, 3).astype(np.int32)
    value = raw[:, 0] | raw[:, 1] << 8 | raw[:, 2] << 16
    value = np.where(value >= 1 << 23, value - (1 << 24), value)
    return (value / 8388608.0).reshape(-1, 2)


def write_wav24(path, audio, rate=48000):
    pcm = np.clip(np.round(audio * 8388607), -8388608, 8388607).astype("<i4")
    frames = pcm.reshape(-1).view(np.uint8).reshape(-1, 4)[:, :3].tobytes()
    header = (b"RIFF" + (36 + len(frames)).to_bytes(4, "little") + b"WAVEfmt "
              + (16).to_bytes(4, "little") + (1).to_bytes(2, "little") + (2).to_bytes(2, "little")
              + rate.to_bytes(4, "little") + (rate * 6).to_bytes(4, "little") + (6).to_bytes(2, "little")
              + (24).to_bytes(2, "little") + b"data" + len(frames).to_bytes(4, "little"))
    pathlib.Path(path).write_bytes(header + frames)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    render = pathlib.Path(sys.argv[1]) / "RenderAculyreDemo"
    out = pathlib.Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    for song, (_, parts) in SONGS.items():
        audio = []
        with tempfile.TemporaryDirectory() as scratch:
            for part, (preset, pan, trim) in parts.items():
                wav = pathlib.Path(scratch) / f"{part}.wav"
                subprocess.run([str(render), str(preset), str(SONG_DIR / f"{song}-{part}.mid"), str(wav),
                                "--tail", "4"], check=True, stdout=subprocess.DEVNULL)
                audio.append(read_wav24(wav) * 10 ** (trim / 20) * [min(1.0, 1.0 - pan), min(1.0, 1.0 + pan)])
        # Each part ends at its own last event; the shorter are padded.
        mix = np.zeros((max(len(a) for a in audio), 2))
        for part in audio:
            mix[:len(part)] += part
        mix *= 10 ** (-1 / 20) / np.abs(mix).max()
        write_wav24(out / f"{song}.wav", mix)
        print(out / f"{song}.wav")


if __name__ == "__main__":
    main()
