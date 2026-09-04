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
	mIn1 = mIn2 = mIn3 = mIn4 = 0.0;
	mOut1 = mOut2 = mOut3 = mOut4 = 0.0;
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

	for (int ix = 1; ix <= kNumTaps; ++ix)
	{
		removePos = mAddPos - static_cast<long> (ix) * mDelaySamples;
		if (removePos < 0)
			removePos = (mLength - 1) + removePos;
		if (removePos < 0)                       // safety net; see notes
			removePos = 0;
		else if (removePos >= mLength)
			removePos = mLength - 1;

		taps[ix] = line[removePos] * mTapsOutGain;

		if (mLoopsFeedback)
		{
			// Tap-to-tap regeneration.  Bounded for the same reason as the
			// main feedback path below - unbounded it grows exponentially.
			const float v = line[oldRemovePos] + mScale * line[removePos];
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
		eqSample += mNoise[mNoisePos];
		if (++mNoisePos >= kNoiseLen)
			mNoisePos = 0;

		// "Moog VCF, variation 2" (musicdsp.org) - unchanged from the DXi.
		const double f  = mFrequency * 1.16;
		const double fb = mResonance * (1.0 - 0.15 * f * f);

		eqSample -= mOut4 * fb;
		eqSample *= 0.35013 * (f * f) * (f * f);

		mOut1 = scrub (eqSample + 0.3 * mIn1 + (1.0 - f) * mOut1);  // Pole 1
		mIn1  = eqSample;
		mOut2 = scrub (mOut1 + 0.3 * mIn2 + (1.0 - f) * mOut2);     // Pole 2
		mIn2  = mOut1;
		mOut3 = scrub (mOut2 + 0.3 * mIn3 + (1.0 - f) * mOut3);     // Pole 3
		mIn3  = mOut2;
		mOut4 = scrub (mOut3 + 0.3 * mIn4 + (1.0 - f) * mOut4);     // Pole 4
		mIn4  = mOut3;

		eqSample = mOut4 * mFilterGain;
		// The filtered tap replaces tap 5 and uses the *main* out gain.
		taps[kNumTaps] = static_cast<float> (eqSample * mMainOutGain);
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

	mDelaySamples = static_cast<int> (computeDelaySamples (delay, mLength, mSamplesPer32, &mTempoQuant));

	const float delayLength = static_cast<float> (mDelaySamples * kNumTaps);
	return delayLength / static_cast<float> (mSampleRate);
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
