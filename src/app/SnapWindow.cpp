/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "SnapWindow.h"

#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <FilePanel.h>
#include <LayoutBuilder.h>
#include <MediaRoster.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <NodeInfo.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <PropertyInfo.h>
#include <Roster.h>
#include <Screen.h>
#include <ScrollView.h>
#include <SeparatorView.h>
#include <StringView.h>

#include <algorithm>

#include "Messages.h"
#include "audio/MicrophoneInput.h"
#include "audio/MusicPlayer.h"
#include "audio/SoundAnalysis.h"
#include "audio/SoundEffects.h"
#include "capture/CameraSource.h"
#include "capture/FrameExchange.h"
#include "effects/EffectCatalog.h"
#include "photos/PhotoLibrary.h"
#include "ui/Controls.h"
#include "ui/PhotoStrip.h"
#include "ui/StageView.h"


namespace snap {


static const int32 kCountdownSeconds = 3;
static const char* kTrackerSignature = "application/x-vnd.Be-TRAK";


// Snap's scripting suite. Every property is "direct": it belongs to the
// window (and the application passes it on), e.g. `hey Snap get Effect`.
static property_info kProperties[] = {
	{ "Effect", { B_GET_PROPERTY, B_SET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0 },
		"The effect on the stage: get its name, set it by name, id or index.",
		0, { B_STRING_TYPE, B_INT32_TYPE, 0 } },
	{ "Effects", { B_GET_PROPERTY, B_COUNT_PROPERTIES, 0 },
		{ B_DIRECT_SPECIFIER, 0 }, "The names of all effects.", 0,
		{ B_STRING_TYPE, 0 } },
	{ "Grid", { B_GET_PROPERTY, B_SET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0 },
		"Whether all effects are shown at once.", 0, { B_BOOL_TYPE, 0 } },
	{ "Mirror", { B_GET_PROPERTY, B_SET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0 },
		"Whether the camera is shown like a mirror.", 0, { B_BOOL_TYPE, 0 } },
	{ "Countdown", { B_GET_PROPERTY, B_SET_PROPERTY, 0 },
		{ B_DIRECT_SPECIFIER, 0 }, "Whether a photo is taken after 3-2-1.", 0,
		{ B_BOOL_TYPE, 0 } },
	{ "Sound", { B_GET_PROPERTY, B_SET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0 },
		"What the effects listen to: off, microphone or music.", 0,
		{ B_STRING_TYPE, 0 } },
	{ "Music", { B_SET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0 },
		"Plays a sound file (a path) for the effects to react to.", 0,
		{ B_STRING_TYPE, 0 } },
	{ "Level", { B_GET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0 },
		"How loud the sound is, 0 to 1.", 0, { B_FLOAT_TYPE, 0 } },
	{ "Beats", { B_GET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0 },
		"How many beats the sound analysis has heard.", 0, { B_INT32_TYPE, 0 } },
	{ "Photo", { B_EXECUTE_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0 },
		"Takes a photo; answers with its path once it is saved.", 0,
		{ B_STRING_TYPE, 0 } },
	{ "Photos", { B_COUNT_PROPERTIES, 0 }, { B_DIRECT_SPECIFIER, 0 },
		"How many photos the photo folder holds.", 0, { B_INT32_TYPE, 0 } },
	{ "Camera", { B_GET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0 },
		"The camera in use, or why there is none.", 0, { B_STRING_TYPE, 0 } },
	{ "Renderer", { B_GET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0 },
		"The graphics renderer that draws the effects.", 0,
		{ B_STRING_TYPE, 0 } },
	{ NULL }
};


// Only folders and sound files in the music file panel.
class AudioFileFilter : public BRefFilter {
public:
	bool Filter(const entry_ref* ref, BNode* node, struct stat_beos* stat,
		const char* mimeType) override
	{
		if (node->IsDirectory() || node->IsSymLink())
			return true;
		return mimeType != NULL && strncmp(mimeType, "audio/", 6) == 0;
	}
};


SnapWindow::SnapWindow(Settings& settings, const BString& renderer)
	:
	BWindow(BRect(0, 0, 959, 759), "Snap", B_TITLED_WINDOW,
		B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS
			| B_QUIT_ON_WINDOW_CLOSE),
	fSettings(settings),
	fRenderer(renderer),
	fMusicPanel(NULL),
	fCountdownRunner(NULL),
	fCountdownLeft(0),
	fEffect(NormalEffectIndex()),
	fGrid(false),
	fCameraRunning(false),
	fWatchingMedia(false)
{
	const BMessenger self(this);
	fFrames = new FrameExchange;
	fSound = new SoundAnalysis;
	fCamera = new CameraSource(*fFrames, self);
	fMicrophone = new MicrophoneInput(*fSound, self);
	fMusic = new MusicPlayer(*fSound);
	fSoundEffects = new SoundEffects;
	fLibrary = new PhotoLibrary(self);
	fLibrary->Run();

	fStage = new StageView(*fFrames, *fSound);
	fStrip = new PhotoStrip(fLibrary);
	BScrollView* photos = new BScrollView("photos", fStrip, 0, true, false,
		B_NO_BORDER);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(_BuildMenuBar())
		.Add(fStage, 10.0f)
		.Add(_BuildControls())
		.Add(new BSeparatorView(B_HORIZONTAL))
		.Add(photos);

	// Where we left off.
	const int32 effect = FindEffect(fSettings.effect.String());
	_SelectEffect(effect >= 0 ? effect : NormalEffectIndex());
	fStage->SetMirror(fSettings.mirror);
	SetPulseRate(50000);

	BRect frame = fSettings.windowFrame;
	if (frame.IsValid() && BScreen(this).Frame().Contains(frame.LeftTop())) {
		MoveTo(frame.LeftTop());
		ResizeTo(frame.Width(), frame.Height());
	} else
		CenterOnScreen();

	// Cameras plugged in or out while Snap runs: the media roster tells about
	// nodes coming and going.
	BMediaRoster* roster = BMediaRoster::Roster();
	if (roster != NULL && roster->StartWatching(self, B_MEDIA_NODE_CREATED) == B_OK
		&& roster->StartWatching(self, B_MEDIA_NODE_DELETED) == B_OK) {
		fWatchingMedia = true;
	}

	_StartCamera();
	// Music does not start by itself: a window that greets you with a song
	// is a surprise. The Sound menu's "Music" plays the last one again.
	_SetSoundSource(fSettings.soundSource == kSoundMusic
		? kSoundMicrophone : fSettings.soundSource);
}


SnapWindow::~SnapWindow()
{
	if (fWatchingMedia) {
		BMediaRoster* roster = BMediaRoster::CurrentRoster();
		if (roster != NULL)
			roster->StopWatching(BMessenger(this));
	}
	delete fCountdownRunner;

	// The render thread reads the frames and the sound: it goes first.
	fStage->StopRendering();
	delete fCamera;
	delete fMicrophone;
	delete fMusic;
	delete fSoundEffects;
	if (fLibrary->Lock())
		fLibrary->Quit();
	delete fMusicPanel;

	for (BMessage* request : fPhotoReplies) {
		BMessage reply(B_REPLY);
		reply.AddInt32("error", B_CANCELED);
		request->SendReply(&reply);
		delete request;
	}
	delete fFrames;
	delete fSound;
}


BMenuBar*
SnapWindow::_BuildMenuBar()
{
	BMenuBar* menuBar = new BMenuBar("menu bar");

	BMenu* file = new BMenu("File");
	file->AddItem(new BMenuItem("Take Photo", new BMessage(kMsgTakePhoto), 'T'));
	file->AddItem(new BMenuItem("Show Photos Folder",
		new BMessage(kMsgOpenPhotoFolder), 'O'));
	file->AddSeparatorItem();
	BMenuItem* about = new BMenuItem("About Snap" B_UTF8_ELLIPSIS,
		new BMessage(B_ABOUT_REQUESTED));
	about->SetTarget(be_app);
	file->AddItem(about);
	file->AddSeparatorItem();
	file->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q'));
	menuBar->AddItem(file);

	fEffectsMenu = new BMenu("Effects");
	fGridItem = new BMenuItem("Show All Effects", new BMessage(kMsgToggleGrid),
		'G');
	fEffectsMenu->AddItem(fGridItem);
	fEffectsMenu->AddSeparatorItem();
	for (int32 i = 0; i < CountEffects(); i++) {
		BMessage* message = new BMessage(kMsgSelectEffect);
		message->AddInt32("index", i);
		// Command-1 to Command-9, then Command-0 for the tenth.
		const char shortcut = i < 9 ? '1' + i : i == 9 ? '0' : 0;
		fEffectsMenu->AddItem(new BMenuItem(EffectAt(i).name, message, shortcut));
	}
	fEffectsMenu->AddSeparatorItem();
	fEffectsMenu->AddItem(new BMenuItem("Next Effect",
		new BMessage(kMsgNextEffect), ']'));
	fEffectsMenu->AddItem(new BMenuItem("Previous Effect",
		new BMessage(kMsgPreviousEffect), '['));
	menuBar->AddItem(fEffectsMenu);

	fCameraMenu = new BMenu("Camera");
	fMirrorItem = new BMenuItem("Mirror Image", new BMessage(kMsgToggleMirror),
		'M');
	fCountdownItem = new BMenuItem("Count Down Before Each Photo",
		new BMessage(kMsgToggleCountdown));
	fFlashItem = new BMenuItem("Flash and Shutter Sound",
		new BMessage(kMsgToggleFlash));
	_UpdateCameraMenu();
	menuBar->AddItem(fCameraMenu);

	fSoundMenu = new BMenu("Sound");
	fSoundMenu->AddItem(new BMenuItem("Off", new BMessage(kMsgSoundOff)));
	fSoundMenu->AddItem(new BMenuItem("Microphone",
		new BMessage(kMsgSoundMicrophone)));
	fSoundMenu->AddItem(new BMenuItem("Music", new BMessage(kMsgSoundMusic)));
	fSoundMenu->AddSeparatorItem();
	fSoundMenu->AddItem(new BMenuItem("Choose Music" B_UTF8_ELLIPSIS,
		new BMessage(kMsgChooseMusic)));
	menuBar->AddItem(fSoundMenu);

	return menuBar;
}


BView*
SnapWindow::_BuildControls()
{
	fEffectsButton = new BButton("effects", "Effects",
		new BMessage(kMsgToggleGrid));
	fEffectsButton->SetBehavior(BButton::B_TOGGLE_BEHAVIOR);
	fEffectsButton->SetToolTip("Show all effects at once (Command-G)");

	BButton* previous = new BButton("previous", "\xE2\x97\x80",	// ◀
		new BMessage(kMsgPreviousEffect));
	previous->SetToolTip("Previous effect (Left arrow)");
	BButton* next = new BButton("next", "\xE2\x96\xB6",	// ▶
		new BMessage(kMsgNextEffect));
	next->SetToolTip("Next effect (Right arrow)");
	// Square arrow buttons rather than the usual minimum button width.
	const float arrowWidth = previous->PreferredSize().height * 1.4f;
	previous->SetExplicitSize(BSize(arrowWidth, B_SIZE_UNSET));
	next->SetExplicitSize(BSize(arrowWidth, B_SIZE_UNSET));

	// Wide enough for the longest name, so the arrows do not jump around.
	fEffectName = new BStringView("effect name", "");
	fEffectName->SetAlignment(B_ALIGN_CENTER);
	fEffectName->SetFont(be_bold_font);
	float widest = 0;
	for (int32 i = 0; i < CountEffects(); i++)
		widest = std::max(widest, be_bold_font->StringWidth(EffectAt(i).name));
	fEffectName->SetExplicitMinSize(BSize(widest + 16, B_SIZE_UNSET));

	fShutter = new ShutterButton(new BMessage(kMsgTakePhoto));

	fSoundPopUp = new BPopUpMenu("sound");
	fSoundPopUp->AddItem(new BMenuItem("Off", new BMessage(kMsgSoundOff)));
	fSoundPopUp->AddItem(new BMenuItem("Microphone",
		new BMessage(kMsgSoundMicrophone)));
	fSoundPopUp->AddItem(new BMenuItem("Music", new BMessage(kMsgSoundMusic)));
	fSoundPopUp->AddSeparatorItem();
	fSoundPopUp->AddItem(new BMenuItem("Choose Music" B_UTF8_ELLIPSIS,
		new BMessage(kMsgChooseMusic)));
	fSoundField = new BMenuField("sound", "Sound:", fSoundPopUp);
	fMeter = new LevelMeter(*fSound);

	return BLayoutBuilder::Group<>(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_SPACING, B_USE_SMALL_SPACING)
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING, 1.0f)
			.Add(fEffectsButton)
			.Add(previous)
			.Add(fEffectName)
			.Add(next)
			.AddGlue()
		.End()
		.Add(fShutter, 0.0f)
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING, 1.0f)
			.AddGlue()
			.Add(fSoundField)
			.Add(fMeter)
		.End()
		.View();
}


bool
SnapWindow::QuitRequested()
{
	fSettings.windowFrame = Frame();
	fSettings.Save();
	return true;
}


void
SnapWindow::Minimize(bool minimize)
{
	// Nothing to draw while minimized: let the GPU rest.
	fStage->SetPaused(minimize);
	BWindow::Minimize(minimize);
}


void
SnapWindow::MenusBeginning()
{
	for (int32 i = 0; i < CountEffects(); i++) {
		if (BMenuItem* item = fEffectsMenu->ItemAt(i + 2))
			item->SetMarked(i == fEffect);
	}
	fGridItem->SetMarked(fGrid);

	for (int32 i = 0; i < fCameraMenu->CountItems(); i++) {
		BMenuItem* item = fCameraMenu->ItemAt(i);
		const char* name;
		if (item->Message() != NULL
			&& item->Message()->FindString("name", &name) == B_OK) {
			item->SetMarked(fCameraRunning && fCamera->Name() == name);
		}
	}
	fMirrorItem->SetMarked(fSettings.mirror);
	fCountdownItem->SetMarked(fSettings.countdown);
	fFlashItem->SetMarked(fSettings.flash);

	fSoundMenu->ItemAt(0)->SetMarked(fSettings.soundSource == kSoundOff);
	fSoundMenu->ItemAt(1)->SetMarked(fSettings.soundSource == kSoundMicrophone);
	fSoundMenu->ItemAt(2)->SetMarked(fSettings.soundSource == kSoundMusic);
}


void
SnapWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		// Effects.
		case kMsgSelectEffect:
		{
			int32 index;
			if (message->FindInt32("index", &index) == B_OK)
				_SelectEffect(index);
			break;
		}
		case kMsgNextEffect:
			_SelectEffect((fEffect + 1) % CountEffects());
			break;
		case kMsgPreviousEffect:
			_SelectEffect((fEffect + CountEffects() - 1) % CountEffects());
			break;
		case kMsgToggleGrid:
			_SetGrid(!fGrid);
			break;
		case kMsgGridCellClicked:
		{
			// Picking an effect in the grid shows it big, as in Photo Booth.
			int32 index;
			if (message->FindInt32("index", &index) == B_OK) {
				_SelectEffect(index);
				_SetGrid(false);
			}
			break;
		}

		// Photos.
		case kMsgTakePhoto:
			_TakePhoto();
			break;
		case kMsgCountdownTick:
			_CountdownTick();
			break;
		case kMsgPhotoTaken:
			_PhotoTaken(message);
			break;
		case kMsgPhotoSaved:
			_PhotoSaved(message);
			break;
		case kMsgPhotoFailed:
			_PhotoFailed(message->GetString("error", "The photo failed."));
			break;
		case kMsgOpenPhotoFolder:
		{
			BPath path;
			entry_ref ref;
			if (PhotoLibrary::Folder(path, true) == B_OK
				&& get_ref_for_path(path.Path(), &ref) == B_OK) {
				BMessage open(B_REFS_RECEIVED);
				open.AddRef("refs", &ref);
				BMessenger(kTrackerSignature).SendMessage(&open);
			}
			break;
		}

		// The camera.
		case kMsgSelectCamera:
		{
			const char* name;
			if (message->FindString("name", &name) != B_OK)
				break;
			fSettings.camera = name;
			fCamera->Stop();
			fCameraRunning = false;
			_StartCamera();
			break;
		}
		case kMsgCameraStarted:
			fCameraRunning = true;
			fSettings.camera = message->GetString("name", "");
			_CameraStatus("", false);
			break;
		case kMsgCameraFailed:
		{
			fCameraRunning = false;
			BString status(message->GetString("error", ""));
			status << " The effects run on a test scene.";
			_CameraStatus(status.String(), true);
			break;
		}
		case B_MEDIA_NODE_CREATED:
		case B_MEDIA_NODE_DELETED:
			_CamerasChanged(message);
			break;
		case kMsgToggleMirror:
			fSettings.mirror = !fSettings.mirror;
			fStage->SetMirror(fSettings.mirror);
			break;
		case kMsgToggleCountdown:
			fSettings.countdown = !fSettings.countdown;
			break;
		case kMsgToggleFlash:
			fSettings.flash = !fSettings.flash;
			break;

		// Sound.
		case kMsgSoundOff:
			_SetSoundSource(kSoundOff);
			break;
		case kMsgSoundMicrophone:
			_SetSoundSource(kSoundMicrophone);
			break;
		case kMsgChooseMusic:
			_ChooseMusic();
			break;
		case kMsgSoundMusic:
		{
			// From the file panel with the file, from the menus without:
			// then the music played last.
			entry_ref ref;
			if (message->FindRef("refs", &ref) == B_OK)
				_SetSoundSource(kSoundMusic, &ref);
			else
				_SetSoundSource(kSoundMusic);
			break;
		}
		case kMsgSoundStatus:
			fSoundStatus = message->GetString("status", "");
			fMeter->SetActive(message->GetBool("listening", false));
			fMeter->SetToolTip(fSoundStatus.String());
			_UpdateStageStatus();
			break;
		case B_SIMPLE_DATA:
		{
			// A sound file dropped on the window plays for the effects.
			entry_ref ref;
			for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK; i++) {
				BNode node(&ref);
				char type[B_MIME_TYPE_LENGTH];
				if (BNodeInfo(&node).GetType(type) == B_OK
					&& strncmp(type, "audio/", 6) == 0) {
					_SetSoundSource(kSoundMusic, &ref);
					break;
				}
			}
			break;
		}

		// The stage.
		case kMsgRendererReady:
			fRenderer = message->GetString("renderer", fRenderer.String());
			break;
		case kMsgRendererFailed:
		{
			BString text("The effects cannot be drawn:\n\n");
			text << message->GetString("error", "unknown error");
			BAlert* alert = new BAlert("Snap", text.String(), "Quit", NULL,
				NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT);
			alert->Go();
			PostMessage(B_QUIT_REQUESTED);
			break;
		}

		case B_GET_PROPERTY:
		case B_SET_PROPERTY:
		case B_EXECUTE_PROPERTY:
		case B_COUNT_PROPERTIES:
			if (!_HandleScripting(message))
				BWindow::MessageReceived(message);
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


// #pragma mark - Effects


void
SnapWindow::_SelectEffect(int32 index)
{
	if (index < 0 || index >= CountEffects())
		return;
	fEffect = index;
	const EffectInfo& info = EffectAt(index);
	fSettings.effect = info.id;
	fStage->SetEffect(index);
	fEffectName->SetText(info.name);

	BString tip(info.description);
	if (info.soundReaction != NULL)
		tip << "\n\n" << info.soundReaction;
	fEffectName->SetToolTip(tip.String());
}


void
SnapWindow::_SetGrid(bool grid)
{
	fGrid = grid;
	fStage->SetGrid(grid);
	fEffectsButton->SetValue(grid ? B_CONTROL_ON : B_CONTROL_OFF);
}


// #pragma mark - Photos


void
SnapWindow::_TakePhoto()
{
	if (fCountdownLeft > 0)
		return;
	// A photo is of one effect, big.
	_SetGrid(false);

	if (!fSettings.countdown) {
		_Capture();
		return;
	}

	// 3, 2, 1: the stage shows the numbers, a BMessageRunner ticks.
	fCountdownLeft = kCountdownSeconds;
	fShutter->SetEnabled(false);
	fStage->StartCountdown(kCountdownSeconds);
	fSoundEffects->PlayTick(false);
	BMessage tick(kMsgCountdownTick);
	delete fCountdownRunner;
	fCountdownRunner = new BMessageRunner(BMessenger(this), &tick, 1000000,
		kCountdownSeconds);
}


void
SnapWindow::_CountdownTick()
{
	if (fCountdownLeft <= 0)
		return;
	fCountdownLeft--;
	if (fCountdownLeft > 0) {
		fSoundEffects->PlayTick(fCountdownLeft == 1);
		return;
	}
	delete fCountdownRunner;
	fCountdownRunner = NULL;
	_Capture();
}


void
SnapWindow::_Capture()
{
	fStage->StopCountdown();
	if (fSettings.flash) {
		fStage->Flash();
		fSoundEffects->PlayShutter();
	}
	// The render thread answers with kMsgPhotoTaken.
	fStage->RequestPhoto();
	fShutter->SetEnabled(true);
}


void
SnapWindow::_PhotoTaken(BMessage* message)
{
	BBitmap* bitmap = NULL;
	if (message->FindPointer("bitmap", (void**)&bitmap) != B_OK)
		return;
	const int32 effect = FindEffect(message->GetString("effect", ""));

	// The library's thread encodes and writes the file.
	BMessage save(kMsgSavePhoto);
	save.AddPointer("bitmap", bitmap);
	save.AddString("effect", EffectAt(effect).name);
	if (fLibrary->PostMessage(&save) != B_OK)
		delete bitmap;
}


void
SnapWindow::_PhotoSaved(BMessage* message)
{
	entry_ref ref;
	BPath path;
	if (message->FindRef("refs", &ref) == B_OK) {
		path.SetTo(&ref);
		fStrip->PhotoAdded(ref);
	}

	for (BMessage* request : fPhotoReplies) {
		BMessage reply(B_REPLY);
		reply.AddString("result", path.Path());
		reply.AddInt32("error", B_OK);
		request->SendReply(&reply);
		delete request;
	}
	fPhotoReplies.clear();
}


void
SnapWindow::_PhotoFailed(const char* error)
{
	for (BMessage* request : fPhotoReplies) {
		BMessage reply(B_REPLY);
		reply.AddString("message", error);
		reply.AddInt32("error", B_ERROR);
		request->SendReply(&reply);
		delete request;
	}
	fPhotoReplies.clear();

	BAlert* alert = new BAlert("Snap", error, "OK", NULL, NULL,
		B_WIDTH_AS_USUAL, B_WARNING_ALERT);
	alert->SetFlags(alert->Flags() | B_CLOSE_ON_ESCAPE);
	alert->Go(NULL);
}


// #pragma mark - Camera


void
SnapWindow::_StartCamera()
{
	ListCameras(fCameras);
	_UpdateCameraMenu();
	if (fCameras.empty()) {
		_CameraStatus("No camera is connected. The effects run on a test "
			"scene until one is.", true);
		return;
	}

	// The camera used last time, else the system's default one.
	const CameraInfo* chosen = &fCameras[0];
	for (const CameraInfo& camera : fCameras) {
		if (camera.name == fSettings.camera)
			chosen = &camera;
	}
	BString status("Starting \xE2\x80\x9C");	// “
	status << chosen->name << "\xE2\x80\x9D" B_UTF8_ELLIPSIS;	// ”
	_CameraStatus(status.String(), true);
	fCamera->Start(chosen->node, chosen->name.String());
}


void
SnapWindow::_CamerasChanged(BMessage* message)
{
	// Many nodes come and go (every program that plays a sound makes some,
	// and so does Snap): only a change in the list of cameras matters.
	std::vector<CameraInfo> cameras;
	ListCameras(cameras);
	bool same = cameras.size() == fCameras.size();
	for (size_t i = 0; same && i < cameras.size(); i++)
		same = cameras[i].node.node == fCameras[i].node.node;
	if (same)
		return;

	bool ours = false;
	for (const CameraInfo& camera : cameras)
		ours |= fCameraRunning && camera.name == fCamera->Name();
	if (fCameraRunning && !ours) {
		// Our camera was unplugged.
		fCamera->Stop();
		fCameraRunning = false;
	}
	if (!fCameraRunning)
		_StartCamera();
	else {
		fCameras = cameras;
		_UpdateCameraMenu();
	}
}


void
SnapWindow::_UpdateCameraMenu()
{
	// The camera list, then the options.
	while (fCameraMenu->CountItems() > 0) {
		BMenuItem* item = fCameraMenu->RemoveItem((int32)0);
		if (item != fMirrorItem && item != fCountdownItem && item != fFlashItem)
			delete item;
	}
	for (const CameraInfo& camera : fCameras) {
		BMessage* message = new BMessage(kMsgSelectCamera);
		message->AddString("name", camera.name);
		fCameraMenu->AddItem(new BMenuItem(camera.name.String(), message));
	}
	if (fCameras.empty()) {
		BMenuItem* none = new BMenuItem("No Camera Connected", NULL);
		none->SetEnabled(false);
		fCameraMenu->AddItem(none);
	}
	fCameraMenu->AddSeparatorItem();
	fCameraMenu->AddItem(fMirrorItem);
	fCameraMenu->AddItem(fCountdownItem);
	fCameraMenu->AddItem(fFlashItem);
}


void
SnapWindow::_CameraStatus(const char* status, bool placeholder)
{
	fCameraStatus = status;
	fStage->SetPlaceholder(placeholder);
	_UpdateStageStatus();
}


void
SnapWindow::_UpdateStageStatus()
{
	// The camera's troubles first; the sound's only when it failed.
	if (fCameraStatus.Length() > 0)
		fStage->SetStatus(fCameraStatus.String());
	else if (fSettings.soundSource == kSoundMicrophone
		&& fSoundStatus.Length() > 0 && !fSoundStatus.StartsWith("Listening"))
		fStage->SetStatus(fSoundStatus.String());
	else
		fStage->SetStatus("");
}


// #pragma mark - Sound


void
SnapWindow::_SetSoundSource(SoundSource source, const entry_ref* music)
{
	entry_ref ref;
	if (source == kSoundMusic) {
		if (music != NULL)
			ref = *music;
		else if (fSettings.musicPath.Length() == 0
			|| get_ref_for_path(fSettings.musicPath.String(), &ref) != B_OK
			|| !BEntry(&ref).Exists()) {
			// Nothing played before: ask for a file, keep what is on.
			_ChooseMusic();
			return;
		}
	}

	fMicrophone->Stop();
	fMusic->Stop();
	fSettings.soundSource = source;
	fSoundStatus = "";

	switch (source) {
		case kSoundOff:
			fMeter->SetActive(false);
			fMeter->SetToolTip("Sound is off");
			break;
		case kSoundMicrophone:
			// It reports back with kMsgSoundStatus.
			fMeter->SetActive(false);
			fMicrophone->Start();
			break;
		case kSoundMusic:
		{
			BString error;
			if (fMusic->Play(ref, error) != B_OK) {
				BAlert* alert = new BAlert("Snap", error.String(), "OK");
				alert->Go(NULL);
				_SetSoundSource(kSoundMicrophone);
				return;
			}
			fSettings.musicPath = BPath(&ref).Path();
			fSoundStatus << "Playing \xE2\x80\x9C" << fMusic->Title()
				<< "\xE2\x80\x9D";
			fMeter->SetActive(true);
			fMeter->SetToolTip(fSoundStatus.String());
			break;
		}
	}
	_UpdateSoundControls();
	_UpdateStageStatus();
}


void
SnapWindow::_ChooseMusic()
{
	if (fMusicPanel == NULL) {
		BMessenger self(this);
		fMusicPanel = new BFilePanel(B_OPEN_PANEL, &self, NULL, B_FILE_NODE,
			false, new BMessage(kMsgSoundMusic), new AudioFileFilter, true);
		fMusicPanel->Window()->SetTitle("Snap: Play Music");
		BPath last(fSettings.musicPath.String());
		BPath folder;
		if (last.InitCheck() == B_OK && last.GetParent(&folder) == B_OK)
			fMusicPanel->SetPanelDirectory(folder.Path());
	}
	fMusicPanel->Show();
	// The pop-up shows the source in use until a file is chosen.
	_UpdateSoundControls();
}


void
SnapWindow::_UpdateSoundControls()
{
	if (BMenuItem* item = fSoundPopUp->ItemAt(fSettings.soundSource))
		item->SetMarked(true);
}


void
SnapWindow::PlayMusic(const entry_ref& ref)
{
	_SetSoundSource(kSoundMusic, &ref);
}


// #pragma mark - Scripting


bool
SnapWindow::HasProperty(const char* property)
{
	for (int32 i = 0; kProperties[i].name != NULL; i++) {
		if (strcmp(kProperties[i].name, property) == 0)
			return true;
	}
	return false;
}


BHandler*
SnapWindow::ResolveSpecifier(BMessage* message, int32 index,
	BMessage* specifier, int32 what, const char* property)
{
	BPropertyInfo info(kProperties);
	if (info.FindMatch(message, index, specifier, what, property) >= 0)
		return this;
	return BWindow::ResolveSpecifier(message, index, specifier, what, property);
}


status_t
SnapWindow::GetSupportedSuites(BMessage* data)
{
	data->AddString("suites", "suite/vnd.airOS-Snap");
	BPropertyInfo info(kProperties);
	data->AddFlat("messages", &info);
	return BWindow::GetSupportedSuites(data);
}


static bool
get_bool_data(BMessage* message, bool& value)
{
	return message->FindBool("data", &value) == B_OK;
}


bool
SnapWindow::_HandleScripting(BMessage* message)
{
	int32 index;
	BMessage specifier;
	int32 what;
	const char* property;
	if (message->GetCurrentSpecifier(&index, &specifier, &what, &property)
			!= B_OK || !HasProperty(property)) {
		return false;
	}

	const BString name(property);
	const bool get = message->what == B_GET_PROPERTY;
	const bool set = message->what == B_SET_PROPERTY;
	BMessage reply(B_REPLY);
	status_t result = B_OK;
	bool value;

	if (name == "Effect") {
		if (get)
			reply.AddString("result", EffectAt(fEffect).name);
		else if (set) {
			int32 effect = -1;
			const char* text;
			if (message->FindInt32("data", &effect) != B_OK
				&& message->FindString("data", &text) == B_OK) {
				effect = FindEffect(text);
				for (int32 i = 0; effect < 0 && i < CountEffects(); i++) {
					if (strcasecmp(EffectAt(i).name, text) == 0)
						effect = i;
				}
			}
			if (effect >= 0 && effect < CountEffects())
				_SelectEffect(effect);
			else
				result = B_BAD_VALUE;
		}
	} else if (name == "Effects") {
		if (message->what == B_COUNT_PROPERTIES)
			reply.AddInt32("result", CountEffects());
		else {
			for (int32 i = 0; i < CountEffects(); i++)
				reply.AddString("result", EffectAt(i).name);
		}
	} else if (name == "Grid") {
		if (get)
			reply.AddBool("result", fGrid);
		else if (set && get_bool_data(message, value))
			_SetGrid(value);
		else
			result = B_BAD_VALUE;
	} else if (name == "Mirror") {
		if (get)
			reply.AddBool("result", fSettings.mirror);
		else if (set && get_bool_data(message, value)) {
			fSettings.mirror = value;
			fStage->SetMirror(value);
		} else
			result = B_BAD_VALUE;
	} else if (name == "Countdown") {
		if (get)
			reply.AddBool("result", fSettings.countdown);
		else if (set && get_bool_data(message, value))
			fSettings.countdown = value;
		else
			result = B_BAD_VALUE;
	} else if (name == "Sound") {
		static const char* kSources[] = { "off", "microphone", "music" };
		if (get)
			reply.AddString("result", kSources[fSettings.soundSource]);
		else {
			const char* text = message->GetString("data", "");
			if (strcasecmp(text, "off") == 0)
				_SetSoundSource(kSoundOff);
			else if (strcasecmp(text, "microphone") == 0)
				_SetSoundSource(kSoundMicrophone);
			else if (strcasecmp(text, "music") == 0)
				_SetSoundSource(kSoundMusic);
			else
				result = B_BAD_VALUE;
		}
	} else if (name == "Music") {
		entry_ref ref;
		if (get_ref_for_path(message->GetString("data", ""), &ref) == B_OK
			&& BEntry(&ref).Exists()) {
			_SetSoundSource(kSoundMusic, &ref);
			if (fSettings.soundSource != kSoundMusic)
				result = B_BAD_VALUE;
		} else
			result = B_ENTRY_NOT_FOUND;
	} else if (name == "Level") {
		reply.AddFloat("result", fSound->Snapshot().level);
	} else if (name == "Beats") {
		reply.AddInt32("result", (int32)fSound->Snapshot().beatCount);
	} else if (name == "Photo") {
		// Answered by _PhotoSaved() once the file is written.
		fPhotoReplies.push_back(DetachCurrentMessage());
		_TakePhoto();
		return true;
	} else if (name == "Photos") {
		reply.AddInt32("result", fStrip->CountPhotos());
	} else if (name == "Camera") {
		reply.AddString("result", fCameraRunning
			? fCamera->Name().String() : fCameraStatus.String());
	} else if (name == "Renderer") {
		reply.AddString("result", fRenderer);
	}

	reply.AddInt32("error", result);
	message->SendReply(&reply);
	return true;
}


}	// namespace snap
