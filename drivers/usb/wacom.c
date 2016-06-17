/*
 * $Id: wacom.c,v 1.23 2001/05/29 12:57:18 vojtech Exp $
 *
 *  Copyright (c) 2000-2001 Vojtech Pavlik	<vojtech@suse.cz>
 *  Copyright (c) 2000 Andreas Bach Aaen	<abach@stofanet.dk>
 *  Copyright (c) 2000 Clifford Wolf		<clifford@clifford.at>
 *  Copyright (c) 2000 Sam Mosel		<sam.mosel@computer.org>
 *  Copyright (c) 2000 James E. Blair		<corvus@gnu.org>
 *  Copyright (c) 2000 Daniel Egger		<egger@suse.de>
 *  Copyright (c) 2001 Frederic Lepied		<flepied@mandrakesoft.com>
 *
 *  USB Wacom Graphire and Wacom Intuos tablet support
 *
 *  Sponsored by SuSE
 *
 *  ChangeLog:
 *      v0.1 (vp)  - Initial release
 *      v0.2 (aba) - Support for all buttons / combinations
 *      v0.3 (vp)  - Support for Intuos added
 *	v0.4 (sm)  - Support for more Intuos models, menustrip
 *			relative mode, proximity.
 *	v0.5 (vp)  - Big cleanup, nifty features removed,
 * 			they belong in userspace
 *	v1.8 (vp)  - Submit URB only when operating, moved to CVS,
 *			use input_report_key instead of report_btn and
 *			other cleanups
 *	v1.11 (vp) - Add URB ->dev setting for new kernels
 *	v1.11 (jb) - Add support for the 4D Mouse & Lens
 *	v1.12 (de) - Add support for two more inking pen IDs
 *	v1.14 (vp) - Use new USB device id probing scheme.
 *		     Fix Wacom Graphire mouse wheel
 *	v1.18 (vp) - Fix mouse wheel direction
 *		     Make mouse relative
 *      v1.20 (fl) - Report tool id for Intuos devices
 *                 - Multi tools support
 *                 - Corrected Intuos protocol decoding (airbrush, 4D mouse, lens cursor...)
 *                 - Add PL models support
 *		   - Fix Wacom Graphire mouse wheel again
 *	v1.21 (vp) - Removed protocol descriptions
 *		   - Added MISC_SERIAL for tool serial numbers
 *	      (gb) - Identify version on module load.
 *    v1.21.1 (fl) - added Graphire2 support
 *    v1.21.2 (fl) - added Intuos2 support
 *                 - added all the PL ids
 *    v1.21.3 (fl) - added another eraser id from Neil Okamoto
 *                 - added smooth filter for Graphire from Peri Hankey
 *                 - added PenPartner support from Olaf van Es
 *                 - new tool ids from Ole Martin Bjoerndalen
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <vojtech@suse.cz>, or by paper mail:
 * Vojtech Pavlik, Ucitelska 1576, Prague 8, 182 00 Czech Republic
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb.h>

/*
 * Version Information
 */
#define DRIVER_VERSION "v1.21.3"
#define DRIVER_AUTHOR "Vojtech Pavlik <vojtech@suse.cz>"
#define DRIVER_DESC "USB Wacom Graphire and Wacom Intuos tablet driver"

MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");

#define USB_VENDOR_ID_WACOM	0x056a

struct wacom_features {
	char *name;
	int pktlen;
	int x_max;
	int y_max;
	int pressure_max;
	int distance_max;
	void (*irq)(struct urb *urb);
	unsigned long evbit;
	unsigned long absbit;
	unsigned long relbit;
	unsigned long btnbit;
	unsigned long digibit;
};

struct wacom {
	signed char data[10];
	struct input_dev dev;
	struct usb_device *usbdev;
	struct urb irq;
	struct wacom_features *features;
	int tool[2];
	int open;
	__u32 serial[2];
};

static void wacom_pl_irq(struct urb *urb)
{
	struct wacom *wacom = urb->context;
	unsigned char *data = wacom->data;
	struct input_dev *dev = &wacom->dev;
	int prox;

	if (urb->status) return;

	if (data[0] != 2) {
		printk(KERN_ERR "wacom_pl_irq: received unknown report #%d\n", data[0]);
		return;
	}
	
	prox = data[1] & 0x20;
	
	input_report_key(dev, BTN_TOOL_PEN, prox);
	
	if (prox) {
		int pressure = (data[4] & 0x04) >> 2 | ((__u32)(data[7] & 0x7f) << 1);

		input_report_abs(dev, ABS_X, data[3] | ((__u32)data[2] << 8) | ((__u32)(data[1] & 0x03) << 16));
		input_report_abs(dev, ABS_Y, data[6] | ((__u32)data[5] << 8) | ((__u32)(data[4] & 0x03) << 8));
		input_report_abs(dev, ABS_PRESSURE, (data[7] & 0x80) ? (255 - pressure) : (pressure + 255));
		input_report_key(dev, BTN_TOUCH, data[4] & 0x08);
		input_report_key(dev, BTN_STYLUS, data[4] & 0x10);
		input_report_key(dev, BTN_STYLUS2, data[4] & 0x20);
	}
	
	input_event(dev, EV_MSC, MSC_SERIAL, 0);
}

static void wacom_penpartner_irq(struct urb *urb)
{
	struct wacom *wacom = urb->context;
	unsigned char *data = wacom->data;
	struct input_dev *dev = &wacom->dev;
	int x, y; 
	char pressure; 
	int leftmb;

	if (urb->status) return;

	x = data[2] << 8 | data[1];
	y = data[4] << 8 | data[3];
	pressure = data[6];
	leftmb = ((pressure > -80) && !(data[5] &20));

	input_report_key(dev, BTN_TOOL_PEN, 1);

	input_report_abs(dev, ABS_X, x);
	input_report_abs(dev, ABS_Y, y);
	input_report_abs(dev, ABS_PRESSURE, pressure+127);
	input_report_key(dev, BTN_LEFT, leftmb);
	input_report_key(dev, BTN_RIGHT, (data[5] & 0x40));
	
	input_event(dev, EV_MSC, MSC_SERIAL, leftmb);
}

static void wacom_graphire_irq(struct urb *urb)
{
	struct wacom *wacom = urb->context;
	unsigned char *data = wacom->data;
	struct input_dev *dev = &wacom->dev;
	int x, y;

	if (urb->status) return;

	if (data[0] != 2) {
		printk(KERN_ERR "wacom_graphire_irq: received unknown report #%d\n", data[0]);
		return;
	}
	
	x = data[2] | ((__u32)data[3] << 8);
	y = data[4] | ((__u32)data[5] << 8);

	switch ((data[1] >> 5) & 3) {

		case 0:	/* Pen */
			input_report_key(dev, BTN_TOOL_PEN, data[1] & 0x80);
			break;

		case 1: /* Rubber */
			input_report_key(dev, BTN_TOOL_RUBBER, data[1] & 0x80);
			break;

		case 2: /* Mouse */
			input_report_key(dev, BTN_TOOL_MOUSE, data[7] > 24);
			input_report_key(dev, BTN_LEFT, data[1] & 0x01);
			input_report_key(dev, BTN_RIGHT, data[1] & 0x02);
			input_report_key(dev, BTN_MIDDLE, data[1] & 0x04);
			input_report_abs(dev, ABS_DISTANCE, data[7]);
			input_report_rel(dev, REL_WHEEL, (signed char) data[6]);

			input_report_abs(dev, ABS_X, x);
			input_report_abs(dev, ABS_Y, y);

			input_event(dev, EV_MSC, MSC_SERIAL, data[1] & 0x01);
			return;
	}

	if (data[1] & 0x80) {
		input_report_abs(dev, ABS_X, x);
		input_report_abs(dev, ABS_Y, y);
	}

	input_report_abs(dev, ABS_PRESSURE, data[6] | ((__u32)data[7] << 8));
	input_report_key(dev, BTN_TOUCH, data[1] & 0x01);
	input_report_key(dev, BTN_STYLUS, data[1] & 0x02);
	input_report_key(dev, BTN_STYLUS2, data[1] & 0x04);

	input_event(dev, EV_MSC, MSC_SERIAL, data[1] & 0x01);
}

static void wacom_intuos_irq(struct urb *urb)
{
	struct wacom *wacom = urb->context;
	unsigned char *data = wacom->data;
	struct input_dev *dev = &wacom->dev;
	unsigned int t;
	int idx;

	if (urb->status) return;

	if (data[0] != 2) {
		printk(KERN_ERR "wacom_intuos_irq: received unknown report #%d\n", data[0]);
		return;
	}
	
	/* tool number */
	idx = data[1] & 0x01;

	if ((data[1] & 0xfc) == 0xc0) {						/* Enter report */

		wacom->serial[idx] = ((__u32)(data[3] & 0x0f) << 4) +		/* serial number of the tool */
			((__u32)data[4] << 16) + ((__u32)data[5] << 12) +
			((__u32)data[6] << 4) + (data[7] >> 4);

		switch (((__u32)data[2] << 4) | (data[3] >> 4)) {
			case 0x832:
			case 0x012: wacom->tool[idx] = BTN_TOOL_PENCIL;		break;	/* Inking pen */
			case 0x822:
		        case 0x852:
			case 0x022: wacom->tool[idx] = BTN_TOOL_PEN;		break;	/* Pen */
			case 0x812:
			case 0x032: wacom->tool[idx] = BTN_TOOL_BRUSH;		break;	/* Stroke pen */
		        case 0x09c:
		        case 0x007:
			case 0x094: wacom->tool[idx] = BTN_TOOL_MOUSE;		break;	/* Mouse 4D */
			case 0x096: wacom->tool[idx] = BTN_TOOL_LENS;		break;	/* Lens cursor */
			case 0x82a:
		        case 0x85a:
		        case 0x91a:
			case 0x0fa: wacom->tool[idx] = BTN_TOOL_RUBBER;		break;	/* Eraser */
			case 0x112: wacom->tool[idx] = BTN_TOOL_AIRBRUSH;	break;	/* Airbrush */
			default:    wacom->tool[idx] = BTN_TOOL_PEN;		break;	/* Unknown tool */
		}

		input_report_key(dev, wacom->tool[idx], 1);
		input_event(dev, EV_MSC, MSC_SERIAL, wacom->serial[idx]);
		return;
	}

	if ((data[1] & 0xfe) == 0x80) {						/* Exit report */
		input_report_key(dev, wacom->tool[idx], 0);
		input_event(dev, EV_MSC, MSC_SERIAL, wacom->serial[idx]);
		return;
	}

	input_report_abs(dev, ABS_X, ((__u32)data[2] << 8) | data[3]);
	input_report_abs(dev, ABS_Y, ((__u32)data[4] << 8) | data[5]);
	input_report_abs(dev, ABS_DISTANCE, data[9] >> 4);
	
	if ((data[1] & 0xb8) == 0xa0) {						/* general pen packet */
		input_report_abs(dev, ABS_PRESSURE, t = ((__u32)data[6] << 2) | ((data[7] >> 6) & 3));
		input_report_abs(dev, ABS_TILT_X, ((data[7] << 1) & 0x7e) | (data[8] >> 7));
		input_report_abs(dev, ABS_TILT_Y, data[8] & 0x7f);
		input_report_key(dev, BTN_STYLUS, data[1] & 2);
		input_report_key(dev, BTN_STYLUS2, data[1] & 4);
		input_report_key(dev, BTN_TOUCH, t > 10);
	}

	if ((data[1] & 0xbc) == 0xb4) {						/* airbrush second packet */
		input_report_abs(dev, ABS_WHEEL, ((__u32)data[6] << 2) | ((data[7] >> 6) & 3));
		input_report_abs(dev, ABS_TILT_X, ((data[7] << 1) & 0x7e) | (data[8] >> 7));
		input_report_abs(dev, ABS_TILT_Y, data[8] & 0x7f);
	}
	
	if ((data[1] & 0xbc) == 0xa8 || (data[1] & 0xbe) == 0xb0) {		/* 4D mouse or Lens cursor packets */

		if (data[1] & 0x02) {						/* Rotation packet */

			input_report_abs(dev, ABS_RZ, (data[7] & 0x20) ?
					 ((__u32)data[6] << 2) | ((data[7] >> 6) & 3):
					 (-(((__u32)data[6] << 2) | ((data[7] >> 6) & 3))) - 1);

		} else {

			input_report_key(dev, BTN_LEFT,   data[8] & 0x01);
			input_report_key(dev, BTN_MIDDLE, data[8] & 0x02);
			input_report_key(dev, BTN_RIGHT,  data[8] & 0x04);

	 		if ((data[1] & 0x10) == 0) {				/* 4D mouse packets */

				input_report_key(dev, BTN_SIDE,   data[8] & 0x20);
				input_report_key(dev, BTN_EXTRA,  data[8] & 0x10);
				input_report_abs(dev, ABS_THROTTLE,  (data[8] & 0x08) ?
						 ((__u32)data[6] << 2) | ((data[7] >> 6) & 3) :
						 -((__u32)data[6] << 2) | ((data[7] >> 6) & 3));

			} else {						/* Lens cursor packets */

				input_report_key(dev, BTN_SIDE,   data[8] & 0x10);
				input_report_key(dev, BTN_EXTRA,  data[8] & 0x08);
			}
		}
	}
	
	input_event(dev, EV_MSC, MSC_SERIAL, wacom->serial[idx]);
}

#define WACOM_INTUOS_TOOLS	(BIT(BTN_TOOL_BRUSH) | BIT(BTN_TOOL_PENCIL) | BIT(BTN_TOOL_AIRBRUSH) | BIT(BTN_TOOL_LENS))
#define WACOM_INTUOS_BUTTONS	(BIT(BTN_SIDE) | BIT(BTN_EXTRA))
#define WACOM_INTUOS_ABS	(BIT(ABS_TILT_X) | BIT(ABS_TILT_Y) | BIT(ABS_RZ) | BIT(ABS_THROTTLE))

struct wacom_features wacom_features[] = {
	{ "Wacom Penpartner",	 7,  5040,  3780,  255, 32, wacom_penpartner_irq,
		0, 0, 0, 0 },
	{ "Wacom Graphire",      8, 10206,  7422,  511, 32, wacom_graphire_irq,
		BIT(EV_REL), 0, BIT(REL_WHEEL), 0 },
	{ "Wacom Graphire2 4x5",     8, 10206,  7422,  511, 32, wacom_graphire_irq,
		BIT(EV_REL), 0, BIT(REL_WHEEL), 0 },
	{ "Wacom Graphire2 5x7",     8, 10206,  7422,  511, 32, wacom_graphire_irq,
		BIT(EV_REL), 0, BIT(REL_WHEEL), 0 },
	{ "Wacom Intuos 4x5",   10, 12700, 10360, 1023, 15, wacom_intuos_irq,
		0, WACOM_INTUOS_ABS, 0, WACOM_INTUOS_BUTTONS, WACOM_INTUOS_TOOLS },
	{ "Wacom Intuos 6x8",   10, 20320, 15040, 1023, 15, wacom_intuos_irq,
		0, WACOM_INTUOS_ABS, 0, WACOM_INTUOS_BUTTONS, WACOM_INTUOS_TOOLS },
	{ "Wacom Intuos 9x12",  10, 30480, 23060, 1023, 15, wacom_intuos_irq,
		0, WACOM_INTUOS_ABS, 0, WACOM_INTUOS_BUTTONS, WACOM_INTUOS_TOOLS },
	{ "Wacom Intuos 12x12", 10, 30480, 30480, 1023, 15, wacom_intuos_irq,
		0, WACOM_INTUOS_ABS, 0, WACOM_INTUOS_BUTTONS, WACOM_INTUOS_TOOLS },
	{ "Wacom Intuos 12x18", 10, 47720, 30480, 1023, 15, wacom_intuos_irq,
		0, WACOM_INTUOS_ABS, 0, WACOM_INTUOS_BUTTONS, WACOM_INTUOS_TOOLS },
	{ "Wacom PL400",        8,  12328, 9256,   511, 32, wacom_pl_irq,
		0,  0, 0, 0 },
	{ "Wacom PL500",        8,  12328, 9256,   511, 32, wacom_pl_irq,
		0,  0, 0, 0 },
	{ "Wacom PL600",        8,  12328, 9256,   511, 32, wacom_pl_irq,
		0,  0, 0, 0 },
	{ "Wacom PL600SX",        8,  12328, 9256,   511, 32, wacom_pl_irq,
		0,  0, 0, 0 },
	{ "Wacom PL550",        8,  12328, 9256,   511, 32, wacom_pl_irq,
		0,  0, 0, 0 },
	{ "Wacom PL800",        8,  12328, 9256,   511, 32, wacom_pl_irq,
		0,  0, 0, 0 },
	{ "Wacom Intuos2 4x5",   10, 12700, 10360, 1023, 15, wacom_intuos_irq,
		0, WACOM_INTUOS_ABS, 0, WACOM_INTUOS_BUTTONS, WACOM_INTUOS_TOOLS },
	{ "Wacom Intuos2 6x8",   10, 20320, 15040, 1023, 15, wacom_intuos_irq,
		0, WACOM_INTUOS_ABS, 0, WACOM_INTUOS_BUTTONS, WACOM_INTUOS_TOOLS },
	{ "Wacom Intuos2 9x12",  10, 30480, 23060, 1023, 15, wacom_intuos_irq,
		0, WACOM_INTUOS_ABS, 0, WACOM_INTUOS_BUTTONS, WACOM_INTUOS_TOOLS },
	{ "Wacom Intuos2 12x12", 10, 30480, 30480, 1023, 15, wacom_intuos_irq,
		0, WACOM_INTUOS_ABS, 0, WACOM_INTUOS_BUTTONS, WACOM_INTUOS_TOOLS },
	{ "Wacom Intuos2 12x18", 10, 47720, 30480, 1023, 15, wacom_intuos_irq,
		0, WACOM_INTUOS_ABS, 0, WACOM_INTUOS_BUTTONS, WACOM_INTUOS_TOOLS },
	{ NULL , 0 }
};

struct usb_device_id wacom_ids[] = {
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x00), driver_info: 0 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x10), driver_info: 1 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x11), driver_info: 2 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x12), driver_info: 3 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x20), driver_info: 4 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x21), driver_info: 5 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x22), driver_info: 6 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x23), driver_info: 7 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x24), driver_info: 8 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x30), driver_info: 9 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x31), driver_info: 10 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x32), driver_info: 11 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x33), driver_info: 12 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x34), driver_info: 13 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x35), driver_info: 14 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x41), driver_info: 15 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x42), driver_info: 16 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x43), driver_info: 17 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x44), driver_info: 18 },
	{ USB_DEVICE(USB_VENDOR_ID_WACOM, 0x45), driver_info: 19 },
	{ }
};

MODULE_DEVICE_TABLE(usb, wacom_ids);

static int wacom_open(struct input_dev *dev)
{
	struct wacom *wacom = dev->private;
	
	if (wacom->open++)
		return 0;

	wacom->irq.dev = wacom->usbdev;
	if (usb_submit_urb(&wacom->irq))
		return -EIO;

	return 0;
}

static void wacom_close(struct input_dev *dev)
{
	struct wacom *wacom = dev->private;

	if (!--wacom->open)
		usb_unlink_urb(&wacom->irq);
}

static void *wacom_probe(struct usb_device *dev, unsigned int ifnum, const struct usb_device_id *id)
{
	struct usb_endpoint_descriptor *endpoint;
	struct wacom *wacom;
	char rep_data[2] = {0x02, 0x02};
	
	if (!(wacom = kmalloc(sizeof(struct wacom), GFP_KERNEL))) return NULL;
	memset(wacom, 0, sizeof(struct wacom));

	wacom->features = wacom_features + id->driver_info;

	wacom->dev.evbit[0] |= BIT(EV_KEY) | BIT(EV_ABS) | BIT(EV_MSC) | wacom->features->evbit;
	wacom->dev.absbit[0] |= BIT(ABS_X) | BIT(ABS_Y) | BIT(ABS_PRESSURE) | BIT(ABS_DISTANCE) | BIT(ABS_WHEEL) | wacom->features->absbit;
	wacom->dev.relbit[0] |= wacom->features->relbit;
	wacom->dev.keybit[LONG(BTN_LEFT)] |= BIT(BTN_LEFT) | BIT(BTN_RIGHT) | BIT(BTN_MIDDLE) | wacom->features->btnbit;
	wacom->dev.keybit[LONG(BTN_DIGI)] |= BIT(BTN_TOOL_PEN) | BIT(BTN_TOOL_RUBBER) | BIT(BTN_TOOL_MOUSE) |
		BIT(BTN_TOUCH) | BIT(BTN_STYLUS) | BIT(BTN_STYLUS2) | wacom->features->digibit;
	wacom->dev.mscbit[0] |= BIT(MSC_SERIAL);

	wacom->dev.absmax[ABS_X] = wacom->features->x_max;
	wacom->dev.absmax[ABS_Y] = wacom->features->y_max;
	wacom->dev.absmax[ABS_PRESSURE] = wacom->features->pressure_max;
	wacom->dev.absmax[ABS_DISTANCE] = wacom->features->distance_max;
	wacom->dev.absmax[ABS_TILT_X] = 127;
	wacom->dev.absmax[ABS_TILT_Y] = 127;
	wacom->dev.absmax[ABS_WHEEL] = 1023;

	wacom->dev.absmin[ABS_RZ] = -900;
	wacom->dev.absmax[ABS_RZ] = 899;
	wacom->dev.absmin[ABS_THROTTLE] = -1023;
	wacom->dev.absmax[ABS_THROTTLE] = 1023;

	wacom->dev.absfuzz[ABS_X] = 4;
	wacom->dev.absfuzz[ABS_Y] = 4;

	wacom->dev.private = wacom;
	wacom->dev.open = wacom_open;
	wacom->dev.close = wacom_close;

	wacom->dev.name = wacom->features->name;
	wacom->dev.idbus = BUS_USB;
	wacom->dev.idvendor = dev->descriptor.idVendor;
	wacom->dev.idproduct = dev->descriptor.idProduct;
	wacom->dev.idversion = dev->descriptor.bcdDevice;
	wacom->usbdev = dev;

	endpoint = dev->config[0].interface[ifnum].altsetting[0].endpoint + 0;

	usb_set_idle(dev, dev->config[0].interface[ifnum].altsetting[0].bInterfaceNumber, 0, 0);

	FILL_INT_URB(&wacom->irq, dev, usb_rcvintpipe(dev, endpoint->bEndpointAddress),
		     wacom->data, wacom->features->pktlen, wacom->features->irq, wacom, endpoint->bInterval);

	input_register_device(&wacom->dev);

	/* ask the tablet to report tablet data */
	usb_set_report(dev, ifnum, 3, 2, rep_data, 2);
	usb_set_report(dev, ifnum, 3, 5, rep_data, 0);
	usb_set_report(dev, ifnum, 3, 6, rep_data, 0);
	
	printk(KERN_INFO "input%d: %s on usb%d:%d.%d\n",
	       wacom->dev.number, wacom->features->name, dev->bus->busnum, dev->devnum, ifnum);

	return wacom;
}

static void wacom_disconnect(struct usb_device *dev, void *ptr)
{
	struct wacom *wacom = ptr;
	usb_unlink_urb(&wacom->irq);
	input_unregister_device(&wacom->dev);
	kfree(wacom);
}

static struct usb_driver wacom_driver = {
	name:		"wacom",
	probe:		wacom_probe,
	disconnect:	wacom_disconnect,
	id_table:	wacom_ids,
};

static int __init wacom_init(void)
{
	usb_register(&wacom_driver);
	info(DRIVER_VERSION " " DRIVER_AUTHOR);
	info(DRIVER_DESC);
	return 0;
}

static void __exit wacom_exit(void)
{
	usb_deregister(&wacom_driver);
}

module_init(wacom_init);
module_exit(wacom_exit);
