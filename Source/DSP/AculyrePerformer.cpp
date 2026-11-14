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
    endFades();
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

void Performer::prepare(double sampleRate, int oversampling)
{
    sampleRate = std::isfinite(sampleRate) && sampleRate >= 22050.0 && sampleRate <= maxEngineRate
        ? sampleRate : 48000.0;
    factor_ = 1;
    while (factor_ * 2 <= std::min(oversampling, maxOversampling) && sampleRate * factor_ * 2 <= maxEngineRate)
        factor_ *= 2;
    for (auto& decimator : decimators_)
        decimator.design(factor_);
    alignmentSamples_ = (factor_ - AculyreEngine::latencySamples % factor_) % factor_;
    alignment_ = {};
    alignmentCursor_ = 0;
    stage_.Prepare(sampleRate, protocodus::kVuGuard);
    stageSilence_ = stage_.DelaySamples() + 1;
    for (int index = 0; index < instrumentCount; ++index)
    {
        AculyreEngine& engine = *engines_[static_cast<std::size_t>(index)];
        const bool hadInstrument = engine.stringCount() > 0;
        engine.prepare(sampleRate * factor_);
        if (!hadInstrument)
            engine.setInstrument(*presets[static_cast<std::size_t>(index)]);
        engine.setControls(controls_);
        engine.reset();
    }
    fadeLength_ = std::max(1, static_cast<int>(0.02 * engines_[0]->sampleRate()));
    endFades();
    silentPushes_ = 0;
}

void Performer::endFades() noexcept
{
    level_.fill(0);
    level_[static_cast<std::size_t>(active_)] = fadeLength_;
    fading_ = false;
}

void Performer::reset() noexcept
{
    for (auto& engine : engines_)
        engine->reset();
    for (auto& decimator : decimators_)
        decimator.clear();
    alignment_ = {};
    alignmentCursor_ = 0;
    stage_.Reset();
    stageSilence_ = stage_.DelaySamples() + 1;
    endFades();
    silentPushes_ = 0;
}

void Performer::setInstrument(int index) noexcept
{
    index = std::clamp(index, 0, instrumentCount - 1);
    if (index == active_)
        return;
    // A silent instrument starts at once. One still fading from a moment
    // ago (a control turned there and back) comes back from where its fade
    // stands, and whatever else is fading goes on fading: until 0.13.5 a
    // second switch within the fade cut the first tail off in one sample.
    if (level_[static_cast<std::size_t>(index)] == 0)
        level_[static_cast<std::size_t>(index)] = fadeLength_;
    active_ = index;
    fading_ = true;
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
    for (auto& engine : engines_)
        engine->noteOff(midiNote);
}

void Performer::setSustain(bool down) noexcept
{
    for (auto& engine : engines_)
        engine->setSustain(down);
}

void Performer::process(float* left, float* right, int numSamples) noexcept
{
    if (numSamples <= 0)
        return;
    if (factor_ == 1)
    {
        render(left, right, numSamples);
        finish(left, right, numSamples);
        return;
    }
    if (!fading_ && engines_[static_cast<std::size_t>(active_)]->idle()
        && silentPushes_ >= decimators_[0].length + alignmentSamples_ && stageResting())
    {
        std::fill(left, left + numSamples, 0.0f);
        std::fill(right, right + numSamples, 0.0f);
        return;
    }
    const int chunk = static_cast<int>(fast_[0].size()) / factor_;
    for (int done = 0; done < numSamples;)
    {
        const int count = std::min(chunk, numSamples - done);
        const bool silent = !fading_ && engines_[static_cast<std::size_t>(active_)]->idle();
        render(fast_[0].data(), fast_[1].data(), count * factor_);
        silentPushes_ = silent ? silentPushes_ + count * factor_ : 0;
        if (alignmentSamples_ > 0)
        {
            for (int i = 0; i < count; ++i)
            {
                for (int k = 0; k < factor_; ++k)
                {
                    const auto idx = static_cast<std::size_t>(i * factor_ + k);
                    const auto cur = static_cast<std::size_t>(alignmentCursor_);
                    for (std::size_t channel = 0; channel < 2; ++channel)
                    {
                        double& delayed = alignment_[channel][cur];
                        const double input = fast_[channel][idx];
                        const double value = delayed;
                        delayed = input;
                        decimators_[channel].push(value);
                    }
                    alignmentCursor_ = (alignmentCursor_ + 1) % alignmentSamples_;
                }
                downsample(left[done + i], right[done + i]);
            }
        }
        else
        {
            for (int i = 0; i < count; ++i)
            {
                for (int k = 0; k < factor_; ++k)
                {
                    const auto idx = static_cast<std::size_t>(i * factor_ + k);
                    decimators_[0].push(fast_[0][idx]);
                    decimators_[1].push(fast_[1][idx]);
                }
                downsample(left[done + i], right[done + i]);
            }
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
        std::array<float, 2> frame { digitalCalibrationGain * left[i], digitalCalibrationGain * right[i] };
        stage_.Process(frame);
        left[i] = finalHeadroomGain * frame[0];
        right[i] = finalHeadroomGain * frame[1];
    }
}

void Performer::render(float* left, float* right, int numSamples) noexcept
{
    const auto active = static_cast<std::size_t>(active_);
    if (level_[active] == fadeLength_)
        engines_[active]->process(left, right, numSamples);
    if (!fading_)
        return;
    // An instrument's share of the output is its level over the fade's
    // length: the active one's rises a step a sample, the others' fall.
    const auto mix = [&] (std::size_t index, int step)
    {
        int& level = level_[index];
        for (int done = 0; done < numSamples && (step > 0 || level > 0);)
        {
            constexpr int chunk = 64;
            float fadeLeft[chunk], fadeRight[chunk];
            const int count = std::min({ chunk, numSamples - done, step > 0 ? chunk : level });
            engines_[index]->process(fadeLeft, fadeRight, count);
            for (int i = 0; i < count; ++i)
            {
                const float gain = static_cast<float>(level) / static_cast<float>(fadeLength_);
                left[done + i] += gain * fadeLeft[i];
                right[done + i] += gain * fadeRight[i];
                level = std::min(level + step, fadeLength_);
            }
            done += count;
        }
        // A faded instrument is reset: its strings do not ring on unheard.
        if (step < 0 && level == 0)
            engines_[index]->reset();
    };
    if (level_[active] < fadeLength_)
    {
        std::fill(left, left + numSamples, 0.0f);
        std::fill(right, right + numSamples, 0.0f);
        mix(active, 1);
    }
    fading_ = level_[active] < fadeLength_;
    for (std::size_t index = 0; index < level_.size(); ++index)
        if (index != active && level_[index] > 0)
        {
            mix(index, -1);
            fading_ = fading_ || level_[index] > 0;
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

// The two channels keep independent sums in the original tap order.
// Parallelize channels, not taps: no reassociation of the FIR's recurrence.
void Performer::downsample(float& left,float& right) const noexcept
{
#if defined(__clang__) || defined(__GNUC__)
    typedef double Stereo __attribute__((vector_size(16)));
    const auto& a=decimators_[0];const auto& b=decimators_[1];
    const double* const __restrict taps=a.taps.data();
    const double* const __restrict l=a.history.data()+a.cursor;
    const double* const __restrict r=b.history.data()+b.cursor;
    const int len=a.length;
    Stereo sum {0.0,0.0};
#if defined(__clang__)
#pragma unroll 4
#endif
    for(int i=0;i<len;++i)
    {
        const double t=taps[i];
        sum+=Stereo{t,t}*Stereo{l[i],r[i]};
    }
    left=static_cast<float>(sum[0]);right=static_cast<float>(sum[1]);
#else
    left=static_cast<float>(decimators_[0].output());
    right=static_cast<float>(decimators_[1].output());
#endif
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
