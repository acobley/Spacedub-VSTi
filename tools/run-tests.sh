#!/bin/sh
#-----------------------------------------------------------------------------
# Every suite this project has, in one command. build-release.sh runs it
# before it builds anything.
#
#   tools/run-tests.sh
#
# The delay-line suite needs no SDK and no host - a C++17 compiler is enough.
# The layout suite needs the VST3 SDK's headers (for the ParamID type only):
# external/vst3sdk, which the first configure fetches, or $VST3_SDK_ROOT.
#-----------------------------------------------------------------------------
cd "$(dirname "$0")/.."
ROOT="$(pwd)"
CXX="${CXX:-c++}"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
FAILED=0

printf '===== delay-line tests\n'
if $CXX -std=c++17 -O2 -I"$ROOT/source" "$ROOT/tests/DelayLineTests.cpp" \
        "$ROOT/source/DelayLine.cpp" -o "$OUT/delayline-tests"; then
	"$OUT/delayline-tests" || FAILED=$((FAILED + 1))
else
	echo "FAILED TO BUILD: delay-line tests"
	FAILED=$((FAILED + 1))
fi

printf '\n===== layout tests\n'
SDK="${VST3_SDK_ROOT:-$ROOT/external/vst3sdk}"
if [ ! -f "$SDK/pluginterfaces/vst/vsttypes.h" ]; then
	echo "NO SDK at $SDK - configure once, or set VST3_SDK_ROOT"
	FAILED=$((FAILED + 1))
elif $CXX -std=c++17 -O2 -I"$ROOT/source" -I"$SDK" "$ROOT/tests/LayoutTests.cpp" \
        -o "$OUT/layout-tests"; then
	"$OUT/layout-tests" || FAILED=$((FAILED + 1))
else
	echo "FAILED TO BUILD: layout tests"
	FAILED=$((FAILED + 1))
fi

#-----------------------------------------------------------------------------
# The release kit: the version and maker's name agree everywhere, and the
# release scripts' own refusals still refuse. None of this needs a Mac.
#-----------------------------------------------------------------------------
printf '\n===== check-versions\n'
python3 "$ROOT/tools/check-versions.py" || FAILED=$((FAILED + 1))
for t in test-build-release test-publish; do
	printf '\n===== installer/%s\n' "$t"
	if [ -x "$ROOT/installer/$t.sh" ]; then
		"$ROOT/installer/$t.sh" || FAILED=$((FAILED + 1))
	else
		# A script that will not run is not a check that passed.
		echo "NOT EXECUTABLE: installer/$t.sh - chmod +x installer/*.sh"
		FAILED=$((FAILED + 1))
	fi
done

printf '\n'
if [ "$FAILED" -eq 0 ]; then
	echo "All suites passed."
	exit 0
fi
echo "$FAILED suite(s) FAILED."
exit 1
