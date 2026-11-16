#include "AculyreEngine.h"
#include "AculyrePresets.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

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
    // The piezo was under the finger's held force before the string slipped
    // off, so a pluck reaches it only when the released wave reaches the
    // bridge, a share `position` of half a period later. Until then it is
    // silent: to 0.13.11 it opened with a step of the whole held force.
    for (const auto* preset : aculyre::presets)
    {
        auto engine = std::make_unique<aculyre::AculyreEngine>();
        engine->prepare(48000.0);
        engine->setInstrument(*preset);
        auto controls = engine->controls();
        controls.piezo = 1.0;
        controls.room = 0.0;
        engine->setControls(controls);
        engine->noteOn(preset->strings[0].midiNote, 1.0f);
        const int arrival = static_cast<int>(engine->lastPluck().position * 48000.0
                                             / (2.0 * engine->stringReport(0).frequency));
        std::vector<float> left(4800), right(4800);
        engine->process(left.data(), right.data(), 4800);
        const int quiet = aculyre::AculyreEngine::latencySamples + arrival / 2;
        float before = 0.0f, peak = 0.0f;
        for (int i = 0; i < 4800; ++i)
        {
            float& loudest = i < quiet ? before : peak;
            loudest = std::max(loudest, std::abs(left[static_cast<std::size_t>(i)]));
        }
        check(arrival >= 8 && before < 0.01f * peak,
              "the piezo is silent until the plucked wave reaches the bridge");
    }
    std::printf(failures == 0 ? "all passed\n" : "%d failed\n", failures);
    return failures == 0 ? 0 : 1;
}
