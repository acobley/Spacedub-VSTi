# SpaceDub: DXi → VST3 port notes

The port lives in **`~/DXi-DEv/Spacedub-VSTi`**. (It was called
`Spaceduo-VSTi` until 4 September 2026 — a build tree from before the rename
will not reconfigure, because CMake caches the absolute path. Delete `build/`
and re-run `./setup-xcode.sh` if you meet one.)

Everything here refers to the original project in `../SpaceDub`
(Visual C++ 6, MFC, Cakewalk DirectX Plug-In Wizard, 2003).

---

## 1. What the plug-in became

The DXi registered itself as a *synth* (`CDXi` / `CSoftSynth` / `IMfxSoftSynth2`)
even though its DSP is an audio effect: it needs an input signal, and it used
the MFX host connection only to read the tempo map. VST3 has a proper tempo
field on `ProcessContext`, so the port is a plain **audio effect**,
category `Fx|Delay`, with a stereo in / stereo out bus and an event input
declared so hosts will still route MIDI to it if you want one.

Consequences:

* `CSoftSynth`, `CInstrument`, `CStringMap`, `MidiFilter.h`, `MfxTime.h` and
  the whole MFX COM layer are **gone** — nothing in the audio path used them.
  `CSoftSynth::OnEvents` only queued note data that the DSP never read.
* `CMediaParams` / `CParamEnvelope` are replaced by VST3 parameters. The
  envelope-decimation machinery is gone; per-sample smoothing is done
  differently (see §4).
* `PersistLoad` / `PersistSave` become `getState` / `setState` on an
  `IBStreamer`. **Old DXi presets cannot be read** — the stream layout and the
  value ranges both differ.
* `CSpaceDubMidiPropPage` (an `COlePropertyPage`) becomes a `VSTGUIEditor`.

`SpaceDub.cpp` and `SpaceDubMidi.cpp` contained two near-identical copies of
the processing loop. The port follows **`SpaceDubMidi.cpp`**, the later of the
two — it is the one with `Straight Thru` (`PARAM_MAINOUT`) and the tempo sync.

---

## 2. Parameter map

The DXi kept two ranges per parameter: the *external* range the user saw
(`Parameters.h`) and the *internal* range the DSP was handed, converted by
`ParamInfo::MapToInternal`. VST3 works in normalised 0..1 plus a plain value,
so `ParamDef` in `SpaceDubParams.h` carries all three and `toInternal()`
reproduces `MapToInternal` exactly. **The DSP therefore receives numerically
identical values to the original.**

| DXi id | DXi label | VST3 name | User range | DSP range | Default |
|---|---|---|---|---|---|
| `PARAM_ENABLE` | Enabled | Enabled | 0/1 | 0/1 | **0 (off)** |
| `PARAM_DELAYLEFT` | DelayLeft | Delay Left | 0–100 % | 0–1 | 20 |
| `PARAM_DELAYRIGHT` | DelayRight | Delay Right | 0–100 % | 0–1 | 20 |
| `PARAM_LEFTGAIN` | LeftLoopsGain | Loop Scale Left | 0–50 % | 0–1 | 5 |
| `PARAM_RIGHTGAIN` | RightLoopsGain | Loop Scale Right | 0–50 % | 0–1 | 5 |
| `PARAM_ENABLEMAINFEEDBACK` | MainFeedbackEnabled | Main Feedback On | 0/1 | 0/1 | 1 |
| `PARAM_ENABLELOOPSFEEDBACK` | LoopsFeedbackEnabled | Taps Feedback On | 0/1 | 0/1 | 0 |
| `PARAM_FREQLEFT` / `RIGHT` | Left/RightCutoff | Cutoff Left/Right | 0–1 Fs | 0–1 | 0.9 |
| `PARAM_RESLEFT` / `RIGHT` | Left/RightResonance | Resonance Left/Right | 0–3 Q | 0–3 | 0.5 |
| `PARAM_FILTERLEFT` / `RIGHT` | Left/RightFilterEn | Filter Left/Right On | 0/1 | 0/1 | 1 |
| `PARAM_LOOPSSOUND` | LoopsOut | Taps Out | 0/1 | 0/1 | 0 |
| `PARAM_FEEDBACKLEFT` / `RIGHT` | Left/RightFeedback | Feedback Left/Right | 0–200 % | 0–2 | 10 |
| `PARAM_LINKDELAYS` … `LINKFILTERS` | Link* | Link Delays / Loop Scale / Feedback / Filters | 0/1 | — (UI only) | 1 |
| `PARAM_LEFTLOOPSGAIN` / `RIGHT` | Left/RightLoopsOutGain | Taps Gain Left/Right | 0–200 % | 0–2 | 100 |
| `PARAM_LEFTFEEDBACKOUTGAIN` / `RIGHT` | … | Feedback Out Left/Right | 0–200 % | 0–2 | 100 |
| `PARAM_LEFTFILTERGAIN` / `RIGHT` | Left/RightFilterGain | Filter Gain Left/Right | 0–500 % | 0–3 | 250 |
| `PARAM_MIDIRATE` | Midi rate | Sync Rate | 0–7 | 0–7 | 0 (Off) |
| `PARAM_TEMPO` | Tempo | Tempo | 0–300 | 0–300 | 100 |
| `PARAM_MAINOUT` | Main Out | **Dry Level** (was Straight Thru) | 0–100 % | 0–1 | 100 |
| — | — | **Tap 1–5 Feedback Left / Right** (new, ids 29–38) | 0–200 % | 0–2 | 100 |
| — | — | **Bypass** (new, id 1000) | 0/1 | — | 0 |
| — | — | **Host Tempo** (new, id 1001, read-only) | 0–300 | — | 0 |
| — | — | **Filter Taps** (new, appended after the tap feedback levels) | 0/1 | 0/1 | 0 (off) |
| — | — | **Filter Mode Left / Right** (new, appended) | Low Pass / High Pass | 0/1 | Low Pass |

`Dry Level` was the DXi's `Straight Thru`, an on/off switch for the dry
signal. On the `Polivoks` branch it became a continuous gain, 0–100 % of the
input mixed in with the echoes, ramped per sample like the other smoothed
parameters. It keeps the id and the normalised ends, so a preset or automation
saved with Straight Thru on loads as 100 % and off as 0 % — what the switch
did. The default is still 100 %.

`Tap n Feedback Left/Right` are new on the `Polivoks` branch. With Taps
Feedback on, `DelayLine::process` regenerates each tap into the one before it
by `mScale` (Loop Scale) — one amount for all five taps in the DXi. Each tap
now multiplies that by its own level, `mTapFeedback[ix]`; at the default of
100 % the product is exactly `mScale`, so the sound is bit-identical to the
original (`tests/DelayLineTests.cpp` check 6 renders it both ways). They are
set per block, not ramped, and Link Loop Scale links them left to right. They
are not on the panel's table: they are handles on the tap scope, shown while
Taps Feedback is on. A state saved before they existed loads them at their
default — both `setState`s now reset any parameter the stream does not carry.

`Host Tempo` is not a control. It is the processor reporting the tempo the
**host** supplied — zero when it supplied none — so the editor can show it and
quote delay times; see section 7.

Watch out for two pieces of DXi naming that are the reverse of what you'd
guess, and which the port renames rather than perpetuates:

* `PARAM_LEFTGAIN` (labelled *"LeftLoopsGain"*) feeds `DelayLine::SetScale` —
  the tap-to-tap regeneration amount. It is now **Loop Scale Left**.
* `PARAM_LEFTLOOPSGAIN` (labelled *"LeftLoopsOutGain"*) feeds
  `SetTapsOutGain` — the level of the taps at the output. It is now
  **Taps Gain Left**.

`Bypass` is new: VST3 hosts expect a `kIsBypass` parameter. It is separate
from the DXi's own `Enabled` switch, which is kept and still defaults to
**off**, exactly as the original did.

---

## 3. DSP: what is identical

`source/DelayLine.cpp` is a line-by-line port of `DelayLine.cpp`. These are
unchanged and were verified numerically:

* Line length: `sampleRate * 8` samples, 5 taps, so each tap can be up to
  1/5 of the line (1.6 s of delay per tap, 8 s total).
* `SetDelay`: free-running maps `delay²` across the line (finer resolution at
  short times); tempo-synced maps linearly then quantises to whole
  `samplesPer32 / 5` steps.
* `SetFrequency`: `f = f²`, floored at 0.001, scaled so 1.0 lands on 10 kHz
  relative to `sampleRate / 4`.
* The ladder filter is the *"Moog VCF, variation 2"* from musicdsp.org,
  coefficients untouched.
* The dither/noise table: the same LCG (`state * 1234567 + 890123`), the same
  mantissa punning (`state & 0x807f0000 | 0x1E000000`), 1000 entries.
* The tap-to-stereo matrix in the processor: tap *n* gets
  `nScale = 1 - (5-n)/10` in its own channel and `oScale = (5-n)/10` in the
  other, so tap 5 is hard-panned and tap 1 sits 40 %/60 %.
* When the filter is on, tap 5 is *replaced* by the filtered signal and is
  scaled by the **main** out gain rather than the taps out gain. That looks
  like a bug in the original; it is part of the sound, so it is kept.
* Tempo sync asks for `4 / divisions` quarter notes, matching
  `4 * TicksPerQuarterNote / nMidiRate`.

Verification run (48 kHz): an impulse with a 0.05 delay setting produces taps
at exactly samples 192, 384, 576, 768, 960 — i.e. `n × 192` — and tempo sync
at 120 BPM quantises the delay to an exact multiple of `samplesPer32 / 5`.

---

## 4. DSP: what deliberately changed

**Buffer zeroing.** The original wrote
`memset(fLine, 0, sizeof(fLine) * nLength)`, where `sizeof(fLine)` is the size
of a *pointer*. On 32-bit Windows that accidentally computed the right byte
count. On 64-bit it would overrun the buffer by 2×. Replaced with a correct
`std::vector::assign`.

**Allocation moved off the audio thread.** `SetSamplesPerSec` did
`delete[] / new[]` and was called from inside `Process()` whenever the rate
changed. It now happens in `setupProcessing`.

**Runaway protection.** Feedback goes to 200 %, filter gain to 500 % and
resonance to 3.0 — well past this filter topology's stability limit. The
original simply let the numbers explode (a five-second worst-case run reaches
~3 × 10¹⁵ and keeps climbing). Three bounds were added at ±16.0, roughly
+24 dBFS:

* the ladder filter state (`scrub()`),
* the value written back by the main feedback path,
* the value written back by the tap-to-tap feedback path.

Below ±16 nothing changes — the default-settings test produces bit-identical
peak and RMS with and without the clamps. The same worst-case run now peaks
at 48 instead of 3 × 10¹⁵. Denormals and non-finite values are also flushed,
which the original did not do.

**Index safety.** `nRemovePos` was wrapped once (`+= nLength - 1`), which is
sufficient only because `nDelay ≤ nLength / 5`. An explicit clamp was added so
a future change to the tap count cannot read out of bounds.

**Delay floor.** `nDelay` is clamped to at least 1 sample; the original could
reach 0, in which case all five taps read the same sample.

**Per-sample parameter smoothing.** The DXi got first and second derivatives
from its automation envelopes (`GetParamDeltas`) and stepped them per sample.
VST3 delivers sample-accurate automation points instead, so the port takes the
value at the end of each block as the target and ramps linearly from the
previous block's value across the buffer. The smoothed set is the same one the
DXi interpolated: delay, loop scale, feedback, cutoff, resonance and filter
gain, left and right.

**The release tail.** `DelayLine::MoreTailsAvailable` / `Get` are ported
(`moreTailsAvailable` / `readTail`) but nothing calls them: VST3 handles tails
by declaring `getTailSamples()` — one full line, 8 seconds — and the host keeps
calling `process` with silence, which flushes the line through the normal path.

**Sample formats.** The DXi accepted 32-bit float only
(`IsValidInputFormat` rejected anything else). The port handles 32-bit and
64-bit host buffers; the delay line itself is still float, as before.

**Mono.** The original was stereo-only. A mono bus arrangement is now
accepted and the single channel is fed to both delay lines.

**Grid taps (tempo sync).** The DXi spaced its five taps evenly whatever the
sync rate: synced, the *echo* (tap 5) landed on the grid but taps 1–4 sat at
1/5, 2/5, 3/5 and 4/5 of it — at an echo of three sixteenths, at 0.6, 1.2, 1.8
and 2.4 of a sixteenth, none of them on a note. Now, whenever `Sync Rate` is
on and the delay is long enough to quantise, every tap is on the grid. With
the echo at N grid units U (U = one step of the sync rate):

* tap 5 sits at exactly N × U, and
* taps 4..1 halve down from the largest power of two strictly below N.

| N | taps 1–5, in U |
|---|---|
| 1 | 1/16, 1/8, 1/4, 1/2, 1 |
| 2 | 1/8, 1/4, 1/2, 1, 2 |
| 3 | 1/4, 1/2, 1, 2, 3 |
| 4 | 1/4, 1/2, 1, 2, 4 |
| 8 | 1/2, 1, 2, 4, 8 |

Tap 5 also moves by a few samples: `computeDelaySamples` quantises per tap to
the integer `samplesPer32 / 5`, which left the echo up to 4 × N samples short
of N × U. Tap 5 is now on N × U exactly (N is stepped down if that would not
fit in the line, which the same integer division can tip over).

The rule lives in one place, `DelayLine::computeTapSamples`, which `setDelay`
and the editor both call — the delay readout, the tap scope's bars and the
feedback handles follow it. Free-running is untouched: the taps are
`n × computeDelaySamples`, and a 400 000-sample render with both feedback paths
and the filter on hashes bit-identical to the previous build. Synced, the same
render differs, as it must. `tests/DelayLineTests.cpp` checks 7 and 8 pin both:
the worked examples above, a sweep of 32 580 synced settings (three sample
rates, six divisions, five tempi) against the rule, and an impulse through a
live `DelayLine` coming out of each tap at exactly `getTapSamples (t)`.

The tap-to-tap regeneration is unchanged in form — each tap still feeds the
position of the tap before it — so with grid taps its loop lengths are the
gaps between grid taps rather than equal fifths.

**Projects saved with tempo sync on will sound different**: their taps 1–4 move
onto the grid. Free-running projects are unaffected.

**Filter Taps.** In the DXi only tap 5 was filtered: the ladder filter runs on
the last tap's read (`fEqSample = fLine[nRemovePos]` after the tap loop), and
its output *replaces* tap 5. Taps 1–4 went to the output straight off the line,
unfiltered, whatever the filter switches said. That is still the default.

The new `Filter Taps` switch (on the rule in the filter section, one for both
sides) sends taps 1–4 through the filter too, on any channel whose filter is
on. Each tap gets **its own ladder** — the filter has state, so one shared
ladder fed four signals would be a different, wrong filter — running the same
cutoff, resonance and filter gain as tap 5's, with the same dither sample.
Filtered taps keep the **taps out gain** (tap 5 keeps the DXi's main out gain).
Only the taps' *output* is filtered: the line, and so the main feedback and the
tap-to-tap regeneration, are untouched — the filter in the feedback path is
still tap 5's alone.

The ladder arithmetic moved into `DelayLine::runLadder`, shared by all five.
With the switch off the result is bit-identical to the previous build (the
same 400 000-sample render hash, filter on), and `tests/DelayLineTests.cpp`
check 9 pins it: off is the existing path; with the filter off the switch does
nothing; on, tap 5 is unchanged sample for sample (proving the line is), every
tap 1–4 changes, and white noise through tap 2 loses its top end (first-
difference energy ratio 1.98 → 0.0025 at a Cutoff reading 900 Hz). The tap scope marks
every filtered tap (LP or HP since Filter Mode, below).

The parameter is appended after the tap feedback levels, so ids are unchanged
and a project saved before it loads with Filter Taps off.

**Filter Mode: low-pass or high-pass.** The DXi's ladder is a low-pass only.
Each side now has a mode selector under its On switch, printed LP / HP, linked
by Link Filters like the rest of the section. High-pass is **mixed from the
same ladder's poles** (the Oberheim Xpander's approach) rather than being a
second filter, so cutoff, resonance, filter gain, dither, the ±16 bounds and
Filter Taps all apply unchanged, and the low-pass arithmetic is untouched.

Each pole here is `(1 + 0.3/z) / (1 − (1−f)/z)` with DC gain `1.3/f`. With
`g = f/1.3` and the ladder's input scaling `scale = 0.35013 f⁴` (≈ g⁴), the
k-th pole normalised to unity DC gain is `p_k = out_k · g^k / scale`, and

    hp = in − 4 p1 + 6 p2 − 4 p3 + p4       (in = after the resonance subtraction)

is `(1 − p)⁴`: since `1 − g(1 + 0.3/z)/(1 − (1−f)/z) = (1−g)(1 − 1/z)/(1 − (1−f)/z)`,
each factor has a zero exactly at DC. Resonance is still fed back from `out4`,
so the peak sits at the corner. The high-pass result goes through the same
`scrub()` bound as the states.

Measured (48 kHz, Cutoff showing 2.5 kHz, resonance 0, filter gain 100 %):

| Hz | LP dB | HP dB |
|---|---|---|
| 50 | 0.0 | −131.6 |
| 625 | −1.5 | −46.2 |
| 2 500 | −15.4 | −11.8 |
| 10 000 | −57.6 | −3.2 |

23 dB per octave two octaves under the corner — four poles. Note that neither
mode is −3 dB at the readout's frequency: this ladder's low-pass corner (−3 dB) sits near 900 Hz, well over an
octave below what `computeCutoffHz` reports, as it always did in the DXi. The
readout is kept as the DXi's number. The worst case (resonance 3, 500 % gain,
200 % feedback, Filter Taps on, high-pass) stays bounded at a peak of 34.

Low-pass renders hash bit-identical to the build before this change.
`tests/DelayLineTests.cpp` check 10 pins the responses, the slope, Filter Taps
following the mode, and the bound; `LayoutTests` checks the LP / HP names fit.

The panel: to make room, Resonance and Gain moved 10 and 18 px left, and the On
switches 10 px left, so the selector's printed names clear the knobs and the
section edge. The tap scope's F mark is now LP or HP.

---

## 5. The interface

> **Superseded on the `Polivoks` branch (27 Oct 2026).** The bitmap panel this
> section describes was replaced by a 1200 × 640 black panel drawn entirely
> with paths — R2Synth's Polivoks-style views, its 70s sci-fi lettering and
> its "Studio Worn" wear, laid out from `Designs/SpaceDub-Polivoks-1-TapScope.png`.
> The layout is data in `source/SpaceDubLayout.h`, checked by
> `tests/LayoutTests.cpp`; the views are `source/SpaceDubPanelViews.*`. What
> did NOT change: `readoutFor()` (bar the cutoff, now in kHz above 1 kHz), `tempoText()` and `applyLink()` (sections 7
> and below) are the same code, so every number and every link behaves as
> before. `SpaceDubControls.*` and the PNGs in `resource/` are gone; the
> section below is kept as the record of how the original was ported.

`IDD_PROPPAGE` in `SpaceDubMidi.rc` is 515 × 287 dialog units with a
`SS_BITMAP` static holding `res/background-all.bmp` (766 × 499 pixels) at
`(3, 0)` sized `511 × 307` DLU. That gives the conversion factors:

```
x_px = (x_dlu - 3) × 1.5        y_px = y_dlu × 1.625
```

(MS Sans Serif 8 pt base units — confirmed by `IDC_SPACEDUB`, 106 × 25 DLU,
which is exactly `Logo.bmp`'s 160 × 40 pixels.) Every position in
`SpaceDubEditor::open` was produced with that formula, so the layout matches
the original pixel for pixel. Note that the Windows dialog was 466 px tall and
*clipped* the bottom of the background bitmap; the VST3 window shows all 499 px,
so the "Left"/"Right" labels at the bottom are now fully visible.

The Windows controls map as follows:

| Windows | VST3 |
|---|---|
| `msctls_trackbar32`, `TBS_VERT` | `SdSlider` (vertical, **top = maximum**) |
| `msctls_trackbar32`, horizontal | `SdSlider` (horizontal) |
| `BS_AUTOCHECKBOX \| BS_PUSHLIKE` | `SdToggle`, two-frame bitmap |
| `NewSpin` / `Digistatic` text buttons | `SdTextStepper` |

The vertical sliders really do run "top = maximum": the property page read
them as `fVal = (max - GetPos()) / scale`, so dragging *up* increased the
value. That is preserved.

The custom MFC classes `BtnST`, `CMySliderControl`, `Label`, `StaticTime`,
`Digistatic`, `VMRotaryFaderCtrl`, `VMBitmap`, `MemDC` and `MyToolTipCtrl` are
not ported — they exist only to draw transparent controls on Win32, which
VSTGUI does natively.

**Link switches.** In the DXi these were purely a property-page feature:
`OnVScroll` checked `IsDlgButtonChecked(IDC_DELAYLINK)` and pushed the value to
the partner parameter. `SpaceDubEditor::applyLink` does the same, and also
issues a `performEdit` so the host records both sides.

`rotaryknob.bmp`, `rotaryscale.bmp` and `rotarydot.bmp` are converted to PNG
and shipped in the bundle, but nothing uses them — `VMRotaryFaderCtrl` was
never instantiated in the shipped dialog either. They are kept in case you
want to add rotary controls later.

---

## 6. Traps already hit

**Never pass a null title or units to `RangeParameter`.** Its constructor
hands both straight to `UString::assign`, which calls `StringCopy` and reads
`src[0]` with no null check. A null title therefore segfaults inside
`EditController::initialize` — which is where the SDK validator crashed with
`Segmentation fault: 11` during the post-build step, long before anything
audio-related ran. `SpaceDubParameter` now passes `USTRING (def.title)` and
`USTRING (def.units)` to the base constructor. `USTRING(x)` builds a
`UString256` temporary that lives to the end of the full expression, so it is
safe to use as a constructor argument.

If a build ever crashes in the validator again, the fastest way to find it is
to attach a debugger to the validator directly rather than reading the Xcode
log:

```sh
lldb -- build/bin/Debug/validator build/VST3/Debug/SpaceDub.vst3
(lldb) run
(lldb) bt
```

**`ProcessContext` is opt-in since VST3 3.7.** A host may leave every field
unset — including `kTempoValid` — unless the plug-in declares what it needs
through `IProcessContextRequirements`. `Vst::AudioEffect` implements that
interface for you, but its `processContextRequirements` member defaults to
**zero flags**, meaning "I need nothing". The tempo sync therefore silently
fell back to 120 bpm in every host. One line in the processor's constructor:

```cpp
processContextRequirements.needTempo ();
```

This is quiet in a way that is worth knowing about: the SDK validator does
*not* flag it as an error, because the interface is present — it just prints
`ProcessContextRequirements: - None` among its messages. If a host-supplied
value ever seems to be missing, read that line in the validator log first.

Only tempo is requested. The DSP does not use the transport state or the
musical position, and the whole point of the interface is to ask for the
minimum so hosts can skip computing the rest.

**`CMAKE_MODULE_PATH` does not come back up from `add_subdirectory`.** The
VST3 SDK appends its own `cmake/modules` to `CMAKE_MODULE_PATH` at line 19 of
its `CMakeLists.txt`. That runs inside the `add_subdirectory` scope, and
variables set in a child scope do not propagate to the parent — so
`include(SMTG_AddVST3AuV2)` in *our* `CMakeLists.txt` failed with
"include could not find requested file". The fix is one line, next to where
`VST3_SDK_ROOT` is settled:

```cmake
list(APPEND CMAKE_MODULE_PATH "${VST3_SDK_ROOT}/cmake/modules")
```

Note the asymmetry that makes this easy to miss: CMake *functions* are global
once defined, which is why `smtg_add_vst3plugin` and friends work in the parent
scope without any help. Only the module search path needed fixing.

### `sendMessage` from the audio thread is silently thrown away

`IMessage` looks like the obvious way to get a value from the processor to the
controller, and it compiles, and it returns success. It does nothing. From
`public.sdk/source/vst/hosting/connectionproxy.cpp`:

```cpp
tresult PLUGIN_API ConnectionProxy::notify (IMessage* message)
{
    if (dstConnection)
    {
        // We discard the message if we are not in the UI main thread
        if (threadChecker && threadChecker->test ())
            return dstConnection->notify (message);
    }
    return kResultFalse;
}
```

Hosts route component-to-controller messages through that proxy, so anything
sent from `process` is dropped on the floor with no warning, no log line and no
validator complaint. This is what made the tempo readout sit at its default
while the DSP itself was tracking the host perfectly — the delay times were
right, only the display was stale.

The sanctioned per-block processor → controller path is
`data.outputParameterChanges`: add a queue for a parameter the controller has
registered `kIsReadOnly`, add a point, and the host delivers it to
`EditController::setParamNormalized` on the correct thread. That is
`kHostTempoOut`. It is deliberately *not* saved in the state — it is a readout,
not a setting — so `getState`/`setState` still write only `kNumParams` values.

**The tap meters use the same route** (branch `Polivoks`). The tap scope
shows how much signal is going through each of the ten taps, so the processor
measures each tap's peak per block in `processAudio`, turns it into a meter
value (`tapMeterValue`: −60 … +6 dBFS, linear in dB, rising at once and
falling back at 20 dB a second), and writes it into `outputParameterChanges`
as ten read-only parameters, `kTapLevelOut` … `kTapLevelOut + 9` (ids
1002–1011: left taps 1–5, then right). It publishes about 30 times a second
and only the meters that moved. Like `kHostTempoOut` they are not saved. The
meters measure the tap itself, so a tap that Taps Out leaves out of the mix
still shows its signal (dimmed on the scope). When the plug-in is switched off
or bypassed they fall to zero.

Messages are still fine from `setActive`, `setState`, `initialize` and the like,
which VST3 documents as `[UI-thread]`; that is why the sample rate can go that
way. If in doubt, check the interface's thread annotation in the SDK headers
before choosing a message.

**Don't index `kParams` with an arbitrary `ParamID`.** `kBypass` is 1000, well
past the end of the array, so `paramDef (id)` must only ever be called with an
id that has already been range-checked (`id < kNumParams`). The processor's
`applyParameterChanges` does this check; keep it if you add parameters.

---

## 7. Value readouts

`resource.h` lists a set of display controls — `IDC_LEFTDELAYDISPLAY`,
`IDC_RIGHTDELAYDISPLAY`, `IDC_LEFTFREQUENCYDISPLAY`, `IDC_RIGHTFREQUENCYDISPLAY`,
`IDC_LRESD`, `IDC_RRESD` and friends — drawn with the DXi's own `Label` and
`StaticTime` classes. They are not in the shipped `IDD_PROPPAGE`, so by the
last build the same information was only reachable through the tooltips.

This port puts it back on the panel. Every slider carries an
`SdValueDisplay` showing its current value, always visible, no hovering.
The switches have none — their colour already says what they are doing — and
the sync rate and tempo have their own text controls in the black panel.

### Where the readouts sit

Found by scanning the background artwork for rows free of its painted blue
labels, rather than by eye:

* **Vertical sliders** — a 12 px band at y 388..399 is clear across all ten
  columns, directly under the slider and directly above the printed name. The
  columns are 58 px apart at the closest, so a 52 px readout centred on each
  slider fits everywhere without touching its neighbour.
* **Horizontal gain sliders** — the two rows are only 8 px apart, so there is
  nowhere below them to put anything. Their readout sits *inside* the slider's
  own footprint, 32 px down, below the handle. `SdValueDisplay` is
  mouse-disabled, so clicks still reach the slider underneath.

The text is Arial 9 with a one-pixel black shadow, matching the panel's own
painted labels — without the shadow, 9 pt on that mottled green is hard to read.

### Why not tooltips

They were tried first and never appeared on macOS. Every link in the chain
checked out — `enableTooltips` matched `VST3Editor`'s own usage, the text was
verifiably stored in `kCViewTooltipAttribute`, the controls were mouse-enabled
and hit-testable — but the failure was somewhere in VSTGUI's platform layer and
could not be reproduced off a Mac. Always-visible readouts need none of that
machinery, and are more useful anyway. If you ever want to revisit it, the
history of this file has the analysis.

### Where the delay time comes from

The delay readout quotes the delay the DSP has actually settled on, which is a
quantised, integer number of samples — not simply what the slider says. The DXi
could read it straight off the DSP object through `MyProps`, a direct C++
back-channel, because a DXi ran as a single in-process object.

VST3 splits the processor and the controller into separate components that may
not even share an address space, so the editor cannot do that. Instead:

* `DelayLine::computeDelaySamples` is the delay-control mapping, factored out
  of `setDelay` as a static function with no state. The processor and the
  editor both call it, so they cannot drift apart.
* `samplesPer32For` in `SpaceDubParams.h` does the same for the sync grid.
* The processor sends the controller its **sample rate** as an `IMessage` from
  `setActive`, which VST3 documents as `[UI-thread]`.
* The **host's tempo** travels the other way: as a *read-only output
  parameter*, `kHostTempoOut` (id 1001), written into
  `data.outputParameterChanges` from `process`, and only when the value
  changes. See the trap below for why it cannot be a message.

The controller keeps both in `mHostTempo` / `mHostSampleRate` and calls
`SpaceDubEditor::refreshAllReadouts` whenever either moves, so every readout
follows the host live.

### Zero means "the host gave us nothing"

`kHostTempoOut` carries the host's tempo, not the tempo in use, and it is zero
whenever the host supplied none. That is the DXi's own convention — look at
`CSpaceDubMidi::Process`:

```cpp
rTempo = pnBMP100 / 100;
if ((rTempo > 0) && (rTempo < 300)) {
    if (m_Tempo != rTempo) m_Tempo = rTempo;
} else {
    m_Tempo = 0;
    dCuBaseTempo = GetParamValue (PARAM_TEMPO);
    m_bAllowTempoSync = false;
}
```

The editor needs that distinction, because with no host tempo the delay follows
the `Tempo` parameter instead and the control has to go back to showing it.

Two shared inline functions in `SpaceDubParams.h` keep the two halves honest:

* `hostTempoUsable (bpm)` — the DXi's `(rTempo > 0) && (rTempo < 300)` test.
* `effectiveTempo (hostBpm, manualBpm)` — the tempo actually used. The
  processor calls it in `updateDelayLines`; the editor calls it in
  `runningTempo()`. Neither has its own copy of the rule.

### The tempo control tracks the host, always

**The host wins whenever it supplies a tempo**, whatever the `Tempo` parameter
says — that is what the DXi did and what the DSP here does. So the control has
to show the host's value at *every* step, not just at step 0.

Getting this wrong is easy and quiet. `Tempo` defaults to **100**, not to the
"Tempo Sync" step, so a control that only consults the host when the step is 0
sits on `100 BPM` for ever while the delay tracks the host perfectly. The DXi
avoided it by brute force — its property page timer read the tempo back through
`MyProps` and physically dragged the control onto it:

```cpp
int nTempo = m_pMyProps->GetTempo();
if (nTempo != 0) {
    m_Tempo.SetnPos(nTempo);
}
```

`SpaceDubEditor::tempoText` does the same thing without moving the parameter,
which in VST3 would read as an automation write. `Tempo Sync` now appears only
when there is no host tempo *and* the step is 0.

If you change the delay mapping, change it in `computeDelaySamples` only — the
readout follows automatically. There is a test that sweeps the control across
four sample rates, five tempi and six divisions and asserts the readout time
matches `DelayLine` exactly.

---

## 8. The Audio Unit wrapper

macOS also gets an AUv2 build, using Steinberg's wrapper
(`public.sdk/source/vst/auwrapper`). It is worth understanding that this is
**not a second implementation**: the `.component` bundle holds the VST3 and
loads it through `CFBundleCreate`, so the DSP, the parameters and the editor
are the same code. `AUWrapper::loadVST3Module` picks the first
`kVstAudioEffectClass` in the factory — our processor — and gets the controller
from `getControllerClassId`, which is why no per-plug-in C++ was needed.

What the port had to supply:

* `resource/au-info.plist` — the AudioComponents entry. `aufx` (effect),
  subtype `SDub`, manufacturer `AECo`. Apple reserves all-lowercase
  manufacturer codes, hence the capitals. `AudioUnit SupportedNumChannels`
  lists 2-in/2-out and 1-in/1-out, matching
  `SpaceDubProcessor::setBusArrangements` — if you ever widen the bus support,
  widen this too or hosts will not offer the new layouts.
* The CMake block, which fetches Apple's AudioUnitSDK into
  `external/AudioUnitSDK` and sets `SMTG_AUDIOUNIT_SDK_PATH` **before**
  `add_subdirectory(vst3sdk)` — that is where the SDK's `setupCoreAudioSupport()`
  reads it and decides whether to switch `SMTG_ENABLE_AUV2_BUILDS` on.

**The AudioUnitSDK version is pinned, and it is load-bearing.** Steinberg's
`smtg_target_add_auv2` does `target_compile_features(cxx_std_17)`, while Apple
has moved the AudioUnitSDK forward:

| tag | headers | its own build standard |
|---|---|---|
| 1.0.0, 1.1.0 | C++17 | c++17 |
| 1.2.0 | C++17-clean | c++20 |
| 1.3.0, 1.4.0 | `std::span`, `concept`, `requires` | c++20 / c++23 |

Compiling 1.3.0+ against the C++17 wrapper dies in `AUUtility.h` with
*"no member named 'span' in namespace 'std'"* followed by a cascade of
`unknown type name 'requires'`. **1.1.0** is the newest release whose own build
standard matches what the wrapper compiles at, so the static library and our
translation units agree — that is the pin.

`SPACEDUB_AU_SDK_TAG` can be pointed at a newer one; the build then greps
`AUUtility.h` for those C++20 constructs and raises `CXX_STANDARD` on the
`SpaceDub-au` target accordingly. Detection is from the headers, not the tag
name, so a hand-placed checkout works too. A stale checkout in `external/` is
compared against the pin and refetched rather than silently used.

Constraints worth remembering:

* The AUv2 helper only exists under the **Xcode generator**
  (`if(XCODE AND SMTG_ENABLE_AUV2_BUILDS)` in `SMTG_AddVST3AuV2.cmake`), so a
  Makefile or Ninja build gets the VST3 only. This is Steinberg's constraint,
  not ours.
* The helper symlinks the built VST3 into
  `SpaceDub.component/Contents/Resources/plugin.vst3` and copies the bundle to
  `~/Library/Audio/Plug-Ins/Components/`. The installed copy therefore contains
  a symlink into your build tree — fine for development, but for anything you
  hand to someone else, replace that symlink with the real `.vst3` bundle
  before shipping.
* A failed AudioUnitSDK fetch is deliberately a warning, not an error. Losing
  the AU should never cost you the VST3 build.
* **Code signing had to be taken off the SDK.** `SMTG_CODE_SIGN_IDENTITY_MAC`
  defaults to `"Apple Development"`, and `smtg_target_add_auv2` ends with an
  unconditional
  `codesign -v -s "${SMTG_CODE_SIGN_IDENTITY_MAC}" --force --timestamp` on the
  installed `.component`. With no developer certificate in the keychain that is
  a hard failure — *after* the wrapper has compiled, linked and been copied
  into place. `SPACEDUB_CODE_SIGN_IDENTITY` now drives it: given an identity it
  is handed to the SDK; left empty, `SMTG_DISABLE_CODE_SIGNING` goes on and
  both bundles are ad-hoc signed by our own post-build step. That step
  deliberately omits `--timestamp`, which an ad-hoc signature cannot carry.

Validate with `auval -v aufx SDub AECo`. It is stricter than most hosts,
particularly about channel layouts and parameter round-tripping.

---

## 9. Things worth knowing before you change anything

* **`Enabled` defaults to off.** Kept for fidelity. If you would rather the
  plug-in make sound as soon as it is inserted, change the default in
  `kParams[kEnable]` in `SpaceDubParams.cpp`.
* **Filter gain default is 250 %.** Combined with resonance, this is loud by
  design; it is what the original shipped with.
* **`Sync Rate` has eight entries but only seven distinct values** — the DXi's
  `switch` mapped both case 6 and case 7 to a whole note. Preserved so
  automation values line up.
* **The AU four-character codes** in `resource/au-info.plist` are as permanent
  as the class UIDs — changing `aufx`/`SDub`/`AECo` after shipping makes every
  existing session lose the plug-in.
* **Class UIDs** are in `source/SpaceDubIDs.h`. They are freshly generated for
  this port. Never change them once you have shipped a build, or existing
  projects will lose their plug-in.
* **Bundle identifier** is `audio.spacedub.vst3` in `CMakeLists.txt` — change
  it if you sign under your own developer ID.
