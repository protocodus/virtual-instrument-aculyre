// What designing the instruments costs. Not real-time: the Performer designs
// all three at startup, and a front end must allow for it (README, Known gaps).
//
//   MeasureAculyreDesign [rate]       the median of 7 Performer::prepare calls
//   MeasureAculyreDesign --once <n>   one AculyreEngine::setInstrument of
//                                     preset n (0-2) at 48 kHz, for an
//                                     instruction count: valgrind
//                                     --tool=callgrind ... --once 0
//
// Wall-clock figures move 15% between runs on a shared machine; compare
// instruction counts, or alternate the two builds and take the medians.
// Development-machine figures, never Reason or DAW CPU.

#include "../Source/DSP/AculyreEngine.h"
#include "../Source/DSP/AculyrePerformer.h"
#include "../Source/DSP/AculyrePresets.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

int main(int argc, char** argv)
{
    if (argc == 3 && std::strcmp(argv[1], "--once") == 0)
    {
        const int preset = std::atoi(argv[2]);
        if (preset < 0 || preset >= static_cast<int>(aculyre::presets.size()))
            return 1;
        auto engine = std::make_unique<aculyre::AculyreEngine>();
        engine->prepare(48000.0);
        engine->setInstrument(*aculyre::presets[static_cast<std::size_t>(preset)]);
        return 0;
    }
    const double rate = argc > 1 ? std::atof(argv[1]) : 48000.0;
    std::vector<double> seconds;
    for (int run = 0; run < 7; ++run)
    {
        auto performer = std::make_unique<aculyre::Performer>();
        const auto start = std::chrono::steady_clock::now();
        performer->prepare(rate);
        seconds.push_back(std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
    }
    std::sort(seconds.begin(), seconds.end());
    std::printf("Performer::prepare at %.0f Hz: median %.3f s (min %.3f, max %.3f)\n",
                rate, seconds[3], seconds.front(), seconds.back());
    return 0;
}
