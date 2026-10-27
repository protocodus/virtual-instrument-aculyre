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

// The open pitch an instrument's string for `note` is tuned to.
double tuned(const aculyre::InstrumentSpec& preset, int note)
{
    aculyre::AculyreEngine engine;
    engine.prepare(48000.0);
    engine.setInstrument(preset);
    return engine.stringReport(engine.stringForNote(note)).frequency;
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
    // Pitch is read from 2.5 s on: a hard pluck's tension glide holds the
    // steel G3 up to 9 cents sharp over its first second and a half.
    constexpr int settled = 120000;
    for (const auto* preset : aculyre::presets)
    {
        for (int index : { 0, preset->stringCount / 2, preset->stringCount - 1 })
        {
            const int note = preset->strings[static_cast<std::size_t>(index)].midiNote;
            const auto audio = renderNote(*preset, note, 3.5, 64);
            bool finite = true;
            for (float sample : audio)
                finite = finite && std::isfinite(sample);
            check(finite, "output is finite");
            const double nominal = tuned(*preset, note);
            const double measured = peakNear(audio, settled, 48000, nominal).frequency;
            const double cents = 1200.0 * std::log2(measured / nominal);
            std::printf("%-16s note %d: %+.2f cents\n", preset->name, note, cents);
            check(std::abs(cents) < 6.0, "the open string sounds its tuning (within 6 cents)");
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
        // Against the sounding fundamental: a hard pluck's tension glide
        // raises every partial alike.
        const double fundamental = peakNear(audio, 4800, 72000, report.frequency).frequency;
        double worst = 0.0;
        for (int n = 2; n <= 12; ++n)
        {
            const double law = n * fundamental
                * std::sqrt((1.0 + report.inharmonicity * n * n) / (1.0 + report.inharmonicity));
            worst = std::max(worst, std::abs(1200.0 * std::log2(peakNear(audio, 4800, 72000, law).frequency / law)));
        }
        std::printf("steel G3 partials 2-12: worst %.1f cents from the stiff-string law\n", worst);
        check(worst < 10.0, "the bass string's partials follow the stiff-string law");
    }

    // Each instrument's strings are tuned by hand: a few cents off their
    // notes, about the instrument's own pitch, and the same each time it is
    // set up.
    for (const auto* preset : aculyre::presets)
    {
        double sum = 0.0, squares = 0.0, worst = 0.0;
        for (int index = 0; index < preset->stringCount; ++index)
        {
            const int note = preset->strings[static_cast<std::size_t>(index)].midiNote;
            const double cents = 1200.0 * std::log2(tuned(*preset, note) / (440.0 * std::pow(2.0, (note - 69) / 12.0)));
            sum += cents;
            squares += cents * cents;
            worst = std::max(worst, std::abs(cents));
        }
        const double mean = sum / preset->stringCount;
        const double spread = std::sqrt(squares / preset->stringCount - mean * mean);
        std::printf("%-16s tuning: mean %+.2f, spread %.2f, worst %.2f cents\n", preset->name, mean, spread, worst);
        check(std::abs(mean) < 1.0e-9 && spread > 1.0 && worst <= 2.0 * 3.0 * 4.1,
              "strings sit a few cents off their notes about the instrument's pitch");
    }

    // The steel lyre harp's pluck a quarter of the way from the bridge leaves
    // out the 4th harmonic, as in the CC0 reference recording. Plucks scatter
    // about the control's point, and a fresh engine draws the same scatter,
    // so the control is moved to land this pluck on the quarter.
    {
        std::vector<float> audio(24000), right(24000);
        const auto pluck = [&] (double control)
        {
            aculyre::AculyreEngine engine;
            engine.prepare(48000.0);
            engine.setInstrument(aculyre::lyreHarpSteel);
            aculyre::PlayingControls controls = engine.controls();
            controls.room = 0.0;
            controls.sympathy = 0.0;
            controls.pluckPosition = control;
            engine.setControls(controls);
            engine.noteOn(69, 0.7f);
            engine.process(audio.data(), right.data(), 24000);
            return engine.lastPluck().position;
        };
        const double landed = pluck(0.5 - pluck(0.25));
        check(std::abs(landed - 0.25) < 1.0e-9, "the moved control lands the pluck on the quarter");
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

    // Plucks scatter as harpists' do (Chadefaux et al., JASA 131 (2012) 844):
    // the pluck point by 0.012 of the length on a 17.6 cm string to 0.022 on
    // a 97.5 cm one (Fig. 7), the force by 14% and the release time by 20%
    // (Fig. 8), the angle from the top by 4 degrees (Table VI), each about
    // the control. A finger's release at velocity 0.7 is 0.1 ms x 0.88.
    for (int note : { 55, 81 })
    {
        aculyre::AculyreEngine engine;
        engine.prepare(48000.0);
        engine.setInstrument(aculyre::lyreHarpSteel);
        const aculyre::PlayingControls controls = engine.controls();
        const double length = aculyre::lyreHarpSteel.strings[static_cast<std::size_t>(engine.stringForNote(note))].length;
        const double angle = std::asin(std::sqrt(controls.normalShare));
        constexpr int plucks = 2000;
        double sum[4] {}, squares[4] {};
        for (int k = 0; k < plucks; ++k)
        {
            engine.noteOn(note, 0.7f);
            const auto pluck = engine.lastPluck();
            const double deviation[4] = { pluck.position - controls.pluckPosition,
                                          std::log(pluck.force / (aculyre::lyreHarpSteel.pluckForce * std::pow(0.7, 1.4))),
                                          std::asin(std::sqrt(pluck.normalShare)) - angle,
                                          std::log(pluck.release / (0.10e-3 * (1.3 - 0.6 * 0.7))) };
            for (int i = 0; i < 4; ++i)
            {
                sum[i] += deviation[i];
                squares[i] += deviation[i] * deviation[i];
            }
        }
        const double spread[4] = { 0.012 + (0.022 - 0.012) * (length - 0.176) / (0.975 - 0.176), 0.14,
                                   4.0 * 3.14159265358979 / 180.0, 0.2 };
        const char* names[4] = { "position", "log force", "angle", "log release" };
        for (int i = 0; i < 4; ++i)
        {
            const double mean = sum[i] / plucks;
            const double deviation = std::sqrt(squares[i] / plucks - mean * mean);
            std::printf("string %d, %s: mean %+.4f, deviation %.4f against %.4f\n", note, names[i], mean,
                        deviation, spread[i]);
            check(std::abs(mean) < 0.1 * spread[i], "plucks scatter about the control");
            check(std::abs(deviation / spread[i] - 1.0) < 0.08, "plucks scatter by the measured spread");
        }
    }

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
        const auto raised = renderNote(aculyre::lyreHarpSteel, 61, 3.5, 64, false, true,
                                       [] (aculyre::PlayingControls& c) { c.levers = true; });
        const double nominal = tuned(aculyre::lyreHarpSteel, 60) * std::pow(2.0, 1.0 / 12.0);
        const double cents = 1200.0 * std::log2(peakNear(raised, settled, 48000, nominal).frequency / nominal);
        std::printf("C#4 on the C4 string, lever up: %+.2f cents\n", cents);
        check(std::abs(cents) < 6.0, "a raised lever sounds the semitone above (within 6 cents)");
        const auto none = renderNote(aculyre::lyreHarpSteel, 61, 0.5, 64);
        check(rms(none, 0, 24000) < 1.0e-9, "without levers a note between strings is silent");
    }

    // The pitch bend moves every string; two semitones up lands on B4 from
    // A4.
    {
        const auto bent = renderNote(aculyre::lyreHarpSteel, 69, 3.5, 64, false, true,
                                     [] (aculyre::PlayingControls& c) { c.pitchBend = 2.0; });
        const double nominal = tuned(aculyre::lyreHarpSteel, 69) * std::pow(2.0, 2.0 / 12.0);
        const double cents = 1200.0 * std::log2(peakNear(bent, settled, 48000, nominal).frequency / nominal);
        std::printf("A4 bent up two semitones: %+.2f cents from B4\n", cents);
        check(std::abs(cents) < 6.0, "the pitch bend retunes the string");
        const auto down = renderNote(aculyre::lyreHarpSteel, 55, 3.5, 64, false, true,
                                     [] (aculyre::PlayingControls& c) { c.pitchBend = -2.0; });
        const double low = tuned(aculyre::lyreHarpSteel, 55) * std::pow(2.0, -2.0 / 12.0);
        const double downCents = 1200.0 * std::log2(peakNear(down, settled, 48000, low).frequency / low);
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

    // A finger re-plucking a sounding string stops it first; a plectrum
    // leaves some of the old note ringing. The reference plucks another
    // string first, so the second pluck takes the same random draws.
    {
        const auto plucked = [] (aculyre::Technique technique, int first)
        {
            aculyre::AculyreEngine engine;
            engine.prepare(48000.0);
            engine.setInstrument(aculyre::lyreHarpSteel);
            aculyre::PlayingControls controls = engine.controls();
            controls.technique = technique;
            engine.setControls(controls);
            std::vector<float> l(12000), r(12000);
            engine.noteOn(first, 0.9f);
            engine.process(l.data(), r.data(), 12000);
            engine.noteOn(69, 0.1f);
            // 25 ms: the energy tracker has settled, and the ringing
            // instrument has not yet fed much back into the stopped string.
            engine.process(l.data(), r.data(), 1200);
            return engine.stringEnergy(engine.stringForNote(69));
        };
        const double finger = plucked(aculyre::Technique::Finger, 69) / plucked(aculyre::Technique::Finger, 60);
        const double plectrum = plucked(aculyre::Technique::Plectrum, 69) / plucked(aculyre::Technique::Plectrum, 60);
        std::printf("soft re-pluck of a loud A4, string energy re a fresh pluck: finger %.2f, plectrum %.2f\n",
                    finger, plectrum);
        check(finger < 1.5, "a finger stops the string it re-plucks");
        check(plectrum > 2.0, "a plectrum leaves some of the old note");
    }

    // A silent engine sleeps: it starts asleep, a pluck wakes it, and once
    // everything has died away it sleeps again and writes exact silence.
    {
        aculyre::AculyreEngine engine;
        engine.prepare(48000.0);
        engine.setInstrument(aculyre::gutLyre);
        aculyre::PlayingControls controls = engine.controls();
        controls.release = 0.0;
        controls.sympathy = 0.0;
        engine.setControls(controls);
        check(engine.idle(), "a fresh engine sleeps");
        std::vector<float> l(24000), r(24000);
        engine.noteOn(57, 0.8f);
        check(!engine.idle(), "a pluck wakes the engine");
        engine.process(l.data(), r.data(), 4800);
        engine.noteOff(57);
        double asleep = -1.0;
        for (int half = 1; half <= 40 && asleep < 0.0; ++half)
        {
            engine.process(l.data(), r.data(), 24000);
            if (engine.idle())
                asleep = 0.5 * half;
        }
        std::printf("a stopped A3 sends the gut lyre to sleep after %.1f s\n", asleep);
        check(asleep > 0.0, "the engine sleeps once everything has died away");
        engine.process(l.data(), r.data(), 24000);
        check(rms(l, 0, 24000) == 0.0 && rms(r, 0, 24000) == 0.0, "a sleeping engine writes silence");
        engine.noteOn(57, 0.8f);
        engine.process(l.data(), r.data(), 24000);
        bool finite = true;
        for (float sample : l)
            finite = finite && std::isfinite(sample);
        check(finite && rms(l, 0, 24000) > 1.0e-3, "a pluck after sleeping sounds");
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

    // Oversampling: the factor stays within the engine's 192 kHz, the
    // decimated note keeps its pitch and level, and the output does not
    // depend on the block size.
    {
        aculyre::Performer performer;
        const auto factor = [&performer] (double rate, int requested)
        {
            performer.prepare(rate, requested);
            return performer.oversampling();
        };
        check(factor(48000.0, 4) == 4 && factor(44100.0, 2) == 2 && factor(48000.0, 3) == 2
                  && factor(96000.0, 4) == 2 && factor(192000.0, 4) == 1 && factor(48000.0, 0) == 1,
              "oversampling is 1, 2 or 4 and keeps the engine at or below 192 kHz");
        check(performer.latency() == 96 && factor(48000.0, 2) == 2 && performer.latency() == 96 + 31,
              "the decimator delays by 31 host samples, and 1x by none, after the limiter's 96");

        const auto play = [] (int oversampling, int block)
        {
            auto performer = std::make_unique<aculyre::Performer>();
            performer->prepare(48000.0, oversampling);
            aculyre::PlayingControls controls = aculyre::Performer::defaultControls(0);
            controls.room = 0.0;
            performer->setControls(controls);
            performer->noteOn(69, 0.7f);
            std::vector<float> l(48000), r(48000);
            for (int position = 0; position < 48000; position += block)
                performer->process(l.data() + position, r.data() + position, std::min(block, 48000 - position));
            return l;
        };
        const auto once = play(1, 64);
        for (int oversampling : { 2, 4 })
        {
            const auto fast = play(oversampling, 64);
            bool finite = true;
            for (float sample : fast)
                finite = finite && std::isfinite(sample);
            check(finite, "the oversampled output is finite");
            const double cents = 1200.0 * std::log2(peakNear(fast, 9600, 16384, 440.0).frequency
                                                     / peakNear(once, 9600, 16384, 440.0).frequency);
            check(std::abs(cents) < 2.0, "oversampling keeps the note's pitch (within 2 cents)");
            const double db = 20.0 * std::log10(rms(fast, 4800, 19200) / rms(once, 4800, 19200));
            check(std::abs(db) < 1.0, "oversampling keeps the note's level (within 1 dB)");
            check(fast == play(oversampling, 37), "the oversampled output does not depend on the block size");
        }
    }

    // Level, as Reason's VU meter reads it: the RMS over 300 ms, 0 VU at
    // -12 dBFS (the Big Meter's default offset). Typical playing, each
    // string alone and three- and four-note chords at velocities 70, 90 and
    // 110, has a median of -7 VU on each instrument at its own controls
    // (outputGain), and the piezo alone reads within 1 dB of the
    // microphones (piezoGain). The hardest playing, chords of up to six
    // strings at full velocity with a plectrum, stays out of the red: under
    // 0 VU, every sample under -1.5 dBFS. The piezo is mono and hears no
    // room.
    {
        struct Reading
        {
            double vu, peak;
            std::vector<float> left, right;
        };
        const auto play = [] (aculyre::Performer& performer, const std::vector<int>& notes, int velocity,
                              double seconds)
        {
            performer.reset();
            for (int note : notes)
                performer.noteOn(note, static_cast<float>(velocity) / 127.0f);
            const auto total = static_cast<std::size_t>(seconds * 48000.0);
            Reading reading { 0.0, 0.0, std::vector<float>(total), std::vector<float>(total) };
            for (std::size_t position = 0; position < total; position += 64)
                performer.process(reading.left.data() + position, reading.right.data() + position,
                                  static_cast<int>(std::min<std::size_t>(64, total - position)));
            const std::size_t window = 14400;
            double sum = 0.0, loudest = 0.0, peak = 0.0;
            for (std::size_t i = 0; i < total; ++i)
            {
                const auto power = [&reading] (std::size_t k)
                {
                    return 0.5 * (static_cast<double>(reading.left[k]) * reading.left[k]
                                  + static_cast<double>(reading.right[k]) * reading.right[k]);
                };
                sum += power(i) - (i >= window ? power(i - window) : 0.0);
                loudest = std::max(loudest, sum / static_cast<double>(window));
                peak = std::max({ peak, static_cast<double>(std::abs(reading.left[i])),
                                  static_cast<double>(std::abs(reading.right[i])) });
            }
            reading.vu = 10.0 * std::log10(loudest + 1e-30) + 12.0;
            reading.peak = 20.0 * std::log10(peak + 1e-30);
            return reading;
        };
        const auto median = [] (std::vector<double> values)
        {
            std::sort(values.begin(), values.end());
            return values[values.size() / 2];
        };
        for (int index = 0; index < aculyre::Performer::instrumentCount; ++index)
        {
            const aculyre::InstrumentSpec& preset = *aculyre::presets[static_cast<std::size_t>(index)];
            const int count = preset.stringCount;
            std::vector<int> strings;
            for (int i = 0; i < count; ++i)
                strings.push_back(preset.strings[static_cast<std::size_t>(i)].midiNote);
            auto performer = std::make_unique<aculyre::Performer>();
            performer->prepare(48000.0);
            performer->setInstrument(index);
            const auto typical = [&] (double piezo)
            {
                aculyre::PlayingControls controls = aculyre::Performer::defaultControls(index);
                controls.piezo = piezo;
                performer->setControls(controls);
                std::vector<double> readings;
                for (int velocity : { 70, 90, 110 })
                {
                    for (int note : strings)
                        readings.push_back(play(*performer, { note }, velocity, 0.8).vu);
                    for (int i = 0; i + 4 < count; i += 2)
                    {
                        readings.push_back(play(*performer, { strings[i], strings[i + 2], strings[i + 4] },
                                                velocity, 0.8).vu);
                        if (i + 6 < count)
                            readings.push_back(play(*performer, { strings[i], strings[i + 2], strings[i + 4],
                                                                  strings[i + 6] }, velocity, 0.8).vu);
                    }
                }
                return median(readings);
            };
            const double mics = typical(0.0), pickup = typical(1.0);
            double redVu = -99.0, redPeak = -99.0;
            for (double piezo : { 0.0, 1.0 })
            {
                aculyre::PlayingControls controls = aculyre::Performer::defaultControls(index);
                controls.technique = aculyre::Technique::Plectrum;
                controls.piezo = piezo;
                performer->setControls(controls);
                std::vector<std::vector<int>> chords;
                for (int i = 0; i + 6 < count; ++i)
                    chords.push_back({ strings[i], strings[i + 2], strings[i + 4], strings[i + 6] });
                for (int i = 0; i + 11 < count; ++i)
                    chords.push_back({ strings[i], strings[i + 2], strings[i + 4], strings[i + 7], strings[i + 9],
                                       strings[i + 11] });
                if (chords.empty())
                    chords.push_back(strings);
                for (const auto& chord : chords)
                {
                    const Reading reading = play(*performer, chord, 127, 1.0);
                    redVu = std::max(redVu, reading.vu);
                    redPeak = std::max(redPeak, reading.peak);
                }
            }
            std::printf("%-16s typical: microphones %+.1f VU, piezo %+.1f VU; hardest chords %+.1f VU, peak %+.1f dBFS\n",
                        preset.name, mics, pickup, redVu, redPeak);
            check(std::abs(mics + 7.0) < 0.5, "typical playing reads -7 VU on the microphones");
            check(std::abs(pickup - mics) <= 1.0, "the piezo reads within 1 dB of the microphones");
            check(redVu < 0.0, "the hardest chords stay under 0 VU");
            check(redPeak <= -1.5 + 1e-6, "no sample passes -1.5 dBFS");

            // Fresh Performers, so both draw the same plucks.
            const auto pickupAlone = [&] (double room)
            {
                auto fresh = std::make_unique<aculyre::Performer>();
                fresh->prepare(48000.0);
                fresh->setInstrument(index);
                aculyre::PlayingControls controls = aculyre::Performer::defaultControls(index);
                controls.piezo = 1.0;
                controls.room = room;
                fresh->setControls(controls);
                return play(*fresh, { strings[0], strings[count / 2] }, 100, 0.5);
            };
            const Reading dry = pickupAlone(0.0);
            check(dry.left == dry.right, "the piezo is mono");
            check(pickupAlone(1.0).left == dry.left, "the piezo hears no room");
        }
    }

    // The output stage leaves quiet playing bit for bit as the engine makes
    // it, only later by the look-ahead, and holds a far too loud one under
    // -1.5 dBFS.
    {
        aculyre::Performer performer;
        performer.prepare(48000.0);
        const int lookahead = performer.latency();
        check(lookahead == 96, "the limiter looks 2 ms ahead");
        aculyre::AculyreEngine engine;
        engine.prepare(48000.0);
        engine.setInstrument(aculyre::lyreHarpSteel);
        const aculyre::PlayingControls controls = aculyre::Performer::defaultControls(0);
        engine.setControls(controls);
        engine.reset();
        performer.setControls(controls);
        engine.noteOn(69, 0.5f);
        performer.noteOn(69, 0.5f);
        std::vector<float> l(24000), r(24000), el(24000), er(24000);
        for (int position = 0; position < 24000; position += 64)
        {
            performer.process(l.data() + position, r.data() + position, 64);
            engine.process(el.data() + position, er.data() + position, 64);
        }
        bool same = true;
        for (int i = 0; i + lookahead < 24000; ++i)
            same = same && l[static_cast<std::size_t>(i + lookahead)] == el[static_cast<std::size_t>(i)];
        check(same, "the output stage passes quiet playing through untouched");

        aculyre::PlayingControls loud = controls;
        loud.volume = 4.0;
        performer.reset();
        performer.setControls(loud);
        for (int note : { 55, 59, 62, 67, 71, 74 })
            performer.noteOn(note, 1.0f);
        double peak = 0.0;
        for (int position = 0; position < 24000; position += 64)
        {
            performer.process(l.data() + position, r.data() + position, 64);
            for (int i = position; i < position + 64; ++i)
                peak = std::max({ peak, static_cast<double>(std::abs(l[static_cast<std::size_t>(i)])),
                                  static_cast<double>(std::abs(r[static_cast<std::size_t>(i)])) });
        }
        check(peak > 0.8 && peak <= 0.8414, "the limiter holds the hardest chord 12 dB too loud under -1.5 dBFS");
    }

    std::printf(failures == 0 ? "all passed\n" : "%d failed\n", failures);
    return failures == 0 ? 0 : 1;
}
