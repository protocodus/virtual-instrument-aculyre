#pragma once

#include <array>
#include <cstddef>

// The soundbox's modes as process() runs them, and the kernels that run
// them. Kept in a header so Tests/ModeBankTests.cpp can hold the kernels to
// an independent sum.

#if defined(__GNUC__) || defined(__clang__)
#define ACULYRE_FORCE_INLINE __attribute__((always_inline)) inline
#else
#define ACULYRE_FORCE_INLINE inline
#endif

namespace aculyre::detail
{
// Two doubles as one vector. The Rack's SDK compiler does not vectorize
// loops itself, but keeps vector types to the end: written with them, the
// strings' filters and the modes run two lanes per instruction there too
// (SSE2, NEON). Each lane is the scalar arithmetic, so the result is the same
// in every build. Natural scalar alignment: the engine's state lives on the
// heap, which the SDK aligns to 8 bytes.
#if defined(__GNUC__) || defined(__clang__)
typedef double Pair __attribute__((vector_size(16), aligned(8), may_alias));
#else
struct Pair
{
    double v[2];
    Pair operator+(Pair o) const noexcept { return { { v[0] + o.v[0], v[1] + o.v[1] } }; }
    Pair operator-(Pair o) const noexcept { return { { v[0] - o.v[0], v[1] - o.v[1] } }; }
    Pair operator*(Pair o) const noexcept { return { { v[0] * o.v[0], v[1] * o.v[1] } }; }
    Pair operator-() const noexcept { return { { -v[0], -v[1] } }; }
    Pair& operator+=(Pair o) noexcept { return *this = *this + o; }
    double operator[](int i) const noexcept { return v[i]; }
};
#endif
inline Pair* pairs(double* p) noexcept { return reinterpret_cast<Pair*>(p); }
inline const Pair* pairs(const double* p) noexcept { return reinterpret_cast<const Pair*>(p); }
inline Pair splat(double x) noexcept { return Pair { x, x }; }

// The modes in transposed direct form II, b0 (1 - z^-2) / (1 + a1 z^-1 +
// a2 z^-2) each, as a structure of arrays padded with silent modes to whole
// groups of `lanes`. The modes that meet only the even bar coordinates come
// first, up to evenEnd, then those that meet only the odd ones, up to
// oddEnd, then the rest, each part padded, so that each part runs with only
// its own coordinates: those of its parity that some mode meets (a bar whose
// higher bends are left out meets none of them), listed in `coordinates`.
//
// A mode's force is b0 sum(participation force) and its pull on the bar
// sum(participation state). Until 0.13.2 the bank kept both tables, b0
// participation and participation: 40 KB read every sample on the harps,
// more than a 32 KB first-level cache holds beside the strings. Kept with
// its state divided by sqrt(b0), a mode reads one table for both,
// `shape` = sqrt(b0) participation, and needs no b0 at all: 28 KB.
// setMode() does the scaling; s1 and s2 are the scaled state.
struct ModeBank
{
    static constexpr int lanes = 8;
    static constexpr int barCoordinates = 10;
    static constexpr int maxModes = 400;
    static constexpr int capacity = maxModes + 3 * lanes;
    using BarVector = std::array<double, barCoordinates>;

    std::array<double, capacity> a1 {}, a2 {}, left {}, right {}, s1 {}, s2 {};
    std::array<std::array<double, capacity>, barCoordinates> shape {};
    std::array<std::array<std::size_t, barCoordinates>, 3> coordinates {};
    std::array<int, 3> coordinateCount {};
    int evenEnd { 0 }, oddEnd { 0 }, count { 0 };

    // Mode k from its coefficients, its shape at the bar's coordinates and
    // what a unit of its velocity radiates to each microphone. b0 > 0.
    void setMode(std::size_t k, double b0, double a1Value, double a2Value, const BarVector& participation,
                 double radiatesLeft, double radiatesRight) noexcept
    {
        const double root = __builtin_sqrt(b0);
        for (std::size_t c = 0; c < barCoordinates; ++c)
            shape[c][k] = root * participation[c];
        a1[k] = a1Value;
        a2[k] = a2Value;
        left[k] = root * radiatesLeft;
        right[k] = root * radiatesRight;
    }

    void clearState() noexcept
    {
        s1.fill(0.0);
        s2.fill(0.0);
    }
};

// Runs the bank's modes one sample on under the force on the bar. Returns
// what they radiate to each microphone and their pull on the bar's next
// velocity. A group's generalized force and its recursion run together,
// while the force is still in registers; the pulls follow, two coordinates
// at a time so that each load of a mode's state serves both. Every sum keeps
// one accumulator per lane (a mode's place in its group of eight) and adds
// the lanes in order at the end, so the fused kernel below, four lanes a
// vector, adds the same terms in the same order.
inline void runModes(ModeBank& bank, const ModeBank::BarVector& force, double& left, double& right,
                     ModeBank::BarVector& pull) noexcept
{
    constexpr int vectors = ModeBank::lanes / 2;
    const int bounds[4] { 0, bank.evenEnd, bank.oddEnd, bank.count };
    Pair accLeft[vectors] { splat(0.0), splat(0.0), splat(0.0), splat(0.0) };
    Pair accRight[vectors] { splat(0.0), splat(0.0), splat(0.0), splat(0.0) };
    for (std::size_t part = 0; part < 3; ++part)
        for (int group = bounds[part]; group < bounds[part + 1]; group += ModeBank::lanes)
        {
            Pair sum[vectors] { splat(0.0), splat(0.0), splat(0.0), splat(0.0) };
            int n = 0;
            for (; n + 1 < bank.coordinateCount[part]; n += 2)
            {
                const std::size_t c = bank.coordinates[part][static_cast<std::size_t>(n)];
                const std::size_t d = bank.coordinates[part][static_cast<std::size_t>(n + 1)];
                const Pair* const shapeC = pairs(bank.shape[c].data() + group);
                const Pair* const shapeD = pairs(bank.shape[d].data() + group);
                const Pair fc = splat(force[c]);
                const Pair fd = splat(force[d]);
                for (int v = 0; v < vectors; ++v)
                    sum[v] = (sum[v] + shapeC[v] * fc) + shapeD[v] * fd;
            }
            for (; n < bank.coordinateCount[part]; ++n)
            {
                const std::size_t c = bank.coordinates[part][static_cast<std::size_t>(n)];
                const Pair* const shape = pairs(bank.shape[c].data() + group);
                const Pair f = splat(force[c]);
                for (int v = 0; v < vectors; ++v)
                    sum[v] += shape[v] * f;
            }
            Pair* const s1 = pairs(bank.s1.data() + group);
            Pair* const s2 = pairs(bank.s2.data() + group);
            const Pair* const a1 = pairs(bank.a1.data() + group);
            const Pair* const a2 = pairs(bank.a2.data() + group);
            const Pair* const toLeft = pairs(bank.left.data() + group);
            const Pair* const toRight = pairs(bank.right.data() + group);
            for (int v = 0; v < vectors; ++v)
            {
                const Pair y = sum[v] + s1[v];
                const Pair next = s2[v] - a1[v] * y;
                s2[v] = -sum[v] - a2[v] * y;
                s1[v] = next;
                accLeft[v] += toLeft[v] * y;
                accRight[v] += toRight[v] * y;
            }
        }
    left = right = 0.0;
    for (int v = 0; v < vectors; ++v)
        for (int i = 0; i < 2; ++i)
        {
            left += accLeft[v][i];
            right += accRight[v][i];
        }

    // A coordinate's lane sums run on from its own part into the mixed one.
    Pair lanePull[ModeBank::barCoordinates][vectors];
    for (auto& lane : lanePull)
        for (int v = 0; v < vectors; ++v)
            lane[v] = splat(0.0);
    const double* const state = bank.s1.data();
    for (std::size_t part = 0; part < 3; ++part)
    {
        int n = 0;
        for (; n + 1 < bank.coordinateCount[part]; n += 2)
        {
            const std::size_t c = bank.coordinates[part][static_cast<std::size_t>(n)];
            const std::size_t d = bank.coordinates[part][static_cast<std::size_t>(n + 1)];
            Pair sumC[vectors], sumD[vectors];
            for (int v = 0; v < vectors; ++v)
            {
                sumC[v] = lanePull[c][v];
                sumD[v] = lanePull[d][v];
            }
            for (int group = bounds[part]; group < bounds[part + 1]; group += ModeBank::lanes)
            {
                const Pair* const s = pairs(state + group);
                const Pair* const shapeC = pairs(bank.shape[c].data() + group);
                const Pair* const shapeD = pairs(bank.shape[d].data() + group);
                for (int v = 0; v < vectors; ++v)
                {
                    sumC[v] += shapeC[v] * s[v];
                    sumD[v] += shapeD[v] * s[v];
                }
            }
            for (int v = 0; v < vectors; ++v)
            {
                lanePull[c][v] = sumC[v];
                lanePull[d][v] = sumD[v];
            }
        }
        for (; n < bank.coordinateCount[part]; ++n)
        {
            const std::size_t c = bank.coordinates[part][static_cast<std::size_t>(n)];
            Pair sumC[vectors];
            for (int v = 0; v < vectors; ++v)
                sumC[v] = lanePull[c][v];
            for (int group = bounds[part]; group < bounds[part + 1]; group += ModeBank::lanes)
            {
                const Pair* const s = pairs(state + group);
                const Pair* const shape = pairs(bank.shape[c].data() + group);
                for (int v = 0; v < vectors; ++v)
                    sumC[v] += shape[v] * s[v];
            }
            for (int v = 0; v < vectors; ++v)
                lanePull[c][v] = sumC[v];
        }
    }
    for (std::size_t c = 0; c < ModeBank::barCoordinates; ++c)
    {
        double total = 0.0;
        for (int v = 0; v < vectors; ++v)
            for (int i = 0; i < 2; ++i)
                total += lanePull[c][v][i];
        pull[c] = total;
    }
}

#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__)) && !defined(ACULYRE_DISABLE_AVX2)
#define ACULYRE_HAS_FUSED_KERNELS 1

// The same sums for CPUs with AVX2 and FMA3: four lanes a vector, each
// product and its sum one fused instruction. Every sum keeps its order and
// its lanes but rounds once per term instead of twice, so the output meets
// the portable bank's to rounding, not to the bit (Tests/ModeBankTests.cpp
// holds it there; Docs/benchmarks/cpu-consolidation-2026-11-09.md has the
// null test). Chosen in prepare().
typedef double Double4 __attribute__((vector_size(32), aligned(8), may_alias));

// One part's pull on `width` of its coordinates, in one pass over the
// part's modes: ten lane sums and a group's state at a width of five, within
// AVX2's sixteen registers.
template <int width>
__attribute__((target("avx2,fma"))) inline void pullOnBarFused(const ModeBank& bank, const std::size_t* list,
                                                                int begin, int end,
                                                                Double4 (*lanePull)[2]) noexcept
{
    Double4 lo[width], hi[width];
    const double* row[width];
    for (int c = 0; c < width; ++c)
    {
        lo[c] = lanePull[list[c]][0];
        hi[c] = lanePull[list[c]][1];
        row[c] = bank.shape[list[c]].data();
    }
    const double* const state = bank.s1.data();
    for (int group = begin; group < end; group += ModeBank::lanes)
    {
        const Double4 s0 = *reinterpret_cast<const Double4*>(state + group);
        const Double4 s1 = *reinterpret_cast<const Double4*>(state + group + 4);
        for (int c = 0; c < width; ++c)
        {
            lo[c] = __builtin_ia32_vfmaddpd256(*reinterpret_cast<const Double4*>(row[c] + group), s0, lo[c]);
            hi[c] = __builtin_ia32_vfmaddpd256(*reinterpret_cast<const Double4*>(row[c] + group + 4), s1, hi[c]);
        }
    }
    for (int c = 0; c < width; ++c)
    {
        lanePull[list[c]][0] = lo[c];
        lanePull[list[c]][1] = hi[c];
    }
}

__attribute__((target("avx2,fma"))) inline void runModesFused(ModeBank& bank, const ModeBank::BarVector& force,
                                                               double& left, double& right,
                                                               ModeBank::BarVector& pull) noexcept
{
    constexpr int vectors = ModeBank::lanes / 4;
    const int bounds[4] { 0, bank.evenEnd, bank.oddEnd, bank.count };
    Double4 accLeft[vectors] {}, accRight[vectors] {};
    for (std::size_t part = 0; part < 3; ++part)
        for (int group = bounds[part]; group < bounds[part + 1]; group += ModeBank::lanes)
        {
            Double4 sum[vectors] {};
            for (int n = 0; n < bank.coordinateCount[part]; ++n)
            {
                const std::size_t c = bank.coordinates[part][static_cast<std::size_t>(n)];
                const Double4* const shape = reinterpret_cast<const Double4*>(bank.shape[c].data() + group);
                const double share = force[c];
                const Double4 f { share, share, share, share };
                for (int v = 0; v < vectors; ++v)
                    sum[v] = __builtin_ia32_vfmaddpd256(shape[v], f, sum[v]);
            }
            Double4* const s1 = reinterpret_cast<Double4*>(bank.s1.data() + group);
            Double4* const s2 = reinterpret_cast<Double4*>(bank.s2.data() + group);
            const Double4* const a1 = reinterpret_cast<const Double4*>(bank.a1.data() + group);
            const Double4* const a2 = reinterpret_cast<const Double4*>(bank.a2.data() + group);
            const Double4* const toLeft = reinterpret_cast<const Double4*>(bank.left.data() + group);
            const Double4* const toRight = reinterpret_cast<const Double4*>(bank.right.data() + group);
            for (int v = 0; v < vectors; ++v)
            {
                const Double4 y = sum[v] + s1[v];
                const Double4 next = __builtin_ia32_vfmaddpd256(-a1[v], y, s2[v]);
                s2[v] = __builtin_ia32_vfmaddpd256(-a2[v], y, -sum[v]);
                s1[v] = next;
                accLeft[v] = __builtin_ia32_vfmaddpd256(toLeft[v], y, accLeft[v]);
                accRight[v] = __builtin_ia32_vfmaddpd256(toRight[v], y, accRight[v]);
            }
        }
    left = right = 0.0;
    for (int v = 0; v < vectors; ++v)
        for (int i = 0; i < 4; ++i)
        {
            left += accLeft[v][i];
            right += accRight[v][i];
        }

    Double4 lanePull[ModeBank::barCoordinates][vectors] {};
    for (std::size_t part = 0; part < 3; ++part)
    {
        const int begin = bounds[part], end = bounds[part + 1];
        const std::size_t* list = bank.coordinates[part].data();
        int remaining = bank.coordinateCount[part];
        for (; remaining >= 5; remaining -= 5, list += 5)
            pullOnBarFused<5>(bank, list, begin, end, lanePull);
        switch (remaining)
        {
            case 4: pullOnBarFused<4>(bank, list, begin, end, lanePull); break;
            case 3: pullOnBarFused<3>(bank, list, begin, end, lanePull); break;
            case 2: pullOnBarFused<2>(bank, list, begin, end, lanePull); break;
            case 1: pullOnBarFused<1>(bank, list, begin, end, lanePull); break;
            default: break;
        }
    }
    for (std::size_t c = 0; c < ModeBank::barCoordinates; ++c)
    {
        double total = 0.0;
        for (int v = 0; v < vectors; ++v)
            for (int i = 0; i < 4; ++i)
                total += lanePull[c][v][i];
        pull[c] = total;
    }
}
#endif
} // namespace aculyre::detail
