#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace aculyre::detail
{

// A passive point contact on the velocity-wave ring. Dispersion and tuning
// filters hold part of the round trip outside its explicit delay line. The
// usual long-arc taps x D/2 and (1-x/2) D therefore need not both fit in the
// line. Keep those physical taps whenever they fit. Otherwise, for an ideal
// standing partial, the shorter, mirror-equivalent arc
// min(x,1-x) D has the same sin^2(n pi x) participation. Centre that pair in
// the live line instead of clipping just one endpoint. D is the CURRENT
// fundamental period, including filter phase, not the line's sample count.
//
// This is a phase-equivalent contact approximation: a lumped dispersive loop
// cannot represent an exact distributed stiff-string contact. If even the
// shorter arc will not fit at an extreme bend/position, shorten it
// symmetrically to the available interval; the report exposes that limit.
struct StringContact
{
    double point { 0.27 }, period { 0.0 }, requestedSeparation { 0.0 };
    double outgoingAge { 0.0 }, returningAge { 0.0 }, share { 0.0 };
    double configuredFrequency { 0.0 }, configuredSampleRate { 0.0 };
    int latestAge { 0 }, configuredWhole { 0 };
    std::array<int, 4> ages {};
    // g samples the velocity difference between the two interpolated taps.
    std::array<double, 4> g {};

    void updateGeometry() noexcept
    {
        period = configuredSampleRate / configuredFrequency;
        latestAge = std::max(1, configuredWhole - 1);
        outgoingAge = 0.5 * point * period;
        returningAge = (1.0 - 0.5 * point) * period;
        requestedSeparation = returningAge - outgoingAge;
        if (outgoingAge < 1.0 || returningAge > latestAge)
        {
            requestedSeparation = std::min(point, 1.0 - point) * period;
            const double separation = std::min(requestedSeparation, static_cast<double>(latestAge - 1));
            const double middle = 0.5 * (1.0 + latestAge);
            outgoingAge = middle - 0.5 * separation;
            returningAge = middle + 0.5 * separation;
        }
    }

    StringContact withGeometry() const noexcept
    {
        if (share > 0.0 || !(configuredFrequency > 0.0))
            return *this;
        StringContact copy = *this;
        copy.updateGeometry();
        return copy;
    }

    void configure(double position, double rate, double frequency,
                   double sampleRate, int whole) noexcept
    {
        point = position;
        configuredFrequency = frequency;
        configuredSampleRate = sampleRate;
        configuredWhole = whole;
        // apply() reads the taps only under a hand; with none on the string
        // they and the period's division are skipped, and contactReport()
        // resolves the diagnostic geometry on demand (withGeometry()).
        if (!(rate > 0.0))
        {
            share = 0.0;
            return;
        }
        updateGeometry();
        share = std::min(0.25, rate / (2.0 * frequency));
        const auto tap = [&] (double age, int offset, double sign)
        {
            // Ages are never negative, so the cast is the floor (a library
            // call in Reason's build, four per loop every control period).
            const int first = static_cast<int>(age);
            const double fraction = age - first;
            ages[static_cast<std::size_t>(offset)] = first;
            ages[static_cast<std::size_t>(offset + 1)] = std::min(first + 1, latestAge);
            g[static_cast<std::size_t>(offset)] = sign * (1.0 - fraction);
            g[static_cast<std::size_t>(offset + 1)] = sign * fraction;
        };
        tap(outgoingAge, 0, -1.0);
        tap(returningAge, 2, 1.0);
    }

    void apply(float* line, int mask, int write) const noexcept
    {
        if (!(share > 0.0))
            return;
        double velocity = 0.0;
        for (std::size_t k = 0; k < ages.size(); ++k)
            velocity += g[k] * line[(write - 1 - ages[k]) & mask];
        // Use the adjoint of the interpolation for scattering too:
        // q' = q - share g (g^T q). Its squared-wave norm decreases by
        // share (2-share ||g||^2) (g^T q)^2 >= 0: ||g||^2 <= 2 and
        // share <= 1/4. Independent read/interpolate/write taps do not
        // provide this power guarantee. Overlapping taps add their weights.
        const double taken = share * velocity;
        for (std::size_t k = 0; k < ages.size(); ++k)
        {
            float& sample = line[(write - 1 - ages[k]) & mask];
            sample = static_cast<float>(sample - g[k] * taken);
        }
    }
};

} // namespace aculyre::detail
