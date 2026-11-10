#include "AculyreEngine.h"
#include "AculyrePresets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
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
    // A0 bent down one octave needs 13964 samples at the supported maximum
    // internal rate. Before 0.13.0 the initial pluck wrote beyond its 8192-
    // sample work array, while the live loop clipped its tap to that capacity.
    // Also cover the opposite corner (C8 + an octave at the lowest rate),
    // both techniques' release models, and a re-pluck while the bend is held.
    for (double rate : { 22050.0, 48000.0, 192000.0 })
    {
        auto engine = std::make_unique<aculyre::AculyreEngine>();
        engine->prepare(rate);
        for (const auto* preset : aculyre::presets)
        {
            engine->setInstrument(*preset);
            for (int note : { 21, 108 })
                for (double bend : { -12.0, 12.0 })
                    for (const auto technique : { aculyre::Technique::Finger, aculyre::Technique::Plectrum })
                    {
                        engine->reset();
                        auto controls = engine->controls();
                        controls.room = 0.0;
                        controls.levers = true;
                        controls.pitchBend = bend;
                        controls.technique = technique;
                        engine->setControls(controls);
                        engine->noteOn(note, 0.5f);
                        std::array<float, 64> l {}, r {};
                        double peak = 0.0;
                        bool finite = true;
                        const int blocks = static_cast<int>(rate * 0.2 / 64.0);
                        for (int block = 0; block < blocks; ++block)
                        {
                            if (block == blocks / 2)
                                engine->noteOn(note, 0.25f);
                            engine->process(l.data(), r.data(), 64);
                            for (int i = 0; i < 64; ++i)
                            {
                                finite = finite && std::isfinite(l[i]) && std::isfinite(r[i]);
                                peak = std::max({ peak, std::abs(static_cast<double>(l[i])), std::abs(static_cast<double>(r[i])) });
                            }
                        }
                        std::printf("%-16s rate %.0f note %d bend %+.0f technique %d: peak %.5g\n",
                                    preset->name, rate, note, bend, static_cast<int>(technique), peak);
                        check(finite && peak < 16.0, "extreme valid notes and re-plucks remain finite and bounded");
                        check(peak > 1.0e-8, "each supported keyboard corner produces sound");
                    }
        }
    }
    // Invalid MIDI integers are ignored before looking for a lever one
    // semitone lower (INT_MIN - 1 would itself be undefined arithmetic).
    aculyre::AculyreEngine engine;
    engine.prepare(48000.0);
    engine.setInstrument(aculyre::lyreHarpSteel);
    auto controls = engine.controls();
    controls.levers = true;
    engine.setControls(controls);
    for (int note : { std::numeric_limits<int>::min(), -1, 128, std::numeric_limits<int>::max() })
        engine.noteOn(note, 1.0f);
    check(engine.idle(), "invalid MIDI integers leave the instrument idle");

    // A live bend crosses the first-/second-order allpass boundary at low
    // rates. Frozen filters being stable does not by itself prove a safe
    // handoff. Traverse the entire bend range twice on a held high note,
    // with identical event times but different callback partitions.
    engine.prepare(22050.0);
    for (const auto* preset : aculyre::presets)
    {
        engine.setInstrument(*preset);
        const auto sweep = [&] (int blockSize)
        {
            engine.reset();
            auto c = engine.controls();
            c.room = 0.0;
            c.release = 1.0;
            c.pitchBend = -12.0;
            engine.setControls(c);
            engine.noteOn(108, 0.5f);
            constexpr int stepLength = 220, steps = 200;
            std::vector<float> left(stepLength * steps), right(left.size());
            for (int step = 0; step < steps; ++step)
            {
                const double phase = step < steps / 2 ? step / 100.0 : (steps - step) / 100.0;
                c.pitchBend = -12.0 + 24.0 * phase;
                engine.setControls(c);
                if (step > 0 && step % 10 == 0)
                {
                    engine.noteOff(108);
                    engine.noteOn(108, 0.5f);
                }
                const int start = step * stepLength;
                for (int position = 0; position < stepLength; position += blockSize)
                    engine.process(left.data() + start + position, right.data() + start + position,
                                   std::min(blockSize, stepLength - position));
            }
            left.insert(left.end(), right.begin(), right.end());
            for (float sample : left)
                check(std::isfinite(sample) && std::abs(sample) < 4.0f,
                      "live interpolation-order handoffs remain finite and bounded");
            return left;
        };
        const auto a = sweep(64), b = sweep(37);
        check(a == b, "live short-loop bends are bit-identical across callback partitions");
    }
    // The highest bend can expose Thiran phase error even when both frozen
    // allpasses are stable. Its two physical polarisations need not coincide:
    // at low rates the normal one dominates, at high rates the parallel one.
    // Require the spectral peak to lie between those designed frequencies,
    // with 2 cents for their finite-window interference. Previously the
    // 48 kHz steel/nylon peaks fell 6-8 cents below BOTH planes.
    engine.setTensionModulation(false);
    for (double rate : { 22050.0, 48000.0, 192000.0 })
    {
        engine.prepare(rate);
        for (const auto* preset : aculyre::presets)
        {
            engine.setInstrument(*preset);
            auto c = engine.controls();
            c.room = 0.0;
            c.pitchBend = 12.0;
            c.technique = aculyre::Technique::Finger;
            engine.setControls(c);
            engine.noteOn(108, 0.5f);
            const int samples = static_cast<int>(0.18 * rate);
            std::vector<float> left(samples), right(samples);
            for (int at = 0; at < samples; at += 64)
                engine.process(left.data() + at, right.data() + at, std::min(64, samples - at));
            const int string = engine.stringCount() - 1;
            const double normal = rate / engine.contactReport(string).period;
            const double parallel = rate / engine.contactReport(string, true).period;
            const double lower = std::min(normal, parallel), upper = std::max(normal, parallel);
            const int begin = static_cast<int>(0.02 * rate), end = static_cast<int>(0.15 * rate);
            constexpr double pi = 3.14159265358979323846;
            std::vector<double> windowLeft(end - begin), windowRight(end - begin);
            for (int k = begin; k < end; ++k)
            {
                const double window = 0.5 - 0.5 * std::cos(2.0 * pi * (k - begin) / (end - begin - 1));
                windowLeft[k - begin] = left[k] * window;
                windowRight[k - begin] = right[k] * window;
            }
            constexpr int bins = 257;
            const double start = lower * 0.98, step = (upper * 1.02 - start) / (bins - 1);
            std::array<double, bins> power {};
            for (int bin = 0; bin < bins; ++bin)
            {
                const double coefficient = 2.0 * std::cos(2.0 * pi * (start + step * bin) / rate);
                for (const auto* input : { &windowLeft, &windowRight })
                {
                    double a = 0.0, b = 0.0;
                    for (double x : *input) { const double next = x + coefficient * a - b; b = a; a = next; }
                    power[bin] += a * a + b * b - coefficient * a * b;
                }
            }
            const int peak = static_cast<int>(std::max_element(power.begin(), power.end()) - power.begin());
            check(peak > 0 && peak + 1 < bins, "short-loop frequency peak is inside the search interval");
            if (peak > 0 && peak + 1 < bins)
            {
                const double a = std::log(power[peak - 1]), b = std::log(power[peak]), d = std::log(power[peak + 1]);
                const double fraction = 0.5 * (a - d) / (a - 2.0 * b + d);
                const double frequency = start + step * (peak + fraction);
                const double allowance = std::pow(2.0, 2.0 / 1200.0);
                std::printf("%s %.0f Hz C8+12: peak %.3f Hz, planes %.3f / %.3f Hz\n",
                            preset->name, rate, frequency, normal, parallel);
                check(frequency >= lower / allowance && frequency <= upper * allowance,
                      "short-loop pitch stays within the two physical polarisation frequencies");
            }
        }
    }
    // A key above the top string is that string at the key's pitch, so its
    // two planes stand as far apart as the string's own (one diameter on its
    // length, a few cents) and both within that of the key. Until 0.13.5 the
    // diameter went onto the imagined shorter length: the gut lyre's C8
    // opened 28 cents sharp, the nylon harp's 14 and the steel's 8.
    engine.prepare(48000.0);
    engine.setTensionModulation(false);  // the periods read are the designs'
    for (const auto* preset : aculyre::presets)
    {
        engine.setInstrument(*preset);
        const auto& top = preset->strings[static_cast<std::size_t>(preset->stringCount - 1)];
        const double split = 1200.0 * std::log2(1.0 + top.diameter / top.length);
        double worst = 0.0;
        for (int key = top.midiNote + 1; key <= 108; ++key)
        {
            engine.reset();
            engine.noteOn(key, 0.5f);
            const double pitch = 440.0 * std::pow(2.0, (key - 69) / 12.0);
            for (bool parallel : { false, true })
                worst = std::max(worst, std::abs(1200.0 * std::log2(
                    48000.0 / engine.contactReport(preset->stringCount - 1, parallel).period / pitch)));
        }
        std::printf("%s: keys above the top string open within %.2f cents (its planes' split %.2f)\n",
                    preset->name, worst, split);
        check(worst <= split + 0.75, "a key above the top string opens within the string's own split of its pitch");
    }
    return failures ? 1 : 0;
}
