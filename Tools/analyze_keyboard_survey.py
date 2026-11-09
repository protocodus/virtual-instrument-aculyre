#!/usr/bin/env python3
"""Add acoustic measurements to AculyreKeyboardSurvey's CSV/raw output.

The raw file contains 128 consecutive, two-second, interleaved float32 stereo
renders. The engine has no output limiter. All notes use identical random
draws after reset. Measurements are observations, not perceptual verdicts:
body-mode bumps and two-polarisation beating must be judged against neighbours
and the baseline, rather than forced onto a flat EQ or single decay constant.

Usage: analyze_keyboard_survey.py survey.csv audio.f32 measurements.csv
Requires NumPy and SciPy. The raw file may be removed after retaining the CSV.
"""

import argparse
import csv
from pathlib import Path

import numpy as np
from scipy.fft import next_fast_len, rfft, rfftfreq


def power_spectrum(audio, rate, start, stop, padding=1):
    signal = audio[round(start * rate):round(stop * rate)].astype(np.float64)
    window = np.hanning(len(signal))
    size = next_fast_len(padding * len(signal))
    transformed = rfft(signal * window[:, None], n=size, axis=0)
    power = np.mean(np.abs(transformed) ** 2, axis=1)
    return rfftfreq(size, 1.0 / rate), power


def fundamental_power(audio, rate, note, start, stop, bend=0):
    # The artificial lowest keys' fundamental lies below the soundbox's
    # radiation band; their third partial carries pitch (the engine test's
    # convention). The +/-60-cent search includes the physical initial glide.
    partial = 3 if note < 40 else 1
    nominal = partial * 440.0 * 2 ** ((note + bend - 69) / 12)
    frequency, power = power_spectrum(audio, rate, start, stop, 8)
    band = np.flatnonzero((frequency >= nominal * 2 ** (-60 / 1200))
                         & (frequency <= nominal * 2 ** (60 / 1200)))
    if not len(band):
        return 0.0, 0.0, 0.0, True
    peak = band[np.argmax(power[band])]
    outside = ((frequency > 0.5 * nominal) & (frequency < 1.5 * nominal)
               & ((frequency < nominal * 2 ** (-100 / 1200))
                  | (frequency > nominal * 2 ** (100 / 1200))))
    floor = float(np.median(power[outside])) if np.any(outside) else 0.0
    confidence = float(10 * np.log10(max(float(power[peak]), 1e-30) / max(floor, 1e-30)))
    at_edge = bool(peak == band[0] or peak == band[-1])
    return float(power[peak]), float(1200 * np.log2(frequency[peak] / nominal)), confidence, at_edge


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("survey", type=Path)
    parser.add_argument("audio", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    with args.survey.open() as source:
        rows = list(csv.DictReader(source))
    if len(rows) != 128:
        raise ValueError("survey must contain all 128 MIDI notes")
    rate = int(rows[0]["rate"])
    if any(int(row["note"]) != index or int(row["rate"]) != rate for index, row in enumerate(rows)):
        raise ValueError("rows must be MIDI 0-127 in order, all at one sample rate")
    expected = 128 * 2 * rate * 2 * 4
    if args.audio.stat().st_size != expected:
        raise ValueError(f"incomplete raw file: expected {expected} bytes")
    audio = np.memmap(args.audio, dtype="<f4", mode="r", shape=(128, 2 * rate, 2))
    for note, row in enumerate(rows):
        row.update(centroid_hz="", dominant_hz="", dominant_power_share="", tracked_partial="",
                   partial_decay_db_per_s="", early_pitch_cents="", late_pitch_cents="",
                   early_peak_prominence_db="", late_peak_prominence_db="",
                   early_peak_at_search_edge="", late_peak_at_search_edge="")
        if not int(row["supported"]):
            continue
        frequency, power = power_spectrum(audio[note], rate, 0.02, 0.3)
        # Exclude DC and frequencies outside the listening band. The most
        # prominent peak's five bins estimate its energy concentration; no
        # arbitrary pass/fail is attached to a physically resonant body.
        power[(frequency < 20) | (frequency > min(20000, 0.49 * rate))] = 0
        total = power.sum()
        peak = int(power.argmax())
        row["centroid_hz"] = float(np.dot(frequency, power) / max(total, 1e-30))
        row["dominant_hz"] = float(frequency[peak])
        row["dominant_power_share"] = float(power[max(0, peak - 2):peak + 3].sum() / max(total, 1e-30))
        bend = float(row.get("pitch_bend", 0))
        early, early_cents, early_confidence, early_edge = fundamental_power(audio[note], rate, note, 0.2, 0.5, bend)
        late, late_cents, late_confidence, late_edge = fundamental_power(audio[note], rate, note, 1.0, 1.3, bend)
        row["tracked_partial"] = 3 if note < 40 else 1
        row["partial_decay_db_per_s"] = float(10 * np.log10(max(early, 1e-30) / max(late, 1e-30)) / 0.8)
        row["early_pitch_cents"] = early_cents
        row["late_pitch_cents"] = late_cents
        row["early_peak_prominence_db"] = early_confidence
        row["late_peak_prominence_db"] = late_confidence
        row["early_peak_at_search_edge"] = int(early_edge)
        row["late_peak_at_search_edge"] = int(late_edge)
    with args.output.open("w", newline="") as destination:
        writer = csv.DictWriter(destination, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    print(args.output)


if __name__ == "__main__":
    main()
