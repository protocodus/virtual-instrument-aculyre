# Decisions

Accepted choices and listening verdicts, newest first. A direction chosen by
ear is recorded as chosen by ear.

## 2026-10-27: five realism changes ship on published measurements; the by-ear verdict is pending (0.4.0)

**What was decided.** The owner asked for five audible steps towards the real
instrument, including natural randomness grounded in reality. Each of the
five rests on a published measurement or a recording:

- **Nylon and gut damping.** Woodhouse and Lynch-Aird's fit to measured harp
  strings (Acta Acustica 105 (2019) 516) replaces estimated loss values.
- **Re-pluck stop.** A finger or nail re-plucking a sounding string stops
  it first: a harpist's finger holds the string 56-431 ms (Chadefaux et al.,
  JASA 131 (2012) 844, Table II).
- **Pluck scatter.** Plucks scatter in point, force and angle as nine
  harpists' did (same paper, Fig. 7, Fig. 8 and Table VI).
- **Slip scatter.** The release time scatters by the share of the release
  velocity's spread that the displacement does not explain (Fig. 8).
- **Hand tuning.** Strings sit off their notes by the spread measured between
  the six strings of a lyre harp phrase (Freesound 649925), 4.1 cents. That
  spread is a pinning, and the kanklės tune's 1 cent is the other defensible
  one.

**What the measurements say.**

- **Nylon and gut damping:**
  - The nylon bass is darker and decays faster: G3's centroid falls from 585
    to 488 Hz, and its fundamental decays at 12.8 dB/s instead of 8.1.
  - The nylon treble is brighter: G5's centroid rises from 1336 to 1493 Hz.
  - The gut lyre is brighter: D4's centroid rises from 794 to 975 Hz.
- **Re-pluck stop:** a soft finger re-pluck of a loud A4 leaves 1.1 times a
  fresh pluck's string energy 25 ms later, against 32 times before.
- **Pluck scatter:** 2000 drawn plucks match the published spreads within 2%.
- **Lyre harp phrase:** replaying the phrase (Freesound 649925), the scatter
  and the re-pluck stop leave its 1.25-5 kHz shortfall 0.6 dB larger on
  average over eight seeds (8.0 against 7.4 dB). That is within one seed's
  range of the other.

**Listening test.** All three instruments play one phrase: repeated notes, a
chord, an octave, an arpeggio and repeated bass notes. The renders are 48 kHz
and matched on whole-file RMS. The set and its key are in the project files
(`aculyre/listening-2026-10-27-realism/`).

| Letter | Engine |
| --- | --- |
| A | 0.4.0 at 1x as shipped (117b9b5) |
| B | the re-pluck stop alone |
| C | all five, 4.1-cent tuning spread (81d56a4) |
| D | the nylon and gut damping alone (not on steel, which it leaves unchanged) |
| E | all five, every string exactly in tune |
| F | the pluck and slip scatter alone |
| G | all five, 1-cent tuning spread |

**Verdict, by ear.** Pending: the owner has not listened yet. Until then, C
ships, chosen from the measurements and not by ear.

**What it licenses.** The measured damping, spreads and re-pluck stop. For
the tuning, a choice between 0, 1 and 4.1 cents. It does not license
tuning any spread by ear beyond those measured values.

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
