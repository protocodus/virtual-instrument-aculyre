// Contracts for the prototype engine: tuning, the stiff string's partials,
// the pluck point, stability, determinism, block partition invariance and
// sympathetic coupling.

#include "AculyreEngine.h"
#include "AculyrePresets.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <memory>
#include <vector>

namespace
{
int failures = 0;

void check(bool condition, const char* what)
{
    if (!condition)
    {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

std::vector<float> renderNote(const aculyre::InstrumentSpec& preset, int note, double seconds,
                              int block, bool room = false, bool sympathy = true)
{
    auto engine = std::make_unique<aculyre::AculyreEngine>();
    engine->prepare(48000.0);
    engine->setInstrument(preset);
    engine->setRoomEnabled(room);
    engine->setSympatheticStrings(sympathy);
    engine->noteOn(note, 0.7f);
    const auto total = static_cast<int>(seconds * 48000.0);
    std::vector<float> left(static_cast<std::size_t>(total)), right(static_cast<std::size_t>(total));
    for (int position = 0; position < total; position += block)
    {
        const int count = std::min(block, total - position);
        engine->process(left.data() + position, right.data() + position, count);
    }
    return left;
}

struct Peak
{
    double frequency;
    double magnitude;
};

// The strongest spectral peak within +-60 cents of `nominal`, by a
// zero-padded DFT scan.
Peak peakNear(const std::vector<float>& x, int start, int length, double nominal)
{
    double best = 0.0, bestFrequency = nominal;
    for (double cents = -60.0; cents <= 60.0; cents += 0.25)
    {
        const double f = nominal * std::pow(2.0, cents / 1200.0);
        std::complex<double> sum = 0.0;
        for (int i = 0; i < length; ++i)
        {
            const double window = 0.5 - 0.5 * std::cos(2.0 * 3.14159265358979 * i / (length - 1));
            sum += window * x[static_cast<std::size_t>(start + i)]
                 * std::exp(std::complex<double>(0.0, -2.0 * 3.14159265358979 * f * i / 48000.0));
        }
        if (std::abs(sum) > best)
        {
            best = std::abs(sum);
            bestFrequency = f;
        }
    }
    return { bestFrequency, best };
}

double rms(const std::vector<float>& x, int start, int length)
{
    double sum = 0.0;
    for (int i = start; i < start + length; ++i)
        sum += static_cast<double>(x[static_cast<std::size_t>(i)]) * x[static_cast<std::size_t>(i)];
    return std::sqrt(sum / length);
}
} // namespace

int main()
{
    for (const auto* preset : aculyre::presets)
    {
        for (int index : { 0, preset->stringCount / 2, preset->stringCount - 1 })
        {
            const int note = preset->strings[static_cast<std::size_t>(index)].midiNote;
            const auto audio = renderNote(*preset, note, 3.0, 64);
            bool finite = true;
            for (float sample : audio)
                finite = finite && std::isfinite(sample);
            check(finite, "output is finite");
            const double nominal = 440.0 * std::pow(2.0, (note - 69) / 12.0);
            const double measured = peakNear(audio, 24000, 48000, nominal).frequency;
            const double cents = 1200.0 * std::log2(measured / nominal);
            std::printf("%-16s note %d: %+.2f cents\n", preset->name, note, cents);
            check(std::abs(cents) < 6.0, "the open string sounds its note (within 6 cents)");
            check(rms(audio, 120000, 4800) < 0.5 * rms(audio, 4800, 4800), "the note decays");
        }
    }

    // The bass steel string's partials follow the stiff-string law,
    // f_n = n f0 sqrt((1 + B n^2) / (1 + B)), through the 12th that the
    // dispersion is fitted to (6 cents by design, plus the two polarisations
    // and the scan's resolution). A dispersion fitted
    // to one high partial put them 30 to 120 cents sharp.
    {
        // Plucked near the bridge, so no partial up to the 12th is missing,
        // on a 1 kg bridge, so the body's coupling (up to about 11 cents on
        // the 6 g one) moves none of them.
        aculyre::InstrumentSpec spec = aculyre::lyreHarpSteel;
        spec.pluckPosition = 0.13;
        spec.body.bridgeMass = 1.0;
        aculyre::AculyreEngine engine;
        engine.prepare(48000.0);
        engine.setInstrument(spec);
        const auto report = engine.stringReport(0);
        const auto audio = renderNote(spec, report.midiNote, 1.6, 64, false, false);
        double worst = 0.0;
        for (int n = 2; n <= 12; ++n)
        {
            const double law = n * report.frequency
                * std::sqrt((1.0 + report.inharmonicity * n * n) / (1.0 + report.inharmonicity));
            worst = std::max(worst, std::abs(1200.0 * std::log2(peakNear(audio, 4800, 72000, law).frequency / law)));
        }
        std::printf("steel G3 partials 2-12: worst %.1f cents from the stiff-string law\n", worst);
        check(worst < 10.0, "the bass string's partials follow the stiff-string law");
    }

    // The steel lyre harp's pluck a quarter of the way from the bridge leaves
    // out the 4th harmonic, as in the CC0 reference recording.
    {
        const auto audio = renderNote(aculyre::lyreHarpSteel, 69, 0.5, 64, false, false);
        const auto level = [&] (int n) { return peakNear(audio, 960, 12000, 440.0 * n).magnitude; };
        const double h3 = level(3), h4 = level(4), h5 = level(5);
        std::printf("A4 plucked at a quarter: H4 %.1f dB below H3, %.1f dB below H5\n",
                    20.0 * std::log10(h3 / h4), 20.0 * std::log10(h5 / h4));
        check(h4 < 0.18 * h3 && h4 < 0.18 * h5, "a quarter-length pluck leaves out the 4th harmonic");
    }

    // Determinism and block-partition invariance.
    const auto a = renderNote(aculyre::lyreHarpSteel, 67, 1.0, 64, true);
    const auto b = renderNote(aculyre::lyreHarpSteel, 67, 1.0, 64, true);
    const auto c = renderNote(aculyre::lyreHarpSteel, 67, 1.0, 1, true);
    check(a == b, "renders are deterministic");
    check(a == c, "block size does not change the output");

    // Sympathy: plucking C4 rings the idle C5 (an octave) well above the idle
    // D5 (a ninth, few shared partials).
    {
        aculyre::AculyreEngine engine;
        engine.prepare(48000.0);
        engine.setInstrument(aculyre::lyreHarpSteel);
        engine.noteOn(60, 0.8f);
        std::vector<float> l(48000), r(48000);
        engine.process(l.data(), r.data(), 48000);
        const double octave = engine.stringEnergy(engine.stringForNote(72));
        const double ninth = engine.stringEnergy(engine.stringForNote(74));
        const double played = engine.stringEnergy(engine.stringForNote(60));
        std::printf("sympathy after 1 s: octave %.1f dB, ninth %.1f dB re the played string\n",
                    10.0 * std::log10(octave / played), 10.0 * std::log10(ninth / played));
        check(octave > 4.0 * ninth, "the octave string rings in sympathy more than the ninth");
        check(octave < played, "a sympathetic string stays below the played one");
    }

    std::printf(failures == 0 ? "all passed\n" : "%d failed\n", failures);
    return failures == 0 ? 0 : 1;
}
