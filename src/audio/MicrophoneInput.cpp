/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "MicrophoneInput.h"

#include <Autolock.h>
#include <MediaRecorder.h>
#include <MediaRoster.h>
#include <TimeSource.h>

#include "SoundAnalysis.h"
#include "app/Messages.h"
#include "capture/MediaNodes.h"


namespace snap {


MicrophoneInput::MicrophoneInput(SoundAnalysis& analysis,
	const BMessenger& target)
	:
	fAnalysis(analysis),
	fTarget(target),
	fLock("microphone"),
	fRecorder(NULL),
	fHasInput(false),
	fStartThread(-1)
{
}


MicrophoneInput::~MicrophoneInput()
{
	Stop();
}


void
MicrophoneInput::Start()
{
	if (fStartThread >= 0 || fRecorder != NULL)
		return;
	fStartThread = spawn_thread(&_StartThread, "Snap microphone",
		B_NORMAL_PRIORITY, this);
	if (fStartThread >= 0)
		resume_thread(fStartThread);
}


void
MicrophoneInput::Stop()
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
	if (recorder != NULL) {
		// Only our own node stops: the sound card's node is usually playing
		// the system's sound at the same time.
		recorder->Stop();
		recorder->Disconnect();
		delete recorder;
	}
	if (fHasInput) {
		BMediaRoster::Roster()->ReleaseNode(fInput);
		fHasInput = false;
	}
	fAnalysis.Reset();
}


status_t
MicrophoneInput::_StartThread(void* data)
{
	return ((MicrophoneInput*)data)->_Connect();
}


status_t
MicrophoneInput::_Connect()
{
	BMediaRoster* roster = BMediaRoster::Roster();
	if (roster == NULL) {
		_Report("The media services are not running.", false);
		return B_ERROR;
	}

	media_node input;
	if (roster->GetAudioInput(&input) != B_OK) {
		_Report("There is no sound input.", false);
		return B_ERROR;
	}

	media_output output;
	if (!FindFreeOutput(roster, input, B_MEDIA_RAW_AUDIO, output)) {
		roster->ReleaseNode(input);
		_Report("The microphone is in use by another application.", false);
		return B_BUSY;
	}

	BMediaRecorder* recorder = new BMediaRecorder("Snap microphone",
		B_MEDIA_RAW_AUDIO);
	status_t status = recorder->InitCheck();
	if (status == B_OK) {
		recorder->SetHooks(&_ProcessBuffer, NULL, this);
		// Whatever the input delivers: the analysis takes any raw format.
		media_format format;
		format.type = B_MEDIA_RAW_AUDIO;
		format.u.raw_audio = media_raw_audio_format::wildcard;
		status = recorder->Connect(input, &output, &format);
		if (status == B_OK)
			status = recorder->Start();
		if (status == B_OK) {
			// A sound card's input is its own time source, which is all
			// BMediaRecorder::Start() starts. An input nothing plays to (a
			// USB microphone) only delivers once it is started as a node.
			BTimeSource* timeSource = roster->MakeTimeSourceFor(input);
			if (timeSource != NULL) {
				roster->StartNode(input, timeSource->Now());
				timeSource->Release();
			}
		}
	}

	if (status != B_OK) {
		if (recorder->IsConnected())
			recorder->Disconnect();
		delete recorder;
		roster->ReleaseNode(input);
		BString text("The microphone could not be opened: ");
		text << strerror(status) << ".";
		_Report(text.String(), false);
		return status;
	}

	{
		BAutolock _(fLock);
		fRecorder = recorder;
		fInput = input;
		fHasInput = true;
	}
	_Report("Listening to the microphone", true);
	return B_OK;
}


void
MicrophoneInput::_ProcessBuffer(void* cookie, bigtime_t time, void* data,
	size_t size, const media_format& format)
{
	// On the recorder node's thread; the buffer is recycled when this
	// returns, so it is analysed right here.
	if (format.type != B_MEDIA_RAW_AUDIO)
		return;
	((MicrophoneInput*)cookie)->fAnalysis.Feed(data, size, format.u.raw_audio);
}


void
MicrophoneInput::_Report(const char* status, bool listening)
{
	BMessage message(kMsgSoundStatus);
	message.AddString("status", status);
	message.AddBool("listening", listening);
	fTarget.SendMessage(&message);
}


}	// namespace snap
