/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "StageView.h"

#include <Autolock.h>
#include <Cursor.h>
#include <Window.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "app/Messages.h"


namespace snap {


static const bigtime_t kFrameInterval = 1000000 / 60;


static const char*
gl_string(GLenum name)
{
	const GLubyte* string = glGetString(name);
	return string != NULL ? (const char*)string : "";
}


StageView::StageView(FrameExchange& frames, SoundAnalysis& sound)
	:
	BGLView(BRect(0, 0, 639, 359), "stage", B_FOLLOW_ALL_SIDES,
		B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE, BGL_RGB | BGL_DOUBLE),
	fFrames(frames),
	fSound(sound),
	fStateLock("stage state"),
	fRenderer(NULL),
	fRendererLock("stage renderer"),
	fThread(-1),
	fQuitting(0),
	fPaused(0),
	fWidth(640),
	fHeight(360)
{
}


StageView::~StageView()
{
	StopRendering();
}


// #pragma mark - What to show


void
StageView::SetEffect(int32 index)
{
	BAutolock _(fStateLock);
	fState.effect = index;
}


void
StageView::SetGrid(bool grid)
{
	BAutolock _(fStateLock);
	fState.grid = grid;
	fState.hoverCell = -1;
}


void
StageView::SetMirror(bool mirror)
{
	BAutolock _(fStateLock);
	fState.mirror = mirror;
}


void
StageView::SetPlaceholder(bool placeholder)
{
	BAutolock _(fStateLock);
	fState.placeholder = placeholder;
}


void
StageView::SetStatus(const char* status)
{
	BAutolock _(fStateLock);
	fState.status = status;
}


void
StageView::SetPaused(bool paused)
{
	atomic_set(&fPaused, paused ? 1 : 0);
}


void
StageView::StartCountdown(int32 seconds)
{
	BAutolock _(fStateLock);
	fState.countdownStart = system_time();
	fState.countdownLength = seconds;
}


void
StageView::StopCountdown()
{
	BAutolock _(fStateLock);
	fState.countdownStart = 0;
}


void
StageView::Flash()
{
	BAutolock _(fStateLock);
	fState.flashStart = system_time();
}


void
StageView::RequestPhoto(int32 id)
{
	BAutolock _(fStateLock);
	fState.photoRequest = id;
}


StageState
StageView::State() const
{
	BAutolock _(fStateLock);
	return fState;
}


// #pragma mark - BView


void
StageView::AttachedToWindow()
{
	BGLView::AttachedToWindow();
	fTarget = BMessenger(Window());
	fWidth = Bounds().IntegerWidth() + 1;
	fHeight = Bounds().IntegerHeight() + 1;

	{
		BAutolock _(fStateLock);
		const rgb_color accent = ui_color(B_CONTROL_HIGHLIGHT_COLOR);
		fState.accent = accent;
	}

	atomic_set(&fQuitting, 0);
	fThread = spawn_thread(&_RenderThread, "Snap render", B_DISPLAY_PRIORITY,
		this);
	if (fThread >= 0)
		resume_thread(fThread);
	MakeFocus(true);
}


void
StageView::DetachedFromWindow()
{
	// The render thread may be waiting for the window (BGLView draws the
	// finished frame on the window's thread): let go of the window while
	// waiting for it, as GLTeapot does. (The window usually stopped it
	// already, see SnapWindow's destructor.)
	if (fThread >= 0) {
		Window()->Unlock();
		StopRendering();
		Window()->Lock();
	}
	BGLView::DetachedFromWindow();
}


void
StageView::FrameResized(float width, float height)
{
	BGLView::FrameResized(width, height);
	// The render thread sets the viewport from these at its next frame.
	atomic_set(&fWidth, (int32)width + 1);
	atomic_set(&fHeight, (int32)height + 1);
}


void
StageView::MouseDown(BPoint where)
{
	MakeFocus(true);
	const StageState state = State();
	if (!state.grid)
		return;

	int32 cell = -1;
	{
		BAutolock _(fRendererLock);
		if (fRenderer != NULL)
			cell = fRenderer->CellAt(where);
	}
	if (cell >= 0) {
		BMessage message(kMsgGridCellClicked);
		message.AddInt32("index", cell);
		Window()->PostMessage(&message);
	}
}


void
StageView::MouseMoved(BPoint where, uint32 transit,
	const BMessage* dragMessage)
{
	int32 cell = -1;
	if (transit != B_EXITED_VIEW && transit != B_OUTSIDE_VIEW) {
		BAutolock _(fRendererLock);
		if (fRenderer != NULL)
			cell = fRenderer->CellAt(where);
	}

	{
		BAutolock _(fStateLock);
		if (!fState.grid)
			cell = -1;
		if (cell == fState.hoverCell)
			return;
		fState.hoverCell = cell;
	}
	// A pointing hand over the cells, to say they can be clicked.
	BCursor cursor(cell >= 0 ? B_CURSOR_ID_FOLLOW_LINK
		: B_CURSOR_ID_SYSTEM_DEFAULT);
	SetViewCursor(&cursor);
}


void
StageView::KeyDown(const char* bytes, int32 count)
{
	switch (bytes[0]) {
		case B_SPACE:
		case B_ENTER:
			Window()->PostMessage(kMsgTakePhoto);
			break;
		case B_LEFT_ARROW:
			Window()->PostMessage(kMsgPreviousEffect);
			break;
		case B_RIGHT_ARROW:
			Window()->PostMessage(kMsgNextEffect);
			break;
		case B_ESCAPE:
			if (State().grid)
				Window()->PostMessage(kMsgToggleGrid);
			break;
		default:
			BGLView::KeyDown(bytes, count);
			break;
	}
}


BSize
StageView::MinSize()
{
	return BSize(320, 180);
}


// #pragma mark - Render thread


status_t
StageView::_RenderThread(void* data)
{
	((StageView*)data)->_RenderLoop();
	return B_OK;
}


void
StageView::_RenderLoop()
{
	// Set up: the renderer builds its shaders with the context held.
	LockGL();
	StageRenderer* renderer = new StageRenderer(fFrames, fSound, fTarget);
	BString error;
	const status_t status = renderer->Init(error);
	BMessage report(status == B_OK ? kMsgRendererReady : kMsgRendererFailed);
	report.AddString("renderer", gl_string(GL_RENDERER));
	report.AddString("version", gl_string(GL_VERSION));
	report.AddString("error", error);
	UnlockGL();
	fTarget.SendMessage(&report);
	if (status != B_OK) {
		LockGL();
		delete renderer;
		UnlockGL();
		return;
	}
	{
		BAutolock _(fRendererLock);
		fRenderer = renderer;
	}

	// SNAP_STATS=1 prints the frame rate and the time a frame takes to draw,
	// every five seconds.
	const char* stats = getenv("SNAP_STATS");
	const bool showStats = stats != NULL && strcmp(stats, "1") == 0;
	bigtime_t statsStart = system_time();
	bigtime_t drawTime = 0;
	int32 frames = 0;

	bigtime_t next = system_time();
	while (atomic_get(&fQuitting) == 0) {
		const StageState state = State();
		// Minimized, frames are only drawn to take a photo asked for.
		if (atomic_get(&fPaused) != 0
			&& renderer->PhotoTaken() >= state.photoRequest) {
			snooze(100000);
			next = system_time();
			continue;
		}

		const int width = atomic_get(&fWidth);
		const int height = atomic_get(&fHeight);

		const bigtime_t drawStart = system_time();
		LockGL();
		renderer->Render(state, width, height);
		SwapBuffers();
		UnlockGL();
		drawTime += system_time() - drawStart;

		if (showStats && ++frames == 300) {
			const bigtime_t now = system_time();
			fprintf(stderr, "Snap: %.1f frames/s, %.1f ms to draw one (%s, %dx%d)\n",
				frames * 1000000.0 / (now - statsStart),
				drawTime / 1000.0 / frames, state.grid ? "grid" : "one effect",
				width, height);
			statsStart = now;
			drawTime = 0;
			frames = 0;
		}

		next += kFrameInterval;
		const bigtime_t now = system_time();
		if (next < now)
			next = now;
		snooze_until(next, B_SYSTEM_TIMEBASE);
	}

	{
		BAutolock _(fRendererLock);
		fRenderer = NULL;
	}
	// GL objects go with the context held.
	LockGL();
	delete renderer;
	UnlockGL();
}


void
StageView::StopRendering()
{
	if (fThread < 0)
		return;
	atomic_set(&fQuitting, 1);
	status_t result;
	wait_for_thread(fThread, &result);
	fThread = -1;
}


}	// namespace snap
