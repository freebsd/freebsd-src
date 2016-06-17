/*****************************************************************************/
/*
 *      auerbuf.h  --  Auerswald PBX/System Telephone urb list storage.
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

/* This module assembles together an URB, an usb_ctrlrequest struct for sending of
 * control messages, and a data buffer.
 * These items (auerbuf) are collected in a list (auerbufctl) and are used
 * for serialized usb data transfer.
 */

#ifndef AUERBUF_H
#define AUERBUF_H

#include <linux/usb.h>

/* buffer element */
struct auerbufctl;			/* forward */
struct auerbuf {
	unsigned char *bufp;		/* reference to allocated data buffer */
	unsigned int len;		/* number of characters in data buffer */
	unsigned int retries;		/* for urb retries */
	struct usb_ctrlrequest *dr;	/* for setup data in control messages */
	struct urb *urbp;		/* USB urb */
	struct auerbufctl *list;	/* pointer to list */
	struct list_head buff_list;	/* reference to next buffer in list */
};

/* buffer list control block */
struct auerbufctl {
	spinlock_t lock;		/* protection in interrupt */
	struct list_head free_buff_list;/* free buffers */
	struct list_head rec_buff_list;	/* buffers with received data */
};

/* Function prototypes */
void auerbuf_free(struct auerbuf *bp);

void auerbuf_free_list(struct list_head *q);

void auerbuf_init(struct auerbufctl *bcp);

void auerbuf_free_buffers(struct auerbufctl *bcp);

int auerbuf_setup(struct auerbufctl *bcp, unsigned int numElements,
		  unsigned int bufsize);

struct auerbuf *auerbuf_getbuf(struct auerbufctl *bcp);

void auerbuf_releasebuf(struct auerbuf *bp);

#endif	/* AUERBUF_H */
