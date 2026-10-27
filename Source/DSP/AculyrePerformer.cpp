#include "AculyrePerformer.h"

#include <algorithm>

namespace aculyre
{

Performer::Performer()
{
    for (auto& engine : engines_)
        engine = std::make_unique<AculyreEngine>();
    controls_ = defaultControls(0);
}

Performer::~Performer() = default;

PlayingControls Performer::defaultControls(int index) noexcept
{
    const InstrumentSpec& spec = *presets[static_cast<std::size_t>(std::clamp(index, 0, instrumentCount - 1))];
    PlayingControls controls {};
    controls.technique = spec.technique;
    controls.pluckPosition = spec.pluckPosition;
    controls.normalShare = spec.normalShare;
    controls.room = spec.room;
    return controls;
}

void Performer::prepare(double sampleRate)
{
    for (int index = 0; index < instrumentCount; ++index)
    {
        AculyreEngine& engine = *engines_[static_cast<std::size_t>(index)];
        engine.prepare(sampleRate);
        engine.setInstrument(*presets[static_cast<std::size_t>(index)]);
        engine.setControls(controls_);
        engine.reset();
    }
    fadeLength_ = std::max(1, static_cast<int>(0.02 * engines_[0]->sampleRate()));
    fading_ = -1;
}

void Performer::reset() noexcept
{
    for (auto& engine : engines_)
        engine->reset();
    fading_ = -1;
}

void Performer::setInstrument(int index) noexcept
{
    index = std::clamp(index, 0, instrumentCount - 1);
    if (index == active_)
        return;
    // A second switch within the fade cuts the tail that was fading.
    if (fading_ >= 0 && fading_ != index)
        engines_[static_cast<std::size_t>(fading_)]->reset();
    fading_ = active_;
    active_ = index;
    fadeRemaining_ = fadeLength_;
}

void Performer::setControls(const PlayingControls& controls) noexcept
{
    controls_ = controls;
    for (auto& engine : engines_)
        engine->setControls(controls);
}

void Performer::noteOn(int midiNote, float velocity) noexcept
{
    engines_[static_cast<std::size_t>(active_)]->noteOn(midiNote, velocity);
}

void Performer::noteOff(int midiNote) noexcept
{
    engines_[static_cast<std::size_t>(active_)]->noteOff(midiNote);
}

void Performer::setSustain(bool down) noexcept
{
    for (auto& engine : engines_)
        engine->setSustain(down);
}

void Performer::process(float* left, float* right, int numSamples) noexcept
{
    engines_[static_cast<std::size_t>(active_)]->process(left, right, numSamples);
    int done = 0;
    while (fading_ >= 0 && done < numSamples)
    {
        constexpr int chunk = 64;
        float fadeLeft[chunk], fadeRight[chunk];
        const int count = std::min({ chunk, numSamples - done, fadeRemaining_ });
        AculyreEngine& fading = *engines_[static_cast<std::size_t>(fading_)];
        fading.process(fadeLeft, fadeRight, count);
        for (int i = 0; i < count; ++i)
        {
            const float gain = static_cast<float>(fadeRemaining_ - i) / static_cast<float>(fadeLength_);
            left[done + i] += gain * fadeLeft[i];
            right[done + i] += gain * fadeRight[i];
        }
        done += count;
        fadeRemaining_ -= count;
        if (fadeRemaining_ <= 0)
        {
            fading.reset();
            fading_ = -1;
        }
    }
}

} // namespace aculyre
