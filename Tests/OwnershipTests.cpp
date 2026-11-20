#include "AculyrePerformer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>

namespace
{
int failures = 0;
void check(bool okay, const char* what)
{
    if (!okay) { std::printf("FAIL: %s\n", what); ++failures; }
}
}

int main()
{
    // The Rack adapter keeps a press ledger, including a separate CV owner,
    // and forwards one Note Off for every Note On. Exercise that public
    // Performer route: an earlier source must not stop a later held pluck.
    auto performer = std::make_unique<aculyre::Performer>();
    performer->prepare(48000.0);
    auto controls = aculyre::Performer::defaultControls(0);
    controls.room = 0.0;
    controls.levers = true;
    performer->setControls(controls);
    const int string = performer->engine().stringForNote(60);
    const auto held = [&] (int index) { return performer->engine().stringActivity(index).held; };
    const auto overlap = [&]
    {
        performer->noteOn(60, 0.7f);
        performer->noteOn(60, 0.4f);
        performer->noteOff(60);
        check(held(string), "one source's Note Off leaves the other source's pluck held");
        performer->noteOff(60);
        check(!held(string), "the last matching Note Off releases the string");
    };
    overlap();
    performer->noteOff(60);  // an extra Off cannot underflow the ownership
    overlap();

    performer->reset();
    performer->noteOn(60, 0.7f);
    performer->noteOn(60, 0.4f);
    performer->setSustain(true);
    performer->noteOff(60);
    performer->setSustain(false);
    check(held(string), "pedal-up does not release a remaining key owner");
    performer->setSustain(true);
    performer->noteOff(60);
    check(held(string), "the pedal retains the final released owner");
    performer->setSustain(false);
    check(!held(string), "pedal-up releases a string with no key owners");

    performer->noteOn(60, 0.7f);
    performer->noteOn(60, 0.4f);
    performer->reset();
    performer->noteOn(60, 0.5f);
    performer->noteOff(60);
    check(!held(string), "reset clears prior owners");

    performer->noteOn(60, 0.5f);
    performer->noteOn(60, 0.0f);
    performer->noteOn(60, -1.0f);
    performer->noteOn(60, std::numeric_limits<float>::quiet_NaN());
    performer->noteOff(60);
    check(!held(string), "ignored velocities do not acquire an owner");

    // A different note retunes/steals the same physical string. Old-pitch
    // releases must neither stop the new pitch nor accumulate latent owners.
    for (const std::array<int, 2> notes : { std::array<int, 2>{60, 61}, {21, 22} })
    {
        performer->reset();
        const int index = notes[0] == 21 ? 0 : string;
        performer->noteOn(notes[0], 0.7f);
        performer->noteOn(notes[0], 0.4f);
        performer->noteOn(notes[1], 0.6f);
        performer->noteOff(notes[0]);
        performer->noteOff(notes[0]);
        check(held(index), "old-pitch releases cannot release the new owner");
        performer->noteOff(notes[1]);
        check(!held(index), "the stolen string has only its new pitch's owner");
        performer->noteOn(notes[0], 0.5f);
        performer->noteOff(notes[0]);
        check(!held(index), "returning to an old pitch does not restore stale owners");
    }

    for (int repeats : {1, 512})
    {
        performer->reset();
        for (int press = 0; press < repeats; ++press)
            performer->noteOn(60, 0.5f);
        performer->noteOn(61, 0.5f);  // raised lever takes the C string
        performer->noteOn(60, 0.5f);  // reacquires it, with all old key-ups still pending
        check(held(string), "retiring an overflowed press leaves the reacquired string held");
        for (int release = 0; release < repeats; ++release)
            performer->noteOff(60);
        check(held(string), "old key-ups leave the reacquired string held");
        performer->noteOff(60);
        check(!held(string), "the reacquired string releases on its own key-up");
    }

    // Silence/CPU sleep is an acoustic state, not a synthetic key-up. Keep
    // the ownership ledger until the sources themselves let their keys go.
    performer->reset();
    controls.mute = 1.0;
    performer->setControls(controls);
    performer->noteOn(60, 0.1f);
    performer->noteOn(60, 0.1f);
    std::array<float, 64> left {}, right {};
    for (int frame = 0; frame < 20 * 48000 && !performer->engine().idle(); frame += 64)
        performer->process(left.data(), right.data(), 64);
    check(performer->engine().idle(), "a muted held string can still sleep");
    performer->noteOff(60);
    check(held(string), "sleep preserves a remaining source's key ownership");
    performer->noteOff(60);
    check(!held(string), "the sleeping string releases its final owner");

    // A Note Off arriving while an earlier instrument is fading out (and then
    // switched back to within the 20 ms crossfade) must still release that
    // instrument's key owner instead of leaving the string permanently held.
    controls.mute = 0.0;
    performer->setControls(controls);
    performer->reset();
    performer->setInstrument(0);
    performer->noteOn(60, 0.7f);
    performer->process(left.data(), right.data(), 64);
    check(held(string), "string is held before switching instruments");
    performer->setInstrument(1);
    performer->process(left.data(), right.data(), 64);
    performer->noteOff(60);
    performer->process(left.data(), right.data(), 64);
    performer->setInstrument(0);
    check(!held(string), "Note Off during an instrument crossfade releases the outgoing instrument's owner");

    performer->noteOn(60, .7f);
    performer->setInstrument(1);
    performer->noteOn(60, .5f);
    const int secondString = performer->engine().stringForNote(60);
    check(held(secondString), "the second instrument plays the overlap");
    performer->noteOff(60);
    check(held(secondString), "the first release must leave the newer instrument playing");
    performer->setInstrument(0);
    check(!held(string), "the first release must reach the older instrument");
    performer->noteOff(60);
    performer->setInstrument(1);
    check(!held(secondString), "the second release ends the second instrument's note");

    performer->noteOn(60, .7f);
    performer->setInstrument(0);
    for (int batch = 0; batch < 20; ++batch)
        performer->process(left.data(), right.data(), 64);
    performer->setInstrument(1);
    performer->noteOn(60, .5f);
    performer->noteOff(60);
    check(held(secondString), "a release from before the fade must not take a newly started note");
    performer->noteOff(60);
    check(!held(secondString), "the new note receives its own release");

    performer->setInstrument(0);
    performer->reset();
    controls.levers = false;
    performer->setControls(controls);
    performer->noteOn(61, .5f);
    check(!held(string), "C sharp is unavailable with the lever down");
    controls.levers = true;
    performer->setControls(controls);
    performer->noteOn(61, .5f);
    check(held(string), "the raised lever plays C sharp");
    performer->noteOff(61);
    check(!held(string), "a skipped note does not consume a later release");

    // A finished fade clears the outgoing instrument's notes. The pedal
    // still belongs to the player when that instrument is selected again.
    performer->setSustain(true);
    performer->setInstrument(1);
    for (int batch = 0; batch < 20; ++batch)
        performer->process(left.data(), right.data(), 64);
    performer->setInstrument(0);
    performer->noteOn(60, 0.5f);
    performer->noteOff(60);
    check(held(string), "the held sustain pedal survives a completed instrument fade");
    performer->setSustain(false);
    check(!held(string), "pedal-up releases the newly selected instrument");
    performer->process(left.data(), right.data(), 0);
    return failures ? 1 : 0;
}
