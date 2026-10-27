# SpaceDub 2.1.0.0

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
records where it deliberately differs and why.

## What changed since 2.0.1.0

- **A new panel.** The window is now a 1200 × 640 black hardware-style
  panel — knurled knobs, rotary selectors, small OLED screens — in place of
  the bitmap copy of the Windows dialog. Every control from before is still
  there, grouped into Echo, Clock, Delay, Feedback, Filter and Output, with
  the left channel's row above the right's and the four LINK switches
  between them.
- **The tap scope.** A screen in the Echo section draws each channel's five
  taps where they land in time, how much of each is heard on its own side
  and how much crosses to the other, and **how much signal is going through
  every tap right now**, with its level in dBFS.
- **Dry Level replaces Straight Thru.** The on/off switch for the dry signal
  is now a **Dry** knob, 0–100 %, so you can mix the input in with the
  echoes rather than only all or nothing.
- **A feedback level for each tap.** With **Taps FB** on, an amber handle
  sits over every tap on the tap scope: drag it to set how much that tap
  feeds back, 0–200 % of Loop Scale. Link Loop Scale links them left to
  right.

**Projects and presets from earlier versions open unchanged.** The plug-in's
identity codes are the same. A saved Straight Thru *on* loads as Dry 100 %
and *off* as 0 %, and every tap's feedback starts at 100 %, which is exactly
the old single amount — so an old session sounds as it did.

Hosts will list eleven parameters they did not before or under a new name:
**Dry Level** (was Straight Thru) and **Tap 1–5 Feedback Left / Right**. Some
also list ten read-only **Tap … Level** meters, which the panel uses and
which cannot be automated.

## Installing

Download `SpaceDub-2.1.0.0.pkg` and open it. The installer offers the two
formats separately:

    /Library/Audio/Plug-Ins/VST3/SpaceDub.vst3
    /Library/Audio/Plug-Ins/Components/SpaceDub.component

Quit your DAW first — a running host holds the old copy open. Signed with a
Developer ID and notarised by Apple.

Requires macOS 10.13 or later.

## Three things that look like faults and are not

* **It makes no sound until you switch it on.** The **Enabled** lamp starts
  *off*, exactly as the original shipped, and the header says so. Until it is
  lit the plug-in passes audio straight through. This is the first thing
  anyone reports.
* **Filter Gain starts at 250 %.** Also as the original shipped. Combined with
  resonance it is loud by design — bring it down before raising feedback.
* **`Sync Rate` has eight entries but seven distinct values.** The DXi's
  `switch` mapped both of the last two to a whole note. Kept so automation
  values line up with the original; the panel prints seven positions.

Two more worth knowing:

* **Presets saved by the Windows DXi cannot be read.** The value ranges and the
  stream layout both changed in the port; there is no conversion path.
* **`FB Out` does not change how much regenerates.** It scales the filtered
  tap on its way out, after the point where feedback is taken. See
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
SHA-256: 82cf37f99c902d5af819b683fafc60beb1ff86a56999f4a11cc5ba5cfca87fae
```

Check it after downloading:

```sh
shasum -a 256 SpaceDub-2.1.0.0.pkg
```

---

Copyright 2026 A. E. Cobley. CC BY-SA 4.0.
VST is a trademark of Steinberg Media Technologies GmbH.
