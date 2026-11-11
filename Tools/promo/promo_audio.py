"""Aculyre promo: the last strains of three performed pieces, each to its
final chord, cut from full-piece renders, joined and mastered as one track.

Run in the work directory holding out/<piece>/<piece>.wav and .csv from
SongRender. The renders already carry the patch's own room; nothing else is
added: no reverb, EQ or compression beyond this mastering. Each excerpt
starts a little before its first downbeat (a 40 ms fade-in) and runs to the
end of its tail (the last 0.9 s faded); excerpts are matched on integrated
BS.1770 loudness, overlapped by 0.35 s, then the whole is brought to -16 LUFS
with true peak <= -1.15 dBTP (4x) by a 5 ms look-ahead limiter. Writes
out/<set>/<set>.wav, timeline.json and report.json; <set> is the film
(promo) or one short.

    python3 promo_audio.py promo | short-minuet | short-kesh | short-butterfly
"""
import json
import os
import pathlib
import sys

import numpy as np
import pyloudnorm as pyln
import soundfile as sf
from scipy.ndimage import minimum_filter1d
from scipy.signal import resample_poly

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from repertoire import PIECES  # noqa: E402

sr = 48000
meter = pyln.Meter(sr)
VERSION = '0.13.5d1'
# piece, title, credit line, patch shown, the beat its excerpt starts on
MIN = dict(piece='minuet', title='Minuet in G', sub='Christian Petzold · 1725', patch='Nylon Lyre Harp', how='finger · levers on auto')
KES = dict(piece='kesh', title='The Kesh', sub='Irish jig · traditional', patch='Gut Lyre', how='finger · 16 gut strings')
BUT = dict(piece='butterfly', title='The Butterfly', sub='Irish slip jig · traditional', patch='Lyre Harp', how='finger · 27 steel strings')
# name -> ([excerpt with its first beat], lead-in s); a short is one excerpt under 30 s
SETS = {
    'promo': ([dict(MIN, beat=48), dict(KES, beat=48), dict(BUT, beat=72)], 0.6),
    'short-minuet': ([dict(MIN, beat=48)], 0.1),
    'short-kesh': ([dict(KES, beat=96)], 0.1),
    'short-butterfly': ([dict(BUT, beat=108)], 0.1),
}
OVERLAP, FADE_IN, FADE_OUT, TAIL, PRE = 0.35, 0.04, 0.9, 2.6, 0.12


def tp(sig):
    return 20 * np.log10(np.abs(resample_poly(sig, 4, 1, axis=0)).max())


def notes_of(performance):
    """[onset, release, pitch, velocity] from the performance's own events."""
    on, out = {}, []
    for seconds, order, data, _ in sorted(performance.events, key=lambda e: (e[0], e[1])):
        if data[0] & 0xF0 == 0x90:
            on[data[1]] = (seconds, data[2])
        elif data[1] in on:
            start, vel = on.pop(data[1])
            out.append([round(start, 4), round(max(seconds, start + 0.04), 4), data[1], vel])
    return sorted(out)


def trace_of(path):
    lines = pathlib.Path(path).read_text().split('\n')
    strings = [int(x) for x in lines[0].split(',')[1:]]
    rows = [r.split(',')[1:] for r in lines[1:] if r]
    # energy as dB re 1 (m/s)^2, rounded; state 0 sympathy only, 1 plucked, 2 held
    energy = [[round(10 * np.log10(float(c.split(':')[0]) + 1e-12)) for c in r] for r in rows]
    state = [[int(c.split(':')[1]) for c in r] for r in rows]
    return strings, energy, state


def main():
    name = sys.argv[1] if len(sys.argv) > 1 else 'promo'
    segments, lead_in = SETS[name]
    clips, report, at = [], dict(segments=[]), lead_in
    for seg in segments:
        perf = PIECES[seg['piece']]()
        notes = notes_of(perf)
        x = sf.read(f"out/{seg['piece']}/{seg['piece']}.wav")[0]
        s0 = perf.seconds(seg['beat']) - PRE
        s1 = min(len(x) / sr, max(n[1] for n in notes) + TAIL)
        a, b = int(s0 * sr), int(s1 * sr)
        clip = x[a:b].copy()
        env = np.ones(b - a)
        fi, fo = int(FADE_IN * sr), int(FADE_OUT * sr)
        env[:fi] = np.linspace(0, 1, fi) ** 2
        env[-fo:] *= np.linspace(1, 0, fo) ** 2
        clip *= env[:, None]
        g = 10 ** ((-16 - meter.integrated_loudness(clip)) / 20)
        clip *= g
        clips.append((at, clip))
        report['segments'].append(dict(title=seg['title'], patch=seg['patch'], piece_s=[round(s0, 3), round(s1, 3)],
                                       promo_s=[round(at, 3), round(at + (b - a) / sr, 3)], match_gain_db=round(20 * np.log10(g), 2)))
        seg.update(start=at, end=at + (b - a) / sr, src0=s0, notes=notes)
        at += (b - a) / sr - OVERLAP
    os.makedirs(f'out/{name}', exist_ok=True)
    total = int((segments[-1]['end'] + (1.2 if len(segments) > 1 else 0.0)) * sr)
    mix = np.zeros((total, 2))
    for s, c in clips:
        i = int(round(s * sr))
        mix[i:i + len(c)] += c[:total - i]
    mix *= 10 ** ((-16 - meter.integrated_loudness(mix)) / 20)
    report['true_peak_before_limit'] = round(float(tp(mix)), 2)
    up = np.abs(resample_poly(mix, 4, 1, axis=0)).max(1).reshape(-1, 4).max(1)[:total]
    need = np.minimum(1, 10 ** (-1.15 / 20) / np.maximum(up, 1e-9))
    la = int(.005 * sr)
    target = minimum_filter1d(need, size=2 * la + 1, origin=-la)
    gain, rel, cur = np.ones(total), np.exp(-1 / (0.12 * sr)), 1.0
    for i in range(total):
        cur = target[i] if target[i] < cur else min(target[i], cur + (1 - cur) * (1 - rel))
        gain[i] = cur
    mix *= gain[:, None]
    report.update(limiter_max_gr_db=round(float(-20 * np.log10(gain.min())), 2), lufs=round(float(meter.integrated_loudness(mix)), 2),
                  true_peak=round(float(tp(mix)), 2), duration=round(total / sr, 3))
    sf.write(f'out/{name}/{name}.wav', mix, sr, subtype='PCM_24')

    # timeline for promo.mjs: each excerpt's notes and its strings' 60 Hz trace, in piece time
    tl = dict(duration=total / sr, fps=30, version=VERSION, segments=[])
    for seg in segments:
        strings, energy, state = trace_of(f"out/{seg['piece']}/{seg['piece']}.csv")
        tl['segments'].append({k: seg[k] for k in ('title', 'sub', 'patch', 'how', 'start', 'end', 'src0', 'notes')}
                              | dict(strings=strings, energy=energy, state=state))
    json.dump(tl, open(f'out/{name}/timeline.json', 'w'))
    json.dump(report, open(f'out/{name}/report.json', 'w'), indent=1)
    print(json.dumps(report, indent=1))


if __name__ == '__main__':
    main()
