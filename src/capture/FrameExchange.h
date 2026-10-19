/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_FRAME_EXCHANGE_H
#define SNAP_FRAME_EXCHANGE_H


#include <Locker.h>
#include <SupportDefs.h>

#include <vector>


namespace snap {


// One video picture in B_RGB32 (bytes B, G, R, X in memory).
struct VideoFrame {
	int32				width = 0;
	int32				height = 0;
	int32				bytesPerRow = 0;
	uint32				sequence = 0;		// counts up with every new frame
	std::vector<uint8>	pixels;

	bool				IsValid() const { return width > 0 && height > 0; }
};


// Hands the newest camera picture from the Media Kit's thread to the render
// thread without either of them waiting for the other: triple buffering.
//
// The producer always writes into a frame of its own, then swaps it with the
// "ready" frame under a lock held only for that pointer swap. The consumer
// swaps the ready frame with its own when it wants something newer. Nothing
// is copied under the lock, a slow renderer just skips pictures, and a frame
// the renderer is uploading is never written to.
class FrameExchange {
public:
								FrameExchange();

	// Producer side, on the capture thread: fill the frame from
	// BeginWrite() and publish it with EndWrite().
			VideoFrame&			BeginWrite() { return *fWriting; }
			void				EndWrite();

	// Consumer side, on the render thread: returns the newest frame if it is
	// newer than the one returned before, otherwise NULL. The frame stays the
	// consumer's until the next call.
			const VideoFrame*	TakeNewest();

	// Forgets the frame waiting to be taken (the camera was switched or
	// stopped).
			void				Clear();

private:
			VideoFrame			fFrames[3];
			VideoFrame*			fWriting;
			VideoFrame*			fReady;
			VideoFrame*			fReading;
			bool				fHasReady;
			uint32				fNextSequence;
			BLocker				fLock;
};


}	// namespace snap


#endif	// SNAP_FRAME_EXCHANGE_H
