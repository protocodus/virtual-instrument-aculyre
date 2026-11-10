#include "AculyrePerformer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

namespace
{
int failures = 0;
void check(bool okay, const char* what)
{
    if (!okay) { std::printf("FAIL: %s\n", what); ++failures; }
}

double largestStep(const std::vector<float>& x, std::size_t from, std::size_t to)
{
    double step = 0.0;
    for (std::size_t i = from + 1; i < to; ++i)
        step = std::max(step, std::abs(static_cast<double>(x[i]) - x[i - 1]));
    return step;
}
}

int main()
{
    // The Rack adapter sets the instrument from a three-position control
    // every batch, so a turn from the first to the last passes the middle
    // one for a batch or more, and a turn there and back returns within the
    // 20 ms fade. Until 0.13.5 the first cut the ringing instrument's tail
    // off in one sample and the second brought it back at full level from
    // wherever its fade stood: a click the size of the wave itself. A low
    // string well after its pluck moves little from sample to sample, so
    // the largest step after the switches is held against the ringing
    // string's own before them.
    constexpr double rate = 48000.0;
    constexpr int block = 64;
    for (int oversampling : { 1, 2 })
        for (int second : { 2, 0 })
            for (int between : { 64, 320, 704 })
            {
                auto performer = std::make_unique<aculyre::Performer>();
                performer->prepare(rate, oversampling);
                auto controls = aculyre::Performer::defaultControls(0);
                controls.room = 0.0;
                performer->setControls(controls);
                std::vector<float> left(static_cast<std::size_t>(0.5 * rate)), right(left.size());
                std::size_t at = 0;
                const auto run = [&] (int samples)
                {
                    for (int done = 0; done < samples; done += block, at += block)
                        performer->process(left.data() + at, right.data() + at, block);
                };
                performer->noteOn(47, 1.0f);
                run(static_cast<int>(0.3 * rate) / block * block);
                const std::size_t switched = at;
                performer->setInstrument(1);
                run(between);
                performer->setInstrument(second);
                run(static_cast<int>(0.1 * rate) / block * block);
                const std::size_t before = switched - static_cast<std::size_t>(0.05 * rate);
                const double ringing = largestStep(left, before, switched);
                const double after = largestStep(left, switched, at);
                check(ringing > 1.0e-4, "the string still rings when the instrument is switched");
                check(after <= 1.5 * ringing,
                      second == 0 ? "an instrument switched back to within its fade returns without a step"
                                  : "a second switch within the fade does not cut the first instrument's tail");
                if (second == 0)
                {
                    // Back on the first instrument, its string is still the
                    // player's: the tail rings on rather than fading out.
                    const auto peak = [&] (std::size_t from, std::size_t to)
                    {
                        double level = 0.0;
                        for (std::size_t i = from; i < to; ++i)
                            level = std::max(level, std::abs(static_cast<double>(left[i])));
                        return level;
                    };
                    check(peak(at - static_cast<std::size_t>(0.05 * rate), at) > 0.5 * peak(before, switched),
                          "the instrument switched back to keeps ringing");
                }
            }

    // One switch alone: the old instrument's tail is gone after the fade and
    // a third instrument then starts from silence.
    {
        auto performer = std::make_unique<aculyre::Performer>();
        performer->prepare(rate);
        std::vector<float> left(block), right(block);
        performer->noteOn(47, 1.0f);
        for (int i = 0; i < 100; ++i)
            performer->process(left.data(), right.data(), block);
        performer->setInstrument(1);
        for (int i = 0; i < 40; ++i)
            performer->process(left.data(), right.data(), block);
        double peak = 0.0;
        for (int i = 0; i < 40; ++i)
        {
            performer->process(left.data(), right.data(), block);
            for (float sample : left)
                peak = std::max(peak, std::abs(static_cast<double>(sample)));
        }
        check(peak == 0.0, "a switched-away instrument is silent once its fade has ended");
        performer->setInstrument(0);
        performer->process(left.data(), right.data(), block);
        check(performer->engine().idle(), "the instrument switched away from was reset");
    }

    if (failures == 0)
        std::printf("instrument switch tests passed\n");
    return failures == 0 ? 0 : 1;
}
