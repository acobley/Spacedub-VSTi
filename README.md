# SpaceDub — VST3 & Audio Unit

A macOS **VST3 and Audio Unit** port of **SpaceDub**, the 2003 Cakewalk DXi
stereo multi-tap delay by A. E. Cobley. The DSP, the parameter set and the control layout are
carried over from the original Visual C++ 6 / MFC project in `../SpaceDub`.

---

## Building

You need **Xcode** (with command line tools) and **CMake 3.25 or newer**.
If you don't have CMake: `brew install cmake`.

```sh
./setup-xcode.sh
open build/SpaceDub.xcodeproj
```

Then pick the **SpaceDub** scheme in Xcode and hit ⌘B.

The first configure clones Steinberg's VST3 SDK (about 250 MB) into
`external/vst3sdk` — that takes a few minutes and needs a network connection.
Every later configure reuses it. If you already have the SDK somewhere:

```sh
cmake -B build -G Xcode -DVST3_SDK_ROOT=/path/to/vst3sdk
```

### Command line instead of Xcode

```sh
cmake -B build -G Xcode
cmake --build build --config Release
```

The built bundle lands in `build/VST3/Release/SpaceDub.vst3`, and CMake also
drops a symlink into `~/Library/Audio/Plug-Ins/VST3/` so your DAW finds it
straight away.

### Turning the validator off

By default Steinberg's `validator` is built and run after every build, which
reports VST3 conformance problems in the build log. It also adds a host
application to the project and a step to every build. To skip it:

```sh
./setup-xcode.sh --no-validator --clean
```

Or, if you are calling CMake yourself:

```sh
cmake -B build -G Xcode -DSMTG_RUN_VST_VALIDATOR=OFF
```

`--minimal` goes one further and also skips `moduleinfo.json`
(`-DSMTG_CREATE_MODULE_INFO=OFF`), which removes the `moduleinfotool` target
as well. That file is only a scanning hint for hosts — the plug-in works
without it — but it is cheap, so prefer `--no-validator` unless you are
chasing build time.

Worth keeping in mind: the validator is the closest thing to a test suite this
project has. It is a good idea to turn it back on before you ship a build, or
run it by hand:

```sh
build/bin/Release/validator build/VST3/Release/SpaceDub.vst3
```

### Audio Unit (.component)

On macOS with the Xcode generator, an **Audio Unit v2** wrapper is built
alongside the VST3, so SpaceDub also loads in Logic, GarageBand and anything
else that only takes AUs. It is Steinberg's wrapper from
`public.sdk/source/vst/auwrapper`: the `.component` bundle contains the VST3
and loads it, so there is no second implementation of the plug-in to keep in
step — one set of sources, one DSP, two plug-in formats.

Building the **SpaceDub-au** scheme produces `build/VST3/<config>/SpaceDub.component`
and copies it to `~/Library/Audio/Plug-Ins/Components/`. Logic rescans on next
launch; to force a rescan, quit Logic and `killall -9 AudioComponentRegistrar`.

Check it the way Apple's own tooling does:

```sh
auval -v aufx SDub AECo
```

That is the AU counterpart of the VST3 validator, and worth running before you
ship — it is stricter about channel layouts and parameter behaviour than most
hosts are.

Two things to know:

* The first configure also clones **Apple's AudioUnitSDK** (small, a few MB)
  into `external/AudioUnitSDK`; Steinberg's wrapper needs it. Skip the whole
  thing with `./setup-xcode.sh --no-au` (or `-DSPACEDUB_BUILD_AU=OFF`). If the
  clone fails the Audio Unit is skipped with a warning — it never breaks the
  VST3 build.
* That SDK is **pinned to AudioUnitSDK-1.1.0**, and the pin matters: Steinberg
  compiles the wrapper as C++17, and AudioUnitSDK 1.3.0 onwards uses
  `std::span` and concepts in its headers. Building those together fails with
  *"no member named 'span' in namespace 'std'"*. If you point
  `-DSPACEDUB_AU_SDK_TAG=` at a newer release, the build raises the C++
  standard on the Audio Unit target for you. A checkout in `external/` left
  over from a different pin is detected and refetched.
* The Audio Unit needs the **Xcode generator**; the SDK only defines the AUv2
  helper there. `--makefiles` gets you the VST3 only.

The plug-in's identity to macOS — the `aufx` / `SDub` / `AECo` four-character
codes — lives in `resource/au-info.plist`. Once you have shipped a build, those
are as fixed as the class UIDs in `SpaceDubIDs.h`: change one and every session
using the plug-in loses it.

### Universal binary

Xcode builds for your own architecture by default. For a fat Intel +
Apple Silicon binary:

```sh
cmake -B build -G Xcode -DSMTG_BUILD_UNIVERSAL_BINARY=ON
```

### Troubleshooting

**`No CMAKE_C_COMPILER could be found`**

CMake cannot find a working compiler. On macOS this is nearly always the
developer-tools selection rather than anything in this project. Check what is
selected:

```sh
xcode-select -p
```

| What it prints | What to do |
|---|---|
| `/Library/Developer/CommandLineTools` | The Xcode generator needs *full* Xcode. Install Xcode from the App Store, then `sudo xcode-select -s /Applications/Xcode.app/Contents/Developer` — or skip Xcode entirely with `./setup-xcode.sh --makefiles` |
| `/Applications/Xcode.app/Contents/Developer` | The licence is probably unaccepted: `sudo xcodebuild -license accept` |
| an error, or nothing | `xcode-select --install` |

Then — and this part matters — **delete the failed cache**. CMake stores the
failure in `build/CMakeCache.txt` and will keep reporting the same error even
after you have fixed the toolchain:

```sh
./setup-xcode.sh --clean
```

`setup-xcode.sh` now checks all of this before it runs CMake and tells you
exactly which case you are in.

**`Apple Development: no identity found`** — you are on a build from before the
signing fix. Reconfigure (`./setup-xcode.sh --clean`); signing now defaults to
ad-hoc and needs no certificate. See **Signing** above.

**Don't have Xcode and don't want it?** `./setup-xcode.sh --makefiles` builds
the plug-in with just the Command Line Tools. You get
`build-make/VST3/SpaceDub.vst3` — a working plug-in, just no `.xcodeproj`.

### Signing

Both bundles are **ad-hoc signed automatically** after every build, which needs
no Apple developer certificate and is enough for macOS to load the plug-in,
including on Apple Silicon. You do not have to do anything.

To sign with a real identity instead — for anything you hand to someone else —
pass it at configure time:

```sh
cmake -B build -G Xcode \
      -DSPACEDUB_CODE_SIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
```

`security find-identity -v -p codesigning` lists what you have.

> **Why this is not left to the SDK.** Steinberg defaults
> `SMTG_CODE_SIGN_IDENTITY_MAC` to `"Apple Development"` and its Audio Unit
> post-build step runs `/usr/bin/codesign -s "Apple Development"`
> unconditionally. On a machine with no certificate that fails with
> *"Apple Development: no identity found"* and takes the whole build down after
> everything has already compiled. So unless you supply an identity, the SDK's
> signing is switched off and the bundles are ad-hoc signed by a step in this
> project's `CMakeLists.txt` instead.

---

## What it is

Two independent delay lines (one per channel), each eight seconds long with
five taps. Each line has a Moog-style four-pole ladder filter in its feedback
path, plus two separate regeneration paths: **main feedback**, which folds the
filtered last tap back into the line, and **taps feedback**, which folds each
tap into the one before it. The five taps are panned across the stereo field
in 10 % steps, with tap 5 hard in its own channel — that spreading is what
gives SpaceDub its width.

The delay time can free-run or lock to the host tempo (Off, 32nd … Whole),
and there is a manual tempo for hosts that don't report one.

## The interface

The window is the original 766 × 499 "Space Dub" panel. Every control sits
where it sat in the Windows dialog — positions were converted from the
`IDD_PROPPAGE` dialog units in `SpaceDubMidi.rc`.

| Area | Controls |
|---|---|
| Left / right column of vertical sliders | Delay, Taps Feedback (loop scale), Main Feedback, Cutoff Frequency, Resonance |
| Top-right horizontal sliders | Taps out gain, Feedback out gain, Filter gain — Left row above, Right row below |
| Row of buttons under the logo | Link Delay / Taps / Feedback / Filter — links the left and right sliders |
| Buttons above them | Main Feedback on, Taps Feedback on, Taps Out |
| Straight Thru | passes the dry signal to the output |
| Power (top right) | the DXi "Enabled" switch |
| Black panel (top right) | tempo readout and sync division — drag or click to change |
| Narrow buttons at the bottom | Filter on, per channel |

Sliders: drag, or hold **Shift** for fine adjustment; the mouse wheel works
too. The tempo and sync readouts advance one step per click and can be
dragged vertically.

Every slider has a value readout under it — the delay sliders in milliseconds
(plus the number of sync steps when tempo-locked), the cutoff sliders in hertz,
the rest as percentages. Each sits on a small black plate with a thin white
border, so it stays readable against the panel artwork, and they are always
visible — no hovering needed. The
switches don't need one: their colour shows their state.

> **Note** — as in the original, the **Power** switch is *off* by default and
> the plug-in passes audio straight through until you turn it on.

## Layout of this project

```
CMakeLists.txt        build definition, fetches the SDK
setup-xcode.sh        one-shot configure helper
source/
  DelayLine.*         the ported DSP (was DelayLine.cpp in the DXi)
  SpaceDubParams.*    parameter table (was Parameters.h + MediaParams)
  SpaceDubProcessor.* audio processing (was CSpaceDubMidi::Process)
  SpaceDubController.*parameter/host plumbing (was CMediaParams)
  SpaceDubEditor.*    the window (was CSpaceDubMidiPropPage)
  SpaceDubControls.*  sliders/buttons (were msctls_trackbar32 and MFC buttons)
  SpaceDubEntry.cpp   plug-in factory (was PlugInApp.cpp)
resource/
  *.png               UI artwork, converted from res/*.bmp
  au-info.plist       Audio Unit identity (type/subtype/manufacturer codes)
tests/
  DelayLineTests.cpp  standalone DSP checks - no SDK, no host needed
external/
  vst3sdk/            the VST3 SDK (fetched, not in version control)
  AudioUnitSDK/       Apple's AudioUnitSDK, for the AU wrapper (likewise)
```

`PORTING-NOTES.md` records exactly how each DXi parameter, range and DSP quirk
maps onto this version — read it before changing the DSP.

---

Copyright 2026 A. E. Cobley. Licensed under
[CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) — see
[`LICENSE`](LICENSE). Credit it, and share anything you build on it under the
same terms.

The Steinberg VST3 SDK, VSTGUI and Apple's AudioUnitSDK are not covered by
that: they are fetched into `external/` at configure time and carry their own
licence terms.

VST is a trademark of Steinberg Media Technologies GmbH.
