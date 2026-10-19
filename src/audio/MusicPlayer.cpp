/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "MusicPlayer.h"

#include <MediaFile.h>
#include <MediaTrack.h>
#include <SoundPlayer.h>

#include <algorithm>
#include <cstring>

#include "SoundAnalysis.h"


namespace snap {


MusicPlayer::MusicPlayer(SoundAnalysis& analysis)
	:
	fAnalysis(analysis),
	fFile(NULL),
	fTrack(NULL),
	fPlayer(NULL),
	fDecodedOffset(0),
	fDecodedFrames(0),
	fFailures(0)
{
}


MusicPlayer::~MusicPlayer()
{
	Stop();
}


status_t
MusicPlayer::Play(const entry_ref& ref, BString& error)
{
	Stop();

	BMediaFile* file = new BMediaFile(&ref);
	status_t status = file->InitCheck();
	if (status != B_OK) {
		error << "\"" << ref.name << "\" is not a sound file Snap can read.";
		delete file;
		return status;
	}

	// The first track that decodes to raw audio. Asking for floats lets the
	// decoder convert, whatever the file holds.
	BMediaTrack* track = NULL;
	media_format format;
	for (int32 i = 0; i < file->CountTracks(); i++) {
		BMediaTrack* candidate = file->TrackAt(i);
		format = media_format();
		format.type = B_MEDIA_RAW_AUDIO;
		format.u.raw_audio.format = media_raw_audio_format::B_AUDIO_FLOAT;
		format.u.raw_audio.byte_order = B_MEDIA_HOST_ENDIAN;
		if (candidate != NULL && candidate->DecodedFormat(&format) == B_OK
			&& format.type == B_MEDIA_RAW_AUDIO
			&& format.u.raw_audio.format
				== media_raw_audio_format::B_AUDIO_FLOAT) {
			track = candidate;
			break;
		}
		if (candidate != NULL)
			file->ReleaseTrack(candidate);
	}
	if (track == NULL) {
		error << "\"" << ref.name << "\" has no sound Snap can play.";
		delete file;
		return B_MEDIA_BAD_FORMAT;
	}

	fFile = file;
	fTrack = track;
	fFormat = format;
	fTitle = ref.name;
	const media_raw_audio_format& raw = format.u.raw_audio;
	// Room for the largest chunk a decoder hands out (buffer_size may be 0
	// or too small for some of them: a FLAC frame is up to 64 Ki samples).
	fDecoded.resize(std::max<size_t>(raw.buffer_size / sizeof(float),
		65536 * std::max<uint32>(raw.channel_count, 1)));
	fDecodedOffset = fDecodedFrames = 0;
	fFailures = 0;

	// The player takes the decoded format; the mixer converts the rate.
	media_raw_audio_format playerFormat = raw;
	playerFormat.buffer_size = 0;	// let the mixer choose
	fPlayer = new BSoundPlayer(&playerFormat, "Snap music", &_PlayBuffer,
		NULL, this);
	status = fPlayer->InitCheck();
	if (status == B_OK) {
		fPlayer->SetHasData(true);
		status = fPlayer->Start();
	}
	if (status != B_OK) {
		error << "The music cannot be played: " << strerror(status) << ".";
		Stop();
		return status;
	}
	return B_OK;
}


void
MusicPlayer::Stop()
{
	if (fPlayer != NULL) {
		// Waits for the player's thread to leave _PlayBuffer().
		fPlayer->Stop();
		delete fPlayer;
		fPlayer = NULL;
	}
	delete fFile;	// releases its tracks
	fFile = NULL;
	fTrack = NULL;
	fTitle = "";
	fAnalysis.Reset();
}


void
MusicPlayer::_PlayBuffer(void* cookie, void* buffer, size_t size,
	const media_raw_audio_format& format)
{
	MusicPlayer* player = (MusicPlayer*)cookie;
	const size_t channels = std::max<uint32>(format.channel_count, 1);
	player->_Fill((float*)buffer, size / (sizeof(float) * channels));
	// The effects react to exactly what goes to the speakers.
	player->fAnalysis.Feed(buffer, size, format);
}


void
MusicPlayer::_Fill(float* buffer, size_t frames)
{
	const size_t channels = std::max<uint32>(fFormat.u.raw_audio.channel_count, 1);
	while (frames > 0) {
		if (fDecodedOffset == fDecodedFrames) {
			// Decode the next chunk; at the end, start over.
			int64 count = 0;
			media_header header;
			status_t status = fTrack->ReadFrames(fDecoded.data(), &count,
				&header);
			if (status == B_LAST_BUFFER_ERROR || (status == B_OK && count == 0)) {
				int64 frame = 0;
				fTrack->SeekToFrame(&frame);
				status = fTrack->ReadFrames(fDecoded.data(), &count, &header);
			}
			if (status != B_OK || count <= 0) {
				// A broken file: play silence rather than spin.
				memset(buffer, 0, frames * channels * sizeof(float));
				fFailures++;
				return;
			}
			fDecodedOffset = 0;
			fDecodedFrames = std::min<size_t>(count,
				fDecoded.size() / channels);
		}

		const size_t take = std::min(frames, fDecodedFrames - fDecodedOffset);
		memcpy(buffer, fDecoded.data() + fDecodedOffset * channels,
			take * channels * sizeof(float));
		buffer += take * channels;
		frames -= take;
		fDecodedOffset += take;
	}
}


}	// namespace snap
