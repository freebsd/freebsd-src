/*
 * $Id: db9.c,v 1.6 2000/06/25 10:57:50 vojtech Exp $
 *
 *  Copyright (c) 1999 Vojtech Pavlik
 *
 *  Based on the work of:
 *	Andree Borrmann		Mats Sjövall
 *
 *  Sponsored by SuSE
 */

/*
 * Atari, Amstrad, Commodore, Amiga, Sega, etc. joystick driver for Linux
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
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/parport.h>
#include <linux/input.h>

MODULE_AUTHOR("Vojtech Pavlik <vojtech@suse.cz>");
MODULE_LICENSE("GPL");
MODULE_PARM(db9, "2i");
MODULE_PARM(db9_2, "2i");
MODULE_PARM(db9_3, "2i");

#define DB9_MULTI_STICK		0x01
#define DB9_MULTI2_STICK	0x02
#define DB9_GENESIS_PAD		0x03
#define DB9_GENESIS5_PAD	0x05
#define DB9_GENESIS6_PAD	0x06
#define DB9_SATURN_PAD		0x07
#define DB9_MULTI_0802		0x08
#define DB9_MULTI_0802_2	0x09
#define DB9_CD32_PAD		0x0A
#define DB9_MAX_PAD		0x0B

#define DB9_UP			0x01
#define DB9_DOWN		0x02
#define DB9_LEFT		0x04
#define DB9_RIGHT		0x08
#define DB9_FIRE1		0x10
#define DB9_FIRE2		0x20
#define DB9_FIRE3		0x40
#define DB9_FIRE4		0x80

#define DB9_NORMAL		0x0a
#define DB9_NOSELECT		0x08

#define DB9_SATURN0		0x00
#define DB9_SATURN1		0x02
#define DB9_SATURN2		0x04
#define DB9_SATURN3		0x06

#define DB9_GENESIS6_DELAY	14
#define DB9_REFRESH_TIME	HZ/100

static int db9[] __initdata = { -1, 0 };
static int db9_2[] __initdata = { -1, 0 };
static int db9_3[] __initdata = { -1, 0 };

struct db9 {
	struct input_dev dev[2];
	struct timer_list timer;
	struct pardevice *pd;	
	int mode;
	int used;
};

static struct db9 *db9_base[3];

static short db9_multi_btn[] = { BTN_TRIGGER, BTN_THUMB };
static short db9_genesis_btn[] = { BTN_START, BTN_A, BTN_B, BTN_C, BTN_X, BTN_Y, BTN_Z, BTN_MODE };
static short db9_cd32_btn[] = { BTN_A, BTN_B, BTN_C, BTN_X, BTN_Y, BTN_Z, BTN_TL, BTN_TR, BTN_START };

static char db9_buttons[DB9_MAX_PAD] = { 0, 1, 2, 4, 0, 6, 8, 8, 1, 1, 7 };
static short *db9_btn[DB9_MAX_PAD] = { NULL, db9_multi_btn, db9_multi_btn, db9_genesis_btn, NULL, db9_genesis_btn,
					db9_genesis_btn, db9_cd32_btn, db9_multi_btn, db9_multi_btn, db9_cd32_btn };
static char *db9_name[DB9_MAX_PAD] = { NULL, "Multisystem joystick", "Multisystem joystick (2 fire)", "Genesis pad",
				      NULL, "Genesis 5 pad", "Genesis 6 pad", "Saturn pad", "Multisystem (0.8.0.2) joystick",
				     "Multisystem (0.8.0.2-dual) joystick", "Amiga CD-32 pad" };

static void db9_timer(unsigned long private)
{
	struct db9 *db9 = (void *) private;
	struct parport *port = db9->pd->port;
	struct input_dev *dev = db9->dev;
	int data, i;

	switch(db9->mode) {
		case DB9_MULTI_0802_2:

			data = parport_read_data(port) >> 3;

			input_report_abs(dev + 1, ABS_X, (data & DB9_RIGHT ? 0 : 1) - (data & DB9_LEFT ? 0 : 1));
			input_report_abs(dev + 1, ABS_Y, (data & DB9_DOWN  ? 0 : 1) - (data & DB9_UP   ? 0 : 1));
			input_report_key(dev + 1, BTN_TRIGGER, ~data & DB9_FIRE1);

		case DB9_MULTI_0802:

			data = parport_read_status(port) >> 3;

			input_report_abs(dev, ABS_X, (data & DB9_RIGHT ? 0 : 1) - (data & DB9_LEFT ? 0 : 1));
			input_report_abs(dev, ABS_Y, (data & DB9_DOWN  ? 0 : 1) - (data & DB9_UP   ? 0 : 1));
			input_report_key(dev, BTN_TRIGGER, data & DB9_FIRE1);
			break;

		case DB9_MULTI_STICK:

			data = parport_read_data(port);

			input_report_abs(dev, ABS_X, (data & DB9_RIGHT ? 0 : 1) - (data & DB9_LEFT ? 0 : 1));
			input_report_abs(dev, ABS_Y, (data & DB9_DOWN  ? 0 : 1) - (data & DB9_UP   ? 0 : 1));
			input_report_key(dev, BTN_TRIGGER, ~data & DB9_FIRE1);
			break;

		case DB9_MULTI2_STICK:

			data = parport_read_data(port);

			input_report_abs(dev, ABS_X, (data & DB9_RIGHT ? 0 : 1) - (data & DB9_LEFT ? 0 : 1));
			input_report_abs(dev, ABS_Y, (data & DB9_DOWN  ? 0 : 1) - (data & DB9_UP   ? 0 : 1));
			input_report_key(dev, BTN_TRIGGER, ~data & DB9_FIRE1);
			input_report_key(dev, BTN_THUMB,   ~data & DB9_FIRE2);
			break;

		case DB9_GENESIS_PAD:

			parport_write_control(port, DB9_NOSELECT);
			data = parport_read_data(port);

			input_report_abs(dev, ABS_X, (data & DB9_RIGHT ? 0 : 1) - (data & DB9_LEFT ? 0 : 1));
			input_report_abs(dev, ABS_Y, (data & DB9_DOWN  ? 0 : 1) - (data & DB9_UP   ? 0 : 1));
			input_report_key(dev, BTN_B, ~data & DB9_FIRE1);
			input_report_key(dev, BTN_C, ~data & DB9_FIRE2);

			parport_write_control(port, DB9_NORMAL);
			data=parport_read_data(port);

			input_report_key(dev, BTN_A,     ~data & DB9_FIRE1);
			input_report_key(dev, BTN_START, ~data & DB9_FIRE2);
			break;

		case DB9_GENESIS5_PAD:

			parport_write_control(port, DB9_NOSELECT);
			data=parport_read_data(port);

			input_report_abs(dev, ABS_X, (data & DB9_RIGHT ? 0 : 1) - (data & DB9_LEFT ? 0 : 1));
			input_report_abs(dev, ABS_Y, (data & DB9_DOWN  ? 0 : 1) - (data & DB9_UP   ? 0 : 1));
			input_report_key(dev, BTN_B, ~data & DB9_FIRE1);
			input_report_key(dev, BTN_C, ~data & DB9_FIRE2);

			parport_write_control(port, DB9_NORMAL);
			data=parport_read_data(port);

			input_report_key(dev, BTN_A,     ~data & DB9_FIRE1);
			input_report_key(dev, BTN_X,     ~data & DB9_FIRE2);
			input_report_key(dev, BTN_Y,     ~data & DB9_LEFT);
			input_report_key(dev, BTN_START, ~data & DB9_RIGHT);
			break;

		case DB9_GENESIS6_PAD:

			parport_write_control(port, DB9_NOSELECT); /* 1 */
			udelay(DB9_GENESIS6_DELAY);
			data=parport_read_data(port);

			input_report_abs(dev, ABS_X, (data & DB9_RIGHT ? 0 : 1) - (data & DB9_LEFT ? 0 : 1));
			input_report_abs(dev, ABS_Y, (data & DB9_DOWN  ? 0 : 1) - (data & DB9_UP   ? 0 : 1));
			input_report_key(dev, BTN_B, ~data & DB9_FIRE1);
			input_report_key(dev, BTN_C, ~data & DB9_FIRE2);

			parport_write_control(port, DB9_NORMAL);
			udelay(DB9_GENESIS6_DELAY);
			data=parport_read_data(port);

			input_report_key(dev, BTN_A, ~data & DB9_FIRE1);
			input_report_key(dev, BTN_X, ~data & DB9_FIRE2);

			parport_write_control(port, DB9_NOSELECT); /* 2 */
			udelay(DB9_GENESIS6_DELAY);
			parport_write_control(port, DB9_NORMAL);
			udelay(DB9_GENESIS6_DELAY);
			parport_write_control(port, DB9_NOSELECT); /* 3 */
			udelay(DB9_GENESIS6_DELAY);
			data=parport_read_data(port);

			input_report_key(dev, BTN_Y,     ~data & DB9_LEFT);
			input_report_key(dev, BTN_Z,     ~data & DB9_DOWN);
			input_report_key(dev, BTN_MODE,  ~data & DB9_UP);
			input_report_key(dev, BTN_START, ~data & DB9_RIGHT);

			parport_write_control(port, DB9_NORMAL);
			udelay(DB9_GENESIS6_DELAY);
			parport_write_control(port, DB9_NOSELECT); /* 4 */
			udelay(DB9_GENESIS6_DELAY);
			parport_write_control(port, DB9_NORMAL);
			break;

		case DB9_SATURN_PAD:

			parport_write_control(port, DB9_SATURN0);
			data = parport_read_data(port);

			input_report_key(dev, BTN_Y,  ~data & DB9_LEFT);
			input_report_key(dev, BTN_Z,  ~data & DB9_DOWN);
			input_report_key(dev, BTN_TL, ~data & DB9_UP);
			input_report_key(dev, BTN_TR, ~data & DB9_RIGHT);

			parport_write_control(port, DB9_SATURN2);
			data = parport_read_data(port);

			input_report_abs(dev, ABS_X, (data & DB9_RIGHT ? 0 : 1) - (data & DB9_LEFT ? 0 : 1));
			input_report_abs(dev, ABS_Y, (data & DB9_DOWN  ? 0 : 1) - (data & DB9_UP   ? 0 : 1));
			
			parport_write_control(port, DB9_NORMAL);
			data = parport_read_data(port);

			input_report_key(dev, BTN_A, ~data & DB9_LEFT);
			input_report_key(dev, BTN_B, ~data & DB9_UP);
			input_report_key(dev, BTN_C, ~data & DB9_DOWN);
			input_report_key(dev, BTN_X, ~data & DB9_RIGHT);
			break;

		case DB9_CD32_PAD:

			data=parport_read_data(port);

			input_report_abs(dev, ABS_X, (data & DB9_RIGHT ? 0 : 1) - (data & DB9_LEFT ? 0 : 1));
			input_report_abs(dev, ABS_Y, (data & DB9_DOWN  ? 0 : 1) - (data & DB9_UP   ? 0 : 1));

			parport_write_control(port, 0x0a); 

			for (i = 0; i < 7; i++) { 
				data = parport_read_data(port);
				parport_write_control(port, 0x02); 
				parport_write_control(port, 0x0a); 
				input_report_key(dev, db9_cd32_btn[i], ~data & DB9_FIRE2);
				}

			parport_write_control(port, 0x00); 
			break;
		}

	mod_timer(&db9->timer, jiffies + DB9_REFRESH_TIME);
}

static int db9_open(struct input_dev *dev)
{
	struct db9 *db9 = dev->private;
	struct parport *port = db9->pd->port;

	if (!db9->used++) {
		parport_claim(db9->pd);
		parport_write_data(port, 0xff);
		parport_data_reverse(port);
		parport_write_control(port, DB9_NORMAL);
		mod_timer(&db9->timer, jiffies + DB9_REFRESH_TIME);
	}

	return 0;
}

static void db9_close(struct input_dev *dev)
{
	struct db9 *db9 = dev->private;
	struct parport *port = db9->pd->port;

	if (!--db9->used) {
		del_timer(&db9->timer);
		parport_write_control(port, 0x00);
		parport_data_forward(port);
		parport_release(db9->pd);
	}
}

static struct db9 __init *db9_probe(int *config)
{
	struct db9 *db9;
	struct parport *pp;
	int i, j;

	if (config[0] < 0)
		return NULL;
	if (config[1] < 1 || config[1] >= DB9_MAX_PAD || !db9_buttons[config[1]]) {
		printk(KERN_ERR "db9.c: bad config\n");
		return NULL;
	}

	for (pp = parport_enumerate(); pp && (config[0] > 0); pp = pp->next)
		config[0]--;

	if (!pp) {
		printk(KERN_ERR "db9.c: no such parport\n");
		return NULL;
	}

	if (!(pp->modes & PARPORT_MODE_TRISTATE) && config[1] != DB9_MULTI_0802) {
		printk(KERN_ERR "db9.c: specified parport is not bidirectional\n");
		return NULL;
	}
	
	if (!(db9 = kmalloc(sizeof(struct db9), GFP_KERNEL)))
		return NULL;
	memset(db9, 0, sizeof(struct db9));

	db9->mode = config[1];
	init_timer(&db9->timer);
	db9->timer.data = (long) db9;
	db9->timer.function = db9_timer;

	db9->pd = parport_register_device(pp, "db9", NULL, NULL, NULL, PARPORT_DEV_EXCL, NULL);

	if (!db9->pd) {
		printk(KERN_ERR "db9.c: parport busy already - lp.o loaded?\n");
		kfree(db9);
		return NULL;
	}

	for (i = 0; i < 1 + (db9->mode == DB9_MULTI_0802_2); i++) {

		db9->dev[i].private = db9;
		db9->dev[i].open = db9_open;
		db9->dev[i].close = db9_close;

		db9->dev[i].name = db9_name[db9->mode];
		db9->dev[i].idbus = BUS_PARPORT;
		db9->dev[i].idvendor = 0x0002;
		db9->dev[i].idproduct = config[1];
		db9->dev[i].idversion = 0x0100;

		db9->dev[i].evbit[0] = BIT(EV_KEY) | BIT(EV_ABS);
		db9->dev[i].absbit[0] = BIT(ABS_X) | BIT(ABS_Y);

		for (j = 0; j < db9_buttons[db9->mode]; j++)
			set_bit(db9_btn[db9->mode][j], db9->dev[i].keybit); 

		db9->dev[i].absmin[ABS_X] = -1; db9->dev[i].absmax[ABS_X] = 1;
		db9->dev[i].absmin[ABS_Y] = -1; db9->dev[i].absmax[ABS_Y] = 1;

		input_register_device(db9->dev + i);
		printk(KERN_INFO "input%d: %s on %s\n",
			db9->dev[i].number, db9_name[db9->mode], db9->pd->port->name);
	}

	return db9;
}

#ifndef MODULE
int __init db9_setup(char *str)
{
	int i, ints[3];
	get_options(str, ARRAY_SIZE(ints), ints);
	for (i = 0; i <= ints[0] && i < 2; i++) db9[i] = ints[i + 1];
	return 1;
}
int __init db9_setup_2(char *str)
{
	int i, ints[3];
	get_options(str, ARRAY_SIZE(ints), ints);
	for (i = 0; i <= ints[0] && i < 2; i++) db9_2[i] = ints[i + 1];
	return 1;
}
int __init db9_setup_3(char *str)
{
	int i, ints[3];
	get_options(str, ARRAY_SIZE(ints), ints);
	for (i = 0; i <= ints[0] && i < 2; i++) db9_3[i] = ints[i + 1];
	return 1;
}
__setup("db9=", db9_setup);
__setup("db9_2=", db9_setup_2);
__setup("db9_3=", db9_setup_3);
#endif

int __init db9_init(void)
{
	db9_base[0] = db9_probe(db9);
	db9_base[1] = db9_probe(db9_2);
	db9_base[2] = db9_probe(db9_3);

	if (db9_base[0] || db9_base[1] || db9_base[2])
		return 0;

	return -ENODEV;
}

void __exit db9_exit(void)
{
	int i, j;

	for (i = 0; i < 3; i++) 
		if (db9_base[i]) {
			for (j = 0; j < 1 + (db9_base[i]->mode == DB9_MULTI_0802_2); j++)
				input_unregister_device(db9_base[i]->dev + j);
		parport_unregister_device(db9_base[i]->pd);
	}
}

module_init(db9_init);
module_exit(db9_exit);
