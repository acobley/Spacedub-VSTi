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
//   1. Every parameter is on the panel exactly once (Bypass too) - bar
//      the ten tap feedback levels, which are handles on the tap scope.
//   2. Every control is inside its own section, and inside the panel.
//   3. No two controls overlap, and none overlaps a screen's cut-out.
//   4. Every knob's label - and the widest reading it can show - fits its
//      rectangle; every selector's printed names and its own name fit its.
//   5. Section titles and subtitles fit their sections.
//   6. The wear stays on the panel, off the glass, and faint enough.
//   7. The tap meters' scale (SpaceDubParams.h), which the processor
//      writes and the tap scope reads.
//   8. The About box (AboutInfo.h): with worst-case content every row fits,
//      a long path wraps at '/' and keeps its end, the bundle is found from
//      the binary for VST3 and AU, and the wordmark's button touches no
//      control.
//------------------------------------------------------------------------

#include "AboutInfo.h"
#include "PanelWear.h"
#include "SpaceDubLayout.h"

#include <cmath>
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
		// the per-tap feedback lives on the tap scope, as handles placed at
		// the taps - not in the table; there must be ten of them
		if (isTapFeedback (tag))
			continue;
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
			if (isFilterMode (c.tag))
				for (int i = 0; i < g.positions; ++i)
				{
					const Box b = selectorLabelBox (g, i, kFilterModeNames[i]);
					chk (inside (r, b.left, b.top, b.right, b.bottom),
					     name (c) + ": position " + kFilterModeNames[i] + " outside");
				}
			const double w = Lettering::width (c.label, kTextLabel);
			const double h = kTextLabel.height ();
			if (std::string (c.label).empty ())
				;   // no name of its own (the filter modes): nothing to place
			else if (g.place == NamePlace::Below)
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

	// ---- 7. the tap meters' scale -------------------------------------------------
	std::printf ("7. the tap meters' scale\n");
	auto near = [] (double a, double b) { return std::fabs (a - b) < 1e-9; };
	chk (tapMeterValue (0.0) == 0.0, "silence is not 0");
	chk (tapMeterValue (-1.0) == 0.0, "a negative peak is not 0");
	chk (tapMeterValue (1e-4) == 0.0, "-80 dBFS is not at the floor");
	chk (near (tapMeterValue (1.0), 60.0 / 66.0), "0 dBFS is not 60/66 up the scale");
	chk (tapMeterValue (4.0) == 1.0, "+12 dBFS does not clamp at the top");
	for (double db = -60.0; db <= 6.0; db += 6.0)
		chk (std::fabs (tapMeterDb (tapMeterValue (std::pow (10.0, db / 20.0))) - db) < 1e-6,
		     "tapMeterDb does not invert tapMeterValue at " + std::to_string (db) + " dB");
	for (int ch = 0; ch < 2; ++ch)
		for (int tap = 1; tap <= 5; ++tap)
			chk (isTapLevel (tapLevelId (ch, tap)), "a tap level id is out of range");
	chk (!isTapLevel (kHostTempoOut) && !isTapLevel (kTapLevelOutEnd), "isTapLevel takes a neighbour");
	int feedbackIds = 0;
	for (Steinberg::Vst::ParamID id = 0; id < kNumParams; ++id)
		feedbackIds += isTapFeedback (id);
	chk (feedbackIds == 10, "there are not ten tap feedback parameters");
	for (int ch = 0; ch < 2; ++ch)
		for (int tap = 1; tap <= 5; ++tap)
			chk (isTapFeedback (tapFeedbackId (ch, tap)), "a tap feedback id is out of range");

	// ---- 8. the About box ---------------------------------------------------------
	std::printf ("8. the About box\n");
	{
		using namespace About;
		const std::string dev = "/Users/andy/DXi-DEv/Spacedub-VSTi/build/VST3/Release/SpaceDub.vst3";
		chk (bundlePath (dev + "/Contents/MacOS/SpaceDub") == dev, "the VST3 bundle is not found from its binary");
		chk (bundlePath ("/Library/Audio/Plug-Ins/Components/SpaceDub.component/Contents/Resources/"
		                 "plugin.vst3/Contents/MacOS/SpaceDub") == "/Library/Audio/Plug-Ins/Components/SpaceDub.component",
		     "under the AU wrapper the .component is not what is named");
		chk (bundlePath ("C:\\Program Files\\Common Files\\VST3\\x.dll") ==
		         "C:\\Program Files\\Common Files\\VST3\\x.dll", "a path with no bundle is not left whole");
		chk (rateText (44100) == "44.1 kHz" && rateText (48000) == "48 kHz" && rateText (88200) == "88.2 kHz",
		     "sample rates are not written as kHz");
		chk (bpmText (120) == "120" && bpmText (92.5) == "92.5" && bpmText (128.126) == "128.13",
		     "tempi are not written plainly");

		// a path that fits comes back whole; one that does not wraps after '/'
		auto joined = [] (const std::vector<std::string>& v) { std::string j; for (auto& l : v) j += l; return j; };
		const auto wrapped = wrapPath (dev, Layout::kAboutValueW, 3);
		chk (joined (wrapped) == dev, "a wrapped path does not read back whole");
		chk (wrapped.size () >= 2, "the development path should need two lines (test is meaningless)");
		for (size_t i = 0; i + 1 < wrapped.size (); ++i)
			chk (wrapped[i].back () == '/', "a path line does not break after a '/'");
		std::string deep = "/Volumes/Projects";
		for (int i = 0; i < 20; ++i)
			deep += "/a-very-long-folder-name-" + std::to_string (i);
		deep += "/SpaceDub.vst3";
		const auto cut = wrapPath (deep, Layout::kAboutValueW, Layout::kAboutPathLines);
		chk (static_cast<int> (cut.size ()) == Layout::kAboutPathLines, "a very long path is not cut to its lines");
		chk (cut.front ().rfind ("...", 0) == 0, "a cut path does not start with ...");
		chk (cut.back ().size () >= 13 && cut.back ().compare (cut.back ().size () - 13, 13, "SpaceDub.vst3") == 0,
		     "a cut path loses its end");

		// worst case: every row as long as it can reasonably get
		Info info;
		info.version = "22.22.222.2222";
		info.commit = "abcdef0"; info.branch = "feature/a-rather-long-branch"; info.dirty = true;
		info.audio = { true, 192000.0, 8192, 64, 2, 2 };
		info.hostName = "Steinberg Cubase Pro 14.0.30";
		info.processName = "Cubase 14";
		info.binaryPath = deep + "/Contents/MacOS/SpaceDub";
		info.hostTempo = 128.125;
		const auto rs = rows (info);
		chk (rs.size () == 9, "the About box does not have its nine rows");
		const double labelRoom = Layout::kAboutValueX - Layout::kAboutLabelX - 10.0;
		double widest = 0.0;
		for (const auto& r : rs)
		{
			chk (Lettering::width (r.label, Layout::kTextAboutLabel) <= labelRoom, "About label too wide: " + r.label);
			for (const auto& l : r.lines)
			{
				widest = std::max (widest, textWidth (l));
				chk (textWidth (l) <= Layout::kAboutValueW, "About value too wide: " + l);
			}
		}
		const double lastY = rowCentreY (rs, rs.size () - 1) +
		                     Layout::kAboutLineGap * static_cast<double> (rs.back ().lines.size () - 1);
		chk (lastY + 10.0 < Layout::kAboutFooterY - 16.0, "the About rows run into the footer");
		const double boxW = Layout::kAboutBox.width () - 48.0;
		chk (textWidth ("STEREO MULTI-TAP DELAY WITH LADDER FILTER") <= boxW, "the About tagline does not fit");
		chk (Lettering::width ("CLICK ANYWHERE OR PRESS ESC TO CLOSE", kTextScreenSm) <= boxW, "the About footer does not fit");
		std::printf ("   widest value %.0f of %.0f px; last row at %.0f, footer rule at %.0f\n",
		             widest, Layout::kAboutValueW, lastY, Layout::kAboutFooterY - 16.0);

		// the Audio Unit wrapper, in and out of process; not yet running
		Info au = info;
		au.hostName = kAUWrapperHostName; au.processName = "Logic Pro";
		au.binaryPath = "/Library/Audio/Plug-Ins/Components/SpaceDub.component/Contents/Resources/plugin.vst3/Contents/MacOS/SpaceDub";
		au.audio.known = false;
		const auto ra = rows (au);
		chk (ra[2].lines[0].find ("Audio Unit") != std::string::npos, "the AU wrapper is not reported as an Audio Unit");
		chk (ra[3].lines[0] == "Logic Pro", "under the AU wrapper the host is not the application");
		chk (ra[4].lines[0] == "not running yet" && ra.size () == 8, "an unstarted engine is not reported plainly");
		au.processName = "AUHostingServiceXPC_arrowhead";
		chk (rows (au)[3].lines[0] == "out-of-process Audio Unit service", "an out-of-process AU host is not named");

		// the wordmark's button sits on no control
		for (const auto& c : kControls)
		{
			const Rect r = controlRect (c);
			chk (r.right <= kWordmark.left || r.left >= kWordmark.right || r.bottom <= kWordmark.top ||
			         r.top >= kWordmark.bottom, "a control sits under the wordmark's button");
		}
	}

	if (fails)
	{
		std::printf ("%d check(s) FAILED\n", fails);
		return 1;
	}
	std::printf ("All layout checks passed.\n");
	return 0;
}
