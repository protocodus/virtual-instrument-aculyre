#pragma once

#include "DspMath.h"
#include "Pair.h"

#include <array>
#include <cmath>
#include <cstddef>

namespace aculyre::detail
{
// A re-plucking finger holding a sounding string at x (a fraction of its
// length) for `hold` time constants: over one period of `count` samples,
// partial n keeps exp(-hold (1 - cos 2 pi n x)). That is a sum of circular
// shifts of the period by k x count, weighted by e^-hold I_k(hold) (the
// Bessel expansion of exp(hold cos)), each shift read by Catmull-Rom.
//
// The period's samples are whole, so each shift's interpolation fraction is
// the same for all of them: the contact is one circular convolution whose
// kernel holds four taps per shift. Evaluating a Catmull-Rom per sample per
// shift instead cost an all-string re-pluck about 3 ms (2026-11-09).
// `kernel` is working space. Setup cost of a pluck, not a running filter.
template<std::size_t capacity>
void fingerContact(const std::array<double, capacity>& wave, std::array<double, capacity>& kept,
                   std::array<double, capacity>& kernel, int count, double position, double hold) noexcept
{
    // e^-a I_k(a) by Miller's backward recurrence, I_(k-1) = I_(k+1) +
    // (2k / a) I_k, normalised by e^a = I_0 + 2 sum I_k.
    constexpr int maxTerms = 64;
    std::array<double, maxTerms + 2> weight {};
    double next = 0.0, current = 1.0e-30;
    for (int k = maxTerms; k >= 1; --k)
    {
        const double previous = next + 2.0 * k / hold * current;
        next = current;
        current = previous;
        weight[static_cast<std::size_t>(k - 1)] = current;
        if (current > 1.0e250)
        {
            for (int j = k - 1; j <= maxTerms; ++j)
                weight[static_cast<std::size_t>(j)] *= 1.0e-250;
            current *= 1.0e-250;
            next *= 1.0e-250;
        }
    }
    double sum = weight[0];
    for (int k = 1; k <= maxTerms; ++k)
        sum += 2.0 * weight[static_cast<std::size_t>(k)];

    for (int d = 0; d < count; ++d)
        kernel[static_cast<std::size_t>(d)] = 0.0;
    const auto wrap = [count] (int j) { return static_cast<std::size_t>(j < 0 ? j + count : (j >= count ? j - count : j)); };
    // Reading the period at j + offset adds these four taps at offset.
    const auto shiftBy = [&] (double offset, double w)
    {
        offset -= count * floorValue(offset / count);
        int i = static_cast<int>(offset);
        const double f = offset - i;
        // Rounding a tiny negative offset can give exactly count.
        if (i == count)
            i = 0;
        const double f2 = f * f, f3 = f2 * f;
        kernel[wrap(i - 1)] += w * (-0.5 * f + f2 - 0.5 * f3);
        kernel[wrap(i)] += w * (1.0 - 2.5 * f2 + 1.5 * f3);
        kernel[wrap(i + 1)] += w * (0.5 * f + 2.0 * f2 - 1.5 * f3);
        kernel[wrap(i + 2)] += w * (-0.5 * f2 + 0.5 * f3);
    };
    kernel[0] += weight[0] / sum;
    const double shift = position * count;
    for (int k = 1; k <= maxTerms; ++k)
    {
        const double w = weight[static_cast<std::size_t>(k)] / sum;
        if (w < 1.0e-9)
            break;
        shiftBy(-k * shift, w);
        shiftBy(k * shift, w);
    }

    // The convolution, eight taps a pass and two outputs at a time. Each
    // output still adds its taps in rising order, so the sums are the ones a
    // tap-by-tap loop makes; a pass short of eight taps is padded with zeros,
    // which change no sum. Reason's compiler does not vectorise loops: tap
    // by tap in scalars this was three quarters of a re-pluck there.
    constexpr int batch = 8;
    // Not cleared: only the taps found are read.
    std::array<int, 8 * maxTerms + 1> tapOffset;
    std::array<double, 8 * maxTerms + 1> tapWeight;
    int taps = 0;
    for (int d = 0; d < count; ++d)
        if (kernel[static_cast<std::size_t>(d)] != 0.0)
        {
            tapOffset[static_cast<std::size_t>(taps)] = d;
            tapWeight[static_cast<std::size_t>(taps++)] = kernel[static_cast<std::size_t>(d)];
        }
    for (int j = 0; j < count; ++j)
        kept[static_cast<std::size_t>(j)] = 0.0;
    const double* const from = wave.data();
    double* const to = kept.data();
    for (int first = 0; first < taps; first += batch)
    {
        // Padding repeats the last tap's offset, so the offsets still rise.
        int offset[batch];
        double weightOf[batch];
        Pair weights[batch];
        for (int t = 0; t < batch; ++t)
        {
            const bool real = first + t < taps;
            offset[t] = tapOffset[static_cast<std::size_t>(real ? first + t : taps - 1)];
            weightOf[t] = real ? tapWeight[static_cast<std::size_t>(first + t)] : 0.0;
            weights[t] = splat(weightOf[t]);
        }
        // The later taps wrap round the period first: the outputs fall into
        // runs, each with its own read places.
        std::ptrdiff_t begin = 0;
        for (int wrapped = 0; wrapped <= batch; ++wrapped)
        {
            const std::ptrdiff_t end = wrapped == batch ? count : count - offset[batch - 1 - wrapped];
            std::ptrdiff_t shift[batch];
            for (int t = 0; t < batch; ++t)
                shift[t] = offset[t] - (t >= batch - wrapped ? count : 0);
            std::ptrdiff_t j = begin;
            for (; j + 1 < end; j += 2)
            {
                Pair sum = *pairs(to + j);
#if defined(__clang__)
#pragma unroll
#endif
                for (int t = 0; t < batch; ++t)
                    sum = sum + weights[t] * *pairs(from + (j + shift[t]));
                *pairs(to + j) = sum;
            }
            if (j < end)
            {
                double sum = to[j];
#if defined(__clang__)
#pragma unroll
#endif
                for (int t = 0; t < batch; ++t)
                    sum += weightOf[t] * from[j + shift[t]];
                to[j] = sum;
            }
            begin = end;
        }
    }
}
} // namespace aculyre::detail
