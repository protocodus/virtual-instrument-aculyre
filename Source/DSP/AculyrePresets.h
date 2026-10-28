// Aculyre's prototype instruments. Every value is a published material
// constant, a dimension typical of the instrument, or marked as authored or
// estimated; none is a measurement of a particular lyre.

#pragma once

#include "AculyreEngine.h"

namespace aculyre
{

// Plain music wire. Its dislocation Q of 5500 is Paté, Le Carrou and Fabre's
// fit (JASA 135 (2014) 3045) and its bending loss factor Acustra's plain
// steel (FittedPhysicalData.h: steelPlainBendingLoss, a by-ear point between
// two fits to steel-string recordings). A wound string takes Acustra's wound
// steel, both by ear: dry friction between its turns, a loss angle of 2.25e-4
// on its tension, and a bending loss factor of 0.0175 in place of the plain
// wire's (FittedPhysicalData.h, after steelPlainBendingLoss).
inline constexpr StringMaterial steelWire { "steel", 7850.0, 2.0e11, 0.0, 1.0 / 5500.0,
                                            0.002334375 * 2.0e11, false, 2.25e-4, 0.0175 };
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
// modes left the steel A4 6.5 dB over that line and G5 6.7 under, the nylon
// A4 9.1 over and G5 7.4 under, and the gut lyre's E3 6.8 over and A3 5.7
// under.
//
// The fifth value is the same touch by the piezo, with the piezo alone. The
// piezo hears the force on the bridge bar rather than what the soundbox
// radiates, so it favours other strings: the microphones' touch would leave
// the piezo's worst string 7.6 dB off its line on steel, 7.8 on nylon and
// 7.0 on gut. Between the two pickups the touch blends geometrically, as
// the PIEZO control blends what is heard. Each instrument's strongest touch
// is 1, so no string is plucked harder than pluckForce.
//
// AculyreEngineTests holds every string within 1 dB of the line by each
// pickup and reports the touch each would need after a change to the strings
// or the body.
constexpr std::array<StringSpec, maxStrings> lyreHarpSteel {{
    { 55, 0.440, 0.024 * inch, 0.89, 0.63, 0.00010857 * 17.857967, 0.38e-3 }, { 57, 0.425, 0.020 * inch, 0.44, 0.73 },
    { 59, 0.411, 0.019 * inch, 0.41, 0.63 }, { 60, 0.396, 0.018 * inch, 0.45, 0.60 },
    { 62, 0.381, 0.017 * inch, 0.57, 0.52 }, { 64, 0.367, 0.016 * inch, 0.54, 0.47 },
    { 65, 0.352, 0.015 * inch, 0.50, 0.48 }, { 67, 0.337, 0.014 * inch, 0.35, 0.45 },
    { 69, 0.323, 0.014 * inch, 0.22, 0.47 }, { 71, 0.308, 0.013 * inch, 0.26, 0.63 },
    { 72, 0.293, 0.012 * inch, 0.34, 1.00 }, { 74, 0.279, 0.012 * inch, 0.49, 0.72 },
    { 76, 0.264, 0.011 * inch, 0.47, 0.57 }, { 77, 0.249, 0.011 * inch, 0.53, 0.57 },
    { 79, 0.235, 0.010 * inch, 1.00, 0.53 }, { 81, 0.220, 0.010 * inch, 0.52, 0.59 }
}};

// The same frame strung with nylon at 45 N in the bass falling to 30 N in
// the treble (diameters from T = mu (2 L f)^2 at those tensions).
constexpr std::array<StringSpec, maxStrings> lyreHarpNylon {{
    { 55, 0.440, 1.30e-3, 0.89, 0.79 }, { 57, 0.425, 1.18e-3, 0.37, 0.86 },
    { 59, 0.411, 1.08e-3, 0.37, 0.82 }, { 60, 0.396, 1.05e-3, 0.45, 0.78 },
    { 62, 0.381, 0.96e-3, 0.57, 0.70 }, { 64, 0.367, 0.87e-3, 0.54, 0.64 },
    { 65, 0.352, 0.85e-3, 0.50, 0.63 }, { 67, 0.337, 0.78e-3, 0.32, 0.55 },
    { 69, 0.323, 0.72e-3, 0.15, 0.49 }, { 71, 0.308, 0.66e-3, 0.20, 0.76 },
    { 72, 0.293, 0.64e-3, 0.28, 1.00 }, { 74, 0.279, 0.60e-3, 0.46, 0.93 },
    { 76, 0.264, 0.55e-3, 0.52, 0.83 }, { 77, 0.249, 0.54e-3, 0.54, 0.75 },
    { 79, 0.235, 0.51e-3, 1.00, 0.70 }, { 81, 0.220, 0.47e-3, 0.46, 0.70 }
}};

// The lyre harp's soundbox: a 20 x 30 cm top of 3 mm spruce on a box 5 cm
// deep, one 6 cm soundhole, a 6 g maple bridge bar a centimetre high a
// third of the way up the top. Early Music Shop's 16-string lyre has a solid
// spruce top on a beech body; Larkin in the Morning's has a birch ply top on
// sheesham (their listings). The dimensions are assumed, typical of the
// instrument.
constexpr BodySpec lyreHarpBody {
    0.20, 0.30, 0.003, sitkaSpruce,
    0.20 * 0.30 * 0.05 * 0.85, 0.03,
    0.30, 0.10, 0.90, 0.010, 0.006, hardMaple
};

// A six-string gut lyre after the Anglo-Saxon instruments (the Sutton Hoo
// and Trossingen reconstructions): strings about 50 cm, tuned here
// D3 E3 G3 A3 B3 D4 at about 40 N, over a hollowed maple soundbox with a
// 4 mm top, no soundhole and a 4 g maple bridge. Dimensions are typical of
// reconstructions, not measured.
constexpr std::array<StringSpec, maxStrings> gutLyreStrings {{
    { 50, 0.50, 1.29e-3, 0.73, 0.98 }, { 52, 0.50, 1.15e-3, 0.24, 1.00 }, { 55, 0.50, 0.96e-3, 0.69, 0.99 },
    { 57, 0.50, 0.86e-3, 1.00, 0.94 }, { 59, 0.50, 0.76e-3, 0.34, 1.00 }, { 62, 0.50, 0.64e-3, 0.49, 1.00 }
}};
constexpr BodySpec gutLyreBody {
    0.36, 0.18, 0.004, hardMaple,
    0.36 * 0.18 * 0.025 * 0.8, 0.0,
    0.15, 0.25, 0.75, 0.012, 0.004, hardMaple
};
} // namespace detail

// pluckPosition: the CC0 lyre harp reference (Freesound 722897, A4) lacks its
// 4th and 8th harmonics, a pluck a quarter of the way from the bridge; the
// gut lyre's is assumed. outputGain and piezoGain are the free gains, not
// loudness claims: outputGain puts the preset's typical playing at a median
// of -9 on Reason's VU meter at its own controls, and piezoGain puts the
// piezo alone within 1 dB of that (AculyreEngineTests, README Level).
inline constexpr InstrumentSpec lyreHarpSteel {
    "Lyre Harp", steelWire, 16, detail::lyreHarpSteel, detail::lyreHarpBody,
    Technique::Finger, 0.25, 0.25, 2.0, 0.35, 1.306, 0.525
};

inline constexpr InstrumentSpec lyreHarpNylon {
    "Nylon Lyre Harp", nylonMonofilament, 16, detail::lyreHarpNylon, detail::lyreHarpBody,
    Technique::Finger, 0.25, 0.25, 2.0, 0.35, 1.598, 0.475
};

inline constexpr InstrumentSpec gutLyre {
    "Gut Lyre", naturalGut, 6, detail::gutLyreStrings, detail::gutLyreBody,
    Technique::Finger, 0.22, 0.3, 2.5, 0.35, 2.415, 0.265
};

inline constexpr std::array<const InstrumentSpec*, 3> presets {{
    &lyreHarpSteel, &lyreHarpNylon, &gutLyre
}};

} // namespace aculyre
