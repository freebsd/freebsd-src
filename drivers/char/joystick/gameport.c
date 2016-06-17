/*
 * $Id: gameport.c,v 1.5 2000/05/29 10:54:53 vojtech Exp $
 *
 *  Copyright (c) 1999-2000 Vojtech Pavlik
 *
 *  Sponsored by SuSE
 */

/*
 * Generic gameport layer
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

#include <asm/io.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/gameport.h>
#include <linux/slab.h>
#include <linux/isapnp.h>
#include <linux/stddef.h>
#include <linux/delay.h>

MODULE_AUTHOR("Vojtech Pavlik <vojtech@suse.cz>");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(gameport_register_port);
EXPORT_SYMBOL(gameport_unregister_port);
EXPORT_SYMBOL(gameport_register_device);
EXPORT_SYMBOL(gameport_unregister_device);
EXPORT_SYMBOL(gameport_open);
EXPORT_SYMBOL(gameport_close);
EXPORT_SYMBOL(gameport_rescan);
EXPORT_SYMBOL(gameport_cooked_read);

static struct gameport *gameport_list;
static struct gameport_dev *gameport_dev;
static int gameport_number;

/*
 * gameport_measure_speed() measures the gameport i/o speed.
 */

static int gameport_measure_speed(struct gameport *gameport)
{
#if defined(__i386__) || defined(__x86_64__)

#define GET_TIME(x)     do { outb(0, 0x43); x = inb(0x40); x |= inb(0x40) << 8; } while (0)
#define DELTA(x,y)      ((y)-(x)+((y)<(x)?1193180L/HZ:0))

	unsigned int i, t, t1, t2, t3, tx;
	unsigned long flags;

	if (gameport_open(gameport, NULL, GAMEPORT_MODE_RAW))
		return 0;

	tx = 1 << 30;

	for(i = 0; i < 50; i++) {
		save_flags(flags);	/* Yes, all CPUs */
		cli();
		GET_TIME(t1);
		for(t = 0; t < 50; t++) gameport_read(gameport);
		GET_TIME(t2);
		GET_TIME(t3);
		restore_flags(flags);
		udelay(i * 10);
		if ((t = DELTA(t2,t1) - DELTA(t3,t2)) < tx) tx = t;
	}

	return 59659 / (tx < 1 ? 1 : tx);

#else

	unsigned int j, t = 0;

	j = jiffies; while (j == jiffies);
	j = jiffies; while (j == jiffies) { t++; gameport_read(gameport); }

	return t * HZ / 1000;

#endif

	gameport_close(gameport);
}

static void gameport_find_dev(struct gameport *gameport)
{
        struct gameport_dev *dev = gameport_dev;

        while (dev && !gameport->dev) {
		if (dev->connect)
                	dev->connect(gameport, dev);
                dev = dev->next;
        }
}

void gameport_rescan(struct gameport *gameport)
{
	gameport_close(gameport);
	gameport_find_dev(gameport);
}

void gameport_register_port(struct gameport *gameport)
{
	gameport->number = gameport_number++;
	gameport->next = gameport_list;	
	gameport_list = gameport;

	gameport->speed = gameport_measure_speed(gameport);

	gameport_find_dev(gameport);
}

void gameport_unregister_port(struct gameport *gameport)
{
        struct gameport **gameportptr = &gameport_list;

        while (*gameportptr && (*gameportptr != gameport)) gameportptr = &((*gameportptr)->next);
        *gameportptr = (*gameportptr)->next;

	if (gameport->dev && gameport->dev->disconnect)
		gameport->dev->disconnect(gameport);

	gameport_number--;
}

void gameport_register_device(struct gameport_dev *dev)
{
	struct gameport *gameport = gameport_list;

	dev->next = gameport_dev;	
	gameport_dev = dev;

	while (gameport) {
		if (!gameport->dev && dev->connect)
			dev->connect(gameport, dev);
		gameport = gameport->next;
	}
}

void gameport_unregister_device(struct gameport_dev *dev)
{
        struct gameport_dev **devptr = &gameport_dev;
	struct gameport *gameport = gameport_list;

        while (*devptr && (*devptr != dev)) devptr = &((*devptr)->next);
        *devptr = (*devptr)->next;

	while (gameport) {
		if (gameport->dev == dev && dev->disconnect)
			dev->disconnect(gameport);
		gameport_find_dev(gameport);
		gameport = gameport->next;
	}
}

int gameport_open(struct gameport *gameport, struct gameport_dev *dev, int mode)
{
	if (gameport->open) {
		if (gameport->open(gameport, mode))
			return -1;
	} else {
		if (mode != GAMEPORT_MODE_RAW)
			return -1;
	}

	if (gameport->dev)
		return -1;

	gameport->dev = dev;
	
	return 0;
}

void gameport_close(struct gameport *gameport)
{
	gameport->dev = NULL;
	if (gameport->close) gameport->close(gameport);
}
