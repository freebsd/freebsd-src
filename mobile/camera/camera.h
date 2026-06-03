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

#ifndef _MOBILE_CAMERA_CAMERA_H_
#define _MOBILE_CAMERA_CAMERA_H_

#include <sys/types.h>
#include <sys/param.h>
#include <stdint.h>
#include <stdbool.h>

#define CAM_MAX_DEVICES	4
#define CAM_MAX_BUFFERS	16

enum cam_format {
	CAM_FORMAT_YUYV	= 0x59555956,
	CAM_FORMAT_NV12	= 0x3231564E,
	CAM_FORMAT_JPEG	= 0x3247504A,
	CAM_FORMAT_RAW10	= 0x4D505031,
	CAM_FORMAT_RAW12	= 0x4D505032,
};

enum cam_state {
	CAM_STATE_IDLE,
	CAM_STATE_STREAMING,
	CAM_STATE_STOPPING,
	CAM_STATE_ERROR,
};

struct cam_frame {
	uint8_t	*data;
	uint32_t	size;
	uint32_t	width;
	uint32_t	height;
	enum cam_format format;
	int64_t	timestamp;
	uint32_t	seq;
	void	*userdata;
};

struct cam_handle {
	uint32_t id;
	enum cam_state state;
	enum cam_format format;
	uint32_t width;
	uint32_t height;
	uint32_t fps;
	struct cam_buffer {
		void *data;
		uint32_t size;
		uint8_t queued;
	} buffers[CAM_MAX_BUFFERS];
	uint32_t buf_count;
};

typedef int (*cam_frame_cb)(struct cam_handle *h, struct cam_frame *f, void *arg);

#define CAM_IOCTL_GETCAP	_IOWR('C', 0x01, uint32_t)
#define CAM_IOCTL_ENUM		_IOWR('C', 0x02, uint32_t)
#define CAM_IOCTL_OPEN		_IOWR('C', 0x03, uint32_t)
#define CAM_IOCTL_CLOSE		_IOW('C',  0x04, uint32_t)
#define CAM_IOCTL_START		_IOW('C',  0x05, struct cam_handle)
#define CAM_IOCTL_STOP		_IO('C',  0x06)
#define CAM_IOCTL_READ		_IOWR('C', 0x07, struct cam_frame)
#define CAM_IOCTL_SETCB		_IOW('C',  0x08, cam_frame_cb)
#define CAM_IOCTL_SETFORMAT	_IOW('C',  0x09, struct cam_handle)
#define CAM_IOCTL_GETINFO	_IOWR('C', 0x0A, uint32_t)

#define CAM_V4L2_PATH		"/dev/video%d"
#define CAM_MAX_RES_WIDTH	3840
#define CAM_MAX_RES_HEIGHT	2160

struct cam_device_info {
	uint32_t id;
	char name[64];
	uint32_t min_width;
	uint32_t max_width;
	uint32_t max_height;
	uint32_t max_fps;
	uint32_t formats;
	enum cam_state state;
};

int cam_init(void);
int cam_enumerate(struct cam_device_info infos[CAM_MAX_DEVICES], uint32_t *count);
struct cam_handle *cam_open(uint32_t id);
int cam_close(struct cam_handle *h);
int cam_start_stream(struct cam_handle *h, enum cam_format fmt,
    uint32_t width, uint32_t height, uint32_t fps);
int cam_stop_stream(struct cam_handle *h);
int cam_read_frame(struct cam_handle *h, struct cam_frame *frame, int timeout);
int cam_set_frame_callback(struct cam_handle *h, cam_frame_cb cb, void *arg);
int cam_get_device_info(uint32_t id, struct cam_device_info *info);
void cam_shutdown(void);

#endif /* _MOBILE_CAMERA_CAMERA_H_ */
