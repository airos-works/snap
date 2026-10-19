/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "PhotoLibrary.h"

#include <Bitmap.h>
#include <BitmapStream.h>
#include <DataIO.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <NodeInfo.h>
#include <TranslationUtils.h>
#include <TranslatorFormats.h>
#include <TranslatorRoster.h>
#include <View.h>
#include <Volume.h>
#include <fs_attr.h>

#include <algorithm>
#include <ctime>

#include "app/Messages.h"


namespace snap {


static const char* kPicturesFolder = "Pictures";
static const char* kSnapFolder = "Snap";

// Tracker's names for these (see Tracker's Attributes.h).
static const char* kAttrThumbnail = "Media:Thumbnail";
static const char* kAttrThumbnailCreationTime = "Media:Thumbnail:CreationTime";
static const char* kAttrOriginalPath = "_trk/original_path";
static const char* kAttrEffect = "Snap:Effect";

static const float kThumbnailSize = 128;


BBitmap*
ScaleBitmap(const BBitmap* source, float maxSize)
{
	const BRect bounds = source->Bounds();
	const float scale = std::min(1.0f, maxSize
		/ std::max(bounds.Width() + 1, bounds.Height() + 1));
	const BRect target(0, 0, roundf((bounds.Width() + 1) * scale) - 1,
		roundf((bounds.Height() + 1) * scale) - 1);

	BBitmap* scaled = new BBitmap(target, B_BITMAP_ACCEPTS_VIEWS, B_RGB32);
	BView* view = new BView(target, "scaler", B_FOLLOW_NONE, 0);
	scaled->AddChild(view);
	scaled->Lock();
	view->DrawBitmap(source, bounds, target, B_FILTER_BITMAP_BILINEAR);
	view->Sync();
	scaled->RemoveChild(view);
	scaled->Unlock();
	delete view;
	return scaled;
}


PhotoLibrary::PhotoLibrary(const BMessenger& window)
	:
	BLooper("Snap photo library", B_LOW_PRIORITY),
	fWindow(window)
{
}


status_t
PhotoLibrary::Folder(BPath& path, bool create)
{
	status_t status = find_directory(B_USER_DIRECTORY, &path);
	if (status != B_OK)
		return status;
	path.Append(kPicturesFolder);
	path.Append(kSnapFolder);
	if (create)
		return create_directory(path.Path(), 0755);
	return B_OK;
}


void
PhotoLibrary::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgSavePhoto:
			_Save(message);
			break;
		case kMsgLoadThumbnail:
			_LoadThumbnail(message);
			break;
		default:
			BLooper::MessageReceived(message);
			break;
	}
}


void
PhotoLibrary::_Save(BMessage* message)
{
	BBitmap* bitmap = NULL;
	if (message->FindPointer("bitmap", (void**)&bitmap) != B_OK
		|| bitmap == NULL) {
		return;
	}
	const char* effect = message->GetString("effect", "");

	entry_ref ref;
	BString error;
	const status_t status = _WriteFile(bitmap, effect, ref, error);
	delete bitmap;

	if (status == B_OK) {
		BMessage saved(kMsgPhotoSaved);
		saved.AddRef("refs", &ref);
		saved.AddInt32("id", message->GetInt32("id", 0));
		fWindow.SendMessage(&saved);
	} else {
		BMessage failed(kMsgPhotoFailed);
		failed.AddString("error", error);
		failed.AddInt32("id", message->GetInt32("id", 0));
		fWindow.SendMessage(&failed);
	}
}


status_t
PhotoLibrary::_WriteFile(BBitmap* bitmap, const char* effect, entry_ref& ref,
	BString& error)
{
	BPath folder;
	status_t status = Folder(folder, true);
	if (status != B_OK) {
		error << "The folder for photos cannot be created: " << strerror(status);
		return status;
	}

	// "Snap 2026-10-19 17.42.05.png", numbered if two are taken in a second.
	char stamp[64];
	const time_t now = time(NULL);
	strftime(stamp, sizeof(stamp), "%Y-%m-%d %H.%M.%S", localtime(&now));
	BDirectory directory(folder.Path());
	BString name;
	BFile file;
	for (int32 attempt = 1; attempt < 100; attempt++) {
		name.SetToFormat(attempt == 1 ? "Snap %s.png" : "Snap %s (%d).png", stamp,
			(int)attempt);
		status = file.SetTo(&directory, name.String(),
			B_WRITE_ONLY | B_CREATE_FILE | B_FAIL_IF_EXISTS);
		if (status != B_FILE_EXISTS)
			break;
	}
	if (status != B_OK) {
		error << "The photo cannot be saved: " << strerror(status);
		return status;
	}

	// The Translation Kit picks the PNG translator; a BBitmapStream presents
	// the bitmap in the kit's own picture format. The stream would delete
	// the bitmap, so it is detached again afterwards.
	BBitmapStream stream(bitmap);
	status = BTranslatorRoster::Default()->Translate(&stream, NULL, NULL,
		&file, B_PNG_FORMAT);
	BBitmap* detached;
	stream.DetachBitmap(&detached);
	if (status != B_OK) {
		error << "The photo cannot be encoded as PNG: " << strerror(status);
		BEntry(&directory, name.String()).Remove();
		return status;
	}

	BNodeInfo(&file).SetType("image/png");
	const BRect bounds = bitmap->Bounds();
	const int32 width = bounds.IntegerWidth() + 1;
	const int32 height = bounds.IntegerHeight() + 1;
	file.WriteAttr("Media:Width", B_INT32_TYPE, 0, &width, sizeof(width));
	file.WriteAttr("Media:Height", B_INT32_TYPE, 0, &height, sizeof(height));
	const BString effectName(effect);
	file.WriteAttrString(kAttrEffect, &effectName);
	_WriteThumbnail(file, bitmap);
	file.Unset();

	BEntry entry(&directory, name.String());
	return entry.GetRef(&ref);
}


void
PhotoLibrary::_WriteThumbnail(BNode& node, const BBitmap* picture)
{
	// What Tracker stores for an image it has shown once: a WebP of at most
	// 128 x 128 pixels and when it was made.
	BBitmap* thumbnail = ScaleBitmap(picture, kThumbnailSize);
	BBitmapStream stream(thumbnail);
	BMallocIO webp;
	if (BTranslatorRoster::Default()->Translate(&stream, NULL, NULL, &webp,
			B_WEBP_FORMAT) == B_OK) {
		node.WriteAttr(kAttrThumbnail, B_RAW_TYPE, 0, webp.Buffer(),
			webp.BufferLength());
		// Tracker keeps a thumbnail only when it is newer than the file
		// (in whole seconds), and the file was written this very second.
		const int64 created = real_time_clock() + 1;
		node.WriteAttr(kAttrThumbnailCreationTime, B_TIME_TYPE, 0, &created,
			sizeof(created));
	}
	// The stream deletes the bitmap it still holds.
}


void
PhotoLibrary::_LoadThumbnail(BMessage* message)
{
	BMessenger target;
	entry_ref ref;
	if (message->FindRef("refs", &ref) != B_OK
		|| message->FindMessenger("target", &target) != B_OK) {
		return;
	}

	// The thumbnail attribute when there is one, else the picture itself
	// (which then gets the attribute for next time).
	BBitmap* thumbnail = NULL;
	BNode node(&ref);
	attr_info info;
	if (node.GetAttrInfo(kAttrThumbnail, &info) == B_OK && info.size > 0
		&& info.size < 1024 * 1024) {
		BMallocIO data;
		data.SetSize(info.size);
		if (node.ReadAttr(kAttrThumbnail, info.type, 0, (void*)data.Buffer(),
				info.size) == info.size) {
			thumbnail = BTranslationUtils::GetBitmap(&data);
		}
	}
	if (thumbnail == NULL) {
		BBitmap* picture = BTranslationUtils::GetBitmap(&ref);
		if (picture != NULL) {
			thumbnail = ScaleBitmap(picture, kThumbnailSize);
			_WriteThumbnail(node, picture);
			delete picture;
		}
	}

	BMessage reply(kMsgThumbnailLoaded);
	reply.AddRef("refs", &ref);
	reply.AddPointer("bitmap", thumbnail);
	if (target.SendMessage(&reply) != B_OK)
		delete thumbnail;
}


status_t
PhotoLibrary::MoveToTrash(const entry_ref& ref)
{
	BEntry entry(&ref);
	BPath original;
	status_t status = entry.GetPath(&original);
	if (status != B_OK)
		return status;

	BVolume volume(ref.device);
	BPath trashPath;
	status = find_directory(B_TRASH_DIRECTORY, &trashPath, true, &volume);
	if (status != B_OK)
		return status;
	BDirectory trash(trashPath.Path());

	// Tracker restores a file to where this attribute says it came from.
	BNode node(&entry);
	node.WriteAttr(kAttrOriginalPath, B_STRING_TYPE, 0, original.Path(),
		strlen(original.Path()) + 1);
	node.Unset();

	BString name(ref.name);
	for (int32 copy = 2; trash.Contains(name.String()); copy++)
		name.SetToFormat("%s %d", ref.name, (int)copy);
	return entry.MoveTo(&trash, name.String());
}


}	// namespace snap
