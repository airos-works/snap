/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */

// The effects themselves. Most are a single fragment shader; Set on Fire and
// Echo keep a picture of their own from frame to frame; Time Warp reads the
// camera's past and Text Mode a font. The looks are all in the GLSL files in
// shaders/; this file only wires them up.


#include "Effect.h"

#include <algorithm>
#include <cstring>

#include "render/CameraHistory.h"
#include "render/GLUtilities.h"
#include "render/ShaderLibrary.h"


namespace snap {


// #pragma mark - EffectContext


void
EffectContext::Apply(ShaderProgram& program, int width, int height) const
{
	program.Set("uRect", -1.0f, -1.0f, 1.0f, 1.0f);
	program.Set("uTexRect", 0.0f, 0.0f, 1.0f, 1.0f);
	program.Set("uCamera", kCameraTextureUnit);
	program.Set("uPrevious", kPreviousTextureUnit);
	program.Set("uAudio", kSoundTextureUnit);
	program.Set("uResolution", (float)width, (float)height);
	program.Set("uCameraSize", (float)cameraWidth, (float)cameraHeight);
	program.Set("uTime", time);
	program.Set("uEffectTime", effectTime);
	program.Set("uDelta", delta);
	program.Set("uMirror", mirror ? 1.0f : 0.0f);
	program.Set("uLevel", sound.level);
	program.Set("uBass", sound.bass);
	program.Set("uMid", sound.mid);
	program.Set("uTreble", sound.treble);
	program.Set("uBeat", sound.beat);
	program.Set("uBeatAge", beatAge);
	program.Set("uBeatCount", (float)(sound.beatCount % 1024));
	program.Set("uAudioTime", audioTime);
}


void
DrawRectangle()
{
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}


// #pragma mark - Effect


Effect::Effect(const EffectInfo& info)
	:
	fInfo(info)
{
}


Effect::~Effect()
{
}


// #pragma mark - ShaderEffect


namespace {


// An effect that is one fragment shader run over the output.
class ShaderEffect : public Effect {
public:
	ShaderEffect(const EffectInfo& info, const char* shader)
		:
		Effect(info),
		fShader(shader)
	{
	}

	status_t Prepare(ShaderLibrary& shaders, BString& error) override
	{
		return shaders.Build(fProgram, fShader.String(), error);
	}

	void Render(const EffectContext& context, RenderTarget& output) override
	{
		output.Bind();
		fProgram.Use();
		context.Apply(fProgram, output.Width(), output.Height());
		SetUniforms(context, output);
		DrawRectangle();
	}

protected:
	// For effects with inputs of their own.
	virtual void SetUniforms(const EffectContext& context,
		RenderTarget& output)
	{
	}

	ShaderProgram		fProgram;
	BString				fShader;
};


// #pragma mark - FeedbackEffect


// An effect with a memory: an update shader draws the new state from the
// old one (both kept in a PingPongTarget, at a fraction of the output size),
// then a display shader draws the output from the state and the camera.
class FeedbackEffect : public Effect {
public:
	FeedbackEffect(const EffectInfo& info, const char* update,
		const char* display, float scale, GLenum stateFormat)
		:
		Effect(info),
		fUpdateShader(update),
		fDisplayShader(display),
		fScale(scale),
		fStateFormat(stateFormat),
		fClear(true)
	{
	}

	status_t Prepare(ShaderLibrary& shaders, BString& error) override
	{
		status_t status = shaders.Build(fUpdate, fUpdateShader, error);
		if (status == B_OK)
			status = shaders.Build(fDisplay, fDisplayShader, error);
		return status;
	}

	void Reset() override
	{
		fClear = true;
	}

	void Render(const EffectContext& context, RenderTarget& output) override
	{
		const int width = std::max(16, (int)(output.Width() * fScale));
		const int height = std::max(16, (int)(output.Height() * fScale));
		if (fState.Resize(width, height, fStateFormat))
			fClear = false;
		if (fClear) {
			for (int i = 0; i < 2; i++) {
				fState.Next().Bind();
				glClearColor(0, 0, 0, 0);
				glClear(GL_COLOR_BUFFER_BIT);
				fState.Swap();
			}
			fClear = false;
		}

		// The new state from the previous one.
		fState.Next().Bind();
		fUpdate.Use();
		context.Apply(fUpdate, width, height);
		fState.Previous().BindTexture(kEffectTextureUnit);
		fUpdate.Set("uState", kEffectTextureUnit);
		fUpdate.Set("uStateSize", (float)width, (float)height);
		DrawRectangle();

		// The picture from the new state.
		output.Bind();
		fDisplay.Use();
		context.Apply(fDisplay, output.Width(), output.Height());
		fState.Next().BindTexture(kEffectTextureUnit);
		fDisplay.Set("uState", kEffectTextureUnit);
		fDisplay.Set("uStateSize", (float)width, (float)height);
		DrawRectangle();

		fState.Swap();
	}

private:
	const char*			fUpdateShader;
	const char*			fDisplayShader;
	float				fScale;
	GLenum				fStateFormat;
	bool				fClear;
	ShaderProgram		fUpdate;
	ShaderProgram		fDisplay;
	PingPongTarget		fState;
};


// #pragma mark - TimeWarpEffect


class TimeWarpEffect : public ShaderEffect {
public:
	TimeWarpEffect(const EffectInfo& info)
		:
		ShaderEffect(info, "timewarp.glsl")
	{
	}

	bool NeedsHistory() const override
	{
		return true;
	}

protected:
	void SetUniforms(const EffectContext& context, RenderTarget& output)
		override
	{
		CameraHistory* history = context.history;
		if (history == NULL)
			return;
		history->Bind(kEffectTextureUnit);
		fProgram.Set("uHistory", kEffectTextureUnit);
		fProgram.Set("uHistoryLayers", (float)CameraHistory::kLayers);
		fProgram.Set("uHistoryNewest", (float)history->Newest());
		fProgram.Set("uHistoryCount", (float)std::max(1, history->Count()));
	}
};


// #pragma mark - TextModeEffect


class TextModeEffect : public ShaderEffect {
public:
	TextModeEffect(const EffectInfo& info)
		:
		ShaderEffect(info, "textmode.glsl")
	{
	}

protected:
	void SetUniforms(const EffectContext& context, RenderTarget& output)
		override
	{
		if (context.glyphs == NULL)
			return;
		context.glyphs->Bind(kEffectTextureUnit);
		fProgram.Set("uGlyphs", kEffectTextureUnit);
		fProgram.Set("uGlyphCount", (float)context.glyphCount);

		// About 40 rows of text on the big stage, at least 16 in a grid cell.
		const float rows = std::clamp(output.Height() / 18.0f, 16.0f, 48.0f);
		const float cellHeight = output.Height() / rows;
		const float glyphAspect = context.glyphs->Width()
			/ (float)std::max(1, context.glyphCount)
			/ std::max(1, context.glyphs->Height());
		fProgram.Set("uCellSize", cellHeight * glyphAspect, cellHeight);
	}
};


}	// namespace


// #pragma mark -


Effect*
CreateEffect(int32 index)
{
	const EffectInfo& info = EffectAt(index);
	const char* id = info.id;

	if (strcmp(id, "fire") == 0) {
		// Heat needs more than 8 bits to cool smoothly; half resolution is
		// plenty for flames and halves the simulation's cost.
		return new FeedbackEffect(info, "fire-simulate.glsl", "fire.glsl",
			0.5f, GL_RGBA16F);
	}
	if (strcmp(id, "echo") == 0) {
		return new FeedbackEffect(info, "echo-update.glsl", "echo.glsl", 1.0f,
			GL_RGBA8);
	}
	if (strcmp(id, "timewarp") == 0)
		return new TimeWarpEffect(info);
	if (strcmp(id, "textmode") == 0)
		return new TextModeEffect(info);

	// The others are one shader each, named after the effect.
	BString shader(id);
	shader << ".glsl";
	return new ShaderEffect(info, shader.String());
}


}	// namespace snap
