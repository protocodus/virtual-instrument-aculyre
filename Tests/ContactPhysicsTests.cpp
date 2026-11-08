#include "AculyreEngine.h"
#include "AculyrePresets.h"
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
    {
        auto engine = std::make_unique<aculyre::AculyreEngine>();
        engine->prepare(rate);
        engine->setInstrument(aculyre::lyreHarpSteel);
        engine->setTensionModulation(false);
        auto controls = engine->controls();
        controls.room = 0.0;
        controls.sympathy = 0.0;
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
