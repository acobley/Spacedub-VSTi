//------------------------------------------------------------------------
// SpaceDub - editor
//
// Recreates IDD_PROPPAGE from SpaceDubMidi.rc.  Every control keeps the
// position it had in the Windows dialog: dialog units were converted with
// the MS Sans Serif 8 pt base units the dialog was designed against
// (x * 1.5, y * 1.625), and the whole layout is anchored to the original
// 766 x 499 "background-all" bitmap.
//
// Each slider carries a value readout, in the spirit of the DXi's
// IDC_*DISPLAY controls.  See PORTING-NOTES section 7.
//------------------------------------------------------------------------

#pragma once

#include "DelayLine.h"
#include "SpaceDubControls.h"
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

	/** Rebuild every readout - used when the tempo or sample rate changes,
	    since the delay readouts are quoted in milliseconds. */
	void refreshAllReadouts ();

	/** The text shown under one slider. Public so it can be unit tested. */
	std::string readoutFor (Steinberg::Vst::ParamID tag) const;

	/** The text drawn in one of the two text steppers (kMidiRate, kTempo).
	    Public for the same reason; only valid once the editor is open. */
	std::string stepperTextFor (Steinberg::Vst::ParamID tag) const;

	/** What the tempo stepper shows for a given step. Whenever the host gives
	    us a tempo that is what appears, because that is what the DSP uses. */
	std::string tempoText (int step) const;

	/** The tempo the delay is running at - host if there is one, otherwise the
	    Tempo parameter. Computed through effectiveTempo(), the same call the
	    processor makes, so the two cannot disagree. */
	double runningTempo () const;

	static const int kEditorWidth  = 766;
	static const int kEditorHeight = 499;

private:
	void addSlider (Steinberg::Vst::ParamID tag, int x, int y, int w, int h, bool vertical);
	void addToggle (Steinberg::Vst::ParamID tag, int x, int y, int w, int h, VSTGUI::CBitmap* frames);
	void registerControl (Steinberg::Vst::ParamID tag, VSTGUI::CControl* control);

	/** Mirrors a value onto the partner channel when a link switch is on,
	    exactly as CSpaceDubMidiPropPage::OnVScroll did. */
	void applyLink (Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue value);

	void refreshReadout (Steinberg::Vst::ParamID tag);

	SpaceDubController* mController = nullptr;

	VSTGUI::SharedPointer<VSTGUI::CBitmap> mBackground;
	VSTGUI::SharedPointer<VSTGUI::CBitmap> mHandleV;
	VSTGUI::SharedPointer<VSTGUI::CBitmap> mHandleH;
	VSTGUI::SharedPointer<VSTGUI::CBitmap> mGrooveV;
	VSTGUI::SharedPointer<VSTGUI::CBitmap> mGrooveH;
	VSTGUI::SharedPointer<VSTGUI::CBitmap> mButtonSmall;
	VSTGUI::SharedPointer<VSTGUI::CBitmap> mButtonFilter;
	VSTGUI::SharedPointer<VSTGUI::CBitmap> mButtonPower;

	std::map<Steinberg::Vst::ParamID, VSTGUI::CControl*>    mControls;
	std::map<Steinberg::Vst::ParamID, SdValueDisplay*>      mDisplays;
	bool mUpdating = false;
};

//------------------------------------------------------------------------
} // namespace SpaceDub
