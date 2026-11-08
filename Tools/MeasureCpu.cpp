// Native Performer probe, not a Reason/target CPU measurement. Run alternating
// baseline/candidate executables compiled with identical flags and this source,
// with other builds/renders idle. Defaults: all presets, 48 kHz, 64 frames,
// 1x/2x/4x, all strings held, 0.25 s untimed warm-up, 2 s timed playing.
// Every callback records elapsed wall and calling-thread CPU time together;
// MIDI/control dispatch and small common timer overhead are included;
// allocation, setup, analysis and I/O are not.
// --samples writes raw native-endian interleaved stereo float32, concatenating
// the cases in JSON order, for byte comparisons with the same options/platform.
// --blocks-csv retains paired timings so an elapsed outlier is not retroactively
// attributed to scheduling using an unrelated run.
#include "DSP/AculyrePerformer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <time.h>
#endif

namespace
{
constexpr double rate = 48000.0;
constexpr int frames = 64;
constexpr int warmupBlocks = 188; // 250.667 ms, rounded up to a whole callback
constexpr double period = frames / rate;

std::int64_t threadTime()
{
#if defined(_WIN32)
    FILETIME created {}, exited {}, kernel {}, user {};
    if (!GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user))
        throw std::runtime_error("GetThreadTimes failed");
    const auto ticks = [](FILETIME t) { return (std::uint64_t(t.dwHighDateTime) << 32) | t.dwLowDateTime; };
    return static_cast<std::int64_t>((ticks(kernel) + ticks(user)) * 100);
#else
    timespec t {};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t) != 0)
        throw std::runtime_error("CLOCK_THREAD_CPUTIME_ID unavailable");
    return std::int64_t(t.tv_sec) * 1000000000 + t.tv_nsec;
#endif
}

std::int64_t wallTime()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct Options
{
    int preset = -1, factor = 0;
    double seconds = 2.0;
    std::string scenario = "dense", samples, csv;
};

Options options(int argc, char** argv)
{
    Options out;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--help")
        {
            std::cout << "AculyreCpuProbe [--preset all|0|1|2] [--oversampling all|1|2|4] "
                         "[--seconds 0.1..30] [--scenario dense|gestures|all] "
                         "[--samples output.f32] [--blocks-csv output.csv]\n";
            std::exit(0);
        }
        if (++i == argc)
            throw std::runtime_error("missing value for " + arg);
        const std::string value = argv[i];
        if (arg == "--preset")
        {
            if (value != "all" && value != "0" && value != "1" && value != "2")
                throw std::runtime_error("preset must be all, 0, 1 or 2");
            out.preset = value == "all" ? -1 : std::stoi(value);
        }
        else if (arg == "--oversampling")
        {
            if (value != "all" && value != "1" && value != "2" && value != "4")
                throw std::runtime_error("oversampling must be all, 1, 2 or 4");
            out.factor = value == "all" ? 0 : std::stoi(value);
        }
        else if (arg == "--seconds")
        {
            std::size_t used = 0;
            out.seconds = std::stod(value, &used);
            if (used != value.size() || !std::isfinite(out.seconds) || out.seconds < 0.1 || out.seconds > 30.0)
                throw std::runtime_error("seconds must be in 0.1..30");
        }
        else if (arg == "--scenario") out.scenario = value;
        else if (arg == "--samples") out.samples = value;
        else if (arg == "--blocks-csv") out.csv = value;
        else throw std::runtime_error("unknown option " + arg);
    }
    if (out.scenario != "dense" && out.scenario != "gestures" && out.scenario != "all")
        throw std::runtime_error("scenario must be dense, gestures or all");
    if (!out.samples.empty() && out.samples == out.csv)
        throw std::runtime_error("samples and CSV must use different paths");
    return out;
}

struct Timing
{
    std::int64_t wall, cpu;
};

void dispatch(aculyre::Performer& performer, aculyre::PlayingControls& controls,
              const aculyre::InstrumentSpec& spec, int block, bool gestures)
{
    if (block % 96 == 0)
    {
        const int string = (block / 96) % spec.stringCount;
        const int note = spec.strings[static_cast<std::size_t>(string)].midiNote;
        performer.noteOff(note);
        performer.noteOn(note, 100.0f / 127.0f);
    }
    if (!gestures)
        return;
    if (block % 8 == 0)
    {
        const int phase = (block / 8) % 64;
        controls.pitchBend = phase < 32 ? phase / 16.0 : (64 - phase) / 16.0 - 1.0;
        controls.mute = (block / 150) % 4 == 2 ? 0.35 : 0.0;
        performer.setControls(controls);
    }
}

void distribution(const char* name, const std::vector<Timing>& timings, bool cpu)
{
    std::vector<double> values;
    values.reserve(timings.size());
    for (const auto& t : timings)
        values.push_back(static_cast<double>(cpu ? t.cpu : t.wall) / 1000.0);
    std::sort(values.begin(), values.end());
    const std::size_t middle = values.size() / 2;
    const double median = values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) * 0.5;
    const auto p99 = static_cast<std::size_t>(std::ceil(0.99 * values.size())) - 1;
    const auto misses = std::count_if(values.begin(), values.end(), [](double us) { return us > period * 1e6; });
    std::cout << '"' << name << "\": {\"median_us\": " << median
              << ", \"p99_us\": " << values[p99] << ", \"max_us\": " << values.back()
              << ", \"deadline_misses\": " << misses << '}';
}

void run(const Options& opt, int preset, int factor, bool gestures,
         std::ofstream& samples, std::ofstream& csv, std::uint64_t& sampleOffset)
{
    auto performer = std::make_unique<aculyre::Performer>();
    performer->prepare(rate, factor);
    performer->setInstrument(preset);
    auto controls = aculyre::Performer::defaultControls(preset);
    performer->setControls(controls);
    const auto& spec = *aculyre::presets[static_cast<std::size_t>(preset)];
    for (int i = 0; i < spec.stringCount; ++i)
        performer->noteOn(spec.strings[static_cast<std::size_t>(i)].midiNote, 100.0f / 127.0f);

    std::array<float, frames> left {}, right {};
    for (int block = 0; block < warmupBlocks; ++block)
        performer->process(left.data(), right.data(), frames);
    const int blocks = static_cast<int>(std::ceil(opt.seconds / period));
    std::vector<Timing> timings(static_cast<std::size_t>(blocks));
    std::vector<float> audio(static_cast<std::size_t>(blocks) * frames * 2);
    for (int block = 0; block < blocks; ++block)
    {
        const auto wallStart = wallTime();
        const auto cpuStart = threadTime();
        dispatch(*performer, controls, spec, block, gestures);
        performer->process(left.data(), right.data(), frames);
        const auto cpuEnd = threadTime();
        const auto wallEnd = wallTime();
        timings[static_cast<std::size_t>(block)] = { wallEnd - wallStart, cpuEnd - cpuStart };
        for (int i = 0; i < frames; ++i)
        {
            const auto offset = (static_cast<std::size_t>(block) * frames + static_cast<std::size_t>(i)) * 2;
            audio[offset] = left[static_cast<std::size_t>(i)];
            audio[offset + 1] = right[static_cast<std::size_t>(i)];
        }
    }
    double energy = 0.0, peak = 0.0, totalCpu = 0.0;
    for (float x : audio)
    {
        if (!std::isfinite(x))
            throw std::runtime_error("nonfinite output");
        energy += static_cast<double>(x) * x;
        peak = std::max(peak, std::abs(static_cast<double>(x)));
    }
    if (peak >= 1.0 || energy == 0.0)
        throw std::runtime_error("output is clipped or silent");
    for (const auto& t : timings)
        totalCpu += static_cast<double>(t.cpu) * 1e-9;
    const char* scenario = gestures ? "gestures" : "dense";
    if (samples.is_open())
    {
        samples.write(reinterpret_cast<const char*>(audio.data()), static_cast<std::streamsize>(audio.size() * sizeof(float)));
        if (!samples) throw std::runtime_error("sample write failed");
    }
    if (csv.is_open())
    {
        for (int block = 0; block < blocks; ++block)
        {
            const auto& t = timings[static_cast<std::size_t>(block)];
            csv << preset << ',' << factor << ',' << scenario << ',' << block << ',' << t.wall << ',' << t.cpu << '\n';
        }
        if (!csv) throw std::runtime_error("CSV write failed");
    }
    std::cout << "    {\"preset\": " << preset << ", \"oversampling\": " << performer->oversampling()
              << ", \"engine_rate\": " << performer->engine().sampleRate() << ", \"scenario\": \"" << scenario
              << "\", \"strings\": " << spec.stringCount << ", \"blocks\": " << blocks
              << ", \"seconds\": " << blocks * period << ", \"sample_offset_frames\": " << sampleOffset
              << ", \"cpu_percent\": " << 100.0 * totalCpu / (blocks * period)
              << ", \"peak\": " << peak << ", \"rms\": " << std::sqrt(energy / audio.size()) << ", ";
    distribution("wall", timings, false);
    std::cout << ", ";
    distribution("thread_cpu", timings, true);
    std::cout << '}';
    sampleOffset += static_cast<std::uint64_t>(blocks) * frames;
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        const auto opt = options(argc, argv);
        std::ofstream samples, csv;
        if (!opt.samples.empty())
        {
            samples.open(opt.samples, std::ios::binary);
            if (!samples) throw std::runtime_error("cannot open samples output");
        }
        if (!opt.csv.empty())
        {
            csv.open(opt.csv);
            if (!csv) throw std::runtime_error("cannot open CSV output");
            csv << "preset,oversampling,scenario,block,wall_ns,thread_cpu_ns\n";
        }
        (void)threadTime();
        std::cout << std::setprecision(10)
                  << "{\n  \"sample_rate\": " << rate << ", \"block_frames\": " << frames
                  << ", \"warmup_frames\": " << warmupBlocks * frames
                  << ", \"timer\": \"calling thread CPU and steady wall\",\n  \"cases\": [\n";
        bool first = true;
        std::uint64_t sampleOffset = 0;
        for (int preset = 0; preset < aculyre::Performer::instrumentCount; ++preset)
            for (int factor : { 1, 2, 4 })
                for (bool gestures : { false, true })
                {
                    if ((opt.preset >= 0 && opt.preset != preset) || (opt.factor && opt.factor != factor)
                        || (opt.scenario == "dense" && gestures) || (opt.scenario == "gestures" && !gestures))
                        continue;
                    if (!first) std::cout << ",\n";
                    first = false;
                    run(opt, preset, factor, gestures, samples, csv, sampleOffset);
                }
        std::cout << "\n  ]\n}\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "AculyreCpuProbe: %s\n", e.what());
        return 1;
    }
}
