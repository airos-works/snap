/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_PHOTO_STRIP_H
#define SNAP_PHOTO_STRIP_H


#include <Entry.h>
#include <Node.h>
#include <View.h>

#include <vector>


class BBitmap;
class BLooper;
class BPopUpMenu;


namespace snap {


// The row of photos under the stage, newest first.
//
// It shows what is in the photo folder, whoever put it there: the folder is
// watched with the node monitor (watch_node() with B_WATCH_DIRECTORY), so a
// photo deleted or renamed in Tracker goes away here as well. Thumbnails are
// loaded by the PhotoLibrary's thread and arrive as messages.
//
// A photo can be dragged out (to Tracker, the Desktop, or any program that
// takes files: the drag message carries its entry_ref, as Tracker's do),
// opened with a double click, or handled from its context menu.
class PhotoStrip : public BView {
public:
								PhotoStrip(BLooper* library);
	virtual						~PhotoStrip();

	virtual	void				AttachedToWindow();
	virtual	void				DetachedFromWindow();
	virtual	void				Draw(BRect updateRect);
	virtual	void				FrameResized(float width, float height);
	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseUp(BPoint where);
	virtual	void				MouseMoved(BPoint where, uint32 transit,
									const BMessage* dragMessage);
	virtual	void				KeyDown(const char* bytes, int32 count);
	virtual	void				MessageReceived(BMessage* message);
	virtual	BSize				MinSize();
	virtual	BSize				MaxSize();
	virtual	BSize				PreferredSize();

			int32				CountPhotos() const { return fItems.size(); }

	// A photo Snap just saved: shown at once and selected (the node monitor
	// reports the file before it is complete).
			void				PhotoAdded(const entry_ref& ref);

private:
			struct Item {
				entry_ref		ref;
				node_ref		node;
				time_t			created;
				BBitmap*		thumbnail;
				bool			requested;
			};

			void				_Rescan();
	static	bool				_IsPicture(const BEntry& entry);
			void				_RequestThumbnails();
			BRect				_ItemFrame(int32 index) const;
			int32				_ItemAt(BPoint where) const;
			void				_UpdateScrollBar();
			void				_Select(int32 index);
			void				_ShowContextMenu(int32 index, BPoint where);
			void				_Open(int32 index);
			void				_ShowInTracker(int32 index);
			void				_Trash(int32 index);
			float				_ThumbnailHeight() const;

			BLooper*			fLibrary;
			std::vector<Item>	fItems;
			node_ref			fFolder;
			bool				fWatching;
			int32				fSelected;
			int32				fPressed;
			BPoint				fPressPoint;
			bool				fRescanPending;
};


}	// namespace snap


#endif	// SNAP_PHOTO_STRIP_H
