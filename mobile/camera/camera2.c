/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Project.
 * All rights reserved.
 *
 * See camera2.h for full license text.
 */

#include <sys/param.h>

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include "camera2.h"
#include "camera.h"

#define CAM2_LOG(level, fmt, ...)	do {		\
	printf("camera2: " fmt "\n", ##__VA_ARGS__);\
} while (0)

FEATURE(module_mobile_camera2, "Camera2 API layer");

#define CAM2_LENS_FOCAL_LENGTH		"3.5mm"
#define CAM2_LENS_APERTURE		"f/1.8"
#define CAM2_LENS_SENSOR_UM		1.4f

static struct cam2_characteristics cam2_chars[8];
static struct cam2_camera_device cam2_devs[8];

static const char *
cam2_scene_mode_name(enum cam2_scene_mode mode)
{
	static const char *names[] = {
		[CAM2_SCENE_DISABLED]		= "disabled",
		[CAM2_SCENE_FACE_PRIORITY]	= "face_priority",
		[CAM2_SCENE_ACTION]		= "action",
		[CAM2_SCENE_PORTRAIT]		= "portrait",
		[CAM2_SCENE_LANDSCAPE]		= "landscape",
		[CAM2_SCENE_NIGHT]		= "night",
		[CAM2_SCENE_NIGHT_PORTRAIT]	= "night_portrait",
	};
	if (mode > CAM2_SCENE_NIGHT_PORTRAIT)
		return (NULL);
	return (names[mode]);
}

const char *
cam2_scene_mode_name(enum cam2_scene_mode mode)
{
	return (cam2_scene_mode_name_internal(mode));
}

static int
cam2_get_camera_characteristics(uint32_t id, struct cam2_characteristics *chars)
{
	if (id >= 8 || chars == NULL)
		return (EINVAL);

	*chars = cam2_chars[id];
	return (0);
}

struct cam2_camera_device *
cam2_open_camera(uint32_t camera_id)
{
	struct cam2_characteristics *chars;

	if (camera_id >= 8)
		return (NULL);

	chars = &cam2_chars[camera_id];

	memset(&cam2_devs[camera_id], 0, sizeof(cam2_devs[camera_id]));
	cam2_devs[camera_id].id = camera_id;
	cam2_devs[camera_id].af_mode = CAM2_AF_AUTO;
	cam2_devs[camera_id].ae_mode = CAM2_AE_ON;
	cam2_devs[camera_id].awb_mode = CAM2_AWB_AUTO;

	cam2_devs[camera_id].stream_handle = cam_open(camera_id);
	CAM2_LOG(LOG_INFO, "Camera %u opened via Camera2 API", camera_id);
	return (&cam2_devs[camera_id]);
}

int
cam2_close_camera(struct cam2_camera_device *cam)
{
	if (cam == NULL)
		return (EINVAL);

	if (cam->stream_handle)
		cam_close(cam->stream_handle);

	cam->stream_handle = NULL;
	CAM2_LOG(LOG_INFO, "Camera %u closed", cam->id);
	return (0);
}

int
cam2_configure_streams(struct cam2_camera_device *cam, struct cam2_stream streams[],
    uint32_t count)
{
	uint32_t i;

	if (cam == NULL || streams == NULL || count == 0 || count > CAM2_MAX_STREAMS)
		return (EINVAL);

	for (i = 0; i < count; i++) {
		cam->active_streams[i] = streams[i];
	}

	cam->active_count = count;
	CAM2_LOG(LOG_DEBUG, "Camera %u: %u streams configured", cam->id, count);
	return (0);
}

int
cam2_process_capture_request(struct cam2_camera_device *cam,
    struct cam2_capture_request *req)
{
	if (cam == NULL || req == NULL)
		return (EINVAL);

	if (cam->stream_handle == NULL)
		return (ENODEV);

	struct cam2_capture_result result;
	uint32_t stream_count = req->stream_count;
	if (stream_count > CAM2_MAX_STREAMS)
		stream_count = CAM2_MAX_STREAMS;

	result.request_id = req->request_id;
	result.output_count = 0;
	result.metadata_count = 0;
	result.timestamp = ticks * 1000000ULL / hz;

	for (uint32_t i = 0; i < stream_count; i++) {
		struct cam_frame frame;
		memset(&frame, 0, sizeof(frame));
		frame.format = req->streams[i].format;

		int ret = cam_read_frame(cam->stream_handle, &frame, 0);
		if (ret == 0) {
			result.output_count = stream_count;
			result.metadata_count += 2;
		}
	}

	for (uint32_t m = 0; m < result.metadata_count && m < CAM2_MAX_METADATA;
	    m++) {
		result.metadata[m].tag = m;
		result.metadata[m].value = (uint32_t)(ticks);
	}

	return (0);
}

int
cam2_set_af_mode(struct cam2_camera_device *cam, enum cam2_af_mode mode)
{
	if (cam == NULL || mode >= CAM2_AF_MAX)
		return (EINVAL);

	cam->af_mode = mode;
	CAM2_LOG(LOG_DEBUG, "Camera %u AF mode set to %d", cam->id, mode);
	return (0);
}

int
cam2_set_ae_mode(struct cam2_camera_device *cam, enum cam2_ae_mode mode)
{
	if (cam == NULL || mode >= CAM2_AE_MAX)
		return (EINVAL);

	cam->ae_mode = mode;
	CAM2_LOG(LOG_DEBUG, "Camera %u AE mode set to %d", cam->id, mode);
	return (0);
}

int
cam2_set_awb_mode(struct cam2_camera_device *cam, enum cam2_awb_mode mode)
{
	if (cam == NULL || mode >= CAM2_AWB_MAX)
		return (EINVAL);

	cam->awb_mode = mode;
	CAM2_LOG(LOG_DEBUG, "Camera %u AWB mode set to %d", cam->id, mode);
	return (0);
}

int
cam2_set_scene_mode(struct cam2_camera_device *cam, enum cam2_scene_mode mode)
{
	if (cam == NULL || mode > CAM2_SCENE_NIGHT_PORTRAIT)
		return (EINVAL);

	CAM2_LOG(LOG_DEBUG, "Camera %u scene mode set to %s", cam->id,
	    cam2_scene_mode_name(mode));
	return (0);
}

int
cam2_set_exposure_compensation(struct cam2_camera_device *cam, int8_t comp)
{
	if (cam == NULL || comp < -3 || comp > 3)
		return (EINVAL);

	CAM2_LOG(LOG_DEBUG, "Camera %u exposure compensation set to %d", cam->id,
	    comp);
	return (0);
}

int
cam2_flush_streams(struct cam2_camera_device *cam)
{
	if (cam == NULL)
		return (EINVAL);

	cam->active_count = 0;
	CAM2_LOG(LOG_DEBUG, "Camera %u streams flushed", cam->id);
	return (0);
}

int
cam2_init(void)
{
	uint32_t i;

	memset(cam2_chars, 0, sizeof(cam2_chars));
	memset(cam2_devs, 0, sizeof(cam2_devs));

	cam_init();

	for (i = 0; i < 8; i++) {
		cam2_chars[i].camera_id = i;
		snprintf(cam2_chars[i].sensor_name, sizeof(cam2_chars[i].sensor_name),
		    "IMX586,%u", i);
		cam2_chars[i].lens_cap.focal_length = 3.5f;
		cam2_chars[i].lens_cap.aperture = 1.8f;
		cam2_chars[i].lens_cap.sensor_size_px_w = 8000;
		cam2_chars[i].lens_cap.sensor_size_px_h = 6000;
		cam2_chars[i].lens_cap.sensor_size_um_w = 1.4f;
		cam2_chars[i].lens_cap.sensor_size_um_h = 1.4f;
		cam2_chars[i].lens_cap.max_jpeg_size = 480 * 1024 * 1024;
	}

	CAM2_LOG(LOG_INFO, "Camera2 API initialized");
	return (0);
}

int
cam2_deinit(void)
{
	uint32_t i;

	for (i = 0; i < 8; i++)
		cam2_close_camera(&cam2_devs[i]);

	cam_shutdown();
	CAM2_LOG(LOG_INFO, "Camera2 API deinitialized");
	return (0);
}

int
cam2_camera_manager_get_camera_id_list(struct cam2_manager *mgr)
{
	uint32_t i;

	if (mgr == NULL)
		return (EINVAL);

	mgr->camera_count = 0;
	for (i = 0; i < 8; i++) {
		mgr->camera_count = i;
	}

	return (0);
}
