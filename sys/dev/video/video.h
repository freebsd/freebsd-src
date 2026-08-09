/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _DEV_VIDEO_VIDEO_H_
#define _DEV_VIDEO_VIDEO_H_

#include <sys/types.h>
#include <sys/bus.h>

struct video_device;
struct video_buf;

struct video_format {
	uint32_t	pixelformat;	/* V4L2 fourcc */
	uint32_t	width;
	uint32_t	height;
	uint32_t	bytesperline;
	uint32_t	sizeimage;
	uint32_t	field;
	uint32_t	colorspace;
	uint32_t	xfer_func;
	uint32_t	ycbcr_enc;
	uint32_t	flags;		/* V4L2_FMT_FLAG_* */
	char		description[32];
};

struct video_fract {
	uint32_t	numerator;
	uint32_t	denominator;
};

struct video_caps {
	char		driver[16];
	char		card[32];
	char		bus_info[32];
	uint32_t	version;
	uint32_t	capabilities;
};

#define	VIDEO_CAP_CAPTURE	0x00000001	/* V4L2_CAP_VIDEO_CAPTURE */
#define	VIDEO_CAP_READWRITE	0x01000000	/* V4L2_CAP_READWRITE */
#define	VIDEO_CAP_STREAMING	0x04000000	/* V4L2_CAP_STREAMING */

struct video_input {
	uint32_t	index;
	char		name[32];
	uint32_t	type;
};

#define	VIDEO_INPUT_TYPE_CAMERA	2

struct video_control_desc {
	uint32_t	id;
	uint32_t	type;
	char		name[32];
	int32_t		minimum;
	int32_t		maximum;
	int32_t		step;
	int32_t		default_value;
	uint32_t	flags;
};

struct video_control {
	uint32_t	id;
	int32_t		value;
};

struct video_frmsizeenum {
	uint32_t	index;
	uint32_t	pixelformat;
	uint32_t	type;
	union {
		struct {
			uint32_t	width;
			uint32_t	height;
		} discrete;
		struct {
			uint32_t	min_width;
			uint32_t	max_width;
			uint32_t	step_width;
			uint32_t	min_height;
			uint32_t	max_height;
			uint32_t	step_height;
		} stepwise;
	};
};

struct video_frmivalenum {
	uint32_t		index;
	uint32_t		pixelformat;
	uint32_t		width;
	uint32_t		height;
	uint32_t		type;
	union {
		struct video_fract	discrete;
		struct {
			struct video_fract	min;
			struct video_fract	max;
			struct video_fract	step;
		} stepwise;
	};
};

int	video_register(device_t dev, struct video_device **vdp);
void	video_unregister(struct video_device *vd);

struct video_buf	*video_buf_acquire(struct video_device *vd);
int			 video_buf_write(struct video_buf *vb,
			    size_t offset, const void *src, size_t len);
void			 video_buf_done(struct video_buf *vb,
			    size_t bytesused, uint32_t sequence);
void			 video_buf_error(struct video_buf *vb);

#endif /* _DEV_VIDEO_VIDEO_H_ */
