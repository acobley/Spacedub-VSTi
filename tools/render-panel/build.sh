#!/bin/sh
#-----------------------------------------------------------------------------
# Draw the REAL editor - SpaceDubEditor, SpaceDubPanelViews, the controller -
# into a PNG through VSTGUI's cairo back end, with no host and no window.
#
#   tools/render-panel/build.sh [out.png] [id=normalised ...]
#
# e.g. tools/render-panel/build.sh doc/plugin-window.png
#      tools/render-panel/build.sh /tmp/on.png 0=1        (Enabled on)
#
# Written for Linux (it was how the panel was checked from a session with no
# Mac): it needs g++, cairo's headers and libraries, and the VST3 SDK in
# external/vst3sdk or $VST3_SDK_ROOT. It needs NONE of VSTGUI's X11, xcb or
# pango dependencies: stubfactory.cpp is a LinuxFactory that only draws, and
# pango/pangocairo.h stands in for pango (the panel draws no font text).
#
# The SDK libraries come from a plain configure of the SDK with VSTGUI off:
#   cmake -B build-sdk -S "$SDK" -DSMTG_ENABLE_VSTGUI_SUPPORT=OFF && cmake --build build-sdk --target sdk
#-----------------------------------------------------------------------------
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SDK="${VST3_SDK_ROOT:-$ROOT/external/vst3sdk}"
VG="$SDK/vstgui4"
SRC="$ROOT/source"
OBJ="${RENDER_OBJ:-$ROOT/build-render}"
LIBS="${SDK_LIBS:-$ROOT/build-sdk/lib/Debug}"
mkdir -p "$OBJ"

if [ ! -f "$LIBS/libsdk.a" ]; then
	cmake -B "$ROOT/build-sdk" -S "$SDK" -DSMTG_ENABLE_VSTGUI_SUPPORT=OFF -DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF \
	      -DSMTG_ENABLE_VST3_HOSTING_EXAMPLES=OFF -DSMTG_RUN_VST_VALIDATOR=OFF -DCMAKE_BUILD_TYPE=Debug >/dev/null
	cmake --build "$ROOT/build-sdk" --target sdk -j4 >/dev/null
	LIBS="$ROOT/build-sdk/lib/Debug"
fi

FLAGS="-std=c++17 -O1 -DDEVELOPMENT=1 -DSMTG_OS_LINUX=1 -DVSTGUI_NEEDS_INIT_LIB=0 -I$HERE -isystem $SDK -isystem $VG -I$SRC $(pkg-config --cflags cairo) -w"
OBJS=""
for f in "$VG/vstgui/vstgui.cpp" \
         "$VG/vstgui/lib/platform/linux/cairobitmap.cpp" \
         "$VG/vstgui/lib/platform/linux/cairographicscontext.cpp" \
         "$VG/vstgui/lib/platform/linux/cairogradient.cpp" \
         "$VG/vstgui/lib/platform/linux/cairopath.cpp" \
         "$VG/vstgui/lib/platform/linux/linuxtaskexecutor.cpp" \
         "$SDK/public.sdk/source/vst/vstguieditor.cpp" \
         "$HERE/stubfactory.cpp" "$HERE/harness.cpp" \
         "$SRC/SpaceDubPanelViews.cpp" "$SRC/SpaceDubEditor.cpp" "$SRC/SpaceDubController.cpp" \
         "$SRC/SpaceDubParams.cpp" "$SRC/DelayLine.cpp"; do
	o="$OBJ/$(basename "$f" .cpp).o"
	if [ ! -f "$o" ] || [ "$f" -nt "$o" ]; then
		g++ $FLAGS -c "$f" -o "$o"
	fi
	OBJS="$OBJS $o"
done
g++ -o "$OBJ/render-panel" $OBJS "$LIBS/libsdk.a" "$LIBS/libsdk_common.a" "$LIBS/libbase.a" \
    "$LIBS/libpluginterfaces.a" $(pkg-config --libs cairo) -lpthread -ldl

OUT="${1:-$ROOT/doc/plugin-window.png}"
[ $# -gt 0 ] && shift
"$OBJ/render-panel" "$OUT" "$@"
