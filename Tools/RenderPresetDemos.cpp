// Renders the README demos (G3) from the committed phrases in Docs/audio/midi/
// through the Performer a front end plays, with the shared renderer in
// Tools/PresetDemoRenderer.h (the portfolio's copy; do not edit it here):
//
//   RenderAculyrePresetDemos [--more] [--jobs N] Docs/audio/midi[/more] <out-dir>
//   RenderAculyrePresetDemos [--more] [--jobs N] --check Docs/audio/midi[/more]
//   RenderAculyrePresetDemos --cpu [--repeats N] [--json FILE] [--note TEXT] Docs/audio/midi
//
// One demo per preset, each played as a freshly loaded instrument plays it:
// prepared at the lowest oversampling (U13), the preset chosen, and its own
// playing controls (Performer::defaultControls), with no per-demo trim.
// --more renders the README's More demos (G3d) instead: what the preset
// demos do not show, each from Docs/audio/midi/more/<NN>-<slug>.mid through
// the preset the table below names. Note on and off and CC64 (sustain) reach
// the Performer at their sample; every other message is ignored, as
// RenderAculyreDemo ignores it. Unlike RenderAculyreDemo, which renders the
// wider listening set (Tools/render_demo_set.sh), these keep the Performer's
// few samples of latency at the start, as a host without delay compensation
// would play them.

#include "../Source/DSP/AculyrePerformer.h"
#include "../Source/DSP/AculyrePresets.h"
#include "PresetDemoRenderer.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace
{

namespace demos = protocodus::demos;

struct Showcase
{
    const char* name;
    int preset;
};

// The order is the file numbering: renaming one renames its files. Every
// phrase is written for the lyre harps' strings (Tools/write_preset_midi.py).
constexpr Showcase showcases[] {
    { "Single Notes", 0 }, // decay, the two-stage sustain and the strings ringing in sympathy
    { "Glissando", 0 },    // a sweep over the strings and back, then a rolled chord
    { "Dynamics", 0 },     // one string from the softest pluck to the hardest
};

class LyreVoice final : public demos::Voice
{
public:
    LyreVoice(int preset, double sampleRate)
    {
        performer->prepare(sampleRate);
        performer->setInstrument(preset);
        performer->setControls(aculyre::Performer::defaultControls(preset));
    }

    void Midi(const std::uint8_t* bytes, int size) override
    {
        const int kind = bytes[0] & 0xf0;
        const int data1 = size > 1 ? bytes[1] : 0;
        const int data2 = size > 2 ? bytes[2] : 0;
        if (kind == 0x90 && data2 > 0)
            performer->noteOn(data1, static_cast<float>(data2) / 127.0f);
        else if (kind == 0x80 || kind == 0x90)
            performer->noteOff(data1);
        else if (kind == 0xb0 && data1 == 64)
            performer->setSustain(data2 >= 64);
    }

    void Render(float* left, float* right, int frames) override { performer->process(left, right, frames); }

private:
    std::unique_ptr<aculyre::Performer> performer = std::make_unique<aculyre::Performer>();
};

} // namespace

int main(int argc, char** argv)
{
    const bool more = argc > 1 && std::strcmp(argv[1], "--more") == 0;
    if (more)
    {
        argv[1] = argv[0];
        ++argv;
        --argc;
    }

    demos::Instrument instrument;
    instrument.name = "Aculyre";
    // The output stage's limiter holds every sample at -1.5 dBFS and the
    // Performer's final reserve takes 0.6 dB off that (AculyrePerformer.h);
    // the margin is the float rounding of that last gain.
    instrument.ceiling = protocodus::OutputStage<2>::kCeiling * aculyre::Performer::finalHeadroomGain * (1.0f + 1.0e-6f);
    std::vector<int> presetFor;
    if (more)
        for (const auto& showcase : showcases)
        {
            instrument.presets.emplace_back(showcase.name);
            presetFor.push_back(showcase.preset);
        }
    else
        for (std::size_t preset = 0; preset < aculyre::presets.size(); ++preset)
        {
            instrument.presets.emplace_back(aculyre::presets[preset]->name);
            presetFor.push_back(static_cast<int>(preset));
        }
    // U7 perf budget: about 3x the slowest demo's median realtime factor on
    // the CI runner, 0.0391 (02-nylon-lyre-harp) in ci.yml run 4
    // (ubuntu-24.04, three repeats), 2026-11-15. The worst block there was
    // 5.8% of a core, under U12's 10%.
    instrument.cpuBudget = 0.12;
    instrument.open = [presetFor](std::size_t demo, double sampleRate, int, std::uint32_t)
    {
        // The engine's plucks vary by a fixed-seed sequence of its own that
        // starts again at every reset, which prepare runs, so it takes no seed.
        return std::make_unique<LyreVoice>(presetFor[demo], sampleRate);
    };
    return demos::Main(argc, argv, instrument);
}
