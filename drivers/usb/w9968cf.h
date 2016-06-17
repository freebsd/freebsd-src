/***************************************************************************
 * Video4Linux driver for W996[87]CF JPEG USB Dual Mode Camera Chip.       *
 *                                                                         *
 * Copyright (C) 2002 2003 by Luca Risolia <luca.risolia@studio.unibo.it>  *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 2 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with this program; if not, write to the Free Software             *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.               *
 ***************************************************************************/

#ifndef _W9968CF_H_
#define _W9968CF_H_

#include <linux/videodev.h>
#include <linux/usb.h>
#include <linux/i2c.h>
#ifdef CONFIG_VIDEO_PROC_FS
#	include <linux/proc_fs.h>
#endif
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/config.h>
#include <linux/param.h>
#include <asm/semaphore.h>
#include <asm/types.h>

#include "w9968cf_externaldef.h"


/****************************************************************************
 * Default values                                                           *
 ****************************************************************************/

#define W9968CF_VPPMOD_LOAD     1  /* automatic 'w9968cf-vpp' module loading */

/* Comment/uncomment the following line to enable/disable debugging messages */
#define W9968CF_DEBUG

/* These have effect only if W9968CF_DEBUG is defined */
#define W9968CF_DEBUG_LEVEL    2 /* from 0 to 6. 0 for no debug informations */
#define W9968CF_SPECIFIC_DEBUG 0 /* 0 or 1 */

#define W9968CF_MAX_DEVICES    32
#define W9968CF_SIMCAMS        W9968CF_MAX_DEVICES /* simultaneous cameras */

#define W9968CF_MAX_BUFFERS   32
#define W9968CF_BUFFERS       2 /* n. of frame buffers from 2 to MAX_BUFFERS */

/* Maximum data payload sizes in bytes for alternate settings */
static const u16 wMaxPacketSize[] = {1023, 959, 895, 831, 767, 703, 639, 575,
                                      511, 447, 383, 319, 255, 191, 127,  63};
#define W9968CF_PACKET_SIZE      1023 /* according to wMaxPacketSizes[] */
#define W9968CF_MIN_PACKET_SIZE  63 /* minimum value */
#define W9968CF_ISO_PACKETS      5 /* n.of packets for isochronous transfers */
#define W9968CF_USB_CTRL_TIMEOUT HZ /* timeout for usb control commands */
#define W9968CF_URBS             2 /* n. of scheduled URBs for ISO transfer */

#define W9968CF_I2C_BUS_DELAY    4 /* delay in us for I2C bit r/w operations */
#define W9968CF_I2C_RW_RETRIES   15 /* number of max I2C r/w retries */

/* Available video formats */
struct w9968cf_format {
	const u16 palette;
	const u16 depth;
	const u8 compression;
};

static const struct w9968cf_format w9968cf_formatlist[] = {
	{ VIDEO_PALETTE_UYVY,    16, 0 }, /* original video */
	{ VIDEO_PALETTE_YUV422P, 16, 1 }, /* with JPEG compression */
	{ VIDEO_PALETTE_YUV420P, 12, 1 }, /* with JPEG compression */
	{ VIDEO_PALETTE_YUV420,  12, 1 }, /* same as YUV420P */
	{ VIDEO_PALETTE_YUYV,    16, 0 }, /* software conversion */
	{ VIDEO_PALETTE_YUV422,  16, 0 }, /* software conversion */
	{ VIDEO_PALETTE_GREY,     8, 0 }, /* software conversion */
	{ VIDEO_PALETTE_RGB555,  16, 0 }, /* software conversion */
	{ VIDEO_PALETTE_RGB565,  16, 0 }, /* software conversion */
	{ VIDEO_PALETTE_RGB24,   24, 0 }, /* software conversion */
	{ VIDEO_PALETTE_RGB32,   32, 0 }, /* software conversion */
	{ 0,                      0, 0 }  /* 0 is a terminating entry */
};

#define W9968CF_DECOMPRESSION    2 /* decomp:0=disable,1=force,2=any formats */
#define W9968CF_PALETTE_DECOMP_OFF   VIDEO_PALETTE_UYVY    /* when decomp=0 */
#define W9968CF_PALETTE_DECOMP_FORCE VIDEO_PALETTE_YUV420P /* when decomp=1 */
#define W9968CF_PALETTE_DECOMP_ON    VIDEO_PALETTE_UYVY    /* when decomp=2 */

#define W9968CF_FORCE_RGB        0  /* read RGB instead of BGR, yes=1/no=0 */

#define W9968CF_MAX_WIDTH      800 /* should be >= 640 */
#define W9968CF_MAX_HEIGHT     600 /* should be >= 480 */
#define W9968CF_WIDTH          320 /* from 128 to 352, multiple of 16 */
#define W9968CF_HEIGHT         240 /* from  96 to 288, multiple of 16 */

#define W9968CF_CLAMPING       0 /* 0 disable, 1 enable video data clamping */
#define W9968CF_FILTER_TYPE    0 /* 0 disable  1 (1-2-1), 2 (2-3-6-3-2) */
#define W9968CF_DOUBLE_BUFFER  1 /* 0 disable, 1 enable double buffer */
#define W9968CF_LARGEVIEW      1 /* 0 disable, 1 enable */
#define W9968CF_UPSCALING      0 /* 0 disable, 1 enable */

#define W9968CF_MONOCHROME     0 /* 0 not monochrome, 1 monochrome sensor */
#define W9968CF_BRIGHTNESS     31000 /* from 0 to 65535 */
#define W9968CF_HUE            32768 /* from 0 to 65535 */
#define W9968CF_COLOUR         32768 /* from 0 to 65535 */
#define W9968CF_CONTRAST       50000 /* from 0 to 65535 */
#define W9968CF_WHITENESS      32768 /* from 0 to 65535 */

#define W9968CF_AUTOBRIGHT     0 /* 0 disable, 1 enable automatic brightness */
#define W9968CF_AUTOEXP        1 /* 0 disable, 1 enable automatic exposure */
#define W9968CF_LIGHTFREQ      50 /* light frequency. 50Hz (Europe) or 60Hz */
#define W9968CF_BANDINGFILTER  0 /* 0 disable, 1 enable banding filter */
#define W9968CF_BACKLIGHT      0 /* 0 or 1, 1=object is lit from behind */
#define W9968CF_MIRROR         0 /* 0 or 1 [don't] reverse image horizontally*/

#define W9968CF_CLOCKDIV         -1 /* -1 = automatic clock divisor */
#define W9968CF_DEF_CLOCKDIVISOR  0 /* default sensor clock divisor value */


/****************************************************************************
 * Globals                                                                  *
 ****************************************************************************/

#define W9968CF_MODULE_NAME     "V4L driver for W996[87]CF JPEG USB " \
                                "Dual Mode Camera Chip"
#define W9968CF_MODULE_VERSION  "v1.24-basic"
#define W9968CF_MODULE_AUTHOR   "(C) 2002 2003 Luca Risolia"
#define W9968CF_AUTHOR_EMAIL    "<luca.risolia@studio.unibo.it>"
#define W9968CF_MODULE_LICENSE  "GPL"

static u8 w9968cf_vppmod_present; /* status flag: yes=1, no=0 */

static const struct usb_device_id winbond_id_table[] = {
	{
		/* Creative Labs Video Blaster WebCam Go Plus */
		USB_DEVICE(0x041e, 0x4003),
		.driver_info = (unsigned long)"w9968cf",
	},
	{
		/* Generic W996[87]CF JPEG USB Dual Mode Camera */
		USB_DEVICE(0x1046, 0x9967),
		.driver_info = (unsigned long)"w9968cf",
	},
	{ } /* terminating entry */
};

MODULE_DEVICE_TABLE(usb, winbond_id_table);

/* W996[87]CF camera models, internal ids: */
enum w9968cf_model_id {
	W9968CF_MOD_GENERIC = 1, /* Generic W996[87]CF based device */
	W9968CF_MOD_CLVBWGP = 11,/*Creative Labs Video Blaster WebCam Go Plus*/
	W9968CF_MOD_ADPA5R = 21, /* Aroma Digi Pen ADG-5000 Refurbished */
	W9986CF_MOD_AU = 31,     /* AVerTV USB */
	W9968CF_MOD_CLVBWG = 34, /* Creative Labs Video Blaster WebCam Go */
	W9968CF_MOD_DLLDK = 37,  /* Die Lebon LDC-D35A Digital Kamera */
	W9968CF_MOD_EEEMC = 40,   /* Ezonics EZ-802 EZMega Cam */
	W9968CF_MOD_ODPVDMPC = 43,/* OPCOM Digi Pen VGA Dual Mode Pen Camera */
};

enum w9968cf_frame_status {
	F_READY,            /* finished grabbing & ready to be read/synced */
	F_GRABBING,         /* in the process of being grabbed into */
	F_ERROR,            /* something bad happened while processing */
	F_UNUSED            /* unused (no VIDIOCMCAPTURE) */
};

struct w9968cf_frame_t {
	#define W9968CF_HW_BUF_SIZE 640*480*2 /* buf.size of original frames */
	void* buffer;
	u32 length;
	enum w9968cf_frame_status status;
	struct w9968cf_frame_t* next;
	u8 queued;
};

enum w9968cf_vpp_flag {
	VPP_NONE = 0x00,
	VPP_UPSCALE = 0x01,
	VPP_SWAP_YUV_BYTES = 0x02,
	VPP_DECOMPRESSION = 0x04,
	VPP_UYVY_TO_RGBX = 0x08,
};

struct list_head w9968cf_dev_list; /* head of V4L registered cameras list */
LIST_HEAD(w9968cf_dev_list);
struct semaphore w9968cf_devlist_sem; /* semaphore for list traversal */

/* Main device driver structure */
struct w9968cf_device {
	enum w9968cf_model_id id;   /* private device identifier */

	struct video_device* v4ldev; /* -> V4L structure */
	struct list_head v4llist;    /* entry of the list of V4L cameras */

	struct usb_device* usbdev;           /* -> main USB structure */
	struct urb* urb[W9968CF_URBS];       /* -> USB request block structs */
	void* transfer_buffer[W9968CF_URBS]; /* -> ISO transfer buffers */
	u16* control_buffer;                 /* -> buffer for control req.*/
	u16* data_buffer;                    /* -> data to send to the FSB */

	struct w9968cf_frame_t frame[W9968CF_MAX_BUFFERS];
	struct w9968cf_frame_t frame_tmp;  /* temporary frame */
	struct w9968cf_frame_t* frame_current; /* -> frame being grabbed */
	struct w9968cf_frame_t* requested_frame[W9968CF_MAX_BUFFERS];
	void* vpp_buffer; /*-> helper buf.for video post-processing routines */

	u8 max_buffers,   /* number of requested buffers */
	   force_palette, /* yes=1/no=0 */
	   force_rgb,     /* read RGB instead of BGR, yes=1, no=0 */
	   double_buffer, /* hardware double buffering yes=1/no=0 */
	   clamping,      /* video data clamping yes=1/no=0 */
	   filter_type,   /* 0=disabled, 1=3 tap, 2=5 tap filter */
	   capture,       /* 0=disabled, 1=enabled */
	   largeview,     /* 0=disabled, 1=enabled */
	   decompression, /* 0=disabled, 1=forced, 2=allowed */
	   upscaling;     /* software image scaling, 0=enabled, 1=disabled */

	struct video_picture picture; /* current picture settings */
	struct video_window window;   /* current window settings */

	u16 hw_depth,    /* depth (used by the chip) */
	    hw_palette,  /* palette (used by the chip) */
	    hw_width,    /* width (used by the chip) */
	    hw_height,   /* height (used by the chip) */
	    hs_polarity, /* 0=negative sync pulse, 1=positive sync pulse */
	    vs_polarity, /* 0=negative sync pulse, 1=positive sync pulse */
	    start_cropx, /* pixels from HS inactive edge to 1st cropped pixel*/
	    start_cropy; /* pixels from VS incative edge to 1st cropped pixel*/

	enum w9968cf_vpp_flag vpp_flag; /* post-processing routines in use */

	u8 nbuffers,      /* number of allocated frame buffers */
	   altsetting,    /* camera alternate setting */
	   disconnected,  /* flag: yes=1, no=0 */
	   misconfigured, /* flag: yes=1, no=0 */
	   users,         /* flag: number of users holding the device */
	   streaming;     /* flag: yes=1, no=0 */

	u8 sensor_initialized; /* flag: yes=1, no=0 */

	/* Determined by CMOS sensor type: */
	int sensor,       /* type of image sensor chip (CC_*) */
	    monochrome;   /* CMOS sensor is (probably) monochrome */
	u16 maxwidth,     /* maximum width supported by the CMOS sensor */
	    maxheight,    /* maximum height supported by the CMOS sensor */
	    minwidth,     /* minimum width supported by the CMOS sensor */
	    minheight;    /* minimum height supported by the CMOS sensor */
	u8  auto_brt,     /* auto brightness enabled flag */
	    auto_exp,     /* auto exposure enabled flag */
	    backlight,    /* backlight exposure algorithm flag */
	    mirror,       /* image is reversed horizontally */
	    lightfreq,    /* power (lighting) frequency */
	    bandfilt;     /* banding filter enabled flag */
	s8  clockdiv;     /* clock divisor */

	/* I2C interface to kernel */
	struct i2c_adapter i2c_adapter;
	struct i2c_client* sensor_client;

#ifdef CONFIG_VIDEO_PROC_FS
	/* /proc entries, relative to /proc/video/w9968cf/ */
	struct proc_dir_entry *proc_dev;   /* rw per-device entry */
#endif

	/* Locks */
	struct semaphore dev_sem,    /* for probe, disconnect,open and close */
	                 fileop_sem; /* for read and ioctl */
#ifdef CONFIG_VIDEO_PROC_FS
	struct semaphore procfs_sem; /* for /proc read/write calls */
#endif
	spinlock_t urb_lock,   /* for submit_urb() and unlink_urb() */
	           flist_lock; /* for requested frame list accesses */
	char command[16]; /* name of the program holding the device */
	wait_queue_head_t open, wait_queue;
};


/****************************************************************************
 * Macros for debugging                                                     *
 ****************************************************************************/

#undef DBG
#ifdef W9968CF_DEBUG
#	define DBG(level, fmt, args...) \
{ \
if ( ((specific_debug) && (debug == (level))) || \
     ((!specific_debug) && (debug >= (level))) ) { \
	if ((level) == 1) \
		err(fmt, ## args); \
	else if ((level) == 2 || (level) == 3) \
		info(fmt, ## args); \
	else if ((level) == 4) \
		warn(fmt, ## args); \
	else if ((level) >= 5) \
		info("[%s:%d] " fmt, \
		     __PRETTY_FUNCTION__, __LINE__ , ## args); \
} \
}
#else
	/* Not debugging: nothing */
#	define DBG(level, fmt, args...) do {;} while(0);
#endif

#undef PDBG
#undef PDBGG
#define PDBG(fmt, args...) info("[%s:%d] "fmt, \
	                        __PRETTY_FUNCTION__, __LINE__ , ## args);
#define PDBGG(fmt, args...) do {;} while(0); /* nothing: it's a placeholder */

#endif /* _W9968CF_H_ */
