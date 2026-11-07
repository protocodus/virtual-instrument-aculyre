// Aculyre's prototype instruments. Every value is a published material
// constant, a dimension typical of the instrument, or marked as authored or
// estimated; none is a measurement of a particular lyre.

#pragma once

#include "AculyreEngine.h"

namespace aculyre
{

// Plain music wire. Its dislocation Q of 5500 is Paté, Le Carrou and Fabre's
// fit (JASA 135 (2014) 3045) and its bending loss factor 7.8e-4 Acustra's
// joint refit to guitar recordings (FittedPhysicalData.h:
// steelPlainBendingLoss, its candidate C). Acustra ships 70% of the way from
// C toward 0.003, chosen by ear on a guitar; on the DR lyre harp's notes
// that value left the model's 0.3-1.5 s from 3 to 10 kHz 4 to 12 dB short of
// the recording's, and C brings 2 to 4.5 dB of it back (Docs/decisions.md,
// 2026-10-30). A wound string takes Acustra's wound
// steel, both by ear: dry friction between its turns, a loss angle of 2.25e-4
// on its tension, and a bending loss factor of 0.0175 in place of the plain
// wire's (FittedPhysicalData.h, after steelPlainBendingLoss).
inline constexpr StringMaterial steelWire { "steel", 7850.0, 2.0e11, 0.0, 1.0 / 5500.0,
                                            0.00078125 * 2.0e11, false, 2.25e-4, 0.0175 };
// Monofilament nylon: density 1072-1159 kg/m^3 measured on harp strings
// (Lynch-Aird and Woodhouse, Materials 11 (2018) 2160, Table 1). Stiffness
// and damping are Woodhouse and Lynch-Aird's fit to their measured nylon
// strings (Acta Acustica 105 (2019) 516, Table I): E' = 4.5 + 39 sigma GPa,
// E'' = 0.25 GPa, air damping by their Equation (13). The fit needs no loss
// constant over the band.
inline constexpr StringMaterial nylonMonofilament { "nylon", 1120.0, 4.5e9, 39.0, 0.0, 0.25e9, true };
// Natural gut: 1320 kg/m^3 (Materials 11 (2018) 2160, Table 1); E' = 6 GPa
// and E'' = 0.24 GPa from the same fit (Acta Acustica 105 (2019) 516,
// Table I), which found gut's damping more scattered from string to string
// than the polymers'.
inline constexpr StringMaterial naturalGut { "gut", 1320.0, 6.0e9, 0.0, 0.0, 0.24e9, true };

// Quarter-cut representative tops at 12% moisture, from the USDA Wood
// Handbook as Acustra tabulates them (AcustraEngine.cpp, woodSpecs).
//
// Spruce's loss factors are Viala, Placet and Cogan's means over thirty
// tonewood samples ("Simultaneous non-destructive identification of
// multiple elastic and damping properties of spruce tonewood to improve
// grading", Journal of Cultural Heritage, 2020; Table 2 of the preprint): 0.73% along the grain, 1.7%
// across it and 1.2% in LR shear. They measured European spruce (Picea
// abies), a sibling of the Sitka whose stiffness the top takes; its moduli
// (12.8 and 1.01 GPa, 0.81 GPa shear) sit beside the Handbook's. No source
// gives maple's in all three directions, so it has none.
inline constexpr WoodSpec sitkaSpruce { 448.0, 11.88e9, 11.88e9 * 0.078, 11.88e9 * 0.064, 0.372,
                                        0.0073, 0.017, 0.012 };
inline constexpr WoodSpec hardMaple { 705.6, 13.86e9, 13.86e9 * 0.132, 13.86e9 * 0.111, 0.424 };

namespace detail
{
constexpr double inch = 0.0254;

// The widest lyre harp sold today, 27 strings in diatonic C major from B2
// to G6 (Pures Music's 21/24/27-string range; a 24-string harp runs D3 to
// F6, Thomann LH24), so with the levers every note from B2 to G#6 plays.
// Its middle sixteen, G3 to A5, are the 16-string lyre harp's (Early Music
// Shop, Larkin in the Morning: steel strings of 0.010-0.028 inch on a frame
// about 25 inches tall), unchanged: their speaking lengths fall from 44 to
// 22 cm along the slanted yoke (assumed), and the eleven added strings keep
// its 1.47 cm a string, 51 cm at B2 and 13 cm at G6. G3 to A5 carry 54-63
// N, under half the wire's strength; the treble's plain .0095 and .009
// carry 54-59 N at 1.25-1.41 GPa, about half a .009 music wire's strength.
// The bass is nickel wound, as Roosebeck's set for a harp tuned up from D3
// winds it: D3 at .028, E3-G3 at .024, and C3 and B2 continue it at .030
// and .032 (46-55 N). Their masses are D'Addario's published unit weights
// (NW024 0.00010857, NW028 0.00014666, NW030 0.00017236, NW032 0.00019347
// lb/in) and their cores Acustra's authored effective cores, 0.38 mm for a
// .024 and 0.412 for a .032 (AcustraEngine.cpp, steelBendingDiameter),
// interpolated between. G3's winding was chosen by ear over a plain .022 on
// 2026-10-28 (Docs/decisions.md). The soundbox is the 16-string harp's,
// whose top the DR lyre harp's notes chose (lyreHarpBody): a 27-string harp
// is larger, so its body here is an assumption kept from the smaller one.
//
// Each string's fourth value is an even touch, a voicing rather than a
// measurement: as a player evens out a scale by ear, the strings the
// soundbox favours are plucked more softly. It puts the string's level over
// its first 300 ms at velocity 90 (the instrument's own controls, without the
// room) on the straight line through every string's level, so the register
// keeps its slope and no string stands out of it. Without it the body's
// modes left the steel G5 6.8 dB over that line and D5 8.3 under, the nylon
// G4 9.3 over and D5 9.0 under, and the gut lyre's E3 5.6 over and D5 7.4
// under (0.7.2).
//
// The fifth value is the same touch by the piezo, with the piezo alone. The
// piezo hears the force on the bridge bar rather than what the soundbox
// radiates, so it favours other strings: the microphones' touch would leave
// the piezo's worst string 10.2 dB off its line on steel, 9.6 on nylon and
// 7.2 on gut. Between the two pickups the touch blends geometrically, as
// the PIEZO control blends what is heard. Each instrument's strongest touch
// is 1, so no string is plucked harder than pluckForce.
//
// AculyreEngineTests holds every string within 1 dB of the line by each
// pickup and reports the touch each would need after a change to the strings
// or the body.
constexpr double poundPerInch = 17.857967;  // kg/m
constexpr std::array<StringSpec, maxStrings> lyreHarpSteel {{
    { 47, 0.513, 0.032 * inch, 0.86, 0.59, 0.00019347 * poundPerInch, 0.412e-3 },
    { 48, 0.499, 0.030 * inch, 1.00, 0.62, 0.00017236 * poundPerInch, 0.404e-3 },
    { 50, 0.484, 0.028 * inch, 0.86, 0.60, 0.00014666 * poundPerInch, 0.396e-3 },
    { 52, 0.469, 0.024 * inch, 0.86, 0.57, 0.00010857 * poundPerInch, 0.38e-3 },
    { 53, 0.455, 0.024 * inch, 0.64, 0.58, 0.00010857 * poundPerInch, 0.38e-3 },
    { 55, 0.440, 0.024 * inch, 0.44, 0.63, 0.00010857 * poundPerInch, 0.38e-3 }, { 57, 0.425, 0.020 * inch, 0.39, 0.67 },
    { 59, 0.411, 0.019 * inch, 0.45, 0.58 }, { 60, 0.396, 0.018 * inch, 0.30, 0.59 },
    { 62, 0.381, 0.017 * inch, 0.43, 0.48 }, { 64, 0.367, 0.016 * inch, 0.40, 0.49 },
    { 65, 0.352, 0.015 * inch, 0.32, 0.49 }, { 67, 0.337, 0.014 * inch, 0.30, 0.58 },
    { 69, 0.323, 0.014 * inch, 0.25, 0.83 }, { 71, 0.308, 0.013 * inch, 0.19, 0.79 },
    { 72, 0.293, 0.012 * inch, 0.14, 0.93 }, { 74, 0.279, 0.012 * inch, 0.23, 0.80 },
    { 76, 0.264, 0.011 * inch, 0.34, 0.64 }, { 77, 0.249, 0.011 * inch, 0.32, 0.62 },
    { 79, 0.235, 0.010 * inch, 0.23, 0.59 }, { 81, 0.220, 0.010 * inch, 0.56, 0.64 },
    { 83, 0.205, 0.0095 * inch, 0.19, 0.71 }, { 84, 0.191, 0.0095 * inch, 0.16, 0.69 },
    { 86, 0.176, 0.009 * inch, 0.15, 1.00 }, { 88, 0.161, 0.009 * inch, 0.33, 0.93 },
    { 89, 0.147, 0.009 * inch, 0.37, 0.79 }, { 91, 0.132, 0.009 * inch, 0.24, 0.71 }
}};

// The same frame strung with nylon at 45 N in the bass falling to 23 N at
// G6 (diameters from T = mu (2 L f)^2 at those tensions; B2 to F3 at 45 N,
// G3 to A5 as the 16-string harp's, B5 to G6 a newton less a string, 180
// to 200 MPa). Plain monofilament for B2 to D3 would be 1.6-1.8 mm, so
// stiff (B 1.6-2.2e-3) that the loop's dispersion could not follow its
// partials (up to 142 cents from the stiff-string law): as a lever harp's
// bass is, they are wound, a 1.0 mm nylon core (estimated) under a copper
// wrap that brings them to 45 N (mu = 45 / (2 L f)^2), about 1.1 mm over
// the wrap.
constexpr std::array<StringSpec, maxStrings> lyreHarpNylon {{
    { 47, 0.513, 1.13e-3, 0.82, 0.71, 2.804e-3, 1.0e-3 },
    { 48, 0.499, 1.12e-3, 0.94, 0.73, 2.640e-3, 1.0e-3 },
    { 50, 0.484, 1.09e-3, 0.83, 0.71, 2.228e-3, 1.0e-3 },
    { 52, 0.469, 1.46e-3, 1.00, 0.74 },
    { 53, 0.455, 1.42e-3, 0.75, 0.76 },
    { 55, 0.440, 1.30e-3, 0.42, 0.81 }, { 57, 0.425, 1.18e-3, 0.40, 0.88 },
    { 59, 0.411, 1.08e-3, 0.47, 0.76 }, { 60, 0.396, 1.05e-3, 0.30, 0.77 },
    { 62, 0.381, 0.96e-3, 0.48, 0.64 }, { 64, 0.367, 0.87e-3, 0.41, 0.60 },
    { 65, 0.352, 0.85e-3, 0.31, 0.60 }, { 67, 0.337, 0.78e-3, 0.25, 0.64 },
    { 69, 0.323, 0.72e-3, 0.21, 1.00 }, { 71, 0.308, 0.66e-3, 0.15, 0.80 },
    { 72, 0.293, 0.64e-3, 0.11, 0.92 }, { 74, 0.279, 0.60e-3, 0.23, 0.83 },
    { 76, 0.264, 0.55e-3, 0.29, 0.75 }, { 77, 0.249, 0.54e-3, 0.36, 0.73 },
    { 79, 0.235, 0.51e-3, 0.24, 0.67 }, { 81, 0.220, 0.47e-3, 0.66, 0.75 },
    { 83, 0.205, 0.44e-3, 0.14, 0.63 }, { 84, 0.191, 0.44e-3, 0.15, 0.73 },
    { 86, 0.176, 0.42e-3, 0.12, 0.96 }, { 88, 0.161, 0.40e-3, 0.21, 0.97 },
    { 89, 0.147, 0.40e-3, 0.25, 0.81 }, { 91, 0.132, 0.39e-3, 0.24, 0.72 }
}};

// The lyre harp's soundbox: a 20 x 30 cm top of 3 mm spruce on a box 5 cm
// deep, one 6 cm soundhole centred 0.70 of the way up the top (assumed), a
// 6 g maple bridge bar a centimetre high a
// third of the way up the top. Early Music Shop's 16-string lyre has a solid
// spruce top on a beech body; Larkin in the Morning's has a birch ply top on
// sheesham (their listings). The dimensions are assumed, typical of the
// instrument. The top is 3 mm, the thickness the sampled lyre harps are
// built with, its first mode at 434 Hz; the owner chose it by ear in 0.10.0
// over the 6 mm top that 0.7.2 had fitted to the DR lyre harp's
// third-octave shape as a stand-in for bracing (Docs/decisions.md). The
// measurements did not settle it: against the DR harp's 16 strings it
// leaves the fundamentals' decay 0.99 (log2 rms) and partials 2-6 0.78
// against the 6 mm top's 0.92 and 0.61, while against the Lyre Lyre it
// brings the shape from 10.9 to 5.6 dB rms and the sustain's spread over
// the naturals from 3.9 to 2.3 dB (the recording's 2.5). The back is a
// flat 4 mm maple plate, clamped, whose modes below 4 kHz move with the top's
// through the air in the box (an assumed thickness; Docs/decisions.md).
constexpr BodySpec lyreHarpBody {
    0.20, 0.30, 0.003, sitkaSpruce,
    0.20 * 0.30 * 0.05 * 0.85, 0.03,
    0.30, 0.10, 0.90, 0.010, 0.006, hardMaple,
    0.004, hardMaple, 0.70
};

// A gut lyre after the Anglo-Saxon instruments (the Sutton Hoo and
// Trossingen reconstructions): six strings about 50 cm, tuned here
// D3 E3 G3 A3 B3 D4 at about 37 N, over a hollowed maple soundbox with a
// 4 mm top, no soundhole and a 4 g maple bridge. Dimensions are typical of
// reconstructions, not measured. It is extended past any reconstruction
// (owner request, 2026-10-29) to the whole G major scale from D3 to E5,
// sixteen strings, so with the sharping levers every note from D3 to F5
// plays: F#3 and C4 fill the six strings' gaps at 50 cm and 37 N, and from
// E4 up a slanted yoke shortens each string to hold D4's stress (114 MPa,
// rho (2 L f)^2, about a third of gut's strength), its tension falling to
// 30 N at E5 as a harp's treble does. Sixteen strings share the six's 9 cm
// bridge bar, 6 mm apart.
constexpr std::array<StringSpec, maxStrings> gutLyreStrings {{
    { 50, 0.500, 1.29e-3, 0.41, 0.89 }, { 52, 0.500, 1.15e-3, 0.35, 0.88 }, { 54, 0.500, 1.02e-3, 0.64, 0.84 },
    { 55, 0.500, 0.96e-3, 0.45, 1.00 }, { 57, 0.500, 0.86e-3, 0.62, 0.84 }, { 59, 0.500, 0.76e-3, 0.79, 0.80 },
    { 60, 0.500, 0.72e-3, 0.63, 0.85 }, { 62, 0.500, 0.64e-3, 0.47, 0.85 }, { 64, 0.445, 0.64e-3, 0.37, 0.86 },
    { 66, 0.397, 0.63e-3, 0.76, 0.81 }, { 67, 0.375, 0.62e-3, 0.51, 0.99 }, { 69, 0.334, 0.61e-3, 0.70, 0.93 },
    { 71, 0.297, 0.60e-3, 1.00, 0.84 }, { 72, 0.281, 0.60e-3, 0.78, 0.83 }, { 74, 0.250, 0.59e-3, 0.64, 0.92 },
    { 76, 0.223, 0.58e-3, 0.84, 0.90 }
}};
// The carved back is too thick to move (backThickness 0).
constexpr BodySpec gutLyreBody {
    0.36, 0.18, 0.004, hardMaple,
    0.36 * 0.18 * 0.025 * 0.8, 0.0,
    0.15, 0.25, 0.75, 0.012, 0.004, hardMaple,
    0.0, hardMaple
};
} // namespace detail

// pluckPosition: 0.27 of the length from the bridge, where the player of
// DeadRobot's DR Plucked Lyre Harp samples plucks: fitted to each note's
// partials, the middle half of its 48 plucks fall at 0.25-0.32, median 0.27.
// It was chosen by ear over 0.47, where the player of the lyre harp phrase
// (Freesound 649925, CC BY) plucks four of its eight notes (Docs/decisions.md,
// 2026-10-29). On the DR harp's notes, each at equal power, the steel harp's
// third-octave shape from 200 Hz to 10 kHz sits 6.3 dB rms from the
// recording's over the first 0.3 s, where 0.47 left 7.6; on the phrase, 0.47
// kept nearer (5.3 dB rms, a quarter 7.2). The CC0 A4 (Freesound 722897) was
// plucked at a quarter, which POSITION still reaches. The nylon harp is
// assumed to be played alike. The gut lyre is plucked at 0.11, where the
// player of the gut lyre recording (Freesound 191883, CC0) plucks: replayed
// note by note (Tools/compare_gut_lyre_phrase.py), the medians of its 2nd
// to 12th partials against the fundamental sit 3.5 dB rms from the
// recording's, where 0.27 left 10.4 (0.09 4.7, 0.10 4.0, 0.12 4.2, 0.16
// 7.4). outputGain and piezoGain are the
// free gains, not loudness claims: outputGain puts the preset's typical
// playing at a median of -9 on Reason's VU meter at its own controls, and
// piezoGain puts the piezo alone within 1 dB of that (AculyreEngineTests,
// README Level).
inline constexpr InstrumentSpec lyreHarpSteel {
    "Lyre Harp", steelWire, 27, detail::lyreHarpSteel, detail::lyreHarpBody,
    Technique::Finger, 0.27, 0.25, 2.0, 0.35, 2.182, 0.484
};

inline constexpr InstrumentSpec lyreHarpNylon {
    "Nylon Lyre Harp", nylonMonofilament, 27, detail::lyreHarpNylon, detail::lyreHarpBody,
    Technique::Finger, 0.27, 0.25, 2.0, 0.35, 2.459, 0.443
};

inline constexpr InstrumentSpec gutLyre {
    "Gut Lyre", naturalGut, 16, detail::gutLyreStrings, detail::gutLyreBody,
    Technique::Finger, 0.11, 0.3, 2.5, 0.35, 2.498, 0.441
};

inline constexpr std::array<const InstrumentSpec*, 3> presets {{
    &lyreHarpSteel, &lyreHarpNylon, &gutLyre
}};

} // namespace aculyre
