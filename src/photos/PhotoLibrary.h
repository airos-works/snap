/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_PHOTO_LIBRARY_H
#define SNAP_PHOTO_LIBRARY_H


#include <Looper.h>
#include <Messenger.h>
#include <Path.h>


class BBitmap;
class BNode;
struct entry_ref;


namespace snap {


// The photos: plain PNG files in ~/Pictures/Snap, so Tracker, ShowImage and
// every other program can use them. Snap keeps no database of its own; what
// it knows about a photo is in the file's attributes, where Tracker shows it
// and queries find it:
//
//   BEOS:TYPE                 image/png
//   Media:Width, Media:Height the picture's size (int32)
//   Media:Thumbnail           a 128 pixel WebP preview, the attribute Tracker
//                             itself writes and shows as the file's icon
//   Snap:Effect               the effect the photo was taken with
//
// Encoding a PNG takes a moment, so the library is a BLooper: saving and
// thumbnail loading happen on its thread, and it answers with messages.
class PhotoLibrary : public BLooper {
public:
								PhotoLibrary(const BMessenger& window);

	// ~/Pictures/Snap (created when asked to).
	static	status_t			Folder(BPath& path, bool create);

	// Moves a photo to the Trash of its volume the way Tracker does, so
	// Tracker's "Restore" puts it back.
	static	status_t			MoveToTrash(const entry_ref& ref);

	virtual	void				MessageReceived(BMessage* message);

private:
			void				_Save(BMessage* message);
			void				_LoadThumbnail(BMessage* message);
			status_t			_WriteFile(BBitmap* bitmap, const char* effect,
									entry_ref& ref, BString& error);
	static	void				_WriteThumbnail(BNode& node,
									const BBitmap* picture);

			BMessenger			fWindow;
};


// A copy of a bitmap scaled to fit in maxSize x maxSize, drawn by app_server
// with bilinear filtering. Used for thumbnails.
BBitmap* ScaleBitmap(const BBitmap* source, float maxSize);


}	// namespace snap


#endif	// SNAP_PHOTO_LIBRARY_H
