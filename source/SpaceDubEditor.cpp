//------------------------------------------------------------------------
// SpaceDub - editor implementation (the black panel)
//------------------------------------------------------------------------

#include "SpaceDubEditor.h"
#include "SpaceDubController.h"
#include "version.h"

// The commit this was built from - generated on every build by
// cmake/SpaceDubGitVersion.cmake. Absent (a build outside CMake), the About
// box says "not built from git".
#if __has_include("SpaceDubGitVersion.h")
#include "SpaceDubGitVersion.h"
#endif
#ifndef SPACEDUB_GIT_COMMIT
#define SPACEDUB_GIT_COMMIT ""
#define SPACEDUB_GIT_BRANCH ""
#define SPACEDUB_GIT_DIRTY  0
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>

#if SMTG_OS_WINDOWS
#include <windows.h>
#else
#include <dlfcn.h>
#include <stdlib.h>
#endif

using namespace VSTGUI;
using namespace Steinberg;
using namespace Steinberg::Vst;

namespace SpaceDub {

namespace {

CRect toCRect (const Layout::Rect& r)
{
	return CRect (r.left, r.top, r.right, r.bottom);
}

#if SMTG_OS_WINDOWS
std::string narrow (const wchar_t* w)
{
	const int n = WideCharToMultiByte (CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
	if (n <= 1)
		return {};
	std::string s (static_cast<size_t> (n - 1), '\0');
	WideCharToMultiByte (CP_UTF8, 0, w, -1, s.data (), n, nullptr, nullptr);
	return s;
}
#endif

/** The file our own code was loaded from: ask the loader which binary holds
    this very function. (The AU wrapper's .component holds the VST3 inside
    it - AboutInfo.h's bundlePath() sorts out which bundle to name.) */
std::string moduleBinaryPath ()
{
#if SMTG_OS_WINDOWS
	HMODULE module = nullptr;
	if (!GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	                         reinterpret_cast<LPCWSTR> (&moduleBinaryPath), &module))
		return {};
	wchar_t buf[4096] = {};
	return GetModuleFileNameW (module, buf, 4096) ? narrow (buf) : std::string ();
#else
	Dl_info info = {};
	if (dladdr (reinterpret_cast<const void*> (&moduleBinaryPath), &info) && info.dli_fname)
		return info.dli_fname;
	return {};
#endif
}

/** The application we are running in. */
std::string processName ()
{
#if SMTG_OS_WINDOWS
	wchar_t buf[4096] = {};
	if (!GetModuleFileNameW (nullptr, buf, 4096))
		return {};
	std::string s = narrow (buf);
	const auto slash = s.find_last_of ("\\/");
	s = (slash == std::string::npos) ? s : s.substr (slash + 1);
	const auto dot = s.rfind ('.');
	return dot == std::string::npos ? s : s.substr (0, dot);
#elif SMTG_OS_MACOS
	const char* name = getprogname ();
	return name ? name : "";
#else
	return {};
#endif
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
double SpaceDubEditor::tapMilliseconds (ParamID delayTag, int* steps, double* tapsMs) const
{
	const int rate      = static_cast<int> (internalOf (kMidiRate));
	const int divisions = midiRateDivisions (rate);

	const double sr         = sampleRate ();
	const long   lineLength = static_cast<long> (sr) * DelayLine::kLineSeconds;
	const int    grid       = samplesPer32For (divisions, runningTempo (), sr);

	int  s = 0;
	long taps[DelayLine::kTapArray] = {};
	DelayLine::computeTapSamples (static_cast<float> (internalOf (delayTag)), lineLength, grid, taps, &s);
	if (steps)
		*steps = s;
	const double msPerSample = (sr > 0.0) ? 1000.0 / sr : 0.0;
	if (tapsMs)
		for (int n = 1; n <= DelayLine::kNumTaps; ++n)
			tapsMs[n - 1] = taps[n] * msPerSample;
	return taps[DelayLine::kNumTaps] * msPerSample;
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

		//-- delay: the whole echo (tap 5), as the DSP has quantised it -
		//   DelayLine::computeTapSamples over the sample rate and tempo
		//   the processor has told us about. The DXi read it back over a
		//   direct C++ back-channel (MyProps); in VST3 the processor and the
		//   controller are separate components, so the editor runs the same
		//   mapping instead.
		case kDelayLeft:
		case kDelayRight:
		{
			int steps = 0;
			const double echo = tapMilliseconds (tag, &steps);
			if (steps == 0)
				std::snprintf (buf, sizeof (buf), "%.0f ms", echo);
			else
				std::snprintf (buf, sizeof (buf), "%.0f ms %du", echo, steps);
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

	// ---- the wordmark opens the About box ---------------------------------
	mLogo = new Panel::LogoButton (toCRect (Layout::kWordmark), [this] () { openAbout (); });
	frame->addView (mLogo);

	// ---- the controls -----------------------------------------------------
	for (const auto& c : Layout::kControls)
		addControl (c);

	// ---- the About box: last, so it is on top of everything; hidden --------
	mAbout = new Panel::AboutBox (frameSize, [this] () { closeAbout (); });
	mAbout->setVisible (false);
	frame->addView (mAbout);
	mEscapeHook.editor = this;
	frame->registerKeyboardHook (&mEscapeHook);

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
	mLogo = nullptr;
	mAbout = nullptr;

	if (frame)
	{
		frame->unregisterKeyboardHook (&mEscapeHook);
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
			else if (isFilterMode (tag))
				sw->setNames (std::vector<std::string> (std::begin (Layout::kFilterModeNames),
				                                        std::end (Layout::kFilterModeNames)));
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
		{kFilterModeLeft,      kFilterModeRight,      kLinkFilters},
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
		case kFilterTaps:
		case kFilterModeLeft:
		case kFilterModeRight:
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
	int stepsL = 0, stepsR = 0;
	const double echoL = tapMilliseconds (kDelayLeft, &stepsL, pic.tapMs[0]);
	const double echoR = tapMilliseconds (kDelayRight, &stepsR, pic.tapMs[1]);
	pic.grid      = stepsL > 0 || stepsR > 0;
	pic.tapsOut   = isOn (kTapsOut);
	pic.filter[0] = isOn (kFilterLeftEnable);
	pic.filter[1] = isOn (kFilterRightEnable);
	pic.filterTaps = isOn (kFilterTaps);
	pic.highPass[0] = isOn (kFilterModeLeft);
	pic.highPass[1] = isOn (kFilterModeRight);
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
	else if (pic.grid && std::fabs (echoL - echoR) < 0.5)
		std::snprintf (buf, sizeof (buf), "Grid taps  %.0f ms", echoL);
	else if (pic.grid)
		std::snprintf (buf, sizeof (buf), "Grid taps  L %.0f / R %.0f ms", echoL, echoR);
	else if (std::fabs (pic.tapMs[0][0] - pic.tapMs[1][0]) < 0.5)
		std::snprintf (buf, sizeof (buf), "%.0f ms / tap", pic.tapMs[0][0]);
	else
		std::snprintf (buf, sizeof (buf), "L %.0f / R %.0f ms a tap", pic.tapMs[0][0], pic.tapMs[1][0]);
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
	if (isAboutOpen ())
		refreshAbout ();                         // the audio settings or tempo moved
}

//------------------------------------------------------------------------
// The About box
//------------------------------------------------------------------------
About::Info SpaceDubEditor::aboutInfo () const
{
	About::Info info;
	info.version     = FULL_VERSION_STR;
	info.commit      = SPACEDUB_GIT_COMMIT;
	info.branch      = SPACEDUB_GIT_BRANCH;
	info.dirty       = SPACEDUB_GIT_DIRTY != 0;
	info.binaryPath  = moduleBinaryPath ();
	info.processName = processName ();
	if (mController)
	{
		info.audio     = mController->getAudioSettings ();
		info.hostName  = mController->getHostName ();
		info.hostTempo = mController->getHostTempo ();
	}
	info.manualTempo = runningTempo ();
	return info;
}

bool SpaceDubEditor::isAboutOpen () const
{
	return mAbout != nullptr && mAbout->isVisible ();
}

void SpaceDubEditor::refreshAbout ()
{
	if (mAbout)
		mAbout->setRows (About::rows (aboutInfo ()));
}

void SpaceDubEditor::openAbout ()
{
	if (!mAbout || isAboutOpen ())
		return;
	refreshAbout ();
	mAbout->setVisible (true);
	mAbout->invalid ();
}

void SpaceDubEditor::closeAbout ()
{
	// Called from the box's own mouse handler: hide it, never remove it.
	if (!isAboutOpen ())
		return;
	mAbout->setVisible (false);
	if (frame)
		frame->invalid ();
}

void SpaceDubEditor::EscapeHook::onKeyboardEvent (KeyboardEvent& event, CFrame*)
{
	if (editor && editor->isAboutOpen () && event.type == EventType::KeyDown &&
	    event.virt == VirtualKey::Escape)
	{
		editor->closeAbout ();
		event.consumed = true;
	}
}

//------------------------------------------------------------------------
} // namespace SpaceDub
