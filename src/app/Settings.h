/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_SETTINGS_H
#define SNAP_SETTINGS_H


#include <Rect.h>
#include <String.h>


namespace snap {


enum SoundSource {
	kSoundOff = 0,
	kSoundMicrophone,
	kSoundMusic
};


// What Snap remembers between runs. Stored the Haiku way: a flattened
// BMessage in ~/config/settings/Snap/settings, so every field is named and
// typed, a missing field falls back to its default, and older or newer
// versions of Snap read each other's files.
class Settings {
public:
								Settings();

			status_t			Load();
			status_t			Save() const;

			BString				effect;			// EffectInfo::id of the live effect
			bool				mirror;			// show the camera like a mirror
			bool				countdown;		// 3-2-1 before a photo
			bool				flash;			// white screen flash and shutter sound
			SoundSource			soundSource;
			BString				musicPath;		// last soundtrack played
			BString				camera;			// name of the chosen camera node
			BString				microphone;		// name of the chosen sound input
			BRect				windowFrame;	// invalid until the window moved

private:
	static	status_t			_Path(BString& path, bool create);
};


}	// namespace snap


#endif	// SNAP_SETTINGS_H
