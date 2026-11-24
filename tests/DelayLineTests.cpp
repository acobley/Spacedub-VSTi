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
//   6.   Per-tap feedback at 100 % is the DXi.
//   7.   Free-running, the taps are the DXi's: n x the per-tap delay.
//   8.   Grid taps: synced, tap 5 on N grid units exactly, taps 4..1 halving
//        down from the largest power of two below N.
//   9.   Filter Taps: off is the existing path; on filters taps 1-4 and
//        leaves tap 5 and the line alone.
//   10.  Filter Mode: low-pass and high-pass responses, 4-pole slope,
//        Filter Taps following the mode, the worst case bounded.
//   11.  Denormals: a dying echo goes to exactly zero, never through
//        denormals, on both feedback paths and with the filter off.
//   12.  ScopedFlushDenormals: flush-to-zero inside, the old mode after.
//   13.  Output gains ramped: Feedback Out and Taps Gain moved across a
//        block, the way processAudio now does it, leave no step.
//   14.  Tempo moves without clicks (branch tempo-glide): a host ramp, a
//        small and a big tempo step and a grid step of the delay control,
//        on a sine - no sample-to-sample kink above the static render's,
//        and no swell.
//   15.  Small moves glide and jumps crossfade; the glide stays under its
//        speed limit; a crossfade lasts kFadeSeconds.
//   16.  Once a move settles the heads are on whole samples again: an
//        impulse comes back as single, unsmeared samples on the targets.
//   17.  Denormals: the gliding and crossfading paths, with both feedback
//        paths on, never write a subnormal either.
//------------------------------------------------------------------------

#include "DelayLine.h"
#include "DenormalGuard.h"
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <vector>
#include <cstdint>
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

    // 6. Per-tap feedback (branch Polivoks). Render an impulse with taps
    //    feedback on and compare: every tap at 1 must be bit-identical to
    //    the DXi's single amount; every tap at 0 identical to taps feedback
    //    off; and one tap turned up must change the result.
    std::printf("6. per-tap feedback\n");
    auto render = [](bool loops, const float* g) {
        DelayLine d; d.setSampleRate(48000);
        d.setDelay(0.05f); d.setScale(0.6f); d.setLoopsFeedback(loops);
        if (g) for (int t = 1; t <= DelayLine::kNumTaps; ++t) d.setTapFeedback(t, g[t - 1]);
        std::vector<float> out;
        float taps[DelayLine::kTapArray] = {};
        for (int i = 0; i < 4000; ++i) {
            d.process(i == 0 ? 1.f : 0.f, taps);
            for (int t = 1; t <= DelayLine::kNumTaps; ++t) out.push_back(taps[t]);
        }
        return out;
    };
    const float ones[5] = {1, 1, 1, 1, 1}, zeros[5] = {0, 0, 0, 0, 0}, louder[5] = {1, 1, 2, 1, 1};
    chk(render(true, ones) == render(true, nullptr), "per-tap feedback at 100 % changed the sound");
    chk(render(true, zeros) == render(false, nullptr), "per-tap feedback at 0 % is not taps feedback off");
    chk(render(true, louder) != render(true, ones), "turning one tap up changed nothing");
    std::printf("   100 %% = the DXi, 0 %% = off, one tap up = different\n");

    // 7. Free-running, the tap positions are the DXi's: n x the per-tap delay,
    //    at every setting and every realistic sample rate.
    std::printf("7. free-running taps are evenly spaced, as the DXi's\n");
    {
        bool even = true;
        for (int sr : {44100, 48000, 88200, 96000, 192000})
            for (int i = 0; i <= 1000; ++i) {
                const float dv = i / 1000.0f;
                const long len = (long) sr * DelayLine::kLineSeconds;
                long taps[DelayLine::kTapArray] = {};
                int steps = -1;
                const long last = DelayLine::computeTapSamples(dv, len, 0, taps, &steps);
                const long per = DelayLine::computeDelaySamples(dv, len, 0);
                for (int n = 1; n <= DelayLine::kNumTaps; ++n) even = even && taps[n] == n * per;
                even = even && last == taps[5] && steps == 0;
            }
        chk(even, "free-running taps moved off n x per-tap");
        std::printf("   5 rates x 1001 settings: all n x per-tap\n");
    }

    // 8. Grid taps. Synced, with the echo at N grid units U, tap 5 sits on
    //    N x U exactly and taps 4..1 halve down from the largest power of two
    //    strictly below N. The example that asked for it: N = 3 -> 1/4, 1/2,
    //    1, 2, 3 U.
    std::printf("8. grid taps when synced\n");
    {
        auto expect = [](long n, long u, long* e) {
            double p = 0.5;
            while (p * 2.0 < (double) n) p *= 2.0;
            e[5] = n * u;
            for (int t = 4; t >= 1; --t) { e[t] = std::max(1L, std::lround(p * u)); p *= 0.5; }
        };
        // the worked examples, in units of U = 4800 samples (an eighth at 150 bpm, 48 kHz)
        struct Ex { long n; double u[5]; };
        const Ex ex[] = { {1, {1/16., 1/8., 1/4., 1/2., 1}}, {2, {1/8., 1/4., 1/2., 1, 2}},
                          {3, {1/4., 1/2., 1, 2, 3}},        {4, {1/4., 1/2., 1, 2, 4}},
                          {5, {1/2., 1, 2, 4, 5}},           {8, {1/2., 1, 2, 4, 8}} };
        const int  u   = 4800;
        const long len = 48000L * DelayLine::kLineSeconds;
        for (const Ex& e : ex) {
            // a control value that lands on n grid units: n x U / 5 per tap
            const float dv = (float) ((e.n * (u / 5) + 0.5) / (double) (len / 5));
            long taps[DelayLine::kTapArray] = {};
            int steps = 0;
            DelayLine::computeTapSamples(dv, len, u, taps, &steps);
            std::printf("   N = %ld:", e.n);
            for (int t = 1; t <= 5; ++t) std::printf(" %gU", taps[t] / (double) u);
            std::printf("\n");
            chk(steps == e.n, "worked example did not land on its step");
            for (int t = 1; t <= 5; ++t)
                chk(taps[t] == std::lround(e.u[t - 1] * u), "worked example tap is off the grid");
        }

        // sweep: every sync rate, tempo, sample rate and setting
        long checked = 0;
        bool ok = true, inc = true, fits = true;
        for (int sr : {44100, 48000, 96000})
            for (int div : {32, 16, 8, 4, 2, 1})
                for (double bpm : {60.0, 90.0, 120.0, 140.0, 174.0})
                    for (int i = 0; i <= 400; ++i) {
                        const float dv = i / 400.0f;
                        const long len = (long) sr * DelayLine::kLineSeconds;
                        const int grid = samplesPer32For(div, bpm, sr);
                        long taps[DelayLine::kTapArray] = {}, e[DelayLine::kTapArray] = {};
                        int steps = 0;
                        DelayLine::computeTapSamples(dv, len, grid, taps, &steps);
                        if (steps == 0) continue;
                        ++checked;
                        expect(steps, grid, e);
                        for (int t = 1; t <= 5; ++t) ok = ok && taps[t] == e[t];
                        for (int t = 2; t <= 5; ++t) inc = inc && taps[t] > taps[t - 1];
                        fits = fits && taps[5] <= (len / 5) * 5 && taps[1] >= 1;
                    }
        std::printf("   %ld synced settings checked\n", checked);
        chk(checked > 1000, "the sweep checked almost nothing");
        chk(ok, "a synced tap is not on the grid");
        chk(inc, "synced taps are not in order");
        chk(fits, "a synced tap falls outside the line");

        // through a live DelayLine: an impulse comes out of tap t after
        // exactly getTapSamples(t) samples
        DelayLine d; d.setSampleRate(48000); d.setSamplesPer32(u);
        const float dv = (float) ((3 * (u / 5) + 0.5) / (double) (len / 5));
        d.setDelay(dv);
        long first[DelayLine::kTapArray] = {-1, -1, -1, -1, -1, -1};
        float taps[DelayLine::kTapArray] = {};
        for (long i = 0; i < 4 * u; ++i) {
            d.process(i == 0 ? 1.f : 0.f, taps);
            for (int t = 1; t <= 5; ++t) if (first[t] < 0 && taps[t] != 0.f) first[t] = i;
        }
        std::printf("   live, N = 3: impulse out of the taps at");
        for (int t = 1; t <= 5; ++t) std::printf(" %ld", first[t]);
        std::printf("\n");
        for (int t = 1; t <= 5; ++t)
            chk(first[t] == d.getTapSamples(t), "live tap does not come out where computeTapSamples says");
        chk(first[3] == u && first[5] == 3 * u, "live taps not at 1U and 3U");
    }

    // 9. Filter Taps. Off is the existing path; on, taps 1-4 go through
    //    the filter and nothing else changes - tap 5 and every feedback path
    //    read the line, which Filter Taps never touches.
    std::printf("9. filter taps\n");
    {
        // tap t's output over a render with both feedback paths on
        auto run = [](bool filter, int filterTaps /* -1: never set */) {
            DelayLine d; d.setSampleRate(48000);
            d.setDelay(0.1f); d.setScale(0.6f); d.setLoopsFeedback(true);
            d.setMainFeedback(true); d.setMainFeedbackValue(0.5f);
            d.setUseFilter(filter); d.setFrequency(0.3); d.setResonance(1.5);
            if (filterTaps >= 0) d.setFilterTaps(filterTaps != 0);
            std::vector<std::vector<float>> out(DelayLine::kTapArray);
            float taps[DelayLine::kTapArray] = {};
            for (int i = 0; i < 60000; ++i) {
                const float in = (i % 7919) < 40 ? 0.5f * (float) ((i * 31) % 17 - 8) / 8.f : 0.f;
                d.process(in, taps);
                for (int t = 1; t <= DelayLine::kNumTaps; ++t) out[t].push_back(taps[t]);
            }
            return out;
        };
        const auto base = run(true, -1), off = run(true, 0), on = run(true, 1);
        chk(off == base, "Filter Taps off is not the existing path");
        chk(run(false, 1) == run(false, -1), "Filter Taps did something with the filter off");
        chk(on[5] == base[5], "Filter Taps changed tap 5 (or the line)");
        bool allChanged = true;
        for (int t = 1; t < DelayLine::kNumTaps; ++t) allChanged = allChanged && on[t] != base[t];
        chk(allChanged, "Filter Taps left a tap 1-4 unfiltered");

        // and it IS the filter: white noise in, cutoff at 900 Hz, and tap 2's
        // first-difference energy (the top end) against its plain energy
        auto noiseTap = [](bool filterTaps) {
            DelayLine d; d.setSampleRate(48000); d.setDelay(0.1f);
            d.setUseFilter(true); d.setFrequency(0.3); d.setResonance(0.5); d.setFilterTaps(filterTaps);
            std::vector<float> x; float taps[DelayLine::kTapArray] = {};
            std::uint32_t r = 22222;
            for (int i = 0; i < 96000; ++i) {
                r = r * 1664525u + 1013904223u;
                d.process((float) ((r >> 8) / 16777216.0 - 0.5), taps);
                if (i >= 48000) x.push_back(taps[2]);
            }
            return x;
        };
        auto hf = [](const std::vector<float>& x) {
            double e = 0.0, a = 0.0;
            for (size_t i = 1; i < x.size(); ++i) { e += (x[i] - x[i-1]) * (x[i] - x[i-1]); a += x[i] * x[i]; }
            return a > 0.0 ? e / a : 0.0;
        };
        const double plain = hf(noiseTap(false)), filt = hf(noiseTap(true));
        std::printf("   noise through tap 2, top-end ratio: unfiltered %.4f, filtered %.6f\n", plain, filt);
        chk(filt < 0.01 * plain, "filtered tap 2 kept its top end");
        std::printf("   off = existing path, tap 5 and the line untouched, taps 1-4 filtered\n");
    }

    // 10. Filter Mode. Low-pass is the DXi's ladder untouched (check 6's
    //     and 9's renders run with it); high-pass is mixed from the same
    //     poles. Measure sine gains through tap 5 - and tap 2 with Filter
    //     Taps - with the cutoff at 2.5 kHz.
    std::printf("10. filter mode: low-pass / high-pass\n");
    {
        auto gainDb = [](bool hp, double res, double hz, int tap, bool filterTaps) {
            DelayLine d; d.setSampleRate(48000); d.setDelay(0.01f);
            d.setUseFilter(true); d.setHighPass(hp); d.setFilterTaps(filterTaps);
            d.setFrequency(0.5); d.setResonance(res); d.setFilterGain(1.f); d.setMainOutGain(1.f);
            float taps[DelayLine::kTapArray] = {};
            double peak = 0.0;
            for (int i = 0; i < 96000; ++i) {
                d.process((float) (0.1 * std::sin(2.0 * 3.14159265358979 * hz * i / 48000.0)), taps);
                if (i > 48000) peak = std::max(peak, (double) std::fabs(taps[tap]));
            }
            return 20.0 * std::log10(peak / 0.1);
        };
        std::printf("       Hz    LP dB    HP dB\n");
        for (double hz : {50.0, 625.0, 2500.0, 10000.0})
            std::printf("   %6.0f  %7.1f  %7.1f\n", hz, gainDb(false, 0.0, hz, 5, false), gainDb(true, 0.0, hz, 5, false));
        chk(gainDb(true, 0.0, 50.0, 5, false) < -100.0, "high-pass lets 50 Hz through");
        chk(gainDb(true, 0.0, 10000.0, 5, false) > -4.0, "high-pass cuts 10 kHz");
        chk(gainDb(false, 0.0, 50.0, 5, false) > -0.5, "low-pass cuts 50 Hz");
        chk(gainDb(false, 0.0, 10000.0, 5, false) < -50.0, "low-pass lets 10 kHz through");
        // 24 dB an octave, two octaves below the corner
        const double slope = gainDb(true, 0.0, 625.0, 5, false) - gainDb(true, 0.0, 312.5, 5, false);
        std::printf("   high-pass slope 312 -> 625 Hz: %.1f dB\n", slope);
        chk(slope > 21.0 && slope < 27.0, "high-pass is not 4-pole");
        // Filter Taps follows the mode
        chk(gainDb(true, 0.0, 50.0, 2, true) < -100.0, "Filter Taps did not high-pass tap 2");
        chk(gainDb(true, 0.0, 50.0, 2, false) > -0.5, "tap 2 was filtered without Filter Taps");

        // the worst case stays bounded: resonance 3, 500 % gain, 200 % feedback
        DelayLine d; d.setSampleRate(48000); d.setDelay(0.02f);
        d.setUseFilter(true); d.setHighPass(true); d.setFilterTaps(true); d.setFrequency(0.7);
        d.setResonance(3.0); d.setFilterGain(3.f); d.setMainFeedback(true); d.setMainFeedbackValue(2.f);
        d.setLoopsFeedback(true); d.setScale(1.f);
        float taps[DelayLine::kTapArray] = {};
        double worst = 0.0; bool finite = true;
        std::uint32_t r = 7;
        for (int i = 0; i < 240000; ++i) {
            r = r * 1664525u + 1013904223u;
            d.process((float) ((r >> 8) / 16777216.0 - 0.5), taps);
            for (int t = 1; t <= 5; ++t) { finite = finite && std::isfinite(taps[t]); worst = std::max(worst, (double) std::fabs(taps[t])); }
        }
        std::printf("   high-pass worst case, 5 s: peak %.1f\n", worst);
        chk(finite && worst <= 16.0 * 3.0 * 2.0 + 1e-3, "high-pass runs away");
    }

    // 11. Denormals. With the filter off nothing scrubs the line, so before
    //     the floor in DelayLine a decaying echo spent hundreds of passes
    //     round the loop as denormals (slow - see DenormalGuard.h). Now it
    //     must reach exactly zero and never be subnormal on the way.
    std::printf("11. denormals: echo tails go to zero\n");
    {
        auto isSub = [](float v) { return std::fpclassify(v) == FP_SUBNORMAL; };
        auto tail = [&](float mainFb, float loopScale, bool filter, const char* what) {
            DelayLine d; d.setSampleRate(48000); d.setDelay(0.05f);  // 960-sample loop
            d.setTapsOutGain(1.f); d.setMainOutGain(1.f); d.setFilterGain(1.f);
            d.setMainFeedback(mainFb > 0.f); d.setMainFeedbackValue(mainFb);
            d.setLoopsFeedback(loopScale > 0.f); d.setScale(loopScale);
            d.setUseFilter(filter); d.setFrequency(0.6); d.setResonance(0.5);
            float taps[DelayLine::kTapArray] = {};
            long subs = 0, lastNonZero = -1;
            const long n = 48000L * 40;
            for (long i = 0; i < n; ++i) {
                // a burst, then silence - with a denormal input mixed in, which
                // a host may hand us and which must not get into the line
                const float in = i < 480 ? (float) std::sin(i * 0.1) : (i == 1000 ? 1.0e-40f : 0.f);
                d.process(in, taps);
                for (int t = 1; t <= 5; ++t) {
                    if (isSub(taps[t])) ++subs;
                    if (taps[t] != 0.f) lastNonZero = i;
                }
            }
            std::printf("   %-34s subnormal tap samples %ld, silent from %.1f s\n",
                        what, subs, (lastNonZero + 1) / 48000.0);
            chk(subs == 0, what);
            chk(lastNonZero < n - 48000, "echo tail never reached zero");
        };
        tail(0.9f, 0.f,  false, "main feedback, filter off");
        tail(0.f,  0.6f, false, "taps feedback, filter off");
        tail(0.5f, 0.1f, false, "both feedback paths, filter off");
        tail(0.9f, 0.f,  true,  "main feedback, filter on");
    }

    // 12. The guard process() runs under: flush-to-zero while it lives, the
    //     host's mode restored when it goes.
    std::printf("12. ScopedFlushDenormals\n");
    if (ScopedFlushDenormals::supported ()) {
        // Every value through memory (volatile): the guard orders memory
        // accesses round its mode switch, not register arithmetic.
        volatile float tiny = 1.0e-37f, half = 0.01f;
        volatile float before = tiny * half;            // denormal: 1e-39
        volatile float inside = 1.f;
        { ScopedFlushDenormals guard; inside = tiny * half; }
        volatile float after = tiny * half;
        std::printf("   1e-37 x 0.01: before %g, inside %g, after %g\n", (double) before, (double) inside, (double) after);
        chk(before != 0.f, "denormals already flushed before the guard (test is meaningless)");
        chk(inside == 0.f, "guard did not flush to zero");
        chk(after == before, "guard did not restore the FPU mode");
    } else {
        std::printf("   not supported on this architecture - skipped\n");
    }

    // 13. Feedback Out (tap 5, filtered) and Taps Gain (taps 1-4) are output
    //     levels. Set once per block, a move jumped at the block boundary - the
    //     click a stress test found on Feedback Out. processAudio now ramps
    //     them per sample over the block like Dry Level; this renders the
    //     same ramp (start + n x (target - start) / block) against the old
    //     step, on a DC input so the steady output does not move at all.
    std::printf("13. output gains ramped\n");
    {
        const int block = 512, edge = 4096, n = edge + 2 * block;
        // the largest sample-to-sample move of tap t from edge - 8 on
        auto worst = [&](bool feedbackOut, bool ramp, float& level) {
            DelayLine d; d.setSampleRate(48000);
            d.setDelay(0.05f);                       // tap 5 at 960 samples
            d.setUseFilter(true); d.setFrequency(0.5); d.setResonance(0.5);
            const int t = feedbackOut ? 5 : 1;
            const float from = 2.f, to = 0.f;
            float taps[DelayLine::kTapArray] = {}, prev = 0.f, big = 0.f;
            for (int i = 0; i < n; ++i) {
                float g = from;
                if (i >= edge)
                    g = (ramp && i < edge + block) ? from + (to - from) * (float) (i - edge) / block : to;
                if (feedbackOut) d.setMainOutGain(g); else d.setTapsOutGain(g);
                d.process(0.25f, taps);
                if (i == edge - 1) level = std::fabs(taps[t]);
                if (i >= edge - 8) big = std::max(big, std::fabs(taps[t] - prev));
                prev = taps[t];
            }
            return big;
        };
        for (int which = 0; which < 2; ++which) {
            const bool fbOut = which == 0;
            float level = 0.f, dummy = 0.f;
            const float stepped = worst(fbOut, false, level);
            const float ramped  = worst(fbOut, true, dummy);
            std::printf("   %-13s 200 %% -> 0 %%: steady %.3f, step %.4f, ramped %.6f per sample\n",
                        fbOut ? "Feedback Out" : "Taps Gain", level, stepped, ramped);
            chk(level > 0.1f, "no steady signal on the tap (test is meaningless)");
            chk(stepped > 0.9f * level, "the old step did not jump (test is meaningless)");
            chk(ramped < 1.5f * level / block + 1e-4f, "the ramped gain still steps");
        }
    }

    // The tempo-glide checks drive a line the way processAudio does: the sync
    // grid set once per block from the tempo, setDelay and process per sample.
    const int gsr = 48000, gblock = 512;
    auto tempoRun = [&](DelayLine& d, int div, long n, auto tempoAt, auto delayAt,
                        auto inputAt, auto perSample) {
        float taps[DelayLine::kTapArray] = {};
        for (long i = 0; i < n; ++i) {
            if (i % gblock == 0)
                d.setSamplesPer32(samplesPer32For(div, tempoAt(i), gsr));
            d.setDelay((float) delayAt(i));
            d.process(inputAt(i), taps);
            perSample(i, taps);
        }
    };
    auto sine = [&](long i) { return 0.5f * (float) std::sin(2.0 * 3.141592653589793 * 220.0 * i / gsr); };

    // 14. A sine through the line is a sine on every tap, so its second
    //     difference is bounded by A (2 pi f / sr)^2 - unless a tap jumps.
    //     Before this branch a tempo step put a spike of 0.45 on a 0.5 sine.
    std::printf("14. tempo moves without clicks\n");
    {
        struct Case { const char* name; double (*tempo)(long); float (*delay)(long); };
        static const Case cases[] = {
            { "static",                 [](long) { return 120.0; }, [](long) { return 0.35f; } },
            { "ramp 120-140 / 4 bars",  [](long i) { return 120.0 + 20.0 * std::clamp((i - 3.0 * 48000) / (8.0 * 48000), 0.0, 1.0); }, [](long) { return 0.35f; } },
            { "ramp 120-170 / 2 s",     [](long i) { return 120.0 + 50.0 * std::clamp((i - 3.0 * 48000) / (2.0 * 48000), 0.0, 1.0); }, [](long) { return 0.35f; } },
            { "step 120 -> 121",        [](long i) { return i < 4 * 48000 ? 120.0 : 121.0; }, [](long) { return 0.35f; } },
            { "step 120 -> 90",         [](long i) { return i < 4 * 48000 ? 120.0 : 90.0; },  [](long) { return 0.35f; } },
            { "delay grid step",        [](long) { return 120.0; }, [](long i) { return i < 4 * 48000 ? 0.35f : 0.43f; } },
        };
        double baseline = 0.0;
        for (const Case& c : cases) {
            DelayLine d; d.setSampleRate(gsr);
            double prev[DelayLine::kTapArray][2] = {}, kink = 0.0, peak = 0.0;
            long moved = 0;
            tempoRun(d, 16, 12L * gsr, c.tempo, c.delay, sine, [&](long i, const float* t) {
                for (int k = 1; k <= DelayLine::kNumTaps; ++k) {
                    if (i > 2L * gsr) {          // every tap has signal by now
                        kink = std::max(kink, std::fabs(t[k] - 2.0 * prev[k][0] + prev[k][1]));
                        peak = std::max(peak, (double) std::fabs(t[k]));
                    }
                    prev[k][1] = prev[k][0]; prev[k][0] = t[k];
                }
                if (i == 2L * gsr) moved = d.getTapSamples(DelayLine::kNumTaps);   // before any move
            });
            const long end = d.getTapSamples(DelayLine::kNumTaps);
            if (baseline == 0.0) baseline = kink;
            std::printf("   %-22s tap 5 %6ld -> %6ld  kink %.5f (static %.5f)  peak %.4f\n",
                        c.name, moved, end, kink, baseline, peak);
            if (&c != &cases[0])
                chk(end != moved, "the targets did not move (test is meaningless)");
            chk(kink <= 1.05 * baseline, "a tap still jumps when the tempo or the delay moves");
            chk(peak <= 0.5001, "the move swells the level");
            chk(!d.isCrossfading() && d.getTapPosition(DelayLine::kNumTaps) == (double) end,
                "the heads did not settle on the targets");
        }
    }

    // 15. Which way the heads move, and how fast. A gentle ramp on a short
    //     synced delay must glide all the way - no crossfade - with no head
    //     moving faster than kGlideMaxRate; a tempo step must crossfade, for
    //     kFadeSeconds.
    std::printf("15. glide for ramps, crossfade for jumps\n");
    {
        DelayLine d; d.setSampleRate(gsr);
        long fades = 0; double fastest = 0.0, last[DelayLine::kTapArray] = {};
        tempoRun(d, 16, 12L * gsr,
                 [](long i) { return 120.0 + 20.0 * std::clamp((i - 2.0 * 48000) / (8.0 * 48000), 0.0, 1.0); },
                 [](long) { return 0.05f; }, sine, [&](long i, const float*) {
            fades += d.isCrossfading() ? 1 : 0;
            for (int k = 1; k <= DelayLine::kNumTaps; ++k) {
                if (i > 0) fastest = std::max(fastest, std::fabs(d.getTapPosition(k) - last[k]));
                last[k] = d.getTapPosition(k);
            }
        });
        std::printf("   ramp: %ld crossfading samples, fastest head %.4f samples/sample (limit %.2f)\n",
                    fades, fastest, DelayLine::kGlideMaxRate);
        chk(fades == 0, "a gentle tempo ramp crossfaded instead of gliding");
        chk(fastest > 0.0 && fastest <= DelayLine::kGlideMaxRate + 1e-12, "a gliding head broke its speed limit");

        DelayLine e; e.setSampleRate(gsr);
        long first = -1, lastFade = -1;
        tempoRun(e, 16, 3L * gsr, [](long i) { return i < 48000 ? 120.0 : 90.0; },
                 [](long) { return 0.05f; }, sine, [&](long i, const float*) {
            if (e.isCrossfading()) { if (first < 0) first = i; lastFade = i; }
        });
        const long len = lastFade - first + 1, want = std::lround(DelayLine::kFadeSeconds * gsr);
        std::printf("   step: crossfade from sample %ld for %ld samples (want %ld)\n", first, len, want);
        const long blockStart = ((gsr + gblock - 1) / gblock) * gblock;    // the first block at 90
        chk(first == blockStart, "a tempo step did not start a crossfade on the block it arrived in");
        chk(std::labs(len - want) <= 1, "the crossfade is not kFadeSeconds long");
    }

    // 16. Once settled, nothing is interpolated: after a tempo ramp the line
    //     is cleared and an impulse sent through - each tap must give back one
    //     sample of exactly 1.0 at exactly its target, and nothing else.
    std::printf("16. settled heads read whole samples\n");
    {
        DelayLine d; d.setSampleRate(gsr);
        const long settle = 12L * gsr;
        tempoRun(d, 16, settle,
                 [](long i) { return 120.0 + 7.0 * std::clamp((i - 1.0 * 48000) / (4.0 * 48000), 0.0, 1.0); },
                 [](long) { return 0.05f; }, sine, [](long, const float*) {});
        // Clear the line but keep the heads where they settled: reset() would
        // snap them anyway, so silence flushes it instead.
        tempoRun(d, 16, 9L * gsr, [](long) { return 127.0; }, [](long) { return 0.05f; },
                 [](long) { return 0.f; }, [](long, const float*) {});
        long hits[DelayLine::kTapArray] = {}, strays = 0; bool exact = true;
        tempoRun(d, 16, (long) d.getTapSamples(DelayLine::kNumTaps) + 10, [](long) { return 127.0; },
                 [](long) { return 0.05f; }, [](long i) { return i == 0 ? 1.f : 0.f; },
                 [&](long i, const float* t) {
            for (int k = 1; k <= DelayLine::kNumTaps; ++k) {
                if (t[k] == 0.f) continue;
                if (i == d.getTapSamples(k)) { ++hits[k]; exact = exact && t[k] == 1.f; }
                else ++strays;
            }
        });
        bool all = true;
        for (int k = 1; k <= DelayLine::kNumTaps; ++k) all = all && hits[k] == 1;
        std::printf("   taps at %ld %ld %ld %ld %ld: %s, %ld stray samples\n",
                    d.getTapSamples(1), d.getTapSamples(2), d.getTapSamples(3), d.getTapSamples(4),
                    d.getTapSamples(5), all && exact ? "one exact sample each" : "SMEARED", strays);
        chk(all && exact && strays == 0, "a settled head still interpolates");
    }

    // 17. Check 11's floor, on the moving paths: a burst, then silence while
    //     the tempo ramps (glide) and steps (crossfade), both feedback paths
    //     on, filter off - no subnormal on any tap, and the tail reaches zero.
    std::printf("17. no denormals while the heads move\n");
    {
        DelayLine d; d.setSampleRate(gsr);
        d.setLoopsFeedback(true); d.setMainFeedback(true);
        d.setScale(0.1f); d.setMainFeedbackValue(0.5f);     // check 11's "both feedback paths"
        long subnormal = 0; float lastTap = 1.f;
        // ramp for 10 s, step every 2 s until 30 s, then hold while it dies
        // (about 160 s at these settings, moving or not)
        tempoRun(d, 16, 180L * gsr,
                 [](long i) { const double s = i / 48000.0;
                              return s < 10 ? 120.0 + 2.0 * s
                                   : s < 30 ? (std::fmod(s, 4.0) < 2.0 ? 100.0 : 150.0) : 120.0; },
                 [](long) { return 0.05f; },
                 [](long i) { return i < 4800 ? 0.5f * (float) std::sin(i * 0.05) : 0.f; },
                 [&](long, const float* t) {
            for (int k = 1; k <= DelayLine::kNumTaps; ++k)
                if (t[k] != 0.f && std::fpclassify(t[k]) == FP_SUBNORMAL) ++subnormal;
            lastTap = std::fabs(t[DelayLine::kNumTaps]);
        });
        std::printf("   %ld subnormal tap samples, last tap 5 %g\n", subnormal, lastTap);
        chk(subnormal == 0, "a moving head produced a subnormal");
        chk(lastTap == 0.f, "the tail did not reach exactly zero");
    }

    std::printf(fails ? "\n%d CHECK(S) FAILED\n" : "\nall checks passed\n", fails);
    return fails != 0;
}
