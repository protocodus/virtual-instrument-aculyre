// Contracts for the prototype engine: tuning, the stiff string's partials,
// the pluck point, stability, determinism, block partition invariance,
// sympathetic coupling, the playing controls and the Performer.

#include "AculyreEngine.h"
#include "AculyrePerformer.h"
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
                              int block, bool room = false, bool sympathy = true,
                              void (*adjust)(aculyre::PlayingControls&) = nullptr)
{
    auto engine = std::make_unique<aculyre::AculyreEngine>();
    engine->prepare(48000.0);
    engine->setInstrument(preset);
    aculyre::PlayingControls controls = engine->controls();
    if (!room)
        controls.room = 0.0;
    if (!sympathy)
        controls.sympathy = 0.0;
    if (adjust)
        adjust(controls);
    engine->setControls(controls);
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
            // The plucked string alone: the strings an octave, a twelfth and
            // two octaves below ring partials within 15 cents of the note, and
            // the steel A5 sits at the bar's mobile end, where its normal
            // polarisation fades at about 45 dB/s and a sympathetic partial
            // outlasts it in the second the scan reads.
            const int note = preset->strings[static_cast<std::size_t>(index)].midiNote;
            const auto audio = renderNote(*preset, note, 3.0, 64, false, false);
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
        // on a 1 kg bridge, so the body's coupling (up to about 14 cents on
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
    // out the 4th harmonic, as in the CC0 reference recording. Measured
    // against the same partial plucked at an eighth, where it is strongest,
    // so the body passes both alike. At the A4 string's point of the bending
    // bar the body passes its 3rd, 4th and 5th harmonics unevenly (0.036,
    // 0.003 and 0.017 m/s/N), so the neighbours no longer measure the notch.
    {
        const auto quarter = renderNote(aculyre::lyreHarpSteel, 69, 0.5, 64, false, false,
                                        [] (aculyre::PlayingControls& c) { c.pluckPosition = 0.25; });
        const auto eighth = renderNote(aculyre::lyreHarpSteel, 69, 0.5, 64, false, false,
                                       [] (aculyre::PlayingControls& c) { c.pluckPosition = 0.125; });
        const double h4 = peakNear(quarter, 960, 12000, 1760.0).magnitude;
        const double open = peakNear(eighth, 960, 12000, 1760.0).magnitude;
        std::printf("A4 plucked at a quarter: H4 %.1f dB below the pluck at an eighth\n",
                    20.0 * std::log10(open / h4));
        check(h4 < 0.18 * open, "a quarter-length pluck leaves out the 4th harmonic");
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

    // A raised sharping lever sounds the semitone above its string; with
    // the levers off that note has no string.
    {
        const auto raised = renderNote(aculyre::lyreHarpSteel, 61, 2.0, 64, false, true,
                                       [] (aculyre::PlayingControls& c) { c.levers = true; });
        const double nominal = 440.0 * std::pow(2.0, (61 - 69) / 12.0);
        const double cents = 1200.0 * std::log2(peakNear(raised, 24000, 48000, nominal).frequency / nominal);
        std::printf("C#4 on the C4 string, lever up: %+.2f cents\n", cents);
        check(std::abs(cents) < 6.0, "a raised lever sounds the semitone above (within 6 cents)");
        const auto none = renderNote(aculyre::lyreHarpSteel, 61, 0.5, 64);
        check(rms(none, 0, 24000) < 1.0e-9, "without levers a note between strings is silent");
    }

    // The pitch bend moves every string; two semitones up lands on B4 from
    // A4.
    {
        const auto bent = renderNote(aculyre::lyreHarpSteel, 69, 2.0, 64, false, true,
                                     [] (aculyre::PlayingControls& c) { c.pitchBend = 2.0; });
        const double nominal = 440.0 * std::pow(2.0, 2.0 / 12.0);
        const double cents = 1200.0 * std::log2(peakNear(bent, 24000, 48000, nominal).frequency / nominal);
        std::printf("A4 bent up two semitones: %+.2f cents from B4\n", cents);
        check(std::abs(cents) < 6.0, "the pitch bend retunes the string");
        const auto down = renderNote(aculyre::lyreHarpSteel, 55, 2.0, 64, false, true,
                                     [] (aculyre::PlayingControls& c) { c.pitchBend = -2.0; });
        const double low = 440.0 * std::pow(2.0, (53 - 69) / 12.0);
        const double downCents = 1200.0 * std::log2(peakNear(down, 24000, 48000, low).frequency / low);
        std::printf("G3 bent down two semitones: %+.2f cents from F3\n", downCents);
        check(std::abs(downCents) < 6.0, "the pitch bend retunes the bass string down");
    }

    // The hands: a full palm shortens the note; letting a released string
    // ring keeps it sounding where the playing hand would stop it.
    {
        const auto open = renderNote(aculyre::lyreHarpSteel, 69, 1.0, 64);
        const auto muted = renderNote(aculyre::lyreHarpSteel, 69, 1.0, 64, false, true,
                                      [] (aculyre::PlayingControls& c) { c.mute = 1.0; });
        check(rms(muted, 24000, 4800) < 0.01 * rms(open, 24000, 4800), "the palm mute stops the strings");
        // The other strings ring on in sympathy either way, so this looks
        // at the played string itself.
        double energy[2] {};
        for (int ring = 0; ring < 2; ++ring)
        {
            aculyre::AculyreEngine engine;
            engine.prepare(48000.0);
            engine.setInstrument(aculyre::lyreHarpSteel);
            aculyre::PlayingControls controls = engine.controls();
            controls.release = ring;
            engine.setControls(controls);
            engine.noteOn(69, 0.7f);
            std::vector<float> l(24000), r(24000);
            engine.process(l.data(), r.data(), 4800);
            engine.noteOff(69);
            engine.process(l.data(), r.data(), 24000);
            energy[ring] = engine.stringEnergy(engine.stringForNote(69));
        }
        check(energy[1] > 1000.0 * energy[0], "release lets a let-go string ring");
    }

    // The Performer switches instruments in real time: the old one fades,
    // the output stays finite and does not depend on the block size.
    {
        const auto play = [] (int block)
        {
            auto performer = std::make_unique<aculyre::Performer>();
            performer->prepare(48000.0);
            std::vector<float> l(48000), r(48000);
            for (int position = 0; position < 48000; position += block)
            {
                if (position == 0)
                    performer->noteOn(67, 0.8f);
                if (position == 9600)
                {
                    performer->setInstrument(2);
                    performer->noteOn(62, 0.8f);
                }
                if (position == 19200)
                    performer->setInstrument(0);
                performer->process(l.data() + position, r.data() + position, block);
            }
            return l;
        };
        const auto a = play(64), b = play(32);
        bool finite = true;
        for (float sample : a)
            finite = finite && std::isfinite(sample);
        check(finite, "the Performer's output is finite across switches");
        check(a == b, "the Performer's switches do not depend on the block size");
        check(rms(a, 9600 + 1200, 2400) > 0.0 && rms(a, 19200 + 1200, 2400) < rms(a, 19200 - 2400, 2400),
              "an instrument switch fades the old instrument out");
    }

    std::printf(failures == 0 ? "all passed\n" : "%d failed\n", failures);
    return failures == 0 ? 0 : 1;
}
