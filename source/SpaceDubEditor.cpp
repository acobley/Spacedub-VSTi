//------------------------------------------------------------------------
// SpaceDub - editor implementation (the black panel)
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

CRect toCRect (const Layout::Rect& r)
{
	return CRect (r.left, r.top, r.right, r.bottom);
}

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
double SpaceDubEditor::normalizedOf (ParamID tag) const
{
	auto it = mControls.find (tag);
	if (it != mControls.end () && it->second != nullptr)
		return it->second->getValueNormalized ();
	if (mController)
		return mController->getParamNormalized (tag);
	return (tag < kNumParams) ? paramDef (tag).defaultNormalized () : 0.0;
}

double SpaceDubEditor::internalOf (ParamID tag) const
{
	return paramDef (tag).toInternal (normalizedOf (tag));
}

double SpaceDubEditor::sampleRate () const
{
	return mController ? mController->getHostSampleRate () : 44100.0;
}

//------------------------------------------------------------------------
// Readings
//
// The DXi's dialog carried a set of display controls (IDC_LEFTDELAYDISPLAY,
// IDC_LEFTFREQUENCYDISPLAY, IDC_LRESD and friends in resource.h). These are
// the short form of what its tooltip handler reported - just the value,
// since every knob prints its name under the reading.
//------------------------------------------------------------------------
double SpaceDubEditor::tapMilliseconds (ParamID delayTag, int* steps) const
{
	const int rate      = static_cast<int> (internalOf (kMidiRate));
	const int divisions = midiRateDivisions (rate);

	const double sr         = sampleRate ();
	const long   lineLength = static_cast<long> (sr) * DelayLine::kLineSeconds;
	const int    grid       = samplesPer32For (divisions, runningTempo (), sr);

	int s = 0;
	const long perTap = DelayLine::computeDelaySamples (static_cast<float> (internalOf (delayTag)), lineLength, grid, &s);
	if (steps)
		*steps = s;
	return (sr > 0.0) ? perTap * 1000.0 / sr : 0.0;
}

std::string SpaceDubEditor::readoutFor (ParamID tag) const
{
	if (tag >= kNumParams)
		return {};

	const ParamDef& def = paramDef (tag);
	const double norm  = normalizedOf (tag);
	const double plain = def.toPlain (norm);
	const double inner = def.toInternal (norm);

	char buf[64] = {};

	switch (tag)
	{
		//-- the manual tempo: 0 means "follow the host", which the tempo
		//   screen shows; the knob says so in a word
		case kTempo:
		{
			const int step = static_cast<int> (inner);
			if (step <= 0)
				return "Auto";
			std::snprintf (buf, sizeof (buf), "%d BPM", step);
			return buf;
		}

		case kMidiRate:
			return {};

		//-- delay: the whole echo, five taps, as the DSP has quantised it -
		//   DelayLine::computeDelaySamples over the sample rate and tempo
		//   the processor has told us about. The DXi read it back over a
		//   direct C++ back-channel (MyProps); in VST3 the processor and the
		//   controller are separate components, so the editor runs the same
		//   mapping instead.
		case kDelayLeft:
		case kDelayRight:
		{
			int steps = 0;
			const double perTap = tapMilliseconds (tag, &steps);
			const int divisions = midiRateDivisions (static_cast<int> (internalOf (kMidiRate)));
			if (divisions == 0)
				std::snprintf (buf, sizeof (buf), "%.0f ms", perTap * DelayLine::kNumTaps);
			else
				std::snprintf (buf, sizeof (buf), "%.0f ms %du", perTap * DelayLine::kNumTaps, steps);
			break;
		}

		//-- cutoff: the frequency the ladder filter actually runs at,
		//   through DelayLine's own mapping (it clamps below ~40 kHz).
		case kCutoffLeft:
		case kCutoffRight:
		{
			const double hz = DelayLine::computeCutoffHz (inner, static_cast<int> (sampleRate ()));
			if (hz >= 1000.0)
				std::snprintf (buf, sizeof (buf), "%.1fk Hz", hz / 1000.0);
			else
				std::snprintf (buf, sizeof (buf), "%.0f Hz", hz);
			break;
		}

		case kResonanceLeft:
		case kResonanceRight:
			std::snprintf (buf, sizeof (buf), "Q %.2f", plain);
			break;

		//-- loop scale was shown by the DXi as a percentage of its own
		//   maximum, i.e. the internal 0..1 value as a percentage.
		case kLoopScaleLeft:
		case kLoopScaleRight:
			std::snprintf (buf, sizeof (buf), "%.0f%%", inner * 100.0);
			break;

		default:
			if (def.type != ParamType::Float)
				return {};
			std::snprintf (buf, sizeof (buf), "%.0f%%", plain);
			break;
	}

	return buf;
}

//------------------------------------------------------------------------
// The tempo screen's reading.
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

	const double host = mController ? mController->getHostTempo () : 0.0;
	if (hostTempoUsable (host))
		std::snprintf (buf, sizeof (buf), "%.5g BPM", host);
	else if (step <= 0)
		std::snprintf (buf, sizeof (buf), "Tempo Sync");
	else
		std::snprintf (buf, sizeof (buf), "%d BPM", step);

	return buf;
}

double SpaceDubEditor::runningTempo () const
{
	const double host = mController ? mController->getHostTempo () : 0.0;
	return effectiveTempo (host, internalOf (kTempo));
}

//------------------------------------------------------------------------
bool PLUGIN_API SpaceDubEditor::open (void* parent, const PlatformType& platformType)
{
	if (frame)
		return false;

	CRect frameSize (0, 0, kEditorWidth, kEditorHeight);
	frame = new CFrame (frameSize, this);
	frame->setBackgroundColor (Panel::Palette::kGround);
	frame->open (parent, platformType);

	// ---- everything that never changes, behind everything else ----------
	frame->addView (new Panel::Backdrop (frameSize));

	// ---- the screens -------------------------------------------------------
	mTapScope = new Panel::TapScope (toCRect (Layout::kTapScope));
	mTapScope->setWear (Wear::glassWear (0));
	frame->addView (mTapScope);

	// ---- the tap feedback handles, over the scope; placed and shown by
	//      refreshTapScope(), which knows where the taps are -------------------
	for (int ch = 0; ch < 2; ++ch)
	{
		for (int tap = 1; tap <= DelayLine::kNumTaps; ++tap)
		{
			const ParamID tag = tapFeedbackId (ch, tap);
			auto* handle = new Panel::TapHandle (CRect (0, 0, 18, 40), this, static_cast<int32_t> (tag), ch);
			handle->setActiveHandler ([this, tag] (bool active) {
				if (active)
					mActiveHandle = tag;
				else if (mActiveHandle == tag)
					mActiveHandle = kNumParams;
				refreshTapScope ();
			});
			handle->setVisible (false);
			mTapHandles[ch][tap - 1] = handle;
			registerControl (tag, handle);
		}
	}

	mTempoScreen = new Panel::TempoScreen (toCRect (Layout::kTempoScreen));
	mTempoScreen->setWear (Wear::glassWear (1));
	frame->addView (mTempoScreen);

	// ---- the header's two lines that change ----------------------------------
	mEngineLine = new Panel::TextLine (toCRect (Layout::kEngineLine), Layout::kTextEngine, Panel::Palette::kInk);
	frame->addView (mEngineLine);
	mStatusPill = new Panel::StatusPill (toCRect (Layout::kStatusPill));
	frame->addView (mStatusPill);

	// ---- the controls -----------------------------------------------------
	for (const auto& c : Layout::kControls)
		addControl (c);

	refreshAllReadouts ();
	return true;
}

//------------------------------------------------------------------------
void PLUGIN_API SpaceDubEditor::close ()
{
	// The map and the view pointers are RAW pointers to views the FRAME
	// owns: clear them before the frame goes, and never use them after.
	mControls.clear ();
	mTapScope = nullptr;
	for (auto& side : mTapHandles)
		for (auto*& h : side)
			h = nullptr;
	mActiveHandle = kNumParams;
	mTempoScreen = nullptr;
	mStatusPill = nullptr;
	mEngineLine = nullptr;

	if (frame)
	{
		frame->forget ();
		frame = nullptr;
	}
}

//------------------------------------------------------------------------
void SpaceDubEditor::addControl (const Layout::Control& c)
{
	const CRect r = toCRect (Layout::controlRect (c));
	const ParamID tag = c.tag;
	const int32_t t = static_cast<int32_t> (tag);
	const int index = Wear::controlIndex (tag);

	switch (c.kind)
	{
		case Layout::Kind::Knob:
		{
			auto* knob = new Panel::Knob (r, this, t, c.diameter, c.bipolar);
			knob->setLabel (c.label);
			knob->setWear (Wear::knobWear (index));
			// THE PANEL BORROWS readoutFor() rather than formatting in the
			// control, so every number on the panel has one source.
			knob->setFormatter ([this, tag] (float) { return readoutFor (tag); });
			registerControl (tag, knob);
			break;
		}

		case Layout::Kind::Switch:
		{
			const int values = (tag < kNumParams) ? paramDef (tag).stepCount + 1 : 2;
			auto* sw = new Panel::RotarySwitch (r, this, t, c, values);
			sw->setLabel (c.label);
			if (tag == kMidiRate)
				sw->setNames (std::vector<std::string> (std::begin (Layout::kSyncNames), std::end (Layout::kSyncNames)));
			sw->setWear (Wear::selectorGrit (index));
			registerControl (tag, sw);
			break;
		}

		case Layout::Kind::Lamp:
			registerControl (tag, new Panel::LampSwitch (r, this, t, c.label));
			break;
	}
}

//------------------------------------------------------------------------
void SpaceDubEditor::registerControl (ParamID tag, CControl* control)
{
	mControls[tag] = control;

	// Double-click on a knob returns here: the table's default, the value a
	// fresh instance opens with. (Bypass is not in the table: off.)
	control->setDefaultValue ((tag < kNumParams) ? static_cast<float> (paramDef (tag).defaultNormalized ()) : 0.f);

	if (mController)
		control->setValueNormalized (static_cast<float> (mController->getParamNormalized (tag)));
	else
		control->setValueNormalized (control->getDefaultValue ());
	frame->addView (control);
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
		// the per-tap feedback goes with Loop Scale, the amount it multiplies
		{kTapFeedbackLeft1,    kTapFeedbackRight1,    kLinkLoops},
		{kTapFeedbackLeft2,    kTapFeedbackRight2,    kLinkLoops},
		{kTapFeedbackLeft3,    kTapFeedbackRight3,    kLinkLoops},
		{kTapFeedbackLeft4,    kTapFeedbackRight4,    kLinkLoops},
		{kTapFeedbackLeft5,    kTapFeedbackRight5,    kLinkLoops},
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

	// BOTH CALLS, in this order: setParamNormalized so the panel and the
	// controller agree at once, performEdit so the HOST hears of it.
	mController->setParamNormalized (tag, value);
	mController->performEdit (tag, value);

	control->invalid ();
	applyLink (tag, value);
	refreshDependents (tag);
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

	refreshDependents (tag);
}

//------------------------------------------------------------------------
void SpaceDubEditor::refreshDependents (ParamID tag)
{
	auto invalidate = [this] (ParamID t) {
		auto it = mControls.find (t);
		if (it != mControls.end () && it->second)
			it->second->invalid ();
	};

	switch (tag)
	{
		case kMidiRate:
		case kTempo:
			// The sync grid moves the delay, so its readings and the scope.
			invalidate (kDelayLeft);
			invalidate (kDelayRight);
			refreshTapScope ();
			refreshTempoScreen ();
			refreshHeader ();
			break;

		case kDelayLeft:
		case kDelayRight:
		case kTapsOut:
		case kMainOut:
		case kLoopsFeedbackEnable:
		case kFilterLeftEnable:
		case kFilterRightEnable:
			refreshTapScope ();
			break;

		case kEnable:
		case kBypass:
			refreshTapScope ();
			refreshHeader ();
			break;

		default:
			// the processor's tap meters, about 30 times a second; a tap's
			// feedback (its value on the scope while its handle is touched)
			if (isTapLevel (tag) || isTapFeedback (tag))
				refreshTapScope ();
			break;
	}
}

//------------------------------------------------------------------------
void SpaceDubEditor::refreshTapScope ()
{
	if (!mTapScope)
		return;

	Panel::TapPicture pic;
	pic.tapMs[0]  = tapMilliseconds (kDelayLeft);
	pic.tapMs[1]  = tapMilliseconds (kDelayRight);
	pic.tapsOut   = isOn (kTapsOut);
	pic.filter[0] = isOn (kFilterLeftEnable);
	pic.filter[1] = isOn (kFilterRightEnable);
	pic.enabled   = isOn (kEnable) && !isOn (kBypass);
	pic.dry       = normalizedOf (kMainOut);
	for (int ch = 0; ch < 2; ++ch)
		for (int tap = 1; tap <= DelayLine::kNumTaps; ++tap)
			pic.level[ch][tap - 1] = normalizedOf (tapLevelId (ch, tap));

	char buf[48] = {};
	if (!isOn (kEnable))
		std::snprintf (buf, sizeof (buf), "Switched off");
	else if (isOn (kBypass))
		std::snprintf (buf, sizeof (buf), "Bypassed");
	else if (std::fabs (pic.tapMs[0] - pic.tapMs[1]) < 0.5)
		std::snprintf (buf, sizeof (buf), "%.0f ms / tap", pic.tapMs[0]);
	else
		std::snprintf (buf, sizeof (buf), "L %.0f / R %.0f ms a tap", pic.tapMs[0], pic.tapMs[1]);
	pic.annotation = Panel::upper (buf);

	// The feedback handles: shown while Taps Feedback is on, each over its tap.
	pic.tapsFeedback = isOn (kLoopsFeedbackEnable);
	if (pic.tapsFeedback && isTapFeedback (mActiveHandle))
	{
		const int index = static_cast<int> (mActiveHandle - kTapFeedbackLeft1);
		char t[64] = {};
		std::snprintf (t, sizeof (t), "%s tap %d feedback %s", index < 5 ? "L" : "R", index % 5 + 1,
		               readoutFor (mActiveHandle).c_str ());
		pic.handleText = Panel::upper (t);
	}

	mTapScope->setPicture (pic);

	for (int ch = 0; ch < 2; ++ch)
	{
		for (int tap = 1; tap <= DelayLine::kNumTaps; ++tap)
		{
			auto* h = mTapHandles[ch][tap - 1];
			if (!h)
				continue;
			const CRect place = mTapScope->handleRect (ch, tap);
			if (place != h->getViewSize ())
			{
				h->setViewSize (place);
				h->setMouseableArea (place);
			}
			if (h->isVisible () != pic.tapsFeedback)
				h->setVisible (pic.tapsFeedback);
		}
	}
}

//------------------------------------------------------------------------
void SpaceDubEditor::refreshTempoScreen ()
{
	if (!mTempoScreen)
		return;

	const double host = mController ? mController->getHostTempo () : 0.0;
	const int step = static_cast<int> (internalOf (kTempo));
	const char* note = hostTempoUsable (host) ? "Host" : (step > 0 ? "Manual" : "No host");
	mTempoScreen->setText ("Tempo", note, tempoText (step));
}

//------------------------------------------------------------------------
void SpaceDubEditor::refreshHeader ()
{
	char buf[96] = {};
	const int rate = std::clamp (static_cast<int> (internalOf (kMidiRate)), 0, 7);
	std::snprintf (buf, sizeof (buf), "%.0f Hz  /  line %d s  /  %d taps a side  /  sync %s", sampleRate (),
	               DelayLine::kLineSeconds, DelayLine::kNumTaps, kMidiRateNames[rate]);
	if (mEngineLine)
		mEngineLine->setText (Panel::upper (buf));

	if (mStatusPill)
	{
		const bool enabled = isOn (kEnable), bypassed = isOn (kBypass);
		const double host = mController ? mController->getHostTempo () : 0.0;
		std::string text;
		if (!enabled)
			text = "Disabled - press Enabled";
		else
		{
			text = bypassed ? "Bypassed" : "Enabled";
			char t[40] = {};
			if (hostTempoUsable (host))
				std::snprintf (t, sizeof (t), " - host %.5g BPM", host);
			else
				std::snprintf (t, sizeof (t), " - no host tempo");
			text += t;
		}
		mStatusPill->setStatus (enabled && !bypassed, text);
	}
}

//------------------------------------------------------------------------
void SpaceDubEditor::refreshAllReadouts ()
{
	for (auto& entry : mControls)
		if (entry.second != nullptr)
			entry.second->invalid ();

	refreshTapScope ();
	refreshTempoScreen ();
	refreshHeader ();
}

//------------------------------------------------------------------------
} // namespace SpaceDub
