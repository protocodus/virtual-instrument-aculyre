#include "AculyreEngine.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstring>
#include <limits>
#include <utility>

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
constexpr int dispersionPairs = 2;

// ---------------------------------------------------------------------------
// String loss laws, copied from Acustra 1.9.0 (Source/DSP/AcustraEngine.cpp),
// where their derivations and references are given in full: the bending loss
// of Valette's and Woodhouse's string damping models, 1/Q_n = eta B n^2 /
// (1 + B n^2), as a unit-DC two-pole section, and a loss angle constant over
// the band (Paté, Le Carrou and Fabre, JASA 135 (2014) 3045) as a unit-DC
// two-pole two-zero section of two relaxations. Aculyre's change: that
// section also carries the measured air damping's rise over the
// fundamental's (airRoot, see setInstrument).

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

LossSectionDesign constantLossSection(double delta, double airRoot, double inharmonicity,
                                      double fundamental, double sampleRate) noexcept
{
    LossSectionDesign section;
    if (!(delta > 0.0 || airRoot > 0.0) || !(inharmonicity >= 0.0) || !(fundamental > 0.0)
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
        const double frequency = fundamental * stretchedPartial(partial, inharmonicity);
        return pi * frequency * delta + airRoot * (std::sqrt(frequency) - std::sqrt(fundamental));
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
// to 5th partials 30 to 120 cents sharp. No cascade of identical sections
// fits a bass string's 25 partials below 5 kHz either: the steel G3's 13th to
// 21st fell 10 to 95 cents flat. Where partials past the 12th fall below
// 5 kHz (strings under about 417 Hz), two second-order sections are fitted
// with the first-order coefficient to every one of them (refineDispersion).
struct DispersionDesign
{
    int stages { 1 };
    double coefficient { 0.0 };
    double worstCents { 0.0 };
    // Second-order sections (c2 + c1 z^-1 + z^-2) / (1 + c1 z^-1 + c2 z^-2),
    // as { c1, c2 }; `pairs` of them are in use.
    int pairs { 0 };
    std::array<std::array<double, 2>, dispersionPairs> pair {};
};

// A first-order section's phase lag and group delay, (a + z^-1)/(1 + a z^-1).
double firstOrderLag(double a, double omega) noexcept
{
    return omega - 2.0 * std::atan2(a * std::sin(omega), 1.0 + a * std::cos(omega));
}

double firstOrderGroupDelay(double a, double omega) noexcept
{
    return (1.0 - a * a) / (1.0 + 2.0 * a * std::cos(omega) + a * a);
}

// The fit's parameters: the first-order coefficient, then each second-order
// section's pole radius and angle.
using DispersionFit = std::array<double, 1 + 2 * dispersionPairs>;
constexpr int maxFittedPartials = 64;

// Each partial's error in cents against the stiff-string law, partials 2 to
// `partials`, with the line a pure delay tuned to f0; false where the line
// would be under the 8 samples the Thiran section and the loss filters need.
bool dispersionErrors(const DispersionFit& fit, int stages, int pairs, double omega1, double inharmonicity,
                      int partials, std::array<double, maxFittedPartials>& cents) noexcept
{
    const auto lag = [&] (double omega)
    {
        double total = stages * firstOrderLag(fit[0], omega);
        for (int k = 0; k < pairs; ++k)
        {
            const Complex pole = std::polar(fit[static_cast<std::size_t>(1 + 2 * k)],
                                            fit[static_cast<std::size_t>(2 + 2 * k)]);
            const Complex z1 = std::exp(Complex(0.0, -omega));
            total += 2.0 * omega + 2.0 * (std::arg(1.0 - pole * z1) + std::arg(1.0 - std::conj(pole) * z1));
        }
        return total;
    };
    const auto groupDelay = [&] (double omega)
    {
        double total = stages * firstOrderGroupDelay(fit[0], omega);
        for (int k = 0; k < pairs; ++k)
        {
            const double r = fit[static_cast<std::size_t>(1 + 2 * k)];
            const double theta = fit[static_cast<std::size_t>(2 + 2 * k)];
            total += (1.0 - r * r) / (1.0 - 2.0 * r * std::cos(omega - theta) + r * r)
                   + (1.0 - r * r) / (1.0 - 2.0 * r * std::cos(omega + theta) + r * r);
        }
        return total;
    };
    const double delay = (2.0 * pi - lag(omega1)) / omega1;
    if (!(delay >= 8.0))
        return false;
    for (int n = 2; n <= partials; ++n)
    {
        const double target = omega1 * stretchedPartial(n, inharmonicity);
        double omega = target;
        for (int iteration = 0; iteration < 4; ++iteration)
            omega -= (omega * delay + lag(omega) - 2.0 * pi * n) / (delay + groupDelay(omega));
        cents[static_cast<std::size_t>(n - 2)] = 1200.0 * std::log2(omega / target);
    }
    return true;
}

double worstOf(const std::array<double, maxFittedPartials>& cents, int partials) noexcept
{
    double worst = 0.0;
    for (int n = 2; n <= partials; ++n)
        worst = std::max(worst, std::abs(cents[static_cast<std::size_t>(n - 2)]));
    return worst;
}

// Levenberg-Marquardt on the partials' cents, the Jacobian by forward
// differences, from the first-order design and poles near 1.25 and 2.5 kHz,
// 800 Hz wide (where a steel G3's fit settles). On the lyre harps' bass
// strings it holds every partial below 5 kHz within 5 cents and the first 12
// within 2; the design is kept only where it beats the first-order one.
DispersionDesign refineDispersion(const DispersionDesign& firstOrder, double fundamental,
                                  double inharmonicity, double sampleRate) noexcept
{
    const double omega1 = 2.0 * pi * fundamental / sampleRate;
    // The partials whose stretched frequency is below the band's top.
    const double top = std::min(5000.0, 0.2 * sampleRate);
    int partials = 1;
    while (partials < maxFittedPartials + 1 && fundamental * stretchedPartial(partials + 1, inharmonicity) < top)
        ++partials;
    DispersionDesign result = firstOrder;
    if (partials <= 12)
        return result;
    std::array<double, maxFittedPartials> cents {};
    DispersionFit fit {};
    fit[0] = firstOrder.coefficient;
    if (!dispersionErrors(fit, firstOrder.stages, 0, omega1, inharmonicity, partials, cents))
        return result;
    result.worstCents = worstOf(cents, partials);

    fit[0] = firstOrder.coefficient;
    for (int k = 0; k < dispersionPairs; ++k)
    {
        fit[static_cast<std::size_t>(1 + 2 * k)] = std::exp(-2.0 * pi * 800.0 / sampleRate);
        fit[static_cast<std::size_t>(2 + 2 * k)] = 2.0 * pi * 1250.0 * (k + 1) / sampleRate;
    }
    const auto valid = [] (const DispersionFit& p)
    {
        bool ok = std::abs(p[0]) < 0.98;
        for (int k = 0; k < dispersionPairs; ++k)
            ok = ok && p[static_cast<std::size_t>(1 + 2 * k)] >= 0.0 && p[static_cast<std::size_t>(1 + 2 * k)] < 0.99
                 && p[static_cast<std::size_t>(2 + 2 * k)] > 0.0 && p[static_cast<std::size_t>(2 + 2 * k)] < pi;
        return ok;
    };
    const auto cost = [&] (const DispersionFit& p, std::array<double, maxFittedPartials>& e)
    {
        if (!valid(p) || !dispersionErrors(p, firstOrder.stages, dispersionPairs, omega1, inharmonicity, partials, e))
            return std::numeric_limits<double>::infinity();
        double sum = 0.0;
        for (int n = 2; n <= partials; ++n)
            sum += e[static_cast<std::size_t>(n - 2)] * e[static_cast<std::size_t>(n - 2)];
        return sum;
    };
    constexpr int parameters = 1 + 2 * dispersionPairs;
    const int residuals = partials - 1;
    double current = cost(fit, cents);
    if (!(current < std::numeric_limits<double>::infinity()))
        return result;
    double damping = 1.0e-3;
    std::array<std::array<double, maxFittedPartials>, parameters> jacobian {};
    std::array<double, maxFittedPartials> trial {};
    for (int iteration = 0; iteration < 100 && damping < 1.0e10; ++iteration)
    {
        for (int j = 0; j < parameters; ++j)
        {
            // Forward, or backward at a bound; a parameter boxed in both
            // ways stays where it is.
            constexpr double step = 1.0e-7;
            auto& column = jacobian[static_cast<std::size_t>(j)];
            DispersionFit nudged = fit;
            nudged[static_cast<std::size_t>(j)] += step;
            double sign = 1.0;
            if (!(cost(nudged, trial) < std::numeric_limits<double>::infinity()))
            {
                nudged[static_cast<std::size_t>(j)] -= 2.0 * step;
                sign = -1.0;
                if (!(cost(nudged, trial) < std::numeric_limits<double>::infinity()))
                {
                    column.fill(0.0);
                    continue;
                }
            }
            for (int i = 0; i < residuals; ++i)
                column[static_cast<std::size_t>(i)]
                    = sign * (trial[static_cast<std::size_t>(i)] - cents[static_cast<std::size_t>(i)]) / step;
        }
        // (J^T J + damping diag(J^T J)) delta = -J^T e, by Gaussian
        // elimination with partial pivoting.
        std::array<std::array<double, parameters + 1>, parameters> system {};
        for (int row = 0; row < parameters; ++row)
        {
            const auto& u = jacobian[static_cast<std::size_t>(row)];
            for (int column = 0; column < parameters; ++column)
            {
                const auto& w = jacobian[static_cast<std::size_t>(column)];
                double sum = 0.0;
                for (int i = 0; i < residuals; ++i)
                    sum += u[static_cast<std::size_t>(i)] * w[static_cast<std::size_t>(i)];
                system[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] = sum;
            }
            double gradient = 0.0;
            for (int i = 0; i < residuals; ++i)
                gradient += u[static_cast<std::size_t>(i)] * cents[static_cast<std::size_t>(i)];
            system[static_cast<std::size_t>(row)][parameters] = -gradient;
            system[static_cast<std::size_t>(row)][static_cast<std::size_t>(row)] *= 1.0 + damping;
        }
        bool solved = true;
        for (int column = 0; column < parameters && solved; ++column)
        {
            int pivot = column;
            for (int row = column + 1; row < parameters; ++row)
                if (std::abs(system[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)])
                    > std::abs(system[static_cast<std::size_t>(pivot)][static_cast<std::size_t>(column)]))
                    pivot = row;
            std::swap(system[static_cast<std::size_t>(column)], system[static_cast<std::size_t>(pivot)]);
            const double diagonal = system[static_cast<std::size_t>(column)][static_cast<std::size_t>(column)];
            solved = std::abs(diagonal) > 1.0e-300;
            for (int row = column + 1; row < parameters && solved; ++row)
            {
                const double factor = system[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] / diagonal;
                for (int k = column; k <= parameters; ++k)
                    system[static_cast<std::size_t>(row)][static_cast<std::size_t>(k)]
                        -= factor * system[static_cast<std::size_t>(column)][static_cast<std::size_t>(k)];
            }
        }
        DispersionFit delta {};
        for (int row = parameters - 1; row >= 0 && solved; --row)
        {
            double sum = system[static_cast<std::size_t>(row)][parameters];
            for (int k = row + 1; k < parameters; ++k)
                sum -= system[static_cast<std::size_t>(row)][static_cast<std::size_t>(k)] * delta[static_cast<std::size_t>(k)];
            delta[static_cast<std::size_t>(row)] = sum / system[static_cast<std::size_t>(row)][static_cast<std::size_t>(row)];
        }
        DispersionFit next = fit;
        for (int k = 0; k < parameters; ++k)
            next[static_cast<std::size_t>(k)] += delta[static_cast<std::size_t>(k)];
        const double candidate = solved ? cost(next, trial) : std::numeric_limits<double>::infinity();
        if (candidate < current)
        {
            // Settled once a step gains under a millionth.
            const bool settled = current - candidate < 1.0e-6 * current;
            fit = next;
            cents = trial;
            current = candidate;
            damping = std::max(1.0e-9, damping / 3.0);
            if (settled)
                break;
        }
        else
            damping *= 4.0;
    }
    const double worst = worstOf(cents, partials);
    if (worst < result.worstCents)
    {
        result.coefficient = fit[0];
        result.pairs = dispersionPairs;
        for (int k = 0; k < dispersionPairs; ++k)
        {
            const double r = fit[static_cast<std::size_t>(1 + 2 * k)];
            const double theta = fit[static_cast<std::size_t>(2 + 2 * k)];
            result.pair[static_cast<std::size_t>(k)] = { -2.0 * r * std::cos(theta), r * r };
        }
        result.worstCents = worst;
    }
    return result;
}

DispersionDesign designDispersion(double fundamental, double inharmonicity, double sampleRate) noexcept
{
    const double omega1 = 2.0 * pi * fundamental / sampleRate;
    const int partials = std::clamp(static_cast<int>(std::floor(std::min(5000.0, 0.2 * sampleRate) / fundamental)), 2, 12);
    const auto lag = firstOrderLag;
    const auto groupDelay = firstOrderGroupDelay;
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
        {
            chosen = design;
            break;
        }
        if (design.worstCents < chosen.worstCents)
            chosen = design;
    }
    return refineDispersion(chosen, fundamental, inharmonicity, sampleRate);
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

// The frequency ratios at which setup measures each loop's phase lag for the
// pitch bend and the tension glide: every quarter semitone over the bend's
// +-12 semitones. The bridge's reflection phase turns across a body mode
// within about half a semitone (a Q of 35); semitone steps over +-2 left
// bends up to 3 cents off between them, and extrapolation past +2 left an
// octave up on G3 59 cents flat.
constexpr int bendStepsPerSemitone = 4;
constexpr int bendPoints = 24 * bendStepsPerSemitone + 1;
const std::array<double, bendPoints> bendRatios = []
{
    std::array<double, bendPoints> ratios {};
    for (int k = 0; k < bendPoints; ++k)
        ratios[static_cast<std::size_t>(k)]
            = std::pow(2.0, static_cast<double>(k - 12 * bendStepsPerSemitone) / (12 * bendStepsPerSemitone));
    return ratios;
}();

// The line's whole samples for a delay of `samples`, leaving the Thiran
// section a fraction in [1.3, 2.3): about 2, where the section is a pure
// delay at every frequency (Loop::setLineDelay).
int wholeFor(double samples) noexcept
{
    return static_cast<int>(std::floor(samples - 1.3));
}

// What sets one polarisation's pitch: its frequency, its filters and the
// line that tunes them. A sharping lever swaps one for another.
struct LoopDesign
{
    double frequency { 0.0 };
    double gain { 1.0 };
    int dispersionStages { 0 };
    double dispersion { 0.0 };
    int dispersionPairs { 0 };
    std::array<std::array<double, 2>, aculyre::dispersionPairs> pair {};
    double dispersionCents { 0.0 };  // the design's worst partial error
    LossSectionDesign constantDesign {};
    LossSectionDesign bendingDesign {};
    // The line delay at rest (whole + fraction) and the whole loop's delay
    // at the fundamental, both in samples.
    double restLineDelay { 68.0 };
    double loopDelay { 70.0 };
    // The phase lag, in cycles, of everything but the line's delay (the
    // filters' dispersion and the bridge's reflection) at bendRatios.
    std::array<double, bendPoints> lagCycles {};

    // Phase lag of everything in the loop but the line's whole samples and
    // the Thiran section, at omega (rad/sample).
    double filterLag(double omega) const noexcept
    {
        double lag = 0.0;
        for (int stage = 0; stage < dispersionStages; ++stage)
            lag += allpassLag(1, dispersion, 0.0, omega);
        for (int k = 0; k < dispersionPairs; ++k)
            lag += allpassLag(2, pair[static_cast<std::size_t>(k)][0], pair[static_cast<std::size_t>(k)][1], omega);
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
    double fraction { 2.0 };
    double thiranA1 { 0.0 }, thiranA2 { 0.0 };
    double tx1 { 0.0 }, tx2 { 0.0 }, ty1 { 0.0 }, ty2 { 0.0 };
    // The wave that last arrived at the bridge.
    double last { 0.0 };
    int dispersionStages { 0 };
    double dispersion { 0.0 };
    double dispersionCents { 0.0 };  // the design's worst partial error
    std::array<double, maxDispersionStages> dispersionState {};
    int dispersionPairs { 0 };
    std::array<std::array<double, 2>, aculyre::dispersionPairs> pair {};
    std::array<std::array<double, 2>, aculyre::dispersionPairs> pairState {};
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
    std::array<double, bendPoints> lagCycles {};

    // Stops the wave and everything still on its way, keeping the pitch and
    // the hand's damping.
    void silence() noexcept
    {
        line.fill(0.0f);
        pending.fill(0.0f);
        tx1 = tx2 = ty1 = ty2 = last = 0.0;
        dispersionState.fill(0.0);
        pairState = {};
        constantLoss.reset();
        bendingLoss.reset();
    }

    void reset() noexcept
    {
        silence();
        damping = dampingTarget = 1.0;
        dampingStep = 0.0;
        setLineDelay(restLineDelay);
    }

    // Keeps the Thiran fraction in [1.1, 2.3), and once it leaves moves the
    // tap to bring it back into [1.3, 2.3) (wholeFor): the section is a pure
    // delay at 2 and strays from it at high frequencies the further the
    // fraction is from 2, at 11 kHz and 48 kHz by under 0.05 samples in that
    // range, 0.27 at 3 and 1.2 at 4.5. Fractions resting 4 to 5 deep had
    // left the steel A5's partials above 6 kHz up to 50 cents sharp, and up
    // to 33 under a held bend of half a semitone while the tap could drift
    // to 5. A treble string's tension glide is under 0.4 samples at 48 kHz;
    // a hard pluck's glide moves a bass string's tap a few times. The
    // section's past inputs move with the tap, so the wave passes it
    // unbroken.
    void setLineDelay(double samples) noexcept
    {
        const double current = samples - whole;
        if (current < 1.1 || current >= 2.3)
        {
            whole = std::clamp(wholeFor(samples), 1, delayCapacity - 8);
            tx1 = line[static_cast<std::size_t>((write - 1 - whole) & (delayCapacity - 1))];
            tx2 = line[static_cast<std::size_t>((write - 2 - whole) & (delayCapacity - 1))];
        }
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
        for (int k = 0; k < dispersionPairs; ++k)
        {
            const double c1 = pair[static_cast<std::size_t>(k)][0], c2 = pair[static_cast<std::size_t>(k)][1];
            auto& state = pairState[static_cast<std::size_t>(k)];
            const double out = c2 * y + state[0];
            state[0] = c1 * (y - out) + state[1];
            state[1] = y - c2 * out;
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
        last = y;
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
    // the ratios setup measured (extrapolated past the last, where the glide
    // takes a full bend). Scaling the line alone would leave the
    // dispersion's phase delay at its own frequency: 11 cents flat of a
    // two-semitone bend on A4.
    double lineDelayAt(double ratio) const noexcept
    {
        if (ratio == 1.0)
            return restLineDelay;
        const auto k = static_cast<std::size_t>(
            std::upper_bound(bendRatios.begin() + 1, bendRatios.end() - 1, ratio) - bendRatios.begin() - 1);
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
        for (int k = dispersionPairs; k < design.dispersionPairs; ++k)
            pairState[static_cast<std::size_t>(k)] = {};
        gain = design.gain;
        dispersionStages = design.dispersionStages;
        dispersion = design.dispersion;
        dispersionPairs = design.dispersionPairs;
        pair = design.pair;
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

// The bridge bar's generalized coordinates: the rigid bar's heave, rock and
// sway, then its first bending modes (setInstrument()).
constexpr int maxBends = 7;
constexpr std::size_t barCoordinates = 3 + maxBends;
using BarVector = std::array<double, barCoordinates>;
// The coordinates even about the bar's middle (heave, sway, the even bends)
// and odd (rock, the odd bends).
using EvenCoordinates = std::index_sequence<0, 2, 3, 5, 7, 9>;
using OddCoordinates = std::index_sequence<1, 4, 6, 8>;
static_assert(maxBends == 7, "EvenCoordinates and OddCoordinates list every bend");
constexpr bool evenCoordinate(std::size_t c) noexcept { return c != 1 && (c < 3 || (c - 3) % 2 == 0); }

// A free-free beam's bend at u, from -1 at one end to 1 at the other:
// cosh + cos - sigma (sinh + sin) of beta x / L. Each has a mean square of 1
// along the beam and is orthogonal to its heave and rock.
constexpr std::array<double, maxBends> bendBetas { 4.730040745, 7.853204624, 10.995607838, 14.137165491,
                                                   17.27875966, 20.42035225, 23.5619449 };
double bendShape(int bend, double u) noexcept
{
    const double beta = bendBetas[static_cast<std::size_t>(bend)];
    const double z = 0.5 * (u + 1.0) * beta;
    const double sigma = (std::cosh(beta) - std::cos(beta)) / (std::sinh(beta) - std::sin(beta));
    return std::cosh(z) + std::cos(z) - sigma * (std::sinh(z) + std::sin(z));
}

// One soundbox mode with unit modal mass: velocity per generalized force
// s / (s^2 + (w/Q) s + w^2), bilinear at its own frequency (passive at every
// rate): b0 (1 - z^-2) / (1 + a1 z^-1 + a2 z^-2).
struct Mode
{
    double frequency { 0.0 };
    double q { 1.0 };
    double b0 { 0.0 }, a1 { 0.0 }, a2 { 0.0 };
    BarVector participation {};  // heave, rock, sway, bends
    double radiationLeft { 0.0 };
    double radiationRight { 0.0 };
    double monopole { 0.0 };
    double airShare { 0.0 };
};

// The modes as process() runs them, in transposed direct form II: a
// structure of arrays, padded with silent modes to whole groups of `lanes`
// so the compiler can vectorize each group. `drive` is the participation
// times b0, `pull` the participation alone. The modes that meet only the
// even coordinates come first, up to evenEnd, then those that meet only the
// odd ones, up to oddEnd, then the rest, each part padded, so that process()
// runs each part with only its own coordinates.
struct ModeBank
{
    static constexpr int lanes = 8;
    static constexpr int capacity = maxModes + 3 * lanes;
    int evenEnd { 0 }, oddEnd { 0 }, count { 0 };
    std::array<std::array<double, capacity>, barCoordinates> drive {}, pull {};
    std::array<double, capacity> a1 {}, a2 {}, left {}, right {}, s1 {}, s2 {};
};

// Runs the bank's modes [from, to) one sample on over the coordinates C,
// adding what they radiate to each microphone and their pull on the bar's
// next velocity, each lane in its own sum so that a group runs as one vector.
template <std::size_t... C>
void runModes(ModeBank& bank, int from, int to, const BarVector& force, double (&left)[ModeBank::lanes],
              double (&right)[ModeBank::lanes], double (&past)[barCoordinates][ModeBank::lanes],
              std::index_sequence<C...>) noexcept
{
    for (int group = from; group < to; group += ModeBank::lanes)
        for (int lane = 0; lane < ModeBank::lanes; ++lane)
        {
            const auto k = static_cast<std::size_t>(group + lane);
            const double x = (0.0 + ... + (bank.drive[C][k] * force[C]));
            const double y = x + bank.s1[k];
            const double s1 = bank.s2[k] - bank.a1[k] * y;
            bank.s2[k] = -x - bank.a2[k] * y;
            bank.s1[k] = s1;
            left[lane] += bank.left[k] * y;
            right[lane] += bank.right[k] * y;
            ((past[C][lane] += bank.pull[C][k] * s1), ...);
        }
}

struct BarMatrix
{
    std::array<std::array<double, barCoordinates>, barCoordinates> m {};

    static BarMatrix identity() noexcept
    {
        BarMatrix r;
        for (std::size_t i = 0; i < barCoordinates; ++i)
            r.m[i][i] = 1.0;
        return r;
    }
    BarMatrix operator*(const BarMatrix& o) const noexcept
    {
        BarMatrix r;
        for (std::size_t i = 0; i < barCoordinates; ++i)
            for (std::size_t j = 0; j < barCoordinates; ++j)
                for (std::size_t k = 0; k < barCoordinates; ++k)
                    r.m[i][j] += m[i][k] * o.m[k][j];
        return r;
    }
    BarVector apply(const BarVector& v) const noexcept
    {
        BarVector r {};
        for (std::size_t i = 0; i < barCoordinates; ++i)
            for (std::size_t j = 0; j < barCoordinates; ++j)
                r[i] += m[i][j] * v[j];
        return r;
    }
};

// Gauss-Jordan with partial pivoting (setup only).
template <typename T, std::size_t N>
std::array<std::array<T, N>, N> inverse(std::array<std::array<T, N>, N> a) noexcept
{
    std::array<std::array<T, N>, N> r {};
    for (std::size_t i = 0; i < N; ++i)
        r[i][i] = T(1.0);
    for (std::size_t c = 0; c < N; ++c)
    {
        std::size_t pivot = c;
        for (std::size_t i = c + 1; i < N; ++i)
            if (std::abs(a[i][c]) > std::abs(a[pivot][c]))
                pivot = i;
        std::swap(a[c], a[pivot]);
        std::swap(r[c], r[pivot]);
        const T scale = T(1.0) / a[c][c];
        for (std::size_t j = 0; j < N; ++j)
        {
            a[c][j] *= scale;
            r[c][j] *= scale;
        }
        for (std::size_t i = 0; i < N; ++i)
            if (i != c)
            {
                const T factor = a[i][c];
                for (std::size_t j = 0; j < N; ++j)
                {
                    a[i][j] -= factor * a[c][j];
                    r[i][j] -= factor * r[c][j];
                }
            }
    }
    return r;
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
    // Standard normal (Box-Muller), cut at three deviations.
    double gaussian() noexcept
    {
        const double u = (static_cast<double>(next() >> 11) + 1.0) * (1.0 / 9007199254740992.0);
        return std::clamp(std::sqrt(-2.0 * std::log(u)) * std::cos(pi * bipolar()), -3.0, 3.0);
    }
};

double normalCdf(double u) noexcept { return 0.5 * std::erfc(-u / std::sqrt(2.0)); }
double normalPdf(double u) noexcept { return std::exp(-0.5 * u * u) / std::sqrt(2.0 * pi); }

// A small room around the microphones: early reflections and an eight-line
// feedback delay network (Householder feedback, one-pole loss per line set
// for a 1.1 s low and 0.45 s high reverberation time). Authored, like any
// room: it is the space the demos are heard in, not part of the instrument.
// What enters the network is first smeared by four allpasses, Dattorro's
// input diffusers (JAES 45(9) 1997, Fig. 1: 142, 107, 379 and 277 samples
// at 29.761 kHz, gains 0.75 and 0.625). A real room's reflections are dense
// within tens of milliseconds; fed a pluck directly, the network's tail
// stayed a train of separate echoes, its normalized echo density (Abel and
// Huang, AES 121, 2006; 1 for a dense tail) 0.10 at 50 ms and 0.35 at
// 100 ms, reaching 1 only after 300 ms. Diffused, it is 0.7 at 50 ms and 1
// from 70 ms, with the same decay and spectrum.
struct Room
{
    static constexpr int lineCount = 8;
    static constexpr int capacity = 16384;
    // A power of two above the longest early tap, the right channel's 31.7 ms,
    // at the Performer's highest engine rate, 192 kHz.
    static constexpr int earlyCapacity = 8192;
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
    static constexpr int diffuserCount = 4;
    // A power of two above the longest diffuser, 12.7 ms, at 192 kHz.
    static constexpr int diffuserCapacity = 4096;
    std::array<std::array<float, diffuserCapacity>, diffuserCount> diffusers {};
    std::array<int, diffuserCount> diffuserLengths {};
    int diffuserPosition { 0 };

    void prepare(double sampleRate) noexcept
    {
        constexpr double diffuserMs[diffuserCount] { 4.771, 3.595, 12.735, 9.307 };
        for (int index = 0; index < diffuserCount; ++index)
            diffuserLengths[static_cast<std::size_t>(index)]
                = static_cast<int>(std::lround(diffuserMs[index] * 1.0e-3 * sampleRate));
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
        for (auto& diffuser : diffusers)
            diffuser.fill(0.0f);
        diffuserPosition = 0;
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
        double delayed = earlyAt(predelay);
        earlyPosition = (earlyPosition + 1) & (earlyCapacity - 1);
        constexpr double diffuserGains[diffuserCount] { 0.75, 0.75, 0.625, 0.625 };
        for (std::size_t index = 0; index < diffuserCount; ++index)
        {
            auto& line = diffusers[index];
            const double past = line[static_cast<std::size_t>(
                (diffuserPosition - diffuserLengths[index]) & (diffuserCapacity - 1))];
            const double fed = delayed + diffuserGains[index] * past;
            line[static_cast<std::size_t>(diffuserPosition)] = static_cast<float>(fed);
            delayed = past - diffuserGains[index] * fed;
        }
        diffuserPosition = (diffuserPosition + 1) & (diffuserCapacity - 1);

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
// of 16-75 (median about 35) below 1.5 kHz and 5-36 above 2 kHz. The anchors
// sit at that spread's lower quartile below 1 kHz and its median above
// 2 kHz, chosen by ear over 40-32 below and 18-12 above (Docs/decisions.md,
// 2026-10-28).
double woodQ(double frequency) noexcept
{
    constexpr double anchorsHz[] { 300.0, 1000.0, 2000.0, 10000.0 };
    constexpr double anchorsQ[] { 23.0, 23.0, 10.0, 9.0 };
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
        int roundTrip { 1 };  // samples, the nearest whole number
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
        // The last round trip's squared wave velocities, both planes, and
        // their sum: `energy` is their mean. A mean over exactly one period
        // holds no ripple at the partials' frequencies; a one-pole of that
        // time constant left a ripple at twice the pitch that, through the
        // glide, modulated the line at 2 f0 and pumped the fundamental (a
        // hard-plucked steel G3 kept its energy for seconds). A pluck enters
        // its wave's squares at once: the string is stretched from the
        // moment it is released, not a round trip later.
        std::array<float, delayCapacity> squares {};
        int squaresIndex { 0 };
        int roundTrip { 1 };
        double squaresSum { 0.0 };
        double tension { 0.0 };
        double impedance { 0.0 };
        double linearMass { 0.0 };
        double lever { 0.0 };  // bridge position, -1 bass end to +1 treble end
        // The bar's bending shapes where the string crosses it.
        std::array<double, maxBends> bend {};
        double energy { 0.0 };
        bool keyDown { false };
        bool sustained { false };
        bool plucked { false };   // played, and still sounding from that pluck
        bool raised { false };    // the sharping lever is up
        bool dampingDirty { true };
        int midiNote { -1 };      // its own note, lever down
        int ownerNote { -1 };     // the note that last plucked it

        // How the bar's coordinates move the string's end normal to the top:
        // the heave, the rock at its lever, no sway, and each bend where it
        // crosses the bar.
        BarVector normalPort() const noexcept
        {
            BarVector port {};
            port[0] = 1.0;
            port[1] = lever;
            for (std::size_t index = 0; index < maxBends; ++index)
                port[3 + index] = bend[index];
            return port;
        }
    };
    std::array<StringVoice, maxStrings> strings {};

    std::array<Mode, maxModes> modes {};
    int modeCount { 0 };
    ModeBank bank {};
    // The modes' state at the bridge, sum(pull s1): process() sums it as it
    // updates them, noteOn's preload makes it stale.
    BarVector modalPast {};
    bool modalPastStale { true };

    // The bridge junction (process() derives it): the immediate generalized
    // mobility Y0 of the modes, the strings' generalized impedance Zg, the
    // bridge bar's own bilinear impedance Zb (diagonal: its mass, and for
    // each bend its loss and stiffness) and solve = (I + Y0 (Zg + Zb))^-1.
    // massPast and springPast hold the mass's and the stiffness's states.
    BarMatrix immediateMobility {};
    BarMatrix stringImpedance {};
    BarVector massImpedance {}, lossImpedance {}, springImpedance {};
    BarMatrix solve {};
    BarVector massPast {}, springPast {};
    // The bar's bends in vacuo (setInstrument()); bends past bendCount are
    // left out and their coordinates stay still.
    std::array<double, maxBends> bendFrequency {};
    int bendCount { 0 };

    // The bar's own impedance on coordinate i at omega: its mass, and for
    // each bend its loss and stiffness, as the junction's are.
    Complex barImpedance(double omega, std::size_t i) const noexcept
    {
        const double mass = instrument.body.bridgeMass;
        if (i < 3)
            return Complex(0.0, omega * (i == 0 ? mass : mass / 3.0));
        const std::size_t bend = i - 3;
        if (static_cast<int>(bend) >= bendCount)
            return 0.0;
        const double bent = 2.0 * pi * bendFrequency[bend];
        return Complex(mass * bent / woodQ(bendFrequency[bend]), omega * mass - mass * bent * bent / omega);
    }

    double radiationScale { 0.0 };
    double previousLeft { 0.0 };
    double previousRight { 0.0 };
    Room room {};
    double outputGain { 1.0 };
    // The piezo's coupling into its preamp (prepare()), as a bilinear
    // one-pole high-pass: y = piezoScale (x - x[n-1]) + piezoPole y[n-1].
    double piezoGain { 0.0 };
    double piezoScale { 0.0 };
    double piezoPole { 0.0 };
    double piezoPreviousForce { 0.0 };
    double piezoPrevious { 0.0 };
    // The strip's compliance under the bar's mass (setInstrument()), a
    // biquad on the bar's heave force: b0 b1 b2 a1 a2, transposed direct
    // form II.
    std::array<double, 5> piezoMount { 1.0, 0.0, 0.0, 0.0, 0.0 };
    double piezoMountZ1 { 0.0 }, piezoMountZ2 { 0.0 };
    bool sustain { false };
    Random random {};
    PluckReport lastPluck {};
    bool idle { true };
    int quietSamples { 0 };
    int controlCountdown { 0 };
    BarVector lastGeneralizedVelocity {};
    std::array<double, delayCapacity> pluckWave {};

    // PlayingControls as the signal path uses them. The hands' damping as
    // decay rates (1/s, they add); the output gains glide to their targets
    // over about 10 ms (per sample), the bend at the control rate.
    double releaseRate { 0.0 };
    double sympathyRate { 0.0 };
    double muteRate { 0.0 };
    double bendTarget { 1.0 };
    double bend { 1.0 };
    // Room send, direct and crossed gain, volume, piezo blend.
    std::array<double, 5> outputTarget {};
    std::array<double, 5> output {};
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
    // The piezo is Aculele's pickup system (Acustra's documented chain): an
    // element of 0.2 V/N at 1.45 nF, 3 m of cable, the ESP Project 202
    // preamp at a gain of 1.62 and a 1 MOhm DI. At a lyre's bridge forces it
    // stays linear: its op-amps reach their limits at about 13 N, and a
    // glissando at full velocity on any factory patch presses at most 7.6 N.
    // Linear, the chain is flat but for its coupling capacitors, and only C3
    // into R5 || R6 (220 nF into 23.5 kOhm) lies above 2 Hz: a first-order
    // high-pass at 31 Hz. Its level is InstrumentSpec::piezoGain.
    {
        const double corner = 1.0 / (2.0 * pi * 23.5e3 * 220.0e-9);
        const double k = std::tan(pi * corner / sampleRate_);
        state_->piezoScale = 1.0 / (1.0 + k);
        state_->piezoPole = (1.0 - k) / (1.0 + k);
    }
    state_->bendSmoothing = 1.0 - std::exp(-controlPeriod / (0.01 * sampleRate_));
    if (stringCount_ > 0)
        setInstrument(state_->instrument);
}

namespace
{

// Plate mode (m along the strings, n across) of a simply supported shape,
// used for participations; frequencies use Warburton's clamped factors.
struct PlateMode
{
    int m { 1 };
    int n { 1 };
    double frequency { 0.0 };
    double mass { 0.0 };
    double volume { 0.0 };
    BarVector participation {};
    double multipoleLeft { 0.0 };
    double multipoleRight { 0.0 };
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

    // The bar bends. As a free-free Euler-Bernoulli beam its bends stand at
    // (beta / L)^2 sqrt(EI / mu); its section is solid and bridgeHeight tall,
    // so EI / mu = E h^2 / (12 rho) whatever its mass: from 790 Hz (then 2.2,
    // 4.3, 7.1, 10.6, 14.7 and 19.6 kHz) for the lyre harp's 24 cm maple
    // bar, 6.8 and 18.6 kHz for the gut lyre's 9 cm one. On a top this light
    // the bar is no rigid line even below its first bend: the top under it
    // bends with it, where a bar that only heaved and rocked spread every
    // string's force evenly along its length. So the junction carries each
    // bend below the top's highest mode as a coordinate beside the rigid
    // three. Fewer leave the top along the bar too free: the decay of a
    // string beside the main mode (the lyre harp's A4) moves with their
    // count until all are in. Each bend's modal mass is the bar's
    // (bendShape). Chosen by ear over the rigid bar (Docs/decisions.md,
    // 2026-10-28).
    const double barLength = (body.bridgeTo - body.bridgeFrom) * b;
    const double beamConstant = body.bridgeHeight
        * std::sqrt(body.bridgeWood.eAlong / (12.0 * body.bridgeWood.density));  // sqrt(EI / mu), m^2/s
    s.bendCount = 0;
    s.bendFrequency = {};
    for (int bend = 0; bend < maxBends; ++bend)
    {
        const double frequency = std::pow(bendBetas[static_cast<std::size_t>(bend)] / barLength, 2.0)
                               * beamConstant / (2.0 * pi);
        if (frequency >= topFrequencyLimit)
            break;
        s.bendFrequency[static_cast<std::size_t>(bend)] = frequency;
        s.bendCount = bend + 1;
    }
    constexpr int points = 64;
    std::array<std::array<double, points>, maxBends> barShapes {};
    for (int bend = 0; bend < s.bendCount; ++bend)
        for (int point = 0; point < points; ++point)
            barShapes[static_cast<std::size_t>(bend)][static_cast<std::size_t>(point)]
                = bendShape(bend, 2.0 * (point + 0.5) / points - 1.0);

    // Participations along the bridge bar: the rigid bar's heave, rock about
    // its middle, normalized so its ends move h +- r, and sway of its top,
    // where the strings cross it, as the bar tips about its foot; then its
    // bends.
    for (int index = 0; index < plateCount; ++index)
    {
        PlateMode& mode = plate[static_cast<std::size_t>(index)];
        const double kx = mode.m * pi / a;
        const double ky = mode.n * pi / b;
        double meanSine = 0.0, meanLeveredSine = 0.0;
        std::array<double, maxBends> meanBentSine {};
        for (int point = 0; point < points; ++point)
        {
            const double t = (point + 0.5) / points;
            const double y = y0 + t * (y1 - y0);
            const double u = 2.0 * t - 1.0;
            const double sine = std::sin(ky * y);
            meanSine += sine / points;
            meanLeveredSine += u * sine / points;
            for (int bend = 0; bend < s.bendCount; ++bend)
                meanBentSine[static_cast<std::size_t>(bend)]
                    += barShapes[static_cast<std::size_t>(bend)][static_cast<std::size_t>(point)] * sine / points;
        }
        mode.mass = 0.25 * plateMass;
        mode.participation[0] = std::sin(kx * bridgeX) * meanSine;
        mode.participation[1] = 3.0 * std::sin(kx * bridgeX) * meanLeveredSine;
        mode.participation[2] = body.bridgeHeight * kx * std::cos(kx * bridgeX) * meanSine;
        for (std::size_t bend = 0; bend < maxBends; ++bend)
            mode.participation[3 + bend] = std::sin(kx * bridgeX) * meanBentSine[bend];
        // A bar centred across the top meets every shape even about its
        // middle (odd n) or odd (even n), so the other half of the
        // participations vanish; exactly zero, process() can skip them.
        if (std::abs(body.bridgeFrom + body.bridgeTo - 1.0) < 1.0e-12)
            for (std::size_t c = 0; c < barCoordinates; ++c)
                if (evenCoordinate(c) != (mode.n % 2 == 1))
                    mode.participation[c] = 0.0;
        mode.volume = (mode.m % 2 == 1 && mode.n % 2 == 1)
            ? 4.0 * a * b / (mode.m * mode.n * pi * pi) : 0.0;
        // What a mode radiates besides its net volume: edge and corner
        // radiation below coincidence, all of it above (radiation efficiency
        // sigma; an equivalent volume velocity c sqrt(4 pi S sigma <v^2>) /
        // omega). Its sign and weight at each microphone are seeded draws: a
        // symmetric pair, each string placed by where it sits on the bar, was
        // heard against this and not chosen (Docs/decisions.md, 2026-10-28).
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

    // The air in the soundhole couples to every plate mode with a net volume
    // through the cavity's stiffness rho c^2 / V (a Helmholtz resonator on a
    // flexible wall: Christensen and Vistisen, JASA 68(3), 1980, generalised
    // to many plate modes). Solved as one symmetric eigenproblem.
    constexpr std::size_t coupledCapacity = 48;
    std::array<int, coupledCapacity> coupledIndex {};
    int coupledCount = 0;
    const bool hasHole = body.holeRadius > 0.0;
    for (int index = 0; index < plateCount && coupledCount < static_cast<int>(coupledCapacity) - 1; ++index)
        if (plate[static_cast<std::size_t>(index)].volume != 0.0
            && plate[static_cast<std::size_t>(index)].frequency < 4000.0)
            coupledIndex[static_cast<std::size_t>(coupledCount++)] = index;
    const double holeArea = hasHole ? pi * body.holeRadius * body.holeRadius : 0.0;
    // End corrections of a hole in a thin wall, flanged both sides.
    const double neckLength = h + 1.7 * body.holeRadius;
    const double plugMass = airDensity * holeArea * neckLength;
    const double cavityStiffness = airDensity * soundSpeed * soundSpeed / body.cavityVolume;
    const int systemSize = coupledCount + (hasHole ? 1 : 0);

    std::array<std::array<double, coupledCapacity>, coupledCapacity> system {};
    std::array<double, coupledCapacity> masses {};
    std::array<double, coupledCapacity> volumes {};
    for (int i = 0; i < coupledCount; ++i)
    {
        const PlateMode& mode = plate[static_cast<std::size_t>(coupledIndex[static_cast<std::size_t>(i)])];
        masses[static_cast<std::size_t>(i)] = mode.mass;
        volumes[static_cast<std::size_t>(i)] = mode.volume;
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
            double stiffness = cavityStiffness * volumes[si] * volumes[sj];
            if (i == j && i < coupledCount)
            {
                const double omega = 2.0 * pi * plate[static_cast<std::size_t>(coupledIndex[si])].frequency;
                stiffness += masses[si] * omega * omega;
            }
            system[si][sj] = stiffness / std::sqrt(masses[si] * masses[sj]);
        }
    std::array<std::array<double, coupledCapacity>, coupledCapacity> vectors {};
    std::array<double, coupledCapacity> values {};
    if (systemSize > 0)
        jacobiEigen(system, systemSize, vectors, values);

    s.modeCount = 0;
    const auto addMode = [&] (double frequency, double airShare,
                              const BarVector& participation,
                              double monopole, double left, double right)
    {
        if (!(frequency > 20.0) || frequency >= topFrequencyLimit || s.modeCount >= maxModes)
            return;
        Mode mode;
        mode.frequency = frequency;
        const double qWood = woodQ(frequency);
        mode.q = 1.0 / (airShare / airQ + (1.0 - airShare) / qWood);
        mode.participation = participation;
        mode.monopole = monopole;
        mode.airShare = airShare;
        mode.radiationLeft = monopole + left;
        mode.radiationRight = monopole + right;
        const double omega = 2.0 * pi * frequency;
        const double k = omega / std::tan(omega / (2.0 * fs));
        const double damping = omega / mode.q;
        const double d0 = k * k + damping * k + omega * omega;
        mode.b0 = k / d0;
        mode.a1 = 2.0 * (omega * omega - k * k) / d0;
        mode.a2 = (k * k - damping * k + omega * omega) / d0;
        s.modes[static_cast<std::size_t>(s.modeCount++)] = mode;
    };
    // The coupled modes, mass-normalized: psi = M^-1/2 chi.
    for (int j = 0; j < systemSize; ++j)
    {
        const auto sj = static_cast<std::size_t>(j);
        const double frequency = std::sqrt(std::max(0.0, values[sj])) / (2.0 * pi);
        BarVector participation {};
        double monopole = 0.0, left = 0.0, right = 0.0;
        for (int i = 0; i < coupledCount; ++i)
        {
            const auto si = static_cast<std::size_t>(i);
            const PlateMode& mode = plate[static_cast<std::size_t>(coupledIndex[si])];
            const double psi = vectors[si][sj] / std::sqrt(masses[si]);
            for (std::size_t c = 0; c < barCoordinates; ++c)
                participation[c] += psi * mode.participation[c];
            monopole += psi * mode.volume;
            left += psi * mode.multipoleLeft;
            right += psi * mode.multipoleRight;
        }
        double airShare = 0.0;
        if (hasHole)
        {
            const auto sa = static_cast<std::size_t>(coupledCount);
            const double chi = vectors[sa][sj];
            airShare = chi * chi;
            monopole += chi / std::sqrt(plugMass) * holeArea;
        }
        addMode(frequency, airShare, participation, monopole, left, right);
    }
    for (int index = 0; index < plateCount; ++index)
    {
        bool coupled = false;
        for (int i = 0; i < coupledCount; ++i)
            if (coupledIndex[static_cast<std::size_t>(i)] == index)
                coupled = true;
        if (coupled)
            continue;
        const PlateMode& mode = plate[static_cast<std::size_t>(index)];
        const double scale = 1.0 / std::sqrt(mode.mass);
        BarVector participation = mode.participation;
        for (double& value : participation)
            value *= scale;
        addMode(mode.frequency, 0.0, participation,
                mode.volume * scale, mode.multipoleLeft * scale, mode.multipoleRight * scale);
    }
    std::sort(s.modes.begin(), s.modes.begin() + s.modeCount,
              [] (const Mode& x, const Mode& y) { return x.frequency < y.frequency; });
    s.radiationScale = airDensity / (4.0 * pi * microphoneDistance) * fs;

    // ---- The strings ----------------------------------------------------
    const StringMaterial& material = instrument.material;
    // A sharping lever shortens the speaking length to raise the note a
    // semitone; the tension stays, so the string's impedance does too.
    const double leverRatio = std::pow(2.0, -1.0 / 12.0);
    // Strings tuned by hand each sit a few cents off, the same ones every
    // time this instrument is set up: the six strings of a lyre harp phrase
    // (Freesound 649925) spread by 4.1 cents about their mean, while
    // repeated notes on one string agree within half a cent.
    std::array<double, maxStrings> detune {};
    {
        std::uint64_t hash = 0xcbf29ce484222325ull;  // FNV-1a of the name
        for (const char* c = instrument.name; c && *c; ++c)
            hash = (hash ^ static_cast<unsigned char>(*c)) * 0x100000001b3ull;
        Random tuning;
        tuning.seed(hash);
        double mean = 0.0;
        for (int index = 0; index < stringCount_; ++index)
        {
            detune[static_cast<std::size_t>(index)] = 4.1 * tuning.gaussian();
            mean += detune[static_cast<std::size_t>(index)];
        }
        mean /= stringCount_;
        for (int index = 0; index < stringCount_; ++index)
            detune[static_cast<std::size_t>(index)] -= mean;
    }
    for (int index = 0; index < stringCount_; ++index)
    {
        State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
        voice = {};
        const StringSpec& spec = instrument.strings[static_cast<std::size_t>(index)];
        voice.midiNote = spec.midiNote;
        const double area = 0.25 * pi * spec.diameter * spec.diameter;
        voice.linearMass = material.density * area;
        const double waveSpeed = 2.0 * spec.length * 440.0
            * std::pow(2.0, (spec.midiNote - 69) / 12.0 + detune[static_cast<std::size_t>(index)] / 1200.0);
        voice.tension = voice.linearMass * waveSpeed * waveSpeed;
        voice.impedance = std::sqrt(voice.tension * voice.linearMass);
        const double secondMoment = pi * std::pow(spec.diameter, 4.0) / 64.0;
        const double modulus = material.youngModulus + material.modulusPerStress * voice.tension / area;
        const double bendingLoss = material.lossModulus / modulus;
        // Woodhouse and Lynch-Aird's air damping (Acta Acustica 105 (2019)
        // 516, Equation (13) with their (d + 0.2) correction, d in mm): a
        // loss factor (d + 0.2) 8 sqrt(2) rho_a / (rho d) sqrt(nu / (2 pi f)),
        // so a decay rate pi f eta = airRoot sqrt(f) that rises with the
        // partial. Valette's law (below) gives every partial the
        // fundamental's rate.
        const double airRoot = material.measuredAirDamping
            ? pi * (spec.diameter * 1.0e3 + 0.2) * 8.0 * std::sqrt(2.0) * airDensity
                / (material.density * spec.diameter) * std::sqrt(airViscosity / airDensity / (2.0 * pi))
            : 0.0;
        // Strings sit evenly across the bridge, bass at the u = -1 end.
        voice.lever = stringCount_ > 1
            ? -0.9 + 1.8 * index / (stringCount_ - 1) : 0.0;
        for (int bend = 0; bend < s.bendCount; ++bend)
            voice.bend[static_cast<std::size_t>(bend)] = bendShape(bend, voice.lever);

        for (int raised = 0; raised < 2; ++raised)
        {
            State::PitchDesign& pitch = voice.pitch[static_cast<std::size_t>(raised)];
            pitch.length = raised ? spec.length * leverRatio : spec.length;
            const double frequency = waveSpeed / (2.0 * pitch.length);
            pitch.inharmonicity = pi * pi * modulus * secondMoment
                                / (voice.tension * pitch.length * pitch.length);
            // The plane parallel to the soundboard is longer by about one
            // string diameter (Woodhouse, Acta Acustica 90 (2004) 945, Sec.
            // 4.3; Acustra ships the same end correction).
            pitch.parallelLength = pitch.length + spec.diameter;
            pitch.glidePerEnergy = modulus * area
                                 / (2.0 * voice.tension * waveSpeed * waveSpeed);
            // Air damping (Valette, in Mechanics of Musical Instruments,
            // 1995): R = 2 pi eta + 2 pi d sqrt(pi eta rho f), decay rate
            // R / (2 mu).
            const double airResistance = 2.0 * pi * airViscosity
                + 2.0 * pi * spec.diameter * std::sqrt(pi * airViscosity * airDensity * frequency);
            const double airDecay = material.measuredAirDamping ? airRoot * std::sqrt(frequency)
                                                                : airResistance / (2.0 * voice.linearMass);
            const double intrinsicDecay = airDecay
                + pi * frequency * material.constantLoss
                - std::log(yokeRoundTripGain) * frequency;
            pitch.intrinsicT60 = 3.0 * std::log(10.0) / intrinsicDecay;
            pitch.roundTrip = std::clamp(static_cast<int>(std::lround(fs / frequency)), 1, delayCapacity - 1);

            // The parallel plane, 0.1 to 0.3% lower, takes the normal
            // plane's dispersion: fitted on its own it settled no closer (the
            // steel G3's partials to the 21st 7.0 cents out at worst, against
            // 5.5 shared) and doubled the design time.
            DispersionDesign design {};
            for (int plane = 0; plane < 2; ++plane)
            {
                LoopDesign& loop = pitch.planes[static_cast<std::size_t>(plane)];
                const double f0 = plane == 0 ? frequency : frequency * pitch.length / pitch.parallelLength;
                loop.frequency = f0;
                loop.gain = std::exp(-airDecay / f0) * yokeRoundTripGain;
                loop.constantDesign = constantLossSection(material.constantLoss, airRoot,
                                                          pitch.inharmonicity, f0, fs);
                loop.bendingDesign = bendingLossSection(bendingLoss,
                                                        pitch.inharmonicity, f0, fs);
                // Dispersion (designDispersion), the line then solved so the
                // fundamental is exact once the junction exists and the
                // bridge's reflection phase is known (tuning below).
                if (plane == 0)
                    design = designDispersion(f0, pitch.inharmonicity, fs);
                loop.dispersionStages = design.stages;
                loop.dispersion = design.coefficient;
                loop.dispersionPairs = design.pairs;
                loop.pair = design.pair;
                loop.dispersionCents = design.worstCents;
                loop.loopDelay = fs / f0;
            }
        }
    }

    // ---- The junction ---------------------------------------------------
    BarMatrix y0m {};
    for (int index = 0; index < s.modeCount; ++index)
    {
        const Mode& mode = s.modes[static_cast<std::size_t>(index)];
        for (std::size_t i = 0; i < barCoordinates; ++i)
            for (std::size_t j = 0; j < barCoordinates; ++j)
                y0m.m[i][j] += mode.b0 * mode.participation[i] * mode.participation[j];
    }
    s.immediateMobility = y0m;
    ModeBank& bank = s.bank;
    bank = {};
    const auto meets = [] (const Mode& mode, bool even)
    {
        for (std::size_t c = 0; c < barCoordinates; ++c)
            if (evenCoordinate(c) == even && mode.participation[c] != 0.0)
                return true;
        return false;
    };
    for (int part = 0; part < 3; ++part)
    {
        for (int index = 0; index < s.modeCount; ++index)
        {
            const Mode& mode = s.modes[static_cast<std::size_t>(index)];
            const bool even = meets(mode, true), odd = meets(mode, false);
            if (part != (!odd ? 0 : !even ? 1 : 2))
                continue;
            const auto k = static_cast<std::size_t>(bank.count++);
            for (std::size_t c = 0; c < barCoordinates; ++c)
            {
                bank.drive[c][k] = mode.b0 * mode.participation[c];
                bank.pull[c][k] = mode.participation[c];
            }
            bank.a1[k] = mode.a1;
            bank.a2[k] = mode.a2;
            bank.left[k] = mode.radiationLeft;
            bank.right[k] = mode.radiationRight;
        }
        bank.count = (bank.count + ModeBank::lanes - 1) / ModeBank::lanes * ModeBank::lanes;
        (part == 0 ? bank.evenEnd : bank.oddEnd) = bank.count;
    }
    BarMatrix zg {};
    for (int index = 0; index < stringCount_; ++index)
    {
        const State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
        const BarVector port = voice.normalPort();
        for (std::size_t i = 0; i < barCoordinates; ++i)
            for (std::size_t j = 0; j < barCoordinates; ++j)
                zg.m[i][j] += voice.impedance * port[i] * port[j];
        zg.m[2][2] += voice.impedance;
    }
    s.stringImpedance = zg;
    // The bar's mass: heave m, rock m/3 (r is its ends' displacement), sway
    // m/3 (its top as it tips about its foot), each bend m. Bilinear,
    // Z = m 2 fs. Each bend is a mass on its spring, k = m w^2, with the
    // wood's loss (woodQ), bilinear prewarped at its frequency so that it
    // stands where the beam does at every rate.
    s.massImpedance = {};
    s.lossImpedance = {};
    s.springImpedance = {};
    s.massImpedance[0] = body.bridgeMass * 2.0 * fs;
    s.massImpedance[1] = s.massImpedance[2] = body.bridgeMass / 3.0 * 2.0 * fs;
    for (int bend = 0; bend < s.bendCount; ++bend)
    {
        const auto c = static_cast<std::size_t>(3 + bend);
        const double omega = 2.0 * pi * s.bendFrequency[static_cast<std::size_t>(bend)];
        const double warp = omega / std::tan(omega / (2.0 * fs));
        s.massImpedance[c] = body.bridgeMass * warp;
        s.lossImpedance[c] = body.bridgeMass * omega / woodQ(s.bendFrequency[static_cast<std::size_t>(bend)]);
        s.springImpedance[c] = body.bridgeMass * omega * omega / warp;
    }
    BarMatrix load = zg;
    for (std::size_t i = 0; i < barCoordinates; ++i)
        load.m[i][i] += s.massImpedance[i] + s.lossImpedance[i] + s.springImpedance[i];
    BarMatrix junction = BarMatrix::identity();
    const BarMatrix product = y0m * load;
    for (std::size_t i = 0; i < barCoordinates; ++i)
        for (std::size_t j = 0; j < barCoordinates; ++j)
            junction.m[i][j] += product.m[i][j];
    s.solve.m = inverse(junction.m);

    // ---- Tuning: each port's reflection phase from the continuous model --
    for (int index = 0; index < stringCount_; ++index)
    {
        State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
        for (int design = 0; design < 4; ++design)
        {
            const int plane = design & 1;
            LoopDesign& loop = voice.pitch[static_cast<std::size_t>(design >> 1)]
                                    .planes[static_cast<std::size_t>(plane)];
            const double f0 = loop.frequency;
            // The port's admittance with every other port loading the bar.
            const auto reflectionLead = [&] (double frequency)
            {
                const double omega = 2.0 * pi * frequency;
                std::array<std::array<Complex, barCoordinates>, barCoordinates> plateMobility {};
                for (int m = 0; m < s.modeCount; ++m)
                {
                    const Mode& mode = s.modes[static_cast<std::size_t>(m)];
                    const double wm = 2.0 * pi * mode.frequency;
                    const Complex response = Complex(0.0, omega)
                        / Complex(wm * wm - omega * omega, omega * wm / mode.q);
                    for (std::size_t i = 0; i < barCoordinates; ++i)
                        for (std::size_t j = 0; j < barCoordinates; ++j)
                            plateMobility[i][j] += response * mode.participation[i] * mode.participation[j];
                }
                std::array<std::array<Complex, barCoordinates>, barCoordinates> others {};
                for (std::size_t i = 0; i < barCoordinates; ++i)
                    for (std::size_t j = 0; j < barCoordinates; ++j)
                        others[i][j] = zg.m[i][j];
                BarVector port {};
                port[2] = 1.0;
                if (plane == 0)
                    port = voice.normalPort();
                for (std::size_t i = 0; i < barCoordinates; ++i)
                    for (std::size_t j = 0; j < barCoordinates; ++j)
                        others[i][j] -= voice.impedance * port[i] * port[j];
                for (std::size_t i = 0; i < barCoordinates; ++i)
                    others[i][i] += s.barImpedance(omega, i);
                // Y_port = e^T (I + Yp W)^-1 Yp e.
                std::array<std::array<Complex, barCoordinates>, barCoordinates> system {};
                for (std::size_t i = 0; i < barCoordinates; ++i)
                    for (std::size_t j = 0; j < barCoordinates; ++j)
                    {
                        Complex sum = i == j ? 1.0 : 0.0;
                        for (std::size_t k = 0; k < barCoordinates; ++k)
                            sum += plateMobility[i][k] * others[k][j];
                        system[i][j] = sum;
                    }
                const auto inverted = inverse(system);
                std::array<Complex, barCoordinates> ype {};
                for (std::size_t i = 0; i < barCoordinates; ++i)
                    for (std::size_t k = 0; k < barCoordinates; ++k)
                        ype[i] += plateMobility[i][k] * port[k];
                Complex admittance = 0.0;
                for (std::size_t i = 0; i < barCoordinates; ++i)
                {
                    Complex row = 0.0;
                    for (std::size_t k = 0; k < barCoordinates; ++k)
                        row += inverted[i][k] * ype[k];
                    admittance += port[i] * row;
                }
                const Complex zy = voice.impedance * admittance;
                // R = (ZY - 1)/(1 + ZY); the loop carries -R.
                return std::arg((1.0 - zy) / (1.0 + zy));
            };
            const auto lagAt = [&] (double omega, double lineDelay)
            {
                const int whole = wholeFor(lineDelay);
                const double fraction = lineDelay - whole;
                double t1 = 0.0, t2 = 0.0;
                thiranCoefficients(fraction, t1, t2);
                return omega * whole + allpassLag(2, t1, t2, omega) + loop.filterLag(omega)
                     - reflectionLead(omega * fs / (2.0 * pi));
            };
            const auto solveLine = [&] (double omega, double cycles)
            {
                double lower = 6.0, upper = delayCapacity - 16.0;
                for (int iteration = 0; iteration < 60; ++iteration)
                {
                    const double middle = 0.5 * (lower + upper);
                    if (lagAt(omega, middle) < 2.0 * pi * cycles)
                        lower = middle;
                    else
                        upper = middle;
                }
                return 0.5 * (lower + upper);
            };
            const double omega0 = 2.0 * pi * f0 / fs;
            loop.restLineDelay = solveLine(omega0, 1.0);
            for (std::size_t k = 0; k < bendRatios.size(); ++k)
            {
                const double omega = omega0 * bendRatios[k];
                loop.lagCycles[k] = (lagAt(omega, loop.restLineDelay) - omega * loop.restLineDelay) / (2.0 * pi);
            }
        }
        voice.normal.whole = wholeFor(voice.pitch[0].planes[0].restLineDelay);
        voice.parallel.whole = wholeFor(voice.pitch[0].planes[1].restLineDelay);
        applyPitch(index, false);
    }

    // The piezo strip is no rigid seat: the bar's mass rides on the element,
    // so the force the element feels falls away above their resonance.
    // Acustra's element is the sibling evidence: a 3.8 g saddle on it rings
    // at 6 kHz (the documented 5-7 kHz), so the element and its seat are
    // 3.8e-3 (2 pi 6000)^2 / 0.072 = 7.5e7 N/m per metre of its 72 mm saddle,
    // with a loss factor of 1/18 (Acustra's PiezoDesign; Zollner, Physics of
    // the Electric Guitar, 2005, ch. 6). Here the strip runs the bar's length
    // under the bar's mass: 8.7 kHz on the lyre harp, 6.5 kHz on the gut
    // lyre. As in Acustra, F_p / F_r = Zk Q / (1 + Zk Q), Zk = k/s + c,
    // Q = 1/(s M + SZ) + G, with SZ the strings' summed wave impedance and G
    // the top's heave conductance under the bar, the mean of Re(Y) within a
    // sixth of the resonance (as Acustra's 5-7 kHz about 6). Bilinear,
    // prewarped at the resonance.
    {
        const double length = (body.bridgeTo - body.bridgeFrom) * b;
        const double stiffness = 3.8e-3 * std::pow(2.0 * pi * 6000.0, 2.0) / 0.072 * length;
        const double mass = body.bridgeMass;
        const double omega0 = std::sqrt(stiffness / mass);
        const double loss = stiffness / (18.0 * omega0);
        double impedance = 0.0;
        for (int index = 0; index < stringCount_; ++index)
            impedance += s.strings[static_cast<std::size_t>(index)].impedance;
        constexpr int points = 64;
        double g = 0.0;
        for (int point = 0; point < points; ++point)
        {
            const double omega = omega0 * (5.0 / 6.0 + (point + 0.5) / (3.0 * points));
            for (int m = 0; m < s.modeCount; ++m)
            {
                const Mode& mode = s.modes[static_cast<std::size_t>(m)];
                const double wm = 2.0 * pi * mode.frequency;
                const Complex response = Complex(0.0, omega) / Complex(wm * wm - omega * omega, omega * wm / mode.q);
                g += response.real() * mode.participation[0] * mode.participation[0] / points;
            }
        }
        const double n2 = loss * g * mass;
        const double n1 = loss * (1.0 + g * impedance) + stiffness * g * mass;
        const double n0 = stiffness * (1.0 + g * impedance);
        const double d2 = n2 + mass;
        const double d1 = n1 + impedance;
        const double d0 = n0;
        const double k = omega0 / std::tan(std::min(0.49 * pi, 0.5 * omega0 / fs));
        const double a0 = d2 * k * k + d1 * k + d0;
        s.piezoMount = { (n2 * k * k + n1 * k + n0) / a0, 2.0 * (n0 - n2 * k * k) / a0,
                         (n2 * k * k - n1 * k + n0) / a0, 2.0 * (d0 - d2 * k * k) / a0,
                         (d2 * k * k - d1 * k + d0) / a0 };
    }

    s.outputGain = instrument.outputGain;
    s.piezoGain = instrument.piezoGain;
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
        voice.squares.fill(0.0f);
        voice.squaresSum = 0.0;
        voice.energy = 0.0;
        voice.keyDown = voice.sustained = voice.plucked = false;
        voice.ownerNote = -1;
        voice.dampingDirty = true;
    }
    s.bank.s1.fill(0.0);
    s.bank.s2.fill(0.0);
    s.modalPastStale = true;
    s.massPast = {};
    s.springPast = {};
    s.previousLeft = s.previousRight = 0.0;
    s.piezoPreviousForce = s.piezoPrevious = 0.0;
    s.piezoMountZ1 = s.piezoMountZ2 = 0.0;
    s.room.reset();
    s.sustain = false;
    s.idle = true;
    s.quietSamples = 0;
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
// The engine sleeps once its output, before the volume and with the room's
// whole tail, has stayed below -170 dBFS for half a second and every
// string's energy is below one where a ringing string radiates about that.
constexpr double idleLevel = 3.0e-9;
constexpr double idleEnergy = 1.0e-12;
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
    // The new round trip's window over the squares already held.
    voice.roundTrip = pitch.roundTrip;
    voice.squaresSum = 0.0;
    for (int k = 1; k <= voice.roundTrip; ++k)
        voice.squaresSum += voice.squares[static_cast<std::size_t>((voice.squaresIndex - k) & (delayCapacity - 1))];
    voice.energy = voice.squaresSum / voice.roundTrip;
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
    c.piezo = unitClamp(c.piezo);
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
    s.outputTarget = { 0.5 * c.room, 0.5 * (1.0 + c.width), 0.5 * (1.0 - c.width), c.volume, c.piezo };
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
    if (s.idle)
    {
        s.idle = false;
        s.controlCountdown = 0;
    }
    s.quietSamples = 0;
    State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
    const InstrumentSpec& instrument = s.instrument;
    const PlayingControls& controls = controls_;
    const double v = std::clamp(static_cast<double>(velocity), 0.0, 1.0);

    // A finger meeting a sounding string stops it before it lets go: a
    // harpist's finger holds the string 56 to 431 ms (Chadefaux et al., JASA
    // 131 (2012) 844, Table II), several times the 10 ms in which a finger's
    // contact silences a plucked string (Laurson et al., CMJ 25(3) 2001). A
    // plectrum glances off and stops most of it (Acustra's picking-hand
    // contact, as one step). The finger's 10 ms are slow against the body's
    // periods, so the body takes the force the stopped wave leaves as the
    // held force below: preloaded, without the click of a step.
    BarVector preload {};
    const BarVector port = voice.normalPort();
    if (voice.keyDown || voice.sustained || voice.energy > silentEnergy)
    {
        const double stopped = controls.technique == Technique::Plectrum ? 0.7 : 1.0;
        const double normalForce = 2.0 * voice.impedance * stopped * voice.normal.last;
        for (std::size_t c = 0; c < barCoordinates; ++c)
            preload[c] -= normalForce * port[c];
        preload[2] -= 2.0 * voice.impedance * stopped * voice.parallel.last;
        for (Loop* loop : { &voice.normal, &voice.parallel })
        {
            if (controls.technique != Technique::Plectrum)
            {
                loop->silence();
                continue;
            }
            for (float& sample : loop->line)
                sample *= 0.3f;
            for (float& sample : loop->pending)
                sample *= 0.3f;
        }
    }
    if (voice.keyDown || voice.sustained || voice.energy > silentEnergy)
    {
        const float kept = controls.technique == Technique::Plectrum ? 0.09f : 0.0f;
        for (float& square : voice.squares)
            square *= kept;
    }
    // The other hand sets the lever before the pluck.
    if (voice.raised != raise)
        applyPitch(index, raise);

    // Each pluck differs as a harpist's do (Chadefaux et al., JASA 131 (2012)
    // 844). Where the finger meets the string: Fig. 7's spread of the pluck
    // point over the string's length, 0.012 on a 17.6 cm string to 0.022 on
    // a 97.5 cm one, pooled over nine harpists and so at most one player's.
    // How hard: one player's string displacement at release spreads by about
    // 14% (Fig. 8, 95% intervals of about 15% over five to seven plucks).
    // Which way: the angle the string leaves at, measured from the strings'
    // plane (on a lyre, the top's), by about 4 degrees (Table VI, median 95%
    // interval 5 degrees over five plucks), around the angle the normal
    // share sets: the share is the angle's sine squared.
    const double positionSpread = std::clamp(0.012 + 0.010 * (voice.length - 0.176) / (0.975 - 0.176),
                                             0.012, 0.022);
    const double position = std::clamp(controls.pluckPosition + positionSpread * s.random.gaussian(), 0.05, 0.7);
    // The player evens the scale by what is heard: the microphones, the
    // piezo, or between the two as the blend sets.
    const StringSpec& string = instrument.strings[static_cast<std::size_t>(index)];
    const double touch = std::pow(string.touch, 1.0 - controls.piezo) * std::pow(string.piezoTouch, controls.piezo);
    const double force = instrument.pluckForce * touch * std::pow(v, 1.4) * std::exp(0.14 * s.random.gaussian());
    const double angle = std::asin(std::sqrt(std::clamp(controls.normalShare, 0.0, 1.0)))
                       + 4.0 * pi / 180.0 * s.random.gaussian();
    const double normalShare = std::clamp(std::sin(angle) * std::sin(angle), 0.01, 0.9);

    // Contact width and release time of the technique: the fingertip's pad,
    // a nail's or a plectrum's edge. A harpist's string slips 0.04 mm along
    // the harder fingertip before it leaves at about 1.5 m/s (Chadefaux, Le
    // Carrou and Fabre, JASA 133 (2013) 2444); accelerating evenly, that
    // takes 0.053 ms, the finger's release time constant. Chosen by ear over
    // the pad's 0.2 mm (0.27 ms) and an authored 0.10 ms (Docs/decisions.md,
    // 2026-10-28). The nail's and the plectrum's are authored, of the order
    // of Acustra's (a 0.2 mm edge over a metre or two per second).
    double contact = 0.005, release = 0.053e-3;
    switch (controls.technique)
    {
        case Technique::Finger: contact = 0.005; release = 0.053e-3; break;
        case Technique::Nail: contact = 0.0012; release = 0.035e-3; break;
        case Technique::Plectrum: contact = 0.0008; release = 0.025e-3; break;
    }
    // A harder stroke slips off faster, so it leaves brighter. Harpists'
    // release velocity rises with the string's displacement, about as D^1.2
    // from 0.5 mm at 0.2 m/s to 7 mm at 5 m/s around an average of 4 mm at
    // 2 m/s (Chadefaux et al., JASA 131 (2012) 844, Sec. III.B.1), and the
    // displacement follows the force, v^1.4 here: the release time goes as
    // v^-1.68 about v = 0.5, within the 0.2-5 m/s the harpists reached.
    // Chosen by ear over a gentler authored law (Docs/decisions.md,
    // 2026-10-28). The slip's speed also scatters more than the displacement
    // does: one harpist's release velocity by about 25%, the displacement by
    // 14% (Fig. 8), so the release time takes the rest, about 20%, and each
    // pluck's brightness with it.
    release *= std::clamp(std::pow(v / 0.5, -1.4 * 1.2), 0.4, 10.0)
             * std::exp(0.2 * s.random.gaussian());
    s.lastPluck = { position, force, normalShare, release };

    voice.keyDown = true;
    voice.sustained = false;
    voice.plucked = true;
    voice.ownerNote = midiNote;
    updateDamping(index);

    // The loop's period as the string is released: the tension glide its
    // stretch sets at once (what a re-pluck kept, plus the released
    // triangle's mean squared wave velocity, (c/2)^2 A^2 (1/x + 1/(L - x)) / L
    // in each plane) and the bend. The wave is laid over that period, so its
    // first round trip meets its own return.
    double releasedEnergy = 0.0;
    for (int k = 1; k <= voice.roundTrip; ++k)
        releasedEnergy += voice.squares[static_cast<std::size_t>((voice.squaresIndex - k) & (delayCapacity - 1))];
    releasedEnergy /= voice.roundTrip;
    for (int plane = 0; plane < 2; ++plane)
    {
        const double length = plane == 0 ? voice.length : voice.parallelLength;
        const double frequency = plane == 0 ? voice.frequency : voice.parallelFrequency;
        const double pluckPoint = position * length;
        const double amplitude = force * pluckPoint * (length - pluckPoint) / (voice.tension * length)
                               * std::sqrt(plane == 0 ? normalShare : 1.0 - normalShare);
        releasedEnergy += length * length * frequency * frequency * amplitude * amplitude
                        * (1.0 / pluckPoint + 1.0 / (length - pluckPoint)) / length;
    }
    const double releasedRatio = (tensionModulation_ ? 1.0 + voice.glidePerEnergy * releasedEnergy : 1.0) * s.bend;

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
        const double period = loop.loopDelay / releasedRatio;
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
            if (k < voice.roundTrip)
            {
                float& square = voice.squares[static_cast<std::size_t>((voice.squaresIndex - voice.roundTrip + k)
                                                                       & (delayCapacity - 1))];
                square += static_cast<float>(value * value);
            }
        }
        // The held force stood on the bridge before the release: the body
        // starts deflected under it, so only what the release changes
        // reaches it, and the top springs back as the wave's mean force
        // falls to zero.
        const double heldForce = 2.0 * voice.impedance * (wave[0] - mean);
        if (plane == 0)
        {
            for (std::size_t c = 0; c < barCoordinates; ++c)
                preload[c] += heldForce * port[c];
        }
        else
        {
            preload[2] += heldForce;
        }
    }
    voice.squaresSum = 0.0;
    for (int k = 1; k <= voice.roundTrip; ++k)
        voice.squaresSum += voice.squares[static_cast<std::size_t>((voice.squaresIndex - k) & (delayCapacity - 1))];
    voice.energy = voice.squaresSum / voice.roundTrip;
    ModeBank& bank = s.bank;
    for (int k = 0; k < bank.count; ++k)
    {
        const auto m = static_cast<std::size_t>(k);
        double modal = 0.0;
        for (std::size_t c = 0; c < barCoordinates; ++c)
            modal += bank.drive[c][m] * preload[c];
        bank.s1[m] -= modal;
        bank.s2[m] -= modal;
    }
    s.modalPastStale = true;
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

void AculyreEngine::sleep() noexcept
{
    State& s = *state_;
    for (int index = 0; index < stringCount_; ++index)
    {
        auto& voice = s.strings[static_cast<std::size_t>(index)];
        voice.normal.silence();
        voice.parallel.silence();
        voice.squares.fill(0.0f);
        voice.squaresSum = 0.0;
        voice.energy = 0.0;
        if (voice.plucked)
        {
            voice.plucked = false;
            voice.dampingDirty = true;
        }
    }
    s.bank.s1.fill(0.0);
    s.bank.s2.fill(0.0);
    s.modalPastStale = true;
    s.massPast = {};
    s.springPast = {};
    s.lastGeneralizedVelocity = {};
    s.previousLeft = s.previousRight = 0.0;
    s.piezoPreviousForce = s.piezoPrevious = 0.0;
    s.piezoMountZ1 = s.piezoMountZ2 = 0.0;
    s.room.reset();
    s.idle = true;
}

bool AculyreEngine::idle() const noexcept { return state_->idle; }

void AculyreEngine::process(float* left, float* right, int numSamples) noexcept
{
    State& s = *state_;
    if (s.idle)
    {
        std::fill(left, left + numSamples, 0.0f);
        std::fill(right, right + numSamples, 0.0f);
        s.output = s.outputTarget;
        s.bend = s.bendTarget;
        return;
    }
    const int count = stringCount_;
    std::array<double, maxStrings> normalIn {};
    std::array<double, maxStrings> parallelIn {};
    const BarMatrix& y0 = s.immediateMobility;
    const BarMatrix& zg = s.stringImpedance;
    const BarMatrix& solve = s.solve;

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
                // A held key or pedal does not keep a string sounding: a
                // string that has died away under it lets the engine sleep.
                if (voice.energy > idleEnergy)
                    s.quietSamples = 0;
                // Tension modulation (Kirchhoff-Carrier): the string's mean
                // squared slope stretches it, Delta f / f = EA <y'^2> / (4T);
                // the pitch bend scales every string alike.
                const double ratio = (tensionModulation_
                    ? 1.0 + voice.glidePerEnergy * voice.energy : 1.0) * s.bend;
                for (Loop* loop : { &voice.normal, &voice.parallel })
                    loop->setLineDelay(loop->lineDelayAt(ratio));
                // The mean follows the period the bend and the glide set.
                const int roundTrip = std::clamp(static_cast<int>(std::lround(sampleRate_ / (voice.frequency * ratio))),
                                                 1, delayCapacity - 1);
                while (voice.roundTrip < roundTrip)
                {
                    ++voice.roundTrip;
                    voice.squaresSum += voice.squares[static_cast<std::size_t>((voice.squaresIndex - voice.roundTrip)
                                                                               & (delayCapacity - 1))];
                }
                while (voice.roundTrip > roundTrip)
                {
                    voice.squaresSum = std::max(0.0, voice.squaresSum
                        - voice.squares[static_cast<std::size_t>((voice.squaresIndex - voice.roundTrip)
                                                                 & (delayCapacity - 1))]);
                    --voice.roundTrip;
                }
            }
        }

        BarVector drive {};
        for (int index = 0; index < count; ++index)
        {
            State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
            const double vn = voice.normal.read();
            const double vp = voice.parallel.read();
            normalIn[static_cast<std::size_t>(index)] = vn;
            parallelIn[static_cast<std::size_t>(index)] = vp;
            const double twiceZ = 2.0 * voice.impedance;
            drive[0] += twiceZ * vn;
            drive[1] += twiceZ * voice.lever * vn;
            drive[2] += twiceZ * vp;
            for (std::size_t bend = 0; bend < maxBends; ++bend)
                drive[3 + bend] += twiceZ * voice.bend[bend] * vn;
            // The loop's mean squared velocity over a round trip; both
            // directions' slopes count, hence the factor in glidePerEnergy.
            const float square = static_cast<float>(vn * vn + vp * vp);
            float& slot = voice.squares[static_cast<std::size_t>(voice.squaresIndex)];
            const float leaving = voice.squares[static_cast<std::size_t>((voice.squaresIndex - voice.roundTrip)
                                                                         & (delayCapacity - 1))];
            voice.squaresSum = std::max(0.0, voice.squaresSum + static_cast<double>(square) - leaving);
            slot = square;
            voice.squaresIndex = (voice.squaresIndex + 1) & (delayCapacity - 1);
            voice.energy = voice.squaresSum / voice.roundTrip;
        }

        ModeBank& bank = s.bank;
        if (s.modalPastStale)
        {
            s.modalPast = {};
            for (std::size_t c = 0; c < barCoordinates; ++c)
                for (int k = 0; k < bank.count; ++k)
                    s.modalPast[c] += bank.pull[c][static_cast<std::size_t>(k)] * bank.s1[static_cast<std::size_t>(k)];
            s.modalPastStale = false;
        }
        const BarVector& past = s.modalPast;
        BarVector net {};
        for (std::size_t i = 0; i < barCoordinates; ++i)
            net[i] = drive[i] - s.massPast[i] - s.springPast[i];
        BarVector rhs = y0.apply(net);
        for (std::size_t i = 0; i < barCoordinates; ++i)
            rhs[i] += past[i];
        const BarVector velocity = solve.apply(rhs);
        const BarVector stringLoad = zg.apply(velocity);
        BarVector plateForce {};
        for (std::size_t i = 0; i < barCoordinates; ++i)
        {
            const double massForce = s.massImpedance[i] * velocity[i] + s.massPast[i];
            const double springForce = s.springImpedance[i] * velocity[i] + s.springPast[i];
            plateForce[i] = drive[i] - stringLoad[i] - massForce - springForce - s.lossImpedance[i] * velocity[i];
            s.massPast[i] = -massForce - s.massImpedance[i] * velocity[i];
            s.springPast[i] = springForce + s.springImpedance[i] * velocity[i];
        }

        constexpr int lanes = ModeBank::lanes;
        double laneLeft[lanes] {}, laneRight[lanes] {}, lanePast[barCoordinates][lanes] {};
        runModes(bank, 0, bank.evenEnd, plateForce, laneLeft, laneRight, lanePast, EvenCoordinates {});
        runModes(bank, bank.evenEnd, bank.oddEnd, plateForce, laneLeft, laneRight, lanePast, OddCoordinates {});
        runModes(bank, bank.oddEnd, bank.count, plateForce, laneLeft, laneRight, lanePast,
                 std::make_index_sequence<barCoordinates> {});
        double radiatedLeft = 0.0, radiatedRight = 0.0;
        s.modalPast = {};
        for (int lane = 0; lane < lanes; ++lane)
        {
            radiatedLeft += laneLeft[lane];
            radiatedRight += laneRight[lane];
            for (std::size_t c = 0; c < barCoordinates; ++c)
                s.modalPast[c] += lanePast[c][lane];
        }

        for (int index = 0; index < count; ++index)
        {
            State::StringVoice& voice = s.strings[static_cast<std::size_t>(index)];
            double normalPort = velocity[0] + voice.lever * velocity[1];
            for (std::size_t bend = 0; bend < maxBends; ++bend)
                normalPort += voice.bend[bend] * velocity[3 + bend];
            voice.normal.writeSample(normalIn[static_cast<std::size_t>(index)] - normalPort);
            voice.parallel.writeSample(parallelIn[static_cast<std::size_t>(index)] - velocity[2]);
        }
        s.lastGeneralizedVelocity = velocity;

        // Far-field pressure: rho / (4 pi r) times the volume acceleration.
        const double pressureLeft = (radiatedLeft - s.previousLeft) * s.radiationScale;
        const double pressureRight = (radiatedRight - s.previousRight) * s.radiationScale;
        s.previousLeft = radiatedLeft;
        s.previousRight = radiatedRight;
        for (std::size_t i = 0; i < s.output.size(); ++i)
            s.output[i] += (s.outputTarget[i] - s.output[i]) * s.outputSmoothing;
        // The room runs even unsent, so turning it up meets a live tail.
        double wetLeft = 0.0, wetRight = 0.0;
        s.room.process(0.5 * (pressureLeft + pressureRight), wetLeft, wetRight);
        // The piezo strip under the bar presses with the bar's whole force on
        // the top, so it hears the heave and not the rock; it runs unheard
        // too, for the same reason as the room.
        const std::array<double, 5>& mount = s.piezoMount;
        const double element = mount[0] * plateForce[0] + s.piezoMountZ1;
        s.piezoMountZ1 = mount[1] * plateForce[0] - mount[3] * element + s.piezoMountZ2;
        s.piezoMountZ2 = mount[2] * plateForce[0] - mount[4] * element;
        const double piezo = s.piezoScale * (element - s.piezoPreviousForce) + s.piezoPole * s.piezoPrevious;
        s.piezoPreviousForce = element;
        s.piezoPrevious = piezo;
        const double level = std::max(s.outputGain * std::max(std::max(std::abs(pressureLeft), std::abs(pressureRight)),
                                                              std::max(std::abs(wetLeft), std::abs(wetRight))),
                                      s.piezoGain * std::abs(piezo));
        s.quietSamples = level > idleLevel ? 0 : s.quietSamples + 1;
        const double outLeft = pressureLeft + s.output[0] * wetLeft;
        const double outRight = pressureRight + s.output[0] * wetRight;
        const double micLeft = s.outputGain * (s.output[1] * outLeft + s.output[2] * outRight);
        const double micRight = s.outputGain * (s.output[1] * outRight + s.output[2] * outLeft);
        // Which way the element is wired is a choice: this way round, a
        // blend with the microphones adds in the low mids instead of
        // cancelling there.
        const double pickup = -s.piezoGain * piezo;
        const double blend = s.output[4];
        const double volume = s.output[3];
        // Unlimited: the Performer's output stage keeps it under full scale.
        // Written so that either end of the blend is exactly one source.
        left[sample] = static_cast<float>(volume * ((1.0 - blend) * micLeft + blend * pickup));
        right[sample] = static_cast<float>(volume * ((1.0 - blend) * micRight + blend * pickup));
        if (s.quietSamples >= static_cast<int>(0.5 * sampleRate_))
        {
            sleep();
            std::fill(left + sample + 1, left + numSamples, 0.0f);
            std::fill(right + sample + 1, right + numSamples, 0.0f);
            return;
        }
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
    report.dispersionPairs = voice.normal.dispersionPairs;
    report.dispersionCents = voice.normal.dispersionCents;
    return report;
}

AculyreEngine::PluckReport AculyreEngine::lastPluck() const noexcept { return state_->lastPluck; }

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
    report.heave = m.participation[0];
    report.monopole = m.monopole;
    report.airShare = m.airShare;
    return report;
}

double AculyreEngine::bridgeMobilityMagnitude(double frequency, double lever) const noexcept
{
    const State& s = *state_;
    const double omega = 2.0 * pi * frequency;
    std::array<std::array<Complex, barCoordinates>, barCoordinates> plateMobility {};
    for (int m = 0; m < s.modeCount; ++m)
    {
        const Mode& mode = s.modes[static_cast<std::size_t>(m)];
        const double wm = 2.0 * pi * mode.frequency;
        const Complex response = Complex(0.0, omega) / Complex(wm * wm - omega * omega, omega * wm / mode.q);
        // Sway held still, as the normal force drives the bar.
        for (std::size_t i = 0; i < barCoordinates; ++i)
            for (std::size_t j = 0; j < barCoordinates; ++j)
                if (i != 2 && j != 2)
                    plateMobility[i][j] += response * mode.participation[i] * mode.participation[j];
    }
    // The point on the bar at the lever, normal to the top; the bar's own
    // impedance in series: Y = e^T (I + Yp Zb)^-1 Yp e.
    BarVector point {};
    point[0] = 1.0;
    point[1] = lever;
    for (int bend = 0; bend < s.bendCount; ++bend)
        point[3 + static_cast<std::size_t>(bend)] = bendShape(bend, lever);
    std::array<std::array<Complex, barCoordinates>, barCoordinates> system {};
    for (std::size_t i = 0; i < barCoordinates; ++i)
        for (std::size_t j = 0; j < barCoordinates; ++j)
            system[i][j] = (i == j ? 1.0 : 0.0) + plateMobility[i][j] * s.barImpedance(omega, j);
    const auto inverted = inverse(system);
    Complex mobility = 0.0;
    for (std::size_t i = 0; i < barCoordinates; ++i)
        for (std::size_t k = 0; k < barCoordinates; ++k)
        {
            Complex ype = 0.0;
            for (std::size_t j = 0; j < barCoordinates; ++j)
                ype += plateMobility[k][j] * point[j];
            mobility += point[i] * inverted[i][k] * ype;
        }
    return std::abs(mobility);
}

} // namespace aculyre
