/*
	Winbond w9966cf Webcam parport driver.

	Copyright (C) 2001 Jakob Kemi <jakob.kemi@telia.com>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
/*
	Supported devices:
	  * Lifeview Flycam Supra (Philips saa7111a chip)

	  * Mikrotek Eyestar2 (Sanyo lc99053 chip)
	    Very rudimentary support, total lack of ccd-control chip settings.
	    Only green video data and no image properties (brightness, etc..)
	    If anyone can parse the Japanese data-sheet for the Sanyo lc99053
	    chip, feel free to help.
	    <http://service.semic.sanyo.co.jp/semi/ds_pdf_j/LC99053.pdf>
	    Thanks to Steven Griffiths <steve@sgriff.com> and
	    James Murray <jsm@jsm-net.demon.co.uk> for testing.

	Todo:
	  * Add a working EPP mode (Is this a parport or a w9966 issue?)
	  * Add proper probing. IEEE1284 probing of w9966 chips haven't
	    worked since parport drivers changed in 2.4.x.
	  * Probe for onboard SRAM, port directions etc. (possible?)

	Changes:

	Alan Cox:	Removed RGB mode for kernel merge, added THIS_MODULE
			and owner support for newer module locks
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/videodev.h>
#include <linux/parport.h>
#include <linux/types.h>
#include <linux/slab.h>

//#define DEBUG				// Define for debug output.

#ifdef DEBUG
#   define DPRINTF(f, a...)						\
        do {								\
            printk ("%s%s, %d (DEBUG) %s(): ",				\
                KERN_DEBUG, __FILE__, __LINE__, __func__);		\
            printk (f, ##a);						\
        } while (0)
#   define DASSERT(x)							\
        do {								\
            if (!x)							\
                DPRINTF("Assertion failed at line %d.\n", __LINE__);	\
        } while (0)
#else
#   define DPRINTF(f, a...) do {} while(0)
#   define DASSERT(f, a...) do {} while(0)
#endif

/*
 *	Defines, simple typedefs etc.
 */

#define W9966_DRIVERNAME	"w9966cf"
#define W9966_MAXCAMS		4	// Maximum number of cameras
#define W9966_RBUFFER		8096	// Read buffer (must be an even number)

#define W9966_WND_MIN_W		2
#define W9966_WND_MIN_H		1

// Keep track of our current state
#define W9966_STATE_PDEV	0x01	// pdev registered
#define W9966_STATE_CLAIMED	0x02	// pdev claimed
#define W9966_STATE_VDEV	0x04	// vdev registered
#define W9966_STATE_BUFFER	0x08	// buffer allocated
#define W9966_STATE_DETECTED	0x10	// model identified

#define W9966_SAA7111_ID	0x24	// I2C device id

#define W9966_I2C_UDELAY	5
#define W9966_I2C_TIMEOUT	100
#define W9966_I2C_R_DATA	0x08
#define W9966_I2C_R_CLOCK	0x04
#define W9966_I2C_W_DATA	0x02
#define W9966_I2C_W_CLOCK	0x01

#define MAX(a, b) ((a > b) ? a : b)
#define MIN(a, b) ((a > b) ? b : a)

struct w9966_dev {
	struct video_device vdev;
	struct parport*     pport;
	struct pardevice*   pdev;
	int ppmode;

	u8* buffer;
	u8  dev_state;
	u8  i2c_state;
	u16 width;
	u16 height;

	// Image properties
	u8 brightness;
	s8 contrast;
	s8 color;
	s8 hue;

	// Model specific:
	const char* name;
	u32 sramsize;
	u8  sramid;		// reg 0x0c, bank layout
	u8  cmask;		// reg 0x01, for polarity
	u16 min_x, max_x;	// Capture window limits
	u16 min_y, max_y;
	int (*image)(struct w9966_dev* cam);
};

/*
 *	Module properties
 */

MODULE_AUTHOR("Jakob Kemi <jakob.kemi@telia.com>");
MODULE_DESCRIPTION("Winbond w9966cf webcam driver (Flycam Supra and others)");
MODULE_LICENSE("GPL");


static const char* pardev[] = {[0 ... W9966_MAXCAMS-1] = "auto"};
MODULE_PARM(pardev, "0-" __MODULE_STRING(W9966_MAXCAMS) "s");
MODULE_PARM_DESC(pardev,"\n\
<auto|name|none[,...]> Where to find cameras.\n\
  auto = probe all parports for camera (default)\n\
  name = name of parport (eg parport0)\n\
  none = don't use this camera\n\
You can specify all cameras this way, for example:\n\
  pardev=parport2,auto,none,parport0 would search for cam1 on parport2, search\n\
  for cam2 on all parports, skip cam3 and search for cam4 on parport0");

static int parmode = 1;
MODULE_PARM(parmode, "i");
MODULE_PARM_DESC(parmode, "\n<0|1|2|3> transfer mode (0=auto, 1=ecp(default), 2=epp, 3=forced hw-ecp)");

static int video_nr[] = {[0 ... W9966_MAXCAMS-1] = -1};
MODULE_PARM(video_nr, "0-" __MODULE_STRING(W9966_MAXCAMS) "i");
MODULE_PARM_DESC(video_nr,"\n\
<-1|n[,...]> Specify V4L minor mode number.\n\
  -1 = use next available (default)\n\
   n = use minor number n (integer >= 0)\n\
You can specify all cameras this way, for example:\n\
  video_nr=-1,2,-1 would assign minor number 2 for cam2 and use auto for cam1,\n\
  cam3 and cam4");

/*
 *	Private data
 */

static struct w9966_dev* w9966_cams;

/*
 *	Private function declarations
 */

static inline void w9966_flag_set(struct w9966_dev* cam, int flag) {
	cam->dev_state |= flag;}

static inline void w9966_flag_clear(struct w9966_dev* cam, int flag) {
	cam->dev_state &= ~flag;}

static inline int  w9966_flag_test(struct w9966_dev* cam, int flag) {
	return (cam->dev_state & flag);}

static inline int  w9966_pdev_claim(struct w9966_dev *vdev);
static inline void w9966_pdev_release(struct w9966_dev *vdev);

static int w9966_rreg(struct w9966_dev* cam, int reg);
static int w9966_wreg(struct w9966_dev* cam, int reg, int data);

static int  w9966_init(struct w9966_dev* cam, struct parport* port, int vidnr);
static void w9966_term(struct w9966_dev* cam);
static int  w9966_setup(struct w9966_dev* cam);
static int  w9966_findlen(int near, int size, int maxlen);
static int  w9966_calcscale(int size, int min, int max,
			int* beg, int* end, u8* factor);
static int  w9966_window(struct w9966_dev* cam, int x1, int y1,
			int x2, int y2, int w, int h);

static int w9966_saa7111_init(struct w9966_dev* cam);
static int w9966_saa7111_image(struct w9966_dev* cam);
static int w9966_lc99053_init(struct w9966_dev* cam);
static int w9966_lc99053_image(struct w9966_dev* cam);

static inline void w9966_i2c_setsda(struct w9966_dev* cam, int state);
static inline int  w9966_i2c_setscl(struct w9966_dev* cam, int state);
static inline int  w9966_i2c_getsda(struct w9966_dev* cam);
static inline int  w9966_i2c_getscl(struct w9966_dev* cam);
static int w9966_i2c_wbyte(struct w9966_dev* cam, int data);
static int w9966_i2c_rbyte(struct w9966_dev* cam);
static int w9966_i2c_rreg(struct w9966_dev* cam, int device, int reg);
static int w9966_i2c_wreg(struct w9966_dev* cam, int device, int reg, int data);

static int  w9966_v4l_open(struct video_device *vdev, int mode);
static void w9966_v4l_close(struct video_device *vdev);
static int  w9966_v4l_ioctl(struct video_device *vdev,
			unsigned int cmd, void *arg);
static long w9966_v4l_read(struct video_device *vdev,
			char *buf, unsigned long count, int noblock);

/*
 *	Private function definitions
 */

// Claim parport for ourself
// 1 on success, else 0
static inline int w9966_pdev_claim(struct w9966_dev* cam)
{
	if (w9966_flag_test(cam, W9966_STATE_CLAIMED))
		return 1;
	if (parport_claim_or_block(cam->pdev) < 0)
		return 0;
	w9966_flag_set(cam, W9966_STATE_CLAIMED);
	return 1;
}

// Release parport for others to use
static inline void w9966_pdev_release(struct w9966_dev* cam)
{
	if (!w9966_flag_test(cam, W9966_STATE_CLAIMED))
		return;
	parport_release(cam->pdev);
	w9966_flag_clear(cam, W9966_STATE_CLAIMED);
}

// Read register from w9966 interface-chip
// Expects a claimed pdev
// -1 on error, else register data (byte)
static int w9966_rreg(struct w9966_dev* cam, int reg)
{
	// ECP, read, regtransfer, REG, REG, REG, REG, REG
	const u8 addr = 0x80 | (reg & 0x1f);
	u8 val;

	if (parport_negotiate(cam->pport, cam->ppmode | IEEE1284_ADDR) != 0 ||
	    parport_write(cam->pport, &addr, 1) != 1 ||
	    parport_negotiate(cam->pport, cam->ppmode | IEEE1284_DATA) != 0 ||
	    parport_read(cam->pport, &val, 1) != 1)
		return -1;

	return val;
}

// Write register to w9966 interface-chip
// Expects a claimed pdev
// 1 on success, else 0
static int w9966_wreg(struct w9966_dev* cam, int reg, int data)
{
	// ECP, write, regtransfer, REG, REG, REG, REG, REG
	const u8 addr = 0xc0 | (reg & 0x1f);
	const u8 val = data;

	if (parport_negotiate(cam->pport, cam->ppmode | IEEE1284_ADDR) != 0 ||
	    parport_write(cam->pport, &addr, 1) != 1 ||
	    parport_negotiate(cam->pport, cam->ppmode | IEEE1284_DATA) != 0 ||
	    parport_write(cam->pport, &val, 1) != 1)
		return 0;

	return 1;
}

// Initialize camera device. Setup all internal flags, set a
// default video mode, setup ccd-chip, register v4l device etc..
// Also used for 'probing' of hardware.
// 1 on success, else 0
static int w9966_init(struct w9966_dev* cam, struct parport* port, int vidnr)
{
	if (cam->dev_state != 0)
		return 0;

	cam->pport = port;
	cam->brightness = 128;
	cam->contrast = 64;
	cam->color = 64;
	cam->hue = 0;

	// Select requested transfer mode
	switch(parmode)
	{
	default:	// Auto-detect (priority: hw-ecp, hw-epp, sw-ecp)
	case 0:
		if (port->modes & PARPORT_MODE_ECP)
			cam->ppmode = IEEE1284_MODE_ECP;
		else if (port->modes & PARPORT_MODE_EPP)
			cam->ppmode = IEEE1284_MODE_EPP;
		else
			cam->ppmode = IEEE1284_MODE_ECPSWE;
		break;
	case 1:		// hw- or sw-ecp
		if (port->modes & PARPORT_MODE_ECP)
			cam->ppmode = IEEE1284_MODE_ECP;
		else
			cam->ppmode = IEEE1284_MODE_ECPSWE;
		break;
	case 2:		// hw- or sw-epp
		if (port->modes & PARPORT_MODE_EPP)
			cam->ppmode = IEEE1284_MODE_EPP;
		else
			cam->ppmode = IEEE1284_MODE_EPPSWE;
		break;
	case 3:		// hw-ecp
		cam->ppmode = IEEE1284_MODE_ECP;
		break;
	}

	// Tell the parport driver that we exists
	cam->pdev = parport_register_device(
	    port, W9966_DRIVERNAME, NULL, NULL, NULL, 0, NULL);

	if (cam->pdev == NULL) {
		DPRINTF("parport_register_device() failed.\n");
		return 0;
	}
	w9966_flag_set(cam, W9966_STATE_PDEV);

	// Claim parport
	if (!w9966_pdev_claim(cam)) {
		DPRINTF("w9966_pdev_claim() failed.\n");
		return 0;
	}

	// Perform initial w9966 setup
	if (!w9966_setup(cam)) {
		DPRINTF("w9966_setup() failed.\n");
		return 0;
	}

	// Detect model
	if (!w9966_saa7111_init(cam)) {
		DPRINTF("w9966_saa7111_init() failed.\n");
		return 0;
	}
	if (!w9966_lc99053_init(cam)) {
		DPRINTF("w9966_lc99053_init() failed.\n");
		return 0;
	}
	if (!w9966_flag_test(cam, W9966_STATE_DETECTED)) {
		DPRINTF("Camera model not identified.\n");
		return 0;
	}

	// Setup w9966 with a default capture mode (QCIF res.)
	if (!w9966_window(cam, 0, 0, 1023, 1023, 176, 144)) {
		DPRINTF("w9966_window() failed.\n");
		return 0;
	}
	w9966_pdev_release(cam);

	// Fill in the video_device struct and register us to v4l
	memset(&cam->vdev, 0, sizeof(struct video_device));
	strcpy(cam->vdev.name, W9966_DRIVERNAME);
	cam->vdev.type = VID_TYPE_CAPTURE | VID_TYPE_SCALES;
	cam->vdev.hardware = VID_HARDWARE_W9966;
	cam->vdev.open = &w9966_v4l_open;
	cam->vdev.close = &w9966_v4l_close;
	cam->vdev.read = &w9966_v4l_read;
	cam->vdev.ioctl = &w9966_v4l_ioctl;
	cam->vdev.priv = (void*)cam;
	cam->vdev.owner = THIS_MODULE;

	if (video_register_device(&cam->vdev, VFL_TYPE_GRABBER, vidnr) == -1) {
		DPRINTF("video_register_device() failed (minor: %d).\n", vidnr);
		return 0;
	}
	w9966_flag_set(cam, W9966_STATE_VDEV);

	// All ok
	printk("w9966: Found and initialized %s on %s.\n",
		cam->name, cam->pport->name);
	return 1;
}

// Terminate everything gracefully
static void w9966_term(struct w9966_dev* cam)
{
	// Delete allocated buffer
	if (w9966_flag_test(cam, W9966_STATE_BUFFER))
		kfree(cam->buffer);

	// Unregister from v4l
	if (w9966_flag_test(cam, W9966_STATE_VDEV))
		video_unregister_device(&cam->vdev);

	// Terminate from IEEE1284 mode and unregister from parport
	if (w9966_flag_test(cam, W9966_STATE_PDEV)) {
		if (w9966_pdev_claim(cam))
			parport_negotiate(cam->pport, IEEE1284_MODE_COMPAT);

		w9966_pdev_release(cam);
		parport_unregister_device(cam->pdev);
	}

	cam->dev_state = 0x00;
}

// Do initial setup for the w9966 chip, init i2c bus, etc.
// this is generic for all models
// expects a claimed pdev
// 1 on success, else 0
static int w9966_setup(struct w9966_dev* cam)
{
	const u8 i2c = cam->i2c_state = W9966_I2C_W_DATA | W9966_I2C_W_CLOCK;
	const u8 regs[] = {
		0x40,			// 0x13 - VEE control (raw 4:2:2)
		0x00, 0x00, 0x00,	// 0x14 - 0x16
		0x00,			// 0x17 - ???
		i2c,			// 0x18 - Serial bus
		0xff,			// 0x19 - I/O port direction control
		0xff,			// 0x1a - I/O port data register
		0x10			// 0x1b - ???
	};
	int i;

	DASSERT(w9966_flag_test(cam, W9966_STATE_CLAIMED));

	// Reset (ECP-fifo & serial-bus)
	if (!w9966_wreg(cam, 0x00, 0x03) ||
	    !w9966_wreg(cam, 0x00, 0x00))
		return 0;

	// Write regs to w9966cf chip
	for (i = 0x13; i < 0x1c; i++)
		if (!w9966_wreg(cam, i, regs[i-0x13]))
			return 0;

	return 1;
}

// Find a good length for capture window (used both for W and H)
// A bit ugly but pretty functional. The capture length
// have to match the downscale
static int w9966_findlen(int near, int size, int maxlen)
{
	int bestlen = size;
	int besterr = abs(near - bestlen);
	int len;

	for(len = size+1; len < maxlen; len++)
	{
		int err;
		if ( ((64*size) %len) != 0)
			continue;

		err = abs(near - len);

		// Only continue as long as we keep getting better values
		if (err > besterr)
			break;

		besterr = err;
		bestlen = len;
	}

	return bestlen;
}

// Modify capture window (if necessary)
// and calculate downscaling
// 1 on success, else 0
static int w9966_calcscale(int size, int min, int max, int* beg, int* end, u8* factor)
{
	const int maxlen = max - min;
	const int len = *end - *beg + 1;
	const int newlen = w9966_findlen(len, size, maxlen);
	const int err = newlen - len;

	// Check for bad format
	if (newlen > maxlen || newlen < size)
		return 0;

	// Set factor (6 bit fixed)
	*factor = (64*size) / newlen;
	if (*factor == 64)
		*factor = 0x00;	// downscale is disabled
	else
		*factor |= 0x80; // set downscale-enable bit

	// Modify old beginning and end
	*beg -= err / 2;
	*end += err - (err / 2);

	// Move window if outside borders
	if (*beg < min) {
		*end += min - *beg;
		*beg += min - *beg;
	}
	if (*end > max) {
		*beg -= *end - max;
		*end -= *end - max;
	}

	return 1;
}

// Setup the w9966 capture window and also set SRAM settings
// expects a claimed pdev and detected camera model
// 1 on success, else 0
static int w9966_window(struct w9966_dev* cam, int x1, int y1, int x2, int y2, int w, int h)
{
	unsigned int enh_s, enh_e;
	u8 scale_x, scale_y;
	u8 regs[0x13];
	int i;

	// Modify width and height to match capture window and SRAM
	w = MAX(W9966_WND_MIN_W, w);
	h = MAX(W9966_WND_MIN_H, h);
	w = MIN(cam->max_x - cam->min_x, w);
	h = MIN(cam->max_y - cam->min_y, h);
	w &= ~0x1;
	if (w*h*2 > cam->sramsize)
		h = cam->sramsize / (w*2);

	cam->width = w;
	cam->height = h;

	enh_s = 0;
	enh_e = w*h*2;

	// Calculate downscaling
	if (!w9966_calcscale(w, cam->min_x, cam->max_x, &x1, &x2, &scale_x) ||
	    !w9966_calcscale(h, cam->min_y, cam->max_y, &y1, &y2, &scale_y))
		return 0;

	DPRINTF("%dx%d, x: %d<->%d, y: %d<->%d, sx: %d/64, sy: %d/64.\n",
		w, h, x1, x2, y1, y2, scale_x&~0x80, scale_y&~0x80);

	// Setup registers
	regs[0x00] = 0x00;			// Set normal operation
	regs[0x01] = cam->cmask;		// Capture mode
	regs[0x02] = scale_y;			// V-scaling
	regs[0x03] = scale_x;			// H-scaling

	// Capture window
	regs[0x04] = (x1 & 0x0ff);		// X-start (8 low bits)
	regs[0x05] = (x1 & 0x300)>>8;		// X-start (2 high bits)
	regs[0x06] = (y1 & 0x0ff);		// Y-start (8 low bits)
	regs[0x07] = (y1 & 0x300)>>8;		// Y-start (2 high bits)
	regs[0x08] = (x2 & 0x0ff);		// X-end (8 low bits)
	regs[0x09] = (x2 & 0x300)>>8;		// X-end (2 high bits)
	regs[0x0a] = (y2 & 0x0ff);		// Y-end (8 low bits)

	regs[0x0c] = cam->sramid;		// SRAM layout

	// Enhancement layer
	regs[0x0d] = (enh_s& 0x000ff);		// Enh. start (0-7)
	regs[0x0e] = (enh_s& 0x0ff00)>>8;	// Enh. start (8-15)
	regs[0x0f] = (enh_s& 0x70000)>>16;	// Enh. start (16-17/18??)
	regs[0x10] = (enh_e& 0x000ff);		// Enh. end (0-7)
	regs[0x11] = (enh_e& 0x0ff00)>>8;	// Enh. end (8-15)
	regs[0x12] = (enh_e& 0x70000)>>16;	// Enh. end (16-17/18??)

	// Write regs to w9966cf chip
	for (i = 0x01; i < 0x13; i++)
		if (!w9966_wreg(cam, i, regs[i]))
			return 0;

	return 1;
}

// Detect and initialize saa7111 ccd-controller chip.
// expects a claimed parport
// expected to always return 1 unless error is _fatal_
// 1 on success, else 0
static int w9966_saa7111_init(struct w9966_dev* cam)
{
	// saa7111 regs 0x00 trough 0x12
	const u8 regs[] = {
		0x00, // not written
		0x00, 0xd8, 0x23, 0x00, 0x80, 0x80, 0x00, 0x88, 0x10,
		cam->brightness,	// 0x0a
		cam->contrast,		// 0x0b
		cam->color,		// 0x0c
		cam->hue,		// 0x0d
		0x01, 0x00, 0x48, 0x0c, 0x00,
	};
	int i;

	if (w9966_flag_test(cam, W9966_STATE_DETECTED))
		return 1;

	// Write regs to saa7111 chip
	for (i = 1; i < 0x13; i++)
		if (!w9966_i2c_wreg(cam, W9966_SAA7111_ID, i, regs[i]))
			return 1;

	// Read back regs
	for (i = 1; i < 0x13; i++)
		if (w9966_i2c_rreg(cam, W9966_SAA7111_ID, i) != regs[i])
			return 1;

	// Fill in model specific data
	cam->name = "Lifeview Flycam Supra";
	cam->sramsize = 128 << 10;	// 128 kib
	cam->sramid = 0x02;		// see w9966.pdf

	cam->cmask = 0x18;		// normal polarity
	cam->min_x = 16;		// empirically determined
	cam->max_x = 705;
	cam->min_y = 14;
	cam->max_y = 253;
	cam->image = &w9966_saa7111_image;

	DPRINTF("Found and initialized a saa7111 chip.\n");
	w9966_flag_set(cam, W9966_STATE_DETECTED);

	return 1;
}

// Setup image properties (brightness, hue, etc.) for the saa7111 chip
// expects a claimed parport
// 1 on success, else 0
static int w9966_saa7111_image(struct w9966_dev* cam)
{
	if (!w9966_i2c_wreg(cam, W9966_SAA7111_ID, 0x0a, cam->brightness) ||
	    !w9966_i2c_wreg(cam, W9966_SAA7111_ID, 0x0b, cam->contrast) ||
	    !w9966_i2c_wreg(cam, W9966_SAA7111_ID, 0x0c, cam->color) ||
	    !w9966_i2c_wreg(cam, W9966_SAA7111_ID, 0x0d, cam->hue))
		return 0;

	return 1;
}

// Detect and initialize lc99053 ccd-controller chip.
// expects a claimed parport
// this is currently a hack, no detection is done, we just assume an Eyestar2
// 1 on success, else 0
static int w9966_lc99053_init(struct w9966_dev* cam)
{
	if (w9966_flag_test(cam, W9966_STATE_DETECTED))
		return 1;

	// Fill in model specific data
	cam->name = "Microtek Eyestar2";
	cam->sramsize = 128 << 10;	// 128 kib
	cam->sramid = 0x02;		// w9966cf.pdf

	cam->cmask = 0x10;		// reverse polarity
	cam->min_x = 16;		// empirically determined
	cam->max_x = 705;
	cam->min_y = 14;
	cam->max_y = 253;
	cam->image = &w9966_lc99053_image;

	DPRINTF("Found and initialized a lc99053 chip.\n");
	w9966_flag_set(cam, W9966_STATE_DETECTED);

	return 1;
}

// Setup image properties (brightness, hue, etc.) for the lc99053 chip
// expects a claimed parport
// 1 on success, else 0
static int w9966_lc99053_image(struct w9966_dev* cam)
{
	return 1;
}

/*
 *	Ugly and primitive i2c protocol functions
 */

// Sets the data line on the i2c bus.
// Expects a claimed pdev.
static inline void w9966_i2c_setsda(struct w9966_dev* cam, int state)
{
	if (state)
		cam->i2c_state |= W9966_I2C_W_DATA;
	else
		cam->i2c_state &= ~W9966_I2C_W_DATA;

	w9966_wreg(cam, 0x18, cam->i2c_state);
	udelay(W9966_I2C_UDELAY);
}

// Sets the clock line on the i2c bus.
// Expects a claimed pdev.
// 1 on success, else 0
static inline int w9966_i2c_setscl(struct w9966_dev* cam, int state)
{
	if (state)
		cam->i2c_state |= W9966_I2C_W_CLOCK;
	else
		cam->i2c_state &= ~W9966_I2C_W_CLOCK;

	w9966_wreg(cam, 0x18, cam->i2c_state);
	udelay(W9966_I2C_UDELAY);

	// when we go to high, we also expect the peripheral to ack.
	if (state) {
		const int timeout = jiffies + W9966_I2C_TIMEOUT;
		while (!w9966_i2c_getscl(cam)) {
			if (time_after(jiffies, timeout))
				return 0;
		}
	}
	return 1;
}

// Get peripheral data line
// Expects a claimed pdev.
static inline int w9966_i2c_getsda(struct w9966_dev* cam)
{
	const u8 pins = w9966_rreg(cam, 0x18);
	return ((pins & W9966_I2C_R_DATA) > 0);
}

// Get peripheral clock line
// Expects a claimed pdev.
static inline int w9966_i2c_getscl(struct w9966_dev* cam)
{
	const u8 pins = w9966_rreg(cam, 0x18);
	return ((pins & W9966_I2C_R_CLOCK) > 0);
}

// Write a byte with ack to the i2c bus.
// Expects a claimed pdev.
// 1 on success, else 0
static int w9966_i2c_wbyte(struct w9966_dev* cam, int data)
{
	int i;
	for (i = 7; i >= 0; i--) {
		w9966_i2c_setsda(cam, (data >> i) & 0x01);

		if (!w9966_i2c_setscl(cam, 1) ||
		    !w9966_i2c_setscl(cam, 0))
			return 0;
	}
	w9966_i2c_setsda(cam, 1);

	if (!w9966_i2c_setscl(cam, 1) ||
	    !w9966_i2c_setscl(cam, 0))
		return 0;

	return 1;
}

// Read a data byte with ack from the i2c-bus
// Expects a claimed pdev. -1 on error
static int w9966_i2c_rbyte(struct w9966_dev* cam)
{
	u8 data = 0x00;
	int i;

	w9966_i2c_setsda(cam, 1);

	for (i = 0; i < 8; i++)
	{
		if (!w9966_i2c_setscl(cam, 1))
			return -1;
		data = data << 1;
		if (w9966_i2c_getsda(cam))
			data |= 0x01;

		w9966_i2c_setscl(cam, 0);
	}
	return data;
}

// Read a register from the i2c device.
// Expects claimed pdev. -1 on error
static int w9966_i2c_rreg(struct w9966_dev* cam, int device, int reg)
{
	int data;

	w9966_i2c_setsda(cam, 0);
	w9966_i2c_setscl(cam, 0);

	if (!w9966_i2c_wbyte(cam, device << 1) ||
	    !w9966_i2c_wbyte(cam, reg))
		return -1;

	w9966_i2c_setsda(cam, 1);
	if (!w9966_i2c_setscl(cam, 1))
		return -1;

	w9966_i2c_setsda(cam, 0);
	w9966_i2c_setscl(cam, 0);

	if (!w9966_i2c_wbyte(cam, (device << 1) | 1) ||
	    (data = w9966_i2c_rbyte(cam)) == -1)
		return -1;

	w9966_i2c_setsda(cam, 0);

	if (!w9966_i2c_setscl(cam, 1))
		return -1;

	w9966_i2c_setsda(cam, 1);

	return data;
}

// Write a register to the i2c device.
// Expects claimed pdev.
// 1 on success, else 0
static int w9966_i2c_wreg(struct w9966_dev* cam, int device, int reg, int data)
{
	w9966_i2c_setsda(cam, 0);
	w9966_i2c_setscl(cam, 0);

	if (!w9966_i2c_wbyte(cam, device << 1) ||
	    !w9966_i2c_wbyte(cam, reg) ||
	    !w9966_i2c_wbyte(cam, data))
		return 0;

	w9966_i2c_setsda(cam, 0);
	if (!w9966_i2c_setscl(cam, 1))
		return 0;

	w9966_i2c_setsda(cam, 1);

	return 1;
}

/*
 *	Video4linux interface
 */

static int w9966_v4l_open(struct video_device *vdev, int flags)
{
	struct w9966_dev *cam = (struct w9966_dev*)vdev->priv;

	// Claim parport
	if (!w9966_pdev_claim(cam)) {
		DPRINTF("Unable to claim parport");
		return -EFAULT;
	}

	// Allocate read buffer
	cam->buffer = (u8*)kmalloc(W9966_RBUFFER, GFP_KERNEL);
	if (cam->buffer == NULL) {
		w9966_pdev_release(cam);
		return -ENOMEM;
	}
	w9966_flag_set(cam, W9966_STATE_BUFFER);

	return 0;
}

static void w9966_v4l_close(struct video_device *vdev)
{
	struct w9966_dev *cam = (struct w9966_dev*)vdev->priv;

	// Free read buffer
	if (w9966_flag_test(cam, W9966_STATE_BUFFER)) {
		kfree(cam->buffer);
		w9966_flag_clear(cam, W9966_STATE_BUFFER);
	}

	// release parport
	w9966_pdev_release(cam);
}

// expects a claimed parport
static int w9966_v4l_ioctl(struct video_device *vdev, unsigned int cmd, void *arg)
{
	struct w9966_dev *cam = (struct w9966_dev*)vdev->priv;

	switch(cmd)
	{
	case VIDIOCGCAP:
	{
		struct video_capability vcap = {
			W9966_DRIVERNAME,	// name
			VID_TYPE_CAPTURE | VID_TYPE_SCALES,	// type
			1, 0,			// vid, aud channels
			cam->max_x - cam->min_x,
			cam->max_y - cam->min_y,
			W9966_WND_MIN_W,
			W9966_WND_MIN_H
		};

		if(copy_to_user(arg, &vcap, sizeof(vcap)) != 0)
			return -EFAULT;

		return 0;
	}
	case VIDIOCGCHAN:
	{
		struct video_channel vch;
		if(copy_from_user(&vch, arg, sizeof(vch)) != 0)
			return -EFAULT;

		if(vch.channel != 0)	// We only support one channel (#0)
			return -EINVAL;

		strcpy(vch.name, "CCD-input");
		vch.flags = 0;		// We have no tuner or audio
		vch.tuners = 0;
		vch.type = VIDEO_TYPE_CAMERA;
		vch.norm = 0;		// ???

		if(copy_to_user(arg, &vch, sizeof(vch)) != 0)
			return -EFAULT;

		return 0;
	}
	case VIDIOCSCHAN:
	{
		struct video_channel vch;
		if(copy_from_user(&vch, arg, sizeof(vch) ) != 0)
			return -EFAULT;

		if(vch.channel != 0)
			return -EINVAL;

		return 0;
	}
	case VIDIOCGTUNER:
	{
		struct video_tuner vtune;
		if(copy_from_user(&vtune, arg, sizeof(vtune)) != 0)
			return -EFAULT;

		if(vtune.tuner != 0)
			return -EINVAL;

		strcpy(vtune.name, "no tuner");
		vtune.rangelow = 0;
		vtune.rangehigh = 0;
		vtune.flags = VIDEO_TUNER_NORM;
		vtune.mode = VIDEO_MODE_AUTO;
		vtune.signal = 0xffff;

		if(copy_to_user(arg, &vtune, sizeof(vtune)) != 0)
			return -EFAULT;

		return 0;
	}
	case VIDIOCSTUNER:
	{
		struct video_tuner vtune;
		if (copy_from_user(&vtune, arg, sizeof(vtune)) != 0)
			return -EFAULT;

		if (vtune.tuner != 0)
			return -EINVAL;

		if (vtune.mode != VIDEO_MODE_AUTO)
			return -EINVAL;

		return 0;
	}
	case VIDIOCGPICT:
	{
		struct video_picture vpic = {
			cam->brightness << 8,	// brightness
			(cam->hue + 128) << 8,	// hue
			cam->color << 9,	// color
			cam->contrast << 9,	// contrast
			0x8000,			// whiteness
			16, VIDEO_PALETTE_YUV422// bpp, palette format
		};

		if(copy_to_user(arg, &vpic, sizeof(vpic)) != 0)
			return -EFAULT;

		return 0;
	}
	case VIDIOCSPICT:
	{
		struct video_picture vpic;
		if(copy_from_user(&vpic, arg, sizeof(vpic)) != 0)
			return -EFAULT;

		if (vpic.depth != 16 || vpic.palette != VIDEO_PALETTE_YUV422)
			return -EINVAL;

		cam->brightness = vpic.brightness >> 8;
		cam->hue = (vpic.hue >> 8) - 128;
		cam->color = vpic.colour >> 9;
		cam->contrast = vpic.contrast >> 9;

		if (!cam->image(cam))
			return -EFAULT;

		return 0;
	}
	case VIDIOCSWIN:
	{
		struct video_window vwin;

		if (copy_from_user(&vwin, arg, sizeof(vwin)) != 0)
			return -EFAULT;
		if (
		  vwin.flags != 0 ||
		  vwin.clipcount != 0)
			return -EINVAL;

		if (vwin.width  > cam->max_x - cam->min_x ||
		    vwin.height > cam->max_y - cam->min_y ||
		    vwin.width  < W9966_WND_MIN_W ||
		    vwin.height < W9966_WND_MIN_H)
			return -EINVAL;

		// Update camera regs
		if (!w9966_window(cam, 0, 0, 1023, 1023, vwin.width, vwin.height))
			return -EFAULT;

		return 0;
	}
	case VIDIOCGWIN:
	{
		struct video_window vwin;
		memset(&vwin, 0, sizeof(vwin));

		vwin.width = cam->width;
		vwin.height = cam->height;

		if(copy_to_user(arg, &vwin, sizeof(vwin)) != 0)
			return -EFAULT;

		return 0;
	}
	// Unimplemented
	case VIDIOCCAPTURE:
	case VIDIOCGFBUF:
	case VIDIOCSFBUF:
	case VIDIOCKEY:
	case VIDIOCGFREQ:
	case VIDIOCSFREQ:
	case VIDIOCGAUDIO:
	case VIDIOCSAUDIO:
		return -EINVAL;
	default:
		return -ENOIOCTLCMD;
	}
	return 0;
}

// Capture data
// expects a claimed parport and allocated read buffer
static long w9966_v4l_read(struct video_device *vdev, char *buf, unsigned long count,  int noblock)
{
	struct w9966_dev *cam = (struct w9966_dev *)vdev->priv;
	const u8 addr = 0xa0;	// ECP, read, CCD-transfer, 00000
	u8* dest = (u8*)buf;
	unsigned long dleft = count;

	// Why would anyone want more than this??
	if (count > cam->width * cam->height * 2)
		count = cam->width * cam->height * 2;

	w9966_wreg(cam, 0x00, 0x02);	// Reset ECP-FIFO buffer
	w9966_wreg(cam, 0x00, 0x00);	// Return to normal operation
	w9966_wreg(cam, 0x01, cam->cmask | 0x80);	// Enable capture

	// write special capture-addr and negotiate into data transfer
	if (parport_negotiate(cam->pport, cam->ppmode|IEEE1284_ADDR) != 0 ||
	    parport_write(cam->pport, &addr, 1) != 1 ||
	    parport_negotiate(cam->pport, cam->ppmode|IEEE1284_DATA) != 0) {
		DPRINTF("Unable to write capture-addr.\n");
		return -EFAULT;
	}

	while(dleft > 0)
	{
		const size_t tsize = (dleft > W9966_RBUFFER) ? W9966_RBUFFER : dleft;

		if (parport_read(cam->pport, cam->buffer, tsize) < tsize)
			return -EFAULT;

		if (copy_to_user(dest, cam->buffer, tsize) != 0)
			return -EFAULT;

		dest += tsize;
		dleft -= tsize;
	}

	w9966_wreg(cam, 0x01, cam->cmask);	// Disable capture

	return count;
}

// Called once for every parport on init
static void w9966_attach(struct parport *port)
{
	int i;

	for (i = 0; i < W9966_MAXCAMS; i++) {
		if (strcmp(pardev[i], "none") == 0 ||	// Skip if 'none' or if
		    w9966_cams[i].dev_state != 0)	// cam already assigned
			continue;

		if (strcmp(pardev[i], "auto") == 0 ||
		    strcmp(pardev[i], port->name) == 0) {
			if (!w9966_init(&w9966_cams[i], port, video_nr[i]))
				w9966_term(&w9966_cams[i]);
			break;	// return
		}
	}
}

// Called once for every parport on termination
static void w9966_detach(struct parport *port)
{
	int i;
	for (i = 0; i < W9966_MAXCAMS; i++)
	if (w9966_cams[i].dev_state != 0 && w9966_cams[i].pport == port)
		w9966_term(&w9966_cams[i]);
}


static struct parport_driver w9966_ppd = {
	W9966_DRIVERNAME,
	w9966_attach,
	w9966_detach,
	NULL
};

// Module entry point
static int __init w9966_mod_init(void)
{
	int i, err;

	w9966_cams = kmalloc(
		sizeof(struct w9966_dev) * W9966_MAXCAMS, GFP_KERNEL);

	if (!w9966_cams)
		return -ENOMEM;

	for (i = 0; i < W9966_MAXCAMS; i++)
		w9966_cams[i].dev_state = 0;

	// Register parport driver
	if ((err = parport_register_driver(&w9966_ppd)) != 0) {
		kfree(w9966_cams);
		w9966_cams = 0;
		return err;
	}

	return 0;
}

// Module cleanup
static void __exit w9966_mod_term(void)
{
	if (w9966_cams)
		kfree(w9966_cams);

	parport_unregister_driver(&w9966_ppd);
}

module_init(w9966_mod_init);
module_exit(w9966_mod_term);
