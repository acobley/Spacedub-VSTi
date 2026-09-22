"""Render the SpaceDub editor as a picture, without a host or a window server.

Reads the shipped artwork in ../resource and reproduces, in Python, exactly
what SpaceDubEditor::open() lays out and what SdSlider / SdToggle /
SdTextStepper / SdValueDisplay draw - control positions, the top-is-maximum
vertical sliders, the two-frame toggles and the value readouts, with every
parameter at its default from SpaceDubParams.cpp.

It is a mock-up, not a screenshot: if you move a control in the editor, move
it here too. Requires Pillow.  Usage:  python3 doc/render-editor.py
"""

from PIL import Image, ImageDraw, ImageFont
import math, os

HERE = os.path.dirname(os.path.abspath(__file__))
RES  = os.path.join(HERE, "..", "resource")
OUT  = os.path.join(HERE, "plugin-window.png")

W, H = 766, 499
SR   = 48000.0          # host sample rate
HOST_TEMPO = 120.0      # host-supplied tempo

# Arial, or a metric-compatible stand-in - the editor asks VSTGUI for
# "Arial"/"ArialMT" (vstgui/lib/cfont.cpp), 9 pt for the readouts, 11 pt for
# the two text steppers.
ARIAL = next(p for p in (
    "/System/Library/Fonts/Supplemental/Arial.ttf",
    "/Library/Fonts/Arial.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
) if os.path.exists(p))
f9  = ImageFont.truetype(ARIAL, 9)
f11 = ImageFont.truetype(ARIAL, 11)

bg      = Image.open(f"{RES}/background.png").convert("RGBA")
handleV = Image.open(f"{RES}/handle_v.png").convert("RGBA")
handleH = Image.open(f"{RES}/handle_h.png").convert("RGBA")
grooveV = Image.open(f"{RES}/groove_v.png").convert("RGBA")
grooveH = Image.open(f"{RES}/groove_h.png").convert("RGBA")
btnSmall  = Image.open(f"{RES}/button_small.png").convert("RGBA")
btnFilter = Image.open(f"{RES}/button_filter.png").convert("RGBA")
btnPower  = Image.open(f"{RES}/button_power.png").convert("RGBA")

# ---- parameter table (SpaceDubParams.cpp) -------------------------------
# name: (plainMin, plainMax, plainDefault, internalMin, internalMax, type)
P = {
 'Enable':(0,1,0,0,1,'B'),
 'DelayLeft':(0,100,20,0,1,'F'), 'DelayRight':(0,100,20,0,1,'F'),
 'LoopScaleLeft':(0,50,5,0,1,'F'), 'LoopScaleRight':(0,50,5,0,1,'F'),
 'MainFeedbackEnable':(0,1,1,0,1,'B'), 'LoopsFeedbackEnable':(0,1,0,0,1,'B'),
 'CutoffLeft':(0,1,0.9,0,1,'F'), 'CutoffRight':(0,1,0.9,0,1,'F'),
 'ResonanceLeft':(0,3,0.5,0,3,'F'), 'ResonanceRight':(0,3,0.5,0,3,'F'),
 'FilterLeftEnable':(0,1,1,0,1,'B'), 'FilterRightEnable':(0,1,1,0,1,'B'),
 'TapsOut':(0,1,0,0,1,'B'),
 'FeedbackLeft':(0,200,10,0,2,'F'), 'FeedbackRight':(0,200,10,0,2,'F'),
 'LinkDelays':(0,1,1,0,1,'B'), 'LinkLoops':(0,1,1,0,1,'B'),
 'LinkFeedback':(0,1,1,0,1,'B'), 'LinkFilters':(0,1,1,0,1,'B'),
 'TapsGainLeft':(0,200,100,0,2,'F'), 'TapsGainRight':(0,200,100,0,2,'F'),
 'FeedbackOutGainLeft':(0,200,100,0,2,'F'), 'FeedbackOutGainRight':(0,200,100,0,2,'F'),
 'FilterGainLeft':(0,500,250,0,3,'F'), 'FilterGainRight':(0,500,250,0,3,'F'),
 'MidiRate':(0,7,0,0,7,'E'), 'Tempo':(0,300,100,0,300,'E'),
 'MainOut':(0,1,1,0,1,'B'),
}
def norm(n):
    lo,hi,d,_,_,_ = P[n]
    return (d-lo)/(hi-lo)
def plain(n):
    lo,hi,_,_,_,_ = P[n]
    return lo + norm(n)*(hi-lo)
def internal(n):
    lo,hi,d,imin,imax,t = P[n]
    x = norm(n)
    if t=='B': return 0.0 if x<0.5 else 1.0
    v = imin + x*(imax-imin)
    if t=='E': return float(int(v+0.5))
    return v

# ---- readout text (SpaceDubEditor::readoutFor) --------------------------
KNUMTAPS, KLINESECS = 5, 8
def samples_per_32(divisions, bpm, sr):
    if divisions==0 or bpm<=0: return 0
    return max(1, int((60.0/bpm)*(4.0/divisions)*sr))
def compute_delay_samples(delay, line_len, sp32):
    steps = 0
    if line_len<=0: return 1, 0
    delay = min(max(delay,0.0),1.0)
    s = delay*delay if sp32==0 else delay
    max_delay = line_len//KNUMTAPS
    n = int(s*max_delay)
    if sp32!=0:
        per = max(1, sp32//KNUMTAPS)
        steps = n//per
        n = steps*per
        if n < per: n = int(delay*max_delay)
    return min(max(n,1),max_delay), steps
def compute_cutoff_hz(f, sr):
    sr = max(sr, 8000)
    f = f*f
    f = max(f, 0.001)
    return min(10000.0*f, sr/4.0)

def host_usable(b): return 1.0 < b < 300.0
rate_step = int(round(norm('MidiRate')*7))
divisions = {1:32,2:16,3:8,4:4,5:2,6:1,7:1}.get(rate_step,0)
running_tempo = HOST_TEMPO if host_usable(HOST_TEMPO) else (internal('Tempo') or 120.0)

def readout(n):
    if n in ('DelayLeft','DelayRight'):
        line = int(SR)*KLINESECS
        grid = samples_per_32(divisions, running_tempo, SR)
        per, steps = compute_delay_samples(internal(n), line, grid)
        secs = per*KNUMTAPS/SR
        return f"{secs*1000:.0f} ms" if divisions==0 else f"{secs*1000:.0f} ms {steps}u"
    if n in ('CutoffLeft','CutoffRight'):
        return f"{compute_cutoff_hz(internal(n), int(SR)):.0f} Hz"
    if n in ('ResonanceLeft','ResonanceRight'):
        return f"Q {plain(n):.2f}"
    if n in ('LoopScaleLeft','LoopScaleRight'):
        return f"{internal(n)*100:.0f}%"
    return f"{plain(n):.0f}%"

# ---- draw ---------------------------------------------------------------
img = bg.copy()
d   = ImageDraw.Draw(img)

READOUT_W, READOUT_H, READOUT_Y, READOUT_INSET_Y = 56, 12, 388, 32

def value_display(rect, text, color=(255,255,255,255)):
    x0,y0,x1,y1 = rect
    font = f9
    tw = d.textlength(text, font=font)
    if tw + 6 > (x1-x0):
        font = ImageFont.truetype(ARIAL, 8)
        tw = d.textlength(text, font=font)
    pw = min(tw+6, x1-x0)
    px = x0 + (x1-x0-pw)/2
    d.rectangle([px+0.5, y0+0.5, px+pw-0.5, y1-0.5], fill=(0,0,0,255), outline=(255,255,255,255), width=1)
    d.text(((x0+x1)/2, (y0+y1)/2), text, font=font, fill=color, anchor="mm")

def slider(name, x, y, w, h, vertical):
    groove = grooveV if vertical else grooveH
    handle = handleV if vertical else handleH
    img.alpha_composite(groove.resize((w,h)), (x,y))
    v = norm(name)
    if vertical:
        travel = h - handle.height
        hy = y + travel*(1.0-v)
        hx = x + (w - handle.width)/2
    else:
        travel = w - handle.width
        hx = x + travel*v
        hy = y + (h - handle.height)/2
    img.alpha_composite(handle, (int(round(hx)), int(round(hy))))
    if vertical:
        cx = x + w/2
        rect = (cx-READOUT_W/2, READOUT_Y, cx-READOUT_W/2+READOUT_W, READOUT_Y+READOUT_H)
    else:
        rect = (x, y+READOUT_INSET_Y, x+w, y+READOUT_INSET_Y+READOUT_H)
    value_display(rect, readout(name))

def toggle(name, x, y, w, h, frames):
    fh = frames.height//2
    off = fh if norm(name) >= 0.5 else 0
    img.alpha_composite(frames.crop((0, off, w, off+h)), (x,y))

# vertical sliders
for n,x in (('DelayLeft',52),('LoopScaleLeft',128),('FeedbackLeft',190),
            ('CutoffLeft',260),('ResonanceLeft',326),
            ('DelayRight',416),('LoopScaleRight',484),('FeedbackRight',542),
            ('CutoffRight',614),('ResonanceRight',680)):
    slider(n, x, 166, 27, 221, True)

# horizontal gain sliders
for n,x,y in (('TapsGainLeft',405,18),('TapsGainRight',405,70),
              ('FeedbackOutGainLeft',471,18),('FeedbackOutGainRight',471,70),
              ('FilterGainLeft',546,18),('FilterGainRight',546,70)):
    slider(n, x, y, 51, 44, False)

# toggles
toggle('Enable',705,21,27,20,btnPower)
for n,x,y in (('MainFeedbackEnable',204,28),('LoopsFeedbackEnable',261,28),
              ('TapsOut',306,28),('MainOut',306,98),
              ('LinkDelays',45,122),('LinkLoops',120,122),
              ('LinkFeedback',183,122),('LinkFilters',252,122)):
    toggle(n,x,y,27,20,btnSmall)
toggle('FilterLeftEnable',297,348,14,50,btnFilter)
toggle('FilterRightEnable',651,348,14,50,btnFilter)

# text steppers
def tempo_text(step):
    if host_usable(HOST_TEMPO):
        return f"{HOST_TEMPO:.5g} BPM"
    return "Tempo Sync" if step<=0 else f"{step} BPM"
MIDI_RATE_NAMES = ["Off","32nd","16th","8th","Quarter","Half","Whole","Whole"]

d.text((670+32, 100+15/2), tempo_text(int(round(norm('Tempo')*300))), font=f11,
       fill=(110,170,255,255), anchor="mm")
d.text((674+30, 117+16/2), MIDI_RATE_NAMES[rate_step], font=f11,
       fill=(110,255,170,255), anchor="mm")

# ---- generic host window chrome ----------------------------------------
PAD, TITLE = 8, 30
cw, ch = W + 2*PAD, H + PAD + TITLE
win = Image.new("RGBA", (cw, ch), (38,41,46,255))
wd  = ImageDraw.Draw(win)
wd.rectangle([0,0,cw-1,TITLE-1], fill=(30,32,36,255))
wd.line([(0,TITLE-1),(cw-1,TITLE-1)], fill=(20,21,24,255))
ft = ImageFont.truetype(ARIAL, 12)
wd.text((PAD+4, TITLE/2), "SpaceDub", font=ft, fill=(228,230,233,255), anchor="lm")
wd.text((cw-PAD-4, TITLE/2), "VST3  ·  Fx|Delay  ·  A.E. Cobley", font=ft,
        fill=(140,146,154,255), anchor="rm")
wd.rectangle([0,0,cw-1,ch-1], outline=(22,23,26,255), width=1)
win.alpha_composite(img, (PAD, TITLE))
win.convert("RGB").save(OUT)
print("wrote", os.path.normpath(OUT), win.size)
