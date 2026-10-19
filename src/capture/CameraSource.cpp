/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "CameraSource.h"

#include <Autolock.h>
#include <MediaRecorder.h>
#include <MediaRoster.h>

#include <algorithm>
#include <cstring>

#include "FrameExchange.h"
#include "MediaNodes.h"
#include "app/Messages.h"


namespace snap {


// The sizes asked for, best first. A camera that has none of them gets a
// wildcard and sends what it likes.
static const struct {
	uint32 width;
	uint32 height;
} kSizes[] = {
	{ 1280, 720 },
	{ 640, 480 },
	{ 0, 0 }
};


CameraSource::CameraSource(FrameExchange& frames, const BMessenger& target)
	:
	fFrames(frames),
	fTarget(target),
	fLock("camera source"),
	fRecorder(NULL),
	fStartThread(-1)
{
}


CameraSource::~CameraSource()
{
	Stop();
}


void
CameraSource::Start(const media_node& node, const char* name)
{
	Stop();
	fNode = node;
	fName = name;
	fStartThread = spawn_thread(&_StartThread, "Snap camera start",
		B_NORMAL_PRIORITY, this);
	if (fStartThread >= 0)
		resume_thread(fStartThread);
}


void
CameraSource::Stop()
{
	if (fStartThread >= 0) {
		status_t result;
		wait_for_thread(fStartThread, &result);
		fStartThread = -1;
	}

	BMediaRecorder* recorder;
	{
		BAutolock _(fLock);
		recorder = fRecorder;
		fRecorder = NULL;
	}
	if (recorder == NULL)
		return;

	// Stop the camera node too, so its light goes out, then let go of it.
	recorder->Stop();
	if (BMediaRoster* roster = BMediaRoster::CurrentRoster())
		roster->StopNode(fNode, 0, true);
	recorder->Disconnect();
	delete recorder;
	fFrames.Clear();
}


status_t
CameraSource::_StartThread(void* data)
{
	return ((CameraSource*)data)->_Connect();
}


status_t
CameraSource::_Connect()
{
	BMediaRoster* roster = BMediaRoster::Roster();
	if (roster == NULL) {
		_Report(kMsgCameraFailed, "The media services are not running.");
		return B_ERROR;
	}

	media_output output;
	if (!FindFreeOutput(roster, fNode, B_MEDIA_RAW_VIDEO, output)) {
		_Report(kMsgCameraFailed,
			"The camera is in use by another application.");
		return B_BUSY;
	}

	BMediaRecorder* recorder = new BMediaRecorder("Snap camera",
		B_MEDIA_RAW_VIDEO);
	status_t status = recorder->InitCheck();
	if (status == B_OK) {
		recorder->SetHooks(&_ProcessFrame, NULL, this);
		for (const auto& size : kSizes) {
			media_format format;
			format.type = B_MEDIA_RAW_VIDEO;
			format.u.raw_video = media_raw_video_format::wildcard;
			format.u.raw_video.display.format = B_RGB32;
			format.u.raw_video.display.line_width = size.width;
			format.u.raw_video.display.line_count = size.height;
			status = recorder->Connect(fNode, &output, &format);
			if (status == B_OK)
				break;
		}
		if (status == B_OK)
			status = recorder->Start();
	}

	if (status != B_OK) {
		if (recorder->IsConnected())
			recorder->Disconnect();
		delete recorder;
		BString error("The camera could not be started: ");
		error << strerror(status) << ".";
		_Report(kMsgCameraFailed, error.String());
		return status;
	}

	{
		BAutolock _(fLock);
		fRecorder = recorder;
	}
	_Report(kMsgCameraStarted, NULL);
	return B_OK;
}


void
CameraSource::_ProcessFrame(void* cookie, bigtime_t time, void* data,
	size_t size, const media_format& format)
{
	((CameraSource*)cookie)->_Frame(data, size, format);
}


void
CameraSource::_Frame(const void* data, size_t size, const media_format& format)
{
	// On the recorder node's thread. The buffer goes back to the camera when
	// this returns, so the picture is copied out (one memcpy per row at
	// most). Only B_RGB32 was asked for; anything else is skipped.
	const media_video_display_info& display = format.u.raw_video.display;
	const int32 width = display.line_width;
	const int32 height = display.line_count;
	const int32 sourceBytesPerRow = display.bytes_per_row != 0
		? display.bytes_per_row : width * 4;
	if (width <= 0 || height <= 0
		|| (display.format != B_RGB32 && display.format != B_RGBA32)
		|| size < (size_t)sourceBytesPerRow * height) {
		return;
	}

	VideoFrame& frame = fFrames.BeginWrite();
	frame.width = width;
	frame.height = height;
	frame.bytesPerRow = width * 4;
	frame.pixels.resize((size_t)frame.bytesPerRow * height);
	if (sourceBytesPerRow == frame.bytesPerRow)
		memcpy(frame.pixels.data(), data, frame.pixels.size());
	else {
		for (int32 y = 0; y < height; y++) {
			memcpy(frame.pixels.data() + y * frame.bytesPerRow,
				(const uint8*)data + y * sourceBytesPerRow, frame.bytesPerRow);
		}
	}
	fFrames.EndWrite();
}


void
CameraSource::_Report(uint32 what, const char* error)
{
	BMessage message(what);
	message.AddString("name", fName);
	if (error != NULL)
		message.AddString("error", error);
	fTarget.SendMessage(&message);
}


}	// namespace snap
