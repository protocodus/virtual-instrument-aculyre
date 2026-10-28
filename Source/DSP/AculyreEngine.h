// Aculyre: an experimental physically modelled lyre.
//
// Every string is open (one pitch per string, no frets). Each is a pair of
// stiff-string waveguides, one per polarisation, and all of them end on one
// rigid bridge bar standing on a modal soundbox: the bar's heave and rock
// carry the strings' motion normal to the soundboard, its sway the motion
// parallel to it. Idle strings stay in the junction, so they ring in
// sympathy. The soundbox radiates through its plate modes and its air
// resonance into a small room; a piezo strip under the bar picks up the
// force the bar presses into the top.
//
// This is a feasibility prototype (README, Known gaps), not a product: setup
// calls allocate nothing but are not real-time safe; process(), the note
// calls and setControls() are.

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
    // Young's modulus at the string's stress sigma (Pa), E' = youngModulus
    // + modulusPerStress * sigma: polymer strings stiffen under tension.
    double youngModulus;  // Pa
    double modulusPerStress;
    // Loss angle constant over the audio band: dislocation in steel
    // (constantLossSection).
    double constantLoss;
    // Loss modulus E'' of the bending stiffness, EI(1 + i E''/E')
    // (bendingLossSection).
    double lossModulus;   // Pa
    // Air damping by Woodhouse and Lynch-Aird's fit to measured polymer and
    // gut strings rather than Valette's law.
    bool measuredAirDamping;
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
    // How the instrument is played by default; setInstrument() starts its
    // PlayingControls from these.
    Technique technique;
    double pluckPosition;  // fraction of the length from the bridge
    double normalShare;    // share of the pluck's energy normal to the top
    double pluckForce;     // N held before release at full velocity
    double room;           // room send, 0-1
    double outputGain;     // digital full scale per pascal at the microphones
    double piezoGain;      // digital full scale per newton on the piezo
};

// What a player and the room change while the instrument sounds. Every
// field takes effect in real time: the plucking fields at the next pluck,
// the rest at once.
struct PlayingControls
{
    Technique technique { Technique::Finger };
    double pluckPosition { 0.25 };  // fraction of the speaking length from the bridge
    double normalShare { 0.25 };    // share of the pluck's energy normal to the top
    // How a string sounds on after its key is let go: 0, the playing hand
    // stops it (an amplitude time constant of 30 ms); 1, it rings until it
    // dies away by itself.
    double release { 0.0 };
    // A palm resting across every string, 0-1.
    double mute { 0.0 };
    // 1, strings nobody has plucked ring in sympathy; 0, a resting hand
    // keeps them still.
    double sympathy { 1.0 };
    double room { 0.35 };       // room send, 0-1
    double width { 1.0 };       // stereo width, 0 (mono) to 2
    // The pickup: 0, the microphones; 1, the piezo under the bridge bar
    // alone; between, a blend of the two. The piezo hears no room.
    double piezo { 0.0 };
    double volume { 1.0 };      // gain on the instrument's outputGain
    double pitchBend { 0.0 };   // semitones, on every string
    // Sharping levers at the yoke follow the notes played: a note a semitone
    // above a string's own raises that string's lever, shortening it; its
    // own note lowers the lever again.
    bool levers { false };
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

    // The string tuned to midiNote plays (with levers, also the string a
    // semitone below, its lever raised); a note no string can sound is
    // ignored (a lyre has no frets).
    void noteOn(int midiNote, float velocity) noexcept;
    // The playing hand damps the string (PlayingControls::release), unless
    // sustain holds it.
    void noteOff(int midiNote) noexcept;
    void setSustain(bool down) noexcept;
    void setControls(const PlayingControls& controls) noexcept;
    [[nodiscard]] const PlayingControls& controls() const noexcept { return controls_; }
    // Ablation for listening: the hard pluck's pitch glide.
    void setTensionModulation(bool enabled) noexcept { tensionModulation_ = enabled; }

    void process(float* left, float* right, int numSamples) noexcept;

    [[nodiscard]] const InstrumentSpec& instrument() const noexcept;
    // The string whose own (lever down) note is midiNote, or -1.
    [[nodiscard]] int stringForNote(int midiNote) const noexcept;
    // The string's smoothed mean squared wave velocity, (m/s)^2: an observer.
    [[nodiscard]] double stringEnergy(int string) const noexcept;
    struct StringActivity
    {
        double energy;  // as stringEnergy()
        bool held;      // its key is down or sustain holds it
        bool plucked;   // played, and still sounding from that pluck
        bool raised;    // its sharping lever is up
    };
    [[nodiscard]] StringActivity stringActivity(int string) const noexcept;
    // Asleep: everything has died away below -170 dBFS, so process() writes
    // silence without running the strings, the body or the room until the
    // next pluck.
    [[nodiscard]] bool idle() const noexcept;
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
        int dispersionPairs;    // second-order sections fitted past the 12th partial
        double dispersionCents; // worst partial below 5 kHz (to H12 at most where
                                // first-order sections alone fit) against the stiff-string law
    };
    [[nodiscard]] StringReport stringReport(int string) const noexcept;

    // The last pluck as drawn around the playing controls.
    struct PluckReport
    {
        double position;     // fraction of the speaking length from the bridge
        double force;        // N, held before the release
        double normalShare;  // share of the pluck's energy normal to the top
        double release;      // s, the held force's unloading time
    };
    [[nodiscard]] PluckReport lastPluck() const noexcept;

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
    void applyPitch(int string, bool raised) noexcept;
    void updateDamping(int string) noexcept;
    void sleep() noexcept;

    std::unique_ptr<State> state_;
    PlayingControls controls_ {};
    double sampleRate_ { 48000.0 };
    int stringCount_ { 0 };
    bool tensionModulation_ { true };
};

} // namespace aculyre
