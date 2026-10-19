/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_CONTROLS_H
#define SNAP_CONTROLS_H


#include <Control.h>
#include <View.h>


namespace snap {


class SoundAnalysis;


// The big round button that takes a photo. A BControl drawn by hand with
// app_server's vector drawing: radial gradients (BGradientRadial) for the
// glossy red button and a BShape for the camera symbol. Like any BControl it
// sends its message when clicked, or on Space/Enter when it has the focus.
class ShutterButton : public BControl {
public:
								ShutterButton(BMessage* message);

	virtual	void				Draw(BRect updateRect);
	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseUp(BPoint where);
	virtual	void				MouseMoved(BPoint where, uint32 transit,
									const BMessage* dragMessage);
	virtual	BSize				MinSize();
	virtual	BSize				MaxSize();

private:
			float				_Diameter() const;

			bool				fPressed;
			bool				fInside;
};


// A row of little lights showing how loud the sound Snap listens to is, and
// a dot that blinks on every beat. It refreshes itself in Pulse(), which
// the window calls 20 times a second for views with B_PULSE_NEEDED.
class LevelMeter : public BView {
public:
								LevelMeter(SoundAnalysis& sound);

			void				SetActive(bool active);

	virtual	void				Draw(BRect updateRect);
	virtual	void				Pulse();
	virtual	BSize				MinSize();
	virtual	BSize				MaxSize();

private:
			SoundAnalysis&		fSound;
			bool				fActive;
			int32				fLit;
			bool				fBeat;
			uint32				fBeatCount;
};


}	// namespace snap


#endif	// SNAP_CONTROLS_H
