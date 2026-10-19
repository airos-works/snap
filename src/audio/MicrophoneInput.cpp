/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "MicrophoneInput.h"

#include <Autolock.h>
#include <MediaRecorder.h>
#include <MediaRoster.h>
#include <TimeSource.h>

#include <vector>

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
	fStartedInput(false),
	fStartThread(-1),
	fGeneration(0)
{
}


MicrophoneInput::~MicrophoneInput()
{
	Stop();
}


void
MicrophoneInput::Start(const char* preferredInput)
{
	if (fStartThread >= 0 || fRecorder != NULL)
		return;
	fPreferredInput = preferredInput;
	fGeneration++;
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
	// What the start thread reported is out of date now.
	fGeneration++;

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

		// An input that is nothing but a microphone (a USB one) was started
		// for Snap: stop it again when nobody else records from it. A sound
		// card that also plays keeps running.
		BMediaRoster* roster = BMediaRoster::CurrentRoster();
		media_output outputs[4];
		int32 count = 0;
		if (fStartedInput && roster != NULL
			&& roster->GetConnectedOutputsFor(fInput, outputs, 4, &count) == B_OK
			&& count == 0) {
			roster->StopNode(fInput, 0, true);
		}
	}
	fStartedInput = false;
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

	std::vector<InputDevice> inputs;
	ListInputDevices(B_MEDIA_RAW_AUDIO, inputs);
	const InputDevice* chosen = ChooseInputDevice(inputs,
		fPreferredInput.String());
	if (chosen == NULL) {
		_Report("There is no sound input.", false);
		return B_ERROR;
	}
	const media_node input = chosen->node;

	media_output output;
	if (!FindFreeOutput(roster, input, B_MEDIA_RAW_AUDIO, output)) {
		BString text("\xE2\x80\x9C");
		text << chosen->name << "\xE2\x80\x9D is in use by another application.";
		_Report(text.String(), false);
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
			fStartedInput = (input.kind & B_PHYSICAL_OUTPUT) == 0;
		}
	}

	if (status != B_OK) {
		if (recorder->IsConnected())
			recorder->Disconnect();
		delete recorder;
		BString text("The microphone could not be opened: ");
		text << strerror(status) << ".";
		_Report(text.String(), false);
		return status;
	}

	{
		BAutolock _(fLock);
		fRecorder = recorder;
		fInput = input;
	}
	BString text("Listening to \xE2\x80\x9C");
	text << chosen->name << "\xE2\x80\x9D";
	_Report(text.String(), true);
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
	message.AddInt32("generation", fGeneration);
	fTarget.SendMessage(&message);
}


}	// namespace snap
