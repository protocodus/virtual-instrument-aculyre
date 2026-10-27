// Renders a standard MIDI file through Aculyre to a 48 kHz 24-bit stereo WAV.
//
//   RenderAculyreDemo <preset 0-2> <input.mid> <output.wav>
//       [--dry] [--no-sympathy] [--piezo 0-1] [--tail seconds] [--rate hz]
//   RenderAculyreDemo --report <preset>
//
// Note on/off and CC64 (sustain) drive the Performer, the shipping signal
// path with its output stage, at their sample; everything else in the file
// is ignored. The Performer's latency is trimmed off the start.

#include "../Source/DSP/AculyreEngine.h"
#include "../Source/DSP/AculyrePerformer.h"
#include "../Source/DSP/AculyrePresets.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{

struct Event
{
    double seconds;
    int order;
    std::uint8_t status;
    std::uint8_t data1;
    std::uint8_t data2;
};

std::uint32_t readVariable(const std::vector<std::uint8_t>& bytes, std::size_t& position)
{
    std::uint32_t value = 0;
    for (int count = 0; count < 4 && position < bytes.size(); ++count)
    {
        const std::uint8_t byte = bytes[position++];
        value = (value << 7) | (byte & 0x7fu);
        if (!(byte & 0x80u))
            break;
    }
    return value;
}

std::uint32_t bigEndian(const std::uint8_t* p, int count)
{
    std::uint32_t value = 0;
    for (int i = 0; i < count; ++i)
        value = (value << 8) | p[i];
    return value;
}

// Format 0 or 1, PPQ division; tempo changes in any track apply to all.
std::vector<Event> readMidi(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("cannot open " + path);
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
    if (bytes.size() < 14 || std::memcmp(bytes.data(), "MThd", 4) != 0)
        throw std::runtime_error("not a MIDI file: " + path);
    const auto tracks = static_cast<int>(bigEndian(&bytes[10], 2));
    const auto division = static_cast<int>(bigEndian(&bytes[12], 2));
    if (division & 0x8000)
        throw std::runtime_error("SMPTE division is not supported");
    struct TickEvent { std::uint64_t tick; int order; std::uint8_t status, data1, data2; std::uint32_t tempo; };
    std::vector<TickEvent> ticked;
    std::size_t position = 8 + bigEndian(&bytes[4], 4);
    int order = 0;
    for (int track = 0; track < tracks && position + 8 <= bytes.size(); ++track)
    {
        if (std::memcmp(&bytes[position], "MTrk", 4) != 0)
            throw std::runtime_error("bad track header");
        const std::size_t length = bigEndian(&bytes[position + 4], 4);
        std::size_t p = position + 8;
        const std::size_t end = std::min(bytes.size(), p + length);
        std::uint64_t tick = 0;
        std::uint8_t running = 0;
        while (p < end)
        {
            tick += readVariable(bytes, p);
            std::uint8_t status = bytes[p];
            if (status & 0x80u)
                ++p;
            else
                status = running;
            if (status == 0xff)
            {
                const std::uint8_t type = bytes[p++];
                const std::uint32_t size = readVariable(bytes, p);
                if (type == 0x51 && size == 3)
                    ticked.push_back({ tick, order++, 0xff, 0, 0, bigEndian(&bytes[p], 3) });
                p += size;
                continue;
            }
            if (status == 0xf0 || status == 0xf7)
            {
                p += readVariable(bytes, p);
                continue;
            }
            running = status;
            const std::uint8_t kind = status & 0xf0u;
            const std::uint8_t d1 = bytes[p++];
            std::uint8_t d2 = 0;
            if (kind != 0xc0 && kind != 0xd0)
                d2 = bytes[p++];
            ticked.push_back({ tick, order++, status, d1, d2, 0 });
        }
        position = end;
    }
    std::stable_sort(ticked.begin(), ticked.end(),
                     [] (const TickEvent& a, const TickEvent& b) { return a.tick < b.tick; });
    std::vector<Event> events;
    double secondsPerTick = 0.5 / division;
    std::uint64_t lastTick = 0;
    double seconds = 0.0;
    for (const auto& e : ticked)
    {
        seconds += static_cast<double>(e.tick - lastTick) * secondsPerTick;
        lastTick = e.tick;
        if (e.status == 0xff)
        {
            secondsPerTick = e.tempo * 1.0e-6 / division;
            continue;
        }
        events.push_back({ seconds, e.order, e.status, e.data1, e.data2 });
    }
    return events;
}

void writeWav24(const std::string& path, const std::vector<float>& left,
                const std::vector<float>& right, int sampleRate)
{
    std::ofstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("cannot write " + path);
    const std::uint32_t frames = static_cast<std::uint32_t>(left.size());
    const std::uint32_t dataBytes = frames * 2u * 3u;
    const auto put32 = [&] (std::uint32_t v) { for (int i = 0; i < 4; ++i) file.put(static_cast<char>((v >> (8 * i)) & 0xff)); };
    const auto put16 = [&] (std::uint16_t v) { file.put(static_cast<char>(v & 0xff)); file.put(static_cast<char>(v >> 8)); };
    file.write("RIFF", 4);
    put32(36u + dataBytes);
    file.write("WAVEfmt ", 8);
    put32(16);
    put16(1);
    put16(2);
    put32(static_cast<std::uint32_t>(sampleRate));
    put32(static_cast<std::uint32_t>(sampleRate) * 6u);
    put16(6);
    put16(24);
    file.write("data", 4);
    put32(dataBytes);
    for (std::uint32_t i = 0; i < frames; ++i)
        for (const float sample : { left[i], right[i] })
        {
            const double clamped = std::clamp(static_cast<double>(sample), -1.0, 1.0);
            const auto value = static_cast<std::int32_t>(std::lround(clamped * 8388607.0));
            file.put(static_cast<char>(value & 0xff));
            file.put(static_cast<char>((value >> 8) & 0xff));
            file.put(static_cast<char>((value >> 16) & 0xff));
        }
}

int report(int preset)
{
    aculyre::AculyreEngine engine;
    engine.prepare(48000.0);
    engine.setInstrument(*aculyre::presets[static_cast<std::size_t>(preset)]);
    std::printf("%s\n", aculyre::presets[static_cast<std::size_t>(preset)]->name);
    std::printf("string  note  f0(Hz)   T(N)   Z(kg/s)  B         stress(MPa)  intrinsic T60(s)  dispersion  stages  worst(cents)\n");
    for (int i = 0; i < engine.stringCount(); ++i)
    {
        const auto r = engine.stringReport(i);
        std::printf("%6d %5d %8.2f %6.1f %8.3f  %.2e  %10.0f  %16.2f  %10.4f  %6d  %12.2f\n", i, r.midiNote,
                    r.frequency, r.tension, r.impedance, r.inharmonicity, r.stress / 1e6,
                    r.intrinsicT60, r.dispersion, r.dispersionStages, r.dispersionCents);
    }
    std::printf("\nmodes: %d\n  f(Hz)     Q    heave(kg^-1/2)  monopole(m^2)  air share\n", engine.modeCount());
    for (int i = 0; i < std::min(engine.modeCount(), 24); ++i)
    {
        const auto m = engine.modeReport(i);
        std::printf("%8.1f %6.1f %12.4f %14.5f %10.3f\n", m.frequency, m.q, m.heave, m.monopole, m.airShare);
    }
    std::printf("\nbridge mobility |Y| (m/s/N) at the middle of the bar:\n");
    for (double f : { 100.0, 150.0, 200.0, 250.0, 300.0, 400.0, 500.0, 700.0, 1000.0, 1500.0, 2000.0, 3000.0, 5000.0, 8000.0 })
        std::printf("  %6.0f Hz  %.4f\n", f, engine.bridgeMobilityMagnitude(f, 0.0));
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc >= 3 && std::string(argv[1]) == "--report")
            return report(std::clamp(std::atoi(argv[2]), 0, 2));
        if (argc < 4)
        {
            std::cerr << "usage: RenderAculyreDemo <preset 0-2> <input.mid> <output.wav> "
                         "[--dry] [--no-sympathy] [--piezo 0-1] [--tail seconds] [--rate hz]\n";
            return 2;
        }
        const int preset = std::clamp(std::atoi(argv[1]), 0, 2);
        const std::string input = argv[2];
        const std::string output = argv[3];
        bool dry = false, sympathy = true;
        double tail = 2.5, piezo = 0.0;
        int rate = 48000;
        for (int i = 4; i < argc; ++i)
        {
            const std::string option = argv[i];
            if (option == "--dry") dry = true;
            else if (option == "--no-sympathy") sympathy = false;
            else if (option == "--piezo" && i + 1 < argc) piezo = std::atof(argv[++i]);
            else if (option == "--tail" && i + 1 < argc) tail = std::atof(argv[++i]);
            else if (option == "--rate" && i + 1 < argc) rate = std::atoi(argv[++i]);
        }
        auto performer = std::make_unique<aculyre::Performer>();
        performer->prepare(rate);
        performer->setInstrument(preset);
        aculyre::PlayingControls controls = aculyre::Performer::defaultControls(preset);
        if (dry)
            controls.room = 0.0;
        if (!sympathy)
            controls.sympathy = 0.0;
        controls.piezo = piezo;
        performer->setControls(controls);
        const auto latency = static_cast<std::size_t>(performer->latency());

        const std::vector<Event> events = readMidi(input);
        const double lastSeconds = events.empty() ? 0.0 : events.back().seconds;
        const auto total = static_cast<std::size_t>(std::ceil((lastSeconds + tail) * rate)) + latency;
        std::vector<float> left(total), right(total);
        constexpr int blockSize = 64;
        std::size_t next = 0;
        std::size_t position = 0;
        while (position < total)
        {
            while (next < events.size()
                   && static_cast<std::size_t>(std::llround(events[next].seconds * rate)) <= position)
            {
                const Event& e = events[next++];
                const std::uint8_t kind = e.status & 0xf0u;
                if (kind == 0x90 && e.data2 > 0)
                    performer->noteOn(e.data1, e.data2 / 127.0f);
                else if (kind == 0x80 || (kind == 0x90 && e.data2 == 0))
                    performer->noteOff(e.data1);
                else if (kind == 0xb0 && e.data1 == 64)
                    performer->setSustain(e.data2 >= 64);
            }
            std::size_t blockEnd = std::min(total, position + blockSize);
            if (next < events.size())
                blockEnd = std::min(blockEnd, std::max(position + 1,
                    static_cast<std::size_t>(std::llround(events[next].seconds * rate))));
            performer->process(left.data() + position, right.data() + position,
                               static_cast<int>(blockEnd - position));
            position = blockEnd;
        }
        left.erase(left.begin(), left.begin() + static_cast<std::ptrdiff_t>(latency));
        right.erase(right.begin(), right.begin() + static_cast<std::ptrdiff_t>(latency));
        // Reason's VU meter: the RMS over 300 ms, 0 VU at -12 dBFS.
        const std::size_t frames = left.size(), window = static_cast<std::size_t>(0.3 * rate);
        double peak = 0.0, sum = 0.0, windowSum = 0.0, loudest = 0.0;
        for (std::size_t i = 0; i < frames; ++i)
        {
            peak = std::max(peak, static_cast<double>(std::max(std::abs(left[i]), std::abs(right[i]))));
            const double power = 0.5 * (left[i] * left[i] + right[i] * right[i]);
            sum += power;
            windowSum += power;
            if (i >= window)
                windowSum -= 0.5 * (left[i - window] * left[i - window] + right[i - window] * right[i - window]);
            loudest = std::max(loudest, windowSum / static_cast<double>(window));
        }
        writeWav24(output, left, right, rate);
        std::printf("%s: %.2f s, peak %.2f dBFS, rms %.2f dBFS, VU max %+.1f\n", output.c_str(),
                    static_cast<double>(frames) / rate, 20.0 * std::log10(peak + 1e-12),
                    10.0 * std::log10(sum / static_cast<double>(frames) + 1e-24),
                    10.0 * std::log10(loudest + 1e-24) + 12.0);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
