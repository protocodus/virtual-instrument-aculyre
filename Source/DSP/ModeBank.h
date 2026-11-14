#pragma once

#include "DspMath.h"
#include "Pair.h"

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

// Four floats as one 128-bit vector with natural 4-byte scalar alignment so
// heap-allocated state stays within the 8-byte alignment ceiling.
#if defined(__GNUC__) || defined(__clang__)
typedef float Float4 __attribute__((vector_size(16), aligned(4), may_alias));
typedef float Float2 __attribute__((vector_size(8), aligned(4), may_alias));
inline Pair widenLo(Float4 v) noexcept
{
    return __builtin_convertvector(__builtin_shufflevector(v, v, 0, 1), Pair);
}
inline Pair widenHi(Float4 v) noexcept
{
    return __builtin_convertvector(__builtin_shufflevector(v, v, 2, 3), Pair);
}
inline Float4 packFloat4(Pair lo, Pair hi) noexcept
{
    return __builtin_shufflevector(__builtin_convertvector(lo, Float2),
                                   __builtin_convertvector(hi, Float2), 0, 1, 2, 3);
}
#else
struct Float4
{
    float v[4];
    Float4 operator+(Float4 o) const noexcept { return { { v[0] + o.v[0], v[1] + o.v[1], v[2] + o.v[2], v[3] + o.v[3] } }; }
    Float4 operator*(Float4 o) const noexcept { return { { v[0] * o.v[0], v[1] * o.v[1], v[2] * o.v[2], v[3] * o.v[3] } }; }
    Float4& operator+=(Float4 o) noexcept { return *this = *this + o; }
    float operator[](int i) const noexcept { return v[i]; }
};
inline Pair widenLo(Float4 v) noexcept { return Pair { static_cast<double>(v[0]), static_cast<double>(v[1]) }; }
inline Pair widenHi(Float4 v) noexcept { return Pair { static_cast<double>(v[2]), static_cast<double>(v[3]) }; }
inline Float4 packFloat4(Pair lo, Pair hi) noexcept
{
    return Float4 { static_cast<float>(lo[0]), static_cast<float>(lo[1]),
                    static_cast<float>(hi[0]), static_cast<float>(hi[1]) };
}
#endif
inline Float4* float4s(float* p) noexcept { return reinterpret_cast<Float4*>(p); }
inline const Float4* float4s(const float* p) noexcept { return reinterpret_cast<const Float4*>(p); }
inline Float4 splatFloat4(float x) noexcept { return Float4 { x, x, x, x }; }

// The modes in transposed direct form II, b0 (1 - z^-2) / (1 + a1 z^-1 +
// a2 z^-2) each, as a structure of arrays padded with silent modes to whole
// groups of `lanes`. The modes that meet only the even bar coordinates come
// first, up to evenEnd, then those that meet only the odd ones, up to
// oddEnd, then the rest, each part padded, so that each part runs with only
// its own coordinates: those of its parity that some mode meets (a bar whose
// higher bends are left out meets none of them), listed in `coordinates`.
//
// `shape` = sqrt(b0) participation is stored in single precision (17 KB across
// 424 capacity x 10 coordinates, fitting inside 32 KB L1D alongside the
// strings) and accumulated 4 lanes per vector (`Float4`) for both the
// generalized bar-force dot product and the bar-pull dot product, while each
// mode's recursive biquad state `(a1, a2, s1, s2)` and Nyquist-zero radiation
// sum `(left, right)` stay in 64-bit `double`.
struct ModeBank
{
    static constexpr int lanes = 8;
    static constexpr int barCoordinates = 10;
    static constexpr int maxModes = 400;
    static constexpr int capacity = maxModes + 3 * lanes;
    using BarVector = std::array<double, barCoordinates>;

    struct Group
    {
        std::array<double, lanes> a1 {}, a2 {}, s1 {}, s2 {}, left {}, right {};
        std::array<float, lanes> s1Float {};
    };

    std::array<Group, capacity / lanes> groups {};
    std::array<std::array<float, capacity>, barCoordinates> shape {};
    std::array<std::array<std::size_t, barCoordinates>, 3> coordinates {};
    std::array<int, 3> coordinateCount {};
    int evenEnd { 0 }, oddEnd { 0 }, count { 0 };

    void setMode(std::size_t k, double b0, double a1Value, double a2Value, const BarVector& participation,
                 double radiatesLeft, double radiatesRight) noexcept
    {
        const double root = squareRoot(b0);
        for (std::size_t c = 0; c < barCoordinates; ++c)
            shape[c][k] = static_cast<float>(root * participation[c]);
        Group& g = groups[k / lanes];
        const std::size_t lane = k % lanes;
        g.a1[lane] = a1Value;
        g.a2[lane] = a2Value;
        g.left[lane] = root * radiatesLeft;
        g.right[lane] = root * radiatesRight;
    }

    void clearState() noexcept
    {
        for (Group& g : groups)
        {
            g.s1.fill(0.0);
            g.s2.fill(0.0);
            g.s1Float.fill(0.0f);
        }
    }
};
static_assert(alignof(ModeBank) <= 8, "ModeBank must fit within the 8-byte heap alignment ceiling");

template <int width>
inline void stepPart(ModeBank& bank, const std::size_t* coords, const ModeBank::BarVector& force,
                     int begin, int end, Pair (&accLeft)[ModeBank::lanes / 2],
                     Pair (&accRight)[ModeBank::lanes / 2]) noexcept
{
    const float* sh0 = width > 0 ? bank.shape[coords[0]].data() + begin : nullptr;
    const Float4 f0 = width > 0 ? splatFloat4(static_cast<float>(force[coords[0]])) : splatFloat4(0.0f);
    const float* sh1 = width > 1 ? bank.shape[coords[1]].data() + begin : nullptr;
    const Float4 f1 = width > 1 ? splatFloat4(static_cast<float>(force[coords[1]])) : splatFloat4(0.0f);
    const float* sh2 = width > 2 ? bank.shape[coords[2]].data() + begin : nullptr;
    const Float4 f2 = width > 2 ? splatFloat4(static_cast<float>(force[coords[2]])) : splatFloat4(0.0f);
    const float* sh3 = width > 3 ? bank.shape[coords[3]].data() + begin : nullptr;
    const Float4 f3 = width > 3 ? splatFloat4(static_cast<float>(force[coords[3]])) : splatFloat4(0.0f);
    const float* sh4 = width > 4 ? bank.shape[coords[4]].data() + begin : nullptr;
    const Float4 f4 = width > 4 ? splatFloat4(static_cast<float>(force[coords[4]])) : splatFloat4(0.0f);
    const float* sh5 = width > 5 ? bank.shape[coords[5]].data() + begin : nullptr;
    const Float4 f5 = width > 5 ? splatFloat4(static_cast<float>(force[coords[5]])) : splatFloat4(0.0f);
    const float* sh6 = width > 6 ? bank.shape[coords[6]].data() + begin : nullptr;
    const Float4 f6 = width > 6 ? splatFloat4(static_cast<float>(force[coords[6]])) : splatFloat4(0.0f);
    const float* sh7 = width > 7 ? bank.shape[coords[7]].data() + begin : nullptr;
    const Float4 f7 = width > 7 ? splatFloat4(static_cast<float>(force[coords[7]])) : splatFloat4(0.0f);
    const float* sh8 = width > 8 ? bank.shape[coords[8]].data() + begin : nullptr;
    const Float4 f8 = width > 8 ? splatFloat4(static_cast<float>(force[coords[8]])) : splatFloat4(0.0f);
    const float* sh9 = width > 9 ? bank.shape[coords[9]].data() + begin : nullptr;
    const Float4 f9 = width > 9 ? splatFloat4(static_cast<float>(force[coords[9]])) : splatFloat4(0.0f);

    ModeBank::Group* gp = bank.groups.data() + begin / ModeBank::lanes;

    for (int groups = (end - begin) / ModeBank::lanes; groups > 0; --groups)
    {
        Float4 sumF0 = splatFloat4(0.0f), sumF1 = splatFloat4(0.0f);
        if constexpr (width > 0) { const Float4* const p = float4s(sh0); sh0 += ModeBank::lanes; sumF0 += p[0] * f0; sumF1 += p[1] * f0; }
        if constexpr (width > 1) { const Float4* const p = float4s(sh1); sh1 += ModeBank::lanes; sumF0 += p[0] * f1; sumF1 += p[1] * f1; }
        if constexpr (width > 2) { const Float4* const p = float4s(sh2); sh2 += ModeBank::lanes; sumF0 += p[0] * f2; sumF1 += p[1] * f2; }
        if constexpr (width > 3) { const Float4* const p = float4s(sh3); sh3 += ModeBank::lanes; sumF0 += p[0] * f3; sumF1 += p[1] * f3; }
        if constexpr (width > 4) { const Float4* const p = float4s(sh4); sh4 += ModeBank::lanes; sumF0 += p[0] * f4; sumF1 += p[1] * f4; }
        if constexpr (width > 5) { const Float4* const p = float4s(sh5); sh5 += ModeBank::lanes; sumF0 += p[0] * f5; sumF1 += p[1] * f5; }
        if constexpr (width > 6) { const Float4* const p = float4s(sh6); sh6 += ModeBank::lanes; sumF0 += p[0] * f6; sumF1 += p[1] * f6; }
        if constexpr (width > 7) { const Float4* const p = float4s(sh7); sh7 += ModeBank::lanes; sumF0 += p[0] * f7; sumF1 += p[1] * f7; }
        if constexpr (width > 8) { const Float4* const p = float4s(sh8); sh8 += ModeBank::lanes; sumF0 += p[0] * f8; sumF1 += p[1] * f8; }
        if constexpr (width > 9) { const Float4* const p = float4s(sh9); sh9 += ModeBank::lanes; sumF0 += p[0] * f9; sumF1 += p[1] * f9; }

        const Pair sum0 = widenLo(sumF0);
        const Pair sum1 = widenHi(sumF0);
        const Pair sum2 = widenLo(sumF1);
        const Pair sum3 = widenHi(sumF1);

        ModeBank::Group& g = *gp++;
        Pair* const s1 = pairs(g.s1.data());
        Pair* const s2 = pairs(g.s2.data());
        const Pair* const a1 = pairs(g.a1.data());
        const Pair* const a2 = pairs(g.a2.data());
        const Pair* const toLeft = pairs(g.left.data());
        const Pair* const toRight = pairs(g.right.data());
        Float4* const s1f = float4s(g.s1Float.data());

        const Pair y0 = sum0 + s1[0], y1 = sum1 + s1[1], y2 = sum2 + s1[2], y3 = sum3 + s1[3];
        const Pair n0 = s2[0] - a1[0] * y0, n1 = s2[1] - a1[1] * y1, n2 = s2[2] - a1[2] * y2, n3 = s2[3] - a1[3] * y3;
        s2[0] = -sum0 - a2[0] * y0;
        s2[1] = -sum1 - a2[1] * y1;
        s2[2] = -sum2 - a2[2] * y2;
        s2[3] = -sum3 - a2[3] * y3;
        s1[0] = n0;
        s1[1] = n1;
        s1[2] = n2;
        s1[3] = n3;
        s1f[0] = packFloat4(n0, n1);
        s1f[1] = packFloat4(n2, n3);
        accLeft[0] += toLeft[0] * y0;
        accLeft[1] += toLeft[1] * y1;
        accLeft[2] += toLeft[2] * y2;
        accLeft[3] += toLeft[3] * y3;
        accRight[0] += toRight[0] * y0;
        accRight[1] += toRight[1] * y1;
        accRight[2] += toRight[2] * y2;
        accRight[3] += toRight[3] * y3;
    }
}

// One part's pull on `width` (up to 6) of its coordinates in one pass over the
// part's modes in 4-lane Float4: 12 accumulators + 2 state vectors + 2 shape
// vectors = 16 vector registers (fits without spilling on both ARM64 and x86_64).
template <int width>
inline void pullOnBarDirect(const ModeBank& bank, const std::size_t* list, int begin, int end,
                            ModeBank::BarVector& pull) noexcept
{
    const float* sh0 = bank.shape[list[0]].data() + begin;
    const float* sh1 = width > 1 ? bank.shape[list[1]].data() + begin : nullptr;
    const float* sh2 = width > 2 ? bank.shape[list[2]].data() + begin : nullptr;
    const float* sh3 = width > 3 ? bank.shape[list[3]].data() + begin : nullptr;
    const float* sh4 = width > 4 ? bank.shape[list[4]].data() + begin : nullptr;
    const float* sh5 = width > 5 ? bank.shape[list[5]].data() + begin : nullptr;
    Float4 c0v0 = splatFloat4(0.0f), c0v1 = splatFloat4(0.0f);
    Float4 c1v0 = splatFloat4(0.0f), c1v1 = splatFloat4(0.0f);
    Float4 c2v0 = splatFloat4(0.0f), c2v1 = splatFloat4(0.0f);
    Float4 c3v0 = splatFloat4(0.0f), c3v1 = splatFloat4(0.0f);
    Float4 c4v0 = splatFloat4(0.0f), c4v1 = splatFloat4(0.0f);
    Float4 c5v0 = splatFloat4(0.0f), c5v1 = splatFloat4(0.0f);

    const ModeBank::Group* gp = bank.groups.data() + begin / ModeBank::lanes;
    for (int groups = (end - begin) / ModeBank::lanes; groups > 0; --groups)
    {
        const Float4* const s = float4s((gp++)->s1Float.data());
        const Float4 s0 = s[0], s1 = s[1];
        {
            const Float4* const p = float4s(sh0);
            sh0 += ModeBank::lanes;
            c0v0 += p[0] * s0;
            c0v1 += p[1] * s1;
        }
        if constexpr (width > 1)
        {
            const Float4* const p = float4s(sh1);
            sh1 += ModeBank::lanes;
            c1v0 += p[0] * s0;
            c1v1 += p[1] * s1;
        }
        if constexpr (width > 2)
        {
            const Float4* const p = float4s(sh2);
            sh2 += ModeBank::lanes;
            c2v0 += p[0] * s0;
            c2v1 += p[1] * s1;
        }
        if constexpr (width > 3)
        {
            const Float4* const p = float4s(sh3);
            sh3 += ModeBank::lanes;
            c3v0 += p[0] * s0;
            c3v1 += p[1] * s1;
        }
        if constexpr (width > 4)
        {
            const Float4* const p = float4s(sh4);
            sh4 += ModeBank::lanes;
            c4v0 += p[0] * s0;
            c4v1 += p[1] * s1;
        }
        if constexpr (width > 5)
        {
            const Float4* const p = float4s(sh5);
            sh5 += ModeBank::lanes;
            c5v0 += p[0] * s0;
            c5v1 += p[1] * s1;
        }
    }

    const auto reduce = [] (Float4 a, Float4 b) noexcept
    {
        const Pair lo = widenLo(a) + widenHi(a);
        const Pair hi = widenLo(b) + widenHi(b);
        return (lo[0] + lo[1]) + (hi[0] + hi[1]);
    };
    pull[list[0]] = reduce(c0v0, c0v1);
    if constexpr (width > 1) pull[list[1]] = reduce(c1v0, c1v1);
    if constexpr (width > 2) pull[list[2]] = reduce(c2v0, c2v1);
    if constexpr (width > 3) pull[list[3]] = reduce(c3v0, c3v1);
    if constexpr (width > 4) pull[list[4]] = reduce(c4v0, c4v1);
    if constexpr (width > 5) pull[list[5]] = reduce(c5v0, c5v1);
}

template <int width>
inline void pullOnBar(const ModeBank& bank, const std::size_t* list, int begin, int end,
                      Float4 (*lanePull)[ModeBank::lanes / 4]) noexcept
{
    const float* sh0 = bank.shape[list[0]].data() + begin;
    const float* sh1 = width > 1 ? bank.shape[list[1]].data() + begin : nullptr;
    const float* sh2 = width > 2 ? bank.shape[list[2]].data() + begin : nullptr;
    const float* sh3 = width > 3 ? bank.shape[list[3]].data() + begin : nullptr;
    const float* sh4 = width > 4 ? bank.shape[list[4]].data() + begin : nullptr;
    Float4 c0v0 = lanePull[list[0]][0], c0v1 = lanePull[list[0]][1];
    Float4 c1v0 = width > 1 ? lanePull[list[1]][0] : splatFloat4(0.0f);
    Float4 c1v1 = width > 1 ? lanePull[list[1]][1] : splatFloat4(0.0f);
    Float4 c2v0 = width > 2 ? lanePull[list[2]][0] : splatFloat4(0.0f);
    Float4 c2v1 = width > 2 ? lanePull[list[2]][1] : splatFloat4(0.0f);
    Float4 c3v0 = width > 3 ? lanePull[list[3]][0] : splatFloat4(0.0f);
    Float4 c3v1 = width > 3 ? lanePull[list[3]][1] : splatFloat4(0.0f);
    Float4 c4v0 = width > 4 ? lanePull[list[4]][0] : splatFloat4(0.0f);
    Float4 c4v1 = width > 4 ? lanePull[list[4]][1] : splatFloat4(0.0f);

    const ModeBank::Group* gp = bank.groups.data() + begin / ModeBank::lanes;
    for (int groups = (end - begin) / ModeBank::lanes; groups > 0; --groups)
    {
        const Float4* const s = float4s((gp++)->s1Float.data());
        const Float4 s0 = s[0], s1 = s[1];
        { const Float4* const p = float4s(sh0); sh0 += ModeBank::lanes; c0v0 += p[0] * s0; c0v1 += p[1] * s1; }
        if constexpr (width > 1) { const Float4* const p = float4s(sh1); sh1 += ModeBank::lanes; c1v0 += p[0] * s0; c1v1 += p[1] * s1; }
        if constexpr (width > 2) { const Float4* const p = float4s(sh2); sh2 += ModeBank::lanes; c2v0 += p[0] * s0; c2v1 += p[1] * s1; }
        if constexpr (width > 3) { const Float4* const p = float4s(sh3); sh3 += ModeBank::lanes; c3v0 += p[0] * s0; c3v1 += p[1] * s1; }
        if constexpr (width > 4) { const Float4* const p = float4s(sh4); sh4 += ModeBank::lanes; c4v0 += p[0] * s0; c4v1 += p[1] * s1; }
    }

    lanePull[list[0]][0] = c0v0;
    lanePull[list[0]][1] = c0v1;
    if constexpr (width > 1) { lanePull[list[1]][0] = c1v0; lanePull[list[1]][1] = c1v1; }
    if constexpr (width > 2) { lanePull[list[2]][0] = c2v0; lanePull[list[2]][1] = c2v1; }
    if constexpr (width > 3) { lanePull[list[3]][0] = c3v0; lanePull[list[3]][1] = c3v1; }
    if constexpr (width > 4) { lanePull[list[4]][0] = c4v0; lanePull[list[4]][1] = c4v1; }
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
inline void runModesMixedPull(const ModeBank& bank, const int (&bounds)[4], ModeBank::BarVector& pull) noexcept
{
    constexpr int floatVectors = ModeBank::lanes / 4;
    alignas(16) Float4 lanePull[ModeBank::barCoordinates][floatVectors];
    for (auto& lane : lanePull)
        for (int v = 0; v < floatVectors; ++v)
            lane[v] = splatFloat4(0.0f);
    for (std::size_t part = 0; part < 3; ++part)
    {
        const int begin = bounds[part], end = bounds[part + 1];
        if (begin >= end)
            continue;
        const std::size_t* list = bank.coordinates[part].data();
        int remaining = bank.coordinateCount[part];
        for (; remaining >= 5; remaining -= 5, list += 5)
            pullOnBar<5>(bank, list, begin, end, lanePull);
        switch (remaining)
        {
            case 4: pullOnBar<4>(bank, list, begin, end, lanePull); break;
            case 3: pullOnBar<3>(bank, list, begin, end, lanePull); break;
            case 2: pullOnBar<2>(bank, list, begin, end, lanePull); break;
            case 1: pullOnBar<1>(bank, list, begin, end, lanePull); break;
            default: break;
        }
    }
    for (std::size_t c = 0; c < ModeBank::barCoordinates; ++c)
    {
        const Pair lo = widenLo(lanePull[c][0]) + widenHi(lanePull[c][0]);
        const Pair hi = widenLo(lanePull[c][1]) + widenHi(lanePull[c][1]);
        pull[c] = (lo[0] + lo[1]) + (hi[0] + hi[1]);
    }
}

inline void runModes(ModeBank& bank, const ModeBank::BarVector& force, double& left, double& right,
                     ModeBank::BarVector& pull) noexcept
{
    constexpr int vectors = ModeBank::lanes / 2;
    const int bounds[4] { 0, bank.evenEnd, bank.oddEnd, bank.count };
    Pair accLeft[vectors] { splat(0.0), splat(0.0), splat(0.0), splat(0.0) };
    Pair accRight[vectors] { splat(0.0), splat(0.0), splat(0.0), splat(0.0) };
    for (std::size_t part = 0; part < 3; ++part)
    {
        const int begin = bounds[part], end = bounds[part + 1];
        if (begin >= end)
            continue;
        const int coordCount = bank.coordinateCount[part];
        const std::size_t* const coords = bank.coordinates[part].data();
        switch (coordCount)
        {
            case 0:  stepPart<0>(bank, coords, force, begin, end, accLeft, accRight); break;
            case 1:  stepPart<1>(bank, coords, force, begin, end, accLeft, accRight); break;
            case 2:  stepPart<2>(bank, coords, force, begin, end, accLeft, accRight); break;
            case 3:  stepPart<3>(bank, coords, force, begin, end, accLeft, accRight); break;
            case 4:  stepPart<4>(bank, coords, force, begin, end, accLeft, accRight); break;
            case 5:  stepPart<5>(bank, coords, force, begin, end, accLeft, accRight); break;
            case 6:  stepPart<6>(bank, coords, force, begin, end, accLeft, accRight); break;
            case 7:  stepPart<7>(bank, coords, force, begin, end, accLeft, accRight); break;
            case 8:  stepPart<8>(bank, coords, force, begin, end, accLeft, accRight); break;
            case 9:  stepPart<9>(bank, coords, force, begin, end, accLeft, accRight); break;
            default: stepPart<10>(bank, coords, force, begin, end, accLeft, accRight); break;
        }
    }
    left = right = 0.0;
    for (int v = 0; v < vectors; ++v)
        for (int i = 0; i < 2; ++i)
        {
            left += accLeft[v][i];
            right += accRight[v][i];
        }

    if (bank.oddEnd < bank.count)
    {
        runModesMixedPull(bank, bounds, pull);
        return;
    }

    pull.fill(0.0);
    for (std::size_t part = 0; part < 2; ++part)
    {
        const int begin = bounds[part], end = bounds[part + 1];
        if (begin >= end)
            continue;
        const std::size_t* list = bank.coordinates[part].data();
        int remaining = bank.coordinateCount[part];
        for (; remaining >= 6; remaining -= 6, list += 6)
            pullOnBarDirect<6>(bank, list, begin, end, pull);
        switch (remaining)
        {
            case 5: pullOnBarDirect<5>(bank, list, begin, end, pull); break;
            case 4: pullOnBarDirect<4>(bank, list, begin, end, pull); break;
            case 3: pullOnBarDirect<3>(bank, list, begin, end, pull); break;
            case 2: pullOnBarDirect<2>(bank, list, begin, end, pull); break;
            case 1: pullOnBarDirect<1>(bank, list, begin, end, pull); break;
            default: break;
        }
    }
}

#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__)) && !defined(ACULYRE_DISABLE_AVX2)
#define ACULYRE_HAS_FUSED_KERNELS 1

typedef double Double4 __attribute__((vector_size(32), aligned(8), may_alias));

template <int width>
__attribute__((target("avx2,fma"))) inline void pullOnBarFused(const ModeBank& bank, const std::size_t* list,
                                                                int begin, int end,
                                                                Float4 (*lanePull)[2]) noexcept
{
    Float4 lo[width], hi[width];
    const float* row[width];
#if defined(__clang__)
#pragma unroll
#endif
    for (int c = 0; c < width; ++c)
    {
        lo[c] = lanePull[list[c]][0];
        hi[c] = lanePull[list[c]][1];
        row[c] = bank.shape[list[c]].data();
    }
    const ModeBank::Group* const groups = bank.groups.data();
    for (int group = begin; group < end; group += ModeBank::lanes)
    {
        const Float4* const s = float4s(groups[group / ModeBank::lanes].s1Float.data());
        const Float4 s0 = s[0], s1 = s[1];
#if defined(__clang__)
#pragma unroll
#endif
        for (int c = 0; c < width; ++c)
        {
            lo[c] = __builtin_ia32_vfmaddps(*float4s(row[c] + group), s0, lo[c]);
            hi[c] = __builtin_ia32_vfmaddps(*float4s(row[c] + group + 4), s1, hi[c]);
        }
    }
#if defined(__clang__)
#pragma unroll
#endif
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
            Float4 sumF[vectors] {};
            for (int n = 0; n < bank.coordinateCount[part]; ++n)
            {
                const std::size_t c = bank.coordinates[part][static_cast<std::size_t>(n)];
                const Float4* const shape = float4s(bank.shape[c].data() + group);
                const Float4 f = splatFloat4(static_cast<float>(force[c]));
                for (int v = 0; v < vectors; ++v)
                    sumF[v] = __builtin_ia32_vfmaddps(shape[v], f, sumF[v]);
            }
            ModeBank::Group& g = bank.groups[static_cast<std::size_t>(group / ModeBank::lanes)];
            Double4* const s1 = reinterpret_cast<Double4*>(g.s1.data());
            Double4* const s2 = reinterpret_cast<Double4*>(g.s2.data());
            const Double4* const a1 = reinterpret_cast<const Double4*>(g.a1.data());
            const Double4* const a2 = reinterpret_cast<const Double4*>(g.a2.data());
            const Double4* const toLeft = reinterpret_cast<const Double4*>(g.left.data());
            const Double4* const toRight = reinterpret_cast<const Double4*>(g.right.data());
            Float4* const s1f = float4s(g.s1Float.data());
            for (int v = 0; v < vectors; ++v)
            {
                const Double4 sum = __builtin_convertvector(sumF[v], Double4);
                const Double4 y = sum + s1[v];
                const Double4 next = __builtin_ia32_vfmaddpd256(-a1[v], y, s2[v]);
                s2[v] = __builtin_ia32_vfmaddpd256(-a2[v], y, -sum);
                s1[v] = next;
                s1f[v] = __builtin_convertvector(next, Float4);
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

    alignas(16) Float4 lanePull[ModeBank::barCoordinates][vectors] {};
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
        const Float4 a = lanePull[c][0], b = lanePull[c][1];
        pull[c] = ((static_cast<double>(a[0]) + static_cast<double>(a[1]))
                 + (static_cast<double>(a[2]) + static_cast<double>(a[3])))
                + ((static_cast<double>(b[0]) + static_cast<double>(b[1]))
                 + (static_cast<double>(b[2]) + static_cast<double>(b[3])));
    }
}
#endif
} // namespace aculyre::detail
