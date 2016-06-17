/*
 * $Id: grip.c,v 1.14 2000/06/06 21:13:36 vojtech Exp $
 *
 *  Copyright (c) 1998-2000 Vojtech Pavlik
 *
 *  Sponsored by SuSE
 */

/*
 * Gravis/Kensington GrIP protocol joystick and gamepad driver for Linux
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
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/gameport.h>
#include <linux/input.h>

#define GRIP_MODE_GPP		1
#define GRIP_MODE_BD		2
#define GRIP_MODE_XT		3
#define GRIP_MODE_DC		4

#define GRIP_LENGTH_GPP		24
#define GRIP_STROBE_GPP		200	/* 200 us */
#define GRIP_LENGTH_XT		4
#define GRIP_STROBE_XT		64	/* 64 us */
#define GRIP_MAX_CHUNKS_XT	10	
#define GRIP_MAX_BITS_XT	30	

#define GRIP_REFRESH_TIME	HZ/50	/* 20 ms */

struct grip {
	struct gameport *gameport;
	struct timer_list timer;
	struct input_dev dev[2];
	unsigned char mode[2];
	int used;
	int reads;
	int bads;
};

static int grip_btn_gpp[] = { BTN_START, BTN_SELECT, BTN_TR2, BTN_Y, 0, BTN_TL2, BTN_A, BTN_B, BTN_X, 0, BTN_TL, BTN_TR, -1 };
static int grip_btn_bd[] = { BTN_THUMB, BTN_THUMB2, BTN_TRIGGER, BTN_TOP, BTN_BASE, -1 };
static int grip_btn_xt[] = { BTN_TRIGGER, BTN_THUMB, BTN_A, BTN_B, BTN_C, BTN_X, BTN_Y, BTN_Z, BTN_SELECT, BTN_START, BTN_MODE, -1 };
static int grip_btn_dc[] = { BTN_TRIGGER, BTN_THUMB, BTN_TOP, BTN_TOP2, BTN_BASE, BTN_BASE2, BTN_BASE3, BTN_BASE4, BTN_BASE5, -1 };

static int grip_abs_gpp[] = { ABS_X, ABS_Y, -1 };
static int grip_abs_bd[] = { ABS_X, ABS_Y, ABS_THROTTLE, ABS_HAT0X, ABS_HAT0Y, -1 };
static int grip_abs_xt[] = { ABS_X, ABS_Y, ABS_BRAKE, ABS_GAS, ABS_THROTTLE, ABS_HAT0X, ABS_HAT0Y, ABS_HAT1X, ABS_HAT1Y, -1 };
static int grip_abs_dc[] = { ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_THROTTLE, ABS_HAT0X, ABS_HAT0Y, -1 };

static char *grip_name[] = { NULL, "Gravis GamePad Pro", "Gravis Blackhawk Digital",
				"Gravis Xterminator Digital", "Gravis Xterminator DualControl" };
static int *grip_abs[] = { 0, grip_abs_gpp, grip_abs_bd, grip_abs_xt, grip_abs_dc };
static int *grip_btn[] = { 0, grip_btn_gpp, grip_btn_bd, grip_btn_xt, grip_btn_dc };
static char grip_anx[] = { 0, 0, 3, 5, 5 };
static char grip_cen[] = { 0, 0, 2, 2, 4 };

/*
 * grip_gpp_read_packet() reads a Gravis GamePad Pro packet.
 */

static int grip_gpp_read_packet(struct gameport *gameport, int shift, unsigned int *data)
{
	unsigned long flags;
	unsigned char u, v;
	unsigned int t;
	int i;

	int strobe = gameport_time(gameport, GRIP_STROBE_GPP);

	data[0] = 0;
	t = strobe;
	i = 0;

	__save_flags(flags);
	__cli();

	v = gameport_read(gameport) >> shift;

	do {
		t--;
		u = v; v = (gameport_read(gameport) >> shift) & 3;
		if (~v & u & 1) {
			data[0] |= (v >> 1) << i++;
			t = strobe;
		}
	} while (i < GRIP_LENGTH_GPP && t > 0);

	__restore_flags(flags);

	if (i < GRIP_LENGTH_GPP) return -1;

	for (i = 0; i < GRIP_LENGTH_GPP && (data[0] & 0xfe4210) ^ 0x7c0000; i++)
		data[0] = data[0] >> 1 | (data[0] & 1) << (GRIP_LENGTH_GPP - 1);

	return -(i == GRIP_LENGTH_GPP);
}

/*
 * grip_xt_read_packet() reads a Gravis Xterminator packet.
 */

static int grip_xt_read_packet(struct gameport *gameport, int shift, unsigned int *data)
{
	unsigned int i, j, buf, crc;
	unsigned char u, v, w;
	unsigned long flags;
	unsigned int t;
	char status;

	int strobe = gameport_time(gameport, GRIP_STROBE_XT);

	data[0] = data[1] = data[2] = data[3] = 0;
	status = buf = i = j = 0;
	t = strobe;

	__save_flags(flags);
	__cli();

	v = w = (gameport_read(gameport) >> shift) & 3;

	do {
		t--;
		u = (gameport_read(gameport) >> shift) & 3;

		if (u ^ v) {

			if ((u ^ v) & 1) {
				buf = (buf << 1) | (u >> 1);
				t = strobe;
				i++;
			} else 

			if ((((u ^ v) & (v ^ w)) >> 1) & ~(u | v | w) & 1) {
				if (i == 20) {
					crc = buf ^ (buf >> 7) ^ (buf >> 14);
					if (!((crc ^ (0x25cb9e70 >> ((crc >> 2) & 0x1c))) & 0xf)) {
						data[buf >> 18] = buf >> 4;
						status |= 1 << (buf >> 18);
					}
					j++;
				}
				t = strobe;
				buf = 0;
				i = 0;
			}
			w = v;
			v = u;
		}

	} while (status != 0xf && i < GRIP_MAX_BITS_XT && j < GRIP_MAX_CHUNKS_XT && t > 0);

	__restore_flags(flags);

	return -(status != 0xf);
}

/*
 * grip_timer() repeatedly polls the joysticks and generates events.
 */

static void grip_timer(unsigned long private)
{
	struct grip *grip = (void*) private;
	unsigned int data[GRIP_LENGTH_XT];
	struct input_dev *dev;
	int i, j;

	for (i = 0; i < 2; i++) {

		dev = grip->dev + i;
		grip->reads++;

		switch (grip->mode[i]) {

			case GRIP_MODE_GPP:

				if (grip_gpp_read_packet(grip->gameport, (i << 1) + 4, data)) {
					grip->bads++;
					break;
				}

				input_report_abs(dev, ABS_X, ((*data >> 15) & 1) - ((*data >> 16) & 1));
				input_report_abs(dev, ABS_Y, ((*data >> 13) & 1) - ((*data >> 12) & 1));

				for (j = 0; j < 12; j++)
					if (grip_btn_gpp[j])
						input_report_key(dev, grip_btn_gpp[j], (*data >> j) & 1);

				break;

			case GRIP_MODE_BD:

				if (grip_xt_read_packet(grip->gameport, (i << 1) + 4, data)) {
					grip->bads++;
					break;
				}

				input_report_abs(dev, ABS_X,        (data[0] >> 2) & 0x3f);
				input_report_abs(dev, ABS_Y,  63 - ((data[0] >> 8) & 0x3f));
				input_report_abs(dev, ABS_THROTTLE, (data[2] >> 8) & 0x3f);

				input_report_abs(dev, ABS_HAT0X, ((data[2] >> 1) & 1) - ( data[2]       & 1));
				input_report_abs(dev, ABS_HAT0Y, ((data[2] >> 2) & 1) - ((data[2] >> 3) & 1));

				for (j = 0; j < 5; j++)
					input_report_key(dev, grip_btn_bd[j], (data[3] >> (j + 4)) & 1);

				break;

			case GRIP_MODE_XT:

				if (grip_xt_read_packet(grip->gameport, (i << 1) + 4, data)) {
					grip->bads++;
					break;
				}

				input_report_abs(dev, ABS_X,        (data[0] >> 2) & 0x3f);
				input_report_abs(dev, ABS_Y,  63 - ((data[0] >> 8) & 0x3f));
				input_report_abs(dev, ABS_BRAKE,    (data[1] >> 2) & 0x3f);
				input_report_abs(dev, ABS_GAS,	    (data[1] >> 8) & 0x3f);
				input_report_abs(dev, ABS_THROTTLE, (data[2] >> 8) & 0x3f);

				input_report_abs(dev, ABS_HAT0X, ((data[2] >> 1) & 1) - ( data[2]       & 1));
				input_report_abs(dev, ABS_HAT0Y, ((data[2] >> 2) & 1) - ((data[2] >> 3) & 1));
				input_report_abs(dev, ABS_HAT1X, ((data[2] >> 5) & 1) - ((data[2] >> 4) & 1));
				input_report_abs(dev, ABS_HAT1Y, ((data[2] >> 6) & 1) - ((data[2] >> 7) & 1));

				for (j = 0; j < 11; j++)
					input_report_key(dev, grip_btn_xt[j], (data[3] >> (j + 3)) & 1);
				break;

			case GRIP_MODE_DC:

				if (grip_xt_read_packet(grip->gameport, (i << 1) + 4, data)) {
					grip->bads++;
					break;
				}

				input_report_abs(dev, ABS_X,        (data[0] >> 2) & 0x3f);
				input_report_abs(dev, ABS_Y,        (data[0] >> 8) & 0x3f);
				input_report_abs(dev, ABS_RX,       (data[1] >> 2) & 0x3f);
				input_report_abs(dev, ABS_RY,	    (data[1] >> 8) & 0x3f);
				input_report_abs(dev, ABS_THROTTLE, (data[2] >> 8) & 0x3f);

				input_report_abs(dev, ABS_HAT0X, ((data[2] >> 1) & 1) - ( data[2]       & 1));
				input_report_abs(dev, ABS_HAT0Y, ((data[2] >> 2) & 1) - ((data[2] >> 3) & 1));

				for (j = 0; j < 9; j++)
					input_report_key(dev, grip_btn_dc[j], (data[3] >> (j + 3)) & 1);
				break;


		}
	}

	mod_timer(&grip->timer, jiffies + GRIP_REFRESH_TIME);
}

static int grip_open(struct input_dev *dev)
{
	struct grip *grip = dev->private;
	if (!grip->used++)
		mod_timer(&grip->timer, jiffies + GRIP_REFRESH_TIME);
	return 0;
}

static void grip_close(struct input_dev *dev)
{
	struct grip *grip = dev->private;
	if (!--grip->used)
		del_timer(&grip->timer);
}

static void grip_connect(struct gameport *gameport, struct gameport_dev *dev)
{
	struct grip *grip;
	unsigned int data[GRIP_LENGTH_XT];
	int i, j, t;

	if (!(grip = kmalloc(sizeof(struct grip), GFP_KERNEL)))
		return;
	memset(grip, 0, sizeof(struct grip));

	gameport->private = grip;

	grip->gameport = gameport;
	init_timer(&grip->timer);
	grip->timer.data = (long) grip;
	grip->timer.function = grip_timer;

	 if (gameport_open(gameport, dev, GAMEPORT_MODE_RAW))
		goto fail1;

	for (i = 0; i < 2; i++) {
		if (!grip_gpp_read_packet(gameport, (i << 1) + 4, data)) {
			grip->mode[i] = GRIP_MODE_GPP;
			continue;
		}
		if (!grip_xt_read_packet(gameport, (i << 1) + 4, data)) {
			if (!(data[3] & 7)) {
				grip->mode[i] = GRIP_MODE_BD;
				continue;
			}
			if (!(data[2] & 0xf0)) {
				grip->mode[i] = GRIP_MODE_XT;
				continue;
			}
			grip->mode[i] = GRIP_MODE_DC;
			continue;
		}
	}

	if (!grip->mode[0] && !grip->mode[1])
		goto fail2;

	for (i = 0; i < 2; i++)
		if (grip->mode[i]) {

			grip->dev[i].private = grip;

			grip->dev[i].open = grip_open;
			grip->dev[i].close = grip_close;

			grip->dev[i].name = grip_name[grip->mode[i]];
			grip->dev[i].idbus = BUS_GAMEPORT;
			grip->dev[i].idvendor = GAMEPORT_ID_VENDOR_GRAVIS;
			grip->dev[i].idproduct = grip->mode[i];
			grip->dev[i].idversion = 0x0100;

			grip->dev[i].evbit[0] = BIT(EV_KEY) | BIT(EV_ABS);

			for (j = 0; (t = grip_abs[grip->mode[i]][j]) >= 0; j++) {

				set_bit(t, grip->dev[i].absbit);

				if (j < grip_cen[grip->mode[i]]) {
					grip->dev[i].absmin[t] = 14;
					grip->dev[i].absmax[t] = 52;
					grip->dev[i].absfuzz[t] = 1;
					grip->dev[i].absflat[t] = 2;
					continue;
				}

				if (j < grip_anx[grip->mode[i]]) {
					grip->dev[i].absmin[t] = 3;
					grip->dev[i].absmax[t] = 57;
					grip->dev[i].absfuzz[t] = 1;
					continue;
				}

				grip->dev[i].absmin[t] = -1;
				grip->dev[i].absmax[t] = 1;
			}

			for (j = 0; (t = grip_btn[grip->mode[i]][j]) >= 0; j++)
				if (t > 0)
					set_bit(t, grip->dev[i].keybit);

			input_register_device(grip->dev + i);

			printk(KERN_INFO "input%d: %s on gameport%d.%d\n",
				grip->dev[i].number, grip_name[grip->mode[i]], gameport->number, i);
		}

	return;
fail2:	gameport_close(gameport);
fail1:	kfree(grip);
}

static void grip_disconnect(struct gameport *gameport)
{
	int i;

	struct grip *grip = gameport->private;
	for (i = 0; i < 2; i++)
		if (grip->mode[i])
			input_unregister_device(grip->dev + i);
	gameport_close(gameport);
	kfree(grip);
}

static struct gameport_dev grip_dev = {
	connect:	grip_connect,
	disconnect:	grip_disconnect,
};

int __init grip_init(void)
{
	gameport_register_device(&grip_dev);
	return 0;
}

void __exit grip_exit(void)
{
	gameport_unregister_device(&grip_dev);
}

module_init(grip_init);
module_exit(grip_exit);

MODULE_LICENSE("GPL");
