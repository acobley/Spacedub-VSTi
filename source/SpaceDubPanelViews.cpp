//------------------------------------------------------------------------
// SpaceDub - the panel's views (the black panel)
//
// See the banner in SpaceDubPanelViews.h: most of this file is R2Synth's
// R2SynthPanelViews.cpp with the namespace changed.
//------------------------------------------------------------------------

#include "SpaceDubPanelViews.h"
#include "DelayLine.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>

using namespace VSTGUI;

namespace SpaceDub {
namespace Panel {

//------------------------------------------------------------------------
namespace {

constexpr double kPi = 3.14159265358979323846;

/** Taps a side - DelayLine's, so the scope cannot disagree with the DSP. */
constexpr int DelayLineTaps = DelayLine::kNumTaps;

/** VSTGUI's angles: degrees, 0 at three o'clock, clockwise (y is down). */
CPoint pointAt (const CPoint& centre, double radius, double degrees)
{
	const double t = degrees * kPi / 180.0;
	return CPoint (centre.x + radius * std::cos (t), centre.y + radius * std::sin (t));
}

/** The layout's angles: degrees from twelve o'clock, clockwise. */
CPoint pointFromTop (const CPoint& centre, double radius, double degrees)
{
	return pointAt (centre, radius, degrees - 90.0);
}

CRect circleRect (const CPoint& centre, double radius)
{
	return CRect (centre.x - radius, centre.y - radius, centre.x + radius, centre.y + radius);
}

CRect toCRect (const Layout::Rect& r)
{
	return CRect (r.left, r.top, r.right, r.bottom);
}

CLineStyle roundLine ()
{
	return CLineStyle (CLineStyle::kLineCapRound, CLineStyle::kLineJoinRound);
}

void strokeLine (CDrawContext* context, const CPoint& from, const CPoint& to, const CColor& colour, CCoord width)
{
	context->setFrameColor (colour);
	context->setLineWidth (width);
	context->setLineStyle (roundLine ());
	context->drawLine (from, to);
}

void fillCircle (CDrawContext* context, const CPoint& centre, double radius, const CColor& fill)
{
	context->setFillColor (fill);
	context->drawEllipse (circleRect (centre, radius), kDrawFilled);
}

void strokeCircle (CDrawContext* context, const CPoint& centre, double radius, const CColor& colour, CCoord width)
{
	context->setFrameColor (colour);
	context->setLineWidth (width);
	context->setLineStyle (kLineSolid);
	context->drawEllipse (circleRect (centre, radius), kDrawStroked);
}

void roundRect (CDrawContext* context, const CRect& r, CCoord radius, const CColor* fill,
                const CColor* stroke = nullptr, CCoord strokeWidth = 1.)
{
	auto path = owned (context->createGraphicsPath ());
	if (!path)
		return;
	path->addRoundRect (r, radius);
	if (fill)
	{
		context->setFillColor (*fill);
		context->drawGraphicsPath (path, CDrawContext::kPathFilled);
	}
	if (stroke)
	{
		context->setFrameColor (*stroke);
		context->setLineWidth (strokeWidth);
		context->setLineStyle (kLineSolid);
		context->drawGraphicsPath (path, CDrawContext::kPathStroked);
	}
}

/** A black knob's cap: lit from the top left, a near-black at the rim. */
void fillCap (CDrawContext* context, CGraphicsPath* path, const CPoint& centre, double radius)
{
	auto gradient = owned (CGradient::create (0.0, 1.0, CColor (58, 58, 58, 255), CColor (5, 5, 5, 255)));
	if (!gradient)
		return;
	gradient->addColorStop (0.6, CColor (22, 22, 22, 255));
	context->fillRadialGradient (path, *gradient, centre, radius * 1.4,
	                             CPoint (-radius * 0.25, -radius * 0.35));
}

//------------------------------------------------------------------------
// The wear (PanelWear.h): the marks as paths
//------------------------------------------------------------------------
CColor wearColour (const Wear::Rgb& c, uint8_t alpha)
{
	return CColor (c.r, c.g, c.b, alpha);
}

/** Does the box (l, t, r, b) touch the area being drawn? */
bool touches (const CRect& clip, double l, double t, double r, double b)
{
	return r >= clip.left && l <= clip.right && b >= clip.top && t <= clip.bottom;
}

bool lineTouches (const CRect& clip, const std::vector<Wear::Pt>& pts, double pad)
{
	if (pts.empty ())
		return false;
	double l = pts[0].x, t = pts[0].y, r = l, b = t;
	for (const auto& p : pts)
	{
		l = std::min (l, p.x); r = std::max (r, p.x);
		t = std::min (t, p.y); b = std::max (b, p.y);
	}
	return touches (clip, l - pad, t - pad, r + pad, b + pad);
}

void polyline (CDrawContext* context, const std::vector<Wear::Pt>& pts, size_t from, size_t to,
               const CColor& colour, CCoord width)
{
	if (to <= from || to >= pts.size () + 1)
		return;
	auto path = owned (context->createGraphicsPath ());
	if (!path)
		return;
	path->beginSubpath (CPoint (pts[from].x, pts[from].y));
	for (size_t i = from + 1; i < to; ++i)
		path->addLine (CPoint (pts[i].x, pts[i].y));
	context->setFrameColor (colour);
	context->setLineWidth (width);
	context->setLineStyle (roundLine ());
	context->drawGraphicsPath (path, CDrawContext::kPathStroked);
}

/** A hairline: half strength in its first and last thirds, so it fades
    in and out. */
void wearLine (CDrawContext* context, const Wear::Line& line, const Wear::Rgb& rgb)
{
	const size_t n = line.pts.size ();
	if (n < 2)
		return;
	const size_t a = std::max<size_t> (1, n / 3), b = std::max (a + 1, n - n / 3);
	const uint8_t half = static_cast<uint8_t> (line.alpha / 2);
	polyline (context, line.pts, 0, a + 1, wearColour (rgb, half), line.width);
	polyline (context, line.pts, a, std::min (b + 1, n), wearColour (rgb, line.alpha), line.width);
	if (b < n - 1)
		polyline (context, line.pts, b, n, wearColour (rgb, half), line.width);
}

/** A soft round patch: `alpha` at the centre, none at the edge. */
void softDisc (CDrawContext* context, const CPoint& centre, double radius, const Wear::Rgb& rgb, uint8_t alpha)
{
	auto path = owned (context->createGraphicsPath ());
	auto gradient = owned (CGradient::create (0.0, 1.0, wearColour (rgb, alpha), wearColour (rgb, 0)));
	if (!path || !gradient)
		return;
	gradient->addColorStop (0.5, wearColour (rgb, static_cast<uint8_t> (alpha * 0.55)));
	path->addEllipse (circleRect (centre, radius));
	context->fillRadialGradient (path, *gradient, centre, radius);
}

/** Dust round a knob's foot: a ring that fades both ways from `peak`. */
void footRing (CDrawContext* context, const Wear::FootRing& f)
{
	auto path = owned (context->createGraphicsPath ());
	const CColor clear = wearColour (Wear::kDust, 0);
	auto gradient = owned (CGradient::create (0.0, 1.0, clear, clear));
	if (!path || !gradient)
		return;
	gradient->addColorStop (f.inner / f.outer, clear);
	gradient->addColorStop (f.peak / f.outer, wearColour (Wear::kDust, f.alpha));
	const CPoint centre (f.cx, f.cy);
	path->addEllipse (circleRect (centre, f.outer));
	context->fillRadialGradient (path, *gradient, centre, f.outer);
}

/** Dust drifted against a box edge: fades from the edge inwards. */
void drift (CDrawContext* context, const Wear::Drift& d)
{
	auto path = owned (context->createGraphicsPath ());
	auto gradient = owned (CGradient::create (0.0, 1.0, wearColour (Wear::kDust, d.alpha), wearColour (Wear::kDust, 0)));
	if (!path || !gradient)
		return;
	const CRect r (d.r.left, d.r.top, d.r.right, d.r.bottom);
	path->addRect (r);
	const CPoint mid ((r.left + r.right) / 2., (r.top + r.bottom) / 2.);
	CPoint from = mid, to = mid;
	if (d.fromX < 0) { from.x = r.left;  to.x = r.right; }
	if (d.fromX > 0) { from.x = r.right; to.x = r.left; }
	if (d.fromY < 0) { from.y = r.top;    to.y = r.bottom; }
	if (d.fromY > 0) { from.y = r.bottom; to.y = r.top; }
	context->fillLinearGradient (path, *gradient, from, to);
}

/** The glyphs, parsed once each. */
const std::vector<Lettering::Command>& commandsOf (const Lettering::Glyph& glyph)
{
	static std::map<const char*, std::vector<Lettering::Command>> cache;
	auto it = cache.find (glyph.path);
	if (it == cache.end ())
		it = cache.emplace (glyph.path, Lettering::parse (glyph.path)).first;
	return it->second;
}

} // anonymous namespace

//------------------------------------------------------------------------
// Lettering
//------------------------------------------------------------------------
std::string upper (const std::string& s)
{
	std::string out (s);
	for (auto& c : out)
		c = static_cast<char> (std::toupper (static_cast<unsigned char> (c)));
	return out;
}

void drawLettering (CDrawContext* context, const std::string& text, const Lettering::Style& style,
                    const CColor& colour, CCoord x, CCoord centreY, Align align, double strokeUnits)
{
	double endUnits = 0.0;
	const auto placed = Lettering::layout (text, style, &endUnits);
	if (placed.empty ())
		return;

	const double s = style.scale ();
	const double width = Lettering::width (text, style);
	double left = x;
	if (align == Align::Centre)
		left = x - width / 2.0;
	else if (align == Align::Right)
		left = x - width;
	const double top = centreY - style.height () / 2.0;
	const double half = style.weight / 2.0;

	auto path = owned (context->createGraphicsPath ());
	if (!path)
		return;

	for (const auto& p : placed)
	{
		const double ox = left + (p.x + half) * s;
		const double oy = top + half * s;
		auto at = [&] (double gx, double gy) { return CPoint (ox + gx * s, oy + gy * s); };

		CPoint current, start;
		for (const auto& c : commandsOf (Lettering::glyphOrDot (p.code)))
		{
			switch (c.op)
			{
				case 'M':
					current = start = at (c.x, c.y);
					path->beginSubpath (current);
					break;
				case 'L':
					current = at (c.x, c.y);
					path->addLine (current);
					break;
				case 'Q':
				{
					// The quadratic as a cubic: c1 = p0 + 2/3 (q - p0),
					// c2 = p2 + 2/3 (q - p2).
					const CPoint q = at (c.x1, c.y1);
					const CPoint end = at (c.x, c.y);
					const CPoint c1 (current.x + 2.0 / 3.0 * (q.x - current.x), current.y + 2.0 / 3.0 * (q.y - current.y));
					const CPoint c2 (end.x + 2.0 / 3.0 * (q.x - end.x), end.y + 2.0 / 3.0 * (q.y - end.y));
					path->addBezierCurve (c1, c2, end);
					current = end;
					break;
				}
				case 'Z':
					path->closeSubpath ();
					current = start;
					break;
				default:
					break;
			}
		}
	}

	context->setFrameColor (colour);
	context->setLineWidth ((strokeUnits > 0.0 ? strokeUnits : style.weight) * s);
	context->setLineStyle (roundLine ());
	context->drawGraphicsPath (path, CDrawContext::kPathStroked);
}

//------------------------------------------------------------------------
// Backdrop
//------------------------------------------------------------------------
Backdrop::Backdrop (const CRect& size)
: CView (size)
, mWear (Wear::panelWear ())
{
	setMouseEnabled (false);
}

void Backdrop::drawWear (CDrawContext* context)
{
	CRect clip;
	context->getClipRect (clip);

	// ---- the haze and the drifts: soft, underneath the sharper marks -------
	for (const auto& b : mWear.haze)
		if (touches (clip, b.x - b.r, b.y - b.r, b.x + b.r, b.y + b.r))
			softDisc (context, CPoint (b.x, b.y), b.r, Wear::kDust, b.alpha);
	for (const auto& d : mWear.drifts)
		if (touches (clip, d.r.left, d.r.top, d.r.right, d.r.bottom))
			drift (context, d);
	for (const auto& f : mWear.rings)
		if (touches (clip, f.cx - f.outer, f.cy - f.outer, f.cx + f.outer, f.cy + f.outer))
			footRing (context, f);

	// ---- hairlines through the paint, and the printing ------------------------
	for (const auto& l : mWear.scratches)
		if (lineTouches (clip, l.pts, l.width))
			wearLine (context, l, Wear::kScratch);

	// ---- specks and fibres ------------------------------------------------------
	for (const auto& sp : mWear.specks)
		if (touches (clip, sp.x - sp.r, sp.y - sp.r, sp.x + sp.r, sp.y + sp.r))
			fillCircle (context, CPoint (sp.x, sp.y), sp.r, wearColour (Wear::kSpeck, sp.alpha));
	for (const auto& l : mWear.fibres)
		if (lineTouches (clip, l.pts, l.width))
			wearLine (context, l, Wear::kSpeck);
}

void Backdrop::draw (CDrawContext* context)
{
	using namespace Layout;
	const CRect all = getViewSize ();

	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	// ---- the ground, the header band and the white rule under it --------
	context->setFillColor (Palette::kGround);
	context->drawRect (all, kDrawFilled);
	context->setFillColor (Palette::kHeader);
	context->drawRect (CRect (all.left, all.top, all.right, all.top + kHeaderHeight), kDrawFilled);
	context->setFillColor (Palette::kHeaderRule);
	context->drawRect (CRect (all.left, kHeaderHeight - 2, all.right, kHeaderHeight), kDrawFilled);

	// ---- the corner screws ------------------------------------------------
	for (const auto& s : kScrews)
	{
		const CPoint c (s.x, s.y);
		fillCircle (context, c, 3.2, Palette::kScrew);
		strokeCircle (context, c, 3.2, CColor (0, 0, 0, 255), 0.6);
		strokeLine (context, CPoint (c.x - 2.2, c.y - 2.2), CPoint (c.x + 2.2, c.y + 2.2), CColor (34, 34, 34, 255), 0.8);
	}

	// ---- the wordmark: SPACE in white, DUB in red, with the inline cut ----
	{
		const double centreY = kHeaderHeight / 2.0 + 1.0;
		const double second = kWordmark.left + Lettering::width ("SPACE", kTextWordmark) + 12.0;
		for (int pass = 0; pass < 2; ++pass)
		{
			const double stroke = pass ? 0.32 : -1.0;
			drawLettering (context, "SPACE", kTextWordmark, pass ? Palette::kHeader : Palette::kInk, kWordmark.left,
			               centreY, Align::Left, stroke);
			drawLettering (context, "DUB", kTextWordmark, pass ? Palette::kHeader : Palette::kRed, second, centreY,
			               Align::Left, stroke);
		}
		context->setFillColor (Palette::kDivider);
		context->drawRect (CRect (kHeaderDivider, 16, kHeaderDivider + 1, 40), kDrawFilled);
	}

	// ---- the sections: boxed in white, the title in its own box -----------
	for (const auto& s : kSections)
	{
		CRect r = toCRect (s.r);
		r.inset (0.5, 0.5);
		roundRect (context, r, kSectionRadius, &Palette::kSection, &Palette::kSectionEdge, 1.2);

		const Box b = titleBox (s);
		roundRect (context, CRect (b.left, b.top, b.right, b.bottom), 2., &Palette::kSection,
		           &Palette::kSectionEdge, 1.2);
		const double centre = (b.top + b.bottom) / 2.0;
		drawLettering (context, s.title, kTextTitle, Palette::kInk, b.left + 8.0, centre);
		if (s.subtitle && *s.subtitle)
			drawLettering (context, s.subtitle, kTextSubtitle, Palette::kInkSoft, subtitleLeft (s), centre);
	}

	// ---- the rules ----------------------------------------------------------
	context->setFillColor (Palette::kRule);
	for (const auto& r : kRules)
		context->drawRect (toCRect (r), kDrawFilled);

	// ---- the L and R badges ---------------------------------------------------
	for (const auto& b : kBadges)
	{
		const CPoint c (b.centre.x, b.centre.y);
		strokeCircle (context, c, kBadgeRadius, Palette::kSectionEdge, 1.4);
		drawLettering (context, b.text, kTextBadge, Palette::kInk, c.x, c.y, Align::Centre);
	}

	// ---- the screens' black cut-outs: the OLEDs sit in them -----------------
	for (const auto& s : kScreens)
	{
		CRect r = toCRect (bezelOf (s));
		r.inset (0.5, 0.5);
		roundRect (context, r, 4., &Palette::kBezel, &Palette::kBezelEdge, 1.);
	}

	// ---- the wear, over the printing ----------------------------------------
	drawWear (context);

	setDirty (false);
}

//------------------------------------------------------------------------
// Knob
//------------------------------------------------------------------------
namespace {
/** Pixels of vertical drag for the whole range. */
constexpr float kDragPixels = 200.f;
} // anonymous namespace

Knob::Knob (const CRect& size, IControlListener* listener, int32_t tag, int diameter, bool bipolar)
: CControl (size, listener, tag)
, mDiameter (diameter)
, mBipolar (bipolar)
{
}

void Knob::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	const double v = std::clamp (static_cast<double> (getValueNormalized ()), 0.0, 1.0);
	const CPoint centre (r.left + r.getWidth () / 2., r.top + mDiameter / 2.);
	const double outer = mDiameter / 2.0;
	const double body = outer - ((mDiameter >= 44) ? 6.0 : 5.0);
	const double a = angleOf (v);

	// ---- the printed scale: eleven ticks, long at the ends and the middle
	for (int i = 0; i <= 10; ++i)
	{
		const double t = 135.0 + 27.0 * i;
		const bool longTick = (i == 0 || i == 5 || i == 10);
		strokeLine (context, pointAt (centre, body + 1.5, t), pointAt (centre, outer - (longTick ? 0.0 : 1.8), t),
		            Palette::kTick, longTick ? 1.3 : 0.9);
	}
	if (mDiameter >= 52)
	{
		const CPoint lo = pointFromTop (centre, outer + 2.0, -138.0);
		const CPoint hi = pointFromTop (centre, outer + 2.0, 138.0);
		drawLettering (context, mBipolar ? "-" : "0", Layout::kTextTick, Palette::kTick, lo.x - 2., lo.y + 4.,
		               Align::Right);
		drawLettering (context, mBipolar ? "+" : "10", Layout::kTextTick, Palette::kTick, hi.x + 2., hi.y + 4.,
		               Align::Left);
	}

	// ---- the knurled body ---------------------------------------------------
	{
		auto rim = owned (context->createGraphicsPath ());
		if (rim)
		{
			constexpr int kTeeth = 28;
			for (int i = 0; i < kTeeth * 2; ++i)
			{
				const CPoint p = pointAt (centre, (i % 2 == 0) ? body : body - 1.4, 360.0 * i / (kTeeth * 2));
				if (i == 0)
					rim->beginSubpath (p);
				else
					rim->addLine (p);
			}
			rim->closeSubpath ();
			context->setFillColor (Palette::kKnobBody);
			context->drawGraphicsPath (rim, CDrawContext::kPathFilled);
			context->setFrameColor (Palette::kKnobRim);
			context->setLineWidth (0.6);
			context->setLineStyle (kLineSolid);
			context->drawGraphicsPath (rim, CDrawContext::kPathStroked);
		}
		auto cap = owned (context->createGraphicsPath ());
		if (cap)
		{
			cap->addEllipse (circleRect (centre, body * 0.78));
			fillCap (context, cap, centre, body * 0.78);
			context->setFrameColor (CColor (0, 0, 0, 255));
			context->setLineWidth (0.8);
			context->setLineStyle (kLineSolid);
			context->drawGraphicsPath (cap, CDrawContext::kPathStroked);
		}
	}

	// ---- the wear: dust in the knurling, the polished cap, turning marks ------
	// Everything but the sheen is placed relative to the pointer (`a`), so
	// it turns with the knob; the sheen is where the light is, so it stays.
	{
		const double capR = body * 0.78;
		if (mWear.knurlAlpha > 0)
			strokeCircle (context, centre, (capR + body) / 2.0, wearColour (Wear::kKnurlDust, mWear.knurlAlpha),
			              body - capR);
		if (mWear.sheen > 0)
			softDisc (context, CPoint (centre.x - capR * 0.25, centre.y - capR * 0.3), capR * 0.55, Wear::kSheen,
			          mWear.sheen);
		for (const auto& s : mWear.scuffs)
		{
			auto arc = owned (context->createGraphicsPath ());
			if (!arc)
				continue;
			const double start = a + s.start;
			arc->addArc (circleRect (centre, capR * s.radius), start, start + s.span, true);
			context->setFrameColor (wearColour (Wear::kScuff, s.alpha));
			context->setLineWidth (s.width);
			context->setLineStyle (roundLine ());
			context->drawGraphicsPath (arc, CDrawContext::kPathStroked);
		}
		for (const auto& g : mWear.grit)
			fillCircle (context, pointAt (centre, body * g.radius, a + g.angle), g.r,
			            wearColour (Wear::kKnurlDust, g.alpha));
	}

	// ---- the white line -------------------------------------------------------
	strokeLine (context, pointAt (centre, body * 0.15, a), pointAt (centre, body - 0.5, a), Palette::kInk,
	            (mDiameter >= 44) ? 2.2 : 1.8);

	// ---- the reading and the label --------------------------------------------
	const CCoord textTop = r.top + mDiameter + Layout::kKnobGap;
	const CCoord cx = centre.x;
	if (mFormatter)
		drawLettering (context, mFormatter (getValueNormalized ()), Layout::kTextValue,
		               Palette::kInk, cx,
		               textTop + Layout::kKnobValueH / 2., Align::Centre);
	if (!mLabel.empty ())
		drawLettering (context, mLabel, Layout::kTextLabel, Palette::kInk, cx,
		               textTop + Layout::kKnobValueH + Layout::kKnobLabelH / 2., Align::Centre);

	setDirty (false);
}

void Knob::onMouseDownEvent (MouseDownEvent& event)
{
	if (!event.buttonState.isLeft ())
		return;

	// DOUBLE-CLICK: the default. The first click of the pair began and
	// ended an ordinary (empty) drag, so this is one edit of its own.
	if (event.clickCount >= 2)
	{
		beginEdit ();
		setValue (getDefaultValue ());
		valueChanged ();
		endEdit ();
		invalid ();
		event.consumed = true;
		return;
	}

	mDragging = true;
	mLastY = event.mousePosition.y;
	beginEdit ();
	invalid ();
	event.consumed = true;
}

void Knob::onMouseMoveEvent (MouseMoveEvent& event)
{
	if (!mDragging)
		return;

	const CCoord dy = mLastY - event.mousePosition.y;   // up is more
	if (std::fabs (dy) < 0.5)
		return;
	mLastY = event.mousePosition.y;

	const float scale = event.modifiers.has (ModifierKey::Shift) ? 0.1f : 1.f;
	setValueNormalized (std::clamp (
		getValueNormalized () + static_cast<float> (dy) / kDragPixels * scale, 0.f, 1.f));
	valueChanged ();
	invalid ();
	event.consumed = true;
}

void Knob::onMouseUpEvent (MouseUpEvent& event)
{
	if (!mDragging)
		return;
	mDragging = false;
	endEdit ();
	invalid ();
	event.consumed = true;
}

void Knob::onMouseCancelEvent (MouseCancelEvent& event)
{
	if (mDragging)
	{
		mDragging = false;
		endEdit ();
		invalid ();
	}
	event.consumed = true;
}

void Knob::onMouseWheelEvent (MouseWheelEvent& event)
{
	const float step = event.modifiers.has (ModifierKey::Shift) ? 0.001f : 0.01f;
	beginEdit ();
	setValueNormalized (std::clamp (
		getValueNormalized () + static_cast<float> (event.deltaY) * step, 0.f, 1.f));
	valueChanged ();
	endEdit ();
	invalid ();
	event.consumed = true;
}


//------------------------------------------------------------------------
// RotarySwitch
//------------------------------------------------------------------------
RotarySwitch::RotarySwitch (const CRect& size, IControlListener* listener, int32_t tag,
                            const Layout::Control& layout, int values)
: CControl (size, listener, tag)
, mGeometry (Layout::selectorOf (layout))
, mValues (std::max (2, values))
{
}

int RotarySwitch::currentValue () const
{
	const int i = static_cast<int> (std::floor (getValueNormalized () * (mValues - 1) + 0.5f));
	return std::clamp (i, 0, mValues - 1);
}

int RotarySwitch::printedIndex () const
{
	return std::min (currentValue (), mGeometry.positions - 1);
}

void RotarySwitch::select (int value)
{
	value = std::clamp (value, 0, mValues - 1);
	beginEdit ();
	setValueNormalized (static_cast<float> (value) / static_cast<float> (mValues - 1));
	valueChanged ();
	endEdit ();
	invalid ();
}

void RotarySwitch::draw (CDrawContext* context)
{
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	const auto& g = mGeometry;
	const CPoint centre (g.cx, g.cy);
	const int current = printedIndex ();

	// ---- the printed positions: a mark for each, a name where it has one --
	const double tickIn = g.radius + 2.5;
	const double tickOut = g.radius + ((g.radius >= 12.0) ? 8.0 : 6.0);
	for (int i = 0; i < g.positions; ++i)
	{
		const bool set = (i == current);
		const double t = g.angle[i];
		strokeLine (context, pointFromTop (centre, tickIn, t), pointFromTop (centre, tickOut, t),
		            set ? Palette::kRed : Palette::kTick, set ? 2.0 : 1.5);
		if (i < static_cast<int> (mNames.size ()) && !mNames[static_cast<size_t> (i)].empty ())
		{
			const std::string name = upper (mNames[static_cast<size_t> (i)]);
			const Layout::Box b = Layout::selectorLabelBox (g, i, name);
			drawLettering (context, name, Layout::kTextLabel, set ? Palette::kInk : Palette::kInkDim, b.left,
			               (b.top + b.bottom) / 2.);
		}
	}

	// ---- the chicken head: a round hub and a beak that points ------------
	const double a = g.angle[current];
	fillCircle (context, centre, g.radius * 0.8, CColor (11, 11, 11, 255));
	strokeCircle (context, centre, g.radius * 0.8, CColor (68, 68, 68, 255), 0.8);
	for (const auto& s : mGrit)   // grit round the hub, turning with the beak
		fillCircle (context, pointFromTop (centre, g.radius * 0.8 * s.radius, a + s.angle), s.r,
		            wearColour (Wear::kKnurlDust, s.alpha));

	auto beak = owned (context->createGraphicsPath ());
	if (beak)
	{
		// R2Synth's beak is 8 px either side on a 16-17 px knob: half its
		// radius, which is what keeps a small link switch in proportion.
		const double half = g.radius * 0.5;
		const CPoint tip = pointFromTop (centre, g.radius + 3.0, a);
		const CPoint left = pointFromTop (centre, half, a - 90.0);
		const CPoint right = pointFromTop (centre, half, a + 90.0);
		const CPoint tail = pointFromTop (centre, g.radius, a + 180.0);
		beak->beginSubpath (tip);
		beak->addLine (right);
		const CPoint c1 (right.x + 2.0 / 3.0 * (tail.x - right.x), right.y + 2.0 / 3.0 * (tail.y - right.y));
		const CPoint c2 (left.x + 2.0 / 3.0 * (tail.x - left.x), left.y + 2.0 / 3.0 * (tail.y - left.y));
		beak->addBezierCurve (c1, c2, left);
		beak->closeSubpath ();
		fillCap (context, beak, centre, g.radius);
		context->setFrameColor (CColor (0, 0, 0, 255));
		context->setLineWidth (0.8);
		context->setLineStyle (roundLine ());
		context->drawGraphicsPath (beak, CDrawContext::kPathStroked);
		strokeLine (context, pointFromTop (centre, 2.0, a), tip, Palette::kInk, (g.radius >= 12.0) ? 1.8 : 1.4);
	}

	// ---- its name -------------------------------------------------------------
	if (!mLabel.empty ())
	{
		switch (g.place)
		{
			case Layout::NamePlace::Below:
				drawLettering (context, mLabel, Layout::kTextLabel, Palette::kInk, g.nameX, g.nameCentreY,
				               Align::Centre);
				break;
			case Layout::NamePlace::Right:
				drawLettering (context, mLabel, Layout::kTextLabel, Palette::kInk, g.nameX, g.nameCentreY);
				break;
			case Layout::NamePlace::RightRed:
				drawLettering (context, mLabel, Layout::kTextLabel, Palette::kRed, g.nameX, g.nameCentreY);
				break;
		}
	}

	setDirty (false);
}

void RotarySwitch::onMouseDownEvent (MouseDownEvent& event)
{
	if (!event.buttonState.isLeft ())
		return;
	const int picked = Layout::selectorPositionAt (mGeometry, event.mousePosition.x, event.mousePosition.y);
	if (picked >= 0)
		select (picked);
	else
		select ((printedIndex () + 1 < mGeometry.positions) ? printedIndex () + 1 : 0);
	event.consumed = true;
}

void RotarySwitch::onMouseUpEvent (MouseUpEvent& event)
{
	event.consumed = true;
}

void RotarySwitch::onMouseWheelEvent (MouseWheelEvent& event)
{
	if (event.deltaY > 0.)
		select (std::min (printedIndex () + 1, mGeometry.positions - 1));
	else if (event.deltaY < 0.)
		select (std::max (printedIndex () - 1, 0));
	event.consumed = true;
}

//------------------------------------------------------------------------
// LampSwitch
//------------------------------------------------------------------------
LampSwitch::LampSwitch (const CRect& size, IControlListener* listener, int32_t tag, const std::string& name)
: CControl (size, listener, tag)
, mName (upper (name))
{
}

void LampSwitch::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	const bool on = getValueNormalized () >= 0.5f;
	const double centreY = (r.top + r.bottom) / 2.;
	const CPoint dot (r.left + 10., centreY);

	// R2Synth's SELF-OSC lamp: a lamp in a black bezel, dark red when off,
	// lit red with a glow.
	fillCircle (context, dot, 5.5, CColor (0, 0, 0, 255));
	strokeCircle (context, dot, 5.5, Palette::kSectionEdge, 1.);
	if (on)
	{
		CColor glow (Palette::kLampOn);
		glow.alpha = 70;
		fillCircle (context, dot, 8., glow);
		fillCircle (context, dot, 4., Palette::kLampOn);
	}
	else
		fillCircle (context, dot, 4., Palette::kLampOff);
	fillCircle (context, CPoint (dot.x - 1.2, dot.y - 1.2), 1.3, CColor (255, 255, 255, 64));

	drawLettering (context, mName, Layout::kTextLabel, on ? Palette::kInk : Palette::kInkSoft, dot.x + 12.,
	               centreY);

	setDirty (false);
}

void LampSwitch::onMouseDownEvent (MouseDownEvent& event)
{
	if (!event.buttonState.isLeft ())
		return;
	beginEdit ();
	setValueNormalized (getValueNormalized () >= 0.5f ? 0.f : 1.f);
	valueChanged ();
	endEdit ();
	invalid ();
	event.consumed = true;
}

void LampSwitch::onMouseUpEvent (MouseUpEvent& event)
{
	event.consumed = true;
}

//------------------------------------------------------------------------
// The OLEDs: R2Synth's Scope glass, shared by the two screens
//------------------------------------------------------------------------
namespace {

constexpr CCoord kScopeGrid   = 24.;
constexpr CCoord kScopeDot    = 12.;
constexpr CCoord kScopeRadius = 2.;

/** True black and a dot grid. Call inside a clip to `r`. */
void drawGlassBase (CDrawContext* context, const CRect& r)
{
	roundRect (context, r, kScopeRadius, &Palette::kScreen);
	context->setFillColor (Palette::kGridDot);
	for (CCoord x = r.left + kScopeGrid; x < r.right - 1; x += kScopeGrid)
		for (CCoord y = r.top + kScopeDot; y < r.bottom - 1; y += kScopeDot)
			context->drawEllipse (circleRect (CPoint (x, y), 0.6), kDrawFilled);
	for (CCoord y = r.top + kScopeGrid; y < r.bottom - 1; y += kScopeGrid)
		for (CCoord x = r.left + kScopeDot; x < r.right - 1; x += kScopeDot)
			context->drawEllipse (circleRect (CPoint (x, y), 0.6), kDrawFilled);
}

/** The glass over the picture: a faint sheen at the top left, a hairline
    highlight, then the wear - fingerprints, dust, the odd fibre. */
void drawGlassTop (CDrawContext* context, const CRect& r, const Wear::GlassWear& wear)
{
	auto sheen = owned (context->createGraphicsPath ());
	auto gradient = owned (CGradient::create (0.0, 0.46, CColor (255, 255, 255, 18), CColor (255, 255, 255, 0)));
	if (sheen && gradient)
	{
		gradient->addColorStop (0.45, CColor (255, 255, 255, 4));
		sheen->addRect (r);
		context->fillLinearGradient (sheen, *gradient, CPoint (r.left, r.top),
		                             CPoint (r.left + 0.35 * r.getWidth (), r.bottom));
	}
	strokeLine (context, CPoint (r.left + 3., r.top + 0.8), CPoint (r.right - 3., r.top + 0.8),
	            CColor (255, 255, 255, 30), 0.6);

	for (const auto& p : wear.prints)
		for (const auto& ridge : p.ridges)
			polyline (context, ridge.pts, 0, ridge.pts.size (), wearColour (Wear::kPrint, ridge.alpha), ridge.width);
	for (const auto& d : wear.dust)
		fillCircle (context, CPoint (d.x, d.y), d.r, wearColour (Wear::kGlassDust, d.alpha));
	for (const auto& l : wear.fibres)
		wearLine (context, l, Wear::kGlassDust);
}

CColor withAlpha (const CColor& c, uint8_t alpha)
{
	return CColor (c.red, c.green, c.blue, alpha);
}

/** Lettering with a slight bloom: the same strokes wide and faint under
    the crisp ones, as an OLED's lit pixels bleed. */
void drawGlowLettering (CDrawContext* context, const std::string& text, const Lettering::Style& style,
                        const CColor& colour, CCoord x, CCoord centreY, Align align)
{
	drawLettering (context, text, style, withAlpha (colour, 60), x, centreY, align, style.weight + 1.6);
	drawLettering (context, text, style, colour, x, centreY, align);
}

} // anonymous namespace

//------------------------------------------------------------------------
// TapScope
//------------------------------------------------------------------------
namespace {

/** The tap feedback handles' colour: amber, so they never read as a level. */
const CColor kFeedbackAmber (255, 178, 58, 255);

/** The plot inside the scope - shared by the drawing and handleRect(), so
    a handle cannot drift off its tap. */
struct ScopeGeometry
{
	double pl, pr, top, bottom, mid, half;
};

ScopeGeometry scopeGeometry (const CRect& r)
{
	ScopeGeometry g {};
	g.pl = r.left + 34.;
	g.pr = r.right - 18.;
	g.top = r.top + 24.;
	g.bottom = r.bottom - 22.;
	g.mid = (g.top + g.bottom) / 2.;
	g.half = (g.bottom - g.top) / 2.;
	return g;
}

} // anonymous namespace

TapScope::TapScope (const CRect& size)
: CView (size)
{
	setMouseEnabled (false);
}

void TapScope::axisFor (double tapMsLeft, double tapMsRight, double& spanMs, double& stepMs)
{
	const double last = DelayLineTaps * std::max ({ tapMsLeft, tapMsRight, 0.0 });
	const double target = (last > 0.0) ? last * 1.2 : 10.0;
	const double raw = target / 8.0;
	const double decade = std::pow (10.0, std::floor (std::log10 (raw)));
	stepMs = decade * 10.0;
	for (double m : { 1.0, 2.0, 5.0, 10.0 })
		if (decade * m >= raw)
		{
			stepMs = decade * m;
			break;
		}
	spanMs = std::ceil (target / stepMs) * stepMs;
}

CRect TapScope::handleRect (int side, int n) const
{
	const ScopeGeometry geo = scopeGeometry (getViewSize ());
	double span = 0.0, step = 0.0;
	axisFor (mPicture.tapMs[0], mPicture.tapMs[1], span, step);
	const double x = geo.pl + (geo.pr - geo.pl) * n * mPicture.tapMs[side] / span;
	const double h = geo.half * 0.92;
	return side ? CRect (x - 9., geo.mid + 1., x + 9., geo.mid + h + 3.)
	            : CRect (x - 9., geo.mid - h - 3., x + 9., geo.mid - 1.);
}

void TapScope::draw (CDrawContext* context)
{
	using namespace Layout;
	const CRect r = getViewSize ();
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	ConcatClip clip (*context, r);
	drawGlassBase (context, r);

	const TapPicture& pic = mPicture;
	const uint8_t lit = pic.enabled ? 255 : 80;   // switched off: the picture dims

	// ---- the lettering round the edge --------------------------------------
	drawLettering (context, "TAP SCOPE  /  LEVELS IN DBFS", kTextScreen, Palette::kScreenText, r.left + 8., r.top + 11.);
	if (pic.tapsFeedback)
	{
		const double x = r.left + 8. + Lettering::width ("TAP SCOPE  /  LEVELS IN DBFS  /  ", kTextScreen);
		drawLettering (context, "TAP FEEDBACK", kTextScreen, kFeedbackAmber, x, r.top + 11.);
	}
	if (!pic.handleText.empty ())
		drawLettering (context, pic.handleText, kTextScreen, kFeedbackAmber, r.right - 8., r.top + 11., Align::Right);
	else if (!pic.annotation.empty ())
		drawLettering (context, pic.annotation, kTextScreen, pic.enabled ? Palette::kTraceLeft : Palette::kRed,
		               r.right - 8., r.top + 11., Align::Right);

	// ---- the plot ----------------------------------------------------------
	const ScopeGeometry geo = scopeGeometry (r);
	const double pl = geo.pl, pr = geo.pr, top = geo.top, bottom = geo.bottom, mid = geo.mid, half = geo.half;

	double span = 0.0, step = 0.0;
	axisFor (pic.tapMs[0], pic.tapMs[1], span, step);
	auto xOf = [&] (double ms) { return pl + (pr - pl) * ms / span; };

	strokeLine (context, CPoint (pl, mid), CPoint (pr, mid), Palette::kAxis, 1.);
	for (int i = 0; i * step * 0.5 <= span + 1e-9; ++i)
	{
		const double ms = i * step * 0.5;
		const double x = xOf (ms);
		strokeLine (context, CPoint (x, mid - 3.), CPoint (x, mid + 3.), CColor (85, 91, 99, 255), 1.);
		if (i % 2 == 0)
		{
			char buf[24];
			std::snprintf (buf, sizeof (buf), "%.0f", ms);
			drawLettering (context, buf, kTextScreenSm, CColor (111, 116, 124, 255), x, r.bottom - 10., Align::Centre);
		}
	}
	drawLettering (context, "MS", kTextScreenSm, CColor (111, 116, 124, 255), r.left + 12., r.bottom - 10.);
	drawLettering (context, "L", kTextScreen, withAlpha (Palette::kTraceLeft, lit), r.left + 12., mid - half * 0.5);
	drawLettering (context, "R", kTextScreen, withAlpha (Palette::kTraceRight, lit), r.left + 12., mid + half * 0.5);

	// ---- the dry signal at 0 ---------------------------------------------------
	// as tall as Dry Level lets it be: at 0 nothing of the input is mixed in
	const double dryH = half * 0.9 * std::clamp (pic.dry, 0.0, 1.0);
	strokeLine (context, CPoint (pl, mid - half * 0.9), CPoint (pl, mid + half * 0.9), withAlpha (Palette::kInk, 40), 1.);
	if (dryH > 0.5)
		strokeLine (context, CPoint (pl, mid - dryH), CPoint (pl, mid + dryH), withAlpha (Palette::kInk, lit), 2.);
	drawLettering (context, "DRY", kTextScreenSm, Palette::kScreenText, pl + 6., top + 2.);

	// ---- the taps ---------------------------------------------------------------
	// Both channels' bars are CENTRED ON THE TAP'S TIME, so a left and a right
	// tap at the same time sit on one vertical line. The FRAME is the share
	// heard on that side; the FILL inside it is how much signal is going
	// through the tap now (the processor's meter). The share that crosses to
	// the OTHER side is drawn second, over the other channel's bar.
	constexpr double kHalfBar = 5.0;
	const bool sameTimes = std::fabs (pic.tapMs[0] - pic.tapMs[1]) < 0.5;
	struct Tap
	{
		double x, h, leak, level;
		bool heard;
	};
	auto tapOf = [&] (int side, int n) {
		const double own = 1.0 - (DelayLineTaps - n) / 10.0;   // DelayLine's matrix
		return Tap { xOf (n * pic.tapMs[side]), own * half * 0.92, (1.0 - own) * half * 0.92,
		             std::clamp (pic.level[side][n - 1], 0.0, 1.0), pic.tapsOut || n == DelayLineTaps };
	};
	auto dbText = [] (double level) -> std::string {
		if (level <= 0.0)
			return {};
		char buf[16];
		std::snprintf (buf, sizeof (buf), "%.0f", tapMeterDb (level));
		return buf;
	};

	// pass 1: each channel's own frame and fill - left up, right down
	for (int side = 0; side < 2; ++side)
	{
		const CColor trace = side ? Palette::kTraceRight : Palette::kTraceLeft;
		const double dir = side ? 1.0 : -1.0;
		for (int n = 1; n <= DelayLineTaps; ++n)
		{
			const Tap t = tapOf (side, n);
			// not in the output: the signal goes through, but dimmed
			const uint8_t tapLit = static_cast<uint8_t> (lit * (t.heard ? 1.0 : 0.45));

			// the frame: the share heard on this side
			const CRect frameRect = side ? CRect (t.x - kHalfBar, mid, t.x + kHalfBar, mid + t.h)
			                             : CRect (t.x - kHalfBar, mid - t.h, t.x + kHalfBar, mid);
			{
				context->setFrameColor (withAlpha (trace, static_cast<uint8_t> (tapLit * 0.5)));
				context->setLineWidth (1.);
				context->setLineStyle (kLineSolid);
				CRect o = frameRect;
				o.inset (0.5, 0.5);
				context->drawRect (o, kDrawStroked);
			}

			// the fill: the signal going through the tap now
			const double f = t.h * t.level;
			if (f > 0.5)
			{
				const CRect fill = side ? CRect (t.x - kHalfBar, mid, t.x + kHalfBar, mid + f)
				                        : CRect (t.x - kHalfBar, mid - f, t.x + kHalfBar, mid);
				auto path = owned (context->createGraphicsPath ());
				auto gradient = owned (CGradient::create (0.0, 1.0, withAlpha (trace, static_cast<uint8_t> (tapLit * 0.95)),
				                                          withAlpha (trace, static_cast<uint8_t> (tapLit * 0.3))));
				if (path && gradient)
				{
					path->addRect (fill);
					context->fillLinearGradient (path, *gradient, CPoint (t.x, mid + dir * f), CPoint (t.x, mid));
				}
				// the bloom along the fill's edge
				strokeLine (context, CPoint (fill.left, mid + dir * f), CPoint (fill.right, mid + dir * f),
				            withAlpha (trace, static_cast<uint8_t> (tapLit * 0.7)), 1.6);
			}

			// the filter replaces tap 5: an F beside the frame's far end
			if (n == DelayLineTaps && pic.filter[side])
				drawLettering (context, "F", kTextScreenSm, withAlpha (trace, lit), t.x + kHalfBar + 5.,
				               mid + dir * (t.h - 5.), Align::Centre);

			// the tap's number and its level in dBFS: the left's along the
			// top; the right's along the bottom, numbered there too when
			// the two sides' times differ
			std::string label;
			if (side == 0 || !sameTimes)
				label = "T" + std::to_string (n);
			const std::string db = dbText (t.level);
			if (!db.empty ())
				label += label.empty () ? db : " " + db;
			if (!label.empty ())
				drawLettering (context, label, kTextScreenSm, side == 0 || !sameTimes ? Palette::kScreenText
				                                                                       : withAlpha (trace, 200),
				               t.x, side ? bottom + 3. : top + 2., Align::Centre);
		}
	}

	// pass 2: the share that crosses over, on top of the other side's bar -
	// a small frame for the share, a bright mark for the signal crossing now
	for (int side = 0; side < 2; ++side)
	{
		const CColor trace = side ? Palette::kTraceRight : Palette::kTraceLeft;
		const double dir = side ? -1.0 : 1.0;   // the OTHER side: left's below, right's above
		for (int n = 1; n <= DelayLineTaps; ++n)
		{
			const Tap t = tapOf (side, n);
			if (!t.heard || t.leak < 0.5)
				continue;
			CRect cross = (dir > 0) ? CRect (t.x - kHalfBar, mid, t.x + kHalfBar, mid + t.leak)
			                        : CRect (t.x - kHalfBar, mid - t.leak, t.x + kHalfBar, mid);
			cross.inset (1.5, 0.0);
			context->setFrameColor (withAlpha (trace, static_cast<uint8_t> (lit * 0.55)));
			context->setLineWidth (1.);
			context->setLineStyle (kLineSolid);
			context->drawRect (cross, kDrawStroked);
			const double m = t.leak * t.level;
			if (m > 0.5)
				strokeLine (context, CPoint (cross.left - 1., mid + dir * m), CPoint (cross.right + 1., mid + dir * m),
				            withAlpha (trace, lit), 2.);
		}
	}

	drawGlassTop (context, r, mWear);
	setDirty (false);
}

//------------------------------------------------------------------------
// TapHandle
//------------------------------------------------------------------------
TapHandle::TapHandle (const CRect& size, IControlListener* listener, int32_t tag, int side)
: CControl (size, listener, tag)
, mSide (side)
{
}

void TapHandle::tellActive (bool wasActive)
{
	if (wasActive != isActive () && mActive)
		mActive (isActive ());
	invalid ();
}

void TapHandle::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	// 0 % at the axis end of the column, 200 % at the far end
	const double inner = 2.0, usable = r.getHeight () - 4.0;
	auto yOf = [&] (double v) { return mSide ? r.top + inner + v * usable : r.bottom - inner - v * usable; };
	const double cx = (r.left + r.right) / 2.;

	// the dim tick at 100 %: the DXi's amount
	const double y100 = yOf (0.5);
	strokeLine (context, CPoint (cx - 8., y100), CPoint (cx - 6., y100), withAlpha (kFeedbackAmber, 110), 1.);
	strokeLine (context, CPoint (cx + 6., y100), CPoint (cx + 8., y100), withAlpha (kFeedbackAmber, 110), 1.);

	// the handle: an amber bar across the tap, brighter while touched
	const double y = yOf (std::clamp (static_cast<double> (getValueNormalized ()), 0.0, 1.0));
	const bool active = isActive ();
	if (active)
		strokeLine (context, CPoint (cx - 8., y), CPoint (cx + 8., y), withAlpha (kFeedbackAmber, 70), 6.);
	strokeLine (context, CPoint (cx - 8., y), CPoint (cx + 8., y), CColor (0, 0, 0, 200), 4.);
	strokeLine (context, CPoint (cx - 7., y), CPoint (cx + 7., y), withAlpha (kFeedbackAmber, active ? 255 : 220),
	            2.2);

	setDirty (false);
}

void TapHandle::onMouseEnterEvent (MouseEnterEvent& event)
{
	const bool was = isActive ();
	mHover = true;
	tellActive (was);
	event.consumed = true;
}

void TapHandle::onMouseExitEvent (MouseExitEvent& event)
{
	const bool was = isActive ();
	mHover = false;
	tellActive (was);
	event.consumed = true;
}

void TapHandle::onMouseDownEvent (MouseDownEvent& event)
{
	if (!event.buttonState.isLeft ())
		return;
	if (event.clickCount >= 2)
	{
		beginEdit ();
		setValue (getDefaultValue ());
		valueChanged ();
		endEdit ();
		invalid ();
		event.consumed = true;
		return;
	}
	const bool was = isActive ();
	mDragging = true;
	mLastY = event.mousePosition.y;
	beginEdit ();
	tellActive (was);
	event.consumed = true;
}

void TapHandle::onMouseMoveEvent (MouseMoveEvent& event)
{
	if (!mDragging)
		return;
	const CCoord dy = event.mousePosition.y - mLastY;
	if (std::fabs (dy) < 0.5)
		return;
	mLastY = event.mousePosition.y;
	// away from the axis increases: up for the left, down for the right
	const double usable = std::max (8.0, getViewSize ().getHeight () - 4.0);
	const float scale = event.modifiers.has (ModifierKey::Shift) ? 0.1f : 1.f;
	const float delta = static_cast<float> ((mSide ? dy : -dy) / usable) * scale;
	setValueNormalized (std::clamp (getValueNormalized () + delta, 0.f, 1.f));
	valueChanged ();
	invalid ();
	event.consumed = true;
}

void TapHandle::onMouseUpEvent (MouseUpEvent& event)
{
	if (!mDragging)
		return;
	const bool was = isActive ();
	mDragging = false;
	endEdit ();
	tellActive (was);
	event.consumed = true;
}

void TapHandle::onMouseCancelEvent (MouseCancelEvent& event)
{
	if (mDragging)
	{
		const bool was = isActive ();
		mDragging = false;
		endEdit ();
		tellActive (was);
	}
	event.consumed = true;
}

void TapHandle::onMouseWheelEvent (MouseWheelEvent& event)
{
	const float step = event.modifiers.has (ModifierKey::Shift) ? 0.0025f : 0.005f;   // 0.5 % / 1 % of 200 %
	beginEdit ();
	setValueNormalized (std::clamp (getValueNormalized () + static_cast<float> (event.deltaY) * step, 0.f, 1.f));
	valueChanged ();
	endEdit ();
	invalid ();
	event.consumed = true;
}

//------------------------------------------------------------------------
// TempoScreen
//------------------------------------------------------------------------
TempoScreen::TempoScreen (const CRect& size)
: CView (size)
{
	setMouseEnabled (false);
}

void TempoScreen::setText (const std::string& caption, const std::string& note, const std::string& reading)
{
	if (caption != mCaption || note != mNote || reading != mReading)
	{
		mCaption = caption;
		mNote = note;
		mReading = reading;
		invalid ();
	}
}

void TempoScreen::draw (CDrawContext* context)
{
	using namespace Layout;
	const CRect r = getViewSize ();
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	ConcatClip clip (*context, r);
	drawGlassBase (context, r);
	drawLettering (context, upper (mCaption), kTextScreen, Palette::kScreenText, r.left + 8., r.top + 11.);
	drawLettering (context, upper (mNote), kTextScreen, Palette::kTraceLeft, r.right - 8., r.top + 11., Align::Right);
	drawGlowLettering (context, upper (mReading), kTextBig, Palette::kTraceLeft, (r.left + r.right) / 2.,
	                   r.top + 34., Align::Centre);
	drawGlassTop (context, r, mWear);
	setDirty (false);
}

//------------------------------------------------------------------------
// StatusPill
//------------------------------------------------------------------------
StatusPill::StatusPill (const CRect& size)
: CView (size)
{
	setMouseEnabled (false);
}

void StatusPill::setStatus (bool running, const std::string& text)
{
	if (running != mRunning || text != mText)
	{
		mRunning = running;
		mText = text;
		invalid ();
	}
}

void StatusPill::draw (CDrawContext* context)
{
	CRect r = getViewSize ();
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);
	r.inset (0.6, 0.6);
	roundRect (context, r, 3., &Palette::kHeader, &Palette::kRed, 1.2);
	const double cy = (r.top + r.bottom) / 2.;
	const CPoint dot (r.left + 16., cy);
	if (mRunning)
	{
		fillCircle (context, dot, 7., withAlpha (Palette::kLampOn, 60));
		fillCircle (context, dot, 4.5, Palette::kRed);
	}
	else
		fillCircle (context, dot, 4.5, Palette::kLampOff);
	drawLettering (context, upper (mText), Layout::kTextPill, Palette::kInk, r.left + 30., cy + 0.5);
	setDirty (false);
}

//------------------------------------------------------------------------
// TextLine
//------------------------------------------------------------------------
TextLine::TextLine (const CRect& size, const Lettering::Style& style, const CColor& colour)
: CView (size)
, mStyle (style)
, mColour (colour)
{
	setMouseEnabled (false);
}

void TextLine::setText (const std::string& text)
{
	if (text != mText)
	{
		mText = text;
		invalid ();
	}
}

void TextLine::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);
	drawLettering (context, mText, mStyle, mColour, r.left, (r.top + r.bottom) / 2.);
	setDirty (false);
}

//------------------------------------------------------------------------
} // namespace Panel
} // namespace SpaceDub
