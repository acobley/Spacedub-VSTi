# doc

**[plugin-window.png](plugin-window.png)** — the editor, drawn by the
editor's own code: [`../tools/render-panel`](../tools/render-panel/build.sh)
builds `SpaceDubEditor`, `SpaceDubPanelViews` and the controller against
VSTGUI's cairo back end and draws the window into a PNG at 2×, with no host
and no window server. So it cannot go stale the way a mock-up can — re-run it
after changing the panel:

    tools/render-panel/build.sh doc/plugin-window.png 0=1

Every parameter sits at its default from `SpaceDubParams.cpp` except
`Enabled`, switched on (`0=1`) so the tap scope is lit; with no host the
sample rate is 44.1 kHz and the tempo screen shows the manual tempo.

**[signal-routing.png](signal-routing.png)** — how audio flows through the
plug-in. [`signal-routing.svg`](signal-routing.svg) is the source; the PNG is
rendered from it at 2× (2080 × 3120) with headless Chromium.

Three panels:

* **A — one delay line.** `DelayLine::process()`: the 8-second circular buffer,
  the five taps (n × d free-running, on the sync grid when synced), the
  tap-to-tap regeneration — five paths, tap 1 feeding the write head, each
  × Loop Scale × that tap's **feedback handle** — the optional Filter Taps
  ladders on taps 1–4, the DXi's ladder on tap 5, and main feedback.
* **B — the stereo tap matrix.** `SpaceDubProcessor::processAudio()`: Taps
  Out, the fixed per-tap panning, and Dry.
* **C — the tap scope's marks.** What the frame, the fill, the crossing frame,
  the F and the amber handle each stand for, and where in A and B they sit.

Details that are easy to get wrong, all deliberate:

* **The handles on the tap scope are tap FEEDBACK, not tap volume.** Each sets
  how much of its tap regenerates into the tap before it (tap 1 into the write
  head), only while Taps FB is on. There is no per-tap volume: Taps Gain and
  FB Out are per side.
* The filtered tap 5 leaves through **FB Out**, not Taps Gain.
* Main feedback is tapped **after Filter Gain but before FB Out**; with the
  filter off it is the raw tap 5.
* The scope's meters are read after the gains and any filter, before the
  matrix, whether or not Taps Out lets the tap be heard.
* Filter Taps filters only what is heard from taps 1–4, never the line.

The previous drawing showed four regeneration arcs; there are five — the
DXi's loop starts `oldRemovePos` at the write head, so tap 1 regenerates into
the newest sample.

The drawing is maintained by hand. If you change the routing, edit the SVG and
re-render the PNG.
