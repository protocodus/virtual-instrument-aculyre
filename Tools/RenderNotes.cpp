// Renders every string of a preset plucked alone, one 32-bit float stereo WAV
// per note, for measuring the model note by note against a sampled lyre
// (Tools/compare_lyre_harp_samples.py).
//
//   RenderAculyreNotes <preset 0-2> <output-directory>
//       [--rate hz] [--velocity 0-1] [--seconds s] [--position p]
//       [--positions note:p,note:p,...] [--flatten note,note,...]
//       [--piezo 0-1] [--takes n]
//
// Each note starts from a reset engine 10 ms into its file, at the preset's
// own controls with the room off. --flatten tunes the named strings a
// semitone down, the same strings slackened, as a sample set strung for C
// major but tuned to another scale has them; --positions sets the pluck
// point note by note, by the note the string sounds; --piezo blends in the
// piezo as PlayingControls::piezo does. --takes plucks each note n times
// in a row, one file's length apart, so the takes draw their own scatter
// (<note>-<k>.wav, k from 1); without it every note takes the first draws,
// from a reset engine. This is the engine
// alone, without the Performer's output stage or oversampling: what is
// measured is the strings and the body, not the level guard.

#include "../Source/DSP/AculyreEngine.h"
#include "../Source/DSP/AculyrePerformer.h"
#include "../Source/DSP/AculyrePresets.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{

void writeFloatWav(const std::string& path, const std::vector<float>& left, const std::vector<float>& right,
                   int sampleRate)
{
    std::ofstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("cannot write " + path);
    const auto frames = static_cast<std::uint32_t>(left.size());
    const std::uint32_t dataBytes = frames * 8u;
    const auto put32 = [&] (std::uint32_t v) { for (int i = 0; i < 4; ++i) file.put(static_cast<char>((v >> (8 * i)) & 0xff)); };
    const auto put16 = [&] (std::uint16_t v) { file.put(static_cast<char>(v & 0xff)); file.put(static_cast<char>(v >> 8)); };
    file.write("RIFF", 4);
    put32(36u + dataBytes);
    file.write("WAVEfmt ", 8);
    put32(16);
    put16(3);  // IEEE float
    put16(2);
    put32(static_cast<std::uint32_t>(sampleRate));
    put32(static_cast<std::uint32_t>(sampleRate) * 8u);
    put16(8);
    put16(32);
    file.write("data", 4);
    put32(dataBytes);
    for (std::uint32_t i = 0; i < frames; ++i)
    {
        file.write(reinterpret_cast<const char*>(&left[i]), 4);
        file.write(reinterpret_cast<const char*>(&right[i]), 4);
    }
}

std::vector<std::string> split(const std::string& text)
{
    std::vector<std::string> items;
    std::stringstream stream(text);
    for (std::string item; std::getline(stream, item, ',');)
        if (!item.empty())
            items.push_back(item);
    return items;
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc < 3)
        {
            std::cerr << "usage: RenderAculyreNotes <preset 0-2> <output-directory> [--rate hz] [--velocity 0-1] "
                         "[--seconds s] [--position p] [--positions note:p,...] [--flatten note,...] "
                         "[--piezo 0-1] [--takes n]\n";
            return 2;
        }
        const int preset = std::clamp(std::atoi(argv[1]), 0, 2);
        const std::string output = argv[2];
        int rate = 48000;
        double velocity = 0.7, seconds = 8.0, position = -1.0, piezo = 0.0;
        int takes = 0;
        std::map<int, double> positions;
        std::vector<int> flatten;
        for (int i = 3; i < argc; ++i)
        {
            const std::string option = argv[i];
            if (i + 1 >= argc)
                throw std::runtime_error("missing value for " + option);
            const std::string value = argv[++i];
            if (option == "--rate") rate = std::atoi(value.c_str());
            else if (option == "--velocity") velocity = std::atof(value.c_str());
            else if (option == "--seconds") seconds = std::atof(value.c_str());
            else if (option == "--position") position = std::atof(value.c_str());
            else if (option == "--positions")
                for (const std::string& item : split(value))
                {
                    const auto colon = item.find(':');
                    if (colon == std::string::npos)
                        throw std::runtime_error("--positions takes note:position pairs");
                    positions[std::atoi(item.substr(0, colon).c_str())] = std::atof(item.substr(colon + 1).c_str());
                }
            else if (option == "--piezo") piezo = std::atof(value.c_str());
            else if (option == "--takes") takes = std::max(1, std::atoi(value.c_str()));
            else if (option == "--flatten")
                for (const std::string& item : split(value))
                    flatten.push_back(std::atoi(item.c_str()));
            else
                throw std::runtime_error("unknown option " + option);
        }

        aculyre::InstrumentSpec spec = *aculyre::presets[static_cast<std::size_t>(preset)];
        for (int index = 0; index < spec.stringCount; ++index)
        {
            int& note = spec.strings[static_cast<std::size_t>(index)].midiNote;
            if (std::find(flatten.begin(), flatten.end(), note) != flatten.end())
                --note;
        }
        auto engine = std::make_unique<aculyre::AculyreEngine>();
        engine->prepare(rate);
        engine->setInstrument(spec);
        aculyre::PlayingControls controls = aculyre::Performer::defaultControls(preset);
        controls.room = 0.0;
        controls.piezo = piezo;
        if (position > 0.0)
            controls.pluckPosition = position;

        const auto frames = static_cast<std::size_t>(seconds * rate);
        const auto start = static_cast<std::size_t>(0.01 * rate);
        for (int index = 0; index < spec.stringCount; ++index)
        {
            const int note = spec.strings[static_cast<std::size_t>(index)].midiNote;
            aculyre::PlayingControls own = controls;
            if (const auto found = positions.find(note); found != positions.end())
                own.pluckPosition = found->second;
            engine->reset();
            engine->setControls(own);
            for (int take = 0; take < std::max(1, takes); ++take)
            {
                std::vector<float> left(frames), right(frames);
                engine->process(left.data(), right.data(), static_cast<int>(start));
                engine->noteOn(note, static_cast<float>(velocity));
                for (std::size_t p = start; p < frames; p += 64)
                    engine->process(left.data() + p, right.data() + p,
                                    static_cast<int>(std::min<std::size_t>(64, frames - p)));
                const std::string name = takes ? std::to_string(note) + "-" + std::to_string(take + 1)
                                               : std::to_string(note);
                writeFloatWav(output + "/" + name + ".wav", left, right, rate);
            }
            const std::string path = output + "/" + std::to_string(note) + (takes ? "-*" : "") + ".wav";
            const auto report = engine->stringReport(index);
            std::printf("%s: f0 %.2f Hz, T %.1f N, B %.2e, pluck at %.3f\n", path.c_str(), report.frequency,
                        report.tension, report.inharmonicity, engine->lastPluck().position);
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
