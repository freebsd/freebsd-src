/*
 * $Id: turbografx.c,v 1.8 2000/05/29 20:39:38 vojtech Exp $
 *
 *  Copyright (c) 1998-2000 Vojtech Pavlik
 *
 *  Based on the work of:
 *	Steffen Schwenke
 *
 *  Sponsored by SuSE
 */

/*
 * TurboGraFX parallel port interface driver for Linux.
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
#include <linux/parport.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/init.h>

MODULE_AUTHOR("Vojtech Pavlik <vojtech@suse.cz>");
MODULE_LICENSE("GPL");
MODULE_PARM(tgfx, "2-8i");
MODULE_PARM(tgfx_2, "2-8i");
MODULE_PARM(tgfx_3, "2-8i");

#define TGFX_REFRESH_TIME	HZ/100	/* 10 ms */

#define TGFX_TRIGGER		0x08
#define TGFX_UP			0x10
#define TGFX_DOWN		0x20	
#define TGFX_LEFT		0x40
#define TGFX_RIGHT		0x80

#define TGFX_THUMB		0x02
#define TGFX_THUMB2		0x04
#define TGFX_TOP		0x01
#define TGFX_TOP2		0x08

static int tgfx[] __initdata = { -1, 0, 0, 0, 0, 0, 0, 0 };
static int tgfx_2[] __initdata = { -1, 0, 0, 0, 0, 0, 0, 0 };
static int tgfx_3[] __initdata = { -1, 0, 0, 0, 0, 0, 0, 0 };

static int tgfx_buttons[] = { BTN_TRIGGER, BTN_THUMB, BTN_THUMB2, BTN_TOP, BTN_TOP2 };
static char *tgfx_name = "TurboGraFX Multisystem joystick";

struct tgfx {
	struct pardevice *pd;
	struct timer_list timer;
	struct input_dev dev[7];
	int sticks;
	int used;
} *tgfx_base[3];

/*
 * tgfx_timer() reads and analyzes TurboGraFX joystick data.
 */

static void tgfx_timer(unsigned long private)
{
	struct tgfx *tgfx = (void *) private;
	struct input_dev *dev;
	int data1, data2, i;

	for (i = 0; i < 7; i++)
		if (tgfx->sticks & (1 << i)) {

 			dev = tgfx->dev + i;

			parport_write_data(tgfx->pd->port, ~(1 << i));
			data1 = parport_read_status(tgfx->pd->port) ^ 0x7f;
			data2 = parport_read_control(tgfx->pd->port) ^ 0x04;	/* CAVEAT parport */

			input_report_abs(dev, ABS_X, !!(data1 & TGFX_RIGHT) - !!(data1 & TGFX_LEFT));
			input_report_abs(dev, ABS_Y, !!(data1 & TGFX_DOWN ) - !!(data1 & TGFX_UP  ));

			input_report_key(dev, BTN_TRIGGER, (data1 & TGFX_TRIGGER));
			input_report_key(dev, BTN_THUMB,   (data2 & TGFX_THUMB  ));
			input_report_key(dev, BTN_THUMB2,  (data2 & TGFX_THUMB2 ));
			input_report_key(dev, BTN_TOP,     (data2 & TGFX_TOP    ));
			input_report_key(dev, BTN_TOP2,    (data2 & TGFX_TOP2   ));
		}

	mod_timer(&tgfx->timer, jiffies + TGFX_REFRESH_TIME);
}

static int tgfx_open(struct input_dev *dev)
{
        struct tgfx *tgfx = dev->private;
        if (!tgfx->used++) {
		parport_claim(tgfx->pd);
		parport_write_control(tgfx->pd->port, 0x04);
                mod_timer(&tgfx->timer, jiffies + TGFX_REFRESH_TIME); 
	}
        return 0;
}

static void tgfx_close(struct input_dev *dev)
{
        struct tgfx *tgfx = dev->private;
        if (!--tgfx->used) {
                del_timer(&tgfx->timer);
		parport_write_control(tgfx->pd->port, 0x00);
        	parport_release(tgfx->pd);
	}
}

/*
 * tgfx_probe() probes for tg gamepads.
 */

static struct tgfx __init *tgfx_probe(int *config)
{
	struct tgfx *tgfx;
	struct parport *pp;
	int i, j;

	if (config[0] < 0)
		return NULL;

	for (pp = parport_enumerate(); pp && (config[0] > 0); pp = pp->next)
		config[0]--;

	if (!pp) {
		printk(KERN_ERR "turbografx.c: no such parport\n");
		return NULL;
	}

	if (!(tgfx = kmalloc(sizeof(struct tgfx), GFP_KERNEL)))
		return NULL;
	memset(tgfx, 0, sizeof(struct tgfx));

	tgfx->pd = parport_register_device(pp, "turbografx", NULL, NULL, NULL, PARPORT_DEV_EXCL, NULL);

	if (!tgfx->pd) {
		printk(KERN_ERR "turbografx.c: parport busy already - lp.o loaded?\n");
		kfree(tgfx);
		return NULL;
	}

	init_timer(&tgfx->timer);
	tgfx->timer.data = (long) tgfx;
	tgfx->timer.function = tgfx_timer;

	tgfx->sticks = 0;

	for (i = 0; i < 7; i++)
		if (config[i+1] > 0 && config[i+1] < 6) {

			tgfx->sticks |= (1 << i);

			tgfx->dev[i].private = tgfx;
			tgfx->dev[i].open = tgfx_open;
			tgfx->dev[i].close = tgfx_close;

			tgfx->dev[i].name = tgfx_name;
			tgfx->dev[i].idbus = BUS_PARPORT;
			tgfx->dev[i].idvendor = 0x0003;
			tgfx->dev[i].idproduct = config[i+1];
			tgfx->dev[i].idversion = 0x0100;

			tgfx->dev[i].evbit[0] = BIT(EV_KEY) | BIT(EV_ABS);
			tgfx->dev[i].absbit[0] = BIT(ABS_X) | BIT(ABS_Y);

			for (j = 0; j < config[i+1]; j++)
				set_bit(tgfx_buttons[j], tgfx->dev[i].keybit); 

			tgfx->dev[i].absmin[ABS_X] = -1; tgfx->dev[i].absmax[ABS_X] = 1;
			tgfx->dev[i].absmin[ABS_Y] = -1; tgfx->dev[i].absmax[ABS_Y] = 1;

			input_register_device(tgfx->dev + i);
			printk(KERN_INFO "input%d: %d-button Multisystem joystick on %s\n",
				tgfx->dev[i].number, config[i+1], tgfx->pd->port->name);
		}

        if (!tgfx->sticks) {
		parport_unregister_device(tgfx->pd);
		kfree(tgfx);
		return NULL;
        }
		
	return tgfx;
}

#ifndef MODULE
int __init tgfx_setup(char *str)
{
	int i, ints[9];
	get_options(str, ARRAY_SIZE(ints), ints);
	for (i = 0; i <= ints[0] && i < 8; i++) tgfx[i] = ints[i + 1];
	return 1;
}
int __init tgfx_setup_2(char *str)
{
	int i, ints[9];
	get_options(str, ARRAY_SIZE(ints), ints);
	for (i = 0; i <= ints[0] && i < 8; i++) tgfx_2[i] = ints[i + 1];
	return 1;
}
int __init tgfx_setup_3(char *str)
{
	int i, ints[9];
	get_options(str, ARRAY_SIZE(ints), ints);
	for (i = 0; i <= ints[0] && i < 8; i++) tgfx_3[i] = ints[i + 1];
	return 1;
}
__setup("tgfx=", tgfx_setup);
__setup("tgfx_2=", tgfx_setup_2);
__setup("tgfx_3=", tgfx_setup_3);
#endif

int __init tgfx_init(void)
{
	tgfx_base[0] = tgfx_probe(tgfx);
	tgfx_base[1] = tgfx_probe(tgfx_2);
	tgfx_base[2] = tgfx_probe(tgfx_3);

	if (tgfx_base[0] || tgfx_base[1] || tgfx_base[2])
		return 0;

	return -ENODEV;
}

void __exit tgfx_exit(void)
{
	int i, j;

	for (i = 0; i < 3; i++) 
		if (tgfx_base[i]) {
			for (j = 0; j < 7; j++)
				if (tgfx_base[i]->sticks & (1 << j))
					input_unregister_device(tgfx_base[i]->dev + j);
		parport_unregister_device(tgfx_base[i]->pd);
	}
}

module_init(tgfx_init);
module_exit(tgfx_exit);
