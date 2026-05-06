/*-
 * SPDX-License-Identifier: ISC
 *
 * Copyright (c) 2008 Robert Nagy <robert@openbsd.org>
 * Copyright (c) 2008 Marcus Glocker <mglocker@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Ported from OpenBSD to FreeBSD by Baptiste Daroussin <bapt@FreeBSD.org>
 */

/*
 * USB Video Class (UVC) driver.
 *
 * Implements standard UVC 1.0/1.1/1.5 devices only.
 * Creates /dev/videoN character devices with V4L2 ioctl interface.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/event.h>
#include <sys/selinfo.h>
#include <sys/limits.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usb_request.h>
#include "usbdevs.h"

#include <dev/usb/video/uvideo.h>

#define	USB_DEBUG_VAR uvideo_debug
#include <dev/usb/usb_debug.h>

static SYSCTL_NODE(_hw_usb, OID_AUTO, uvideo, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "USB uvideo");

#ifdef USB_DEBUG
static int uvideo_debug = 0;

SYSCTL_INT(_hw_usb_uvideo, OID_AUTO, debug, CTLFLAG_RWTUN,
    &uvideo_debug, 0, "Debug level");
#endif

#define	byteof(x)	((x) >> 3)
#define	bitof(x)	(1L << ((x) & 0x7))

/* OpenBSD macros not present in FreeBSD USB headers */
#define	UE_GET_SIZE(x)	((x) & 0x7FF)
#define	UE_GET_TRANS(x)	(((x) >> 11) & 0x03)

/* IO_NDELAY from sys/vnode.h - avoid pulling in vnode_if.h dependency */
#ifndef IO_NDELAY
#define	IO_NDELAY	0x0004
#endif

/* Forward declarations */
struct uvideo_softc;

static device_probe_t	uvideo_probe;
static device_attach_t	uvideo_attach;
static device_detach_t	uvideo_detach;

static usb_callback_t	uvideo_isoc_callback;
static usb_callback_t	uvideo_bulk_callback;

static usb_error_t	uvideo_vc_parse_desc(struct uvideo_softc *);
static usb_error_t	uvideo_vc_parse_desc_header(struct uvideo_softc *,
			    const struct usb_descriptor *);
static usb_error_t	uvideo_vc_parse_desc_pu(struct uvideo_softc *,
			    const struct usb_descriptor *);
static usb_error_t	uvideo_vc_parse_desc_ct(struct uvideo_softc *,
			    const struct usb_descriptor *);
static int		uvideo_has_ct_ctrl(
			    struct usb_video_camera_terminal_desc *, int);
static usb_error_t	uvideo_vc_get_ctrl(struct uvideo_softc *, uint8_t *,
			    uint8_t, uint8_t, uint16_t, uint16_t);
static usb_error_t	uvideo_vc_set_ctrl(struct uvideo_softc *, uint8_t *,
			    uint8_t, uint8_t, uint16_t, uint16_t);
static int		uvideo_find_ctrl(struct uvideo_softc *, int);
static int		uvideo_has_ctrl(struct usb_video_vc_processing_desc *,
			    int);

static usb_error_t	uvideo_vs_parse_desc(struct uvideo_softc *,
			    struct usb_config_descriptor *);
static usb_error_t	uvideo_vs_parse_desc_input_header(struct uvideo_softc *,
			    const struct usb_descriptor *);
static usb_error_t	uvideo_vs_parse_desc_format(struct uvideo_softc *);
static void		uvideo_vs_parse_desc_colorformat(struct uvideo_softc *,
			    const struct usb_descriptor *);
static void		uvideo_vs_parse_desc_format_frame_based(
			    struct uvideo_softc *,
			    const struct usb_descriptor *);
static void		uvideo_vs_parse_desc_format_h264(struct uvideo_softc *,
			    const struct usb_descriptor *);
static void		uvideo_vs_parse_desc_format_mjpeg(struct uvideo_softc *,
			    const struct usb_descriptor *);
static void		uvideo_vs_parse_desc_format_uncompressed(
			    struct uvideo_softc *,
			    const struct usb_descriptor *);
static usb_error_t	uvideo_vs_parse_desc_frame(struct uvideo_softc *);
static usb_error_t	uvideo_vs_parse_desc_frame_buffer_size(
			    struct uvideo_softc *,
			    const struct usb_descriptor *);
static usb_error_t	uvideo_vs_parse_desc_frame_max_rate(
			    struct uvideo_softc *,
			    const struct usb_descriptor *);
static usb_error_t	uvideo_vs_parse_desc_alt(struct uvideo_softc *, int,
			    int, int);
static int		uvideo_desc_len(const struct usb_descriptor *, int,
			    int, int, int);
static void		uvideo_find_res(struct uvideo_softc *, int, int, int,
			    struct uvideo_res *);
static usb_error_t	uvideo_vs_negotiation(struct uvideo_softc *, int);
static usb_error_t	uvideo_vs_set_probe(struct uvideo_softc *, uint8_t *);
static usb_error_t	uvideo_vs_get_probe(struct uvideo_softc *, uint8_t *,
			    uint8_t);
static usb_error_t	uvideo_vs_set_commit(struct uvideo_softc *, uint8_t *);
static usb_error_t	uvideo_vs_alloc_frame(struct uvideo_softc *);
static void		uvideo_vs_free_frame(struct uvideo_softc *);
static usb_error_t	uvideo_vs_open(struct uvideo_softc *);
static void		uvideo_vs_close(struct uvideo_softc *);
static usb_error_t	uvideo_vs_init(struct uvideo_softc *);
static void		uvideo_vs_decode_stream_header(struct uvideo_softc *,
			    uint8_t *, int);
static void		uvideo_isoc_decode(struct uvideo_softc *,
			    struct usb_page_cache *, int, int);
static uint8_t		*uvideo_mmap_getbuf(struct uvideo_softc *);
static void		uvideo_mmap_queue(struct uvideo_softc *, int, int);
static void		uvideo_read_frame(struct uvideo_softc *, uint8_t *, int);

static d_open_t		uvideo_cdev_open;
static d_close_t	uvideo_cdev_close;
static d_read_t		uvideo_cdev_read;
static d_ioctl_t	uvideo_cdev_ioctl;
static d_poll_t		uvideo_cdev_poll;
static d_kqfilter_t	uvideo_cdev_kqfilter;
static d_mmap_t		uvideo_cdev_mmap;

static int	uvideo_querycap(struct uvideo_softc *, struct v4l2_capability *);
static int	uvideo_enum_fmt(struct uvideo_softc *, struct v4l2_fmtdesc *);
static int	uvideo_enum_fsizes(struct uvideo_softc *,
		    struct v4l2_frmsizeenum *);
static int	uvideo_enum_fivals(struct uvideo_softc *,
		    struct v4l2_frmivalenum *);
static int	uvideo_s_fmt(struct uvideo_softc *, struct v4l2_format *);
static int	uvideo_g_fmt(struct uvideo_softc *, struct v4l2_format *);
static int	uvideo_s_parm(struct uvideo_softc *, struct v4l2_streamparm *);
static int	uvideo_g_parm(struct uvideo_softc *, struct v4l2_streamparm *);
static int	uvideo_enum_input(struct uvideo_softc *, struct v4l2_input *);
static int	uvideo_s_input(struct uvideo_softc *, int);
static int	uvideo_g_input(struct uvideo_softc *, int *);
static int	uvideo_reqbufs(struct uvideo_softc *,
		    struct v4l2_requestbuffers *);
static int	uvideo_querybuf(struct uvideo_softc *, struct v4l2_buffer *);
static int	uvideo_qbuf(struct uvideo_softc *, struct v4l2_buffer *);
static int	uvideo_dqbuf(struct uvideo_softc *, struct v4l2_buffer *);
static int	uvideo_streamon(struct uvideo_softc *, int);
static int	uvideo_streamoff(struct uvideo_softc *, int);
static int	uvideo_try_fmt(struct uvideo_softc *, struct v4l2_format *);
static int	uvideo_queryctrl(struct uvideo_softc *,
		    struct v4l2_queryctrl *);
static int	uvideo_g_ctrl(struct uvideo_softc *, struct v4l2_control *);
static int	uvideo_s_ctrl(struct uvideo_softc *, struct v4l2_control *);

/*
 * Transfer configuration indices.
 */
enum {
	UVIDEO_ISOC_RX_0,
	UVIDEO_ISOC_RX_1,
	UVIDEO_ISOC_RX_2,
	UVIDEO_BULK_RX,
	UVIDEO_N_XFER
};

/*
 * The softc structure.
 */
struct uvideo_softc {
	device_t		sc_dev;
	struct usb_device	*sc_udev;
	struct mtx		sc_mtx;
	struct cdev		*sc_cdev;
	int			sc_unit;

	uint8_t			sc_iface_index;
	uint8_t			sc_nifaces;
	int			sc_dying;
	int			sc_open;
	uint32_t		sc_priority;
	struct proc		*sc_owner;

	struct usb_xfer		*sc_xfer[UVIDEO_N_XFER];
	int			sc_streaming;

	int			sc_max_ctrl_size;
	int			sc_max_fbuf_size;
	int			sc_negotiated_flag;
	int			sc_frame_rate;

	struct uvideo_frame_buffer sc_frame_buffer;

	struct uvideo_mmap	sc_mmap[UVIDEO_MAX_BUFFERS];
	struct uvideo_mmap	*sc_mmap_cur;
	uint8_t			*sc_mmap_buffer;
	size_t			sc_mmap_buffer_size;
	int			sc_mmap_buffer_idx;
	q_mmap			sc_mmap_q;
	int			sc_mmap_count;
	int			sc_mmap_flag;

	uint8_t			*sc_tmpbuf;
	int			sc_tmpbuf_size;

	int			sc_nframes;
	struct usb_video_probe_commit sc_desc_probe;
	struct usb_video_header_desc_all sc_desc_vc_header;
	struct usb_video_input_header_desc_all sc_desc_vs_input_header;

#define	UVIDEO_MAX_PU		8
	int			sc_desc_vc_pu_num;
	struct usb_video_vc_processing_desc *sc_desc_vc_pu_cur;
	struct usb_video_vc_processing_desc *sc_desc_vc_pu[UVIDEO_MAX_PU];

#define	UVIDEO_MAX_CT		8
	int			sc_desc_vc_ct_num;
	struct usb_video_camera_terminal_desc *sc_desc_vc_ct_cur;
	struct usb_video_camera_terminal_desc *sc_desc_vc_ct[UVIDEO_MAX_CT];

#define	UVIDEO_MAX_FORMAT	8
	int			sc_fmtgrp_idx;
	int			sc_fmtgrp_num;
	struct uvideo_format_group *sc_fmtgrp_cur;
	struct uvideo_format_group sc_fmtgrp[UVIDEO_MAX_FORMAT];

#define	UVIDEO_MAX_VS_NUM	8
	struct uvideo_vs_iface	*sc_vs_cur;
	struct uvideo_vs_iface	sc_vs_coll[UVIDEO_MAX_VS_NUM];

	int			sc_fsize;
	uint8_t			*sc_fbuffer;
	size_t			sc_fbufferlen;
	int			sc_vidmode;
#define	VIDMODE_NONE	0
#define	VIDMODE_MMAP	1
#define	VIDMODE_READ	2
	int			sc_frames_ready;

	struct selinfo		sc_selinfo;

	void			(*sc_decode_stream_header)(
				    struct uvideo_softc *, uint8_t *, int);
};

/*
 * Processing Unit control descriptors
 */
static struct uvideo_controls uvideo_ctrls[] = {
	{
	    V4L2_CID_BRIGHTNESS,
	    V4L2_CTRL_TYPE_INTEGER,
	    "Brightness",
	    0,
	    PU_BRIGHTNESS_CONTROL,
	    2,
	    1
	},
	{
	    V4L2_CID_CONTRAST,
	    V4L2_CTRL_TYPE_INTEGER,
	    "Contrast",
	    1,
	    PU_CONTRAST_CONTROL,
	    2,
	    0
	},
	{
	    V4L2_CID_HUE,
	    V4L2_CTRL_TYPE_INTEGER,
	    "Hue",
	    2,
	    PU_HUE_CONTROL,
	    2,
	    1
	},
	{
	    V4L2_CID_SATURATION,
	    V4L2_CTRL_TYPE_INTEGER,
	    "Saturation",
	    3,
	    PU_SATURATION_CONTROL,
	    2,
	    0
	},
	{
	    V4L2_CID_SHARPNESS,
	    V4L2_CTRL_TYPE_INTEGER,
	    "Sharpness",
	    4,
	    PU_SHARPNESS_CONTROL,
	    2,
	    0
	},
	{
	    V4L2_CID_GAMMA,
	    V4L2_CTRL_TYPE_INTEGER,
	    "Gamma",
	    5,
	    PU_GAMMA_CONTROL,
	    2,
	    0
	},
	{
	    V4L2_CID_WHITE_BALANCE_TEMPERATURE,
	    V4L2_CTRL_TYPE_INTEGER,
	    "White Balance Temperature",
	    6,
	    PU_WHITE_BALANCE_TEMPERATURE_CONTROL,
	    2,
	    0
	},
	{
	    V4L2_CID_BACKLIGHT_COMPENSATION,
	    V4L2_CTRL_TYPE_INTEGER,
	    "Backlight Compensation",
	    8,
	    PU_BACKLIGHT_COMPENSATION_CONTROL,
	    2,
	    0
	},
	{
	    V4L2_CID_GAIN,
	    V4L2_CTRL_TYPE_INTEGER,
	    "Gain",
	    9,
	    PU_GAIN_CONTROL,
	    2,
	    0
	},
	{
	    V4L2_CID_POWER_LINE_FREQUENCY,
	    V4L2_CTRL_TYPE_MENU,
	    "Power Line Frequency",
	    10,
	    PU_POWER_LINE_FREQUENCY_CONTROL,
	    2,
	    0
	},
	{
	    V4L2_CID_HUE_AUTO,
	    V4L2_CTRL_TYPE_BOOLEAN,
	    "Hue Auto",
	    11,
	    PU_HUE_AUTO_CONTROL,
	    1,
	    0
	},
	{
	    V4L2_CID_AUTO_WHITE_BALANCE,
	    V4L2_CTRL_TYPE_BOOLEAN,
	    "White Balance Temperature Auto",
	    12,
	    PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL,
	    1,
	    0
	},
	{
	    V4L2_CID_AUTO_WHITE_BALANCE,
	    V4L2_CTRL_TYPE_BOOLEAN,
	    "White Balance Component Auto",
	    13,
	    PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL,
	    1,
	    0
	},
	/* Camera Terminal Controls (UVC 1.5 spec Table A-12) */
	{
	    V4L2_CID_EXPOSURE_AUTO,
	    V4L2_CTRL_TYPE_MENU,
	    "Exposure, Auto",
	    1,
	    CT_AE_MODE_CONTROL,
	    1,
	    0
	},
	{
	    V4L2_CID_EXPOSURE_AUTO_PRIORITY,
	    V4L2_CTRL_TYPE_BOOLEAN,
	    "Exposure, Auto Priority",
	    2,
	    CT_AE_PRIORITY_CONTROL,
	    1,
	    0
	},
	{
	    V4L2_CID_EXPOSURE_ABSOLUTE,
	    V4L2_CTRL_TYPE_INTEGER,
	    "Exposure (Absolute)",
	    3,
	    CT_EXPOSURE_TIME_ABSOLUTE_CONTROL,
	    4,
	    0
	},
	{
	    V4L2_CID_FOCUS_ABSOLUTE,
	    V4L2_CTRL_TYPE_INTEGER,
	    "Focus (Absolute)",
	    5,
	    CT_FOCUS_ABSOLUTE_CONTROL,
	    2,
	    0
	},
	{
	    V4L2_CID_FOCUS_AUTO,
	    V4L2_CTRL_TYPE_BOOLEAN,
	    "Focus, Auto",
	    17,
	    CT_FOCUS_AUTO_CONTROL,
	    1,
	    0
	},
	{
	    V4L2_CID_ZOOM_ABSOLUTE,
	    V4L2_CTRL_TYPE_INTEGER,
	    "Zoom (Absolute)",
	    9,
	    CT_ZOOM_ABSOLUTE_CONTROL,
	    2,
	    0
	},
	{
	    V4L2_CID_PAN_ABSOLUTE,
	    V4L2_CTRL_TYPE_INTEGER,
	    "Pan (Absolute)",
	    11,
	    CT_PANTILT_ABSOLUTE_CONTROL,
	    4,
	    1
	},
	{
	    V4L2_CID_TILT_ABSOLUTE,
	    V4L2_CTRL_TYPE_INTEGER,
	    "Tilt (Absolute)",
	    11,
	    CT_PANTILT_ABSOLUTE_CONTROL,
	    4,
	    1
	},
	{
	    V4L2_CID_PRIVACY,
	    V4L2_CTRL_TYPE_BOOLEAN,
	    "Privacy",
	    18,
	    CT_PRIVACY_CONTROL,
	    1,
	    0
	},
	{ 0, 0, "", 0, 0, 0, 0 }
};

/*
 * Format GUID to V4L2 pixel format mapping
 */
static const struct {
	uint8_t		guidFormat[16];
	uint32_t	pixelformat;
} uvideo_map_fmts[] = {
	{ UVIDEO_FORMAT_GUID_YUY2, V4L2_PIX_FMT_YUYV },
	{ UVIDEO_FORMAT_GUID_YV12, V4L2_PIX_FMT_YVU420 },
	{ UVIDEO_FORMAT_GUID_I420, V4L2_PIX_FMT_YUV420 },
	{ UVIDEO_FORMAT_GUID_Y800, V4L2_PIX_FMT_GREY },
	{ UVIDEO_FORMAT_GUID_Y8, V4L2_PIX_FMT_GREY },
	{ UVIDEO_FORMAT_GUID_D3DFMT_L8, V4L2_PIX_FMT_GREY },
	{ UVIDEO_FORMAT_GUID_KSMEDIA_L8_IR, V4L2_PIX_FMT_GREY },
	{ UVIDEO_FORMAT_GUID_BY8, V4L2_PIX_FMT_SBGGR8 },
	{ UVIDEO_FORMAT_GUID_BA81, V4L2_PIX_FMT_SBGGR8 },
	{ UVIDEO_FORMAT_GUID_GBRG, V4L2_PIX_FMT_SGBRG8 },
	{ UVIDEO_FORMAT_GUID_GRBG, V4L2_PIX_FMT_SGRBG8 },
	{ UVIDEO_FORMAT_GUID_RGGB, V4L2_PIX_FMT_SRGGB8 },
	{ UVIDEO_FORMAT_GUID_RGBP, V4L2_PIX_FMT_RGB565 },
	{ UVIDEO_FORMAT_GUID_D3DFMT_R5G6B5, V4L2_PIX_FMT_RGB565 },
	{ UVIDEO_FORMAT_GUID_BGR3, V4L2_PIX_FMT_BGR24 },
	{ UVIDEO_FORMAT_GUID_BGR4, V4L2_PIX_FMT_XBGR32 },
	{ UVIDEO_FORMAT_GUID_H265, V4L2_PIX_FMT_HEVC },
	{ UVIDEO_FORMAT_GUID_RW10, V4L2_PIX_FMT_SRGGB10P },
	{ UVIDEO_FORMAT_GUID_BG16, V4L2_PIX_FMT_SBGGR16 },
	{ UVIDEO_FORMAT_GUID_GB16, V4L2_PIX_FMT_SGBRG16 },
	{ UVIDEO_FORMAT_GUID_RG16, V4L2_PIX_FMT_SRGGB16 },
	{ UVIDEO_FORMAT_GUID_GR16, V4L2_PIX_FMT_SGRBG16 },
	{ UVIDEO_FORMAT_GUID_INVZ, V4L2_PIX_FMT_Z16 },
	{ UVIDEO_FORMAT_GUID_INVI, V4L2_PIX_FMT_Y10 },
};

/*
 * Color matching tables from UVC spec
 */
static const enum v4l2_colorspace uvideo_color_primaries[] = {
	V4L2_COLORSPACE_SRGB,		/* Unspecified */
	V4L2_COLORSPACE_SRGB,
	V4L2_COLORSPACE_470_SYSTEM_M,
	V4L2_COLORSPACE_470_SYSTEM_BG,
	V4L2_COLORSPACE_SMPTE170M,
	V4L2_COLORSPACE_SMPTE240M,
};

static const enum v4l2_xfer_func uvideo_xfer_characteristics[] = {
	V4L2_XFER_FUNC_DEFAULT,	/* Unspecified */
	V4L2_XFER_FUNC_709,
	V4L2_XFER_FUNC_709,		/* Substitution for BT.470-2 M */
	V4L2_XFER_FUNC_709,		/* Substitution for BT.470-2 B, G */
	V4L2_XFER_FUNC_709,		/* Substitution for SMPTE 170M */
	V4L2_XFER_FUNC_SMPTE240M,
	V4L2_XFER_FUNC_NONE,
	V4L2_XFER_FUNC_SRGB,
};

static const enum v4l2_ycbcr_encoding uvideo_matrix_coefficients[] = {
	V4L2_YCBCR_ENC_DEFAULT,	/* Unspecified */
	V4L2_YCBCR_ENC_709,
	V4L2_YCBCR_ENC_601,		/* Substitution for FCC */
	V4L2_YCBCR_ENC_601,		/* Substitution for BT.470-2 B, G */
	V4L2_YCBCR_ENC_601,
	V4L2_YCBCR_ENC_SMPTE240M,
};

/*
 * USB device ID table - match standard UVC devices
 */
static const STRUCT_USB_HOST_ID uvideo_devs[] = {
	{USB_IFACE_CLASS(UICLASS_VIDEO),
	 USB_IFACE_SUBCLASS(UISUBCLASS_VIDEOCONTROL),},
};

/*
 * Device methods
 */
static device_method_t uvideo_methods[] = {
	DEVMETHOD(device_probe, uvideo_probe),
	DEVMETHOD(device_attach, uvideo_attach),
	DEVMETHOD(device_detach, uvideo_detach),
	DEVMETHOD_END
};

static driver_t uvideo_driver = {
	.name = "uvideo",
	.methods = uvideo_methods,
	.size = sizeof(struct uvideo_softc),
};

DRIVER_MODULE(uvideo, uhub, uvideo_driver, NULL, NULL);
MODULE_DEPEND(uvideo, usb, 1, 1, 1);
MODULE_VERSION(uvideo, 1);
USB_PNP_HOST_INFO(uvideo_devs);

/*
 * Transfer configuration: triple-buffered isochronous + single bulk
 */
static const struct usb_config uvideo_isoc_config[UVIDEO_IXFERS] = {
	[0] = {
		.type = UE_ISOCHRONOUS,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = 0,	/* use wMaxPacketSize * frames */
		.frames = UVIDEO_NFRAMES_MAX,
		.flags = {.short_xfer_ok = 1, .short_frames_ok = 1,},
		.callback = &uvideo_isoc_callback,
	},
	[1] = {
		.type = UE_ISOCHRONOUS,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = 0,
		.frames = UVIDEO_NFRAMES_MAX,
		.flags = {.short_xfer_ok = 1, .short_frames_ok = 1,},
		.callback = &uvideo_isoc_callback,
	},
	[2] = {
		.type = UE_ISOCHRONOUS,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = 0,
		.frames = UVIDEO_NFRAMES_MAX,
		.flags = {.short_xfer_ok = 1, .short_frames_ok = 1,},
		.callback = &uvideo_isoc_callback,
	},
};

static const struct usb_config uvideo_bulk_config[1] = {
	[0] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = 65536,
		.flags = {.short_xfer_ok = 1, .pipe_bof = 1,},
		.callback = &uvideo_bulk_callback,
	},
};

/*
 * Character device switch
 */
static struct cdevsw uvideo_cdevsw = {
	.d_version = D_VERSION,
	.d_open = uvideo_cdev_open,
	.d_close = uvideo_cdev_close,
	.d_read = uvideo_cdev_read,
	.d_ioctl = uvideo_cdev_ioctl,
	.d_poll = uvideo_cdev_poll,
	.d_kqfilter = uvideo_cdev_kqfilter,
	.d_mmap = uvideo_cdev_mmap,
	.d_name = "video",
};

/*
 * Unit number allocator
 */
/* Unit number allocation is handled by scanning for free /dev/videoN names */

/* ---------------------------------------------------------------- */
/*  Probe / Attach / Detach                                         */
/* ---------------------------------------------------------------- */

static int
uvideo_probe(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);

	if (uaa->info.bInterfaceClass != UICLASS_VIDEO)
		return (ENXIO);

	if (uaa->info.bInterfaceSubClass != UISUBCLASS_VIDEOCONTROL)
		return (ENXIO);

	return (usbd_lookup_id_by_uaa(uvideo_devs, sizeof(uvideo_devs), uaa));
}

static int
uvideo_attach(device_t dev)
{
	struct uvideo_softc *sc = device_get_softc(dev);
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct usb_config_descriptor *cdesc;
	struct usb_descriptor *desc;
	struct usb_interface_assoc_descriptor *iad;
	struct make_dev_args args;
	usb_error_t error;
	int first_iface, nifaces;
	int i;

	sc->sc_dev = dev;
	sc->sc_udev = uaa->device;
	sc->sc_iface_index = uaa->info.bIfaceIndex;

	device_set_usb_desc(dev);
	mtx_init(&sc->sc_mtx, "uvideo", NULL, MTX_DEF);
	knlist_init_mtx(&sc->sc_selinfo.si_note, &sc->sc_mtx);

	/* Get the config descriptor to iterate */
	cdesc = usbd_get_config_descriptor(sc->sc_udev);
	if (cdesc == NULL) {
		device_printf(dev, "failed to get config descriptor\n");
		goto detach;
	}

	/*
	 * Find the Interface Association Descriptor (IAD) that groups
	 * the video control and video streaming interfaces.
	 */
	iad = NULL;
	desc = NULL;
	while ((desc = usb_desc_foreach(cdesc, desc)) != NULL) {
		if (desc->bDescriptorType != UDESC_IFACE_ASSOC)
			continue;
		iad = (struct usb_interface_assoc_descriptor *)desc;
		if (uaa->info.bIfaceIndex >= iad->bFirstInterface &&
		    uaa->info.bIfaceIndex <
		    iad->bFirstInterface + iad->bInterfaceCount)
			break;
		iad = NULL;
	}
	if (iad == NULL) {
		device_printf(dev, "can't find interface association\n");
		goto detach;
	}

	first_iface = iad->bFirstInterface;
	nifaces = iad->bInterfaceCount;

	/* Claim all interfaces in this association */
	for (i = first_iface; i < first_iface + nifaces; i++) {
		if (i == uaa->info.bIfaceIndex)
			continue;
		usbd_set_parent_iface(sc->sc_udev, i, uaa->info.bIfaceIndex);
	}

	sc->sc_iface_index = first_iface;
	sc->sc_nifaces = nifaces;

	/* Standard UVC stream header decode */
	sc->sc_decode_stream_header = uvideo_vs_decode_stream_header;

	/* Parse video control descriptors */
	error = uvideo_vc_parse_desc(sc);
	if (error != USB_ERR_NORMAL_COMPLETION) {
		device_printf(dev, "failed to parse VC descriptors\n");
		goto detach;
	}

	/* Parse video stream descriptors */
	error = uvideo_vs_parse_desc(sc, cdesc);
	if (error != USB_ERR_NORMAL_COMPLETION) {
		device_printf(dev, "failed to parse VS descriptors\n");
		goto detach;
	}

	/* Set default video stream interface to alt 0 */
	if (sc->sc_vs_cur != NULL) {
		error = usbd_set_alt_interface_index(sc->sc_udev,
		    sc->sc_vs_cur->iface_index, 0);
		if (error != USB_ERR_NORMAL_COMPLETION) {
			device_printf(dev,
			    "failed to set default alt interface\n");
			goto detach;
		}
	}

	/* Do device negotiation without commit */
	error = uvideo_vs_negotiation(sc, 0);
	if (error != USB_ERR_NORMAL_COMPLETION) {
		device_printf(dev, "initial negotiation failed\n");
		goto detach;
	}

	/* Report what we found */
	if (sc->sc_vs_cur != NULL) {
		device_printf(dev, "%d format(s), iface_index=%d, "
		    "endpoint=0x%02x, psize=%u, %s\n",
		    sc->sc_fmtgrp_num,
		    sc->sc_vs_cur->iface_index,
		    sc->sc_vs_cur->endpoint,
		    sc->sc_vs_cur->psize,
		    sc->sc_vs_cur->bulk_endpoint ? "bulk" : "isoc");
		if (sc->sc_fmtgrp_cur != NULL) {
			struct usb_video_frame_desc *fr =
			    sc->sc_fmtgrp_cur->frame_cur;
			device_printf(dev, "default format: pixfmt=0x%08x, "
			    "%dx%d, max_fbuf=%d\n",
			    sc->sc_fmtgrp_cur->pixelformat,
			    fr ? UGETW(UVIDEO_FRAME_FIELD(fr, wWidth)) : 0,
			    fr ? UGETW(UVIDEO_FRAME_FIELD(fr, wHeight)) : 0,
			    sc->sc_max_fbuf_size);
		}
	}

	/* Init mmap queue */
	STAILQ_INIT(&sc->sc_mmap_q);
	sc->sc_mmap_count = 0;

	/* Allocate unit number and create character device */
	make_dev_args_init(&args);
	args.mda_devsw = &uvideo_cdevsw;
	args.mda_uid = UID_ROOT;
	args.mda_gid = GID_VIDEO;
	args.mda_mode = 0660;
	args.mda_si_drv1 = sc;
	args.mda_flags = MAKEDEV_CHECKNAME;

	sc->sc_unit = -1;
	for (i = 0; i < 256; i++) {
		if (make_dev_s(&args, &sc->sc_cdev, "video%d", i) == 0) {
			sc->sc_unit = i;
			break;
		}
	}
	if (sc->sc_unit < 0) {
		device_printf(dev, "failed to create /dev/video device\n");
		goto detach;
	}

	device_printf(dev, "UVC camera on /dev/video%d\n", sc->sc_unit);

	return (0);

detach:
	uvideo_detach(dev);
	return (ENXIO);
}

static int
uvideo_detach(device_t dev)
{
	struct uvideo_softc *sc = device_get_softc(dev);

	sc->sc_dying = 1;

	/* Stop any active streaming */
	if (sc->sc_streaming) {
		mtx_lock(&sc->sc_mtx);
		sc->sc_streaming = 0;
		mtx_unlock(&sc->sc_mtx);
		uvideo_vs_close(sc);
	}

	/* Destroy character device */
	if (sc->sc_cdev != NULL) {
		destroy_dev(sc->sc_cdev);
		sc->sc_cdev = NULL;
	}

	/* Unit number is implicitly freed when the cdev is destroyed */

	/* Free frame buffers */
	uvideo_vs_free_frame(sc);

	/* Unsetup USB transfers */
	usbd_transfer_unsetup(sc->sc_xfer, UVIDEO_N_XFER);

	seldrain(&sc->sc_selinfo);
	knlist_destroy(&sc->sc_selinfo.si_note);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

/* ---------------------------------------------------------------- */
/*  Descriptor Parsing                                              */
/* ---------------------------------------------------------------- */

static usb_error_t
uvideo_vc_parse_desc(struct uvideo_softc *sc)
{
	struct usb_config_descriptor *cdesc;
	struct usb_descriptor *desc;
	struct usb_interface_descriptor *id;
	int vc_header_found;
	usb_error_t error;
	int past_our_iface;

	DPRINTFN(1, "uvideo_vc_parse_desc\n");

	vc_header_found = 0;
	past_our_iface = 0;

	cdesc = usbd_get_config_descriptor(sc->sc_udev);
	if (cdesc == NULL)
		return (USB_ERR_INVAL);

	desc = NULL;
	while ((desc = usb_desc_foreach(cdesc, desc)) != NULL) {
		/* Look for our VC interface */
		if (desc->bDescriptorType == UDESC_INTERFACE) {
			id = (struct usb_interface_descriptor *)desc;
			if (id->bInterfaceNumber == sc->sc_iface_index) {
				past_our_iface = 1;
				continue;
			} else if (past_our_iface &&
			    id->bInterfaceNumber != sc->sc_iface_index) {
				/*
				 * We have left our VC interface;
				 * stop if we hit a new IAD or unrelated iface.
				 */
			}
		}
		if (desc->bDescriptorType == UDESC_IFACE_ASSOC &&
		    past_our_iface)
			break;

		if (!past_our_iface)
			continue;

		if (desc->bDescriptorType != UDESC_CS_INTERFACE)
			continue;

		switch (desc->bDescriptorSubtype) {
		case UDESCSUB_VC_HEADER:
			if (!uvideo_desc_len(desc, 12, 11, 1, 0))
				break;
			if (vc_header_found) {
				device_printf(sc->sc_dev,
				    "too many VC_HEADERs!\n");
				return (USB_ERR_INVAL);
			}
			error = uvideo_vc_parse_desc_header(sc, desc);
			if (error != USB_ERR_NORMAL_COMPLETION)
				return (error);
			vc_header_found = 1;
			break;
		case UDESCSUB_VC_INPUT_TERMINAL:
		    {
			struct usb_video_input_terminal_desc *itd;
			itd = (struct usb_video_input_terminal_desc *)desc;
			if (UGETW(itd->wTerminalType) == ITT_CAMERA)
				(void)uvideo_vc_parse_desc_ct(sc, desc);
			break;
		    }
		case UDESCSUB_VC_PROCESSING_UNIT:
			(void)uvideo_vc_parse_desc_pu(sc, desc);
			break;
		}
	}

	if (vc_header_found == 0) {
		device_printf(sc->sc_dev, "no VC_HEADER found!\n");
		return (USB_ERR_INVAL);
	}

	return (USB_ERR_NORMAL_COMPLETION);
}

static usb_error_t
uvideo_vc_parse_desc_header(struct uvideo_softc *sc,
    const struct usb_descriptor *desc)
{
	struct usb_video_header_desc *d;

	d = __DECONST(struct usb_video_header_desc *, desc);

	if (d->bInCollection == 0) {
		device_printf(sc->sc_dev, "no VS interface found!\n");
		return (USB_ERR_INVAL);
	}

	sc->sc_desc_vc_header.fix = d;
	sc->sc_desc_vc_header.baInterfaceNr = (uByte *)(d + 1);
	if (UGETW(d->bcdUVC) < 0x0110)
		sc->sc_max_ctrl_size = 26;
	else if (UGETW(d->bcdUVC) < 0x0150)
		sc->sc_max_ctrl_size = 34;
	else
		sc->sc_max_ctrl_size = 48;

	return (USB_ERR_NORMAL_COMPLETION);
}

static usb_error_t
uvideo_vc_parse_desc_pu(struct uvideo_softc *sc,
    const struct usb_descriptor *desc)
{
	struct usb_video_vc_processing_desc *d;

	d = __DECONST(struct usb_video_vc_processing_desc *, desc);

	if (sc->sc_desc_vc_pu_num == UVIDEO_MAX_PU) {
		device_printf(sc->sc_dev,
		    "too many PU descriptors found!\n");
		return (USB_ERR_INVAL);
	}

	sc->sc_desc_vc_pu[sc->sc_desc_vc_pu_num] = d;
	sc->sc_desc_vc_pu_num++;

	return (USB_ERR_NORMAL_COMPLETION);
}

static usb_error_t
uvideo_vc_parse_desc_ct(struct uvideo_softc *sc,
    const struct usb_descriptor *desc)
{
	struct usb_video_camera_terminal_desc *d;

	d = __DECONST(struct usb_video_camera_terminal_desc *, desc);

	if (sc->sc_desc_vc_ct_num == UVIDEO_MAX_CT) {
		device_printf(sc->sc_dev, "too many CT descriptors\n");
		return (USB_ERR_INVAL);
	}

	sc->sc_desc_vc_ct[sc->sc_desc_vc_ct_num] = d;
	sc->sc_desc_vc_ct_num++;

	return (USB_ERR_NORMAL_COMPLETION);
}

static usb_error_t
uvideo_vc_get_ctrl(struct uvideo_softc *sc, uint8_t *ctrl_data,
    uint8_t request, uint8_t unitid, uint16_t ctrl_selector, uint16_t ctrl_len)
{
	struct usb_device_request req;
	usb_error_t error;

	req.bmRequestType = UVIDEO_GET_IF;
	req.bRequest = request;
	USETW(req.wValue, (ctrl_selector << 8));
	USETW(req.wIndex, (unitid << 8));
	USETW(req.wLength, ctrl_len);

	error = usbd_do_request(sc->sc_udev, NULL, &req, ctrl_data);
	if (error) {
		DPRINTFN(1, "could not GET ctrl: %s\n",
		    usbd_errstr(error));
		return (USB_ERR_INVAL);
	}

	return (USB_ERR_NORMAL_COMPLETION);
}

static usb_error_t
uvideo_vc_set_ctrl(struct uvideo_softc *sc, uint8_t *ctrl_data,
    uint8_t request, uint8_t unitid, uint16_t ctrl_selector, uint16_t ctrl_len)
{
	struct usb_device_request req;
	usb_error_t error;

	req.bmRequestType = UVIDEO_SET_IF;
	req.bRequest = request;
	USETW(req.wValue, (ctrl_selector << 8));
	USETW(req.wIndex, (unitid << 8));
	USETW(req.wLength, ctrl_len);

	error = usbd_do_request(sc->sc_udev, NULL, &req, ctrl_data);
	if (error) {
		DPRINTFN(1, "could not SET ctrl: %s\n",
		    usbd_errstr(error));
		return (USB_ERR_INVAL);
	}

	return (USB_ERR_NORMAL_COMPLETION);
}

static int
uvideo_find_ctrl(struct uvideo_softc *sc, int id)
{
	int i, j, found;

	if (sc->sc_desc_vc_pu_num == 0 && sc->sc_desc_vc_ct_num == 0) {
		DPRINTFN(1, "no PU or CT descriptors found!\n");
		return (EINVAL);
	}

	/* do we support this control? */
	for (found = 0, i = 0; uvideo_ctrls[i].cid != 0; i++) {
		if (id == uvideo_ctrls[i].cid) {
			found = 1;
			break;
		}
	}
	if (found == 0) {
		DPRINTFN(1, "control not supported by driver!\n");
		return (EINVAL);
	}

	/* does a PU support this control? */
	sc->sc_desc_vc_pu_cur = NULL;
	sc->sc_desc_vc_ct_cur = NULL;
	for (found = 0, j = 0; j < sc->sc_desc_vc_pu_num; j++) {
		if (uvideo_has_ctrl(sc->sc_desc_vc_pu[j],
		    uvideo_ctrls[i].ctrl_bit) != 0) {
			found = 1;
			sc->sc_desc_vc_pu_cur = sc->sc_desc_vc_pu[j];
			break;
		}
	}

	/* does a CT support this control? */
	if (found == 0) {
		for (j = 0; j < sc->sc_desc_vc_ct_num; j++) {
			if (uvideo_has_ct_ctrl(sc->sc_desc_vc_ct[j],
			    uvideo_ctrls[i].ctrl_bit) != 0) {
				found = 1;
				sc->sc_desc_vc_ct_cur = sc->sc_desc_vc_ct[j];
				break;
			}
		}
	}

	if (found == 0) {
		DPRINTFN(1, "control not supported by device!\n");
		return (EINVAL);
	}

	return (i);
}

static int
uvideo_has_ctrl(struct usb_video_vc_processing_desc *desc, int ctrl_bit)
{

	if (desc->bControlSize * 8 <= ctrl_bit)
		return (0);

	return (desc->bmControls[byteof(ctrl_bit)] & bitof(ctrl_bit));
}

static int
uvideo_has_ct_ctrl(struct usb_video_camera_terminal_desc *desc, int ctrl_bit)
{

	if (desc->bControlSize * 8 <= ctrl_bit)
		return (0);

	return (desc->bmControls[byteof(ctrl_bit)] & bitof(ctrl_bit));
}

static usb_error_t
uvideo_vs_parse_desc(struct uvideo_softc *sc,
    struct usb_config_descriptor *cdesc)
{
	struct usb_descriptor *desc;
	struct usb_interface_descriptor *id;
	struct usb_interface *iface;
	int i, iface_num, numalts;
	usb_error_t error;
	int past_our_iface;

	DPRINTFN(1, "number of total interfaces=%d\n", sc->sc_nifaces);
	DPRINTFN(1, "number of VS interfaces=%d\n",
	    sc->sc_desc_vc_header.fix->bInCollection);

	/* First pass: find VS_INPUT_HEADER */
	past_our_iface = 0;
	desc = NULL;
	while ((desc = usb_desc_foreach(cdesc, desc)) != NULL) {
		if (desc->bDescriptorType == UDESC_INTERFACE) {
			id = (struct usb_interface_descriptor *)desc;
			if (id->bInterfaceNumber == sc->sc_iface_index) {
				past_our_iface = 1;
				continue;
			}
		}
		if (desc->bDescriptorType == UDESC_IFACE_ASSOC &&
		    past_our_iface)
			break;
		if (!past_our_iface)
			continue;
		if (desc->bDescriptorType != UDESC_CS_INTERFACE)
			continue;

		switch (desc->bDescriptorSubtype) {
		case UDESCSUB_VS_INPUT_HEADER:
			if (!uvideo_desc_len(desc, 13, 3, 0, 12))
				break;
			error = uvideo_vs_parse_desc_input_header(sc, desc);
			if (error != USB_ERR_NORMAL_COMPLETION)
				return (error);
			break;
		}
	}

	/* Parse video stream format descriptors */
	error = uvideo_vs_parse_desc_format(sc);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (error);

	/* Parse video stream frame descriptors */
	error = uvideo_vs_parse_desc_frame(sc);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (error);

	/* Parse interface collection (alternates for each VS interface) */
	for (i = 0; i < sc->sc_desc_vc_header.fix->bInCollection; i++) {
		iface_num = sc->sc_desc_vc_header.baInterfaceNr[i];

		iface = usbd_get_iface(sc->sc_udev, iface_num);
		if (iface == NULL) {
			device_printf(sc->sc_dev,
			    "can't get VS interface %d!\n", iface_num);
			return (USB_ERR_INVAL);
		}

		id = usbd_get_interface_descriptor(iface);
		if (id == NULL) {
			device_printf(sc->sc_dev,
			    "can't get VS iface descriptor %d!\n", iface_num);
			return (USB_ERR_INVAL);
		}

		/* Claim this interface */
		usbd_set_parent_iface(sc->sc_udev, iface_num,
		    sc->sc_iface_index);

		/* Count alternates by iterating descriptors */
		numalts = usbd_get_no_alts(cdesc, id);

		DPRINTFN(1, "VS interface %d, bInterfaceNumber=0x%02x, "
		    "numalts=%d\n", i, id->bInterfaceNumber, numalts);

		error = uvideo_vs_parse_desc_alt(sc, i, iface_num, numalts);
		if (error != USB_ERR_NORMAL_COMPLETION)
			return (error);
	}

	/* For now always use the first video stream */
	sc->sc_vs_cur = &sc->sc_vs_coll[0];

	return (USB_ERR_NORMAL_COMPLETION);
}

static usb_error_t
uvideo_vs_parse_desc_input_header(struct uvideo_softc *sc,
    const struct usb_descriptor *desc)
{
	struct usb_video_input_header_desc *d;

	d = __DECONST(struct usb_video_input_header_desc *, desc);

	if (d->bNumFormats == 0) {
		device_printf(sc->sc_dev,
		    "no INPUT FORMAT descriptors found!\n");
		return (USB_ERR_INVAL);
	}

	sc->sc_desc_vs_input_header.fix = d;
	sc->sc_desc_vs_input_header.bmaControls = (uByte *)(d + 1);

	return (USB_ERR_NORMAL_COMPLETION);
}

static usb_error_t
uvideo_vs_parse_desc_format(struct uvideo_softc *sc)
{
	struct usb_config_descriptor *cdesc;
	struct usb_descriptor *desc;
	struct usb_interface_descriptor *id;
	int past_our_iface;

	DPRINTFN(1, "uvideo_vs_parse_desc_format\n");

	cdesc = usbd_get_config_descriptor(sc->sc_udev);
	if (cdesc == NULL)
		return (USB_ERR_INVAL);

	past_our_iface = 0;
	desc = NULL;
	while ((desc = usb_desc_foreach(cdesc, desc)) != NULL) {
		if (desc->bDescriptorType == UDESC_INTERFACE) {
			id = (struct usb_interface_descriptor *)desc;
			if (id->bInterfaceNumber == sc->sc_iface_index) {
				past_our_iface = 1;
				continue;
			}
		}
		if (desc->bDescriptorType == UDESC_IFACE_ASSOC &&
		    past_our_iface)
			break;
		if (!past_our_iface)
			continue;

		if (desc->bDescriptorType != UDESC_CS_INTERFACE)
			continue;

		if (desc->bLength != UVIDEO_FORMAT_LEN(desc))
			continue;

		switch (desc->bDescriptorSubtype) {
		case UDESCSUB_VS_COLORFORMAT:
			uvideo_vs_parse_desc_colorformat(sc, desc);
			break;
		case UDESCSUB_VS_FORMAT_MJPEG:
			uvideo_vs_parse_desc_format_mjpeg(sc, desc);
			break;
		case UDESCSUB_VS_FORMAT_UNCOMPRESSED:
			uvideo_vs_parse_desc_format_uncompressed(sc, desc);
			break;
		case UDESCSUB_VS_FORMAT_FRAME_BASED:
			uvideo_vs_parse_desc_format_frame_based(sc, desc);
			break;
		case UDESCSUB_VS_FORMAT_H264:
		case UDESCSUB_VS_FORMAT_H264_SIMULCAST:
			uvideo_vs_parse_desc_format_h264(sc, desc);
			break;
		}
	}

	sc->sc_fmtgrp_idx = 0;

	if (sc->sc_fmtgrp_num == 0) {
		device_printf(sc->sc_dev, "no format descriptors found!\n");
		return (USB_ERR_INVAL);
	}
	DPRINTFN(1, "number of total format descriptors=%d\n",
	    sc->sc_fmtgrp_num);

	return (USB_ERR_NORMAL_COMPLETION);
}

static void
uvideo_vs_parse_desc_colorformat(struct uvideo_softc *sc,
    const struct usb_descriptor *desc)
{
	int fmtidx;
	struct usb_video_colorformat_desc *d;

	d = __DECONST(struct usb_video_colorformat_desc *, desc);

	fmtidx = sc->sc_fmtgrp_idx - 1;
	if (fmtidx < 0 || sc->sc_fmtgrp[fmtidx].has_colorformat)
		return;

	if (d->bColorPrimaries < nitems(uvideo_color_primaries))
		sc->sc_fmtgrp[fmtidx].colorspace =
		    uvideo_color_primaries[d->bColorPrimaries];
	else
		sc->sc_fmtgrp[fmtidx].colorspace = V4L2_COLORSPACE_SRGB;

	if (d->bTransferCharacteristics < nitems(uvideo_xfer_characteristics))
		sc->sc_fmtgrp[fmtidx].xfer_func =
		    uvideo_xfer_characteristics[d->bTransferCharacteristics];
	else
		sc->sc_fmtgrp[fmtidx].xfer_func = V4L2_XFER_FUNC_DEFAULT;

	if (d->bMatrixCoefficients < nitems(uvideo_matrix_coefficients))
		sc->sc_fmtgrp[fmtidx].ycbcr_enc =
		    uvideo_matrix_coefficients[d->bMatrixCoefficients];
	else
		sc->sc_fmtgrp[fmtidx].ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;

	sc->sc_fmtgrp[fmtidx].has_colorformat = 1;
}

static void
uvideo_vs_parse_desc_format_mjpeg(struct uvideo_softc *sc,
    const struct usb_descriptor *desc)
{
	struct usb_video_format_desc *d;

	d = __DECONST(struct usb_video_format_desc *, desc);

	if (d->bNumFrameDescriptors == 0) {
		device_printf(sc->sc_dev,
		    "no MJPEG frame descriptors available!\n");
		return;
	}

	if (sc->sc_fmtgrp_idx >= UVIDEO_MAX_FORMAT) {
		device_printf(sc->sc_dev,
		    "too many format descriptors found!\n");
		return;
	}

	sc->sc_fmtgrp[sc->sc_fmtgrp_idx].format = d;
	if (d->u.mjpeg.bDefaultFrameIndex > d->bNumFrameDescriptors ||
	    d->u.mjpeg.bDefaultFrameIndex < 1)
		sc->sc_fmtgrp[sc->sc_fmtgrp_idx].format_dfidx = 1;
	else
		sc->sc_fmtgrp[sc->sc_fmtgrp_idx].format_dfidx =
		    d->u.mjpeg.bDefaultFrameIndex;

	sc->sc_fmtgrp[sc->sc_fmtgrp_idx].pixelformat = V4L2_PIX_FMT_MJPEG;

	if (sc->sc_fmtgrp_cur == NULL)
		sc->sc_fmtgrp_cur = &sc->sc_fmtgrp[sc->sc_fmtgrp_idx];

	sc->sc_fmtgrp_idx++;
	sc->sc_fmtgrp_num++;
}

static void
uvideo_vs_parse_desc_format_h264(struct uvideo_softc *sc,
    const struct usb_descriptor *desc)
{
	struct usb_video_format_desc *d;

	d = __DECONST(struct usb_video_format_desc *, desc);

	if (d->bNumFrameDescriptors == 0) {
		device_printf(sc->sc_dev,
		    "no H264 frame descriptors available!\n");
		return;
	}

	if (sc->sc_fmtgrp_idx >= UVIDEO_MAX_FORMAT) {
		device_printf(sc->sc_dev,
		    "too many format descriptors found!\n");
		return;
	}

	sc->sc_fmtgrp[sc->sc_fmtgrp_idx].format = d;
	if (d->u.h264.bDefaultFrameIndex > d->bNumFrameDescriptors ||
	    d->u.h264.bDefaultFrameIndex < 1)
		sc->sc_fmtgrp[sc->sc_fmtgrp_idx].format_dfidx = 1;
	else
		sc->sc_fmtgrp[sc->sc_fmtgrp_idx].format_dfidx =
		    d->u.h264.bDefaultFrameIndex;

	sc->sc_fmtgrp[sc->sc_fmtgrp_idx].pixelformat = V4L2_PIX_FMT_H264;

	if (sc->sc_fmtgrp_cur == NULL)
		sc->sc_fmtgrp_cur = &sc->sc_fmtgrp[sc->sc_fmtgrp_idx];

	sc->sc_fmtgrp_idx++;
	sc->sc_fmtgrp_num++;
}

static void
uvideo_vs_parse_desc_format_frame_based(struct uvideo_softc *sc,
    const struct usb_descriptor *desc)
{
	struct usb_video_format_desc *d;
	int i, j, nent;

	d = __DECONST(struct usb_video_format_desc *, desc);

	if (d->bNumFrameDescriptors == 0) {
		device_printf(sc->sc_dev,
		    "no frame-based frame descriptors available!\n");
		return;
	}

	if (sc->sc_fmtgrp_idx >= UVIDEO_MAX_FORMAT) {
		device_printf(sc->sc_dev,
		    "too many format descriptors found!\n");
		return;
	}

	sc->sc_fmtgrp[sc->sc_fmtgrp_idx].format = d;
	if (d->u.fb.bDefaultFrameIndex > d->bNumFrameDescriptors ||
	    d->u.fb.bDefaultFrameIndex < 1)
		sc->sc_fmtgrp[sc->sc_fmtgrp_idx].format_dfidx = 1;
	else
		sc->sc_fmtgrp[sc->sc_fmtgrp_idx].format_dfidx =
		    d->u.fb.bDefaultFrameIndex;

	i = sc->sc_fmtgrp_idx;

	/* Map GUID to pixel format */
	nent = nitems(uvideo_map_fmts);
	for (j = 0; j < nent; j++) {
		if (!memcmp(sc->sc_fmtgrp[i].format->u.uc.guidFormat,
		    uvideo_map_fmts[j].guidFormat, 16)) {
			sc->sc_fmtgrp[i].pixelformat =
			    uvideo_map_fmts[j].pixelformat;
			break;
		}
	}
	if (j == nent)
		memcpy(&sc->sc_fmtgrp[i].pixelformat,
		    sc->sc_fmtgrp[i].format->u.uc.guidFormat,
		    sizeof(uint32_t));

	if (sc->sc_fmtgrp_cur == NULL)
		sc->sc_fmtgrp_cur = &sc->sc_fmtgrp[sc->sc_fmtgrp_idx];

	sc->sc_fmtgrp_idx++;
	sc->sc_fmtgrp_num++;
}

static void
uvideo_vs_parse_desc_format_uncompressed(struct uvideo_softc *sc,
    const struct usb_descriptor *desc)
{
	struct usb_video_format_desc *d;
	int i, j, nent;

	d = __DECONST(struct usb_video_format_desc *, desc);

	if (d->bNumFrameDescriptors == 0) {
		device_printf(sc->sc_dev,
		    "no UNCOMPRESSED frame descriptors available!\n");
		return;
	}

	if (sc->sc_fmtgrp_idx >= UVIDEO_MAX_FORMAT) {
		device_printf(sc->sc_dev,
		    "too many format descriptors found!\n");
		return;
	}

	sc->sc_fmtgrp[sc->sc_fmtgrp_idx].format = d;
	if (d->u.uc.bDefaultFrameIndex > d->bNumFrameDescriptors ||
	    d->u.uc.bDefaultFrameIndex < 1)
		sc->sc_fmtgrp[sc->sc_fmtgrp_idx].format_dfidx = 1;
	else
		sc->sc_fmtgrp[sc->sc_fmtgrp_idx].format_dfidx =
		    d->u.uc.bDefaultFrameIndex;

	i = sc->sc_fmtgrp_idx;

	/* Map GUID to pixel format */
	nent = nitems(uvideo_map_fmts);
	for (j = 0; j < nent; j++) {
		if (!memcmp(sc->sc_fmtgrp[i].format->u.uc.guidFormat,
		    uvideo_map_fmts[j].guidFormat, 16)) {
			sc->sc_fmtgrp[i].pixelformat =
			    uvideo_map_fmts[j].pixelformat;
			break;
		}
	}
	if (j == nent)
		memcpy(&sc->sc_fmtgrp[i].pixelformat,
		    sc->sc_fmtgrp[i].format->u.uc.guidFormat,
		    sizeof(uint32_t));

	if (sc->sc_fmtgrp_cur == NULL)
		sc->sc_fmtgrp_cur = &sc->sc_fmtgrp[sc->sc_fmtgrp_idx];

	sc->sc_fmtgrp_idx++;
	sc->sc_fmtgrp_num++;
}

static usb_error_t
uvideo_vs_parse_desc_frame(struct uvideo_softc *sc)
{
	struct usb_config_descriptor *cdesc;
	struct usb_descriptor *desc;
	struct usb_interface_descriptor *id;
	usb_error_t error;
	int past_our_iface;

	DPRINTFN(1, "uvideo_vs_parse_desc_frame\n");

	cdesc = usbd_get_config_descriptor(sc->sc_udev);
	if (cdesc == NULL)
		return (USB_ERR_INVAL);

	past_our_iface = 0;
	desc = NULL;
	while ((desc = usb_desc_foreach(cdesc, desc)) != NULL) {
		if (desc->bDescriptorType == UDESC_INTERFACE) {
			id = (struct usb_interface_descriptor *)desc;
			if (id->bInterfaceNumber == sc->sc_iface_index) {
				past_our_iface = 1;
				continue;
			}
		}
		if (desc->bDescriptorType == UDESC_IFACE_ASSOC &&
		    past_our_iface)
			break;
		if (!past_our_iface)
			continue;

		if (desc->bDescriptorType == UDESC_CS_INTERFACE &&
		    desc->bLength > UVIDEO_FRAME_MIN_LEN(desc) &&
		    (desc->bDescriptorSubtype == UDESCSUB_VS_FRAME_MJPEG ||
		    desc->bDescriptorSubtype ==
		    UDESCSUB_VS_FRAME_UNCOMPRESSED)) {
			error = uvideo_vs_parse_desc_frame_buffer_size(sc,
			    desc);
			if (error != USB_ERR_NORMAL_COMPLETION)
				return (error);
		}
		if (desc->bDescriptorType == UDESC_CS_INTERFACE &&
		    desc->bLength > UVIDEO_FRAME_MIN_LEN(desc) &&
		    (desc->bDescriptorSubtype == UDESCSUB_VS_FRAME_H264 ||
		    desc->bDescriptorSubtype ==
		    UDESCSUB_VS_FRAME_FRAME_BASED)) {
			error = uvideo_vs_parse_desc_frame_max_rate(sc, desc);
			if (error != USB_ERR_NORMAL_COMPLETION)
				return (error);
		}
	}

	return (USB_ERR_NORMAL_COMPLETION);
}

static usb_error_t
uvideo_vs_parse_desc_frame_buffer_size(struct uvideo_softc *sc,
    const struct usb_descriptor *desc)
{
	struct usb_video_frame_desc *fd =
	    __DECONST(struct usb_video_frame_desc *, desc);
	int fmtidx, frame_num;
	uint32_t fbuf_size;

	fmtidx = sc->sc_fmtgrp_idx;
	frame_num = sc->sc_fmtgrp[fmtidx].frame_num;
	if (frame_num >= UVIDEO_MAX_FRAME) {
		device_printf(sc->sc_dev,
		    "too many %s frame descriptors found!\n",
		    desc->bDescriptorSubtype == UDESCSUB_VS_FRAME_MJPEG ?
		    "MJPEG" : "UNCOMPRESSED");
		return (USB_ERR_INVAL);
	}
	sc->sc_fmtgrp[fmtidx].frame[frame_num] = fd;

	if (sc->sc_fmtgrp[fmtidx].frame_cur == NULL ||
	    sc->sc_fmtgrp[fmtidx].format_dfidx == fd->bFrameIndex)
		sc->sc_fmtgrp[fmtidx].frame_cur = fd;

	/*
	 * For uncompressed formats, compute the frame buffer size from
	 * width * height * bpp since dwMaxVideoFrameBufferSize may be wrong.
	 */
	if (desc->bDescriptorSubtype == UDESCSUB_VS_FRAME_UNCOMPRESSED) {
		fbuf_size = UGETW(fd->u.uc.wWidth) *
		    UGETW(fd->u.uc.wHeight) *
		    sc->sc_fmtgrp[fmtidx].format->u.uc.bBitsPerPixel / NBBY;
	} else
		fbuf_size = UGETDW(fd->u.uc.dwMaxVideoFrameBufferSize);

	if (fbuf_size > sc->sc_max_fbuf_size)
		sc->sc_max_fbuf_size = fbuf_size;

	if (++sc->sc_fmtgrp[fmtidx].frame_num ==
	    sc->sc_fmtgrp[fmtidx].format->bNumFrameDescriptors)
		sc->sc_fmtgrp_idx++;

	return (USB_ERR_NORMAL_COMPLETION);
}

static usb_error_t
uvideo_vs_parse_desc_frame_max_rate(struct uvideo_softc *sc,
    const struct usb_descriptor *desc)
{
	struct usb_video_frame_desc *fd =
	    __DECONST(struct usb_video_frame_desc *, desc);
	uint8_t *p;
	int i, fmtidx, frame_num, length, nivals;
	uint32_t fbuf_size, frame_ival, next_frame_ival;

	fmtidx = sc->sc_fmtgrp_idx;
	frame_num = sc->sc_fmtgrp[fmtidx].frame_num;
	if (frame_num >= UVIDEO_MAX_FRAME) {
		device_printf(sc->sc_dev,
		    "too many %s frame descriptors found!\n",
		    desc->bDescriptorSubtype == UDESCSUB_VS_FRAME_H264 ?
		    "H264" : "FRAME BASED");
		return (USB_ERR_INVAL);
	}
	sc->sc_fmtgrp[fmtidx].frame[frame_num] = fd;

	if (sc->sc_fmtgrp[fmtidx].frame_cur == NULL ||
	    sc->sc_fmtgrp[fmtidx].format_dfidx == fd->bFrameIndex)
		sc->sc_fmtgrp[fmtidx].frame_cur = fd;

	/*
	 * Frame Based and H264 frames don't have dwMaxVideoFrameBufferSize;
	 * compute required buffer from dwMaxBitRate and dwFrameInterval.
	 */
	frame_ival = UGETDW(fd->u.h264.dwDefaultFrameInterval);

	p = __DECONST(uint8_t *, desc) + UVIDEO_FRAME_MIN_LEN(fd);
	length = fd->bLength - UVIDEO_FRAME_MIN_LEN(fd);

	nivals = UVIDEO_FRAME_NUM_INTERVALS(fd);

	for (i = 0; i < nivals; i++) {
		if (length <= 0)
			break;
		next_frame_ival = UGETDW(p);
		if (next_frame_ival > frame_ival)
			frame_ival = next_frame_ival;
		p += sizeof(uDWord);
		length -= sizeof(uDWord);
	}

	fbuf_size = UGETDW(UVIDEO_FRAME_FIELD(fd, dwMaxBitRate)) * frame_ival;
	fbuf_size /= 8 * 10000000;

	if (fbuf_size > sc->sc_max_fbuf_size)
		sc->sc_max_fbuf_size = fbuf_size;

	if (++sc->sc_fmtgrp[fmtidx].frame_num ==
	    sc->sc_fmtgrp[fmtidx].format->bNumFrameDescriptors)
		sc->sc_fmtgrp_idx++;

	return (USB_ERR_NORMAL_COMPLETION);
}

static usb_error_t
uvideo_vs_parse_desc_alt(struct uvideo_softc *sc, int vs_nr, int iface,
    int numalts)
{
	struct uvideo_vs_iface *vs;
	struct usb_config_descriptor *cdesc;
	struct usb_descriptor *desc;
	struct usb_interface_descriptor *id;
	struct usb_endpoint_descriptor *ed;
	uint8_t ep_dir, ep_type;
	int bulk_endpoint;
	uint32_t psize;
	int past_our_iface;

	vs = &sc->sc_vs_coll[vs_nr];

	cdesc = usbd_get_config_descriptor(sc->sc_udev);
	if (cdesc == NULL)
		return (USB_ERR_INVAL);

	vs->bulk_endpoint = 1;
	past_our_iface = 0;

	desc = NULL;
	while ((desc = usb_desc_foreach(cdesc, desc)) != NULL) {
		if (desc->bDescriptorType == UDESC_INTERFACE) {
			id = (struct usb_interface_descriptor *)desc;
			if (id->bInterfaceNumber == sc->sc_iface_index) {
				past_our_iface = 1;
				continue;
			}
		}
		if (desc->bDescriptorType == UDESC_IFACE_ASSOC &&
		    past_our_iface)
			break;
		if (!past_our_iface)
			continue;

		/* Find video stream interface */
		if (desc->bDescriptorType != UDESC_INTERFACE)
			continue;
		id = (struct usb_interface_descriptor *)(uint8_t *)desc;
		if (id->bInterfaceNumber != iface)
			continue;

		DPRINTFN(1, "bAlternateSetting=0x%02x\n",
		    id->bAlternateSetting);
		if (id->bNumEndpoints == 0)
			continue;

		/* Jump to the endpoint descriptor */
		while ((desc = usb_desc_foreach(cdesc, desc)) != NULL) {
			if (desc->bDescriptorType == UDESC_ENDPOINT)
				break;
		}
		if (desc == NULL)
			break;
		ed = (struct usb_endpoint_descriptor *)(uint8_t *)desc;

		/* Locate endpoint type */
		ep_dir = UE_GET_DIR(ed->bEndpointAddress);
		ep_type = UE_GET_XFERTYPE(ed->bmAttributes);
		if (ep_dir == UE_DIR_IN && ep_type == UE_ISOCHRONOUS)
			bulk_endpoint = 0;
		else if (ep_dir == UE_DIR_IN && ep_type == UE_BULK)
			bulk_endpoint = 1;
		else
			continue;

		if (bulk_endpoint && !vs->bulk_endpoint)
			continue;

		psize = UGETW(ed->wMaxPacketSize);
		psize = UE_GET_SIZE(psize) * (1 + UE_GET_TRANS(psize));

		/* Save endpoint with largest bandwidth */
		if (psize > vs->psize) {
			vs->endpoint = ed->bEndpointAddress;
			vs->numalts = numalts;
			vs->curalt = id->bAlternateSetting;
			vs->psize = psize;
			vs->iface_index = iface;
			vs->bulk_endpoint = bulk_endpoint;
		}
	}

	/* Check if we found a valid alternate interface */
	if (vs->psize == 0) {
		device_printf(sc->sc_dev,
		    "no valid alternate interface found!\n");
		return (USB_ERR_INVAL);
	}

	return (USB_ERR_NORMAL_COMPLETION);
}

/*
 * Validate a variable-length descriptor.
 */
static int
uvideo_desc_len(const struct usb_descriptor *desc,
    int size_fix, int off_num_elements, int size_element, int off_size_element)
{
	uint8_t *buf;
	int size_elements, size_total;

	if (desc->bLength < size_fix)
		return (0);

	buf = __DECONST(uint8_t *, desc);

	if (size_element == 0)
		size_element = buf[off_size_element];

	size_elements = buf[off_num_elements] * size_element;
	size_total = size_fix + size_elements;

	if (desc->bLength == size_total && size_elements != 0)
		return (1);

	return (0);
}

/*
 * Find the best matching resolution for a given format group.
 */
static void
uvideo_find_res(struct uvideo_softc *sc, int idx, int width, int height,
    struct uvideo_res *r)
{
	int i, w, h, diff, diff_best, size_want, size_is;
	struct usb_video_frame_desc *frame;

	size_want = width * height;

	for (i = 0; i < sc->sc_fmtgrp[idx].frame_num; i++) {
		frame = sc->sc_fmtgrp[idx].frame[i];
		w = UGETW(UVIDEO_FRAME_FIELD(frame, wWidth));
		h = UGETW(UVIDEO_FRAME_FIELD(frame, wHeight));
		size_is = w * h;
		if (size_is > size_want)
			diff = size_is - size_want;
		else
			diff = size_want - size_is;
		if (i == 0)
			diff_best = diff;
		if (diff <= diff_best) {
			diff_best = diff;
			r->width = w;
			r->height = h;
			r->fidx = i;
		}
	}
}

/* ---------------------------------------------------------------- */
/*  UVC Protocol (Negotiation, Probe/Commit)                        */
/* ---------------------------------------------------------------- */

static usb_error_t
uvideo_vs_negotiation(struct uvideo_softc *sc, int commit)
{
	struct usb_video_probe_commit *pc;
	struct uvideo_format_group *fmtgrp;
	struct usb_video_header_desc *hd;
	struct usb_video_frame_desc *frame;
	uint8_t *p, *cur;
	uint8_t probe_data[48];
	uint32_t frame_ival, nivals, min, max, step, diff;
	usb_error_t error;
	int i, ival_bytes, changed = 0;
	size_t len;

	pc = (struct usb_video_probe_commit *)probe_data;

	fmtgrp = sc->sc_fmtgrp_cur;

	if (fmtgrp->frame_num == 0) {
		device_printf(sc->sc_dev,
		    "negotiation: no frame descriptors found!\n");
		return (USB_ERR_INVAL);
	}

	/* Set probe */
	bzero(probe_data, sizeof(probe_data));
	USETW(pc->bmHint, 0x1);
	pc->bFormatIndex = fmtgrp->format->bFormatIndex;
	pc->bFrameIndex = fmtgrp->frame_cur->bFrameIndex;

	frame = fmtgrp->frame_cur;
	frame_ival = UGETDW(UVIDEO_FRAME_FIELD(frame, dwDefaultFrameInterval));
	if (sc->sc_frame_rate != 0) {
		frame_ival = 10000000 / sc->sc_frame_rate;
		/* Find closest matching interval */
		len = UVIDEO_FRAME_MIN_LEN(frame);
		nivals = UVIDEO_FRAME_NUM_INTERVALS(frame);
		p = (uint8_t *)fmtgrp->frame_cur;
		p += len;
		ival_bytes = frame->bLength - len;
		if (!nivals && (ival_bytes >= (int)sizeof(uDWord) * 3)) {
			/* continuous */
			min = UGETDW(p);
			p += sizeof(uDWord);
			max = UGETDW(p);
			p += sizeof(uDWord);
			step = UGETDW(p);
			if (frame_ival <= min)
				frame_ival = min;
			else if (frame_ival >= max)
				frame_ival = max;
			else {
				for (i = min;
				    i + step / 2 < frame_ival;
				    i += step)
					;
				frame_ival = i;
			}
		} else if (nivals > 0 &&
		    ival_bytes >= (int)sizeof(uDWord)) {
			/* discrete */
			cur = p;
			min = UINT_MAX;
			for (i = 0; i < (int)nivals; i++) {
				if (ival_bytes < (int)sizeof(uDWord))
					break;
				diff = abs((int)UGETDW(p) -
				    (int)frame_ival);
				if (diff < min) {
					min = diff;
					cur = p;
					if (diff == 0)
						break;
				}
				p += sizeof(uDWord);
				ival_bytes -= sizeof(uDWord);
			}
			frame_ival = UGETDW(cur);
		}
	}
	USETDW(pc->dwFrameInterval, frame_ival);
	error = uvideo_vs_set_probe(sc, probe_data);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (error);

	/* Get probe */
	bzero(probe_data, sizeof(probe_data));
	error = uvideo_vs_get_probe(sc, probe_data, GET_CUR);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (error);

	/* Check that the format/frame indexes match */
	if (pc->bFormatIndex != fmtgrp->format->bFormatIndex) {
		changed++;
		DPRINTFN(1, "wanted format 0x%x, got 0x%x\n",
		    fmtgrp->format->bFormatIndex, pc->bFormatIndex);
		for (i = 0; i < sc->sc_fmtgrp_num; i++) {
			if (sc->sc_fmtgrp[i].format->bFormatIndex ==
			    pc->bFormatIndex) {
				fmtgrp = &sc->sc_fmtgrp[i];
				break;
			}
		}
		if (i == sc->sc_fmtgrp_num) {
			DPRINTFN(1, "invalid format index 0x%x\n",
			    pc->bFormatIndex);
			return (USB_ERR_INVAL);
		}
	}
	if (pc->bFrameIndex != fmtgrp->frame_cur->bFrameIndex) {
		changed++;
		DPRINTFN(1, "wanted frame 0x%x, got 0x%x\n",
		    fmtgrp->frame_cur->bFrameIndex, pc->bFrameIndex);
		for (i = 0; i < fmtgrp->frame_num; i++) {
			if (fmtgrp->frame[i]->bFrameIndex ==
			    pc->bFrameIndex) {
				frame = fmtgrp->frame[i];
				break;
			}
		}
		if (i == fmtgrp->frame_num) {
			DPRINTFN(1, "invalid frame index 0x%x\n",
			    pc->bFrameIndex);
			return (USB_ERR_INVAL);
		}
	} else
		frame = fmtgrp->frame_cur;

	/* Fix uncompressed frame sizes */
	if (frame->bDescriptorSubtype == UDESCSUB_VS_FRAME_UNCOMPRESSED) {
		USETDW(pc->dwMaxVideoFrameSize,
		    UGETW(frame->u.uc.wWidth) *
		    UGETW(frame->u.uc.wHeight) *
		    fmtgrp->format->u.uc.bBitsPerPixel / NBBY);
	} else {
		hd = sc->sc_desc_vc_header.fix;
		if (UGETDW(pc->dwMaxVideoFrameSize) == 0 &&
		    UGETW(hd->bcdUVC) < 0x0110) {
			USETDW(pc->dwMaxVideoFrameSize,
			    UGETDW(frame->u.uc.dwMaxVideoFrameBufferSize));
		}
	}

	/* Commit */
	if (commit) {
		if (changed > 0)
			return (USB_ERR_INVAL);
		error = uvideo_vs_set_commit(sc, probe_data);
		if (error != USB_ERR_NORMAL_COMPLETION)
			return (error);
	}

	/* Save a copy of probe commit */
	bcopy(pc, &sc->sc_desc_probe, sizeof(sc->sc_desc_probe));

	return (USB_ERR_NORMAL_COMPLETION);
}

static usb_error_t
uvideo_vs_set_probe(struct uvideo_softc *sc, uint8_t *probe_data)
{
	struct usb_device_request req;
	usb_error_t error;
	uint16_t tmp;

	req.bmRequestType = UVIDEO_SET_IF;
	req.bRequest = SET_CUR;
	tmp = VS_PROBE_CONTROL;
	tmp = tmp << 8;
	USETW(req.wValue, tmp);
	USETW(req.wIndex, sc->sc_vs_cur->iface_index);
	USETW(req.wLength, sc->sc_max_ctrl_size);

	error = usbd_do_request(sc->sc_udev, NULL, &req, probe_data);
	if (error) {
		device_printf(sc->sc_dev, "could not SET probe: %s\n",
		    usbd_errstr(error));
		return (USB_ERR_INVAL);
	}

	DPRINTFN(1, "SET probe OK\n");
	return (USB_ERR_NORMAL_COMPLETION);
}

static usb_error_t
uvideo_vs_get_probe(struct uvideo_softc *sc, uint8_t *probe_data,
    uint8_t request)
{
	struct usb_device_request req;
	usb_error_t error;
	uint16_t tmp, actlen;

	req.bmRequestType = UVIDEO_GET_IF;
	req.bRequest = request;
	tmp = VS_PROBE_CONTROL;
	tmp = tmp << 8;
	USETW(req.wValue, tmp);
	USETW(req.wIndex, sc->sc_vs_cur->iface_index);
	USETW(req.wLength, sc->sc_max_ctrl_size);

	error = usbd_do_request_flags(sc->sc_udev, NULL, &req,
	    probe_data, USB_SHORT_XFER_OK, &actlen, 5000);
	if (error != USB_ERR_NORMAL_COMPLETION) {
		device_printf(sc->sc_dev, "could not GET probe: %s\n",
		    usbd_errstr(error));
		return (USB_ERR_INVAL);
	}

	/* Zero unused portion */
	if (actlen < sizeof(struct usb_video_probe_commit))
		bzero(probe_data + actlen,
		    sizeof(struct usb_video_probe_commit) - actlen);

	DPRINTFN(1, "GET probe OK, length=%d\n", actlen);
	return (USB_ERR_NORMAL_COMPLETION);
}

static usb_error_t
uvideo_vs_set_commit(struct uvideo_softc *sc, uint8_t *probe_data)
{
	struct usb_device_request req;
	usb_error_t error;
	uint16_t tmp;

	req.bmRequestType = UVIDEO_SET_IF;
	req.bRequest = SET_CUR;
	tmp = VS_COMMIT_CONTROL;
	tmp = tmp << 8;
	USETW(req.wValue, tmp);
	USETW(req.wIndex, sc->sc_vs_cur->iface_index);
	USETW(req.wLength, sc->sc_max_ctrl_size);

	error = usbd_do_request(sc->sc_udev, NULL, &req, probe_data);
	if (error) {
		device_printf(sc->sc_dev, "could not SET commit: %s\n",
		    usbd_errstr(error));
		return (USB_ERR_INVAL);
	}

	DPRINTFN(1, "SET commit OK\n");
	return (USB_ERR_NORMAL_COMPLETION);
}

/* ---------------------------------------------------------------- */
/*  Stream Management                                               */
/* ---------------------------------------------------------------- */

static usb_error_t
uvideo_vs_alloc_frame(struct uvideo_softc *sc)
{
	struct uvideo_frame_buffer *fb = &sc->sc_frame_buffer;

	fb->buf_size = UGETDW(sc->sc_desc_probe.dwMaxVideoFrameSize);

	if (sc->sc_max_fbuf_size < fb->buf_size && sc->sc_mmap_flag == 0) {
		device_printf(sc->sc_dev,
		    "software video buffer too small!\n");
		return (USB_ERR_NOMEM);
	}

	fb->buf = malloc(fb->buf_size, M_USBDEV, M_WAITOK | M_ZERO);
	if (fb->buf == NULL) {
		device_printf(sc->sc_dev,
		    "can't allocate frame buffer!\n");
		return (USB_ERR_NOMEM);
	}

	DPRINTFN(1, "allocated %d bytes frame buffer\n", fb->buf_size);

	fb->sample = 0;
	fb->fid = 0;
	fb->offset = 0;
	fb->error = 0;
	fb->mmap_q_full = 0;
	fb->fmt_flags = sc->sc_fmtgrp_cur->frame_cur->bDescriptorSubtype ==
	    UDESCSUB_VS_FRAME_UNCOMPRESSED ? 0 : V4L2_FMT_FLAG_COMPRESSED;

	return (USB_ERR_NORMAL_COMPLETION);
}

static void
uvideo_vs_free_frame(struct uvideo_softc *sc)
{
	struct uvideo_frame_buffer *fb = &sc->sc_frame_buffer;

	if (fb->buf != NULL) {
		free(fb->buf, M_USBDEV);
		fb->buf = NULL;
	}

	if (sc->sc_mmap_buffer != NULL) {
		contigfree(sc->sc_mmap_buffer, sc->sc_mmap_buffer_size,
		    M_USBDEV);
		sc->sc_mmap_buffer = NULL;
		sc->sc_mmap_buffer_size = 0;
	}

	while (!STAILQ_EMPTY(&sc->sc_mmap_q))
		STAILQ_REMOVE_HEAD(&sc->sc_mmap_q, q_frames);

	sc->sc_mmap_count = 0;
}

static usb_error_t
uvideo_vs_open(struct uvideo_softc *sc)
{
	usb_error_t error;
	uint32_t dwMaxVideoFrameSize;
	uint8_t iface_index;

	DPRINTFN(1, "uvideo_vs_open\n");

	if (sc->sc_negotiated_flag == 0) {
		error = uvideo_vs_negotiation(sc, 1);
		if (error != USB_ERR_NORMAL_COMPLETION)
			return (error);
	}

	/* For bulk endpoints, alt 0 is always used */
	if (!sc->sc_vs_cur->bulk_endpoint) {
		/*
		 * Set alternate interface to the one matching
		 * dwMaxPayloadTransferSize.
		 */
		error = usbd_set_alt_interface_index(sc->sc_udev,
		    sc->sc_vs_cur->iface_index, sc->sc_vs_cur->curalt);
		if (error != USB_ERR_NORMAL_COMPLETION) {
			device_printf(sc->sc_dev,
			    "could not set alt interface %d!\n",
			    sc->sc_vs_cur->curalt);
			return (error);
		}
	}

	/*
	 * Setup USB transfers.  FreeBSD uses declarative config + callback.
	 */
	iface_index = sc->sc_vs_cur->iface_index;
	if (sc->sc_vs_cur->bulk_endpoint) {
		error = usbd_transfer_setup(sc->sc_udev, &iface_index,
		    sc->sc_xfer, uvideo_bulk_config, 1, sc, &sc->sc_mtx);
	} else {
		error = usbd_transfer_setup(sc->sc_udev, &iface_index,
		    sc->sc_xfer, uvideo_isoc_config, UVIDEO_IXFERS, sc,
		    &sc->sc_mtx);
	}
	if (error != USB_ERR_NORMAL_COMPLETION) {
		device_printf(sc->sc_dev, "transfer setup failed: %s\n",
		    usbd_errstr(error));
		return (error);
	}

	/* Calculate optimal isoc transfer size */
	dwMaxVideoFrameSize = UGETDW(sc->sc_desc_probe.dwMaxVideoFrameSize);
	sc->sc_nframes = (dwMaxVideoFrameSize + sc->sc_vs_cur->psize - 1) /
	    sc->sc_vs_cur->psize;
	if (sc->sc_nframes > UVIDEO_NFRAMES_MAX)
		sc->sc_nframes = UVIDEO_NFRAMES_MAX;

	/* Pre-allocate scratch buffer for bulk USB callbacks */
	if (sc->sc_vs_cur->bulk_endpoint) {
		sc->sc_tmpbuf_size = 65536;
		sc->sc_tmpbuf = malloc(sc->sc_tmpbuf_size, M_USBDEV, M_WAITOK);
	}

	device_printf(sc->sc_dev, "stream open: nframes=%d, psize=%u, "
	    "maxVideoFrameSize=%u, maxPayloadSize=%u, alt=%d, %s\n",
	    sc->sc_nframes, sc->sc_vs_cur->psize,
	    UGETDW(sc->sc_desc_probe.dwMaxVideoFrameSize),
	    UGETDW(sc->sc_desc_probe.dwMaxPayloadTransferSize),
	    sc->sc_vs_cur->curalt,
	    sc->sc_vs_cur->bulk_endpoint ? "bulk" : "isoc");

	return (USB_ERR_NORMAL_COMPLETION);
}

static void
uvideo_vs_close(struct uvideo_softc *sc)
{

	DPRINTFN(1, "uvideo_vs_close\n");

	/* Stop and drain all transfers */
	usbd_transfer_unsetup(sc->sc_xfer, UVIDEO_N_XFER);

	if (sc->sc_tmpbuf != NULL) {
		free(sc->sc_tmpbuf, M_USBDEV);
		sc->sc_tmpbuf = NULL;
		sc->sc_tmpbuf_size = 0;
	}

	if (sc->sc_dying)
		return;

	if (!sc->sc_vs_cur->bulk_endpoint) {
		/* Switch back to alt 0 (turns off camera LED) */
		usbd_set_alt_interface_index(sc->sc_udev,
		    sc->sc_vs_cur->iface_index, 0);
	}
}

static usb_error_t
uvideo_vs_init(struct uvideo_softc *sc)
{
	usb_error_t error;

	error = uvideo_vs_open(sc);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (USB_ERR_INVAL);

	error = uvideo_vs_alloc_frame(sc);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (USB_ERR_INVAL);

	return (USB_ERR_NORMAL_COMPLETION);
}

/* ---------------------------------------------------------------- */
/*  Transfer Callbacks                                              */
/* ---------------------------------------------------------------- */

/*
 * Zero-copy isochronous decode: read only the 2-byte UVC stream header
 * from the USB page cache, then copy the payload directly into the
 * destination frame buffer, skipping the intermediate staging buffer.
 */
static void
uvideo_isoc_decode(struct uvideo_softc *sc, struct usb_page_cache *pc,
    int offset, int len)
{
	struct uvideo_frame_buffer *fb = &sc->sc_frame_buffer;
	uint8_t shdr[2];
	uint8_t flags;
	uint8_t *buf;
	int hdrlen, payload_len;

	if (len < UVIDEO_SH_MIN_LEN)
		return;

	/* Read only bLength and bFlags (2 bytes) */
	usbd_copy_out(pc, offset, shdr, sizeof(shdr));

	hdrlen = shdr[0];
	flags = shdr[1];
	if (hdrlen > len || hdrlen < UVIDEO_SH_MIN_LEN)
		return;

	if (fb->sample == 0) {
		fb->sample = 1;
		fb->fid = flags & UVIDEO_SH_FLAG_FID;
		fb->offset = 0;
		fb->error = 0;
		fb->mmap_q_full = 0;
	} else if (fb->fid != (flags & UVIDEO_SH_FLAG_FID)) {
		DPRINTFN(1, "wrong FID, ignoring last frame\n");
		fb->sample = 1;
		fb->fid = flags & UVIDEO_SH_FLAG_FID;
		fb->offset = 0;
		fb->error = 0;
		fb->mmap_q_full = 0;
	}

	if (flags & UVIDEO_SH_FLAG_ERR) {
		DPRINTFN(1, "stream error!\n");
		fb->error = 1;
	}

	/* Get destination buffer */
	if (sc->sc_mmap_flag) {
		if (!fb->mmap_q_full) {
			buf = uvideo_mmap_getbuf(sc);
			if (buf == NULL)
				fb->mmap_q_full = 1;
		}
	} else
		buf = fb->buf;

	/* Copy payload directly from USB DMA into frame buffer */
	payload_len = len - hdrlen;
	if (payload_len > fb->buf_size - fb->offset) {
		DPRINTFN(1, "frame too large, marked as error\n");
		payload_len = fb->buf_size - fb->offset;
		fb->error = 1;
	}
	if (!fb->mmap_q_full && payload_len > 0) {
		usbd_copy_out(pc, offset + hdrlen,
		    buf + fb->offset, payload_len);
		fb->offset += payload_len;
	}

	if (flags & UVIDEO_SH_FLAG_EOF) {
		DPRINTFN(2, "EOF (frame size=%d bytes)\n", fb->offset);

		if (fb->offset < fb->buf_size &&
		    !(fb->fmt_flags & V4L2_FMT_FLAG_COMPRESSED)) {
			DPRINTFN(1, "frame too small, marked as error\n");
			fb->error = 1;
		}

		if (sc->sc_mmap_flag) {
			if (!fb->mmap_q_full)
				uvideo_mmap_queue(sc, fb->offset, fb->error);
		} else if (fb->error) {
			DPRINTFN(1, "error frame, skipped\n");
		} else {
			uvideo_read_frame(sc, fb->buf, fb->offset);
		}

		fb->sample = 0;
		fb->fid = 0;
		fb->error = 0;
		fb->mmap_q_full = 0;
	}
}

static void
uvideo_isoc_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uvideo_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	int nframes, i, offset, len;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		pc = usbd_xfer_get_frame(xfer, 0);
		nframes = usbd_xfer_max_frames(xfer);
		offset = 0;
		for (i = 0; i < nframes; i++) {
			len = usbd_xfer_frame_len(xfer, i);
			if (len > 0)
				uvideo_isoc_decode(sc, pc, offset, len);
			offset += usbd_xfer_old_frame_length(xfer, i);
		}
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		nframes = usbd_xfer_max_frames(xfer);
		usbd_xfer_set_frames(xfer, nframes);
		for (i = 0; i < nframes; i++)
			usbd_xfer_set_frame_len(xfer, i,
			    sc->sc_vs_cur->psize);
		usbd_transfer_submit(xfer);
		break;
	default:
		if (error != USB_ERR_CANCELLED) {
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}
}

static void
uvideo_bulk_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uvideo_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	int actlen;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		if (actlen > 0 && actlen <= sc->sc_tmpbuf_size) {
			pc = usbd_xfer_get_frame(xfer, 0);
			usbd_copy_out(pc, 0, sc->sc_tmpbuf, actlen);
			sc->sc_decode_stream_header(sc, sc->sc_tmpbuf,
			    actlen);
		}
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		usbd_xfer_set_frame_len(xfer, 0,
		    usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
		break;
	default:
		if (error != USB_ERR_CANCELLED) {
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}
}

/* ---------------------------------------------------------------- */
/*  Frame Assembly                                                  */
/* ---------------------------------------------------------------- */

static void
uvideo_vs_decode_stream_header(struct uvideo_softc *sc, uint8_t *frame,
    int frame_size)
{
	struct uvideo_frame_buffer *fb = &sc->sc_frame_buffer;
	struct usb_video_stream_header *sh;
	int sample_len;
	uint8_t *buf;

	if (frame_size < UVIDEO_SH_MIN_LEN)
		return;

	sh = (struct usb_video_stream_header *)frame;

	if (sh->bLength > frame_size || sh->bLength < UVIDEO_SH_MIN_LEN)
		return;

	if (fb->sample == 0) {
		/* First sample for a frame */
		fb->sample = 1;
		fb->fid = sh->bFlags & UVIDEO_SH_FLAG_FID;
		fb->offset = 0;
		fb->error = 0;
		fb->mmap_q_full = 0;
	} else {
		/* Continuation sample; check FID consistency */
		if (fb->fid != (sh->bFlags & UVIDEO_SH_FLAG_FID)) {
			DPRINTFN(1, "wrong FID, ignoring last frame\n");
			fb->sample = 1;
			fb->fid = sh->bFlags & UVIDEO_SH_FLAG_FID;
			fb->offset = 0;
			fb->error = 0;
			fb->mmap_q_full = 0;
		}
	}

	if (sh->bFlags & UVIDEO_SH_FLAG_ERR) {
		DPRINTFN(1, "stream error!\n");
		fb->error = 1;
	}

	if (sc->sc_mmap_flag) {
		if (!fb->mmap_q_full) {
			buf = uvideo_mmap_getbuf(sc);
			if (buf == NULL)
				fb->mmap_q_full = 1;
		}
	} else
		buf = sc->sc_frame_buffer.buf;

	/* Save sample data */
	sample_len = frame_size - sh->bLength;
	if (sample_len > fb->buf_size - fb->offset) {
		DPRINTFN(1, "frame too large, marked as error\n");
		sample_len = fb->buf_size - fb->offset;
		fb->error = 1;
	}
	if (!fb->mmap_q_full && sample_len > 0) {
		bcopy(frame + sh->bLength, buf + fb->offset, sample_len);
		fb->offset += sample_len;
	}

	if (sh->bFlags & UVIDEO_SH_FLAG_EOF) {
		/* Got a full frame */
		DPRINTFN(2, "EOF (frame size=%d bytes)\n", fb->offset);

		if (fb->offset < fb->buf_size &&
		    !(fb->fmt_flags & V4L2_FMT_FLAG_COMPRESSED)) {
			DPRINTFN(1, "frame too small, marked as error\n");
			fb->error = 1;
		}

		if (sc->sc_mmap_flag) {
			if (!fb->mmap_q_full)
				uvideo_mmap_queue(sc, fb->offset, fb->error);
		} else if (fb->error) {
			DPRINTFN(1, "error frame, skipped\n");
		} else {
			uvideo_read_frame(sc, fb->buf, fb->offset);
		}

		fb->sample = 0;
		fb->fid = 0;
		fb->error = 0;
		fb->mmap_q_full = 0;
	}
}

static uint8_t *
uvideo_mmap_getbuf(struct uvideo_softc *sc)
{
	int i, idx;

	/*
	 * Multiple frames per transfer / multiple transfers per frame.
	 */
	if (sc->sc_mmap_cur != NULL)
		return (sc->sc_mmap_cur->buf);

	if (sc->sc_mmap_count == 0 || sc->sc_mmap_buffer == NULL)
		return (NULL);

	idx = sc->sc_mmap_buffer_idx;

	/* Find a buffer which is queued and ready */
	for (i = 0; i < sc->sc_mmap_count; i++) {
		if (sc->sc_mmap[sc->sc_mmap_buffer_idx].v4l2_buf.flags &
		    V4L2_BUF_FLAG_QUEUED) {
			idx = sc->sc_mmap_buffer_idx;
			if (++sc->sc_mmap_buffer_idx == sc->sc_mmap_count)
				sc->sc_mmap_buffer_idx = 0;
			break;
		}
		if (++sc->sc_mmap_buffer_idx == sc->sc_mmap_count)
			sc->sc_mmap_buffer_idx = 0;
	}

	if (i == sc->sc_mmap_count) {
		DPRINTFN(1, "mmap queue is full!\n");
		return (NULL);
	}

	sc->sc_mmap_cur = &sc->sc_mmap[idx];
	return (sc->sc_mmap_cur->buf);
}

static void
uvideo_mmap_queue(struct uvideo_softc *sc, int len, int err)
{

	if (sc->sc_mmap_cur == NULL)
		return;

	sc->sc_mmap_cur->v4l2_buf.bytesused = len;

	getmicrouptime(&sc->sc_mmap_cur->v4l2_buf.timestamp);
	sc->sc_mmap_cur->v4l2_buf.flags &= ~V4L2_BUF_FLAG_TIMESTAMP_MASK;
	sc->sc_mmap_cur->v4l2_buf.flags |= V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	sc->sc_mmap_cur->v4l2_buf.flags &= ~V4L2_BUF_FLAG_TSTAMP_SRC_MASK;
	sc->sc_mmap_cur->v4l2_buf.flags |= V4L2_BUF_FLAG_TSTAMP_SRC_EOF;
	sc->sc_mmap_cur->v4l2_buf.flags &= ~V4L2_BUF_FLAG_TIMECODE;

	sc->sc_mmap_cur->v4l2_buf.flags &= ~V4L2_BUF_FLAG_ERROR;
	if (err)
		sc->sc_mmap_cur->v4l2_buf.flags |= V4L2_BUF_FLAG_ERROR;

	sc->sc_mmap_cur->v4l2_buf.flags |= V4L2_BUF_FLAG_DONE;
	sc->sc_mmap_cur->v4l2_buf.flags &= ~V4L2_BUF_FLAG_QUEUED;
	STAILQ_INSERT_TAIL(&sc->sc_mmap_q, sc->sc_mmap_cur, q_frames);
	sc->sc_mmap_cur = NULL;

	DPRINTFN(2, "frame queued\n");

	wakeup(sc);
	selwakeup(&sc->sc_selinfo);
	KNOTE_LOCKED(&sc->sc_selinfo.si_note, 0);
}

static void
uvideo_read_frame(struct uvideo_softc *sc, uint8_t *buf, int len)
{

	/*
	 * In read mode, copy the frame into the upper-layer buffer
	 * so the USB callback can start assembling the next frame
	 * without racing with the cdev read.
	 */
	if (sc->sc_fbuffer == NULL || len > sc->sc_fbufferlen)
		return;

	bcopy(buf, sc->sc_fbuffer, len);
	sc->sc_fsize = len;
	sc->sc_frames_ready++;

	wakeup(sc);
	selwakeup(&sc->sc_selinfo);
	KNOTE_LOCKED(&sc->sc_selinfo.si_note, 0);
}

/* ---------------------------------------------------------------- */
/*  Character Device Operations                                     */
/* ---------------------------------------------------------------- */

static int
uvideo_cdev_open(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct uvideo_softc *sc = dev->si_drv1;

	if (sc == NULL || sc->sc_dying)
		return (ENXIO);

	if (sc->sc_vs_cur == NULL)
		return (EIO);

	mtx_lock(&sc->sc_mtx);
	if (sc->sc_open == 0) {
		/* First open: initialize state */
		sc->sc_owner = td->td_proc;
		sc->sc_mmap_flag = 0;
		sc->sc_negotiated_flag = 0;
		sc->sc_vidmode = VIDMODE_NONE;
		sc->sc_frames_ready = 0;
		sc->sc_priority = 1;	/* V4L2_PRIORITY_DEFAULT */
	}
	sc->sc_open++;
	mtx_unlock(&sc->sc_mtx);

	return (0);
}

static int
uvideo_cdev_close(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct uvideo_softc *sc = dev->si_drv1;

	if (sc == NULL)
		return (0);

	mtx_lock(&sc->sc_mtx);
	sc->sc_open--;
	if (sc->sc_open > 0) {
		mtx_unlock(&sc->sc_mtx);
		return (0);
	}
	mtx_unlock(&sc->sc_mtx);

	/* Last close: stop streaming if active */
	if (sc->sc_streaming) {
		mtx_lock(&sc->sc_mtx);
		sc->sc_streaming = 0;
		mtx_unlock(&sc->sc_mtx);
		uvideo_vs_close(sc);
		uvideo_vs_free_frame(sc);
	}

	if (sc->sc_fbuffer != NULL) {
		free(sc->sc_fbuffer, M_USBDEV);
		sc->sc_fbuffer = NULL;
		sc->sc_fbufferlen = 0;
	}

	sc->sc_open = 0;
	sc->sc_owner = NULL;
	sc->sc_vidmode = VIDMODE_NONE;

	return (0);
}

static int
uvideo_cdev_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct uvideo_softc *sc = dev->si_drv1;
	usb_error_t error;
	int ret;

	if (sc == NULL || sc->sc_dying)
		return (ENXIO);

	if (sc->sc_vs_cur == NULL)
		return (EIO);

	/* Start streaming in read mode if not already running */
	if (sc->sc_vidmode == VIDMODE_NONE) {
		sc->sc_mmap_flag = 0;
		sc->sc_vidmode = VIDMODE_READ;

		error = uvideo_vs_init(sc);
		if (error != USB_ERR_NORMAL_COMPLETION) {
			sc->sc_vidmode = VIDMODE_NONE;
			return (EIO);
		}

		/* Allocate a separate read buffer for frame delivery */
		sc->sc_fbufferlen = sc->sc_max_fbuf_size;
		if (sc->sc_fbufferlen == 0)
			sc->sc_fbufferlen =
			    UGETDW(sc->sc_desc_probe.dwMaxVideoFrameSize);
		if (sc->sc_fbuffer == NULL) {
			sc->sc_fbuffer = malloc(sc->sc_fbufferlen, M_USBDEV,
			    M_WAITOK | M_ZERO);
			if (sc->sc_fbuffer == NULL) {
				sc->sc_vidmode = VIDMODE_NONE;
				return (ENOMEM);
			}
		}

		mtx_lock(&sc->sc_mtx);
		sc->sc_streaming = 1;
		if (sc->sc_vs_cur->bulk_endpoint)
			usbd_transfer_start(sc->sc_xfer[0]);
		else {
			int i;
			for (i = 0; i < UVIDEO_IXFERS; i++)
				usbd_transfer_start(sc->sc_xfer[i]);
		}
		mtx_unlock(&sc->sc_mtx);
	}

	if (sc->sc_vidmode != VIDMODE_READ)
		return (EBUSY);

	/* Wait for a frame */
	while (sc->sc_frames_ready == 0) {
		if (ioflag & IO_NDELAY)
			return (EWOULDBLOCK);
		ret = tsleep(sc, PCATCH, "uvread", hz * 10);
		if (ret != 0)
			return (ret);
		if (sc->sc_dying)
			return (ENXIO);
	}

	sc->sc_frames_ready--;

	if (sc->sc_fsize == 0)
		return (0);

	return (uiomove(sc->sc_fbuffer, MIN(uio->uio_resid, sc->sc_fsize),
	    uio));
}

static int
uvideo_cdev_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct uvideo_softc *sc = dev->si_drv1;

	if (sc == NULL || sc->sc_dying)
		return (ENXIO);

	DPRINTFN(2, "ioctl cmd=0x%08lx\n", cmd);

	switch (cmd) {
	case VIDIOC_QUERYCAP:
		return (uvideo_querycap(sc, (struct v4l2_capability *)data));
	case VIDIOC_ENUM_FMT:
		return (uvideo_enum_fmt(sc, (struct v4l2_fmtdesc *)data));
	case VIDIOC_ENUM_FRAMESIZES:
		return (uvideo_enum_fsizes(sc,
		    (struct v4l2_frmsizeenum *)data));
	case VIDIOC_ENUM_FRAMEINTERVALS:
		return (uvideo_enum_fivals(sc,
		    (struct v4l2_frmivalenum *)data));
	case VIDIOC_S_FMT:
		return (uvideo_s_fmt(sc, (struct v4l2_format *)data));
	case VIDIOC_G_FMT:
		return (uvideo_g_fmt(sc, (struct v4l2_format *)data));
	case VIDIOC_TRY_FMT:
		return (uvideo_try_fmt(sc, (struct v4l2_format *)data));
	case VIDIOC_S_PARM:
		return (uvideo_s_parm(sc, (struct v4l2_streamparm *)data));
	case VIDIOC_G_PARM:
		return (uvideo_g_parm(sc, (struct v4l2_streamparm *)data));
	case VIDIOC_ENUMINPUT:
		return (uvideo_enum_input(sc, (struct v4l2_input *)data));
	case VIDIOC_S_INPUT:
		return (uvideo_s_input(sc, *(int *)data));
	case VIDIOC_G_INPUT:
		return (uvideo_g_input(sc, (int *)data));
	case VIDIOC_REQBUFS:
		return (uvideo_reqbufs(sc,
		    (struct v4l2_requestbuffers *)data));
	case VIDIOC_QUERYBUF:
		return (uvideo_querybuf(sc, (struct v4l2_buffer *)data));
	case VIDIOC_QBUF:
		return (uvideo_qbuf(sc, (struct v4l2_buffer *)data));
	case VIDIOC_DQBUF:
		return (uvideo_dqbuf(sc, (struct v4l2_buffer *)data));
	case VIDIOC_STREAMON:
		return (uvideo_streamon(sc, *(int *)data));
	case VIDIOC_STREAMOFF:
		return (uvideo_streamoff(sc, *(int *)data));
	case VIDIOC_QUERYCTRL:
		return (uvideo_queryctrl(sc,
		    (struct v4l2_queryctrl *)data));
	case VIDIOC_G_CTRL:
		return (uvideo_g_ctrl(sc, (struct v4l2_control *)data));
	case VIDIOC_S_CTRL:
		return (uvideo_s_ctrl(sc, (struct v4l2_control *)data));
	case VIDIOC_G_PRIORITY:
		*(uint32_t *)data = sc->sc_priority;
		return (0);
	case VIDIOC_S_PRIORITY:
		sc->sc_priority = *(uint32_t *)data;
		return (0);
	default:
		return (ENOTTY);
	}
}

static int
uvideo_cdev_poll(struct cdev *dev, int events, struct thread *td)
{
	struct uvideo_softc *sc = dev->si_drv1;
	int revents = 0;

	if (sc == NULL || sc->sc_dying)
		return (POLLHUP);

	if (events & (POLLIN | POLLRDNORM)) {
		if (sc->sc_mmap_flag) {
			if (!STAILQ_EMPTY(&sc->sc_mmap_q))
				revents |= events & (POLLIN | POLLRDNORM);
		} else {
			if (sc->sc_frames_ready > 0)
				revents |= events & (POLLIN | POLLRDNORM);
		}
		if (revents == 0)
			selrecord(td, &sc->sc_selinfo);
	}

	return (revents);
}

static void
uvideo_kqfilter_detach(struct knote *kn)
{
	struct uvideo_softc *sc = kn->kn_hook;

	knlist_remove(&sc->sc_selinfo.si_note, kn, 0);
}

static int
uvideo_kqfilter_read(struct knote *kn, long hint __unused)
{
	struct uvideo_softc *sc = kn->kn_hook;

	if (sc->sc_mmap_flag)
		return (!STAILQ_EMPTY(&sc->sc_mmap_q));
	return (sc->sc_frames_ready > 0);
}

static struct filterops uvideo_filtops_read = {
	.f_isfd = 1,
	.f_detach = uvideo_kqfilter_detach,
	.f_event = uvideo_kqfilter_read,
};

static int
uvideo_cdev_kqfilter(struct cdev *dev, struct knote *kn)
{
	struct uvideo_softc *sc = dev->si_drv1;

	if (sc == NULL || sc->sc_dying)
		return (ENXIO);

	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &uvideo_filtops_read;
		kn->kn_hook = sc;
		knlist_add(&sc->sc_selinfo.si_note, kn, 0);
		return (0);
	default:
		return (EINVAL);
	}
}

static int
uvideo_cdev_mmap(struct cdev *dev, vm_ooffset_t offset, vm_paddr_t *paddr,
    int nprot, vm_memattr_t *memattr)
{
	struct uvideo_softc *sc = dev->si_drv1;

	if (sc == NULL || sc->sc_dying)
		return (ENXIO);

	if (offset >= sc->sc_mmap_buffer_size)
		return (EINVAL);

	if (sc->sc_mmap_buffer == NULL)
		return (EINVAL);

	if (!sc->sc_mmap_flag)
		sc->sc_mmap_flag = 1;

	*paddr = vtophys(sc->sc_mmap_buffer + offset);

	return (0);
}

/* ---------------------------------------------------------------- */
/*  V4L2 Ioctl Handlers                                             */
/* ---------------------------------------------------------------- */

static int
uvideo_querycap(struct uvideo_softc *sc, struct v4l2_capability *caps)
{

	bzero(caps, sizeof(*caps));
	strlcpy(caps->driver, "uvideo", sizeof(caps->driver));
	strlcpy(caps->card, usb_get_product(sc->sc_udev),
	    sizeof(caps->card));
	snprintf(caps->bus_info, sizeof(caps->bus_info), "usb-%s",
	    device_get_nameunit(sc->sc_dev));

	caps->version = (5 << 16) | (0 << 8) | 0;	/* 5.0.0 */
	caps->device_caps = V4L2_CAP_VIDEO_CAPTURE |
	    V4L2_CAP_STREAMING | V4L2_CAP_READWRITE |
	    V4L2_CAP_EXT_PIX_FORMAT;
	caps->capabilities = caps->device_caps | V4L2_CAP_DEVICE_CAPS;

	return (0);
}

/*
 * Map pixel format to canonical V4L2 description string.
 * v4l2-compliance checks these names against an internal table.
 */
static const struct {
	uint32_t	pixfmt;
	const char	*name;
	uint32_t	flags;
} uvideo_fmt_names[] = {
	{ V4L2_PIX_FMT_MJPEG,		"Motion-JPEG",		V4L2_FMT_FLAG_COMPRESSED },
	{ V4L2_PIX_FMT_YUYV,		"YUYV 4:2:2",		0 },
	{ V4L2_PIX_FMT_YVU420,		"Planar YVU 4:2:0",	0 },
	{ V4L2_PIX_FMT_YUV420,		"Planar YUV 4:2:0",	0 },
	{ V4L2_PIX_FMT_GREY,		"8-bit Greyscale",	0 },
	{ V4L2_PIX_FMT_RGB565,		"16-bit RGB 5-6-5",	0 },
	{ V4L2_PIX_FMT_BGR24,		"24-bit BGR 8-8-8",	0 },
	{ V4L2_PIX_FMT_XBGR32,	"32-bit BGRX 8-8-8-8",	0 },
	{ V4L2_PIX_FMT_H264,		"H.264",		V4L2_FMT_FLAG_COMPRESSED },
	{ V4L2_PIX_FMT_HEVC,		"HEVC",			V4L2_FMT_FLAG_COMPRESSED },
	{ V4L2_PIX_FMT_SBGGR8,		"8-bit Bayer BGBG/GRGR", 0 },
	{ V4L2_PIX_FMT_SGBRG8,		"8-bit Bayer GBGB/RGRG", 0 },
	{ V4L2_PIX_FMT_SGRBG8,		"8-bit Bayer GRGR/BGBG", 0 },
	{ V4L2_PIX_FMT_SRGGB8,		"8-bit Bayer RGRG/GBGB", 0 },
	{ 0, NULL, 0 }
};

static int
uvideo_enum_fmt(struct uvideo_softc *sc, struct v4l2_fmtdesc *fmtdesc)
{
	uint32_t idx, type, pixfmt, flags;
	const char *name;
	int i;

	type = fmtdesc->type;
	idx = fmtdesc->index;

	if (type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return (EINVAL);

	if (idx >= (uint32_t)sc->sc_fmtgrp_num)
		return (EINVAL);

	pixfmt = sc->sc_fmtgrp[idx].pixelformat;
	flags = 0;
	name = "Unknown Format";

	/* Look up canonical name and flags */
	for (i = 0; uvideo_fmt_names[i].name != NULL; i++) {
		if (uvideo_fmt_names[i].pixfmt == pixfmt) {
			name = uvideo_fmt_names[i].name;
			flags = uvideo_fmt_names[i].flags;
			break;
		}
	}

	/* Override flags for special descriptor subtypes */
	switch (sc->sc_fmtgrp[idx].format->bDescriptorSubtype) {
	case UDESCSUB_VS_FORMAT_MJPEG:
		pixfmt = V4L2_PIX_FMT_MJPEG;
		flags = V4L2_FMT_FLAG_COMPRESSED;
		name = "Motion-JPEG";
		break;
	case UDESCSUB_VS_FORMAT_H264:
	case UDESCSUB_VS_FORMAT_H264_SIMULCAST:
		pixfmt = V4L2_PIX_FMT_H264;
		flags = V4L2_FMT_FLAG_COMPRESSED;
		name = "H.264";
		break;
	case UDESCSUB_VS_FORMAT_FRAME_BASED:
		if (sc->sc_fmtgrp[idx].format->u.fb.bVariableSize)
			flags = V4L2_FMT_FLAG_COMPRESSED;
		break;
	}

	bzero(fmtdesc, sizeof(*fmtdesc));
	fmtdesc->index = idx;
	fmtdesc->type = type;
	fmtdesc->flags = flags;
	fmtdesc->pixelformat = pixfmt;
	strlcpy(fmtdesc->description, name, sizeof(fmtdesc->description));

	return (0);
}

static int
uvideo_enum_fsizes(struct uvideo_softc *sc, struct v4l2_frmsizeenum *fsizes)
{
	int idx, found = 0;
	uint32_t index, pixel_format;
	struct usb_video_frame_desc *frame;

	index = fsizes->index;
	pixel_format = fsizes->pixel_format;

	for (idx = 0; idx < sc->sc_fmtgrp_num; idx++) {
		if (sc->sc_fmtgrp[idx].pixelformat == pixel_format) {
			found = 1;
			break;
		}
	}
	if (found == 0)
		return (EINVAL);

	if (index >= (uint32_t)sc->sc_fmtgrp[idx].frame_num)
		return (EINVAL);

	bzero(fsizes, sizeof(*fsizes));
	fsizes->index = index;
	fsizes->pixel_format = pixel_format;
	fsizes->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	frame = sc->sc_fmtgrp[idx].frame[index];
	fsizes->discrete.width = UGETW(UVIDEO_FRAME_FIELD(frame, wWidth));
	fsizes->discrete.height = UGETW(UVIDEO_FRAME_FIELD(frame, wHeight));

	return (0);
}

static int
uvideo_enum_fivals(struct uvideo_softc *sc, struct v4l2_frmivalenum *fivals)
{
	int idx;
	struct uvideo_format_group *fmtgrp = NULL;
	struct usb_video_frame_desc *frame = NULL;
	uint8_t *p;
	uint32_t fi_index, fi_pixfmt, fi_width, fi_height;

	fi_index = fivals->index;
	fi_pixfmt = fivals->pixel_format;
	fi_width = fivals->width;
	fi_height = fivals->height;

	for (idx = 0; idx < sc->sc_fmtgrp_num; idx++) {
		if (sc->sc_fmtgrp[idx].pixelformat == fi_pixfmt) {
			fmtgrp = &sc->sc_fmtgrp[idx];
			break;
		}
	}
	if (fmtgrp == NULL)
		return (EINVAL);

	for (idx = 0; idx < fmtgrp->frame_num; idx++) {
		if (UGETW(UVIDEO_FRAME_FIELD(fmtgrp->frame[idx], wWidth))
		    == fi_width &&
		    UGETW(UVIDEO_FRAME_FIELD(fmtgrp->frame[idx], wHeight))
		    == fi_height) {
			frame = fmtgrp->frame[idx];
			break;
		}
	}
	if (frame == NULL)
		return (EINVAL);

	p = (uint8_t *)frame + UVIDEO_FRAME_MIN_LEN(frame);

	bzero(fivals, sizeof(*fivals));
	fivals->index = fi_index;
	fivals->pixel_format = fi_pixfmt;
	fivals->width = fi_width;
	fivals->height = fi_height;

	if (UVIDEO_FRAME_NUM_INTERVALS(frame) == 0) {
		if (fi_index != 0)
			return (EINVAL);
		fivals->type = V4L2_FRMIVAL_TYPE_STEPWISE;
		fivals->stepwise.min.numerator = UGETDW(p);
		fivals->stepwise.min.denominator = 10000000;
		p += sizeof(uDWord);
		fivals->stepwise.max.numerator = UGETDW(p);
		fivals->stepwise.max.denominator = 10000000;
		p += sizeof(uDWord);
		fivals->stepwise.step.numerator = UGETDW(p);
		fivals->stepwise.step.denominator = 10000000;
	} else {
		if (fi_index >= (uint32_t)UVIDEO_FRAME_NUM_INTERVALS(frame))
			return (EINVAL);
		p += sizeof(uDWord) * fi_index;
		if (p > frame->bLength + (uint8_t *)frame) {
			device_printf(sc->sc_dev,
			    "frame desc too short?\n");
			return (EINVAL);
		}
		fivals->type = V4L2_FRMIVAL_TYPE_DISCRETE;
		fivals->discrete.numerator = UGETDW(p);
		fivals->discrete.denominator = 10000000;
	}

	return (0);
}

static int
uvideo_s_fmt(struct uvideo_softc *sc, struct v4l2_format *fmt)
{
	struct uvideo_format_group *fmtgrp_save;
	struct usb_video_frame_desc *frame_save;
	struct uvideo_res r;
	int found, i;
	usb_error_t error;

	if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return (EINVAL);

	DPRINTFN(1, "s_fmt: requested %dx%d\n",
	    fmt->fmt.pix.width, fmt->fmt.pix.height);

	/* Search requested pixel format */
	for (found = 0, i = 0; i < sc->sc_fmtgrp_num; i++) {
		if (fmt->fmt.pix.pixelformat == sc->sc_fmtgrp[i].pixelformat) {
			found = 1;
			break;
		}
	}
	if (found == 0)
		return (EINVAL);

	if (sc->sc_fmtgrp[i].frame_num == 0) {
		device_printf(sc->sc_dev, "no frame descriptors!\n");
		return (EINVAL);
	}

	uvideo_find_res(sc, i, fmt->fmt.pix.width, fmt->fmt.pix.height, &r);

	/* Save current format in case negotiation fails */
	fmtgrp_save = sc->sc_fmtgrp_cur;
	frame_save = sc->sc_fmtgrp_cur->frame_cur;

	sc->sc_fmtgrp_cur = &sc->sc_fmtgrp[i];
	sc->sc_fmtgrp[i].frame_cur = sc->sc_fmtgrp[i].frame[r.fidx];

	error = uvideo_vs_negotiation(sc, 1);
	if (error != USB_ERR_NORMAL_COMPLETION) {
		sc->sc_fmtgrp_cur = fmtgrp_save;
		sc->sc_fmtgrp_cur->frame_cur = frame_save;
		return (EINVAL);
	}
	sc->sc_negotiated_flag = 1;

	fmt->fmt.pix.width = r.width;
	fmt->fmt.pix.height = r.height;
	fmt->fmt.pix.sizeimage =
	    UGETDW(sc->sc_desc_probe.dwMaxVideoFrameSize);

	DPRINTFN(1, "s_fmt: offered %dx%d\n", r.width, r.height);

	return (0);
}

static int
uvideo_g_fmt(struct uvideo_softc *sc, struct v4l2_format *fmt)
{
	struct usb_video_frame_desc *frame;
	uint32_t type;

	type = fmt->type;
	if (type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return (EINVAL);

	bzero(fmt, sizeof(*fmt));
	fmt->type = type;
	fmt->fmt.pix.pixelformat = sc->sc_fmtgrp_cur->pixelformat;
	fmt->fmt.pix.field = V4L2_FIELD_NONE;

	frame = sc->sc_fmtgrp_cur->frame_cur;
	fmt->fmt.pix.width = UGETW(UVIDEO_FRAME_FIELD(frame, wWidth));
	fmt->fmt.pix.height = UGETW(UVIDEO_FRAME_FIELD(frame, wHeight));
	fmt->fmt.pix.sizeimage =
	    UGETDW(sc->sc_desc_probe.dwMaxVideoFrameSize);

	if (sc->sc_fmtgrp_cur->has_colorformat) {
		fmt->fmt.pix.colorspace = sc->sc_fmtgrp_cur->colorspace;
		fmt->fmt.pix.xfer_func = sc->sc_fmtgrp_cur->xfer_func;
		fmt->fmt.pix.ycbcr_enc = sc->sc_fmtgrp_cur->ycbcr_enc;
	}


	return (0);
}

static int
uvideo_s_parm(struct uvideo_softc *sc, struct v4l2_streamparm *parm)
{
	usb_error_t error;

	if (parm->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		if (parm->parm.capture.timeperframe.numerator == 0 ||
		    parm->parm.capture.timeperframe.denominator == 0)
			sc->sc_frame_rate = 0;
		else
			sc->sc_frame_rate =
			    parm->parm.capture.timeperframe.denominator /
			    parm->parm.capture.timeperframe.numerator;
	} else
		return (EINVAL);

	/* Renegotiate if needed */
	if (sc->sc_negotiated_flag) {
		error = uvideo_vs_negotiation(sc, 1);
		if (error != USB_ERR_NORMAL_COMPLETION)
			return (EINVAL);
	}

	/* Return current parameters (zeroes reserved fields) */
	return (uvideo_g_parm(sc, parm));
}

static int
uvideo_g_parm(struct uvideo_softc *sc, struct v4l2_streamparm *parm)
{
	uint32_t type;

	type = parm->type;
	if (type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return (EINVAL);

	bzero(parm, sizeof(*parm));
	parm->type = type;
	parm->parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
	parm->parm.capture.capturemode = 0;
	parm->parm.capture.readbuffers = UVIDEO_MAX_BUFFERS;
	parm->parm.capture.timeperframe.numerator =
	    UGETDW(sc->sc_desc_probe.dwFrameInterval);
	parm->parm.capture.timeperframe.denominator = 10000000;

	return (0);
}

static int
uvideo_enum_input(struct uvideo_softc *sc, struct v4l2_input *input)
{
	uint32_t idx;

	idx = input->index;
	if (idx != 0)
		return (EINVAL);

	bzero(input, sizeof(*input));
	input->index = idx;
	strlcpy(input->name, "Camera Terminal", sizeof(input->name));
	input->type = V4L2_INPUT_TYPE_CAMERA;
	input->status = 0;	/* no error */
	input->std = 0;		/* no standard TV norms */

	return (0);
}

static int
uvideo_s_input(struct uvideo_softc *sc, int input)
{

	if (input != 0)
		return (EINVAL);

	return (0);
}

static int
uvideo_g_input(struct uvideo_softc *sc, int *input)
{

	*input = 0;
	return (0);
}

static int
uvideo_reqbufs(struct uvideo_softc *sc, struct v4l2_requestbuffers *rb)
{
	int i, buf_size, buf_size_total;

	DPRINTFN(1, "reqbufs: count=%d\n", rb->count);

	if (rb->count == 0)
		return (EINVAL);

	if (sc->sc_mmap_count > 0 || sc->sc_mmap_buffer != NULL) {
		DPRINTFN(1, "mmap buffers already allocated\n");
		return (EINVAL);
	}

	if (rb->count > UVIDEO_MAX_BUFFERS)
		sc->sc_mmap_count = UVIDEO_MAX_BUFFERS;
	else
		sc->sc_mmap_count = rb->count;

	buf_size = UGETDW(sc->sc_desc_probe.dwMaxVideoFrameSize);
	buf_size_total = sc->sc_mmap_count * buf_size;
	buf_size_total = round_page(buf_size_total);

	sc->sc_mmap_buffer = contigmalloc(buf_size_total, M_USBDEV,
	    M_WAITOK | M_ZERO, 0, ~0UL, PAGE_SIZE, 0);
	if (sc->sc_mmap_buffer == NULL) {
		device_printf(sc->sc_dev, "can't allocate mmap buffer!\n");
		sc->sc_mmap_count = 0;
		return (ENOMEM);
	}
	sc->sc_mmap_buffer_size = buf_size_total;

	DPRINTFN(1, "allocated %d bytes mmap buffer\n", buf_size_total);

	for (i = 0; i < sc->sc_mmap_count; i++) {
		sc->sc_mmap[i].buf = sc->sc_mmap_buffer + (i * buf_size);

		sc->sc_mmap[i].v4l2_buf.index = i;
		sc->sc_mmap[i].v4l2_buf.m.offset = i * buf_size;
		sc->sc_mmap[i].v4l2_buf.length = buf_size;
		sc->sc_mmap[i].v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		sc->sc_mmap[i].v4l2_buf.sequence = 0;
		sc->sc_mmap[i].v4l2_buf.field = V4L2_FIELD_NONE;
		sc->sc_mmap[i].v4l2_buf.memory = V4L2_MEMORY_MMAP;
		sc->sc_mmap[i].v4l2_buf.flags = V4L2_BUF_FLAG_MAPPED;
	}

	sc->sc_mmap_buffer_idx = 0;
	sc->sc_mmap_cur = NULL;

	rb->count = sc->sc_mmap_count;
	rb->capabilities = V4L2_BUF_CAP_SUPPORTS_MMAP;

	return (0);
}

static int
uvideo_querybuf(struct uvideo_softc *sc, struct v4l2_buffer *qb)
{

	if (qb->type != V4L2_BUF_TYPE_VIDEO_CAPTURE ||
	    qb->memory != V4L2_MEMORY_MMAP ||
	    qb->index >= sc->sc_mmap_count)
		return (EINVAL);

	bcopy(&sc->sc_mmap[qb->index].v4l2_buf, qb,
	    sizeof(struct v4l2_buffer));

	return (0);
}

static int
uvideo_qbuf(struct uvideo_softc *sc, struct v4l2_buffer *qb)
{

	if (qb->type != V4L2_BUF_TYPE_VIDEO_CAPTURE ||
	    qb->memory != V4L2_MEMORY_MMAP ||
	    qb->index >= sc->sc_mmap_count)
		return (EINVAL);

	sc->sc_mmap[qb->index].v4l2_buf.flags &= ~V4L2_BUF_FLAG_DONE;
	sc->sc_mmap[qb->index].v4l2_buf.flags |= V4L2_BUF_FLAG_QUEUED;

	DPRINTFN(2, "buffer %d ready for queueing\n", qb->index);

	return (0);
}

static int
uvideo_dqbuf(struct uvideo_softc *sc, struct v4l2_buffer *dqb)
{
	struct uvideo_mmap *mmap;
	int error;

	if (dqb->type != V4L2_BUF_TYPE_VIDEO_CAPTURE ||
	    dqb->memory != V4L2_MEMORY_MMAP)
		return (EINVAL);

	if (STAILQ_EMPTY(&sc->sc_mmap_q)) {
		error = tsleep(sc, PCATCH, "uvdqbuf", hz * 10);
		if (error)
			return (EINVAL);
	}

	mmap = STAILQ_FIRST(&sc->sc_mmap_q);
	if (mmap == NULL)
		return (EINVAL);

	bcopy(&mmap->v4l2_buf, dqb, sizeof(struct v4l2_buffer));

	mmap->v4l2_buf.flags &= ~V4L2_BUF_FLAG_DONE;
	mmap->v4l2_buf.flags &= ~V4L2_BUF_FLAG_QUEUED;

	DPRINTFN(2, "frame dequeued from index %d\n",
	    mmap->v4l2_buf.index);
	STAILQ_REMOVE_HEAD(&sc->sc_mmap_q, q_frames);

	return (0);
}

static int
uvideo_streamon(struct uvideo_softc *sc, int type)
{
	usb_error_t error;

	if (type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return (EINVAL);

	if (sc->sc_streaming)
		return (0);

	sc->sc_vidmode = VIDMODE_MMAP;

	error = uvideo_vs_init(sc);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EINVAL);

	mtx_lock(&sc->sc_mtx);
	sc->sc_streaming = 1;
	if (sc->sc_vs_cur->bulk_endpoint)
		usbd_transfer_start(sc->sc_xfer[0]);
	else {
		int i;
		for (i = 0; i < UVIDEO_IXFERS; i++)
			usbd_transfer_start(sc->sc_xfer[i]);
	}
	mtx_unlock(&sc->sc_mtx);

	return (0);
}

static int
uvideo_streamoff(struct uvideo_softc *sc, int type)
{

	if (type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return (EINVAL);

	if (!sc->sc_streaming)
		return (0);

	mtx_lock(&sc->sc_mtx);
	sc->sc_streaming = 0;
	mtx_unlock(&sc->sc_mtx);

	uvideo_vs_close(sc);
	uvideo_vs_free_frame(sc);

	return (0);
}

static int
uvideo_try_fmt(struct uvideo_softc *sc, struct v4l2_format *fmt)
{
	struct uvideo_res r;
	int found, i;

	if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return (EINVAL);

	for (found = 0, i = 0; i < sc->sc_fmtgrp_num; i++) {
		if (fmt->fmt.pix.pixelformat == sc->sc_fmtgrp[i].pixelformat) {
			found = 1;
			break;
		}
	}
	if (found == 0)
		return (EINVAL);

	uvideo_find_res(sc, i, fmt->fmt.pix.width, fmt->fmt.pix.height, &r);

	fmt->fmt.pix.width = r.width;
	fmt->fmt.pix.height = r.height;
	fmt->fmt.pix.sizeimage = sc->sc_frame_buffer.buf_size;

	return (0);
}

static int
uvideo_queryctrl(struct uvideo_softc *sc, struct v4l2_queryctrl *qctrl)
{
	int i, ret = 0;
	usb_error_t error;
	uint8_t *ctrl_data;
	uint16_t ctrl_len;
	uint8_t unit_id;

	i = uvideo_find_ctrl(sc, qctrl->id);
	if (i == EINVAL)
		return (i);

	if (sc->sc_desc_vc_ct_cur != NULL)
		unit_id = sc->sc_desc_vc_ct_cur->bTerminalID;
	else
		unit_id = sc->sc_desc_vc_pu_cur->bUnitID;

	ctrl_len = uvideo_ctrls[i].ctrl_len;
	if (ctrl_len < 1 || ctrl_len > 4) {
		device_printf(sc->sc_dev,
		    "invalid control length: %d\n", ctrl_len);
		return (EINVAL);
	}

	ctrl_data = malloc(ctrl_len, M_USBDEV, M_WAITOK | M_ZERO);
	if (ctrl_data == NULL)
		return (ENOMEM);

	qctrl->type = uvideo_ctrls[i].type;
	strlcpy(qctrl->name, uvideo_ctrls[i].name, sizeof(qctrl->name));

	/* get minimum */
	error = uvideo_vc_get_ctrl(sc, ctrl_data, GET_MIN,
	    unit_id,
	    uvideo_ctrls[i].ctrl_selector, uvideo_ctrls[i].ctrl_len);
	if (error != USB_ERR_NORMAL_COMPLETION) {
		ret = EINVAL;
		goto out;
	}
	switch (ctrl_len) {
	case 1:
		qctrl->minimum = uvideo_ctrls[i].sig ?
		    *(int8_t *)ctrl_data : *ctrl_data;
		break;
	case 2:
		qctrl->minimum = uvideo_ctrls[i].sig ?
		    (int16_t)UGETW(ctrl_data) : UGETW(ctrl_data);
		break;
	case 4:
		qctrl->minimum = uvideo_ctrls[i].sig ?
		    (int32_t)UGETDW(ctrl_data) : UGETDW(ctrl_data);
		break;
	}

	/* get maximum */
	error = uvideo_vc_get_ctrl(sc, ctrl_data, GET_MAX,
	    unit_id,
	    uvideo_ctrls[i].ctrl_selector, uvideo_ctrls[i].ctrl_len);
	if (error != USB_ERR_NORMAL_COMPLETION) {
		ret = EINVAL;
		goto out;
	}
	switch (ctrl_len) {
	case 1:
		qctrl->maximum = uvideo_ctrls[i].sig ?
		    *(int8_t *)ctrl_data : *ctrl_data;
		break;
	case 2:
		qctrl->maximum = uvideo_ctrls[i].sig ?
		    (int16_t)UGETW(ctrl_data) : UGETW(ctrl_data);
		break;
	case 4:
		qctrl->maximum = uvideo_ctrls[i].sig ?
		    (int32_t)UGETDW(ctrl_data) : UGETDW(ctrl_data);
		break;
	}

	/* get resolution/step */
	error = uvideo_vc_get_ctrl(sc, ctrl_data, GET_RES,
	    unit_id,
	    uvideo_ctrls[i].ctrl_selector, uvideo_ctrls[i].ctrl_len);
	if (error != USB_ERR_NORMAL_COMPLETION) {
		ret = EINVAL;
		goto out;
	}
	switch (ctrl_len) {
	case 1:
		qctrl->step = uvideo_ctrls[i].sig ?
		    *(int8_t *)ctrl_data : *ctrl_data;
		break;
	case 2:
		qctrl->step = uvideo_ctrls[i].sig ?
		    (int16_t)UGETW(ctrl_data) : UGETW(ctrl_data);
		break;
	case 4:
		qctrl->step = uvideo_ctrls[i].sig ?
		    (int32_t)UGETDW(ctrl_data) : UGETDW(ctrl_data);
		break;
	}

	/* get default */
	error = uvideo_vc_get_ctrl(sc, ctrl_data, GET_DEF,
	    unit_id,
	    uvideo_ctrls[i].ctrl_selector, uvideo_ctrls[i].ctrl_len);
	if (error != USB_ERR_NORMAL_COMPLETION) {
		ret = EINVAL;
		goto out;
	}
	switch (ctrl_len) {
	case 1:
		qctrl->default_value = uvideo_ctrls[i].sig ?
		    *(int8_t *)ctrl_data : *ctrl_data;
		break;
	case 2:
		qctrl->default_value = uvideo_ctrls[i].sig ?
		    (int16_t)UGETW(ctrl_data) : UGETW(ctrl_data);
		break;
	case 4:
		qctrl->default_value = uvideo_ctrls[i].sig ?
		    (int32_t)UGETDW(ctrl_data) : UGETDW(ctrl_data);
		break;
	}

	qctrl->flags = 0;

out:
	free(ctrl_data, M_USBDEV);
	return (ret);
}

static int
uvideo_g_ctrl(struct uvideo_softc *sc, struct v4l2_control *gctrl)
{
	int i, ret = 0;
	usb_error_t error;
	uint8_t *ctrl_data;
	uint16_t ctrl_len;
	uint8_t unit_id;

	i = uvideo_find_ctrl(sc, gctrl->id);
	if (i == EINVAL)
		return (i);

	if (sc->sc_desc_vc_ct_cur != NULL)
		unit_id = sc->sc_desc_vc_ct_cur->bTerminalID;
	else
		unit_id = sc->sc_desc_vc_pu_cur->bUnitID;

	ctrl_len = uvideo_ctrls[i].ctrl_len;
	if (ctrl_len < 1 || ctrl_len > 4)
		return (EINVAL);

	ctrl_data = malloc(ctrl_len, M_USBDEV, M_WAITOK | M_ZERO);
	if (ctrl_data == NULL)
		return (ENOMEM);

	error = uvideo_vc_get_ctrl(sc, ctrl_data, GET_CUR,
	    unit_id,
	    uvideo_ctrls[i].ctrl_selector, uvideo_ctrls[i].ctrl_len);
	if (error != USB_ERR_NORMAL_COMPLETION) {
		ret = EINVAL;
		goto out;
	}
	switch (ctrl_len) {
	case 1:
		gctrl->value = uvideo_ctrls[i].sig ?
		    *(int8_t *)ctrl_data : *ctrl_data;
		break;
	case 2:
		gctrl->value = uvideo_ctrls[i].sig ?
		    (int16_t)UGETW(ctrl_data) : UGETW(ctrl_data);
		break;
	case 4:
		gctrl->value = uvideo_ctrls[i].sig ?
		    (int32_t)UGETDW(ctrl_data) : UGETDW(ctrl_data);
		break;
	}

out:
	free(ctrl_data, M_USBDEV);
	return (ret);
}

static int
uvideo_s_ctrl(struct uvideo_softc *sc, struct v4l2_control *sctrl)
{
	int i, ret = 0;
	usb_error_t error;
	uint8_t *ctrl_data;
	uint16_t ctrl_len;
	uint8_t unit_id;

	i = uvideo_find_ctrl(sc, sctrl->id);
	if (i == EINVAL)
		return (i);

	if (sc->sc_desc_vc_ct_cur != NULL)
		unit_id = sc->sc_desc_vc_ct_cur->bTerminalID;
	else
		unit_id = sc->sc_desc_vc_pu_cur->bUnitID;

	ctrl_len = uvideo_ctrls[i].ctrl_len;
	if (ctrl_len < 1 || ctrl_len > 4)
		return (EINVAL);

	ctrl_data = malloc(ctrl_len, M_USBDEV, M_WAITOK | M_ZERO);
	if (ctrl_data == NULL)
		return (ENOMEM);

	switch (ctrl_len) {
	case 1:
		if (uvideo_ctrls[i].sig)
			*(int8_t *)ctrl_data = sctrl->value;
		else
			*ctrl_data = sctrl->value;
		break;
	case 2:
		USETW(ctrl_data, sctrl->value);
		break;
	case 4:
		USETDW(ctrl_data, sctrl->value);
		break;
	}

	error = uvideo_vc_set_ctrl(sc, ctrl_data, SET_CUR,
	    unit_id,
	    uvideo_ctrls[i].ctrl_selector, uvideo_ctrls[i].ctrl_len);
	if (error != USB_ERR_NORMAL_COMPLETION)
		ret = EINVAL;

	free(ctrl_data, M_USBDEV);
	return (ret);
}
