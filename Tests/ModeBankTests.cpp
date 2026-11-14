// The mode bank's kernels (Source/DSP/ModeBank.h) against an independent
// mode-by-mode sum in long double, straight from each mode's definition
// (force b0 sum(participation force), unscaled state, pull
// sum(participation state)): the portable kernel, and the AVX2 + FMA one
// where the CPU has it. So the bank's scaled state is held to the definition
// too. Each layout puts modes in all three parts with a different number of
// listed coordinates, so every width of the kernels' coordinate loops runs
// (pairs and the odd one out; five at a time and each remainder), and every
// listed coordinate carries a mode nothing else drives: a kernel that leaves
// one out fails here.
#include "CpuFeatures.h"
#include "ModeBank.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
using aculyre::detail::ModeBank;
using Kernel = void (*)(ModeBank&, const ModeBank::BarVector&, double&, double&, ModeBank::BarVector&);

int failures = 0;

void check(bool ok, const char* what)
{
    if (ok)
        return;
    ++failures;
    std::cerr << "FAILED: " << what << "\n";
}

struct Random
{
    std::uint64_t state { 0x9E3779B97F4A7C15ull };
    double next() noexcept // in [-1, 1)
    {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<double>(static_cast<std::int64_t>(state >> 11)) * 0x1p-52 - 1.0;
    }
};

struct Mode
{
    double b0 { 0.0 }, a1 { 0.0 }, a2 { 0.0 }, left { 0.0 }, right { 0.0 };
    ModeBank::BarVector participation {};
};

// A bank of `counts` modes per part (even-only, odd-only, mixed), the parts
// meeting `listed` of their coordinates, resonators from 60 Hz to 9 kHz.
// `modes` gets the same modes as they are defined, the padding silent.
std::unique_ptr<ModeBank> makeBank(const int counts[3], const int listed[3], Random& random, std::vector<Mode>& modes)
{
    auto bank = std::make_unique<ModeBank>();
    constexpr double pi = 3.14159265358979323846, fs = 48000.0;
    for (std::size_t part = 0; part < 3; ++part)
    {
        int available[ModeBank::barCoordinates], n = 0;
        for (int c = 0; c < ModeBank::barCoordinates; ++c)
        {
            const bool evenBar = listed[0] > 5 ? (c != 1 && (c < 3 || (c - 3) % 2 == 0)) : (c % 2 == 0);
            if (part == 2 || (part == 0 ? evenBar : !evenBar))
                available[n++] = c;
        }
        bank->coordinateCount[part] = listed[part];
        for (int i = 0; i < listed[part]; ++i)
            bank->coordinates[part][static_cast<std::size_t>(i)] = static_cast<std::size_t>(available[i]);
        for (int m = 0; m < counts[part]; ++m)
        {
            const auto k = static_cast<std::size_t>(bank->count++);
            const double frequency = 60.0 * std::pow(150.0, 0.5 + 0.5 * random.next());
            const double omega = 2.0 * pi * frequency, q = 20.0 + 15.0 * random.next();
            const double warp = omega / std::tan(omega / (2.0 * fs));
            const double d0 = warp * warp + omega / q * warp + omega * omega;
            Mode mode;
            mode.b0 = warp / d0;
            mode.a1 = 2.0 * (omega * omega - warp * warp) / d0;
            mode.a2 = (warp * warp - omega / q * warp + omega * omega) / d0;
            mode.left = random.next();
            mode.right = random.next();
            // The first modes of a part each meet one listed coordinate
            // alone; the rest meet them all.
            for (int i = 0; i < listed[part]; ++i)
                if (m >= listed[part] || m == i)
                    mode.participation[bank->coordinates[part][static_cast<std::size_t>(i)]] = 1.0e4 * random.next();
            bank->setMode(k, mode.b0, mode.a1, mode.a2, mode.participation, mode.left, mode.right);
            modes.resize(k + 1);
            modes[k] = mode;
        }
        bank->count = (bank->count + ModeBank::lanes - 1) / ModeBank::lanes * ModeBank::lanes;
        (part == 0 ? bank->evenEnd : part == 1 ? bank->oddEnd : bank->count) = bank->count;
    }
    modes.resize(static_cast<std::size_t>(bank->count));
    return bank;
}

struct Step
{
    long double left { 0.0L }, right { 0.0L };
    long double pull[ModeBank::barCoordinates] {};
};

// One sample of every mode on its own.
struct Reference
{
    explicit Reference(const std::vector<Mode>& source) : modes(source), s1(source.size()), s2(source.size()) {}

    Step run(const ModeBank::BarVector& force)
    {
        Step out;
        for (std::size_t k = 0; k < modes.size(); ++k)
        {
            const Mode& mode = modes[k];
            long double sum = 0.0L;
            for (std::size_t c = 0; c < ModeBank::barCoordinates; ++c)
                sum += static_cast<long double>(mode.participation[c]) * force[c];
            const long double x = mode.b0 * sum;
            const long double y = x + s1[k];
            s1[k] = s2[k] - mode.a1 * y;
            s2[k] = -x - mode.a2 * y;
            out.left += mode.left * y;
            out.right += mode.right * y;
            for (std::size_t c = 0; c < ModeBank::barCoordinates; ++c)
                out.pull[c] += static_cast<long double>(mode.participation[c]) * s1[k];
        }
        return out;
    }

    const std::vector<Mode>& modes;
    std::vector<long double> s1, s2;
};

void compare(const char* name, Kernel kernel, const int counts[3], const int listed[3])
{
    Random random;
    std::vector<Mode> modes;
    const auto bank = makeBank(counts, listed, random, modes);
    Reference reference(modes);
    long double errorSquares = 0.0L, signalSquares = 0.0L;
    bool everyCoordinatePulls = true;
    std::vector<long double> coordinateSquares(ModeBank::barCoordinates, 0.0L);
    for (int sample = 0; sample < 4000; ++sample)
    {
        ModeBank::BarVector force {};
        for (double& f : force)
            f = sample < 2000 ? random.next() : 0.0; // driven, then ringing
        const Step expected = reference.run(force);
        double left = 0.0, right = 0.0;
        ModeBank::BarVector pull;
        pull.fill(123.0); // the kernel must overwrite, not add to, what it is given
        kernel(*bank, force, left, right, pull);
        const auto add = [&] (long double got, long double want)
        {
            errorSquares += (got - want) * (got - want);
            signalSquares += want * want;
        };
        add(left, expected.left);
        add(right, expected.right);
        for (std::size_t c = 0; c < ModeBank::barCoordinates; ++c)
        {
            add(pull[c], expected.pull[c]);
            coordinateSquares[c] += expected.pull[c] * expected.pull[c];
        }
    }
    for (std::size_t part = 0; part < 3; ++part)
        for (int i = 0; i < listed[part]; ++i)
            everyCoordinatePulls = everyCoordinatePulls
                && coordinateSquares[bank->coordinates[part][static_cast<std::size_t>(i)]] > 0.0L;
    check(signalSquares > 0.0L && everyCoordinatePulls, "the layout drives every listed coordinate");
    const double relative = static_cast<double>(std::sqrt(errorSquares / signalSquares));
    std::cout << name << " modes " << counts[0] << "/" << counts[1] << "/" << counts[2] << " coordinates "
              << listed[0] << "/" << listed[1] << "/" << listed[2] << ": error " << relative << " of the signal\n";
    // Single-precision Float4 shape and s1Float accumulation around 64-bit
    // double resonators of Q 5 to 35 leaves ~1e-7..3e-6; a coordinate or a
    // mode left out, or a wrong lane, leaves 1e-2 or more.
    check(relative < 1.0e-5, name);
}
} // namespace

int main()
{
    // Modes per part, and how many of its coordinates each part lists.
    const int layouts[][2][3] {
        { { 16, 24, 0 }, { 6, 4, 0 } },
        { { 16, 24, 0 }, { 5, 5, 0 } },
        { { 19, 13, 0 }, { 4, 3, 0 } },
        { { 16, 24, 40 }, { 5, 5, 10 } },
        { { 9, 3, 21 }, { 4, 3, 9 } },
        { { 24, 8, 12 }, { 3, 2, 7 } },
        { { 8, 16, 13 }, { 2, 1, 6 } },
        { { 3, 0, 30 }, { 1, 0, 8 } },
        { { 0, 0, 290 }, { 0, 0, 10 } },
    };
    for (const auto& layout : layouts)
    {
        compare("portable", aculyre::detail::runModes, layout[0], layout[1]);
#if defined(ACULYRE_HAS_FUSED_KERNELS)
        if (instrumentCpu::hasAvx2Fma())
            compare("fused", aculyre::detail::runModesFused, layout[0], layout[1]);
#endif
    }
#if defined(ACULYRE_HAS_FUSED_KERNELS)
    if (!instrumentCpu::hasAvx2Fma())
        std::cout << "This CPU has no AVX2 + FMA: the fused kernel did not run.\n";
#endif
    if (failures != 0)
    {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "Mode bank kernels match the independent sum.\n";
    return 0;
}
