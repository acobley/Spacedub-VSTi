# doc

**[signal-routing.svg](signal-routing.svg)** — how audio flows through the plug-in.

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

The drawing is maintained by hand. If you change the routing, change it here
too — `PORTING-NOTES.md` sections 3 and 4 explain the reasoning behind each
path.
