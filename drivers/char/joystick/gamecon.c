/*
 * $Id: gamecon.c,v 1.14 2001/04/29 22:42:14 vojtech Exp $
 *
 *  Copyright (c) 1999-2001 Vojtech Pavlik
 *
 *  Based on the work of:
 *  	Andree Borrmann		John Dahlstrom
 *  	David Kuder		Nathan Hand
 *
 *  Sponsored by SuSE
 */

/*
 * NES, SNES, N64, Multi1, Multi2, PSX gamepad driver for Linux
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
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/parport.h>
#include <linux/input.h>

MODULE_AUTHOR("Vojtech Pavlik <vojtech@suse.cz>");
MODULE_LICENSE("GPL");
MODULE_PARM(gc, "2-6i");
MODULE_PARM(gc_2,"2-6i");
MODULE_PARM(gc_3,"2-6i");

#define GC_SNES		1
#define GC_NES		2
#define GC_NES4		3
#define GC_MULTI	4
#define GC_MULTI2	5
#define GC_N64		6	
#define GC_PSX		7

#define GC_MAX		7

#define GC_REFRESH_TIME	HZ/100
 
struct gc {
	struct pardevice *pd;
	struct input_dev dev[5];
	struct timer_list timer;
	unsigned char pads[GC_MAX + 1];
	int used;
};

static struct gc *gc_base[3];

static int gc[] __initdata = { -1, 0, 0, 0, 0, 0 };
static int gc_2[] __initdata = { -1, 0, 0, 0, 0, 0 };
static int gc_3[] __initdata = { -1, 0, 0, 0, 0, 0 };

static int gc_status_bit[] = { 0x40, 0x80, 0x20, 0x10, 0x08 };

static char *gc_names[] = { NULL, "SNES pad", "NES pad", "NES FourPort", "Multisystem joystick",
				"Multisystem 2-button joystick", "N64 controller", "PSX controller" };
/*
 * N64 support.
 */

static unsigned char gc_n64_bytes[] = { 0, 1, 13, 15, 14, 12, 10, 11, 2, 3 };
static short gc_n64_btn[] = { BTN_A, BTN_B, BTN_C, BTN_X, BTN_Y, BTN_Z, BTN_TL, BTN_TR, BTN_TRIGGER, BTN_START };

#define GC_N64_LENGTH		32		/* N64 bit length, not including stop bit */
#define GC_N64_REQUEST_LENGTH	37		/* transmit request sequence is 9 bits long */
#define GC_N64_DELAY		133		/* delay between transmit request, and response ready (us) */
#define GC_N64_REQUEST		0x1dd1111111ULL /* the request data command (encoded for 000000011) */
#define GC_N64_DWS		3		/* delay between write segments (required for sound playback because of ISA DMA) */
						/* GC_N64_DWS > 24 is known to fail */ 
#define GC_N64_POWER_W		0xe2		/* power during write (transmit request) */
#define GC_N64_POWER_R		0xfd		/* power during read */
#define GC_N64_OUT		0x1d		/* output bits to the 4 pads */
						/* Reading the main axes of any N64 pad is known to fail if the corresponding bit */
						/* in GC_N64_OUT is pulled low on the output port (by any routine) for more */
						/* than 123 us */
#define GC_N64_CLOCK		0x02		/* clock bits for read */

/* 
 * gc_n64_read_packet() reads an N64 packet. 
 * Each pad uses one bit per byte. So all pads connected to this port are read in parallel.
 */

static void gc_n64_read_packet(struct gc *gc, unsigned char *data)
{
	int i;
	unsigned long flags;

/*
 * Request the pad to transmit data
 */

	__save_flags(flags);
	__cli();
	for (i = 0; i < GC_N64_REQUEST_LENGTH; i++) {
		parport_write_data(gc->pd->port, GC_N64_POWER_W | ((GC_N64_REQUEST >> i) & 1 ? GC_N64_OUT : 0));
		udelay(GC_N64_DWS);
	}
	__restore_flags(flags);

/*
 * Wait for the pad response to be loaded into the 33-bit register of the adapter
 */

	udelay(GC_N64_DELAY);

/*
 * Grab data (ignoring the last bit, which is a stop bit)
 */

	for (i = 0; i < GC_N64_LENGTH; i++) {
		parport_write_data(gc->pd->port, GC_N64_POWER_R);
		data[i] = parport_read_status(gc->pd->port);
		parport_write_data(gc->pd->port, GC_N64_POWER_R | GC_N64_CLOCK);
	 }

/*
 * We must wait 200 ms here for the controller to reinitialize before the next read request.
 * No worries as long as gc_read is polled less frequently than this.
 */

}

/*
 * NES/SNES support.
 */

#define GC_NES_DELAY	6	/* Delay between bits - 6us */
#define GC_NES_LENGTH	8	/* The NES pads use 8 bits of data */
#define GC_SNES_LENGTH	12	/* The SNES true length is 16, but the last 4 bits are unused */

#define GC_NES_POWER	0xfc
#define GC_NES_CLOCK	0x01
#define GC_NES_LATCH	0x02

static unsigned char gc_nes_bytes[] = { 0, 1, 2, 3 };
static unsigned char gc_snes_bytes[] = { 8, 0, 2, 3, 9, 1, 10, 11 };
static short gc_snes_btn[] = { BTN_A, BTN_B, BTN_SELECT, BTN_START, BTN_X, BTN_Y, BTN_TL, BTN_TR };

/*
 * gc_nes_read_packet() reads a NES/SNES packet.
 * Each pad uses one bit per byte. So all pads connected to
 * this port are read in parallel.
 */

static void gc_nes_read_packet(struct gc *gc, int length, unsigned char *data)
{
	int i;

	parport_write_data(gc->pd->port, GC_NES_POWER | GC_NES_CLOCK | GC_NES_LATCH);
	udelay(GC_NES_DELAY * 2);
	parport_write_data(gc->pd->port, GC_NES_POWER | GC_NES_CLOCK);

	for (i = 0; i < length; i++) {
		udelay(GC_NES_DELAY);
		parport_write_data(gc->pd->port, GC_NES_POWER);
		data[i] = parport_read_status(gc->pd->port) ^ 0x7f;
		udelay(GC_NES_DELAY);
		parport_write_data(gc->pd->port, GC_NES_POWER | GC_NES_CLOCK);
	}
}

/*
 * Multisystem joystick support
 */

#define GC_MULTI_LENGTH		5	/* Multi system joystick packet length is 5 */
#define GC_MULTI2_LENGTH	6	/* One more bit for one more button */

/*
 * gc_multi_read_packet() reads a Multisystem joystick packet.
 */

static void gc_multi_read_packet(struct gc *gc, int length, unsigned char *data)
{
	int i;

	for (i = 0; i < length; i++) {
		parport_write_data(gc->pd->port, ~(1 << i));
		data[i] = parport_read_status(gc->pd->port) ^ 0x7f;
	}
}

/*
 * PSX support
 *
 * See documentation at:
 *	http://www.dim.com/~mackys/psxmemcard/ps-eng2.txt
 *	http://www.gamesx.com/controldata/psxcont/psxcont.htm
 *	ftp://milano.usal.es/pablo/
 *	
 */

#define GC_PSX_DELAY	60		/* 60 usec */
#define GC_PSX_LENGTH	8		/* talk to the controller in bytes */

#define GC_PSX_MOUSE	1		/* Mouse */
#define GC_PSX_NEGCON	2		/* NegCon */
#define GC_PSX_NORMAL	4		/* Digital / Analog or Rumble in Digital mode  */
#define GC_PSX_ANALOG	5		/* Analog in Analog mode / Rumble in Green mode */
#define GC_PSX_RUMBLE	7		/* Rumble in Red mode */

#define GC_PSX_CLOCK	0x04		/* Pin 4 */
#define GC_PSX_COMMAND	0x01		/* Pin 1 */
#define GC_PSX_POWER	0xf8		/* Pins 5-9 */
#define GC_PSX_SELECT	0x02		/* Pin 3 */

#define GC_PSX_ID(x)	((x) >> 4)	/* High nibble is device type */
#define GC_PSX_LEN(x)	((x) & 0xf)	/* Low nibble is length in words */

static short gc_psx_abs[] = { ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_HAT0X, ABS_HAT0Y };
static short gc_psx_btn[] = { BTN_TL, BTN_TR, BTN_TL2, BTN_TR2, BTN_A, BTN_B, BTN_X, BTN_Y,
				BTN_START, BTN_SELECT, BTN_THUMBL, BTN_THUMBR };

/*
 * gc_psx_command() writes 8bit command and reads 8bit data from
 * the psx pad.
 */

static int gc_psx_command(struct gc *gc, int b)
{
	int i, cmd, data = 0;

	for (i = 0; i < 8; i++, b >>= 1) {
		cmd = (b & 1) ? GC_PSX_COMMAND : 0;
		parport_write_data(gc->pd->port, cmd | GC_PSX_POWER);
		udelay(GC_PSX_DELAY);
		data |= ((parport_read_status(gc->pd->port) ^ 0x80) & gc->pads[GC_PSX]) ? (1 << i) : 0;
		parport_write_data(gc->pd->port, cmd | GC_PSX_CLOCK | GC_PSX_POWER);
		udelay(GC_PSX_DELAY);
	}
	return data;
}

/*
 * gc_psx_read_packet() reads a whole psx packet and returns
 * device identifier code.
 */

static int gc_psx_read_packet(struct gc *gc, unsigned char *data)
{
	int i, id;
	unsigned long flags;

	parport_write_data(gc->pd->port, GC_PSX_CLOCK | GC_PSX_SELECT | GC_PSX_POWER);	/* Select pad */
	udelay(GC_PSX_DELAY * 2);
	parport_write_data(gc->pd->port, GC_PSX_CLOCK | GC_PSX_POWER);			/* Deselect, begin command */
	udelay(GC_PSX_DELAY * 2);

	__save_flags(flags);
	__cli();

	gc_psx_command(gc, 0x01);							/* Access pad */
	id = gc_psx_command(gc, 0x42);							/* Get device id */
	if (gc_psx_command(gc, 0) == 0x5a) {						/* Okay? */
		for (i = 0; i < GC_PSX_LEN(id) * 2; i++)
			data[i] = gc_psx_command(gc, 0);
	} else id = 0;

	__restore_flags(flags);

	parport_write_data(gc->pd->port, GC_PSX_CLOCK | GC_PSX_SELECT | GC_PSX_POWER);

	return GC_PSX_ID(id);
}

/*
 * gc_timer() reads and analyzes console pads data.
 */

#define GC_MAX_LENGTH GC_N64_LENGTH

static void gc_timer(unsigned long private)
{
	struct gc *gc = (void *) private;
	struct input_dev *dev = gc->dev;
	unsigned char data[GC_MAX_LENGTH];
	int i, j, s;

/*
 * N64 pads - must be read first, any read confuses them for 200 us
 */

	if (gc->pads[GC_N64]) {

		gc_n64_read_packet(gc, data);

		for (i = 0; i < 5; i++) {

			s = gc_status_bit[i];

			if (s & gc->pads[GC_N64] & ~(data[8] | data[9])) {
	
				signed char axes[2];
				axes[0] = axes[1] = 0;

				for (j = 0; j < 8; j++) {
					if (data[23 - j] & s) axes[0] |= 1 << j; 
					if (data[31 - j] & s) axes[1] |= 1 << j; 
				}

				input_report_abs(dev + i, ABS_X,  axes[0]);
				input_report_abs(dev + i, ABS_Y, -axes[1]);

				input_report_abs(dev + i, ABS_HAT0X, !(s & data[6]) - !(s & data[7]));
				input_report_abs(dev + i, ABS_HAT0Y, !(s & data[4]) - !(s & data[5]));

				for (j = 0; j < 10; j++)
					input_report_key(dev + i, gc_n64_btn[j], s & data[gc_n64_bytes[j]]);
			}
		}
	}

/*
 * NES and SNES pads
 */

	if (gc->pads[GC_NES] || gc->pads[GC_SNES]) {

		gc_nes_read_packet(gc, gc->pads[GC_SNES] ? GC_SNES_LENGTH : GC_NES_LENGTH, data);

		for (i = 0; i < 5; i++) {

			s = gc_status_bit[i];

			if (s & (gc->pads[GC_NES] | gc->pads[GC_SNES])) {
				input_report_abs(dev + i, ABS_X, !(s & data[6]) - !(s & data[7]));
				input_report_abs(dev + i, ABS_Y, !(s & data[4]) - !(s & data[5]));
			}

			if (s & gc->pads[GC_NES])
				for (j = 0; j < 4; j++)
					input_report_key(dev + i, gc_snes_btn[j], s & data[gc_nes_bytes[j]]);

			if (s & gc->pads[GC_SNES])
				for (j = 0; j < 8; j++)
					input_report_key(dev + i, gc_snes_btn[j], s & data[gc_snes_bytes[j]]);
		}
	}

/*
 * Multi and Multi2 joysticks
 */

	if (gc->pads[GC_MULTI] || gc->pads[GC_MULTI2]) {

		gc_multi_read_packet(gc, gc->pads[GC_MULTI2] ? GC_MULTI2_LENGTH : GC_MULTI_LENGTH, data);

		for (i = 0; i < 5; i++) {

			s = gc_status_bit[i];

			if (s & (gc->pads[GC_MULTI] | gc->pads[GC_MULTI2])) {
				input_report_abs(dev + i, ABS_X,  !(s & data[2]) - !(s & data[3]));
				input_report_abs(dev + i, ABS_Y,  !(s & data[0]) - !(s & data[1]));
				input_report_key(dev + i, BTN_TRIGGER, s & data[4]);
			}

			if (s & gc->pads[GC_MULTI2])
				input_report_key(dev + i, BTN_THUMB, s & data[5]);
		}
	}

/*
 * PSX controllers
 */

	if (gc->pads[GC_PSX]) {

		for (i = 0; i < 5; i++)
	       		if (gc->pads[GC_PSX] & gc_status_bit[i])
				break;

 		switch (gc_psx_read_packet(gc, data)) {

			case GC_PSX_RUMBLE:

				input_report_key(dev + i, BTN_THUMB,  ~data[0] & 0x04);
				input_report_key(dev + i, BTN_THUMB2, ~data[0] & 0x02);

			case GC_PSX_NEGCON:
			case GC_PSX_ANALOG:

				for (j = 0; j < 4; j++)
					input_report_abs(dev + i, gc_psx_abs[j], data[j + 2]);

				input_report_abs(dev + i, ABS_HAT0X, !(data[0] & 0x20) - !(data[0] & 0x80));
				input_report_abs(dev + i, ABS_HAT0Y, !(data[0] & 0x40) - !(data[0] & 0x10));

				for (j = 0; j < 8; j++)
					input_report_key(dev + i, gc_psx_btn[j], ~data[1] & (1 << j));

				input_report_key(dev + i, BTN_START,  ~data[0] & 0x08);
				input_report_key(dev + i, BTN_SELECT, ~data[0] & 0x01);

				break;

			case GC_PSX_NORMAL:

				input_report_abs(dev + i, ABS_X, 128 + !(data[0] & 0x20) * 127 - !(data[0] & 0x80) * 128);
				input_report_abs(dev + i, ABS_Y, 128 + !(data[0] & 0x40) * 127 - !(data[0] & 0x10) * 128);

				for (j = 0; j < 8; j++)
					input_report_key(dev + i, gc_psx_btn[j], ~data[1] & (1 << j));

				input_report_key(dev + i, BTN_START,  ~data[0] & 0x08);
				input_report_key(dev + i, BTN_SELECT, ~data[0] & 0x01);

				break;
		}
	}

	mod_timer(&gc->timer, jiffies + GC_REFRESH_TIME);
}

static int gc_open(struct input_dev *dev)
{
	struct gc *gc = dev->private;
	if (!gc->used++) {
		parport_claim(gc->pd);
		parport_write_control(gc->pd->port, 0x04);
		mod_timer(&gc->timer, jiffies + GC_REFRESH_TIME);
	}
	return 0;
}

static void gc_close(struct input_dev *dev)
{
	struct gc *gc = dev->private;
	if (!--gc->used) {
		del_timer(&gc->timer);
		parport_write_control(gc->pd->port, 0x00);
		parport_release(gc->pd);
	}
}

static struct gc __init *gc_probe(int *config)
{
	struct gc *gc;
	struct parport *pp;
	int i, j, psx;
	unsigned char data[32];

	if (config[0] < 0)
		return NULL;

	for (pp = parport_enumerate(); pp && (config[0] > 0); pp = pp->next)
		config[0]--;

	if (!pp) {
		printk(KERN_ERR "gamecon.c: no such parport\n");
		return NULL;
	}

	if (!(gc = kmalloc(sizeof(struct gc), GFP_KERNEL)))
		return NULL;
	memset(gc, 0, sizeof(struct gc));

	gc->pd = parport_register_device(pp, "gamecon", NULL, NULL, NULL, PARPORT_DEV_EXCL, NULL);

	if (!gc->pd) {
		printk(KERN_ERR "gamecon.c: parport busy already - lp.o loaded?\n");
		kfree(gc);
		return NULL;
	}

	parport_claim(gc->pd);

	init_timer(&gc->timer);
	gc->timer.data = (long) gc;
	gc->timer.function = gc_timer;

	for (i = 0; i < 5; i++) {

		if (!config[i + 1])
			continue;

		if (config[i + 1] < 1 || config[i + 1] > GC_MAX) {
			printk(KERN_WARNING "gamecon.c: Pad type %d unknown\n", config[i + 1]);
			continue;
		}

                gc->dev[i].private = gc;
                gc->dev[i].open = gc_open;
                gc->dev[i].close = gc_close;

                gc->dev[i].evbit[0] = BIT(EV_KEY) | BIT(EV_ABS);

		for (j = 0; j < 2; j++) {
			set_bit(ABS_X + j, gc->dev[i].absbit);
			gc->dev[i].absmin[ABS_X + j] = -1;
			gc->dev[i].absmax[ABS_X + j] =  1;
		}

		gc->pads[0] |= gc_status_bit[i];
		gc->pads[config[i + 1]] |= gc_status_bit[i];

		switch(config[i + 1]) {

			case GC_N64:
				for (j = 0; j < 10; j++)
					set_bit(gc_n64_btn[j], gc->dev[i].keybit);

				for (j = 0; j < 2; j++) {
					set_bit(ABS_X + j, gc->dev[i].absbit);
					gc->dev[i].absmin[ABS_X + j] = -127;
					gc->dev[i].absmax[ABS_X + j] =  126;
					gc->dev[i].absflat[ABS_X + j] = 2;
					set_bit(ABS_HAT0X + j, gc->dev[i].absbit);
					gc->dev[i].absmin[ABS_HAT0X + j] = -1;
					gc->dev[i].absmax[ABS_HAT0X + j] =  1;
				}

				break;

			case GC_SNES:
				for (j = 4; j < 8; j++)
					set_bit(gc_snes_btn[j], gc->dev[i].keybit);
			case GC_NES:
				for (j = 0; j < 4; j++)
					set_bit(gc_snes_btn[j], gc->dev[i].keybit);
				break;

			case GC_MULTI2:
				set_bit(BTN_THUMB, gc->dev[i].keybit);
			case GC_MULTI:
				set_bit(BTN_TRIGGER, gc->dev[i].keybit);
				break;

			case GC_PSX:
				
				psx = gc_psx_read_packet(gc, data);

				switch(psx) {
					case GC_PSX_NEGCON:
					case GC_PSX_NORMAL:
					case GC_PSX_ANALOG:
					case GC_PSX_RUMBLE:

						for (j = 0; j < 6; j++) {
							psx = gc_psx_abs[j];
							set_bit(psx, gc->dev[i].absbit);
							if (j < 4) {
								gc->dev[i].absmin[psx] = 4;
								gc->dev[i].absmax[psx] = 252;
								gc->dev[i].absflat[psx] = 2;
							} else {
								gc->dev[i].absmin[psx] = -1;
								gc->dev[i].absmax[psx] = 1;
							}
						}

						for (j = 0; j < 12; j++)
							set_bit(gc_psx_btn[j], gc->dev[i].keybit);

						break;

					case 0:
						gc->pads[GC_PSX] &= ~gc_status_bit[i];
						printk(KERN_ERR "gamecon.c: No PSX controller found.\n");
						break;

					default:
						gc->pads[GC_PSX] &= ~gc_status_bit[i];
						printk(KERN_WARNING "gamecon.c: Unsupported PSX controller %#x,"
							" please report to <vojtech@suse.cz>.\n", psx);
				}
				break;
		}

                gc->dev[i].name = gc_names[config[i + 1]];
                gc->dev[i].idbus = BUS_PARPORT;
                gc->dev[i].idvendor = 0x0001;
                gc->dev[i].idproduct = config[i + 1];
                gc->dev[i].idversion = 0x0100;
	}

	parport_release(gc->pd);

	if (!gc->pads[0]) {
		parport_unregister_device(gc->pd);
		kfree(gc);
		return NULL;
	}

	for (i = 0; i < 5; i++) 
		if (gc->pads[0] & gc_status_bit[i]) {
			input_register_device(gc->dev + i);
			printk(KERN_INFO "input%d: %s on %s\n", gc->dev[i].number, gc->dev[i].name, gc->pd->port->name);
		}

	return gc;
}

#ifndef MODULE
int __init gc_setup(char *str)
{
	int i, ints[7];
	get_options(str, ARRAY_SIZE(ints), ints);
	for (i = 0; i <= ints[0] && i < 6; i++) gc[i] = ints[i + 1];
	return 1;
}
int __init gc_setup_2(char *str)
{
	int i, ints[7];
	get_options(str, ARRAY_SIZE(ints), ints);
	for (i = 0; i <= ints[0] && i < 6; i++) gc_2[i] = ints[i + 1];
	return 1;
}
int __init gc_setup_3(char *str)
{
	int i, ints[7];
	get_options(str, ARRAY_SIZE(ints), ints);
	for (i = 0; i <= ints[0] && i < 6; i++) gc_3[i] = ints[i + 1];
	return 1;
}
__setup("gc=", gc_setup);
__setup("gc_2=", gc_setup_2);
__setup("gc_3=", gc_setup_3);
#endif

int __init gc_init(void)
{
	gc_base[0] = gc_probe(gc);
	gc_base[1] = gc_probe(gc_2);
	gc_base[2] = gc_probe(gc_3);

	if (gc_base[0] || gc_base[1] || gc_base[2])
		return 0;

	return -ENODEV;
}

void __exit gc_exit(void)
{
	int i, j;

	for (i = 0; i < 3; i++)
		if (gc_base[i]) {
			for (j = 0; j < 5; j++)
				if (gc_base[i]->pads[0] & gc_status_bit[j])
					input_unregister_device(gc_base[i]->dev + j); 
			parport_unregister_device(gc_base[i]->pd);
		}
}

module_init(gc_init);
module_exit(gc_exit);
