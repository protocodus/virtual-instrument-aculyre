// Electrical-only pickup against an independent component/node simulation,
// Tools/PiezoCircuitReference.py. The input is force on the element in N;
// no mechanical seat is included in either side. Checks DC, loaded AC,
// physical volts/headroom, nonlinear harmonics, aliasing, overload recovery,
// exact silence, reset and the seven-sample observation latency.
#include "DSP/PiezoCircuit.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#ifndef ACULYRE_PIEZO_FIXTURES
#error "ACULYRE_PIEZO_FIXTURES must name Tests/Fixtures"
#endif

namespace aculyre
{
struct PiezoCircuitTestAccess
{
    using Design = PiezoCircuit::Design;
    // One element-force sample; DI volts after the seven-sample latency.
    static double volts(PiezoCircuit& engine, double newtons)
    {
        engine.process(newtons);
        return engine.outputVolts();
    }
    static double chain(PiezoCircuit& engine, double force) { return engine.process(force); }
    // C3's, C4's and C5's voltages (C4 and C5 seven samples back, with the
    // output) and U1B's clipped output seven samples back.
    static std::array<double, 4> slowStates(const PiezoCircuit& engine)
    {
        return { engine.piezoC3_, engine.piezoC4_, engine.piezoC5_, engine.piezoLastStage_ };
    }
    static std::vector<double> state(const PiezoCircuit& engine)
    {
        std::vector<double> all { engine.piezoFrontW_, engine.piezoFrontC2_,
            engine.piezoLastOpen_, engine.piezoLastClamp_, engine.piezoC3_,
            engine.piezoC4_, engine.piezoC5_, engine.piezoLastBuffer_,
            engine.piezoLastStage_, engine.piezoOutputVolts_ };
        all.insert(all.end(), engine.piezoDrive_.begin(), engine.piezoDrive_.end());
        all.insert(all.end(), engine.piezoStage_.begin(), engine.piezoStage_.end());
        return all;
    }
};
} // namespace aculyre

namespace
{
using Access = aculyre::PiezoCircuitTestAccess;
using Design = Access::Design;
constexpr double pi = 3.14159265358979323846;
constexpr int latency = 7;
int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

double decibels(double magnitude) { return 20.0 * std::log10(magnitude); }

// ------------------------------------------------------------ fixtures
struct Table
{
    std::vector<std::string> comments;
    std::vector<std::string> columns;
    std::vector<std::vector<double>> rows;
    std::size_t column(const std::string& name) const
    {
        const auto found = std::find(columns.begin(), columns.end(), name);
        if (found == columns.end())
        {
            std::cerr << "fixture column missing: " << name << '\n';
            std::exit(EXIT_FAILURE);
        }
        return static_cast<std::size_t>(found - columns.begin());
    }
};

Table readTable(const std::string& name, bool firstColumnNames = false)
{
    std::ifstream input(std::string(ACULYRE_PIEZO_FIXTURES) + "/" + name);
    if (!input)
    {
        std::cerr << "cannot read fixture " << name << '\n';
        std::exit(EXIT_FAILURE);
    }
    Table table;
    std::string line;
    while (std::getline(input, line))
    {
        if (line.empty())
            continue;
        if (line[0] == '#')
        {
            table.comments.push_back(line);
            continue;
        }
        std::stringstream stream(line);
        std::string cell;
        std::vector<std::string> cells;
        while (std::getline(stream, cell, ','))
            cells.push_back(cell);
        if (table.columns.empty())
        {
            table.columns = cells;
            continue;
        }
        std::vector<double> row;
        for (std::size_t i = firstColumnNames ? 1 : 0; i < cells.size(); ++i)
            row.push_back(std::stod(cells[i]));
        if (firstColumnNames)
            table.comments.push_back("@" + cells[0]);
        table.rows.push_back(row);
    }
    return table;
}

std::map<std::string, double> readDc()
{
    const auto table = readTable("piezo-reference-dc.csv", true);
    std::map<std::string, double> values;
    std::size_t row = 0;
    for (const auto& comment : table.comments)
        if (!comment.empty() && comment[0] == '@')
            values[comment.substr(1)] = table.rows[row++][0];
    return values;
}

struct Reference
{
    std::vector<double> hz;
    std::vector<std::complex<double>> vout;
};

Reference readAc()
{
    const auto table = readTable("piezo-reference-ac.csv");
    Reference reference;
    for (const auto& row : table.rows)
    {
        reference.hz.push_back(row[table.column("hz")]);
        reference.vout.emplace_back(row[table.column("vout_re")], row[table.column("vout_im")]);
    }
    return reference;
}

std::complex<double> interpolate(const Reference& reference, double hz)
{
    // Log-frequency interpolation of magnitude and phase between grid points.
    const auto upper = std::upper_bound(reference.hz.begin(), reference.hz.end(), hz);
    const auto i = static_cast<std::size_t>(std::clamp<long>(upper - reference.hz.begin(), 1,
                                                             static_cast<long>(reference.hz.size()) - 1));
    const double t = std::log(hz / reference.hz[i - 1]) / std::log(reference.hz[i] / reference.hz[i - 1]);
    const double magnitude = std::exp((1.0 - t) * std::log(std::abs(reference.vout[i - 1]))
                                      + t * std::log(std::abs(reference.vout[i])));
    double phase0 = std::arg(reference.vout[i - 1]), phase1 = std::arg(reference.vout[i]);
    if (phase1 - phase0 > pi)
        phase1 -= 2.0 * pi;
    if (phase1 - phase0 < -pi)
        phase1 += 2.0 * pi;
    return std::polar(magnitude, (1.0 - t) * phase0 + t * phase1);
}

std::unique_ptr<aculyre::PiezoCircuit> engineAt(int rate)
{
    auto engine = std::make_unique<aculyre::PiezoCircuit>();
    engine->prepare(rate);
    return engine;
}

// The complex response at frequency of a sampled response, latency removed.
std::complex<double> responseAt(const std::vector<double>& response, double frequency,
                                double rate)
{
    const auto step = std::polar(1.0, -2.0 * pi * frequency / rate);
    std::complex<double> phase { 1.0, 0.0 }, sum {};
    for (std::size_t i = 0; i < response.size(); ++i)
    {
        if (response[i] != 0.0)
            sum += response[i] * phase;
        phase *= step;
        if ((i & 4095) == 4095)
            phase /= std::abs(phase);
    }
    return sum * std::polar(1.0, 2.0 * pi * frequency * latency / rate);
}

// Harmonic h's complex amplitude (peak) of a block holding a whole number of
// periods: bin h * bin0 of a rectangular DFT.
std::complex<double> harmonic(const std::vector<double>& block, int bin)
{
    const double size = static_cast<double>(block.size());
    std::complex<double> sum {};
    for (std::size_t i = 0; i < block.size(); ++i)
        sum += block[i] * std::polar(1.0, -2.0 * pi * bin * static_cast<double>(i) / size);
    return 2.0 * sum / size;
}

void fft(std::vector<std::complex<double>>& data, int sign)
{
    const std::size_t size = data.size();
    for (std::size_t i = 1, j = 0; i < size; ++i)
    {
        std::size_t bit = size >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(data[i], data[j]);
    }
    for (std::size_t length = 2; length <= size; length <<= 1)
    {
        const auto root = std::polar(1.0, sign * 2.0 * pi / static_cast<double>(length));
        for (std::size_t start = 0; start < size; start += length)
        {
            std::complex<double> twiddle { 1.0, 0.0 };
            for (std::size_t k = 0; k < length / 2; ++k)
            {
                const auto even = data[start + k];
                const auto odd = data[start + k + length / 2] * twiddle;
                data[start + k] = even + odd;
                data[start + k + length / 2] = even - odd;
                twiddle *= root;
            }
        }
    }
}

// ------------------------------------------------------------ 1. DC
void testUnitsAndLatency()
{
    // Independent capacitive division and amplifier gain, not a fitted
    // engine calibration. A positive element force gives positive voltage.
    constexpr double cp = 1.45e-9, cable = 390e-12, stray = 1e-12;
    constexpr double c1 = 4.7e-9, cin = 6e-12 + stray;
    constexpr double shunt = cp + cable + stray;
    constexpr double share = cp * c1 / (c1 * shunt + cin * (c1 + shunt));
    constexpr double load = 1.0 / (1.0 / 10000.0 + 1.0 / 1e6);
    constexpr double gain = 0.2 * share * 1.62 * load / (100.0 + load);
    for (int rate : { 44100, 48000, 96000, 192000 })
    {
        auto engine = engineAt(rate);
        expect(std::abs(engine->midbandVoltsPerNewton() / gain - 1.0) < 1e-12,
               "normalization does not follow component V/N");
        for (int sample = 0; sample <= latency; ++sample)
        {
            const double output = engine->process(sample == 0 ? 1.0 : 0.0);
            if (sample == 0)
            {
                const auto probe = engine->probe();
                expect(probe.openCircuit == 0.2, "one newton is not 0.2 V open circuit");
                expect(probe.jack > 0.15 && probe.jack < 0.16,
                       "cable and input capacitance loading missing from jack");
                expect(probe.bufferInput > 0.15 && probe.bufferInput < 0.16,
                       "loaded buffer input has wrong physical volts");
            }
            if (sample < latency)
                expect(output == 0.0, "electrical output precedes seven-sample latency");
            else
                expect(output > 0.99 && output < 1.0,
                       "force-equivalent normalization/latency wrong");
            expect(std::abs(output * gain - engine->outputVolts()) < 1e-15,
                   "normalization changed the physical internal voltage");
        }
        engine->reset();
        auto fresh = engineAt(rate);
        for (int i = 0; i < 1000; ++i)
        {
            const double force = 24.0 * std::sin(0.157 * i);
            expect(engine->process(force) == fresh->process(force), "reset differs from fresh state");
        }
    }
    // Reciprocity is deliberately not claimed for the seat; the omitted
    // electrical stiffness correction alone is bounded from d=Cp*S.
    constexpr double d = cp * 0.2;
    const double k = 0.0038 * std::pow(2.0 * pi * 6000.0, 2);
    expect(d * d * k / cp < 0.00032, "electrical backreaction bound invalid");
    std::cout << "Piezo units: " << gain << " V/N; latency " << latency
              << " samples; electrical coupling bound " << d * d * k / cp << '\n';
}

void testOperatingPoint()
{
    const auto dc = readDc();
    expect(std::abs(dc.at("IN") - 4.5) < 1.0e-5 && std::abs(dc.at("B") - 4.5) < 1.0e-5,
           "the reference's input does not sit at half the battery");
    expect(std::abs(Design::railHigh - dc.at("u1b_rail_high")) < 5.0e-3
               && std::abs(Design::railLow - dc.at("u1b_rail_low")) < 5.0e-3,
           "U1B's rails about its operating point differ from the reference's");
    expect(std::abs(Design::commonModeLimit - dc.at("u1a_cm_high")) < 5.0e-3
               && std::abs(-Design::commonModeLimit - dc.at("u1a_cm_low")) < 5.0e-3,
           "U1A's common-mode limits differ from the reference's");
    std::cout << "Piezo DC: IN " << dc.at("IN") << " V, B " << dc.at("B") << " V; U1B rails "
              << Design::railHigh << " / " << Design::railLow << " V against "
              << dc.at("u1b_rail_high") << " / " << dc.at("u1b_rail_low") << '\n';
    // A never-driven chain is exact zero: the engine works about that point.
    auto engine = engineAt(48000);
    bool zero = true;
    for (int i = 0; i < 48000; ++i)
        zero = zero && Access::volts(*engine, 0.0) == 0.0;
    expect(zero, "an undriven chain is not exact zero");
}

// ------------------------------------------------------------ 2. small signal
// The impulse response to a 1 N element-force sample (U1B stays below 0.26 V), with
// the seven-sample latency removed, against the reference's AC analysis.
std::vector<double> impulseResponse(int rate)
{
    auto engine = engineAt(rate);
    constexpr double newtons = 1.0;
    std::vector<double> response;
    long lastNonzero = 0;
    for (long i = 0; i < 12L * rate; ++i)
    {
        const double v = Access::volts(*engine, i == 0 ? newtons : 0.0) / newtons;
        response.push_back(v);
        if (v != 0.0)
            lastNonzero = i;
    }
    response.resize(static_cast<std::size_t>(lastNonzero + 1));
    return response;
}

void testSmallSignal()
{
    {
        const auto reference = readAc();
        for (int rate : { 44100, 48000, 96000, 192000 })
        {
            const auto response = impulseResponse(rate);
            double worstLow = 0.0, worstMagnitude = 0.0, worstPhase = 0.0, worstTop = 0.0;
            double worstAt = 0.0;
            for (std::size_t i = 0; i < reference.hz.size(); ++i)
            {
                const double f = reference.hz[i];
                if (f < 5.0 || f > 0.45 * rate)
                    continue;
                const auto actual = responseAt(response, f, rate);
                const auto ratio = actual / reference.vout[i];
                const double magnitude = std::abs(decibels(std::abs(ratio)));
                const double phase = std::abs(std::arg(ratio)) * 180.0 / pi;
                if (f < 20.0)
                    worstLow = std::max(worstLow, magnitude);
                else if (f <= 12000.0 || (rate >= 88200 && f <= 20000.0))
                {
                    if (magnitude > worstMagnitude)
                        worstAt = f;
                    worstMagnitude = std::max(worstMagnitude, magnitude);
                    worstPhase = std::max(worstPhase, phase);
                }
                else if (rate < 88200)
                    worstTop = std::max(worstTop, magnitude);
            }
            const std::string where = "at " + std::to_string(rate);
            expect(worstLow <= 0.1, "piezo 5-20 Hz departs from the circuit by "
                                        + std::to_string(worstLow) + " dB, " + where);
            // Electrical reduction errors are dominated by the omitted
            // finite op-amp bandwidth/stray poles and bilinear warping;
            // there is no mechanical fit to excuse extra magnitude error.
            expect(worstMagnitude <= 0.02 && worstPhase <= 1.0,
                   "piezo in-band response departs from the circuit, " + where);
            expect(worstTop <= 0.02, "piezo top octave departs from the circuit, " + where);
            std::cout << "Piezo small signal, " << where << ": 5-20 Hz " << worstLow
                      << " dB; 20 Hz-" << (rate >= 88200 ? "20" : "12") << " kHz "
                      << worstMagnitude << " dB (at " << worstAt << " Hz), " << worstPhase
                      << " deg";
            if (rate < 88200)
                std::cout << "; 12 kHz-0.45 fs " << worstTop << " dB";
            std::cout << '\n';
        }
    }
}

// ------------------------------------------------------------ 4. harmonics
void testHarmonics()
{
    constexpr int rate = 48000, block = 16384;
    const auto reference = readAc();
    const auto table = readTable("piezo-reference-thd.csv");
    const auto response = impulseResponse(rate);
    double worst = 0.0;
    for (const auto& row : table.rows)
    {
        const double hz = row[table.column("hz")];
        const int bin = static_cast<int>(std::lround(hz * block / rate));
        // Drive the chain so its small-signal fundamental is the circuit's:
        // the level the clip sees is what these harmonics depend on.
        const double match = std::abs(interpolate(reference, hz)) / std::abs(responseAt(response, hz, rate));
        const double amplitude = row[table.column("amplitude")] * match;
        auto engine = engineAt(rate);
        std::vector<double> out;
        const long settle = 4L * rate;
        for (long i = 0; i < settle + block + latency; ++i)
        {
            const double v = Access::volts(*engine, amplitude * std::sin(2.0 * pi * hz * i / rate));
            if (i >= settle + latency)
                out.push_back(v);
        }
        const double h1 = std::abs(harmonic(out, bin));
        double total = 0.0;
        for (int h = 2; h * hz < 0.45 * rate; ++h)
            total += std::norm(harmonic(out, h * bin));
        const std::array<std::string, 4> names { "hd2", "hd3", "hd4", "hd5" };
        std::cout << "Piezo harmonics " << hz << " Hz +" << row[table.column("level_db")] << " dB:";
        for (int h = 2; h <= 5; ++h)
        {
            const double ref = row[table.column(names[static_cast<std::size_t>(h - 2)])];
            // The BLAMP's kernel rolls off above 0.375 fs (18 kHz): HD4 of
            // 5 kHz, at 20 kHz, comes out 1-7 dB low, and is reported only.
            if (h * hz >= 0.45 * rate)
                continue;
            if (h * hz >= 0.375 * rate)
            {
                std::cout << " (HD" << h << ' ' << decibels(std::abs(harmonic(out, h * bin)) / h1)
                          << " against " << ref << ", above 0.375 fs)";
                continue;
            }
            const double actual = decibels(std::abs(harmonic(out, h * bin)) / h1);
            std::cout << " HD" << h << " " << actual << " (" << ref << ")";
            if (ref > -90.0)
            {
                worst = std::max(worst, std::abs(actual - ref));
                expect(std::abs(actual - ref) <= 0.5, "HD" + std::to_string(h) + " at "
                           + std::to_string(hz) + " Hz differs from the circuit's");
            }
        }
        const double thd = decibels(std::sqrt(total) / h1);
        const double ref = row[table.column("thd")];
        std::cout << " THD " << thd << " (" << ref << ")\n";
        expect(std::abs(thd - ref) <= 0.5, "THD at " + std::to_string(hz) + " Hz differs from the circuit's");
        worst = std::max(worst, std::abs(thd - ref));
    }
    // Below the clip the chain is linear: -40 to -1 dB re the clip level.
    double worstClean = -300.0;
    for (const auto& row : table.rows)
    {
        if (row[table.column("level_db")] != table.rows.front()[table.column("level_db")])
            continue;
        const double hz = row[table.column("hz")];
        const int bin = static_cast<int>(std::lround(hz * block / rate));
        const double clip = row[table.column("amplitude")]
            / std::pow(10.0, row[table.column("level_db")] / 20.0);
        for (double level : { -40.0, -20.0, -6.0, -1.0 })
        {
            auto engine = engineAt(rate);
            std::vector<double> out;
            const long settle = 2L * rate;
            const double amplitude = clip * std::pow(10.0, level / 20.0);
            for (long i = 0; i < settle + block + latency; ++i)
            {
                const double v = Access::volts(*engine, amplitude * std::sin(2.0 * pi * hz * i / rate));
                if (i >= settle + latency)
                    out.push_back(v);
            }
            double total = 0.0;
            for (int h = 2; h * hz < 0.45 * rate; ++h)
                total += std::norm(harmonic(out, h * bin));
            const double thd = decibels(std::sqrt(total) / std::abs(harmonic(out, bin)));
            worstClean = std::max(worstClean, thd);
            expect(thd <= -100.0, "the chain distorts below its clip: " + std::to_string(thd)
                                      + " dB at " + std::to_string(hz) + " Hz, "
                                      + std::to_string(level) + " dB");
        }
    }
    std::cout << "Piezo harmonics: worst departure " << worst
              << " dB above the clip; worst THD below it " << worstClean << " dB\n";
}

// ------------------------------------------------------------ 5. aliasing
// A sine 3 and 6 dB over U1B's clip, a whole number of periods in the
// block: everything off the harmonics' bins (above 20 Hz, where the slow
// states' settling sits) is what the clip folded back. From 1.9 dB over
// its positive swing (0.35 dB over the negative) U1A's input range stops it
// too, and the diodes conduct past that: with U1B's gain of 1.62
// (PiezoDesign, R7 6.2 kOhm) both engage at 3 dB over; with the figure's
// gain of two they engaged from 3.7 and about 5 dB.
void testAliasing()
{
    const auto reference = readAc();
    constexpr int block = 16384;
    for (int rate : { 44100, 48000 })
        for (int nominal : { 1000, 3000 })
            for (double over : { 3.0, 6.0 })
            {
                int bin = static_cast<int>(std::lround(static_cast<double>(nominal) * block / rate));
                bin |= 1;
                const double hz = static_cast<double>(bin) * rate / block;
                // U1B's clip level at hz from the circuit's own gain: O2 is
                // Vout over the output divider in band.
                constexpr double load = Design::volume * Design::diInput / (Design::volume + Design::diInput);
                const double o2PerNewton = std::abs(interpolate(reference, hz)) * (Design::r9 + load) / load;
                const double amplitude = std::pow(10.0, over / 20.0) * Design::railHigh / o2PerNewton;
                auto engine = engineAt(rate);
                std::vector<std::complex<double>> out;
                const long settle = 3L * rate;
                for (long i = 0; i < settle + block + latency; ++i)
                {
                    const double v = Access::volts(*engine, amplitude * std::sin(2.0 * pi * hz * i / rate));
                    if (i >= settle + latency)
                        out.emplace_back(v, 0.0);
                }
                fft(out, -1);
                double fundamental = 0.0, folded = 0.0;
                for (int k = 1; k < block / 2; ++k)
                {
                    const double power = std::norm(out[static_cast<std::size_t>(k)]);
                    if (k == bin)
                        fundamental = power;
                    else if (k % bin != 0 && static_cast<double>(k) * rate / block > 20.0)
                        folded += power;
                }
                const double db = 10.0 * std::log10(folded / fundamental);
                // A bare clip folds back -49 (1 kHz) and -33 dB (3 kHz) at
                // 6 dB over. There the 12-tap BLAMP alone reaches -49 dB at
                // 3 kHz (Tools/PiezoCircuitReference.py, blamp_harmonic_errors),
                // and U1A's range and the diodes, which U1B's clip hides but
                // C3 and C1 integrate, cost up to 3.8 dB more (-45.2 dB at
                // 48 kHz; 2.5 dB at U1B's former gain of two).
                const double gate = nominal == 1000 ? (over < 4.0 ? -65.0 : -60.0)
                                                    : (over < 4.0 ? -50.0 : -45.0);
                expect(db <= gate, "piezo clip aliasing at " + std::to_string(hz) + " Hz, "
                                       + std::to_string(rate) + ", " + std::to_string(over)
                                       + " dB over: " + std::to_string(db) + " dB");
                std::cout << "Piezo aliasing, " << hz << " Hz " << over << " dB over the clip at "
                          << rate << " Hz: " << db << " dB\n";
            }
}

// ------------------------------------------------------------ 6. overload
void testOverload()
{
    constexpr int rate = 48000;
    const auto burst = readTable("piezo-reference-burst.csv");
    const auto recovery = readTable("piezo-reference-recovery.csv");
    std::vector<double> amplitudes;
    for (const auto& comment : burst.comments)
    {
        const auto at = comment.find("amplitudes in N: ");
        if (at == std::string::npos)
            continue;
        std::stringstream stream(comment.substr(at + 17));
        std::string cell;
        while (std::getline(stream, cell, ','))
            amplitudes.push_back(std::stod(cell));
    }
    expect(amplitudes.size() == 3, "burst fixture lacks its amplitudes");
    constexpr double frequency = 500.0, length = 0.040, ramp = 0.005;
    const std::array<std::string, 3> levels { "3", "10", "20" };
    for (std::size_t level = 0; level < amplitudes.size(); ++level)
    {
        auto engine = engineAt(rate);
        const auto force = [&] (double t)
        {
            if (t <= 0.0 || t >= length)
                return 0.0;
            double envelope = 1.0;
            if (t < ramp)
                envelope = 0.5 * (1.0 - std::cos(pi * t / ramp));
            else if (t > length - ramp)
                envelope = 0.5 * (1.0 - std::cos(pi * (length - t) / ramp));
            return amplitudes[level] * envelope * std::sin(2.0 * pi * frequency * t);
        };
        const long samples = static_cast<long>(3.0 * rate) + latency + 1;
        std::vector<double> vout(static_cast<std::size_t>(samples));
        std::vector<std::array<double, 5>> states(static_cast<std::size_t>(samples));
        for (long i = 0; i < samples; ++i)
        {
            vout[static_cast<std::size_t>(i)] = Access::volts(*engine, force(static_cast<double>(i) / rate));
            const auto slow = Access::slowStates(*engine);
            states[static_cast<std::size_t>(i)] = { slow[0], slow[1], slow[2],
                static_cast<double>(engine->probe().bufferInput), 0.0 };
        }
        // Vout through the burst.
        double error = 0.0, energy = 0.0;
        const auto column = burst.column("db" + levels[level]);
        for (std::size_t i = 0; i < burst.rows.size(); ++i)
        {
            const double ref = burst.rows[i][column];
            const double actual = vout[i + latency];
            error += (actual - ref) * (actual - ref);
            energy += ref * ref;
        }
        const double waveDb = 10.0 * std::log10(error / energy);
        expect(waveDb <= -30.0, "piezo overload waveform at +" + levels[level]
                                    + " dB departs from the circuit: " + std::to_string(waveDb) + " dB");
        // The recovery: C3, C4, C5 and U1A's input over the 2.96 s after the
        // burst, as trajectories and as effective time constants
        // (integral over the first value, where the state keeps its sign).
        std::cout << "Piezo overload +" << levels[level] << " dB: waveform " << waveDb << " dB;";
        const std::array<std::string, 4> names { "vc3", "vc4", "vc5", "vin" };
        const std::array<int, 4> delays { 0, latency, latency, 0 };
        for (std::size_t s = 0; s < names.size(); ++s)
        {
            const auto refColumn = recovery.column(names[s] + "_db" + levels[level]);
            double diff = 0.0, norm = 0.0, refIntegral = 0.0, integral = 0.0;
            bool oneSign = true;
            const double first = recovery.rows.front()[refColumn];
            double firstActual = 0.0;
            for (std::size_t i = 0; i < recovery.rows.size(); ++i)
            {
                const double t = recovery.rows[i][0];
                const auto at = static_cast<std::size_t>(std::lround(t * rate)) + static_cast<std::size_t>(delays[s]);
                const double ref = recovery.rows[i][refColumn];
                const double actual = states[at][s];
                if (i == 0)
                    firstActual = actual;
                diff += (actual - ref) * (actual - ref);
                norm += ref * ref;
                oneSign = oneSign && ref * first > 0.0;
                refIntegral += ref;
                integral += actual;
            }
            const double trajectory = std::sqrt(diff / norm);
            expect(trajectory <= 0.10, "piezo recovery of " + names[s] + " after +" + levels[level]
                                           + " dB departs from the circuit by "
                                           + std::to_string(100.0 * trajectory) + "%");
            std::cout << ' ' << names[s] << ' ' << 100.0 * trajectory << "%";
            if (oneSign && std::abs(first) > 1.0e-4)
            {
                const double tauRef = refIntegral * 0.005 / first;
                const double tau = integral * 0.005 / firstActual;
                expect(std::abs(tau / tauRef - 1.0) <= 0.02, "piezo recovery time of " + names[s]
                                                                  + " differs by more than 2%");
                std::cout << " (tau " << tau << " s vs " << tauRef << ")";
            }
        }
        std::cout << '\n';
    }
}

// ------------------------------------------------------------ 7. silence
void testSilenceAndGuards()
{
    for (int rate : { 44100, 48000, 96000, 192000 })
    {
        // A 10 N burst below U1B's clip and a 57 N overload, cut at
        // their peak; the overload shifts every coupling capacitor's charge.
        for (const double amplitude : { 10.0, 57.0 })
        {
            auto engine = engineAt(rate);
            const long cut = static_cast<long>(0.0205 * rate);
            for (long i = 0; i <= cut; ++i)
                Access::volts(*engine, amplitude * std::sin(2.0 * pi * 500.0 * i / rate));
            long lastNonzero = -1;
            for (long i = 0; i < 12L * rate; ++i)
                if (Access::chain(*engine, 0.0f) != 0.0f)
                    lastNonzero = i;
            const double seconds = static_cast<double>(lastNonzero + 1) / rate;
            const double gate = amplitude < 20.0 ? 5.0 : 7.0;
            expect(seconds <= gate, "piezo chain took " + std::to_string(seconds)
                                        + " s to reach exact silence at " + std::to_string(rate));
            for (double value : Access::state(*engine))
                expect(value == 0.0, "piezo chain kept state after reaching silence");
            std::cout << "Piezo chain silent " << seconds << " s after a cut at "
                      << amplitude << " N, " << rate << " Hz\n";
        }
    }
    // A non-finite force resets the chain rather than poisoning Main.
    auto engine = engineAt(48000);
    for (int i = 0; i < 100; ++i)
        Access::chain(*engine, 0.01f * static_cast<float>(std::sin(0.1 * i)));
    const float nan = Access::chain(*engine, std::numeric_limits<float>::quiet_NaN());
    expect(nan == 0.0f, "a non-finite force reached the piezo output");
    bool clean = true;
    for (int i = 0; i < 10; ++i)
        clean = clean && Access::chain(*engine, 0.0f) == 0.0f;
    expect(clean, "a non-finite force left state in the piezo chain");
    const float inf = Access::chain(*engine, std::numeric_limits<float>::infinity());
    expect(inf == 0.0f && Access::chain(*engine, 0.0f) == 0.0f,
           "an infinite force reached the piezo output");
}

} // namespace

int main(int argc, char** argv)
{
    // With arguments, only the named parts run (dc units small harmonics
    // aliasing overload silence).
    const auto wanted = [&] (const std::string& name)
    {
        if (argc < 2)
            return true;
        for (int i = 1; i < argc; ++i)
            if (name == argv[i])
                return true;
        return false;
    };
    if (wanted("dc")) testOperatingPoint();
    if (wanted("units")) testUnitsAndLatency();
    if (wanted("small")) testSmallSignal();
    if (wanted("harmonics")) testHarmonics();
    if (wanted("aliasing")) testAliasing();
    if (wanted("overload")) testOverload();
    if (wanted("silence")) testSilenceAndGuards();
    if (failures == 0)
        std::cout << "All Aculyre piezo circuit tests passed\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
