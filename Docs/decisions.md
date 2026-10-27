# Decisions

Accepted choices and listening verdicts, newest first. A direction chosen by
ear is recorded as chosen by ear.

## 2026-10-27: a bending bridge bar and an exponential pluck release (0.3.0)

**What is being decided.** Whether Aculyre keeps two changes aimed at the
steel lyre harp sounding too dark against recordings: the bridge bar bending
with the top (each string on its own point of it, instead of one rigid bar
that averaged the top's modes away), and the pluck's held force unloading
exponentially as the string slides off the finger (instead of a wider
Gaussian rounding the apex).

**What the measurements say.** On a lyre harp phrase (Freesound 649925,
replayed note for note, 1/3-octave long-term spectra normalised to their
loudest band), the model's mean shortfall from 1.25 to 5 kHz is 10.0 dB for
0.2.0, 7.6 dB with the release alone, 7.9 dB with the bar alone and 5.4 dB with
both. The recording is one unmeasured instrument and microphone, so this
ranks the candidates against one reference; it does not calibrate them. Both
changes are defensible on their own physics (Chadefaux et al. 2012 on the
finger's contact and release; the bar's mass and stiffness from its assumed
size and wood), and neither introduces a fitted constant.

**Listening test.** Four letters, the shipping 0.2.0 as A and the three
candidates, on the steel lyre harp and the gut lyre, rendered and handed over
uncommitted with their key.

**Verdict.** Pending the owner's listening. The branch carries both changes
as 0.3.0 until then.
