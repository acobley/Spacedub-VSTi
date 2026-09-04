//------------------------------------------------------------------------
// SpaceDub - custom VSTGUI controls implementation
//------------------------------------------------------------------------

#include "SpaceDubControls.h"

#include <algorithm>
#include <cmath>

using namespace VSTGUI;

namespace SpaceDub {

//------------------------------------------------------------------------
// SdSlider
//------------------------------------------------------------------------
SdSlider::SdSlider (const CRect& size, IControlListener* listener, int32_t tag, CBitmap* handle,
                    CBitmap* groove, bool vertical)
: CControl (size, listener, tag, nullptr)
, mHandle (handle)
, mGroove (groove)
, mVertical (vertical)
{
	setWantsFocus (true);
	setMin (0.f);
	setMax (1.f);
	setValue (0.f);
}

//------------------------------------------------------------------------
CCoord SdSlider::travel () const
{
	const CRect r = getViewSize ();
	if (mVertical)
		return r.getHeight () - (mHandle ? mHandle->getHeight () : 10.);
	return r.getWidth () - (mHandle ? mHandle->getWidth () : 10.);
}

//------------------------------------------------------------------------
void SdSlider::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();

	if (mGroove)
		mGroove->draw (context, r);

	if (mHandle)
	{
		const float norm = getValueNormalized ();
		CRect h (r);
		if (mVertical)
		{
			// Top of the slider is the maximum, as in the DXi property page.
			const CCoord y = r.top + travel () * static_cast<CCoord> (1.f - norm);
			h.setHeight (mHandle->getHeight ());
			h.setWidth (mHandle->getWidth ());
			h.offset ((r.getWidth () - mHandle->getWidth ()) / 2., 0.);
			h.top    = y;
			h.bottom = y + mHandle->getHeight ();
		}
		else
		{
			const CCoord x = r.left + travel () * static_cast<CCoord> (norm);
			h.setWidth (mHandle->getWidth ());
			h.setHeight (mHandle->getHeight ());
			h.offset (0., (r.getHeight () - mHandle->getHeight ()) / 2.);
			h.left  = x;
			h.right = x + mHandle->getWidth ();
		}
		mHandle->draw (context, h);
	}

	setDirty (false);
}

//------------------------------------------------------------------------
float SdSlider::valueFromPoint (const CPoint& where) const
{
	const CRect  r = getViewSize ();
	const CCoord t = travel ();
	if (t <= 0.)
		return getMin ();

	float norm;
	if (mVertical)
	{
		const CCoord half = (mHandle ? mHandle->getHeight () : 10.) / 2.;
		norm = static_cast<float> (1. - ((where.y - r.top - half) / t));
	}
	else
	{
		const CCoord half = (mHandle ? mHandle->getWidth () : 10.) / 2.;
		norm = static_cast<float> ((where.x - r.left - half) / t);
	}
	norm = std::clamp (norm, 0.f, 1.f);
	return getMin () + norm * (getMax () - getMin ());
}

//------------------------------------------------------------------------
void SdSlider::onMouseDownEvent (MouseDownEvent& event)
{
	if (!event.buttonState.isLeft ())
		return;

	beginEdit ();
	mDragging   = true;
	mStartPoint = event.mousePosition;
	mStartValue = getValue ();

	CPoint local = event.mousePosition;
	local.offsetInverse (getViewSize ().getTopLeft ());
	local.offset (getViewSize ().left, getViewSize ().top);

	setValue (valueFromPoint (event.mousePosition));
	valueChanged ();
	invalid ();

	event.consumed = true;
}

//------------------------------------------------------------------------
void SdSlider::onMouseMoveEvent (MouseMoveEvent& event)
{
	if (!mDragging)
		return;

	float v = valueFromPoint (event.mousePosition);

	// Shift = fine adjust, relative to where the drag started.
	if (event.modifiers.has (ModifierKey::Shift))
	{
		const CCoord t = travel ();
		if (t > 0.)
		{
			const CCoord delta = mVertical ? (mStartPoint.y - event.mousePosition.y)
			                               : (event.mousePosition.x - mStartPoint.x);
			v = mStartValue + static_cast<float> (delta / t) * (getMax () - getMin ()) * 0.15f;
		}
	}

	v = std::clamp (v, getMin (), getMax ());
	if (v != getValue ())
	{
		setValue (v);
		valueChanged ();
		invalid ();
	}
	event.consumed = true;
}

//------------------------------------------------------------------------
void SdSlider::onMouseUpEvent (MouseUpEvent& event)
{
	if (mDragging)
	{
		mDragging = false;
		endEdit ();
		event.consumed = true;
	}
}

//------------------------------------------------------------------------
void SdSlider::onMouseCancelEvent (MouseCancelEvent& event)
{
	if (mDragging)
	{
		mDragging = false;
		setValue (mStartValue);
		valueChanged ();
		endEdit ();
		invalid ();
	}
	event.consumed = true;
}

//------------------------------------------------------------------------
void SdSlider::onMouseWheelEvent (MouseWheelEvent& event)
{
	const float range = getMax () - getMin ();
	const float step  = range * (event.modifiers.has (ModifierKey::Shift) ? 0.002f : 0.02f);
	const float v = std::clamp (getValue () + static_cast<float> (event.deltaY) * step, getMin (), getMax ());
	if (v != getValue ())
	{
		beginEdit ();
		setValue (v);
		valueChanged ();
		endEdit ();
		invalid ();
	}
	event.consumed = true;
}

//------------------------------------------------------------------------
// SdToggle
//------------------------------------------------------------------------
SdToggle::SdToggle (const CRect& size, IControlListener* listener, int32_t tag, CBitmap* frames)
: CControl (size, listener, tag, nullptr)
, mFrames (frames)
{
	setWantsFocus (true);
	setMin (0.f);
	setMax (1.f);
	setValue (0.f);
}

//------------------------------------------------------------------------
void SdToggle::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();
	if (mFrames)
	{
		const CCoord frameHeight = mFrames->getHeight () / 2.;
		const CPoint offset (0., (getValue () >= 0.5f) ? frameHeight : 0.);
		mFrames->draw (context, r, offset);
	}
	setDirty (false);
}

//------------------------------------------------------------------------
void SdToggle::onMouseDownEvent (MouseDownEvent& event)
{
	if (!event.buttonState.isLeft ())
		return;

	beginEdit ();
	setValue ((getValue () >= 0.5f) ? getMin () : getMax ());
	valueChanged ();
	endEdit ();
	invalid ();
	event.consumed = true;
}

//------------------------------------------------------------------------
// SdTextStepper
//------------------------------------------------------------------------
SdTextStepper::SdTextStepper (const CRect& size, IControlListener* listener, int32_t tag,
                              int32_t numSteps, TextProvider provider)
: CControl (size, listener, tag, nullptr)
, mNumSteps (numSteps < 1 ? 1 : numSteps)
, mProvider (std::move (provider))
{
	setWantsFocus (true);
	setMin (0.f);
	setMax (1.f);
	setValue (0.f);
	mFont = kNormalFontSmall;
}

//------------------------------------------------------------------------
int32_t SdTextStepper::currentStep () const
{
	return static_cast<int32_t> (std::lround (getValueNormalized () * static_cast<float> (mNumSteps)));
}

//------------------------------------------------------------------------
void SdTextStepper::setStep (int32_t step)
{
	step = std::clamp (step, 0, mNumSteps);
	const float v = getMin () + (getMax () - getMin ()) *
	                (static_cast<float> (step) / static_cast<float> (mNumSteps));
	if (v != getValue ())
	{
		setValue (v);
		valueChanged ();
		invalid ();
	}
}

//------------------------------------------------------------------------
std::string SdTextStepper::currentText () const
{
	return mProvider ? mProvider (currentStep ()) : std::string ();
}

//------------------------------------------------------------------------
void SdTextStepper::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();
	if (mProvider)
	{
		const std::string text = currentText ();
		context->setFont (mFont ? mFont.get () : kNormalFontSmall);
		context->setFontColor (mTextColor);
		context->drawString (text.c_str (), r, kCenterText, true);
	}
	setDirty (false);
}

//------------------------------------------------------------------------
void SdTextStepper::onMouseDownEvent (MouseDownEvent& event)
{
	if (!event.buttonState.isLeft ())
		return;

	beginEdit ();
	mDragging   = true;
	mMoved      = false;
	mStartPoint = event.mousePosition;
	mStartStep  = currentStep ();
	event.consumed = true;
}

//------------------------------------------------------------------------
void SdTextStepper::onMouseMoveEvent (MouseMoveEvent& event)
{
	if (!mDragging)
		return;

	const CCoord dy = mStartPoint.y - event.mousePosition.y;
	if (std::abs (dy) >= 3.)
		mMoved = true;

	const double perStep = (mNumSteps > 32) ? 2.0 : 6.0;
	setStep (mStartStep + static_cast<int32_t> (dy / perStep));
	event.consumed = true;
}

//------------------------------------------------------------------------
void SdTextStepper::onMouseUpEvent (MouseUpEvent& event)
{
	if (!mDragging)
		return;

	mDragging = false;
	if (!mMoved)
	{
		// A plain click advances one step and wraps, like the DXi spin button.
		int32_t next = currentStep () + 1;
		if (next > mNumSteps)
			next = 0;
		setStep (next);
	}
	endEdit ();
	event.consumed = true;
}

//------------------------------------------------------------------------
void SdTextStepper::onMouseWheelEvent (MouseWheelEvent& event)
{
	beginEdit ();
	setStep (currentStep () + ((event.deltaY > 0.) ? 1 : -1));
	endEdit ();
	event.consumed = true;
}

//------------------------------------------------------------------------
// SdValueDisplay
//------------------------------------------------------------------------
namespace {
/** Space between the text and the plate's border, each side. */
constexpr CCoord kPlatePadX = 3.;
} // anonymous namespace

//------------------------------------------------------------------------
SdValueDisplay::SdValueDisplay (const CRect& size)
: CView (size)
{
	setMouseEnabled (false);   // clicks fall through to the slider underneath
	setTransparency (true);    // the panel artwork shows through
}

//------------------------------------------------------------------------
void SdValueDisplay::setText (const std::string& text)
{
	if (text == mText)
		return;
	mText = text;
	invalid ();
}

//------------------------------------------------------------------------
void SdValueDisplay::draw (CDrawContext* context)
{
	if (!mText.empty ())
	{
		const CRect r = getViewSize ();

		// The plate is measured from the string, not taken from the view, so
		// the view can be sized for the longest reading a slider can produce
		// while "20%" still gets a plate only as wide as it needs.
		context->setFont (kNormalFontVerySmall);
		CCoord textWidth = context->getStringWidth (mText.c_str ());

		// A tempo-locked delay at the end of its travel is a great deal longer
		// than a percentage - "8000 ms 213u" against "20%" - and the columns
		// are only 58 px apart, so there is no reserving space for it. Drop a
		// size rather than clip: an unreadable readout is worse than a small
		// one, and this only ever engages at the extremes of the sync grid.
		if (textWidth + 2 * kPlatePadX > r.getWidth ())
		{
			if (!mSmallFont)
			{
				mSmallFont = makeOwned<CFontDesc> (*kNormalFontVerySmall);
				mSmallFont->setSize (kNormalFontVerySmall->getSize () - 1.);
			}
			context->setFont (mSmallFont);
			textWidth = context->getStringWidth (mText.c_str ());
		}

		// Still centred on the view, which is where the text is drawn, and
		// never wider than it - two neighbouring plates must not touch.
		CCoord plateWidth = textWidth + 2 * kPlatePadX;
		if (plateWidth > r.getWidth ())
			plateWidth = r.getWidth ();

		CRect plate (r);
		plate.setWidth (plateWidth);
		plate.offset ((r.getWidth () - plateWidth) / 2., 0);

		// Half-pixel inset: a one-pixel frame drawn on whole coordinates
		// straddles the pixel boundary and comes out as two grey lines.
		plate.inset (0.5, 0.5);

		context->setDrawMode (kAliasing);
		context->setLineWidth (1.);
		context->setFillColor (mPlate);
		context->setFrameColor (mBorder);
		context->drawRect (plate, kDrawFilledAndStroked);

		// White on black needs no shadow; the plate is the contrast now.
		context->setFontColor (mColor);
		context->drawString (mText.c_str (), r, kCenterText, true);
	}
	setDirty (false);
}

//------------------------------------------------------------------------
} // namespace SpaceDub
