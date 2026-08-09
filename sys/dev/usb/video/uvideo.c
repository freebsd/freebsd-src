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
#include <sys/proc.h>
#include <sys/limits.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/rwlock.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usb_request.h>
#include "usbdevs.h"

#include <dev/usb/video/uvideo.h>
#include <dev/video/video.h>

#include "video_if.h"

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
static void		uvideo_vs_decode_stream_header(struct uvideo_softc *,
			    uint8_t *, int);
static void		uvideo_vs_decode_stream_header_isight(
			    struct uvideo_softc *, uint8_t *, int);
static void		uvideo_isoc_decode(struct uvideo_softc *,
			    struct usb_page_cache *, int, int);
static void		uvideo_frame_done(struct uvideo_softc *);

static int	uvideo_hw_querycap(device_t, struct video_caps *);
static int	uvideo_hw_enum_format(device_t, uint32_t, struct video_format *);
static int	uvideo_hw_get_format(device_t, struct video_format *);
static int	uvideo_hw_try_format(device_t, struct video_format *);
static int	uvideo_hw_set_format(device_t, const struct video_format *);
static int	uvideo_hw_enum_framesizes(device_t, struct video_frmsizeenum *);
static int	uvideo_hw_enum_frameintervals(device_t,
		    struct video_frmivalenum *);
static int	uvideo_hw_get_parm(device_t, struct video_fract *);
static int	uvideo_hw_set_parm(device_t, struct video_fract *);
static int	uvideo_hw_enum_input(device_t, uint32_t, struct video_input *);
static int	uvideo_hw_get_input(device_t, uint32_t *);
static int	uvideo_hw_set_input(device_t, uint32_t);
static int	uvideo_hw_query_control(device_t, struct video_control_desc *);
static int	uvideo_hw_get_control(device_t, struct video_control *);
static int	uvideo_hw_set_control(device_t, const struct video_control *);
static int	uvideo_hw_start_stream(device_t);
static void	uvideo_hw_stop_stream(device_t);

/*
 * Transfer configuration indices.
 */
enum {
	UVIDEO_ISOC_RX_0,
	UVIDEO_ISOC_RX_1,
	UVIDEO_ISOC_RX_2,
	UVIDEO_ISOC_RX_3,
	UVIDEO_ISOC_RX_4,
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

	struct video_device	*sc_vd;
	uint32_t		sc_sequence;

	uint8_t			sc_iface_index;
	uint8_t			sc_nifaces;
	int			sc_dying;

	struct usb_xfer		*sc_xfer[UVIDEO_N_XFER];
	int			sc_streaming;

	int			sc_max_ctrl_size;
	uint32_t		sc_max_fbuf_size;
	int			sc_negotiated_flag;
	int			sc_frame_rate;

	struct uvideo_frame_buffer sc_frame_buffer;

	uint8_t			*sc_tmpbuf;
	int			sc_tmpbuf_size;

	int			sc_nframes;
	struct usb_video_probe_commit sc_desc_probe;
	struct usb_video_header_desc_all sc_desc_vc_header;
	struct usb_video_input_header_desc_all sc_desc_vs_input_header;

#define	UVIDEO_MAX_PU		32
	int			sc_desc_vc_pu_num;
	struct usb_video_vc_processing_desc *sc_desc_vc_pu_cur;
	struct usb_video_vc_processing_desc *sc_desc_vc_pu[UVIDEO_MAX_PU];

#define	UVIDEO_MAX_CT		32
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

	const struct uvideo_quirk *sc_quirk;

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
	{ UVIDEO_FORMAT_GUID_NV12, V4L2_PIX_FMT_NV12 },
	{ UVIDEO_FORMAT_GUID_NV21, V4L2_PIX_FMT_NV21 },
	{ UVIDEO_FORMAT_GUID_YV12, V4L2_PIX_FMT_YVU420 },
	{ UVIDEO_FORMAT_GUID_I420, V4L2_PIX_FMT_YUV420 },
	{ UVIDEO_FORMAT_GUID_M420, V4L2_PIX_FMT_M420 },
	{ UVIDEO_FORMAT_GUID_UYVY, V4L2_PIX_FMT_UYVY },
	{ UVIDEO_FORMAT_GUID_Y800, V4L2_PIX_FMT_GREY },
	{ UVIDEO_FORMAT_GUID_Y8, V4L2_PIX_FMT_GREY },
	{ UVIDEO_FORMAT_GUID_D3DFMT_L8, V4L2_PIX_FMT_GREY },
	{ UVIDEO_FORMAT_GUID_KSMEDIA_L8_IR, V4L2_PIX_FMT_GREY },
	{ UVIDEO_FORMAT_GUID_Y12, V4L2_PIX_FMT_Y12 },
	{ UVIDEO_FORMAT_GUID_Y16, V4L2_PIX_FMT_Y16 },
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
 * Quirk flags for devices needing special handling.
 */
#define	UVIDEO_FLAG_ISIGHT_STREAM_HEADER	0x01
#define	UVIDEO_FLAG_REATTACH			0x02
#define	UVIDEO_FLAG_VENDOR_CLASS		0x04
#define	UVIDEO_FLAG_NOATTACH			0x08
#define	UVIDEO_FLAG_FORMAT_INDEX_IN_BMHINT	0x10

/*
 * Devices which either fail to declare themselves as UICLASS_VIDEO,
 * or which need firmware uploads or other quirk handling later on.
 */
static const struct uvideo_quirk {
	struct usb_device_id	uv_dev;
	const char		*ucode_name;
	usb_error_t		(*ucode_loader)(struct uvideo_softc *);
	int			flags;
} uvideo_quirks[] = {
	{ { USB_VP(0x05ca, 0x1835) }, "uvideo_r5u87x_05ca-1835",
	  NULL, 0 },	/* Ricoh VGP VCC5 */
	{ { USB_VP(0x05ca, 0x1836) }, "uvideo_r5u87x_05ca-1836",
	  NULL, 0 },	/* Ricoh VGP VCC4 */
	{ { USB_VP(0x05ca, 0x1837) }, "uvideo_r5u87x_05ca-1837",
	  NULL, 0 },	/* Ricoh VGP VCC4 (2) */
	{ { USB_VP(0x05ca, 0x1839) }, "uvideo_r5u87x_05ca-1839",
	  NULL, 0 },	/* Ricoh VGP VCC6 */
	{ { USB_VP(0x05ca, 0x183a) }, "uvideo_r5u87x_05ca-183a",
	  NULL, 0 },	/* Ricoh VGP VCC7 */
	{ { USB_VP(0x05ca, 0x183b) }, "uvideo_r5u87x_05ca-183b",
	  NULL, 0 },	/* Ricoh VGP VCC8 */
	{ { USB_VP(0x05ca, 0x183e) }, "uvideo_r5u87x_05ca-183e",
	  NULL, 0 },	/* Ricoh VGP VCC9 */
	{ { USB_VP(0x05ac, 0x8300) }, "uvideo_isight_05ac-8300",
	  NULL, UVIDEO_FLAG_REATTACH },	/* Apple iSight (needs firmware) */
	{ { USB_VP(0x05ac, 0x8501) }, NULL, NULL,
	  UVIDEO_FLAG_ISIGHT_STREAM_HEADER },	/* Apple iSight (non-standard header) */
	{ { USB_VP(0x046d, 0x08b0) }, NULL, NULL,
	  UVIDEO_FLAG_VENDOR_CLASS },	/* Logitech QuickCam Fusion */
	{ { USB_VP(0x046d, 0x08bc) }, NULL, NULL,
	  UVIDEO_FLAG_VENDOR_CLASS },	/* Logitech QuickCam Orbit MP */
	{ { USB_VP(0x046d, 0x08c1) }, NULL, NULL,
	  UVIDEO_FLAG_VENDOR_CLASS },	/* Logitech QuickCam NB Pro */
	{ { USB_VP(0x046d, 0x08c6) }, NULL, NULL,
	  UVIDEO_FLAG_VENDOR_CLASS },	/* Logitech QuickCam Pro 5000 */
	{ { USB_VP(0x046d, 0x08c7) }, NULL, NULL,
	  UVIDEO_FLAG_VENDOR_CLASS },	/* Logitech QuickCam OEM */
	{ { USB_VP(0x046d, 0x08c8) }, NULL, NULL,
	  UVIDEO_FLAG_VENDOR_CLASS },	/* Logitech QuickCam OEM */
	{ { USB_VP(0x04f2, 0xb2ea) }, NULL, NULL,
	  UVIDEO_FLAG_NOATTACH },	/* Chicony IR camera (unsupported) */
	{ { USB_VP(0x0fd9, 0x0066) }, NULL, NULL,
	  UVIDEO_FLAG_FORMAT_INDEX_IN_BMHINT },	/* Elgato Game Capture HD60 */
};

static const struct uvideo_quirk *
uvideo_lookup_quirk(struct usb_attach_arg *uaa)
{
	int i;

	for (i = 0; i < nitems(uvideo_quirks); i++) {
		if (uaa->info.idVendor == uvideo_quirks[i].uv_dev.idVendor &&
		    uaa->info.idProduct == uvideo_quirks[i].uv_dev.idProduct)
			return (&uvideo_quirks[i]);
	}
	return (NULL);
}

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

	/* video(4) interface */
	DEVMETHOD(video_querycap, uvideo_hw_querycap),
	DEVMETHOD(video_enum_format, uvideo_hw_enum_format),
	DEVMETHOD(video_get_format, uvideo_hw_get_format),
	DEVMETHOD(video_try_format, uvideo_hw_try_format),
	DEVMETHOD(video_set_format, uvideo_hw_set_format),
	DEVMETHOD(video_enum_framesizes, uvideo_hw_enum_framesizes),
	DEVMETHOD(video_enum_frameintervals, uvideo_hw_enum_frameintervals),
	DEVMETHOD(video_get_parm, uvideo_hw_get_parm),
	DEVMETHOD(video_set_parm, uvideo_hw_set_parm),
	DEVMETHOD(video_enum_input, uvideo_hw_enum_input),
	DEVMETHOD(video_get_input, uvideo_hw_get_input),
	DEVMETHOD(video_set_input, uvideo_hw_set_input),
	DEVMETHOD(video_query_control, uvideo_hw_query_control),
	DEVMETHOD(video_get_control, uvideo_hw_get_control),
	DEVMETHOD(video_set_control, uvideo_hw_set_control),
	DEVMETHOD(video_start_stream, uvideo_hw_start_stream),
	DEVMETHOD(video_stop_stream, uvideo_hw_stop_stream),

	DEVMETHOD_END
};

static driver_t uvideo_driver = {
	.name = "uvideo",
	.methods = uvideo_methods,
	.size = sizeof(struct uvideo_softc),
};

DRIVER_MODULE(uvideo, uhub, uvideo_driver, NULL, NULL);
MODULE_DEPEND(uvideo, usb, 1, 1, 1);
MODULE_DEPEND(uvideo, video, 1, 1, 1);
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
	[3] = {
		.type = UE_ISOCHRONOUS,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = 0,
		.frames = UVIDEO_NFRAMES_MAX,
		.flags = {.short_xfer_ok = 1, .short_frames_ok = 1,},
		.callback = &uvideo_isoc_callback,
	},
	[4] = {
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

/* ---------------------------------------------------------------- */
/*  Probe / Attach / Detach                                         */
/* ---------------------------------------------------------------- */

static int
uvideo_probe(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	const struct uvideo_quirk *quirk;

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);

	/* Check quirks table first */
	quirk = uvideo_lookup_quirk(uaa);
	if (quirk != NULL) {
		if (quirk->flags & UVIDEO_FLAG_REATTACH)
			return (BUS_PROBE_DEFAULT);
		if (quirk->flags & UVIDEO_FLAG_VENDOR_CLASS &&
		    uaa->info.bInterfaceClass == UICLASS_VENDOR &&
		    uaa->info.bInterfaceSubClass == UISUBCLASS_VIDEOCONTROL)
			return (BUS_PROBE_DEFAULT);
	}

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
	usb_error_t error;
	int first_iface, nifaces;
	int i;

	sc->sc_dev = dev;
	sc->sc_udev = uaa->device;
	sc->sc_iface_index = uaa->info.bIfaceIndex;

	device_set_usb_desc(dev);
	mtx_init(&sc->sc_mtx, "uvideo", NULL, MTX_DEF);

	/* Look up quirks for this device */
	sc->sc_quirk = uvideo_lookup_quirk(uaa);

	if (sc->sc_quirk != NULL &&
	    sc->sc_quirk->flags & UVIDEO_FLAG_NOATTACH) {
		device_printf(dev, "device not supported\n");
		goto detach;
	}

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

	/* Map stream header decode function based on quirks */
	if (sc->sc_quirk != NULL &&
	    sc->sc_quirk->flags & UVIDEO_FLAG_ISIGHT_STREAM_HEADER) {
		sc->sc_decode_stream_header =
		    uvideo_vs_decode_stream_header_isight;
	} else {
		sc->sc_decode_stream_header = uvideo_vs_decode_stream_header;
	}

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

	i = video_register(dev, &sc->sc_vd);
	if (i != 0) {
		device_printf(dev, "failed to register video device\n");
		goto detach;
	}

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

	if (sc->sc_vd != NULL)
		video_unregister(sc->sc_vd);

	if (sc->sc_streaming) {
		mtx_lock(&sc->sc_mtx);
		sc->sc_streaming = 0;
		mtx_unlock(&sc->sc_mtx);
		uvideo_vs_close(sc);
	}


	uvideo_vs_free_frame(sc);

	/* Unsetup USB transfers */
	usbd_transfer_unsetup(sc->sc_xfer, UVIDEO_N_XFER);

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
	sc->sc_fmtgrp_idx = 0;
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
	uint64_t fbuf_size;

	fmtidx = sc->sc_fmtgrp_idx;
	if (fmtidx >= UVIDEO_MAX_FORMAT ||
	    sc->sc_fmtgrp[fmtidx].format == NULL) {
		device_printf(sc->sc_dev,
		    "frame descriptor without format!\n");
		return (USB_ERR_INVAL);
	}
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
		fbuf_size = (uint64_t)UGETW(fd->u.uc.wWidth) *
		    UGETW(fd->u.uc.wHeight) *
		    sc->sc_fmtgrp[fmtidx].format->u.uc.bBitsPerPixel / NBBY;
	} else
		fbuf_size = UGETDW(fd->u.uc.dwMaxVideoFrameBufferSize);

	if (fbuf_size > sc->sc_max_fbuf_size)
		sc->sc_max_fbuf_size = (uint32_t)fbuf_size;

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
	uint64_t fbuf_size;
	uint32_t frame_ival, next_frame_ival;

	fmtidx = sc->sc_fmtgrp_idx;
	if (fmtidx >= UVIDEO_MAX_FORMAT ||
	    sc->sc_fmtgrp[fmtidx].format == NULL) {
		device_printf(sc->sc_dev,
		    "frame descriptor without format!\n");
		return (USB_ERR_INVAL);
	}
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
		if (length < (int)sizeof(uDWord))
			break;
		next_frame_ival = UGETDW(p);
		if (next_frame_ival > frame_ival)
			frame_ival = next_frame_ival;
		p += sizeof(uDWord);
		length -= sizeof(uDWord);
	}

	fbuf_size = (uint64_t)UGETDW(UVIDEO_FRAME_FIELD(fd, dwMaxBitRate)) *
	    frame_ival;
	fbuf_size /= 8 * 10000000;

	if (fbuf_size > sc->sc_max_fbuf_size)
		sc->sc_max_fbuf_size = (uint32_t)fbuf_size;

	if (++sc->sc_fmtgrp[fmtidx].frame_num ==
	    sc->sc_fmtgrp[fmtidx].format->bNumFrameDescriptors)
		sc->sc_fmtgrp_idx++;

	return (USB_ERR_NORMAL_COMPLETION);
}

/*
 * Smallest alt satisfying dwMaxPayloadTransferSize.  A larger one loses the
 * payload framing.  XXX high speed only: bMaxBurst from the SuperSpeed
 * endpoint companion descriptor is not accounted for.
 */
static void
uvideo_vs_select_alt(struct uvideo_softc *sc, uint32_t payload)
{
	struct uvideo_vs_iface *vs = sc->sc_vs_cur;
	struct usb_config_descriptor *cdesc;
	struct usb_descriptor *desc;
	struct usb_interface_descriptor *id;
	struct usb_endpoint_descriptor *ed;
	uint32_t psize, best_psize;
	int best_alt;

	if (vs->bulk_endpoint || payload == 0)
		return;

	cdesc = usbd_get_config_descriptor(sc->sc_udev);
	if (cdesc == NULL)
		return;

	best_alt = -1;
	best_psize = 0;

	desc = NULL;
	id = NULL;
	while ((desc = usb_desc_foreach(cdesc, desc)) != NULL) {
		if (desc->bDescriptorType == UDESC_INTERFACE) {
			id = (struct usb_interface_descriptor *)desc;
			continue;
		}
		if (desc->bDescriptorType != UDESC_ENDPOINT || id == NULL)
			continue;
		if (id->bInterfaceNumber != vs->iface_index)
			continue;

		ed = (struct usb_endpoint_descriptor *)desc;
		if (UE_GET_DIR(ed->bEndpointAddress) != UE_DIR_IN ||
		    UE_GET_XFERTYPE(ed->bmAttributes) != UE_ISOCHRONOUS)
			continue;

		psize = UGETW(ed->wMaxPacketSize);
		psize = UE_GET_SIZE(psize) * (1 + UE_GET_TRANS(psize));
		if (psize < payload)
			continue;
		if (best_alt < 0 || psize < best_psize) {
			best_alt = id->bAlternateSetting;
			best_psize = psize;
		}
	}

	if (best_alt >= 0) {
		DPRINTFN(1, "alt %d psize %u for payload %u (was alt %d "
		    "psize %u)\n", best_alt, best_psize, payload, vs->curalt,
		    vs->psize);
		vs->curalt = best_alt;
		vs->psize = best_psize;
	}
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
				if (step == 0)
					step = 1;
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

	/*
	 * Some devices (e.g. Elgato Cam Link 4K, Elgato Game Capture HD60)
	 * return an invalid bmHint response which contains the bFormatIndex
	 * in the second byte. Fix it up.
	 */
	if (sc->sc_quirk != NULL &&
	    sc->sc_quirk->flags & UVIDEO_FLAG_FORMAT_INDEX_IN_BMHINT) {
		struct usb_video_probe_commit *pc =
		    (struct usb_video_probe_commit *)probe_data;
		if (UGETW(pc->bmHint) > 255) {
			pc->bFormatIndex = UGETW(pc->bmHint) >> 8;
			USETW(pc->bmHint, 1);
		}
	}

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

	if (sc->sc_max_fbuf_size < fb->buf_size) {
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

	uvideo_vs_select_alt(sc,
	    UGETDW(sc->sc_desc_probe.dwMaxPayloadTransferSize));

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

	/* Copy payload directly from USB DMA into frame buffer */
	payload_len = len - hdrlen;
	if (payload_len > fb->buf_size - fb->offset) {
		DPRINTFN(1, "frame too large, marked as error\n");
		payload_len = fb->buf_size - fb->offset;
		fb->error = 1;
	}
	if (payload_len > 0) {
		usbd_copy_out(pc, offset + hdrlen,
		    fb->buf + fb->offset, payload_len);
		fb->offset += payload_len;
	}

	if (flags & UVIDEO_SH_FLAG_EOF) {
		DPRINTFN(2, "EOF (frame size=%d bytes)\n", fb->offset);

		if (fb->offset < fb->buf_size &&
		    !(fb->fmt_flags & V4L2_FMT_FLAG_COMPRESSED)) {
			DPRINTFN(1, "frame too small, marked as error\n");
			fb->error = 1;
		}

		if (!fb->error) {
			uvideo_frame_done(sc);
		} else {
			struct video_buf *vb;

			vb = video_buf_acquire(sc->sc_vd);
			if (vb != NULL)
				video_buf_error(vb);
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
	usb_frcount_t maxframes;
	int nframes, i, offset, len;

	maxframes = usbd_xfer_max_frames(xfer);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		usbd_xfer_status(xfer, NULL, NULL, NULL, &nframes);
		pc = usbd_xfer_get_frame(xfer, 0);
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
		nframes = sc->sc_nframes;
		if (nframes > (int)maxframes)
			nframes = maxframes;
		if (nframes < 1)
			nframes = 1;
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

	/* Save sample data */
	sample_len = frame_size - sh->bLength;
	if (sample_len > fb->buf_size - fb->offset) {
		DPRINTFN(1, "frame too large, marked as error\n");
		sample_len = fb->buf_size - fb->offset;
		fb->error = 1;
	}
	if (sample_len > 0) {
		bcopy(frame + sh->bLength, fb->buf + fb->offset, sample_len);
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

		if (!fb->error) {
			uvideo_frame_done(sc);
		} else {
			struct video_buf *vb;

			vb = video_buf_acquire(sc->sc_vd);
			if (vb != NULL)
				video_buf_error(vb);
		}

		fb->sample = 0;
		fb->fid = 0;
		fb->error = 0;
		fb->mmap_q_full = 0;
	}
}

static void
uvideo_vs_decode_stream_header_isight(struct uvideo_softc *sc,
    uint8_t *frame, int frame_size)
{
	struct uvideo_frame_buffer *fb = &sc->sc_frame_buffer;
	int sample_len, header = 0;
	uint8_t magic[] = { 0x11, 0x22, 0x33, 0x44, 0xde, 0xad, 0xbe,
	    0xef, 0xde, 0xad, 0xfa, 0xce };

	if (frame_size > 13 && !memcmp(&frame[2], magic, 12))
		header = 1;
	if (frame_size > 14 && !memcmp(&frame[3], magic, 12))
		header = 1;

	if (header && fb->fid == 0) {
		fb->fid = 1;
		return;
	}

	if (header) {
		if (fb->offset > 0)
			uvideo_frame_done(sc);
		fb->offset = 0;
	} else {
		sample_len = frame_size;
		if ((fb->offset + sample_len) < fb->buf_size) {
			bcopy(frame, fb->buf + fb->offset, sample_len);
			fb->offset += sample_len;
		}
	}
}

static void
uvideo_frame_done(struct uvideo_softc *sc)
{
	struct video_buf *vb;

	vb = video_buf_acquire(sc->sc_vd);
	if (vb == NULL)
		return;

	if (video_buf_write(vb, 0, sc->sc_frame_buffer.buf,
	    sc->sc_frame_buffer.offset) != 0) {
		video_buf_error(vb);
		return;
	}
	video_buf_done(vb, sc->sc_frame_buffer.offset, sc->sc_sequence++);
}

static int
uvideo_hw_querycap(device_t dev, struct video_caps *caps)
{
	struct uvideo_softc *sc = device_get_softc(dev);

	bzero(caps, sizeof(*caps));
	strlcpy(caps->driver, "uvideo", sizeof(caps->driver));
	strlcpy(caps->card, usb_get_product(sc->sc_udev),
	    sizeof(caps->card));
	snprintf(caps->bus_info, sizeof(caps->bus_info), "usb-%s",
	    device_get_nameunit(sc->sc_dev));
	caps->version = (5 << 16) | (0 << 8) | 0;	/* 5.0.0 */
	caps->capabilities = VIDEO_CAP_CAPTURE |
	    VIDEO_CAP_READWRITE | VIDEO_CAP_STREAMING;

	return (0);
}

static int
uvideo_hw_enum_format(device_t dev, uint32_t index, struct video_format *fmt)
{
	struct uvideo_softc *sc = device_get_softc(dev);
	struct uvideo_format_group *fmtgrp;
	struct usb_video_frame_desc *frame;

	if (index >= (uint32_t)sc->sc_fmtgrp_num)
		return (EINVAL);

	fmtgrp = &sc->sc_fmtgrp[index];
	frame = fmtgrp->frame_cur;

	bzero(fmt, sizeof(*fmt));
	fmt->pixelformat = fmtgrp->pixelformat;
	if (frame != NULL) {
		fmt->width = UGETW(UVIDEO_FRAME_FIELD(frame, wWidth));
		fmt->height = UGETW(UVIDEO_FRAME_FIELD(frame, wHeight));
	}
	fmt->sizeimage = UGETDW(sc->sc_desc_probe.dwMaxVideoFrameSize);
	fmt->field = V4L2_FIELD_NONE;

	if (fmtgrp->has_colorformat) {
		fmt->colorspace = fmtgrp->colorspace;
		fmt->xfer_func = fmtgrp->xfer_func;
		fmt->ycbcr_enc = fmtgrp->ycbcr_enc;
	}

	switch (fmtgrp->format->bDescriptorSubtype) {
	case UDESCSUB_VS_FORMAT_MJPEG:
		fmt->flags = V4L2_FMT_FLAG_COMPRESSED;
		strlcpy(fmt->description, "Motion-JPEG",
		    sizeof(fmt->description));
		break;
	case UDESCSUB_VS_FORMAT_H264:
	case UDESCSUB_VS_FORMAT_H264_SIMULCAST:
		fmt->flags = V4L2_FMT_FLAG_COMPRESSED;
		strlcpy(fmt->description, "H.264",
		    sizeof(fmt->description));
		break;
	case UDESCSUB_VS_FORMAT_FRAME_BASED:
		if (fmtgrp->format->u.fb.bVariableSize)
			fmt->flags = V4L2_FMT_FLAG_COMPRESSED;
		break;
	default:
		strlcpy(fmt->description, "YUV",
		    sizeof(fmt->description));
		break;
	}

	return (0);
}

static int
uvideo_hw_get_format(device_t dev, struct video_format *fmt)
{
	struct uvideo_softc *sc = device_get_softc(dev);
	struct usb_video_frame_desc *frame;

	if (sc->sc_fmtgrp_cur == NULL)
		return (EIO);

	frame = sc->sc_fmtgrp_cur->frame_cur;

	bzero(fmt, sizeof(*fmt));
	fmt->pixelformat = sc->sc_fmtgrp_cur->pixelformat;
	fmt->field = V4L2_FIELD_NONE;
	if (frame != NULL) {
		fmt->width = UGETW(UVIDEO_FRAME_FIELD(frame, wWidth));
		fmt->height = UGETW(UVIDEO_FRAME_FIELD(frame, wHeight));
	}
	fmt->sizeimage = UGETDW(sc->sc_desc_probe.dwMaxVideoFrameSize);

	if (sc->sc_fmtgrp_cur->has_colorformat) {
		fmt->colorspace = sc->sc_fmtgrp_cur->colorspace;
		fmt->xfer_func = sc->sc_fmtgrp_cur->xfer_func;
		fmt->ycbcr_enc = sc->sc_fmtgrp_cur->ycbcr_enc;
	}

	return (0);
}

static int
uvideo_hw_try_format(device_t dev, struct video_format *fmt)
{
	struct uvideo_softc *sc = device_get_softc(dev);
	struct uvideo_res r;
	int found, i;

	for (found = 0, i = 0; i < sc->sc_fmtgrp_num; i++) {
		if (fmt->pixelformat == sc->sc_fmtgrp[i].pixelformat) {
			found = 1;
			break;
		}
	}
	if (found == 0)
		return (EINVAL);

	if (sc->sc_fmtgrp[i].frame_num == 0)
		return (EINVAL);

	uvideo_find_res(sc, i, fmt->width, fmt->height, &r);

	fmt->width = r.width;
	fmt->height = r.height;
	fmt->field = V4L2_FIELD_NONE;
	fmt->sizeimage = UGETDW(sc->sc_desc_probe.dwMaxVideoFrameSize);

	if (sc->sc_fmtgrp[i].has_colorformat) {
		fmt->colorspace = sc->sc_fmtgrp[i].colorspace;
		fmt->xfer_func = sc->sc_fmtgrp[i].xfer_func;
		fmt->ycbcr_enc = sc->sc_fmtgrp[i].ycbcr_enc;
	}

	return (0);
}

static int
uvideo_hw_set_format(device_t dev, const struct video_format *fmt)
{
	struct uvideo_softc *sc = device_get_softc(dev);
	struct uvideo_format_group *fmtgrp_save;
	struct usb_video_frame_desc *frame_save;
	struct uvideo_res r;
	int found, i;
	usb_error_t error;

	for (found = 0, i = 0; i < sc->sc_fmtgrp_num; i++) {
		if (fmt->pixelformat == sc->sc_fmtgrp[i].pixelformat) {
			found = 1;
			break;
		}
	}
	if (found == 0)
		return (EINVAL);

	if (sc->sc_fmtgrp[i].frame_num == 0)
		return (EINVAL);

	uvideo_find_res(sc, i, fmt->width, fmt->height, &r);

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

	return (0);
}

static int
uvideo_hw_enum_framesizes(device_t dev, struct video_frmsizeenum *fse)
{
	struct uvideo_softc *sc = device_get_softc(dev);
	int idx, found = 0;
	struct usb_video_frame_desc *frame;

	for (idx = 0; idx < sc->sc_fmtgrp_num; idx++) {
		if (sc->sc_fmtgrp[idx].pixelformat == fse->pixelformat) {
			found = 1;
			break;
		}
	}
	if (found == 0)
		return (EINVAL);

	if (fse->index >= (uint32_t)sc->sc_fmtgrp[idx].frame_num)
		return (EINVAL);

	frame = sc->sc_fmtgrp[idx].frame[fse->index];
	fse->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fse->discrete.width = UGETW(UVIDEO_FRAME_FIELD(frame, wWidth));
	fse->discrete.height = UGETW(UVIDEO_FRAME_FIELD(frame, wHeight));

	return (0);
}

static int
uvideo_hw_enum_frameintervals(device_t dev, struct video_frmivalenum *fie)
{
	struct uvideo_softc *sc = device_get_softc(dev);
	int idx, ival_bytes;
	struct uvideo_format_group *fmtgrp = NULL;
	struct usb_video_frame_desc *frame = NULL;
	uint8_t *p;

	for (idx = 0; idx < sc->sc_fmtgrp_num; idx++) {
		if (sc->sc_fmtgrp[idx].pixelformat == fie->pixelformat) {
			fmtgrp = &sc->sc_fmtgrp[idx];
			break;
		}
	}
	if (fmtgrp == NULL)
		return (EINVAL);

	for (idx = 0; idx < fmtgrp->frame_num; idx++) {
		if (UGETW(UVIDEO_FRAME_FIELD(fmtgrp->frame[idx], wWidth))
		    == fie->width &&
		    UGETW(UVIDEO_FRAME_FIELD(fmtgrp->frame[idx], wHeight))
		    == fie->height) {
			frame = fmtgrp->frame[idx];
			break;
		}
	}
	if (frame == NULL)
		return (EINVAL);

	p = (uint8_t *)frame + UVIDEO_FRAME_MIN_LEN(frame);
	ival_bytes = (int)frame->bLength - (int)UVIDEO_FRAME_MIN_LEN(frame);
	if (ival_bytes < 0)
		return (EINVAL);

	if (UVIDEO_FRAME_NUM_INTERVALS(frame) == 0) {
		if (fie->index != 0)
			return (EINVAL);
		if (ival_bytes < (int)(3 * sizeof(uDWord)))
			return (EINVAL);
		fie->type = V4L2_FRMIVAL_TYPE_STEPWISE;
		fie->stepwise.min.numerator = UGETDW(p);
		fie->stepwise.min.denominator = 10000000;
		p += sizeof(uDWord);
		fie->stepwise.max.numerator = UGETDW(p);
		fie->stepwise.max.denominator = 10000000;
		p += sizeof(uDWord);
		fie->stepwise.step.numerator = UGETDW(p);
		fie->stepwise.step.denominator = 10000000;
	} else {
		if (fie->index >= (uint32_t)UVIDEO_FRAME_NUM_INTERVALS(frame))
			return (EINVAL);
		if (ival_bytes < (int)((fie->index + 1) * sizeof(uDWord)))
			return (EINVAL);
		p += sizeof(uDWord) * fie->index;
		fie->type = V4L2_FRMIVAL_TYPE_DISCRETE;
		fie->discrete.numerator = UGETDW(p);
		fie->discrete.denominator = 10000000;
	}

	return (0);
}

static int
uvideo_hw_get_parm(device_t dev, struct video_fract *fract)
{
	struct uvideo_softc *sc = device_get_softc(dev);

	fract->numerator = UGETDW(sc->sc_desc_probe.dwFrameInterval);
	fract->denominator = 10000000;

	return (0);
}

static int
uvideo_hw_set_parm(device_t dev, struct video_fract *fract)
{
	struct uvideo_softc *sc = device_get_softc(dev);
	usb_error_t error;

	if (fract->numerator == 0 || fract->denominator == 0)
		sc->sc_frame_rate = 0;
	else
		sc->sc_frame_rate = fract->denominator / fract->numerator;

	/* Renegotiate if needed */
	if (sc->sc_negotiated_flag) {
		error = uvideo_vs_negotiation(sc, 1);
		if (error != USB_ERR_NORMAL_COMPLETION)
			return (EINVAL);
	}

	fract->numerator = UGETDW(sc->sc_desc_probe.dwFrameInterval);
	fract->denominator = 10000000;

	return (0);
}

static int
uvideo_hw_enum_input(device_t dev, uint32_t index, struct video_input *inp)
{

	if (index != 0)
		return (EINVAL);

	bzero(inp, sizeof(*inp));
	inp->index = 0;
	strlcpy(inp->name, "Camera Terminal", sizeof(inp->name));
	inp->type = VIDEO_INPUT_TYPE_CAMERA;

	return (0);
}

static int
uvideo_hw_get_input(device_t dev, uint32_t *index)
{

	*index = 0;
	return (0);
}

static int
uvideo_hw_set_input(device_t dev, uint32_t index)
{

	if (index != 0)
		return (EINVAL);
	return (0);
}

static int
uvideo_hw_query_control(device_t dev, struct video_control_desc *qc)
{
	struct uvideo_softc *sc = device_get_softc(dev);
	int i, ret = 0;
	usb_error_t error;
	uint8_t *ctrl_data;
	uint16_t ctrl_len;
	uint8_t unit_id;

	i = uvideo_find_ctrl(sc, qc->id);
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

	qc->type = uvideo_ctrls[i].type;
	strlcpy(qc->name, uvideo_ctrls[i].name, sizeof(qc->name));

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
		qc->minimum = uvideo_ctrls[i].sig ?
		    *(int8_t *)ctrl_data : *ctrl_data;
		break;
	case 2:
		qc->minimum = uvideo_ctrls[i].sig ?
		    (int16_t)UGETW(ctrl_data) : UGETW(ctrl_data);
		break;
	case 4:
		qc->minimum = uvideo_ctrls[i].sig ?
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
		qc->maximum = uvideo_ctrls[i].sig ?
		    *(int8_t *)ctrl_data : *ctrl_data;
		break;
	case 2:
		qc->maximum = uvideo_ctrls[i].sig ?
		    (int16_t)UGETW(ctrl_data) : UGETW(ctrl_data);
		break;
	case 4:
		qc->maximum = uvideo_ctrls[i].sig ?
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
		qc->step = uvideo_ctrls[i].sig ?
		    *(int8_t *)ctrl_data : *ctrl_data;
		break;
	case 2:
		qc->step = uvideo_ctrls[i].sig ?
		    (int16_t)UGETW(ctrl_data) : UGETW(ctrl_data);
		break;
	case 4:
		qc->step = uvideo_ctrls[i].sig ?
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
		qc->default_value = uvideo_ctrls[i].sig ?
		    *(int8_t *)ctrl_data : *ctrl_data;
		break;
	case 2:
		qc->default_value = uvideo_ctrls[i].sig ?
		    (int16_t)UGETW(ctrl_data) : UGETW(ctrl_data);
		break;
	case 4:
		qc->default_value = uvideo_ctrls[i].sig ?
		    (int32_t)UGETDW(ctrl_data) : UGETDW(ctrl_data);
		break;
	}

	qc->flags = 0;

out:
	free(ctrl_data, M_USBDEV);
	return (ret);
}

static int
uvideo_hw_get_control(device_t dev, struct video_control *ctrl)
{
	struct uvideo_softc *sc = device_get_softc(dev);
	int i, ret = 0;
	usb_error_t error;
	uint8_t *ctrl_data;
	uint16_t ctrl_len;
	uint8_t unit_id;

	i = uvideo_find_ctrl(sc, ctrl->id);
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
		ctrl->value = uvideo_ctrls[i].sig ?
		    *(int8_t *)ctrl_data : *ctrl_data;
		break;
	case 2:
		ctrl->value = uvideo_ctrls[i].sig ?
		    (int16_t)UGETW(ctrl_data) : UGETW(ctrl_data);
		break;
	case 4:
		ctrl->value = uvideo_ctrls[i].sig ?
		    (int32_t)UGETDW(ctrl_data) : UGETDW(ctrl_data);
		break;
	}

out:
	free(ctrl_data, M_USBDEV);
	return (ret);
}

static int
uvideo_hw_set_control(device_t dev, const struct video_control *ctrl)
{
	struct uvideo_softc *sc = device_get_softc(dev);
	int i, ret = 0;
	usb_error_t error;
	uint8_t *ctrl_data;
	uint16_t ctrl_len;
	uint8_t unit_id;

	i = uvideo_find_ctrl(sc, ctrl->id);
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
			*(int8_t *)ctrl_data = ctrl->value;
		else
			*ctrl_data = ctrl->value;
		break;
	case 2:
		USETW(ctrl_data, ctrl->value);
		break;
	case 4:
		USETDW(ctrl_data, ctrl->value);
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

static int
uvideo_hw_start_stream(device_t dev)
{
	struct uvideo_softc *sc = device_get_softc(dev);
	usb_error_t error;

	if (sc->sc_vs_cur == NULL)
		return (EIO);

	sc->sc_sequence = 0;

	error = uvideo_vs_open(sc);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);

	error = uvideo_vs_alloc_frame(sc);
	if (error != USB_ERR_NORMAL_COMPLETION) {
		uvideo_vs_close(sc);
		return (EIO);
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

	return (0);
}

static void
uvideo_hw_stop_stream(device_t dev)
{
	struct uvideo_softc *sc = device_get_softc(dev);

	mtx_lock(&sc->sc_mtx);
	sc->sc_streaming = 0;
	mtx_unlock(&sc->sc_mtx);

	uvideo_vs_close(sc);
	uvideo_vs_free_frame(sc);
}
