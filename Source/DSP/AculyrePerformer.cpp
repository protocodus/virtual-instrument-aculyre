#include "AculyrePerformer.h"

#include <algorithm>
#include <cmath>

namespace aculyre
{

Performer::Performer()
{
    for (auto& engine : engines_)
        engine = std::make_unique<AculyreEngine>();
    controls_ = defaultControls(0);
}

Performer::~Performer() = default;

namespace
{
// The portfolio's kVuGuard follows each sample's power with an 80 ms attack,
// so a chord's first tens of milliseconds set its gain for the whole 300 ms
// the meter reads, at 10:1: chords at velocities 90, 110 and 127 read within
// 0.5 dB of one another, and the gut lyre's harder chords read quieter.
// Aculyre's guard reads the power averaged over the meter's own 300 ms and
// bends over a 16 dB knee (-11 to +5 VU), so harder playing still reads
// louder while the factory patches' hardest chords stay at -1.4 VU or less.
constexpr protocodus::CompressorSettings guard { -15.0, 16.0, 10.0, 0.3, 0.01, 0.3 };
} // namespace

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

void Performer::prepare(double sampleRate, int oversampling)
{
    factor_ = 1;
    while (factor_ * 2 <= std::min(oversampling, maxOversampling) && sampleRate * factor_ * 2 <= maxEngineRate)
        factor_ *= 2;
    for (auto& decimator : decimators_)
        decimator.design(factor_);
    stage_.Prepare(sampleRate, guard);
    stageSilence_ = stage_.DelaySamples() + 1;
    for (int index = 0; index < instrumentCount; ++index)
    {
        AculyreEngine& engine = *engines_[static_cast<std::size_t>(index)];
        engine.prepare(sampleRate * factor_);
        engine.setInstrument(*presets[static_cast<std::size_t>(index)]);
        engine.setControls(controls_);
        engine.reset();
    }
    fadeLength_ = std::max(1, static_cast<int>(0.02 * engines_[0]->sampleRate()));
    fading_ = -1;
    silentPushes_ = 0;
}

void Performer::reset() noexcept
{
    for (auto& engine : engines_)
        engine->reset();
    for (auto& decimator : decimators_)
        decimator.clear();
    stage_.Reset();
    stageSilence_ = stage_.DelaySamples() + 1;
    fading_ = -1;
    silentPushes_ = 0;
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
    if (factor_ == 1)
    {
        render(left, right, numSamples);
        finish(left, right, numSamples);
        return;
    }
    if (fading_ < 0 && engines_[static_cast<std::size_t>(active_)]->idle()
        && silentPushes_ >= decimators_[0].length && stageResting())
    {
        std::fill(left, left + numSamples, 0.0f);
        std::fill(right, right + numSamples, 0.0f);
        return;
    }
    const int chunk = static_cast<int>(fast_[0].size()) / factor_;
    for (int done = 0; done < numSamples;)
    {
        const int count = std::min(chunk, numSamples - done);
        const bool silent = fading_ < 0 && engines_[static_cast<std::size_t>(active_)]->idle();
        render(fast_[0].data(), fast_[1].data(), count * factor_);
        silentPushes_ = silent ? silentPushes_ + count * factor_ : 0;
        for (int i = 0; i < count; ++i)
        {
            for (int k = 0; k < factor_; ++k)
            {
                decimators_[0].push(fast_[0][static_cast<std::size_t>(i * factor_ + k)]);
                decimators_[1].push(fast_[1][static_cast<std::size_t>(i * factor_ + k)]);
            }
            left[done + i] = static_cast<float>(decimators_[0].output());
            right[done + i] = static_cast<float>(decimators_[1].output());
        }
        done += count;
    }
    finish(left, right, numSamples);
}

void Performer::finish(float* left, float* right, int numSamples) noexcept
{
    const int delay = stage_.DelaySamples();
    for (int i = 0; i < numSamples; ++i)
    {
        if (left[i] == 0.0f && right[i] == 0.0f)
        {
            if (stageResting())
                continue;
            // The delay now holds only silence, so this frame's output is
            // silence too.
            if (++stageSilence_ > delay)
            {
                stage_.Reset();
                continue;
            }
        }
        else
            stageSilence_ = 0;
        std::array<float, 2> frame { left[i], right[i] };
        stage_.Process(frame);
        left[i] = frame[0];
        right[i] = frame[1];
    }
}

void Performer::render(float* left, float* right, int numSamples) noexcept
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

void Performer::Decimator::design(int factor) noexcept
{
    length = factor > 1 ? 2 * decimatorDelay * factor + 1 : 1;
    if (factor == 1)
    {
        taps[0] = 1.0;
        clear();
        return;
    }
    // Cut off at 90% of the host Nyquist; beta 10 keeps the stopband
    // about 100 dB down.
    constexpr double pi = 3.14159265358979323846, beta = 10.0;
    const auto besselI0 = [] (double x)
    {
        double term = 1.0, sum = 1.0;
        for (int k = 1; k < 40 && term > sum * 1.0e-16; ++k)
        {
            term *= x * x * 0.25 / (static_cast<double>(k) * k);
            sum += term;
        }
        return sum;
    };
    const double cutoff = 0.45 / factor, middle = (length - 1) * 0.5;
    double sum = 0.0;
    for (int i = 0; i < length; ++i)
    {
        const double n = i - middle, r = n / middle;
        const double sinc = n == 0.0 ? 2.0 * cutoff : std::sin(2.0 * pi * cutoff * n) / (pi * n);
        taps[static_cast<std::size_t>(i)] = sinc * besselI0(beta * std::sqrt(std::max(0.0, 1.0 - r * r)));
        sum += taps[static_cast<std::size_t>(i)];
    }
    for (int i = 0; i < length; ++i)
        taps[static_cast<std::size_t>(i)] /= sum;
    clear();
}

void Performer::Decimator::clear() noexcept
{
    history.fill(0.0);
    cursor = 0;
}

void Performer::Decimator::push(double x) noexcept
{
    cursor = (cursor == 0 ? length : cursor) - 1;
    history[static_cast<std::size_t>(cursor)] = x;
    history[static_cast<std::size_t>(cursor + length)] = x;
}

double Performer::Decimator::output() const noexcept
{
    const double* window = history.data() + cursor;
    double sum = 0.0;
    for (int i = 0; i < length; ++i)
        sum += taps[static_cast<std::size_t>(i)] * window[i];
    return sum;
}

} // namespace aculyre
