//------------------------------------------------------------------------
// SpaceDub - editor
//
// THE BLACK PANEL (branch Polivoks, 27 Oct 2026): built from the layout
// tables in SpaceDubLayout.h and drawn by SpaceDubPanelViews - no bitmaps.
// It replaced the 766 x 499 bitmap panel that copied the DXi's
// IDD_PROPPAGE pixel for pixel (see PORTING-NOTES section 5 for that one,
// and the git history for its code).
//
// What carried over unchanged: every number on the panel still comes from
// readoutFor() and tempoText(), the link switches still mirror a move onto
// the partner channel exactly as CSpaceDubMidiPropPage::OnVScroll did, and
// the controller still calls updateControl() and refreshAllReadouts().
//------------------------------------------------------------------------

#pragma once

#include "DelayLine.h"
#include "SpaceDubLayout.h"
#include "SpaceDubPanelViews.h"
#include "SpaceDubParams.h"

#include "public.sdk/source/vst/vstguieditor.h"

#include <map>
#include <string>

namespace SpaceDub {

class SpaceDubController;

//------------------------------------------------------------------------
class SpaceDubEditor : public Steinberg::Vst::VSTGUIEditor, public VSTGUI::IControlListener
{
public:
	explicit SpaceDubEditor (SpaceDubController* controller);

	bool PLUGIN_API open (void* parent, const VSTGUI::PlatformType& platformType) SMTG_OVERRIDE;
	void PLUGIN_API close () SMTG_OVERRIDE;

	// IControlListener
	void valueChanged (VSTGUI::CControl* control) SMTG_OVERRIDE;

	/** Called by the controller when a parameter changes elsewhere. */
	void updateControl (Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue normalized);

	/** Rebuild every reading and both screens - used when the tempo or
	    sample rate changes, since the delay readings are in milliseconds. */
	void refreshAllReadouts ();

	/** The reading under one knob. Public so it can be unit tested. */
	std::string readoutFor (Steinberg::Vst::ParamID tag) const;

	/** What the tempo screen shows for a given Tempo step. Whenever the host
	    gives us a tempo that is what appears, because that is what the DSP
	    uses. */
	std::string tempoText (int step) const;

	/** The tempo the delay is running at - host if there is one, otherwise the
	    Tempo parameter. Computed through effectiveTempo(), the same call the
	    processor makes, so the two cannot disagree. */
	double runningTempo () const;

	/** One tap's delay in milliseconds for a channel's Delay parameter, by
	    DelayLine::computeDelaySamples - the DSP's own mapping. */
	double tapMilliseconds (Steinberg::Vst::ParamID delayTag, int* steps = nullptr) const;

	static const int kEditorWidth  = Layout::kEditorWidth;
	static const int kEditorHeight = Layout::kEditorHeight;

private:
	void addControl (const Layout::Control& c);
	void registerControl (Steinberg::Vst::ParamID tag, VSTGUI::CControl* control);

	/** Mirrors a value onto the partner channel when a link switch is on,
	    exactly as CSpaceDubMidiPropPage::OnVScroll did. */
	void applyLink (Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue value);

	/** Everything that depends on `tag` besides its own control. */
	void refreshDependents (Steinberg::Vst::ParamID tag);
	void refreshTapScope ();
	void refreshTempoScreen ();
	void refreshHeader ();

	double normalizedOf (Steinberg::Vst::ParamID tag) const;
	double internalOf (Steinberg::Vst::ParamID tag) const;
	double sampleRate () const;
	bool isOn (Steinberg::Vst::ParamID tag) const { return normalizedOf (tag) >= 0.5; }

	SpaceDubController* mController = nullptr;

	std::map<Steinberg::Vst::ParamID, VSTGUI::CControl*> mControls;
	Panel::TapScope*    mTapScope = nullptr;
	Panel::TempoScreen* mTempoScreen = nullptr;
	Panel::StatusPill*  mStatusPill = nullptr;
	Panel::TextLine*    mEngineLine = nullptr;
	bool mUpdating = false;
};

//------------------------------------------------------------------------
} // namespace SpaceDub
