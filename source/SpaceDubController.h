//------------------------------------------------------------------------
// SpaceDub - edit controller
//
// Replaces CMediaParams + CSpaceDubMidiPropPage's parameter plumbing.
//------------------------------------------------------------------------

#pragma once

#include "SpaceDubParams.h"

#include "public.sdk/source/vst/vsteditcontroller.h"

#include <vector>

namespace SpaceDub {

class SpaceDubEditor;

//------------------------------------------------------------------------
class SpaceDubController : public Steinberg::Vst::EditControllerEx1
{
public:
	SpaceDubController () = default;
	~SpaceDubController () SMTG_OVERRIDE = default;

	static Steinberg::FUnknown* createInstance (void*)
	{
		return (Steinberg::Vst::IEditController*)new SpaceDubController;
	}

	Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;

	Steinberg::tresult PLUGIN_API setComponentState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;

	Steinberg::tresult PLUGIN_API setParamNormalized (Steinberg::Vst::ParamID tag,
	                                                  Steinberg::Vst::ParamValue value) SMTG_OVERRIDE;

	Steinberg::IPlugView* PLUGIN_API createView (Steinberg::FIDString name) SMTG_OVERRIDE;

	/** Receives the sample rate the processor is running at. */
	Steinberg::tresult PLUGIN_API notify (Steinberg::Vst::IMessage* message) SMTG_OVERRIDE;

	/** The tempo the HOST supplied, or 0 when it supplied none, plus the
	    sample rate the processor is running at. The editor needs both to show
	    delay times in its readouts, and needs the zero case to know whether
	    the tempo control should show the host or the Tempo parameter. */
	double getHostTempo () const      { return mHostTempo; }
	double getHostSampleRate () const { return mHostSampleRate; }

	void editorAttached (Steinberg::Vst::EditorView* editor) SMTG_OVERRIDE;
	void editorRemoved (Steinberg::Vst::EditorView* editor) SMTG_OVERRIDE;
	void editorDestroyed (Steinberg::Vst::EditorView* editor) SMTG_OVERRIDE;

private:
	std::vector<SpaceDubEditor*> mEditors;
	double mHostTempo      = 0.0;   // 0 = the host has not given us one
	double mHostSampleRate = 44100.0;
};

//------------------------------------------------------------------------
} // namespace SpaceDub
