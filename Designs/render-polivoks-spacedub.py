#!/usr/bin/env python3
"""Three SpaceDub panel concepts in R2Synth's Polivoks / OLED / 70s sci-fi
lettering style. Each design writes <name>.svg (1200 x 640 layout units) and
<name>.json (the layout the weathering pass reads: knobs, selectors, screens,
sections, screws).

Values shown are SpaceDub's defaults (Enabled shown on): delay 20 % (64 ms a
tap free-running), loop scale 5 %, feedback 10 %, cutoff 0.9 (8.1 kHz),
resonance 0.5, filter gain 250 %, taps gain 100 %, feedback out 100 %.
"""
import json, math, os, sys
from html import escape

W, H, HDR = 1200, 640, 56
OUT = sys.argv[1] if len(sys.argv) > 1 else '.'


def pol(cx, cy, r, deg):
    t = math.radians(deg)
    return cx + r * math.sin(t), cy - r * math.cos(t)


THEMES = {
    'green': dict(trace='#5fe07a', dim='#2f7a40', alt='#9be8ff', word2='#e2342b'),
    'amber': dict(trace='#ffb23a', dim='#8a5e1c', alt='#ff6a3a', word2='#ffb23a'),
    'cyan':  dict(trace='#5fd4ff', dim='#2a6f8a', alt='#c58bff', word2='#5fd4ff'),
}
INK, MUTED, TICK, RED = '#ecE9e0', '#a19e96', '#d9d6cc', '#e2342b'


class Panel:
    def __init__(s, theme):
        s.T = THEMES[theme]
        s.p = []
        s.L = dict(width=W, height=H, controls=[], screens=[], sections=[],
                   screws=[(8, 8), (W - 8, 8), (8, H - 8), (W - 8, H - 8)])
        s.ngrad = 0

    # -- primitives ---------------------------------------------------------
    def add(s, x): s.p.append(x)

    def text(s, x, y, msg, size, fill, anchor='start', weight='700', extra=''):
        y = y + size * 0.32
        s.add(f'<text x="{x:.1f}" y="{y:.1f}" font-family="DejaVu Sans" font-size="{size}" font-weight="{weight}" '
              f'fill="{fill}" text-anchor="{anchor}" {extra}>{escape(msg)}</text>')

    def rect(s, r, fill='none', stroke='none', sw=1, rx=0, extra=''):
        s.add(f'<rect x="{r[0]}" y="{r[1]}" width="{r[2]-r[0]}" height="{r[3]-r[1]}" rx="{rx}" fill="{fill}" '
              f'stroke="{stroke}" stroke-width="{sw}" {extra}/>')

    def line(s, x1, y1, x2, y2, stroke, sw=1, extra=''):
        s.add(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{stroke}" stroke-width="{sw}" '
              f'stroke-linecap="round" {extra}/>')

    def circle(s, cx, cy, r, fill='none', stroke='none', sw=1, extra=''):
        s.add(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{r:.1f}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}" {extra}/>')

    # -- furniture ----------------------------------------------------------
    def backdrop(s, info, status):
        s.rect([0, 0, W, H], fill='#121212')
        s.rect([0, 0, W, HDR], fill='#0c0c0c')
        s.line(0, HDR - 1, W, HDR - 1, '#e9e6dc', 2)
        for x, y in s.L['screws']:
            s.circle(x, y, 3.2, fill='#3a3a3a', stroke='#000', sw=0.6)
            s.line(x - 2.2, y - 2.2, x + 2.2, y + 2.2, '#222', 0.8)
        s.add(f'<g id="wm" transform="translate(22,{HDR/2 - 12:.1f})"></g>')
        s.line(282, 16, 282, 40, '#5a5852', 1)
        s.text(296, HDR / 2, info, 10.5, MUTED, weight='normal')
        # status window, right
        w = 250
        r = [W - 16 - w, 14, W - 16, 42]
        s.rect(r, fill='#0c0c0c', stroke=RED, sw=1.2, rx=3)
        s.circle(r[0] + 16, 28, 4.5, fill=RED)
        s.text(r[0] + 30, 29, status, 11, INK)

    def section(s, r, title, sub=''):
        s.rect(r, fill='#151515', stroke='#d9d6cc', sw=1.2, rx=2)
        tx, ty = r[0] + 12, r[1] + 20
        tw = len(title) * 10.4 + 14
        s.rect([tx - 2, ty - 10, tx - 2 + tw, ty + 10], fill='#e9e6dc', rx=1)
        s.text(tx + 5, ty + 0.5, title, 13, '#121212')
        if sub:
            s.text(tx + tw + 12, ty, sub, 9.5, '#8e8b84', weight='normal')
        s.L['sections'].append(dict(rect=r, title=title))

    def badge(s, cx, cy, ch, sub=None):
        s.circle(cx, cy, 11, stroke=TICK, sw=1.4)
        s.text(cx, cy + 0.5, ch, 12, INK, anchor='middle')
        if sub:
            s.text(cx, cy + 20, sub, 8, MUTED, anchor='middle')

    def knob(s, cx, top, dia, v, value, label, bipolar=False):
        cy = top + dia / 2
        outer = dia / 2
        body = outer - (6 if dia >= 44 else 5)
        ang = -135 + 270 * v
        for i in range(11):
            t = -135 + 27 * i
            lg = i in (0, 5, 10)
            x0, y0 = pol(cx, cy, body + 1.5, t)
            x1, y1 = pol(cx, cy, outer - (0 if lg else 1.8), t)
            s.line(x0, y0, x1, y1, TICK, 1.3 if lg else 0.9)
        if dia >= 50:
            lo, hi = ('-', '+') if bipolar else ('0', '10')
            x, y = pol(cx, cy, outer + 2, -138)
            s.text(x - 2, y + 4, lo, 7.5, TICK, anchor='end', weight='normal')
            x, y = pol(cx, cy, outer + 2, 138)
            s.text(x + 2, y + 4, hi, 7.5, TICK, weight='normal')
        teeth, pts = 28, []
        for i in range(teeth * 2):
            rr = body if i % 2 == 0 else body - 1.4
            pts.append('%.2f,%.2f' % pol(cx, cy, rr, 360 * i / (teeth * 2)))
        s.add(f'<polygon points="{" ".join(pts)}" fill="#0a0a0a" stroke="#2f2f2f" stroke-width="0.6"/>')
        s.circle(cx, cy, body * 0.78, fill='url(#capgrad)', stroke='#000', sw=0.8)
        x0, y0 = pol(cx, cy, body * 0.15, ang)
        x1, y1 = pol(cx, cy, body - 0.5, ang)
        s.line(x0, y0, x1, y1, '#f4f1e8', 2.2 if dia >= 44 else 1.8)
        ty = top + dia + 6
        s.text(cx, ty + 7, value, 10, INK, anchor='middle', weight='normal')
        s.text(cx, ty + 20, label, 9.5, MUTED, anchor='middle')
        s.L['controls'].append(dict(kind='knob', rect=[cx - dia / 2, top, cx + dia / 2, ty + 26],
                                    diameter=dia, label=label.title()))

    def selector(s, cx, cy, names, cur, label, r=13, angles=None, lr=None, label_dy=None, small=False):
        n = len(names)
        angles = angles or ([-45, 45] if n == 2 else [-60 + 120 * i / (n - 1) for i in range(n)])
        lr = lr or r + 11
        fs = 8.5 if small else 9.5
        for i, (t, nm) in enumerate(zip(angles, names)):
            act = i == cur
            x0, y0 = pol(cx, cy, r + 2.5, t)
            x1, y1 = pol(cx, cy, r + 7, t)
            s.line(x0, y0, x1, y1, RED if act else TICK, 2 if act else 1.5)
            if not nm:
                continue
            lx, ly = pol(cx, cy, lr, t)
            anchor = 'middle' if abs(t) < 10 or abs(abs(t) - 180) < 10 else ('end' if t < 0 else 'start')
            dx = 0 if anchor == 'middle' else (-2 if t < 0 else 2)
            dy = -6 if (anchor == 'middle' and abs(t) < 10) else 2
            s.text(lx + dx, ly + dy, nm, fs, INK if act else MUTED, anchor=anchor)
        ang = angles[cur]
        s.circle(cx, cy, r * 0.8, fill='#0b0b0b', stroke='#444', sw=0.8)
        tip = pol(cx, cy, r + 3, ang)
        hw = r * 0.55
        l, rg = pol(cx, cy, hw, ang - 90), pol(cx, cy, hw, ang + 90)
        tail = pol(cx, cy, r, ang + 180)
        s.add(f'<path d="M{tip[0]:.1f},{tip[1]:.1f} L{rg[0]:.1f},{rg[1]:.1f} Q{tail[0]:.1f},{tail[1]:.1f} '
              f'{l[0]:.1f},{l[1]:.1f} Z" fill="url(#capgrad)" stroke="#000" stroke-width="0.8"/>')
        x0, y0 = pol(cx, cy, 2, ang)
        s.line(x0, y0, tip[0], tip[1], '#f4f1e8', 1.8)
        if label:
            s.text(cx, cy + (label_dy if label_dy is not None else r + 16), label, 9.5, MUTED, anchor='middle')
        s.L['controls'].append(dict(kind='switch', label=label.title(), selector=dict(cx=cx, cy=cy, radius=r)))

    def lamp(s, cx, cy, on, label=None, anchor='start'):
        s.circle(cx, cy, 5.5, fill='#000', stroke=TICK, sw=1)
        s.circle(cx, cy, 4, fill='#ff3b2f' if on else '#3a1512')
        if on:
            s.circle(cx, cy, 8, fill='#ff3b2f', extra='opacity="0.18"')
        s.circle(cx - 1.2, cy - 1.2, 1.3, fill='#fff', extra='opacity="0.3"')
        if label:
            s.text(cx + (10 if anchor == 'start' else -10), cy + 0.5, label, 9.5, MUTED, anchor=anchor)

    def oled(s, r, caption=None, note=None):
        """black cut-out, true-black glass, dot grid; returns the plot rect"""
        s.rect([r[0] - 3, r[1] - 3, r[2] + 3, r[3] + 3], fill='#050505', stroke='#3c3c3c', sw=1, rx=4)
        s.rect(r, fill='#000', rx=2)
        for x in range(int(r[0]) + 24, int(r[2]) - 1, 24):
            for y in range(int(r[1]) + 12, int(r[3]) - 1, 12):
                s.circle(x, y, 0.6, fill='#2a2d33')
        for y in range(int(r[1]) + 24, int(r[3]) - 1, 24):
            for x in range(int(r[0]) + 12, int(r[2]) - 1, 12):
                s.circle(x, y, 0.6, fill='#2a2d33')
        if caption:
            s.text(r[0] + 8, r[1] + 11, caption, 9.5, '#8e8b84', weight='normal')
        if note:
            s.text(r[2] - 8, r[1] + 11, note, 9.5, s.T['trace'], anchor='end', weight='normal')
        s.L['screens'].append(dict(rect=list(r)))

    def glass(s, r):
        s.ngrad += 1
        gid = 'gl%d' % s.ngrad
        s.add(f'<linearGradient id="{gid}" x1="0" y1="0" x2="0.35" y2="1"><stop offset="0" stop-color="#fff" '
              f'stop-opacity="0.07"/><stop offset="0.45" stop-color="#fff" stop-opacity="0.015"/>'
              f'<stop offset="0.46" stop-color="#fff" stop-opacity="0"/></linearGradient>')
        s.rect(r, fill=f'url(#{gid})', rx=2)
        s.line(r[0] + 3, r[1] + 0.8, r[2] - 3, r[1] + 0.8, '#fff', 0.6, extra='opacity="0.12"')

    def vgrad(s, col, a0=0.38, a1=0.02, horiz=False):
        s.ngrad += 1
        gid = 'vg%d' % s.ngrad
        x2, y2 = ('1', '0') if horiz else ('0', '1')
        s.add(f'<linearGradient id="{gid}" x1="0" y1="0" x2="{x2}" y2="{y2}"><stop offset="0" stop-color="{col}" '
              f'stop-opacity="{a0}"/><stop offset="1" stop-color="{col}" stop-opacity="{a1}"/></linearGradient>')
        return f'url(#{gid})'

    def filter_curve(s, r, col, cutoff=0.82, res=0.5):
        """ladder low-pass magnitude, log frequency, in plot rect r"""
        pl, pt, pr, pb = r
        pts = []
        for i in range(81):
            x = i / 80
            f = 20 * (1000 ** x)
            fc = 20 * (1000 ** cutoff)
            w = f / fc
            mag = 1 / math.sqrt((1 - w * w) ** 2 + (w / (0.7 / (1 + res))) ** 2) if w < 1.6 else (1 / w ** 4)
            db = 20 * math.log10(max(mag, 1e-4))
            y = 0.62 + db / 60
            pts.append((pl + (pr - pl) * x, pb - max(0, min(1, y)) * (pb - pt)))
        d = ' '.join('%.1f,%.1f' % p for p in pts)
        s.add(f'<polygon points="{pl},{pb} {d} {pr},{pb}" fill="{s.vgrad(col)}"/>')
        s.add(f'<polyline points="{d}" fill="none" stroke="{col}" stroke-width="1.6" stroke-linejoin="round" filter="url(#bloom)"/>')

    def svg(s, name):
        defs = ('<defs><radialGradient id="capgrad" cx="40%" cy="35%" r="70%"><stop offset="0" stop-color="#3a3a3a"/>'
                '<stop offset="0.6" stop-color="#161616"/><stop offset="1" stop-color="#050505"/></radialGradient>'
                '<filter id="bloom" filterUnits="userSpaceOnUse" x="0" y="0" width="1200" height="640"><feGaussianBlur stdDeviation="0.9" result="b"/>'
                '<feComponentTransfer in="b" result="c"><feFuncA type="linear" slope="0.7"/></feComponentTransfer>'
                '<feMerge><feMergeNode in="c"/><feMergeNode in="SourceGraphic"/></feMerge></filter></defs>')
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">'
               + defs + ''.join(s.p) + '</svg>')
        open(os.path.join(OUT, name + '.svg'), 'w').write(svg)
        s.L['word2'] = s.T['word2']
        json.dump(s.L, open(os.path.join(OUT, name + '.json'), 'w'))
        print('wrote', name)


# tap pattern: tap n at n x 64 ms; level decays by loop scale; the matrix
# puts tap n at nScale = 1 - (5-n)/10 in its own side, so tap 5 is hard.
TAP_MS = 64
TAPS = [(n, n * TAP_MS, 0.95 * (0.86 ** (n - 1)), 1 - (5 - n) / 10) for n in range(1, 6)]


# ===========================================================================
# 1  TAP SCOPE: a wide OLED scope over four L/R sections
# ===========================================================================
def design1():
    P = Panel('green')
    T = P.T
    P.backdrop('48000 HZ  /  LINE 8.0 S  /  5 TAPS A SIDE', 'ENABLED - HOST 120 BPM')
    # --- echo scope ----------------------------------------------------------
    P.section([16, 72, 700, 300], 'ECHO', 'FIVE TAPS EACH SIDE, CROSS-PANNED')
    r = [32, 104, 684, 288]
    P.oled(r, 'TAP SCOPE', f'{TAP_MS} MS / TAP')
    pl, pr = r[0] + 34, r[2] - 18
    top, bot = r[1] + 24, r[3] - 22
    mid, half = (top + bot) / 2, (bot - top) / 2
    span = 6 * TAP_MS
    P.line(pl, mid, pr, mid, '#3b4048', 1)
    for ms in range(0, span + 1, TAP_MS // 2):
        x = pl + (pr - pl) * ms / span
        P.line(x, mid - 3, x, mid + 3, '#555b63', 1)
        if ms % TAP_MS == 0:
            P.text(x, r[3] - 10, str(ms), 8.5, '#6f747c', anchor='middle', weight='normal')
    P.text(r[0] + 12, mid - half * 0.5, 'L', 10, T['trace'])
    P.text(r[0] + 12, mid + half * 0.5, 'R', 10, T['alt'])
    P.line(pl, mid - half * 0.9, pl, mid + half * 0.9, '#e9e6dc', 2)
    P.text(pl + 6, top + 2, 'DRY', 8, '#8e8b84', weight='normal')
    for n, ms, lev, own in TAPS:
        x = pl + (pr - pl) * ms / span
        for col, sgn, frac in ((T['trace'], -1, own), (T['alt'], 1, own)):
            hgt = lev * frac * half * 0.95
            # own side, bright
            P.rect([x - 9 if sgn < 0 else x + 1, mid - hgt if sgn < 0 else mid, x - 1 if sgn < 0 else x + 9,
                    mid if sgn < 0 else mid + hgt], fill=P.vgrad(col, 0.95 if sgn > 0 else 0.25, 0.25 if sgn > 0 else 0.95),
                   extra='filter="url(#bloom)"')
        # cross-feed, dim, on the far side
        lk = lev * (1 - own) * half * 0.95
        P.rect([x + 1, mid - lk, x + 9, mid], fill=T['alt'], extra='opacity="0.35"')
        P.rect([x - 9, mid, x - 1, mid + lk], fill=T['trace'], extra='opacity="0.35"')
        P.text(x, top + 2, f'T{n}', 8, '#8e8b84', anchor='middle', weight='normal')
    P.glass(r)
    # --- clock + master -----------------------------------------------------
    P.section([716, 72, 1184, 300], 'CLOCK', 'TEMPO SYNC + MASTER')
    P.selector(800, 180, ['OFF', '32ND', '16TH', '8TH', 'QTR', 'HALF', 'WHOLE'], 0, 'SYNC RATE',
               r=20, angles=[-120, -80, -40, 0, 40, 80, 120], lr=34, label_dy=64)
    tr = [890, 108, 1046, 160]
    P.oled(tr, 'TEMPO', 'HOST')
    P.text(968, 143, '120.0 BPM', 15, T['trace'], anchor='middle', extra='filter="url(#bloom)"')
    P.glass(tr)
    P.knob(968, 188, 44, 100 / 300, '100', 'TEMPO')
    P.line(1066, 104, 1066, 288, '#2e2e2e', 1)
    P.selector(1100, 132, ['', ''], 1, '', r=10)
    P.text(1116, 132, 'THRU', 9.5, MUTED)
    P.selector(1100, 182, ['', ''], 0, '', r=10)
    P.text(1116, 182, 'TAPS OUT', 9.5, MUTED) if False else P.text(1116, 176, 'TAPS', 9.5, MUTED)
    P.text(1116, 189, 'OUT', 9.5, MUTED)
    P.lamp(1090, 236, True, 'ENABLED')
    P.lamp(1090, 264, False, 'BYPASS')
    # --- four L/R sections ----------------------------------------------------
    rowL, rowR = 380, 512
    ly = 486
    secs = [([16, 316, 316, 624], 'DELAY', 'TIME + REGEN'),
            ([332, 316, 584, 624], 'FEEDBACK', ''),
            ([600, 316, 936, 624], 'FILTER', 'LADDER LOW-PASS'),
            ([952, 316, 1184, 624], 'OUTPUT', 'LEVELS')]
    for rr, t, sub in secs:
        P.section(rr, t, sub)
        P.badge(rr[0] + 24, rowL + 24, 'L')
        P.badge(rr[0] + 24, rowR + 24, 'R')
        P.line(rr[0] + 12, ly, rr[2] - 12, ly, '#2e2e2e', 1)
    for top in (rowL, rowR):
        P.knob(110, top - 12, 56, 0.2, '64 MS', 'DELAY')
        P.knob(226, top - 6, 44, 0.1, '5 %', 'LOOP SCALE')
        P.knob(420, top - 12, 56, 0.05, '10 %', 'FEEDBACK')
        P.knob(680, top - 12, 56, 0.9, '8.1K', 'CUTOFF')
        P.knob(766, top - 6, 44, 0.167, '0.50 Q', 'RESONANCE')
        P.knob(848, top - 6, 44, 0.5, '250 %', 'GAIN')
        P.selector(906, top + 16, ['', ''], 1, 'ON', r=9, label_dy=20, small=True)
        P.knob(1040, top - 6, 44, 0.5, '100 %', 'TAPS GAIN')
        P.knob(1130, top - 6, 44, 0.5, '100 %', 'FB OUT')
    # links sit on the rule between the L and R rows, under what they join
    for x in (150, 266, 470, 720):
        P.selector(x, ly, ['', ''], 1, '', r=8)
        P.text(x + 14, ly + 1, 'LINK', 8, RED)
    # the two feedback paths are global switches
    P.selector(530, rowL + 22, ['', ''], 1, 'MAIN', r=10, label_dy=22, small=True)
    P.selector(530, rowR + 22, ['', ''], 0, 'TAPS', r=10, label_dy=22, small=True)
    P.text(530, 344, 'FB PATHS', 8, '#8e8b84', anchor='middle', weight='normal')
    P.svg('SpaceDub-Polivoks-1-TapScope')


# ===========================================================================
# 2  CHANNEL STRIPS: L and R as signal-flow rows, links on a spine
# ===========================================================================
def design2():
    P = Panel('amber')
    T = P.T
    P.backdrop('48000 HZ  /  LINE 8.0 S  /  SYNC OFF', 'ENABLED - HOST 120 BPM')
    strips = [([16, 72, 1184, 252], 'LEFT', 'L'), ([16, 332, 1184, 512], 'RIGHT', 'R')]
    xs = dict(delay=150, scale=240, fb=330, cut=462, res=552, gain=634, tgain=752, fbout=842)
    for rr, title, ch in strips:
        P.section(rr, title + ' CHANNEL', 'DELAY  >  FEEDBACK  >  FILTER  >  OUT')
        top = rr[1] + 46
        P.badge(52, top + 30, ch)
        P.knob(xs['delay'], top, 60, 0.2, '64 MS', 'DELAY')
        P.knob(xs['scale'], top + 8, 44, 0.1, '5 %', 'LOOP SCALE')
        P.knob(xs['fb'], top + 8, 44, 0.05, '10 %', 'FEEDBACK')
        P.knob(xs['cut'], top, 60, 0.9, '8.1K', 'CUTOFF')
        P.knob(xs['res'], top + 8, 44, 0.167, '0.50 Q', 'RESONANCE')
        P.knob(xs['gain'], top + 8, 44, 0.5, '250 %', 'FILTER GAIN')
        P.selector(694, top + 30, ['', ''], 1, 'FILTER', r=11, label_dy=24, small=True)
        P.knob(xs['tgain'], top + 8, 44, 0.5, '100 %', 'TAPS GAIN')
        P.knob(xs['fbout'], top + 8, 44, 0.5, '100 %', 'FB OUT')
        for x in (402,):
            P.add(f'<path d="M{x-4},{top+22} L{x+3},{top+30} L{x-4},{top+38}" fill="none" stroke="{TICK}" '
                  f'stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>')
        # per-channel OLED: taps above, filter curve below
        o = [900, rr[1] + 14, 1168, rr[3] - 14]
        P.oled(o, ch + '  TAPS + FILTER', '24 DB/OCT')
        pl, pr = o[0] + 10, o[2] - 10
        base = o[1] + 84
        for n, ms, lev, own in TAPS:
            x = pl + 8 + (pr - pl - 16) * ms / 400
            P.line(x, base, x, base - lev * own * 58, T['trace'], 5, extra='filter="url(#bloom)"')
        P.line(pl + 8, base, pl + 8, base - 58, '#e9e6dc', 2)
        P.line(pl, base + 0.5, pr, base + 0.5, '#3b4048', 1)
        P.filter_curve([pl, base + 18, pr, o[3] - 18], T['alt'])
        P.text(pl, o[3] - 9, '20 HZ', 8.5, '#8e8b84', weight='normal')
        P.text(pr, o[3] - 9, '8.1 KHZ', 8.5, T['alt'], anchor='end', weight='normal')
        P.glass(o)
    # link spine between the strips
    sp = [16, 262, 1184, 322]
    P.rect(sp, fill='#0f0f0f', stroke='#3a3a3a', sw=1, rx=2)
    P.text(32, 292, 'LINK', 11, RED)
    for x, nm in ((xs['delay'], 'DELAYS'), (xs['scale'], 'LOOP SCALE'), (xs['fb'], 'FEEDBACK'), (xs['res'], 'FILTERS')):
        P.line(x, 252, x, 332, '#4a4a4a', 1, extra='stroke-dasharray="2 3"')
        P.selector(x, 286, ['', ''], 1, nm, r=10, label_dy=24, small=True)
    P.text(900, 292, 'ONE KNOB DRIVES BOTH SIDES WHEN LINKED', 8.5, '#6f6c66', weight='normal')
    # master row
    P.section([16, 528, 1184, 624], 'MASTER', '')
    y = 586
    P.selector(70, y + 4, ['OFF', 'ON'], 1, 'ENABLED', r=12, label_dy=24)
    P.selector(170, y + 4, ['OFF', 'ON'], 1, 'MAIN FB', r=12, label_dy=24)
    P.selector(270, y + 4, ['OFF', 'ON'], 0, 'TAPS FB', r=12, label_dy=24)
    P.selector(370, y + 4, ['OFF', 'ON'], 0, 'TAPS OUT', r=12, label_dy=24)
    P.selector(470, y + 4, ['OFF', 'ON'], 1, 'THRU', r=12, label_dy=24)
    P.selector(600, y + 4, ['OFF', '32', '16', '8', '4', '2', '1'], 0, 'SYNC', r=12,
               angles=[-90, -60, -30, 0, 30, 60, 90], lr=22, label_dy=24, small=True)
    tr = [700, 548, 900, 612]
    P.oled(tr, 'TEMPO', 'HOST')
    P.text(800, 590, '120.0 BPM', 16, T['trace'], anchor='middle', extra='filter="url(#bloom)"')
    P.glass(tr)
    P.knob(960, 546, 40, 1 / 3, '', '')
    P.text(1000, 566, 'TEMPO', 9.5, MUTED)
    P.text(1000, 582, '100 BPM', 9.5, INK, weight='normal')
    P.lamp(1120, 566, True, 'ACTIVE', anchor='end')
    P.lamp(1120, 596, False, 'BYPASS', anchor='end')
    P.svg('SpaceDub-Polivoks-2-ChannelStrips')


# ===========================================================================
# 3  ECHO MAP: symmetrical console, a radar-style OLED in the middle
# ===========================================================================
def design3():
    P = Panel('cyan')
    T = P.T
    P.backdrop('48000 HZ  /  LINE 8.0 S  /  STEREO', 'ENABLED - HOST 120 BPM')
    # wings
    for rr, ch, side in (([16, 72, 392, 624], 'LEFT', 0), ([808, 72, 1184, 624], 'RIGHT', 1)):
        P.section(rr, ch, 'CHANNEL ' + ch[0])
        x0 = rr[0]
        cols = [x0 + 70, x0 + 188, x0 + 306] if side == 0 else [x0 + 306, x0 + 188, x0 + 70]
        # row 1 time
        P.text(rr[0] + 14, 120, 'TIME', 10.5, INK)
        P.knob(cols[0], 132, 60, 0.2, '64 MS', 'DELAY')
        P.knob(cols[1], 140, 44, 0.1, '5 %', 'LOOP SCALE')
        P.knob(cols[2], 140, 44, 0.05, '10 %', 'FEEDBACK')
        P.line(rr[0] + 12, 244, rr[2] - 12, 244, '#2e2e2e', 1)
        P.text(rr[0] + 14, 262, 'FILTER', 10.5, INK)
        P.knob(cols[0], 276, 60, 0.9, '8.1K', 'CUTOFF')
        P.knob(cols[1], 284, 44, 0.167, '0.50 Q', 'RESONANCE')
        P.knob(cols[2], 284, 44, 0.5, '250 %', 'GAIN')
        P.selector(cols[2] + (-48 if side == 0 else 48), 270, ['', ''], 1, '', r=9)
        P.text(cols[2] + (-48 if side == 0 else 48), 256, 'ON', 8.5, MUTED, anchor='middle')
        P.line(rr[0] + 12, 388, rr[2] - 12, 388, '#2e2e2e', 1)
        P.text(rr[0] + 14, 406, 'OUTPUT', 10.5, INK)
        P.knob(cols[1], 424, 44, 0.5, '100 %', 'TAPS GAIN')
        P.knob(cols[2], 424, 44, 0.5, '100 %', 'FB OUT')
        P.badge(cols[0], 446, ch[0], 'CH')
        # small per-side filter OLED at the bottom
        o = [rr[0] + 16, 520, rr[2] - 16, 608]
        P.oled(o, 'LADDER', '8.1K  0.50 Q')
        P.filter_curve([o[0] + 8, o[1] + 22, o[2] - 8, o[3] - 6], T['trace'])
        P.glass(o)
    # centre: echo map
    P.section([408, 72, 792, 624], 'ECHO MAP', '')
    r = [424, 104, 776, 384]
    P.oled(r, 'PAN  /  TIME', f'{TAP_MS} MS / TAP')
    cx, top, bot = (r[0] + r[2]) / 2, r[1] + 30, r[3] - 14
    halfw = (r[2] - r[0]) / 2 - 26
    span = bot - top - 6
    for k in range(1, 6):
        d = span * k / 5
        P.add(f'<path d="M{cx - halfw:.1f},{top:.1f} A{halfw:.1f},{d:.1f} 0 0 0 {cx + halfw:.1f},{top:.1f}" '
              f'fill="none" stroke="#1d2a33" stroke-width="1"/>')
        P.text(cx + 4, top + d - 6, f'{k * TAP_MS}', 7.5, '#4e5a63', weight='normal')
    P.line(cx, top, cx, bot, '#1d2a33', 1)
    P.line(r[0] + 10, top, r[2] - 10, top, '#1d2a33', 1)
    P.text(r[0] + 10, r[3] - 12, 'L', 10, T['trace'])
    P.text(r[2] - 10, r[3] - 12, 'R', 10, T['alt'], anchor='end')
    P.circle(cx, top, 4, fill='#e9e6dc')
    for n, ms, lev, own in TAPS:
        d = span * n / 5
        u = (own - 0.5) / 0.5 * 0.92 * halfw
        y = top + d * math.sqrt(max(0, 1 - (u / halfw) ** 2))
        for sgn, col in ((-1, T['trace']), (1, T['alt'])):
            x = cx + sgn * u
            P.circle(x, y, 3 + 4 * lev, fill=col, extra='filter="url(#bloom)"')
            P.circle(x, y, 9 + 6 * lev, fill='none', stroke=col, sw=0.8, extra=f'opacity="{0.25 + 0.3 * lev:.2f}"')
    P.glass(r)
    # centre controls
    P.selector(480, 450, ['OFF', '32ND', '16TH', '8TH', 'QTR', 'HALF', 'WHOLE'], 0, 'SYNC RATE',
               r=18, angles=[-120, -80, -40, 0, 40, 80, 120], lr=31, label_dy=52)
    tr = [560, 402, 776, 446]
    P.oled(tr, None, None)
    P.text(572, 424, 'HOST', 9.5, '#8e8b84', weight='normal')
    P.text(764, 424, '120.0 BPM', 15, T['trace'], anchor='end', extra='filter="url(#bloom)"')
    P.glass(tr)
    P.knob(612, 456, 40, 1 / 3, '100', 'TEMPO')
    P.selector(694, 482, ['', ''], 1, 'THRU', r=10, label_dy=24, small=True)
    P.selector(748, 482, ['', ''], 0, 'TAPS OUT', r=10, label_dy=24, small=True)
    # links row
    P.line(424, 540, 776, 540, '#2e2e2e', 1)
    P.text(424, 556, 'LINK', 10.5, RED)
    for x, nm in ((486, 'DELAY'), (558, 'SCALE'), (630, 'FEEDBK'), (702, 'FILTER')):
        P.selector(x + 28, 580, ['', ''], 1, nm, r=9, label_dy=22, small=True)
    P.selector(480 - 50 + 50, 0, [], 0, '') if False else None
    # master toggles along the top of the centre (under the title)
    P.lamp(540, 92, True, 'ON')
    P.selector(610, 92, ['', ''], 1, '', r=8)
    P.text(624, 92, 'MAIN FB', 8.5, MUTED)
    P.selector(696, 92, ['', ''], 0, '', r=8)
    P.text(710, 92, 'TAPS FB', 8.5, MUTED)
    P.svg('SpaceDub-Polivoks-3-EchoMap')


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    design1(); design2(); design3()
