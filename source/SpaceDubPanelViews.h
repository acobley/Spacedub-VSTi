//------------------------------------------------------------------------
// SpaceDub - the panel's views (the black panel)
//
// FROM THE DESIGN Designs/SpaceDub-Polivoks-1-TapScope.png (branch
// Polivoks, 27 Oct 2026). The drawing is R2Synth's black panel
// (~/DXi-DEv/R2Synth-VSTi/source/R2SynthPanelViews.*, branch PolivoksUI):
// Backdrop, Knob, RotarySwitch and TextLine are its classes with the
// namespace changed and these differences -
//
//   * RotarySwitch takes ANY number of printed positions (Sync Rate has
//     seven), may have more VALUES than printed positions (Sync Rate's
//     eighth value is a second whole note - the DXi's duplicate), sizes
//     its beak to its knob (the link switches are small), and prints its
//     name under it or beside it.
//   * The Backdrop prints SPACE DUB, the L and R badges, and no chevrons.
//
// and these are SpaceDub's own -
//
//   TapScope     the echo section's OLED: where each of the five taps a
//                side lands in time, and how much of it is heard on each
//                side (DelayLine's tap-to-stereo matrix).
//   TempoScreen  a small OLED with one big reading: the tempo the delay
//                runs at, and where it came from.
//   LampSwitch   an illuminated push button - Enabled and Bypass.
//   StatusPill   the header's framed window: running or not, and the
//                host's tempo.
//
// THE SAME TWO RULES AS R2SYNTH. No bitmaps - everything is paths, the
// lettering included, so the panel is resolution-independent and
// resource/ needs no artwork. And no numbers made here: every reading
// comes in through a formatter the editor hands over (readoutFor()), and
// the tap scope gets its times from the editor, which gets them from
// DelayLine::computeDelaySamples - the DSP's own mapping.
//
// THE WEAR is R2Synth's "Studio Worn" (PanelWear.h): dust, hairline
// scratches, polished and scuffed knob tops, specks and fingerprints on
// the screens' glass.
//------------------------------------------------------------------------

#pragma once

#include "PanelWear.h"
#include "SciFiLettering.h"
#include "SpaceDubLayout.h"

#include "vstgui/vstgui.h"

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace SpaceDub {
namespace Panel {

//------------------------------------------------------------------------
// The palette - the design's tokens (R2Synth's, plus the two traces)
//------------------------------------------------------------------------
namespace Palette {

const VSTGUI::CColor kGround     ( 18,  18,  18, 255);
const VSTGUI::CColor kHeader     ( 12,  12,  12, 255);
const VSTGUI::CColor kHeaderRule (233, 230, 220, 255);
const VSTGUI::CColor kSection    ( 21,  21,  21, 255);
const VSTGUI::CColor kSectionEdge(217, 214, 204, 255);
const VSTGUI::CColor kInk        (244, 241, 232, 255);
const VSTGUI::CColor kInkSoft    (185, 182, 174, 255);
const VSTGUI::CColor kInkDim     (143, 140, 133, 255);
const VSTGUI::CColor kDivider    ( 90,  88,  82, 255);
const VSTGUI::CColor kRule       ( 58,  58,  58, 255);
const VSTGUI::CColor kTick       (233, 230, 220, 255);
const VSTGUI::CColor kRed        (226,  52,  43, 255);
const VSTGUI::CColor kScrew      ( 58,  58,  58, 255);
const VSTGUI::CColor kKnobBody   ( 10,  10,  10, 255);
const VSTGUI::CColor kKnobRim    ( 47,  47,  47, 255);
const VSTGUI::CColor kBezel      (  5,   5,   5, 255);
const VSTGUI::CColor kBezelEdge  ( 60,  60,  60, 255);
const VSTGUI::CColor kScreen     (  0,   0,   0, 255);
const VSTGUI::CColor kGridDot    ( 42,  45,  51, 255);
const VSTGUI::CColor kScreenText (141, 150, 163, 255);
const VSTGUI::CColor kAxis       ( 59,  64,  72, 255);
const VSTGUI::CColor kLampOn     (255,  59,  47, 255);
const VSTGUI::CColor kLampOff    ( 58,  21,  18, 255);

/** The tap scope's two traces: the left channel green phosphor, the
    right a pale blue, so the two halves of the picture never blur. */
const VSTGUI::CColor kTraceLeft  ( 95, 224, 122, 255);
const VSTGUI::CColor kTraceRight (155, 232, 255, 255);

} // namespace Palette

//------------------------------------------------------------------------
// The lettering
//------------------------------------------------------------------------
enum class Align
{
	Left,
	Centre,
	Right
};

/** DRAW A RUN OF LETTERING: `text` in `style`, its ink box vertically
    centred on `centreY`, starting at, centred on or ending at `x`.
    `strokeUnits` > 0 strokes the same glyphs at another weight without
    moving them - the wordmark's inline cut. */
void drawLettering (VSTGUI::CDrawContext* context, const std::string& text, const Lettering::Style& style,
                    const VSTGUI::CColor& colour, VSTGUI::CCoord x, VSTGUI::CCoord centreY,
                    Align align = Align::Left, double strokeUnits = -1.0);

/** A string in capitals. ASCII only, which is all the labels are. */
std::string upper (const std::string& s);

//------------------------------------------------------------------------
/** Everything that never changes, from the layout tables, in one view
    behind the others. Mouse-disabled. */
class Backdrop : public VSTGUI::CView
{
public:
	explicit Backdrop (const VSTGUI::CRect& size);
	void draw (VSTGUI::CDrawContext* context) override;
	CLASS_METHODS (Backdrop, VSTGUI::CView)

private:
	void drawWear (VSTGUI::CDrawContext* context);

	Wear::PanelWear mWear;
};

//------------------------------------------------------------------------
/** A ROTARY KNOB: black, knurled, a white line, a printed scale of
    eleven ticks over 270 degrees.

      drag       vertically; up increases; 200 pixels is the whole range
      Shift      ten times finer, drag and wheel alike
      double-click   the parameter's default (setDefaultValue)
      wheel      one per cent a click

    Relative: a click never jumps the value to the pointer. */
class Knob : public VSTGUI::CControl
{
public:
	Knob (const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag, int diameter, bool bipolar);

	void setLabel (const std::string& label) { mLabel = upper (label); }
	void setFormatter (std::function<std::string (float)> formatter) { mFormatter = std::move (formatter); }
	void setWear (const Wear::KnobWear& wear) { mWear = wear; }

	void draw (VSTGUI::CDrawContext* context) override;

	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseMoveEvent (VSTGUI::MouseMoveEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseCancelEvent (VSTGUI::MouseCancelEvent& event) override;
	void onMouseWheelEvent (VSTGUI::MouseWheelEvent& event) override;

	CLASS_METHODS (Knob, VSTGUI::CControl)

	/** VSTGUI's degrees (0 = three o'clock, clockwise) of a normalised
	    value: 135 at 0, 270 (the top) at 0.5, 405 at 1. */
	static double angleOf (double normalized) { return 135.0 + 270.0 * normalized; }

private:
	int  mDiameter;
	bool mBipolar;
	std::string mLabel;
	std::function<std::string (float)> mFormatter;
	Wear::KnobWear mWear;

	bool mDragging = false;
	VSTGUI::CCoord mLastY = 0.;
};

//------------------------------------------------------------------------
/** A ROTARY SELECTOR - every switch on the panel: a chicken-head knob,
    its positions marked round it (the one that is set in red, its name
    in white), its own name under it or beside it.

      click a position (or anywhere off the knob towards it)
                 that position
      click the knob     the next position, round to the first again
      wheel      steps

    `values` is how many values the parameter has (stepCount + 1); the
    PRINTED positions come from the layout and may be fewer - a value past
    the last printed position points at the last. */
class RotarySwitch : public VSTGUI::CControl
{
public:
	RotarySwitch (const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag,
	              const Layout::Control& layout, int values);

	/** The names printed at the positions; empty strings print none. */
	void setNames (const std::vector<std::string>& names) { mNames = names; invalid (); }
	void setLabel (const std::string& label) { mLabel = upper (label); }
	void setWear (const std::vector<Wear::PolarSpeck>& grit) { mGrit = grit; }

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseWheelEvent (VSTGUI::MouseWheelEvent& event) override;

	CLASS_METHODS (RotarySwitch, VSTGUI::CControl)

private:
	int currentValue () const;
	int printedIndex () const;
	void select (int value);

	Layout::Selector mGeometry;
	int mValues;
	std::vector<std::string> mNames;
	std::string mLabel;
	std::vector<Wear::PolarSpeck> mGrit;
};

//------------------------------------------------------------------------
/** AN ILLUMINATED PUSH BUTTON: a red lamp in a black bezel with its name
    beside it, lit when the parameter is on. A click toggles it. */
class LampSwitch : public VSTGUI::CControl
{
public:
	LampSwitch (const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag,
	            const std::string& name);

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;

	CLASS_METHODS (LampSwitch, VSTGUI::CControl)

private:
	std::string mName;
};

//------------------------------------------------------------------------
/** What the tap scope shows: each channel's per-tap delay, and whether
    the taps before the last are heard (Taps Out). Filled in by the
    editor from DelayLine::computeDelaySamples. */
struct TapPicture
{
	double tapMs[2] = { 0.0, 0.0 };   // left, right: one tap's delay
	bool   tapsOut  = false;          // taps 1-4 in the output, or tap 5 only
	bool   filter[2] = { false, false };
	bool   enabled  = true;
	double dry      = 1.0;            // Dry Level, 0..1: the height of the DRY line
	std::string annotation;           // top right, e.g. "64 MS / TAP"
	/** How much signal is coming out of each tap right now: the
	    processor's meters (kTapLevelOut), 0..1 = -60..+6 dBFS.
	    [channel][tap - 1]. */
	double level[2][5] = {};
	/** Taps Feedback is on: the scope says so, and the editor shows a
	    TapHandle over each tap. */
	bool   tapsFeedback = false;
	/** While a handle is being touched: what it is set to, shown top right
	    in amber in place of the annotation. */
	std::string handleText;
};

/** THE TAP SCOPE - the echo section's OLED.

    A time axis across the middle. Each channel's five taps sit CENTRED
    on n x its per-tap delay, so a left and a right tap at the same time
    share one vertical line: the LEFT channel's go UP from the axis in
    green, the RIGHT's DOWN in pale blue.

    Each tap is a FRAME and a FILL:

      the frame   how much of that tap is heard on its own side -
                  DelayLine's matrix gives tap n 1 - (5 - n) / 10 of its
                  own channel, so tap 5's frame is full height and tap 1's
                  six tenths. Fixed by the settings.
      the fill    how much signal is going through the tap NOW - the
                  processor's meter for it, -60..+6 dBFS up the frame -
                  so the fill is the signal that tap puts on that side.
                  Empty when no audio is playing.

    The tap's figure in dBFS is printed by its number. The share that
    crosses to the other side is a small frame on the far side of the
    axis, drawn over the other channel's bar, with a bright mark at the
    level crossing over. A tap that is not in the output (Taps Out off:
    only tap 5 is) is drawn dimmed - the signal goes through it but is
    not heard. F marks the tap the filter replaces.

    Mouse-disabled. */
class TapScope : public VSTGUI::CView
{
public:
	explicit TapScope (const VSTGUI::CRect& size);

	void setPicture (const TapPicture& picture) { mPicture = picture; invalid (); }
	void setWear (const Wear::GlassWear& wear) { mWear = wear; }

	void draw (VSTGUI::CDrawContext* context) override;

	CLASS_METHODS (TapScope, VSTGUI::CView)

	/** The axis's full scale in ms and its tick step, for the two
	    channels' per-tap delays: the last tap plus a fifth, rounded up to
	    a whole number of 1/2/5 steps, at most eight labelled ticks. Public
	    for the tests. */
	static void axisFor (double tapMsLeft, double tapMsRight, double& spanMs, double& stepMs);

	/** Where a TapHandle goes for tap `n` (1..5) of `side` (0 left, 1
	    right), in the frame's coordinates: a column 18 px wide centred on
	    the tap, from the axis to the full height of a frame - up for the
	    left, down for the right. */
	VSTGUI::CRect handleRect (int side, int n) const;

private:
	TapPicture mPicture;
	Wear::GlassWear mWear;
};

//------------------------------------------------------------------------
/** A TAP FEEDBACK HANDLE - one per tap per channel, laid over the tap
    scope while Taps Feedback is on: an amber bar across the tap that
    sets how much that tap feeds back (0..200 % of Loop Scale; the dim
    tick is 100 %, the DXi's amount). Its column runs from the axis
    outwards, so 0 % is at the axis and 200 % at the far end, on the
    channel's own side.

      drag       up and down (away from the axis increases), relative
      Shift      ten times finer
      double-click   100 %
      wheel      one per cent a click

    While it is hovered or dragged the editor shows its value on the
    scope (setActiveHandler). */
class TapHandle : public VSTGUI::CControl
{
public:
	TapHandle (const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag, int side);

	/** Called with true when the pointer comes over it or a drag starts,
	    false when both are over. */
	void setActiveHandler (std::function<void (bool)> handler) { mActive = std::move (handler); }
	bool isActive () const { return mHover || mDragging; }

	void draw (VSTGUI::CDrawContext* context) override;

	void onMouseEnterEvent (VSTGUI::MouseEnterEvent& event) override;
	void onMouseExitEvent (VSTGUI::MouseExitEvent& event) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseMoveEvent (VSTGUI::MouseMoveEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseCancelEvent (VSTGUI::MouseCancelEvent& event) override;
	void onMouseWheelEvent (VSTGUI::MouseWheelEvent& event) override;

	CLASS_METHODS (TapHandle, VSTGUI::CControl)

private:
	void tellActive (bool wasActive);

	int  mSide;
	bool mHover = false;
	bool mDragging = false;
	VSTGUI::CCoord mLastY = 0.;
	std::function<void (bool)> mActive;
};

//------------------------------------------------------------------------
/** A SMALL OLED WITH ONE BIG READING - the tempo: a caption top left, a
    note top right in the trace colour, the reading centred under them. */
class TempoScreen : public VSTGUI::CView
{
public:
	explicit TempoScreen (const VSTGUI::CRect& size);

	void setText (const std::string& caption, const std::string& note, const std::string& reading);
	void setWear (const Wear::GlassWear& wear) { mWear = wear; }

	void draw (VSTGUI::CDrawContext* context) override;

	CLASS_METHODS (TempoScreen, VSTGUI::CView)

private:
	std::string mCaption, mNote, mReading;
	Wear::GlassWear mWear;
};

//------------------------------------------------------------------------
/** THE HEADER'S FRAMED WINDOW: a red frame, a lamp, a line of lettering.
    The lamp is lit while the delay is running. Mouse-disabled. */
class StatusPill : public VSTGUI::CView
{
public:
	explicit StatusPill (const VSTGUI::CRect& size);
	void setStatus (bool running, const std::string& text);
	void draw (VSTGUI::CDrawContext* context) override;
	CLASS_METHODS (StatusPill, VSTGUI::CView)

private:
	bool mRunning = false;
	std::string mText;
};

//------------------------------------------------------------------------
/** ONE LINE OF LETTERING whose words change - the engine line in the
    header. Left-aligned, vertically centred in the view. Mouse-disabled. */
class TextLine : public VSTGUI::CView
{
public:
	TextLine (const VSTGUI::CRect& size, const Lettering::Style& style, const VSTGUI::CColor& colour);
	void setText (const std::string& text);
	const std::string& getText () const { return mText; }
	void draw (VSTGUI::CDrawContext* context) override;
	CLASS_METHODS (TextLine, VSTGUI::CView)

private:
	Lettering::Style mStyle;
	VSTGUI::CColor mColour;
	std::string mText;
};

//------------------------------------------------------------------------
} // namespace Panel
} // namespace SpaceDub
