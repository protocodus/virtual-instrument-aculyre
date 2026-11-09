// Whole-keyboard acoustic survey, through the engine without its output guard.
// Each key starts from reset, so every velocity/pickup uses the same draws.
// A note is held for 1.5 s, then released for 0.5 s; room is off, levers on.
// The CSV records stereo RMS and peak, including all unsupported MIDI keys.
// Optional interleaved float32 audio supports spectrum/decay analysis without
// imposing one supposedly universal resonance threshold on a physical body.
// Usage: AculyreKeyboardSurvey <preset 0-2> <velocity 1-127> <piezo 0-1>
//                            <csv-path> [raw-path] [sample-rate] [technique 0-2] [bend]

#include "AculyreEngine.h"
#include "AculyrePerformer.h"
#include "AculyrePresets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <memory>
#include <stdexcept>
#include <vector>

namespace
{
double power(const std::vector<float>& l, const std::vector<float>& r, int from, int to)
{
    double sum = 0.0;
    for (int i = from; i < to; ++i)
        sum += 0.5 * (static_cast<double>(l[i]) * l[i] + static_cast<double>(r[i]) * r[i]);
    return sum / (to - from);
}

double db(double power) { return 10.0 * std::log10(std::max(1.0e-30, power)); }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc < 5 || argc > 9)
            throw std::runtime_error("usage: AculyreKeyboardSurvey preset velocity piezo csv [raw] [rate] [technique] [bend]");
        const int preset = std::atoi(argv[1]);
        const int velocity = std::atoi(argv[2]);
        const double piezo = std::atof(argv[3]);
        const int rate = argc > 6 ? std::atoi(argv[6]) : 48000;
        const int technique = argc > 7 ? std::atoi(argv[7]) : 0;
        const double bend = argc > 8 ? std::atof(argv[8]) : 0.0;
        if (preset < 0 || preset > 2 || velocity < 1 || velocity > 127 || !std::isfinite(piezo)
            || piezo < 0.0 || piezo > 1.0 || rate < 22050 || rate > 192000 || technique < 0 || technique > 2
            || !std::isfinite(bend) || bend < -12.0 || bend > 12.0)
            throw std::runtime_error("invalid survey setting");
        std::ofstream csv(argv[4]);
        std::ofstream raw;
        if (argc > 5 && argv[5][0] != '-')
            raw.open(argv[5], std::ios::binary);
        if (!csv || (argc > 5 && argv[5][0] != '-' && !raw))
            throw std::runtime_error("cannot open survey output");
        csv << std::setprecision(10)
            << "preset,velocity,piezo,rate,technique,pitch_bend,note,supported,peak_dbfs,attack_dbfs,sustain_dbfs,late_dbfs,release_dbfs,finite\n";
        auto engine = std::make_unique<aculyre::AculyreEngine>();
        engine->prepare(rate);
        engine->setInstrument(*aculyre::presets[static_cast<std::size_t>(preset)]);
        auto controls = aculyre::Performer::defaultControls(preset);
        controls.room = 0.0;
        controls.levers = true;
        controls.piezo = piezo;
        controls.technique = static_cast<aculyre::Technique>(technique);
        controls.pitchBend = bend;
        engine->setControls(controls);
        const int total = 2 * rate;
        const int off = 3 * rate / 2;
        std::vector<float> l(total), r(total);
        std::array<float, 128> interleaved {};
        int failures = 0;
        for (int note = 0; note < 128; ++note)
        {
            engine->reset();
            engine->noteOn(note, static_cast<float>(velocity) / 127.0f);
            for (int position = 0; position < total;)
            {
                if (position == off)
                    engine->noteOff(note);
                const int next = position < off ? off : total;
                const int count = std::min(64, next - position);
                engine->process(l.data() + position, r.data() + position, count);
                if (raw.is_open())
                {
                    for (int i = 0; i < count; ++i)
                    {
                        interleaved[2 * i] = l[position + i];
                        interleaved[2 * i + 1] = r[position + i];
                    }
                    raw.write(reinterpret_cast<const char*>(interleaved.data()), count * 2 * sizeof(float));
                }
                position += count;
            }
            double peak = 0.0;
            bool finite = true;
            for (int i = 0; i < total; ++i)
            {
                finite = finite && std::isfinite(l[i]) && std::isfinite(r[i]);
                peak = std::max(peak, static_cast<double>(std::max(std::abs(l[i]), std::abs(r[i]))));
            }
            const bool supported = note >= 21 && note <= 108;
            const double attack = power(l, r, rate / 50, 3 * rate / 10);
            failures += !finite || (supported ? !(attack > 1.0e-20) : peak != 0.0);
            csv << preset << ',' << velocity << ',' << piezo << ',' << rate << ',' << technique << ',' << bend << ','
                << note << ',' << supported << ',' << db(peak * peak) << ',' << db(attack) << ','
                << db(power(l, r, rate / 2, rate)) << ',' << db(power(l, r, rate, off)) << ','
                << db(power(l, r, 9 * rate / 5, total)) << ',' << finite << '\n';
        }
        if (!csv || (raw.is_open() && !raw))
            throw std::runtime_error("could not write the complete survey");
        std::fprintf(stderr, "preset %d velocity %d piezo %.2f at %d Hz: 128 keys, %d failures\n",
                     preset, velocity, piezo, rate, failures);
        return failures ? 1 : 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 2;
    }
}
