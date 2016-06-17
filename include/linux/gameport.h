#ifndef _GAMEPORT_H
#define _GAMEPORT_H

/*
 * $Id: gameport.h,v 1.11 2001/04/26 10:24:46 vojtech Exp $
 *
 *  Copyright (c) 1999-2000 Vojtech Pavlik
 *
 *  Sponsored by SuSE
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
 * e-mail - mail your message to <vojtech@ucw.cz>, or by paper mail:
 * Vojtech Pavlik, Ucitelska 1576, Prague 8, 182 00 Czech Republic
 */

#include <linux/sched.h>
#include <linux/delay.h>
#include <asm/io.h>

struct gameport;

struct gameport {

	void *private;
	void *driver;

	int number;

	int io;
	int speed;
	int fuzz;

	void (*trigger)(struct gameport *);
	unsigned char (*read)(struct gameport *);
	int (*cooked_read)(struct gameport *, int *, int *);
	int (*calibrate)(struct gameport *, int *, int *);
	int (*open)(struct gameport *, int);
	void (*close)(struct gameport *);

	struct gameport_dev *dev;
	struct gameport *next;
};

struct gameport_dev {

	void *private;

	void (*connect)(struct gameport *, struct gameport_dev *dev);
	void (*disconnect)(struct gameport *);

	struct gameport_dev *next;
};

int gameport_open(struct gameport *gameport, struct gameport_dev *dev, int mode);
void gameport_close(struct gameport *gameport);
void gameport_rescan(struct gameport *gameport);

#if defined(CONFIG_INPUT_GAMEPORT) || defined(CONFIG_INPUT_GAMEPORT_MODULE)
void gameport_register_port(struct gameport *gameport);
void gameport_unregister_port(struct gameport *gameport);
#else
static void __inline__ gameport_register_port(struct gameport *gameport) { return; }
static void __inline__ gameport_unregister_port(struct gameport *gameport) { return; }
#endif

void gameport_register_device(struct gameport_dev *dev);
void gameport_unregister_device(struct gameport_dev *dev);

#define GAMEPORT_MODE_DISABLED		0
#define GAMEPORT_MODE_RAW		1
#define GAMEPORT_MODE_COOKED		2

#define GAMEPORT_ID_VENDOR_ANALOG	0x0001
#define GAMEPORT_ID_VENDOR_MADCATZ	0x0002
#define GAMEPORT_ID_VENDOR_LOGITECH	0x0003
#define GAMEPORT_ID_VENDOR_CREATIVE	0x0004
#define GAMEPORT_ID_VENDOR_GENIUS	0x0005
#define GAMEPORT_ID_VENDOR_INTERACT	0x0006
#define GAMEPORT_ID_VENDOR_MICROSOFT	0x0007
#define GAMEPORT_ID_VENDOR_THRUSTMASTER	0x0008
#define GAMEPORT_ID_VENDOR_GRAVIS	0x0009

static __inline__ void gameport_trigger(struct gameport *gameport)
{
	if (gameport->trigger)
		gameport->trigger(gameport);
	else
		outb(0xff, gameport->io);
}

static __inline__ unsigned char gameport_read(struct gameport *gameport)
{
	if (gameport->read)
		return gameport->read(gameport);
	else
		return inb(gameport->io);
}

static __inline__ int gameport_cooked_read(struct gameport *gameport, int *axes, int *buttons)
{
	if (gameport->cooked_read)
		return gameport->cooked_read(gameport, axes, buttons);
	else
		return -1;
}

static __inline__ int gameport_calibrate(struct gameport *gameport, int *axes, int *max)
{
	if (gameport->calibrate)
		return gameport->calibrate(gameport, axes, max);
	else
		return -1;
}

static __inline__ int gameport_time(struct gameport *gameport, int time)
{
	return (time * gameport->speed) / 1000;
}

static __inline__ void wait_ms(unsigned int ms)
{
	current->state = TASK_UNINTERRUPTIBLE;
	schedule_timeout(1 + ms * HZ / 1000);
}

#endif
