// The instrument as a front end plays it. All three presets are designed
// once in prepare() (not real time), so choosing an instrument is a
// real-time switch: the old one fades out over 20 ms while the new one
// starts. The Rack adapter plays through it, and so would a plug-in.
//
// Oversampling runs the instruments at 2 or 4 times the host rate and
// filters the result back down; it changes how finely the strings, bridge
// and body are sampled, not what they are designed to be.

#pragma once

#include "AculyreEngine.h"
#include "AculyrePresets.h"

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
    // Output delay of the decimating filter, in host samples (0 at 1x).
    [[nodiscard]] int latency() const noexcept { return factor_ > 1 ? decimatorDelay : 0; }
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

    void render(float* left, float* right, int numSamples) noexcept;

    std::array<std::unique_ptr<AculyreEngine>, instrumentCount> engines_;
    std::array<Decimator, 2> decimators_ {};
    std::array<std::array<float, 64 * maxOversampling>, 2> fast_ {};
    int factor_ { 1 };
    PlayingControls controls_ {};
    int active_ { 0 };
    int fading_ { -1 };
    int fadeRemaining_ { 0 };
    int fadeLength_ { 960 };
};

} // namespace aculyre
