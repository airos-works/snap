/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "StageRenderer.h"

#include <Autolock.h>
#include <Bitmap.h>
#include <Font.h>
#include <OS.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "app/Messages.h"
#include "audio/SoundAnalysis.h"
#include "capture/FrameExchange.h"


namespace snap {


// The size of the stand-in scene when there is no camera, and the largest
// picture an effect draws on the big stage (photos have this size at most).
static const int kPlaceholderWidth = 1280;
static const int kPlaceholderHeight = 720;
static const int kMaxStageWidth = 1920;

// The sound texture: a row of spectrum and a row of waveform.
static const int kSoundTextureWidth = 128;

// The grid: margins and gaps in view pixels.
static const float kGridMargin = 14.0f;
static const float kGridGap = 10.0f;
static const float kCellRadius = 8.0f;

static const rgb_color kBackground = { 24, 24, 27, 255 };
static const rgb_color kWhite = { 255, 255, 255, 255 };
static const rgb_color kBlack = { 0, 0, 0, 255 };

static const bigtime_t kToastDuration = 1600000;
static const bigtime_t kFlashDuration = 450000;
static const bigtime_t kPhotoSettleTime = 500000;


StageState::StageState()
	:
	effect(NormalEffectIndex()),
	grid(false),
	mirror(true),
	placeholder(true),
	hoverCell(-1),
	countdownStart(0),
	countdownLength(3),
	flashStart(0),
	photoRequest(0)
{
	accent = make_color(255, 184, 0);
}


bool
StageRenderer::Box::Contains(BPoint point) const
{
	return point.x >= x && point.x < x + width && point.y >= y
		&& point.y < y + height;
}


StageRenderer::StageRenderer(FrameExchange& frames, SoundAnalysis& sound,
	const BMessenger& target)
	:
	fFrames(frames),
	fSound(sound),
	fTarget(target),
	fVertexArray(0),
	fCurrentCamera(0),
	fHasPicture(false),
	fShowingPlaceholder(false),
	fHistoryInUse(false),
	fLastBeatCount(0),
	fLastBeatTime(0),
	fAudioTime(0),
	fGlyphCount(0),
	fFrameNumber(1),
	fStartTime(system_time()),
	fLastFrame(0),
	fPhotoTaken(0),
	fLiveShaders(getenv("SNAP_SHADER_DIR") != NULL),
	fViewWidth(0),
	fViewHeight(0),
	fLayoutLock("stage layout"),
	fToastEffect(-1),
	fToastStart(0)
{
}


StageRenderer::~StageRenderer()
{
	for (std::unique_ptr<Slot>& slot : fSlots)
		delete slot->effect;
	if (fVertexArray != 0)
		glDeleteVertexArrays(1, &fVertexArray);
}


status_t
StageRenderer::Init(BString& error)
{
	// Everything is drawn as rectangles made up in the vertex shader; an
	// (empty) vertex array object still has to be bound for that.
	glGenVertexArrays(1, &fVertexArray);
	glBindVertexArray(fVertexArray);

	status_t status = fShaders.Init(error);
	if (status == B_OK)
		status = fShaders.Build(fImageProgram, "ui-image.glsl", error);
	if (status == B_OK)
		status = fShaders.Build(fFillProgram, "ui-fill.glsl", error);
	if (status == B_OK)
		status = fShaders.Build(fTextProgram, "ui-text.glsl", error);
	if (status == B_OK)
		status = fShaders.Build(fPlaceholderProgram, "placeholder.glsl", error);
	if (status != B_OK)
		return status;

	// The effects. One that does not build is left out and reported; the
	// others still work.
	BFont labelFont(be_bold_font);
	for (int32 i = 0; i < CountEffects(); i++) {
		std::unique_ptr<Slot> slot(new Slot);
		slot->effect = CreateEffect(i);
		slot->lastFrame = 0;
		slot->shownSince = 0;
		slot->sizedSince = 0;
		BString effectError;
		slot->ready = slot->effect->Prepare(fShaders, effectError) == B_OK;
		if (!slot->ready)
			fprintf(stderr, "Snap: effect %s: %s\n", EffectAt(i).id,
				effectError.String());
		slot->label.SetTo(EffectAt(i).name, labelFont);
		fSlots.push_back(std::move(slot));
	}

	fGlyphCount = MakeGlyphRamp(fGlyphs);
	fSoundTexture.Allocate(kSoundTextureWidth, 2, GL_R32F);

	CheckGLError("StageRenderer::Init");
	return B_OK;
}


void
StageRenderer::Render(const StageState& state, int width, int height)
{
	const bigtime_t now = system_time();
	const float delta = fLastFrame == 0 ? 1.0f / 60
		: std::clamp((now - fLastFrame) / 1000000.0f, 0.0f, 0.1f);
	fLastFrame = now;
	fFrameNumber++;

	glBindVertexArray(fVertexArray);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

	_UpdateSound(now, delta);
	const bool newPicture = _UpdateCamera(state, now);
	_Layout(state, width, height);
	_RenderEffects(state, now, delta, newPicture);

	if (state.photoRequest > fPhotoTaken && _ReadyForPhoto(state, now)) {
		fPhotoTaken = state.photoRequest;
		_TakePhoto(state);
	}

	// The view itself.
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, width, height);
	glClearColor(kBackground.red / 255.0f, kBackground.green / 255.0f,
		kBackground.blue / 255.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (state.grid)
		_DrawGrid(state);
	else
		_DrawStage(state, now);
	_DrawOverlays(state, now);
	glDisable(GL_BLEND);

	CheckGLError("StageRenderer::Render");
}


int32
StageRenderer::CellAt(BPoint point) const
{
	BAutolock _(fLayoutLock);
	for (size_t i = 0; i < fCellBoxes.size(); i++) {
		if (fCellBoxes[i].Contains(point))
			return (int32)i;
	}
	return -1;
}


// #pragma mark - Inputs


bool
StageRenderer::_UpdateCamera(const StageState& state, bigtime_t now)
{
	const VideoFrame* frame = state.placeholder ? NULL : fFrames.TakeNewest();
	if (frame != NULL && frame->IsValid()) {
		// The new picture replaces the older of the two; the other one
		// becomes "previous".
		fCurrentCamera ^= 1;
		RenderTarget& current = fCamera[fCurrentCamera];
		current.Resize(frame->width, frame->height, GL_RGBA8, true);
		current.ColorTexture().UploadBGRA(frame->pixels.data(),
			frame->bytesPerRow);
		// The camera's first picture (or a new size): it is its own
		// "previous", so nothing seems to move.
		RenderTarget& previous = fCamera[fCurrentCamera ^ 1];
		if (fShowingPlaceholder || !fHasPicture
			|| previous.Width() != frame->width
			|| previous.Height() != frame->height) {
			previous.Resize(frame->width, frame->height, GL_RGBA8, true);
			previous.ColorTexture().UploadBGRA(frame->pixels.data(),
				frame->bytesPerRow);
		}
		if (fShowingPlaceholder || !fHasPicture)
			_PictureSourceChanged(now);
		fShowingPlaceholder = false;
		fHasPicture = true;
		return true;
	}

	if (state.placeholder) {
		// The stand-in scene, drawn anew every frame like a camera picture
		// (into both pictures the first time, so nothing seems to move).
		const bool first = !fShowingPlaceholder || !fHasPicture;
		for (int i = 0; i < (first ? 2 : 1); i++) {
			fCurrentCamera ^= 1;
			RenderTarget& current = fCamera[fCurrentCamera];
			current.Resize(kPlaceholderWidth, kPlaceholderHeight, GL_RGBA8,
				true);
			current.Bind();
			fPlaceholderProgram.Use();
			EffectContext context = {};
			context.cameraWidth = kPlaceholderWidth;
			context.cameraHeight = kPlaceholderHeight;
			context.time = (now - fStartTime) / 1000000.0f;
			context.Apply(fPlaceholderProgram, kPlaceholderWidth,
				kPlaceholderHeight);
			DrawRectangle();
			current.GenerateMipmaps();
		}
		if (first)
			_PictureSourceChanged(now);
		fShowingPlaceholder = true;
		fHasPicture = true;
		return true;
	}
	return false;
}


void
StageRenderer::_PictureSourceChanged(bigtime_t now)
{
	// What the effects remember (flames, trails, the past) belongs to the
	// pictures that came before: start them all afresh.
	fHistoryInUse = false;
	for (std::unique_ptr<Slot>& slot : fSlots) {
		slot->effect->Reset();
		slot->shownSince = now;
	}
}


void
StageRenderer::_UpdateSound(bigtime_t now, float delta)
{
	fFeatures = fSound.Snapshot();
	if (fFeatures.beatCount != fLastBeatCount) {
		fLastBeatCount = fFeatures.beatCount;
		fLastBeatTime = now;
	}
	// A clock for the effects that runs faster the louder it is.
	fAudioTime += delta * (0.3f + 2.2f * fFeatures.level);

	// Row 0: the 32 spectrum bands stretched over 128 texels; row 1: the
	// waveform.
	float texels[kSoundTextureWidth * 2];
	const int bands = AudioFeatures::kSpectrumBands;
	for (int x = 0; x < kSoundTextureWidth; x++) {
		const float position = (x + 0.5f) * bands / kSoundTextureWidth - 0.5f;
		const int low = std::clamp((int)floorf(position), 0, bands - 1);
		const int high = std::min(low + 1, bands - 1);
		const float t = std::clamp(position - low, 0.0f, 1.0f);
		texels[x] = fFeatures.spectrum[low] * (1 - t)
			+ fFeatures.spectrum[high] * t;
		texels[kSoundTextureWidth + x] = fFeatures.waveform[
			x * AudioFeatures::kWaveformPoints / kSoundTextureWidth];
	}
	fSoundTexture.UploadFloats(texels);
}


// #pragma mark - Effects


void
StageRenderer::_Layout(const StageState& state, int width, int height)
{
	fViewWidth = width;
	fViewHeight = height;
	const float aspect = fCamera[fCurrentCamera].IsValid()
		? (float)fCamera[fCurrentCamera].Width() / fCamera[fCurrentCamera].Height()
		: 16.0f / 9.0f;

	// The big stage: as large as fits, centred.
	float stageWidth = width;
	float stageHeight = stageWidth / aspect;
	if (stageHeight > height) {
		stageHeight = height;
		stageWidth = stageHeight * aspect;
	}
	fStageBox = { (width - stageWidth) / 2, (height - stageHeight) / 2,
		stageWidth, stageHeight };

	BAutolock _(fLayoutLock);
	fCellBoxes.clear();
	if (!state.grid)
		return;

	// The grid: four across for more than nine effects, three otherwise;
	// cells keep the camera's shape.
	const int count = (int)fSlots.size();
	const int columns = count > 9 ? 4 : 3;
	const int rows = (count + columns - 1) / columns;
	const float roomWidth = (width - 2 * kGridMargin - (columns - 1) * kGridGap)
		/ columns;
	const float roomHeight = (height - 2 * kGridMargin - (rows - 1) * kGridGap)
		/ rows;
	float cellWidth = std::max(8.0f, roomWidth);
	float cellHeight = cellWidth / aspect;
	if (cellHeight > roomHeight) {
		cellHeight = std::max(8.0f, roomHeight);
		cellWidth = cellHeight * aspect;
	}
	const float gridWidth = columns * cellWidth + (columns - 1) * kGridGap;
	const float gridHeight = rows * cellHeight + (rows - 1) * kGridGap;
	const float left = (width - gridWidth) / 2;
	const float top = (height - gridHeight) / 2;
	for (int i = 0; i < count; i++) {
		const int column = i % columns;
		const int row = i / columns;
		fCellBoxes.push_back({ floorf(left + column * (cellWidth + kGridGap)),
			floorf(top + row * (cellHeight + kGridGap)), floorf(cellWidth),
			floorf(cellHeight) });
	}
}


void
StageRenderer::_RenderEffects(const StageState& state, bigtime_t now,
	float delta, bool newPicture)
{
	const RenderTarget& camera = fCamera[fCurrentCamera];
	if (!camera.IsValid())
		return;

	// Which effects are on screen, and at what size.
	std::vector<int32> shown;
	if (state.grid) {
		for (int32 i = 0; i < (int32)fSlots.size(); i++)
			shown.push_back(i);
	} else if (state.effect >= 0 && state.effect < (int32)fSlots.size())
		shown.push_back(state.effect);

	// Time Warp's memory is only kept while it is on screen.
	bool needHistory = false;
	for (int32 index : shown)
		needHistory |= fSlots[index]->ready && fSlots[index]->effect->NeedsHistory();
	if (needHistory && !fHistoryInUse) {
		// Back on screen: only the pictures from now on.
		fHistory.Clear();
		fHistory.Push(camera);
	} else if (needHistory && newPicture)
		fHistory.Push(camera);
	fHistoryInUse = needHistory;

	EffectContext context;
	context.cameraWidth = camera.Width();
	context.cameraHeight = camera.Height();
	context.time = (now - fStartTime) / 1000000.0f;
	context.delta = delta;
	context.mirror = state.mirror;
	context.sound = fFeatures;
	context.beatAge = fLastBeatTime == 0 ? 100.0f
		: std::min(100.0f, (now - fLastBeatTime) / 1000000.0f);
	context.audioTime = fAudioTime;
	context.history = needHistory && fHistory.Count() > 0 ? &fHistory : NULL;
	context.glyphs = &fGlyphs;
	context.glyphCount = fGlyphCount;

	camera.BindTexture(kCameraTextureUnit);
	fCamera[fCurrentCamera ^ 1].BindTexture(kPreviousTextureUnit);
	fSoundTexture.Bind(kSoundTextureUnit);

	for (int32 index : shown) {
		Slot& slot = *fSlots[index];
		if (!slot.ready)
			continue;

		// Shown again after a while: start the effect's clock and memory
		// afresh (and, while working on the shaders, read them again).
		if (slot.lastFrame != fFrameNumber - 1) {
			slot.shownSince = now;
			if (fLiveShaders) {
				BString error;
				if (slot.effect->Prepare(fShaders, error) != B_OK)
					fprintf(stderr, "Snap: %s\n", error.String());
			}
			slot.effect->Reset();
		}
		slot.lastFrame = fFrameNumber;

		// On the big stage an effect draws at the camera's resolution (that
		// is the photo); in the grid at the size of its cell.
		int width;
		int height;
		if (state.grid) {
			BAutolock _(fLayoutLock);
			width = std::max(16, (int)fCellBoxes[index].width);
			height = std::max(16, (int)fCellBoxes[index].height);
		} else {
			width = std::min(camera.Width(), kMaxStageWidth);
			height = std::max(16, camera.Height() * width / camera.Width());
		}
		if (slot.output.Resize(width, height))
			slot.sizedSince = now;

		context.effectTime = (now - slot.shownSince) / 1000000.0f;
		slot.effect->Render(context, slot.output);
	}
}


bool
StageRenderer::_ReadyForPhoto(const StageState& state, bigtime_t now) const
{
	// A photo is of the effect on the big stage, and only once it has been
	// there for a moment: when it came from the grid, its picture was made
	// anew at the stage's size, and an effect with a memory (the flames,
	// the trails) needs a little time to build it up again.
	if (state.grid || state.effect < 0 || state.effect >= (int32)fSlots.size())
		return false;
	const Slot& slot = *fSlots[state.effect];
	if (!slot.ready)
		return true;	// answered with an error by _TakePhoto()
	const bigtime_t since = std::max(slot.sizedSince, slot.shownSince);
	return slot.output.IsValid() && now - since >= kPhotoSettleTime;
}


void
StageRenderer::_TakePhoto(const StageState& state)
{
	BMessage failed(kMsgPhotoFailed);
	failed.AddInt32("id", state.photoRequest);
	if (state.effect < 0 || state.effect >= (int32)fSlots.size()
		|| !fSlots[state.effect]->ready
		|| !fSlots[state.effect]->output.IsValid()) {
		failed.AddString("error", "There is no picture to take yet.");
		fTarget.SendMessage(&failed);
		return;
	}

	const RenderTarget& output = fSlots[state.effect]->output;
	const int width = output.Width();
	const int height = output.Height();
	BBitmap* bitmap = new BBitmap(BRect(0, 0, width - 1, height - 1), B_RGB32);
	if (bitmap->InitCheck() != B_OK) {
		delete bitmap;
		failed.AddString("error", "Out of memory for the photo.");
		fTarget.SendMessage(&failed);
		return;
	}

	// B_RGB32 is B, G, R, A in memory, which is GL_BGRA. OpenGL's rows go
	// from the bottom up, a BBitmap's from the top down.
	std::vector<uint8> pixels((size_t)width * height * 4);
	{
		FramebufferScope scope;
		glBindFramebuffer(GL_READ_FRAMEBUFFER, output.Framebuffer());
		glPixelStorei(GL_PACK_ALIGNMENT, 4);
		glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE,
			pixels.data());
	}
	for (int y = 0; y < height; y++) {
		memcpy((uint8*)bitmap->Bits() + y * bitmap->BytesPerRow(),
			pixels.data() + (size_t)(height - 1 - y) * width * 4, width * 4);
	}

	BMessage message(kMsgPhotoTaken);
	message.AddPointer("bitmap", bitmap);
	message.AddString("effect", EffectAt(state.effect).id);
	message.AddInt32("id", state.photoRequest);
	if (fTarget.SendMessage(&message) != B_OK)
		delete bitmap;
}


// #pragma mark - Drawing


void
StageRenderer::_DrawStage(const StageState& state, bigtime_t now)
{
	if (state.effect < 0 || state.effect >= (int32)fSlots.size())
		return;
	Slot& slot = *fSlots[state.effect];
	if (slot.ready && slot.output.IsValid())
		_DrawImage(slot.output.TextureId(), fStageBox, 0, 0, kBlack, 0);

	// The effect's name, for a moment after it was chosen.
	if (fToastEffect != state.effect) {
		fToastEffect = state.effect;
		fToastStart = now;
	}
	const bigtime_t age = now - fToastStart;
	if (age < kToastDuration) {
		BFont font(be_bold_font);
		font.SetSize(be_bold_font->Size() * 1.6f);
		fToastText.SetTo(EffectAt(state.effect).name, font);
		const float alpha = 1.0f - std::clamp(
			(age - 1100000) / 500000.0f, 0.0f, 1.0f);
		const float centerX = fStageBox.x + fStageBox.width / 2;
		const float top = fStageBox.y + 16;
		_Fill({ centerX - fToastText.Width() / 2 - 14, top,
			fToastText.Width() + 28, fToastText.Height() + 10 }, kBlack,
			0.55f * alpha, (fToastText.Height() + 10) / 2);
		_DrawText(fToastText, centerX - fToastText.Width() / 2, top + 5,
			kWhite, alpha);
	}
}


void
StageRenderer::_DrawGrid(const StageState& state)
{
	std::vector<Box> cells;
	{
		BAutolock _(fLayoutLock);
		cells = fCellBoxes;
	}
	for (size_t i = 0; i < cells.size() && i < fSlots.size(); i++) {
		Slot& slot = *fSlots[i];
		const Box& cell = cells[i];
		const bool selected = (int32)i == state.effect;
		const bool hovered = (int32)i == state.hoverCell;
		if (slot.ready && slot.output.IsValid()) {
			_DrawImage(slot.output.TextureId(), cell, kCellRadius,
				selected ? 3.0f : hovered ? 2.0f : 0.0f,
				selected ? state.accent : kWhite, selected ? 1.0f : 0.6f);
		} else
			_Fill(cell, kBlack, 1.0f, kCellRadius);
		_DrawLabel(slot.label, cell.x + cell.width / 2, cell.y + cell.height - 8,
			1.0f);
	}
}


void
StageRenderer::_DrawOverlays(const StageState& state, bigtime_t now)
{
	const Box view = { 0, 0, (float)fViewWidth, (float)fViewHeight };
	const Box& area = state.grid ? view : fStageBox;

	// The countdown: a big number for each second left, popping in.
	if (state.countdownStart != 0 && now >= state.countdownStart) {
		const float elapsed = (now - state.countdownStart) / 1000000.0f;
		const int left = state.countdownLength - (int)floorf(elapsed);
		if (left > 0) {
			const float phase = elapsed - floorf(elapsed);
			BFont font(be_bold_font);
			font.SetSize(std::max(24.0f, area.height * 0.3f));
			BString digit;
			digit << left;
			fCountdownText.SetTo(digit.String(), font);
			_Fill(area, kBlack, 0.25f, 0);
			const float scale = 1.0f + 0.3f * powf(1.0f - phase, 3.0f);
			const float width = fCountdownText.Width() * scale;
			const float height = fCountdownText.Height() * scale;
			_DrawText(fCountdownText, area.x + (area.width - width) / 2,
				area.y + (area.height - height) / 2, kWhite,
				1.0f - 0.35f * phase, scale);
		}
	}

	// A line of status at the bottom ("No camera", ...).
	if (state.status.Length() > 0) {
		BFont font(be_bold_font);
		font.SetSize(be_bold_font->Size() * 1.1f);
		fStatusText.SetTo(state.status.String(), font);
		_DrawLabel(fStatusText, area.x + area.width / 2,
			area.y + area.height - (state.grid ? 2 : 18), 1.0f);
	}

	// The flash: the whole view white, fading out.
	if (state.flashStart != 0 && now >= state.flashStart
		&& now - state.flashStart < kFlashDuration) {
		const float t = (now - state.flashStart) / (float)kFlashDuration;
		_Fill(view, kWhite, (1.0f - t) * (1.0f - t), 0);
	}
}


void
StageRenderer::_SetRect(ShaderProgram& program, const Box& box)
{
	// View coordinates (top left origin, y down) to OpenGL's normalized
	// device coordinates (centre origin, y up).
	const float left = box.x / fViewWidth * 2 - 1;
	const float right = (box.x + box.width) / fViewWidth * 2 - 1;
	const float top = 1 - box.y / fViewHeight * 2;
	const float bottom = 1 - (box.y + box.height) / fViewHeight * 2;
	program.Set("uRect", left, bottom, right, top);
}


void
StageRenderer::_DrawImage(GLuint texture, const Box& box, float radius,
	float borderWidth, rgb_color border, float borderAlpha)
{
	fImageProgram.Use();
	_SetRect(fImageProgram, box);
	fImageProgram.Set("uTexRect", 0.0f, 0.0f, 1.0f, 1.0f);
	fImageProgram.Set("uImage", 0);
	fImageProgram.Set("uSize", box.width, box.height);
	fImageProgram.Set("uRadius", radius);
	fImageProgram.Set("uBorderWidth", borderWidth);
	fImageProgram.Set("uBorderColor", border.red / 255.0f,
		border.green / 255.0f, border.blue / 255.0f, borderAlpha);
	fImageProgram.Set("uOpacity", 1.0f);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	DrawRectangle();
}


void
StageRenderer::_Fill(const Box& box, rgb_color color, float alpha,
	float radius)
{
	fFillProgram.Use();
	_SetRect(fFillProgram, box);
	fFillProgram.Set("uTexRect", 0.0f, 0.0f, 1.0f, 1.0f);
	fFillProgram.Set("uColor", color.red / 255.0f, color.green / 255.0f,
		color.blue / 255.0f, alpha);
	fFillProgram.Set("uSize", box.width, box.height);
	fFillProgram.Set("uRadius", radius);
	DrawRectangle();
}


void
StageRenderer::_DrawText(const TextTexture& text, float x, float y,
	rgb_color color, float alpha, float scale)
{
	if (!text.IsValid())
		return;
	fTextProgram.Use();
	_SetRect(fTextProgram, { roundf(x), roundf(y), text.Width() * scale,
		text.Height() * scale });
	// The bitmap's first row is its top: flip vertically.
	fTextProgram.Set("uTexRect", 0.0f, 1.0f, 1.0f, 0.0f);
	fTextProgram.Set("uImage", 0);
	fTextProgram.Set("uColor", color.red / 255.0f, color.green / 255.0f,
		color.blue / 255.0f, alpha);
	text.GetTexture().Bind(0);
	DrawRectangle();
}


void
StageRenderer::_DrawLabel(const TextTexture& text, float centerX,
	float bottom, float alpha)
{
	// White text on a dark, rounded plate.
	const float plateHeight = text.Height() + 4;
	const float plateWidth = text.Width() + 16;
	const Box plate = { roundf(centerX - plateWidth / 2),
		roundf(bottom - plateHeight), plateWidth, plateHeight };
	_Fill(plate, kBlack, 0.6f * alpha, plateHeight / 2);
	_DrawText(text, plate.x + 8, plate.y + 2, kWhite, alpha);
}


}	// namespace snap
