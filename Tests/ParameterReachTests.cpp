// U11 parameter reach over the Performer and the controls the Rack device
// gives it, through the shared portfolio harness (Tests/ParameterMatrix.h is
// a byte-identical copy of the master; change it in reason-rack-extensions).
// There is no plug-in yet, so the device stores each control normalised and
// maps it as the Rack adapter's publish() does (Examples/Aculyre/Aculyre.cpp).
// Oversampling is left out: the Rack device builds a new instance for it.
//
//   AculyreParameterReachTests --reach [--jobs N] [--case PARAMETER]

#include "AculyrePerformer.h"
#include "ParameterMatrix.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>

namespace {

using namespace aculyre;
namespace matrix = protocodus::matrix;

enum Control {
    kInstrument, kTechnique, kLevers, kPluckPosition, kPluckAngle, kRelease, kMute, kSympathy, kRoom, kWidth,
    kVolume, kPiezo, kPiezoPolarity, kSustain, kControlCount
};

// Name, steps (0 continuous) and the Rack device's default.
const std::array<matrix::Parameter, kControlCount> kControls {{
    { "Instrument", 3, 0.0f }, { "Technique", 3, 0.0f }, { "Levers", 2, 1.0f }, { "Pluck position", 0, 0.4773f },
    { "Pluck angle", 0, 0.4545f }, { "Release", 0, 0.6f }, { "Mute", 0, 0.0f }, { "Sympathy", 0, 1.0f },
    { "Room", 0, 1.0f }, { "Width", 0, 0.5f }, { "Volume", 0, 0.8667f }, { "Piezo", 0, 0.5f },
    { "Piezo polarity", 2, 0.0f }, { "Sustain", 2, 0.0f },
}};

int Index(float normalised, int steps) { return static_cast<int>(std::lround(normalised * static_cast<float>(steps - 1))); }

class AculyreDevice final : public matrix::Device {
public:
    AculyreDevice() : performer(std::make_unique<Performer>()) { LoadPreset(0); }

    std::vector<matrix::Parameter> Parameters() const override {
        return { kControls.begin(), kControls.end() };
    }
    // Each instrument is a preset, at the Rack device's default controls.
    int PresetCount() const override { return Performer::instrumentCount; }
    void Prepare(double rate, int) override {
        performer->prepare(rate);
        Apply();
    }
    void LoadPreset(int index) override {
        for (std::size_t i = 0; i < values.size(); ++i) values[i] = kControls[i].defaultValue;
        values[kInstrument] = static_cast<float>(index) / static_cast<float>(Performer::instrumentCount - 1);
        Apply();
    }
    float GetParameter(std::size_t index) const override { return values[index]; }
    void SetParameter(std::size_t index, float value) override {
        values[index] = value;
        Apply();
    }
    // Performer::process takes no events, so the block is split at each one.
    void Process(float* left, float* right, int frames, const matrix::Event* events, int count) override {
        int done = 0;
        for (int e = 0; e < count; ++e) {
            const int at = std::clamp(events[e].offset, done, frames);
            performer->process(left + done, right + done, at - done);
            done = at;
            Handle(events[e]);
        }
        performer->process(left + done, right + done, frames - done);
    }
    std::vector<std::uint8_t> SaveState() override {
        std::vector<std::uint8_t> state(sizeof values);
        std::memcpy(state.data(), values.data(), sizeof values);
        return state;
    }
    void RestoreState(const std::vector<std::uint8_t>& state) override {
        if (state.size() != sizeof values) return;
        std::memcpy(values.data(), state.data(), sizeof values);
        Apply();
    }

    matrix::ReachRule ReachFor(std::size_t index, float) const override {
        using matrix::Reach;
        switch (static_cast<int>(index)) {
        // AculyreEngine::pluck: a sounding string keeps the pluck it was given.
        // Nail differs from Finger only in letting go at once, brighter from
        // 3 to 10 kHz: heard on the piezo, without the room, plucked near
        // the bridge where those partials are strong.
        case kTechnique:
            return { Reach::NewNotes, "how each new pluck lets go", { { kPiezo, 1.0f }, { kRoom, 0.0f }, { kPluckPosition, 0.0f } } };
        case kPluckPosition: case kPluckAngle:
            return { Reach::NewNotes, "where and which way each new pluck meets the string" };
        // A lever serves only a key between strings (AculyreEngine::noteOn),
        // and the probes play keys with strings of their own, so R and T
        // differ by the long play's cluster alone. Aculyre.Engine plays such
        // a key after long play with the levers down and up again.
        case kLevers:
            return { Reach::Exempt, "only a key between strings uses a lever, and the probes play none", {},
                     "the keys between strings that the long play's cluster played" };
        default: return {};
        }
    }

private:
    void Handle(const matrix::Event& event) {
        const std::uint8_t status = event.bytes[0] & 0xF0;
        const int data1 = event.bytes[1], data2 = event.bytes[2];
        if (status == 0x90 && data2 > 0) {
            performer->noteOn(data1, static_cast<float>(data2) / 127.0f);
        } else if (status == 0x80 || status == 0x90) {
            performer->noteOff(data1);
        } else if (status == 0xE0) {
            // The Rack's pitch wheel: +/-2 semitones.
            pitchWheel = static_cast<double>(data1 | data2 << 7) / 16383.0;
            Apply();
        }
    }
    void Apply() {
        constexpr double pi = 3.14159265358979323846;
        PlayingControls c;
        c.technique = static_cast<Technique>(Index(values[kTechnique], 3));
        c.pluckPosition = 0.06 + 0.44 * values[kPluckPosition];
        const double angle = (5.0 + 55.0 * values[kPluckAngle]) * pi / 180.0;
        c.normalShare = std::sin(angle) * std::sin(angle);
        c.release = values[kRelease];
        c.mute = values[kMute];
        c.sympathy = values[kSympathy];
        c.room = values[kRoom];
        c.width = 2.0 * values[kWidth];
        c.piezo = values[kPiezo];
        c.piezoInverted = values[kPiezoPolarity] >= 0.5f;
        c.volume = std::pow(10.0, (-24.0 + 30.0 * values[kVolume]) / 20.0);
        c.pitchBend = (pitchWheel - 0.5) * 4.0;
        c.levers = values[kLevers] >= 0.5f;
        performer->setControls(c);
        performer->setInstrument(Index(values[kInstrument], 3));
        performer->setSustain(values[kSustain] >= 0.5f);
    }

    std::unique_ptr<Performer> performer;
    std::array<float, kControlCount> values {};
    double pitchWheel = 0.5;
};

} // namespace

int main(int argc, char** argv) {
    return protocodus::matrix::Main(argc, argv, [] { return std::make_unique<AculyreDevice>(); });
}
