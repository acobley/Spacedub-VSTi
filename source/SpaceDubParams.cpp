//------------------------------------------------------------------------
// SpaceDub - parameter table
//
// Transcribed from CMediaParams::m_aParamInfo in the DXi's Parameters.h.
// Column order there was:
//   MP_TYPE, MP_CAPS, min, max, default, units, label, int.min, int.max
//------------------------------------------------------------------------

#include "SpaceDubParams.h"

namespace SpaceDub {

//------------------------------------------------------------------------
const ParamDef kParams[kNumParams] = {
//   id                      title                    units  type                min  max   def   iMin iMax  steps smooth
	{kEnable,               "Enabled",               "",    ParamType::Bool,    0,   1,    0,    0,   1,    1,    false},
	{kDelayLeft,            "Delay Left",            "%",   ParamType::Float,   0,   100,  20,   0,   1,    0,    true },
	{kDelayRight,           "Delay Right",           "%",   ParamType::Float,   0,   100,  20,   0,   1,    0,    true },
	{kLoopScaleLeft,        "Loop Scale Left",       "%",   ParamType::Float,   0,   50,   5,    0,   1,    0,    true },
	{kLoopScaleRight,       "Loop Scale Right",      "%",   ParamType::Float,   0,   50,   5,    0,   1,    0,    true },
	{kMainFeedbackEnable,   "Main Feedback On",      "",    ParamType::Bool,    0,   1,    1,    0,   1,    1,    false},
	{kLoopsFeedbackEnable,  "Taps Feedback On",      "",    ParamType::Bool,    0,   1,    0,    0,   1,    1,    false},
	{kCutoffLeft,           "Cutoff Left",           "Fs",  ParamType::Float,   0,   1,    0.9,  0,   1,    0,    true },
	{kCutoffRight,          "Cutoff Right",          "Fs",  ParamType::Float,   0,   1,    0.9,  0,   1,    0,    true },
	{kResonanceLeft,        "Resonance Left",        "Q",   ParamType::Float,   0,   3,    0.5,  0,   3,    0,    true },
	{kResonanceRight,       "Resonance Right",       "Q",   ParamType::Float,   0,   3,    0.5,  0,   3,    0,    true },
	{kFilterLeftEnable,     "Filter Left On",        "",    ParamType::Bool,    0,   1,    1,    0,   1,    1,    false},
	{kFilterRightEnable,    "Filter Right On",       "",    ParamType::Bool,    0,   1,    1,    0,   1,    1,    false},
	{kTapsOut,              "Taps Out",              "",    ParamType::Bool,    0,   1,    0,    0,   1,    1,    false},
	{kFeedbackLeft,         "Feedback Left",         "%",   ParamType::Float,   0,   200,  10,   0,   2,    0,    true },
	{kFeedbackRight,        "Feedback Right",        "%",   ParamType::Float,   0,   200,  10,   0,   2,    0,    true },
	{kLinkDelays,           "Link Delays",           "",    ParamType::Bool,    0,   1,    1,    0,   1,    1,    false},
	{kLinkLoops,            "Link Loop Scale",       "",    ParamType::Bool,    0,   1,    1,    0,   1,    1,    false},
	{kLinkFeedback,         "Link Feedback",         "",    ParamType::Bool,    0,   1,    1,    0,   1,    1,    false},
	{kLinkFilters,          "Link Filters",          "",    ParamType::Bool,    0,   1,    1,    0,   1,    1,    false},
	{kTapsGainLeft,         "Taps Gain Left",        "%",   ParamType::Float,   0,   200,  100,  0,   2,    0,    false},
	{kTapsGainRight,        "Taps Gain Right",       "%",   ParamType::Float,   0,   200,  100,  0,   2,    0,    false},
	{kFeedbackOutGainLeft,  "Feedback Out Left",     "%",   ParamType::Float,   0,   200,  100,  0,   2,    0,    false},
	{kFeedbackOutGainRight, "Feedback Out Right",    "%",   ParamType::Float,   0,   200,  100,  0,   2,    0,    false},
	{kFilterGainLeft,       "Filter Gain Left",      "%",   ParamType::Float,   0,   500,  250,  0,   3,    0,    true },
	{kFilterGainRight,      "Filter Gain Right",     "%",   ParamType::Float,   0,   500,  250,  0,   3,    0,    true },
	{kMidiRate,             "Sync Rate",             "",    ParamType::Enum,    0,   7,    0,    0,   7,    7,    false},
	{kTempo,                "Tempo",                 "BPM", ParamType::Enum,    0,   300,  100,  0,   300,  300,  false},
	{kMainOut,              "Straight Thru",         "",    ParamType::Bool,    0,   1,    1,    0,   1,    1,    false},
};

//------------------------------------------------------------------------
const char* const kMidiRateNames[8] = {
	"Off", "32nd", "16th", "8th", "Quarter", "Half", "Whole", "Whole"
};

//------------------------------------------------------------------------
} // namespace SpaceDub
