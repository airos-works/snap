/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_MICROPHONE_INPUT_H
#define SNAP_MICROPHONE_INPUT_H


#include <Locker.h>
#include <MediaDefs.h>
#include <MediaNode.h>
#include <Messenger.h>
#include <String.h>


class BMediaRecorder;


namespace snap {


class SoundAnalysis;


// A microphone feeding the effects: the sound input chosen in Snap's Sound
// menu (a camera's own microphone, say), else the system's default input as
// set in Media preferences. A BMediaRecorder - the Media Kit's
// ready-made consumer node - is connected to the input node and hands every
// buffer to a hook function, which passes it on to the SoundAnalysis. Nothing
// is recorded or kept.
//
// Connecting can take a moment (the media_server talks to the sound card),
// so Start() does it on a thread of its own and reports to the target with
// kMsgSoundStatus.
class MicrophoneInput {
public:
								MicrophoneInput(SoundAnalysis& analysis,
									const BMessenger& target);
								~MicrophoneInput();

			void				Start(const char* preferredInput);
			void				Stop();

	// Counts the attempts to start; reports (kMsgSoundStatus) carry the one
	// they belong to, so the window can ignore those of an attempt given up.
			int32				Generation() const { return fGeneration; }

private:
	static	status_t			_StartThread(void* data);
			status_t			_Connect();
	static	void				_ProcessBuffer(void* cookie, bigtime_t time,
									void* data, size_t size,
									const media_format& format);
			void				_Report(const char* status, bool listening);

			SoundAnalysis&		fAnalysis;
			BMessenger			fTarget;
			BLocker				fLock;
			BMediaRecorder*		fRecorder;
			media_node			fInput;
			bool				fStartedInput;	// Snap started its node
			BString				fPreferredInput;
			thread_id			fStartThread;
			int32				fGeneration;
};


}	// namespace snap


#endif	// SNAP_MICROPHONE_INPUT_H
