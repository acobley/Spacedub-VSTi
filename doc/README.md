# doc

**[plugin-window.png](plugin-window.png)** — the editor as a host shows it,
composited from the shipped artwork in `../resource` by
[`render-editor.py`](render-editor.py). Every parameter sits at its default
from `SpaceDubParams.cpp` — so `Enabled` is **off**, as the DXi shipped it —
and the readouts are quoted for a host running at 48 kHz and 120 BPM, which is
why the tempo control shows the host's tempo rather than "Tempo Sync".

It is a mock-up rather than a screenshot: the script reproduces in Python what
`SpaceDubEditor::open()` positions and what `SdSlider`, `SdToggle`,
`SdTextStepper` and `SdValueDisplay` draw, including the top-is-maximum
vertical sliders and the plate sizing in the readouts. Move a control in the
editor and it must be moved here too, or the picture quietly goes stale. Needs
Pillow; run it as `python3 doc/render-editor.py`.

**[signal-routing.png](signal-routing.png)** — how audio flows through the
plug-in. [`signal-routing.svg`](signal-routing.svg) is the source; the PNG is
exported from it at 2× (2080 × 2370) for anywhere that will not render SVG.

Two panels:

* **A — one delay line.** `DelayLine::process()`: the 8-second circular buffer,
  the five taps at −1×d … −5×d, the tap-to-tap regeneration path (Loop Scale),
  the ladder filter that replaces tap 5, and the main feedback path back into
  the write head. The processor instantiates two of these, `mDelayL` and
  `mDelayR`.
* **B — the stereo tap matrix.** `SpaceDubProcessor::processAudio()`: the
  per-tap panning weights, carried over from the DXi unchanged, and the dry
  (Straight Thru) path.

Three details in the diagram are easy to get wrong, and all three are
deliberate behaviour inherited from the DXi:

* The filtered tap leaves through **Feedback Out**, not Taps Gain — unlike the
  other four taps.
* Main feedback is tapped **after Filter Gain but before Feedback Out**, so the
  amount fed back is not the amount you hear.
* With the filter off, the signal fed back is the **raw** last tap.

The drawing is maintained by hand. If you change the routing, edit the SVG and
re-export the PNG — `PORTING-NOTES.md` sections 3 and 4 explain the reasoning
behind each path.
