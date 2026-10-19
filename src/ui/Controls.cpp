/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "Controls.h"

#include <GradientRadial.h>
#include <Shape.h>
#include <Window.h>

#include <algorithm>
#include <cmath>

#include "audio/SoundAnalysis.h"


namespace snap {


static float
ui_scale()
{
	// Haiku scales its interface with the font size; 12 pt is 100 %.
	return std::max(1.0f, be_plain_font->Size() / 12.0f);
}


// #pragma mark - ShutterButton


ShutterButton::ShutterButton(BMessage* message)
	:
	BControl("shutter", "Take Photo", message,
		B_WILL_DRAW | B_NAVIGABLE | B_FULL_UPDATE_ON_RESIZE),
	fPressed(false),
	fInside(false)
{
	SetToolTip("Take a photo (Space)");
}


float
ShutterButton::_Diameter() const
{
	return roundf(46 * ui_scale());
}


BSize
ShutterButton::MinSize()
{
	const float size = _Diameter() + 4;
	return BSize(size, size);
}


BSize
ShutterButton::MaxSize()
{
	return MinSize();
}


void
ShutterButton::Draw(BRect updateRect)
{
	SetDrawingMode(B_OP_ALPHA);
	SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);

	const BRect bounds = Bounds();
	const float diameter = _Diameter();
	const BPoint center((bounds.left + bounds.right) / 2,
		(bounds.top + bounds.bottom) / 2);
	const float radius = diameter / 2;

	// A metal ring, lit from the upper left.
	BGradientRadial ring(BPoint(center.x - radius * 0.4f,
		center.y - radius * 0.5f), radius * 1.6f);
	ring.AddColor(make_color(250, 250, 250), 0);
	ring.AddColor(make_color(150, 152, 158), 255);
	FillEllipse(center, radius, radius, ring);

	// The red button, darker while pressed, grey while disabled.
	const float inner = radius - 4 * ui_scale();
	rgb_color light = make_color(255, 112, 96);
	rgb_color dark = make_color(196, 24, 36);
	if (!IsEnabled()) {
		light = make_color(200, 200, 200);
		dark = make_color(140, 140, 140);
	} else if (fPressed && fInside) {
		light = make_color(214, 70, 60);
		dark = make_color(150, 10, 24);
	}
	BGradientRadial button(BPoint(center.x - inner * 0.3f,
		center.y - inner * 0.4f), inner * 1.4f);
	button.AddColor(light, 0);
	button.AddColor(dark, 255);
	FillEllipse(center, inner, inner, button);

	// The camera symbol: a body with a bump on top and a lens.
	const float unit = inner / 11;
	BShape camera;
	camera.MoveTo(BPoint(center.x - 6 * unit, center.y - 3 * unit));
	camera.LineTo(BPoint(center.x - 2.5f * unit, center.y - 3 * unit));
	camera.LineTo(BPoint(center.x - 1.5f * unit, center.y - 4.5f * unit));
	camera.LineTo(BPoint(center.x + 1.5f * unit, center.y - 4.5f * unit));
	camera.LineTo(BPoint(center.x + 2.5f * unit, center.y - 3 * unit));
	camera.LineTo(BPoint(center.x + 6 * unit, center.y - 3 * unit));
	camera.LineTo(BPoint(center.x + 6 * unit, center.y + 4 * unit));
	camera.LineTo(BPoint(center.x - 6 * unit, center.y + 4 * unit));
	camera.Close();
	SetHighColor(255, 255, 255, IsEnabled() ? 235 : 160);
	MovePenTo(B_ORIGIN);
	FillShape(&camera);
	SetHighColor(dark);
	FillEllipse(BPoint(center.x, center.y + 0.5f * unit), 2.6f * unit,
		2.6f * unit);
	SetHighColor(255, 255, 255, IsEnabled() ? 235 : 160);
	FillEllipse(BPoint(center.x, center.y + 0.5f * unit), 1.5f * unit,
		1.5f * unit);

	// The keyboard focus, as a ring in the system's navigation colour.
	if (IsFocus() && Window()->IsActive()) {
		SetHighUIColor(B_KEYBOARD_NAVIGATION_COLOR);
		SetPenSize(2);
		StrokeEllipse(center, radius + 1, radius + 1);
		SetPenSize(1);
	}
}


void
ShutterButton::MouseDown(BPoint where)
{
	if (!IsEnabled())
		return;
	fPressed = true;
	fInside = true;
	SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	Invalidate();
}


void
ShutterButton::MouseMoved(BPoint where, uint32 transit,
	const BMessage* dragMessage)
{
	if (!fPressed)
		return;
	const bool inside = Bounds().Contains(where);
	if (inside != fInside) {
		fInside = inside;
		Invalidate();
	}
}


void
ShutterButton::MouseUp(BPoint where)
{
	if (!fPressed)
		return;
	fPressed = false;
	Invalidate();
	if (Bounds().Contains(where))
		Invoke();
}


// #pragma mark - LevelMeter


static const int32 kLights = 12;


LevelMeter::LevelMeter(SoundAnalysis& sound)
	:
	BView("level meter", B_WILL_DRAW | B_PULSE_NEEDED),
	fSound(sound),
	fActive(false),
	fLit(0),
	fBeat(false),
	fBeatCount(0)
{
	SetToolTip("How loud the sound is that the effects react to");
}


void
LevelMeter::SetActive(bool active)
{
	fActive = active;
	Invalidate();
}


BSize
LevelMeter::MinSize()
{
	const float scale = ui_scale();
	return BSize(kLights * 5 * scale + 14 * scale, 14 * scale);
}


BSize
LevelMeter::MaxSize()
{
	return MinSize();
}


void
LevelMeter::Pulse()
{
	const AudioFeatures features = fSound.Snapshot();
	const int32 lit = fActive ? (int32)roundf(features.level * kLights) : 0;
	// A beat lights the dot for one pulse, however short it was.
	const bool beat = fActive && features.beatCount != fBeatCount;
	fBeatCount = features.beatCount;
	if (lit != fLit || beat != fBeat) {
		fLit = lit;
		fBeat = beat;
		Invalidate();
	}
}


void
LevelMeter::Draw(BRect updateRect)
{
	const float scale = ui_scale();
	const BRect bounds = Bounds();
	const float lightWidth = 3 * scale;
	const float step = 5 * scale;
	const float top = bounds.top + 2 * scale;
	const float bottom = bounds.bottom - 2 * scale;
	const rgb_color background = ViewColor();

	for (int32 i = 0; i < kLights; i++) {
		// Green, then yellow, then red at the top of the scale.
		rgb_color color = i < 8 ? make_color(60, 200, 90)
			: i < 10 ? make_color(240, 200, 40) : make_color(240, 70, 50);
		if (i >= fLit)
			color = tint_color(background, B_DARKEN_2_TINT);
		SetHighColor(color);
		const float left = bounds.left + i * step;
		FillRect(BRect(left, top, left + lightWidth - 1, bottom));
	}

	// The beat light.
	const float size = bottom - top;
	SetHighColor(fBeat ? make_color(255, 184, 0)
		: tint_color(background, B_DARKEN_2_TINT));
	FillEllipse(BRect(bounds.right - size, top, bounds.right, bottom));
}


}	// namespace snap
