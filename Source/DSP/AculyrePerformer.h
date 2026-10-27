// The instrument as a front end plays it. All three presets are designed
// once in prepare() (not real time), so choosing an instrument is a
// real-time switch: the old one fades out over 20 ms while the new one
// starts. The Rack adapter plays through it, and so would a plug-in.

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

    Performer();
    ~Performer();

    // Setup: designs every instrument. Not real-time.
    void prepare(double sampleRate);
    void reset() noexcept;

    void setInstrument(int index) noexcept;
    // Applies to every instrument, so a switch keeps the player's settings.
    void setControls(const PlayingControls& controls) noexcept;
    void noteOn(int midiNote, float velocity) noexcept;
    void noteOff(int midiNote) noexcept;
    void setSustain(bool down) noexcept;

    void process(float* left, float* right, int numSamples) noexcept;

    [[nodiscard]] int instrument() const noexcept { return active_; }
    [[nodiscard]] const AculyreEngine& engine() const noexcept { return *engines_[static_cast<std::size_t>(active_)]; }
    // An instrument's own way of playing (its preset's PlayingControls).
    [[nodiscard]] static PlayingControls defaultControls(int index) noexcept;

private:
    std::array<std::unique_ptr<AculyreEngine>, instrumentCount> engines_;
    PlayingControls controls_ {};
    int active_ { 0 };
    int fading_ { -1 };
    int fadeRemaining_ { 0 };
    int fadeLength_ { 960 };
};

} // namespace aculyre
