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
	/** How much tap `tap` (1..kNumTaps) feeds back when loops feedback is
	    on, as a multiple of the scale: 1 is the DXi's regeneration. */
	void  setTapFeedback (int tap, float g)  { if (tap >= 1 && tap <= kNumTaps) mTapFeedback[tap] = g; }
	/** delay: 0..1.  Returns the resulting delay time in seconds. */
	float setDelay (float delay);

	/** The delay-control mapping, factored out so the editor can show the
	    delay time in its readouts without owning a DelayLine. Returns the
	    per-tap delay in samples; the audible delay is five times that.
	    stepsOut, if given, receives the tempo quantisation count. */
	static long computeDelaySamples (float delay, long lineLength, int samplesPer32,
	                                 int* stepsOut = nullptr);

	/** Where every tap sits, in samples: tapsOut[1..kNumTaps] (tapsOut must
	    hold kTapArray longs; [0] is set to 0). The processor and the editor
	    both call this, so the tap scope cannot drift from the DSP.

	    Free-running, or synced but too short to quantise: the DXi's equal
	    spacing, tap n at n x computeDelaySamples().

	    Synced (GRID TAPS): with the echo at N grid units U (U = samplesPer32,
	    one step of the sync rate), tap 5 sits at exactly N x U and taps 4..1
	    halve down from the largest power of two strictly below N:
	        N = 1   ->  1/16, 1/8, 1/4, 1/2, 1 U
	        N = 3   ->  1/4, 1/2, 1, 2, 3 U
	        N = 8   ->  1/2, 1, 2, 4, 8 U
	    stepsOut receives N (0 when not quantised). Returns tap 5. */
	static long computeTapSamples (float delay, long lineLength, int samplesPer32,
	                               long* tapsOut, int* stepsOut = nullptr);
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
	/** Filter Taps: when the filter is on, taps 1..4 go through a ladder
	    filter of their own as well (same cutoff, resonance and gain as tap
	    5's), scaled by the taps out gain. Off is the DXi: only tap 5 is
	    filtered. Only the taps' OUTPUT is filtered - the tap-to-tap
	    regeneration still reads the line. */
	void  setFilterTaps (bool b)            { mFilterTaps = b; }
	/** Filter mode: false the DXi's 4-pole low-pass, true a 4-pole
	    high-pass mixed from the same ladder's poles (see runLadder). */
	void  setHighPass (bool b)              { mHighPass = b; }
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
	/** Where tap `tap` (1..kNumTaps) reads from, in samples. */
	long  getTapSamples (int tap) const { return (tap >= 1 && tap <= kNumTaps) ? mTapSamples[tap] : 0; }

private:
	void  buildNoiseTable ();

	/** One 4-pole ladder's state. */
	struct Ladder
	{
		double in1 = 0.0, in2 = 0.0, in3 = 0.0, in4 = 0.0;
		double out1 = 0.0, out2 = 0.0, out3 = 0.0, out4 = 0.0;
	};
	/** "Moog VCF, variation 2", one sample, before the filter gain: its
	    low-pass output, or with mHighPass its high-pass mix. */
	double runLadder (Ladder& s, double x) const;

	std::vector<float> mLine;
	long   mLength      = 0;
	long   mAddPos      = 0;
	long   mRemovePos   = 1;
	long   mLastValid   = 0;
	int    mDelaySamples = 1;
	/** Each tap's delay in samples, [1..kNumTaps]; set by setDelay() from
	    computeTapSamples(). */
	long   mTapSamples[kTapArray] = { 0, 1, 2, 3, 4, 5 };

	float  mScale             = 0.5f;
	float  mTapFeedback[kTapArray] = { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
	float  mMainFeedbackValue = 0.5f;
	float  mMainOutGain       = 1.0f;
	float  mTapsOutGain       = 1.0f;
	float  mFilterGain        = 2.0f;

	bool   mLoopsFeedback = false;
	bool   mMainFeedback  = false;
	bool   mUseFilter     = false;
	bool   mFilterTaps    = false;
	bool   mHighPass      = false;

	double mFrequency = 0.9;
	double mResonance = 0.5;

	// Moog ladder state: [kNumTaps] is the DXi's own filter on tap 5;
	// [1..kNumTaps-1] filter taps 1..4 when Filter Taps is on.
	Ladder mLadder[kTapArray];

	int    mSampleRate   = 44100;
	int    mSamplesPer32 = 0;
	int    mTempoQuant   = 0;

	float        mNoise[kNoiseLen] = {};
	long         mNoisePos  = 0;
	std::uint32_t mRandState = 1;
};

//------------------------------------------------------------------------
} // namespace SpaceDub
