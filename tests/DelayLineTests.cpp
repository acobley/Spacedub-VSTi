//------------------------------------------------------------------------
// SpaceDub - DelayLine checks
//
// DelayLine.cpp is portable C++17 and depends on nothing but the standard
// library, so it can be checked without the VST3 SDK, without a host and
// without a window server:
//
//     c++ -std=c++17 -O2 -Wall -Wextra -I../source ../source/DelayLine.cpp DelayLineTests.cpp -o delayline-tests
//     ./delayline-tests
//
// What it pins down:
//
//   1/5. The cutoff mapping.  computeCutoffHz() clamps at sampleRate/4 where
//        the original silently froze the cutoff instead (PORTING-NOTES 4).
//        Check 1 is the fidelity check for that change: at every sample rate
//        a host will realistically use, the clamp never engages and the
//        result is bit-identical to the mapping the DXi had.
//   2.   The clamp itself, at the low rates where it does engage.
//   3.   The delay mapping - the verification run quoted in PORTING-NOTES 3,
//        where 48 kHz at a 0.05 delay setting puts the taps on n x 192.
//   4.   Tempo sync landing on a whole number of samplesPer32 / 5 steps.
//------------------------------------------------------------------------

#include "DelayLine.h"
#include <cmath>
#include <cstdio>
#include <algorithm>
using namespace SpaceDub;

static int fails = 0;
static void chk(bool ok, const char* what) {
    if (!ok) { std::printf("  FAIL: %s\n", what); ++fails; }
}

// The mapping exactly as it stood before the change.
// samplesPer32For() lives in SpaceDubParams.h, which pulls in the VST3 SDK.
// Reproduced here verbatim so this test stays a standalone translation unit.
static int samplesPer32For (int divisions, double bpm, double sampleRate) {
    if (divisions == 0 || bpm <= 0.0) return 0;
    const double seconds = (60.0 / bpm) * (4.0 / (double) divisions);
    const int samples = (int) (seconds * sampleRate);
    return samples < 1 ? 1 : samples;
}

static double oldHz (double f) { f = f*f; if (f < 0.001) f = 0.001; return 10000.0 * f; }

int main () {
    // 1. At every sample rate a host will realistically use, the new clamped
    //    mapping must be bit-identical to the old one.
    std::printf("1. computeCutoffHz vs the original mapping\n");
    for (int sr : {44100, 48000, 88200, 96000, 176400, 192000}) {
        double worst = 0.0;
        for (int i = 0; i <= 10000; ++i) {
            double f = i / 10000.0;
            worst = std::max(worst, std::fabs(DelayLine::computeCutoffHz(f, sr) - oldHz(f)));
        }
        std::printf("   %6d Hz: max deviation %g\n", sr, worst);
        chk(worst == 0.0, "cutoff mapping changed at a normal sample rate");
    }
    // 2. Below ~40 kHz the clamp is what engages, and it must sit on SR/4.
    std::printf("2. the clamp, at rates where the original froze instead\n");
    for (int sr : {32000, 22050, 16000, 11025, 8000}) {
        double hz = DelayLine::computeCutoffHz(1.0, sr);
        std::printf("   %6d Hz: full-scale cutoff %8.1f Hz  (SR/4 = %8.1f)\n", sr, hz, sr/4.0);
        chk(hz <= sr/4.0 + 1e-9, "cutoff above SR/4");
        chk(std::fabs(hz - sr/4.0) < 1e-9, "clamp not sitting on SR/4");
    }
    chk(DelayLine::computeCutoffHz(0.0, 48000) == 10.0, "floor of 0.001 lost");

    // 3. The tap spacing the notes record: 48 kHz, delay 0.05 -> n x 192.
    std::printf("3. delay mapping (the notes' verification run)\n");
    long perTap = DelayLine::computeDelaySamples(0.05f, 48000L * DelayLine::kLineSeconds, 0);
    std::printf("   48 kHz, delay 0.05: taps at");
    for (int n = 1; n <= DelayLine::kNumTaps; ++n) std::printf(" %ld", n * perTap);
    std::printf("\n");
    chk(perTap == 192, "per-tap delay is not 192 samples");

    // 4. Tempo sync must land on a whole number of samplesPer32/5 steps.
    std::printf("4. tempo sync quantisation\n");
    for (double bpm : {90.0, 120.0, 140.0}) {
        int grid = samplesPer32For(16, bpm, 48000.0);   // 16th notes
        int steps = 0;
        long d = DelayLine::computeDelaySamples(0.37f, 48000L*8, grid, &steps);
        long step = std::max(1, grid / DelayLine::kNumTaps);
        std::printf("   %5.0f bpm: grid %d, step %ld, delay %ld, steps %d\n", bpm, grid, step, d, steps);
        chk(d % step == 0, "delay not a whole number of grid steps");
        chk(d == steps * step, "steps count disagrees with the delay");
    }

    // 5. Round trip through a live DelayLine: setFrequency's return is the Hz.
    std::printf("5. setFrequency returns the shared mapping\n");
    DelayLine dl; dl.setSampleRate(48000);
    double worst = 0.0;
    for (int i = 0; i <= 1000; ++i) {
        double f = i / 1000.0;
        worst = std::max(worst, std::fabs((double)dl.setFrequency(f)
                                          - DelayLine::computeCutoffHz(f, 48000)));
    }
    std::printf("   max deviation %g\n", worst);
    chk(worst < 1e-3, "setFrequency disagrees with computeCutoffHz");

    std::printf(fails ? "\n%d CHECK(S) FAILED\n" : "\nall checks passed\n", fails);
    return fails != 0;
}
