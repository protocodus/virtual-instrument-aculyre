# Damping contact coordinates

Candidate 0.12.2, based on `da1a4be`. This is a correctness correction with a
listening comparison pending, not a preference verdict.

At 48 kHz the steel C3 loop's full phase period was 366.855 samples, its
explicit line only 262. A contact at 0.27 of the string requested a return
tap past that line and clipped it to age 261. After an octave bend the line
was 79 samples long but the old ages stayed 50 and 261. Dispersion and tuning
filters store the rest of the propagation phase; line length is not period.

The candidate retains the physical long-arc taps wherever they fit. Otherwise
it uses separation `min(x, 1-x) D`, where `D` is the current full fundamental
period, centred inside the live line. For ideal standing partial
`n`, the mirror-equivalent short and long arcs have the same
`sin²(n pi x)` participation. The default hand point fits every native steel
string through ±12 semitones at 44.1, 48 and 96 kHz. The contact is refreshed
after each control-period delay update, including on idle strings under the
sympathy hand. Fractional interpolation avoids whole-sample contact jumps.

Read and scatter interpolation are adjoints: with `g` the difference of the
two kernels, `q' = q - a g (gᵀq)`. The stored squared-wave norm drops by
`a (2-a ||g||²) (gᵀq)²`, nonnegative for the implemented `a <= 1/4` and
`||g||² <= 2`. This is a local contact-work check, not a Joule ledger of the
complete instrument. Material and finger losses may become heat.

The lumped dispersive loop still cannot represent an exact physical contact
at every stiff-string partial. If an extreme bend/position leaves too little
pure delay for even the short arc, the separation is bounded symmetrically;
`contactReport()` exposes both requested and realised coordinates.

Validation uses GCC 14 Release and the C++17/no-exceptions DSP build.
`Aculyre.ContactPhysics` checks independent sinusoidal participation and
nodes, fractional interpolation's analytic error bound, actual wave-norm
contraction including overlapping kernels, and live engine contact geometry
and phase-period tracking through bends. Its initial run passed. The full
engine suite and separate one-change listening package are recorded in the
off-tree review manifest after the candidate is frozen.
