#include "AculyrePerformer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

int main()
{
    int failures = 0;
    const auto check = [&](bool okay, const char* message)
    { if (!okay) { std::printf("FAIL: %s\n", message); ++failures; } };
    for (int factor : { 1, 2, 4 })
    {
        auto performer = std::make_unique<aculyre::Performer>();
        performer->prepare(48000.0, factor);
        performer->setInstrument(2);
        protocodus::OutputStage<2> stage;
        stage.Prepare(48000.0, protocodus::kVuGuard);
        const int circuitHostDelay = factor == 1 ? 7 : factor == 2 ? 4 : 2;
        check(performer->latency() == stage.DelaySamples() + (factor > 1 ? 31 : 0) + circuitHostDelay,
              "reported latency includes circuit and host-sample alignment");
        const auto render = [&](int block)
        {
            auto controls = aculyre::Performer::defaultControls(2);
            controls.room = 0.0;
            controls.piezo = 0.5;
            controls.normalShare = 0.9;
            controls.pluckPosition = 0.7;
            controls.technique = aculyre::Technique::Plectrum;
            performer->setControls(controls);
            performer->reset();
            for (int i = 0; i < 6; ++i) performer->noteOn(aculyre::gutLyre.strings[i].midiNote, 1.0f);
            std::array<std::vector<float>, 2> audio {std::vector<float>(16384), std::vector<float>(16384)};
            for (int frame = 0; frame < 16384;)
            {
                if (frame == 4096)
                {
                    controls.piezoInverted = true;
                    performer->setControls(controls);
                }
                const int count = std::min({block, 16384-frame, frame < 4096 ? 4096-frame : 16384-frame});
                performer->process(audio[0].data()+frame, audio[1].data()+frame, count);
                frame += count;
            }
            return audio;
        };
        const auto first = render(64), partitioned = render(37), reset = render(64);
        check(first == partitioned, "electrical and fractional-host alignment are block invariant");
        check(first == reset, "reset clears circuit, microphone and decimation-alignment histories");
        for (const auto& channel : first)
            for (float sample : channel)
                check(std::isfinite(sample) && std::abs(sample) <= protocodus::OutputStage<2>::kCeiling*(1.0+1e-6),
                      "an extreme valid pickup blend stays finite and under the output ceiling");
    }
    std::printf(failures == 0 ? "all passed\n" : "%d failed\n", failures);
    return failures == 0 ? 0 : 1;
}
