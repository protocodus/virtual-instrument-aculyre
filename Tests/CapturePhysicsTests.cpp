#include "AculyreEngine.h"
#include "AculyrePresets.h"
#include <array>
#include <cstdio>
#include <memory>

int main()
{
    int failures = 0;
    const auto check = [&](bool value, const char* message)
    {
        if (!value) { std::printf("FAIL: %s\n", message); ++failures; }
    };
    for (const auto* preset : aculyre::presets)
    {
        std::array<std::unique_ptr<aculyre::AculyreEngine>, 3> engines;
        for (std::size_t i = 0; i < engines.size(); ++i)
        {
            engines[i] = std::make_unique<aculyre::AculyreEngine>();
            engines[i]->prepare(48000.0);
            engines[i]->setInstrument(*preset);
            auto controls = engines[i]->controls();
            controls.levers = true;
            controls.piezo = 0.5 * i;
            controls.room = 0.0;
            engines[i]->setControls(controls);
        }
        float left[64], right[64];
        for (int block = 0; block < 1600; ++block)
        {
            if (block % 150 == 0)
            {
                const int string = (block / 150 * 7) % preset->stringCount;
                const int key = preset->strings[string].midiNote + (block % 300 ? 1 : 0);
                for (auto& engine : engines) engine->noteOn(key, block % 300 ? 0.43f : 0.83f);
                const auto reference = engines[0]->lastPluck();
                for (std::size_t i = 1; i < engines.size(); ++i)
                {
                    const auto other = engines[i]->lastPluck();
                    check(other.force == reference.force && other.position == reference.position
                              && other.normalShare == reference.normalShare && other.release == reference.release,
                          "capture leaves the same player's pluck unchanged");
                }
            }
            if (block == 600)
                for (std::size_t i = 0; i < engines.size(); ++i)
                {
                    auto controls = engines[i]->controls();
                    controls.piezo = 1.0 - 0.5 * i;
                    controls.piezoInverted = i != 0;
                    engines[i]->setControls(controls);
                }
            for (auto& engine : engines) engine->process(left, right, 64);
            for (int string = 0; string < preset->stringCount; ++string)
                for (std::size_t i = 1; i < engines.size(); ++i)
                {
                    check(engines[i]->stringEnergy(string) == engines[0]->stringEnergy(string),
                          "capture leaves mechanical string energy observers bit-identical");
                    for (bool parallel : { false, true })
                        check(engines[i]->contactReport(string, parallel).incomingWaveVelocity
                                  == engines[0]->contactReport(string, parallel).incomingWaveVelocity,
                              "capture leaves both mechanical travelling waves bit-identical");
                }
        }
    }
    std::printf(failures == 0 ? "all passed\n" : "%d failed\n", failures);
    return failures == 0 ? 0 : 1;
}
