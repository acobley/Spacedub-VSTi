//------------------------------------------------------------------------
// SpaceDub - the panel's layout, as data
//
// THE BLACK PANEL (branch Polivoks, 27 Oct 2026), from the design
// Designs/SpaceDub-Polivoks-1-TapScope.png. It is R2Synth's black panel
// (~/DXi-DEv/R2Synth-VSTi, branch PolivoksUI) dressed for a delay: matte
// black, white printing, boxed sections, black knurled knobs with printed
// scales, chicken-head rotary selectors for every switch, small OLED
// screens behind glass, and every letter in Andy's 70s sci-fi lettering
// (SciFiLettering.h). It replaces the 766 x 499 bitmap panel that copied
// the DXi's property page.
//
//   +--------------------------------------------------------------+
//   | SPACE DUB | engine line                      [status pill]   |  header
//   +--------------------------------------------------------------+
//   | ECHO: the tap scope - five taps a     | CLOCK: sync rate,     |
//   | side, where each lands, which side    | tempo screen + knob,  |
//   | it is heard on                        | thru, taps out, lamps |
//   +--------------------------------------------------------------+
//   | DELAY       | FEEDBACK    | FILTER            | OUTPUT        |
//   |  L row      |  L row      |  L row            |  L row        |
//   |  - LINK -   |  - LINK -   |  - LINK -         |               |
//   |  R row      |  R row      |  R row            |  R row        |
//   +--------------------------------------------------------------+
//
// THE LAYOUT IS DATA, in a header with NO VSTGUI in it, so the editor and
// tests/LayoutTests.cpp read the same numbers: the tests check that every
// parameter is on the panel once, that nothing overlaps, and that every
// label fits - the only way to check a layout without a host.
//------------------------------------------------------------------------

#pragma once

#include "SciFiLettering.h"
#include "SpaceDubParams.h"

#include <cmath>
#include <string>

namespace SpaceDub {
namespace Layout {

//------------------------------------------------------------------------
struct Rect
{
	int left, top, right, bottom;

	constexpr int width () const { return right - left; }
	constexpr int height () const { return bottom - top; }
};

/** The same in fractional pixels - where a run of lettering lands. */
struct Box
{
	double left, top, right, bottom;
};

struct Point { int x, y; };

//------------------------------------------------------------------------
// The panel
//------------------------------------------------------------------------
constexpr int kEditorWidth  = 1200;
constexpr int kEditorHeight = 640;
constexpr int kMargin       = 16;
constexpr int kHeaderHeight = 56;

constexpr int kSectionPadX   = 14;
constexpr int kSectionPadTop = 12;
constexpr int kSectionTitleH = 18;
constexpr int kSectionRadius = 2;

/** Under every knob: two pixels, the reading, the label. */
constexpr int kKnobGap      = 2;
constexpr int kKnobValueH   = 13;
constexpr int kKnobLabelH   = 12;
constexpr int kKnobTextBand = kKnobGap + kKnobValueH + kKnobLabelH;

/** How far the black cut-out round each screen reaches past it. */
constexpr int kScreenBezel = 3;

//------------------------------------------------------------------------
// The lettering - R2Synth's sizes, so the two panels read as a family.
//------------------------------------------------------------------------
constexpr Lettering::Style kTextWordmark { 24.0, 1.7, 1.2 };
constexpr Lettering::Style kTextTitle    { 8.3, Lettering::kBold };
constexpr Lettering::Style kTextSubtitle { 5.8, Lettering::kRegular };
constexpr Lettering::Style kTextLabel    { 6.1, Lettering::kBold };
constexpr Lettering::Style kTextValue    { 6.4, Lettering::kRegular };
constexpr Lettering::Style kTextEngine   { 6.7, Lettering::kRegular };
constexpr Lettering::Style kTextScreen   { 6.1, Lettering::kRegular };
constexpr Lettering::Style kTextScreenSm { 5.4, Lettering::kRegular };   // the tap scope's axis
constexpr Lettering::Style kTextBig      { 9.6, Lettering::kBold };      // the tempo screen
constexpr Lettering::Style kTextBadge    { 7.7, Lettering::kBold };
constexpr Lettering::Style kTextTick     { 4.8, Lettering::kRegular };
constexpr Lettering::Style kTextPill     { 7.0, Lettering::kBold };

//------------------------------------------------------------------------
// Sections
//------------------------------------------------------------------------
enum class Accent
{
	Echo,
	Clock,
	Delay,
	Feedback,
	Filter,
	Output
};

struct Section
{
	const char* title;
	const char* subtitle;
	Accent accent;
	Rect r;
};

constexpr Rect kEchoRect     { 16,  72,  700,  300 };
constexpr Rect kClockRect    { 716, 72,  1184, 300 };
constexpr Rect kDelayRect    { 16,  316, 316,  624 };
constexpr Rect kFeedbackRect { 332, 316, 584,  624 };
constexpr Rect kFilterRect   { 600, 316, 936,  624 };
constexpr Rect kOutputRect   { 952, 316, 1184, 624 };

constexpr Section kSections[] = {
	{ "ECHO",     "five taps each side, cross-panned", Accent::Echo,     kEchoRect     },
	{ "CLOCK",    "tempo sync + master",               Accent::Clock,    kClockRect    },
	{ "DELAY",    "time + regen",                      Accent::Delay,    kDelayRect    },
	{ "FEEDBACK", "",                                  Accent::Feedback, kFeedbackRect },
	{ "FILTER",   "ladder low-pass",                   Accent::Filter,   kFilterRect   },
	{ "OUTPUT",   "levels",                            Accent::Output,   kOutputRect   },
};
constexpr int kNumSections = static_cast<int> (sizeof (kSections) / sizeof (kSections[0]));

constexpr double titleRowCentre (const Rect& section)
{
	return section.top + kSectionPadTop + kSectionTitleH / 2.0 + 1.0;
}

/** THE TITLE BOX: the section's name in a white-outlined box at the left
    of its title row, 8 px of black each side of the lettering. */
inline Box titleBox (const Section& s)
{
	const double left = s.r.left + kSectionPadX - 2;
	const double centre = titleRowCentre (s.r);
	return { left, centre - 11.0, left + 16.0 + Lettering::width (s.title, kTextTitle), centre + 11.0 };
}

inline double subtitleLeft (const Section& s)
{
	return titleBox (s).right + 12.0;
}

//------------------------------------------------------------------------
// The two rows: LEFT over RIGHT in every lower section, the link switches
// on the rule between them.
//------------------------------------------------------------------------
constexpr int kRowLeft  = 380;
constexpr int kRowRight = 512;
constexpr int kLinkY    = 486;

//------------------------------------------------------------------------
// Controls
//------------------------------------------------------------------------
enum class Kind
{
	Knob,     // Panel::Knob - vertical drag
	Switch,   // Panel::RotarySwitch - a chicken head
	Lamp      // Panel::LampSwitch - an illuminated push-on push-off lamp
};

/** Where a switch's name goes. */
enum class NamePlace
{
	Below,
	Right,
	RightRed    // the LINK switches: their name in red, beside them
};

struct Control
{
	Steinberg::Vst::ParamID tag;
	Kind   kind;
	Accent accent;
	int    x, y;          // top left of the control's rectangle
	int    width;
	int    diameter;      // knob: its outer diameter; switch: its chicken head's circle
	bool   bipolar;
	const char* label;
	int    height = 0;    // switches and lamps
	int    pivotX = 0;    // switches: the knob's centre
	int    pivotY = 0;
	int    positions = 2; // switches: the PRINTED positions (Sync Rate has 8 values and 7 names)
	NamePlace place = NamePlace::Below;
};

constexpr Rect controlRect (const Control& c)
{
	return { c.x, c.y, c.x + c.width,
	         c.y + ((c.kind == Kind::Knob) ? c.diameter + kKnobTextBand : c.height) };
}

/** A knob centred on `cx`, its top at `top`. */
constexpr Control knob (Steinberg::Vst::ParamID tag, Accent a, int cx, int top, int diameter, const char* label)
{
	const int w = (diameter >= 56) ? 76 : 72;
	return { tag, Kind::Knob, a, cx - w / 2, top, w, diameter, false, label };
}

/** A two-position switch with its name under it. */
constexpr Control switchBelow (Steinberg::Vst::ParamID tag, Accent a, int cx, int cy, int radius, const char* label,
                               int width = 60)
{
	return { tag, Kind::Switch, a, cx - width / 2, cy - radius - 10, width, radius * 2, false, label,
	         2 * radius + 10 + 22, cx, cy, 2, NamePlace::Below };
}

/** A two-position switch with its name beside it. */
constexpr Control switchRight (Steinberg::Vst::ParamID tag, Accent a, int cx, int cy, int radius, const char* label,
                               int width, NamePlace place = NamePlace::Right)
{
	return { tag, Kind::Switch, a, cx - radius - 6, cy - radius - 6, width, radius * 2, false, label,
	         2 * radius + 12, cx, cy, 2, place };
}

/** A lamp with its name beside it. */
constexpr Control lamp (Steinberg::Vst::ParamID tag, Accent a, int cx, int cy, const char* label)
{
	return { tag, Kind::Lamp, a, cx - 10, cy - 10, 92, 11, false, label, 20, cx, cy };
}

constexpr int kSyncCx = 800, kSyncCy = 178, kSyncRadius = 20;

/** EVERY PARAMETER ON THE PANEL, once each - the tests hold it to that.
    (Bypass is here too, as a lamp: hosts have their own, but the DXi's
    Enabled and the VST3 Bypass are different switches and both belong
    where you can see them.) */
constexpr Control kControls[] = {
	// ---- clock + master -------------------------------------------------
	{ kMidiRate, Kind::Switch, Accent::Clock, 736, 126, 132, kSyncRadius * 2, false, "Sync Rate", 116,
	  kSyncCx, kSyncCy, 7, NamePlace::Below },
	knob (kTempo, Accent::Clock, 968, 186, 44, "Tempo"),
	knob (kMainOut, Accent::Clock, 1125, 106, 44, "Dry"),
	switchRight (kTapsOut, Accent::Clock, 1100, 206, 10, "Taps Out", 82),
	lamp (kEnable, Accent::Clock, 1090, 246, "Enabled"),
	lamp (kBypass, Accent::Clock, 1090, 272, "Bypass"),

	// ---- delay ----------------------------------------------------------
	knob (kDelayLeft,      Accent::Delay, 110, kRowLeft - 12,  56, "Delay"),
	knob (kLoopScaleLeft,  Accent::Delay, 226, kRowLeft - 6,   44, "Loop Scale"),
	knob (kDelayRight,     Accent::Delay, 110, kRowRight - 12, 56, "Delay"),
	knob (kLoopScaleRight, Accent::Delay, 226, kRowRight - 6,  44, "Loop Scale"),
	switchRight (kLinkDelays, Accent::Delay, 150, kLinkY, 8, "Link", 64, NamePlace::RightRed),
	switchRight (kLinkLoops,  Accent::Delay, 266, kLinkY, 8, "Link", 52, NamePlace::RightRed),

	// ---- feedback -------------------------------------------------------
	knob (kFeedbackLeft,  Accent::Feedback, 420, kRowLeft - 12,  56, "Feedback"),
	knob (kFeedbackRight, Accent::Feedback, 420, kRowRight - 12, 56, "Feedback"),
	switchRight (kLinkFeedback, Accent::Feedback, 470, kLinkY, 8, "Link", 64, NamePlace::RightRed),
	switchBelow (kMainFeedbackEnable,  Accent::Feedback, 530, kRowLeft + 22,  10, "Main FB"),
	switchBelow (kLoopsFeedbackEnable, Accent::Feedback, 530, kRowRight + 22, 10, "Taps FB"),

	// ---- filter ---------------------------------------------------------
	knob (kCutoffLeft,     Accent::Filter, 680, kRowLeft - 12,  56, "Cutoff"),
	knob (kResonanceLeft,  Accent::Filter, 766, kRowLeft - 6,   44, "Resonance"),
	knob (kFilterGainLeft, Accent::Filter, 848, kRowLeft - 6,   44, "Gain"),
	switchBelow (kFilterLeftEnable,  Accent::Filter, 906, kRowLeft + 16,  9, "On", 28),
	knob (kCutoffRight,     Accent::Filter, 680, kRowRight - 12, 56, "Cutoff"),
	knob (kResonanceRight,  Accent::Filter, 766, kRowRight - 6,  44, "Resonance"),
	knob (kFilterGainRight, Accent::Filter, 848, kRowRight - 6,  44, "Gain"),
	switchBelow (kFilterRightEnable, Accent::Filter, 906, kRowRight + 16, 9, "On", 28),
	switchRight (kLinkFilters, Accent::Filter, 720, kLinkY, 8, "Link", 64, NamePlace::RightRed),

	// ---- output ---------------------------------------------------------
	knob (kTapsGainLeft,         Accent::Output, 1040, kRowLeft - 6,  44, "Taps Gain"),
	knob (kFeedbackOutGainLeft,  Accent::Output, 1130, kRowLeft - 6,  44, "FB Out"),
	knob (kTapsGainRight,        Accent::Output, 1040, kRowRight - 6, 44, "Taps Gain"),
	knob (kFeedbackOutGainRight, Accent::Output, 1130, kRowRight - 6, 44, "FB Out"),
};
constexpr int kNumControls = static_cast<int> (sizeof (kControls) / sizeof (kControls[0]));

/** The printed names round the Sync Rate selector. The parameter has
    EIGHT values and the DXi mapped the last two both to a whole note
    (PORTING-NOTES 9), so seven are printed and value 7 points at WHOLE. */
constexpr const char* kSyncNames[] = { "Off", "32nd", "16th", "8th", "Qtr", "Half", "Whole" };

//------------------------------------------------------------------------
// The rotary selectors
//------------------------------------------------------------------------
constexpr int kMaxPositions = 8;

/** A selector's geometry: its knob, where its positions point (degrees
    from twelve o'clock, clockwise), and where its name goes. Two
    positions sit 45 degrees either side of the top; more spread over
    240 degrees. */
struct Selector
{
	double cx, cy, radius, labelRadius;
	int positions;
	double angle[kMaxPositions];
	double nameX, nameCentreY;
	NamePlace place;
};

inline Selector selectorOf (const Control& c)
{
	Selector s {};
	s.radius = c.diameter / 2.0;
	s.cx = c.pivotX;
	s.cy = c.pivotY;
	s.positions = (c.positions < 2) ? 2 : (c.positions > kMaxPositions ? kMaxPositions : c.positions);
	s.labelRadius = s.radius + ((s.positions > 3) ? 14.0 : 12.0);
	for (int i = 0; i < s.positions; ++i)
	{
		if (s.positions == 2)
			s.angle[i] = i ? 45.0 : -45.0;
		else if (s.positions == 3)
			s.angle[i] = -45.0 + 45.0 * i;
		else
			s.angle[i] = -120.0 + 240.0 * i / (s.positions - 1);
	}
	s.place = c.place;
	if (c.place == NamePlace::Below)
	{
		s.nameX = s.cx;
		s.nameCentreY = s.cy + ((s.positions > 3) ? s.labelRadius + 16.0 : s.radius + 12.0);
	}
	else
	{
		s.nameX = s.cx + s.radius + 6.0;
		s.nameCentreY = s.cy;
	}
	return s;
}

inline void selectorPoint (const Selector& s, double radius, double degrees, double& x, double& y)
{
	const double t = degrees * 3.14159265358979323846 / 180.0;
	x = s.cx + radius * std::sin (t);
	y = s.cy - radius * std::cos (t);
}

/** WHERE A POSITION'S NAME IS PRINTED: hung from labelRadius along its
    angle - above the knob for one at the top, ending short of the line
    on the left, starting past it on the right. */
inline Box selectorLabelBox (const Selector& s, int index, const std::string& text)
{
	const double t = s.angle[index];
	double px = 0.0, py = 0.0;
	selectorPoint (s, s.labelRadius, t, px, py);
	const double w = Lettering::width (text, kTextLabel);
	const double h = kTextLabel.height ();
	double left = 0.0, centreY = 0.0;
	if (std::fabs (t) < 10.0)
	{
		left = px - w / 2.0;
		centreY = py - 6.0;
	}
	else if (t < 0.0)
	{
		left = px - 2.0 - w;
		centreY = py + 2.0;
	}
	else
	{
		left = px + 2.0;
		centreY = py + 2.0;
	}
	return { left, centreY - h / 2.0, left + w, centreY + h / 2.0 };
}

/** THE POSITION A CLICK PICKS: on the knob itself, -1 (the knob steps
    on); anywhere else, the printed position whose angle is nearest. */
inline int selectorPositionAt (const Selector& s, double x, double y)
{
	const double dx = x - s.cx, dy = y - s.cy;
	if (dx * dx + dy * dy <= s.radius * s.radius)
		return -1;
	const double t = std::atan2 (dx, -dy) * 180.0 / 3.14159265358979323846;
	int best = 0;
	for (int i = 1; i < s.positions; ++i)
		if (std::fabs (s.angle[i] - t) < std::fabs (s.angle[best] - t))
			best = i;
	return best;
}

//------------------------------------------------------------------------
// The screens
//------------------------------------------------------------------------
constexpr Rect kTapScope    { 32,  104, 684,  288 };
constexpr Rect kTempoScreen { 890, 108, 1046, 160 };

constexpr Rect kScreens[] = { kTapScope, kTempoScreen };
constexpr int kNumScreens = static_cast<int> (sizeof (kScreens) / sizeof (kScreens[0]));

constexpr Rect bezelOf (const Rect& r)
{
	return { r.left - kScreenBezel, r.top - kScreenBezel, r.right + kScreenBezel, r.bottom + kScreenBezel };
}

//------------------------------------------------------------------------
// Fixed lettering, badges and rules
//------------------------------------------------------------------------
/** The L and R rings at the start of every row. */
struct Badge
{
	const char* text;
	Point centre;
};

constexpr int kBadgeRadius = 11;

constexpr Badge kBadges[] = {
	{ "L", { 40, kRowLeft + 24 } },   { "R", { 40, kRowRight + 24 } },
	{ "L", { 356, kRowLeft + 24 } },  { "R", { 356, kRowRight + 24 } },
	{ "L", { 624, kRowLeft + 24 } },  { "R", { 624, kRowRight + 24 } },
	{ "L", { 976, kRowLeft + 24 } },  { "R", { 976, kRowRight + 24 } },
};

/** Hairlines: between the L and R rows of each lower section (the links
    sit on it), and between the clock's tempo and its master switches. */
constexpr Rect kRules[] = {
	{ 28,   kLinkY, 304,  kLinkY + 1 },
	{ 344,  kLinkY, 572,  kLinkY + 1 },
	{ 612,  kLinkY, 924,  kLinkY + 1 },
	{ 964,  kLinkY, 1172, kLinkY + 1 },
	{ 1066, 104,    1067, 288 },
};

constexpr Point kScrews[] = { { 8, 8 }, { kEditorWidth - 8, 8 }, { 8, kEditorHeight - 8 },
                              { kEditorWidth - 8, kEditorHeight - 8 } };

//------------------------------------------------------------------------
// The header
//------------------------------------------------------------------------
constexpr Rect kWordmark      { 22,  0,  260, kHeaderHeight };
constexpr int  kHeaderDivider = 282;
constexpr Rect kEngineLine    { 296, 20, 900, 36 };
constexpr Rect kStatusPill    { 914, 14, 1184, 42 };

//------------------------------------------------------------------------
// The arithmetic, asserted rather than trusted
//------------------------------------------------------------------------
static_assert (kOutputRect.right + kMargin == kEditorWidth, "the output section ends a margin from the right");
static_assert (kDelayRect.bottom + kMargin == kEditorHeight, "the lower sections end a margin above the bottom");
static_assert (kRowRight - 12 + 56 + kKnobTextBand <= kDelayRect.bottom - 12, "the R row fits its sections");
static_assert (kRowLeft - 12 + 56 + kKnobTextBand < kLinkY - 10, "the L row clears the links");
static_assert (kWordmark.right < kHeaderDivider && kHeaderDivider < kEngineLine.left, "wordmark, divider, engine line");
static_assert (kEditorWidth == 1200 && kEditorHeight == 640, "the panel changed size - update the banner and this assert");

//------------------------------------------------------------------------
} // namespace Layout
} // namespace SpaceDub
