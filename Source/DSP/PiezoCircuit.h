// Electrical portion of Acustra's component-modelled pickup, extracted
// unchanged in its parts and integration rules. The input is the force ON
// THE ELEMENT in newtons, after Aculyre's existing mechanical seat filter.
// There is deliberately no second saddle/seat here.
//
// Charge source d*F, d=Cp*0.2 V/N=290 pC/N, drives Cp and the loaded cable /
// preamp network. Every internal signal is volts about the 4.5 V bias.
// process() divides out only the small-signal mid-band V/N; its result is
// force-equivalent newtons for the existing pickup output gain. This does
// not divide the input drive or move the overload threshold.
//
// This remains a weak electromechanical-loading approximation: using the
// existing 3.8 g / 6 kHz reference seat, d^2*k/Cp=0.0003133. With
// Aculyre's length-scaled stiffness and this fixed electrical pickup, the
// largest (240 mm) bar gives 0.001045: under 0.105% open/short stiffness
// change and 0.91 cents resonance shift (the 90 mm gut bar is smaller).
// The separate one-way mechanical seat approximation is not made reciprocal
// by this circuit and must not be counted twice in the bridge mobility.
//
// Tools/PiezoCircuitReference.py independently solves every circuit node,
// finite-bandwidth op-amp and diode; Tests/PiezoCircuitTests.cpp compares
// this audio-rate reduction with its DC/AC/overload/harmonic fixtures.
#pragma once

#include "PiezoCircuitBlampTable.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace aculyre
{
class PiezoCircuit
{
public:
    static constexpr int latencySamples = 7;
    struct Probe
    {
        double openCircuit {}, jack {}, bufferInput {}, gainStageDrive {};
    };

    // Non-real-time coefficient/table design. No allocation is needed in
    // prepare, reset or process. Engine preparation supplies a valid rate.
    void prepare(double rate)
    {
        sampleRate_ = std::isfinite(rate) && rate >= 8000.0 ? rate : 48000.0;
        using D = Design;
        // The input section as deviations from its operating point. The jack
        // node carries only capacitors, so its charge stays where it was and
        // V_J = (Cp V_oc + C1 V_IN) / (Cs + C1); the IN node then sees C1 in
        // series with the jack's shunt, beside its own capacitance to ground.
        // With w = V_IN - kin V_oc, V_B from its KCL and U1A's output at
        // V_IN + e (e is nonzero only while its input range stops it):
        //   w'    = -(V_IN - V_B) / (R3 Ceq)
        //   V_C2' = (V_B - V_C2 - O1) / (R4 C2)
        const double shunt = D::elementCapacitance + D::cableCapacitance
            + D::strayCapacitance;
        const double atInput = D::inputCapacitance + D::strayCapacitance;
        const double total = shunt + D::c1;
        const double series = D::c1 * shunt + atInput * total;
        piezoInputShare_ = D::c1 * D::elementCapacitance / series;
        piezoJackElement_ = D::elementCapacitance / total;
        piezoJackInput_ = D::c1 / total;
        const double inputCapacitance = series / total;
        piezoDiodeScale_ = inputCapacitance * sampleRate_ / D::diodeSaturation;
        // The diodes' dump z over one sample at a drive x past U1A's range:
        // Ceq fs z / IS = e^((x - z) / N Vt) - 1, zero at zero, by bisection
        // (z lies in [0, x], the right side falling in z) and Newton; and
        // its slope dz/dx = (Kz + 1) / (N Vt K + Kz + 1), K = Ceq fs / IS.
        // Cubic Hermite between the points is within 5 nV of the solution.
        for (std::size_t point = 0; point < piezoDiodeDump_.size(); ++point)
        {
            const double drive = piezoDiodeStep * static_cast<double>(point);
            double low = 0.0, high = drive;
            for (int step = 0; step < 200 && high - low > 1.0e-15; ++step)
            {
                const double middle = 0.5 * (low + high);
                const double balance = piezoDiodeScale_ * middle
                    - std::expm1((drive - middle) / D::diodeThermalVoltage);
                (balance > 0.0 ? high : low) = middle;
            }
            const double dump = 0.5 * (low + high);
            piezoDiodeDump_[point] = dump;
            piezoDiodeSlope_[point] = (piezoDiodeScale_ * dump + 1.0)
                / (D::diodeThermalVoltage * piezoDiodeScale_ + piezoDiodeScale_ * dump + 1.0);
        }
        const double bias = D::r1 * D::r2 / (D::r1 + D::r2);
        const double node = 1.0 / D::r3 + 1.0 / D::r4 + 1.0 / bias;
        const double share = 1.0 / (bias * node);
        const double back = 1.0 / (D::r4 * node);
        const double inputRate = 1.0 / (D::r3 * inputCapacitance);
        const double bootstrapRate = 1.0 / (D::r4 * D::c2);
        const double a00 = -share * inputRate, a01 = back * inputRate;
        const double a10 = -share * bootstrapRate, a11 = (back - 1.0) * bootstrapRate;
        const double bv0 = a00 * piezoInputShare_, bv1 = a10 * piezoInputShare_;
        const double be0 = a01, be1 = a11;
        // Trapezoidal: x[n] = (I - hA)^-1 ((I + hA) x[n-1] + h B (u[n] + u[n-1])).
        const double h = 0.5 / sampleRate_;
        const double m00 = 1.0 - h * a11, m01 = h * a01;
        const double m10 = h * a10, m11 = 1.0 - h * a00;
        const double determinant = m11 * m00 - m01 * m10;
        const auto solve = [&] (double v0, double v1, double& out0, double& out1)
        {
            out0 = (m00 * v0 + m01 * v1) / determinant;
            out1 = (m10 * v0 + m11 * v1) / determinant;
        };
        solve(1.0 + h * a00, h * a10, piezoFrontA00_, piezoFrontA10_);
        solve(h * a01, 1.0 + h * a11, piezoFrontA01_, piezoFrontA11_);
        solve(h * bv0, h * bv1, piezoFrontB0_, piezoFrontB1_);
        solve(h * be0, h * be1, piezoFrontE0_, piezoFrontE1_);
        // C3 into R5 || R6; C4 always sees U1B's output through R7 + R8
        // (R8 alone while U1B follows its input, since then N = Y and
        // O2 - V_C4 = (1 + R7/R8)(Y - V_C4)); C5 into R9 and the pot || DI.
        const auto onePole = [h] (double timeConstant, double& pole, double& gain)
        {
            const double k = h / timeConstant;
            pole = (1.0 - k) / (1.0 + k);
            gain = k / (1.0 + k);
        };
        const double load = D::volume * D::diInput / (D::volume + D::diInput);
        onePole(D::r5 * D::r6 / (D::r5 + D::r6) * D::c3, piezoC3Pole_, piezoC3Gain_);
        onePole((D::r7 + D::r8) * D::c4, piezoC4Pole_, piezoC4Gain_);
        onePole((D::r9 + load) * D::c5, piezoC5Pole_, piezoC5Gain_);
        // The mid-band gain from the element's open-circuit voltage to the
        // DI, which the level match divides out with the element's
        // sensitivity: one newton through the flat band remains one
        // force-equivalent newton before the engine's pickup output gain.
        piezoMidbandGain_ = piezoInputShare_ * (1.0 + D::r7 / D::r8) * load / (D::r9 + load);
        reset();
    }

    void reset() noexcept
    {
        piezoFrontW_ = piezoFrontC2_ = 0.0;
        piezoLastOpen_ = piezoLastClamp_ = 0.0;
        piezoC3_ = piezoC4_ = piezoC5_ = 0.0;
        piezoLastBuffer_ = piezoLastStage_ = 0.0;
        piezoDrive_.fill(0.0);
        piezoStage_.fill(0.0);
        piezoOutputVolts_ = 0.0;
        lastPiezoOpen_ = lastPiezoVoltage_ = lastPiezoInput_ = lastPiezoDrive_ = 0.0;
    }

    double process(double elementForceNewtons) noexcept
    {
        using D = Design;
        if (!std::isfinite(elementForceNewtons) || piezoMidbandGain_ <= 0.0)
        {
            reset();
            return 0.0;
        }
        // 5 pN is a picovolt at the element, about 130 dB below its
        // capacitance's thermal noise; avoid carrying last-bit residue.
        const double input = std::abs(elementForceNewtons) < 5.0e-12
            ? 0.0 : elementForceNewtons;
        // 2. The element's open-circuit voltage.
        const double open = D::voltsPerNewton * input;
        // 3. The input section (trapezoidal; U1A's output stopping at its input
        // range enters a sample late, through C2's bootstrap, 0.13 s).
        const double drive = open + piezoLastOpen_;
        const double clampSum = 2.0 * piezoLastClamp_;
        const double w = piezoFrontA00_ * piezoFrontW_ + piezoFrontA01_ * piezoFrontC2_
            + piezoFrontB0_ * drive + piezoFrontE0_ * clampSum;
        piezoFrontC2_ = piezoFrontA10_ * piezoFrontW_ + piezoFrontA11_ * piezoFrontC2_
            + piezoFrontB1_ * drive + piezoFrontE1_ * clampSum;
        piezoFrontW_ = w;
        double bufferInput = w + piezoInputShare_ * open;
        const double freeInput = bufferInput;
        // Past U1A's input range its output stops at the limit and D1/D2 see
        // the rest: they conduct as Shockley diodes, taking from the IN node,
        // over one sample, the charge Ceq z that brings the diode's voltage
        // x - z to what that current needs (backward Euler), tabulated in
        // prepare() (piezoDiodeDump_). C1 keeps the charge it gave up: the bias
        // shift a hard overload leaves, recovering over the input network's
        // 1 Hz modes. For a sine none of this runs below 1.9 dB over U1B's
        // positive swing or 0.35 dB over its negative one (U1B's gain of 1.62);
        // The reference strums originally stayed below it. At extreme bridge
        // position, maximum velocity and softer Touch, coherent string forces
        // can briefly reach it; retain the modeled circuit's physical overload.
        const double excess = (bufferInput > 0.0 ? bufferInput : -bufferInput) - D::commonModeLimit;
        if (excess > 0.0)
        {
            double dump;
            constexpr double top = piezoDiodeStep * (piezoDiodePoints - 1);
            if (excess < top)
            {
                // Cubic Hermite between the table's points, its slopes exact.
                const double position = excess / piezoDiodeStep;
                const auto point = static_cast<std::size_t>(position);
                const double t = position - static_cast<double>(point);
                const double t2 = t * t, t3 = t2 * t;
                dump = (2.0 * t3 - 3.0 * t2 + 1.0) * piezoDiodeDump_[point]
                    + (t3 - 2.0 * t2 + t) * piezoDiodeStep * piezoDiodeSlope_[point]
                    + (3.0 * t2 - 2.0 * t3) * piezoDiodeDump_[point + 1]
                    + (t3 - t2) * piezoDiodeStep * piezoDiodeSlope_[point + 1];
            }
            else
            {
                // Past 8 V the diode voltage is a slowly growing logarithm:
                // z = x - N Vt ln(1 + Ceq fs z / IS), by two fixed-point steps.
                dump = excess;
                for (int step = 0; step < 3; ++step)
                    dump = excess - D::diodeThermalVoltage * std::log1p(piezoDiodeScale_ * dump);
            }
            const double sign = bufferInput > 0.0 ? 1.0 : -1.0;
            bufferInput -= sign * dump;
            piezoFrontW_ -= sign * dump;
        }
        const double buffer = bufferInput > D::commonModeLimit ? D::commonModeLimit
            : (bufferInput < -D::commonModeLimit ? -D::commonModeLimit : bufferInput);
        piezoLastClamp_ = buffer - bufferInput;
        piezoLastOpen_ = open;
        // 4. C3 into U1B's bias network.
        piezoC3_ = piezoC3Pole_ * piezoC3_ + piezoC3Gain_ * (buffer + piezoLastBuffer_);
        piezoLastBuffer_ = buffer;
        const double coupled = buffer - piezoC3_;
        // 5. U1B: the drive its feedback would give, clipped at its output
        // swing, with the clip's corners band-limited by a 12-tap BLAMP
        // (PiezoCircuitBlampTable.h). C4's state lags the drive by the seven samples
        // the pipeline holds, in a 0.48 Hz loop: 5e-4 rad.
        constexpr double feedback = D::r7 / D::r8;
        const double stageDrive = (1.0 + feedback) * coupled - feedback * piezoC4_;
        // The corners are placed on the drive U1A's input would give if neither
        // its input range nor the diodes stopped it: they only act while U1B
        // is already past its swing, and their own corners, a sample or less
        // from U1B's at 3 kHz, would bend the cubic the corner is found on.
        const double freeDrive = (1.0 + feedback) * (freeInput - piezoC3_)
            - feedback * piezoC4_;
        auto& u = piezoDrive_;
        auto& stage = piezoStage_;
        u[0] = u[1];
        u[1] = u[2];
        u[2] = u[3];
        u[3] = freeDrive;
        // stage[m] is sample n - 7 + m: seven held back, this one, and four
        // ahead that only hold corrections so far.
        for (std::size_t m = 0; m + 1 < stage.size(); ++m)
            stage[m] = stage[m + 1];
        stage[stage.size() - 1] = 0.0;
        stage[7] += stageDrive > D::railHigh ? D::railHigh
            : (stageDrive < D::railLow ? D::railLow : stageDrive);
        for (const double rail : { D::railHigh, D::railLow })
        {
            const double before = u[1] - rail, after = u[2] - rail;
            if (!(before * after < 0.0))
                continue;
            // The corner lies between samples n - 2 and n - 1, on the cubic
            // through the four drives about u[1], relative to the rail.
            const double p0 = u[0] - rail, p3 = u[3] - rail;
            const double c1 = -p0 / 3.0 - before / 2.0 + after - p3 / 6.0;
            const double c2 = 0.5 * p0 - before + 0.5 * after;
            const double c3 = -p0 / 6.0 + before / 2.0 - after / 2.0 + p3 / 6.0;
            double d = before / (before - after);
            for (int step = 0; step < 2; ++step)
            {
                const double value = before + d * (c1 + d * (c2 + d * c3));
                const double slope = c1 + d * (2.0 * c2 + 3.0 * d * c3);
                if (slope != 0.0)
                    d -= value / slope;
                d = d < 0.0 ? 0.0 : (d > 0.999999 ? 0.999999 : d);
            }
            const double slope = c1 + d * (2.0 * c2 + 3.0 * d * c3);
            const bool entering = rail > 0.0 ? after > 0.0 : after < 0.0;
            const double change = entering ? -slope : slope;
            // The residual at samples n - 7 .. n + 4, between two of the table's
            // 129 fractional positions.
            const double position = d * piezoBlampPhases;
            const auto row = static_cast<std::size_t>(position);
            const double share = position - static_cast<double>(row);
            const auto& lower = piezoBlampTable[row];
            const auto& upper = piezoBlampTable[row + 1];
            for (std::size_t m = 0; m < stage.size(); ++m)
                stage[m] += change * (lower[m] + share * (upper[m] - lower[m]));
        }
        // 6. The output seven samples back: C4's and C5's states follow it, and
        // the DI takes it through R9 and C5 off the pot.
        const double gainStage = stage[0];
        piezoC4_ = piezoC4Pole_ * piezoC4_ + piezoC4Gain_ * (gainStage + piezoLastStage_);
        piezoC5_ = piezoC5Pole_ * piezoC5_ + piezoC5Gain_ * (gainStage + piezoLastStage_);
        piezoLastStage_ = gainStage;
        constexpr double load = D::volume * D::diInput / (D::volume + D::diInput);
        piezoOutputVolts_ = load / (D::r9 + load) * (gainStage - piezoC5_);
        lastPiezoOpen_ = open;
        lastPiezoVoltage_ = piezoJackElement_ * open + piezoJackInput_ * bufferInput;
        lastPiezoInput_ = bufferInput;
        lastPiezoDrive_ = stageDrive;

        // Flush all electrical histories together below 10 nV, 50 dB
        // below the preamp's 1.2 uV noise. No mechanical state lives here.
        if (input == 0.0)
        {
            constexpr double floor = 1.0e-8;
            bool quiet = true;
            for (double value : { piezoFrontW_, piezoFrontC2_, piezoLastOpen_, piezoLastClamp_,
                                  piezoC3_, piezoC4_, piezoC5_, piezoLastBuffer_,
                                  piezoLastStage_, u[0], u[1], u[2], u[3] })
                quiet = quiet && std::abs(value) < floor;
            for (double value : stage)
                quiet = quiet && std::abs(value) < floor;
            if (quiet)
                reset();
        }
        const double result = piezoOutputVolts_ / midbandVoltsPerNewton();
        if (!std::isfinite(result))
        {
            reset();
            return 0.0;
        }
        return result;
    }

    Probe probe() const noexcept
    {
        return { lastPiezoOpen_, lastPiezoVoltage_, lastPiezoInput_, lastPiezoDrive_ };
    }
    double outputVolts() const noexcept { return piezoOutputVolts_; }
    double midbandVoltsPerNewton() const noexcept
    {
        return Design::voltsPerNewton * piezoMidbandGain_;
    }

private:
    friend struct PiezoCircuitTestAccess;
    struct Design
    {
        // 3. The element: a charge source across its capacitance, 0.2 V/N
        // open-circuit at 1.45 nF (Zollner ch.6, Ovation EA-68).
        static constexpr double voltsPerNewton = 0.2;
        static constexpr double elementCapacitance = 1.45e-9;
        // 4. The cable: 3 m of Mogami 2524 at 130 pF/m (length chosen), and
        // a stray picofarad at each input node (chosen).
        static constexpr double cableCapacitance = 390.0e-12;
        static constexpr double strayCapacitance = 1.0e-12;
        // The preamp is Acustra's existing ESP Project 202 Figure 1 design
        // (R. Elliott, sound-au.com/project202.htm), one 9 V battery, OPA2134
        // (TI SBOS058B), into the Radial PZ-DI's 1 MOhm input. R7=6.2 kOhm
        // is the existing E24 gain choice, unchanged for this pickup. U1A is
        // bootstrapped by C2/R4; D1/D2 protect IN; C3 couples U1B, whose
        // gain 1+R7/R8 falls to one below C4's corner; C5 feeds the full pot.
        static constexpr double c1 = 4.7e-9;
        static constexpr double r1 = 1.0e6, r2 = 1.0e6, r3 = 1.0e6;
        static constexpr double c2 = 33.0e-6, r4 = 3.9e3;
        static constexpr double c3 = 220.0e-9, r5 = 47.0e3, r6 = 47.0e3;
        static constexpr double r7 = 6.2e3, r8 = 10.0e3, c4 = 33.0e-6;
        static constexpr double r9 = 100.0, c5 = 10.0e-6;
        static constexpr double volume = 10.0e3, diInput = 1.0e6;
        // The OPA2134's common-mode input capacitance (6 pF), and its
        // limits about the 4.5 V bias: the typical input common-mode range,
        // 2 V inside each rail (+-13 V at +-15 V), and the output swing for
        // U1B's 6.18 kOhm load ((R7 + R8) || (R9 + pot || DI)), the
        // datasheet's guaranteed 10 kOhm and 2 kOhm rows interpolated in
        // conductance: V+ - 1.2463 V, V- + 0.6080 V.
        // The OPA2134 has no output phase reversal.
        static constexpr double inputCapacitance = 6.0e-12;
        static constexpr double commonModeLimit = 2.5;
        static constexpr double railHigh = 3.2537111;
        static constexpr double railLow = -3.8919926;
        // D1/D2, 1N4148s: IS 2.52 nA, N 1.752 (the widely reposted SPICE
        // model), at 25 C: N kT/q = 45.0 mV.
        static constexpr double diodeSaturation = 2.52e-9;
        static constexpr double diodeThermalVoltage = 1.752 * 0.025692579;
    };
    double sampleRate_ { 48000.0 };
    // The preamp's input section, trapezoidal, as deviations from its
    // operating point: w = V_IN - kin V_oc and C2's voltage.
    double piezoFrontA00_ {}, piezoFrontA01_ {}, piezoFrontA10_ {}, piezoFrontA11_ {};
    double piezoFrontB0_ {}, piezoFrontB1_ {}, piezoFrontE0_ {}, piezoFrontE1_ {};
    double piezoInputShare_ {}, piezoJackInput_ {}, piezoJackElement_ {};
    double piezoFrontW_ {}, piezoFrontC2_ {};
    double piezoLastOpen_ {}, piezoLastClamp_ {};
    // D1/D2: Ceq fs / IS, the IN node's charge per volt and sample over
    // the diodes' saturation current, and the charge (as volts at IN) they
    // take in one sample at 0 to 8 V past U1A's range, with its slope.
    static constexpr int piezoDiodePoints = 1025;
    static constexpr double piezoDiodeStep = 8.0 / (piezoDiodePoints - 1);
    double piezoDiodeScale_ {};
    std::array<double, piezoDiodePoints> piezoDiodeDump_ {};
    std::array<double, piezoDiodePoints> piezoDiodeSlope_ {};
    // C3, C4 and C5 as trapezoidal one-poles: state = pole * state
    // + gain * (input + previous input).
    double piezoC3Pole_ {}, piezoC3Gain_ {}, piezoC4Pole_ {}, piezoC4Gain_ {};
    double piezoC5Pole_ {}, piezoC5Gain_ {};
    double piezoC3_ {}, piezoC4_ {}, piezoC5_ {};
    double piezoLastBuffer_ {}, piezoLastStage_ {};
    // U1B's drive for the four samples a corner is found on, and its
    // clipped output for the twelve the BLAMP spans (seven held back, the
    // newest, and four ahead that only hold corrections), oldest first; the
    // chain's output is seven samples behind its input.
    std::array<double, 4> piezoDrive_ {};
    std::array<double, 12> piezoStage_ {};
    double piezoOutputVolts_ { 0.0 };
    double piezoMidbandGain_ {};
    double lastPiezoOpen_ {}, lastPiezoVoltage_ {}, lastPiezoInput_ {}, lastPiezoDrive_ {};
};
} // namespace aculyre
