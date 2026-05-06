/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 *  Video for Linux Two header file
 *
 *  Copyright (C) 1999-2012 the contributors
 *  Copyright (C) 2012 Nokia Corporation
 *  Contact: Sakari Ailus <sakari.ailus@iki.fi>
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *  3. The names of its contributors may not be used to endorse or promote
 *     products derived from this software without specific prior written
 *     permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 *  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  Minimal V4L2 definitions for the FreeBSD UVC driver.
 *  Extracted from OpenBSD sys/videoio.h.
 */

#ifndef _UVIDEO_V4L2_H_
#define _UVIDEO_V4L2_H_

#include <sys/types.h>
#include <sys/ioccom.h>
#include <sys/time.h>

/*
 *  Four-character-code (FOURCC)
 */
#define v4l2_fourcc(a, b, c, d)	\
	((u_int32_t)(a) | ((u_int32_t)(b) << 8) | \
	 ((u_int32_t)(c) << 16) | ((u_int32_t)(d) << 24))

/*
 *  Enums
 */
enum v4l2_field {
	V4L2_FIELD_ANY           = 0,
	V4L2_FIELD_NONE          = 1,
	V4L2_FIELD_TOP           = 2,
	V4L2_FIELD_BOTTOM        = 3,
	V4L2_FIELD_INTERLACED    = 4,
	V4L2_FIELD_SEQ_TB        = 5,
	V4L2_FIELD_SEQ_BT        = 6,
	V4L2_FIELD_ALTERNATE     = 7,
	V4L2_FIELD_INTERLACED_TB = 8,
	V4L2_FIELD_INTERLACED_BT = 9,
};

enum v4l2_buf_type {
	V4L2_BUF_TYPE_VIDEO_CAPTURE        = 1,
	V4L2_BUF_TYPE_VIDEO_OUTPUT         = 2,
	V4L2_BUF_TYPE_VIDEO_OVERLAY        = 3,
	V4L2_BUF_TYPE_VBI_CAPTURE          = 4,
	V4L2_BUF_TYPE_VBI_OUTPUT           = 5,
	V4L2_BUF_TYPE_SLICED_VBI_CAPTURE   = 6,
	V4L2_BUF_TYPE_SLICED_VBI_OUTPUT    = 7,
	V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY = 8,
	V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE = 9,
	V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE  = 10,
	V4L2_BUF_TYPE_SDR_CAPTURE          = 11,
	V4L2_BUF_TYPE_SDR_OUTPUT           = 12,
	V4L2_BUF_TYPE_META_CAPTURE         = 13,
	V4L2_BUF_TYPE_META_OUTPUT          = 14,
	/* Deprecated, do not use */
	V4L2_BUF_TYPE_PRIVATE              = 0x80,
};

enum v4l2_memory {
	V4L2_MEMORY_MMAP             = 1,
	V4L2_MEMORY_USERPTR          = 2,
	V4L2_MEMORY_OVERLAY          = 3,
	V4L2_MEMORY_DMABUF           = 4,
};

enum v4l2_colorspace {
	V4L2_COLORSPACE_DEFAULT       = 0,
	V4L2_COLORSPACE_SMPTE170M     = 1,
	V4L2_COLORSPACE_SMPTE240M     = 2,
	V4L2_COLORSPACE_REC709        = 3,
	V4L2_COLORSPACE_BT878         = 4,
	V4L2_COLORSPACE_470_SYSTEM_M  = 5,
	V4L2_COLORSPACE_470_SYSTEM_BG = 6,
	V4L2_COLORSPACE_JPEG          = 7,
	V4L2_COLORSPACE_SRGB          = 8,
	V4L2_COLORSPACE_OPRGB         = 9,
	V4L2_COLORSPACE_BT2020        = 10,
	V4L2_COLORSPACE_RAW           = 11,
	V4L2_COLORSPACE_DCI_P3        = 12,
};

enum v4l2_xfer_func {
	V4L2_XFER_FUNC_DEFAULT     = 0,
	V4L2_XFER_FUNC_709         = 1,
	V4L2_XFER_FUNC_SRGB        = 2,
	V4L2_XFER_FUNC_OPRGB       = 3,
	V4L2_XFER_FUNC_SMPTE240M   = 4,
	V4L2_XFER_FUNC_NONE        = 5,
	V4L2_XFER_FUNC_DCI_P3      = 6,
	V4L2_XFER_FUNC_SMPTE2084   = 7,
};

enum v4l2_ycbcr_encoding {
	V4L2_YCBCR_ENC_DEFAULT        = 0,
	V4L2_YCBCR_ENC_601            = 1,
	V4L2_YCBCR_ENC_709            = 2,
	V4L2_YCBCR_ENC_XV601          = 3,
	V4L2_YCBCR_ENC_XV709          = 4,
	V4L2_YCBCR_ENC_SYCC           = 5,
	V4L2_YCBCR_ENC_BT2020         = 6,
	V4L2_YCBCR_ENC_BT2020_CONST_LUM = 7,
	V4L2_YCBCR_ENC_SMPTE240M      = 8,
};

enum v4l2_ctrl_type {
	V4L2_CTRL_TYPE_INTEGER       = 1,
	V4L2_CTRL_TYPE_BOOLEAN       = 2,
	V4L2_CTRL_TYPE_MENU          = 3,
	V4L2_CTRL_TYPE_BUTTON        = 4,
	V4L2_CTRL_TYPE_INTEGER64     = 5,
	V4L2_CTRL_TYPE_CTRL_CLASS    = 6,
	V4L2_CTRL_TYPE_STRING        = 7,
	V4L2_CTRL_TYPE_BITMASK       = 8,
	V4L2_CTRL_TYPE_INTEGER_MENU  = 9,
};

enum v4l2_frmsizetypes {
	V4L2_FRMSIZE_TYPE_DISCRETE   = 1,
	V4L2_FRMSIZE_TYPE_CONTINUOUS = 2,
	V4L2_FRMSIZE_TYPE_STEPWISE   = 3,
};

enum v4l2_frmivaltypes {
	V4L2_FRMIVAL_TYPE_DISCRETE   = 1,
	V4L2_FRMIVAL_TYPE_CONTINUOUS = 2,
	V4L2_FRMIVAL_TYPE_STEPWISE   = 3,
};

/*
 *  Structures
 */
struct v4l2_fract {
	u_int32_t   numerator;
	u_int32_t   denominator;
};

struct v4l2_capability {
	u_int8_t	driver[16];
	u_int8_t	card[32];
	u_int8_t	bus_info[32];
	u_int32_t	version;
	u_int32_t	capabilities;
	u_int32_t	device_caps;
	u_int32_t	reserved[3];
};

struct v4l2_pix_format {
	u_int32_t		width;
	u_int32_t		height;
	u_int32_t		pixelformat;
	u_int32_t		field;		/* enum v4l2_field */
	u_int32_t		bytesperline;	/* for padding, zero if unused */
	u_int32_t		sizeimage;
	u_int32_t		colorspace;	/* enum v4l2_colorspace */
	u_int32_t		priv;		/* private data, depends on pixelformat */
	u_int32_t		flags;		/* format flags (V4L2_PIX_FMT_FLAG_*) */
	union {
		/* enum v4l2_ycbcr_encoding */
		u_int32_t	ycbcr_enc;
		/* enum v4l2_hsv_encoding */
		u_int32_t	hsv_enc;
	};
	u_int32_t		quantization;	/* enum v4l2_quantization */
	u_int32_t		xfer_func;	/* enum v4l2_xfer_func */
};

/*
 * v4l2_format: the system header (v4l_compat) includes struct v4l2_window
 * (which contains a pointer) in the fmt union, forcing 8-byte alignment.
 * This creates 4 bytes of implicit padding after 'type' on LP64.
 * We must match this layout for ioctl ABI compatibility.
 * Total size: 4 (type) + 4 (pad) + 200 (union) = 208.
 */
struct v4l2_format {
	u_int32_t	type;
	u_int32_t	_pad;
	union {
		struct v4l2_pix_format	pix;
		u_int8_t		raw_data[200];
	} fmt;
};

struct v4l2_fmtdesc {
	u_int32_t		index;		/* Format number      */
	u_int32_t		type;		/* enum v4l2_buf_type */
	u_int32_t		flags;
	u_int8_t		description[32]; /* Description string */
	u_int32_t		pixelformat;	/* Format fourcc      */
	u_int32_t		mbus_code;	/* Media bus code    */
	u_int32_t		reserved[3];
};

struct v4l2_timecode {
	u_int32_t	type;
	u_int32_t	flags;
	u_int8_t	frames;
	u_int8_t	seconds;
	u_int8_t	minutes;
	u_int8_t	hours;
	u_int8_t	userbits[4];
};

struct v4l2_buffer {
	u_int32_t			index;
	u_int32_t			type;
	u_int32_t			bytesused;
	u_int32_t			flags;
	u_int32_t			field;
	struct timeval			timestamp;
	struct v4l2_timecode		timecode;
	u_int32_t			sequence;

	/* memory location */
	u_int32_t			memory;
	union {
		u_int32_t		offset;
		unsigned long		userptr;
		int32_t			fd;
	} m;
	u_int32_t			length;
	u_int32_t			reserved2;
	union {
		int32_t			request_fd;
		u_int32_t		reserved;
	};
};

struct v4l2_requestbuffers {
	u_int32_t		count;
	u_int32_t		type;		/* enum v4l2_buf_type */
	u_int32_t		memory;		/* enum v4l2_memory */
	u_int32_t		capabilities;
	u_int8_t		flags;
	u_int8_t		reserved[3];
};

struct v4l2_captureparm {
	u_int32_t		capability;	/*  Supported modes */
	u_int32_t		capturemode;	/*  Current mode */
	struct v4l2_fract	timeperframe;	/*  Time per frame in seconds */
	u_int32_t		extendedmode;	/*  Driver-specific extensions */
	u_int32_t		readbuffers;	/*  # of buffers for read */
	u_int32_t		reserved[4];
};

/* Simplified v4l2_streamparm: only the capture member is included */
struct v4l2_streamparm {
	u_int32_t	type;			/* enum v4l2_buf_type */
	union {
		struct v4l2_captureparm	capture;
		u_int8_t		raw_data[200];
	} parm;
};

struct v4l2_input {
	u_int32_t	index;		/*  Which input */
	u_int8_t	name[32];	/*  Label */
	u_int32_t	type;		/*  Type of input */
	u_int32_t	audioset;	/*  Associated audios (bitfield) */
	u_int32_t	tuner;		/*  Tuner index */
	u_int64_t	std;
	u_int32_t	status;
	u_int32_t	capabilities;
	u_int32_t	reserved[3];
};

struct v4l2_control {
	u_int32_t	id;
	int32_t		value;
};

struct v4l2_queryctrl {
	u_int32_t	id;
	u_int32_t	type;		/* enum v4l2_ctrl_type */
	u_int8_t	name[32];	/* Whatever */
	int32_t		minimum;	/* Note signedness */
	int32_t		maximum;
	int32_t		step;
	int32_t		default_value;
	u_int32_t	flags;
	u_int32_t	reserved[2];
};

struct v4l2_frmsize_discrete {
	u_int32_t	width;		/* Frame width [pixel] */
	u_int32_t	height;		/* Frame height [pixel] */
};

struct v4l2_frmsize_stepwise {
	u_int32_t	min_width;	/* Minimum frame width [pixel] */
	u_int32_t	max_width;	/* Maximum frame width [pixel] */
	u_int32_t	step_width;	/* Frame width step size [pixel] */
	u_int32_t	min_height;	/* Minimum frame height [pixel] */
	u_int32_t	max_height;	/* Maximum frame height [pixel] */
	u_int32_t	step_height;	/* Frame height step size [pixel] */
};

struct v4l2_frmsizeenum {
	u_int32_t	index;		/* Frame size number */
	u_int32_t	pixel_format;	/* Pixel format */
	u_int32_t	type;		/* Frame size type the device supports. */

	union {				/* Frame size */
		struct v4l2_frmsize_discrete	discrete;
		struct v4l2_frmsize_stepwise	stepwise;
	};

	u_int32_t	reserved[2];	/* Reserved space for future use */
};

struct v4l2_frmival_stepwise {
	struct v4l2_fract	min;	/* Minimum frame interval [s] */
	struct v4l2_fract	max;	/* Maximum frame interval [s] */
	struct v4l2_fract	step;	/* Frame interval step size [s] */
};

struct v4l2_frmivalenum {
	u_int32_t	index;		/* Frame format index */
	u_int32_t	pixel_format;	/* Pixel format */
	u_int32_t	width;		/* Frame width */
	u_int32_t	height;		/* Frame height */
	u_int32_t	type;		/* Frame interval type the device supports. */

	union {				/* Frame interval */
		struct v4l2_fract		discrete;
		struct v4l2_frmival_stepwise	stepwise;
	};

	u_int32_t	reserved[2];	/* Reserved space for future use */
};

/*
 *  Pixel formats
 */

/* Luminance+Chrominance formats */
#define V4L2_PIX_FMT_YUYV	v4l2_fourcc('Y', 'U', 'Y', 'V')
#define V4L2_PIX_FMT_YVU420	v4l2_fourcc('Y', 'V', '1', '2')
#define V4L2_PIX_FMT_YUV420	v4l2_fourcc('Y', 'U', '1', '2')

/* Compressed formats */
#define V4L2_PIX_FMT_MJPEG	v4l2_fourcc('M', 'J', 'P', 'G')
#define V4L2_PIX_FMT_H264	v4l2_fourcc('H', '2', '6', '4')
#define V4L2_PIX_FMT_HEVC	v4l2_fourcc('H', 'E', 'V', 'C')

/* Grey formats */
#define V4L2_PIX_FMT_GREY	v4l2_fourcc('G', 'R', 'E', 'Y')
#define V4L2_PIX_FMT_Y10	v4l2_fourcc('Y', '1', '0', ' ')

/* RGB formats */
#define V4L2_PIX_FMT_RGB565	v4l2_fourcc('R', 'G', 'B', 'P')
#define V4L2_PIX_FMT_BGR24	v4l2_fourcc('B', 'G', 'R', '3')
#define V4L2_PIX_FMT_XBGR32	v4l2_fourcc('X', 'R', '2', '4')

/* Bayer formats */
#define V4L2_PIX_FMT_SBGGR8	v4l2_fourcc('B', 'A', '8', '1')
#define V4L2_PIX_FMT_SGBRG8	v4l2_fourcc('G', 'B', 'R', 'G')
#define V4L2_PIX_FMT_SGRBG8	v4l2_fourcc('G', 'R', 'B', 'G')
#define V4L2_PIX_FMT_SRGGB8	v4l2_fourcc('R', 'G', 'G', 'B')
#define V4L2_PIX_FMT_SBGGR16	v4l2_fourcc('B', 'Y', 'R', '2')
#define V4L2_PIX_FMT_SGBRG16	v4l2_fourcc('G', 'B', '1', '6')
#define V4L2_PIX_FMT_SRGGB16	v4l2_fourcc('R', 'G', '1', '6')
#define V4L2_PIX_FMT_SGRBG16	v4l2_fourcc('G', 'R', '1', '6')
#define V4L2_PIX_FMT_SRGGB10P	v4l2_fourcc('p', 'R', 'A', 'A')

/* Depth format */
#define V4L2_PIX_FMT_Z16	v4l2_fourcc('Z', '1', '6', ' ')

/*
 *  Capability flags
 */
#define V4L2_CAP_VIDEO_CAPTURE		0x00000001
#define V4L2_CAP_STREAMING		0x04000000
#define V4L2_CAP_EXT_PIX_FORMAT		0x00200000
#define V4L2_CAP_READWRITE		0x01000000
#define V4L2_CAP_DEVICE_CAPS		0x80000000
#define V4L2_CAP_TIMEPERFRAME		0x1000

/*
 *  Buffer flags
 */
#define V4L2_BUF_FLAG_MAPPED		0x00000001
#define V4L2_BUF_FLAG_QUEUED		0x00000002
#define V4L2_BUF_FLAG_DONE		0x00000004
#define V4L2_BUF_FLAG_ERROR		0x00000040
#define V4L2_BUF_FLAG_TIMESTAMP_MASK	0x0000e000
#define V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC 0x00002000
#define V4L2_BUF_FLAG_TSTAMP_SRC_MASK	0x00070000
#define V4L2_BUF_FLAG_TSTAMP_SRC_EOF	0x00000000
#define V4L2_BUF_FLAG_TIMECODE		0x00000100

/*
 *  Format flags
 */
#define V4L2_FMT_FLAG_COMPRESSED	0x0001

/*
 *  Buffer capabilities
 */
#define V4L2_BUF_CAP_SUPPORTS_MMAP	(1 << 0)

/*
 *  Input types
 */
#define V4L2_INPUT_TYPE_CAMERA		2

/*
 *  Control IDs
 */
#define V4L2_CTRL_CLASS_USER		0x00980000
#define V4L2_CID_BASE			(V4L2_CTRL_CLASS_USER | 0x900)
#define V4L2_CID_BRIGHTNESS		(V4L2_CID_BASE + 0)
#define V4L2_CID_CONTRAST		(V4L2_CID_BASE + 1)
#define V4L2_CID_SATURATION		(V4L2_CID_BASE + 2)
#define V4L2_CID_HUE			(V4L2_CID_BASE + 3)
#define V4L2_CID_AUTO_WHITE_BALANCE	(V4L2_CID_BASE + 12)
#define V4L2_CID_RED_BALANCE		(V4L2_CID_BASE + 14)
#define V4L2_CID_BLUE_BALANCE		(V4L2_CID_BASE + 15)
#define V4L2_CID_GAMMA			(V4L2_CID_BASE + 16)
#define V4L2_CID_GAIN			(V4L2_CID_BASE + 19)
#define V4L2_CID_POWER_LINE_FREQUENCY	(V4L2_CID_BASE + 24)
#define V4L2_CID_HUE_AUTO		(V4L2_CID_BASE + 25)
#define V4L2_CID_WHITE_BALANCE_TEMPERATURE (V4L2_CID_BASE + 26)
#define V4L2_CID_SHARPNESS		(V4L2_CID_BASE + 27)
#define V4L2_CID_BACKLIGHT_COMPENSATION	(V4L2_CID_BASE + 28)

#define V4L2_CID_CAMERA_CLASS_BASE	0x009a0000
#define V4L2_CID_EXPOSURE_AUTO		(V4L2_CID_CAMERA_CLASS_BASE + 1)
#define V4L2_CID_EXPOSURE_ABSOLUTE	(V4L2_CID_CAMERA_CLASS_BASE + 2)
#define V4L2_CID_EXPOSURE_AUTO_PRIORITY	(V4L2_CID_CAMERA_CLASS_BASE + 3)
#define V4L2_CID_FOCUS_ABSOLUTE		(V4L2_CID_CAMERA_CLASS_BASE + 4)
#define V4L2_CID_FOCUS_RELATIVE		(V4L2_CID_CAMERA_CLASS_BASE + 5)
#define V4L2_CID_PAN_ABSOLUTE		(V4L2_CID_CAMERA_CLASS_BASE + 8)
#define V4L2_CID_TILT_ABSOLUTE		(V4L2_CID_CAMERA_CLASS_BASE + 9)
#define V4L2_CID_FOCUS_AUTO		(V4L2_CID_CAMERA_CLASS_BASE + 12)
#define V4L2_CID_ZOOM_ABSOLUTE		(V4L2_CID_CAMERA_CLASS_BASE + 13)
#define V4L2_CID_ZOOM_CONTINUOUS	(V4L2_CID_CAMERA_CLASS_BASE + 15)
#define V4L2_CID_PRIVACY		(V4L2_CID_CAMERA_CLASS_BASE + 16)

/*
 *  V4L2 ioctl definitions
 */
#define VIDIOC_QUERYCAP		_IOR('V',  0, struct v4l2_capability)
#define VIDIOC_ENUM_FMT		_IOWR('V',  2, struct v4l2_fmtdesc)
#define VIDIOC_G_FMT		_IOWR('V',  4, struct v4l2_format)
#define VIDIOC_S_FMT		_IOWR('V',  5, struct v4l2_format)
#define VIDIOC_REQBUFS		_IOWR('V',  8, struct v4l2_requestbuffers)
#define VIDIOC_QUERYBUF		_IOWR('V',  9, struct v4l2_buffer)
#define VIDIOC_QBUF		_IOWR('V', 15, struct v4l2_buffer)
#define VIDIOC_DQBUF		_IOWR('V', 17, struct v4l2_buffer)
#define VIDIOC_STREAMON		_IOW('V', 18, int)
#define VIDIOC_STREAMOFF	_IOW('V', 19, int)
#define VIDIOC_G_PARM		_IOWR('V', 21, struct v4l2_streamparm)
#define VIDIOC_S_PARM		_IOWR('V', 22, struct v4l2_streamparm)
#define VIDIOC_ENUMINPUT	_IOWR('V', 26, struct v4l2_input)
#define VIDIOC_G_CTRL		_IOWR('V', 27, struct v4l2_control)
#define VIDIOC_S_CTRL		_IOWR('V', 28, struct v4l2_control)
#define VIDIOC_QUERYCTRL	_IOWR('V', 36, struct v4l2_queryctrl)
#define VIDIOC_G_INPUT		_IOR('V', 38, int)
#define VIDIOC_S_INPUT		_IOWR('V', 39, int)
#define VIDIOC_G_PRIORITY	_IOR('V', 67, u_int32_t)
#define VIDIOC_S_PRIORITY	_IOW('V', 68, u_int32_t)
#define VIDIOC_TRY_FMT		_IOWR('V', 64, struct v4l2_format)
#define VIDIOC_ENUM_FRAMESIZES	_IOWR('V', 74, struct v4l2_frmsizeenum)
#define VIDIOC_ENUM_FRAMEINTERVALS _IOWR('V', 75, struct v4l2_frmivalenum)

#endif /* _UVIDEO_V4L2_H_ */
