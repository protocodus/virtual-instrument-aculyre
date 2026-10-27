# Decisions

Accepted choices and listening verdicts, newest first. A direction chosen by
ear is recorded as chosen by ear.

## 2026-10-27: the exponential pluck release ships; the bending bridge bar does not (0.3.0)

**What was decided.** Which of two changes aimed at the steel lyre harp
sounding too dark against recordings Aculyre keeps: the pluck's held force
unloading exponentially as the string slides off the finger (instead of a
wider Gaussian rounding the apex), and a bridge bar that bends with the top,
each string driving it at its own point (instead of one rigid bar that
averages the top's modes away above a kilohertz).

**What the measurements said.** Replaying a lyre harp phrase (Freesound
649925) note for note, the model's mean shortfall from 1.25 to 5 kHz in
1/3-octave long-term spectra was 10.0 dB for 0.2.0, 7.6 dB with the release
alone, 7.9 dB with the bar alone and 5.4 dB with both. The bar also made the
steel A4 decay faster (50 against the recording's 30 dB/s; 43 without it) and
the gut lyre (23-27 against 18 dB/s), and it doubled the CPU with every string
ringing. One unmeasured recording ranks the candidates; it does not
calibrate them.

**Listening test.** Steel lyre harp (the recording's phrase, then a sweep
over all 16 strings) and gut lyre, 48 kHz, matched on whole-file RMS:

| Letter | Engine |
| --- | --- |
| A | 0.2.0 as shipped (35e6f44) |
| B | both changes (f209037) |
| C | the bending bar alone |
| D | the exponential release alone (d3b9481) |

**Verdict, by ear.** The owner chose C or D, which sounded much alike to
them, over A and B. Between the two, D ships: it matches C's treble
measurement, keeps the decay rates closer to the recordings and costs no
CPU. The bar is kept in Git history at f209037 for when a measured lyre
body can tell whether its coupling is right.

**What it licenses.** The exponential release as the pluck model for every
technique. It does not license the release times, which stay authored.
