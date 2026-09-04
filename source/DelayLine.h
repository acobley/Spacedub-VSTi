//------------------------------------------------------------------------
// SpaceDub - DelayLine
//
// Direct port of the original DXi "DelayLine" class (DelayLine.cpp/.h,
// 2003, A.E. Cobley) to portable C++17.  All Win32/MFC types (BOOL, TRACE,
// implicit-int return values) have been removed; the arithmetic is
// unchanged so the plug-in sounds like the original.
//
// See PORTING-NOTES.md for the small number of deliberate deviations.
//------------------------------------------------------------------------

#pragma once

#include <vector>
#include <cstdint>

namespace SpaceDub {

//------------------------------------------------------------------------
/** A 5-tap delay line with a Moog-style 4-pole ladder filter in the
    feedback path.  One instance per channel (the original used Delay1 for
    left and Delay2 for right). */
class DelayLine
{
public:
	/** Number of taps.  The original hard-coded nTaps = 5 and the host code
	    indexes taps[1..5], so the tap array must hold kNumTaps + 1 floats. */
	static constexpr int kNumTaps  = 5;
	static constexpr int kTapArray = kNumTaps + 1;
	static constexpr int kNoiseLen = 1000;
	/** Line length in seconds; the original allocated sampleRate * 8. */
	static constexpr int kLineSeconds = 8;

	DelayLine ();

	/** Allocate / reallocate the line.  Call from setupProcessing, never
	    from the audio thread (the original called it from Process()). */
	void  setSampleRate (int sampleRate);
	int   getSampleRate () const { return mSampleRate; }

	/** Zero the line and the filter state. */
	void  reset ();

	/** One sample in, kNumTaps taps out.  taps must point at an array of at
	    least kTapArray floats; taps[1..kNumTaps] are written. */
	void  process (float input, float* taps);

	/** True while the line still holds material worth flushing (used for
	    the release tail). */
	bool  moreTailsAvailable () const { return mRemovePos != mLastValid; }
	/** Advance the tail read position, as the original Get() did. */
	float readTail ();

	// -- parameter setters; all take the plug-in's *internal* ranges ------
	void  setScale (float v)                { mScale = v; }
	/** delay: 0..1.  Returns the resulting delay time in seconds. */
	float setDelay (float delay);

	/** The delay-control mapping, factored out so the editor can show the
	    delay time in its readouts without owning a DelayLine. Returns the
	    per-tap delay in samples; the audible delay is five times that.
	    stepsOut, if given, receives the tempo quantisation count. */
	static long computeDelaySamples (float delay, long lineLength, int samplesPer32,
	                                 int* stepsOut = nullptr);
	void  setLoopsFeedback (bool b)         { mLoopsFeedback = b; }
	void  setMainFeedback (bool b)          { mMainFeedback = b; }
	void  setMainFeedbackValue (float v)    { mMainFeedbackValue = v; }
	/** frequency: 0..1.  Returns the resulting cutoff in Hz. */
	float setFrequency (double frequency);

	/** The cutoff mapping, factored out for the same reason as
	    computeDelaySamples: the editor quotes this frequency in its readout
	    and must not own a second copy of the arithmetic.  Returns the cutoff
	    in Hz that setFrequency() will apply at this sample rate. */
	static double computeCutoffHz (double frequency, int sampleRate);
	void  setResonance (double resonance);
	void  setUseFilter (bool b)             { mUseFilter = b; }
	void  setMainOutGain (float g)          { mMainOutGain = g; }
	void  setTapsOutGain (float g)          { mTapsOutGain = g; }
	void  setFilterGain (float g)           { mFilterGain = g; }
	/** Number of samples in one bar's worth of the sync rate, or 0 for
	    free-running.  Drives the tempo quantisation in setDelay(). */
	void  setSamplesPer32 (int n)           { mSamplesPer32 = n; }

	// -- readouts ---------------------------------------------------------
	int   getTaps () const        { return kNumTaps; }
	long  getLength () const      { return mLength; }
	int   getTempoQuant () const  { return mTempoQuant; }
	int   getDelaySamples () const{ return mDelaySamples; }

private:
	void  buildNoiseTable ();

	std::vector<float> mLine;
	long   mLength      = 0;
	long   mAddPos      = 0;
	long   mRemovePos   = 1;
	long   mLastValid   = 0;
	int    mDelaySamples = 1;

	float  mScale             = 0.5f;
	float  mMainFeedbackValue = 0.5f;
	float  mMainOutGain       = 1.0f;
	float  mTapsOutGain       = 1.0f;
	float  mFilterGain        = 2.0f;

	bool   mLoopsFeedback = false;
	bool   mMainFeedback  = false;
	bool   mUseFilter     = false;

	double mFrequency = 0.9;
	double mResonance = 0.5;

	// Moog ladder state
	double mIn1 = 0.0, mIn2 = 0.0, mIn3 = 0.0, mIn4 = 0.0;
	double mOut1 = 0.0, mOut2 = 0.0, mOut3 = 0.0, mOut4 = 0.0;

	int    mSampleRate   = 44100;
	int    mSamplesPer32 = 0;
	int    mTempoQuant   = 0;

	float        mNoise[kNoiseLen] = {};
	long         mNoisePos  = 0;
	std::uint32_t mRandState = 1;
};

//------------------------------------------------------------------------
} // namespace SpaceDub
