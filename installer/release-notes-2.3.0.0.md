# SpaceDub 2.3.0.0

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

## What changed since 2.2.2.0

- **Tempo changes no longer click.** When the tempo changed, every tap
  jumped to its new place in a single sample: a sudden change clicked, and a
  tempo ramp — which a host sends as a slightly different tempo every few
  milliseconds — made a zipper noise. Now small moves **glide**, the delay
  sliding smoothly to stay on the beat, and jumps — a tempo step, a new Sync
  Rate, a turn of Delay — **crossfade** over 30 ms. With Taps FB on, the
  regeneration moves just as smoothly, so the click does not come back one
  echo later.
- **A synced delay keeps its note value when the tempo changes.** Set to
  3 sixteenths at 120 bpm (375 ms), it went to 5 sixteenths (436.7 ms)
  at 171.7 bpm: the Delay knob held a *time* and re-counted the steps at the
  new tempo. It now holds the *number of steps* — 3 sixteenths stays
  3 sixteenths, 262 ms at 171.7 bpm — and the panel's step count stays put.
  Found in a REAPER project with a tempo change.
- **An About box.** Click the **SPACE DUB** logo for the version, the exact
  build, whether it is running as a VST3 or an Audio Unit, the host, the
  audio settings (sample rate, channels, block size, realtime or offline),
  the tempo, and the folder it was loaded from. Click anywhere, or press
  Esc, to close it.
- **Requires macOS 12.0 or later**, which is the oldest system Xcode 27 can
  build for.

**Projects and presets from 2.2.2.0 open unchanged, with one difference.** A
project saved **with Sync Rate on at a tempo other than 120 bpm** may come
back with a different number of steps on Delay — the number the knob shows at
120 bpm, which is now what it means at every tempo. Re-set Delay if it moved.
At 120 bpm, and free-running, nothing has changed: with the tempo and the
knobs still, the output is bit-identical to 2.2.2.0. No parameters were added
or removed.

## Installing

Download `SpaceDub-2.3.0.0.pkg` and open it. The installer offers the two
formats separately:

    /Library/Audio/Plug-Ins/VST3/SpaceDub.vst3
    /Library/Audio/Plug-Ins/Components/SpaceDub.component

Quit your DAW first — a running host holds the old copy open. Signed with a
Developer ID and notarised by Apple.

Requires **macOS 12.0 or later** (2.2.2.0 and earlier ran on 10.13). The
installer refuses older systems rather than installing a plug-in that will
not load.

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

Five more worth knowing:

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
* **During a tempo ramp the echoes bend in pitch, slightly.** That is the
  delay sliding to stay on the beat, the way a tape delay would — about a
  semitone at most, usually a few cents. A sudden change crossfades
  instead, with no bend.

## Known gaps

* No preset library ships with it; the defaults are the DXi's.
* Free-running, the five taps are evenly spaced; they cannot be offset
  individually. There is no per-tap level.
* The delay line is eight seconds. A long synced delay at a slow tempo can
  outgrow it — 16 quarter notes at 60 bpm, say — and then drops to the most
  steps that fit.

Universal binary — Apple Silicon and Intel.

## Uninstalling

A `.pkg` never will, so:

```sh
sudo rm -rf /Library/Audio/Plug-Ins/VST3/SpaceDub.vst3
sudo rm -rf /Library/Audio/Plug-Ins/Components/SpaceDub.component
```

## Checksum

```
SHA-256: <paste from: shasum -a 256 installer/SpaceDub-2.3.0.0.pkg>
```

Check it after downloading:

```sh
shasum -a 256 SpaceDub-2.3.0.0.pkg
```

---

Copyright 2026 A. E. Cobley. CC BY-SA 4.0.
VST is a trademark of Steinberg Media Technologies GmbH.
