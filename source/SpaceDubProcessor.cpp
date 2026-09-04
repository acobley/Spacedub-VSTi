//------------------------------------------------------------------------
// SpaceDub - audio processor implementation
//------------------------------------------------------------------------

#include "SpaceDubProcessor.h"
#include "SpaceDubIDs.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace SpaceDub {

//------------------------------------------------------------------------
SpaceDubProcessor::SpaceDubProcessor ()
{
	setControllerClass (kSpaceDubControllerUID);

	// Since VST3 3.7 the ProcessContext is opt-in: a host is entitled to leave
	// every field unset (and kTempoValid clear) unless the plug-in says what it
	// needs through IProcessContextRequirements. AudioEffect implements that
	// interface for us but defaults the flags to zero - "I need nothing" - so
	// without this line data.processContext->tempo never arrives and the sync
	// rates silently fall back to 120 bpm.
	//
	// Only tempo is asked for; the DSP does not use the transport state or the
	// musical position, and the point of the interface is to request the
	// minimum so hosts can skip the rest.
	processContextRequirements.needTempo ();

	for (int i = 0; i < kNumParams; ++i)
	{
		mParams[i]       = kParams[i].defaultNormalized ();
		mParamTargets[i] = mParams[i];
	}
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpaceDubProcessor::initialize (FUnknown* context)
{
	tresult result = AudioEffect::initialize (context);
	if (result != kResultOk)
		return result;

	addAudioInput (STR16 ("Stereo In"), SpeakerArr::kStereo);
	addAudioOutput (STR16 ("Stereo Out"), SpeakerArr::kStereo);

	// The DXi took its tempo from the MFX host context.  A VST3 effect gets
	// tempo from ProcessContext, but an event input is still declared so
	// hosts route the plug-in a MIDI connection if the user wants one.
	addEventInput (STR16 ("Event In"), 1);

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpaceDubProcessor::terminate ()
{
	return AudioEffect::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpaceDubProcessor::setActive (TBool state)
{
	if (state)
	{
		mDelayL.reset ();
		mDelayR.reset ();
		mTailCountdown = 0;
		// Push the sample rate to the controller now, off the audio thread, so
		// the editor's delay readouts are right before the transport rolls.
		mLastSentTempo = -1.0;
		sendSampleRateToController ();
	}
	return AudioEffect::setActive (state);
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpaceDubProcessor::setupProcessing (ProcessSetup& setup)
{
	mSampleRate  = setup.sampleRate;
	mSampleRateI = static_cast<int> (setup.sampleRate + 0.5);

	// The DXi reallocated its 8-second lines inside Process(); doing it here
	// keeps the audio thread allocation-free.
	mDelayL.setSampleRate (mSampleRateI);
	mDelayR.setSampleRate (mSampleRateI);

	return AudioEffect::setupProcessing (setup);
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpaceDubProcessor::canProcessSampleSize (int32 symbolicSampleSize)
{
	if (symbolicSampleSize == kSample32 || symbolicSampleSize == kSample64)
		return kResultTrue;
	return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpaceDubProcessor::setBusArrangements (SpeakerArrangement* inputs, int32 numIns,
                                                          SpeakerArrangement* outputs, int32 numOuts)
{
	if (numIns == 1 && numOuts == 1)
	{
		// Stereo in / stereo out is the native format; mono is accepted and
		// handled by duplicating the channel (the DXi was stereo-only).
		if ((inputs[0] == SpeakerArr::kStereo && outputs[0] == SpeakerArr::kStereo) ||
		    (inputs[0] == SpeakerArr::kMono && outputs[0] == SpeakerArr::kMono))
		{
			return AudioEffect::setBusArrangements (inputs, numIns, outputs, numOuts);
		}
	}
	return kResultFalse;
}

//------------------------------------------------------------------------
uint32 PLUGIN_API SpaceDubProcessor::getTailSamples ()
{
	// One full delay line; anything still in it can ring out.
	return static_cast<uint32> (mSampleRateI * DelayLine::kLineSeconds);
}

//------------------------------------------------------------------------
void SpaceDubProcessor::applyParameterChanges (IParameterChanges* changes)
{
	if (!changes)
		return;

	const int32 count = changes->getParameterCount ();
	for (int32 i = 0; i < count; ++i)
	{
		IParamValueQueue* queue = changes->getParameterData (i);
		if (!queue)
			continue;

		const int32 points = queue->getPointCount ();
		if (points <= 0)
			continue;

		int32       offset = 0;
		ParamValue  value  = 0.0;
		if (queue->getPoint (points - 1, offset, value) != kResultTrue)
			continue;

		const ParamID id = queue->getParameterId ();
		if (id == kBypass)
			mBypass = (value >= 0.5);
		else if (id < kNumParams)
			mParamTargets[id] = value;
	}
}

//------------------------------------------------------------------------
void SpaceDubProcessor::updateDelayLines (double hostBpm)
{
	// --- tempo sync (CSpaceDubMidi::Process) ----------------------------
	const int rate        = static_cast<int> (paramDef (kMidiRate).toInternal (mParamTargets[kMidiRate]));
	const int divisions   = midiRateDivisions (rate);
	const double manual   = paramDef (kTempo).toInternal (mParamTargets[kTempo]);

	int samplesPer32 = 0;
	if (divisions != 0)
	{
		// The DXi asked for 4 * TPQN / rate ticks, i.e. 4/rate quarter notes.
		// hostBpm is already 0 when the host gave us nothing, so effectiveTempo
		// falls back to the Tempo parameter - and the editor computes its
		// readouts through the very same call.
		samplesPer32 = samplesPer32For (divisions, effectiveTempo (hostBpm, manual), mSampleRate);
	}

	mDelayL.setSamplesPer32 (samplesPer32);
	mDelayR.setSamplesPer32 (samplesPer32);

	// --- per-block switches ---------------------------------------------
	const bool loopsFb = paramDef (kLoopsFeedbackEnable).toInternal (mParamTargets[kLoopsFeedbackEnable]) >= 0.5;
	const bool mainFb  = paramDef (kMainFeedbackEnable).toInternal (mParamTargets[kMainFeedbackEnable]) >= 0.5;

	mDelayL.setLoopsFeedback (loopsFb);
	mDelayR.setLoopsFeedback (loopsFb);
	mDelayL.setMainFeedback (mainFb);
	mDelayR.setMainFeedback (mainFb);

	mDelayL.setUseFilter (paramDef (kFilterLeftEnable).toInternal (mParamTargets[kFilterLeftEnable]) >= 0.5);
	mDelayR.setUseFilter (paramDef (kFilterRightEnable).toInternal (mParamTargets[kFilterRightEnable]) >= 0.5);

	mDelayL.setMainOutGain (static_cast<float> (paramDef (kFeedbackOutGainLeft).toInternal (mParamTargets[kFeedbackOutGainLeft])));
	mDelayR.setMainOutGain (static_cast<float> (paramDef (kFeedbackOutGainRight).toInternal (mParamTargets[kFeedbackOutGainRight])));
	mDelayL.setTapsOutGain (static_cast<float> (paramDef (kTapsGainLeft).toInternal (mParamTargets[kTapsGainLeft])));
	mDelayR.setTapsOutGain (static_cast<float> (paramDef (kTapsGainRight).toInternal (mParamTargets[kTapsGainRight])));
}

//------------------------------------------------------------------------
template <typename SampleType>
void SpaceDubProcessor::processAudio (ProcessData& data)
{
	const int32 numSamples = data.numSamples;

	AudioBusBuffers& inBus  = data.inputs[0];
	AudioBusBuffers& outBus = data.outputs[0];

	const int32 numIn  = inBus.numChannels;
	const int32 numOut = outBus.numChannels;

	auto** in  = reinterpret_cast<SampleType**> (inBus.channelBuffers32);
	auto** out = reinterpret_cast<SampleType**> (outBus.channelBuffers32);

	const SampleType* srcL = (numIn > 0) ? in[0] : nullptr;
	const SampleType* srcR = (numIn > 1) ? in[1] : srcL;
	SampleType*       dstL = (numOut > 0) ? out[0] : nullptr;
	SampleType*       dstR = (numOut > 1) ? out[1] : nullptr;

	if (!dstL)
		return;

	const bool enabled = paramDef (kEnable).toInternal (mParamTargets[kEnable]) >= 0.5;

	// --- bypass ---------------------------------------------------------
	if (mBypass || !enabled)
	{
		for (int32 i = 0; i < numSamples; ++i)
		{
			const SampleType l = srcL ? srcL[i] : SampleType (0);
			const SampleType r = srcR ? srcR[i] : l;
			dstL[i] = l;
			if (dstR)
				dstR[i] = r;
		}
		outBus.silenceFlags = inBus.silenceFlags;
		return;
	}

	outBus.silenceFlags = 0;

	const bool tapsOut      = paramDef (kTapsOut).toInternal (mParamTargets[kTapsOut]) >= 0.5;
	const bool straightThru = paramDef (kMainOut).toInternal (mParamTargets[kMainOut]) >= 0.5;
	const int  tapStart     = tapsOut ? 1 : DelayLine::kNumTaps;

	// --- per-sample interpolation (the DXi's envelope deltas) ------------
	struct Ramp
	{
		double value = 0.0;
		double step  = 0.0;
		void   advance () { value += step; }
	};

	const double invSamples = (numSamples > 0) ? 1.0 / static_cast<double> (numSamples) : 0.0;

	auto makeRamp = [&] (ParamID id) {
		const ParamDef& def = paramDef (id);
		Ramp r;
		r.value = def.toInternal (mParams[id]);
		const double target = def.toInternal (mParamTargets[id]);
		r.step  = (target - r.value) * invSamples;
		return r;
	};

	Ramp delayL   = makeRamp (kDelayLeft);
	Ramp delayR   = makeRamp (kDelayRight);
	Ramp scaleL   = makeRamp (kLoopScaleLeft);
	Ramp scaleR   = makeRamp (kLoopScaleRight);
	Ramp cutoffL  = makeRamp (kCutoffLeft);
	Ramp cutoffR  = makeRamp (kCutoffRight);
	Ramp resoL    = makeRamp (kResonanceLeft);
	Ramp resoR    = makeRamp (kResonanceRight);
	Ramp fbL      = makeRamp (kFeedbackLeft);
	Ramp fbR      = makeRamp (kFeedbackRight);
	Ramp fgainL   = makeRamp (kFilterGainLeft);
	Ramp fgainR   = makeRamp (kFilterGainRight);

	float tapsL[DelayLine::kTapArray] = {};
	float tapsR[DelayLine::kTapArray] = {};

	for (int32 i = 0; i < numSamples; ++i)
	{
		mDelayL.setScale (static_cast<float> (scaleL.value));
		mDelayR.setScale (static_cast<float> (scaleR.value));
		mDelayL.setDelay (static_cast<float> (delayL.value));
		mDelayR.setDelay (static_cast<float> (delayR.value));
		mDelayL.setFrequency (cutoffL.value);
		mDelayR.setFrequency (cutoffR.value);
		mDelayL.setResonance (resoL.value);
		mDelayR.setResonance (resoR.value);
		mDelayL.setMainFeedbackValue (static_cast<float> (fbL.value));
		mDelayR.setMainFeedbackValue (static_cast<float> (fbR.value));
		mDelayL.setFilterGain (static_cast<float> (fgainL.value));
		mDelayR.setFilterGain (static_cast<float> (fgainR.value));

		const float inL = srcL ? static_cast<float> (srcL[i]) : 0.f;
		const float inR = srcR ? static_cast<float> (srcR[i]) : inL;

		mDelayL.process (inL, tapsL);
		mDelayR.process (inR, tapsR);

		float outL = straightThru ? inL : 0.f;
		float outR = straightThru ? inR : 0.f;

		// Tap-to-stereo matrix, verbatim from the DXi: tap 5 sits hard in
		// its own channel, earlier taps walk towards the centre in 10 % steps.
		for (int tap = tapStart; tap <= DelayLine::kNumTaps; ++tap)
		{
			const float t      = static_cast<float> (DelayLine::kNumTaps - tap);
			const float nScale = 1.0f - (t / 10.0f);
			const float oScale = t / 10.0f;

			outL += tapsL[tap] * nScale + tapsR[tap] * oScale;
			outR += tapsL[tap] * oScale + tapsR[tap] * nScale;
		}

		if (!std::isfinite (outL)) outL = 0.f;
		if (!std::isfinite (outR)) outR = 0.f;

		dstL[i] = static_cast<SampleType> (outL);
		if (dstR)
			dstR[i] = static_cast<SampleType> (outR);

		delayL.advance ();  delayR.advance ();
		scaleL.advance ();  scaleR.advance ();
		cutoffL.advance (); cutoffR.advance ();
		resoL.advance ();   resoR.advance ();
		fbL.advance ();     fbR.advance ();
		fgainL.advance ();  fgainR.advance ();
	}
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpaceDubProcessor::process (ProcessData& data)
{
	applyParameterChanges (data.inputParameterChanges);

	// The host's tempo, or zero if it gave us none - see kHostTempoOut.
	double hostBpm = 0.0;
	if (data.processContext && (data.processContext->state & ProcessContext::kTempoValid))
	{
		if (hostTempoUsable (data.processContext->tempo))
			hostBpm = data.processContext->tempo;
	}

	updateDelayLines (hostBpm);

	// Publish the host's tempo, so the editor can quote delay times in
	// milliseconds and the tempo control can follow the host.
	//
	// Zero means "the host gave us nothing" - exactly the convention the DXi's
	// CSpaceDubMidi::m_Tempo used, and what its property page tested before
	// overwriting the tempo control on its timer. The editor needs that
	// distinction: with no host tempo the delay follows the Tempo parameter
	// instead, and the control must go back to showing that value.
	//
	// This goes out as an output parameter, not as an IMessage: sendMessage
	// from the audio thread is discarded by the host's connection proxy, which
	// only forwards on the UI thread. outputParameterChanges is the mechanism
	// VST3 provides for a value travelling processor -> controller per block,
	// and the host delivers it to the controller on the right thread.
	{
		const double rounded = std::floor (hostBpm * 10.0 + 0.5) / 10.0;
		if (rounded != mLastSentTempo && data.outputParameterChanges)
		{
			int32 index = 0;
			if (auto* queue = data.outputParameterChanges->addParameterData (kHostTempoOut, index))
			{
				int32 pointIndex = 0;
				queue->addPoint (0, std::clamp (rounded / kHostTempoMax, 0.0, 1.0), pointIndex);
				mLastSentTempo = rounded;
			}
		}
	}

	if (data.numSamples > 0 && data.numInputs > 0 && data.numOutputs > 0)
	{
		if (processSetup.symbolicSampleSize == kSample64)
			processAudio<Sample64> (data);
		else
			processAudio<Sample32> (data);
	}

	// The block has been rendered; this block's targets become the next
	// block's starting point.
	std::memcpy (mParams, mParamTargets, sizeof (mParams));

	return kResultOk;
}

//------------------------------------------------------------------------
void SpaceDubProcessor::sendSampleRateToController ()
{
	// Called from setActive, which VST3 documents as [UI-thread] - so unlike
	// the tempo, this one may legitimately go out as a message.
	if (!getPeer ())
		return;
	if (IPtr<IMessage> message = owned (allocateMessage ()))
	{
		message->setMessageID (kSpaceDubSampleRateMessage);
		message->getAttributes ()->setFloat (kSpaceDubSampleRateAttribute, mSampleRate);
		sendMessage (message);
	}
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpaceDubProcessor::setState (IBStream* state)
{
	if (!state)
		return kResultFalse;

	IBStreamer streamer (state, kLittleEndian);

	int32 version = 0;
	if (!streamer.readInt32 (version))
		return kResultFalse;

	int32 count = 0;
	if (!streamer.readInt32 (count))
		return kResultFalse;

	for (int32 i = 0; i < count; ++i)
	{
		double v = 0.0;
		if (!streamer.readDouble (v))
			return kResultFalse;
		if (i < kNumParams)
		{
			mParams[i]       = v;
			mParamTargets[i] = v;
		}
	}

	int32 bypass = 0;
	if (streamer.readInt32 (bypass))
		mBypass = (bypass != 0);

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpaceDubProcessor::getState (IBStream* state)
{
	if (!state)
		return kResultFalse;

	IBStreamer streamer (state, kLittleEndian);

	streamer.writeInt32 (1);                       // state version
	streamer.writeInt32 (static_cast<int32> (kNumParams));
	for (int i = 0; i < kNumParams; ++i)
		streamer.writeDouble (mParamTargets[i]);
	streamer.writeInt32 (mBypass ? 1 : 0);

	return kResultOk;
}

//------------------------------------------------------------------------
} // namespace SpaceDub
