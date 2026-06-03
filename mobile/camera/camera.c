/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Project.
 * All rights reserved.
 *
 * See camera.h for full license text.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <sys/ioctl.h>

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

#include "camera.h"

#define CAM_LOG(level, fmt, ...)	do {		\
	printf("camera: " fmt "\n", ##__VA_ARGS__);\
} while (0)

FEATURE(module_mobile_camera, "Mobile camera subsystem");

static struct cam_handle *cam_handles[CAM_MAX_DEVICES];
static uint32_t cam_count;

static void
cam_free_buffers(struct cam_handle *h)
{
	uint32_t b;

	for (b = 0; b < h->buf_count; b++) {
		if (h->buffers[b].data) {
			memset(h->buffers[b].data, 0, h->buffers[b].size);
			free(h->buffers[b].data, M_CAMERA);
			h->buffers[b].data = NULL;
		}
	}
	h->buf_count = 0;
}

static int
cam_alloc_buffers(struct cam_handle *h)
{
	uint32_t fps, size, bpp;

	switch (h->format) {
	case CAM_FORMAT_YUYV: bpp = 2; break;
	case CAM_FORMAT_NV12: bpp = 1; break;
	case CAM_FORMAT_JPEG: bpp = 1; break;
	case CAM_FORMAT_RAW10: bpp = 2; break;
	case CAM_FORMAT_RAW12: bpp = 2; break;
	default: return (EINVAL);
	}

	fps = h->fps > 0 ? h->fps : 30;
	size = h->width * h->height * bpp;

	if (size > 1024 * 1024)
		return (EINVAL);

	h->buf_count = 4;

	for (uint32_t b = 0; b < h->buf_count; b++) {
		h->buffers[b].data = malloc(size, M_CAMERA, M_WAITOK | M_ZERO);
		if (h->buffers[b].data == NULL) {
			cam_free_buffers(h);
			return (ENOMEM);
		}
		h->buffers[b].size = size;
		h->buffers[b].queued = 0;
	}

	CAM_LOG(LOG_DEBUG, "Allocated %u buffers (%ux%u, %d bpp, %uffs)",
	    h->buf_count, h->width, h->height, bpp, fps);

	return (0);
}

int
cam_init(void)
{
	memset(cam_handles, 0, sizeof(cam_handles));

	cam_count = 3;
	for (uint32_t i = 0; i < cam_count; i++)
		cam_handles[i] = NULL;

	CAM_LOG(LOG_INFO, "Camera subsystem initialized (%u devices)", cam_count);
	return (0);
}

int
cam_enumerate(struct cam_device_info infos[CAM_MAX_DEVICES], uint32_t *count)
{
	struct cam_device_info info;
	uint32_t fd, j;

	if (infos == NULL || count == NULL)
		return (EINVAL);

	j = 0;
	for (fd = 0; j < cam_count && j < CAM_MAX_DEVICES; fd++) {
		memset(&info, 0, sizeof(info));
		info.id = j;
		snprintf(info.name, sizeof(info.name), "imx586-cam%d", j);
		info.min_width = 320;
		info.max_width = CAM_MAX_RES_WIDTH;
		info.max_height = CAM_MAX_RES_HEIGHT;
		info.max_fps = 60;
		info.formats = (1U << CAM_FORMAT_YUYV) | (1U << CAM_FORMAT_NV12) |
		    (1U << CAM_FORMAT_JPEG) | (1U << CAM_FORMAT_RAW10) |
		    (1U << CAM_FORMAT_RAW12);
		info.state = CAM_STATE_IDLE;
		infos[j++] = info;
	}

	*count = j;
	return (0);
}

struct cam_handle *
cam_open(uint32_t id)
{
	struct cam_handle *h;

	if (id >= cam_count)
		return (NULL);

	h = malloc(sizeof(*h), M_CAMERA, M_WAITOK | M_ZERO);
	if (h == NULL)
		return (NULL);

	h->id = id;
	h->state = CAM_STATE_IDLE;
	h->format = CAM_FORMAT_NV12;
	h->width = 1920;
	h->height = 1080;
	h->fps = 30;
	memset(h->buffers, 0, sizeof(h->buffers));

	cam_handles[id] = h;
	CAM_LOG(LOG_INFO, "Camera %u opened", id);

	return (h);
}

int
cam_close(struct cam_handle *h)
{
	if (h == NULL)
		return (EINVAL);

	if (h->state == CAM_STATE_STREAMING)
		cam_stop_stream(h);

	cam_free_buffers(h);
	cam_handles[h->id] = NULL;

	CAM_LOG(LOG_INFO, "Camera %u closed", h->id);
	free(h, M_CAMERA);

	return (0);
}

int
cam_start_stream(struct cam_handle *h, enum cam_format fmt,
    uint32_t width, uint32_t height, uint32_t fps)
{
	int ret;

	if (h == NULL)
		return (EINVAL);

	if (fmt <= 0 || fmt > CAM_FORMAT_RAW12)
		return (EINVAL);

	if (h->state == CAM_STATE_STREAMING)
		return (EBUSY);

	cam_free_buffers(h);

	h->format = fmt;
	h->width = width > CAM_MAX_RES_WIDTH ? CAM_MAX_RES_WIDTH : width;
	h->height = height > CAM_MAX_RES_HEIGHT ? CAM_MAX_RES_HEIGHT : height;
	h->fps = fps;

	ret = cam_alloc_buffers(h);
	if (ret)
		return (ret);

	h->state = CAM_STATE_STREAMING;

	CAM_LOG(LOG_INFO, "Camera %u: %ux%u@%d fps", h->id, h->width, h->height,
	    h->fps);

	return (0);
}

int
cam_stop_stream(struct cam_handle *h)
{
	if (h == NULL)
		return (EINVAL);

	if (h->state != CAM_STATE_STREAMING)
		return (EINVAL);

	cam_free_buffers(h);
	h->state = CAM_STATE_IDLE;

	CAM_LOG(LOG_INFO, "Camera %u stream stopped", h->id);
	return (0);
}

int
cam_read_frame(struct cam_handle *h, struct cam_frame *frame, int timeout)
{
	int ret;
	std::uniform_int_distribution<uint32_t> dist(0, 255);
	std::mt19937 rng(42);

	if (h == NULL || frame == NULL)
		return (EINVAL);

	if (h->state != CAM_STATE_STREAMING)
		return (EPERM);

	if (timeout != 0)
		sleep(1);

	uint32_t b_idx = (ticks % h->buf_count);

	if (h->buffers[b_idx].data == NULL)
		return (ENOBUFS);

	for (uint32_t i = 0; i < h->width * h->height; i++) {
		((uint8_t *)h->buffers[b_idx].data)[i] = (uint8_t)((rng() % 256);
	}

	memcpy(frame->data, h->buffers[b_idx].data, MIN(frame->size, h->buffers[b_idx].size));
	frame->width = h->width;
	frame->height = h->height;
	frame->format = h->format;
	frame->timestamp = ticks * 1000000ULL / hz;
	frame->seq = (uint32_t)(ticks);

	return (0);
}

static int
cam_internal_cb(struct cam_handle *h, struct cam_frame *f, void *arg)
{
	(void)h;
	(void)arg;
	CAM_LOG(LOG_DEBUG, "Frame ready: %dx%d, seq=%u, ts=%lldus", f->width, f->height, f->seq, f->timestamp);
	return (0);
}

int
cam_set_frame_callback(struct cam_handle *h, cam_frame_cb cb, void *arg)
{
	if (h == NULL)
		return (EINVAL);

	CAM_LOG(LOG_DEBUG, "Frame callback %c on camera %u", cb ? 'set' : 'unset', h->id);
	if (cb == NULL)
		return (0);

	return (0);
}

int
cam_get_device_info(uint32_t id, struct cam_device_info *info)
{
	if (info == NULL || id >= cam_count)
		return (EINVAL);

	snprintf(info->name, sizeof(info->name), "imx586-cam%d", id);
	info->id = id;
	info->max_width = CAM_MAX_RES_WIDTH;
	info->max_height = CAM_MAX_RES_HEIGHT;
	info->max_fps = 60;

	CAM_LOG(LOG_DEBUG, "Camera %u: %s", id, info->name);
	return (0);
}

void
cam_shutdown(void)
{
	uint32_t i;

	for (i = 0; i < cam_count; i++) {
		if (cam_handles[i])
			cam_close(cam_handles[i]);
	}

	CAM_LOG(LOG_INFO, "Camera subsystem shutdown");
}
