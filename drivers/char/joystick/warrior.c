/*
 * $Id: warrior.c,v 1.8 2000/05/31 13:17:12 vojtech Exp $
 *
 *  Copyright (c) 1999-2000 Vojtech Pavlik
 *
 *  Sponsored by SuSE
 */

/*
 * Logitech WingMan Warrior joystick driver for Linux
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

#define WARRIOR_MAX_LENGTH	16
static char warrior_lengths[] = { 0, 4, 12, 3, 4, 4, 0, 0 }; 
static char *warrior_name = "Logitech WingMan Warrior";

/*
 * Per-Warrior data.
 */

struct warrior {
	struct input_dev dev;
	int idx, len;
	unsigned char data[WARRIOR_MAX_LENGTH];
};

/*
 * warrior_process_packet() decodes packets the driver receives from the
 * Warrior. It updates the data accordingly.
 */

static void warrior_process_packet(struct warrior *warrior)
{
	struct input_dev *dev = &warrior->dev;
	unsigned char *data = warrior->data;

	if (!warrior->idx) return;

	switch ((data[0] >> 4) & 7) {
		case 1:					/* Button data */
			input_report_key(dev, BTN_TRIGGER,  data[3]       & 1);
			input_report_key(dev, BTN_THUMB,   (data[3] >> 1) & 1);
			input_report_key(dev, BTN_TOP,     (data[3] >> 2) & 1);
			input_report_key(dev, BTN_TOP2,    (data[3] >> 3) & 1);
			return;
		case 3:					/* XY-axis info->data */
			input_report_abs(dev, ABS_X, ((data[0] & 8) << 5) - (data[2] | ((data[0] & 4) << 5)));
			input_report_abs(dev, ABS_Y, (data[1] | ((data[0] & 1) << 7)) - ((data[0] & 2) << 7));
			return;
		case 5:					/* Throttle, spinner, hat info->data */
			input_report_abs(dev, ABS_THROTTLE, (data[1] | ((data[0] & 1) << 7)) - ((data[0] & 2) << 7));
			input_report_abs(dev, ABS_HAT0X, (data[3] & 2 ? 1 : 0) - (data[3] & 1 ? 1 : 0));
			input_report_abs(dev, ABS_HAT0Y, (data[3] & 8 ? 1 : 0) - (data[3] & 4 ? 1 : 0));
			input_report_rel(dev, REL_DIAL,  (data[2] | ((data[0] & 4) << 5)) - ((data[0] & 8) << 5));
			return;
	}
}

/*
 * warrior_interrupt() is called by the low level driver when characters
 * are ready for us. We then buffer them for further processing, or call the
 * packet processing routine.
 */

static void warrior_interrupt(struct serio *serio, unsigned char data, unsigned int flags)
{
	struct warrior* warrior = serio->private;

	if (data & 0x80) {
		if (warrior->idx) warrior_process_packet(warrior);
		warrior->idx = 0;
		warrior->len = warrior_lengths[(data >> 4) & 7];
	}

	if (warrior->idx < warrior->len)
		warrior->data[warrior->idx++] = data;

	if (warrior->idx == warrior->len) {
		if (warrior->idx) warrior_process_packet(warrior);	
		warrior->idx = 0;
		warrior->len = 0;
	}
}

/*
 * warrior_disconnect() is the opposite of warrior_connect()
 */

static void warrior_disconnect(struct serio *serio)
{
	struct warrior* warrior = serio->private;
	input_unregister_device(&warrior->dev);
	serio_close(serio);
	kfree(warrior);
}

/*
 * warrior_connect() is the routine that is called when someone adds a
 * new serio device. It looks for the Warrior, and if found, registers
 * it as an input device.
 */

static void warrior_connect(struct serio *serio, struct serio_dev *dev)
{
	struct warrior *warrior;
	int i;

	if (serio->type != (SERIO_RS232 | SERIO_WARRIOR))
		return;

	if (!(warrior = kmalloc(sizeof(struct warrior), GFP_KERNEL)))
		return;

	memset(warrior, 0, sizeof(struct warrior));

	warrior->dev.evbit[0] = BIT(EV_KEY) | BIT(EV_REL) | BIT(EV_ABS);	
	warrior->dev.keybit[LONG(BTN_TRIGGER)] = BIT(BTN_TRIGGER) | BIT(BTN_THUMB) | BIT(BTN_TOP) | BIT(BTN_TOP2);
	warrior->dev.relbit[0] = BIT(REL_DIAL);
	warrior->dev.absbit[0] = BIT(ABS_X) | BIT(ABS_Y) | BIT(ABS_THROTTLE) | BIT(ABS_HAT0X) | BIT(ABS_HAT0Y);

	warrior->dev.name = warrior_name;
	warrior->dev.idbus = BUS_RS232;
	warrior->dev.idvendor = SERIO_WARRIOR;
	warrior->dev.idproduct = 0x0001;
	warrior->dev.idversion = 0x0100;

	for (i = 0; i < 2; i++) {
		warrior->dev.absmax[ABS_X+i] = -64;	
		warrior->dev.absmin[ABS_X+i] =  64;	
		warrior->dev.absflat[ABS_X+i] = 8;	
	}

	warrior->dev.absmax[ABS_THROTTLE] = -112;	
	warrior->dev.absmin[ABS_THROTTLE] =  112;	

	for (i = 0; i < 2; i++) {
		warrior->dev.absmax[ABS_HAT0X+i] = -1;	
		warrior->dev.absmin[ABS_HAT0X+i] =  1;	
	}

	warrior->dev.private = warrior;
	
	serio->private = warrior;

	if (serio_open(serio, dev)) {
		kfree(warrior);
		return;
	}

	input_register_device(&warrior->dev);

	printk(KERN_INFO "input%d: Logitech WingMan Warrior on serio%d\n", warrior->dev.number, serio->number);
}

/*
 * The serio device structure.
 */

static struct serio_dev warrior_dev = {
	interrupt:	warrior_interrupt,
	connect:	warrior_connect,
	disconnect:	warrior_disconnect,
};

/*
 * The functions for inserting/removing us as a module.
 */

int __init warrior_init(void)
{
	serio_register_device(&warrior_dev);
	return 0;
}

void __exit warrior_exit(void)
{
	serio_unregister_device(&warrior_dev);
}

module_init(warrior_init);
module_exit(warrior_exit);

MODULE_LICENSE("GPL");
