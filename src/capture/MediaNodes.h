/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_MEDIA_NODES_H
#define SNAP_MEDIA_NODES_H


#include <MediaNode.h>
#include <String.h>

#include <vector>


class BMediaRoster;


namespace snap {


// A camera or a sound input the media_server knows: a live node that
// produces raw video or raw audio and stands for a physical input.
struct InputDevice {
	BString				name;
	media_node			node;
	bool				isDefault;	// the one set in Media preferences
};


// Lists the inputs of a type (B_MEDIA_RAW_VIDEO for cameras,
// B_MEDIA_RAW_AUDIO for microphones), the default one first.
void ListInputDevices(media_type type, std::vector<InputDevice>& devices);

// The device called name, else the default one; NULL when there are none.
const InputDevice* ChooseInputDevice(const std::vector<InputDevice>& devices,
	const char* name);


// Finds a free output of the given type on a node. A program that ended
// without disconnecting (it crashed) leaves its output connected to a port
// that no longer exists, and the node does not notice: such outputs are
// disconnected first. Returns false when every output is really in use.
bool FindFreeOutput(BMediaRoster* roster, const media_node& node,
	media_type type, media_output& output);


}	// namespace snap


#endif	// SNAP_MEDIA_NODES_H
