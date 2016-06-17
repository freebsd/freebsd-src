/*****************************************************************************/
/*
 *      auerbuf.c  --  Auerswald PBX/System Telephone urb list storage.
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

#undef DEBUG			/* include debug macros until it's done */
#include <linux/usb.h>
#include "auerbuf.h"
#include <linux/slab.h>

/* free a single auerbuf */
void auerbuf_free(struct auerbuf *bp)
{
	if (!bp) return;
	kfree(bp->bufp);
	kfree(bp->dr);
	if (bp->urbp) {
		usb_free_urb(bp->urbp);
	}
	kfree(bp);
}

/* free the buffers from an auerbuf list */
void auerbuf_free_list(struct list_head *q)
{
	struct list_head *tmp;
	struct list_head *p;
	struct auerbuf *bp;

	dbg("auerbuf_free_list");
	for (p = q->next; p != q;) {
		bp = list_entry(p, struct auerbuf, buff_list);
		tmp = p->next;
		list_del(p);
		p = tmp;
		auerbuf_free(bp);
	}
}

/* free all buffers from an auerbuf chain */
void auerbuf_free_buffers(struct auerbufctl *bcp)
{
	unsigned long flags;
	dbg("auerbuf_free_buffers");

	spin_lock_irqsave(&bcp->lock, flags);

	auerbuf_free_list(&bcp->free_buff_list);
	auerbuf_free_list(&bcp->rec_buff_list);

	spin_unlock_irqrestore(&bcp->lock, flags);
}

/* init the members of a list control block */
void auerbuf_init(struct auerbufctl *bcp)
{
	dbg("auerbuf_init");
	spin_lock_init(&bcp->lock);
	INIT_LIST_HEAD(&bcp->free_buff_list);
	INIT_LIST_HEAD(&bcp->rec_buff_list);
}

/* setup a list of buffers */
/* requirement: auerbuf_init() */
int auerbuf_setup(struct auerbufctl *bcp, unsigned int numElements,
		  unsigned int bufsize)
{
	struct auerbuf *bep = NULL;

	dbg("auerbuf_setup called with %d elements of %d bytes",
	    numElements, bufsize);

	/* fill the list of free elements */
	for (; numElements; numElements--) {
		bep =
		    (struct auerbuf *) kmalloc(sizeof(struct auerbuf),
					       GFP_KERNEL);
		if (!bep)
			goto bl_fail;
		memset(bep, 0, sizeof(struct auerbuf));
		bep->list = bcp;
		INIT_LIST_HEAD(&bep->buff_list);
		bep->bufp = (char *) kmalloc(bufsize, GFP_KERNEL);
		if (!bep->bufp)
			goto bl_fail;
		bep->dr =
		    (struct usb_ctrlrequest *)
		    kmalloc(sizeof(struct usb_ctrlrequest), GFP_KERNEL);
		if (!bep->dr)
			goto bl_fail;
		bep->urbp = usb_alloc_urb(0);
		if (!bep->urbp)
			goto bl_fail;
		list_add_tail(&bep->buff_list, &bcp->free_buff_list);
	}
	return 0;

      bl_fail:			/* not enought memory. Free allocated elements */
	dbg("auerbuf_setup: no more memory");
	auerbuf_free (bep);
	auerbuf_free_buffers(bcp);
	return -ENOMEM;
}

/* alloc a free buffer from the list. Returns NULL if no buffer available */
struct auerbuf *auerbuf_getbuf(struct auerbufctl *bcp)
{
	unsigned long flags;
	struct auerbuf *bp = NULL;

	spin_lock_irqsave(&bcp->lock, flags);
	if (!list_empty(&bcp->free_buff_list)) {
		/* yes: get the entry */
		struct list_head *tmp = bcp->free_buff_list.next;
		list_del(tmp);
		bp = list_entry(tmp, struct auerbuf, buff_list);
	}
	spin_unlock_irqrestore(&bcp->lock, flags);
	return bp;
}

/* insert a used buffer into the free list */
void auerbuf_releasebuf(struct auerbuf *bp)
{
	unsigned long flags;
	struct auerbufctl *bcp = bp->list;
	bp->retries = 0;

	dbg("auerbuf_releasebuf called");
	spin_lock_irqsave(&bcp->lock, flags);
	list_add_tail(&bp->buff_list, &bcp->free_buff_list);
	spin_unlock_irqrestore(&bcp->lock, flags);
}
