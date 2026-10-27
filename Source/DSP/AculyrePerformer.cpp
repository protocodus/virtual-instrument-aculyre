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
    leveller_.prepare(sampleRate);
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
    leveller_.clear();
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
        leveller_.process(left, right, numSamples);
        return;
    }
    if (fading_ < 0 && engines_[static_cast<std::size_t>(active_)]->idle()
        && silentPushes_ >= decimators_[0].length && leveller_.resting())
    {
        std::fill(left, left + numSamples, 0.0f);
        std::fill(right, right + numSamples, 0.0f);
        leveller_.process(left, right, numSamples);
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
    leveller_.process(left, right, numSamples);
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

void Performer::Leveller::prepare(double sampleRate) noexcept
{
    lookahead = std::clamp(static_cast<int>(std::lround(lookaheadSeconds * sampleRate)), 0, ring - 2);
    attack = 1.0 - std::exp(-1.0 / (attackSeconds * sampleRate));
    release = 1.0 - std::exp(-1.0 / (releaseSeconds * sampleRate));
    limiterRelease = std::exp(-1.0 / (limiterReleaseSeconds * sampleRate));
    clear();
}

void Performer::Leveller::clear() noexcept
{
    envelope = 0.0;
    held = 1.0;
    gains.fill(1.0);
    gainSum = lookahead + 1.0;
    for (auto& line : delay)
        line.fill(0.0f);
    queueFront = queueBack = 0;
    silent = lookahead + 1;
}

void Performer::Leveller::process(float* left, float* right, int numSamples) noexcept
{
    // At rest, silence in is silence out and changes nothing but the clock.
    if (resting() && std::all_of(left, left + numSamples, [] (float x) { return x == 0.0f; })
        && std::all_of(right, right + numSamples, [] (float x) { return x == 0.0f; }))
    {
        now += static_cast<unsigned>(numSamples);
        return;
    }
    constexpr unsigned mask = ring - 1;
    const unsigned span = static_cast<unsigned>(lookahead);
    const double slope = 1.0 - 1.0 / ratio;
    // The mean square where the knee starts; below it the compressor is idle.
    const double kneeStart = std::pow(10.0, (thresholdDb - 0.5 * kneeDb) / 10.0);
    for (int i = 0; i < numSamples; ++i)
    {
        const double inLeft = left[i], inRight = right[i];
        const bool wasResting = resting();
        silent = inLeft == 0.0 && inRight == 0.0 ? std::min(silent + 1, lookahead + 1) : 0;

        // The compressor: one-pole ballistics on the power, a soft knee.
        const double power = 0.5 * (inLeft * inLeft + inRight * inRight);
        envelope += (power - envelope) * (power > envelope ? attack : release);
        double gain = 1.0;
        if (envelope > kneeStart)
        {
            const double over = 10.0 * std::log10(envelope) - thresholdDb;
            const double reduction = over < 0.5 * kneeDb
                                   ? slope * (over + 0.5 * kneeDb) * (over + 0.5 * kneeDb) / (2.0 * kneeDb)
                                   : slope * over;
            gain = std::pow(10.0, -0.05 * reduction);
        }
        const double x0 = inLeft * gain, x1 = inRight * gain;

        // The limiter: the gain each sample needs, its minimum over the
        // look-ahead, an exponential release, then a moving average over the
        // look-ahead. Every gain averaged at the delayed sample is at most
        // what that sample needs, so no sample passes the ceiling.
        const double peak = std::max(std::abs(x0), std::abs(x1));
        const double needed = peak > ceiling ? ceiling / peak : 1.0;
        while (queueBack != queueFront && queueGain[(queueBack - 1) & mask] >= needed)
            --queueBack;
        queueGain[queueBack & mask] = needed;
        queueTime[queueBack & mask] = now;
        ++queueBack;
        while (now - queueTime[queueFront & mask] > span)
            ++queueFront;
        held = std::min(queueGain[queueFront & mask], 1.0 - (1.0 - held) * limiterRelease);
        gainSum += held - gains[(now - span - 1) & mask];
        gains[now & mask] = held;
        // Re-add the window now and then, at fixed sample times, so rounding
        // cannot drift and the block size cannot change the result.
        if ((now & mask) == 0)
        {
            gainSum = 0.0;
            for (unsigned k = 0; k <= span; ++k)
                gainSum += gains[(now - k) & mask];
        }
        const double smoothed = gainSum / (lookahead + 1.0);
        delay[0][now & mask] = static_cast<float>(x0);
        delay[1][now & mask] = static_cast<float>(x1);
        const double outLeft = delay[0][(now - span) & mask] * smoothed;
        const double outRight = delay[1][(now - span) & mask] * smoothed;
        left[i] = static_cast<float>(std::clamp(outLeft, -ceiling, ceiling));
        right[i] = static_cast<float>(std::clamp(outRight, -ceiling, ceiling));
        ++now;
        // The delay holds only silence now: settle every state at once, as
        // the engine does when it sleeps.
        if (resting() && !wasResting)
            clear();
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
