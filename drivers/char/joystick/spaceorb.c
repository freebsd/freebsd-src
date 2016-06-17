/*
 * $Id: spaceorb.c,v 1.7 2000/05/29 11:19:51 vojtech Exp $
 *
 *  Copyright (c) 1999-2000 Vojtech Pavlik
 * 
 *  Based on the work of:
 *  	David Thompson
 *
 *  Sponsored by SuSE
 */

/*
 * SpaceTec SpaceOrb 360 and Avenger 6dof controller driver for Linux
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
 *  Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <vojtech@suse.cz>, or by paper mail:
 * Vojtech Pavlik, Ucitelska 1576, Prague 8, 182 00 Czech Republic
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/serio.h>

/*
 * Constants.
 */

#define SPACEORB_MAX_LENGTH	64

static int spaceorb_buttons[] = { BTN_TL, BTN_TR, BTN_Y, BTN_X, BTN_B, BTN_A, BTN_MODE};
static int spaceorb_axes[] = { ABS_X, ABS_Y, ABS_Z, ABS_RX, ABS_RY, ABS_RZ};
static char *spaceorb_name = "SpaceTec SpaceOrb 360";

/*
 * Per-Orb data.
 */

struct spaceorb {
	struct input_dev dev;
	struct serio *serio;
	int idx;
	unsigned char data[SPACEORB_MAX_LENGTH];
};

static unsigned char spaceorb_xor[] = "SpaceWare";

static unsigned char *spaceorb_errors[] = { "EEPROM storing 0 failed", "Receive queue overflow", "Transmit queue timeout",
		"Bad packet", "Power brown-out", "EEPROM checksum error", "Hardware fault" }; 

/*
 * spaceorb_process_packet() decodes packets the driver receives from the
 * SpaceOrb.
 */

static void spaceorb_process_packet(struct spaceorb *spaceorb)
{
	struct input_dev *dev = &spaceorb->dev;
	unsigned char *data = spaceorb->data;
	unsigned char c = 0;
	int axes[6];
	int i;

	if (spaceorb->idx < 2) return;
	for (i = 0; i < spaceorb->idx; i++) c ^= data[i];
	if (c) return;

	switch (data[0]) {

		case 'R':				/* Reset packet */
			spaceorb->data[spaceorb->idx - 1] = 0;
			for (i = 1; i < spaceorb->idx && spaceorb->data[i] == ' '; i++);
			printk(KERN_INFO "input%d: %s [%s] on serio%d\n",
				 spaceorb->dev.number, spaceorb_name, spaceorb->data + i, spaceorb->serio->number);
			break;

		case 'D':				/* Ball + button data */
			if (spaceorb->idx != 12) return;
			for (i = 0; i < 9; i++) spaceorb->data[i+2] ^= spaceorb_xor[i]; 
			axes[0] = ( data[2]	 << 3) | (data[ 3] >> 4);
			axes[1] = ((data[3] & 0x0f) << 6) | (data[ 4] >> 1);
			axes[2] = ((data[4] & 0x01) << 9) | (data[ 5] << 2) | (data[4] >> 5);
			axes[3] = ((data[6] & 0x1f) << 5) | (data[ 7] >> 2);
			axes[4] = ((data[7] & 0x03) << 8) | (data[ 8] << 1) | (data[7] >> 6);
			axes[5] = ((data[9] & 0x3f) << 4) | (data[10] >> 3);
			for (i = 0; i < 6; i++)
				input_report_abs(dev, spaceorb_axes[i], axes[i] - ((axes[i] & 0x200) ? 1024 : 0));
			for (i = 0; i < 8; i++)
				input_report_key(dev, spaceorb_buttons[i], (data[1] >> i) & 1);
			break;

		case 'K':				/* Button data */
			if (spaceorb->idx != 5) return;
			for (i = 0; i < 7; i++)
				input_report_key(dev, spaceorb_buttons[i], (data[2] >> i) & 1);

			break;

		case 'E':				/* Error packet */
			if (spaceorb->idx != 4) return;
			printk(KERN_ERR "joy-spaceorb: Device error. [ ");
			for (i = 0; i < 7; i++) if (data[1] & (1 << i)) printk("%s ", spaceorb_errors[i]);
			printk("]\n");
			break;
	}
}

static void spaceorb_interrupt(struct serio *serio, unsigned char data, unsigned int flags)
{
	struct spaceorb* spaceorb = serio->private;

	if (~data & 0x80) {
		if (spaceorb->idx) spaceorb_process_packet(spaceorb);
		spaceorb->idx = 0;
	}
	if (spaceorb->idx < SPACEORB_MAX_LENGTH)
		spaceorb->data[spaceorb->idx++] = data & 0x7f;
}

/*
 * spaceorb_disconnect() is the opposite of spaceorb_connect()
 */

static void spaceorb_disconnect(struct serio *serio)
{
	struct spaceorb* spaceorb = serio->private;
	input_unregister_device(&spaceorb->dev);
	serio_close(serio);
	kfree(spaceorb);
}

/*
 * spaceorb_connect() is the routine that is called when someone adds a
 * new serio device. It looks for the SpaceOrb/Avenger, and if found, registers
 * it as an input device.
 */

static void spaceorb_connect(struct serio *serio, struct serio_dev *dev)
{
	struct spaceorb *spaceorb;
	int i, t;

	if (serio->type != (SERIO_RS232 | SERIO_SPACEORB))
		return;

	if (!(spaceorb = kmalloc(sizeof(struct spaceorb), GFP_KERNEL)))
		return;
	memset(spaceorb, 0, sizeof(struct spaceorb));

	spaceorb->dev.evbit[0] = BIT(EV_KEY) | BIT(EV_ABS);	

	for (i = 0; i < 7; i++)
		set_bit(spaceorb_buttons[i], &spaceorb->dev.keybit);

	for (i = 0; i < 6; i++) {
		t = spaceorb_axes[i];
		set_bit(t, spaceorb->dev.absbit);
		spaceorb->dev.absmin[t] = -508;
		spaceorb->dev.absmax[t] =  508;
	}

	spaceorb->serio = serio;
	spaceorb->dev.private = spaceorb;

	spaceorb->dev.name = spaceorb_name;
	spaceorb->dev.idbus = BUS_RS232;
	spaceorb->dev.idvendor = SERIO_SPACEORB;
	spaceorb->dev.idproduct = 0x0001;
	spaceorb->dev.idversion = 0x0100;
	
	serio->private = spaceorb;

	if (serio_open(serio, dev)) {
		kfree(spaceorb);
		return;
	}

	input_register_device(&spaceorb->dev);
}

/*
 * The serio device structure.
 */

static struct serio_dev spaceorb_dev = {
	interrupt:	spaceorb_interrupt,
	connect:	spaceorb_connect,
	disconnect:	spaceorb_disconnect,
};

/*
 * The functions for inserting/removing us as a module.
 */

int __init spaceorb_init(void)
{
	serio_register_device(&spaceorb_dev);
	return 0;
}

void __exit spaceorb_exit(void)
{
	serio_unregister_device(&spaceorb_dev);
}

module_init(spaceorb_init);
module_exit(spaceorb_exit);

MODULE_LICENSE("GPL");
