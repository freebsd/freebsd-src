/*
 * $Id: cobra.c,v 1.10 2000/06/08 10:23:45 vojtech Exp $
 *
 *  Copyright (c) 1999-2000 Vojtech Pavlik
 *
 *  Sponsored by SuSE
 */

/*
 * Creative Labd Blaster GamePad Cobra driver for Linux
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
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/gameport.h>
#include <linux/input.h>

#define COBRA_MAX_STROBE	45	/* 45 us max wait for first strobe */
#define COBRA_REFRESH_TIME	HZ/50	/* 20 ms between reads */
#define COBRA_LENGTH		36

static char* cobra_name = "Creative Labs Blaster GamePad Cobra";

static int cobra_btn[] = { BTN_START, BTN_SELECT, BTN_TL, BTN_TR, BTN_X, BTN_Y, BTN_Z, BTN_A, BTN_B, BTN_C, BTN_TL2, BTN_TR2, 0 };

struct cobra {
	struct gameport *gameport;
	struct timer_list timer;
	struct input_dev dev[2];
	int used;
	int reads;
	int bads;
	unsigned char exists;
};

static unsigned char cobra_read_packet(struct gameport *gameport, unsigned int *data)
{
	unsigned long flags;
	unsigned char u, v, w;
	__u64 buf[2];
	int r[2], t[2];
	int i, j, ret;

	int strobe = gameport_time(gameport, COBRA_MAX_STROBE);

	for (i = 0; i < 2; i++) {
		r[i] = buf[i] = 0;
		t[i] = COBRA_MAX_STROBE;
	}
	
	__save_flags(flags);
	__cli();

	u = gameport_read(gameport);

	do {
		t[0]--; t[1]--;
		v = gameport_read(gameport);
		for (i = 0, w = u ^ v; i < 2 && w; i++, w >>= 2)
			if (w & 0x30) {
				if ((w & 0x30) < 0x30 && r[i] < COBRA_LENGTH && t[i] > 0) {
					buf[i] |= (__u64)((w >> 5) & 1) << r[i]++;
					t[i] = strobe;
					u = v;
				} else t[i] = 0;
			}
	} while (t[0] > 0 || t[1] > 0);

	__restore_flags(flags);

	ret = 0;

	for (i = 0; i < 2; i++) {

		if (r[i] != COBRA_LENGTH) continue;

		for (j = 0; j < COBRA_LENGTH && (buf[i] & 0x04104107f) ^ 0x041041040; j++)
			buf[i] = (buf[i] >> 1) | ((__u64)(buf[i] & 1) << (COBRA_LENGTH - 1));

		if (j < COBRA_LENGTH) ret |= (1 << i);

		data[i] = ((buf[i] >>  7) & 0x000001f) | ((buf[i] >>  8) & 0x00003e0)
			| ((buf[i] >>  9) & 0x0007c00) | ((buf[i] >> 10) & 0x00f8000)
			| ((buf[i] >> 11) & 0x1f00000);

	}

	return ret;
}

static void cobra_timer(unsigned long private)
{
	struct cobra *cobra = (void *) private;
	struct input_dev *dev;
	unsigned int data[2];
	int i, j, r;

	cobra->reads++;

	if ((r = cobra_read_packet(cobra->gameport, data)) != cobra->exists)
		cobra->bads++;

	for (i = 0; i < 2; i++)
		if (cobra->exists & r & (1 << i)) {

			dev = cobra->dev + i;

			input_report_abs(dev, ABS_X, ((data[i] >> 4) & 1) - ((data[i] >> 3) & 1));
			input_report_abs(dev, ABS_Y, ((data[i] >> 2) & 1) - ((data[i] >> 1) & 1));

			for (j = 0; cobra_btn[j]; j++)
				input_report_key(dev, cobra_btn[j], data[i] & (0x20 << j));

		}

	mod_timer(&cobra->timer, jiffies + COBRA_REFRESH_TIME);	
}

static int cobra_open(struct input_dev *dev)
{
	struct cobra *cobra = dev->private;
	if (!cobra->used++)
		mod_timer(&cobra->timer, jiffies + COBRA_REFRESH_TIME);	
	return 0;
}

static void cobra_close(struct input_dev *dev)
{
	struct cobra *cobra = dev->private;
	if (!--cobra->used)
		del_timer(&cobra->timer);
}

static void cobra_connect(struct gameport *gameport, struct gameport_dev *dev)
{
	struct cobra *cobra;
	unsigned int data[2];
	int i, j;

	if (!(cobra = kmalloc(sizeof(struct cobra), GFP_KERNEL)))
		return;
	memset(cobra, 0, sizeof(struct cobra));

	gameport->private = cobra;

	cobra->gameport = gameport;
	init_timer(&cobra->timer);
	cobra->timer.data = (long) cobra;
	cobra->timer.function = cobra_timer;

	if (gameport_open(gameport, dev, GAMEPORT_MODE_RAW))
		goto fail1;

	cobra->exists = cobra_read_packet(gameport, data);

	for (i = 0; i < 2; i++) 
		if ((cobra->exists >> i) & data[i] & 1) {
			printk(KERN_WARNING "cobra.c: Device on gameport%d.%d has the Ext bit set. ID is: %d"
				" Contact vojtech@suse.cz\n", gameport->number, i, (data[i] >> 2) & 7);
			cobra->exists &= ~(1 << i);
		}

	if (!cobra->exists)
		goto fail2;

	for (i = 0; i < 2; i++)
		if ((cobra->exists >> i) & 1) {

			cobra->dev[i].private = cobra;
			cobra->dev[i].open = cobra_open;
			cobra->dev[i].close = cobra_close;

			cobra->dev[i].name = cobra_name;
			cobra->dev[i].idbus = BUS_GAMEPORT;
			cobra->dev[i].idvendor = GAMEPORT_ID_VENDOR_CREATIVE;
			cobra->dev[i].idproduct = 0x0008;
			cobra->dev[i].idversion = 0x0100;
		
			cobra->dev[i].evbit[0] = BIT(EV_KEY) | BIT(EV_ABS);
			cobra->dev[i].absbit[0] = BIT(ABS_X) | BIT(ABS_Y);

			for (j = 0; cobra_btn[j]; j++)
				set_bit(cobra_btn[j], cobra->dev[i].keybit);

			cobra->dev[i].absmin[ABS_X] = -1; cobra->dev[i].absmax[ABS_X] = 1;
			cobra->dev[i].absmin[ABS_Y] = -1; cobra->dev[i].absmax[ABS_Y] = 1;

			input_register_device(cobra->dev + i);
			printk(KERN_INFO "input%d: %s on gameport%d.%d\n",
				cobra->dev[i].number, cobra_name, gameport->number, i);
		}


	return;
fail2:	gameport_close(gameport);
fail1:	kfree(cobra);
}

static void cobra_disconnect(struct gameport *gameport)
{
	int i;

	struct cobra *cobra = gameport->private;
	for (i = 0; i < 2; i++)
		if ((cobra->exists >> i) & 1)
			input_unregister_device(cobra->dev + i);
	gameport_close(gameport);
	kfree(cobra);
}

static struct gameport_dev cobra_dev = {
	connect:	cobra_connect,
	disconnect:	cobra_disconnect,
};

int __init cobra_init(void)
{
	gameport_register_device(&cobra_dev);
	return 0;
}

void __exit cobra_exit(void)
{
	gameport_unregister_device(&cobra_dev);
}

module_init(cobra_init);
module_exit(cobra_exit);

MODULE_LICENSE("GPL");
