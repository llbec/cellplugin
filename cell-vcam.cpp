#include <objbase.h>

#include <obs-module.h>
#include <obs.hpp>
#include "utils/shared-memory-queue.h"
#include "libvcam/vcam.h"

struct cellcamera_data {
	obs_output_t *output;
	video_queue_t *vq;
	void *ccam;
	int width;
	int height;
};

static const char *cellcamera_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Cell Camera";
}

static void cellcamera_destroy(void *data)
{
	struct cellcamera_data *vcam = (struct cellcamera_data *)data;
	video_queue_close(vcam->vq);
	bfree(data);
}

static void *cellcamera_create(obs_data_t *settings, obs_output_t *output)
{
	struct cellcamera_data *vcam =
		(struct cellcamera_data *)bzalloc(sizeof(*vcam));
	vcam->output = output;

	UNUSED_PARAMETER(settings);
	return vcam;
}

static bool cellcamera_start(void *data)
{
	struct cellcamera_data *vcam = (struct cellcamera_data *)data;
	uint32_t width = obs_output_get_width(vcam->output);
	uint32_t height = obs_output_get_height(vcam->output);

	struct obs_video_info ovi;
	obs_get_video_info(&ovi);

	uint64_t interval = ovi.fps_den * 10000000ULL / ovi.fps_num;

	vcam->vq = video_queue_create(width, height, interval);
	if (!vcam->vq) {
		blog(LOG_WARNING, "starting virtual-output failed");
		return false;
	}

	// to start cell camera
	if (!vcamera_start(vcam->vq, width, height)) {
		blog(LOG_WARNING, "starting vcamera_start failed");
		return false;
	}

	vcam->width = width;
	vcam->height = height;

	struct video_scale_info vsi = {};
	vsi.format = VIDEO_FORMAT_NV12;
	vsi.width = width;
	vsi.height = height;
	obs_output_set_video_conversion(vcam->output, &vsi);

	blog(LOG_INFO, "Cellcamera output started");
	obs_output_begin_data_capture(vcam->output, 0);
	return true;
}

static void cellcamera_stop(void *data, uint64_t ts)
{
	struct cellcamera_data *vcam = (struct cellcamera_data *)data;
	obs_output_end_data_capture(vcam->output);
	video_queue_close(vcam->vq);
	vcam->vq = NULL;

	//vcamera_stop();

	blog(LOG_INFO, "Cellcamera output stopped");

	UNUSED_PARAMETER(ts);
}

static void virtual_video(void *param, struct video_data *frame)
{
	struct cellcamera_data *vcam = (struct cellcamera_data *)param;

	if (!vcam->vq)
		return;

	video_queue_write(vcam->vq, frame->data, frame->linesize,
			  frame->timestamp);
	vcamera_frame_event();
}

void RegisterCamOutput()
{
	obs_output_info info = {};
	info.id = "virtualcam_output";
	info.flags = OBS_OUTPUT_VIDEO;
	info.get_name = cellcamera_name;
	info.create = cellcamera_create;
	info.destroy = cellcamera_destroy;
	info.start = cellcamera_start;
	info.stop = cellcamera_stop;
	info.raw_video = virtual_video;
	obs_register_output(&info);
}
