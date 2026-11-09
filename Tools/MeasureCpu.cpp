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
// The stress scenario is the portfolio's stress phrase (standard U12), which
// judges 256-frame blocks on their fastest of three repeats: block256_percent
// is one run's, four callbacks a block, as a share of the block's period.
#include "DSP/AculyrePerformer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(ACULYRE_USE_CALLGRIND)
#include <valgrind/callgrind.h>
#endif

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
    std::string scenario = "dense", samples, csv, patchDirectory;
};

enum class Scenario { Dense, Gestures, Repluck, Idle, Factory, Burst, Cold, Stress };

const char* name(Scenario scenario)
{
    switch (scenario)
    {
    case Scenario::Dense: return "dense";
    case Scenario::Gestures: return "gestures";
    case Scenario::Repluck: return "repluck";
    case Scenario::Idle: return "idle";
    case Scenario::Factory: return "factory";
    case Scenario::Burst: return "burst";
    case Scenario::Cold: return "cold";
    case Scenario::Stress: return "stress";
    }
    return "unknown";
}

Options options(int argc, char** argv)
{
    Options out;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--help")
        {
            std::cout << "AculyreCpuProbe [--preset all|0|1|2] [--oversampling all|1|2|4] "
                         "[--seconds 0.1..30] [--scenario dense|gestures|repluck|idle|factory|burst|cold|stress|all] "
                         "[--patch-dir factory-repatch-directory] "
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
        else if (arg == "--patch-dir") out.patchDirectory = value;
        else throw std::runtime_error("unknown option " + arg);
    }
    if (out.scenario != "dense" && out.scenario != "gestures" && out.scenario != "repluck"
        && out.scenario != "idle" && out.scenario != "factory" && out.scenario != "burst"
        && out.scenario != "cold" && out.scenario != "stress" && out.scenario != "all")
        throw std::runtime_error("scenario must be dense, gestures, repluck, idle, factory, burst, cold, stress or all");
    if (!out.samples.empty() && out.samples == out.csv)
        throw std::runtime_error("samples and CSV must use different paths");
    return out;
}

struct Patch
{
    std::string name;
    int preset;
    aculyre::PlayingControls controls;
};

std::vector<Patch> patches(const std::string& directory)
{
    std::vector<Patch> result;
    if (directory.empty())
        return result;
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
        if (entry.is_regular_file() && entry.path().extension() == ".repatch")
            paths.push_back(entry.path());
    std::sort(paths.begin(), paths.end());
    for (const auto& path : paths)
    {
        std::ifstream file(path);
        if (!file) throw std::runtime_error("cannot read patch " + path.string());
        const std::string xml((std::istreambuf_iterator<char>(file)), {});
        if (xml.find("cz.protocodus.Aculyre") == std::string::npos)
            throw std::runtime_error("not an Aculyre factory patch: " + path.string());
        // Frozen Rack panel defaults/mapping, matching its host contract.
        // This optional tool path reads supplied public patch resources;
        // canonical DSP has no dependency on the wrapper or its SDK.
        const auto value = [&] (const char* key, double fallback)
        {
            const std::string tag = std::string("property=\"") + key + "\" type=\"number\">";
            const auto at = xml.find(tag);
            if (at == std::string::npos) return fallback;
            const double number = std::stod(xml.substr(at + tag.size()));
            if (!std::isfinite(number)) throw std::runtime_error("nonfinite patch property");
            return number;
        };
        const double instrument = value("instrument", 0);
        if (instrument < 0 || instrument >= aculyre::Performer::instrumentCount || instrument != std::floor(instrument))
            throw std::runtime_error("invalid patch instrument");
        Patch patch { path.stem().string(), static_cast<int>(instrument), {} };
        if (patch.name.find_first_of(",\r\n\"\\") != std::string::npos)
            throw std::runtime_error("unsupported punctuation in patch name");
        auto& c = patch.controls;
        const double technique = value("technique", 0);
        if (technique < 0 || technique > 2 || technique != std::floor(technique))
            throw std::runtime_error("invalid patch technique");
        c.technique = static_cast<aculyre::Technique>(static_cast<int>(technique));
        c.levers = value("levers", 1) >= 1;
        c.pluckPosition = 0.06 + 0.44 * value("pluck_position", 0.4773);
        const double angle = (5.0 + 55.0 * value("pluck_angle", 0.4545)) * 3.14159265358979323846 / 180.0;
        c.normalShare = std::sin(angle) * std::sin(angle);
        c.release = value("release", 0.6);
        c.mute = std::max(value("mute", 0), value("mod_wheel", 0));
        c.sympathy = value("sympathy", 1);
        c.room = value("room", 1);
        c.width = 2.0 * value("width", 0.5);
        c.volume = std::pow(10.0, (-24.0 + 30.0 * value("volume", 0.8667)) / 20.0);
        c.piezo = value("piezo", 0.5);
        c.piezoInverted = value("piezo_polarity", 0) >= 1;
        c.pitchBend = (value("pitch_wheel", 0.5) - 0.5) * 4.0;
        result.push_back(patch);
    }
    if (result.empty()) throw std::runtime_error("no factory patches found");
    return result;
}

struct Timing
{
    std::int64_t wall, cpu;
};

void dispatch(aculyre::Performer& performer, aculyre::PlayingControls& controls,
              const aculyre::InstrumentSpec& spec, int block, Scenario scenario)
{
    if (scenario == Scenario::Idle)
        return;
    if (scenario == Scenario::Burst || scenario == Scenario::Cold)
    {
        if ((scenario == Scenario::Cold && block == 0) || (scenario == Scenario::Burst && block % 96 == 0))
            for (int string = 0; string < spec.stringCount; ++string)
            {
                const int note = spec.strings[static_cast<std::size_t>(string)].midiNote;
                if (scenario == Scenario::Burst) performer.noteOff(note);
                performer.noteOn(note, 100.0f / 127.0f);
            }
        return;
    }
    if (scenario == Scenario::Stress)
    {
        // The portfolio's stress phrase (standard U12), once a second: an
        // eight-note chord at full velocity under the sustain pedal,
        // re-struck note by note, and a second chord on top (16 voices),
        // re-struck the same way. The chords take every third string from
        // the lowest and from the second, so they span the compass.
        const int step = block % 750;
        const auto strike = [&] (int voice)
        {
            const int string = (voice < 8 ? 3 * voice : 3 * (voice - 8) + 1) % spec.stringCount;
            const int note = spec.strings[static_cast<std::size_t>(string)].midiNote;
            performer.noteOff(note);
            performer.noteOn(note, 1.0f);
        };
        if (block == 0)
            performer.setSustain(true);
        if (step == 0)
            for (int voice = 0; voice < 8; ++voice) strike(voice);
        else if (step == 240)
            for (int voice = 8; voice < 16; ++voice) strike(voice);
        else if (step % 24 == 0 && step < 240)
            strike((step / 24 - 1) % 8);
        else if (step % 24 == 0)
            strike((step / 24 - 11) % 16);
        return;
    }
    if (scenario == Scenario::Factory)
    {
        // 120 BPM eighth notes, rounded to 188 callbacks (250.667 ms).
        // Alternate single strings and three-note chords through the
        // compass, cycling velocities 70/90/110 like the level protocol.
        if (block % 188 == 0)
        {
            const int event = block / 188;
            const auto notes = [&] (int step, bool on)
            {
                const int first = (3 * step) % (spec.stringCount - 5);
                for (int k = 0; k < (step % 2 ? 3 : 1); ++k)
                {
                    const int note = spec.strings[static_cast<std::size_t>(first + 2 * k)].midiNote;
                    if (on) performer.noteOn(note, static_cast<float>(70 + 20 * (step % 3)) / 127.0f);
                    else performer.noteOff(note);
                }
            };
            if (event > 0) notes(event - 1, false);
            notes(event, true);
        }
        return;
    }
    const int interval = scenario == Scenario::Repluck ? 16 : 96;
    if (block % interval == 0)
    {
        // A fast four-string arpeggio: 46.875 plucks/s, 11.72/s per string.
        // It exposes the contact projection/copy cost, not just sustain DSP.
        const int string = scenario == Scenario::Repluck ? (block / interval) % 4 + spec.stringCount / 2
                                                        : (block / interval) % spec.stringCount;
        const int note = spec.strings[static_cast<std::size_t>(string)].midiNote;
        performer.noteOff(note);
        performer.noteOn(note, 100.0f / 127.0f);
    }
    if (scenario != Scenario::Gestures)
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

void run(aculyre::Performer& performer, const Options& opt, int preset, int factor, Scenario scenario,
         std::ofstream& samples, std::ofstream& csv, std::uint64_t& sampleOffset,
         const Patch* patch = nullptr)
{
    performer.reset();
    performer.setInstrument(preset);
    auto controls = patch ? patch->controls : aculyre::Performer::defaultControls(preset);
    performer.setControls(controls);
    const auto& spec = *aculyre::presets[static_cast<std::size_t>(preset)];
    if (scenario != Scenario::Idle && scenario != Scenario::Factory && scenario != Scenario::Cold
        && scenario != Scenario::Stress)
        for (int i = 0; i < spec.stringCount; ++i)
            performer.noteOn(spec.strings[static_cast<std::size_t>(i)].midiNote, 100.0f / 127.0f);

    std::array<float, frames> left {}, right {};
    for (int block = 0; block < warmupBlocks; ++block)
        performer.process(left.data(), right.data(), frames);
    const int blocks = static_cast<int>(std::ceil(opt.seconds / period));
    std::vector<Timing> timings(static_cast<std::size_t>(blocks));
    std::vector<float> audio(static_cast<std::size_t>(blocks) * frames * 2);
#if defined(ACULYRE_USE_CALLGRIND)
    // Setup, warm-up, allocations, analysis and export are outside the
    // instruction collection just as they are outside native timing.
    CALLGRIND_START_INSTRUMENTATION;
    CALLGRIND_ZERO_STATS;
#endif
    for (int block = 0; block < blocks; ++block)
    {
        const auto wallStart = wallTime();
        const auto cpuStart = threadTime();
        dispatch(performer, controls, spec, block, scenario);
        performer.process(left.data(), right.data(), frames);
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
#if defined(ACULYRE_USE_CALLGRIND)
    CALLGRIND_STOP_INSTRUMENTATION;
    CALLGRIND_DUMP_STATS_AT(name(scenario));
#endif
    double energy = 0.0, peak = 0.0, totalCpu = 0.0;
    for (float x : audio)
    {
        if (!std::isfinite(x))
            throw std::runtime_error("nonfinite output");
        energy += static_cast<double>(x) * x;
        peak = std::max(peak, std::abs(static_cast<double>(x)));
    }
    if (peak >= 1.0 || (scenario == Scenario::Idle ? energy != 0.0 : energy == 0.0))
        throw std::runtime_error("output is clipped or silent");
    for (const auto& t : timings)
        totalCpu += static_cast<double>(t.cpu) * 1e-9;
    const char* scenarioName = name(scenario);
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
            csv << (patch ? patch->name : "default") << ',' << preset << ',' << factor << ',' << scenarioName << ',' << block << ',' << t.wall << ',' << t.cpu << '\n';
        }
        if (!csv) throw std::runtime_error("CSV write failed");
    }
    std::cout << "    {\"patch\": " << std::quoted(patch ? patch->name : "default")
              << ", \"preset\": " << preset << ", \"oversampling\": " << performer.oversampling()
              << ", \"engine_rate\": " << performer.engine().sampleRate() << ", \"scenario\": \"" << scenarioName
              << "\", \"strings\": " << spec.stringCount << ", \"blocks\": " << blocks
              << ", \"seconds\": " << blocks * period << ", \"sample_offset_frames\": " << sampleOffset
              << ", \"cpu_percent\": " << 100.0 * totalCpu / (blocks * period)
              << ", \"peak\": " << peak << ", \"rms\": " << std::sqrt(energy / audio.size()) << ", ";
    distribution("wall", timings, false);
    std::cout << ", ";
    distribution("thread_cpu", timings, true);
    // Standard U12 judges 256-frame blocks: four callbacks here.
    std::vector<double> quads;
    for (std::size_t block = 0; block + 4 <= timings.size(); block += 4)
        quads.push_back(static_cast<double>(timings[block].cpu + timings[block + 1].cpu + timings[block + 2].cpu
                                            + timings[block + 3].cpu) / (4.0e7 * period));
    std::sort(quads.begin(), quads.end());
    if (!quads.empty())
        std::cout << ", \"block256_percent\": {\"median\": " << quads[quads.size() / 2] << ", \"max\": " << quads.back() << '}';
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
            csv << "patch,preset,oversampling,scenario,block,wall_ns,thread_cpu_ns\n";
        }
        (void)threadTime();
        std::cout << std::setprecision(10)
                  << "{\n  \"sample_rate\": " << rate << ", \"block_frames\": " << frames
                  << ", \"warmup_frames\": " << warmupBlocks * frames
                  << ", \"timer\": \"calling thread CPU and steady wall\",\n  \"cases\": [\n";
        bool first = true;
        std::uint64_t sampleOffset = 0;
        const auto factory = patches(opt.patchDirectory);
        const int configurations = factory.empty() ? aculyre::Performer::instrumentCount : static_cast<int>(factory.size());
        for (int factor : { 1, 2, 4 })
        {
            if (opt.factor && opt.factor != factor) continue;
            // Designing all three instruments is setup work. Reuse that
            // immutable design across cases, resetting every playing state
            // and applying each case's controls before its untimed warm-up.
            auto performer = std::make_unique<aculyre::Performer>();
            performer->prepare(rate, factor);
            for (int configuration = 0; configuration < configurations; ++configuration)
            {
                const Patch* patch = factory.empty() ? nullptr : &factory[static_cast<std::size_t>(configuration)];
                const int preset = patch ? patch->preset : configuration;
                for (Scenario scenario : { Scenario::Dense, Scenario::Gestures, Scenario::Repluck, Scenario::Idle,
                                           Scenario::Factory, Scenario::Burst, Scenario::Cold, Scenario::Stress })
                {
                    if ((opt.preset >= 0 && opt.preset != preset) || (opt.factor && opt.factor != factor)
                        || (opt.scenario != "all" && opt.scenario != name(scenario)))
                        continue;
                    if (!first) std::cout << ",\n";
                    first = false;
                    run(*performer, opt, preset, factor, scenario, samples, csv, sampleOffset, patch);
                }
            }
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
