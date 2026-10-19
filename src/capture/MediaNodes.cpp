/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "MediaNodes.h"

#include <MediaRoster.h>
#include <OS.h>


namespace snap {


void
ListInputDevices(media_type type, std::vector<InputDevice>& devices)
{
	devices.clear();
	BMediaRoster* roster = BMediaRoster::Roster();
	if (roster == NULL)
		return;

	media_format format;
	format.type = type;
	if (type == B_MEDIA_RAW_VIDEO)
		format.u.raw_video = media_raw_video_format::wildcard;
	else
		format.u.raw_audio = media_raw_audio_format::wildcard;

	live_node_info nodes[32];
	int32 count = 32;
	if (roster->GetLiveNodes(nodes, &count, NULL, &format, NULL,
			B_BUFFER_PRODUCER | B_PHYSICAL_INPUT) != B_OK) {
		return;
	}

	// The default input, as chosen in Media preferences.
	media_node defaultNode;
	media_node_id defaultId = -1;
	if ((type == B_MEDIA_RAW_VIDEO ? roster->GetVideoInput(&defaultNode)
			: roster->GetAudioInput(&defaultNode)) == B_OK) {
		defaultId = defaultNode.node;
		roster->ReleaseNode(defaultNode);
	}

	for (int32 i = 0; i < count; i++) {
		InputDevice device;
		device.name = nodes[i].name;
		device.node = nodes[i].node;
		device.isDefault = nodes[i].node.node == defaultId;
		if (device.isDefault)
			devices.insert(devices.begin(), device);
		else
			devices.push_back(device);
	}
}


const InputDevice*
ChooseInputDevice(const std::vector<InputDevice>& devices, const char* name)
{
	for (const InputDevice& device : devices) {
		if (name != NULL && device.name == name)
			return &device;
	}
	return devices.empty() ? NULL : &devices[0];
}


bool
FindFreeOutput(BMediaRoster* roster, const media_node& node, media_type type,
	media_output& output)
{
	int32 count = 0;
	if (roster->GetFreeOutputsFor(node, &output, 1, &count, type) == B_OK
		&& count > 0) {
		return true;
	}

	media_output outputs[16];
	if (roster->GetConnectedOutputsFor(node, outputs, 16, &count) != B_OK)
		return false;

	bool cleared = false;
	for (int32 i = 0; i < count; i++) {
		port_info info;
		if (get_port_info(outputs[i].destination.port, &info) == B_OK)
			continue;
		// The consumer's node went with its team; Disconnect() only uses
		// the id it is given to tell that node, which is gone.
		roster->Disconnect(node.node, outputs[i].source, node.node,
			outputs[i].destination);
		cleared = true;
	}

	count = 0;
	return cleared && roster->GetFreeOutputsFor(node, &output, 1, &count, type)
		== B_OK && count > 0;
}


}	// namespace snap
