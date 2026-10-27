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
//------------------------------------------------------------------------

#include "DelayLine.h"
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

    std::printf(fails ? "\n%d CHECK(S) FAILED\n" : "\nall checks passed\n", fails);
    return fails != 0;
}
