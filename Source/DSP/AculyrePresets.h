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
// two fits to steel-string recordings).
inline constexpr StringMaterial steelWire { "steel", 7850.0, 2.0e11, 0.0, 1.0 / 5500.0,
                                            0.002334375 * 2.0e11, false };
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
constexpr std::array<StringSpec, maxStrings> lyreHarpSteel {{
    { 55, 0.440, 0.022 * inch }, { 57, 0.425, 0.020 * inch }, { 59, 0.411, 0.019 * inch },
    { 60, 0.396, 0.018 * inch }, { 62, 0.381, 0.017 * inch }, { 64, 0.367, 0.016 * inch },
    { 65, 0.352, 0.015 * inch }, { 67, 0.337, 0.014 * inch }, { 69, 0.323, 0.014 * inch },
    { 71, 0.308, 0.013 * inch }, { 72, 0.293, 0.012 * inch }, { 74, 0.279, 0.012 * inch },
    { 76, 0.264, 0.011 * inch }, { 77, 0.249, 0.011 * inch }, { 79, 0.235, 0.010 * inch },
    { 81, 0.220, 0.010 * inch }
}};

// The same frame strung with nylon at 45 N in the bass falling to 30 N in
// the treble (diameters from T = mu (2 L f)^2 at those tensions).
constexpr std::array<StringSpec, maxStrings> lyreHarpNylon {{
    { 55, 0.440, 1.30e-3 }, { 57, 0.425, 1.18e-3 }, { 59, 0.411, 1.08e-3 },
    { 60, 0.396, 1.05e-3 }, { 62, 0.381, 0.96e-3 }, { 64, 0.367, 0.87e-3 },
    { 65, 0.352, 0.85e-3 }, { 67, 0.337, 0.78e-3 }, { 69, 0.323, 0.72e-3 },
    { 71, 0.308, 0.66e-3 }, { 72, 0.293, 0.64e-3 }, { 74, 0.279, 0.60e-3 },
    { 76, 0.264, 0.55e-3 }, { 77, 0.249, 0.54e-3 }, { 79, 0.235, 0.51e-3 },
    { 81, 0.220, 0.47e-3 }
}};

// The lyre harp's soundbox: a 20 x 30 cm top of 3 mm spruce on a box 5 cm
// deep, one 6 cm soundhole, a 6 g bridge bar a centimetre high a third of
// the way up the top. Assumed dimensions of a typical instrument.
constexpr BodySpec lyreHarpBody {
    0.20, 0.30, 0.003, sitkaSpruce,
    0.20 * 0.30 * 0.05 * 0.85, 0.03,
    0.30, 0.10, 0.90, 0.010, 0.006
};

// A six-string gut lyre after the Anglo-Saxon instruments (the Sutton Hoo
// and Trossingen reconstructions): strings about 50 cm, tuned here
// D3 E3 G3 A3 B3 D4 at about 40 N, over a hollowed maple soundbox with a
// 4 mm top and no soundhole. Dimensions are typical of reconstructions,
// not measured.
constexpr std::array<StringSpec, maxStrings> gutLyreStrings {{
    { 50, 0.50, 1.29e-3 }, { 52, 0.50, 1.15e-3 }, { 55, 0.50, 0.96e-3 },
    { 57, 0.50, 0.86e-3 }, { 59, 0.50, 0.76e-3 }, { 62, 0.50, 0.64e-3 }
}};
constexpr BodySpec gutLyreBody {
    0.36, 0.18, 0.004, hardMaple,
    0.36 * 0.18 * 0.025 * 0.8, 0.0,
    0.15, 0.25, 0.75, 0.012, 0.004
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
    Technique::Finger, 0.25, 0.25, 2.0, 0.35, 0.475, 0.322
};

inline constexpr InstrumentSpec lyreHarpNylon {
    "Nylon Lyre Harp", nylonMonofilament, 16, detail::lyreHarpNylon, detail::lyreHarpBody,
    Technique::Finger, 0.25, 0.25, 2.0, 0.35, 0.515, 0.303
};

inline constexpr InstrumentSpec gutLyre {
    "Gut Lyre", naturalGut, 6, detail::gutLyreStrings, detail::gutLyreBody,
    Technique::Finger, 0.22, 0.3, 2.5, 0.35, 0.895, 0.271
};

inline constexpr std::array<const InstrumentSpec*, 3> presets {{
    &lyreHarpSteel, &lyreHarpNylon, &gutLyre
}};

} // namespace aculyre
