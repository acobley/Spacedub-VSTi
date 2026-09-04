//------------------------------------------------------------------------
// SpaceDub - edit controller implementation
//------------------------------------------------------------------------

#include "SpaceDubController.h"
#include "SpaceDubEditor.h"
#include "SpaceDubIDs.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"

#include <algorithm>
#include <cstdio>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace SpaceDub {

//------------------------------------------------------------------------
namespace {

/** A RangeParameter whose text formatting matches the DXi property page. */
class SpaceDubParameter : public RangeParameter
{
public:
	SpaceDubParameter (const ParamDef& def)
	// NOTE: title and units must not be null. RangeParameter passes them
	// straight to UString::assign, which dereferences without a null check -
	// a null title segfaults the host (and the SDK validator) during
	// EditController::initialize.
	: RangeParameter (USTRING (def.title), def.id, USTRING (def.units), def.plainMin, def.plainMax,
	                  def.plainDefault, def.stepCount, ParameterInfo::kCanAutomate,
	                  kRootUnitId, USTRING (def.title))
	, mDef (def)
	{
	}

	void toString (ParamValue normalized, String128 string) const SMTG_OVERRIDE
	{
		char text[64] = {};

		if (mDef.id == kMidiRate)
		{
			int step = static_cast<int> (mDef.toInternal (normalized));
			step = std::max (0, std::min (7, step));
			std::snprintf (text, sizeof (text), "%s", kMidiRateNames[step]);
		}
		else if (mDef.id == kTempo)
		{
			const int bpm = static_cast<int> (mDef.toInternal (normalized));
			if (bpm <= 0)
				std::snprintf (text, sizeof (text), "Host Sync");
			else
				std::snprintf (text, sizeof (text), "%d", bpm);
		}
		else if (mDef.type == ParamType::Bool)
		{
			std::snprintf (text, sizeof (text), "%s", (normalized >= 0.5) ? "On" : "Off");
		}
		else
		{
			std::snprintf (text, sizeof (text), "%.2f", mDef.toPlain (normalized));
		}

		UString (string, str16BufferSize (String128)).assign (USTRING (text));
	}

private:
	ParamDef mDef;
};

} // anonymous namespace

//------------------------------------------------------------------------
tresult PLUGIN_API SpaceDubController::initialize (FUnknown* context)
{
	tresult result = EditControllerEx1::initialize (context);
	if (result != kResultOk)
		return result;

	for (int i = 0; i < kNumParams; ++i)
		parameters.addParameter (new SpaceDubParameter (kParams[i]));

	// Standard VST3 bypass, separate from the DXi's own "Enabled" switch.
	parameters.addParameter (STR16 ("Bypass"), nullptr, 1, 0,
	                         ParameterInfo::kCanAutomate | ParameterInfo::kIsBypass, kBypass);

	// Read-only: the processor writes the tempo it is locked to into this every
	// block, and the host delivers it here on the UI thread. Not automatable,
	// not saved - it is a readout, not a setting.
	parameters.addParameter (STR16 ("Host Tempo"), STR16 ("BPM"), 0, 0.0,
	                         ParameterInfo::kIsReadOnly, kHostTempoOut);

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpaceDubController::terminate ()
{
	mEditors.clear ();
	return EditControllerEx1::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpaceDubController::setComponentState (IBStream* state)
{
	if (!state)
		return kResultFalse;

	IBStreamer streamer (state, kLittleEndian);

	int32 version = 0;
	if (!streamer.readInt32 (version))
		return kResultFalse;

	int32 count = 0;
	if (!streamer.readInt32 (count))
		return kResultFalse;

	for (int32 i = 0; i < count; ++i)
	{
		double v = 0.0;
		if (!streamer.readDouble (v))
			return kResultFalse;
		if (i < kNumParams)
			setParamNormalized (static_cast<ParamID> (i), v);
	}

	int32 bypass = 0;
	if (streamer.readInt32 (bypass))
		setParamNormalized (kBypass, bypass ? 1.0 : 0.0);

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpaceDubController::setState (IBStream* /*state*/)
{
	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpaceDubController::getState (IBStream* /*state*/)
{
	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpaceDubController::setParamNormalized (ParamID tag, ParamValue value)
{
	const tresult result = EditControllerEx1::setParamNormalized (tag, value);
	if (result != kResultOk)
		return result;

	if (tag == kHostTempoOut)
	{
		// The processor telling us what the HOST supplied, or 0 for "nothing".
		// Zero is a meaningful value here, not a missing one: it is what sends
		// the tempo control back to showing the Tempo parameter. Everything
		// that quotes a time in milliseconds depends on this too.
		const double bpm = value * kHostTempoMax;
		if (bpm != mHostTempo)
		{
			mHostTempo = bpm;
			for (auto* editor : mEditors)
				editor->refreshAllReadouts ();
		}
		return result;
	}

	for (auto* editor : mEditors)
		editor->updateControl (tag, value);

	return result;
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API SpaceDubController::createView (FIDString name)
{
	if (name && FIDStringsEqual (name, ViewType::kEditor))
		return new SpaceDubEditor (this);
	return nullptr;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpaceDubController::notify (IMessage* message)
{
	if (message && FIDStringsEqual (message->getMessageID (), kSpaceDubSampleRateMessage))
	{
		double sampleRate = 0.0;
		if (message->getAttributes ()->getFloat (kSpaceDubSampleRateAttribute, sampleRate) == kResultOk &&
		    sampleRate > 1000.0 && sampleRate != mHostSampleRate)
		{
			mHostSampleRate = sampleRate;
			// The delay readouts are quoted in milliseconds, and the delay maps
			// onto a whole number of samples, so they depend on this.
			for (auto* editor : mEditors)
				editor->refreshAllReadouts ();
		}
		return kResultOk;
	}
	return EditControllerEx1::notify (message);
}

//------------------------------------------------------------------------
void SpaceDubController::editorAttached (EditorView* editor)
{
	if (auto* e = dynamic_cast<SpaceDubEditor*> (editor))
	{
		if (std::find (mEditors.begin (), mEditors.end (), e) == mEditors.end ())
			mEditors.push_back (e);
	}
}

//------------------------------------------------------------------------
void SpaceDubController::editorRemoved (EditorView* editor)
{
	editorDestroyed (editor);
}

//------------------------------------------------------------------------
void SpaceDubController::editorDestroyed (EditorView* editor)
{
	// Do NOT dynamic_cast here. EditorView::~EditorView() calls this, and by
	// then the SpaceDubEditor sub-object has already been destroyed, so the
	// object's dynamic type is plain EditorView and dynamic_cast<SpaceDubEditor*>
	// yields null - the entry would silently survive as a dangling pointer.
	// It only stays out of trouble because hosts normally call removed()
	// first; one that releases the view without removing it would leave a
	// freed editor in mEditors, and the next setParamNormalized() - which the
	// readouts made an unconditional walk of that list - would follow it.
	//
	// Comparing the upcast pointer is well defined at every point in the
	// destruction sequence, so it works on both paths.
	mEditors.erase (std::remove_if (mEditors.begin (), mEditors.end (),
	                                [editor] (SpaceDubEditor* e) {
		                                return static_cast<EditorView*> (e) == editor;
	                                }),
	                mEditors.end ());
}

//------------------------------------------------------------------------
} // namespace SpaceDub
