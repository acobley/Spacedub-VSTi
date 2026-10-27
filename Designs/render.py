import sys, os, json
from playwright.sync_api import sync_playwright
d = os.path.dirname(os.path.abspath(__file__))
js = open(os.path.join(d, 'rbfont.js')).read()
with sync_playwright() as p:
    b = p.chromium.launch()
    for name in sys.argv[1:]:
        svg = open(os.path.join(d, name + '.svg')).read()
        L = json.load(open(os.path.join(d, name + '.json')))
        html = f"""<!doctype html><html><head><meta charset="utf-8"><style>html,body{{margin:0;background:#121212}}svg{{display:block}}</style></head>
<body>{svg}<script>{js}
const wm = document.getElementById('wm');
const big = (t, col, x) => {{ const a = rbPaths(t, 24, 1.7, 1.2); return {{ a, svg: `<g transform="translate(${{x}},0)"><g fill="none" stroke="${{col}}" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round">${{a.paths}}</g><g fill="none" stroke="#0c0c0c" stroke-width=".32" stroke-linecap="round" stroke-linejoin="round">${{a.paths}}</g></g>` }}; }};
const p1 = big('SPACE', '#f4f1e8', 0), p2 = big('DUB', '{L['word2']}', p1.a.width + 12);
wm.innerHTML = p1.svg + p2.svg;
window.wmWidth = p1.a.width + 12 + p2.a.width;
rbConvert();
</script></body></html>"""
        open(os.path.join(d, name + '.html'), 'w').write(html)
        pg = b.new_page(viewport={'width': 1200, 'height': 640}, device_scale_factor=2)
        pg.goto('file://' + os.path.join(d, name + '.html')); pg.wait_for_timeout(300)
        print(name, 'wordmark', pg.evaluate('window.wmWidth'), 'page', pg.evaluate('[document.body.scrollWidth, document.body.scrollHeight]'))
        pg.screenshot(path=os.path.join(d, name + '-clean.png'), clip={'x': 0, 'y': 0, 'width': 1200, 'height': 640})
    b.close()
