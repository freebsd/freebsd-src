#ifndef _SERIO_H
#define _SERIO_H

/*
 * $Id: serio.h,v 1.11 2001/05/29 02:58:50 jsimmons Exp $
 *
 * Copyright (C) 1999 Vojtech Pavlik
 *
 * Sponsored by SuSE
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

/*
 * The serial port set type ioctl.
 */

#include <linux/ioctl.h>
#define SPIOCSTYPE	_IOW('q', 0x01, unsigned long)

struct serio;

struct serio {

	void *private;
	void *driver;

	unsigned long type;
	int number;

	int (*write)(struct serio *, unsigned char);
	int (*open)(struct serio *);
	void (*close)(struct serio *);

	struct serio_dev *dev;

	struct serio *next;
};

struct serio_dev {

	void *private;

	void (*interrupt)(struct serio *, unsigned char, unsigned int);
	void (*connect)(struct serio *, struct serio_dev *dev);
	void (*disconnect)(struct serio *);

	struct serio_dev *next;
};

int serio_open(struct serio *serio, struct serio_dev *dev);
void serio_close(struct serio *serio);
void serio_rescan(struct serio *serio);

void serio_register_port(struct serio *serio);
void serio_unregister_port(struct serio *serio);
void serio_register_device(struct serio_dev *dev);
void serio_unregister_device(struct serio_dev *dev);

static __inline__ int serio_write(struct serio *serio, unsigned char data)
{
	return serio->write(serio, data);
}

#define SERIO_TIMEOUT	1
#define SERIO_PARITY	2

#define SERIO_TYPE	0xff000000UL
#define SERIO_XT	0x00000000UL
#define SERIO_8042	0x01000000UL
#define SERIO_RS232	0x02000000UL
#define SERIO_HIL_MLC	0x03000000UL

#define SERIO_PROTO	0xFFUL
#define SERIO_MSC	0x01
#define SERIO_SUN	0x02
#define SERIO_MS	0x03
#define SERIO_MP	0x04
#define SERIO_MZ	0x05
#define SERIO_MZP	0x06
#define SERIO_MZPP	0x07
#define SERIO_SUNKBD	0x10
#define SERIO_WARRIOR	0x18
#define SERIO_SPACEORB	0x19
#define SERIO_MAGELLAN	0x1a
#define SERIO_SPACEBALL	0x1b
#define SERIO_GUNZE	0x1c
#define SERIO_IFORCE	0x1d
#define SERIO_STINGER	0x1e
#define SERIO_NEWTON	0x1f
#define SERIO_STOWAWAY	0x20
#define SERIO_H3600	0x21
#define SERIO_PS2SER	0x22
#define SERIO_HIL	0x25

#define SERIO_ID	0xff00UL
#define SERIO_EXTRA	0xff0000UL

#endif
