/*
 * $Id: serio.c,v 1.5 2000/06/04 17:44:59 vojtech Exp $
 *
 *  Copyright (c) 1999-2000 Vojtech Pavlik
 *
 *  Sponsored by SuSE
 */

/*
 *  The Serio abstraction module
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

#include <linux/stddef.h>
#include <linux/module.h>
#include <linux/serio.h>

MODULE_AUTHOR("Vojtech Pavlik <vojtech@ucw.cz>");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(serio_register_port);
EXPORT_SYMBOL(serio_unregister_port);
EXPORT_SYMBOL(serio_register_device);
EXPORT_SYMBOL(serio_unregister_device);
EXPORT_SYMBOL(serio_open);
EXPORT_SYMBOL(serio_close);
EXPORT_SYMBOL(serio_rescan);

static struct serio *serio_list;
static struct serio_dev *serio_dev;
static int serio_number;

static void serio_find_dev(struct serio *serio)
{
        struct serio_dev *dev = serio_dev;

        while (dev && !serio->dev) {
		if (dev->connect)
                	dev->connect(serio, dev);
                dev = dev->next;
        }
}

void serio_rescan(struct serio *serio)
{
	if (serio->dev && serio->dev->disconnect)
		serio->dev->disconnect(serio);
	serio_find_dev(serio);
}

void serio_register_port(struct serio *serio)
{
	serio->number = serio_number++;
	serio->next = serio_list;	
	serio_list = serio;
	serio_find_dev(serio);
}

void serio_unregister_port(struct serio *serio)
{
        struct serio **serioptr = &serio_list;

        while (*serioptr && (*serioptr != serio)) serioptr = &((*serioptr)->next);
        *serioptr = (*serioptr)->next;

	if (serio->dev && serio->dev->disconnect)
		serio->dev->disconnect(serio);

	serio_number--;
}

void serio_register_device(struct serio_dev *dev)
{
	struct serio *serio = serio_list;

	dev->next = serio_dev;	
	serio_dev = dev;

	while (serio) {
		if (!serio->dev && dev->connect)
			dev->connect(serio, dev);
		serio = serio->next;
	}
}

void serio_unregister_device(struct serio_dev *dev)
{
        struct serio_dev **devptr = &serio_dev;
	struct serio *serio = serio_list;

        while (*devptr && (*devptr != dev)) devptr = &((*devptr)->next);
        *devptr = (*devptr)->next;

	while (serio) {
		if (serio->dev == dev && dev->disconnect)
			dev->disconnect(serio);
		serio_find_dev(serio);
		serio = serio->next;
	}
}

int serio_open(struct serio *serio, struct serio_dev *dev)
{
	if (serio->open(serio))
		return -1;
	serio->dev = dev;
	return 0;
}

void serio_close(struct serio *serio)
{
	serio->close(serio);
	serio->dev = NULL;
}
