/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "CameraHistory.h"

#include <algorithm>


namespace snap {


// The history is kept at most this wide: Time Warp smears the picture
// anyway, and 48 full HD layers would take 400 MB of video memory.
static const int kMaxWidth = 640;


CameraHistory::CameraHistory()
	:
	fTexture(0),
	fFramebuffer(0),
	fWidth(0),
	fHeight(0),
	fNewest(0),
	fCount(0)
{
}


CameraHistory::~CameraHistory()
{
	if (fFramebuffer != 0)
		glDeleteFramebuffers(1, &fFramebuffer);
	if (fTexture != 0)
		glDeleteTextures(1, &fTexture);
}


void
CameraHistory::_Allocate(int width, int height)
{
	if (fTexture == 0) {
		glGenTextures(1, &fTexture);
		glGenFramebuffers(1, &fFramebuffer);
	}
	glBindTexture(GL_TEXTURE_2D_ARRAY, fTexture);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, width, height, kLayers, 0,
		GL_BGRA, GL_UNSIGNED_BYTE, NULL);
	fWidth = width;
	fHeight = height;
	fCount = 0;
}


void
CameraHistory::Push(const RenderTarget& camera)
{
	int width = std::min(camera.Width(), kMaxWidth);
	int height = std::max(1, camera.Height() * width / std::max(1, camera.Width()));
	if (width != fWidth || height != fHeight)
		_Allocate(width, height);

	fNewest = fCount == 0 ? 0 : (fNewest + 1) % kLayers;
	fCount = std::min(fCount + 1, kLayers);

	// Draw into the layer, read from the camera's frame buffer, let the blit
	// do the scaling.
	FramebufferScope scope;
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fFramebuffer);
	glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		fTexture, 0, fNewest);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, camera.Framebuffer());
	glBlitFramebuffer(0, 0, camera.Width(), camera.Height(), 0, 0, fWidth,
		fHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
}


void
CameraHistory::Bind(int unit) const
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D_ARRAY, fTexture);
}


}	// namespace snap
