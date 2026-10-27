//------------------------------------------------------------------------
// SpaceDub - the black panel's layout, checked without a host
//
// SpaceDubLayout.h, SciFiLettering.h and PanelWear.h include no VSTGUI, so
// the panel's geometry can be checked from any machine with a C++17
// compiler and the VST3 SDK's headers (for the ParamID type only):
//
//     c++ -std=c++17 -I../source -I<vst3sdk> LayoutTests.cpp -o layout-tests
//
// What it pins down:
//
//   1. Every parameter is on the panel exactly once (Bypass too).
//   2. Every control is inside its own section, and inside the panel.
//   3. No two controls overlap, and none overlaps a screen's cut-out.
//   4. Every knob's label - and the widest reading it can show - fits its
//      rectangle; every selector's printed names and its own name fit its.
//   5. Section titles and subtitles fit their sections.
//   6. The wear stays on the panel, off the glass, and faint enough.
//------------------------------------------------------------------------

#include "PanelWear.h"
#include "SpaceDubLayout.h"

#include <cstdio>
#include <string>

using namespace SpaceDub;
using namespace SpaceDub::Layout;

static int fails = 0;
static void chk (bool ok, const std::string& what)
{
	if (!ok)
	{
		std::printf ("  FAIL: %s\n", what.c_str ());
		++fails;
	}
}

static bool overlaps (const Rect& a, const Rect& b)
{
	return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}

static bool inside (const Rect& outer, double l, double t, double r, double b, double pad = 0.0)
{
	return l >= outer.left - pad && r <= outer.right + pad && t >= outer.top - pad && b <= outer.bottom + pad;
}

static const Section& sectionOf (Accent a)
{
	for (const auto& s : kSections)
		if (s.accent == a)
			return s;
	return kSections[0];
}

static std::string name (const Control& c)
{
	char buf[64];
	std::snprintf (buf, sizeof (buf), "control %u (%s)", static_cast<unsigned> (c.tag), c.label ? c.label : "");
	return buf;
}

/** The widest reading each kind of knob can show, as readoutFor() spells it. */
static std::string widestReading (Steinberg::Vst::ParamID tag)
{
	switch (tag)
	{
		case kDelayLeft: case kDelayRight:         return "8000 MS 40U";
		case kCutoffLeft: case kCutoffRight:       return "10.0K HZ";
		case kResonanceLeft: case kResonanceRight: return "Q 3.00";
		case kTempo:                               return "300 BPM";
		default:                                   return "500%";
	}
}

int main ()
{
	// ---- 1. every parameter, once ------------------------------------------
	std::printf ("1. every parameter on the panel once\n");
	for (Steinberg::Vst::ParamID id = 0; id <= kNumParams; ++id)
	{
		const Steinberg::Vst::ParamID tag = (id == kNumParams) ? Steinberg::Vst::ParamID (kBypass) : id;
		int n = 0;
		for (const auto& c : kControls)
			n += (c.tag == tag);
		chk (n == 1, "parameter " + std::to_string (tag) + " appears " + std::to_string (n) + " times");
	}

	// ---- 2. in its section, on the panel -------------------------------------
	std::printf ("2. every control inside its section\n");
	for (const auto& c : kControls)
	{
		const Rect r = controlRect (c);
		const Rect& s = sectionOf (c.accent).r;
		chk (inside (s, r.left, r.top, r.right, r.bottom), name (c) + " is outside its section");
		chk (r.top > titleRowCentre (s) + 10, name (c) + " is in its section's title row");
	}

	// ---- 3. no overlaps --------------------------------------------------------
	std::printf ("3. no overlaps\n");
	for (int i = 0; i < kNumControls; ++i)
	{
		for (int j = i + 1; j < kNumControls; ++j)
			chk (!overlaps (controlRect (kControls[i]), controlRect (kControls[j])),
			     name (kControls[i]) + " overlaps " + name (kControls[j]));
		for (const auto& sc : kScreens)
			chk (!overlaps (controlRect (kControls[i]), bezelOf (sc)), name (kControls[i]) + " overlaps a screen");
		for (const auto& b : kBadges)
		{
			const Rect br { b.centre.x - kBadgeRadius, b.centre.y - kBadgeRadius, b.centre.x + kBadgeRadius,
			                b.centre.y + kBadgeRadius };
			chk (!overlaps (controlRect (kControls[i]), br), name (kControls[i]) + " overlaps a badge");
		}
	}

	// ---- 4. everything fits its rectangle ----------------------------------------
	std::printf ("4. every label fits\n");
	for (const auto& c : kControls)
	{
		const Rect r = controlRect (c);
		if (c.kind == Kind::Knob)
		{
			chk (Lettering::width (c.label, kTextLabel) <= r.width (), name (c) + ": label too wide");
			chk (Lettering::width (widestReading (c.tag), kTextValue) <= r.width (), name (c) + ": reading too wide");
		}
		else if (c.kind == Kind::Switch)
		{
			const Selector g = selectorOf (c);
			if (c.tag == kMidiRate)
				for (int i = 0; i < g.positions; ++i)
				{
					const Box b = selectorLabelBox (g, i, kSyncNames[i]);
					chk (inside (r, b.left, b.top, b.right, b.bottom), name (c) + ": position " + kSyncNames[i] + " outside");
				}
			const double w = Lettering::width (c.label, kTextLabel);
			const double h = kTextLabel.height ();
			if (g.place == NamePlace::Below)
				chk (inside (r, g.nameX - w / 2, g.nameCentreY - h / 2, g.nameX + w / 2, g.nameCentreY + h / 2),
				     name (c) + ": name outside");
			else
				chk (inside (r, g.nameX, g.nameCentreY - h / 2, g.nameX + w, g.nameCentreY + h / 2),
				     name (c) + ": name outside");
			// the knob, and the end of every position mark (as RotarySwitch
			// draws them: radius + 8, or + 6 on a small switch)
			chk (inside (r, g.cx - g.radius, g.cy - g.radius, g.cx + g.radius, g.cy + g.radius), name (c) + ": knob outside");
			const double tick = g.radius + ((g.radius >= 12.0) ? 8.0 : 6.0);
			for (int i = 0; i < g.positions; ++i)
			{
				double x = 0.0, y = 0.0;
				selectorPoint (g, tick, g.angle[i], x, y);
				chk (inside (r, x, y, x, y, 1.0), name (c) + ": a position mark is outside");
			}
		}
		else
		{
			chk (c.pivotX + 12 + Lettering::width (c.label, kTextLabel) <= r.right, name (c) + ": lamp name outside");
		}
	}

	// ---- 5. titles fit -------------------------------------------------------------
	std::printf ("5. section titles fit\n");
	for (const auto& s : kSections)
	{
		const double right = subtitleLeft (s) + Lettering::width (s.subtitle, kTextSubtitle);
		chk (right <= s.r.right - 12, std::string (s.title) + ": title row too wide");
	}
	chk (kWordmark.left + Lettering::width ("SPACE", kTextWordmark) + 12 + Lettering::width ("DUB", kTextWordmark) <=
	         kHeaderDivider - 10,
	     "wordmark runs into the divider");
	chk (kStatusPill.left + 30 + Lettering::width ("DISABLED - PRESS ENABLED", kTextPill) <= kStatusPill.right - 8,
	     "status pill text too wide");
	chk (kEngineLine.left + Lettering::width ("192000 HZ  /  LINE 8 S  /  5 TAPS A SIDE  /  SYNC QUARTER", kTextEngine) <=
	         kStatusPill.left - 10,
	     "engine line runs into the status pill");

	// ---- 6. the wear ---------------------------------------------------------------
	std::printf ("6. the wear\n");
	const auto w = Wear::panelWear ();
	for (const auto& b : w.haze)
		chk (b.alpha <= Wear::kMaxHazeAlpha, "a haze blob is too strong");
	for (const auto& l : w.scratches)
	{
		chk (l.alpha <= Wear::kMaxMarkAlpha, "a scratch is too strong");
		for (const auto& sc : kScreens)
			for (const auto& p : l.pts)
				chk (!inside (bezelOf (sc), p.x, p.y, p.x, p.y), "a scratch crosses a screen");
	}
	for (int i = 0; i < kNumScreens; ++i)
	{
		const auto g = Wear::glassWear (i);
		for (const auto& d : g.dust)
			chk (inside (kScreens[i], d.x, d.y, d.x, d.y), "glass dust off its screen");
	}
	chk (Wear::glassWear (0).prints.size () == 1 && Wear::glassWear (1).prints.size () == 1,
	     "one fingerprint on each screen");

	if (fails)
	{
		std::printf ("%d check(s) FAILED\n", fails);
		return 1;
	}
	std::printf ("All layout checks passed.\n");
	return 0;
}
