# SpaceDub 2.2.0.0

A **five-tap stereo dub delay with a resonant ladder filter in the feedback
path**, for macOS, as a **VST3** and an **Audio Unit**.

Each channel runs an eight-second delay line read at five taps. The taps are
panned by index — the last one hard in its own channel, each earlier one 10 %
further toward the centre — so repeats spread across the image as they decay.
Tap 5 can be routed through a four-pole Moog-style ladder filter — low-pass or
high-pass — with cutoff, resonance and gain, and fed back into the line, which
is where the dub-siren sweeps come from. A separate tap-to-tap regeneration path lets each tap feed
the one before it.

The delay runs free or locks to the host tempo, quantised to a grid from
1/32 to a whole note. When the host supplies a tempo the delay follows it live,
and the panel shows the tempo it is locked to.

This is a port of the 2003 Windows Cakewalk DXi of the same name. The DSP is
carried over line by line, so it sounds like the original; `PORTING-NOTES.md`
records where it deliberately differs and why.

## What changed since 2.1.0.0

- **Taps on the grid.** With **Sync Rate** on, every tap now lands on the
  beat grid, not just the last. With the echo at N steps of the sync rate,
  tap 5 sits on exactly N steps and taps 4 to 1 halve down from the power of
  two below it — at 3 sixteenths, the taps fall at ¼, ½, 1, 2 and 3
  sixteenths. Before, taps 1–4 sat at even fifths of the echo, between the
  notes. Free-running is unchanged.
- **High-pass filter.** Each side's ladder filter can now be a **low-pass** or
  a **high-pass**, 24 dB per octave either way, with a **LP / HP** selector
  under its On switch. Cutoff, Resonance and Gain work the same in both modes;
  Link Filters links the selectors.
- **Filter Taps.** Until now only tap 5 went through the filter. A new
  **Taps** switch in the Filter section sends taps 1–4 through it too, each
  through a filter of its own, on any side whose filter is on. It changes
  what you hear from those taps only — the feedback paths are untouched.
- The tap scope marks every filtered tap **LP** or **HP**, and the Filter
  section's Resonance and Gain knobs have moved slightly left to make room.

**Projects and presets from earlier versions open, with one difference.** New
parameters start where the old sound was: Filter Taps off, both filters
low-pass. But **a project saved with Sync Rate on will sound different**:
its taps 1–4 move onto the grid, and tap 5 moves by a few samples to sit on
it exactly. Free-running projects are unchanged.

Hosts will list three parameters they did not before: **Filter Taps** and
**Filter Mode Left / Right** (shown as Low Pass / High Pass).

## Installing

Download `SpaceDub-2.2.0.0.pkg` and open it. The installer offers the two
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

Four more worth knowing:

* **Presets saved by the Windows DXi cannot be read.** The value ranges and the
  stream layout both changed in the port; there is no conversion path.
* **`FB Out` does not change how much regenerates.** It scales the filtered
  tap on its way out, after the point where feedback is taken. See
  `doc/signal-routing.png` — this is inherited from the original, not a bug.
* **The amber handles on the tap scope are feedback, not volume.** Each sets
  how much its tap regenerates into the tap before it, and shows only while
  Taps FB is on. A tap's level is set per side, by Taps Gain (and FB Out for
  the filtered tap 5). `doc/signal-routing.png` shows where each sits.
* **The Cutoff reading is the original's number.** The filter's −3 dB point
  is more than an octave below it, as it always was in the DXi.

## Known gaps

* No preset library ships with it; the defaults are the DXi's.
* Free-running, the five taps are evenly spaced; they cannot be offset
  individually. There is no per-tap level.

Universal binary — Apple Silicon and Intel.

## Uninstalling

A `.pkg` never will, so:

```sh
sudo rm -rf /Library/Audio/Plug-Ins/VST3/SpaceDub.vst3
sudo rm -rf /Library/Audio/Plug-Ins/Components/SpaceDub.component
```

## Checksum

```
SHA-256: 8998998efc0d1d67324b5ac34a66acf2b81d3dfa7d46c193faee13c26e8e5c22
```

Check it after downloading:

```sh
shasum -a 256 SpaceDub-2.2.0.0.pkg
```

---

Copyright 2026 A. E. Cobley. CC BY-SA 4.0.
VST is a trademark of Steinberg Media Technologies GmbH.
