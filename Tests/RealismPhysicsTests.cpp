#include "AculyreEngine.h"
#include "AculyrePresets.h"
#include "PluckRelease.h"
#include "PolarizationRotation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <memory>
#include <vector>

namespace
{
int failures = 0;
void check(bool okay, const char* what)
{
    if (!okay) { std::printf("FAIL: %s\n", what); ++failures; }
}
}

int main()
{
    // Integrating the same continuous release on a finer grid and adding
    // its cells must reproduce the coarse cells, including a fractional
    // last period. This catches the former average-then-filter ordering
    // without enshrining a particular waveform's sample values as a golden.
    for (double period : { 3.71, 31.0, 64.3, 137.82 })
        for (double point : { 0.02, 0.27, 0.7 })
            for (double tau : { 0.0, 0.05, 0.5, 4.0, 32.0 })
            {
                std::array<double, 1024> coarse {}, fine {};
                const double high = 1.0, low = -point / (1.0 - point);
                const int count = aculyre::detail::releasedPluck(coarse, period, point, high, low, tau);
                aculyre::detail::releasedPluck(fine, 4.0 * period, point, high, low, 4.0 * tau);
                double integral = 0.0, worst = 0.0;
                for (int k = 0; k < count; ++k)
                {
                    integral += coarse[static_cast<std::size_t>(k)];
                    double finer = 0.0;
                    for (int sub = 0; sub < 4; ++sub)
                        finer += fine[static_cast<std::size_t>(4 * k + sub)] / 4.0;
                    worst = std::max(worst, std::abs(coarse[static_cast<std::size_t>(k)] - finer));
                }
                check(worst < 1e-12, "continuous release integrals agree across sample grids");
                check(std::abs(integral) < 1e-11, "a released fixed-end triangle introduces no DC velocity");
            }
    // Separate frequency-domain oracle: the rectangular velocity wave's
    // analytic Fourier coefficient is (high-low) sin(n pi x)/(n pi). The
    // physical exponential multiplies it by 1/(1+j omega tau), and the cell
    // average by sinc(omega/2). Fine-grid aliases fall as n/(tau P^2);
    // 4 n (high-low)/(tau P^2) conservatively bounds their omitted sum.
    constexpr double pi = 3.14159265358979323846, period = 512.0, point = 0.27;
    for (double tau : { 0.5, 4.0, 32.0 })
    {
        std::array<double, 512> wave {};
        const double high = 1.0, low = -point / (1.0 - point);
        aculyre::detail::releasedPluck(wave, period, point, high, low, tau);
        for (int harmonic = 1; harmonic <= 4; ++harmonic)
        {
            const double omega = 2.0 * pi * harmonic / period;
            std::complex<double> measured {};
            for (int k = 0; k < 512; ++k)
                measured += wave[static_cast<std::size_t>(k)]
                    * std::exp(std::complex<double>(0.0, -omega * (k + 0.5))) / period;
            const double ideal = (high - low) * std::sin(pi * harmonic * point) / (pi * harmonic);
            const auto expected = ideal / std::complex<double>(1.0, omega * tau)
                                  * (std::sin(0.5 * omega) / (0.5 * omega));
            check(std::abs(measured - expected) < 4.0 * harmonic * (high - low) / (tau * period * period),
                  "release amplitude and phase follow the continuous one-pole law");
        }
    }
    // Independent port identities: a coordinate rotation cannot create
    // travelling-wave power and must preserve force/velocity work. Exercise
    // much larger slopes too, so a missing normalization/transpose cannot
    // disappear under the production slope's small numerical effect.
    for (double slope : { -0.9, -0.0018, 0.0, 0.0018, 0.9 })
        for (double normal : { -1.2, 0.0, 0.7 })
            for (double parallel : { -0.3, 0.0, 1.1 })
            {
                const auto rotation = aculyre::detail::PolarizationRotation::fromSlope(slope);
                const double n = rotation.normalForce(normal, parallel);
                const double p = rotation.parallelForce(normal, parallel);
                check(std::abs(n * n + p * p - normal * normal - parallel * parallel) < 2e-15,
                      "saddle rotation preserves travelling-wave power");
                const double bodyNormal = 0.63, bodyParallel = -0.19;
                const double bodyWork = n * bodyNormal + p * bodyParallel;
                const double stringWork = normal * rotation.normalMotion(bodyNormal, bodyParallel)
                                          + parallel * rotation.parallelMotion(bodyNormal, bodyParallel);
                check(std::abs(bodyWork - stringWork) < 2e-15,
                      "saddle force and returning motion are reciprocal");
            }

    // With the tension glide disabled, the string/body/microphone path is
    // linear. A chord must therefore equal the sum of its individual
    // plucks, including each string's friction attack. Prime the earlier
    // random draws with inaudible plucks so every target gets the identical
    // force, point and slip; do not put a later pluck over the target.
    // The old global friction generator failed this by replacing every
    // attack except the chord's last note.
    auto engine = std::make_unique<aculyre::AculyreEngine>();
    engine->prepare(48000.0);
    engine->setInstrument(aculyre::lyreHarpSteel);
    engine->setTensionModulation(false);
    auto controls = engine->controls();
    controls.room = 0.0;
    controls.sympathy = 1.0;
    controls.technique = aculyre::Technique::Nail;
    engine->setControls(controls);
    constexpr std::array<int, 4> notes { 55, 60, 64, 69 };
    constexpr int samples = 4800;
    std::vector<float> left(samples), right(samples);
    std::vector<double> sumLeft(samples), sumRight(samples);
    const auto render = [&]
    {
        for (int position = 0; position < samples; position += 64)
            engine->process(left.data() + position, right.data() + position, 64);
    };
    for (std::size_t target = 0; target < notes.size(); ++target)
    {
        engine->reset();
        for (std::size_t before = 0; before < target; ++before)
            engine->noteOn(notes[before], 1.0e-12f);
        engine->noteOn(notes[target], 0.8f);
        render();
        for (int sample = 0; sample < samples; ++sample)
        {
            sumLeft[sample] += left[sample];
            sumRight[sample] += right[sample];
        }
    }
    engine->reset();
    for (int note : notes)
        engine->noteOn(note, 0.8f);
    render();
    double error = 0.0, signal = 0.0;
    for (int sample = 0; sample < samples; ++sample)
    {
        const double dl = left[sample] - sumLeft[sample], dr = right[sample] - sumRight[sample];
        error += dl * dl + dr * dr;
        signal += sumLeft[sample] * sumLeft[sample] + sumRight[sample] * sumRight[sample];
    }
    const double relative = std::sqrt(error / std::max(signal, 1e-30));
    std::printf("chord against the sum of its plucks: %.1f dB residual\n", 20.0 * std::log10(std::max(relative, 1e-30)));
    check(relative < 1e-5, "a chord retains every string's attack in the linear acoustic path");

    // A distributed palm loss is an amplitude decay rate in seconds, not
    // a fixed loss per OPEN-string round trip. Remove the intrinsic/body
    // rate using an otherwise identical unmuted run. A heavy bridge keeps
    // body transients out of this string-energy measurement. The old rule
    // gave about 15, 30 and 60 /s on bends -12, 0 and +12 respectively.
    auto single = aculyre::lyreHarpSteel;
    single.strings[0] = single.strings[13];  // A4, plain steel
    single.stringCount = 1;
    single.body.bridgeMass = 1.0;
    engine->setInstrument(single);
    for (double bend : { -12.0, 0.0, 12.0 })
    {
        double rates[2] {};
        for (int muted = 0; muted < 2; ++muted)
        {
            engine->reset();
            auto c = engine->controls();
            c.room = 0.0;
            c.mute = muted;
            c.pitchBend = bend;
            engine->setControls(c);
            engine->noteOn(69, 0.7f);
            const auto advance = [&] (int frames)
            {
                std::array<float, 64> l {}, r {};
                for (int position = 0; position < frames; position += 64)
                    engine->process(l.data(), r.data(), std::min(64, frames - position));
            };
            advance(2400);  // 50 ms, after the contact's one-period ramp
            const double first = engine->stringEnergy(0);
            advance(3840);  // 80 ms of decay
            const double last = engine->stringEnergy(0);
            rates[muted] = -std::log(last / first) / (2.0 * 0.080);
        }
        const double added = rates[1] - rates[0];
        std::printf("palm decay at bend %+.0f: %.3f /s additional amplitude loss\n", bend, added);
        check(std::isfinite(added) && std::abs(added - 30.0) < 3.0,
              "the palm's decay time stays in seconds through octave bends");
    }
    return failures ? 1 : 0;
}
