/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */

// A stand-in camera for testing Snap where there is no real one (a virtual
// machine, a build server): a live Media Kit producer node that says it is a
// physical input, like the usb_webcam add-on's nodes, and sends an animated
// B_RGB32 picture 30 times a second, in whatever size the consumer asks for.
//
// Usage: SnapTestCamera [seconds]
//   Runs until quit (hey SnapTestCamera quit), or for the given number of
//   seconds - which tests unplugging a camera while Snap uses it.
//
// Build it with `make test-camera`.


#include <Application.h>
#include <Buffer.h>
#include <BufferGroup.h>
#include <BufferProducer.h>
#include <MediaEventLooper.h>
#include <MediaRoster.h>
#include <MessageRunner.h>
#include <TimeSource.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>


static const float kFrameRate = 30.0f;


class TestCameraNode : public BMediaEventLooper, public BBufferProducer {
public:
	TestCameraNode()
		:
		BMediaNode("Snap Test Camera"),
		BMediaEventLooper(),
		BBufferProducer(B_MEDIA_RAW_VIDEO),
		fBuffers(NULL),
		fConnected(false),
		fEnabled(true),
		fRunning(false),
		fFrame(0)
	{
		// What makes Snap (and Media preferences) take it for a camera.
		AddNodeKind(B_PHYSICAL_INPUT);
	}

	~TestCameraNode()
	{
		Quit();
		delete fBuffers;
	}

	BMediaAddOn* AddOn(int32* id) const override
	{
		return NULL;
	}

	void NodeRegistered() override
	{
		fOutput.node = Node();
		fOutput.source.port = ControlPort();
		fOutput.source.id = 0;
		fOutput.destination = media_destination::null;
		_DefaultFormat(fOutput.format);
		strlcpy(fOutput.name, "video", sizeof(fOutput.name));
		Run();
	}

	// #pragma mark - BBufferProducer

	status_t FormatSuggestionRequested(media_type type, int32 quality,
		media_format* format) override
	{
		if (type != B_MEDIA_RAW_VIDEO && type != B_MEDIA_UNKNOWN_TYPE)
			return B_MEDIA_BAD_FORMAT;
		_DefaultFormat(*format);
		return B_OK;
	}

	status_t FormatProposal(const media_source& output,
		media_format* format) override
	{
		if (output != fOutput.source)
			return B_MEDIA_BAD_SOURCE;
		if (format->type != B_MEDIA_RAW_VIDEO
			&& format->type != B_MEDIA_UNKNOWN_TYPE) {
			return B_MEDIA_BAD_FORMAT;
		}
		_Complete(*format);
		return B_OK;
	}

	status_t FormatChangeRequested(const media_source& source,
		const media_destination& destination, media_format* format,
		int32* _deprecated_) override
	{
		return B_ERROR;
	}

	status_t GetNextOutput(int32* cookie, media_output* output) override
	{
		if (*cookie != 0)
			return B_BAD_INDEX;
		*output = fOutput;
		(*cookie)++;
		return B_OK;
	}

	status_t DisposeOutputCookie(int32 cookie) override
	{
		return B_OK;
	}

	status_t SetBufferGroup(const media_source& source,
		BBufferGroup* group) override
	{
		// Like the camera add-on: only our own buffers.
		return B_ERROR;
	}

	status_t PrepareToConnect(const media_source& what,
		const media_destination& where, media_format* format,
		media_source* source, char* name) override
	{
		if (what != fOutput.source)
			return B_MEDIA_BAD_SOURCE;
		if (fConnected)
			return B_MEDIA_ALREADY_CONNECTED;
		_Complete(*format);
		*source = fOutput.source;
		strlcpy(name, fOutput.name, B_MEDIA_NAME_LENGTH);
		return B_OK;
	}

	void Connect(status_t error, const media_source& source,
		const media_destination& destination, const media_format& format,
		char* name) override
	{
		if (error != B_OK)
			return;
		fOutput.destination = destination;
		fOutput.format = format;
		const media_video_display_info& display = format.u.raw_video.display;
		delete fBuffers;
		fBuffers = new BBufferGroup(display.bytes_per_row * display.line_count, 8);
		_DrawBackground();
		fConnected = true;
		strlcpy(name, fOutput.name, B_MEDIA_NAME_LENGTH);
		printf("SnapTestCamera: connected, %ux%u\n",
			(unsigned)display.line_width, (unsigned)display.line_count);
	}

	void Disconnect(const media_source& what,
		const media_destination& where) override
	{
		fConnected = false;
		fOutput.destination = media_destination::null;
		delete fBuffers;
		fBuffers = NULL;
		printf("SnapTestCamera: disconnected\n");
	}

	void LateNoticeReceived(const media_source& what, bigtime_t howMuch,
		bigtime_t performanceTime) override
	{
	}

	void EnableOutput(const media_source& what, bool enabled,
		int32* _deprecated_) override
	{
		fEnabled = enabled;
	}

	// #pragma mark - BMediaEventLooper

	void HandleEvent(const media_timed_event* event, bigtime_t lateness,
		bool realTimeEvent) override
	{
		switch (event->type) {
			case BTimedEventQueue::B_START:
				if (!fRunning) {
					fRunning = true;
					_Schedule(std::max(event->event_time, TimeSource()->Now()));
				}
				break;
			case BTimedEventQueue::B_STOP:
				fRunning = false;
				EventQueue()->FlushEvents(0, BTimedEventQueue::B_ALWAYS, true,
					BTimedEventQueue::B_HANDLE_BUFFER);
				break;
			case BTimedEventQueue::B_HANDLE_BUFFER:
				if (!fRunning)
					break;
				// This thread runs at real-time priority: a frame that is
				// already late is dropped rather than drawn, or a slow machine
				// would never get the processor back from it.
				if (fConnected && fEnabled && lateness < 1000000 / kFrameRate)
					_SendFrame(event->event_time);
				{
					// The next frame; after falling behind (a start time given
					// in another time source's terms, a busy machine), the
					// schedule starts again from now instead of catching up.
					const bigtime_t interval = (bigtime_t)(1000000 / kFrameRate);
					bigtime_t next = event->event_time + interval;
					const bigtime_t now = TimeSource()->Now();
					if (next < now)
						next = now + interval;
					_Schedule(next);
				}
				break;
			default:
				break;
		}
	}

private:
	void _DefaultFormat(media_format& format)
	{
		format.type = B_MEDIA_RAW_VIDEO;
		format.u.raw_video = media_raw_video_format::wildcard;
		_Complete(format);
	}

	// Fills in what the consumer left open: 640 x 480, B_RGB32, 30 a second.
	void _Complete(media_format& format)
	{
		format.type = B_MEDIA_RAW_VIDEO;
		media_raw_video_format& video = format.u.raw_video;
		if (video.display.line_width == 0)
			video.display.line_width = 640;
		if (video.display.line_count == 0)
			video.display.line_count = 480;
		video.display.format = B_RGB32;
		video.display.bytes_per_row = video.display.line_width * 4;
		video.field_rate = kFrameRate;
		video.interlace = 1;
		video.first_active = 0;
		video.last_active = video.display.line_count - 1;
		video.orientation = B_VIDEO_TOP_LEFT_RIGHT;
		video.pixel_width_aspect = 1;
		video.pixel_height_aspect = 1;
	}

	// A still colour wash, as a room would be; drawn once per connection.
	void _DrawBackground()
	{
		const media_video_display_info& display
			= fOutput.format.u.raw_video.display;
		const uint32 width = display.line_width;
		const uint32 height = display.line_count;
		fBackground.resize(display.bytes_per_row * height);
		for (uint32 y = 0; y < height; y++) {
			uint8* row = fBackground.data() + y * display.bytes_per_row;
			for (uint32 x = 0; x < width; x++) {
				uint8* pixel = row + x * 4;
				pixel[0] = (uint8)(128 + 100 * sinf(x * 0.01f));	// B
				pixel[1] = (uint8)(255 * y / height);			// G
				pixel[2] = (uint8)(128 + 100 * cosf(y * 0.01f));	// R
				pixel[3] = 255;
			}
		}
	}

	void _Schedule(bigtime_t time)
	{
		media_timed_event event(time, BTimedEventQueue::B_HANDLE_BUFFER);
		EventQueue()->AddEvent(event);
	}

	void _SendFrame(bigtime_t performanceTime)
	{
		const media_video_display_info& display
			= fOutput.format.u.raw_video.display;
		const uint32 width = display.line_width;
		const uint32 height = display.line_count;
		const size_t size = display.bytes_per_row * height;
		BBuffer* buffer = fBuffers->RequestBuffer(size, 0);
		if (buffer == NULL)
			return;

		// The still background, then moving stripes along the bottom and a
		// white ball bouncing around: colour, edges and motion for the
		// effects. Only the ball and the stripes are drawn per frame.
		memcpy(buffer->Data(), fBackground.data(), size);
		const float t = fFrame++ / kFrameRate;
		uint8* bits = (uint8*)buffer->Data();
		const uint32 stripesTop = height * 3 / 4;
		const int offset = (int)(t * 60);
		for (uint32 y = stripesTop; y < height; y++) {
			uint32* row = (uint32*)(bits + y * display.bytes_per_row);
			for (uint32 x = 0; x < width; x++) {
				if (((x + offset) / 40) % 2 == 0)
					row[x] = 0xff282828;
			}
		}
		const float ballX = width * (0.5f + 0.35f * sinf(t * 1.3f));
		const float ballY = height * (0.5f + 0.3f * sinf(t * 2.1f));
		const float radius = height * 0.12f;
		const int top = std::max(0, (int)(ballY - radius));
		const int bottom = std::min((int)height - 1, (int)(ballY + radius));
		for (int y = top; y <= bottom; y++) {
			uint32* row = (uint32*)(bits + y * display.bytes_per_row);
			const float dy = y - ballY;
			const float half = sqrtf(std::max(0.0f, radius * radius - dy * dy));
			const int left = std::max(0, (int)(ballX - half));
			const int right = std::min((int)width - 1, (int)(ballX + half));
			for (int x = left; x <= right; x++)
				row[x] = 0xfffafafa;
		}

		media_header* header = buffer->Header();
		header->type = B_MEDIA_RAW_VIDEO;
		header->size_used = size;
		header->start_time = performanceTime;
		if (SendBuffer(buffer, fOutput.source, fOutput.destination) != B_OK)
			buffer->Recycle();
	}

	media_output	fOutput;
	BBufferGroup*	fBuffers;
	bool			fConnected;
	bool			fEnabled;
	bool			fRunning;
	uint32			fFrame;
	std::vector<uint8> fBackground;
};


class TestCameraApp : public BApplication {
public:
	TestCameraApp(bigtime_t lifetime)
		:
		BApplication("application/x-vnd.airOS-SnapTestCamera"),
		fNode(NULL),
		fLifetime(lifetime)
	{
	}

	void ReadyToRun() override
	{
		fNode = new TestCameraNode;
		const status_t status = BMediaRoster::Roster()->RegisterNode(fNode);
		if (status != B_OK) {
			fprintf(stderr, "SnapTestCamera: cannot register: %s\n",
				strerror(status));
			Quit();
			return;
		}
		printf("SnapTestCamera: running as node %d\n", (int)fNode->ID());
		if (fLifetime > 0) {
			BMessage quit(B_QUIT_REQUESTED);
			new BMessageRunner(be_app_messenger, &quit, fLifetime, 1);
		}
	}

	bool QuitRequested() override
	{
		if (fNode != NULL) {
			BMediaRoster::Roster()->UnregisterNode(fNode);
			fNode->Release();
			fNode = NULL;
		}
		return true;
	}

private:
	TestCameraNode*	fNode;
	bigtime_t		fLifetime;
};


int
main(int argc, char** argv)
{
	const bigtime_t lifetime = argc > 1 ? atoll(argv[1]) * 1000000LL : 0;
	TestCameraApp app(lifetime);
	app.Run();
	return 0;
}
