# SpaceDub 2.0.0.1

A **five-tap stereo dub delay with a resonant ladder filter in the feedback
path**, for macOS, as a **VST3** and an **Audio Unit**.

Each channel runs an eight-second delay line read at five taps. The taps are
panned by index — the last one hard in its own channel, each earlier one 10 %
further toward the centre — so repeats spread across the image as they decay.
Tap 5 can be routed through a four-pole Moog-style ladder filter with cutoff,
resonance and gain, and fed back into the line, which is where the dub-siren
sweeps come from. A separate tap-to-tap regeneration path lets each tap feed
the one before it.

The delay runs free or locks to the host tempo, quantised to a grid from
1/32 to a whole note. When the host supplies a tempo the delay follows it live,
and the panel shows the tempo it is locked to.

This is a port of the 2003 Windows Cakewalk DXi of the same name. The DSP is
carried over line by line, so it sounds like the original; `PORTING-NOTES.md`
records the four places it deliberately differs and why.

## Installing

Download `SpaceDub-2.0.0.1.pkg` and open it. The installer offers the two
formats separately:

    /Library/Audio/Plug-Ins/VST3/SpaceDub.vst3
    /Library/Audio/Plug-Ins/Components/SpaceDub.component

Quit your DAW first — a running host holds the old copy open. Signed with a
Developer ID and notarised by Apple.

Requires macOS 10.13 or later.

## Three things that look like faults and are not

* **It makes no sound until you switch it on.** The `Enabled` switch starts
  *off*, exactly as the original shipped. Until it is lit the plug-in passes
  audio straight through. This is the first thing anyone reports.
* **Filter Gain starts at 250 %.** Also as the original shipped. Combined with
  resonance it is loud by design — bring it down before raising feedback.
* **`Sync Rate` has eight entries but seven distinct values.** The DXi's
  `switch` mapped both of the last two to a whole note. Kept so automation
  values line up with the original.

Two more worth knowing:

* **Presets saved by the Windows DXi cannot be read.** The value ranges and the
  stream layout both changed in the port; there is no conversion path.
* **`Feedback Out` does not change how much regenerates.** It scales the
  filtered tap on its way out, after the point where feedback is taken. See
  `doc/signal-routing.png` — this is inherited from the original, not a bug.

## Known gaps

* No preset library ships with it; the defaults are the DXi's.
* The five taps are evenly spaced. The original had no way to offset them
  individually either.

Universal binary — Apple Silicon and Intel.

## Uninstalling

A `.pkg` never will, so:

```sh
sudo rm -rf /Library/Audio/Plug-Ins/VST3/SpaceDub.vst3
sudo rm -rf /Library/Audio/Plug-Ins/Components/SpaceDub.component
```

## Checksum

```
PASTE-THE-SHA256-HERE  SpaceDub-2.0.0.1.pkg
```

Take it from the finished, stapled package — stapling changes the bytes:

```sh
shasum -a 256 installer/SpaceDub-2.0.0.1.pkg
```

---

Copyright 2026 A. E. Cobley. CC BY-SA 4.0.
VST is a trademark of Steinberg Media Technologies GmbH.
