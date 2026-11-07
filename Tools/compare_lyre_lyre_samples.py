#!/usr/bin/env python3
"""Measures Jordan Stevenson's Lyre Lyre samples and Aculyre's steel lyre
harp note by note and layer by layer.

    for v in 0.35 0.7 1.0; do
        build/RenderAculyreNotes 0 <renders>/$v --rate 44100 --strings 55,72 --velocity $v
    done
    python3 Tools/compare_lyre_lyre_samples.py <Samples> <renders>

<Samples> is the "Lyre Lyre Samples" folder of the Kontakt library Lyre
Lyre (Jordan Stevenson, jscomposition.nz): a 10-string steel lyre harp
(the maker's photo: a mahogany box, a 25 mm soundhole, a curved beech
saddle) finger-plucked at three dynamics, soft, medium and loud, with two
or three round robins each, 44.1 kHz 24-bit mono, every note of the
chromatic scale from G3 to C5 (the sharps on strings retuned; "8 <note>"
are the strings an octave up the lyre). It is the owner's copy, shared for
analysis and never committed. The model plays its own diatonic strings G3
to C5 (--strings 55,72), the eleven naturals the lyre has.

The protocol is compare_lyre_harp_samples.py's (onset, partials, decays
over 0.08-0.6 s, pluck point, third-octave shape); each layer of the
recording is set beside one velocity of the model. Added here:
- Brightness: the power above 2.5 kHz against all of it over 30-330 ms
  from the onset, in dB.
- Level: RMS over 30-330 ms, in dB; between layers the step in level and
  the step in brightness per dB of level, the recording's law of touch.
- Evenness across the range: over the naturals, the SD of each note's
  sustain (its 300-1000 ms RMS against its 0-300 ms RMS) and of its
  brightness, the recording (medians over its takes) against the model.

The recording's gain is unknown and its layers' names are the player's,
so levels compare steps between layers, never the absolute level.
"""

import glob
import os
import re
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from compare_lyre_harp_samples import BANDS, db, measure, midi, pluck_point, thirds  # noqa: E402

NATURALS = ["G3", "A3", "B3", "C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5"]
LAYERS = ["soft", "medium", "loud"]
VELOCITIES = ["0.35", "0.7", "1.0"]


def sample_files(samples, note):
    name = note[:-1]
    octave = "8 " if midi(note) >= 67 else ""
    files = sorted(glob.glob(os.path.join(samples, f"{octave}{name} *.wav")))
    if midi(note) >= 67:
        files += sorted(glob.glob(os.path.join(samples, f"{name} *.wav")))
    return {layer: [f for f in files if re.search(rf" {layer}\.\d", f)] for layer in LAYERS}


def extras(m):
    x, rate, start = m["signal"], m["rate"], m["start"]
    seg = x[start + int(0.03 * rate):start + int(0.33 * rate)]
    power = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
    f = np.fft.rfftfreq(len(seg), 1 / rate)
    brightness = 10 * np.log10(power[f >= 2500].sum() / power.sum())
    rms = lambda t0, t1: db(np.sqrt(np.mean(x[start + int(t0 * rate):start + int(t1 * rate)] ** 2)))
    return dict(brightness=float(brightness), level=float(rms(0.03, 0.33)),
                sustain=float(rms(0.3, 1.0) - rms(0.0, 0.3)))


def median(values):
    values = [v for v in values if v is not None and np.isfinite(v)]
    return float(np.median(values)) if values else float("nan")


def upper(m):
    return median([m["decays"].get(k) for k in range(2, 7)])


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    samples, renders = sys.argv[1:]
    rec = {}
    for note in NATURALS:
        files = sample_files(samples, note)
        rec[note] = {}
        for layer in LAYERS:
            ms = [measure(f, note) for f in files[layer]]
            for m in ms:
                m.update(extras(m))
            rec[note][layer] = ms
    mod = {}
    for note in NATURALS:
        mod[note] = {}
        for v in VELOCITIES:
            m = measure(os.path.join(renders, v, f"{midi(note)}.wav"), note)
            m.update(extras(m))
            mod[note][v] = m

    for layer, v in zip(LAYERS, VELOCITIES):
        print(f"\n== recording {layer} (medians over takes) | model velocity {v}")
        print("note   f0 (cents)  B       H1 dB/s H2-6 dB/s pluck  bright  sust | B       H1 dB/s H2-6 dB/s pluck  bright  sust")
        h1, h26 = [], []
        for note in NATURALS:
            r, m = rec[note][layer], mod[note][v]
            if not r:
                continue
            f0 = median([x["f0"] for x in r])
            cents = 1200 * np.log2(f0 / (440 * 2 ** ((midi(note) - 69) / 12)))
            r1, m1 = median([x["decays"].get(1) for x in r]), m["decays"].get(1)
            r26, m26 = median([upper(x) for x in r]), upper(m)
            if m1 and np.isfinite(r1):
                h1.append(np.log2(max(m1, 0.5) / max(r1, 0.5)))
            if np.isfinite(m26) and np.isfinite(r26):
                h26.append(np.log2(max(m26, 0.5) / max(r26, 0.5)))
            print(f"{note:4s} {f0:8.2f} ({cents:+5.1f}) {median([x['B'] for x in r]):.1e} {r1:7.1f} {r26:8.1f}"
                  f"  {median([pluck_point(x) for x in r]):.3f} {median([x['brightness'] for x in r]):6.1f}"
                  f" {median([x['sustain'] for x in r]):5.1f} | {m['B']:.1e} {m1 or float('nan'):7.1f} {m26:8.1f}"
                  f"  {pluck_point(m):.3f} {m['brightness']:6.1f} {m['sustain']:5.1f}")
        # The rms as compare_lyre_harp_samples.py; the median besides, since
        # one fundamental caught rising on a two-plane beat (a negative
        # decay, on the recording and on the model) can own the rms.
        print(f"decay log2(model / recording): fundamentals rms {np.sqrt(np.mean(np.square(h1))):.2f}, "
              f"median {np.median(h1):+.2f}; partials 2-6 rms {np.sqrt(np.mean(np.square(h26))):.2f}, "
              f"median {np.median(h26):+.2f}")
        recs = [m for note in NATURALS for m in rec[note][layer]]
        mods = [mod[note][v] for note in NATURALS]
        for t0, t1 in ((0.0, 0.3), (0.3, 1.5)):
            a = 10 * np.log10(np.mean([thirds(m, t0, t1) for m in recs], axis=0))
            b = 10 * np.log10(np.mean([thirds(m, t0, t1) for m in mods], axis=0))
            print(f"third-octaves {t0}-{t1} s, model - recording (dB): "
                  + " ".join(f"{band:.0f}:{y - x - np.mean(b - a):+.1f}" for band, x, y in zip(BANDS, a, b)))
            print(f"shape rms 200 Hz-10 kHz: {np.std(b - a):.1f} dB")
        rs = [median([x["sustain"] for x in rec[note][layer]]) for note in NATURALS]
        rb = [median([x["brightness"] for x in rec[note][layer]]) for note in NATURALS]
        print(f"evenness over the naturals, SD: sustain recording {np.std(rs):.1f} dB, model "
              f"{np.std([m['sustain'] for m in mods]):.1f}; brightness recording {np.std(rb):.1f} dB, model "
              f"{np.std([m['brightness'] for m in mods]):.1f}")

    print("\n== layers: level and brightness, medians over the naturals")
    print("layer/velocity   recording level  bright | model level  bright")
    for layer, v in zip(LAYERS, VELOCITIES):
        rl = median([median([x["level"] for x in rec[note][layer]]) for note in NATURALS])
        rb = median([median([x["brightness"] for x in rec[note][layer]]) for note in NATURALS])
        ml = median([mod[note][v]["level"] for note in NATURALS])
        mb = median([mod[note][v]["brightness"] for note in NATURALS])
        print(f"{layer:7s} {v:6s}   {rl:8.1f} {rb:8.1f}      | {ml:8.1f} {mb:8.1f}")
    print("steps between layers, per note then median: level dB, brightness dB, brightness per dB of level")
    for (la, va), (lb, vb) in (((LAYERS[0], VELOCITIES[0]), (LAYERS[1], VELOCITIES[1])),
                               ((LAYERS[1], VELOCITIES[1]), (LAYERS[2], VELOCITIES[2]))):
        rl, rb, ml, mb = [], [], [], []
        for note in NATURALS:
            if rec[note][la] and rec[note][lb]:
                rl.append(median([x["level"] for x in rec[note][lb]]) - median([x["level"] for x in rec[note][la]]))
                rb.append(median([x["brightness"] for x in rec[note][lb]])
                          - median([x["brightness"] for x in rec[note][la]]))
            ml.append(mod[note][vb]["level"] - mod[note][va]["level"])
            mb.append(mod[note][vb]["brightness"] - mod[note][va]["brightness"])
        print(f"{la} -> {lb}: recording {median(rl):+.1f} dB, {median(rb):+.1f} dB, {median(rb) / median(rl):.2f}"
              f" | model {va} -> {vb}: {median(ml):+.1f} dB, {median(mb):+.1f} dB, {median(mb) / median(ml):.2f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
