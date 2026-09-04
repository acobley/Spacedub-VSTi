//------------------------------------------------------------------------
// SpaceDub - audio processor
//
// Replaces CSpaceDubMidi::Process() from the DXi.  The two DelayLine
// instances, the tap-to-stereo panning matrix and the tempo-sync maths are
// all carried over unchanged; only the host plumbing is new.
//------------------------------------------------------------------------

#pragma once

#include "DelayLine.h"
#include "SpaceDubParams.h"

#include "public.sdk/source/vst/vstaudioeffect.h"

namespace SpaceDub {

//------------------------------------------------------------------------
class SpaceDubProcessor : public Steinberg::Vst::AudioEffect
{
public:
	SpaceDubProcessor ();
	~SpaceDubProcessor () SMTG_OVERRIDE = default;

	static Steinberg::FUnknown* createInstance (void*)
	{
		return (Steinberg::Vst::IAudioProcessor*)new SpaceDubProcessor;
	}

	Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setActive (Steinberg::TBool state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& setup) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setBusArrangements (Steinberg::Vst::SpeakerArrangement* inputs,
	                                                  Steinberg::int32 numIns,
	                                                  Steinberg::Vst::SpeakerArrangement* outputs,
	                                                  Steinberg::int32 numOuts) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;

	Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;

	Steinberg::uint32 PLUGIN_API getTailSamples () SMTG_OVERRIDE;

private:
	/** Pull the value of one parameter at a given sample offset, honouring
	    sample-accurate automation. */
	void applyParameterChanges (Steinberg::Vst::IParameterChanges* changes);
	/** @param hostBpm the host's tempo, or 0 when it supplied none. */
	void updateDelayLines (double hostBpm);
	template <typename SampleType>
	void processAudio (Steinberg::Vst::ProcessData& data);

	DelayLine mDelayL;
	DelayLine mDelayR;

	// Normalised parameter values: mParams is where the block starts,
	// mParamTargets where it ends (smoothed params ramp between the two).
	double mParams[kNumParams]        = {};
	double mParamTargets[kNumParams]  = {};
	bool   mBypass                    = false;

	double mSampleRate  = 44100.0;
	int    mSampleRateI = 44100;
	/** Samples still to flush after the input goes silent (release tail). */
	Steinberg::int64 mTailCountdown = 0;

	/** The sample rate goes to the controller as a message from setActive,
	    which VST3 documents as running on the UI thread. The tempo cannot go
	    that way - see the comment in process() - so it is published as an
	    output parameter instead. */
	void sendSampleRateToController ();
	double mLastSentTempo = -1.0;
};

//------------------------------------------------------------------------
} // namespace SpaceDub
