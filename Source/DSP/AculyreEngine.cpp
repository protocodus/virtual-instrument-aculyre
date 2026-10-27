#include "AculyreEngine.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstring>
#include <limits>
#include <vector>

namespace aculyre
{
namespace
{

constexpr double pi = 3.14159265358979323846;
using Complex = std::complex<double>;

// Air at 20 C.
constexpr double airDensity = 1.204;      // kg/m^3
constexpr double airViscosity = 1.81e-5;  // Pa s
constexpr double soundSpeed = 343.0;      // m/s
// The microphones' distance from the soundbox, for the far-field monopole
// pressure rho/(4 pi r) dQ/dt.
constexpr double microphoneDistance = 0.5;

// Loss at the yoke end (tuning pin and crossbar), per round trip. Authored:
// no lyre measurement separates it, and it only matters where the air and
// the material leave a string almost lossless.
constexpr double yokeRoundTripGain = 0.9995;

constexpr int delayCapacity = 4096;  // a string down to 47 Hz at 192 kHz
constexpr int controlPeriod = 16;
constexpr int maxDispersionStages = 8;
// The bridge's ports: each string's point on the bar, then the bar's tipping.
constexpr int maxPorts = maxStrings + 1;

// ---------------------------------------------------------------------------
// String loss laws, copied from Acustra 1.9.0 (Source/DSP/AcustraEngine.cpp),
// where their derivations and references are given in full: the bending loss
// of Valette's and Woodhouse's string damping models, 1/Q_n = eta B n^2 /
// (1 + B n^2), as a unit-DC two-pole section, and a loss angle constant over
// the band (Paté, Le Carrou and Fabre, JASA 135 (2014) 3045) as a unit-DC
// two-pole two-zero section of two relaxations.

double stretchedPartial(double partial, double inharmonicity) noexcept
{
    return partial * std::sqrt((1.0 + inharmonicity * partial * partial)
                               / (1.0 + inharmonicity));
}

double bendingLossRate(double partial, double inharmonicity, double factor,
                       double fundamental) noexcept
{
    const double bending = inharmonicity * partial * partial;
    return pi * fundamental * stretchedPartial(partial, inharmonicity)
         * factor * bending / (1.0 + bending);
}

std::complex<double> insideRoot(std::complex<double> u) noexcept
{
    const std::complex<double> sum = 2.0 - u;
    const std::complex<double> root = std::sqrt(sum * sum - 4.0);
    const std::complex<double> first = 0.5 * (sum + root);
    const std::complex<double> second = 0.5 * (sum - root);
    return std::abs(first) < std::abs(second) ? first : second;
}

// H(z) = (gain + n1 z^-1 + n2 z^-2) / (1 + a1 z^-1 + a2 z^-2), unit gain at DC.
struct LossSectionDesign
{
    double gain { 1.0 };
    double a1 { 0.0 };
    double a2 { 0.0 };
    double n1 { 0.0 };
    double n2 { 0.0 };
};

LossSectionDesign bendingLossSection(double factor, double inharmonicity,
                                     double fundamental, double sampleRate) noexcept
{
    LossSectionDesign section;
    if (!(factor > 0.0) || !(inharmonicity > 0.0) || !(fundamental > 0.0)
        || !(sampleRate > 0.0))
        return section;
    constexpr double collocationLimit = 0.3;
    double below = 1.0;
    double above = collocationLimit * sampleRate / fundamental;
    if (!(above > 1.0))
        return section;
    for (int iteration = 0; iteration < 48; ++iteration)
    {
        const double middle = 0.5 * (below + above);
        if (stretchedPartial(middle, inharmonicity) * fundamental
            < collocationLimit * sampleRate)
            below = middle;
        else
            above = middle;
    }
    const double highest = below;
    const auto partialAt = [&] (double rate)
    {
        double lower = 1.0;
        double upper = highest;
        if (bendingLossRate(upper, inharmonicity, factor, fundamental) <= rate)
            return upper;
        for (int iteration = 0; iteration < 48; ++iteration)
        {
            const double middle = 0.5 * (lower + upper);
            if (bendingLossRate(middle, inharmonicity, factor, fundamental) < rate)
                lower = middle;
            else
                upper = middle;
        }
        return 0.5 * (lower + upper);
    };
    constexpr double decibelsPerNeper = 8.685889638065035;
    const double upperPartial = partialAt(160.0 / decibelsPerNeper);
    const double lowerPartial = std::min(partialAt(20.0 / decibelsPerNeper),
                                         0.5 * upperPartial);
    if (!(lowerPartial >= 1.0) || !(upperPartial > lowerPartial))
        return section;
    double target[2] {};
    double s[2] {};
    const double partials[2] { lowerPartial, upperPartial };
    for (int index = 0; index < 2; ++index)
    {
        const double loss = bendingLossRate(partials[index], inharmonicity,
                                            factor, fundamental) / fundamental;
        target[index] = std::expm1(2.0 * loss);
        const double halfOmega = pi * fundamental
            * stretchedPartial(partials[index], inharmonicity) / sampleRate;
        s[index] = std::sin(halfOmega) * std::sin(halfOmega);
    }
    const double determinant = 64.0 * s[0] * s[1] * (s[1] - s[0]);
    if (!(std::abs(determinant) > 1.0e-300))
        return section;
    double p = 16.0 * (target[0] * s[1] * s[1] - target[1] * s[0] * s[0]) / determinant;
    double q = 4.0 * (s[0] * target[1] - s[1] * target[0]) / determinant;
    if (q < 0.0)
    {
        q = 0.0;
        p = target[1] / (4.0 * s[1]);
    }
    else if (p < 0.0)
    {
        p = 0.0;
        q = target[1] / (16.0 * s[1] * s[1]);
    }
    if (!(p > 0.0 || q > 0.0) || !std::isfinite(p) || !std::isfinite(q))
        return section;
    if (q > 0.0)
    {
        const std::complex<double> discriminant
            = std::sqrt(std::complex<double>(p * p - 4.0 * q, 0.0));
        const std::complex<double> first = insideRoot((-p + discriminant) / (2.0 * q));
        const std::complex<double> second = insideRoot((-p - discriminant) / (2.0 * q));
        section.a1 = -(first + second).real();
        section.a2 = (first * second).real();
    }
    else
    {
        section.a1 = -insideRoot(std::complex<double>(-1.0 / p, 0.0)).real();
        section.a2 = 0.0;
    }
    section.gain = 1.0 + section.a1 + section.a2;
    if (!std::isfinite(section.gain) || !(section.gain > 0.0)
        || !(std::abs(section.a2) < 1.0)
        || !(std::abs(section.a1) < 1.0 + section.a2))
        return {};
    return section;
}

LossSectionDesign constantLossSection(double delta, double inharmonicity,
                                      double fundamental, double sampleRate) noexcept
{
    LossSectionDesign section;
    if (!(delta > 0.0) || !(inharmonicity >= 0.0) || !(fundamental > 0.0)
        || !(sampleRate > 0.0))
        return section;
    constexpr double decibelsPerNeper = 8.685889638065035;
    const double topHz = std::min(0.45 * sampleRate, 20000.0);
    double below = 1.0;
    double above = topHz / fundamental + 1.0;
    if (!(stretchedPartial(2.0, inharmonicity) * fundamental < topHz))
        return section;
    for (int iteration = 0; iteration < 48; ++iteration)
    {
        const double middle = 0.5 * (below + above);
        if (stretchedPartial(middle, inharmonicity) * fundamental < topHz)
            below = middle;
        else
            above = middle;
    }
    const double top = below;
    const auto rateAt = [&] (double partial)
    {
        return pi * fundamental * stretchedPartial(partial, inharmonicity) * delta;
    };
    const auto partialAt = [&] (double rate)
    {
        double lower = 1.0;
        double upper = top;
        if (rateAt(upper) <= rate)
            return upper;
        if (rateAt(lower) >= rate)
            return lower;
        for (int iteration = 0; iteration < 48; ++iteration)
        {
            const double middle = 0.5 * (lower + upper);
            if (rateAt(middle) < rate)
                lower = middle;
            else
                upper = middle;
        }
        return 0.5 * (lower + upper);
    };
    double lowest = std::max(2.0, partialAt(3.0 / decibelsPerNeper));
    if (!(lowest < 0.8 * top))
        lowest = std::max(2.0, std::min(lowest, 0.25 * top));
    double highest = std::min(top, partialAt(300.0 / decibelsPerNeper));
    highest = std::max(highest, std::min(top, 1.5 * lowest));
    if (!(highest > lowest))
        return section;
    constexpr int bandCount = 12;
    constexpr int tailCount = 4;
    const bool tail = top > 1.05 * highest;
    const int count = bandCount + (tail ? tailCount : 0);
    double u[bandCount + tailCount] {};
    double target[bandCount + tailCount] {};
    double weight[bandCount + tailCount] {};
    double loss[bandCount + tailCount] {};
    for (int index = 0; index < count; ++index)
    {
        const bool inBand = index < bandCount;
        const double partial = inBand
            ? lowest * std::pow(highest / lowest,
                                static_cast<double>(index) / (bandCount - 1))
            : highest * std::pow(top / highest,
                                 static_cast<double>(index - bandCount + 1) / tailCount);
        const double half = pi * fundamental
            * stretchedPartial(partial, inharmonicity) / sampleRate;
        u[index] = 4.0 * std::sin(half) * std::sin(half);
        loss[index] = rateAt(partial) / fundamental;
        target[index] = std::expm1(2.0 * loss[index]);
        weight[index] = (inBand ? 1.0 : 0.1) / target[index];
    }
    const double first = u[0];
    const double last = u[bandCount - 1];
    if (!(first > 0.0) || !(last > first))
        return section;
    constexpr double lowerCorners[] { -0.25, 0.10, 0.15, 0.20 };
    constexpr double upperCorners[] { 0.90, 0.95, 1.00, 1.05, 1.50 };
    double bestError = std::numeric_limits<double>::infinity();
    double bestU1 = 0.0, bestU2 = 0.0, bestA = 0.0, bestB = 0.0;
    for (const double lowerCorner : lowerCorners)
        for (const double upperCorner : upperCorners)
        {
            const double u1 = first * std::pow(last / first, lowerCorner);
            const double u2 = first * std::pow(last / first, upperCorner);
            double s11 = 0.0, s12 = 0.0, s22 = 0.0, r1 = 0.0, r2 = 0.0;
            for (int index = 0; index < count; ++index)
            {
                const double denominator = (1.0 + u[index] / u1) * (1.0 + u[index] / u2);
                const double phi1 = weight[index] * u[index] / denominator;
                const double phi2 = phi1 * u[index];
                const double y = weight[index] * target[index];
                s11 += phi1 * phi1;
                s12 += phi1 * phi2;
                s22 += phi2 * phi2;
                r1 += phi1 * y;
                r2 += phi2 * y;
            }
            const double determinant = s11 * s22 - s12 * s12;
            double a = 0.0, b = 0.0;
            if (determinant > 1.0e-12 * s11 * s22)
            {
                a = (r1 * s22 - r2 * s12) / determinant;
                b = (r2 * s11 - r1 * s12) / determinant;
            }
            if (!(a >= 0.0) || !(a + 4.0 * b >= 0.0))
            {
                a = s11 > 0.0 ? r1 / s11 : 0.0;
                b = 0.0;
            }
            if (!(a > 0.0) || !std::isfinite(a) || !std::isfinite(b))
                continue;
            double error = 0.0;
            for (int index = 0; index < bandCount; ++index)
            {
                const double denominator = (1.0 + u[index] / u1) * (1.0 + u[index] / u2);
                const double fitted = 0.5 * std::log1p(
                    (a * u[index] + b * u[index] * u[index]) / denominator);
                error = std::max(error, std::abs(fitted / loss[index] - 1.0));
            }
            if (error < bestError)
            {
                bestError = error;
                bestU1 = u1;
                bestU2 = u2;
                bestA = a;
                bestB = b;
            }
        }
    if (!(bestError < std::numeric_limits<double>::infinity()))
        return section;
    const double zero1 = insideRoot(std::complex<double>(-bestU1, 0.0)).real();
    const double zero2 = insideRoot(std::complex<double>(-bestU2, 0.0)).real();
    const double b1 = -(zero1 + zero2);
    const double b2 = zero1 * zero2;
    const double p = 1.0 / bestU1 + 1.0 / bestU2 + bestA;
    const double q = 1.0 / (bestU1 * bestU2) + bestB;
    double a1 = 0.0, a2 = 0.0;
    if (q != 0.0)
    {
        const std::complex<double> discriminant
            = std::sqrt(std::complex<double>(p * p - 4.0 * q, 0.0));
        const std::complex<double> pole1 = insideRoot((-p + discriminant) / (2.0 * q));
        const std::complex<double> pole2 = insideRoot((-p - discriminant) / (2.0 * q));
        a1 = -(pole1 + pole2).real();
        a2 = (pole1 * pole2).real();
    }
    else
    {
        a1 = -insideRoot(std::complex<double>(-1.0 / p, 0.0)).real();
    }
    const double gain = (1.0 + a1 + a2) / (1.0 + b1 + b2);
    if (!std::isfinite(a1) || !std::isfinite(a2) || !std::isfinite(gain)
        || !(gain > 0.0) || !(std::abs(a2) < 1.0) || !(std::abs(a1) < 1.0 + a2)
        || !(std::abs(b2) < 1.0) || !(std::abs(b1) < 1.0 + b2))
        return section;
    section.gain = gain;
    section.a1 = a1;
    section.a2 = a2;
    section.n1 = gain * b1;
    section.n2 = gain * b2;
    return section;
}

// Second-order Thiran allpass for a delay of `samples` (Acustra 1.9.0).
void thiranCoefficients(double samples, double& a1, double& a2) noexcept
{
    a1 = -2.0 * (samples - 2.0) / (samples + 1.0);
    a2 = (samples - 2.0) * (samples - 1.0) / ((samples + 1.0) * (samples + 2.0));
}
// End of the code copied from Acustra.
// ---------------------------------------------------------------------------

// Phase lag of an allpass z^-N A(1/z)/A(z) is N omega + 2 arg A(e^{j omega});
// A's roots lie inside the circle, so arg A never wraps.
double allpassLag(int order, double c1, double c2, double omega) noexcept
{
    const Complex a = 1.0 + c1 * std::exp(Complex(0.0, -omega))
                    + c2 * std::exp(Complex(0.0, -2.0 * omega));
    return order * omega + 2.0 * std::arg(a);
}

// Dispersion as `stages` identical first-order allpass sections (Rauhala and
// Valimaki's tunable design, here searched rather than read off their
// formula): the stage count and coefficient whose worst partial up to the
// 12th, and below 5 kHz, lands nearest in cents to the stiff-string law, with
// the rest of the loop a pure delay tuned to f0. Acustra fits the same 12
// partials. One section fitted to a single high partial left a steel G3's 2nd
// to 5th partials 30 to 120 cents sharp; no cascade of identical sections
// fits a bass string's 25 partials below 5 kHz (ours stays within 6 cents to
// H12 and falls short above it, a fifth of the stretch by H16).
struct DispersionDesign
{
    int stages { 1 };
    double coefficient { 0.0 };
    double worstCents { 0.0 };
};

DispersionDesign designDispersion(double fundamental, double inharmonicity, double sampleRate) noexcept
{
    const double omega1 = 2.0 * pi * fundamental / sampleRate;
    const int partials = std::clamp(static_cast<int>(std::floor(std::min(5000.0, 0.2 * sampleRate) / fundamental)), 2, 12);
    // A first-order section's phase lag and group delay, (a + z^-1)/(1 + a z^-1).
    const auto lag = [] (double a, double omega)
    {
        return omega - 2.0 * std::atan2(a * std::sin(omega), 1.0 + a * std::cos(omega));
    };
    const auto groupDelay = [] (double a, double omega)
    {
        return (1.0 - a * a) / (1.0 + 2.0 * a * std::cos(omega) + a * a);
    };
    const double infinity = std::numeric_limits<double>::infinity();
    // The worst partial's error in cents with coefficient a = -0.0025 step.
    const auto worstAt = [&] (int stages, int step)
    {
        const double a = -0.0025 * step;
        const double delay = (2.0 * pi - stages * lag(a, omega1)) / omega1;
        // The line, its Thiran section and the loss filters need some.
        if (delay < 8.0)
            return infinity;
        double worst = 0.0;
        for (int n = 2; n <= partials; ++n)
        {
            const double target = omega1 * stretchedPartial(n, inharmonicity);
            double omega = target;
            for (int iteration = 0; iteration < 4; ++iteration)
                omega -= (omega * delay + stages * lag(a, omega) - 2.0 * pi * n)
                       / (delay + stages * groupDelay(a, omega));
            worst = std::max(worst, std::abs(1200.0 * std::log2(omega / target)));
        }
        return worst;
    };
    // Coarse steps of 0.02, then every 0.0025 step around the best: a sixth
    // of the evaluations of the full grid (the setup of three instruments,
    // levers included, fits in about half a second).
    constexpr int lastStep = 380, coarse = 8;
    std::array<DispersionDesign, maxDispersionStages + 1> best {};
    for (int stages = 1; stages <= maxDispersionStages; ++stages)
    {
        DispersionDesign& found = best[static_cast<std::size_t>(stages)];
        found.worstCents = infinity;
        int centre = 0;
        for (int step = 0; step <= lastStep; step += coarse)
        {
            const double worst = worstAt(stages, step);
            if (worst == infinity)
                break;
            if (worst < found.worstCents)
            {
                found = { stages, -0.0025 * step, worst };
                centre = step;
            }
        }
        for (int step = std::max(0, centre - coarse + 1); step < std::min(lastStep + 1, centre + coarse); ++step)
        {
            const double worst = worstAt(stages, step);
            if (worst < found.worstCents)
                found = { stages, -0.0025 * step, worst };
        }
    }
    // The fewest sections within a cent; otherwise the closest fit.
    DispersionDesign chosen = best[1];
    for (int stages = 1; stages <= maxDispersionStages; ++stages)
    {
        const DispersionDesign& design = best[static_cast<std::size_t>(stages)];
        if (design.worstCents <= 1.0)
            return design;
        if (design.worstCents < chosen.worstCents)
            chosen = design;
    }
    return chosen;
}

Complex lossSectionResponse(const LossSectionDesign& s, double omega) noexcept
{
    const Complex z1 = std::exp(Complex(0.0, -omega));
    const Complex z2 = z1 * z1;
    return (s.gain + s.n1 * z1 + s.n2 * z2) / (1.0 + s.a1 * z1 + s.a2 * z2);
}

// y = x - (1 - z^-1)(c0 + c1 z^-1) / A(z) x: the section above with its unit
// gain at DC exact in any precision (Acustra's StringLoop runs its loss
// sections this way).
struct UnitDcSection
{
    double c0 { 0.0 }, c1 { 0.0 }, a1 { 0.0 }, a2 { 0.0 };
    double previousInput { 0.0 }, previousDifference { 0.0 };
    double u1 { 0.0 }, u2 { 0.0 };
    bool active { false };

    void design(const LossSectionDesign& s) noexcept
    {
        active = s.gain != 1.0 || s.a1 != 0.0 || s.a2 != 0.0;
        c0 = 1.0 - s.gain;
        c1 = s.n2 - s.a2;
        a1 = s.a1;
        a2 = s.a2;
    }
    void reset() noexcept { previousInput = previousDifference = u1 = u2 = 0.0; }
    double process(double x) noexcept
    {
        if (!active)
            return x;
        const double difference = x - previousInput;
        previousInput = x;
        const double w = c0 * difference + c1 * previousDifference;
        previousDifference = difference;
        const double u = w - a1 * u1 - a2 * u2;
        u2 = u1;
        u1 = u;
        return x - u;
    }
};

// The frequency ratios, -2 to +2 semitones, at which setup measures each
// loop's phase lag for the pitch bend and the tension glide.
constexpr std::array<double, 5> bendRatios {
    0.8908987181403393, 0.9438743126816935, 1.0, 1.0594630943592953, 1.122462048309373
};

// What sets one polarisation's pitch: its frequency, its filters and the
// line that tunes them. A sharping lever swaps one for another.
struct LoopDesign
{
    double frequency { 0.0 };
    double gain { 1.0 };
    int dispersionStages { 0 };
    double dispersion { 0.0 };
    double dispersionCents { 0.0 };  // the design's worst partial error
    LossSectionDesign constantDesign {};
    LossSectionDesign bendingDesign {};
    // The line delay at rest (whole + fraction) and the whole loop's delay
    // at the fundamental, both in samples.
    double restLineDelay { 68.0 };
    double loopDelay { 70.0 };
    // The phase lag, in cycles, of everything but the line's delay (the
    // filters' dispersion and the bridge's reflection) at bendRatios.
    std::array<double, 5> lagCycles {};

    // Phase lag of everything in the loop but the line's whole samples and
    // the Thiran section, at omega (rad/sample).
    double filterLag(double omega) const noexcept
    {
        double lag = 0.0;
        for (int stage = 0; stage < dispersionStages; ++stage)
            lag += allpassLag(1, dispersion, 0.0, omega);
        lag -= std::arg(lossSectionResponse(constantDesign, omega));
        lag -= std::arg(lossSectionResponse(bendingDesign, omega));
        return lag;
    }
};

// One polarisation of one string: the round trip from the bridge to the yoke
// and back, holding the velocity wave that will next arrive at the bridge.
// The yoke's inversion is folded into what the bridge writes.
struct Loop
{
    std::array<float, delayCapacity> line {};
    // A pluck's first round trip, still to arrive at the bridge, by sample.
    // It enters at the loop's output rather than the line because the loop's
    // filters hold part of a period (the dispersion stages up to a fifth of
    // it on a bass string): a wave squeezed into the line alone would leave
    // a gap and pluck at the wrong point.
    std::array<float, delayCapacity> pending {};
    int write { 0 };
    int whole { 64 };
    double fraction { 4.0 };
    double thiranA1 { 0.0 }, thiranA2 { 0.0 };
    double tx1 { 0.0 }, tx2 { 0.0 }, ty1 { 0.0 }, ty2 { 0.0 };
    int dispersionStages { 0 };
    double dispersion { 0.0 };
    double dispersionCents { 0.0 };  // the design's worst partial error
    std::array<double, maxDispersionStages> dispersionState {};
    UnitDcSection constantLoss {};
    UnitDcSection bendingLoss {};
    LossSectionDesign constantDesign {};
    LossSectionDesign bendingDesign {};
    double gain { 1.0 };
    double damping { 1.0 };
    double dampingTarget { 1.0 };
    double dampingStep { 0.0 };
    // The line delay at rest (whole + fraction) and the whole loop's delay
    // at the fundamental, both in samples.
    double restLineDelay { 68.0 };
    double loopDelay { 70.0 };
    std::array<double, 5> lagCycles {};

    void reset() noexcept
    {
        line.fill(0.0f);
        pending.fill(0.0f);
        tx1 = tx2 = ty1 = ty2 = 0.0;
        dispersionState.fill(0.0);
        constantLoss.reset();
        bendingLoss.reset();
        damping = dampingTarget = 1.0;
        dampingStep = 0.0;
        setLineDelay(restLineDelay);
    }

    // Keeps the Thiran fraction in [1.1, 5.1): a tension glide shortens the
    // delay by a few samples from a rest fraction in [4, 5) before the tap has
    // to move.
    void setLineDelay(double samples) noexcept
    {
        int newWhole = whole;
        double newFraction = samples - newWhole;
        if (newFraction < 1.1 || newFraction >= 5.1)
        {
            // Truncation is floor here: every line is longer than 6 samples.
            newWhole = static_cast<int>(samples - 4.0);
            newFraction = samples - newWhole;
        }
        newWhole = std::clamp(newWhole, 1, delayCapacity - 8);
        whole = newWhole;
        fraction = samples - whole;
        thiranCoefficients(fraction, thiranA1, thiranA2);
    }

    double read() noexcept
    {
        const double x = line[static_cast<std::size_t>((write - whole) & (delayCapacity - 1))];
        double y = thiranA2 * x + thiranA1 * tx1 + tx2 - thiranA1 * ty1 - thiranA2 * ty2;
        tx2 = tx1;
        tx1 = x;
        ty2 = ty1;
        ty1 = y;
        for (int stage = 0; stage < dispersionStages; ++stage)
        {
            const double out = dispersion * y + dispersionState[stage];
            dispersionState[stage] = y - dispersion * out;
            y = out;
        }
        y = constantLoss.process(y);
        y = bendingLoss.process(y);
        if (dampingStep != 0.0)
        {
            damping += dampingStep;
            if ((dampingStep < 0.0 && damping <= dampingTarget)
                || (dampingStep > 0.0 && damping >= dampingTarget))
            {
                damping = dampingTarget;
                dampingStep = 0.0;
            }
        }
        float& arriving = pending[static_cast<std::size_t>(write)];
        y = y * gain * damping + arriving;
        arriving = 0.0f;
        return y;
    }

    void writeSample(double value) noexcept
    {
        line[static_cast<std::size_t>(write)] = static_cast<float>(value);
        write = (write + 1) & (delayCapacity - 1);
    }

    // The loss per round trip a resting hand adds, reached over one round trip.
    void setDamping(double target) noexcept
    {
        dampingTarget = target;
        const double steps = std::max(1.0, loopDelay);
        dampingStep = (target - damping) / steps;
        if (dampingStep == 0.0)
            damping = target;
    }

    // The line delay that keeps the loop resonant at `ratio` times its own
    // frequency, (1 - lag) loopDelay / ratio, the lag interpolated between
    // the semitones setup measured (extrapolated beyond two). Scaling the
    // line alone would leave the dispersion's phase delay at its own
    // frequency: 11 cents flat of a two-semitone bend on A4.
    double lineDelayAt(double ratio) const noexcept
    {
        if (ratio == 1.0)
            return restLineDelay;
        std::size_t k = 0;
        while (k < 3 && ratio > bendRatios[k + 1])
            ++k;
        const double t = (ratio - bendRatios[k]) / (bendRatios[k + 1] - bendRatios[k]);
        const double lag = lagCycles[k] + t * (lagCycles[k + 1] - lagCycles[k]);
        return (1.0 - lag) * loopDelay / ratio;
    }

    // Takes a new pitch while the wave keeps travelling.
    void apply(const LoopDesign& design) noexcept
    {
        // Sections the old design left idle hold stale state.
        for (int stage = dispersionStages; stage < design.dispersionStages; ++stage)
            dispersionState[static_cast<std::size_t>(stage)] = 0.0;
        gain = design.gain;
        dispersionStages = design.dispersionStages;
        dispersion = design.dispersion;
        dispersionCents = design.dispersionCents;
        constantDesign = design.constantDesign;
        bendingDesign = design.bendingDesign;
        constantLoss.design(constantDesign);
        bendingLoss.design(bendingDesign);
        restLineDelay = design.restLineDelay;
        loopDelay = design.loopDelay;
        lagCycles = design.lagCycles;
        setLineDelay(restLineDelay);
    }
};

// One soundbox mode with unit modal mass: velocity per generalized force
// s / (s^2 + (w/Q) s + w^2), bilinear at its own frequency (passive at every
// rate), run in transposed direct form II.
struct Mode
{
    double frequency { 0.0 };
    double q { 1.0 };
    double b0 { 0.0 }, b2 { 0.0 }, a1 { 0.0 }, a2 { 0.0 };
    double radiationLeft { 0.0 };
    double radiationRight { 0.0 };
    double monopole { 0.0 };
    double airShare { 0.0 };
    double s1 { 0.0 }, s2 { 0.0 };
};

// Gauss-Jordan inverse with partial pivoting of the leading n x n block
// (setup only; real for the junction, complex for the tuning).
template <typename T>
void invertPorts(std::array<std::array<T, maxPorts>, maxPorts>& a, int n) noexcept
{
    std::array<int, maxPorts> row {};
    for (int i = 0; i < n; ++i)
        row[static_cast<std::size_t>(i)] = i;
    std::array<std::array<T, maxPorts>, maxPorts> inverse {};
    for (int i = 0; i < n; ++i)
        inverse[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)] = T(1.0);
    for (int column = 0; column < n; ++column)
    {
        int pivot = column;
        for (int i = column + 1; i < n; ++i)
            if (std::abs(a[static_cast<std::size_t>(i)][static_cast<std::size_t>(column)])
                > std::abs(a[static_cast<std::size_t>(pivot)][static_cast<std::size_t>(column)]))
                pivot = i;
        std::swap(a[static_cast<std::size_t>(pivot)], a[static_cast<std::size_t>(column)]);
        std::swap(inverse[static_cast<std::size_t>(pivot)], inverse[static_cast<std::size_t>(column)]);
        const T scale = T(1.0) / a[static_cast<std::size_t>(column)][static_cast<std::size_t>(column)];
        for (int j = 0; j < n; ++j)
        {
            a[static_cast<std::size_t>(column)][static_cast<std::size_t>(j)] *= scale;
            inverse[static_cast<std::size_t>(column)][static_cast<std::size_t>(j)] *= scale;
        }
        for (int i = 0; i < n; ++i)
        {
            if (i == column)
                continue;
            const T factor = a[static_cast<std::size_t>(i)][static_cast<std::size_t>(column)];
            for (int j = 0; j < n; ++j)
            {
                a[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]
                    -= factor * a[static_cast<std::size_t>(column)][static_cast<std::size_t>(j)];
                inverse[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]
                    -= factor * inverse[static_cast<std::size_t>(column)][static_cast<std::size_t>(j)];
            }
        }
    }
    a = inverse;
}

struct Random
{
    std::uint64_t state { 0x9e3779b97f4a7c15ull };
    void seed(std::uint64_t value) noexcept { state = value ? value : 0x9e3779b97f4a7c15ull; }
    std::uint64_t next() noexcept
    {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }
    // Uniform in [-1, 1).
    double bipolar() noexcept
    {
        return static_cast<double>(next() >> 11) * (2.0 / 9007199254740992.0) - 1.0;
    }
};

double normalCdf(double u) noexcept { return 0.5 * std::erfc(-u / std::sqrt(2.0)); }
double normalPdf(double u) noexcept { return std::exp(-0.5 * u * u) / std::sqrt(2.0 * pi); }

// A small room around the microphones: early reflections and an eight-line
// feedback delay network (Householder feedback, one-pole loss per line set
// for a 1.1 s low and 0.45 s high reverberation time). Authored, like any
// room: it is the space the demos are heard in, not part of the instrument.
struct Room
{
    static constexpr int lineCount = 8;
    static constexpr int capacity = 16384;
    static constexpr int earlyCapacity = 4096;
    std::array<std::array<float, capacity>, lineCount> lines {};
    std::array<int, lineCount> lengths {};
    std::array<int, lineCount> positions {};
    std::array<double, lineCount> gains {};
    std::array<double, lineCount> poles {};
    std::array<double, lineCount> filterStates {};
    std::array<float, earlyCapacity> early {};
    int earlyPosition { 0 };
    std::array<int, 6> earlyTapsLeft {}, earlyTapsRight {};
    std::array<double, 6> earlyGainsLeft {}, earlyGainsRight {};
    int predelay { 0 };

    void prepare(double sampleRate) noexcept
    {
        constexpr double milliseconds[lineCount] { 23.1, 29.7, 34.3, 39.9, 44.3, 51.7, 57.1, 63.7 };
        constexpr double lowT60 = 1.1;
        constexpr double highT60 = 0.45;
        constexpr double highHz = 4000.0;
        for (int index = 0; index < lineCount; ++index)
        {
            lengths[static_cast<std::size_t>(index)] = std::min(capacity - 1,
                static_cast<int>(std::lround(milliseconds[index] * 1.0e-3 * sampleRate)));
            const double length = lengths[static_cast<std::size_t>(index)];
            const double low = std::pow(10.0, -3.0 * length / (sampleRate * lowT60));
            const double high = std::pow(10.0, -3.0 * length / (sampleRate * highT60));
            const double ratio = high / low;
            const double omega = 2.0 * pi * highHz / sampleRate;
            double lower = 0.0, upper = 0.999;
            for (int iteration = 0; iteration < 60; ++iteration)
            {
                const double pole = 0.5 * (lower + upper);
                const double magnitude = (1.0 - pole)
                    / std::sqrt(1.0 - 2.0 * pole * std::cos(omega) + pole * pole);
                if (magnitude > ratio)
                    lower = pole;
                else
                    upper = pole;
            }
            gains[static_cast<std::size_t>(index)] = low;
            poles[static_cast<std::size_t>(index)] = 0.5 * (lower + upper);
        }
        constexpr double earlyMs[6] { 7.3, 11.9, 14.6, 19.2, 23.8, 27.1 };
        constexpr double earlyGain[6] { 0.42, -0.33, 0.29, -0.24, 0.2, -0.16 };
        for (int index = 0; index < 6; ++index)
        {
            earlyTapsLeft[static_cast<std::size_t>(index)] = static_cast<int>(earlyMs[index] * 1.0e-3 * sampleRate);
            earlyTapsRight[static_cast<std::size_t>(index)] = static_cast<int>((earlyMs[index] * 1.13 + 1.1) * 1.0e-3 * sampleRate);
            earlyGainsLeft[static_cast<std::size_t>(index)] = earlyGain[index];
            earlyGainsRight[static_cast<std::size_t>(index)] = -earlyGain[5 - index];
        }
        predelay = static_cast<int>(9.0e-3 * sampleRate);
        reset();
    }
    void reset() noexcept
    {
        for (auto& line : lines)
            line.fill(0.0f);
        positions.fill(0);
        filterStates.fill(0.0);
        early.fill(0.0f);
        earlyPosition = 0;
    }
    void process(double input, double& left, double& right) noexcept
    {
        early[static_cast<std::size_t>(earlyPosition)] = static_cast<float>(input);
        const auto earlyAt = [&] (int delay)
        {
            return static_cast<double>(early[static_cast<std::size_t>(
                (earlyPosition - delay) & (earlyCapacity - 1))]);
        };
        double earlyLeft = 0.0, earlyRight = 0.0;
        for (std::size_t tap = 0; tap < 6; ++tap)
        {
            earlyLeft += earlyGainsLeft[tap] * earlyAt(earlyTapsLeft[tap]);
            earlyRight += earlyGainsRight[tap] * earlyAt(earlyTapsRight[tap]);
        }
        const double delayed = earlyAt(predelay);
        earlyPosition = (earlyPosition + 1) & (earlyCapacity - 1);

        std::array<double, lineCount> outputs {};
        double sum = 0.0;
        for (std::size_t index = 0; index < lineCount; ++index)
        {
            const int readPosition = positions[index] - lengths[index];
            const double value = lines[index][static_cast<std::size_t>(
                (readPosition + capacity) % capacity)];
            filterStates[index] = (1.0 - poles[index]) * value + poles[index] * filterStates[index];
            outputs[index] = gains[index] * filterStates[index];
            sum += outputs[index];
        }
        const double householder = 2.0 / lineCount * sum;
        double lateLeft = 0.0, lateRight = 0.0;
        for (std::size_t index = 0; index < lineCount; ++index)
        {
            const double feedback = outputs[index] - householder;
            lines[index][static_cast<std::size_t>(positions[index])]
                = static_cast<float>(feedback + 0.35 * delayed);
            positions[index] = (positions[index] + 1) % capacity;
            lateLeft += (index & 1u) ? -outputs[index] : outputs[index];
            lateRight += (index & 2u) ? -outputs[index] : outputs[index];
        }
        left = earlyLeft + 0.5 * lateLeft;
        right = earlyRight + 0.5 * lateRight;
    }
};

// Clamped-plate beam factors (Warburton, Proc. IMechE 168 (1954) 371): the
// G and H of a clamped-clamped mode m in his approximate frequency formula.
double warburtonG(int m) noexcept { return m == 1 ? 1.506 : m + 0.5; }
double warburtonH(int m) noexcept
{
    if (m == 1)
        return 1.248;
    const double g = warburtonG(m);
    return g * g * (1.0 - 2.0 / (g * pi));
}

// The body's mode damping. Wooden-box statistics, not a lyre measurement:
// Acustra's measured guitar bridge (g21, MeasuredBridgeData.h) holds mode Qs
// of 16-75 (median about 35) below 1.5 kHz and 5-36 above 2 kHz.
double woodQ(double frequency) noexcept
{
    constexpr double anchorsHz[] { 300.0, 1000.0, 4000.0, 10000.0 };
    constexpr double anchorsQ[] { 40.0, 32.0, 18.0, 12.0 };
    if (frequency <= anchorsHz[0])
        return anchorsQ[0];
    for (int index = 1; index < 4; ++index)
        if (frequency <= anchorsHz[index])
        {
            const double t = std::log(frequency / anchorsHz[index - 1])
                           / std::log(anchorsHz[index] / anchorsHz[index - 1]);
            return anchorsQ[index - 1] * std::pow(anchorsQ[index] / anchorsQ[index - 1], t);
        }
    return anchorsQ[3];
}
// The air resonance: g21's air-mode group measures Q 16-21.
constexpr double airQ = 20.0;

// Jacobi eigen-decomposition of a symmetric matrix (setup only).
template <std::size_t N>
void jacobiEigen(std::array<std::array<double, N>, N>& a, int n,
                 std::array<std::array<double, N>, N>& vectors,
                 std::array<double, N>& values) noexcept
{
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            vectors[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = i == j ? 1.0 : 0.0;
    for (int sweep = 0; sweep < 100; ++sweep)
    {
        double offDiagonal = 0.0;
        double diagonal = 0.0;
        for (int i = 0; i < n; ++i)
        {
            diagonal += a[i][i] * a[i][i];
            for (int j = i + 1; j < n; ++j)
                offDiagonal += a[i][j] * a[i][j];
        }
        if (offDiagonal <= 1.0e-24 * diagonal)
            break;
        for (int p = 0; p < n; ++p)
            for (int q = p + 1; q < n; ++q)
            {
                const double apq = a[p][q];
                if (std::abs(apq) < 1.0e-300)
                    continue;
                const double theta = 0.5 * (a[q][q] - a[p][p]) / apq;
                const double t = (theta >= 0.0 ? 1.0 : -1.0)
                               / (std::abs(theta) + std::sqrt(theta * theta + 1.0));
                const double c = 1.0 / std::sqrt(t * t + 1.0);
                const double s = t * c;
                for (int k = 0; k < n; ++k)
                {
                    const double akp = a[k][p];
                    const double akq = a[k][q];
                    a[k][p] = c * akp - s * akq;
                    a[k][q] = s * akp + c * akq;
                }
                for (int k = 0; k < n; ++k)
                {
                    const double apk = a[p][k];
                    const double aqk = a[q][k];
                    a[p][k] = c * apk - s * aqk;
                    a[q][k] = s * apk + c * aqk;
                }
                for (int k = 0; k < n; ++k)
                {
                    const double vkp = vectors[k][p];
                    const double vkq = vectors[k][q];
                    vectors[k][p] = c * vkp - s * vkq;
                    vectors[k][q] = s * vkp + c * vkq;
                }
            }
    }
    for (int i = 0; i < n; ++i)
        values[static_cast<std::size_t>(i)] = a[i][i];
}

// Eigenvalues and eigenvectors of a symmetric n x n matrix (row-major, the
// vectors returned as its columns): Householder reduction to tridiagonal
// form, then the implicit QL algorithm (EISPACK's tred2 and tql2; Golub and
// Van Loan, Matrix Computations, 8.3). Setup only.
void symmetricEigen(double* a, int n, double* values, double* off) noexcept
{
    const auto at = [a, n] (int i, int j) -> double& { return a[i * n + j]; };
    for (int i = n - 1; i > 0; --i)
    {
        const int l = i - 1;
        double h = 0.0;
        if (l > 0)
        {
            double scale = 0.0;
            for (int k = 0; k <= l; ++k)
                scale += std::abs(at(i, k));
            if (scale == 0.0)
                off[i] = at(i, l);
            else
            {
                for (int k = 0; k <= l; ++k)
                {
                    at(i, k) /= scale;
                    h += at(i, k) * at(i, k);
                }
                double f = at(i, l);
                double g = f >= 0.0 ? -std::sqrt(h) : std::sqrt(h);
                off[i] = scale * g;
                h -= f * g;
                at(i, l) = f - g;
                f = 0.0;
                for (int j = 0; j <= l; ++j)
                {
                    at(j, i) = at(i, j) / h;
                    g = 0.0;
                    for (int k = 0; k <= j; ++k)
                        g += at(j, k) * at(i, k);
                    for (int k = j + 1; k <= l; ++k)
                        g += at(k, j) * at(i, k);
                    off[j] = g / h;
                    f += off[j] * at(i, j);
                }
                const double hh = f / (h + h);
                for (int j = 0; j <= l; ++j)
                {
                    f = at(i, j);
                    g = off[j] - hh * f;
                    off[j] = g;
                    for (int k = 0; k <= j; ++k)
                        at(j, k) -= f * off[k] + g * at(i, k);
                }
            }
        }
        else
            off[i] = at(i, l);
        values[i] = h;
    }
    values[0] = 0.0;
    off[0] = 0.0;
    // Accumulate the transformations.
    for (int i = 0; i < n; ++i)
    {
        if (values[i] != 0.0)
            for (int j = 0; j < i; ++j)
            {
                double g = 0.0;
                for (int k = 0; k < i; ++k)
                    g += at(i, k) * at(k, j);
                for (int k = 0; k < i; ++k)
                    at(k, j) -= g * at(k, i);
            }
        values[i] = at(i, i);
        at(i, i) = 1.0;
        for (int j = 0; j < i; ++j)
            at(j, i) = at(i, j) = 0.0;
    }

    for (int i = 1; i < n; ++i)
        off[i - 1] = off[i];
    off[n - 1] = 0.0;
    for (int l = 0; l < n; ++l)
    {
        int iterations = 0;
        int m = l;
        do
        {
            for (m = l; m < n - 1; ++m)
            {
                const double scale = std::abs(values[m]) + std::abs(values[m + 1]);
                if (std::abs(off[m]) <= std::numeric_limits<double>::epsilon() * scale)
                    break;
            }
            if (m == l)
                break;
            if (iterations++ == 60)
                break;
            double g = (values[l + 1] - values[l]) / (2.0 * off[l]);
            double r = std::hypot(g, 1.0);
            g = values[m] - values[l] + off[l] / (g + (g >= 0.0 ? r : -r));
            double s = 1.0, c = 1.0, p = 0.0;
            int i = m - 1;
            for (; i >= l; --i)
            {
                double f = s * off[i];
                const double e = c * off[i];
                r = std::hypot(f, g);
                off[i + 1] = r;
                if (r == 0.0)
                {
                    values[i + 1] -= p;
                    off[m] = 0.0;
                    break;
                }
                s = f / r;
                c = g / r;
                g = values[i + 1] - p;
                r = (values[i] - g) * s + 2.0 * c * e;
                p = s * r;
                values[i + 1] = g + p;
                g = c * r - e;
                for (int k = 0; k < n; ++k)
                {
                    f = at(k, i + 1);
                    at(k, i + 1) = s * at(k, i) + c * f;
                    at(k, i) = c * at(k, i) - s * f;
                }
            }
            if (r == 0.0 && i >= l)
                continue;
            values[l] -= p;
            off[l] = g;
            off[m] = 0.0;
        } while (m != l);
    }
}

// The modes of K x = w^2 M x, K symmetric and M positive definite (both
// row-major n x n): M = L L^T reduces it to the standard problem of
// L^-1 K L^-T. On return `k` holds the shapes as columns, normalized to unit
// modal mass, and `values` the w^2; `m` holds L. Setup only.
void generalizedEigen(std::vector<double>& k, std::vector<double>& m, int n,
                      std::vector<double>& values, std::vector<double>& off)
{
    double* const kk = k.data();
    double* const lower = m.data();
    for (int j = 0; j < n; ++j)
    {
        double d = lower[j * n + j];
        for (int p = 0; p < j; ++p)
            d -= lower[j * n + p] * lower[j * n + p];
        d = std::sqrt(std::max(d, 1.0e-300));
        lower[j * n + j] = d;
        for (int i = j + 1; i < n; ++i)
        {
            double v = lower[i * n + j];
            for (int p = 0; p < j; ++p)
                v -= lower[i * n + p] * lower[j * n + p];
            lower[i * n + j] = v / d;
        }
    }
    // L^-1 applied to every column, in place.
    const auto forward = [&] ()
    {
        for (int i = 0; i < n; ++i)
        {
            double* const row = kk + i * n;
            for (int p = 0; p < i; ++p)
            {
                const double factor = lower[i * n + p];
                const double* const above = kk + p * n;
                for (int c = 0; c < n; ++c)
                    row[c] -= factor * above[c];
            }
            const double inverse = 1.0 / lower[i * n + i];
            for (int c = 0; c < n; ++c)
                row[c] *= inverse;
        }
    };
    forward();
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            std::swap(kk[i * n + j], kk[j * n + i]);
    forward();
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            kk[i * n + j] = kk[j * n + i] = 0.5 * (kk[i * n + j] + kk[j * n + i]);
    symmetricEigen(kk, n, values.data(), off.data());
    // Back to the physical coordinates: x = L^-T q, in place.
    for (int i = n - 1; i >= 0; --i)
    {
        double* const row = kk + i * n;
        for (int p = i + 1; p < n; ++p)
        {
            const double factor = lower[p * n + i];
            const double* const below = kk + p * n;
            for (int c = 0; c < n; ++c)
                row[c] -= factor * below[c];
        }
        const double inverse = 1.0 / lower[i * n + i];
        for (int c = 0; c < n; ++c)
            row[c] *= inverse;
    }
}

// The integral of sin(p y) sin(q y) over [y0, y1].
double sineOverlap(double p, double q, double y0, double y1) noexcept
{
    const auto cosineIntegral = [y0, y1] (double k)
    {
        return k == 0.0 ? y1 - y0 : (std::sin(k * y1) - std::sin(k * y0)) / k;
    };
    return 0.5 * (cosineIntegral(p - q) - cosineIntegral(p + q));
}

} // namespace

struct AculyreEngine::State
{
    InstrumentSpec instrument {};

    // A string's pitch with its lever down or up: the lever shortens the
    // speaking length a semitone's worth at the same tension.
    struct PitchDesign
    {
        std::array<LoopDesign, 2> planes {};  // normal, parallel
        double length { 0.0 };
        double parallelLength { 0.0 };
        double inharmonicity { 0.0 };
        double intrinsicT60 { 0.0 };
        double glidePerEnergy { 0.0 };
        double energySmoothing { 0.0 };
    };

    struct StringVoice
    {
        Loop normal {};
        Loop parallel {};
        std::array<PitchDesign, 2> pitch {};  // lever down, lever up
        // The sounding pitch (pitch[raised ? 1 : 0]).
        double frequency { 0.0 };
        double parallelFrequency { 0.0 };
        double length { 0.0 };
        double parallelLength { 0.0 };
        double inharmonicity { 0.0 };
        double intrinsicT60 { 0.0 };
        // Delta f / f per unit of the loop's mean squared wave velocity.
        double glidePerEnergy { 0.0 };
        double energySmoothing { 0.0 };
        double tension { 0.0 };
        double impedance { 0.0 };
        double linearMass { 0.0 };
        double energy { 0.0 };
        bool keyDown { false };
        bool sustained { false };
        bool plucked { false };   // played, and still sounding from that pluck
        bool raised { false };    // the sharping lever is up
        bool dampingDirty { true };
        int midiNote { -1 };      // its own note, lever down
        int ownerNote { -1 };     // the note that last plucked it
    };
    std::array<StringVoice, maxStrings> strings {};

    std::array<Mode, maxModes> modes {};
    int modeCount { 0 };

    // Each mode's shape at the bridge ports (each string's point on the bar,
    // then the bar's tipping), both ways round for the per-sample loops.
    std::array<double, maxModes * maxPorts> modeShape {};  // [mode][port]
    std::array<double, maxPorts * maxModes> portShape {};  // [port][mode]
    std::array<double, maxModes> modalForce {};
    int portCount { 0 };

    // The bridge junction (process() derives it): the strings' impedance at
    // each port Zg (diagonal), the modes' immediate mobility Y0,
    // solve = (I + Y0 Zg)^-1 and driveGain = solve Y0.
    std::array<double, maxPorts> portImpedance {};
    std::array<std::array<double, maxPorts>, maxPorts> solve {};
    std::array<std::array<double, maxPorts>, maxPorts> driveGain {};

    double radiationScale { 0.0 };
    double previousLeft { 0.0 };
    double previousRight { 0.0 };
    Room room {};
    double outputGain { 1.0 };
    bool sustain { false };
    Random random {};
    int controlCountdown { 0 };
    std::array<double, delayCapacity> pluckWave {};

    // PlayingControls as the signal path uses them. The hands' damping as
    // decay rates (1/s, they add); the output gains glide to their targets
    // over about 10 ms (per sample), the bend at the control rate.
    double releaseRate { 0.0 };
    double sympathyRate { 0.0 };
    double muteRate { 0.0 };
    double bendTarget { 1.0 };
    double bend { 1.0 };
    std::array<double, 4> outputTarget {};  // room send, direct and crossed gain, volume
    std::array<double, 4> output {};
    double outputSmoothing { 0.0 };
    double bendSmoothing { 0.0 };
};

AculyreEngine::AculyreEngine() : state_(std::make_unique<State>()) {}
AculyreEngine::~AculyreEngine() = default;

void AculyreEngine::prepare(double sampleRate)
{
    sampleRate_ = std::isfinite(sampleRate) && sampleRate >= 22050.0 && sampleRate <= 192000.0
        ? sampleRate : 48000.0;
    state_->room.prepare(sampleRate_);
    state_->outputSmoothing = 1.0 - std::exp(-1.0 / (0.01 * sampleRate_));
    state_->bendSmoothing = 1.0 - std::exp(-controlPeriod / (0.01 * sampleRate_));
    if (stringCount_ > 0)
        setInstrument(state_->instrument);
}

namespace
{

// Plate mode (m along the strings, n across) of a simply supported shape,
// the bridge bar's basis; frequencies use Warburton's clamped factors.
struct PlateMode
{
    int m { 1 };
    int n { 1 };
    double frequency { 0.0 };
    double mass { 0.0 };
    double volume { 0.0 };
    double multipoleLeft { 0.0 };
    double multipoleRight { 0.0 };
};

// A mode of the soundbox with unit modal mass, as setup builds it: its shape
// at the bridge ports, its net volume and its radiation besides.
struct BodyMode
{
    double frequency { 0.0 };
    double airShare { 0.0 };
    double volume { 0.0 };
    double multipoleLeft { 0.0 };
    double multipoleRight { 0.0 };
    std::array<double, maxPorts> shape {};
};

} // namespace

void AculyreEngine::setInstrument(const InstrumentSpec& instrument)
{
    State& s = *state_;
    s.instrument = instrument;
    const double fs = sampleRate_;
    stringCount_ = std::clamp(instrument.stringCount, 1, maxStrings);
    s.random.seed(0x5eed1ac0ffeeull);

    // ---- The soundbox --------------------------------------------------
    const BodySpec& body = instrument.body;
    const double a = body.topLength;
    const double b = body.topWidth;
    const double h = body.topThickness;
    const WoodSpec& wood = body.wood;
    const double surfaceMass = wood.density * h;
    const double nuAcrossAlong = wood.poisson * wood.eAcross / wood.eAlong;
    const double bendingCommon = h * h * h / (12.0 * (1.0 - wood.poisson * nuAcrossAlong));
    const double dAlong = wood.eAlong * bendingCommon;
    const double dAcross = wood.eAcross * bendingCommon;
    const double dTwist = wood.shear * h * h * h / 12.0;
    const double dCross = wood.poisson * dAcross + 2.0 * dTwist;
    const double plateMass = surfaceMass * a * b;
    const double coincidenceHz = soundSpeed * soundSpeed / (2.0 * pi)
                               * std::sqrt(surfaceMass / std::sqrt(dAlong * dAcross));
    const double topFrequencyLimit = std::min(20000.0, 0.45 * fs);

    const double bridgeX = body.bridgeAlong * a;
    const double y0 = body.bridgeFrom * b;
    const double y1 = body.bridgeTo * b;

    std::array<PlateMode, maxModes> plate {};
    int plateCount = 0;
    Random modeRandom;
    modeRandom.seed(0xa1c0ffee12345ull);
    for (int m = 1; m <= 60; ++m)
        for (int n = 1; n <= 60; ++n)
        {
            const double gm = warburtonG(m), gn = warburtonG(n);
            const double stiffness = dAlong * std::pow(gm / a, 4.0)
                + 2.0 * dCross * warburtonH(m) * warburtonH(n) / (a * a * b * b)
                + dAcross * std::pow(gn / b, 4.0);
            double frequency = 0.5 * pi * std::sqrt(stiffness / surfaceMass);
            // A real soundboard is no perfect rectangle: modes past the first
            // few scatter about the ideal plate's (a seeded, fixed draw).
            const double scatter = modeRandom.bipolar();
            if (m + n > 3)
                frequency *= std::exp(0.05 * scatter);
            if (frequency >= topFrequencyLimit)
                continue;
            if (plateCount >= maxModes - 1)
            {
                // Keep the lowest modes.
                int highest = 0;
                for (int index = 1; index < plateCount; ++index)
                    if (plate[static_cast<std::size_t>(index)].frequency
                        > plate[static_cast<std::size_t>(highest)].frequency)
                        highest = index;
                if (plate[static_cast<std::size_t>(highest)].frequency <= frequency)
                    continue;
                plate[static_cast<std::size_t>(highest)] = {};
                plate[static_cast<std::size_t>(highest)].m = m;
                plate[static_cast<std::size_t>(highest)].n = n;
                plate[static_cast<std::size_t>(highest)].frequency = frequency;
                continue;
            }
            PlateMode mode;
            mode.m = m;
            mode.n = n;
            mode.frequency = frequency;
            plate[static_cast<std::size_t>(plateCount++)] = mode;
        }
    std::sort(plate.begin(), plate.begin() + plateCount,
              [] (const PlateMode& x, const PlateMode& y) { return x.frequency < y.frequency; });

    // What each plate shape radiates besides its net volume: edge and corner
    // radiation below coincidence, all of it above (radiation efficiency
    // sigma; an equivalent volume velocity c sqrt(4 pi S sigma <v^2>) /
    // omega). Its sign at each microphone is a seeded draw.
    for (int index = 0; index < plateCount; ++index)
    {
        PlateMode& mode = plate[static_cast<std::size_t>(index)];
        mode.mass = 0.25 * plateMass;
        mode.volume = (mode.m % 2 == 1 && mode.n % 2 == 1)
            ? 4.0 * a * b / (mode.m * mode.n * pi * pi) : 0.0;
        const double omega = 2.0 * pi * mode.frequency;
        const double ratio = mode.frequency / coincidenceHz;
        const double sigma = ratio >= 1.0 ? 1.0 : std::min(1.0, 0.28 * std::sqrt(ratio));
        const double multipole = soundSpeed / omega
            * std::sqrt(4.0 * pi * a * b * sigma) * 0.5;
        mode.multipoleLeft = multipole * (modeRandom.bipolar() < 0.0 ? -1.0 : 1.0)
                           * (0.7 + 0.3 * std::abs(modeRandom.bipolar()));
        mode.multipoleRight = multipole * (modeRandom.bipolar() < 0.0 ? -1.0 : 1.0)
                            * (0.7 + 0.3 * std::abs(modeRandom.bipolar()));
    }

    // ---- The bridge bar on the top ----------------------------------------
    // The bar is glued across the top under every string, and it is no rigid
    // body: the lyre harp's 24 cm, 6 g bar bends freely at about 785 Hz, and
    // above a kilohertz the top's modes change sign along it. A bar that only
    // heaved and rocked averaged them away, leaving 0.002-0.007 m/s/N of
    // mobility at 1.6-5 kHz against 0.03-0.05 at one point of the same top.
    // Here it is an Euler-Bernoulli beam that bends with the top along its
    // line: its mass, its tipping inertia about its foot and its bending
    // stiffness join the plate's in the basis of the plate's shapes
    // (Rayleigh-Ritz), and each string drives the combined modes at its own
    // point. The parallel polarisation tips the bar about its foot, as one
    // port for all strings (the mean slope under the bar). Its section is
    // the bar's height by the thickness its mass gives in the bridge wood;
    // the top under it adds no composite stiffness.
    const int basisCount = plateCount;
    const int ports = stringCount_ + 1;
    const int swayPort = stringCount_;
    const double barLength = y1 - y0;
    const double barHeight = body.bridgeHeight;
    const double barLinearMass = body.bridgeMass / barLength;
    const double barThickness = barLinearMass / (body.bridgeWood.density * barHeight);
    const double barBending = body.bridgeWood.eAlong * barThickness * barHeight * barHeight * barHeight / 12.0;
    const double barTipping = barLinearMass * barHeight * barHeight / 3.0;
    // Strings sit evenly along the bar, bass at the u = -1 end.
    const auto stringSpot = [&] (int index)
    {
        const double u = stringCount_ > 1 ? -0.9 + 1.8 * index / (stringCount_ - 1) : 0.0;
        return y0 + 0.5 * (u + 1.0) * barLength;
    };

    // Each plate shape at the bar: its value at each string's point, its
    // slope across the bar, and the basis rows the projections below read
    // (the string points, the sway, the net volume and the radiation).
    const int rows = ports + 3;
    std::vector<double> basis(static_cast<std::size_t>(rows * basisCount));
    std::vector<double> alongBar(static_cast<std::size_t>(basisCount));
    std::vector<double> slopeAtBar(static_cast<std::size_t>(basisCount));
    std::vector<double> across(static_cast<std::size_t>(basisCount));
    for (int index = 0; index < basisCount; ++index)
    {
        const PlateMode& mode = plate[static_cast<std::size_t>(index)];
        const double kx = mode.m * pi / a;
        const double ky = mode.n * pi / b;
        const double sine = std::sin(kx * bridgeX);
        const double slope = kx * std::cos(kx * bridgeX);
        constexpr int points = 64;
        double meanSine = 0.0;
        for (int point = 0; point < points; ++point)
            meanSine += std::sin(ky * (y0 + (point + 0.5) / points * barLength)) / points;
        alongBar[static_cast<std::size_t>(index)] = sine;
        slopeAtBar[static_cast<std::size_t>(index)] = slope;
        across[static_cast<std::size_t>(index)] = ky;
        for (int string = 0; string < stringCount_; ++string)
            basis[static_cast<std::size_t>(string * basisCount + index)] = sine * std::sin(ky * stringSpot(string));
        basis[static_cast<std::size_t>(swayPort * basisCount + index)] = barHeight * slope * meanSine;
        basis[static_cast<std::size_t>((ports + 0) * basisCount + index)] = mode.volume;
        basis[static_cast<std::size_t>((ports + 1) * basisCount + index)] = mode.multipoleLeft;
        basis[static_cast<std::size_t>((ports + 2) * basisCount + index)] = mode.multipoleRight;
    }

    std::vector<double> stiffness(static_cast<std::size_t>(basisCount * basisCount));
    std::vector<double> inertia(static_cast<std::size_t>(basisCount * basisCount));
    for (int i = 0; i < basisCount; ++i)
        for (int j = 0; j <= i; ++j)
        {
            const auto si = static_cast<std::size_t>(i), sj = static_cast<std::size_t>(j);
            const double overlap = sineOverlap(across[si], across[sj], y0, y1);
            double k = barBending * alongBar[si] * across[si] * across[si]
                     * alongBar[sj] * across[sj] * across[sj] * overlap;
            double m = (barLinearMass * alongBar[si] * alongBar[sj]
                        + barTipping * slopeAtBar[si] * slopeAtBar[sj]) * overlap;
            if (i == j)
            {
                const PlateMode& mode = plate[si];
                const double omega = 2.0 * pi * mode.frequency;
                k += mode.mass * omega * omega;
                m += mode.mass;
            }
            stiffness[static_cast<std::size_t>(i * basisCount + j)] = k;
            stiffness[static_cast<std::size_t>(j * basisCount + i)] = k;
            inertia[static_cast<std::size_t>(i * basisCount + j)] = m;
            inertia[static_cast<std::size_t>(j * basisCount + i)] = m;
        }
    std::vector<double> squares(static_cast<std::size_t>(basisCount));
    std::vector<double> scratch(static_cast<std::size_t>(basisCount));
    if (basisCount > 0)
        generalizedEigen(stiffness, inertia, basisCount, squares, scratch);

    // The combined modes' shapes at the ports, net volumes and radiation:
    // the basis rows times the mode shapes.
    std::vector<double> projected(static_cast<std::size_t>(rows * basisCount));
    for (int row = 0; row < rows; ++row)
    {
        double* const out = projected.data() + row * basisCount;
        for (int i = 0; i < basisCount; ++i)
        {
            const double weight = basis[static_cast<std::size_t>(row * basisCount + i)];
            if (weight == 0.0)
                continue;
            const double* const shape = stiffness.data() + i * basisCount;
            for (int j = 0; j < basisCount; ++j)
                out[j] += weight * shape[j];
        }
    }
    std::vector<BodyMode> combined(static_cast<std::size_t>(basisCount));
    for (int j = 0; j < basisCount; ++j)
    {
        BodyMode& mode = combined[static_cast<std::size_t>(j)];
        mode.frequency = std::sqrt(std::max(0.0, squares[static_cast<std::size_t>(j)])) / (2.0 * pi);
        for (int port = 0; port < ports; ++port)
            mode.shape[static_cast<std::size_t>(port)] = projected[static_cast<std::size_t>(port * basisCount + j)];
        mode.volume = projected[static_cast<std::size_t>((ports + 0) * basisCount + j)];
        mode.multipoleLeft = projected[static_cast<std::size_t>((ports + 1) * basisCount + j)];
        mode.multipoleRight = projected[static_cast<std::size_t>((ports + 2) * basisCount + j)];
    }
    std::sort(combined.begin(), combined.end(),
              [] (const BodyMode& x, const BodyMode& y) { return x.frequency < y.frequency; });

    // The air in the soundhole couples to every mode with a net volume
    // through the cavity's stiffness rho c^2 / V (a Helmholtz resonator on a
    // flexible wall: Christensen and Vistisen, JASA 68(3), 1980, generalised
    // to many modes). Solved as one symmetric eigenproblem.
    constexpr std::size_t coupledCapacity = 48;
    std::array<int, coupledCapacity> coupledIndex {};
    int coupledCount = 0;
    const bool hasHole = body.holeRadius > 0.0;
    for (int index = 0; index < basisCount && coupledCount < static_cast<int>(coupledCapacity) - 1; ++index)
        if (combined[static_cast<std::size_t>(index)].volume != 0.0
            && combined[static_cast<std::size_t>(index)].frequency < 4000.0)
            coupledIndex[static_cast<std::size_t>(coupledCount++)] = index;
    const double holeArea = hasHole ? pi * body.holeRadius * body.holeRadius : 0.0;
    // End corrections of a hole in a thin wall, flanged both sides.
    const double neckLength = h + 1.7 * body.holeRadius;
    const double plugMass = airDensity * holeArea * neckLength;
    const double cavityStiffness = airDensity * soundSpeed * soundSpeed / body.cavityVolume;
    const int systemSize = coupledCount + (hasHole ? 1 : 0);

    // The modes have unit mass; the plug its own.
    std::array<std::array<double, coupledCapacity>, coupledCapacity> system {};
    std::array<double, coupledCapacity> masses {};
    std::array<double, coupledCapacity> volumes {};
    for (int i = 0; i < coupledCount; ++i)
    {
        masses[static_cast<std::size_t>(i)] = 1.0;
        volumes[static_cast<std::size_t>(i)] = combined[static_cast<std::size_t>(coupledIndex[static_cast<std::size_t>(i)])].volume;
    }
    if (hasHole)
    {
        masses[static_cast<std::size_t>(coupledCount)] = plugMass;
        volumes[static_cast<std::size_t>(coupledCount)] = holeArea;
    }
    for (int i = 0; i < systemSize; ++i)
        for (int j = 0; j < systemSize; ++j)
        {
            const auto si = static_cast<std::size_t>(i), sj = static_cast<std::size_t>(j);
            double k = cavityStiffness * volumes[si] * volumes[sj];
            if (i == j && i < coupledCount)
            {
                const double omega = 2.0 * pi * combined[static_cast<std::size_t>(coupledIndex[si])].frequency;
                k += masses[si] * omega * omega;
            }
            system[si][sj] = k / std::sqrt(masses[si] * masses[sj]);
        }
    std::array<std::array<double, coupledCapacity>, coupledCapacity> vectors {};
    std::array<double, coupledCapacity> values {};
    if (systemSize > 0)
        jacobiEigen(system, systemSize, vectors, values);

    std::vector<BodyMode> modes;
    modes.reserve(static_cast<std::size_t>(basisCount + 1));
    const auto addMode = [&] (const BodyMode& mode)
    {
        if (mode.frequency > 20.0 && mode.frequency < topFrequencyLimit
            && static_cast<int>(modes.size()) < maxModes)
            modes.push_back(mode);
    };
    // The coupled modes, mass-normalized: psi = M^-1/2 chi.
    for (int j = 0; j < systemSize; ++j)
    {
        const auto sj = static_cast<std::size_t>(j);
        BodyMode mode;
        mode.frequency = std::sqrt(std::max(0.0, values[sj])) / (2.0 * pi);
        for (int i = 0; i < coupledCount; ++i)
        {
            const auto si = static_cast<std::size_t>(i);
            const BodyMode& part = combined[static_cast<std::size_t>(coupledIndex[si])];
            const double psi = vectors[si][sj];
            for (int port = 0; port < ports; ++port)
                mode.shape[static_cast<std::size_t>(port)] += psi * part.shape[static_cast<std::size_t>(port)];
            mode.volume += psi * part.volume;
            mode.multipoleLeft += psi * part.multipoleLeft;
            mode.multipoleRight += psi * part.multipoleRight;
        }
        if (hasHole)
        {
            const double chi = vectors[static_cast<std::size_t>(coupledCount)][sj];
            mode.airShare = chi * chi;
            mode.volume += chi / std::sqrt(plugMass) * holeArea;
        }
        addMode(mode);
    }
    for (int index = 0; index < basisCount; ++index)
    {
        bool coupled = false;
        for (int i = 0; i < coupledCount; ++i)
            if (coupledIndex[static_cast<std::size_t>(i)] == index)
                coupled = true;
        if (!coupled)
            addMode(combined[static_cast<std::size_t>(index)]);
    }
    std::sort(modes.begin(), modes.end(),
              [] (const BodyMode& x, const BodyMode& y) { return x.frequency < y.frequency; });

    s.modeCount = static_cast<int>(modes.size());
    s.portCount = ports;
    s.modeShape = {};
    s.portShape = {};
    for (int index = 0; index < s.modeCount; ++index)
    {
        const BodyMode& built = modes[static_cast<std::size_t>(index)];
        Mode mode;
        mode.frequency = built.frequency;
        const double qWood = woodQ(built.frequency);
        mode.q = 1.0 / (built.airShare / airQ + (1.0 - built.airShare) / qWood);
        mode.monopole = built.volume;
        mode.airShare = built.airShare;
        mode.radiationLeft = built.volume + built.multipoleLeft;
        mode.radiationRight = built.volume + built.multipoleRight;
        const double omega = 2.0 * pi * built.frequency;
        const double k = omega / std::tan(omega / (2.0 * fs));
        const double damping = omega / mode.q;
        const double d0 = k * k + damping * k + omega * omega;
        mode.b0 = k / d0;
        mode.b2 = -k / d0;
        mode.a1 = 2.0 * (omega * omega - k * k) / d0;
        mode.a2 = (k * k - damping * k + omega * omega) / d0;
        s.modes[static_cast<std::size_t>(index)] = mode;
        for (int port = 0; port < ports; ++port)
        {
            const double value = built.shape[static_cast<std::size_t>(port)];
            s.modeShape[static_cast<std::size_t>(index * maxPorts + port)] = value;
            s.portShape[static_cast<std::size_t>(port * maxModes + index)] = value;
        }
    }
    s.radiationScale = airDensity / (4.0 * pi * microphoneDistance) * fs;

    // ---- The strings ----------------------------------------------------
    const StringMaterial& material = instrument.material;
    // A sharping lever shortens the speaking length to raise the note a
    // semitone; the tension stays, so the string's impedance does too.
    const double leverRatio = std::pow(2.0, -1.0 / 12.0);
    for (int index = 0; index < stringCount_; ++index)
    {
        State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
        voice = {};
        const StringSpec& spec = instrument.strings[static_cast<std::size_t>(index)];
        voice.midiNote = spec.midiNote;
        const double area = 0.25 * pi * spec.diameter * spec.diameter;
        voice.linearMass = material.density * area;
        const double waveSpeed = 2.0 * spec.length * 440.0 * std::pow(2.0, (spec.midiNote - 69) / 12.0);
        voice.tension = voice.linearMass * waveSpeed * waveSpeed;
        voice.impedance = std::sqrt(voice.tension * voice.linearMass);
        const double secondMoment = pi * std::pow(spec.diameter, 4.0) / 64.0;

        for (int raised = 0; raised < 2; ++raised)
        {
            State::PitchDesign& pitch = voice.pitch[static_cast<std::size_t>(raised)];
            pitch.length = raised ? spec.length * leverRatio : spec.length;
            const double frequency = waveSpeed / (2.0 * pitch.length);
            pitch.inharmonicity = pi * pi * material.youngModulus * secondMoment
                                / (voice.tension * pitch.length * pitch.length);
            // The plane parallel to the soundboard is longer by about one
            // string diameter (Woodhouse, Acta Acustica 90 (2004) 945, Sec.
            // 4.3; Acustra ships the same end correction).
            pitch.parallelLength = pitch.length + spec.diameter;
            pitch.glidePerEnergy = material.youngModulus * area
                                 / (2.0 * voice.tension * waveSpeed * waveSpeed);
            // Air damping (Valette, in Mechanics of Musical Instruments,
            // 1995): R = 2 pi eta + 2 pi d sqrt(pi eta rho f), decay rate
            // R / (2 mu).
            const double airResistance = 2.0 * pi * airViscosity
                + 2.0 * pi * spec.diameter * std::sqrt(pi * airViscosity * airDensity * frequency);
            const double airDecay = airResistance / (2.0 * voice.linearMass);
            const double intrinsicDecay = airDecay
                + pi * frequency * material.constantLoss
                - std::log(yokeRoundTripGain) * frequency;
            pitch.intrinsicT60 = 3.0 * std::log(10.0) / intrinsicDecay;
            pitch.energySmoothing = std::exp(-frequency / fs);

            for (int plane = 0; plane < 2; ++plane)
            {
                LoopDesign& loop = pitch.planes[static_cast<std::size_t>(plane)];
                const double f0 = plane == 0 ? frequency : frequency * pitch.length / pitch.parallelLength;
                loop.frequency = f0;
                loop.gain = std::exp(-airDecay / f0) * yokeRoundTripGain;
                loop.constantDesign = constantLossSection(material.constantLoss,
                                                          pitch.inharmonicity, f0, fs);
                loop.bendingDesign = bendingLossSection(material.bendingLoss,
                                                        pitch.inharmonicity, f0, fs);
                // Dispersion (designDispersion), the line then solved so the
                // fundamental is exact once the junction exists and the
                // bridge's reflection phase is known (tuning below).
                const DispersionDesign design = designDispersion(f0, pitch.inharmonicity, fs);
                loop.dispersionStages = design.stages;
                loop.dispersion = design.coefficient;
                loop.dispersionCents = design.worstCents;
                loop.loopDelay = fs / f0;
            }
        }
    }

    // ---- The junction ---------------------------------------------------
    std::array<std::array<double, maxPorts>, maxPorts> immediate {};
    for (int index = 0; index < s.modeCount; ++index)
    {
        const double b0 = s.modes[static_cast<std::size_t>(index)].b0;
        const double* const shape = s.modeShape.data() + index * maxPorts;
        for (int i = 0; i < ports; ++i)
            for (int j = 0; j < ports; ++j)
                immediate[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] += b0 * shape[i] * shape[j];
    }
    s.portImpedance = {};
    for (int index = 0; index < stringCount_; ++index)
    {
        const double impedance = s.strings[static_cast<std::size_t>(index)].impedance;
        s.portImpedance[static_cast<std::size_t>(index)] = impedance;
        s.portImpedance[static_cast<std::size_t>(swayPort)] += impedance;
    }
    std::array<std::array<double, maxPorts>, maxPorts> junctionSystem {};
    for (int i = 0; i < ports; ++i)
        for (int j = 0; j < ports; ++j)
            junctionSystem[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]
                = (i == j ? 1.0 : 0.0)
                + immediate[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]
                * s.portImpedance[static_cast<std::size_t>(j)];
    invertPorts(junctionSystem, ports);
    s.solve = junctionSystem;
    s.driveGain = {};
    for (int i = 0; i < ports; ++i)
        for (int j = 0; j < ports; ++j)
            for (int k = 0; k < ports; ++k)
                s.driveGain[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]
                    += s.solve[static_cast<std::size_t>(i)][static_cast<std::size_t>(k)]
                     * immediate[static_cast<std::size_t>(k)][static_cast<std::size_t>(j)];

    // ---- Tuning: each port's reflection phase from the continuous model --
    using PortMatrix = std::array<std::array<Complex, maxPorts>, maxPorts>;
    for (int index = 0; index < stringCount_; ++index)
    {
        State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
        for (int design = 0; design < 4; ++design)
        {
            const int plane = design & 1;
            LoopDesign& loop = voice.pitch[static_cast<std::size_t>(design >> 1)]
                                    .planes[static_cast<std::size_t>(plane)];
            const double f0 = loop.frequency;
            const int own = plane == 0 ? index : swayPort;
            // The port's admittance with every other string loading the
            // bridge: Y_port = e^T (I + Yb W)^-1 Yb e, Yb the body's port
            // mobility and W the other strings' impedance at each port.
            const auto reflectionLead = [&] (double frequency)
            {
                const double omega = 2.0 * pi * frequency;
                PortMatrix mobility {};
                for (int m = 0; m < s.modeCount; ++m)
                {
                    const Mode& mode = s.modes[static_cast<std::size_t>(m)];
                    const double wm = 2.0 * pi * mode.frequency;
                    const Complex response = Complex(0.0, omega)
                        / Complex(wm * wm - omega * omega, omega * wm / mode.q);
                    const double* const shape = s.modeShape.data() + m * maxPorts;
                    for (int i = 0; i < ports; ++i)
                    {
                        const Complex weighted = response * shape[i];
                        for (int j = i; j < ports; ++j)
                            mobility[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] += weighted * shape[j];
                    }
                }
                for (int i = 0; i < ports; ++i)
                    for (int j = 0; j < i; ++j)
                        mobility[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]
                            = mobility[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)];
                PortMatrix loaded {};
                for (int i = 0; i < ports; ++i)
                    for (int j = 0; j < ports; ++j)
                    {
                        double others = s.portImpedance[static_cast<std::size_t>(j)];
                        if (j == own)
                            others -= voice.impedance;
                        loaded[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]
                            = (i == j ? 1.0 : 0.0) + mobility[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] * others;
                    }
                invertPorts(loaded, ports);
                Complex admittance = 0.0;
                for (int k = 0; k < ports; ++k)
                    admittance += loaded[static_cast<std::size_t>(own)][static_cast<std::size_t>(k)]
                                * mobility[static_cast<std::size_t>(k)][static_cast<std::size_t>(own)];
                const Complex zy = voice.impedance * admittance;
                // R = (ZY - 1)/(1 + ZY); the loop carries -R.
                return std::arg((1.0 - zy) / (1.0 + zy));
            };
            // The lag of the loop at omega (rad/sample) with the bridge's
            // reflection lead there.
            const auto lagAt = [&] (double omega, double lead, double lineDelay)
            {
                const int whole = static_cast<int>(std::floor(lineDelay - 4.0));
                const double fraction = lineDelay - whole;
                double t1 = 0.0, t2 = 0.0;
                thiranCoefficients(fraction, t1, t2);
                return omega * whole + allpassLag(2, t1, t2, omega) + loop.filterLag(omega) - lead;
            };
            const double omega0 = 2.0 * pi * f0 / fs;
            const double lead0 = reflectionLead(f0);
            double lower = 6.0, upper = delayCapacity - 16.0;
            for (int iteration = 0; iteration < 60; ++iteration)
            {
                const double middle = 0.5 * (lower + upper);
                if (lagAt(omega0, lead0, middle) < 2.0 * pi)
                    lower = middle;
                else
                    upper = middle;
            }
            loop.restLineDelay = 0.5 * (lower + upper);
            for (std::size_t k = 0; k < bendRatios.size(); ++k)
            {
                const double omega = omega0 * bendRatios[k];
                const double lead = reflectionLead(f0 * bendRatios[k]);
                loop.lagCycles[k] = (lagAt(omega, lead, loop.restLineDelay) - omega * loop.restLineDelay) / (2.0 * pi);
            }
        }
        voice.normal.whole = static_cast<int>(voice.pitch[0].planes[0].restLineDelay - 4.0);
        voice.parallel.whole = static_cast<int>(voice.pitch[0].planes[1].restLineDelay - 4.0);
        applyPitch(index, false);
    }

    s.outputGain = instrument.outputGain;
    PlayingControls controls {};
    controls.technique = instrument.technique;
    controls.pluckPosition = instrument.pluckPosition;
    controls.normalShare = instrument.normalShare;
    controls.room = instrument.room;
    setControls(controls);
    reset();
}

void AculyreEngine::reset() noexcept
{
    State& s = *state_;
    for (int index = 0; index < stringCount_; ++index)
    {
        auto& voice = s.strings[static_cast<std::size_t>(index)];
        if (voice.raised)
            applyPitch(index, false);
        voice.normal.reset();
        voice.parallel.reset();
        voice.energy = 0.0;
        voice.keyDown = voice.sustained = voice.plucked = false;
        voice.ownerNote = -1;
        voice.dampingDirty = true;
    }
    for (int index = 0; index < s.modeCount; ++index)
        s.modes[static_cast<std::size_t>(index)].s1 = s.modes[static_cast<std::size_t>(index)].s2 = 0.0;
    s.previousLeft = s.previousRight = 0.0;
    s.room.reset();
    s.sustain = false;
    s.controlCountdown = 0;
    s.random.seed(0x5eed1ac0ffeeull);
    s.bend = s.bendTarget;
    s.output = s.outputTarget;
}

const InstrumentSpec& AculyreEngine::instrument() const noexcept { return state_->instrument; }

int AculyreEngine::stringForNote(int midiNote) const noexcept
{
    for (int index = 0; index < stringCount_; ++index)
        if (state_->strings[static_cast<std::size_t>(index)].midiNote == midiNote)
            return index;
    return -1;
}

namespace
{
// A hand control as a decay rate (1/s): 0, a hand resting on the string, an
// amplitude time constant of 30 ms (Acustra's); 1, no hand at all. Between,
// the time constant grows geometrically, to 6 s just short of 1.
double handRate(double setting) noexcept
{
    return setting >= 1.0 ? 0.0 : 1.0 / (0.03 * std::pow(200.0, setting));
}

double unitClamp(double x) noexcept { return x > 1.0 ? 1.0 : (x >= 0.0 ? x : 0.0); }

// Below this mean squared wave velocity a released string has stopped
// sounding from its pluck.
constexpr double silentEnergy = 1.0e-6;
} // namespace

void AculyreEngine::applyPitch(int string, bool raised) noexcept
{
    State::StringVoice& voice = state_->strings[static_cast<std::size_t>(string)];
    const State::PitchDesign& pitch = voice.pitch[raised ? 1u : 0u];
    voice.raised = raised;
    voice.frequency = pitch.planes[0].frequency;
    voice.parallelFrequency = pitch.planes[1].frequency;
    voice.length = pitch.length;
    voice.parallelLength = pitch.parallelLength;
    voice.inharmonicity = pitch.inharmonicity;
    voice.intrinsicT60 = pitch.intrinsicT60;
    voice.glidePerEnergy = pitch.glidePerEnergy;
    voice.energySmoothing = pitch.energySmoothing;
    voice.normal.apply(pitch.planes[0]);
    voice.parallel.apply(pitch.planes[1]);
    voice.dampingDirty = true;
}

// Every hand on the string, as one loss per round trip reached over a round
// trip: the palm on all strings, and on a string nobody holds, the playing
// hand (plucked) or the resting hand (idle).
void AculyreEngine::updateDamping(int string) noexcept
{
    const State& s = *state_;
    State::StringVoice& voice = state_->strings[static_cast<std::size_t>(string)];
    voice.dampingDirty = false;
    double rate = s.muteRate;
    if (!voice.keyDown && !voice.sustained)
        rate += voice.plucked ? s.releaseRate : s.sympathyRate;
    voice.normal.setDamping(rate > 0.0 ? std::exp(-rate / voice.frequency) : 1.0);
    voice.parallel.setDamping(rate > 0.0 ? std::exp(-rate / voice.parallelFrequency) : 1.0);
}

void AculyreEngine::setControls(const PlayingControls& controls) noexcept
{
    State& s = *state_;
    const PlayingControls previous = controls_;
    PlayingControls& c = controls_;
    c = controls;
    c.pluckPosition = std::clamp(unitClamp(c.pluckPosition), 0.05, 0.7);
    c.normalShare = std::clamp(unitClamp(c.normalShare), 0.01, 0.9);
    c.release = unitClamp(c.release);
    c.mute = unitClamp(c.mute);
    c.sympathy = unitClamp(c.sympathy);
    c.room = unitClamp(c.room);
    c.width = 2.0 * unitClamp(0.5 * c.width);
    c.volume = c.volume >= 0.0 && c.volume <= 4.0 ? c.volume : (c.volume > 4.0 ? 4.0 : 0.0);
    c.pitchBend = c.pitchBend >= -12.0 && c.pitchBend <= 12.0 ? c.pitchBend
                                                               : (c.pitchBend > 12.0 ? 12.0 : 0.0);

    s.releaseRate = handRate(c.release);
    s.sympathyRate = handRate(c.sympathy);
    // A full palm: an amplitude time constant of about 33 ms. Authored.
    s.muteRate = 30.0 * c.mute * c.mute;
    if (c.release != previous.release || c.sympathy != previous.sympathy || c.mute != previous.mute)
        for (int index = 0; index < stringCount_; ++index)
            s.strings[static_cast<std::size_t>(index)].dampingDirty = true;
    if (c.pitchBend != previous.pitchBend)
        s.bendTarget = std::pow(2.0, c.pitchBend / 12.0);
    // Mid/side: width 1 is the two microphones as they are.
    s.outputTarget = { 0.5 * c.room, 0.5 * (1.0 + c.width), 0.5 * (1.0 - c.width), c.volume };
    if (!c.levers && previous.levers)
        for (int index = 0; index < stringCount_; ++index)
            if (s.strings[static_cast<std::size_t>(index)].raised)
                applyPitch(index, false);
}

void AculyreEngine::noteOn(int midiNote, float velocity) noexcept
{
    int index = stringForNote(midiNote);
    bool raise = false;
    if (index < 0 && controls_.levers)
    {
        index = stringForNote(midiNote - 1);
        raise = true;
    }
    if (index < 0 || !(velocity > 0.0f))
        return;
    State& s = *state_;
    State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
    const InstrumentSpec& instrument = s.instrument;
    const PlayingControls& controls = controls_;
    const double v = std::clamp(static_cast<double>(velocity), 0.0, 1.0);

    // A finger meeting a sounding string stops most of it before it lets go
    // (Acustra's picking-hand contact, about 10 ms, as one step).
    if (voice.keyDown || voice.sustained || voice.energy > silentEnergy)
        for (Loop* loop : { &voice.normal, &voice.parallel })
        {
            for (float& sample : loop->line)
                sample *= 0.3f;
            for (float& sample : loop->pending)
                sample *= 0.3f;
        }
    // The other hand sets the lever before the pluck.
    if (voice.raised != raise)
        applyPitch(index, raise);

    // Each pluck differs a little: where the finger meets the string, how
    // hard, and which way it lets go. Authored spreads.
    const double position = std::clamp(controls.pluckPosition + 0.02 * s.random.bipolar(), 0.05, 0.7);
    const double force = instrument.pluckForce * std::pow(v, 1.4)
                       * std::pow(10.0, 0.75 * s.random.bipolar() / 20.0);
    const double normalShare = std::clamp(controls.normalShare + 0.05 * s.random.bipolar(), 0.01, 0.9);

    // Contact width and release time of the technique: the fingertip's pad,
    // a nail's or a plectrum's edge. The string slides off faster from a
    // harder stroke, so it leaves brighter. Authored values; the release
    // time is of the order of Acustra's (a 0.2 mm edge over the string's
    // speed of a metre or two per second).
    double contact = 0.005, release = 0.10e-3;
    switch (controls.technique)
    {
        case Technique::Finger: contact = 0.005; release = 0.10e-3; break;
        case Technique::Nail: contact = 0.0012; release = 0.035e-3; break;
        case Technique::Plectrum: contact = 0.0008; release = 0.025e-3; break;
    }
    release *= 1.3 - 0.6 * v;

    voice.keyDown = true;
    voice.sustained = false;
    voice.plucked = true;
    voice.ownerNote = midiNote;
    updateDamping(index);

    std::array<double, 2> preload {};  // at the string's own port, at the sway port
    for (int plane = 0; plane < 2; ++plane)
    {
        Loop& loop = plane == 0 ? voice.normal : voice.parallel;
        const double length = plane == 0 ? voice.length : voice.parallelLength;
        const double frequency = plane == 0 ? voice.frequency : voice.parallelFrequency;
        const double waveSpeed = 2.0 * length * frequency;
        const double pluckPoint = position * length;
        // The displacement the held force sets, A = F x (L - x) / (T L).
        const double amplitude = force * pluckPoint * (length - pluckPoint) / (voice.tension * length)
                               * std::sqrt(plane == 0 ? normalShare : 1.0 - normalShare);
        // The contact only rounds the apex: harpists' fingers touch at most
        // 2 cm of string, which filters nothing below 6.7 kHz on a concert
        // harp (Chadefaux et al., JASA 131 (2012) 844, Table V).
        const double width = contact;
        const double kink = amplitude * (1.0 / pluckPoint + 1.0 / (length - pluckPoint));
        // The released shape, a triangle rounded by a Gaussian of `width`.
        const auto shape = [&] (double x)
        {
            const double u = (x - pluckPoint) / width;
            return amplitude * x / pluckPoint - kink * width * (u * normalCdf(u) + normalPdf(u));
        };
        // The wave reaching the bridge over the next round trip, one period
        // of the loop: first the half that travels straight there,
        // (c/2) y0'(c t), then the half that went to the yoke,
        // (c/2) y0'(2L - c t). Each sample holds its cell's mean slope, which
        // band-limits the release; the last cell ends at the round trip.
        const double period = loop.loopDelay;
        const int count = static_cast<int>(std::ceil(period));
        const double cellLength = 2.0 * length / period;
        std::array<double, delayCapacity>& wave = s.pluckWave;
        double mean = 0.0;
        for (int k = 0; k < count; ++k)
        {
            const double start = cellLength * k;
            const double end = std::min(2.0 * length, cellLength * (k + 1));
            double integral = 0.0;
            if (end <= length)
                integral = shape(end) - shape(start);
            else if (start >= length)
                integral = shape(2.0 * length - start) - shape(2.0 * length - end);
            else
                integral = (shape(length) - shape(start)) + (shape(length) - shape(2.0 * length - end));
            wave[static_cast<std::size_t>(k)] = 0.5 * waveSpeed * integral / cellLength;
        }
        // The release: the held force unloads exponentially as the string
        // slides off, as Acustra's slip does (a Gaussian release of the same
        // time left a lyre harp phrase 10 dB short of its recording from
        // 1.25 to 5 kHz, the exponential 7.6 dB). Each kink reaching the
        // bridge arrives smoothed by that one-pole; over the loop's period it
        // runs circularly, starting from its steady state at the period's end.
        const double pole = std::exp(-1.0 / (release * sampleRate_));
        double weighted = 0.0, weight = 1.0;
        for (int k = count - 1; k >= 0; --k)
        {
            weighted += weight * wave[static_cast<std::size_t>(k)];
            weight *= pole;
        }
        double smoothed = (1.0 - pole) * weighted / (1.0 - weight);
        for (int k = 0; k < count; ++k)
        {
            smoothed = pole * smoothed + (1.0 - pole) * wave[static_cast<std::size_t>(k)];
            wave[static_cast<std::size_t>(k)] = smoothed;
            mean += smoothed;
        }
        mean /= count;
        for (int k = 0; k < count; ++k)
        {
            const double value = wave[static_cast<std::size_t>(k)] - mean;
            const auto slot = static_cast<std::size_t>((loop.write + k) & (delayCapacity - 1));
            loop.pending[slot] += static_cast<float>(value);
        }
        // The held force stood on the bridge before the release: the body
        // starts deflected under it, so only what the release changes
        // reaches it, and the top springs back as the wave's mean force
        // falls to zero.
        preload[static_cast<std::size_t>(plane)] = 2.0 * voice.impedance * (wave[0] - mean);
    }
    for (int index2 = 0; index2 < s.modeCount; ++index2)
    {
        Mode& mode = s.modes[static_cast<std::size_t>(index2)];
        const double* const shape = s.modeShape.data() + index2 * maxPorts;
        const double modal = shape[index] * preload[0] + shape[stringCount_] * preload[1];
        mode.s1 += mode.b2 * modal;
        mode.s2 += mode.b2 * modal;
    }
}

void AculyreEngine::noteOff(int midiNote) noexcept
{
    State& s = *state_;
    for (int index = 0; index < stringCount_; ++index)
    {
        State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
        if (!voice.keyDown || voice.ownerNote != midiNote)
            continue;
        voice.keyDown = false;
        voice.sustained = s.sustain;
        updateDamping(index);
    }
}

void AculyreEngine::setSustain(bool down) noexcept
{
    State& s = *state_;
    s.sustain = down;
    if (down)
        return;
    for (int index = 0; index < stringCount_; ++index)
    {
        State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
        if (voice.sustained)
        {
            voice.sustained = false;
            updateDamping(index);
        }
    }
}

void AculyreEngine::process(float* left, float* right, int numSamples) noexcept
{
    State& s = *state_;
    const int count = stringCount_;
    const int ports = s.portCount;
    const int modeCount = s.modeCount;
    std::array<double, maxStrings> normalIn {};
    std::array<double, maxStrings> parallelIn {};

    for (int sample = 0; sample < numSamples; ++sample)
    {
        if (s.controlCountdown-- <= 0)
        {
            s.controlCountdown = controlPeriod - 1;
            s.bend += (s.bendTarget - s.bend) * s.bendSmoothing;
            for (int index = 0; index < count; ++index)
            {
                State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
                // A released string that has died away is idle again: the
                // playing hand has left it.
                if (voice.plucked && !voice.keyDown && !voice.sustained && voice.energy < silentEnergy)
                {
                    voice.plucked = false;
                    voice.dampingDirty = true;
                }
                if (voice.dampingDirty)
                    updateDamping(index);
                // Tension modulation (Kirchhoff-Carrier): the string's mean
                // squared slope stretches it, Delta f / f = EA <y'^2> / (4T);
                // the pitch bend scales every string alike.
                const double ratio = (tensionModulation_
                    ? 1.0 + voice.glidePerEnergy * voice.energy : 1.0) * s.bend;
                for (Loop* loop : { &voice.normal, &voice.parallel })
                    loop->setLineDelay(loop->lineDelayAt(ratio));
            }
        }

        // The strings' incoming waves as forces at the ports (each string's
        // point, then the bar's tipping, which every parallel plane drives).
        std::array<double, maxPorts> drive {};
        for (int index = 0; index < count; ++index)
        {
            State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
            const double vn = voice.normal.read();
            const double vp = voice.parallel.read();
            normalIn[static_cast<std::size_t>(index)] = vn;
            parallelIn[static_cast<std::size_t>(index)] = vp;
            const double twiceZ = 2.0 * voice.impedance;
            drive[static_cast<std::size_t>(index)] = twiceZ * vn;
            drive[static_cast<std::size_t>(count)] += twiceZ * vp;
            // The loop's mean squared velocity over a round trip; both
            // directions' slopes count, hence the factor in glidePerEnergy.
            voice.energy = voice.energySmoothing * voice.energy
                         + (1.0 - voice.energySmoothing) * (vn * vn + vp * vp);
        }

        // The modes' velocity at the ports from their past alone.
        std::array<double, maxPorts> past {};
        for (int index = 0; index < modeCount; ++index)
        {
            const double state = s.modes[static_cast<std::size_t>(index)].s1;
            const double* const shape = s.modeShape.data() + index * maxPorts;
            for (int port = 0; port < ports; ++port)
                past[static_cast<std::size_t>(port)] += shape[port] * state;
        }
        // The ports' velocity, v = solve (Y0 drive + past), and the force
        // each port puts on the body, drive - Zg v.
        std::array<double, maxPorts> velocity {};
        std::array<double, maxPorts> force {};
        for (int i = 0; i < ports; ++i)
        {
            const auto si = static_cast<std::size_t>(i);
            double v = 0.0;
            for (int j = 0; j < ports; ++j)
            {
                const auto sj = static_cast<std::size_t>(j);
                v += s.driveGain[si][sj] * drive[sj] + s.solve[si][sj] * past[sj];
            }
            velocity[si] = v;
            force[si] = drive[si] - s.portImpedance[si] * v;
        }
        double* const modal = s.modalForce.data();
        {
            const double* const shape = s.portShape.data();
            const double f = force[0];
            for (int index = 0; index < modeCount; ++index)
                modal[index] = shape[index] * f;
        }
        for (int port = 1; port < ports; ++port)
        {
            const double* const shape = s.portShape.data() + port * maxModes;
            const double f = force[static_cast<std::size_t>(port)];
            for (int index = 0; index < modeCount; ++index)
                modal[index] += shape[index] * f;
        }

        double radiatedLeft = 0.0, radiatedRight = 0.0;
        for (int index = 0; index < modeCount; ++index)
        {
            Mode& mode = s.modes[static_cast<std::size_t>(index)];
            const double x = modal[index];
            const double y = mode.b0 * x + mode.s1;
            mode.s1 = -mode.a1 * y + mode.s2;
            mode.s2 = mode.b2 * x - mode.a2 * y;
            radiatedLeft += mode.radiationLeft * y;
            radiatedRight += mode.radiationRight * y;
        }

        for (int index = 0; index < count; ++index)
        {
            State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
            voice.normal.writeSample(normalIn[static_cast<std::size_t>(index)] - velocity[static_cast<std::size_t>(index)]);
            voice.parallel.writeSample(parallelIn[static_cast<std::size_t>(index)] - velocity[static_cast<std::size_t>(count)]);
        }

        // Far-field pressure: rho / (4 pi r) times the volume acceleration.
        const double pressureLeft = (radiatedLeft - s.previousLeft) * s.radiationScale;
        const double pressureRight = (radiatedRight - s.previousRight) * s.radiationScale;
        s.previousLeft = radiatedLeft;
        s.previousRight = radiatedRight;
        for (std::size_t i = 0; i < 4; ++i)
            s.output[i] += (s.outputTarget[i] - s.output[i]) * s.outputSmoothing;
        // The room runs even unsent, so turning it up meets a live tail.
        double wetLeft = 0.0, wetRight = 0.0;
        s.room.process(0.5 * (pressureLeft + pressureRight), wetLeft, wetRight);
        const double outLeft = pressureLeft + s.output[0] * wetLeft;
        const double outRight = pressureRight + s.output[0] * wetRight;
        const double gain = s.outputGain * s.output[3];
        const auto limit = [] (double x)
        {
            const double magnitude = x < 0.0 ? -x : x;
            if (magnitude <= 0.9)
                return x;
            const double limited = 0.9 + 0.1 * std::tanh((magnitude - 0.9) / 0.1);
            return x < 0.0 ? -limited : limited;
        };
        left[sample] = static_cast<float>(limit(gain * (s.output[1] * outLeft + s.output[2] * outRight)));
        right[sample] = static_cast<float>(limit(gain * (s.output[1] * outRight + s.output[2] * outLeft)));
    }
}

AculyreEngine::StringReport AculyreEngine::stringReport(int string) const noexcept
{
    StringReport report {};
    if (string < 0 || string >= stringCount_)
        return report;
    const auto& voice = state_->strings[static_cast<std::size_t>(string)];
    const auto& spec = state_->instrument.strings[static_cast<std::size_t>(string)];
    report.midiNote = voice.midiNote;
    report.frequency = voice.frequency;
    report.tension = voice.tension;
    report.impedance = voice.impedance;
    report.inharmonicity = voice.inharmonicity;
    report.stress = voice.tension / (0.25 * pi * spec.diameter * spec.diameter);
    report.intrinsicT60 = voice.intrinsicT60;
    report.dispersion = voice.normal.dispersion;
    report.dispersionStages = voice.normal.dispersionStages;
    report.dispersionCents = voice.normal.dispersionCents;
    return report;
}

double AculyreEngine::stringEnergy(int string) const noexcept
{
    if (string < 0 || string >= stringCount_)
        return 0.0;
    return state_->strings[static_cast<std::size_t>(string)].energy;
}

AculyreEngine::StringActivity AculyreEngine::stringActivity(int string) const noexcept
{
    if (string < 0 || string >= stringCount_)
        return {};
    const auto& voice = state_->strings[static_cast<std::size_t>(string)];
    return { voice.energy, voice.keyDown || voice.sustained, voice.plucked, voice.raised };
}

int AculyreEngine::modeCount() const noexcept { return state_->modeCount; }

AculyreEngine::ModeReport AculyreEngine::modeReport(int mode) const noexcept
{
    ModeReport report {};
    if (mode < 0 || mode >= state_->modeCount)
        return report;
    const Mode& m = state_->modes[static_cast<std::size_t>(mode)];
    report.frequency = m.frequency;
    report.q = m.q;
    double sum = 0.0;
    for (int string = 0; string < stringCount_; ++string)
    {
        const double value = state_->modeShape[static_cast<std::size_t>(mode * maxPorts + string)];
        sum += value * value;
    }
    report.bridge = std::sqrt(sum / stringCount_);
    report.monopole = m.monopole;
    report.airShare = m.airShare;
    return report;
}

double AculyreEngine::bridgeMobilityMagnitude(double frequency, int string) const noexcept
{
    const State& s = *state_;
    if (string < 0 || string >= stringCount_)
        return 0.0;
    const double omega = 2.0 * pi * frequency;
    Complex mobility = 0.0;
    for (int m = 0; m < s.modeCount; ++m)
    {
        const Mode& mode = s.modes[static_cast<std::size_t>(m)];
        const double wm = 2.0 * pi * mode.frequency;
        const double shape = s.modeShape[static_cast<std::size_t>(m * maxPorts + string)];
        mobility += shape * shape * Complex(0.0, omega) / Complex(wm * wm - omega * omega, omega * wm / mode.q);
    }
    return std::abs(mobility);
}

} // namespace aculyre
