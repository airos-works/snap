/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "Settings.h"

#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>


namespace snap {


static const char* kSettingsFolder = "Snap";
static const char* kSettingsFile = "settings";


Settings::Settings()
	:
	effect("normal"),
	mirror(true),
	countdown(true),
	flash(true),
	soundSource(kSoundMicrophone),
	windowFrame()
{
}


status_t
Settings::_Path(BString& result, bool create)
{
	BPath path;
	status_t status = find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	if (status != B_OK)
		return status;
	path.Append(kSettingsFolder);
	if (create) {
		status = create_directory(path.Path(), 0755);
		if (status != B_OK)
			return status;
	}
	path.Append(kSettingsFile);
	result = path.Path();
	return B_OK;
}


status_t
Settings::Load()
{
	BString path;
	status_t status = _Path(path, false);
	if (status != B_OK)
		return status;

	BFile file(path.String(), B_READ_ONLY);
	BMessage archive;
	status = file.InitCheck();
	if (status == B_OK)
		status = archive.Unflatten(&file);
	if (status != B_OK)
		return status;

	// Each field keeps its default when the file does not have it.
	effect = archive.GetString("effect", effect.String());
	mirror = archive.GetBool("mirror", mirror);
	countdown = archive.GetBool("countdown", countdown);
	flash = archive.GetBool("flash", flash);
	const int32 source = archive.GetInt32("sound source", soundSource);
	if (source >= kSoundOff && source <= kSoundMusic)
		soundSource = (SoundSource)source;
	musicPath = archive.GetString("music path", musicPath.String());
	camera = archive.GetString("camera", camera.String());
	microphone = archive.GetString("microphone", microphone.String());
	windowFrame = archive.GetRect("window frame", windowFrame);
	return B_OK;
}


status_t
Settings::Save() const
{
	BMessage archive('SnpS');
	archive.AddString("effect", effect);
	archive.AddBool("mirror", mirror);
	archive.AddBool("countdown", countdown);
	archive.AddBool("flash", flash);
	archive.AddInt32("sound source", soundSource);
	archive.AddString("music path", musicPath);
	archive.AddString("camera", camera);
	archive.AddString("microphone", microphone);
	if (windowFrame.IsValid())
		archive.AddRect("window frame", windowFrame);

	BString path;
	status_t status = _Path(path, true);
	if (status != B_OK)
		return status;
	BFile file(path.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	status = file.InitCheck();
	if (status != B_OK)
		return status;
	return archive.Flatten(&file);
}


}	// namespace snap
