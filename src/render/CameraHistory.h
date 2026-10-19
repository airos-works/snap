/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_CAMERA_HISTORY_H
#define SNAP_CAMERA_HISTORY_H


#include "GLUtilities.h"


namespace snap {


// The most recent camera pictures, for Time Warp: a ring of layers in one
// array texture (GL_TEXTURE_2D_ARRAY), filled by the GPU itself with
// glBlitFramebuffer, which also scales each picture down on the way.
class CameraHistory {
public:
	static const int	kLayers = 48;	// 1.6 s at 30 pictures a second

								CameraHistory();
								~CameraHistory();

	// Copies the camera picture in as the newest layer.
			void				Push(const RenderTarget& camera);
	// Starts over (the next Push() is the only picture).
			void				Clear() { fCount = 0; }

			void				Bind(int unit) const;
			int					Newest() const { return fNewest; }
			int					Count() const { return fCount; }

private:
			void				_Allocate(int width, int height);

			GLuint				fTexture;
			GLuint				fFramebuffer;
			int					fWidth;
			int					fHeight;
			int					fNewest;
			int					fCount;
};


}	// namespace snap


#endif	// SNAP_CAMERA_HISTORY_H
