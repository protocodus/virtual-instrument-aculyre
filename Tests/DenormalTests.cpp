// No subnormal arithmetic anywhere a tail could decay into it. Reason's
// documents do not say whether it calls with flush-to-zero set, so the
// engine must not need it: each case below plays through the Performer with
// the CPU's default rounding (subnormals kept), and the sticky
// denormal-operand and underflow flags must stay clear to the end. Not a
// timing test: on x86 a subnormal operand costs a microcode assist, here it
// only raises a flag.
//
// The cases are the ones where a state could decay alone while the engine
// stays awake: a stopped string beside a ringing one, a release into sleep,
// a full palm mute under held keys, the softest pluck (alone and muted), and
// the seconds after an instrument switch; each with the idle strings ringing
// in sympathy and held still.
#include "AculyrePerformer.h"
#include "AculyrePresets.h"

#include <array>
#include <cfenv>
#include <iostream>
#include <memory>

#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#define ACULYRE_TEST_MXCSR 1
#endif

namespace
{
void clearFlags() noexcept
{
#if defined(ACULYRE_TEST_MXCSR)
    // Flush-to-zero (0x8000) and denormals-are-zero (0x0040) off, flags clear.
    _mm_setcsr(_mm_getcsr() & ~0x803fu & ~0x0040u);
#else
    std::feclearexcept(FE_ALL_EXCEPT);
#endif
}

bool subnormalFlagged() noexcept
{
#if defined(ACULYRE_TEST_MXCSR)
    return (_mm_getcsr() & 0x12u) != 0; // denormal operand, underflow
#else
    return std::fetestexcept(FE_UNDERFLOW) != 0;
#endif
}

enum class Case { StoppedBesideRinging, ReleaseIntoSleep, PalmMuteHeld, SoftestPluck, SoftestPluckMuted, InstrumentSwitch };

const char* name(Case which)
{
    switch (which)
    {
        case Case::StoppedBesideRinging: return "a stopped string beside a ringing one";
        case Case::ReleaseIntoSleep: return "a release into sleep";
        case Case::PalmMuteHeld: return "a full palm mute under held keys";
        case Case::SoftestPluck: return "the softest pluck, held";
        case Case::SoftestPluckMuted: return "the softest pluck under a full palm mute";
        case Case::InstrumentSwitch: return "an instrument switch under ringing strings";
    }
    return "";
}
} // namespace

int main()
{
    constexpr int frames = 64;
    constexpr double rate = 48000.0;
    // 25 s: a string the hand stops (30 ms) would pass double precision's
    // smallest normal 21 s after its key-up if nothing else drove it.
    constexpr int longest = static_cast<int>(25.0 * rate / frames);
    auto performer = std::make_unique<aculyre::Performer>();
    performer->prepare(rate, 1);
    std::array<float, frames> left {}, right {};
    int failures = 0, cases = 0;
    for (int preset = 0; preset < aculyre::Performer::instrumentCount; ++preset)
        for (const double sympathy : { 1.0, 0.0 })
            for (const Case which : { Case::StoppedBesideRinging, Case::ReleaseIntoSleep, Case::PalmMuteHeld,
                                      Case::SoftestPluck, Case::SoftestPluckMuted, Case::InstrumentSwitch })
            {
                const auto& spec = *aculyre::presets[static_cast<std::size_t>(preset)];
                auto controls = aculyre::Performer::defaultControls(preset);
                controls.sympathy = sympathy;
                if (which == Case::PalmMuteHeld || which == Case::SoftestPluckMuted)
                    controls.mute = 1.0;
                performer->reset();
                performer->setInstrument(preset);
                performer->setControls(controls);
                const int low = spec.strings[4].midiNote, high = spec.strings[9].midiNote;
                const bool soft = which == Case::SoftestPluck || which == Case::SoftestPluckMuted;
                int firstFlagged = -1, flagged = 0, block = 0;
                bool sounded = false;
                for (; block < longest; ++block)
                {
                    if (block == 0)
                    {
                        performer->noteOn(low, soft ? 1.0f / 127.0f : 0.8f);
                        if (!soft)
                            performer->noteOn(high, 0.8f);
                    }
                    if (block == 150 && which == Case::StoppedBesideRinging)
                        performer->noteOff(high);
                    if (block == 375 && which == Case::ReleaseIntoSleep)
                    {
                        performer->noteOff(low);
                        performer->noteOff(high);
                    }
                    if (block == 375 && which == Case::InstrumentSwitch)
                        performer->setInstrument((preset + 1) % aculyre::Performer::instrumentCount);
                    clearFlags();
                    performer->process(left.data(), right.data(), frames);
                    if (subnormalFlagged())
                    {
                        ++flagged;
                        if (firstFlagged < 0)
                            firstFlagged = block;
                    }
                    bool silent = true;
                    for (int i = 0; i < frames; ++i)
                        silent = silent && left[static_cast<std::size_t>(i)] == 0.0f
                              && right[static_cast<std::size_t>(i)] == 0.0f;
                    sounded = sounded || !silent;
                    // Asleep, and a second of silence after it.
                    if (performer->engine().idle() && silent && block > 750 && sounded)
                        break;
                }
                ++cases;
                if (!sounded)
                {
                    ++failures;
                    std::cerr << "FAILED: preset " << preset << ", sympathy " << sympathy << ", " << name(which)
                              << ": no sound\n";
                }
                if (flagged != 0)
                {
                    ++failures;
                    std::cerr << "FAILED: preset " << preset << ", sympathy " << sympathy << ", " << name(which)
                              << ": subnormal arithmetic in " << flagged << " of " << block + 1 << " callbacks, first at "
                              << firstFlagged * frames / rate << " s\n";
                }
            }
    if (failures != 0)
    {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << cases << " cases stayed clear of subnormal arithmetic.\n";
    return 0;
}
