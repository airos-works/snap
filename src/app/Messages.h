/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_MESSAGES_H
#define SNAP_MESSAGES_H


#include <SupportDefs.h>


namespace snap {


// The messages Snap's loopers send each other. Commands come from menus,
// buttons, keyboard shortcuts and scripting alike; notifications come from
// the capture, sound and photo threads.
enum {
	// commands
	kMsgTakePhoto			= 'tkph',
	kMsgSelectEffect		= 'slef',	// "index" int32
	kMsgNextEffect			= 'nxef',
	kMsgPreviousEffect		= 'pvef',
	kMsgToggleGrid			= 'grid',
	kMsgToggleMirror		= 'mirr',
	kMsgToggleCountdown		= 'cntd',
	kMsgToggleFlash			= 'flsh',
	kMsgSelectCamera		= 'slcm',	// "name" string
	kMsgSoundOff			= 'sdof',
	kMsgSoundMicrophone		= 'sdmc',
	kMsgSoundMusic			= 'sdmu',	// optional "refs"
	kMsgChooseMusic			= 'chmu',
	kMsgOpenPhotoFolder		= 'opfd',

	// notifications
	kMsgCameraStarted		= 'cmst',	// "name"
	kMsgCameraFailed		= 'cmfl',	// "name", "error"
	kMsgSoundStatus			= 'sdst',	// "status" string, "listening" bool
	kMsgPhotoTaken			= 'phtk',	// "bitmap" pointer, "effect" string
	kMsgPhotoSaved			= 'phsv',	// "refs"
	kMsgPhotoFailed			= 'phfl',	// "error"
	kMsgPhotoFolderChanged	= 'pfch',	// the strip to itself
	kMsgCountdownTick		= 'cdtk',
	kMsgRendererReady		= 'rdrd',	// "renderer", "version"
	kMsgRendererFailed		= 'rdfl',	// "renderer", "version", "error"
	kMsgGridCellClicked		= 'gccl',	// "index" int32

	// the photo library's work, on its own thread
	kMsgSavePhoto			= 'svph',	// "bitmap" pointer, "effect" string
	kMsgLoadThumbnail		= 'ldth',	// "refs"; replies kMsgThumbnailLoaded
	kMsgThumbnailLoaded		= 'thld'	// "refs", "bitmap" pointer
};


}	// namespace snap


#endif	// SNAP_MESSAGES_H
