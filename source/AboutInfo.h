//------------------------------------------------------------------------
// SpaceDub - what the About box says
//
// Clicking the wordmark opens the About box (SpaceDubEditor::openAbout).
// This header decides WHAT it says - the rows, their wording, where a long
// path wraps - and SpaceDubPanelViews only draws the result. Like
// SpaceDubLayout.h it includes no VSTGUI, so tests/LayoutTests.cpp can
// check that every row fits the box without a host.
//
// Where each piece comes from:
//
//   version     version.h, FULL_VERSION_STR - the same number as the bundle
//   build       the git commit, branch and whether the tree had local
//               changes, stamped at BUILD time by cmake/SpaceDubGitVersion.cmake
//               (so it is the commit you built, not the one you configured)
//   audio       the processor, which sends its ProcessSetup and channel
//               count with the sample rate from setActive (SpaceDubIDs.h)
//   host        IHostApplication::getName; under the Audio Unit wrapper that
//               only ever says "VST3-AU Wrapper", so the app's own process
//               name is used instead
//   loaded from the plug-in's own binary (dladdr), trimmed to its bundle
//------------------------------------------------------------------------

#pragma once

#include "SciFiLettering.h"
#include "SpaceDubLayout.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace SpaceDub {
namespace About {

/** What the processor told us about how it is being run. */
struct AudioSettings
{
	bool   known       = false;   // false until the processor has been activated
	double sampleRate  = 0.0;
	int    maxBlock    = 0;       // ProcessSetup::maxSamplesPerBlock
	int    sampleBits  = 32;      // 32 or 64 (ProcessSetup::symbolicSampleSize)
	int    processMode = 0;       // Vst::ProcessModes: 0 realtime, 1 prefetch, 2 offline
	int    channels    = 2;
};

struct Info
{
	std::string   version;
	std::string   commit;         // empty if the build was not from a git checkout
	std::string   branch;
	bool          dirty = false;
	AudioSettings audio;
	std::string   hostName;       // IHostApplication's
	std::string   processName;    // the application the plug-in is running in
	std::string   binaryPath;     // the plug-in's own binary
	double        hostTempo   = 0.0;   // 0 = the host gave none
	double        manualTempo = 0.0;   // the Tempo control
};

/** One row of the box: a label, and a value that may wrap onto several lines. */
struct Row
{
	std::string label;
	std::vector<std::string> lines;
};

/** What Steinberg's Audio Unit wrapper calls itself to the plug-in. */
constexpr const char* kAUWrapperHostName = "VST3-AU Wrapper";

inline bool contains (const std::string& s, const char* what)
{
	return s.find (what) != std::string::npos;
}

inline bool isAudioUnit (const Info& info)
{
	return info.hostName == kAUWrapperHostName || contains (info.binaryPath, ".component/");
}

/** The bundle the binary is in: .../SpaceDub.vst3/Contents/MacOS/SpaceDub ->
    .../SpaceDub.vst3. Under the Audio Unit wrapper the VST3 sits inside the
    .component (Contents/Resources/plugin.vst3), and the .component is what
    was installed, so that is what is named. Anything else comes back whole. */
inline std::string bundlePath (const std::string& binary)
{
	for (const char* ext : { ".component", ".vst3" })
	{
		const std::string e (ext);
		const auto at = binary.find (e + "/");
		if (at != std::string::npos)
			return binary.substr (0, at + e.size ());
		if (binary.size () >= e.size () && binary.compare (binary.size () - e.size (), e.size (), e) == 0)
			return binary;
	}
	return binary;
}

/** 44100 -> "44.1 kHz", 48000 -> "48 kHz". */
inline std::string rateText (double sampleRate)
{
	char buf[32];
	const double k = sampleRate / 1000.0;
	if (std::fabs (k - std::round (k)) < 1e-6)
		std::snprintf (buf, sizeof (buf), "%d kHz", static_cast<int> (std::lround (k)));
	else
		std::snprintf (buf, sizeof (buf), "%.1f kHz", k);
	return buf;
}

/** 120 -> "120", 92.5 -> "92.5", 128.125 -> "128.13". */
inline std::string bpmText (double bpm)
{
	char buf[32];
	std::snprintf (buf, sizeof (buf), "%.2f", bpm);
	std::string s (buf);
	while (!s.empty () && s.back () == '0')
		s.pop_back ();
	if (!s.empty () && s.back () == '.')
		s.pop_back ();
	return s;
}

inline double textWidth (const std::string& s)
{
	return Lettering::width (s, Layout::kTextAboutValue);
}

/** Wrap a path to `width`, breaking after a '/', or anywhere in a single
    piece too long for a line. More than maxLines and the START is dropped
    for "..." - the end of a path, the bundle's name, is what matters. */
inline std::vector<std::string> wrapPath (const std::string& path, double width, int maxLines)
{
	// the pieces: each runs up to and including a '/'
	std::vector<std::string> pieces;
	std::string cur;
	for (char c : path)
	{
		cur += c;
		if (c == '/')
		{
			pieces.push_back (cur);
			cur.clear ();
		}
	}
	if (!cur.empty ())
		pieces.push_back (cur);

	std::vector<std::string> lines;
	std::string line;
	for (const std::string& piece : pieces)
	{
		if (textWidth (line + piece) <= width)
		{
			line += piece;
			continue;
		}
		if (!line.empty ())
		{
			lines.push_back (line);
			line.clear ();
		}
		// a piece wider than a whole line: break it by characters
		for (char c : piece)
		{
			if (!line.empty () && textWidth (line + c) > width)
			{
				lines.push_back (line);
				line.clear ();
			}
			line += c;
		}
	}
	if (!line.empty ())
		lines.push_back (line);

	if (maxLines > 0 && static_cast<int> (lines.size ()) > maxLines)
	{
		lines.erase (lines.begin (), lines.end () - maxLines);
		std::string& first = lines.front ();
		first = "..." + first;
		while (first.size () > 4 && textWidth (first) > width)
			first.erase (3, 1);
	}
	return lines;
}

inline const char* processModeText (int mode)
{
	switch (mode)
	{
		case 1:  return "prefetch";
		case 2:  return "offline (bounce)";
		default: return "realtime";
	}
}

/** The rows, top to bottom. `width` is what a value may use. */
inline std::vector<Row> rows (const Info& info, double width = Layout::kAboutValueW)
{
	std::vector<Row> out;
	auto add = [&] (const char* label, std::string value) {
		out.push_back ({ label, { std::move (value) } });
	};

	add ("Version", info.version);

	if (info.commit.empty ())
		add ("Build", "not built from git");
	else
	{
		std::string b = "commit " + info.commit;
		if (!info.branch.empty () && info.branch != "HEAD")
			b += " on " + info.branch;
		if (info.dirty)
			b += " + local changes";
		add ("Build", b);
	}

	const bool au = isAudioUnit (info);
	add ("Format", au ? "Audio Unit, via the VST3 wrapper" : "VST3");

	std::string host = au ? info.processName : info.hostName;
	if (host.empty ())
		host = info.processName.empty () ? "unknown" : info.processName;
	// Logic, GarageBand and MainStage can run an Audio Unit in a separate
	// process, which is then what we find ourselves in.
	if (contains (host, "AUHostingService"))
		host = "out-of-process Audio Unit service";
	add ("Host", host);

	if (!info.audio.known)
	{
		add ("Audio", "not running yet");
	}
	else
	{
		const auto& a = info.audio;
		std::string chans = a.channels == 1 ? "mono" : a.channels == 2 ? "stereo"
		                                             : std::to_string (a.channels) + " channels";
		add ("Audio", rateText (a.sampleRate) + " \xC2\xB7 " + chans + " \xC2\xB7 " +
		                  std::to_string (a.sampleBits) + "-bit float");
		add ("Processing", "blocks of up to " + std::to_string (a.maxBlock) + " \xC2\xB7 " +
		                       processModeText (a.processMode));
	}

	if (info.hostTempo > 0.0)
		add ("Tempo", bpmText (info.hostTempo) + " bpm, from the host");
	else
		add ("Tempo", bpmText (info.manualTempo) + " bpm, from the Tempo control");

	out.push_back ({ "Loaded from", info.binaryPath.empty ()
	                                     ? std::vector<std::string> { "unknown" }
	                                     : wrapPath (bundlePath (info.binaryPath), width, Layout::kAboutPathLines) });

	add ("Authors", "Andy Cobley and Claude");
	return out;
}

/** Where row `index`'s first line is centred, given the rows above it. */
inline double rowCentreY (const std::vector<Row>& rs, size_t index)
{
	double y = Layout::kAboutFirstRowY;
	for (size_t i = 0; i < index && i < rs.size (); ++i)
		y += Layout::kAboutRowGap + Layout::kAboutLineGap * (rs[i].lines.size () > 1 ? rs[i].lines.size () - 1 : 0);
	return y;
}

//------------------------------------------------------------------------
} // namespace About
} // namespace SpaceDub
