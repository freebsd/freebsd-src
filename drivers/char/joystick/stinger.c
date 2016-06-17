/*
 * $Id: stinger.c,v 1.4 2001/05/23 09:25:02 vojtech Exp $
 *
 *  Copyright (c) 2000-2001 Vojtech Pavlik
 *  Copyright (c) 2000 Mark Fletcher
 *
 *  Sponsored by SuSE
 */

/*
 * Gravis Stinger gamepad driver for Linux
 */

/*
 * This program is free warftware; you can redistribute it and/or modify
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
 *  Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <vojtech@ucw.cz>, or by paper mail:
 * Vojtech Pavlik, Ucitelska 1576, Prague 8, 182 00 Czech Republic
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/serio.h>
#include <linux/init.h>

/*
 * Constants.
 */

#define STINGER_MAX_LENGTH 8

static char *stinger_name = "Gravis Stinger";

/*
 * Per-Stinger data.
 */

struct stinger {
	struct input_dev dev;
	int idx;
	unsigned char data[STINGER_MAX_LENGTH];
};

/*
 * stinger_process_packet() decodes packets the driver receives from the
 * Stinger. It updates the data accordingly.
 */

static void stinger_process_packet(struct stinger *stinger)
{
	struct input_dev *dev = &stinger->dev;
	unsigned char *data = stinger->data;

	if (!stinger->idx) return;

	input_report_key(dev, BTN_A,	  ((data[0] & 0x20) >> 5));
	input_report_key(dev, BTN_B,	  ((data[0] & 0x10) >> 4));
	input_report_key(dev, BTN_C,	  ((data[0] & 0x08) >> 3));
	input_report_key(dev, BTN_X,	  ((data[0] & 0x04) >> 2));
	input_report_key(dev, BTN_Y,	  ((data[3] & 0x20) >> 5));
	input_report_key(dev, BTN_Z,	  ((data[3] & 0x10) >> 4));
	input_report_key(dev, BTN_TL,     ((data[3] & 0x08) >> 3));
	input_report_key(dev, BTN_TR,     ((data[3] & 0x04) >> 2));
	input_report_key(dev, BTN_SELECT, ((data[3] & 0x02) >> 1));
	input_report_key(dev, BTN_START,   (data[3] & 0x01));

	input_report_abs(dev, ABS_X, (data[1] & 0x3F) - ((data[0] & 0x01) << 6));
	input_report_abs(dev, ABS_Y, ((data[0] & 0x02) << 5) - (data[2] & 0x3F));

	return;
}

/*
 * stinger_interrupt() is called by the low level driver when characters
 * are ready for us. We then buffer them for further processing, or call the
 * packet processing routine.
 */

static void stinger_interrupt(struct serio *serio, unsigned char data, unsigned int flags)
{
	struct stinger* stinger = serio->private;

	/* All Stinger packets are 4 bytes */

	if (stinger->idx < STINGER_MAX_LENGTH)
		stinger->data[stinger->idx++] = data;

	if (stinger->idx == 4) {
		stinger_process_packet(stinger);
		stinger->idx = 0;
	}

	return;
}

/*
 * stinger_disconnect() is the opposite of stinger_connect()
 */

static void stinger_disconnect(struct serio *serio)
{
	struct stinger* stinger = serio->private;
	input_unregister_device(&stinger->dev);
	serio_close(serio);
	kfree(stinger);
}

/*
 * stinger_connect() is the routine that is called when someone adds a
 * new serio device. It looks for the Stinger, and if found, registers
 * it as an input device.
 */

static void stinger_connect(struct serio *serio, struct serio_dev *dev)
{
	struct stinger *stinger;
	int i;

	if (serio->type != (SERIO_RS232 | SERIO_STINGER))
		return;

	if (!(stinger = kmalloc(sizeof(struct stinger), GFP_KERNEL)))
		return;

	memset(stinger, 0, sizeof(struct stinger));

	stinger->dev.evbit[0] = BIT(EV_KEY) | BIT(EV_ABS);	
	stinger->dev.keybit[LONG(BTN_A)] = BIT(BTN_A) | BIT(BTN_B) | BIT(BTN_C) | BIT(BTN_X) | \
					   BIT(BTN_Y) | BIT(BTN_Z) | BIT(BTN_TL) | BIT(BTN_TR) | \
					   BIT(BTN_START) | BIT(BTN_SELECT);
	stinger->dev.absbit[0] = BIT(ABS_X) | BIT(ABS_Y);

	stinger->dev.name = stinger_name;
	stinger->dev.idbus = BUS_RS232;
	stinger->dev.idvendor = SERIO_STINGER;
	stinger->dev.idproduct = 0x0001;
	stinger->dev.idversion = 0x0100;

	for (i = 0; i < 2; i++) {
		stinger->dev.absmax[ABS_X+i] =  64;	
		stinger->dev.absmin[ABS_X+i] = -64;	
		stinger->dev.absflat[ABS_X+i] = 4;
	}

	stinger->dev.private = stinger;
	
	serio->private = stinger;

	if (serio_open(serio, dev)) {
		kfree(stinger);
		return;
	}

	input_register_device(&stinger->dev);

	printk(KERN_INFO "input%d: %s on serio%d\n", stinger->dev.number, stinger_name, serio->number);
}

/*
 * The serio device structure.
 */

static struct serio_dev stinger_dev = {
	interrupt:	stinger_interrupt,
	connect:	stinger_connect,
	disconnect:	stinger_disconnect,
};

/*
 * The functions for inserting/removing us as a module.
 */

int __init stinger_init(void)
{
	serio_register_device(&stinger_dev);
	return 0;
}

void __exit stinger_exit(void)
{
	serio_unregister_device(&stinger_dev);
}

module_init(stinger_init);
module_exit(stinger_exit);

MODULE_LICENSE("GPL");
