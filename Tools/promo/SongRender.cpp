// SongRender <patch> <in.mid> <out.wav> <trace.csv> [--oversampling 1|2|4]
// One Aculyre playing a performed part through the Performer (the shipping
// signal path with its output stage), set as the Rack factory patch <patch>
// is, at 48 kHz. The trace, at 60 Hz, reads the active instrument's
// read-only string observers: per string its smoothed mean squared wave
// velocity, (m/s)^2, and its state (0 quiet or ringing in sympathy only,
// 1 plucked and sounding, 2 held). The first trace row lists each string's
// own MIDI note.
#include "../../Source/DSP/AculyreEngine.h"
#include "../../Source/DSP/AculyrePerformer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace
{

// The factory patches the promo's players use, in the panel's units, copied
// from reason-rack-extensions Examples/Aculyre/Design/generate_patches.py and
// mapped as Aculyre.cpp's publish() maps the panel.
struct Patch { int instrument, technique; bool levers; double position, angle, release, mute, sympathy, room, width, volumeDb, piezo; };
const std::map<std::string, Patch> patches {
    { "Lyre Harp",       { 0, 0, true, 27, 30, 0.6, 0.0, 1.0, 1.0, 100, 2.0, 50 } },
    { "Nylon Lyre Harp", { 1, 0, true, 27, 30, 0.6, 0.0, 1.0, 1.0, 100, 1.0, 50 } },
    { "Gut Lyre",        { 2, 0, true, 11, 32.9, 0.7, 0.0, 1.0, 1.0, 100, 3.6, 50 } },   // asin(sqrt 0.3)
    { "Bright Nail Harp",  { 0, 1, true, 19, 35, 0.8, 0.0, 1.0, 0.30, 110, -0.6, 0 } },
    { "Picked Gut Lyre",   { 2, 2, true, 50, 25, 0.4, 0.15, 0.8, 0.25, 100, -1.0, 0 } },
    { "Damped Steel Pick", { 0, 2, true, 50, 50, 0.2, 0.35, 0.3, 0.20, 90, -0.1, 0 } },
};

aculyre::PlayingControls controlsOf(const Patch& p)
{
    constexpr double pi = 3.14159265358979323846;
    aculyre::PlayingControls c;
    c.technique = static_cast<aculyre::Technique>(p.technique);
    c.pluckPosition = p.position / 100.0;   // the panel's 6-50 %
    const double angle = p.angle * pi / 180.0;
    c.normalShare = std::sin(angle) * std::sin(angle);
    c.release = p.release;
    c.mute = p.mute;
    c.sympathy = p.sympathy;
    c.room = p.room;
    c.width = p.width / 100.0;
    c.piezo = p.piezo / 100.0;
    c.volume = std::pow(10.0, p.volumeDb / 20.0);
    c.levers = p.levers;
    return c;
}

struct Event { double seconds; unsigned char status, data1, data2; };

unsigned readVariable(const std::vector<unsigned char>& b, std::size_t& i)
{
    unsigned v = 0;
    for (int k = 0; k < 4 && i < b.size(); ++k) { const unsigned char c = b[i++]; v = (v << 7) | (c & 0x7f); if (!(c & 0x80)) break; }
    return v;
}

// Format 0 or 1 with PPQ division; tempo changes in any track apply to all.
std::vector<Event> readMidi(const char* path)
{
    std::ifstream f(path, std::ios::binary);
    const std::vector<unsigned char> b((std::istreambuf_iterator<char>(f)), {});
    if (b.size() < 14 || std::memcmp(b.data(), "MThd", 4) != 0) { std::fprintf(stderr, "not a MIDI file: %s\n", path); std::exit(1); }
    const int tracks = b[10] << 8 | b[11], ppq = b[12] << 8 | b[13];
    struct Raw { unsigned long tick; int order; unsigned char s, d1, d2; unsigned tempo; };
    std::vector<Raw> raw;
    std::size_t at = 14;
    for (int t = 0; t < tracks && at + 8 <= b.size(); ++t)
    {
        const std::size_t length = static_cast<std::size_t>(b[at + 4]) << 24 | b[at + 5] << 16 | b[at + 6] << 8 | b[at + 7];
        std::size_t i = at + 8;
        const std::size_t end = i + length;
        unsigned long tick = 0;
        unsigned char running = 0;
        while (i < end)
        {
            tick += readVariable(b, i);
            unsigned char s = b[i];
            if (s == 0xff) { const unsigned char type = b[i + 1]; i += 2; const unsigned n = readVariable(b, i);
                if (type == 0x51 && n == 3) raw.push_back({ tick, 0, 0, 0, 0, static_cast<unsigned>(b[i] << 16 | b[i + 1] << 8 | b[i + 2]) });
                i += n; continue; }
            if (s == 0xf0 || s == 0xf7) { ++i; i += readVariable(b, i); continue; }
            if (s & 0x80) { running = s; ++i; } else s = running;
            const int kind = s & 0xf0;
            const unsigned char d1 = b[i], d2 = (kind == 0xc0 || kind == 0xd0) ? 0 : b[i + 1];
            i += (kind == 0xc0 || kind == 0xd0) ? 1 : 2;
            raw.push_back({ tick, static_cast<int>(raw.size()) + 1, s, d1, d2, 0 });
        }
        at = end;
    }
    std::stable_sort(raw.begin(), raw.end(), [] (const Raw& a, const Raw& c) { return a.tick < c.tick; });
    std::vector<Event> events;
    double seconds = 0.0, tempo = 500000.0;
    unsigned long last = 0;
    for (const Raw& r : raw)
    {
        seconds += (r.tick - last) * tempo / 1e6 / ppq;
        last = r.tick;
        if (r.tempo) tempo = r.tempo; else events.push_back({ seconds, r.s, r.d1, r.d2 });
    }
    return events;
}

void writeWav24(const char* path, const std::vector<float>& l, const std::vector<float>& r, int rate)
{
    std::ofstream f(path, std::ios::binary);
    const auto put = [&] (unsigned v, int n) { for (int k = 0; k < n; ++k) f.put(static_cast<char>((v >> (8 * k)) & 0xff)); };
    const unsigned bytes = static_cast<unsigned>(l.size()) * 6u;
    f.write("RIFF", 4); put(36 + bytes, 4); f.write("WAVEfmt ", 8); put(16, 4); put(1, 2); put(2, 2);
    put(static_cast<unsigned>(rate), 4); put(static_cast<unsigned>(rate) * 6, 4); put(6, 2); put(24, 2); f.write("data", 4); put(bytes, 4);
    for (std::size_t i = 0; i < l.size(); ++i)
        for (const float s : { l[i], r[i] })
            put(static_cast<unsigned>(static_cast<int>(std::lround(std::clamp(static_cast<double>(s), -1.0, 1.0) * 8388607.0))), 3);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 5 || !patches.count(argv[1])) { std::fprintf(stderr, "usage: SongRender <patch> <in.mid> <out.wav> <trace.csv> [--oversampling 1|2|4]\n"); return 2; }
    int oversampling = 1;
    if (argc >= 7 && std::string(argv[5]) == "--oversampling") oversampling = std::atoi(argv[6]);
    const Patch& patch = patches.at(argv[1]);
    constexpr int rate = 48000, step = 800;   // the trace's 60 Hz
    auto performer = std::make_unique<aculyre::Performer>();
    performer->prepare(rate, oversampling);
    performer->setControls(controlsOf(patch));
    performer->setInstrument(patch.instrument);
    // Each observer is read at the output time it describes: the latency is
    // trimmed off the audio, so the trace is taken that much later too.
    const int latency = performer->latency();
    const auto events = readMidi(argv[2]);
    const auto total = static_cast<std::size_t>(std::ceil((events.back().seconds + 4.0) * rate)) + latency;
    std::vector<float> left(total), right(total);
    const auto& engine = performer->engine();
    const int strings = engine.stringCount();
    std::ofstream csv(argv[4]);
    csv << "notes";
    for (int s = 0; s < strings; ++s) csv << ',' << engine.stringReport(s).midiNote;
    csv << '\n';
    std::size_t next = 0, done = 0;
    const auto due = [&] (std::size_t i) { return static_cast<std::size_t>(std::llround(events[i].seconds * rate)); };
    while (done < total)
    {
        while (next < events.size() && due(next) <= done)
        {
            const Event& e = events[next++];
            const int kind = e.status & 0xf0;
            if (kind == 0x90 && e.data2 > 0) performer->noteOn(e.data1, e.data2 / 127.0f);
            else if (kind == 0x80 || kind == 0x90) performer->noteOff(e.data1);
            else if (kind == 0xb0 && e.data1 == 64) performer->setSustain(e.data2 >= 64);
        }
        std::size_t until = std::min(total, done + 64);
        if (done >= static_cast<std::size_t>(latency)) until = std::min(until, (done - latency) / step * step + step + latency);
        if (next < events.size()) until = std::min(until, std::max(done + 1, due(next)));
        performer->process(&left[done], &right[done], static_cast<int>(until - done));
        done = until;
        if (done > static_cast<std::size_t>(latency) && (done - latency) % step == 0)
        {
            char cell[48];
            std::snprintf(cell, sizeof cell, "%.4f", static_cast<double>(done - latency) / rate);
            csv << cell;
            for (int s = 0; s < strings; ++s)
            {
                const auto a = engine.stringActivity(s);
                std::snprintf(cell, sizeof cell, ",%.3g:%d", a.energy, a.held ? 2 : a.plucked ? 1 : 0);
                csv << cell;
            }
            csv << '\n';
        }
    }
    left.erase(left.begin(), left.begin() + latency);
    right.erase(right.begin(), right.begin() + latency);
    float peak = 0.0f;
    for (std::size_t i = 0; i < left.size(); ++i) peak = std::max({ peak, std::fabs(left[i]), std::fabs(right[i]) });
    writeWav24(argv[3], left, right, rate);
    std::printf("%s: %s, %.1f s, %dx, peak %.2f dBFS\n", argv[3], argv[1], left.size() / double(rate), performer->oversampling(), 20 * std::log10(peak + 1e-12));
}
