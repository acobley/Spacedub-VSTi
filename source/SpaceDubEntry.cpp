//------------------------------------------------------------------------
// SpaceDub - plug-in factory
//
// The DXi equivalent was PlugInApp.cpp's DllRegisterServer / class factory.
//------------------------------------------------------------------------

#include "SpaceDubIDs.h"
#include "SpaceDubProcessor.h"
#include "SpaceDubController.h"
#include "version.h"

#include "public.sdk/source/main/pluginfactory.h"

#define stringPluginCategory "Fx|Delay"

using namespace Steinberg::Vst;
using namespace SpaceDub;

//------------------------------------------------------------------------
BEGIN_FACTORY_DEF (stringCompanyName, "https://www.spacedub.audio", "mailto:info@spacedub.audio")

	DEF_CLASS2 (INLINE_UID_FROM_FUID (kSpaceDubProcessorUID),
	            PClassInfo::kManyInstances,
	            kVstAudioEffectClass,
	            stringPluginName,
	            Vst::kDistributable,
	            stringPluginCategory,
	            FULL_VERSION_STR,
	            kVstVersionString,
	            SpaceDub::SpaceDubProcessor::createInstance)

	DEF_CLASS2 (INLINE_UID_FROM_FUID (kSpaceDubControllerUID),
	            PClassInfo::kManyInstances,
	            kVstComponentControllerClass,
	            stringPluginName "Controller",
	            0,
	            "",
	            FULL_VERSION_STR,
	            kVstVersionString,
	            SpaceDub::SpaceDubController::createInstance)

END_FACTORY
