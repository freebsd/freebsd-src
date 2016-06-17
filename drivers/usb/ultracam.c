/*
 * USB NB Camera driver
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/wrapper.h>
#include <linux/module.h>
#include <linux/init.h>

#include "usbvideo.h"

#define	ULTRACAM_VENDOR_ID	0x0461
#define	ULTRACAM_PRODUCT_ID	0x0813

#define MAX_CAMERAS		4	/* How many devices we allow to connect */

/*
 * This structure lives in uvd->user field.
 */
typedef struct {
	int initialized;	/* Had we already sent init sequence? */
	int camera_model;	/* What type of IBM camera we got? */
        int has_hdr;
} ultracam_t;
#define	ULTRACAM_T(uvd)	((ultracam_t *)((uvd)->user_data))

static struct usbvideo *cams = NULL;

static int debug = 0;

static int flags = 0; /* FLAGS_DISPLAY_HINTS | FLAGS_OVERLAY_STATS; */

static const int min_canvasWidth  = 8;
static const int min_canvasHeight = 4;

//static int lighting = 1; /* Medium */

#define SHARPNESS_MIN	0
#define SHARPNESS_MAX	6
//static int sharpness = 4; /* Low noise, good details */

#define FRAMERATE_MIN	0
#define FRAMERATE_MAX	6
static int framerate = -1;

/*
 * Here we define several initialization variables. They may
 * be used to automatically set color, hue, brightness and
 * contrast to desired values. This is particularly useful in
 * case of webcams (which have no controls and no on-screen
 * output) and also when a client V4L software is used that
 * does not have some of those controls. In any case it's
 * good to have startup values as options.
 *
 * These values are all in [0..255] range. This simplifies
 * operation. Note that actual values of V4L variables may
 * be scaled up (as much as << 8). User can see that only
 * on overlay output, however, or through a V4L client.
 */
static int init_brightness = 128;
static int init_contrast = 192;
static int init_color = 128;
static int init_hue = 128;
static int hue_correction = 128;

MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debug level: 0-9 (default=0)");
MODULE_PARM(flags, "i");
MODULE_PARM_DESC(flags,
		"Bitfield: 0=VIDIOCSYNC, "
		"1=B/W, "
		"2=show hints, "
		"3=show stats, "
		"4=test pattern, "
		"5=separate frames, "
		"6=clean frames");
MODULE_PARM(framerate, "i");
MODULE_PARM_DESC(framerate, "Framerate setting: 0=slowest, 6=fastest (default=2)");
MODULE_PARM(lighting, "i");
MODULE_PARM_DESC(lighting, "Photosensitivity: 0=bright, 1=medium (default), 2=low light");
MODULE_PARM(sharpness, "i");
MODULE_PARM_DESC(sharpness, "Model1 noise reduction: 0=smooth, 6=sharp (default=4)");

MODULE_PARM(init_brightness, "i");
MODULE_PARM_DESC(init_brightness, "Brightness preconfiguration: 0-255 (default=128)");
MODULE_PARM(init_contrast, "i");
MODULE_PARM_DESC(init_contrast, "Contrast preconfiguration: 0-255 (default=192)");
MODULE_PARM(init_color, "i");
MODULE_PARM_DESC(init_color, "Color preconfiguration: 0-255 (default=128)");
MODULE_PARM(init_hue, "i");
MODULE_PARM_DESC(init_hue, "Hue preconfiguration: 0-255 (default=128)");
MODULE_PARM(hue_correction, "i");
MODULE_PARM_DESC(hue_correction, "YUV colorspace regulation: 0-255 (default=128)");

/*
 * ultracam_ProcessIsocData()
 *
 * Generic routine to parse the ring queue data. It employs either
 * ultracam_find_header() or ultracam_parse_lines() to do most
 * of work.
 *
 * 02-Nov-2000 First (mostly dummy) version.
 * 06-Nov-2000 Rewrote to dump all data into frame.
 */
void ultracam_ProcessIsocData(struct uvd *uvd, struct usbvideo_frame *frame)
{
	int n;

	assert(uvd != NULL);
	assert(frame != NULL);

	/* Try to move data from queue into frame buffer */
	n = RingQueue_GetLength(&uvd->dp);
	if (n > 0) {
		int m;
		/* See how much spare we have left */
		m = uvd->max_frame_size - frame->seqRead_Length;
		if (n > m)
			n = m;
		/* Now move that much data into frame buffer */
		RingQueue_Dequeue(
			&uvd->dp,
			frame->data + frame->seqRead_Length,
			m);
		frame->seqRead_Length += m;
	}
	/* See if we filled the frame */
	if (frame->seqRead_Length >= uvd->max_frame_size) {
		frame->frameState = FrameState_Done;
		uvd->curframe = -1;
		uvd->stats.frame_num++;
	}
}

/*
 * ultracam_veio()
 *
 * History:
 * 1/27/00  Added check for dev == NULL; this happens if camera is unplugged.
 */
static int ultracam_veio(
	struct uvd *uvd,
	unsigned char req,
	unsigned short value,
	unsigned short index,
	int is_out)
{
	static const char proc[] = "ultracam_veio";
	unsigned char cp[8] /* = { 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef } */;
	int i;

	if (!CAMERA_IS_OPERATIONAL(uvd))
		return 0;

	if (!is_out) {
		i = usb_control_msg(
			uvd->dev,
			usb_rcvctrlpipe(uvd->dev, 0),
			req,
			USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			value,
			index,
			cp,
			sizeof(cp),
			HZ);
#if 1
		info("USB => %02x%02x%02x%02x%02x%02x%02x%02x "
		       "(req=$%02x val=$%04x ind=$%04x)",
		       cp[0],cp[1],cp[2],cp[3],cp[4],cp[5],cp[6],cp[7],
		       req, value, index);
#endif
	} else {
		i = usb_control_msg(
			uvd->dev,
			usb_sndctrlpipe(uvd->dev, 0),
			req,
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			value,
			index,
			NULL,
			0,
			HZ);
	}
	if (i < 0) {
		err("%s: ERROR=%d. Camera stopped; Reconnect or reload driver.",
		    proc, i);
		uvd->last_error = i;
	}
	return i;
}

/*
 * ultracam_calculate_fps()
 */
static int ultracam_calculate_fps(struct uvd *uvd)
{
	return 3 + framerate*4 + framerate/2;
}

/*
 * ultracam_adjust_contrast()
 */
static void ultracam_adjust_contrast(struct uvd *uvd)
{
}

/*
 * ultracam_change_lighting_conditions()
 */
static void ultracam_change_lighting_conditions(struct uvd *uvd)
{
}

/*
 * ultracam_set_sharpness()
 *
 * Cameras model 1 have internal smoothing feature. It is controlled by value in
 * range [0..6], where 0 is most smooth and 6 is most sharp (raw image, I guess).
 * Recommended value is 4. Cameras model 2 do not have this feature at all.
 */
static void ultracam_set_sharpness(struct uvd *uvd)
{
}

/*
 * ultracam_set_brightness()
 *
 * This procedure changes brightness of the picture.
 */
static void ultracam_set_brightness(struct uvd *uvd)
{
}

static void ultracam_set_hue(struct uvd *uvd)
{
}

/*
 * ultracam_adjust_picture()
 *
 * This procedure gets called from V4L interface to update picture settings.
 * Here we change brightness and contrast.
 */
static void ultracam_adjust_picture(struct uvd *uvd)
{
	ultracam_adjust_contrast(uvd);
	ultracam_set_brightness(uvd);
	ultracam_set_hue(uvd);
}

/*
 * ultracam_video_stop()
 *
 * This code tells camera to stop streaming. The interface remains
 * configured and bandwidth - claimed.
 */
static void ultracam_video_stop(struct uvd *uvd)
{
}

/*
 * ultracam_reinit_iso()
 *
 * This procedure sends couple of commands to the camera and then
 * resets the video pipe. This sequence was observed to reinit the
 * camera or, at least, to initiate ISO data stream.
 */
static void ultracam_reinit_iso(struct uvd *uvd, int do_stop)
{
}

static void ultracam_video_start(struct uvd *uvd)
{
	ultracam_change_lighting_conditions(uvd);
	ultracam_set_sharpness(uvd);
	ultracam_reinit_iso(uvd, 0);
}

static int ultracam_resetPipe(struct uvd *uvd)
{
	usb_clear_halt(uvd->dev, uvd->video_endp);
	return 0;
}

static int ultracam_alternateSetting(struct uvd *uvd, int setting)
{
	static const char proc[] = "ultracam_alternateSetting";
	int i;
	i = usb_set_interface(uvd->dev, uvd->iface, setting);
	if (i < 0) {
		err("%s: usb_set_interface error", proc);
		uvd->last_error = i;
		return -EBUSY;
	}
	return 0;
}

/*
 * Return negative code on failure, 0 on success.
 */
static int ultracam_setup_on_open(struct uvd *uvd)
{
	int setup_ok = 0; /* Success by default */
	/* Send init sequence only once, it's large! */
	if (!ULTRACAM_T(uvd)->initialized) {
		ultracam_alternateSetting(uvd, 0x04);
		ultracam_alternateSetting(uvd, 0x00);
		ultracam_veio(uvd, 0x02, 0x0004, 0x000b, 1);
		ultracam_veio(uvd, 0x02, 0x0001, 0x0005, 1);
		ultracam_veio(uvd, 0x02, 0x8000, 0x0000, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x0000, 1);
		ultracam_veio(uvd, 0x00, 0x00b0, 0x0001, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x0002, 1);
		ultracam_veio(uvd, 0x00, 0x000c, 0x0003, 1);
		ultracam_veio(uvd, 0x00, 0x000b, 0x0004, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x0005, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x0006, 1);
		ultracam_veio(uvd, 0x00, 0x0079, 0x0007, 1);
		ultracam_veio(uvd, 0x00, 0x003b, 0x0008, 1);
		ultracam_veio(uvd, 0x00, 0x0002, 0x000f, 1);
		ultracam_veio(uvd, 0x00, 0x0001, 0x0010, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x0011, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00bf, 1);
		ultracam_veio(uvd, 0x00, 0x0001, 0x00c0, 1);
		ultracam_veio(uvd, 0x00, 0x0010, 0x00cb, 1);
		ultracam_veio(uvd, 0x01, 0x00a4, 0x0001, 1);
		ultracam_veio(uvd, 0x01, 0x0010, 0x0002, 1);
		ultracam_veio(uvd, 0x01, 0x0066, 0x0007, 1);
		ultracam_veio(uvd, 0x01, 0x000b, 0x0008, 1);
		ultracam_veio(uvd, 0x01, 0x0034, 0x0009, 1);
		ultracam_veio(uvd, 0x01, 0x0000, 0x000a, 1);
		ultracam_veio(uvd, 0x01, 0x002e, 0x000b, 1);
		ultracam_veio(uvd, 0x01, 0x00d6, 0x000c, 1);
		ultracam_veio(uvd, 0x01, 0x00fc, 0x000d, 1);
		ultracam_veio(uvd, 0x01, 0x00f1, 0x000e, 1);
		ultracam_veio(uvd, 0x01, 0x00da, 0x000f, 1);
		ultracam_veio(uvd, 0x01, 0x0036, 0x0010, 1);
		ultracam_veio(uvd, 0x01, 0x000b, 0x0011, 1);
		ultracam_veio(uvd, 0x01, 0x0001, 0x0012, 1);
		ultracam_veio(uvd, 0x01, 0x0000, 0x0013, 1);
		ultracam_veio(uvd, 0x01, 0x0000, 0x0014, 1);
		ultracam_veio(uvd, 0x01, 0x0087, 0x0051, 1);
		ultracam_veio(uvd, 0x01, 0x0040, 0x0052, 1);
		ultracam_veio(uvd, 0x01, 0x0058, 0x0053, 1);
		ultracam_veio(uvd, 0x01, 0x0040, 0x0054, 1);
		ultracam_veio(uvd, 0x01, 0x0000, 0x0040, 1);
		ultracam_veio(uvd, 0x01, 0x0010, 0x0041, 1);
		ultracam_veio(uvd, 0x01, 0x0020, 0x0042, 1);
		ultracam_veio(uvd, 0x01, 0x0030, 0x0043, 1);
		ultracam_veio(uvd, 0x01, 0x0040, 0x0044, 1);
		ultracam_veio(uvd, 0x01, 0x0050, 0x0045, 1);
		ultracam_veio(uvd, 0x01, 0x0060, 0x0046, 1);
		ultracam_veio(uvd, 0x01, 0x0070, 0x0047, 1);
		ultracam_veio(uvd, 0x01, 0x0080, 0x0048, 1);
		ultracam_veio(uvd, 0x01, 0x0090, 0x0049, 1);
		ultracam_veio(uvd, 0x01, 0x00a0, 0x004a, 1);
		ultracam_veio(uvd, 0x01, 0x00b0, 0x004b, 1);
		ultracam_veio(uvd, 0x01, 0x00c0, 0x004c, 1);
		ultracam_veio(uvd, 0x01, 0x00d0, 0x004d, 1);
		ultracam_veio(uvd, 0x01, 0x00e0, 0x004e, 1);
		ultracam_veio(uvd, 0x01, 0x00f0, 0x004f, 1);
		ultracam_veio(uvd, 0x01, 0x00ff, 0x0050, 1);
		ultracam_veio(uvd, 0x01, 0x0000, 0x0056, 1);
		ultracam_veio(uvd, 0x00, 0x0080, 0x00c1, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c2, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00ca, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0080, 0x00c1, 1);
		ultracam_veio(uvd, 0x00, 0x0004, 0x00c2, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00ca, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0002, 0x00c1, 1);
		ultracam_veio(uvd, 0x00, 0x0020, 0x00c2, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00ca, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c3, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c4, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c5, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c6, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c7, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c8, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c3, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c4, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c5, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c6, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c7, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c8, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0040, 0x00c1, 1);
		ultracam_veio(uvd, 0x00, 0x0017, 0x00c2, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00ca, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c3, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c4, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c5, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c6, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c7, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c8, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c3, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c4, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c5, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c6, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c7, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c8, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c9, 1);
		ultracam_veio(uvd, 0x00, 0x00c0, 0x00c1, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00c2, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00ca, 1);
		ultracam_veio(uvd, 0x02, 0xc040, 0x0001, 1);
		ultracam_veio(uvd, 0x01, 0x0000, 0x0008, 0);
		ultracam_veio(uvd, 0x01, 0x0000, 0x0009, 0);
		ultracam_veio(uvd, 0x01, 0x0000, 0x000a, 0);
		ultracam_veio(uvd, 0x01, 0x0000, 0x000b, 0);
		ultracam_veio(uvd, 0x01, 0x0000, 0x000c, 0);
		ultracam_veio(uvd, 0x01, 0x0000, 0x000d, 0);
		ultracam_veio(uvd, 0x01, 0x0000, 0x000e, 0);
		ultracam_veio(uvd, 0x01, 0x0000, 0x000f, 0);
		ultracam_veio(uvd, 0x01, 0x0000, 0x0010, 0);
		ultracam_veio(uvd, 0x01, 0x000b, 0x0008, 1);
		ultracam_veio(uvd, 0x01, 0x0034, 0x0009, 1);
		ultracam_veio(uvd, 0x01, 0x0000, 0x000a, 1);
		ultracam_veio(uvd, 0x01, 0x002e, 0x000b, 1);
		ultracam_veio(uvd, 0x01, 0x00d6, 0x000c, 1);
		ultracam_veio(uvd, 0x01, 0x00fc, 0x000d, 1);
		ultracam_veio(uvd, 0x01, 0x00f1, 0x000e, 1);
		ultracam_veio(uvd, 0x01, 0x00da, 0x000f, 1);
		ultracam_veio(uvd, 0x01, 0x0036, 0x0010, 1);
		ultracam_veio(uvd, 0x01, 0x0000, 0x0001, 0);
		ultracam_veio(uvd, 0x01, 0x0064, 0x0001, 1);
		ultracam_veio(uvd, 0x01, 0x0059, 0x0051, 1);
		ultracam_veio(uvd, 0x01, 0x003f, 0x0052, 1);
		ultracam_veio(uvd, 0x01, 0x0094, 0x0053, 1);
		ultracam_veio(uvd, 0x01, 0x00ff, 0x0011, 1);
		ultracam_veio(uvd, 0x01, 0x0003, 0x0012, 1);
		ultracam_veio(uvd, 0x01, 0x00f7, 0x0013, 1);
		ultracam_veio(uvd, 0x00, 0x0009, 0x0011, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x0001, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x0000, 1);
		ultracam_veio(uvd, 0x00, 0x0020, 0x00c1, 1);
		ultracam_veio(uvd, 0x00, 0x0010, 0x00c2, 1);
		ultracam_veio(uvd, 0x00, 0x0000, 0x00ca, 1);
		ultracam_alternateSetting(uvd, 0x04);
		ultracam_veio(uvd, 0x02, 0x0000, 0x0001, 1);
		ultracam_veio(uvd, 0x02, 0x0000, 0x0001, 1);
		ultracam_veio(uvd, 0x02, 0x0000, 0x0006, 1);
		ultracam_veio(uvd, 0x02, 0x9000, 0x0007, 1);
		ultracam_veio(uvd, 0x02, 0x0042, 0x0001, 1);
		ultracam_veio(uvd, 0x02, 0x0000, 0x000b, 0);
		ultracam_resetPipe(uvd);
		ULTRACAM_T(uvd)->initialized = (setup_ok != 0);
	}
	return setup_ok;
}

static void ultracam_configure_video(struct uvd *uvd)
{
	if (uvd == NULL)
		return;

	RESTRICT_TO_RANGE(init_brightness, 0, 255);
	RESTRICT_TO_RANGE(init_contrast, 0, 255);
	RESTRICT_TO_RANGE(init_color, 0, 255);
	RESTRICT_TO_RANGE(init_hue, 0, 255);
	RESTRICT_TO_RANGE(hue_correction, 0, 255);

	memset(&uvd->vpic, 0, sizeof(uvd->vpic));
	memset(&uvd->vpic_old, 0x55, sizeof(uvd->vpic_old));

	uvd->vpic.colour = init_color << 8;
	uvd->vpic.hue = init_hue << 8;
	uvd->vpic.brightness = init_brightness << 8;
	uvd->vpic.contrast = init_contrast << 8;
	uvd->vpic.whiteness = 105 << 8; /* This one isn't used */
	uvd->vpic.depth = 24;
	uvd->vpic.palette = VIDEO_PALETTE_RGB24;

	memset(&uvd->vcap, 0, sizeof(uvd->vcap));
	strcpy(uvd->vcap.name, "IBM Ultra Camera");
	uvd->vcap.type = VID_TYPE_CAPTURE;
	uvd->vcap.channels = 1;
	uvd->vcap.audios = 0;
	uvd->vcap.maxwidth = VIDEOSIZE_X(uvd->canvas);
	uvd->vcap.maxheight = VIDEOSIZE_Y(uvd->canvas);
	uvd->vcap.minwidth = min_canvasWidth;
	uvd->vcap.minheight = min_canvasHeight;

	memset(&uvd->vchan, 0, sizeof(uvd->vchan));
	uvd->vchan.flags = 0;
	uvd->vchan.tuners = 0;
	uvd->vchan.channel = 0;
	uvd->vchan.type = VIDEO_TYPE_CAMERA;
	strcpy(uvd->vchan.name, "Camera");
}

/*
 * ultracam_probe()
 *
 * This procedure queries device descriptor and accepts the interface
 * if it looks like our camera.
 *
 * History:
 * 12-Nov-2000 Reworked to comply with new probe() signature.
 * 23-Jan-2001 Added compatibility with 2.2.x kernels.
 */
static void *ultracam_probe(struct usb_device *dev, unsigned int ifnum ,const struct usb_device_id *devid)
{
	struct uvd *uvd = NULL;
	int i, nas;
	int actInterface=-1, inactInterface=-1, maxPS=0;
	unsigned char video_ep = 0;

	if (debug >= 1)
		info("ultracam_probe(%p,%u.)", dev, ifnum);

	/* We don't handle multi-config cameras */
	if (dev->descriptor.bNumConfigurations != 1)
		return NULL;

	/* Is it an IBM camera? */
	if ((dev->descriptor.idVendor != ULTRACAM_VENDOR_ID) ||
	    (dev->descriptor.idProduct != ULTRACAM_PRODUCT_ID))
		return NULL;

	info("IBM Ultra camera found (rev. 0x%04x)", dev->descriptor.bcdDevice);

	/* Validate found interface: must have one ISO endpoint */
	nas = dev->actconfig->interface[ifnum].num_altsetting;
	if (debug > 0)
		info("Number of alternate settings=%d.", nas);
	if (nas < 8) {
		err("Too few alternate settings for this camera!");
		return NULL;
	}
	/* Validate all alternate settings */
	for (i=0; i < nas; i++) {
		const struct usb_interface_descriptor *interface;
		const struct usb_endpoint_descriptor *endpoint;

		interface = &dev->actconfig->interface[ifnum].altsetting[i];
		if (interface->bNumEndpoints != 1) {
			err("Interface %d. has %u. endpoints!",
			    ifnum, (unsigned)(interface->bNumEndpoints));
			return NULL;
		}
		endpoint = &interface->endpoint[0];
		if (video_ep == 0)
			video_ep = endpoint->bEndpointAddress;
		else if (video_ep != endpoint->bEndpointAddress) {
			err("Alternate settings have different endpoint addresses!");
			return NULL;
		}
		if ((endpoint->bmAttributes & 0x03) != 0x01) {
			err("Interface %d. has non-ISO endpoint!", ifnum);
			return NULL;
		}
		if ((endpoint->bEndpointAddress & 0x80) == 0) {
			err("Interface %d. has ISO OUT endpoint!", ifnum);
			return NULL;
		}
		if (endpoint->wMaxPacketSize == 0) {
			if (inactInterface < 0)
				inactInterface = i;
			else {
				err("More than one inactive alt. setting!");
				return NULL;
			}
		} else {
			if (actInterface < 0) {
				actInterface = i;
				maxPS = endpoint->wMaxPacketSize;
				if (debug > 0)
					info("Active setting=%d. maxPS=%d.", i, maxPS);
			} else {
				/* Got another active alt. setting */
				if (maxPS < endpoint->wMaxPacketSize) {
					/* This one is better! */
					actInterface = i;
					maxPS = endpoint->wMaxPacketSize;
					if (debug > 0) {
						info("Even better ctive setting=%d. maxPS=%d.",
						     i, maxPS);
					}
				}
			}
		}
	}
	if ((maxPS <= 0) || (actInterface < 0) || (inactInterface < 0)) {
		err("Failed to recognize the camera!");
		return NULL;
	}

	/* Code below may sleep, need to lock module while we are here */
	MOD_INC_USE_COUNT;
	uvd = usbvideo_AllocateDevice(cams);
	if (uvd != NULL) {
		/* Here uvd is a fully allocated uvd object */
		uvd->flags = flags;
		uvd->debug = debug;
		uvd->dev = dev;
		uvd->iface = ifnum;
		uvd->ifaceAltInactive = inactInterface;
		uvd->ifaceAltActive = actInterface;
		uvd->video_endp = video_ep;
		uvd->iso_packet_len = maxPS;
		uvd->paletteBits = 1L << VIDEO_PALETTE_RGB24;
		uvd->defaultPalette = VIDEO_PALETTE_RGB24;
		uvd->canvas = VIDEOSIZE(640, 480);	/* FIXME */
		uvd->videosize = uvd->canvas; /* ultracam_size_to_videosize(size);*/

		/* Initialize ibmcam-specific data */
		assert(ULTRACAM_T(uvd) != NULL);
		ULTRACAM_T(uvd)->camera_model = 0; /* Not used yet */
		ULTRACAM_T(uvd)->initialized = 0;

		ultracam_configure_video(uvd);

		i = usbvideo_RegisterVideoDevice(uvd);
		if (i != 0) {
			err("usbvideo_RegisterVideoDevice() failed.");
			uvd = NULL;
		}
	}
	MOD_DEC_USE_COUNT;
	return uvd;
}


static struct usb_device_id id_table[] = {
	{ USB_DEVICE(ULTRACAM_VENDOR_ID, ULTRACAM_PRODUCT_ID) },
	{ }  /* Terminating entry */
};

/*
 * ultracam_init()
 *
 * This code is run to initialize the driver.
 */
static int __init ultracam_init(void)
{
	struct usbvideo_cb cbTbl;
	memset(&cbTbl, 0, sizeof(cbTbl));
	cbTbl.probe = ultracam_probe;
	cbTbl.setupOnOpen = ultracam_setup_on_open;
	cbTbl.videoStart = ultracam_video_start;
	cbTbl.videoStop = ultracam_video_stop;
	cbTbl.processData = ultracam_ProcessIsocData;
	cbTbl.postProcess = usbvideo_DeinterlaceFrame;
	cbTbl.adjustPicture = ultracam_adjust_picture;
	cbTbl.getFPS = ultracam_calculate_fps;
	return usbvideo_register(
		&cams,
		MAX_CAMERAS,
		sizeof(ultracam_t),
		"ultracam",
		&cbTbl,
		THIS_MODULE,
		id_table);
}

static void __exit ultracam_cleanup(void)
{
	usbvideo_Deregister(&cams);
}

MODULE_DEVICE_TABLE(usb, id_table);
MODULE_LICENSE("GPL");

module_init(ultracam_init);
module_exit(ultracam_cleanup);
