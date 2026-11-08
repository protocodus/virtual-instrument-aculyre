// The instrument as a front end plays it. All three presets are designed
// once in prepare() (not real time), so choosing an instrument is a
// real-time switch: the old one fades out over 20 ms while the new one
// starts. The Rack adapter plays through it, and so would a plug-in.
//
// Oversampling runs the instruments at 2 or 4 times the host rate and
// filters the result back down; it changes how finely the strings, bridge
// and body are sampled, not what they are designed to be.
//
// The output stage is the portfolio's (OutputStage.h) with its plucked
// guard, kVuGuard: a soft-knee compressor that reins in strong chords while
// retaining their velocity differences, then a look-ahead peak limiter
// that holds every sample under
// -1.5 dBFS. Digital calibration before that stage preserves the nominal
// unity-control level; a final 0.6 dB reserve leaves its sample ceiling at
// -2.1 dBFS. The reserve covers the measured intersample excursion without
// changing physical excitation, capture electronics or the shared stage.

#pragma once

#include "AculyreEngine.h"
#include "AculyrePresets.h"
#include "OutputStage.h"

#include <array>
#include <memory>

namespace aculyre
{

class Performer
{
public:
    static constexpr int instrumentCount = static_cast<int>(presets.size());
    static constexpr int maxOversampling = 4;
    // The engine designs up to 192 kHz, so a high host rate lowers the
    // factor: 4x runs as 2x at 88.2 and 96 kHz and as 1x above.
    static constexpr double maxEngineRate = 192000.0;
    // Output-only calibration, after all physical/circuit observations.
    // The 2026-11-08 612-case audit found +0.090 dB reconstructed output
    // despite the shared stage's -1.5 dBFS sample ceiling (22.05 kHz/2x,
    // steel pickup, repeated dense chords). 0.6 dB adds about 0.5 dB margin.
    // These reciprocal trims retain unity gain when the stage is inactive;
    // normal-playing levels are rechecked by the existing VU protocol.
    static constexpr float digitalCalibrationGain = 1.071519305f; // +0.6 dB
    static constexpr float finalHeadroomGain = 0.933254301f;       // -0.6 dB

    Performer();
    ~Performer();

    // Setup: designs every instrument at `oversampling` (1, 2 or 4) times
    // sampleRate. Not real-time.
    void prepare(double sampleRate, int oversampling = 1);
    void reset() noexcept;

    void setInstrument(int index) noexcept;
    // Applies to every instrument, so a switch keeps the player's settings.
    void setControls(const PlayingControls& controls) noexcept;
    void noteOn(int midiNote, float velocity) noexcept;
    void noteOff(int midiNote) noexcept;
    void setSustain(bool down) noexcept;

    void process(float* left, float* right, int numSamples) noexcept;

    [[nodiscard]] int instrument() const noexcept { return active_; }
    // The factor in use, after the engine's rate limit.
    [[nodiscard]] int oversampling() const noexcept { return factor_; }
    // Output delay in host samples: the limiter's look-ahead, plus the
    // decimating filter's when oversampling.
    [[nodiscard]] int latency() const noexcept
    {
        return stage_.DelaySamples() + (factor_ > 1 ? decimatorDelay : 0)
             + (AculyreEngine::latencySamples + factor_ - 1) / factor_;
    }
    [[nodiscard]] const AculyreEngine& engine() const noexcept { return *engines_[static_cast<std::size_t>(active_)]; }
    // An instrument's own way of playing (its preset's PlayingControls).
    [[nodiscard]] static PlayingControls defaultControls(int index) noexcept;

private:
    // A linear-phase Kaiser-windowed sinc, as Ceres decimates: 62 taps per
    // factor, so its delay is 31 host samples at 2x and 4x alike.
    static constexpr int decimatorDelay = 31;
    struct Decimator
    {
        static constexpr int maxTaps = 2 * decimatorDelay * maxOversampling + 1;
        std::array<double, maxTaps> taps {};
        // Each sample is written twice, so the newest `length` are contiguous.
        std::array<double, 2 * maxTaps> history {};
        int length { 1 };
        int cursor { 0 };

        void design(int factor) noexcept;
        void clear() noexcept;
        void push(double x) noexcept;
        [[nodiscard]] double output() const noexcept;
    };

    // Runs the output stage. Once its delay holds only silence the stage is
    // reset and skipped while the silence lasts, as a sleeping engine is, so
    // what follows depends on nothing before it, whatever the block sizes.
    void finish(float* left, float* right, int numSamples) noexcept;
    [[nodiscard]] bool stageResting() const noexcept { return stageSilence_ > stage_.DelaySamples(); }

    void render(float* left, float* right, int numSamples) noexcept;

    std::array<std::unique_ptr<AculyreEngine>, instrumentCount> engines_;
    std::array<Decimator, 2> decimators_ {};
    // Round the electrical delay up to a whole host sample before decimating;
    // both capture paths share it. At most factor-1 extra engine samples.
    std::array<std::array<double, maxOversampling>, 2> alignment_ {};
    int alignmentSamples_ { 0 }, alignmentCursor_ { 0 };
    protocodus::OutputStage<2> stage_ {};
    int stageSilence_ { 0 };
    std::array<std::array<float, 64 * maxOversampling>, 2> fast_ {};
    // Fast samples of silence pushed while the engines slept; once they
    // fill the decimators, those hold only zeros and are skipped.
    int silentPushes_ { 0 };
    int factor_ { 1 };
    PlayingControls controls_ {};
    int active_ { 0 };
    int fading_ { -1 };
    int fadeRemaining_ { 0 };
    int fadeLength_ { 960 };
};

} // namespace aculyre
