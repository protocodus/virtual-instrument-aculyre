#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace aculyre::detail
{
// The periodic one-pole's value at the start of its period (below): the
// low middle's pull, relaxed toward high over the period's high ends.
inline double releasedStart(double period, double position, double high, double low, double tau) noexcept
{
    if (tau <= 0.0)
        return high;
    const double first = 0.5 * position * period;
    const double second = period - first;
    return high + (low - high) * (-std::expm1(-(second - first) / tau)) * std::exp(-first / tau)
                  / (-std::expm1(-period / tau));
}

// Cell integrals of the released triangular displacement's velocity wave.
// The ideal wave is high from 0 to x P/2 and (1-x/2) P to P, and low in
// between. The accepted slip model is the CONTINUOUS one-pole
// tau dy/dt + y = ideal, in periodic steady state. Integrate that solution
// over each sample, including the fractional final cell. Averaging the
// ideal wave first and applying a sampled one-pole afterward does not
// commute with this operation: its attack depends on the sample grid.
//
// Time is in samples; each returned value is an integral over a unit cell
// (the final partial cell is zero-padded), not an instantaneous sample.
// Setup cost only, no additional filter in the playing string loop.
template<std::size_t capacity>
int releasedPluck(std::array<double, capacity>& wave, double period, double position,
                  double high, double low, double tau) noexcept
{
    period = std::clamp(period, 1.0, static_cast<double>(capacity));
    const int count = static_cast<int>(std::ceil(period));
    const double first = 0.5 * position * period;
    const double second = period - first;
    double state = releasedStart(period, position, high, low, tau);
    const double fullCell = tau > 0.0 ? -std::expm1(-1.0 / tau) : 1.0;
    for (int cell = 0; cell < count; ++cell)
    {
        double time = cell;
        const double end = std::min(cell + 1.0, period);
        double integral = 0.0;
        while (time < end)
        {
            const bool middle = time >= first && time < second;
            const double target = middle ? low : high;
            const double boundary = time < first ? first : (middle ? second : period);
            const double next = std::min(end, boundary);
            const double duration = next - time;
            if (tau > 0.0)
            {
                const double advance = duration == 1.0 ? fullCell : -std::expm1(-duration / tau);
                integral += target * duration + (state - target) * tau * advance;
                state += (target - state) * advance;
            }
            else
                integral += target * duration;
            time = next;
        }
        wave[static_cast<std::size_t>(cell)] = integral;
    }
    return count;
}

// Before the pluck the string was held, at `high`, not playing the period:
// the causal one-pole is the periodic one above plus
// (high - releasedStart) e^(-t / tau), which arrives at the bridge once and
// does not come round again. Its first cell's integral and its decay per
// sample; each later cell is the one before times the decay.
struct ReleaseUnloading
{
    double first { 0.0 };
    double decay { 0.0 };
};

inline ReleaseUnloading releaseUnloading(double period, double position, double high, double low,
                                         double tau) noexcept
{
    if (tau <= 0.0)
        return {};
    return { (high - releasedStart(period, position, high, low, tau)) * tau * (-std::expm1(-1.0 / tau)),
             std::exp(-1.0 / tau) };
}
} // namespace aculyre::detail
