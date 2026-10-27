// A LinuxFactory with no X11: just enough to draw into a cairo image.
#include "vstgui/lib/platform/linux/linuxfactory.h"
#include "vstgui/lib/platform/linux/cairogradient.h"
#include "vstgui/lib/platform/linux/cairographicscontext.h"
#include "vstgui/lib/platform/iplatformstring.h"
#include "vstgui/lib/platform/iplatformtimer.h"
#include "vstgui/lib/platform/iplatformbitmap.h"
#include "vstgui/lib/platform/iplatformfont.h"
#include "vstgui/lib/platform/iplatformframe.h"
#include "vstgui/lib/platform/iplatformresourceinputstream.h"
#include "vstgui/lib/platform/iplatformfileselector.h"
#include "vstgui/lib/idatapackage.h"
#include <chrono>

namespace VSTGUI {

struct StubString : IPlatformString
{
	void setUTF8String (UTF8StringPtr) override {}
};

struct LinuxFactory::Impl
{
	CairoGraphicsDeviceFactory deviceFactory;
	LinuxTaskExecutor executor;
};

LinuxFactory::LinuxFactory (void*) : impl (std::make_unique<Impl> ()) {}
void LinuxFactory::setResourcePath (const std::string&) const noexcept {}
std::string LinuxFactory::getResourcePath () const noexcept { return {}; }
void LinuxFactory::finalize () noexcept {}
uint64_t LinuxFactory::getTicks () const noexcept
{
	return std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::steady_clock::now ().time_since_epoch ()).count ();
}
PlatformFramePtr LinuxFactory::createFrame (IPlatformFrameCallback*, const CRect&, void*, PlatformType, IPlatformFrameConfig*) const noexcept { return nullptr; }
PlatformFontPtr LinuxFactory::createFont (const UTF8String&, const CCoord&, const int32_t&) const noexcept { return nullptr; }
bool LinuxFactory::getAllFontFamilies (const FontFamilyCallback&) const noexcept { return false; }
PlatformBitmapPtr LinuxFactory::createBitmap (const CPoint&) const noexcept { return nullptr; }
PlatformBitmapPtr LinuxFactory::createBitmap (const CResourceDescription&) const noexcept { return nullptr; }
PlatformBitmapPtr LinuxFactory::createBitmapFromPath (UTF8StringPtr) const noexcept { return nullptr; }
PlatformBitmapPtr LinuxFactory::createBitmapFromMemory (const void*, uint32_t) const noexcept { return nullptr; }
PNGBitmapBuffer LinuxFactory::createBitmapMemoryPNGRepresentation (const PlatformBitmapPtr&) const noexcept { return {}; }
PlatformResourceInputStreamPtr LinuxFactory::createResourceInputStream (const CResourceDescription&) const noexcept { return nullptr; }
PlatformStringPtr LinuxFactory::createString (UTF8StringPtr) const noexcept { return makeOwned<StubString> (); }
PlatformTimerPtr LinuxFactory::createTimer (IPlatformTimerCallback*) const noexcept { return nullptr; }
bool LinuxFactory::setClipboard (const DataPackagePtr&) const noexcept { return false; }
SharedPointer<IDataPackage> LinuxFactory::getClipboard () const noexcept { return nullptr; }
PlatformGradientPtr LinuxFactory::createGradient () const noexcept { return std::make_unique<Cairo::Gradient> (); }
PlatformFileSelectorPtr LinuxFactory::createFileSelector (PlatformFileSelectorStyle, IPlatformFrame*) const noexcept { return nullptr; }
const IPlatformGraphicsDeviceFactory& LinuxFactory::getGraphicsDeviceFactory () const noexcept { return impl->deviceFactory; }
const IPlatformTaskExecutor& LinuxFactory::getTaskExecutor () const noexcept { return impl->executor; }
bool LinuxFactory::replaceTaskExecutor (const ReplaceTaskExecFunc&) const noexcept { return false; }
const LinuxFactory* LinuxFactory::asLinuxFactory () const noexcept { return this; }
const MacFactory* LinuxFactory::asMacFactory () const noexcept { return nullptr; }
const Win32Factory* LinuxFactory::asWin32Factory () const noexcept { return nullptr; }
CairoGraphicsDeviceFactory& LinuxFactory::getCairoGraphicsDeviceFactory () const noexcept { return impl->deviceFactory; }

} // VSTGUI
