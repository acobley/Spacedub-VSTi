//------------------------------------------------------------------------
// SpaceDub - parameter definitions
//
// The DXi kept two ranges per parameter: an "external" range shown to the
// user (Parameters.h, e.g. 0..100 %) and an "internal" range used by the
// DSP (e.g. 0..1).  CParamEnvelope::MapToInternal did the conversion.
//
// VST3 works in normalised 0..1 plus a "plain" value shown to the user, so
// the table below keeps all three: the plain range is the DXi external
// range (so the numbers on screen match the original), and toInternal()
// reproduces MapToInternal exactly so the DSP is fed what it always was.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/vst/vsttypes.h"

namespace SpaceDub {

//------------------------------------------------------------------------
enum Param : Steinberg::Vst::ParamID
{
	kEnable = 0,            // "Enabled"
	kDelayLeft,             // "DelayLeft"
	kDelayRight,            // "DelayRight"
	kLoopScaleLeft,         // DXi "LeftLoopsGain"  -> DelayLine::setScale
	kLoopScaleRight,        // DXi "RightLoopsGain" -> DelayLine::setScale
	kMainFeedbackEnable,    // "MainFeedbackEnabled"
	kLoopsFeedbackEnable,   // "LoopsFeedbackEnabled"
	kCutoffLeft,            // "LeftCutoff"
	kCutoffRight,           // "RightCutoff"
	kResonanceLeft,         // "LeftResonance"
	kResonanceRight,        // "RightResonance"
	kFilterLeftEnable,      // "LeftFilterEn"
	kFilterRightEnable,     // "RightFilterEn"
	kTapsOut,               // "LoopsOut"  (all five taps audible, not just the last)
	kFeedbackLeft,          // "LeftFeedback"
	kFeedbackRight,         // "RightFeedback"
	kLinkDelays,            // UI link switches - not used by the DSP
	kLinkLoops,
	kLinkFeedback,
	kLinkFilters,
	kTapsGainLeft,          // DXi "LeftLoopsOutGain"  -> DelayLine::setTapsOutGain
	kTapsGainRight,
	kFeedbackOutGainLeft,   // DXi "LeftFeedbackOutGain" -> DelayLine::setMainOutGain
	kFeedbackOutGainRight,
	kFilterGainLeft,        // "LeftFilterGain"
	kFilterGainRight,       // "RightFilterGain"
	kMidiRate,              // tempo-sync division, 0 = off
	kTempo,                 // manual tempo, 0 = follow the host
	kMainOut,               // "Main Out" - dry signal straight through

	kNumParams,

	kBypass = 1000,         // standard VST3 bypass (kIsBypass)

	/** Read-only, processor -> controller: the tempo the HOST supplied, or
	    zero when it supplied none.

	    Only the processor sees ProcessContext, and IMessage cannot be sent
	    from the audio thread (see PORTING-NOTES section 9), so this is
	    published as an output parameter instead - the mechanism VST3 provides
	    for exactly this. Not part of the saved state.

	    Zero-means-none is the DXi's own convention: CSpaceDubMidi set
	    m_Tempo = 0 whenever the host's tempo was out of range, and the
	    property page tested `if (nTempo != 0)` before overwriting its tempo
	    control on the timer. The editor needs the same distinction. */
	kHostTempoOut = 1001
};

/** Range of kHostTempoOut, matching the Tempo parameter's own scale. */
constexpr double kHostTempoMax = 300.0;

/** Is a tempo the host handed us usable? The DXi's test was
    `(rTempo > 0) && (rTempo < 300)`; anything else meant "no tempo". */
inline bool hostTempoUsable (double bpm)
{
	return bpm > 1.0 && bpm < kHostTempoMax;
}

/** The tempo the delay actually runs at, given what the host supplied (0 if
    nothing) and the Tempo parameter's own internal value.

    The host always wins - the DXi only fell back to PARAM_TEMPO when its
    tempo map gave nothing. Processor and editor both call this so the number
    on screen cannot drift away from the number the DSP used. */
inline double effectiveTempo (double hostBpm, double manualBpm)
{
	if (hostTempoUsable (hostBpm))
		return hostBpm;
	if (manualBpm > 0.0)
		return manualBpm;
	return 120.0;
}

//------------------------------------------------------------------------
enum class ParamType
{
	Float,
	Bool,
	Enum
};

//------------------------------------------------------------------------
struct ParamDef
{
	Steinberg::Vst::ParamID id;
	const char* title;          // shown by the host
	const char* units;
	ParamType   type;
	double      plainMin;       // DXi "external" range
	double      plainMax;
	double      plainDefault;
	double      internalMin;    // range the DSP expects
	double      internalMax;
	int         stepCount;      // 0 = continuous
	bool        smoothed;       // interpolated per sample, as the DXi deltas did

	//--------------------------------------------------------------------
	double toPlain (double normalized) const
	{
		return plainMin + normalized * (plainMax - plainMin);
	}

	double toNormalized (double plain) const
	{
		if (plainMax == plainMin)
			return 0.0;
		return (plain - plainMin) / (plainMax - plainMin);
	}

	/** Reproduces ParamInfo::MapToInternal from the DXi. */
	double toInternal (double normalized) const
	{
		if (type == ParamType::Bool)
			return (normalized < 0.5) ? 0.0 : 1.0;

		const double v = internalMin + normalized * (internalMax - internalMin);
		if (type == ParamType::Enum)
		{
			// Enums were rounded to the nearest step.
			return static_cast<double> (static_cast<long> (v + 0.5));
		}
		return v;
	}

	double defaultNormalized () const { return toNormalized (plainDefault); }
};

//------------------------------------------------------------------------
extern const ParamDef kParams[kNumParams];

/** Look a definition up by id (ids are dense, so this is just an index). */
inline const ParamDef& paramDef (Steinberg::Vst::ParamID id) { return kParams[id]; }

/** Names shown for the tempo-sync division (DXi: m_MidiRate.AddValue). */
extern const char* const kMidiRateNames[8];

/** DXi rate -> divisions per bar (0 = free running). */
inline int midiRateDivisions (int rate)
{
	switch (rate)
	{
		case 1: return 32;   // 32nd
		case 2: return 16;   // 16th
		case 3: return 8;    // 8th
		case 4: return 4;    // quarter
		case 5: return 2;    // half
		case 6: return 1;    // whole
		case 7: return 1;    // whole (the DXi had a duplicate entry here)
		default: return 0;   // off
	}
}

/** Length of one sync grid step in samples, or 0 when free-running.
    Shared by the processor (which feeds it to DelayLine) and the editor
    (which needs it to show the delay time in its readout). */
inline int samplesPer32For (int divisions, double bpm, double sampleRate)
{
	if (divisions == 0 || bpm <= 0.0)
		return 0;
	const double seconds = (60.0 / bpm) * (4.0 / static_cast<double> (divisions));
	const int    samples = static_cast<int> (seconds * sampleRate);
	return samples < 1 ? 1 : samples;
}

//------------------------------------------------------------------------
} // namespace SpaceDub
