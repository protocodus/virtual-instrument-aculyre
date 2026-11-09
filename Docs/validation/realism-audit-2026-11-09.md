# Release, sustain and keyboard audit (0.13.0)

This candidate starts from `3f4f064` (0.12.7). It keeps the three lyres,
their dimensions, materials, player variability and output calibration.
The soundboxes are still assumed geometries, and the nylon/gut losses are
estimates. This report records mathematical and rendered evidence, not a
listening verdict or a claim of measured instrument calibration.

## Changes and their physical basis

- The exponential release now integrates the continuous velocity wave over
  each sample cell, including the fractional last cell of a round trip.
  Averaging the ideal triangle first and applying a discrete circular filter
  afterwards did not commute with that integration. The existing release
  times and velocity law remain; no running string filter was added.
- Re-pluck contact uses the sounding, bent period and the upcoming string
  length. The new wave's delay and contact are set immediately, even when
  another string keeps the control clock running. Previously an octave-down
  re-pluck projected only about half of its sounding period.
- Distributed palm/release loss is specified per second. Its per-round-trip
  gain now follows the actual frequency through a bend. A fixed gain per
  *open* round trip made the same palm twice as strong at +12 semitones and
  half as strong at -12.
- Each string retains its contact-noise attack when a chord starts. The old
  single shared transient was overwritten by every subsequent pluck. The
  existing short noise envelope remains authored, not measured tribology.
- The small authored saddle slope now rotates both the outgoing force and
  returning velocity with reciprocal, orthogonal maps. Equal impedances of
  the two planes make the combined bridge load invariant under this change
  of basis. The previous one-way map had norm greater than one.
- The extra axial drive added in 0.12.7 was removed: `glidePerEnergy` has
  units s²/m², so multiplying it by squared wave velocity gives a
  dimensionless frequency ratio, not a velocity to add to a bridge drive.
  The established Kirchhoff tension glide remains. A future longitudinal
  force model needs its own geometry and energy-consistent coupling.

The retained 88/12 point/distributed release damping and normal/parallel
pulp timing factors are inexpensive authored approximations. They are not
resolved fingertip geometry or quantitative fits to the cited studies.

## Correctness and playability

At 192 kHz, A0 bent down an octave needs about 13,964 samples. The previous
8,192-sample work array overflowed during the initial pluck, and the live
delay clipped. Capacity is now 16,384. At the opposite corner, C8 +12 at
22.05 kHz asked a second-order Thiran section to operate outside its stable
domain and ran away. The short-loop case now uses a stable first-order
section and an explicit phase solve. The order changes at their common
one-sample fractional delay. Short second-order lines also need continuous
integer-tap branches: the ordinary D=2.3 to D=1.3 switch has a finite phase
gap, so an exact solver can land in that gap. Below eight line samples,
the canonical fraction is now in (1, 2], joining at exact pure delays and
choosing D=2 at integer samples to avoid D=1's marginal pole cancellation.
The bend table solves the same realization during setup; the callback adds
no solve or transcendental. Longer loops retain the established hysteresis.

At 48 kHz, the old short-loop approximation put steel/nylon C8 +12 spectral
peaks 6–8 cents below both designed polarisations. The correction puts them
at the parallel-plane frequency. At 22.05 kHz the normal plane dominates
instead: its higher peak (up to about 28 cents on the artificial gut C9)
matches its own designed period, not an interpolation error. The two planes'
different endpoint corrections must not be confused with one tuning offset.

Overlapping Note Ons for the current pitch own separate bounded presses.
The Rack adapter already forwards paired releases, including an independent
CV press. One source's release therefore leaves the other held. A different
pitch stealing the same physical string starts new ownership; stale
old-pitch releases cannot stop it. Reset clears ownership, while acoustic
sleep preserves it until the actual releases arrive. Nonpositive or NaN
velocities remain ignored, as documented by the public engine API.

Unused private loop scaling, cached loss-design copies and last-velocity
state were removed. Public `piezoTouch`/`leverPiezoTouch` preset fields remain
for source compatibility; capture-dependent excitation was already retired
in 0.12.3, not in this pass.

## Validation protocol

The frozen 0.12.7 baseline passed all six original CTests. The first
integrated candidate also passed the unchanged acoustic contracts. New
tests cover:

- continuous-release cell refinement and zero DC, plus a separate Fourier
  magnitude/phase oracle against `1 / (1 + j omega tau)`;
- rotation power and force/velocity work identities;
- a linear acoustic chord against the sum of its independently seeded
  plucks: residual improved from -74 dB to -142.6 dB;
- additional palm amplitude decay of 29.877, 30.139 and 30.091 s⁻¹ at
  -12, 0 and +12 semitones for the requested 30 s⁻¹;
- 72 static range/bend/re-pluck corners and live C8 bend sweeps through the
  interpolation-order boundary, including callback-partition identity;
- public-Performer ownership, pedal, reset, revoicing, ignored-velocity and
  acoustic-sleep cases. The baseline fails five ownership assertions.

An optional Clang libFuzzer target exercises bounded factory-engine event
sequences at 22.05, 48 and 192 kHz with address/undefined-behavior sanitizers
and library bounds assertions. Its first deterministic 301-second campaign
completed 3,998 runs without a crash. A rebuilt final-source campaign with
a second deterministic seed completes another 3,871 runs in 301 seconds,
with no address, undefined-behavior or bounds error. This is a bounded stress
result, not an exhaustive proof. The unlimited engine output is checked
separately from the Performer's output protection.

`AculyreKeyboardSurvey` renders MIDI 0–127 with deterministic draws, room
off and levers on: 1.5 seconds held, then 0.5 seconds released. It measures
stereo peak and attack (20–300 ms), sustain (0.5–1 s), late (1–1.5 s) and
release (1.8–2 s) RMS. The companion Python tool adds spectrum, centroid,
tracked-partial decay and pitch-peak confidence. Below MIDI 40 it tracks
the third partial because the body radiates little fundamental; that is
explicit metadata, not a mislabeled fundamental decay.

At 48 kHz the complete matrix covers three presets, velocities 32/64/127,
microphone/piezo and all 128 MIDI notes: 2,304 renders, including 1,584
supported-key cases. All are finite; A0–C8 sounds and unsupported keys stay
silent. No attack, sustain or late-level velocity inversion appears between
the three tested velocities. Minimum increments are respectively +7.38,
+3.95 and +0.84 dB. This does not assert monotonicity between every adjacent
MIDI velocity or for every possible control setting.

A separate minimum-velocity scan adds MIDI velocities 1 and 2 on all keys
and presets through the microphones: 768 renders, all finite with the same
support behavior. Every supported key increases in attack, sustain and late
level; the smallest measured increment is +8.42865 dB. The attack increments
span only +8.42886 to +8.42912 dB, consistent with the unchanged force law
`velocity^1.4` in this quiet, effectively linear regime.

The velocity-64 microphone sweep also covers 22.05, 44.1, 96 and 192 kHz,
adding 1,536 candidate renders and matching baseline renders. All support
and finite checks pass. Native attack level differs from 48 kHz by at most
0.085 dB over 44.1–192 kHz. At 22.05 kHz that bound is 1.56 dB, and the
artificial gut C7 is 6.94 dB quieter than at 48 kHz (6.34 dB in the baseline).
That retained low-rate extension discrepancy is documented, not counted as
an evenness success. Across all matrices the candidate has 4,608 renders,
3,168 of them on supported keys; duplicate CTest sweeps are not included.

At velocity 64, microphone attack deviations from the mean of adjacent
native keys are -1.10 to +1.09 dB (steel), -1.31 to +1.50 dB (nylon), and
-1.73 to +0.92 dB (gut), excluding the compass endpoints. Piezo deviations
are larger: -6.44 to +3.97, -7.29 to +5.35 and -4.03 to +5.47 dB. The pickup
observes the same physical pluck as the microphones; its natural bridge
response has not been flattened by changing excitation with capture choice.

## Limits that remain

The artificial extensions beyond the native string/lever compass are useful
keyboard coverage, not measured lyres. They retain stronger body-mode bumps
and rapid high-note decays. For example, steel MIDI 106 is louder and drains
faster than its neighbours. Its behavior persists across sample rates and
coincides with strong modeled modes near 3.7 kHz; it is not evidence for an
arbitrary per-note gain correction. The body still needs measured geometry
and admittance to decide whether those particular modes describe a real
instrument.

The body preload matches the first cell of the periodically released wave,
not the full static held force before a causal slip. With nominal position
0.27 and median draws at 48 kHz, its normal-plane ratio to that held force
at velocity 32 is about 0.995 at B2, 0.721 at A4, 0.205 at G6 and 0.076 at
C8; at velocity 127 it is nearly one throughout. This is a substantial
soft/high-note approximation. Restoring full preload alone would mismatch
the released boundary and can add a click; a causal correction must treat
the string and body together. The present work corrects the continuous
periodic integration without claiming to resolve that separate model.

The contact remains a passive phase-equivalent point in a dispersive
waveguide, not a spatially resolved stiff string and fingertip. Extreme
short loops necessarily retain fewer harmonics. Numerical finite/bounded
tests do not imply that the artificial C9 pitch at a 22.05 kHz engine rate
has the same timbre as an oversampled render.

The final rebuilt source, including the setup-cache correction and exact
CPU transformation, passes all 12 CTests (nine unit, three audio) in
400.25 seconds under shared load. Its C++17/no-exceptions library also
builds successfully. The frozen final source also passes strict Clang 19
C++17 compilation for x86-64 and AArch64 with warnings as errors, exceptions
disabled and floating contraction off. This is ARM compile coverage, not
ARM execution or a Reason SDK build.

The final-source native Rack host-mock run passes MIDI/CV ownership,
controls, reset, sample-offset, 2x/4x and callback-allocation contracts.
All nine factory-level checks pass the final source unchanged. These authored
mocks do not substitute for SDK/Reason validation. Shared-machine
test wall time is not a CPU comparison.

Two off-tree preload prototypes were evaluated and not adopted. A causal
exponential convolution of the entire ideal linear response confirms that
the existing periodic approximation can materially change the first 20 ms
of soft/high notes. Replacing first-cell preload with its continuous initial
boundary value does not resolve the low-rate gut C7 level discrepancy
(-43.96 versus -36.98 dBFS at 22.05/48 kHz in that trial). The scalar modal
equilibrium identity supports a pointwise boundary state, but a complete
causal body/string treatment needs separate validation; neither experiment
was used to retune the shipping candidate.

The acoustic A/B set uses the public Performer and existing committed
dynamics/shared-phrase MIDI, plus a six-second palm/re-pluck bend witness.
It is an aggregate correctness-change review, not a one-mechanism preference
test. Files and separate keys/raw levels are kept outside the checkout.
Whole-file stereo RMS is matched, and no listening verdict is claimed.

The final audition set is under
`/workspace/scratch/aculyre-evidence/listening-final/`, with complete native
and matched A/B files, separate keys/levels and a source manifest. Regeneration
from the final source proves byte-identical native PCM to the earlier acoustic
set for all six phrase clips and the targeted bend/contact witness. Independent
validation passes dimensions, finite samples, positive scalar raw-to-matched
relation, whole-file RMS matching (worst mismatch 1.18e-9 dB) and 4x reconstructed
peaks (worst -3.486 dBFS). This verifies the files, not a listening preference.

The final CPU phase is documented in
[the native timing report](../benchmarks/cpu-2026-11-09.md). The exact contact-wrap
change is byte-identical over 72 Performer cases and 2,764,800 float samples.
It reduces simultaneous all-string recontact CPU by a median 31.51% across
nine material/factor cases and rapid re-pluck workload CPU by 3.37%. The dense
workload is 2.39% slower in that CPU-only comparison (eight of nine case medians),
so this is a targeted event-cost improvement, not a universal speedup. All-string
reattacks still exceed a 64-frame deadline; the report retains that limitation.

The final-source survey rerenders all 36 settings / 4,608 keys, with 3,168
supported sounding cases and 1,440 exactly silent unsupported cases. Its
source hashes, PCM hashes and level/spectrum CSVs are retained under
`/workspace/scratch/aculyre-evidence/final-surveys/`. All 18 settings at 48 kHz
are byte-identical to the earlier acoustic candidate's PCM; the 44.1, 96 and
192 kHz level CSVs also remain identical. The continuous short-loop correction
changes a few virtual treble keys at 22.05 kHz, at most -0.988 dB in attack
level versus that earlier candidate (gut C8). Native-compass results and the
retained gut C7 cross-rate discrepancy above are unchanged.

Independent final-summary checks reproduce the velocity minima and verify
all source hashes. The 30 settings with spectral analysis have zero early/late
search-boundary peak flags; the six minimum-velocity settings use RMS/finite
checks, without a spectral CSV. Confidence metadata and tracked-partial
identity remain part of the measurements, not a guarantee of one fundamental
or decay rate for a beating, coupled string/body system.

A separate safe low-rate review witness is under
`/workspace/scratch/aculyre-evidence/listening-short-loop/`: six one-second
gut virtual-treble plucks at 22.05 kHz, with matched A/B and native files.
Both unlimited baseline/final Engines are verified finite before using the
protected Performer audio. It deliberately excludes the unstable +12-semitone
baseline, whose regression-test failure is the appropriate evidence. No
listening preference is asserted.
