/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_WINDOW_H
#define SNAP_WINDOW_H


#include <Window.h>

#include <vector>

#include "Settings.h"
#include "capture/MediaNodes.h"


class BButton;
class BFilePanel;
class BMenu;
class BMenuField;
class BMenuBar;
class BMenuItem;
class BMessageRunner;
class BPopUpMenu;
class BStringView;
struct entry_ref;


namespace snap {


class CameraSource;
class FrameExchange;
class LevelMeter;
class MicrophoneInput;
class MusicPlayer;
class PhotoLibrary;
class PhotoStrip;
class ShutterButton;
class SoundAnalysis;
class SoundEffects;
class StageView;


// Snap's one window: the stage on top, the controls below it, the photos
// at the bottom. It owns the camera, the sound sources and the photo
// library, and connects them: the camera and the sound feed the stage, the
// stage's pictures go to the library, the library's files appear in the
// strip.
//
// Everything the menus and buttons do can also be done by scripting (see
// kProperties in the .cpp file), for example from a Terminal:
//
//   hey Snap set Effect to "Set on Fire"
//   hey Snap do Photo
class SnapWindow : public BWindow {
public:
								SnapWindow(Settings& settings,
									const BString& renderer);
	virtual						~SnapWindow();

	virtual	bool				QuitRequested();
	virtual	void				MessageReceived(BMessage* message);
	virtual	void				MenusBeginning();
	virtual	void				Minimize(bool minimize);

	virtual	BHandler*			ResolveSpecifier(BMessage* message,
									int32 index, BMessage* specifier,
									int32 what, const char* property);
	virtual	status_t			GetSupportedSuites(BMessage* data);

	// Snap's own scripting properties, which the application passes on.
	static	bool				HasProperty(const char* property);

			void				PlayMusic(const entry_ref& ref);

private:
			BMenuBar*			_BuildMenuBar();
			BView*				_BuildControls();

			void				_SelectEffect(int32 index);
			void				_SetGrid(bool grid);
			void				_TakePhoto();
			void				_CountdownTick();
			void				_Capture();
			void				_PhotoTaken(BMessage* message);
			void				_PhotoSaved(BMessage* message);
			void				_PhotoFailed(const char* error);

			void				_StartCamera();
			void				_CamerasChanged(BMessage* message);
			void				_UpdateCameraMenu();
			void				_CameraStatus(const char* status,
									bool placeholder);
			void				_UpdateStageStatus();

			void				_SetSoundSource(SoundSource source,
									const entry_ref* music = NULL);
			void				_ChooseMusic();
			void				_UpdateSoundControls();

			bool				_HandleScripting(BMessage* message);

			Settings&			fSettings;
			BString				fRenderer;

			FrameExchange*		fFrames;
			SoundAnalysis*		fSound;
			CameraSource*		fCamera;
			MicrophoneInput*	fMicrophone;
			MusicPlayer*		fMusic;
			SoundEffects*		fSoundEffects;
			PhotoLibrary*		fLibrary;

			StageView*			fStage;
			PhotoStrip*			fStrip;
			BButton*			fEffectsButton;
			BStringView*		fEffectName;
			ShutterButton*		fShutter;
			BPopUpMenu*			fSoundPopUp;
			BMenuField*			fSoundField;
			LevelMeter*			fMeter;

			BMenu*				fEffectsMenu;
			BMenuItem*			fGridItem;
			BMenu*				fCameraMenu;
			BMenuItem*			fMirrorItem;
			BMenuItem*			fCountdownItem;
			BMenuItem*			fFlashItem;
			BMenu*				fSoundMenu;

			BFilePanel*			fMusicPanel;
			BMessageRunner*		fCountdownRunner;
			int32				fCountdownLeft;
			int32				fEffect;
			bool				fGrid;
			std::vector<CameraInfo> fCameras;
			bool				fCameraRunning;
			bool				fWatchingMedia;
			BString				fCameraStatus;
			BString				fSoundStatus;

			// "do Photo" scripting requests, answered once the file exists.
			std::vector<BMessage*> fPhotoReplies;
};


}	// namespace snap


#endif	// SNAP_WINDOW_H
