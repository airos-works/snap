/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_MUSIC_PLAYER_H
#define SNAP_MUSIC_PLAYER_H


#include <Entry.h>
#include <MediaDefs.h>
#include <String.h>

#include <vector>


class BMediaFile;
class BMediaTrack;
class BSoundPlayer;


namespace snap {


class SoundAnalysis;


// Plays a music file over and over for the effects to dance to. The system
// audio mixer has only one output and it goes to the speakers, so another
// program's sound cannot be tapped; instead Snap plays the music itself and
// analyses exactly the samples it hands to the mixer.
//
// BMediaFile and BMediaTrack decode whatever format the installed media
// plug-ins read (MP3, AAC, FLAC, Ogg Vorbis, WAV, ...) to floating point
// samples; a BSoundPlayer, a ready-made producer node connected to the mixer,
// asks for each buffer from its own real-time thread.
class MusicPlayer {
public:
								MusicPlayer(SoundAnalysis& analysis);
								~MusicPlayer();

			status_t			Play(const entry_ref& ref, BString& error);
			void				Stop();

			const BString&		Title() const { return fTitle; }

private:
	static	void				_PlayBuffer(void* cookie, void* buffer,
									size_t size,
									const media_raw_audio_format& format);
			void				_Fill(float* buffer, size_t frames);

			SoundAnalysis&		fAnalysis;
			BMediaFile*			fFile;
			BMediaTrack*		fTrack;			// belongs to fFile
			BSoundPlayer*		fPlayer;
			media_format		fFormat;		// what the track decodes to
			BString				fTitle;

			// ReadFrames() delivers whole decoder buffers, of whatever size
			// the decoder likes; what does not fit the player's buffer waits
			// here for the next one.
			std::vector<float>	fDecoded;
			size_t				fDecodedOffset;
			size_t				fDecodedFrames;
			int32				fFailures;
};


}	// namespace snap


#endif	// SNAP_MUSIC_PLAYER_H
