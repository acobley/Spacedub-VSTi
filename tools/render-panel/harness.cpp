// Renders the real SpaceDubEditor into a PNG through VSTGUI's cairo back end,
// at 2x. See build.sh. argv: out.png [id=normalised ...]
#include "SpaceDubController.h"
#include "SpaceDubEditor.h"
#include "SpaceDubParams.h"
#include "vstgui/lib/platform/linux/cairographicscontext.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/vstguiinit.h"
#include <cairo/cairo.h>
#include "pluginterfaces/base/ipluginbase.h"
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace VSTGUI;
using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace SpaceDub;

void* moduleHandle = nullptr;

int main (int argc, char** argv)
{
	const char* out = argc > 1 ? argv[1] : "panel.png";
	VSTGUI::init (nullptr);

	auto* controller = new SpaceDubController;
	controller->initialize (nullptr);
	// argv[2..]: id=value (normalised) settings
	for (int i = 2; i < argc; ++i)
	{
		unsigned id = 0; double v = 0;
		if (std::sscanf (argv[i], "%u=%lf", &id, &v) == 2)
			controller->setParamNormalized (id, v);
	}

	auto* editor = new SpaceDubEditor (controller);
	editor->open (nullptr, PlatformType::kDefaultNative);
	auto* frame = editor->getFrame ();

	const double scale = 2.0;
	auto* surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 1200 * 2, 640 * 2);
	cairo_surface_set_device_scale (surface, scale, scale);
	Cairo::SurfaceHandle handle (surface);
	auto device = std::make_shared<CairoGraphicsDevice> (nullptr);
	auto deviceContext = std::make_shared<CairoGraphicsDeviceContext> (*device, handle);
	{
		CDrawContext dc (deviceContext, CRect (0, 0, 1200, 640), 1.0);
		dc.beginDraw ();
		frame->CViewContainer::drawRect (&dc, CRect (0, 0, 1200, 640));
		dc.endDraw ();
	}
	cairo_surface_flush (surface);
	cairo_surface_write_to_png (surface, out);
	std::printf ("wrote %s\n", out);
	editor->close ();
	return 0;
}

// Link stubs: the module machinery the plug-in's entry point would bring.
extern "C" Steinberg::IPluginFactory* PLUGIN_API GetPluginFactory () { return nullptr; }
namespace Steinberg { namespace Linux { void setupVSTGUIRunloop (FUnknown*) {} } }
