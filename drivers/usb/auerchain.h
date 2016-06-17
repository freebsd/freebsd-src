/*****************************************************************************/
/*
 *      auerchain.h  --  Auerswald PBX/System Telephone chained urb support.
 *
 *      Copyright (C) 2002  Wolfgang Mües (wolfgang@iksw-muees.de)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
 /*****************************************************************************/

/* This module is used to make a FIFO of URBs, to serialize the submit.
 * This may be used to serialize control messages, which is not supported
 * by the Linux USB subsystem.
 */

#ifndef AUERCHAIN_H
#define AUERCHAIN_H

#include <linux/usb.h>

/* urb chain element */
struct auerchain;			/* forward for circular reference */
struct auerchainelement {
	struct auerchain *chain;	/* pointer to the chain to which this element belongs */
	struct urb *urbp;		/* pointer to attached urb */
	void *context;			/* saved URB context */
	usb_complete_t complete;	/* saved URB completion function */
	struct list_head list;		/* to include element into a list */
};

/* urb chain */
struct auerchain {
	struct auerchainelement *active;/* element which is submitted to urb */
	spinlock_t lock;		/* protection agains interrupts */
	struct list_head waiting_list;	/* list of waiting elements */
	struct list_head free_list;	/* list of available elements */
};

/* urb blocking completion helper struct */
struct auerchain_chs {
	wait_queue_head_t wqh;		/* wait for completion */
	unsigned int done;		/* completion flag */
};


/* Function prototypes */
int auerchain_submit_urb_list(struct auerchain *acp, struct urb *urb,
			      int early);

int auerchain_submit_urb(struct auerchain *acp, struct urb *urb);

int auerchain_unlink_urb(struct auerchain *acp, struct urb *urb);

void auerchain_unlink_all(struct auerchain *acp);

void auerchain_free(struct auerchain *acp);

void auerchain_init(struct auerchain *acp);

int auerchain_setup(struct auerchain *acp, unsigned int numElements);

int auerchain_control_msg(struct auerchain *acp, struct usb_device *dev,
			  unsigned int pipe, __u8 request,
			  __u8 requesttype, __u16 value, __u16 index,
			  void *data, __u16 size, int timeout);

#endif	/* AUERCHAIN_H */
