# SpaceDub panel designs

All three use R2Synth's Polivoks look: black panel, white silkscreen, knurled
knobs with printed scales, chicken-head selectors for every switch, OLED
screens behind glass, and the 70s sci-fi lettering throughout. Values shown
are SpaceDub's defaults (Enabled shown on): 64 ms a tap, 8.1 kHz cutoff.
Each concept gets one of R2Synth's three wear passes.

* `SpaceDub-Polivoks-1-TapScope.png` — **Tap Scope, studio worn, green OLED.**
  A wide scope shows the five taps a side (own side bright, cross-feed dim)
  over four boxed L/R sections: Delay, Feedback, Filter, Output. Link
  selectors sit on the rule between the L and R rows. Clock section has the
  sync rate, the host tempo OLED, Thru, Taps Out, Enabled and Bypass.
* `SpaceDub-Polivoks-2-ChannelStrips.png` — **Channel Strips, road case, amber OLED.**
  Left and right as signal-flow strips (Delay > Feedback > Filter > Out),
  each with its own taps + filter OLED. The four Link selectors sit on a
  spine between the strips, lined up with the knobs they join. Master row at
  the bottom.
* `SpaceDub-Polivoks-3-EchoMap.png` — **Echo Map, barn find, cyan OLED.**
  A symmetrical console with a mirrored L wing and R wing. A centre "echo map"
  OLED plots each tap by pan against time, and each wing has its own ladder
  filter screen.

Regenerate with:

```sh
python3 render-polivoks-spacedub.py .        # -> *.svg + *.json layouts
python3 render.py SpaceDub-Polivoks-1-TapScope ...   # -> *-clean.png (2x)
SAVE_AS=SpaceDub-Polivoks-1-TapScope.png python3 weather-spacedub.py \
    SpaceDub-Polivoks-1-TapScope-clean.png SpaceDub-Polivoks-1-TapScope.json . 1
```

`render.py` reads the .svg/.json files from its own folder. `weather-spacedub.py` is R2Synth's `render-weathered.py`
with the screen-specific marks (fingerprints, the cracked glass, the finger
swipe and the mug ring) moved to fit these layouts.
