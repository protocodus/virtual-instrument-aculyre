# Final output headroom

The 0.12.5 candidate adds output-only calibration and reserve around the
unchanged shared output stage. `PlayingControls::volume` remains a unity-
default multiplier; the physical engine and pickup circuit are unchanged.

The baseline audit contains 612 deterministic 5.5-second takes through the
shipping Performer, with no normalization or extra processing. Three core
defaults and nine stored Reason patches cover microphone, half blend, half
blend with inverted piezo, and pickup capture. All distinct valid 1x/2x/4x
factors are measured at 44.1, 48, 88.2, 96, 176.4 and 192 kHz; core defaults
also cover the 22.05 kHz API boundary. The score separates native hard chords,
dense all-string MIDI stress, high artificial-range keys, and maximum
volume/room/width/plectrum controls. All-string chords are electronic stress,
not a natural hand gesture.

An independent 8x Kaiser-8.6 polyphase reconstruction found +0.040105 dBTP at
22.05 kHz/2x on the steel pickup, while every stored sample remained under
-1.5 dBFS. The same event measured +0.043752 dB at 32x. Extending the kernel
from 10 to 32/64/128 input samples per side gave +0.090249/+0.089812/
+0.089659 dB, at 2.943604 seconds. This establishes a small intersample
headroom gap on a supported path; it is not an electrical-preamp diagnosis.

The final reserve is -0.6 dB, the observed excursion plus about 0.5 dB margin.
A reciprocal +0.6 dB digital calibration before the stage preserves nominal
quiet level. The delivered sample ceiling becomes -2.1 dBFS. The two float
multiplications may add a rounding unit; they add no filter, delay or
mechanical feedback. A fresh complete 22.05 kHz/2x render of all three core
presets and four captures puts the former event at -0.559935 dBTP under the
primary 8x estimator, with sample peaks at -2.1 dBFS.

Normal calibration retains the existing 300 ms stereo-mean RMS convention,
0 VU at -12 dBFS, and median native-note/chord readings at velocities
70/90/110. Existing normal/hard thresholds are unchanged. Independent left
and right 300 ms readings are also recorded for stress; a result above
0 VU is a loudness-reference exceedance, not digital full-scale clipping.

The full six-test unit suite passes with the output reserve, including the
original core normal/hard level checks and capture-state invariance. The
full build and C++17/no-exceptions target pass. All nine stored Reason
patches also pass their original level, balance and hardest-chord bounds;
their typical medians move by at most 0.012 dB, so no further factory volume
change is needed. The shared output-stage source remains byte-identical.

The final logs, exact normal-level table, high-resolution reconstruction
checks, source hashes and native float WAVs are retained in the task's
external `review/aculyre/output-levels/` package. This estimate is not a
certified BS.1770 meter or a proof for every continuous setting and score.
The shared stage guarantees sample peaks; the reconstructed-peak statements
apply to measured signals. Physical piezo rail/diode saturation remains
upstream of digital output gains and is not removed by this calibration.
