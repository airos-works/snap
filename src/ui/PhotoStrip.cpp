/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "PhotoStrip.h"

#include <Bitmap.h>
#include <Directory.h>
#include <Looper.h>
#include <MenuItem.h>
#include <NodeInfo.h>
#include <NodeMonitor.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <Roster.h>
#include <ScrollBar.h>
#include <Window.h>

#include <algorithm>
#include <cmath>

#include "app/Messages.h"
#include "photos/PhotoLibrary.h"


namespace snap {


static const float kPadding = 8;
static const float kGap = 8;
static const float kRadius = 4;
static const char* kTrackerSignature = "application/x-vnd.Be-TRAK";


PhotoStrip::PhotoStrip(BLooper* library)
	:
	BView("photo strip", B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE),
	fLibrary(library),
	fWatching(false),
	fSelected(-1),
	fPressed(-1),
	fRescanPending(false)
{
}


PhotoStrip::~PhotoStrip()
{
	for (Item& item : fItems)
		delete item.thumbnail;
}


void
PhotoStrip::AttachedToWindow()
{
	BView::AttachedToWindow();
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR, B_DARKEN_1_TINT);

	// Watch the folder: entries created, removed or renamed in it arrive as
	// B_NODE_MONITOR messages.
	BPath path;
	if (PhotoLibrary::Folder(path, true) == B_OK) {
		BDirectory folder(path.Path());
		if (folder.GetNodeRef(&fFolder) == B_OK
			&& watch_node(&fFolder, B_WATCH_DIRECTORY, this) == B_OK) {
			fWatching = true;
		}
	}
	_Rescan();
}


void
PhotoStrip::DetachedFromWindow()
{
	if (fWatching)
		stop_watching(this);
	fWatching = false;
	BView::DetachedFromWindow();
}


float
PhotoStrip::_ThumbnailHeight() const
{
	// Follows the font size, which is how Haiku scales its interface.
	return roundf(72 * std::max(1.0f, be_plain_font->Size() / 12.0f));
}


BSize
PhotoStrip::MinSize()
{
	return BSize(120, _ThumbnailHeight() + 2 * kPadding);
}


BSize
PhotoStrip::MaxSize()
{
	return BSize(B_SIZE_UNLIMITED, _ThumbnailHeight() + 2 * kPadding);
}


BSize
PhotoStrip::PreferredSize()
{
	return BSize(600, _ThumbnailHeight() + 2 * kPadding);
}


BRect
PhotoStrip::_ItemFrame(int32 index) const
{
	const float height = _ThumbnailHeight();
	const float width = roundf(height * 16 / 9);
	const float left = kPadding + index * (width + kGap);
	return BRect(left, kPadding, left + width - 1, kPadding + height - 1);
}


int32
PhotoStrip::_ItemAt(BPoint where) const
{
	for (int32 i = 0; i < (int32)fItems.size(); i++) {
		if (_ItemFrame(i).Contains(where))
			return i;
	}
	return -1;
}


void
PhotoStrip::Draw(BRect updateRect)
{
	if (fItems.empty()) {
		const char* text = "Photos you take appear here.";
		font_height height;
		GetFontHeight(&height);
		SetHighUIColor(B_PANEL_TEXT_COLOR, B_LIGHTEN_1_TINT);
		const BRect bounds = Bounds();
		DrawString(text, BPoint(bounds.left + (bounds.Width() - StringWidth(text)) / 2,
			bounds.top + (bounds.Height() + height.ascent - height.descent) / 2));
		return;
	}

	for (int32 i = 0; i < (int32)fItems.size(); i++) {
		const BRect frame = _ItemFrame(i);
		if (!frame.Intersects(updateRect))
			continue;
		const Item& item = fItems[i];

		SetHighColor(0, 0, 0);
		FillRoundRect(frame, kRadius, kRadius);
		if (item.thumbnail != NULL) {
			// Fit the thumbnail into the frame, keeping its shape.
			const BRect source = item.thumbnail->Bounds();
			const float scale = std::min(frame.Width() / source.Width(),
				frame.Height() / source.Height());
			const float width = source.Width() * scale;
			const float height = source.Height() * scale;
			BRect target(0, 0, width, height);
			target.OffsetTo(frame.left + (frame.Width() - width) / 2,
				frame.top + (frame.Height() - height) / 2);
			DrawBitmap(item.thumbnail, source, target, B_FILTER_BITMAP_BILINEAR);
		}

		if (i == fSelected) {
			SetHighUIColor(B_CONTROL_HIGHLIGHT_COLOR);
			SetPenSize(2);
			StrokeRoundRect(frame.InsetByCopy(1, 1), kRadius, kRadius);
			SetPenSize(1);
		}
	}
}


void
PhotoStrip::FrameResized(float width, float height)
{
	BView::FrameResized(width, height);
	_UpdateScrollBar();
}


void
PhotoStrip::_UpdateScrollBar()
{
	BScrollBar* scrollBar = ScrollBar(B_HORIZONTAL);
	if (scrollBar == NULL)
		return;
	const float content = fItems.empty() ? 0
		: _ItemFrame(fItems.size() - 1).right + kPadding;
	const float visible = Bounds().Width();
	scrollBar->SetRange(0, std::max(0.0f, content - visible));
	scrollBar->SetProportion(content > 0 ? std::min(1.0f, visible / content) : 1);
	scrollBar->SetSteps(_ItemFrame(0).Width() / 2, visible);
}


// #pragma mark - The folder


void
PhotoStrip::_Rescan()
{
	fRescanPending = false;
	BPath path;
	if (PhotoLibrary::Folder(path, false) != B_OK)
		return;
	BDirectory folder(path.Path());

	entry_ref selected;
	const bool hadSelection = fSelected >= 0 && fSelected < (int32)fItems.size();
	if (hadSelection)
		selected = fItems[fSelected].ref;

	// What is there now; thumbnails of photos still there are kept.
	std::vector<Item> items;
	BEntry entry;
	while (folder.GetNextEntry(&entry) == B_OK) {
		if (!_IsPicture(entry))
			continue;
		Item item;
		entry.GetRef(&item.ref);
		entry.GetNodeRef(&item.node);
		entry.GetCreationTime(&item.created);
		item.thumbnail = NULL;
		item.requested = false;
		for (Item& old : fItems) {
			if (old.node == item.node) {
				item.thumbnail = old.thumbnail;
				item.requested = old.requested;
				old.thumbnail = NULL;
				break;
			}
		}
		items.push_back(item);
	}
	for (Item& old : fItems)
		delete old.thumbnail;

	// Newest first.
	std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
		if (a.created != b.created)
			return a.created > b.created;
		return strcmp(a.ref.name, b.ref.name) > 0;
	});
	fItems.swap(items);

	fSelected = -1;
	for (int32 i = 0; hadSelection && i < (int32)fItems.size(); i++) {
		if (fItems[i].ref == selected)
			fSelected = i;
	}
	_UpdateScrollBar();
	_RequestThumbnails();
	Invalidate();
}


bool
PhotoStrip::_IsPicture(const BEntry& entry)
{
	// By its type, or by its name while it has none yet (a file being
	// written gets its type last).
	BNode node(&entry);
	char type[B_MIME_TYPE_LENGTH];
	if (BNodeInfo(&node).GetType(type) == B_OK)
		return strncmp(type, "image/", 6) == 0;
	char name[B_FILE_NAME_LENGTH];
	entry.GetName(name);
	const BString extension = BString(name).ToLower();
	return extension.EndsWith(".png") || extension.EndsWith(".jpg")
		|| extension.EndsWith(".jpeg");
}


void
PhotoStrip::PhotoAdded(const entry_ref& ref)
{
	_Rescan();
	for (int32 i = 0; i < (int32)fItems.size(); i++) {
		if (fItems[i].ref == ref) {
			// Its thumbnail was asked for before the file had one.
			if (fItems[i].thumbnail == NULL)
				fItems[i].requested = false;
			_Select(i);
			break;
		}
	}
	_RequestThumbnails();
}


void
PhotoStrip::_RequestThumbnails()
{
	for (Item& item : fItems) {
		if (item.thumbnail != NULL || item.requested)
			continue;
		BMessage request(kMsgLoadThumbnail);
		request.AddRef("refs", &item.ref);
		request.AddMessenger("target", BMessenger(this));
		fLibrary->PostMessage(&request);
		item.requested = true;
	}
}


void
PhotoStrip::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case B_NODE_MONITOR:
			// A burst of changes (a photo's file being written) becomes one
			// rescan.
			if (!fRescanPending) {
				fRescanPending = true;
				Looper()->PostMessage(kMsgPhotoFolderChanged, this);
			}
			break;

		case kMsgPhotoFolderChanged:
			_Rescan();
			break;

		case kMsgThumbnailLoaded:
		{
			entry_ref ref;
			BBitmap* bitmap = NULL;
			message->FindRef("refs", &ref);
			message->FindPointer("bitmap", (void**)&bitmap);
			for (int32 i = 0; i < (int32)fItems.size(); i++) {
				if (fItems[i].ref == ref) {
					delete fItems[i].thumbnail;
					fItems[i].thumbnail = bitmap;
					bitmap = NULL;
					Invalidate(_ItemFrame(i));
					break;
				}
			}
			delete bitmap;
			break;
		}

		case B_MOUSE_WHEEL_CHANGED:
		{
			// Either wheel direction scrolls along the strip.
			float deltaX = message->GetFloat("be:wheel_delta_x", 0);
			float deltaY = message->GetFloat("be:wheel_delta_y", 0);
			BScrollBar* scrollBar = ScrollBar(B_HORIZONTAL);
			if (scrollBar != NULL) {
				float small;
				float large;
				scrollBar->GetSteps(&small, &large);
				scrollBar->SetValue(scrollBar->Value()
					+ (deltaX + deltaY) * small);
			}
			break;
		}

		default:
			BView::MessageReceived(message);
			break;
	}
}


// #pragma mark - Mouse and keyboard


void
PhotoStrip::_Select(int32 index)
{
	if (index == fSelected)
		return;
	if (fSelected >= 0 && fSelected < (int32)fItems.size())
		Invalidate(_ItemFrame(fSelected));
	fSelected = index;
	if (index >= 0) {
		Invalidate(_ItemFrame(index));
		// Scroll it into view.
		const BRect frame = _ItemFrame(index);
		const BRect bounds = Bounds();
		if (frame.left < bounds.left)
			ScrollTo(frame.left - kPadding, 0);
		else if (frame.right > bounds.right)
			ScrollTo(frame.right + kPadding - bounds.Width(), 0);
	}
}


void
PhotoStrip::MouseDown(BPoint where)
{
	MakeFocus(true);
	const int32 index = _ItemAt(where);
	_Select(index);
	if (index < 0)
		return;

	int32 buttons = 0;
	int32 clicks = 1;
	if (BMessage* message = Window()->CurrentMessage()) {
		message->FindInt32("buttons", &buttons);
		message->FindInt32("clicks", &clicks);
	}
	if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0) {
		_ShowContextMenu(index, where);
		return;
	}
	if (clicks == 2) {
		_Open(index);
		return;
	}
	fPressed = index;
	fPressPoint = where;
	SetMouseEventMask(B_POINTER_EVENTS, B_NO_POINTER_HISTORY);
}


void
PhotoStrip::MouseUp(BPoint where)
{
	fPressed = -1;
}


void
PhotoStrip::MouseMoved(BPoint where, uint32 transit,
	const BMessage* dragMessage)
{
	if (fPressed < 0 || fPressed >= (int32)fItems.size())
		return;
	if (fabsf(where.x - fPressPoint.x) < 4 && fabsf(where.y - fPressPoint.y) < 4)
		return;

	// Drag the photo out: a B_SIMPLE_DATA message with its entry_ref is what
	// Tracker and other programs expect from a dragged file.
	const Item& item = fItems[fPressed];
	BMessage drag(B_SIMPLE_DATA);
	drag.AddRef("refs", &item.ref);
	const BRect frame = _ItemFrame(fPressed);
	if (item.thumbnail != NULL) {
		// DragMessage() takes the bitmap and deletes it when done.
		BBitmap* image = ScaleBitmap(item.thumbnail, frame.Width());
		DragMessage(&drag, image, B_OP_BLEND,
			BPoint(fPressPoint.x - frame.left, fPressPoint.y - frame.top));
	} else
		DragMessage(&drag, frame);
	fPressed = -1;
}


void
PhotoStrip::KeyDown(const char* bytes, int32 count)
{
	const int32 last = (int32)fItems.size() - 1;
	switch (bytes[0]) {
		case B_LEFT_ARROW:
			_Select(fSelected < 0 ? 0 : std::max(0, fSelected - 1));
			break;
		case B_RIGHT_ARROW:
			_Select(fSelected < 0 ? 0 : std::min(last, fSelected + 1));
			break;
		case B_HOME:
			_Select(last >= 0 ? 0 : -1);
			break;
		case B_END:
			_Select(last);
			break;
		case B_ENTER:
			_Open(fSelected);
			break;
		case B_DELETE:
			_Trash(fSelected);
			break;
		default:
			BView::KeyDown(bytes, count);
			break;
	}
}


// #pragma mark - Actions


void
PhotoStrip::_ShowContextMenu(int32 index, BPoint where)
{
	BPopUpMenu* menu = new BPopUpMenu("photo", false, false);
	BMenuItem* open = new BMenuItem("Open", NULL);
	BMenuItem* show = new BMenuItem("Show in Tracker", NULL);
	BMenuItem* trash = new BMenuItem("Move to Trash", NULL);
	menu->AddItem(open);
	menu->AddItem(show);
	menu->AddSeparatorItem();
	menu->AddItem(trash);

	// Go() without a target returns the chosen item.
	BMenuItem* chosen = menu->Go(ConvertToScreen(where), false, true);
	if (chosen == open)
		_Open(index);
	else if (chosen == show)
		_ShowInTracker(index);
	else if (chosen == trash)
		_Trash(index);
	delete menu;
}


void
PhotoStrip::_Open(int32 index)
{
	if (index < 0 || index >= (int32)fItems.size())
		return;
	// The registrar opens it with the user's preferred application for
	// PNG images (ShowImage, unless changed in FileTypes).
	be_roster->Launch(&fItems[index].ref);
}


void
PhotoStrip::_ShowInTracker(int32 index)
{
	if (index < 0 || index >= (int32)fItems.size())
		return;
	BEntry entry(&fItems[index].ref);
	BEntry folder;
	entry_ref folderRef;
	if (entry.GetParent(&folder) != B_OK || folder.GetRef(&folderRef) != B_OK)
		return;

	// Tracker opens the folder and selects the node it is given.
	BMessage message(B_REFS_RECEIVED);
	message.AddRef("refs", &folderRef);
	message.AddData("nodeRefToSelect", B_RAW_TYPE, &fItems[index].node,
		sizeof(node_ref));
	BMessenger(kTrackerSignature).SendMessage(&message);
}


void
PhotoStrip::_Trash(int32 index)
{
	if (index < 0 || index >= (int32)fItems.size())
		return;
	// The node monitor reports the move, and the strip rescans.
	PhotoLibrary::MoveToTrash(fItems[index].ref);
}


}	// namespace snap
