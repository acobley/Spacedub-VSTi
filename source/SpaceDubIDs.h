//------------------------------------------------------------------------
// SpaceDub - class UIDs
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace SpaceDub {

static const Steinberg::FUID kSpaceDubProcessorUID (0x48B10276, 0xD1F4494F, 0xA2BC745C, 0x9D5CF6A2);
static const Steinberg::FUID kSpaceDubControllerUID (0x7D3DF2D0, 0x2F20420A, 0x92FC3845, 0xFC915769);

#define SpaceDubVST3Category "Fx|Delay"

/** Processor -> controller: the sample rate the delay is running at, sent
    from setActive (UI thread). The tempo travels as the kHostTempoOut output
    parameter instead, because it changes during processing. */
static const char* const kSpaceDubSampleRateMessage   = "SpaceDubSampleRate";
static const char* const kSpaceDubSampleRateAttribute = "sampleRate";

} // namespace SpaceDub
