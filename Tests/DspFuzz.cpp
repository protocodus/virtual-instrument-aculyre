// Bounded factory-engine state-machine fuzzing. Cache setup outside the
// callback sequence, then reset before each case for deterministic replay.
// Valid host rates include the two storage/interpolation corners that the
// 2026-11-09 range audit found missing. Output is the unlimited engine path;
// its check detects non-finite/runaway state, not an output-limiter ceiling.
#include "AculyreEngine.h"
#include "AculyrePresets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (size < 2)
        return 0;
    constexpr std::array<double, 3> rates { 22050.0, 48000.0, 192000.0 };
    static std::array<std::unique_ptr<aculyre::AculyreEngine>, 9> engines;
    const int preset = data[0] % 3, rate = data[1] % 3;
    auto& pointer = engines[static_cast<std::size_t>(3 * preset + rate)];
    const auto& spec = *aculyre::presets[static_cast<std::size_t>(preset)];
    if (!pointer)
    {
        pointer = std::make_unique<aculyre::AculyreEngine>();
        pointer->prepare(rates[static_cast<std::size_t>(rate)]);
        pointer->setInstrument(spec);
    }
    auto& engine = *pointer;
    engine.reset();
    engine.setTensionModulation(true);
    engine.setRealismEnhancements(true);
    aculyre::PlayingControls controls {};
    controls.technique = spec.technique;
    controls.pluckPosition = spec.pluckPosition;
    controls.normalShare = spec.normalShare;
    controls.room = spec.room;
    controls.levers = true;
    engine.setControls(controls);
    std::array<float, 256> left {}, right {};
    const auto render = [&] (int count)
    {
        engine.process(left.data(), right.data(), count);
        for (int sample = 0; sample < count; ++sample)
            if (!std::isfinite(left[sample]) || !std::isfinite(right[sample])
                || std::abs(left[sample]) > 1.0e6f || std::abs(right[sample]) > 1.0e6f)
                std::abort();
    };
    const std::size_t end = std::min<std::size_t>(size, 4096);
    for (std::size_t at = 2; at + 3 < end; at += 4)
    {
        const int value = data[at + 2];
        const double unit = value / 127.0;
        const int note = data[at + 1] == 255 ? std::numeric_limits<int>::min()
                                             : static_cast<int>(data[at + 1]) - 32;
        switch (data[at] % 8)
        {
        case 0:
            engine.noteOn(note, value == 255 ? std::numeric_limits<float>::quiet_NaN()
                                             : static_cast<float>(value) / 127.0f);
            break;
        case 1: engine.noteOff(note); break;
        case 2: engine.setSustain(value & 1); break;
        case 3:
            switch (data[at + 1] % 11)
            {
            case 0: controls.pitchBend = (value - 128) / 8.0; break;
            case 1: controls.mute = unit; break;
            case 2: controls.release = unit; break;
            case 3: controls.sympathy = unit; break;
            case 4: controls.pluckPosition = unit; break;
            case 5: controls.normalShare = unit; break;
            case 6: controls.room = unit; break;
            case 7: controls.piezo = unit; break;
            case 8: controls.volume = value / 32.0; break;
            case 9: controls.width = unit; break;
            case 10: controls.technique = static_cast<aculyre::Technique>(value % 3); break;
            }
            engine.setControls(controls);
            break;
        case 4: render(value + 1); break;
        case 5: engine.setTensionModulation(value & 1); break;
        case 6: engine.setRealismEnhancements(value & 1); break;
        case 7:
            controls.levers = value & 1;
            controls.piezoInverted = value & 2;
            engine.setControls(controls);
            break;
        }
        render(1 + data[at + 3] % 64);
    }
    render(256);
    return 0;
}
