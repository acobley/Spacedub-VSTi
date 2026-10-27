//------------------------------------------------------------------------
// SpaceDub - DelayLine implementation (port of the DXi DelayLine.cpp)
//------------------------------------------------------------------------

#include "DelayLine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace SpaceDub {

namespace {
/** Far above any musical level (~+24 dBFS).  Everything below this is
    bit-for-bit what the DXi produced; above it the ladder filter and the
    feedback path are simply prevented from running away to infinity. */
constexpr double kCeiling = 16.0;

/** Kill denormals and bound the recursive filter state.  Resonance can be
    driven to 3.0, which puts this filter topology well past its stability
    limit - the original just let the numbers explode. */
inline double scrub (double v)
{
	if (!std::isfinite (v))
		return 0.0;
	if (v > -1.0e-15 && v < 1.0e-15)
		return 0.0;
	return std::clamp (v, -kCeiling, kCeiling);
}
} // anonymous namespace

//------------------------------------------------------------------------
DelayLine::DelayLine ()
{
	buildNoiseTable ();
	setSampleRate (44100);
}

//------------------------------------------------------------------------
void DelayLine::buildNoiseTable ()
{
	// The original produced its dither by punning an LCG's bits into the
	// mantissa of a float.  Reproduced here with memcpy so it is well
	// defined, and with an explicit 32-bit state so the result matches the
	// original 32-bit build.
	mRandState = 1;
	for (int i = 0; i < kNoiseLen; ++i)
	{
		mRandState = mRandState * 1234567u + 890123u;
		std::uint32_t mantissa = mRandState & 0x807f0000u;
		std::uint32_t fltRnd   = mantissa | 0x1E000000u;
		float f = 0.f;
		std::memcpy (&f, &fltRnd, sizeof (f));
		mNoise[i] = std::isfinite (f) ? f : 0.f;
	}
	mNoisePos = 0;
}

//------------------------------------------------------------------------
void DelayLine::setSampleRate (int sampleRate)
{
	if (sampleRate < 8000)
		sampleRate = 8000;

	const long newLength = static_cast<long> (sampleRate) * kLineSeconds;
	if (sampleRate == mSampleRate && newLength == mLength && !mLine.empty ())
		return;

	mSampleRate = sampleRate;
	mLength     = newLength;
	// NOTE: the original used  memset(fLine, 0, sizeof(fLine)*nLength)  where
	// sizeof(fLine) is the size of a *pointer*.  That happened to be correct
	// on 32-bit Windows and overruns the buffer on 64-bit builds; assign()
	// is the correct equivalent.
	mLine.assign (static_cast<size_t> (mLength), 0.f);
	reset ();
}

//------------------------------------------------------------------------
void DelayLine::reset ()
{
	std::fill (mLine.begin (), mLine.end (), 0.f);
	mAddPos    = 0;
	mRemovePos = 1;
	mLastValid = 0;
	for (Ladder& l : mLadder)
		l = Ladder ();
	mNoisePos = 0;
}

//------------------------------------------------------------------------
void DelayLine::process (float input, float* taps)
{
	if (mLine.empty () || taps == nullptr)
		return;

	float* line = mLine.data ();
	line[mAddPos] = input;

	long removePos    = mAddPos;
	long oldRemovePos = mAddPos;
	float raw[kTapArray] = {};                   // each tap as read, for Filter Taps

	for (int ix = 1; ix <= kNumTaps; ++ix)
	{
		// Each tap's own delay: n x the per-tap delay free-running (the DXi),
		// or on the sync grid when synced - see computeTapSamples().
		removePos = mAddPos - mTapSamples[ix];
		if (removePos < 0)
			removePos = (mLength - 1) + removePos;
		if (removePos < 0)                       // safety net; see notes
			removePos = 0;
		else if (removePos >= mLength)
			removePos = mLength - 1;

		raw[ix]  = line[removePos];
		taps[ix] = raw[ix] * mTapsOutGain;

		if (mLoopsFeedback)
		{
			// Tap-to-tap regeneration.  Bounded for the same reason as the
			// main feedback path below - unbounded it grows exponentially.
			// mTapFeedback is per tap (branch Polivoks); at its default of 1
			// the product is exactly the DXi's mScale.
			const float v = line[oldRemovePos] + (mScale * mTapFeedback[ix]) * line[removePos];
			line[oldRemovePos] = std::isfinite (v)
			                         ? std::clamp (v, static_cast<float> (-kCeiling),
			                                       static_cast<float> (kCeiling))
			                         : 0.f;
			oldRemovePos = removePos;
		}
	}

	// The filtered signal is taken from the *last* tap, matching the
	// original (fEqSample = fLine[nRemovePos] after the loop).
	double eqSample = static_cast<double> (line[removePos]);

	if (mUseFilter)
	{
		const float noise = mNoise[mNoisePos];
		eqSample += noise;
		if (++mNoisePos >= kNoiseLen)
			mNoisePos = 0;

		eqSample = runLadder (mLadder[kNumTaps], eqSample) * mFilterGain;
		// The filtered tap replaces tap 5 and uses the *main* out gain.
		taps[kNumTaps] = static_cast<float> (eqSample * mMainOutGain);

		// Filter Taps: taps 1..4 through ladders of their own, with the same
		// dither sample, filter gain, and their own (taps out) gain. The
		// line - and so every feedback path - is untouched.
		if (mFilterTaps)
		{
			for (int ix = 1; ix < kNumTaps; ++ix)
			{
				const double y = runLadder (mLadder[ix], static_cast<double> (raw[ix]) + noise) * mFilterGain;
				taps[ix] = static_cast<float> (y * mTapsOutGain);
			}
		}
	}

	if (mMainFeedback)
		line[mAddPos] = line[mAddPos] + static_cast<float> (mMainFeedbackValue * eqSample);

	// Safety net the DXi did not have: 200 % feedback combined with a
	// resonant filter at 500 % gain is a genuine runaway.
	if (!std::isfinite (line[mAddPos]))
		line[mAddPos] = 0.f;
	else
		line[mAddPos] = std::clamp (line[mAddPos],
		                            static_cast<float> (-kCeiling),
		                            static_cast<float> (kCeiling));

	mLastValid = mAddPos;
	if (++mAddPos >= mLength)
		mAddPos = 0;
}

//------------------------------------------------------------------------
double DelayLine::runLadder (Ladder& s, double x) const
{
	// "Moog VCF, variation 2" (musicdsp.org) - unchanged from the DXi.
	const double f  = mFrequency * 1.16;
	const double fb = mResonance * (1.0 - 0.15 * f * f);

	x -= s.out4 * fb;
	const double input = x;                   // after the resonance, before the scaling
	const double scale = 0.35013 * (f * f) * (f * f);
	x *= scale;

	s.out1 = scrub (x + 0.3 * s.in1 + (1.0 - f) * s.out1);       // Pole 1
	s.in1  = x;
	s.out2 = scrub (s.out1 + 0.3 * s.in2 + (1.0 - f) * s.out2);  // Pole 2
	s.in2  = s.out1;
	s.out3 = scrub (s.out2 + 0.3 * s.in3 + (1.0 - f) * s.out3);  // Pole 3
	s.in3  = s.out2;
	s.out4 = scrub (s.out3 + 0.3 * s.in4 + (1.0 - f) * s.out4);  // Pole 4
	s.in4  = s.out3;

	if (!mHighPass || !(scale > 0.0))
		return s.out4;

	// HIGH-PASS, by mixing the poles (the Oberheim Xpander's trick). Each
	// pole is (1 + 0.3/z) / (1 - (1-f)/z), DC gain 1.3/f; call g = f/1.3.
	// Normalised to unity DC gain the k-th pole output is
	//     p_k = out_k * g^k / scale        (scale = 0.35013 f^4 ~ g^4)
	// and (1 - p)^4 is a 4-pole high-pass, because 1 - g(1 + 0.3/z) /
	// (1 - (1-f)/z) = (1 - g)(1 - 1/z) / (1 - (1-f)/z) - a zero at DC:
	//     hp = input - 4 p1 + 6 p2 - 4 p3 + p4
	// The resonance still comes from out4, so the peak sits at the cutoff.
	const double g  = f / 1.3;
	const double g2 = g * g;
	const double hp = input - (4.0 * s.out1 * g - 6.0 * s.out2 * g2 + 4.0 * s.out3 * g2 * g - s.out4 * g2 * g2) / scale;
	return scrub (hp);
}

//------------------------------------------------------------------------
float DelayLine::readTail ()
{
	if (mLine.empty ())
		return 0.f;
	const float res = mLine[mRemovePos];
	if (++mRemovePos >= mLength)
		mRemovePos = 0;
	return res;
}

//------------------------------------------------------------------------
float DelayLine::setDelay (float delay)
{
	if (mLength <= 0)
		return 0.f;

	const long last = computeTapSamples (delay, mLength, mSamplesPer32, mTapSamples, &mTempoQuant);
	mDelaySamples   = static_cast<int> (std::max (1L, last / kNumTaps));   // the average spacing

	return static_cast<float> (last) / static_cast<float> (mSampleRate);
}

//------------------------------------------------------------------------
long DelayLine::computeTapSamples (float delay, long lineLength, int samplesPer32,
                                   long* tapsOut, int* stepsOut)
{
	int steps = 0;
	const long perTap = computeDelaySamples (delay, lineLength, samplesPer32, &steps);

	// The DXi's spacing: five equal steps.
	tapsOut[0] = 0;
	for (int n = 1; n <= kNumTaps; ++n)
		tapsOut[n] = n * perTap;
	if (stepsOut)
		*stepsOut = steps;

	if (samplesPer32 <= 0 || steps < 1)
		return tapsOut[kNumTaps];

	// GRID TAPS. computeDelaySamples counts the echo in whole grid units
	// (`steps` of samplesPer32 / 5 per tap, i.e. `steps` of samplesPer32 for
	// the echo), but its integer samplesPer32 / 5 leaves the echo a few
	// samples short of steps x U. Put tap 5 on the grid exactly - as long as
	// that still fits in the line, which the integer division can tip over.
	const long unit     = samplesPer32;
	const long maxTotal = (lineLength / kNumTaps) * kNumTaps;
	long n = steps;
	while (n > 0 && n * unit > maxTotal)
		--n;
	if (n < 1)
		return tapsOut[kNumTaps];             // one grid step is longer than the line

	if (stepsOut)
		*stepsOut = static_cast<int> (n);
	tapsOut[kNumTaps] = n * unit;

	// The largest power of two strictly below n (1/2 when n is 1), then
	// halving for each tap before it.
	double p = 0.5;
	while (p * 2.0 < static_cast<double> (n))
		p *= 2.0;
	for (int t = kNumTaps - 1; t >= 1; --t)
	{
		tapsOut[t] = std::max (1L, std::lround (p * static_cast<double> (unit)));
		p *= 0.5;
	}
	return tapsOut[kNumTaps];
}

//------------------------------------------------------------------------
long DelayLine::computeDelaySamples (float delay, long lineLength, int samplesPer32, int* stepsOut)
{
	if (stepsOut)
		*stepsOut = 0;
	if (lineLength <= 0)
		return 1;

	delay = std::clamp (delay, 0.f, 1.f);

	// Free-running: the control is squared, giving finer resolution at short
	// delay times.  Tempo-synced: the control stays linear because it is
	// about to be quantised anyway.
	const float sDelay = (samplesPer32 == 0) ? delay * delay : delay;

	const long maxDelay = lineLength / kNumTaps;
	long nDelay = static_cast<long> (sDelay * static_cast<double> (maxDelay));

	if (samplesPer32 != 0)
	{
		long samplesPerTap = samplesPer32 / kNumTaps;
		if (samplesPerTap < 1)
			samplesPerTap = 1;
		const long steps = nDelay / samplesPerTap;
		if (stepsOut)
			*stepsOut = static_cast<int> (steps);
		nDelay = steps * samplesPerTap;
		if (nDelay < samplesPerTap)
		{
			// Too short to quantise - fall back to the free-running mapping.
			nDelay = static_cast<long> (delay * static_cast<double> (maxDelay));
		}
	}

	return std::clamp (nDelay, 1L, maxDelay);
}

//------------------------------------------------------------------------
double DelayLine::computeCutoffHz (double frequency, int sampleRate)
{
	if (sampleRate < 8000)
		sampleRate = 8000;                    // as setSampleRate() floors it

	// (0,1) maps to 0..SampleRate/4, rescaled so that 1.0 lands on 10 kHz.
	frequency = frequency * frequency;
	if (frequency < 0.001)
		frequency = 0.001;                    // protect the ladder filter

	// The ladder coefficient is a normalised frequency, so the cutoff cannot
	// exceed SampleRate/4.  At 44.1 kHz and above 10 kHz is comfortably below
	// that and this clamp never engages, so nothing the DXi did changes.
	// Below ~40 kHz it does: the original silently left the cutoff at its
	// previous value in that case (the assignment was inside the range test),
	// which froze the control and could not have been shown in a readout.
	// Clamping is the honest version of the same intent.
	const double quarterRate = sampleRate / 4.0;
	return std::min (10000.0 * frequency, quarterRate);
}

//------------------------------------------------------------------------
float DelayLine::setFrequency (double frequency)
{
	const double hz          = computeCutoffHz (frequency, mSampleRate);
	const double quarterRate = mSampleRate / 4.0;

	mFrequency = hz / quarterRate;            // 0..1, the ladder's coefficient
	return static_cast<float> (hz);
}

//------------------------------------------------------------------------
void DelayLine::setResonance (double resonance)
{
	if (resonance >= 0.0 && resonance < 4.0)
		mResonance = resonance;
}

//------------------------------------------------------------------------
} // namespace SpaceDub
