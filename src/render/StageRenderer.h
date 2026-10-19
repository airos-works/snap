/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_STAGE_RENDERER_H
#define SNAP_STAGE_RENDERER_H


#include <GraphicsDefs.h>
#include <Locker.h>
#include <Messenger.h>
#include <Point.h>
#include <String.h>

#include <memory>
#include <vector>

#include "CameraHistory.h"
#include "GLUtilities.h"
#include "ShaderLibrary.h"
#include "TextTexture.h"
#include "effects/Effect.h"


namespace snap {


class FrameExchange;
class SoundAnalysis;


// What the window wants on the stage. The window changes it through
// StageView; the render thread takes a copy at the start of every frame.
struct StageState {
			int32		effect;			// index into the EffectCatalog
			bool		grid;			// all effects at once
			bool		mirror;
			bool		placeholder;	// no camera: run the test scene
			int32		hoverCell;		// grid cell under the mouse, or -1
			bigtime_t	countdownStart;	// 0 when not counting down
			int32		countdownLength; // in seconds
			bigtime_t	flashStart;		// 0 when not flashing
			BString		status;			// a line at the bottom, or empty
			int32		photoRequest;	// bumped by one to take a photo
			rgb_color	accent;

						StageState();
};


// Draws Snap's stage with OpenGL: the camera picture through the effects,
// one big or all of them in a grid, and the bits of interface on top of it
// (names, countdown, flash). Lives on the render thread, which must hold the
// GL context for every call except CellAt().
//
// Each frame:
//  1. the newest camera picture (FrameExchange) is uploaded into a mipmapped
//     texture, keeping the one before for motion; with no camera the
//     placeholder scene is drawn instead;
//  2. the sound features (SoundAnalysis) go into a small float texture;
//  3. every effect on screen draws into its own offscreen RenderTarget;
//  4. those are composited onto the view, with the overlays;
//  5. when a photo was asked for, the shown effect's target is read back
//     into a BBitmap and sent to the window as kMsgPhotoTaken.
class StageRenderer {
public:
								StageRenderer(FrameExchange& frames,
									SoundAnalysis& sound,
									const BMessenger& target);
								~StageRenderer();

			status_t			Init(BString& error);
			void				Render(const StageState& state, int width,
									int height);

	// The grid cell at a point of the view, as last drawn; -1 for none.
	// Thread safe.
			int32				CellAt(BPoint point) const;

private:
			struct Box {
				float			x;
				float			y;
				float			width;
				float			height;

				bool			Contains(BPoint point) const;
			};

			struct Slot {
				Effect*			effect;
				RenderTarget	output;
				TextTexture		label;
				bool			ready;		// built without errors
				uint32			lastFrame;	// when it last drew
				bigtime_t		shownSince;
			};

			bool				_UpdateCamera(const StageState& state,
									bigtime_t now);
			void				_UpdateSound(bigtime_t now, float delta);
			void				_Layout(const StageState& state, int width,
									int height);
			void				_RenderEffects(const StageState& state,
									bigtime_t now, float delta,
									bool newPicture);
			void				_TakePhoto(const StageState& state);

			void				_DrawStage(const StageState& state,
									bigtime_t now);
			void				_DrawGrid(const StageState& state);
			void				_DrawOverlays(const StageState& state,
									bigtime_t now);

			void				_SetRect(ShaderProgram& program,
									const Box& box);
			void				_DrawImage(GLuint texture, const Box& box,
									float radius, float borderWidth,
									rgb_color border, float borderAlpha);
			void				_Fill(const Box& box, rgb_color color,
									float alpha, float radius);
			void				_DrawText(const TextTexture& text, float x,
									float y, rgb_color color, float alpha,
									float scale = 1.0f);
			void				_DrawLabel(const TextTexture& text,
									float centerX, float bottom, float alpha);

			FrameExchange&		fFrames;
			SoundAnalysis&		fSound;
			BMessenger			fTarget;

			ShaderLibrary		fShaders;
			GLuint				fVertexArray;
			ShaderProgram		fImageProgram;
			ShaderProgram		fFillProgram;
			ShaderProgram		fTextProgram;
			ShaderProgram		fPlaceholderProgram;

			// Camera pictures: the newest and the one before.
			RenderTarget		fCamera[2];
			int					fCurrentCamera;
			bool				fHasPicture;
			CameraHistory		fHistory;
			bool				fHistoryInUse;

			Texture				fSoundTexture;
			AudioFeatures		fFeatures;
			uint32				fLastBeatCount;
			bigtime_t			fLastBeatTime;
			float				fAudioTime;

			Texture				fGlyphs;
			int32				fGlyphCount;

			std::vector<std::unique_ptr<Slot> > fSlots;
			uint32				fFrameNumber;
			bigtime_t			fStartTime;
			bigtime_t			fLastFrame;
			int32				fPhotoRequest;
			bool				fLiveShaders;	// SNAP_SHADER_DIR is set

			// The layout of the last frame, in view coordinates.
			int					fViewWidth;
			int					fViewHeight;
			Box					fStageBox;
			std::vector<Box>	fCellBoxes;
			mutable BLocker		fLayoutLock;

			// Interface text.
			TextTexture			fStatusText;
			TextTexture			fToastText;
			TextTexture			fCountdownText;
			int32				fToastEffect;
			bigtime_t			fToastStart;
};


}	// namespace snap


#endif	// SNAP_STAGE_RENDERER_H
