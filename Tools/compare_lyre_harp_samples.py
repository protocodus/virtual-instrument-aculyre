#!/usr/bin/env python3
"""Measures a sampled lyre harp and Aculyre's steel lyre harp note by note.

    build/RenderAculyreNotes 0 <renders> --rate 44100 --flatten 57,59,62,69,71,74,81
    python3 Tools/compare_lyre_harp_samples.py <Samples> <renders>
    python3 Tools/compare_lyre_harp_samples.py <Samples> --positions
    build/RenderAculyreNotes 0 <renders> --rate 44100 --flatten 57,59,62,69,71,74,81 --takes 3
    python3 Tools/compare_lyre_harp_samples.py <Samples> <renders> --takes

<Samples> is the Samples folder of DeadRobot's free DR Plucked Lyre Harp v1
(Decent Sampler; deadrobotmusic.com): "Lyre <note> RR<1-3>.wav", 16 strings
G3-G#5, three plucks each, 44.1 kHz float stereo. It is fetched for
analysis and never committed. Its harp is strung C major from G3 to A5 with
the A, B and D strings tuned a semitone down, so --flatten gives the model
the same strings slackened. --positions prints each note's pluck point
fitted to the recording (the median of its three plucks) as the renderer's
--positions argument, so the model can be rendered where the player plucked.

Protocol, the same on the recording and on the model:
- Onset: the first sample of the channel mean within 30 dB of its peak.
- Partials: peaks of a Hann-windowed 0.1-1.6 s FFT near n f0 sqrt(1 + B n^2),
  B refitted as partials are found (f_n / n)^2 = F^2 (1 + B n^2); a partial
  counts 15 dB over the median between its neighbours, up to 12 kHz.
- Levels: each partial's peak in Hann-windowed spans 0-45 ms and 100-300 ms.
- Decays: each partial's track (8192-point frames, hop 256) fitted by a line
  in dB over 0.08-0.6 s, only where it stands 10 dB over its own late floor.
  The recording's tails are denoised and faded (no steady noise floor), so
  its decays past a second are not used.
- Pluck point: log level = log|sin(n pi b)| plus a quadratic in log n, the
  first 45 ms of partials below 6 kHz and up to the 16th; b from 0.05 to 0.5
  is the distance from the nearer end. On the model's own renders it lands
  within 0.03 of the drawn point on 11 of 16 notes.
- Spectrum: third-octaves of each note's 0-0.3 s and 0.3-1.5 s, each note at
  equal power, then averaged over the notes; shape = the rms of the model
  minus the recording from 200 Hz to 10 kHz after removing the mean.
- Attack: each band's energy within 40 Hz of a partial and between
  partials, over windows from the onset, each note at its 0-300 ms energy.
- Channels: over 0-1.5 s from the onset, each octave band's real
  correlation between left and right (the cross-spectrum's real part over
  the geometric mean of the two powers, 2048-point Welch frames) and the
  two sides' level difference, medians over the notes.

Microphones, room and pluck force are unknown, so levels and spectra compare
shapes; partial frequencies and decay rates do not depend on them.

--takes compares each note's three plucks with each other instead, on the
recording and on renders of <note>-1..3.wav, so the capture, the same for
every take of a note, drops out (the protocol of Aculele's
compare_sample_packs.py --takes). Over 30-330 ms after the onset each take
gives its level (RMS), its brightness (power above 2.5 kHz against all of
it) and partials 1-12 at the frequencies the note's long span found, each
dropped 50 dB under the note's strongest. Printed: the level and brightness
SDs within a note, the median over notes, and their correlation over every
take's departure from its note's mean; each take's partials against its
note's mean, fitted with a + b log2(n): the SD of the tilt b over every take
(a sharper or duller release), and the median RMS of what the line leaves,
the comb scatter (a pluck point that moves). The same over 0-30 ms gives the
attack's own scatter.
"""

import argparse
import os
import sys

import numpy as np
from scipy.io import wavfile

NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
NOTES = ["G3", "G#3", "A#3", "C4", "C#4", "E4", "F4", "G4", "G#4", "A#4", "C5", "C#5", "E5", "F5", "G5", "G#5"]
BANDS = 1000.0 * 2.0 ** (np.arange(-7, 11) / 3.0)  # 198 Hz to 10 kHz
OCTAVES = [200, 400, 800, 1600, 3200, 6400]  # lower edges, Hz


def midi(note):
    return NAMES.index(note[:-1]) + 12 * (int(note[-1]) + 1)


def db(x):
    return 20.0 * np.log10(np.maximum(x, 1e-15))


def read(path):
    rate, data = wavfile.read(path)
    if data.dtype.kind == "i":
        data = data / float(np.iinfo(data.dtype).max)
    data = np.asarray(data, dtype=float)
    return (data if data.ndim == 1 else data.mean(axis=1)), rate


def channels(path):
    from scipy.signal import csd, welch
    rate, data = wavfile.read(path)
    if data.dtype.kind == "i":
        data = data / float(np.iinfo(data.dtype).max)
    data = np.asarray(data, dtype=float)
    if data.ndim == 1:
        return None
    start = onset(data.mean(axis=1))
    left, right = data[start:start + int(1.5 * rate), 0], data[start:start + int(1.5 * rate), 1]
    f, cross = csd(left, right, fs=rate, nperseg=2048)
    _, pl = welch(left, fs=rate, nperseg=2048)
    _, pr = welch(right, fs=rate, nperseg=2048)
    rows = []
    for lo in OCTAVES:
        m = (f >= lo) & (f < 2 * lo)
        rows.append((np.real(cross[m]).sum() / np.sqrt(pl[m].sum() * pr[m].sum()),
                     abs(10 * np.log10(pl[m].sum() / pr[m].sum()))))
    return rows


def onset(x):
    return int(np.argmax(np.abs(x) > np.abs(x).max() * 10 ** (-30 / 20)))


def peak(spectrum, k):
    a, b, c = np.log(spectrum[k - 1:k + 2] + 1e-20)
    d = a - 2 * b + c
    p = 0.5 * (a - c) / d if d < 0 else 0.0
    return k + p, np.exp(b - 0.25 * (a - c) * p)


def measure(path, note):
    x, rate = read(path)
    start = onset(x)
    nominal = 440.0 * 2 ** ((midi(note) - 69) / 12)
    span = x[start + int(0.1 * rate):start + int(1.6 * rate)]
    size = 1 << 20
    spectrum = np.abs(np.fft.rfft(span * np.hanning(len(span)), size))
    step = rate / size
    lo, hi = int(nominal * 2 ** (-0.6 / 12) / step), int(nominal * 2 ** (0.6 / 12) / step)
    f0 = peak(spectrum, lo + int(np.argmax(spectrum[lo:hi])))[0] * step
    inharmonicity, freqs = 0.0, {}
    for n in range(1, 41):
        predicted = n * f0 * np.sqrt((1 + inharmonicity * n * n) / (1 + inharmonicity))
        if predicted > min(12000.0, 0.475 * rate):
            break
        width = max(3, int((0.012 if n > 1 else 0.003) * predicted / step))
        centre = int(predicted / step)
        k = centre - width + int(np.argmax(spectrum[centre - width:centre + width]))
        bin_, amplitude = peak(spectrum, k)
        floor = np.median(spectrum[int((predicted - 0.5 * f0) / step):int((predicted + 0.5 * f0) / step)])
        if db(amplitude) - db(floor) > 15:
            freqs[n] = bin_ * step
        ns = np.array(sorted(freqs))
        if len(ns) >= 4:
            fs = np.array([freqs[m] for m in ns])
            c = np.linalg.lstsq(np.vstack([np.ones(len(ns)), ns ** 2]).T, (fs / ns) ** 2, rcond=None)[0]
            if c[0] > 0 and c[1] > 0:
                inharmonicity = c[1] / c[0]
    ns = np.array(sorted(freqs))
    fs = np.array([freqs[m] for m in ns])
    c = np.linalg.lstsq(np.vstack([np.ones(len(ns)), ns ** 2]).T, (fs / ns) ** 2, rcond=None)[0]

    def levels(t0, t1):
        s = x[start + int(t0 * rate):start + int(t1 * rate)]
        w = np.hanning(len(s))
        y = np.abs(np.fft.rfft(s * w, 1 << 18)) * 2 / w.sum()
        dy = rate / (1 << 18)
        reach = max(2, int(0.25 * f0 / dy))
        return {n: float(db(y[int(f / dy) - reach:int(f / dy) + reach].max())) for n, f in freqs.items()}

    frame, hop = 8192, 256
    window = np.hanning(frame)
    frames = np.array([np.abs(np.fft.rfft(x[start + i:start + i + frame] * window)) * 2 / window.sum()
                       for i in range(0, len(x) - start - frame, hop)])
    times = (np.arange(len(frames)) * hop + frame / 2) / rate
    decays = {}
    for n, f in freqs.items():
        b = int(round(f / (rate / frame)))
        track = db(frames[:, max(b - 2, 0):b + 3].max(axis=1))
        floor = np.median(track[int(0.8 * len(track)):])
        use = (times >= 0.08) & (times <= 0.6) & (track > floor + 10)
        if use.sum() >= 5 and np.ptp(times[use]) >= 0.13:
            decays[n] = float(-np.polyfit(times[use], track[use], 1)[0])
    return dict(f0=float(f0), B=float(c[1] / c[0]), freqs=freqs, early=levels(0.0, 0.045),
                mid=levels(0.1, 0.3), decays=decays, signal=x, rate=rate, start=start)


def pluck_point(m):
    ns = np.array([n for n in sorted(m["early"]) if m["freqs"][n] < 6000 and n <= 16])
    y = np.array([m["early"][n] for n in ns])
    envelope = np.vstack([np.ones(len(ns)), np.log(ns), np.log(ns) ** 2]).T
    best = (None, np.inf)
    for b in np.arange(0.05, 0.5001, 0.0025):
        comb = db(np.maximum(np.abs(np.sin(ns * np.pi * b)), 10 ** (-30 / 20)))
        residual = y - comb - envelope @ np.linalg.lstsq(envelope, y - comb, rcond=None)[0]
        error = np.sqrt(np.mean(residual ** 2))
        if error < best[1]:
            best = (float(b), error)
    return best[0]


def thirds(m, t0, t1):
    x, rate, start = m["signal"], m["rate"], m["start"]
    s = x[start + int(t0 * rate):start + int(t1 * rate)]
    power = np.abs(np.fft.rfft(s * np.hanning(len(s)), 1 << 17)) ** 2
    f = np.fft.rfftfreq(1 << 17, 1 / rate)
    v = np.array([power[(f >= b * 2 ** (-1 / 6)) & (f < b * 2 ** (1 / 6))].sum() for b in BANDS])
    return v / v.sum()


def average_spectrum(ms, t0, t1):
    return 10 * np.log10(np.mean([thirds(m, t0, t1) for m in ms], axis=0))


ATTACK_BANDS = [(1000, 2000), (2000, 4000), (4000, 8000), (8000, 16000)]
ATTACK_WINDOWS = [(0.0, 0.01), (0.01, 0.02), (0.02, 0.04), (0.04, 0.08), (0.08, 0.16), (0.16, 0.32)]


def attack(m):
    x, rate = m["signal"], m["rate"]
    start = max(m["start"] - int(0.001 * rate), 0)
    total = np.sum(x[start:start + int(0.3 * rate)] ** 2)
    out = np.zeros((len(ATTACK_WINDOWS), len(ATTACK_BANDS), 2))
    for i, (a, b) in enumerate(ATTACK_WINDOWS):
        s = x[start + int(a * rate):start + int(b * rate)]
        w = np.hanning(len(s))
        power = np.abs(np.fft.rfft(s * w, 1 << 15)) ** 2 / np.sum(w ** 2) * len(s)
        f = np.fft.rfftfreq(1 << 15, 1 / rate)
        near = np.zeros(len(f), bool)
        for fn in m["freqs"].values():
            near |= np.abs(f - fn) < max(40.0, 1.5 * rate / len(s))
        for j, (lo, hi) in enumerate(ATTACK_BANDS):
            band = (f >= lo) & (f < hi)
            out[i, j] = power[band & near].sum() / total, power[band & ~near].sum() / total
    return out


def take(m, t0, t1):
    x, rate, start = m["signal"], m["rate"], m["start"]
    s = x[start + int(t0 * rate):start + int(t1 * rate)]
    w = np.hanning(len(s))
    y = np.abs(np.fft.rfft(s * w, 1 << 18)) * 2 / w.sum()
    f = np.fft.rfftfreq(1 << 18, 1 / rate)
    reach = max(2, int(0.25 * m["f0"] / (rate / (1 << 18))))
    partials = np.full(12, np.nan)
    for n, fn in m["freqs"].items():
        if n <= 12:
            k = int(fn / (rate / (1 << 18)))
            partials[n - 1] = db(y[k - reach:k + reach].max())
    power = y ** 2
    return dict(level=10 * np.log10(np.mean(s ** 2)),
                brightness=10 * np.log10(power[f > 2500].sum() / power.sum()), partials=partials)


def report_takes(label, notes, t0, t1):
    spreads, departures, tilts, scatter = [], [], [], []
    order = np.log2(np.arange(1, 13))
    for ms in notes:
        takes = [take(m, t0, t1) for m in ms]
        spreads.append([np.std([t[k] for t in takes], ddof=1) for k in ("level", "brightness")])
        pairs = np.array([[t["level"], t["brightness"]] for t in takes])
        departures.extend(pairs - pairs.mean(axis=0))
        levels = np.array([t["partials"] for t in takes])
        levels[levels < np.nanmax(levels) - 50] = np.nan
        for deviation in levels - np.nanmean(levels, axis=0):
            use = np.isfinite(deviation)
            if use.sum() >= 6:
                slope, offset = np.polyfit(order[use], deviation[use], 1)
                tilts.append(slope)
                scatter.append(np.sqrt(np.mean((deviation[use] - offset - slope * order[use]) ** 2)))
    level, brightness = np.median(spreads, axis=0)
    together = np.corrcoef(np.array(departures).T)[0, 1]
    print(f"{label:10s} {t0 * 1000:3.0f}-{t1 * 1000:3.0f} ms | {level:8.2f} {brightness:9.2f} {together:+9.2f}"
          f" | {np.std(tilts):7.2f} {np.median(scatter):12.2f}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("samples")
    parser.add_argument("renders", nargs="?")
    parser.add_argument("--positions", action="store_true", help="print the recording's pluck points and stop")
    parser.add_argument("--takes", action="store_true", help="compare each note's takes with each other")
    args = parser.parse_args()

    rec = {n: [measure(os.path.join(args.samples, f"Lyre {n} RR{k}.wav"), n) for k in (1, 2, 3)] for n in NOTES}
    points = {n: [pluck_point(m) for m in rec[n]] for n in NOTES}
    if args.positions:
        print(",".join(f"{midi(n)}:{np.median(points[n]):.3f}" for n in NOTES))
        return 0
    if not args.renders:
        parser.error("renders is required unless --positions")
    if args.takes:
        mods = [[measure(os.path.join(args.renders, f"{midi(n)}-{k}.wav"), n) for k in (1, 2, 3)] for n in NOTES]
        print("           window     | level SD bright SD  together | tilt SD comb scatter (dB)")
        for t0, t1 in ((0.03, 0.33), (0.0, 0.03)):
            report_takes("recording", [rec[n] for n in NOTES], t0, t1)
            report_takes("model", mods, t0, t1)
        return 0
    mod = {n: measure(os.path.join(args.renders, f"{midi(n)}.wav"), n) for n in NOTES}

    def median(values):
        values = [v for v in values if v is not None]
        return float(np.median(values)) if values else float("nan")

    def upper(m):
        return median([m["decays"].get(k) for k in range(2, 7)])

    print("note   recording: f0 (cents)   B      pluck points        H1 dB/s  H2-6 dB/s | model: B      H1 dB/s  H2-6 dB/s")
    h1, h26 = [], []
    for n in NOTES:
        r, m = rec[n], mod[n]
        f0 = median([x["f0"] for x in r])
        cents = 1200 * np.log2(f0 / (440 * 2 ** ((midi(n) - 69) / 12)))
        r1, m1 = median([x["decays"].get(1) for x in r]), m["decays"].get(1)
        r26, m26 = median([upper(x) for x in r]), upper(m)
        if m1:
            h1.append(np.log2(max(m1, 0.5) / r1))
        if np.isfinite(m26):
            h26.append(np.log2(max(m26, 0.5) / r26))
        print(f"{n:4s} {f0:8.2f} ({cents:+5.1f})  {median([x['B'] for x in r]):.1e}  "
              + " ".join(f"{p:.3f}" for p in points[n])
              + f"  {r1:7.1f}  {r26:9.1f} |        {m['B']:.1e}  {m1 or float('nan'):7.1f}  {m26:9.1f}")
    allpoints = [p for n in NOTES for p in points[n]]
    q1, q2, q3 = np.percentile(allpoints, [25, 50, 75])
    print(f"\npluck points over all plucks: median {q2:.3f}, quartiles {q1:.3f}-{q3:.3f}")
    print(f"decay rms log2(model / recording): fundamentals {np.sqrt(np.mean(np.square(h1))):.2f}, "
          f"partials 2-6 {np.sqrt(np.mean(np.square(h26))):.2f}")

    recs = [m for n in NOTES for m in rec[n]]
    mods = [mod[n] for n in NOTES]
    for t0, t1 in ((0.0, 0.3), (0.3, 1.5)):
        a, b = average_spectrum(recs, t0, t1), average_spectrum(mods, t0, t1)
        print(f"\nthird-octaves {t0}-{t1} s, each note at equal power (dB): band, recording, model, model - recording")
        for band, x, y in zip(BANDS, a, b):
            print(f"{band:7.0f} {x:7.1f} {y:7.1f} {y - x:+6.1f}")
        print(f"shape rms 200 Hz-10 kHz: {np.std(b - a):.1f} dB")

    print("\npartial decays 0.08-0.6 s by third-octave (dB/s, median): band, recording (n), model (n)")
    for band in 1000.0 * 2.0 ** (np.arange(-7, 13) / 3.0):
        lo, hi = band * 2 ** (-1 / 6), band * 2 ** (1 / 6)
        r = [d for m in recs for k, d in m["decays"].items() if lo <= m["freqs"][k] < hi]
        s = [d for m in mods for k, d in m["decays"].items() if lo <= m["freqs"][k] < hi]
        if len(r) >= 3 and len(s) >= 2:
            print(f"{band:7.0f} {np.median(r):7.1f} ({len(r):3d}) {np.median(s):7.1f} ({len(s):3d})")

    print("\nattack, dB re each note's 0-300 ms energy: window, then per band partials / between, model - recording")
    ra = 10 * np.log10(np.mean([attack(m) for m in recs], axis=0) + 1e-20)
    ma = 10 * np.log10(np.mean([attack(m) for m in mods], axis=0) + 1e-20)
    print("window     " + "".join(f"{lo // 1000}-{hi // 1000} kHz".ljust(16) for lo, hi in ATTACK_BANDS))
    for i, (a, b) in enumerate(ATTACK_WINDOWS):
        print(f"{a * 1000:3.0f}-{b * 1000:3.0f} ms " + "".join(
            f"{ma[i, j, 0] - ra[i, j, 0]:+6.1f} /{ma[i, j, 1] - ra[i, j, 1]:+6.1f}   " for j in range(len(ATTACK_BANDS))))

    print("\nchannels, median over notes: octave from (Hz), correlation recording / model, |L-R| dB recording / model")
    rc = [c for c in (channels(os.path.join(args.samples, f"Lyre {n} RR{k}.wav")) for n in NOTES for k in (1, 2, 3)) if c]
    mc = [c for c in (channels(os.path.join(args.renders, f"{midi(n)}.wav")) for n in NOTES) if c]
    if rc and mc:
        rm, mm = np.median(rc, axis=0), np.median(mc, axis=0)
        for i, lo in enumerate(OCTAVES):
            print(f"{lo:6d} {rm[i, 0]:5.2f} / {mm[i, 0]:5.2f}   {rm[i, 1]:4.1f} / {mm[i, 1]:4.1f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
