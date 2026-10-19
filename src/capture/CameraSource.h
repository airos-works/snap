/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_CAMERA_SOURCE_H
#define SNAP_CAMERA_SOURCE_H


#include <Locker.h>
#include <MediaNode.h>
#include <Messenger.h>
#include <String.h>


class BMediaRecorder;


namespace snap {


class FrameExchange;


// A camera delivering pictures to a FrameExchange.
//
// Cameras are Media Kit producer nodes (on air/OS the usb_webcam add-on
// publishes one per UVC camera). Snap connects a BMediaRecorder to the
// node's raw video output and asks for B_RGB32 pictures of 1280x720 - the
// camera add-on converts from YUV or Motion-JPEG and scales or crops to the
// size asked for. The recorder calls a hook function for every picture, on
// its own thread, and the picture is copied into the FrameExchange for the
// render thread.
//
// A camera serves one program at a time. Start() connects on a thread of
// its own and reports to the target: kMsgCameraStarted or kMsgCameraFailed.
class CameraSource {
public:
								CameraSource(FrameExchange& frames,
									const BMessenger& target);
								~CameraSource();

			void				Start(const media_node& node,
									const char* name);
			void				Stop();

			const BString&		Name() const { return fName; }

private:
	static	status_t			_StartThread(void* data);
			status_t			_Connect();
	static	void				_ProcessFrame(void* cookie, bigtime_t time,
									void* data, size_t size,
									const media_format& format);
			void				_Frame(const void* data, size_t size,
									const media_format& format);
			void				_Report(uint32 what, const char* error);

			FrameExchange&		fFrames;
			BMessenger			fTarget;
			BLocker				fLock;		// fRecorder, set by the start thread
			BMediaRecorder*		fRecorder;
			media_node			fNode;
			BString				fName;
			thread_id			fStartThread;
};


}	// namespace snap


#endif	// SNAP_CAMERA_SOURCE_H
