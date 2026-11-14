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

    // One eight-mode group's recursion, state and radiation arrays, packed
    // within 384 bytes so one pointer addresses all six via paired vector
    // loads/stores (ARM64 ldp/stp q immediate range is 1008 bytes) inside 19 KB
    // of L1 cache.
    struct Group
    {
        std::array<double, lanes> a1 {}, a2 {}, s1 {}, s2 {}, left {}, right {};
    };

    std::array<Group, capacity / lanes> groups {};
    std::array<std::array<double, capacity>, barCoordinates> shape {};
    std::array<std::array<std::size_t, barCoordinates>, 3> coordinates {};
    std::array<int, 3> coordinateCount {};
    int evenEnd { 0 }, oddEnd { 0 }, count { 0 };

    // Mode k from its coefficients, its shape at the bar's coordinates and
    // what a unit of its velocity radiates to each microphone. b0 > 0.
    void setMode(std::size_t k, double b0, double a1Value, double a2Value, const BarVector& participation,
                 double radiatesLeft, double radiatesRight) noexcept
    {
        const double root = squareRoot(b0);
        for (std::size_t c = 0; c < barCoordinates; ++c)
            shape[c][k] = root * participation[c];
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
        }
    }
};

// Drives and steps one parity part of `bank` when the part couples to a small
// fixed number `width` of bar coordinates, keeping the coordinates' forces and
// shape pointers in registers across all groups of eight modes.
template <int width>
inline void stepPart(ModeBank& bank, const std::size_t* coords, const ModeBank::BarVector& force,
                     int begin, int end, Pair (&accLeft)[ModeBank::lanes / 2],
                     Pair (&accRight)[ModeBank::lanes / 2]) noexcept
{
    const double* sh0 = width > 0 ? bank.shape[coords[0]].data() + begin : nullptr;
    const Pair f0 = width > 0 ? splat(force[coords[0]]) : splat(0.0);
    const double* sh1 = width > 1 ? bank.shape[coords[1]].data() + begin : nullptr;
    const Pair f1 = width > 1 ? splat(force[coords[1]]) : splat(0.0);
    const double* sh2 = width > 2 ? bank.shape[coords[2]].data() + begin : nullptr;
    const Pair f2 = width > 2 ? splat(force[coords[2]]) : splat(0.0);
    const double* sh3 = width > 3 ? bank.shape[coords[3]].data() + begin : nullptr;
    const Pair f3 = width > 3 ? splat(force[coords[3]]) : splat(0.0);
    const double* sh4 = width > 4 ? bank.shape[coords[4]].data() + begin : nullptr;
    const Pair f4 = width > 4 ? splat(force[coords[4]]) : splat(0.0);

    ModeBank::Group* gp = bank.groups.data() + begin / ModeBank::lanes;

    for (int groups = (end - begin) / ModeBank::lanes; groups > 0; --groups)
    {
        Pair sum0 = splat(0.0), sum1 = splat(0.0), sum2 = splat(0.0), sum3 = splat(0.0);
        if constexpr (width > 0)
        {
            const Pair* const p = pairs(sh0);
            sh0 += ModeBank::lanes;
            sum0 += p[0] * f0;
            sum1 += p[1] * f0;
            sum2 += p[2] * f0;
            sum3 += p[3] * f0;
        }
        if constexpr (width > 1)
        {
            const Pair* const p = pairs(sh1);
            sh1 += ModeBank::lanes;
            sum0 += p[0] * f1;
            sum1 += p[1] * f1;
            sum2 += p[2] * f1;
            sum3 += p[3] * f1;
        }
        if constexpr (width > 2)
        {
            const Pair* const p = pairs(sh2);
            sh2 += ModeBank::lanes;
            sum0 += p[0] * f2;
            sum1 += p[1] * f2;
            sum2 += p[2] * f2;
            sum3 += p[3] * f2;
        }
        if constexpr (width > 3)
        {
            const Pair* const p = pairs(sh3);
            sh3 += ModeBank::lanes;
            sum0 += p[0] * f3;
            sum1 += p[1] * f3;
            sum2 += p[2] * f3;
            sum3 += p[3] * f3;
        }
        if constexpr (width > 4)
        {
            const Pair* const p = pairs(sh4);
            sh4 += ModeBank::lanes;
            sum0 += p[0] * f4;
            sum1 += p[1] * f4;
            sum2 += p[2] * f4;
            sum3 += p[3] * f4;
        }

        ModeBank::Group& g = *gp++;
        Pair* const s1 = pairs(g.s1.data());
        Pair* const s2 = pairs(g.s2.data());
        const Pair* const a1 = pairs(g.a1.data());
        const Pair* const a2 = pairs(g.a2.data());
        const Pair* const toLeft = pairs(g.left.data());
        const Pair* const toRight = pairs(g.right.data());

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

// One part's pull on `width` of its coordinates in one pass over the part's
// modes, when no mixed-parity modes follow (oddEnd == count): each parity's
// coordinates are distinct, so the lane sums reduce straight into `pull`
// without a 640-byte stack table.
template <int width>
inline void pullOnBarDirect(const ModeBank& bank, const std::size_t* list, int begin, int end,
                            ModeBank::BarVector& pull) noexcept
{
    const double* sh0 = bank.shape[list[0]].data() + begin;
    const double* sh1 = width > 1 ? bank.shape[list[1]].data() + begin : nullptr;
    const double* sh2 = width > 2 ? bank.shape[list[2]].data() + begin : nullptr;
    const double* sh3 = width > 3 ? bank.shape[list[3]].data() + begin : nullptr;
    const double* sh4 = width > 4 ? bank.shape[list[4]].data() + begin : nullptr;
    Pair c0v0 = splat(0.0), c0v1 = splat(0.0), c0v2 = splat(0.0), c0v3 = splat(0.0);
    Pair c1v0 = splat(0.0), c1v1 = splat(0.0), c1v2 = splat(0.0), c1v3 = splat(0.0);
    Pair c2v0 = splat(0.0), c2v1 = splat(0.0), c2v2 = splat(0.0), c2v3 = splat(0.0);
    Pair c3v0 = splat(0.0), c3v1 = splat(0.0), c3v2 = splat(0.0), c3v3 = splat(0.0);
    Pair c4v0 = splat(0.0), c4v1 = splat(0.0), c4v2 = splat(0.0), c4v3 = splat(0.0);

    const ModeBank::Group* gp = bank.groups.data() + begin / ModeBank::lanes;
    for (int groups = (end - begin) / ModeBank::lanes; groups > 0; --groups)
    {
        const Pair* const s = pairs((gp++)->s1.data());
        const Pair s0 = s[0], s1 = s[1], s2 = s[2], s3 = s[3];
        {
            const Pair* const p = pairs(sh0);
            sh0 += ModeBank::lanes;
            c0v0 += p[0] * s0;
            c0v1 += p[1] * s1;
            c0v2 += p[2] * s2;
            c0v3 += p[3] * s3;
        }
        if constexpr (width > 1)
        {
            const Pair* const p = pairs(sh1);
            sh1 += ModeBank::lanes;
            c1v0 += p[0] * s0;
            c1v1 += p[1] * s1;
            c1v2 += p[2] * s2;
            c1v3 += p[3] * s3;
        }
        if constexpr (width > 2)
        {
            const Pair* const p = pairs(sh2);
            sh2 += ModeBank::lanes;
            c2v0 += p[0] * s0;
            c2v1 += p[1] * s1;
            c2v2 += p[2] * s2;
            c2v3 += p[3] * s3;
        }
        if constexpr (width > 3)
        {
            const Pair* const p = pairs(sh3);
            sh3 += ModeBank::lanes;
            c3v0 += p[0] * s0;
            c3v1 += p[1] * s1;
            c3v2 += p[2] * s2;
            c3v3 += p[3] * s3;
        }
        if constexpr (width > 4)
        {
            const Pair* const p = pairs(sh4);
            sh4 += ModeBank::lanes;
            c4v0 += p[0] * s0;
            c4v1 += p[1] * s1;
            c4v2 += p[2] * s2;
            c4v3 += p[3] * s3;
        }
    }

    pull[list[0]] = (((0.0 + c0v0[0]) + c0v0[1]) + c0v1[0]) + c0v1[1];
    pull[list[0]] = (((pull[list[0]] + c0v2[0]) + c0v2[1]) + c0v3[0]) + c0v3[1];
    if constexpr (width > 1)
    {
        double total = (((0.0 + c1v0[0]) + c1v0[1]) + c1v1[0]) + c1v1[1];
        pull[list[1]] = (((total + c1v2[0]) + c1v2[1]) + c1v3[0]) + c1v3[1];
    }
    if constexpr (width > 2)
    {
        double total = (((0.0 + c2v0[0]) + c2v0[1]) + c2v1[0]) + c2v1[1];
        pull[list[2]] = (((total + c2v2[0]) + c2v2[1]) + c2v3[0]) + c2v3[1];
    }
    if constexpr (width > 3)
    {
        double total = (((0.0 + c3v0[0]) + c3v0[1]) + c3v1[0]) + c3v1[1];
        pull[list[3]] = (((total + c3v2[0]) + c3v2[1]) + c3v3[0]) + c3v3[1];
    }
    if constexpr (width > 4)
    {
        double total = (((0.0 + c4v0[0]) + c4v0[1]) + c4v1[0]) + c4v1[1];
        pull[list[4]] = (((total + c4v2[0]) + c4v2[1]) + c4v3[0]) + c4v3[1];
    }
}

template <int width>
inline void pullOnBar(const ModeBank& bank, const std::size_t* list, int begin, int end,
                      Pair (*lanePull)[ModeBank::lanes / 2]) noexcept
{
    const double* sh0 = bank.shape[list[0]].data() + begin;
    const double* sh1 = width > 1 ? bank.shape[list[1]].data() + begin : nullptr;
    const double* sh2 = width > 2 ? bank.shape[list[2]].data() + begin : nullptr;
    Pair c0v0 = lanePull[list[0]][0], c0v1 = lanePull[list[0]][1], c0v2 = lanePull[list[0]][2], c0v3 = lanePull[list[0]][3];
    Pair c1v0 = width > 1 ? lanePull[list[1]][0] : splat(0.0);
    Pair c1v1 = width > 1 ? lanePull[list[1]][1] : splat(0.0);
    Pair c1v2 = width > 1 ? lanePull[list[1]][2] : splat(0.0);
    Pair c1v3 = width > 1 ? lanePull[list[1]][3] : splat(0.0);
    Pair c2v0 = width > 2 ? lanePull[list[2]][0] : splat(0.0);
    Pair c2v1 = width > 2 ? lanePull[list[2]][1] : splat(0.0);
    Pair c2v2 = width > 2 ? lanePull[list[2]][2] : splat(0.0);
    Pair c2v3 = width > 2 ? lanePull[list[2]][3] : splat(0.0);

    const ModeBank::Group* gp = bank.groups.data() + begin / ModeBank::lanes;
    for (int groups = (end - begin) / ModeBank::lanes; groups > 0; --groups)
    {
        const Pair* const s = pairs((gp++)->s1.data());
        const Pair s0 = s[0], s1 = s[1], s2 = s[2], s3 = s[3];
        {
            const Pair* const p = pairs(sh0);
            sh0 += ModeBank::lanes;
            c0v0 += p[0] * s0;
            c0v1 += p[1] * s1;
            c0v2 += p[2] * s2;
            c0v3 += p[3] * s3;
        }
        if constexpr (width > 1)
        {
            const Pair* const p = pairs(sh1);
            sh1 += ModeBank::lanes;
            c1v0 += p[0] * s0;
            c1v1 += p[1] * s1;
            c1v2 += p[2] * s2;
            c1v3 += p[3] * s3;
        }
        if constexpr (width > 2)
        {
            const Pair* const p = pairs(sh2);
            sh2 += ModeBank::lanes;
            c2v0 += p[0] * s0;
            c2v1 += p[1] * s1;
            c2v2 += p[2] * s2;
            c2v3 += p[3] * s3;
        }
    }

    lanePull[list[0]][0] = c0v0;
    lanePull[list[0]][1] = c0v1;
    lanePull[list[0]][2] = c0v2;
    lanePull[list[0]][3] = c0v3;
    if constexpr (width > 1)
    {
        lanePull[list[1]][0] = c1v0;
        lanePull[list[1]][1] = c1v1;
        lanePull[list[1]][2] = c1v2;
        lanePull[list[1]][3] = c1v3;
    }
    if constexpr (width > 2)
    {
        lanePull[list[2]][0] = c2v0;
        lanePull[list[2]][1] = c2v1;
        lanePull[list[2]][2] = c2v2;
        lanePull[list[2]][3] = c2v3;
    }
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
inline void runModesMixedPull(const ModeBank& bank, const int (&bounds)[4], ModeBank::BarVector& pull) noexcept
{
    constexpr int vectors = ModeBank::lanes / 2;
    Pair lanePull[ModeBank::barCoordinates][vectors];
    for (auto& lane : lanePull)
        for (int v = 0; v < vectors; ++v)
            lane[v] = splat(0.0);
    for (std::size_t part = 0; part < 3; ++part)
    {
        const int begin = bounds[part], end = bounds[part + 1];
        if (begin >= end)
            continue;
        const std::size_t* list = bank.coordinates[part].data();
        int remaining = bank.coordinateCount[part];
        for (; remaining >= 3; remaining -= 3, list += 3)
            pullOnBar<3>(bank, list, begin, end, lanePull);
        switch (remaining)
        {
            case 2: pullOnBar<2>(bank, list, begin, end, lanePull); break;
            case 1: pullOnBar<1>(bank, list, begin, end, lanePull); break;
            default: break;
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

// Runs the bank's modes one sample on under the force on the bar. Returns
// what they radiate to each microphone and their pull on the bar's next
// velocity. A group's generalized force and its recursion run together,
// while the force is still in registers; the pulls follow, up to three
// coordinates a pass so each load of a mode's state serves them all without
// spilling vector registers. Every sum keeps one accumulator per lane (a
// mode's place in its group of eight) and adds the lanes in order at the end,
// so the fused kernel below, four lanes a vector, adds the same terms in the
// same order.
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
            case 0:
                stepPart<0>(bank, coords, force, begin, end, accLeft, accRight);
                break;
            case 1:
                stepPart<1>(bank, coords, force, begin, end, accLeft, accRight);
                break;
            case 2:
                stepPart<2>(bank, coords, force, begin, end, accLeft, accRight);
                break;
            case 3:
                stepPart<3>(bank, coords, force, begin, end, accLeft, accRight);
                break;
            case 4:
                stepPart<4>(bank, coords, force, begin, end, accLeft, accRight);
                break;
            case 5:
                stepPart<5>(bank, coords, force, begin, end, accLeft, accRight);
                break;
            default:
                for (int group = begin; group < end; group += ModeBank::lanes)
                {
                    Pair sum[vectors] { splat(0.0), splat(0.0), splat(0.0), splat(0.0) };
                    for (int n = 0; n < coordCount; ++n)
                    {
                        const std::size_t c = coords[n];
                        const Pair* const shape = pairs(bank.shape[c].data() + group);
                        const Pair fc = splat(force[c]);
                        for (int v = 0; v < vectors; ++v)
                            sum[v] += shape[v] * fc;
                    }
                    ModeBank::Group& g = bank.groups[static_cast<std::size_t>(group / ModeBank::lanes)];
                    Pair* const s1 = pairs(g.s1.data());
                    Pair* const s2 = pairs(g.s2.data());
                    const Pair* const a1 = pairs(g.a1.data());
                    const Pair* const a2 = pairs(g.a2.data());
                    const Pair* const toLeft = pairs(g.left.data());
                    const Pair* const toRight = pairs(g.right.data());
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
                break;
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
#if defined(__aarch64__) || defined(_M_ARM64)
        for (; remaining >= 5; remaining -= 5, list += 5)
            pullOnBarDirect<5>(bank, list, begin, end, pull);
        switch (remaining)
        {
            case 4: pullOnBarDirect<4>(bank, list, begin, end, pull); break;
            case 3: pullOnBarDirect<3>(bank, list, begin, end, pull); break;
            case 2: pullOnBarDirect<2>(bank, list, begin, end, pull); break;
            case 1: pullOnBarDirect<1>(bank, list, begin, end, pull); break;
            default: break;
        }
#else
        for (; remaining >= 3; remaining -= 3, list += 3)
            pullOnBarDirect<3>(bank, list, begin, end, pull);
        switch (remaining)
        {
            case 2: pullOnBarDirect<2>(bank, list, begin, end, pull); break;
            case 1: pullOnBarDirect<1>(bank, list, begin, end, pull); break;
            default: break;
        }
#endif
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
    const ModeBank::Group* const groups = bank.groups.data();
    for (int group = begin; group < end; group += ModeBank::lanes)
    {
        const double* const s = groups[group / ModeBank::lanes].s1.data();
        const Double4 s0 = *reinterpret_cast<const Double4*>(s);
        const Double4 s1 = *reinterpret_cast<const Double4*>(s + 4);
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
            ModeBank::Group& g = bank.groups[static_cast<std::size_t>(group / ModeBank::lanes)];
            Double4* const s1 = reinterpret_cast<Double4*>(g.s1.data());
            Double4* const s2 = reinterpret_cast<Double4*>(g.s2.data());
            const Double4* const a1 = reinterpret_cast<const Double4*>(g.a1.data());
            const Double4* const a2 = reinterpret_cast<const Double4*>(g.a2.data());
            const Double4* const toLeft = reinterpret_cast<const Double4*>(g.left.data());
            const Double4* const toRight = reinterpret_cast<const Double4*>(g.right.data());
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
