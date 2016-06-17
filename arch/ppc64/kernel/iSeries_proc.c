/*
  * iSeries_proc.c
  * Copyright (C) 2001  Kyle A. Lucke IBM Corporation
  * 
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
  * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
  */


/* Change Activity: */
/* End Change Activity */

#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#ifndef _ISERIES_PROC_H
#include <asm/iSeries/iSeries_proc.h>
#endif


static struct proc_dir_entry * iSeries_proc_root = NULL;
static int iSeries_proc_initializationDone = 0;
static spinlock_t iSeries_proc_lock;

struct iSeries_proc_registration
{
	struct iSeries_proc_registration *next;
	iSeriesProcFunction functionMember;
};


struct iSeries_proc_registration preallocated[16];
#define MYQUEUETYPE(T) struct MYQueue##T
#define MYQUEUE(T) \
MYQUEUETYPE(T) \
{ \
	struct T *head; \
	struct T *tail; \
}
#define MYQUEUECTOR(q) do { (q)->head = NULL; (q)->tail = NULL; } while(0)
#define MYQUEUEENQ(q, p) \
do { \
	(p)->next = NULL; \
	if ((q)->head != NULL) { \
		(q)->head->next = (p); \
		(q)->head = (p); \
	} else { \
		(q)->tail = (q)->head = (p); \
	} \
} while(0)

#define MYQUEUEDEQ(q,p) \
do { \
	(p) = (q)->tail; \
	if ((p) != NULL) { \
		(q)->tail = (p)->next; \
		(p)->next = NULL; \
	} \
	if ((q)->tail == NULL) \
		(q)->head = NULL; \
} while(0)
MYQUEUE(iSeries_proc_registration);
typedef MYQUEUETYPE(iSeries_proc_registration) aQueue;


aQueue iSeries_free;
aQueue iSeries_queued;

void iSeries_proc_early_init(void)
{
	int i = 0;
	unsigned long flags;
	iSeries_proc_initializationDone = 0;
	spin_lock_init(&iSeries_proc_lock);
	MYQUEUECTOR(&iSeries_free);
	MYQUEUECTOR(&iSeries_queued);

	spin_lock_irqsave(&iSeries_proc_lock, flags);
	for (i = 0; i < 16; ++i) {
		MYQUEUEENQ(&iSeries_free, preallocated+i);
	}
	spin_unlock_irqrestore(&iSeries_proc_lock, flags);
}

void iSeries_proc_create(void)
{
	unsigned long flags;
	struct iSeries_proc_registration *reg = NULL;
	spin_lock_irqsave(&iSeries_proc_lock, flags);
	printk("iSeries_proc: Creating /proc/iSeries\n");

	iSeries_proc_root = proc_mkdir("iSeries", 0);
	if (!iSeries_proc_root) return;

	MYQUEUEDEQ(&iSeries_queued, reg);

	while (reg != NULL) {
		(*(reg->functionMember))(iSeries_proc_root);

		MYQUEUEDEQ(&iSeries_queued, reg);
	}

	iSeries_proc_initializationDone = 1;
	spin_unlock_irqrestore(&iSeries_proc_lock, flags);
}

void iSeries_proc_callback(iSeriesProcFunction initFunction)
{
	unsigned long flags;
	spin_lock_irqsave(&iSeries_proc_lock, flags);

	if (iSeries_proc_initializationDone) {
		(*initFunction)(iSeries_proc_root);
	} else {
		struct iSeries_proc_registration *reg = NULL;

		MYQUEUEDEQ(&iSeries_free, reg);

		if (reg != NULL) {
			/* printk("Registering %p in reg %p\n", initFunction, reg); */
			reg->functionMember = initFunction;

			MYQUEUEENQ(&iSeries_queued, reg);
		} else {
			printk("Couldn't get a queue entry\n");
		}
	}

	spin_unlock_irqrestore(&iSeries_proc_lock, flags);
}


