#include "AculyreEngine.h"
#include "AculyrePresets.h"
#include "FingerContact.h"
#include "StringContact.h"

#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <memory>
#include <vector>

namespace
{
constexpr double pi = 3.14159265358979323846;
int failures = 0;
void check(bool okay, const char* reason)
{
    if (!okay) { std::printf("FAIL: %s\n", reason); ++failures; }
}
double squaredNorm(const std::array<float, 512>& wave)
{
    double result = 0.0;
    for (float sample : wave) result += double(sample) * sample;
    return result;
}
}

int main()
{
    // The re-pluck contact's convolution against its definition: the period
    // read at every Bessel-weighted shift by Catmull-Rom, sample by sample,
    // and partial n kept as exp(-hold (1 - cos 2 pi n x)) on a whole period.
    for (int count : { 2, 3, 7, 41, 389, 1745 })
        for (double point : { 0.02, 0.25, 0.27, 0.5, 0.7 })
            for (double hold : { 0.3, 2.15, 5.6 })
            {
                std::array<double, 2048> wave {}, kept {}, kernel {};
                for (int j = 0; j < count; ++j)
                    wave[std::size_t(j)] = std::sin(2.0 * pi * j / count) + 0.4 * std::cos(6.0 * pi * j / count + 0.3)
                                         + 0.1 * std::sin(2.0 * pi * 17.0 * j / count);
                aculyre::detail::fingerContact(wave, kept, kernel, count, point, hold);
                // e^-a I_k(a), by the series, independent of the recurrence.
                const auto bessel = [&] (int k)
                {
                    double term = std::pow(0.5 * hold, k) / std::tgamma(k + 1.0), sum = 0.0;
                    for (int m = 0; m < 200; ++m)
                    {
                        sum += term;
                        term *= 0.25 * hold * hold / ((m + 1.0) * (m + 1.0 + k));
                    }
                    return std::exp(-hold) * sum;
                };
                const auto at = [&] (double t)
                {
                    t -= count * std::floor(t / count);
                    const int i = std::min(int(t), count - 1);
                    const double f = t - i;
                    const auto w = [&] (int j) { return wave[std::size_t(((j % count) + count) % count)]; };
                    const double y0 = w(i - 1), y1 = w(i), y2 = w(i + 1), y3 = w(i + 2);
                    return y1 + 0.5 * f * (y2 - y0 + f * (2.0 * y0 - 5.0 * y1 + 4.0 * y2 - y3
                                                          + f * (3.0 * (y1 - y2) + y3 - y0)));
                };
                double worst = 0.0;
                for (int j = 0; j < count; ++j)
                {
                    double value = bessel(0) * wave[std::size_t(j)];
                    for (int k = 1; k < 60; ++k)
                        value += bessel(k) * (at(j - k * point * count) + at(j + k * point * count));
                    worst = std::max(worst, std::abs(value - kept[std::size_t(j)]));
                }
                check(worst < 1e-8, "the contact convolution is the shifted Catmull-Rom sum");
                if (count >= 389)
                {
                    // Low partials, where Catmull-Rom is near exact, keep the
                    // Bessel law's exp(-hold (1 - cos 2 pi n x)).
                    for (int n : { 1, 3 })
                    {
                        std::complex<double> in = 0.0, out = 0.0;
                        for (int j = 0; j < count; ++j)
                        {
                            in += wave[std::size_t(j)] * std::polar(1.0, -2.0 * pi * n * j / count);
                            out += kept[std::size_t(j)] * std::polar(1.0, -2.0 * pi * n * j / count);
                        }
                        const double expected = std::exp(-hold * (1.0 - std::cos(2.0 * pi * n * point)));
                        check(std::abs(std::abs(out / in) - expected) < 2e-3 * (1.0 + expected),
                              "the contact keeps partial n as exp(-hold (1 - cos 2 pi n x))");
                    }
                }
            }

    // The independent standing-wave solution has local velocity proportional
    // to sin(n pi x). Include fractional periods/taps and a bass line whose
    // missing filter phase made the old long-arc contact clip its return tap.
    for (double period : { 30.5630618, 108.949093, 244.897151, 366.854720 })
        for (double point : { 0.125, 0.25, 0.27, 0.5, 0.7 })
        {
            aculyre::detail::StringContact contact;
            contact.configure(point, 30.0, 48000.0 / period, 48000.0, int(0.72 * period));
            check(contact.outgoingAge >= 1.0 && contact.returningAge <= contact.latestAge,
                  "both contact kernels lie inside the live line");
            check(std::abs(contact.returningAge - contact.outgoingAge - contact.requestedSeparation) < 1e-12,
                  "native-range short arc retains the requested spatial phase");
            for (int n = 1; n <= 6; ++n)
            {
                const double omega = 2.0 * pi * n / period;
                std::complex<double> observed = 0.0;
                for (std::size_t k = 0; k < contact.ages.size(); ++k)
                    observed += contact.g[k] * std::polar(1.0, -omega * contact.ages[k]);
                const double expected = 2.0 * std::abs(std::sin(n * pi * point));
                // Linear interpolation of exp(i w t) has error <= w^2/8
                // per tap (linear-interpolation remainder); the difference
                // of two taps therefore has error <= w^2/4. No fitted limit.
                check(std::abs(std::abs(observed) - expected) <= omega * omega / 4.0 + 1e-12,
                      "fractional contact follows the standing-wave participation, including nodes");
            }
            // This test measures the actual stored wave norm before/after
            // scattering, not a smoothed energy observer. The independent
            // dashpot must never supply work. Exercise fractional kernels,
            // wraparound, and their overlap on a very short line as well.
            for (int write : { 0, 1, 317, 511 })
            {
                std::array<float, 512> wave {};
                for (std::size_t k = 0; k < wave.size(); ++k)
                    wave[k] = float(std::sin(0.219 * k) + 0.7 * std::cos(0.123 * k));
                const double before = squaredNorm(wave);
                for (int step = 0; step < 64; ++step) contact.apply(wave.data(), 511, write);
                check(squaredNorm(wave) <= before + 2e-7 * before,
                      "contact removes wave energy rather than supplying it");
            }
        }
    for (int whole : { 1, 2, 3, 4, 9 })
    {
        aculyre::detail::StringContact contact;
        contact.configure(0.5, 10000.0, 1000.0, 48000.0, whole);
        std::array<float, 512> wave {};
        for (std::size_t k = 0; k < wave.size(); ++k) wave[k] = float(std::sin(double(k)));
        const double before = squaredNorm(wave);
        contact.apply(wave.data(), 511, 0);
        check(squaredNorm(wave) <= before + 2e-7 * before, "overlapping/clipped contact remains passive");
    }

    // Exercise the real engine's full dispersion/Thiran/body-phase designs:
    // the observed period is its resonant period, not the explicit delay
    // length. Contact coordinates and rate must follow continuous bend
    // updates even when release/sympathy controls have not changed.
    for (double rate : { 44100.0, 48000.0, 96000.0 })
        for (double sympathy : { 0.0, 1.0 })
        {
            auto engine = std::make_unique<aculyre::AculyreEngine>();
            engine->prepare(rate);
            engine->setInstrument(aculyre::lyreHarpSteel);
            engine->setTensionModulation(false);
            auto controls = engine->controls();
            controls.room = 0.0;
            controls.sympathy = sympathy;
            engine->setControls(controls);
            engine->noteOn(69, 0.7f);
            float left[64], right[64];
            engine->process(left, right, 64);
            std::array<std::array<double, 2>, aculyre::maxStrings> periods {};
            for (int string = 0; string < engine->stringCount(); ++string)
                for (int plane = 0; plane < 2; ++plane)
                    periods[string][plane] = engine->contactReport(string, plane != 0).period;
            for (double bend : { 2.0, 12.0, -12.0, 0.0 })
            {
                controls.pitchBend = bend;
                engine->setControls(controls);
                for (int frame = 0; frame < int(0.25 * rate); frame += 64)
                    engine->process(left, right, 64);
                for (int string = 0; string < engine->stringCount(); ++string)
                    for (int plane = 0; plane < 2; ++plane)
                    {
                        const auto contact = engine->contactReport(string, plane != 0);
                        check(contact.outgoingAge >= 1.0 && contact.returningAge <= contact.latestAge,
                              "bent engine contact never addresses retired propagation storage");
                        const double expected = periods[string][plane] / std::pow(2.0, bend / 12.0);
                        check(std::abs(contact.period / expected - 1.0) < 1e-8,
                              "contact follows the full current resonant period through bends");
                        check(std::abs(contact.returningAge - contact.outgoingAge - contact.requestedSeparation) < 1e-9,
                              "the default contact's short arc fits every native string over +/-12 semitones");
                    }
            }
        }
    // End-to-end bass regression through the production dispersion filters
    // and bridge. A contact close to the fourth partial's node should spare
    // that partial relative to the second, whose antinode is near it. Compare
    // held/released versions of the same excitation, so the pluck's own comb
    // and the unforced string/body decay are not mistaken for damping.
    {
        constexpr int rate = 48000, samples = rate / 2;
        std::array<std::vector<double>, 2> wave;
        double frequency = 0.0, inharmonicity = 0.0;
        for (int released = 0; released < 2; ++released)
        {
            auto engine = std::make_unique<aculyre::AculyreEngine>();
            engine->prepare(rate);
            engine->setInstrument(aculyre::lyreHarpSteel);
            engine->setTensionModulation(false);
            auto controls = engine->controls();
            controls.room = 0.0;
            controls.release = 0.0;
            controls.sympathy = 1.0;
            controls.pluckPosition = 0.26;
            engine->setControls(controls);
            engine->noteOn(48, 0.7f);
            const double drawn = engine->lastPluck().position - 0.26;
            engine->reset();
            controls.pluckPosition -= drawn;
            engine->setControls(controls);
            engine->noteOn(48, 0.7f);
            check(std::abs(engine->lastPluck().position - 0.26) < 1e-12,
                  "the bass contact is tested at the intended near-node position");
            const int string = engine->stringForNote(48);
            const auto report = engine->stringReport(string);
            frequency = report.frequency;
            inharmonicity = report.inharmonicity;
            wave[released].resize(samples);
            for (int frame = 0; frame < samples; ++frame)
            {
                if (released && frame == rate / 10) engine->noteOff(48);
                float left, right;
                engine->process(&left, &right, 1);
                wave[released][frame] = engine->contactReport(string).incomingWaveVelocity;
            }
        }
        const auto power = [&](int released, int partial)
        {
            const double f = partial * frequency
                * std::sqrt((1.0 + inharmonicity * partial * partial) / (1.0 + inharmonicity));
            std::complex<double> sum = 0.0;
            constexpr int start = rate / 5, count = rate / 5;
            for (int frame = 0; frame < count; ++frame)
            {
                const double window = 0.5 - 0.5 * std::cos(2.0 * pi * frame / (count - 1));
                sum += window * wave[released][start + frame]
                     * std::polar(1.0, -2.0 * pi * f * frame / rate);
            }
            return std::norm(sum);
        };
        const double nearNode = power(1, 4) / power(0, 4);
        const double away = power(1, 2) / power(0, 2);
        std::printf("production C3 contact: H4 attenuation %.2f dB, H2 %.2f dB\n",
                    10.0 * std::log10(nearNode), 10.0 * std::log10(away));
        check(nearNode < 1.0 && away < 1.0, "the production contact damps both tested partials");
        check(nearNode > 4.0 * away,
              "production dispersive contact retains a near-node partial over an antinode partial");
    }
    std::printf(failures == 0 ? "all passed\n" : "%d failed\n", failures);
    return failures == 0 ? 0 : 1;
}
