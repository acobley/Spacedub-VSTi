//------------------------------------------------------------------------
// SpaceDub - custom VSTGUI controls
//
// The Windows dialog used three kinds of control:
//   * msctls_trackbar32 sliders, vertical and horizontal, drawn over a
//     transparent background;
//   * push-like BS_AUTOCHECKBOX buttons acting as LEDs / toggles;
//   * owner-drawn "spin" text buttons for the sync rate and the tempo.
//
// These are the VSTGUI equivalents, written against CControl directly so
// they behave exactly like the originals - in particular the vertical
// sliders run *top = maximum*, which is what the DXi property page did with
// its  fVal = (max - GetPos())  conversion.
//------------------------------------------------------------------------

#pragma once

#include "vstgui/vstgui.h"

#include <functional>
#include <string>

namespace SpaceDub {

//------------------------------------------------------------------------
/** Bitmap slider. Vertical sliders put the maximum at the top. */
class SdSlider : public VSTGUI::CControl
{
public:
	SdSlider (const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag,
	          VSTGUI::CBitmap* handle, VSTGUI::CBitmap* groove, bool vertical);

	void draw (VSTGUI::CDrawContext* context) override;

	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseMoveEvent (VSTGUI::MouseMoveEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseCancelEvent (VSTGUI::MouseCancelEvent& event) override;
	void onMouseWheelEvent (VSTGUI::MouseWheelEvent& event) override;

	CLASS_METHODS (SdSlider, VSTGUI::CControl)

private:
	float valueFromPoint (const VSTGUI::CPoint& where) const;
	VSTGUI::CCoord travel () const;

	VSTGUI::SharedPointer<VSTGUI::CBitmap> mHandle;
	VSTGUI::SharedPointer<VSTGUI::CBitmap> mGroove;
	bool  mVertical  = true;
	bool  mDragging  = false;
	float mStartValue = 0.f;
	VSTGUI::CPoint mStartPoint;
};

//------------------------------------------------------------------------
/** Two-frame on/off button (frame 0 on top = off, frame 1 below = on). */
class SdToggle : public VSTGUI::CControl
{
public:
	SdToggle (const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag,
	          VSTGUI::CBitmap* frames);

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;

	CLASS_METHODS (SdToggle, VSTGUI::CControl)

private:
	VSTGUI::SharedPointer<VSTGUI::CBitmap> mFrames;
};

//------------------------------------------------------------------------
/** Stepped text readout, dragged vertically or clicked to advance - the
    stand-in for the DXi's owner-drawn Digistatic / NewSpin controls. */
class SdTextStepper : public VSTGUI::CControl
{
public:
	using TextProvider = std::function<std::string (int step)>;

	SdTextStepper (const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag,
	               int32_t numSteps, TextProvider provider);

	void setTextColor (const VSTGUI::CColor& c) { mTextColor = c; invalid (); }
	void setFont (VSTGUI::CFontRef f)           { mFont = f; invalid (); }

	/** Exactly the string draw() paints. Kept public so the readout can be
	    asserted in a test without a running window server. */
	std::string currentText () const;

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseMoveEvent (VSTGUI::MouseMoveEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseWheelEvent (VSTGUI::MouseWheelEvent& event) override;

	CLASS_METHODS (SdTextStepper, VSTGUI::CControl)

private:
	void setStep (int32_t step);
	int32_t currentStep () const;

	int32_t      mNumSteps = 1;
	TextProvider mProvider;
	VSTGUI::CColor  mTextColor = VSTGUI::CColor (120, 170, 255, 255);
	VSTGUI::SharedPointer<VSTGUI::CFontDesc> mFont;

	bool           mDragging = false;
	VSTGUI::CPoint mStartPoint;
	int32_t        mStartStep = 0;
	bool           mMoved = false;
};

//------------------------------------------------------------------------
/** A small read-only value readout drawn under (or inside) a slider - the
    VST3 stand-in for the DXi's Label / StaticTime display controls.

    The text sits on an opaque black plate with a hairline white border. The
    plate is sized to the string rather than to the view, so a view can be
    made wide enough for the longest reading its slider can produce without a
    short one painting a slab of black across the panel.

    It is mouse-disabled, so it never intercepts a click meant for the slider
    it sits on, and transparent outside the plate, so the panel artwork still
    shows through around it. */
class SdValueDisplay : public VSTGUI::CView
{
public:
	explicit SdValueDisplay (const VSTGUI::CRect& size);

	/** No-op when the text has not actually changed, so dragging a slider
	    does not repaint the readout on every mouse move. */
	void setText (const std::string& text);

	void setTextColor (const VSTGUI::CColor& c)   { mColor = c; invalid (); }
	void setPlateColor (const VSTGUI::CColor& c)  { mPlate = c; invalid (); }
	void setBorderColor (const VSTGUI::CColor& c) { mBorder = c; invalid (); }

	void draw (VSTGUI::CDrawContext* context) override;

	CLASS_METHODS (SdValueDisplay, VSTGUI::CView)

private:
	std::string    mText;
	/** Built on first need: the fallback size for a reading too long for the
	    space its slider reserved. */
	VSTGUI::SharedPointer<VSTGUI::CFontDesc> mSmallFont;
	VSTGUI::CColor mColor  = VSTGUI::CColor (255, 255, 255, 255);
	VSTGUI::CColor mPlate  = VSTGUI::CColor (0, 0, 0, 255);
	VSTGUI::CColor mBorder = VSTGUI::CColor (255, 255, 255, 255);
};

//------------------------------------------------------------------------
} // namespace SpaceDub
