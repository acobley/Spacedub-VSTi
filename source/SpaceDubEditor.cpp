//------------------------------------------------------------------------
// SpaceDub - editor implementation
//------------------------------------------------------------------------

#include "SpaceDubEditor.h"
#include "SpaceDubController.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace VSTGUI;
using namespace Steinberg;
using namespace Steinberg::Vst;

namespace SpaceDub {

namespace {
/** Geometry of the value readouts.
 *
 *  Under a vertical slider there is a 12 px band (y 388..399) that is clear
 *  of the panel's painted labels across all ten columns, and the columns are
 *  58 px apart at the closest, so a 56 px readout centred on the slider fits
 *  everywhere with 2 px to spare between neighbours.
 *
 *  This is the space *reserved* for a readout, not the size of what gets
 *  drawn: SdValueDisplay sizes its black plate to the string, so a wide
 *  reservation costs nothing visually. It is set by the longest reading a
 *  slider can produce - a tempo-locked delay at the end of its travel,
 *  "8000 ms 40u" - so that reading is not clipped.
 *
 *  The horizontal gain sliders have no free space beneath them - the two rows
 *  are only 8 px apart - so their readout sits inside the slider's own
 *  footprint, below the groove. SdValueDisplay is mouse-disabled, so the
 *  slider still gets the clicks. */
constexpr int kReadoutWidth  = 56;
constexpr int kReadoutHeight = 12;
constexpr int kReadoutY      = 388;   // vertical sliders
constexpr int kReadoutInsetY = 32;    // horizontal sliders, from the slider top
} // anonymous namespace

//------------------------------------------------------------------------
SpaceDubEditor::SpaceDubEditor (SpaceDubController* controller)
: VSTGUIEditor (controller)
, mController (controller)
{
	ViewRect rect (0, 0, kEditorWidth, kEditorHeight);
	setRect (rect);
}

//------------------------------------------------------------------------
void SpaceDubEditor::registerControl (ParamID tag, CControl* control)
{
	mControls[tag] = control;
	if (mController)
		control->setValueNormalized (static_cast<float> (mController->getParamNormalized (tag)));
	frame->addView (control);
}

//------------------------------------------------------------------------
void SpaceDubEditor::addSlider (ParamID tag, int x, int y, int w, int h, bool vertical)
{
	CRect r (x, y, x + w, y + h);
	auto* s = new SdSlider (r, this, static_cast<int32_t> (tag),
	                        vertical ? mHandleV : mHandleH,
	                        vertical ? mGrooveV : mGrooveH,
	                        vertical);
	registerControl (tag, s);

	// The readout is added after the slider so it draws on top, which matters
	// for the horizontal sliders where the two overlap.
	CRect d;
	if (vertical)
	{
		const int cx = x + w / 2;
		d = CRect (cx - kReadoutWidth / 2, kReadoutY,
		           cx - kReadoutWidth / 2 + kReadoutWidth, kReadoutY + kReadoutHeight);
	}
	else
	{
		d = CRect (x, y + kReadoutInsetY, x + w, y + kReadoutInsetY + kReadoutHeight);
	}

	auto* display = new SdValueDisplay (d);
	mDisplays[tag] = display;
	frame->addView (display);
	refreshReadout (tag);
}

//------------------------------------------------------------------------
void SpaceDubEditor::addToggle (ParamID tag, int x, int y, int w, int h, CBitmap* frames)
{
	CRect r (x, y, x + w, y + h);
	auto* t = new SdToggle (r, this, static_cast<int32_t> (tag), frames);
	registerControl (tag, t);
}

//------------------------------------------------------------------------
// Value readouts
//
// The DXi's dialog carried a set of display controls (IDC_LEFTDELAYDISPLAY,
// IDC_LEFTFREQUENCYDISPLAY, IDC_LRESD and friends in resource.h) drawn with
// its custom Label / StaticTime classes. The text below is the short form of
// what its tooltip handler reported - just the value, since the panel artwork
// already prints the name of every slider underneath it.
//------------------------------------------------------------------------
std::string SpaceDubEditor::readoutFor (ParamID tag) const
{
	if (!mController || tag >= kNumParams)
		return {};

	const ParamDef& def = paramDef (tag);

	// Only the sliders carry a readout. The switches show their state by their
	// colour, and the sync rate and tempo have their own text controls.
	if (def.type != ParamType::Float || tag == kMidiRate || tag == kTempo)
		return {};

	const double norm  = mController->getParamNormalized (tag);
	const double plain = def.toPlain (norm);
	const double inner = def.toInternal (norm);

	char buf[64] = {};

	switch (tag)
	{
		//-- delay: the DXi read back the delay the DSP had actually settled on
		//   over a direct C++ back-channel (MyProps). Processor and controller
		//   are separate components in VST3, so the editor instead runs the
		//   very same mapping - DelayLine::computeDelaySamples - over the
		//   sample rate and tempo the processor has told it about.
		case kDelayLeft:
		case kDelayRight:
		{
			const int rate      = static_cast<int> (paramDef (kMidiRate).toInternal (
			                          mController->getParamNormalized (kMidiRate)));
			const int divisions = midiRateDivisions (rate);

			const double sampleRate = mController->getHostSampleRate ();
			const double bpm        = runningTempo ();
			const long   lineLength = static_cast<long> (sampleRate) * DelayLine::kLineSeconds;
			const int    grid       = samplesPer32For (divisions, bpm, sampleRate);

			int steps = 0;
			const long   perTap  = DelayLine::computeDelaySamples (static_cast<float> (inner),
			                                                       lineLength, grid, &steps);
			const double seconds = (perTap * DelayLine::kNumTaps) / sampleRate;

			if (divisions == 0)
				std::snprintf (buf, sizeof (buf), "%.0f ms", seconds * 1000.0);
			else
				std::snprintf (buf, sizeof (buf), "%.0f ms %du", seconds * 1000.0, steps);
			break;
		}

		//-- cutoff: the frequency the ladder filter actually runs at. (The DXi
		//   used sampleRate/4 for the left channel but sampleRate/2 for the
		//   right, so its right-hand readout was double the real value.)
		//
		//   As with the delay, the mapping lives in DelayLine and is called
		//   from here rather than copied - at sample rates below ~40 kHz the
		//   cutoff is clamped to SampleRate/4, and a private copy of the
		//   arithmetic would have gone on reporting 10 kHz.
		case kCutoffLeft:
		case kCutoffRight:
		{
			const double hz = DelayLine::computeCutoffHz (
			    inner, static_cast<int> (mController->getHostSampleRate ()));
			std::snprintf (buf, sizeof (buf), "%.0f Hz", hz);
			break;
		}

		case kResonanceLeft:
		case kResonanceRight:
			std::snprintf (buf, sizeof (buf), "Q %.2f", plain);
			break;

		//-- loop scale was shown by the DXi as a percentage of its own maximum,
		//   i.e. the internal 0..1 value as a percentage.
		case kLoopScaleLeft:
		case kLoopScaleRight:
			std::snprintf (buf, sizeof (buf), "%.0f%%", inner * 100.0);
			break;

		default:
			std::snprintf (buf, sizeof (buf), "%.0f%%", plain);
			break;
	}

	return buf;
}

//------------------------------------------------------------------------
// The tempo stepper's text.
//
// The DXi's property page did this on a timer: it read the tempo the DSP was
// running at back through MyProps and, if it was non-zero, called SetnPos() to
// move the control there - so the control tracked the host regardless of what
// the user had dialled in. This is the same behaviour without moving the
// parameter, which in VST3 would look like an automation write.
//------------------------------------------------------------------------
std::string SpaceDubEditor::tempoText (int step) const
{
	char buf[32] = {};

	// The host wins whenever it gives us a tempo, whatever this control is set
	// to - that is what the DSP does, so it is what the display must say. The
	// DXi did the same thing more bluntly: its property page timer called
	// SetnPos() to drag the control itself onto the host's tempo.
	const double host = mController ? mController->getHostTempo () : 0.0;
	if (hostTempoUsable (host))
		std::snprintf (buf, sizeof (buf), "%.5g BPM", host);
	else if (step <= 0)
		std::snprintf (buf, sizeof (buf), "Tempo Sync");
	else
		std::snprintf (buf, sizeof (buf), "%d BPM", step);

	return buf;
}

//------------------------------------------------------------------------
/** The tempo the delay is running at, by the same rule the processor uses. */
double SpaceDubEditor::runningTempo () const
{
	if (!mController)
		return 120.0;
	const double manual = paramDef (kTempo).toInternal (mController->getParamNormalized (kTempo));
	return effectiveTempo (mController->getHostTempo (), manual);
}

//------------------------------------------------------------------------
std::string SpaceDubEditor::stepperTextFor (ParamID tag) const
{
	auto it = mControls.find (tag);
	if (it == mControls.end ())
		return {};
	if (auto* stepper = dynamic_cast<SdTextStepper*> (it->second))
		return stepper->currentText ();
	return {};
}

//------------------------------------------------------------------------
void SpaceDubEditor::refreshReadout (ParamID tag)
{
	auto it = mDisplays.find (tag);
	if (it == mDisplays.end () || it->second == nullptr)
		return;
	it->second->setText (readoutFor (tag));
}

//------------------------------------------------------------------------
void SpaceDubEditor::refreshAllReadouts ()
{
	for (auto& entry : mDisplays)
		refreshReadout (entry.first);

	// The tempo control draws the host tempo when it is set to "follow", and
	// it is an SdTextStepper rather than an SdValueDisplay, so it needs
	// repainting explicitly.
	auto it = mControls.find (kTempo);
	if (it != mControls.end () && it->second)
		it->second->invalid ();
}

//------------------------------------------------------------------------
bool PLUGIN_API SpaceDubEditor::open (void* parent, const PlatformType& platformType)
{
	if (frame)
		return false;

	CRect frameSize (0, 0, kEditorWidth, kEditorHeight);
	frame = new CFrame (frameSize, this);
	frame->setBackgroundColor (CColor (60, 70, 60, 255));
	frame->open (parent, platformType);

	mBackground   = makeOwned<CBitmap> ("background.png");
	mHandleV      = makeOwned<CBitmap> ("handle_v.png");
	mHandleH      = makeOwned<CBitmap> ("handle_h.png");
	mGrooveV      = makeOwned<CBitmap> ("groove_v.png");
	mGrooveH      = makeOwned<CBitmap> ("groove_h.png");
	mButtonSmall  = makeOwned<CBitmap> ("button_small.png");
	mButtonFilter = makeOwned<CBitmap> ("button_filter.png");
	mButtonPower  = makeOwned<CBitmap> ("button_power.png");

	if (mBackground)
		frame->setBackground (mBackground);

	// ---- vertical sliders (IDD_PROPPAGE, 18 x 136 dlu) -----------------
	addSlider (kDelayLeft,       52, 166, 27, 221, true);
	addSlider (kLoopScaleLeft,  128, 166, 27, 221, true);
	addSlider (kFeedbackLeft,   190, 166, 27, 221, true);
	addSlider (kCutoffLeft,     260, 166, 27, 221, true);
	addSlider (kResonanceLeft,  326, 166, 27, 221, true);

	addSlider (kDelayRight,     416, 166, 27, 221, true);
	addSlider (kLoopScaleRight, 484, 166, 27, 221, true);
	addSlider (kFeedbackRight,  542, 166, 27, 221, true);
	addSlider (kCutoffRight,    614, 166, 27, 221, true);
	addSlider (kResonanceRight, 680, 166, 27, 221, true);

	// ---- horizontal gain sliders (34 x 27 dlu) -------------------------
	addSlider (kTapsGainLeft,          405, 18, 51, 44, false);
	addSlider (kTapsGainRight,         405, 70, 51, 44, false);
	addSlider (kFeedbackOutGainLeft,   471, 18, 51, 44, false);
	addSlider (kFeedbackOutGainRight,  471, 70, 51, 44, false);
	addSlider (kFilterGainLeft,        546, 18, 51, 44, false);
	addSlider (kFilterGainRight,       546, 70, 51, 44, false);

	// ---- toggles -------------------------------------------------------
	addToggle (kEnable,              705,  21, 27, 20, mButtonPower);
	addToggle (kMainFeedbackEnable,  204,  28, 27, 20, mButtonSmall);
	addToggle (kLoopsFeedbackEnable, 261,  28, 27, 20, mButtonSmall);
	addToggle (kTapsOut,             306,  28, 27, 20, mButtonSmall);
	addToggle (kMainOut,             306,  98, 27, 20, mButtonSmall);

	addToggle (kLinkDelays,           45, 122, 27, 20, mButtonSmall);
	addToggle (kLinkLoops,           120, 122, 27, 20, mButtonSmall);
	addToggle (kLinkFeedback,        183, 122, 27, 20, mButtonSmall);
	addToggle (kLinkFilters,         252, 122, 27, 20, mButtonSmall);

	addToggle (kFilterLeftEnable,    297, 348, 14, 50, mButtonFilter);
	addToggle (kFilterRightEnable,   651, 348, 14, 50, mButtonFilter);

	// ---- tempo / sync readouts (the black panel, top right) ------------
	{
		// At 0 the control means "follow the host", so it shows the tempo the
		// processor is actually locked to rather than the bare word "Sync".
		// Capturing `this` is safe: the frame that owns the control is
		// destroyed in close(), before the editor itself.
		CRect r (670, 100, 670 + 64, 100 + 15);
		auto* tempo = new SdTextStepper (r, this, static_cast<int32_t> (kTempo), 300,
		                                 [this] (int step) { return tempoText (step); });
		tempo->setTextColor (CColor (110, 170, 255, 255));
		registerControl (kTempo, tempo);
	}
	{
		CRect r (674, 117, 674 + 60, 117 + 16);
		auto* rate = new SdTextStepper (r, this, static_cast<int32_t> (kMidiRate), 7,
		                                [] (int step) -> std::string {
			                                if (step < 0) step = 0;
			                                if (step > 7) step = 7;
			                                return kMidiRateNames[step];
		                                });
		rate->setTextColor (CColor (110, 255, 170, 255));
		registerControl (kMidiRate, rate);
	}

	return true;
}

//------------------------------------------------------------------------
void PLUGIN_API SpaceDubEditor::close ()
{
	mControls.clear ();
	mDisplays.clear ();

	if (frame)
	{
		frame->forget ();
		frame = nullptr;
	}

	mBackground   = nullptr;
	mHandleV      = nullptr;
	mHandleH      = nullptr;
	mGrooveV      = nullptr;
	mGrooveH      = nullptr;
	mButtonSmall  = nullptr;
	mButtonFilter = nullptr;
	mButtonPower  = nullptr;
}

//------------------------------------------------------------------------
void SpaceDubEditor::applyLink (ParamID tag, ParamValue value)
{
	// left <-> right pairs, and the switch that links them
	struct LinkPair { ParamID a, b, link; };
	static const LinkPair pairs[] = {
		{kDelayLeft,           kDelayRight,           kLinkDelays},
		{kLoopScaleLeft,       kLoopScaleRight,       kLinkLoops},
		{kFeedbackLeft,        kFeedbackRight,        kLinkFeedback},
		{kTapsGainLeft,        kTapsGainRight,        kLinkLoops},
		{kFeedbackOutGainLeft, kFeedbackOutGainRight, kLinkFeedback},
		{kCutoffLeft,          kCutoffRight,          kLinkFilters},
		{kResonanceLeft,       kResonanceRight,       kLinkFilters},
		{kFilterGainLeft,      kFilterGainRight,      kLinkFilters},
		{kFilterLeftEnable,    kFilterRightEnable,    kLinkFilters},
	};

	if (!mController)
		return;

	for (const auto& p : pairs)
	{
		ParamID partner = kNumParams;
		if (tag == p.a)      partner = p.b;
		else if (tag == p.b) partner = p.a;
		else continue;

		if (mController->getParamNormalized (p.link) < 0.5)
			return;

		mController->setParamNormalized (partner, value);
		mController->beginEdit (partner);
		mController->performEdit (partner, value);
		mController->endEdit (partner);
		updateControl (partner, value);
		return;
	}
}

//------------------------------------------------------------------------
void SpaceDubEditor::valueChanged (CControl* control)
{
	if (!control || !mController || mUpdating)
		return;

	const ParamID    tag   = static_cast<ParamID> (control->getTag ());
	const ParamValue value = control->getValueNormalized ();

	mController->setParamNormalized (tag, value);
	mController->performEdit (tag, value);

	applyLink (tag, value);
	refreshReadout (tag);

	// Changing the sync division or the tempo re-scales the delay readouts.
	if (tag == kMidiRate || tag == kTempo)
	{
		refreshReadout (kDelayLeft);
		refreshReadout (kDelayRight);
	}
}

//------------------------------------------------------------------------
void SpaceDubEditor::updateControl (ParamID tag, ParamValue normalized)
{
	auto it = mControls.find (tag);
	if (it != mControls.end () && it->second != nullptr)
	{
		mUpdating = true;
		it->second->setValueNormalized (static_cast<float> (normalized));
		it->second->invalid ();
		mUpdating = false;
	}

	refreshReadout (tag);

	if (tag == kMidiRate || tag == kTempo)
	{
		refreshReadout (kDelayLeft);
		refreshReadout (kDelayRight);
	}
}

//------------------------------------------------------------------------
} // namespace SpaceDub
