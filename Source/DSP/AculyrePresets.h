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
inline constexpr WoodSpec sitkaSpruce { 448.0, 11.88e9, 11.88e9 * 0.078, 11.88e9 * 0.064, 0.372 };
inline constexpr WoodSpec hardMaple { 705.6, 13.86e9, 13.86e9 * 0.132, 13.86e9 * 0.111, 0.424 };

namespace detail
{
constexpr double inch = 0.0254;

// The 16-string lyre harp sold today: diatonic C major from G3 to A5, steel
// strings of 0.010-0.028 inch on a frame about 25 inches tall (retail
// listings: Early Music Shop, Larkin in the Morning). Speaking lengths fall
// from 44 to 22 cm along the slanted yoke (assumed), and the gauges are
// chosen so every string carries 54-63 N, under half the wire's strength.
// G3 is a nickel-wound .024, as Roosebeck's set winds it. That set is for a
// harp tuned up from D3 and winds D3 at .028 and E3-G3 at .024; no set for a
// G3-A5 harp lists its gauges, so only the note both share is wound.
// Its mass is D'Addario's published unit weight for that string (NW024,
// 0.00010857 lb/in) and its core the 0.38 mm Acustra authors for its own
// .024 wound G (AcustraEngine.cpp, steelBendingDiameter). It was chosen by
// ear over a plain .022 on 2026-10-28 (Docs/decisions.md).
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
constexpr std::array<StringSpec, maxStrings> lyreHarpSteel {{
    { 55, 0.440, 0.024 * inch, 0.61, 0.75, 0.00010857 * 17.857967, 0.38e-3 }, { 57, 0.425, 0.020 * inch, 0.40, 0.70 },
    { 59, 0.411, 0.019 * inch, 0.35, 0.68 }, { 60, 0.396, 0.018 * inch, 0.32, 0.69 },
    { 62, 0.381, 0.017 * inch, 0.37, 0.69 }, { 64, 0.367, 0.016 * inch, 0.33, 0.66 },
    { 65, 0.352, 0.015 * inch, 0.31, 0.67 }, { 67, 0.337, 0.014 * inch, 0.15, 0.70 },
    { 69, 0.323, 0.014 * inch, 0.17, 0.74 }, { 71, 0.308, 0.013 * inch, 0.52, 0.70 },
    { 72, 0.293, 0.012 * inch, 0.52, 0.71 }, { 74, 0.279, 0.012 * inch, 1.00, 0.66 },
    { 76, 0.264, 0.011 * inch, 0.89, 0.66 }, { 77, 0.249, 0.011 * inch, 0.74, 0.64 },
    { 79, 0.235, 0.010 * inch, 0.20, 0.59 }, { 81, 0.220, 0.010 * inch, 0.17, 1.00 }
}};

// The same frame strung with nylon at 45 N in the bass falling to 30 N in
// the treble (diameters from T = mu (2 L f)^2 at those tensions).
constexpr std::array<StringSpec, maxStrings> lyreHarpNylon {{
    { 55, 0.440, 1.30e-3, 0.68, 0.90 }, { 57, 0.425, 1.18e-3, 0.50, 0.88 },
    { 59, 0.411, 1.08e-3, 0.35, 0.86 }, { 60, 0.396, 1.05e-3, 0.23, 0.86 },
    { 62, 0.381, 0.96e-3, 0.27, 0.85 }, { 64, 0.367, 0.87e-3, 0.30, 0.82 },
    { 65, 0.352, 0.85e-3, 0.29, 0.82 }, { 67, 0.337, 0.78e-3, 0.12, 0.75 },
    { 69, 0.323, 0.72e-3, 0.20, 0.94 }, { 71, 0.308, 0.66e-3, 0.48, 0.88 },
    { 72, 0.293, 0.64e-3, 0.47, 0.87 }, { 74, 0.279, 0.60e-3, 1.00, 0.83 },
    { 76, 0.264, 0.55e-3, 0.82, 0.80 }, { 77, 0.249, 0.54e-3, 0.75, 0.80 },
    { 79, 0.235, 0.51e-3, 0.22, 0.80 }, { 81, 0.220, 0.47e-3, 0.14, 1.00 }
}};

// The lyre harp's soundbox: a 20 x 30 cm top of 6 mm spruce on a box 5 cm
// deep, one 6 cm soundhole, a 6 g maple bridge bar a centimetre high a
// third of the way up the top. Early Music Shop's 16-string lyre has a solid
// spruce top on a beech body; Larkin in the Morning's has a birch ply top on
// sheesham (their listings). The dimensions are assumed, typical of the
// instrument, except the top's thickness, which the DR lyre harp's notes
// chose among 3 to 8 mm over four draws of the top's irregularities: the
// third-octave shape 4.6 / 5.2 dB rms (first 0.3 s / after) and the
// fundamentals' decay 1.07 (log2 rms) at 6 mm, against 5.6 / 6.7 and 1.41
// at 3 mm and 5.1 / 6.1 and 0.97 at 5 mm. The top is unbraced, so its
// thickness also stands in for the bracing a real top carries.
constexpr BodySpec lyreHarpBody {
    0.20, 0.30, 0.006, sitkaSpruce,
    0.20 * 0.30 * 0.05 * 0.85, 0.03,
    0.30, 0.10, 0.90, 0.010, 0.006, hardMaple
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
    { 50, 0.500, 1.29e-3, 0.39, 0.76 }, { 52, 0.500, 1.15e-3, 0.15, 0.78 }, { 54, 0.500, 1.02e-3, 0.26, 0.73 },
    { 55, 0.500, 0.96e-3, 0.43, 0.71 }, { 57, 0.500, 0.86e-3, 0.45, 0.64 }, { 59, 0.500, 0.76e-3, 0.19, 0.75 },
    { 60, 0.500, 0.72e-3, 0.23, 0.72 }, { 62, 0.500, 0.64e-3, 0.31, 0.64 }, { 64, 0.445, 0.64e-3, 0.18, 0.62 },
    { 66, 0.397, 0.63e-3, 0.37, 0.63 }, { 67, 0.375, 0.62e-3, 0.53, 0.66 }, { 69, 0.334, 0.61e-3, 0.40, 0.65 },
    { 71, 0.297, 0.60e-3, 0.23, 1.00 }, { 72, 0.281, 0.60e-3, 0.27, 0.86 }, { 74, 0.250, 0.59e-3, 1.00, 0.75 },
    { 76, 0.223, 0.58e-3, 0.50, 0.63 }
}};
constexpr BodySpec gutLyreBody {
    0.36, 0.18, 0.004, hardMaple,
    0.36 * 0.18 * 0.025 * 0.8, 0.0,
    0.15, 0.25, 0.75, 0.012, 0.004, hardMaple
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
// plucked at a quarter, which POSITION still reaches. The nylon harp and the
// gut lyre are assumed to be played alike. outputGain and piezoGain are the
// free gains, not loudness claims: outputGain puts the preset's typical
// playing at a median of -9 on Reason's VU meter at its own controls, and
// piezoGain puts the piezo alone within 1 dB of that (AculyreEngineTests,
// README Level).
inline constexpr InstrumentSpec lyreHarpSteel {
    "Lyre Harp", steelWire, 16, detail::lyreHarpSteel, detail::lyreHarpBody,
    Technique::Finger, 0.27, 0.25, 2.0, 0.35, 3.519, 0.363
};

inline constexpr InstrumentSpec lyreHarpNylon {
    "Nylon Lyre Harp", nylonMonofilament, 16, detail::lyreHarpNylon, detail::lyreHarpBody,
    Technique::Finger, 0.27, 0.25, 2.0, 0.35, 4.738, 0.352
};

inline constexpr InstrumentSpec gutLyre {
    "Gut Lyre", naturalGut, 16, detail::gutLyreStrings, detail::gutLyreBody,
    Technique::Finger, 0.27, 0.3, 2.5, 0.35, 2.711, 0.307
};

inline constexpr std::array<const InstrumentSpec*, 3> presets {{
    &lyreHarpSteel, &lyreHarpNylon, &gutLyre
}};

} // namespace aculyre
