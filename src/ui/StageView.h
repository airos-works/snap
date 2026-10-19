/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_STAGE_VIEW_H
#define SNAP_STAGE_VIEW_H


#include <GLView.h>
#include <Locker.h>
#include <Messenger.h>

#include "render/StageRenderer.h"


namespace snap {


class FrameExchange;
class SoundAnalysis;


// The live picture: a BGLView with a render thread of its own.
//
// BGLView gives a view an OpenGL context. Like GLTeapot, Snap draws from a
// separate thread so that the window's thread stays free for the interface:
// the thread takes the context with LockGL(), draws a frame through the
// StageRenderer, shows it with SwapBuffers() and lets go with UnlockGL().
// Frames are paced to 60 a second by the thread itself, because on most
// air/OS graphics stacks SwapBuffers() does not wait for the display.
//
// The window changes what is shown through the setters below; they only
// change a StageState under a lock, which the render thread copies at the
// start of every frame. Clicks and keys on the stage are turned into
// messages to the window.
class StageView : public BGLView {
public:
								StageView(FrameExchange& frames,
									SoundAnalysis& sound);
	virtual						~StageView();

			void				SetEffect(int32 index);
			void				SetGrid(bool grid);
			void				SetMirror(bool mirror);
			void				SetPlaceholder(bool placeholder);
			void				SetStatus(const char* status);
			void				SetPaused(bool paused);
			void				StartCountdown(int32 seconds);
			void				StopCountdown();
			void				Flash();
			void				RequestPhoto(int32 id);
			StageState			State() const;

	// Ends the render thread; the window calls it before it closes.
			void				StopRendering();

	virtual	void				AttachedToWindow();
	virtual	void				DetachedFromWindow();
	virtual	void				FrameResized(float width, float height);
	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseMoved(BPoint where, uint32 transit,
									const BMessage* dragMessage);
	virtual	void				KeyDown(const char* bytes, int32 count);
	virtual	BSize				MinSize();

private:
	static	status_t			_RenderThread(void* data);
			void				_RenderLoop();

			FrameExchange&		fFrames;
			SoundAnalysis&		fSound;
			BMessenger			fTarget;

			mutable BLocker		fStateLock;
			StageState			fState;

			StageRenderer*		fRenderer;
			BLocker				fRendererLock;
			thread_id			fThread;
			int32				fQuitting;
			int32				fPaused;
			int32				fWidth;
			int32				fHeight;
};


}	// namespace snap


#endif	// SNAP_STAGE_VIEW_H
