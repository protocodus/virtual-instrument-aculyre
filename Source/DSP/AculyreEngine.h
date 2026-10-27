// Aculyre: an experimental physically modelled lyre.
//
// Every string is open (one pitch per string, no frets). Each is a pair of
// stiff-string waveguides, one per polarisation, and all of them end on one
// rigid bridge bar standing on a modal soundbox: the bar's heave and rock
// carry the strings' motion normal to the soundboard, its sway the motion
// parallel to it. Idle strings stay in the junction, so they ring in
// sympathy. The soundbox radiates through its plate modes and its air
// resonance into a small room.
//
// This is a feasibility prototype (README, Known gaps), not a product: setup
// calls allocate nothing but are not real-time safe; process() and the note
// calls are.

#pragma once

#include <array>
#include <cstdint>
#include <memory>

namespace aculyre
{

inline constexpr int maxStrings = 24;
inline constexpr int maxModes = 400;

struct StringMaterial
{
    const char* name;
    double density;       // kg/m^3
    double youngModulus;  // Pa
    // Loss angle constant over the audio band: dislocation in steel,
    // viscoelastic loss in polymer and gut (constantLossSection).
    double constantLoss;
    // Loss factor eta of the bending stiffness, EI(1 + i eta)
    // (bendingLossSection).
    double bendingLoss;
};

struct StringSpec
{
    int midiNote;
    double length;    // speaking length, bridge to the yoke, m
    double diameter;  // m
};

struct WoodSpec
{
    double density;      // kg/m^3
    double eAlong;       // Pa, along the grain (the strings' direction)
    double eAcross;      // Pa
    double shear;        // Pa, G_LR
    double poisson;      // nu_LR
};

struct BodySpec
{
    double topLength;      // soundboard along the strings, m
    double topWidth;       // across the strings, m
    double topThickness;   // m
    WoodSpec wood;
    double cavityVolume;   // m^3
    double holeRadius;     // m; 0 for a closed box
    double bridgeAlong;    // bridge line from the lower edge, fraction of topLength
    double bridgeFrom;     // bridge span across the width, fractions
    double bridgeTo;
    double bridgeHeight;   // string height above the soundboard, m
    double bridgeMass;     // kg
};

enum class Technique
{
    Finger,
    Nail,
    Plectrum
};

struct InstrumentSpec
{
    const char* name;
    StringMaterial material;
    int stringCount;
    std::array<StringSpec, maxStrings> strings;
    BodySpec body;
    Technique technique;
    double pluckPosition;  // fraction of the length from the bridge
    double normalShare;    // share of the pluck's energy normal to the top
    double pluckForce;     // N held before release at full velocity
    double room;           // room send, 0-1
    double outputGain;     // digital full scale per pascal at the microphones
};

class AculyreEngine
{
public:
    AculyreEngine();
    ~AculyreEngine();

    // Setup: designs the strings, the body and the room. Not real-time.
    void prepare(double sampleRate);
    void setInstrument(const InstrumentSpec& instrument);
    void reset() noexcept;

    // The string tuned to midiNote plays; a note no string is tuned to is
    // ignored (a lyre has no frets).
    void noteOn(int midiNote, float velocity) noexcept;
    // The playing hand damps the string, unless sustain holds it.
    void noteOff(int midiNote) noexcept;
    void setSustain(bool down) noexcept;
    // Ablation for listening: off, a hand rests on every string that is not
    // sounding a note, so nothing rings in sympathy.
    void setSympatheticStrings(bool enabled) noexcept { sympathetic_ = enabled; }
    void setRoomEnabled(bool enabled) noexcept { roomEnabled_ = enabled; }
    void setTensionModulation(bool enabled) noexcept { tensionModulation_ = enabled; }

    void process(float* left, float* right, int numSamples) noexcept;

    [[nodiscard]] int stringForNote(int midiNote) const noexcept;
    // The string's smoothed mean squared wave velocity, (m/s)^2: an observer.
    [[nodiscard]] double stringEnergy(int string) const noexcept;
    [[nodiscard]] int stringCount() const noexcept { return stringCount_; }
    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }

    struct StringReport
    {
        int midiNote;
        double frequency;    // designed open pitch, Hz
        double tension;      // N
        double impedance;    // kg/s
        double inharmonicity;
        double stress;       // Pa
        double intrinsicT60; // s, fundamental, string losses alone
        double dispersion;   // first-order allpass coefficient
        int dispersionStages;
        double dispersionCents; // worst partial to H12 against the stiff-string law
    };
    [[nodiscard]] StringReport stringReport(int string) const noexcept;

    struct ModeReport
    {
        double frequency;
        double q;
        double heave;       // bridge participation, unit modal mass
        double monopole;    // net volume per unit modal displacement, m^2
        double airShare;    // kinetic energy share of the air in the hole
    };
    [[nodiscard]] int modeCount() const noexcept;
    [[nodiscard]] ModeReport modeReport(int mode) const noexcept;
    // Driving-point mobility of the bridge at a lever arm u (-1 bass end,
    // +1 treble end), normal to the top, m/s/N, with the strings' load.
    [[nodiscard]] double bridgeMobilityMagnitude(double frequency, double lever) const noexcept;

private:
    struct State;
    std::unique_ptr<State> state_;
    double sampleRate_ { 48000.0 };
    int stringCount_ { 0 };
    bool sympathetic_ { true };
    bool roomEnabled_ { true };
    bool tensionModulation_ { true };
};

} // namespace aculyre
