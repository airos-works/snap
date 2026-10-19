/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "FrameExchange.h"

#include <Autolock.h>

#include <utility>


namespace snap {


FrameExchange::FrameExchange()
	:
	fWriting(&fFrames[0]),
	fReady(&fFrames[1]),
	fReading(&fFrames[2]),
	fHasReady(false),
	fNextSequence(1),
	fLock("frame exchange")
{
}


void
FrameExchange::EndWrite()
{
	BAutolock _(fLock);
	fWriting->sequence = fNextSequence++;
	std::swap(fWriting, fReady);
	fHasReady = true;
}


const VideoFrame*
FrameExchange::TakeNewest()
{
	BAutolock _(fLock);
	if (!fHasReady)
		return NULL;
	std::swap(fReading, fReady);
	fHasReady = false;
	return fReading;
}


void
FrameExchange::Clear()
{
	// The consumer's frame is left alone: the render thread may be reading
	// it right now. It is replaced by the next picture that arrives.
	BAutolock _(fLock);
	fHasReady = false;
	fReady->width = fReady->height = 0;
}


}	// namespace snap
