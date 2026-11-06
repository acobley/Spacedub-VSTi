# SpaceDub 2.2.1.0

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

## What changed since 2.2.0.0

- **Dying echoes no longer eat CPU.** With the filter off, every fading
  repeat spent hundreds of trips round the delay line as "denormal" numbers,
  far too small to hear but slow to calculate on Intel Macs: the delay took
  7× as long at 90 % feedback, and 16× with Taps Feedback on. Anything below
  10⁻³⁰ (−600 dB) is now set to zero where the line is written, and each
  processing block runs with the CPU's flush-to-zero mode on, putting the
  host's setting back afterwards. The same two cases now take 168 ms and
  208 ms of CPU where they took 715 ms and 2,873 ms.

Nothing you can hear has changed: above that floor the output is
bit-identical to 2.2.0.0. Parameters, presets and saved projects are
exactly as before.

## Installing

Download `SpaceDub-2.2.1.0.pkg` and open it. The installer offers the two
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
SHA-256: 46d1d48b58b3d5d701303e93efdbd3ab9ccfaadf4aee8e3fdcc8bdbc0030bf0c
```

Check it after downloading:

```sh
shasum -a 256 SpaceDub-2.2.1.0.pkg
```

---

Copyright 2026 A. E. Cobley. CC BY-SA 4.0.
VST is a trademark of Steinberg Media Technologies GmbH.
