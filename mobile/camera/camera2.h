/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _MOBILE_CAMERA_CAMERA2_H_
#define _MOBILE_CAMERA_CAMERA2_H_

#include <sys/types.h>
#include <sys/param.h>
#include <stdint.h>
#include <stdbool.h>
#include "camera.h"

#define CAM2_MAX_REQUESTS	16
#define CAM2_MAX_STREAMS	8
#define CAM2_MAX_METADATA	64

enum cam2_awb_mode {
	CAM2_AWB_OFF,
	CAM2_AWB_AUTO,
	CAM2_AWB_INCANDESCENT,
	CAM2_AWB_FLUORESCENT,
	CAM2_AWB_DAYLIGHT,
	CAM2_AWB_CLOUDY,
	CAM2_AWB_TWILIGHT,
	CAM2_AWB_SHADE,
};

enum cam2_af_mode {
	CAM2_AF_OFF,
	CAM2_AF_AUTO,
	CAM2_AF_MACRO,
	CAM2_AF_CONTINUOUS_PICTURE,
	CAM2_AF_CONTINUOUS_VIDEO,
	CAM2_AF_EDOF,
	CAM2_AF_INFINITY,
};

enum cam2_ae_mode {
	CAM2_AE_OFF,
	CAM2_AE_ON,
	CAM2_AE_ON_AUTO_FLASH,
	CAM2_AE_ON_ALWAYS_FLASH,
	CAM2_AE_ON_AUTO_REDEYE,
};

enum cam2_scene_mode {
	CAM2_SCENE_DISABLED,
	CAM2_SCENE_FACE_PRIORITY,
	CAM2_SCENE_ACTION,
	CAM2_SCENE_PORTRAIT,
	CAM2_SCENE_LANDSCAPE,
	CAM2_SCENE_NIGHT,
	CAM2_SCENE_NIGHT_PORTRAIT,
};

enum cam2_stream_type {
	CAM2_STREAM_OUTPUT,
	CAM2_STREAM_INPUT,
};

struct cam2_stream {
	uint32_t width;
	uint32_t height;
	enum cam2_stream_type stream_type;
	uint32_t format;
	uint32_t max_buffers;
};

struct cam2_capture_request {
	struct cam2_stream *streams[CAM2_MAX_STREAMS];
	uint32_t stream_count;
	uint32_t request_id;
	enum cam2_awb_mode awb_mode;
	enum cam2_af_mode af_mode;
	enum cam2_ae_mode ae_mode;
	enum cam2_scene_mode scene_mode;
	bool flash_mode;
	uint8_t ae_exposure_comp;
};

struct cam2_capture_result {
	uint32_t request_id;
	struct cam_frame *output_buffers[CAM2_MAX_STREAMS];
	uint32_t output_count;
	struct cam2_metadata {
		uint32_t tag;
		uint32_t value;
	} metadata[CAM2_MAX_METADATA];
	uint32_t metadata_count;
	int64_t timestamp;
};

struct cam2lens_capabilities {
	float focal_length;
	float aperture;
	uint32_t sensor_size_px_w;
	uint32_t sensor_size_px_h;
	float sensor_size_um_w;
	float sensor_size_um_h;
	uint32_t max_jpeg_size;
	uint32_t supported_formats;
	uint32_t supported_fps;
	const char *focal_lengths;
	const char *apertures;
	const char *filter_densities;
	const char *optical_stabilization_modes;
};

struct cam2_characteristics {
	uint32_t camera_id;
	char sensor_name[32];
	struct cam2lens_capabilities lens_cap;
	float min_focus_distance;
	float hyperfocal_distance;
	uint8_t auto_exposure_lock;
	uint8_t auto_white_balance_lock;
	uint8_t video_stabilization;
};

struct cam2_camera_device {
	uint32_t id;
	struct cam_handle *stream_handle;
	struct cam2_characteristics *chars;
	struct cam2_stream active_streams[CAM2_MAX_STREAMS];
	uint32_t active_count;
	enum cam2_af_mode af_mode;
	enum cam2_ae_mode ae_mode;
	enum cam2_awb_mode awb_mode;
};

typedef int (*cam2_process_cb)(struct cam2_capture_result *result, void *arg);

#define CAM2_IOCTL_GETCHARACTERISTICS	_IOWR('2', 0x01,				      \
    struct cam2_characteristics)
#define CAM2_IOCTL_CONFIGURE		_IOW('2',  0x02, struct cam2_stream)
#define CAM2_IOCTL_CAPTURE		_IOW('2',  0x03, struct cam2_capture_request)
#define CAM2_IOCTL_SETREP_CB		_IOW('2',  0x04, cam2_process_cb)
#define CAM2_IOCTL_ABORT		_IOW('2',  0x05, uint32_t)
#define CAM2_IOCTL_GETMETERINGREGIONS	_IOWR('2', 0x06, uint32_t)
#define CAM2_IOCTL_SETEXPOSURECOMP	_IOW('2',  0x07, int8_t)
#define CAM2_IOCTL_SETSCENEMODE		_IOW('2',  0x08,				      \
    enum cam2_scene_mode)
#define CAM2_IOCTL_GET3AREGIONS		_IOWR('2', 0x09, uint32_t)
#define CAM2_IOCTL_FLUSHSTREAMS		_IO('2',  0x0A)
#define CAM2_IOCTL_REPROCESS		_IOW('2',  0x0B, struct cam2_stream)

struct cam2_manager {
	uint32_t camera_count;
	struct cam2_characteristics chars[8];
	struct cam2_camera_device devices[8];
};

int cam2_init(void);
int cam2_deinit(void);
int cam2_camera_manager_get_camera_id_list(struct cam2_manager *mgr);
struct cam2_camera_device *cam2_open_camera(uint32_t camera_id);
int cam2_close_camera(struct cam2_camera_device *cam);
int cam2_configure_streams(struct cam2_camera_device *cam,
    struct cam2_stream streams[], uint32_t count);
int cam2_process_capture_request(struct cam2_camera_device *cam,
    struct cam2_capture_request *req);
int cam2_get_camera_characteristics(uint32_t id,
    struct cam2_characteristics *chars);
int cam2_set_af_mode(struct cam2_camera_device *cam, enum cam2_af_mode mode);
int cam2_set_ae_mode(struct cam2_camera_device *cam, enum cam2_ae_mode mode);
int cam2_set_awb_mode(struct cam2_camera_device *cam, enum cam2_awb_mode mode);
int cam2_set_scene_mode(struct cam2_camera_device *cam,
    enum cam2_scene_mode mode);
int cam2_set_exposure_compensation(struct cam2_camera_device *cam,
    int8_t comp);
int cam2_flush_streams(struct cam2_camera_device *cam);
const char *cam2_scene_mode_name(enum cam2_scene_mode mode);

#endif /* _MOBILE_CAMERA_CAMERA2_H_ */
