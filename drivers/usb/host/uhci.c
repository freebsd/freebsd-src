/*
 * Universal Host Controller Interface driver for USB.
 *
 * Maintainer: Johannes Erdfelt <johannes@erdfelt.com>
 *
 * (C) Copyright 1999 Linus Torvalds
 * (C) Copyright 1999-2002 Johannes Erdfelt, johannes@erdfelt.com
 * (C) Copyright 1999 Randy Dunlap
 * (C) Copyright 1999 Georg Acher, acher@in.tum.de
 * (C) Copyright 1999 Deti Fliegl, deti@fliegl.de
 * (C) Copyright 1999 Thomas Sailer, sailer@ife.ee.ethz.ch
 * (C) Copyright 1999 Roman Weissgaerber, weissg@vienna.at
 * (C) Copyright 2000 Yggdrasil Computing, Inc. (port of new PCI interface
 *               support from usb-ohci.c by Adam Richter, adam@yggdrasil.com).
 * (C) Copyright 1999 Gregory P. Smith (from usb-ohci.c)
 *
 * Intel documents this fairly well, and as far as I know there
 * are no royalties or anything like that, but even so there are
 * people who decided that they want to do the same thing in a
 * completely different way.
 *
 * WARNING! The USB documentation is downright evil. Most of it
 * is just crap, written by a committee. You're better off ignoring
 * most of it, the important stuff is:
 *  - the low-level protocol (fairly simple but lots of small details)
 *  - working around the horridness of the rest
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/errno.h>
#include <linux/unistd.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#ifdef CONFIG_USB_DEBUG
#define DEBUG
#else
#undef DEBUG
#endif
#include <linux/usb.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>

#include "uhci.h"

#include <linux/pm.h>

#include "../hcd.h"

/*
 * Version Information
 */
#define DRIVER_VERSION "v1.1"
#define DRIVER_AUTHOR "Linus 'Frodo Rabbit' Torvalds, Johannes Erdfelt, Randy Dunlap, Georg Acher, Deti Fliegl, Thomas Sailer, Roman Weissgaerber"
#define DRIVER_DESC "USB Universal Host Controller Interface driver"

/*
 * debug = 0, no debugging messages
 * debug = 1, dump failed URB's except for stalls
 * debug = 2, dump all failed URB's (including stalls)
 *            show all queues in /proc/uhci/hc*
 * debug = 3, show all TD's in URB's when dumping
 */
#ifdef DEBUG
static int debug = 1;
#else
static int debug = 0;
#endif
MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debug level");
static char *errbuf;
#define ERRBUF_LEN    (PAGE_SIZE * 8)

#include "uhci-debug.h"

static kmem_cache_t *uhci_up_cachep;	/* urb_priv */

static int rh_submit_urb(struct urb *urb);
static int rh_unlink_urb(struct urb *urb);
static int uhci_get_current_frame_number(struct usb_device *dev);
static int uhci_unlink_urb(struct urb *urb);
static void uhci_unlink_generic(struct uhci *uhci, struct urb *urb);
static void uhci_call_completion(struct urb *urb);

static int  ports_active(struct uhci *uhci);
static void suspend_hc(struct uhci *uhci);
static void wakeup_hc(struct uhci *uhci);

/* If a transfer is still active after this much time, turn off FSBR */
#define IDLE_TIMEOUT	(HZ / 20)	/* 50 ms */
#define FSBR_DELAY	(HZ / 20)	/* 50 ms */

/* When we timeout an idle transfer for FSBR, we'll switch it over to */
/* depth first traversal. We'll do it in groups of this number of TD's */
/* to make sure it doesn't hog all of the bandwidth */
#define DEPTH_INTERVAL	5

#define MAX_URB_LOOP	2048		/* Maximum number of linked URB's */

/*
 * Only the USB core should call uhci_alloc_dev and uhci_free_dev
 */
static int uhci_alloc_dev(struct usb_device *dev)
{
	return 0;
}

static int uhci_free_dev(struct usb_device *dev)
{
	return 0;
}

/*
 * Technically, updating td->status here is a race, but it's not really a
 * problem. The worst that can happen is that we set the IOC bit again
 * generating a spurios interrupt. We could fix this by creating another
 * QH and leaving the IOC bit always set, but then we would have to play
 * games with the FSBR code to make sure we get the correct order in all
 * the cases. I don't think it's worth the effort
 */
static inline void uhci_set_next_interrupt(struct uhci *uhci)
{
	unsigned long flags;

	spin_lock_irqsave(&uhci->frame_list_lock, flags);
	uhci->skel_term_td->status |= TD_CTRL_IOC;
	spin_unlock_irqrestore(&uhci->frame_list_lock, flags);
}

static inline void uhci_clear_next_interrupt(struct uhci *uhci)
{
	unsigned long flags;

	spin_lock_irqsave(&uhci->frame_list_lock, flags);
	uhci->skel_term_td->status &= ~TD_CTRL_IOC;
	spin_unlock_irqrestore(&uhci->frame_list_lock, flags);
}

static inline void uhci_add_complete(struct urb *urb)
{
	struct uhci *uhci = (struct uhci *)urb->dev->bus->hcpriv;
	struct urb_priv *urbp = (struct urb_priv *)urb->hcpriv;
	unsigned long flags;

	spin_lock_irqsave(&uhci->complete_list_lock, flags);
	list_add_tail(&urbp->complete_list, &uhci->complete_list);
	spin_unlock_irqrestore(&uhci->complete_list_lock, flags);
}

static struct uhci_td *uhci_alloc_td(struct uhci *uhci, struct usb_device *dev)
{
	dma_addr_t dma_handle;
	struct uhci_td *td;

	td = pci_pool_alloc(uhci->td_pool, GFP_DMA | GFP_ATOMIC, &dma_handle);
	if (!td)
		return NULL;

	td->dma_handle = dma_handle;

	td->link = UHCI_PTR_TERM;
	td->buffer = 0;

	td->frame = -1;
	td->dev = dev;

	INIT_LIST_HEAD(&td->list);
	INIT_LIST_HEAD(&td->fl_list);

	usb_inc_dev_use(dev);

	return td;
}

static void inline uhci_fill_td(struct uhci_td *td, __u32 status,
		__u32 info, __u32 buffer)
{
	td->status = status;
	td->info = info;
	td->buffer = buffer;
}

static void uhci_insert_td(struct uhci *uhci, struct uhci_td *skeltd, struct uhci_td *td)
{
	unsigned long flags;
	struct uhci_td *ltd;

	spin_lock_irqsave(&uhci->frame_list_lock, flags);

	ltd = list_entry(skeltd->fl_list.prev, struct uhci_td, fl_list);

	td->link = ltd->link;
	mb();
	ltd->link = td->dma_handle;

	list_add_tail(&td->fl_list, &skeltd->fl_list);

	spin_unlock_irqrestore(&uhci->frame_list_lock, flags);
}

/*
 * We insert Isochronous transfers directly into the frame list at the
 * beginning
 * The layout looks as follows:
 * frame list pointer -> iso td's (if any) ->
 * periodic interrupt td (if frame 0) -> irq td's -> control qh -> bulk qh
 */
static void uhci_insert_td_frame_list(struct uhci *uhci, struct uhci_td *td, unsigned framenum)
{
	unsigned long flags;

	framenum %= UHCI_NUMFRAMES;

	spin_lock_irqsave(&uhci->frame_list_lock, flags);

	td->frame = framenum;

	/* Is there a TD already mapped there? */
	if (uhci->fl->frame_cpu[framenum]) {
		struct uhci_td *ftd, *ltd;

		ftd = uhci->fl->frame_cpu[framenum];
		ltd = list_entry(ftd->fl_list.prev, struct uhci_td, fl_list);

		list_add_tail(&td->fl_list, &ftd->fl_list);

		td->link = ltd->link;
		mb();
		ltd->link = td->dma_handle;
	} else {
		td->link = uhci->fl->frame[framenum];
		mb();
		uhci->fl->frame[framenum] = td->dma_handle;
		uhci->fl->frame_cpu[framenum] = td;
	}

	spin_unlock_irqrestore(&uhci->frame_list_lock, flags);
}

static void uhci_remove_td(struct uhci *uhci, struct uhci_td *td)
{
	unsigned long flags;

	/* If it's not inserted, don't remove it */
	spin_lock_irqsave(&uhci->frame_list_lock, flags);
	if (td->frame == -1 && list_empty(&td->fl_list))
		goto out;

	if (td->frame != -1 && uhci->fl->frame_cpu[td->frame] == td) {
		if (list_empty(&td->fl_list)) {
			uhci->fl->frame[td->frame] = td->link;
			uhci->fl->frame_cpu[td->frame] = NULL;
		} else {
			struct uhci_td *ntd;

			ntd = list_entry(td->fl_list.next, struct uhci_td, fl_list);
			uhci->fl->frame[td->frame] = ntd->dma_handle;
			uhci->fl->frame_cpu[td->frame] = ntd;
		}
	} else {
		struct uhci_td *ptd;

		ptd = list_entry(td->fl_list.prev, struct uhci_td, fl_list);
		ptd->link = td->link;
	}

	mb();
	td->link = UHCI_PTR_TERM;

	list_del_init(&td->fl_list);
	td->frame = -1;

out:
	spin_unlock_irqrestore(&uhci->frame_list_lock, flags);
}

/*
 * Inserts a td into qh list at the top.
 */
static void uhci_insert_tds_in_qh(struct uhci_qh *qh, struct urb *urb, int breadth)
{
	struct list_head *tmp, *head;
	struct urb_priv *urbp = (struct urb_priv *)urb->hcpriv;
	struct uhci_td *td, *ptd;

	if (list_empty(&urbp->td_list))
		return;

	head = &urbp->td_list;
	tmp = head->next;

	/* Ordering isn't important here yet since the QH hasn't been */
	/*  inserted into the schedule yet */
	td = list_entry(tmp, struct uhci_td, list);

	/* Add the first TD to the QH element pointer */
	qh->element = td->dma_handle | (breadth ? 0 : UHCI_PTR_DEPTH);

	ptd = td;

	/* Then link the rest of the TD's */
	tmp = tmp->next;
	while (tmp != head) {
		td = list_entry(tmp, struct uhci_td, list);

		tmp = tmp->next;

		ptd->link = td->dma_handle | (breadth ? 0 : UHCI_PTR_DEPTH);

		ptd = td;
	}

	ptd->link = UHCI_PTR_TERM;
}

static void uhci_free_td(struct uhci *uhci, struct uhci_td *td)
{
	if (!list_empty(&td->list) || !list_empty(&td->fl_list))
		dbg("td is still in URB list!");

	if (td->dev)
		usb_dec_dev_use(td->dev);

	pci_pool_free(uhci->td_pool, td, td->dma_handle);
}

static struct uhci_qh *uhci_alloc_qh(struct uhci *uhci, struct usb_device *dev)
{
	dma_addr_t dma_handle;
	struct uhci_qh *qh;

	qh = pci_pool_alloc(uhci->qh_pool, GFP_DMA | GFP_ATOMIC, &dma_handle);
	if (!qh)
		return NULL;

	qh->dma_handle = dma_handle;

	qh->element = UHCI_PTR_TERM;
	qh->link = UHCI_PTR_TERM;

	qh->dev = dev;
	qh->urbp = NULL;

	INIT_LIST_HEAD(&qh->list);
	INIT_LIST_HEAD(&qh->remove_list);

	usb_inc_dev_use(dev);

	return qh;
}

static void uhci_free_qh(struct uhci *uhci, struct uhci_qh *qh)
{
	if (!list_empty(&qh->list))
		dbg("qh list not empty!");
	if (!list_empty(&qh->remove_list))
		dbg("qh still in remove_list!");

	if (qh->dev)
		usb_dec_dev_use(qh->dev);

	pci_pool_free(uhci->qh_pool, qh, qh->dma_handle);
}

/*
 * MUST be called with uhci->frame_list_lock acquired
 */
static void _uhci_insert_qh(struct uhci *uhci, struct uhci_qh *skelqh, struct urb *urb)
{
	struct urb_priv *urbp = (struct urb_priv *)urb->hcpriv;
	struct list_head *head, *tmp;
	struct uhci_qh *lqh;

	/* Grab the last QH */
	lqh = list_entry(skelqh->list.prev, struct uhci_qh, list);

	if (lqh->urbp) {
		head = &lqh->urbp->queue_list;
		tmp = head->next;
		while (head != tmp) {
			struct urb_priv *turbp =
				list_entry(tmp, struct urb_priv, queue_list);

			tmp = tmp->next;

			turbp->qh->link = urbp->qh->dma_handle | UHCI_PTR_QH;
		}
	}

	head = &urbp->queue_list;
	tmp = head->next;
	while (head != tmp) {
		struct urb_priv *turbp =
			list_entry(tmp, struct urb_priv, queue_list);

		tmp = tmp->next;

		turbp->qh->link = lqh->link;
	}

	urbp->qh->link = lqh->link;
	mb();				/* Ordering is important */
	lqh->link = urbp->qh->dma_handle | UHCI_PTR_QH;

	list_add_tail(&urbp->qh->list, &skelqh->list);
}

static void uhci_insert_qh(struct uhci *uhci, struct uhci_qh *skelqh, struct urb *urb)
{
	unsigned long flags;

	spin_lock_irqsave(&uhci->frame_list_lock, flags);
	_uhci_insert_qh(uhci, skelqh, urb);
	spin_unlock_irqrestore(&uhci->frame_list_lock, flags);
}

static void uhci_remove_qh(struct uhci *uhci, struct uhci_qh *qh)
{
	unsigned long flags;
	struct uhci_qh *pqh;

	if (!qh)
		return;

	qh->urbp = NULL;

	/* Only go through the hoops if it's actually linked in */
	spin_lock_irqsave(&uhci->frame_list_lock, flags);
	if (!list_empty(&qh->list)) {
		pqh = list_entry(qh->list.prev, struct uhci_qh, list);

		if (pqh->urbp) {
			struct list_head *head, *tmp;

			head = &pqh->urbp->queue_list;
			tmp = head->next;
			while (head != tmp) {
				struct urb_priv *turbp =
					list_entry(tmp, struct urb_priv, queue_list);

				tmp = tmp->next;

				turbp->qh->link = qh->link;
			}
		}

		pqh->link = qh->link;
		mb();
		qh->element = qh->link = UHCI_PTR_TERM;

		list_del_init(&qh->list);
	}
	spin_unlock_irqrestore(&uhci->frame_list_lock, flags);

	spin_lock_irqsave(&uhci->qh_remove_list_lock, flags);

	/* Check to see if the remove list is empty. Set the IOC bit */
	/* to force an interrupt so we can remove the QH */
	if (list_empty(&uhci->qh_remove_list))
		uhci_set_next_interrupt(uhci);

	list_add(&qh->remove_list, &uhci->qh_remove_list);

	spin_unlock_irqrestore(&uhci->qh_remove_list_lock, flags);
}

static int uhci_fixup_toggle(struct urb *urb, unsigned int toggle)
{
	struct urb_priv *urbp = (struct urb_priv *)urb->hcpriv;
	struct list_head *head, *tmp;

	head = &urbp->td_list;
	tmp = head->next;
	while (head != tmp) {
		struct uhci_td *td = list_entry(tmp, struct uhci_td, list);

		tmp = tmp->next;

		if (toggle)
			td->info |= TD_TOKEN_TOGGLE;
		else
			td->info &= ~TD_TOKEN_TOGGLE;

		toggle ^= 1;
	}

	return toggle;
}

/* This function will append one URB's QH to another URB's QH. This is for */
/*  USB_QUEUE_BULK support for bulk transfers and soon implicitily for */
/*  control transfers */
static void uhci_append_queued_urb(struct uhci *uhci, struct urb *eurb, struct urb *urb)
{
	struct urb_priv *eurbp, *urbp, *furbp, *lurbp;
	struct list_head *tmp;
	struct uhci_td *lltd;
	unsigned long flags;

	eurbp = eurb->hcpriv;
	urbp = urb->hcpriv;

	spin_lock_irqsave(&uhci->frame_list_lock, flags);

	/* Find the first URB in the queue */
	if (eurbp->queued) {
		struct list_head *head = &eurbp->queue_list;

		tmp = head->next;
		while (tmp != head) {
			struct urb_priv *turbp =
				list_entry(tmp, struct urb_priv, queue_list);

			if (!turbp->queued)
				break;

			tmp = tmp->next;
		}
	} else
		tmp = &eurbp->queue_list;

	furbp = list_entry(tmp, struct urb_priv, queue_list);
	lurbp = list_entry(furbp->queue_list.prev, struct urb_priv, queue_list);

	lltd = list_entry(lurbp->td_list.prev, struct uhci_td, list);

	usb_settoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe),
		uhci_fixup_toggle(urb, uhci_toggle(lltd->info) ^ 1));

	/* All qh's in the queue need to link to the next queue */
	urbp->qh->link = eurbp->qh->link;

	mb();			/* Make sure we flush everything */
	/* Only support bulk right now, so no depth */
	lltd->link = urbp->qh->dma_handle | UHCI_PTR_QH;

	list_add_tail(&urbp->queue_list, &furbp->queue_list);

	urbp->queued = 1;

	spin_unlock_irqrestore(&uhci->frame_list_lock, flags);
}

static void uhci_delete_queued_urb(struct uhci *uhci, struct urb *urb)
{
	struct urb_priv *urbp, *nurbp;
	struct list_head *head, *tmp;
	struct urb_priv *purbp;
	struct uhci_td *pltd;
	unsigned int toggle;
	unsigned long flags;

	urbp = urb->hcpriv;

	spin_lock_irqsave(&uhci->frame_list_lock, flags);

	if (list_empty(&urbp->queue_list))
		goto out;

	nurbp = list_entry(urbp->queue_list.next, struct urb_priv, queue_list);

	/* Fix up the toggle for the next URB's */
	if (!urbp->queued)
		/* We set the toggle when we unlink */
		toggle = usb_gettoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe));
	else {
		/* If we're in the middle of the queue, grab the toggle */
		/*  from the TD previous to us */
		purbp = list_entry(urbp->queue_list.prev, struct urb_priv,
				queue_list);

		pltd = list_entry(purbp->td_list.prev, struct uhci_td, list);

		toggle = uhci_toggle(pltd->info) ^ 1;
	}

	head = &urbp->queue_list;
	tmp = head->next;
	while (head != tmp) {
		struct urb_priv *turbp;

		turbp = list_entry(tmp, struct urb_priv, queue_list);

		tmp = tmp->next;

		if (!turbp->queued)
			break;

		toggle = uhci_fixup_toggle(turbp->urb, toggle);
	}

	usb_settoggle(urb->dev, usb_pipeendpoint(urb->pipe),
		usb_pipeout(urb->pipe), toggle);

	if (!urbp->queued) {
		nurbp->queued = 0;

		_uhci_insert_qh(uhci, uhci->skel_bulk_qh, nurbp->urb);
	} else {
		/* We're somewhere in the middle (or end). A bit trickier */
		/*  than the head scenario */
		purbp = list_entry(urbp->queue_list.prev, struct urb_priv,
				queue_list);

		pltd = list_entry(purbp->td_list.prev, struct uhci_td, list);
		if (nurbp->queued)
			pltd->link = nurbp->qh->dma_handle | UHCI_PTR_QH;
		else
			/* The next URB happens to be the beginning, so */
			/*  we're the last, end the chain */
			pltd->link = UHCI_PTR_TERM;
	}

	list_del_init(&urbp->queue_list);

out:
	spin_unlock_irqrestore(&uhci->frame_list_lock, flags);
}

static struct urb_priv *uhci_alloc_urb_priv(struct uhci *uhci, struct urb *urb)
{
	struct urb_priv *urbp;

	urbp = kmem_cache_alloc(uhci_up_cachep, SLAB_ATOMIC);
	if (!urbp) {
		err("uhci_alloc_urb_priv: couldn't allocate memory for urb_priv\n");
		return NULL;
	}

	memset((void *)urbp, 0, sizeof(*urbp));

	urbp->inserttime = jiffies;
	urbp->fsbrtime = jiffies;
	urbp->urb = urb;
	urbp->dev = urb->dev;
	
	INIT_LIST_HEAD(&urbp->td_list);
	INIT_LIST_HEAD(&urbp->queue_list);
	INIT_LIST_HEAD(&urbp->complete_list);

	urb->hcpriv = urbp;

	if (urb->dev != uhci->rh.dev) {
		if (urb->transfer_buffer_length) {
			urbp->transfer_buffer_dma_handle = pci_map_single(uhci->dev,
				urb->transfer_buffer, urb->transfer_buffer_length,
				usb_pipein(urb->pipe) ? PCI_DMA_FROMDEVICE :
				PCI_DMA_TODEVICE);
			if (!urbp->transfer_buffer_dma_handle)
				return NULL;
		}

		if (usb_pipetype(urb->pipe) == PIPE_CONTROL && urb->setup_packet) {
			urbp->setup_packet_dma_handle = pci_map_single(uhci->dev,
				urb->setup_packet, sizeof(struct usb_ctrlrequest),
				PCI_DMA_TODEVICE);
			if (!urbp->setup_packet_dma_handle)
				return NULL;
		}
	}

	return urbp;
}

/*
 * MUST be called with urb->lock acquired
 */
static void uhci_add_td_to_urb(struct urb *urb, struct uhci_td *td)
{
	struct urb_priv *urbp = (struct urb_priv *)urb->hcpriv;

	td->urb = urb;

	list_add_tail(&td->list, &urbp->td_list);
}

/*
 * MUST be called with urb->lock acquired
 */
static void uhci_remove_td_from_urb(struct uhci_td *td)
{
	if (list_empty(&td->list))
		return;

	list_del_init(&td->list);

	td->urb = NULL;
}

/*
 * MUST be called with urb->lock acquired
 */
static void uhci_destroy_urb_priv(struct urb *urb)
{
	struct list_head *head, *tmp;
	struct urb_priv *urbp;
	struct uhci *uhci;

	urbp = (struct urb_priv *)urb->hcpriv;
	if (!urbp)
		return;

	if (!urbp->dev || !urbp->dev->bus || !urbp->dev->bus->hcpriv) {
		warn("uhci_destroy_urb_priv: urb %p belongs to disconnected device or bus?", urb);
		return;
	}

	if (!list_empty(&urb->urb_list))
		warn("uhci_destroy_urb_priv: urb %p still on uhci->urb_list or uhci->remove_list", urb);

	if (!list_empty(&urbp->complete_list))
		warn("uhci_destroy_urb_priv: urb %p still on uhci->complete_list", urb);

	uhci = urbp->dev->bus->hcpriv;

	head = &urbp->td_list;
	tmp = head->next;
	while (tmp != head) {
		struct uhci_td *td = list_entry(tmp, struct uhci_td, list);

		tmp = tmp->next;

		uhci_remove_td_from_urb(td);
		uhci_remove_td(uhci, td);
		uhci_free_td(uhci, td);
	}

	if (urbp->setup_packet_dma_handle) {
		pci_unmap_single(uhci->dev, urbp->setup_packet_dma_handle,
			sizeof(struct usb_ctrlrequest), PCI_DMA_TODEVICE);
		urbp->setup_packet_dma_handle = 0;
	}

	if (urbp->transfer_buffer_dma_handle) {
		pci_unmap_single(uhci->dev, urbp->transfer_buffer_dma_handle,
			urb->transfer_buffer_length, usb_pipein(urb->pipe) ?
			PCI_DMA_FROMDEVICE : PCI_DMA_TODEVICE);
		urbp->transfer_buffer_dma_handle = 0;
	}

	urb->hcpriv = NULL;
	kmem_cache_free(uhci_up_cachep, urbp);
}

static void uhci_inc_fsbr(struct uhci *uhci, struct urb *urb)
{
	unsigned long flags;
	struct urb_priv *urbp = (struct urb_priv *)urb->hcpriv;

	spin_lock_irqsave(&uhci->frame_list_lock, flags);

	if ((!(urb->transfer_flags & USB_NO_FSBR)) && !urbp->fsbr) {
		urbp->fsbr = 1;
		if (!uhci->fsbr++ && !uhci->fsbrtimeout)
			uhci->skel_term_qh->link = uhci->skel_hs_control_qh->dma_handle | UHCI_PTR_QH;
	}

	spin_unlock_irqrestore(&uhci->frame_list_lock, flags);
}

static void uhci_dec_fsbr(struct uhci *uhci, struct urb *urb)
{
	unsigned long flags;
	struct urb_priv *urbp = (struct urb_priv *)urb->hcpriv;

	spin_lock_irqsave(&uhci->frame_list_lock, flags);

	if ((!(urb->transfer_flags & USB_NO_FSBR)) && urbp->fsbr) {
		urbp->fsbr = 0;
		if (!--uhci->fsbr)
			uhci->fsbrtimeout = jiffies + FSBR_DELAY;
	}

	spin_unlock_irqrestore(&uhci->frame_list_lock, flags);
}

/*
 * Map status to standard result codes
 *
 * <status> is (td->status & 0xFE0000) [a.k.a. uhci_status_bits(td->status)]
 * <dir_out> is True for output TDs and False for input TDs.
 */
static int uhci_map_status(int status, int dir_out)
{
	if (!status)
		return 0;
	if (status & TD_CTRL_BITSTUFF)			/* Bitstuff error */
		return -EPROTO;
	if (status & TD_CTRL_CRCTIMEO) {		/* CRC/Timeout */
		if (dir_out)
			return -ETIMEDOUT;
		else
			return -EILSEQ;
	}
	if (status & TD_CTRL_NAK)			/* NAK */
		return -ETIMEDOUT;
	if (status & TD_CTRL_BABBLE)			/* Babble */
		return -EOVERFLOW;
	if (status & TD_CTRL_DBUFERR)			/* Buffer error */
		return -ENOSR;
	if (status & TD_CTRL_STALLED)			/* Stalled */
		return -EPIPE;
	if (status & TD_CTRL_ACTIVE)			/* Active */
		return 0;

	return -EINVAL;
}

/*
 * Control transfers
 */
static int uhci_submit_control(struct urb *urb)
{
	struct urb_priv *urbp = (struct urb_priv *)urb->hcpriv;
	struct uhci *uhci = (struct uhci *)urb->dev->bus->hcpriv;
	struct uhci_td *td;
	struct uhci_qh *qh;
	unsigned long destination, status;
	int maxsze = usb_maxpacket(urb->dev, urb->pipe, usb_pipeout(urb->pipe));
	int len = urb->transfer_buffer_length;
	dma_addr_t data = urbp->transfer_buffer_dma_handle;

	/* The "pipe" thing contains the destination in bits 8--18 */
	destination = (urb->pipe & PIPE_DEVEP_MASK) | USB_PID_SETUP;

	/* 3 errors */
	status = (urb->pipe & TD_CTRL_LS) | TD_CTRL_ACTIVE | (3 << 27);

	/*
	 * Build the TD for the control request
	 */
	td = uhci_alloc_td(uhci, urb->dev);
	if (!td)
		return -ENOMEM;

	uhci_add_td_to_urb(urb, td);
	uhci_fill_td(td, status, destination | (7 << 21),
		urbp->setup_packet_dma_handle);

	/*
	 * If direction is "send", change the frame from SETUP (0x2D)
	 * to OUT (0xE1). Else change it from SETUP to IN (0x69).
	 */
	destination ^= (USB_PID_SETUP ^ usb_packetid(urb->pipe));

	if (!(urb->transfer_flags & USB_DISABLE_SPD))
		status |= TD_CTRL_SPD;

	/*
	 * Build the DATA TD's
	 */
	while (len > 0) {
		int pktsze = len;

		if (pktsze > maxsze)
			pktsze = maxsze;

		td = uhci_alloc_td(uhci, urb->dev);
		if (!td)
			return -ENOMEM;

		/* Alternate Data0/1 (start with Data1) */
		destination ^= TD_TOKEN_TOGGLE;
	
		uhci_add_td_to_urb(urb, td);
		uhci_fill_td(td, status, destination | ((pktsze - 1) << 21),
			data);

		data += pktsze;
		len -= pktsze;
	}

	/*
	 * Build the final TD for control status 
	 */
	td = uhci_alloc_td(uhci, urb->dev);
	if (!td)
		return -ENOMEM;

	/*
	 * It's IN if the pipe is an output pipe or we're not expecting
	 * data back.
	 */
	destination &= ~TD_TOKEN_PID_MASK;
	if (usb_pipeout(urb->pipe) || !urb->transfer_buffer_length)
		destination |= USB_PID_IN;
	else
		destination |= USB_PID_OUT;

	destination |= TD_TOKEN_TOGGLE;		/* End in Data1 */

	status &= ~TD_CTRL_SPD;

	uhci_add_td_to_urb(urb, td);
	uhci_fill_td(td, status | TD_CTRL_IOC,
		destination | (UHCI_NULL_DATA_SIZE << 21), 0);

	qh = uhci_alloc_qh(uhci, urb->dev);
	if (!qh)
		return -ENOMEM;

	urbp->qh = qh;
	qh->urbp = urbp;

	/* Low speed or small transfers gets a different queue and treatment */
	if (urb->pipe & TD_CTRL_LS) {
		uhci_insert_tds_in_qh(qh, urb, 0);
		uhci_insert_qh(uhci, uhci->skel_ls_control_qh, urb);
	} else {
		uhci_insert_tds_in_qh(qh, urb, 1);
		uhci_insert_qh(uhci, uhci->skel_hs_control_qh, urb);
		uhci_inc_fsbr(uhci, urb);
	}

	return -EINPROGRESS;
}

static int usb_control_retrigger_status(struct urb *urb);

static int uhci_result_control(struct urb *urb)
{
	struct list_head *tmp, *head;
	struct urb_priv *urbp = urb->hcpriv;
	struct uhci_td *td;
	unsigned int status;
	int ret = 0;

	if (list_empty(&urbp->td_list))
		return -EINVAL;

	head = &urbp->td_list;

	if (urbp->short_control_packet) {
		tmp = head->prev;
		goto status_phase;
	}

	tmp = head->next;
	td = list_entry(tmp, struct uhci_td, list);

	/* The first TD is the SETUP phase, check the status, but skip */
	/*  the count */
	status = uhci_status_bits(td->status);
	if (status & TD_CTRL_ACTIVE)
		return -EINPROGRESS;

	if (status)
		goto td_error;

	urb->actual_length = 0;

	/* The rest of the TD's (but the last) are data */
	tmp = tmp->next;
	while (tmp != head && tmp->next != head) {
		td = list_entry(tmp, struct uhci_td, list);

		tmp = tmp->next;

		status = uhci_status_bits(td->status);
		if (status & TD_CTRL_ACTIVE)
			return -EINPROGRESS;

		urb->actual_length += uhci_actual_length(td->status);

		if (status)
			goto td_error;

		/* Check to see if we received a short packet */
		if (uhci_actual_length(td->status) < uhci_expected_length(td->info)) {
			if (urb->transfer_flags & USB_DISABLE_SPD) {
				ret = -EREMOTEIO;
				goto err;
			}

			if (uhci_packetid(td->info) == USB_PID_IN)
				return usb_control_retrigger_status(urb);
			else
				return 0;
		}
	}

status_phase:
	td = list_entry(tmp, struct uhci_td, list);

	/* Control status phase */
	status = uhci_status_bits(td->status);

#ifdef I_HAVE_BUGGY_APC_BACKUPS
	/* APC BackUPS Pro kludge */
	/* It tries to send all of the descriptor instead of the amount */
	/*  we requested */
	if (td->status & TD_CTRL_IOC &&	/* IOC is masked out by uhci_status_bits */
	    status & TD_CTRL_ACTIVE &&
	    status & TD_CTRL_NAK)
		return 0;
#endif

	if (status & TD_CTRL_ACTIVE)
		return -EINPROGRESS;

	if (status)
		goto td_error;

	return 0;

td_error:
	ret = uhci_map_status(status, uhci_packetout(td->info));
	if (ret == -EPIPE)
		/* endpoint has stalled - mark it halted */
		usb_endpoint_halt(urb->dev, uhci_endpoint(td->info),
	    			uhci_packetout(td->info));

err:
	if ((debug == 1 && ret != -EPIPE) || debug > 1) {
		/* Some debugging code */
		dbg("uhci_result_control() failed with status %x", status);

		if (errbuf) {
			/* Print the chain for debugging purposes */
			uhci_show_qh(urbp->qh, errbuf, ERRBUF_LEN, 0);

			lprintk(errbuf);
		}
	}

	return ret;
}

static int usb_control_retrigger_status(struct urb *urb)
{
	struct list_head *tmp, *head;
	struct urb_priv *urbp = (struct urb_priv *)urb->hcpriv;
	struct uhci *uhci = urb->dev->bus->hcpriv;

	urbp->short_control_packet = 1;

	/* Create a new QH to avoid pointer overwriting problems */
	uhci_remove_qh(uhci, urbp->qh);

	/* Delete all of the TD's except for the status TD at the end */
	head = &urbp->td_list;
	tmp = head->next;
	while (tmp != head && tmp->next != head) {
		struct uhci_td *td = list_entry(tmp, struct uhci_td, list);

		tmp = tmp->next;

		uhci_remove_td_from_urb(td);
		uhci_remove_td(uhci, td);
		uhci_free_td(uhci, td);
	}

	urbp->qh = uhci_alloc_qh(uhci, urb->dev);
	if (!urbp->qh) {
		err("unable to allocate new QH for control retrigger");
		return -ENOMEM;
	}

	urbp->qh->urbp = urbp;

	/* One TD, who cares about Breadth first? */
	uhci_insert_tds_in_qh(urbp->qh, urb, 0);

	/* Low speed or small transfers gets a different queue and treatment */
	if (urb->pipe & TD_CTRL_LS)
		uhci_insert_qh(uhci, uhci->skel_ls_control_qh, urb);
	else
		uhci_insert_qh(uhci, uhci->skel_hs_control_qh, urb);

	return -EINPROGRESS;
}

/*
 * Interrupt transfers
 */
static int uhci_submit_interrupt(struct urb *urb)
{
	struct uhci_td *td;
	unsigned long destination, status;
	struct uhci *uhci = (struct uhci *)urb->dev->bus->hcpriv;
	struct urb_priv *urbp = (struct urb_priv *)urb->hcpriv;

	if (urb->transfer_buffer_length > usb_maxpacket(urb->dev, urb->pipe, usb_pipeout(urb->pipe)))
		return -EINVAL;

	/* The "pipe" thing contains the destination in bits 8--18 */
	destination = (urb->pipe & PIPE_DEVEP_MASK) | usb_packetid(urb->pipe);

	status = (urb->pipe & TD_CTRL_LS) | TD_CTRL_ACTIVE | TD_CTRL_IOC;

	td = uhci_alloc_td(uhci, urb->dev);
	if (!td)
		return -ENOMEM;

	destination |= (usb_gettoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe)) << TD_TOKEN_TOGGLE_SHIFT);
	destination |= ((urb->transfer_buffer_length - 1) << 21);

	usb_dotoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe));

	uhci_add_td_to_urb(urb, td);
	uhci_fill_td(td, status, destination, urbp->transfer_buffer_dma_handle);

	uhci_insert_td(uhci, uhci->skeltd[__interval_to_skel(urb->interval)], td);

	return -EINPROGRESS;
}

static int uhci_result_interrupt(struct urb *urb)
{
	struct list_head *tmp, *head;
	struct urb_priv *urbp = urb->hcpriv;
	struct uhci_td *td;
	unsigned int status;
	int ret = 0;

	urb->actual_length = 0;

	head = &urbp->td_list;
	tmp = head->next;
	while (tmp != head) {
		td = list_entry(tmp, struct uhci_td, list);

		tmp = tmp->next;

		status = uhci_status_bits(td->status);
		if (status & TD_CTRL_ACTIVE)
			return -EINPROGRESS;

		urb->actual_length += uhci_actual_length(td->status);

		if (status)
			goto td_error;

		if (uhci_actual_length(td->status) < uhci_expected_length(td->info)) {
			if (urb->transfer_flags & USB_DISABLE_SPD) {
				ret = -EREMOTEIO;
				goto err;
			} else
				return 0;
		}
	}

	return 0;

td_error:
	ret = uhci_map_status(status, uhci_packetout(td->info));
	if (ret == -EPIPE)
		/* endpoint has stalled - mark it halted */
		usb_endpoint_halt(urb->dev, uhci_endpoint(td->info),
	    			uhci_packetout(td->info));

err:
	if ((debug == 1 && ret != -EPIPE) || debug > 1) {
		/* Some debugging code */
		dbg("uhci_result_interrupt/bulk() failed with status %x",
			status);

		if (errbuf) {
			/* Print the chain for debugging purposes */
			if (urbp->qh)
				uhci_show_qh(urbp->qh, errbuf, ERRBUF_LEN, 0);
			else
				uhci_show_td(td, errbuf, ERRBUF_LEN, 0);

			lprintk(errbuf);
		}
	}

	return ret;
}

static void uhci_reset_interrupt(struct urb *urb)
{
	struct uhci *uhci = (struct uhci *)urb->dev->bus->hcpriv;
	struct urb_priv *urbp = (struct urb_priv *)urb->hcpriv;
	struct uhci_td *td;
	unsigned long flags;

	spin_lock_irqsave(&urb->lock, flags);

	/* Root hub is special */
	if (urb->dev == uhci->rh.dev)
		goto out;

	td = list_entry(urbp->td_list.next, struct uhci_td, list);

	td->status = (td->status & 0x2F000000) | TD_CTRL_ACTIVE | TD_CTRL_IOC;
	td->info &= ~TD_TOKEN_TOGGLE;
	td->info |= (usb_gettoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe)) << TD_TOKEN_TOGGLE_SHIFT);
	usb_dotoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe));

out:
	urb->status = -EINPROGRESS;

	spin_unlock_irqrestore(&urb->lock, flags);
}

/*
 * Bulk transfers
 */
static int uhci_submit_bulk(struct urb *urb, struct urb *eurb)
{
	struct uhci_td *td;
	struct uhci_qh *qh;
	unsigned long destination, status;
	struct uhci *uhci = (struct uhci *)urb->dev->bus->hcpriv;
	int maxsze = usb_maxpacket(urb->dev, urb->pipe, usb_pipeout(urb->pipe));
	int len = urb->transfer_buffer_length;
	struct urb_priv *urbp = (struct urb_priv *)urb->hcpriv;
	dma_addr_t data = urbp->transfer_buffer_dma_handle;

	if (len < 0 || maxsze <= 0)
		return -EINVAL;

	/* Can't have low speed bulk transfers */
	if (urb->pipe & TD_CTRL_LS)
		return -EINVAL;

	/* The "pipe" thing contains the destination in bits 8--18 */
	destination = (urb->pipe & PIPE_DEVEP_MASK) | usb_packetid(urb->pipe);

	/* 3 errors */
	status = TD_CTRL_ACTIVE | (3 << TD_CTRL_C_ERR_SHIFT);

	if (!(urb->transfer_flags & USB_DISABLE_SPD))
		status |= TD_CTRL_SPD;

	/*
	 * Build the DATA TD's
	 */
	do {	/* Allow zero length packets */
		int pktsze = len;

		if (pktsze > maxsze)
			pktsze = maxsze;

		td = uhci_alloc_td(uhci, urb->dev);
		if (!td)
			return -ENOMEM;

		uhci_add_td_to_urb(urb, td);
		uhci_fill_td(td, status, destination |
			(((pktsze - 1) & UHCI_NULL_DATA_SIZE) << 21) |
			(usb_gettoggle(urb->dev, usb_pipeendpoint(urb->pipe),
			 usb_pipeout(urb->pipe)) << TD_TOKEN_TOGGLE_SHIFT),
			data);

		data += pktsze;
		len -= maxsze;

		usb_dotoggle(urb->dev, usb_pipeendpoint(urb->pipe),
			usb_pipeout(urb->pipe));
	} while (len > 0);

	/*
	 * USB_ZERO_PACKET means adding a 0-length packet, if
	 * direction is OUT and the transfer_length was an
	 * exact multiple of maxsze, hence
	 * (len = transfer_length - N * maxsze) == 0
	 * however, if transfer_length == 0, the zero packet
	 * was already prepared above.
	 */
	if (usb_pipeout(urb->pipe) && (urb->transfer_flags & USB_ZERO_PACKET) &&
	   !len && urb->transfer_buffer_length) {
		td = uhci_alloc_td(uhci, urb->dev);
		if (!td)
			return -ENOMEM;

		uhci_add_td_to_urb(urb, td);
		uhci_fill_td(td, status, destination |
			(UHCI_NULL_DATA_SIZE << 21) |
			(usb_gettoggle(urb->dev, usb_pipeendpoint(urb->pipe),
			 usb_pipeout(urb->pipe)) << TD_TOKEN_TOGGLE_SHIFT),
			data);

		usb_dotoggle(urb->dev, usb_pipeendpoint(urb->pipe),
			usb_pipeout(urb->pipe));
	}

	/* Set the flag on the last packet */
	td->status |= TD_CTRL_IOC;

	qh = uhci_alloc_qh(uhci, urb->dev);
	if (!qh)
		return -ENOMEM;

	urbp->qh = qh;
	qh->urbp = urbp;

	/* Always assume breadth first */
	uhci_insert_tds_in_qh(qh, urb, 1);

	if (urb->transfer_flags & USB_QUEUE_BULK && eurb)
		uhci_append_queued_urb(uhci, eurb, urb);
	else
		uhci_insert_qh(uhci, uhci->skel_bulk_qh, urb);

	uhci_inc_fsbr(uhci, urb);

	return -EINPROGRESS;
}

/* We can use the result interrupt since they're identical */
#define uhci_result_bulk uhci_result_interrupt

/*
 * Isochronous transfers
 */
static int isochronous_find_limits(struct urb *urb, unsigned int *start, unsigned int *end)
{
	struct urb *last_urb = NULL;
	struct uhci *uhci = (struct uhci *)urb->dev->bus->hcpriv;
	struct list_head *tmp, *head;
	int ret = 0;

	head = &uhci->urb_list;
	tmp = head->next;
	while (tmp != head) {
		struct urb *u = list_entry(tmp, struct urb, urb_list);

		tmp = tmp->next;

		/* look for pending URB's with identical pipe handle */
		if ((urb->pipe == u->pipe) && (urb->dev == u->dev) &&
		    (u->status == -EINPROGRESS) && (u != urb)) {
			if (!last_urb)
				*start = u->start_frame;
			last_urb = u;
		}
	}

	if (last_urb) {
		*end = (last_urb->start_frame + last_urb->number_of_packets) & 1023;
		ret = 0;
	} else
		ret = -1;	/* no previous urb found */

	return ret;
}

static int isochronous_find_start(struct urb *urb)
{
	int limits;
	unsigned int start = 0, end = 0;

	if (urb->number_of_packets > 900)	/* 900? Why? */
		return -EFBIG;

	limits = isochronous_find_limits(urb, &start, &end);

	if (urb->transfer_flags & USB_ISO_ASAP) {
		if (limits) {
			int curframe;

			curframe = uhci_get_current_frame_number(urb->dev) % UHCI_NUMFRAMES;
			urb->start_frame = (curframe + 10) % UHCI_NUMFRAMES;
		} else
			urb->start_frame = end;
	} else {
		urb->start_frame %= UHCI_NUMFRAMES;
		/* FIXME: Sanity check */
	}

	return 0;
}

/*
 * Isochronous transfers
 */
static int uhci_submit_isochronous(struct urb *urb)
{
	struct uhci_td *td;
	struct uhci *uhci = (struct uhci *)urb->dev->bus->hcpriv;
	int i, ret, framenum;
	int status, destination;
	struct urb_priv *urbp = (struct urb_priv *)urb->hcpriv;

	status = TD_CTRL_ACTIVE | TD_CTRL_IOS;
	destination = (urb->pipe & PIPE_DEVEP_MASK) | usb_packetid(urb->pipe);

	ret = isochronous_find_start(urb);
	if (ret)
		return ret;

	framenum = urb->start_frame;
	for (i = 0; i < urb->number_of_packets; i++, framenum++) {
		if (!urb->iso_frame_desc[i].length)
			continue;

		td = uhci_alloc_td(uhci, urb->dev);
		if (!td)
			return -ENOMEM;

		uhci_add_td_to_urb(urb, td);
		uhci_fill_td(td, status, destination | ((urb->iso_frame_desc[i].length - 1) << 21),
			urbp->transfer_buffer_dma_handle + urb->iso_frame_desc[i].offset);

		if (i + 1 >= urb->number_of_packets)
			td->status |= TD_CTRL_IOC;

		uhci_insert_td_frame_list(uhci, td, framenum);
	}

	return -EINPROGRESS;
}

static int uhci_result_isochronous(struct urb *urb)
{
	struct list_head *tmp, *head;
	struct urb_priv *urbp = (struct urb_priv *)urb->hcpriv;
	int status;
	int i, ret = 0;

	urb->actual_length = 0;

	i = 0;
	head = &urbp->td_list;
	tmp = head->next;
	while (tmp != head) {
		struct uhci_td *td = list_entry(tmp, struct uhci_td, list);
		int actlength;

		tmp = tmp->next;

		if (td->status & TD_CTRL_ACTIVE)
			return -EINPROGRESS;

		actlength = uhci_actual_length(td->status);
		urb->iso_frame_desc[i].actual_length = actlength;
		urb->actual_length += actlength;

		status = uhci_map_status(uhci_status_bits(td->status), usb_pipeout(urb->pipe));
		urb->iso_frame_desc[i].status = status;
		if (status) {
			urb->error_count++;
			ret = status;
		}

		i++;
	}

	return ret;
}

/*
 * MUST be called with uhci->urb_list_lock acquired
 */
static struct urb *uhci_find_urb_ep(struct uhci *uhci, struct urb *urb)
{
	struct list_head *tmp, *head;

	/* We don't match Isoc transfers since they are special */
	if (usb_pipeisoc(urb->pipe))
		return NULL;

	head = &uhci->urb_list;
	tmp = head->next;
	while (tmp != head) {
		struct urb *u = list_entry(tmp, struct urb, urb_list);

		tmp = tmp->next;

		if (u->dev == urb->dev && u->pipe == urb->pipe &&
		    u->status == -EINPROGRESS)
			return u;
	}

	return NULL;
}

static int uhci_submit_urb(struct urb *urb)
{
	int ret = -EINVAL;
	struct uhci *uhci;
	unsigned long flags;
	struct urb *eurb;
	int bustime;

	if (!urb)
		return -EINVAL;

	if (!urb->dev || !urb->dev->bus || !urb->dev->bus->hcpriv) {
		warn("uhci_submit_urb: urb %p belongs to disconnected device or bus?", urb);
		return -ENODEV;
	}

	uhci = (struct uhci *)urb->dev->bus->hcpriv;

	usb_inc_dev_use(urb->dev);

	spin_lock_irqsave(&uhci->urb_list_lock, flags);
	spin_lock(&urb->lock);

	if (urb->status == -EINPROGRESS || urb->status == -ECONNRESET ||
	    urb->status == -ECONNABORTED) {
		dbg("uhci_submit_urb: urb not available to submit (status = %d)", urb->status);
		/* Since we can have problems on the out path */
		spin_unlock(&urb->lock);
		spin_unlock_irqrestore(&uhci->urb_list_lock, flags);
		usb_dec_dev_use(urb->dev);

		return ret;
	}

	INIT_LIST_HEAD(&urb->urb_list);
	if (!uhci_alloc_urb_priv(uhci, urb)) {
		ret = -ENOMEM;

		goto out;
	}

	eurb = uhci_find_urb_ep(uhci, urb);
	if (eurb && !(urb->transfer_flags & USB_QUEUE_BULK)) {
		ret = -ENXIO;

		goto out;
	}

	/* Short circuit the virtual root hub */
	if (urb->dev == uhci->rh.dev) {
		ret = rh_submit_urb(urb);

		goto out;
	}

	switch (usb_pipetype(urb->pipe)) {
	case PIPE_CONTROL:
		ret = uhci_submit_control(urb);
		break;
	case PIPE_INTERRUPT:
		if (urb->bandwidth == 0) {	/* not yet checked/allocated */
			bustime = usb_check_bandwidth(urb->dev, urb);
			if (bustime < 0)
				ret = bustime;
			else {
				ret = uhci_submit_interrupt(urb);
				if (ret == -EINPROGRESS)
					usb_claim_bandwidth(urb->dev, urb, bustime, 0);
			}
		} else		/* bandwidth is already set */
			ret = uhci_submit_interrupt(urb);
		break;
	case PIPE_BULK:
		ret = uhci_submit_bulk(urb, eurb);
		break;
	case PIPE_ISOCHRONOUS:
		if (urb->bandwidth == 0) {	/* not yet checked/allocated */
			if (urb->number_of_packets <= 0) {
				ret = -EINVAL;
				break;
			}
			bustime = usb_check_bandwidth(urb->dev, urb);
			if (bustime < 0) {
				ret = bustime;
				break;
			}

			ret = uhci_submit_isochronous(urb);
			if (ret == -EINPROGRESS)
				usb_claim_bandwidth(urb->dev, urb, bustime, 1);
		} else		/* bandwidth is already set */
			ret = uhci_submit_isochronous(urb);
		break;
	}

out:
	urb->status = ret;

	if (ret == -EINPROGRESS) {
		/* We use _tail to make find_urb_ep more efficient */
		list_add_tail(&urb->urb_list, &uhci->urb_list);

		spin_unlock(&urb->lock);
		spin_unlock_irqrestore(&uhci->urb_list_lock, flags);

		return 0;
	}

	uhci_unlink_generic(uhci, urb);

	spin_unlock(&urb->lock);
	spin_unlock_irqrestore(&uhci->urb_list_lock, flags);

	/* Only call completion if it was successful */
	if (!ret)
		uhci_call_completion(urb);

	return ret;
}

/*
 * Return the result of a transfer
 *
 * MUST be called with urb_list_lock acquired
 */
static void uhci_transfer_result(struct uhci *uhci, struct urb *urb)
{
	int ret = -EINVAL;
	unsigned long flags;
	struct urb_priv *urbp;

	/* The root hub is special */
	if (urb->dev == uhci->rh.dev)
		return;

	spin_lock_irqsave(&urb->lock, flags);

	urbp = (struct urb_priv *)urb->hcpriv;

	if (urb->status != -EINPROGRESS) {
		info("uhci_transfer_result: called for URB %p not in flight?", urb);
		goto out;
	}

	switch (usb_pipetype(urb->pipe)) {
	case PIPE_CONTROL:
		ret = uhci_result_control(urb);
		break;
	case PIPE_INTERRUPT:
		ret = uhci_result_interrupt(urb);
		break;
	case PIPE_BULK:
		ret = uhci_result_bulk(urb);
		break;
	case PIPE_ISOCHRONOUS:
		ret = uhci_result_isochronous(urb);
		break;
	}

	urbp->status = ret;

	if (ret == -EINPROGRESS)
		goto out;

	switch (usb_pipetype(urb->pipe)) {
	case PIPE_CONTROL:
	case PIPE_BULK:
	case PIPE_ISOCHRONOUS:
		/* Release bandwidth for Interrupt or Isoc. transfers */
		/* Spinlock needed ? */
		if (urb->bandwidth)
			usb_release_bandwidth(urb->dev, urb, 1);
		uhci_unlink_generic(uhci, urb);
		break;
	case PIPE_INTERRUPT:
		/* Interrupts are an exception */
		if (urb->interval)
			goto out_complete;

		/* Release bandwidth for Interrupt or Isoc. transfers */
		/* Spinlock needed ? */
		if (urb->bandwidth)
			usb_release_bandwidth(urb->dev, urb, 0);
		uhci_unlink_generic(uhci, urb);
		break;
	default:
		info("uhci_transfer_result: unknown pipe type %d for urb %p\n",
			usb_pipetype(urb->pipe), urb);
	}

	/* Remove it from uhci->urb_list */
	list_del_init(&urb->urb_list);

out_complete:
	uhci_add_complete(urb);

out:
	spin_unlock_irqrestore(&urb->lock, flags);
}

/*
 * MUST be called with urb->lock acquired
 */
static void uhci_unlink_generic(struct uhci *uhci, struct urb *urb)
{
	struct list_head *head, *tmp;
	struct urb_priv *urbp = urb->hcpriv;
	int prevactive = 1;

	/* We can get called when urbp allocation fails, so check */
	if (!urbp)
		return;

	uhci_dec_fsbr(uhci, urb);	/* Safe since it checks */

	/*
	 * Now we need to find out what the last successful toggle was
	 * so we can update the local data toggle for the next transfer
	 *
	 * There's 3 way's the last successful completed TD is found:
	 *
	 * 1) The TD is NOT active and the actual length < expected length
	 * 2) The TD is NOT active and it's the last TD in the chain
	 * 3) The TD is active and the previous TD is NOT active
	 *
	 * Control and Isochronous ignore the toggle, so this is safe
	 * for all types
	 */
	head = &urbp->td_list;
	tmp = head->next;
	while (tmp != head) {
		struct uhci_td *td = list_entry(tmp, struct uhci_td, list);

		tmp = tmp->next;

		if (!(td->status & TD_CTRL_ACTIVE) &&
		    (uhci_actual_length(td->status) < uhci_expected_length(td->info) ||
		    tmp == head))
			usb_settoggle(urb->dev, uhci_endpoint(td->info),
				uhci_packetout(td->info),
				uhci_toggle(td->info) ^ 1);
		else if ((td->status & TD_CTRL_ACTIVE) && !prevactive)
			usb_settoggle(urb->dev, uhci_endpoint(td->info),
				uhci_packetout(td->info),
				uhci_toggle(td->info));

		prevactive = td->status & TD_CTRL_ACTIVE;
	}

	uhci_delete_queued_urb(uhci, urb);

	/* The interrupt loop will reclaim the QH's */
	uhci_remove_qh(uhci, urbp->qh);
	urbp->qh = NULL;
}

static int uhci_unlink_urb(struct urb *urb)
{
	struct uhci *uhci;
	unsigned long flags;
	struct urb_priv *urbp = urb->hcpriv;

	if (!urb)
		return -EINVAL;

	if (!urb->dev || !urb->dev->bus || !urb->dev->bus->hcpriv)
		return -ENODEV;

	uhci = (struct uhci *)urb->dev->bus->hcpriv;

	spin_lock_irqsave(&uhci->urb_list_lock, flags);
	spin_lock(&urb->lock);

	/* Release bandwidth for Interrupt or Isoc. transfers */
	/* Spinlock needed ? */
	if (urb->bandwidth) {
		switch (usb_pipetype(urb->pipe)) {
		case PIPE_INTERRUPT:
			usb_release_bandwidth(urb->dev, urb, 0);
			break;
		case PIPE_ISOCHRONOUS:
			usb_release_bandwidth(urb->dev, urb, 1);
			break;
		default:
			break;
		}
	}

	if (urb->status != -EINPROGRESS) {
		spin_unlock(&urb->lock);
		spin_unlock_irqrestore(&uhci->urb_list_lock, flags);
		return 0;
	}

	list_del_init(&urb->urb_list);

	uhci_unlink_generic(uhci, urb);

	/* Short circuit the virtual root hub */
	if (urb->dev == uhci->rh.dev) {
		rh_unlink_urb(urb);

		spin_unlock(&urb->lock);
		spin_unlock_irqrestore(&uhci->urb_list_lock, flags);

		uhci_call_completion(urb);
	} else {
		if (urb->transfer_flags & USB_ASYNC_UNLINK) {
			urbp->status = urb->status = -ECONNABORTED;

			spin_lock(&uhci->urb_remove_list_lock);

			/* If we're the first, set the next interrupt bit */
			if (list_empty(&uhci->urb_remove_list))
				uhci_set_next_interrupt(uhci);
			
			list_add(&urb->urb_list, &uhci->urb_remove_list);

			spin_unlock(&uhci->urb_remove_list_lock);

			spin_unlock(&urb->lock);
			spin_unlock_irqrestore(&uhci->urb_list_lock, flags);

		} else {
			urb->status = -ENOENT;

			spin_unlock(&urb->lock);
			spin_unlock_irqrestore(&uhci->urb_list_lock, flags);

			if (in_interrupt()) {	/* wait at least 1 frame */
				static int errorcount = 10;

				if (errorcount--)
					dbg("uhci_unlink_urb called from interrupt for urb %p", urb);
				udelay(1000);
			} else
				schedule_timeout(1+1*HZ/1000); 

			uhci_call_completion(urb);
		}
	}

	return 0;
}

static int uhci_fsbr_timeout(struct uhci *uhci, struct urb *urb)
{
	struct urb_priv *urbp = (struct urb_priv *)urb->hcpriv;
	struct list_head *head, *tmp;
	int count = 0;

	uhci_dec_fsbr(uhci, urb);

	urbp->fsbr_timeout = 1;

	/*
	 * Ideally we would want to fix qh->element as well, but it's
	 * read/write by the HC, so that can introduce a race. It's not
	 * really worth the hassle
	 */

	head = &urbp->td_list;
	tmp = head->next;
	while (tmp != head) {
		struct uhci_td *td = list_entry(tmp, struct uhci_td, list);

		tmp = tmp->next;

		/*
		 * Make sure we don't do the last one (since it'll have the
		 * TERM bit set) as well as we skip every so many TD's to
		 * make sure it doesn't hog the bandwidth
		 */
		if (tmp != head && (count % DEPTH_INTERVAL) == (DEPTH_INTERVAL - 1))
			td->link |= UHCI_PTR_DEPTH;

		count++;
	}

	return 0;
}

/*
 * uhci_get_current_frame_number()
 *
 * returns the current frame number for a USB bus/controller.
 */
static int uhci_get_current_frame_number(struct usb_device *dev)
{
	struct uhci *uhci = (struct uhci *)dev->bus->hcpriv;

	return inw(uhci->io_addr + USBFRNUM);
}

struct usb_operations uhci_device_operations = {
	uhci_alloc_dev,
	uhci_free_dev,
	uhci_get_current_frame_number,
	uhci_submit_urb,
	uhci_unlink_urb
};

/* Virtual Root Hub */

static __u8 root_hub_dev_des[] =
{
 	0x12,			/*  __u8  bLength; */
	0x01,			/*  __u8  bDescriptorType; Device */
	0x00,			/*  __u16 bcdUSB; v1.0 */
	0x01,
	0x09,			/*  __u8  bDeviceClass; HUB_CLASSCODE */
	0x00,			/*  __u8  bDeviceSubClass; */
	0x00,			/*  __u8  bDeviceProtocol; */
	0x08,			/*  __u8  bMaxPacketSize0; 8 Bytes */
	0x00,			/*  __u16 idVendor; */
	0x00,
	0x00,			/*  __u16 idProduct; */
	0x00,
	0x00,			/*  __u16 bcdDevice; */
	0x00,
	0x00,			/*  __u8  iManufacturer; */
	0x02,			/*  __u8  iProduct; */
	0x01,			/*  __u8  iSerialNumber; */
	0x01			/*  __u8  bNumConfigurations; */
};


/* Configuration descriptor */
static __u8 root_hub_config_des[] =
{
	0x09,			/*  __u8  bLength; */
	0x02,			/*  __u8  bDescriptorType; Configuration */
	0x19,			/*  __u16 wTotalLength; */
	0x00,
	0x01,			/*  __u8  bNumInterfaces; */
	0x01,			/*  __u8  bConfigurationValue; */
	0x00,			/*  __u8  iConfiguration; */
	0x40,			/*  __u8  bmAttributes;
					Bit 7: Bus-powered, 6: Self-powered,
					Bit 5 Remote-wakeup, 4..0: resvd */
	0x00,			/*  __u8  MaxPower; */

	/* interface */
	0x09,			/*  __u8  if_bLength; */
	0x04,			/*  __u8  if_bDescriptorType; Interface */
	0x00,			/*  __u8  if_bInterfaceNumber; */
	0x00,			/*  __u8  if_bAlternateSetting; */
	0x01,			/*  __u8  if_bNumEndpoints; */
	0x09,			/*  __u8  if_bInterfaceClass; HUB_CLASSCODE */
	0x00,			/*  __u8  if_bInterfaceSubClass; */
	0x00,			/*  __u8  if_bInterfaceProtocol; */
	0x00,			/*  __u8  if_iInterface; */

	/* endpoint */
	0x07,			/*  __u8  ep_bLength; */
	0x05,			/*  __u8  ep_bDescriptorType; Endpoint */
	0x81,			/*  __u8  ep_bEndpointAddress; IN Endpoint 1 */
	0x03,			/*  __u8  ep_bmAttributes; Interrupt */
	0x08,			/*  __u16 ep_wMaxPacketSize; 8 Bytes */
	0x00,
	0xff			/*  __u8  ep_bInterval; 255 ms */
};

static __u8 root_hub_hub_des[] =
{
	0x09,			/*  __u8  bLength; */
	0x29,			/*  __u8  bDescriptorType; Hub-descriptor */
	0x02,			/*  __u8  bNbrPorts; */
	0x00,			/* __u16  wHubCharacteristics; */
	0x00,
	0x01,			/*  __u8  bPwrOn2pwrGood; 2ms */
	0x00,			/*  __u8  bHubContrCurrent; 0 mA */
	0x00,			/*  __u8  DeviceRemovable; *** 7 Ports max *** */
	0xff			/*  __u8  PortPwrCtrlMask; *** 7 ports max *** */
};

/* prepare Interrupt pipe transaction data; HUB INTERRUPT ENDPOINT */
static int rh_send_irq(struct urb *urb)
{
	struct uhci *uhci = (struct uhci *)urb->dev->bus->hcpriv;
	struct urb_priv *urbp = (struct urb_priv *)urb->hcpriv;
	unsigned int io_addr = uhci->io_addr;
	unsigned long flags;
	int i, len = 1;
	__u16 data = 0;

	spin_lock_irqsave(&urb->lock, flags);
	for (i = 0; i < uhci->rh.numports; i++) {
		data |= ((inw(io_addr + USBPORTSC1 + i * 2) & 0xa) > 0 ? (1 << (i + 1)) : 0);
		len = (i + 1) / 8 + 1;
	}

	*(__u16 *) urb->transfer_buffer = cpu_to_le16(data);
	urb->actual_length = len;
	urbp->status = 0;

	spin_unlock_irqrestore(&urb->lock, flags);

	if ((data > 0) && (uhci->rh.send != 0)) {
		dbg("root-hub INT complete: port1: %x port2: %x data: %x",
			inw(io_addr + USBPORTSC1), inw(io_addr + USBPORTSC2), data);
		uhci_call_completion(urb);
	}

	return 0;
}

/* Virtual Root Hub INTs are polled by this timer every "interval" ms */
static int rh_init_int_timer(struct urb *urb);

static void rh_int_timer_do(unsigned long ptr)
{
	struct urb *urb = (struct urb *)ptr;
	struct uhci *uhci = (struct uhci *)urb->dev->bus->hcpriv;
	struct list_head list, *tmp, *head;
	unsigned long flags;

	if (uhci->rh.send)
		rh_send_irq(urb);

	INIT_LIST_HEAD(&list);

	spin_lock_irqsave(&uhci->urb_list_lock, flags);
	head = &uhci->urb_list;
	tmp = head->next;
	while (tmp != head) {
		struct urb *u = list_entry(tmp, struct urb, urb_list);
		struct urb_priv *up = (struct urb_priv *)u->hcpriv;

		tmp = tmp->next;

		spin_lock(&u->lock);

		/* Check if the FSBR timed out */
		if (up->fsbr && !up->fsbr_timeout && time_after_eq(jiffies, up->fsbrtime + IDLE_TIMEOUT))
			uhci_fsbr_timeout(uhci, u);

		/* Check if the URB timed out */
		if (u->timeout && time_after_eq(jiffies, up->inserttime + u->timeout)) {
			list_del(&u->urb_list);
			list_add_tail(&u->urb_list, &list);
		}

		spin_unlock(&u->lock);
	}
	spin_unlock_irqrestore(&uhci->urb_list_lock, flags);

	head = &list;
	tmp = head->next;
	while (tmp != head) {
		struct urb *u = list_entry(tmp, struct urb, urb_list);

		tmp = tmp->next;

		u->transfer_flags |= USB_ASYNC_UNLINK | USB_TIMEOUT_KILLED;
		uhci_unlink_urb(u);
	}

	/* Really disable FSBR */
	if (!uhci->fsbr && uhci->fsbrtimeout && time_after_eq(jiffies, uhci->fsbrtimeout)) {
		uhci->fsbrtimeout = 0;
		uhci->skel_term_qh->link = UHCI_PTR_TERM;
	}

	/* enter global suspend if nothing connected */
	if (!uhci->is_suspended && !ports_active(uhci))
		suspend_hc(uhci);

	rh_init_int_timer(urb);
}

/* Root Hub INTs are polled by this timer */
static int rh_init_int_timer(struct urb *urb)
{
	struct uhci *uhci = (struct uhci *)urb->dev->bus->hcpriv;

	uhci->rh.interval = urb->interval;
	init_timer(&uhci->rh.rh_int_timer);
	uhci->rh.rh_int_timer.function = rh_int_timer_do;
	uhci->rh.rh_int_timer.data = (unsigned long)urb;
	uhci->rh.rh_int_timer.expires = jiffies + (HZ * (urb->interval < 30 ? 30 : urb->interval)) / 1000;
	add_timer(&uhci->rh.rh_int_timer);

	return 0;
}

#define OK(x)			len = (x); break

#define CLR_RH_PORTSTAT(x) \
	status = inw(io_addr + USBPORTSC1 + 2 * (wIndex-1)); \
	status = (status & 0xfff5) & ~(x); \
	outw(status, io_addr + USBPORTSC1 + 2 * (wIndex-1))

#define SET_RH_PORTSTAT(x) \
	status = inw(io_addr + USBPORTSC1 + 2 * (wIndex-1)); \
	status = (status & 0xfff5) | (x); \
	outw(status, io_addr + USBPORTSC1 + 2 * (wIndex-1))


/* Root Hub Control Pipe */
static int rh_submit_urb(struct urb *urb)
{
	struct uhci *uhci = (struct uhci *)urb->dev->bus->hcpriv;
	unsigned int pipe = urb->pipe;
	struct usb_ctrlrequest *cmd = (struct usb_ctrlrequest *)urb->setup_packet;
	void *data = urb->transfer_buffer;
	int leni = urb->transfer_buffer_length;
	int len = 0;
	int status = 0;
	int stat = 0;
	int i;
	unsigned int io_addr = uhci->io_addr;
	__u16 cstatus;
	__u16 bmRType_bReq;
	__u16 wValue;
	__u16 wIndex;
	__u16 wLength;

	if (usb_pipetype(pipe) == PIPE_INTERRUPT) {
		uhci->rh.urb = urb;
		uhci->rh.send = 1;
		uhci->rh.interval = urb->interval;
		rh_init_int_timer(urb);

		return -EINPROGRESS;
	}

	bmRType_bReq = cmd->bRequestType | cmd->bRequest << 8;
	wValue = le16_to_cpu(cmd->wValue);
	wIndex = le16_to_cpu(cmd->wIndex);
	wLength = le16_to_cpu(cmd->wLength);

	for (i = 0; i < 8; i++)
		uhci->rh.c_p_r[i] = 0;

	switch (bmRType_bReq) {
		/* Request Destination:
		   without flags: Device,
		   RH_INTERFACE: interface,
		   RH_ENDPOINT: endpoint,
		   RH_CLASS means HUB here,
		   RH_OTHER | RH_CLASS  almost ever means HUB_PORT here
		*/

	case RH_GET_STATUS:
		*(__u16 *)data = cpu_to_le16(1);
		OK(2);
	case RH_GET_STATUS | RH_INTERFACE:
		*(__u16 *)data = cpu_to_le16(0);
		OK(2);
	case RH_GET_STATUS | RH_ENDPOINT:
		*(__u16 *)data = cpu_to_le16(0);
		OK(2);
	case RH_GET_STATUS | RH_CLASS:
		*(__u32 *)data = cpu_to_le32(0);
		OK(4);		/* hub power */
	case RH_GET_STATUS | RH_OTHER | RH_CLASS:
		status = inw(io_addr + USBPORTSC1 + 2 * (wIndex - 1));
		cstatus = ((status & USBPORTSC_CSC) >> (1 - 0)) |
			((status & USBPORTSC_PEC) >> (3 - 1)) |
			(uhci->rh.c_p_r[wIndex - 1] << (0 + 4));
			status = (status & USBPORTSC_CCS) |
			((status & USBPORTSC_PE) >> (2 - 1)) |
			((status & USBPORTSC_SUSP) >> (12 - 2)) |
			((status & USBPORTSC_PR) >> (9 - 4)) |
			(1 << 8) |      /* power on */
			((status & USBPORTSC_LSDA) << (-8 + 9));

		*(__u16 *)data = cpu_to_le16(status);
		*(__u16 *)(data + 2) = cpu_to_le16(cstatus);
		OK(4);
	case RH_CLEAR_FEATURE | RH_ENDPOINT:
		switch (wValue) {
		case RH_ENDPOINT_STALL:
			OK(0);
		}
		break;
	case RH_CLEAR_FEATURE | RH_CLASS:
		switch (wValue) {
		case RH_C_HUB_OVER_CURRENT:
			OK(0);	/* hub power over current */
		}
		break;
	case RH_CLEAR_FEATURE | RH_OTHER | RH_CLASS:
		switch (wValue) {
		case RH_PORT_ENABLE:
			CLR_RH_PORTSTAT(USBPORTSC_PE);
			OK(0);
		case RH_PORT_SUSPEND:
			CLR_RH_PORTSTAT(USBPORTSC_SUSP);
			OK(0);
		case RH_PORT_POWER:
			OK(0);	/* port power */
		case RH_C_PORT_CONNECTION:
			SET_RH_PORTSTAT(USBPORTSC_CSC);
			OK(0);
		case RH_C_PORT_ENABLE:
			SET_RH_PORTSTAT(USBPORTSC_PEC);
			OK(0);
		case RH_C_PORT_SUSPEND:
			/*** WR_RH_PORTSTAT(RH_PS_PSSC); */
			OK(0);
		case RH_C_PORT_OVER_CURRENT:
			OK(0);	/* port power over current */
		case RH_C_PORT_RESET:
			uhci->rh.c_p_r[wIndex - 1] = 0;
			OK(0);
		}
		break;
	case RH_SET_FEATURE | RH_OTHER | RH_CLASS:
		switch (wValue) {
		case RH_PORT_SUSPEND:
			SET_RH_PORTSTAT(USBPORTSC_SUSP);
			OK(0);
		case RH_PORT_RESET:
			SET_RH_PORTSTAT(USBPORTSC_PR);
			mdelay(50);	/* USB v1.1 7.1.7.3 */
			uhci->rh.c_p_r[wIndex - 1] = 1;
			CLR_RH_PORTSTAT(USBPORTSC_PR);
			udelay(10);
			SET_RH_PORTSTAT(USBPORTSC_PE);
			mdelay(10);
			SET_RH_PORTSTAT(0xa);
			OK(0);
		case RH_PORT_POWER:
			OK(0); /* port power ** */
		case RH_PORT_ENABLE:
			SET_RH_PORTSTAT(USBPORTSC_PE);
			OK(0);
		}
		break;
	case RH_SET_ADDRESS:
		uhci->rh.devnum = wValue;
		OK(0);
	case RH_GET_DESCRIPTOR:
		switch ((wValue & 0xff00) >> 8) {
		case 0x01:	/* device descriptor */
			len = min_t(unsigned int, leni,
				  min_t(unsigned int,
				      sizeof(root_hub_dev_des), wLength));
			memcpy(data, root_hub_dev_des, len);
			OK(len);
		case 0x02:	/* configuration descriptor */
			len = min_t(unsigned int, leni,
				  min_t(unsigned int,
				      sizeof(root_hub_config_des), wLength));
			memcpy (data, root_hub_config_des, len);
			OK(len);
		case 0x03:	/* string descriptors */
			len = usb_root_hub_string (wValue & 0xff,
				uhci->io_addr, "UHCI-alt",
				data, wLength);
			if (len > 0) {
				OK(min_t(int, leni, len));
			} else 
				stat = -EPIPE;
		}
		break;
	case RH_GET_DESCRIPTOR | RH_CLASS:
		root_hub_hub_des[2] = uhci->rh.numports;
		len = min_t(unsigned int, leni,
			  min_t(unsigned int, sizeof(root_hub_hub_des), wLength));
		memcpy(data, root_hub_hub_des, len);
		OK(len);
	case RH_GET_CONFIGURATION:
		*(__u8 *)data = 0x01;
		OK(1);
	case RH_SET_CONFIGURATION:
		OK(0);
	case RH_GET_INTERFACE | RH_INTERFACE:
		*(__u8 *)data = 0x00;
		OK(1);
	case RH_SET_INTERFACE | RH_INTERFACE:
		OK(0);
	default:
		stat = -EPIPE;
	}

	urb->actual_length = len;

	return stat;
}

/*
 * MUST be called with urb->lock acquired
 */
static int rh_unlink_urb(struct urb *urb)
{
	struct uhci *uhci = (struct uhci *)urb->dev->bus->hcpriv;

	if (uhci->rh.urb == urb) {
		urb->status = -ENOENT;
		uhci->rh.send = 0;
		uhci->rh.urb = NULL;
		del_timer(&uhci->rh.rh_int_timer);
	}
	return 0;
}

static void uhci_free_pending_qhs(struct uhci *uhci)
{
	struct list_head *tmp, *head;
	unsigned long flags;

	spin_lock_irqsave(&uhci->qh_remove_list_lock, flags);
	head = &uhci->qh_remove_list;
	tmp = head->next;
	while (tmp != head) {
		struct uhci_qh *qh = list_entry(tmp, struct uhci_qh, remove_list);

		tmp = tmp->next;

		list_del_init(&qh->remove_list);

		uhci_free_qh(uhci, qh);
	}
	spin_unlock_irqrestore(&uhci->qh_remove_list_lock, flags);
}

static void uhci_call_completion(struct urb *urb)
{
	struct urb_priv *urbp;
	struct usb_device *dev = urb->dev;
	struct uhci *uhci = (struct uhci *)dev->bus->hcpriv;
	int is_ring = 0, killed, resubmit_interrupt, status;
	struct urb *nurb;
	unsigned long flags;

	spin_lock_irqsave(&urb->lock, flags);

	urbp = (struct urb_priv *)urb->hcpriv;
	if (!urbp || !urb->dev) {
		spin_unlock_irqrestore(&urb->lock, flags);
		return;
	}

	killed = (urb->status == -ENOENT || urb->status == -ECONNABORTED ||
			urb->status == -ECONNRESET);
	resubmit_interrupt = (usb_pipetype(urb->pipe) == PIPE_INTERRUPT &&
			urb->interval);

	nurb = urb->next;
	if (nurb && !killed) {
		int count = 0;

		while (nurb && nurb != urb && count < MAX_URB_LOOP) {
			if (nurb->status == -ENOENT ||
			    nurb->status == -ECONNABORTED ||
			    nurb->status == -ECONNRESET) {
				killed = 1;
				break;
			}

			nurb = nurb->next;
			count++;
		}

		if (count == MAX_URB_LOOP)
			err("uhci_call_completion: too many linked URB's, loop? (first loop)");

		/* Check to see if chain is a ring */
		is_ring = (nurb == urb);
	}

	if (urbp->transfer_buffer_dma_handle)
		pci_dma_sync_single(uhci->dev, urbp->transfer_buffer_dma_handle,
			urb->transfer_buffer_length, usb_pipein(urb->pipe) ?
			PCI_DMA_FROMDEVICE : PCI_DMA_TODEVICE);

	if (urbp->setup_packet_dma_handle)
		pci_dma_sync_single(uhci->dev, urbp->setup_packet_dma_handle,
			sizeof(struct usb_ctrlrequest), PCI_DMA_TODEVICE);

	status = urbp->status;
	if (!resubmit_interrupt || killed)
		/* We don't need urb_priv anymore */
		uhci_destroy_urb_priv(urb);

	if (!killed)
		urb->status = status;

	urb->dev = NULL;
	spin_unlock_irqrestore(&urb->lock, flags);

	if (urb->complete)
		urb->complete(urb);

	if (resubmit_interrupt)
		/* Recheck the status. The completion handler may have */
		/*  unlinked the resubmitting interrupt URB */
		killed = (urb->status == -ENOENT ||
			  urb->status == -ECONNABORTED ||
			  urb->status == -ECONNRESET);

	if (resubmit_interrupt && !killed) {
		urb->dev = dev;
		uhci_reset_interrupt(urb);
	} else {
		if (is_ring && !killed) {
			urb->dev = dev;
			uhci_submit_urb(urb);
		} else {
			/* We decrement the usage count after we're done */
			/*  with everything */
			usb_dec_dev_use(dev);
		}
	}
}

static void uhci_finish_completion(struct uhci *uhci)
{
	struct list_head *tmp, *head;
	unsigned long flags;

	spin_lock_irqsave(&uhci->complete_list_lock, flags);
	head = &uhci->complete_list;
	tmp = head->next;
	while (tmp != head) {
		struct urb_priv *urbp = list_entry(tmp, struct urb_priv, complete_list);
		struct urb *urb = urbp->urb;

		list_del_init(&urbp->complete_list);
		spin_unlock_irqrestore(&uhci->complete_list_lock, flags);

		uhci_call_completion(urb);

		spin_lock_irqsave(&uhci->complete_list_lock, flags);
		head = &uhci->complete_list;
		tmp = head->next;
	}
	spin_unlock_irqrestore(&uhci->complete_list_lock, flags);
}

static void uhci_remove_pending_qhs(struct uhci *uhci)
{
	struct list_head *tmp, *head;
	unsigned long flags;

	spin_lock_irqsave(&uhci->urb_remove_list_lock, flags);
	head = &uhci->urb_remove_list;
	tmp = head->next;
	while (tmp != head) {
		struct urb *urb = list_entry(tmp, struct urb, urb_list);
		struct urb_priv *urbp = (struct urb_priv *)urb->hcpriv;

		tmp = tmp->next;

		list_del_init(&urb->urb_list);

		urbp->status = urb->status = -ECONNRESET;

		uhci_add_complete(urb);
	}
	spin_unlock_irqrestore(&uhci->urb_remove_list_lock, flags);
}

static void uhci_interrupt(int irq, void *__uhci, struct pt_regs *regs)
{
	struct uhci *uhci = __uhci;
	unsigned int io_addr = uhci->io_addr;
	unsigned short status;
	struct list_head *tmp, *head;

	/*
	 * Read the interrupt status, and write it back to clear the
	 * interrupt cause
	 */
	status = inw(io_addr + USBSTS);
	if (!status)	/* shared interrupt, not mine */
		return;
	outw(status, io_addr + USBSTS);		/* Clear it */

	if (status & ~(USBSTS_USBINT | USBSTS_ERROR | USBSTS_RD)) {
		if (status & USBSTS_HSE)
			err("%x: host system error, PCI problems?", io_addr);
		if (status & USBSTS_HCPE)
			err("%x: host controller process error. something bad happened", io_addr);
		if ((status & USBSTS_HCH) && !uhci->is_suspended) {
			err("%x: host controller halted. very bad", io_addr);
			/* FIXME: Reset the controller, fix the offending TD */
		}
	}

	if (status & USBSTS_RD)
		wakeup_hc(uhci);

	uhci_free_pending_qhs(uhci);

	uhci_remove_pending_qhs(uhci);

	uhci_clear_next_interrupt(uhci);

	/* Walk the list of pending URB's to see which ones completed */
	spin_lock(&uhci->urb_list_lock);
	head = &uhci->urb_list;
	tmp = head->next;
	while (tmp != head) {
		struct urb *urb = list_entry(tmp, struct urb, urb_list);

		tmp = tmp->next;

		/* Checks the status and does all of the magic necessary */
		uhci_transfer_result(uhci, urb);
	}
	spin_unlock(&uhci->urb_list_lock);

	uhci_finish_completion(uhci);
}

static void reset_hc(struct uhci *uhci)
{
	unsigned int io_addr = uhci->io_addr;

	/* Global reset for 50ms */
	outw(USBCMD_GRESET, io_addr + USBCMD);
	wait_ms(50);
	outw(0, io_addr + USBCMD);
	wait_ms(10);
}

static void suspend_hc(struct uhci *uhci)
{
	unsigned int io_addr = uhci->io_addr;

	dbg("%x: suspend_hc", io_addr);

	outw(USBCMD_EGSM, io_addr + USBCMD);

	uhci->is_suspended = 1;
}

static void wakeup_hc(struct uhci *uhci)
{
	unsigned int io_addr = uhci->io_addr;
	unsigned int status;

	dbg("%x: wakeup_hc", io_addr);

	outw(0, io_addr + USBCMD);
	
	/* wait for EOP to be sent */
	status = inw(io_addr + USBCMD);
	while (status & USBCMD_FGR)
		status = inw(io_addr + USBCMD);

	uhci->is_suspended = 0;

	/* Run and mark it configured with a 64-byte max packet */
	outw(USBCMD_RS | USBCMD_CF | USBCMD_MAXP, io_addr + USBCMD);
}

static int ports_active(struct uhci *uhci)
{
	unsigned int io_addr = uhci->io_addr;
	int connection = 0;
	int i;

	for (i = 0; i < uhci->rh.numports; i++)
		connection |= (inw(io_addr + USBPORTSC1 + i * 2) & 0x1);

	return connection;
}

static void start_hc(struct uhci *uhci)
{
	unsigned int io_addr = uhci->io_addr;
	int timeout = 1000;

	/*
	 * Reset the HC - this will force us to get a
	 * new notification of any already connected
	 * ports due to the virtual disconnect that it
	 * implies.
	 */
	outw(USBCMD_HCRESET, io_addr + USBCMD);
	while (inw(io_addr + USBCMD) & USBCMD_HCRESET) {
		if (!--timeout) {
			printk(KERN_ERR "uhci: USBCMD_HCRESET timed out!\n");
			break;
		}
	}

	/* Turn on all interrupts */
	outw(USBINTR_TIMEOUT | USBINTR_RESUME | USBINTR_IOC | USBINTR_SP,
		io_addr + USBINTR);

	/* Start at frame 0 */
	outw(0, io_addr + USBFRNUM);
	outl(uhci->fl->dma_handle, io_addr + USBFLBASEADD);

	/* Run and mark it configured with a 64-byte max packet */
	outw(USBCMD_RS | USBCMD_CF | USBCMD_MAXP, io_addr + USBCMD);
}

#ifdef CONFIG_PROC_FS
static int uhci_num = 0;
#endif

static void free_uhci(struct uhci *uhci)
{
	kfree(uhci);
}

/*
 * De-allocate all resources..
 */
static void release_uhci(struct uhci *uhci)
{
	int i;
#ifdef CONFIG_PROC_FS
	char buf[8];
#endif

	if (uhci->irq >= 0) {
		free_irq(uhci->irq, uhci);
		uhci->irq = -1;
	}

	for (i = 0; i < UHCI_NUM_SKELQH; i++)
		if (uhci->skelqh[i]) {
			uhci_free_qh(uhci, uhci->skelqh[i]);
			uhci->skelqh[i] = NULL;
		}

	for (i = 0; i < UHCI_NUM_SKELTD; i++)
		if (uhci->skeltd[i]) {
			uhci_free_td(uhci, uhci->skeltd[i]);
			uhci->skeltd[i] = NULL;
		}

	if (uhci->qh_pool) {
		pci_pool_destroy(uhci->qh_pool);
		uhci->qh_pool = NULL;
	}

	if (uhci->td_pool) {
		pci_pool_destroy(uhci->td_pool);
		uhci->td_pool = NULL;
	}

	if (uhci->fl) {
		pci_free_consistent(uhci->dev, sizeof(*uhci->fl), uhci->fl, uhci->fl->dma_handle);
		uhci->fl = NULL;
	}

	if (uhci->bus) {
		usb_free_bus(uhci->bus);
		uhci->bus = NULL;
	}

#ifdef CONFIG_PROC_FS
	if (uhci->proc_entry) {
		sprintf(buf, "hc%d", uhci->num);

		remove_proc_entry(buf, uhci_proc_root);
		uhci->proc_entry = NULL;
	}
#endif

	free_uhci(uhci);
}

/*
 * Allocate a frame list, and then setup the skeleton
 *
 * The hardware doesn't really know any difference
 * in the queues, but the order does matter for the
 * protocols higher up. The order is:
 *
 *  - any isochronous events handled before any
 *    of the queues. We don't do that here, because
 *    we'll create the actual TD entries on demand.
 *  - The first queue is the interrupt queue.
 *  - The second queue is the control queue, split into low and high speed
 *  - The third queue is bulk queue.
 *  - The fourth queue is the bandwidth reclamation queue, which loops back
 *    to the high speed control queue.
 */
static int alloc_uhci(struct pci_dev *dev, unsigned int io_addr, unsigned int io_size)
{
	struct uhci *uhci;
	int retval;
	char buf[8], *bufp = buf;
	int i, port;
	struct usb_bus *bus;
	dma_addr_t dma_handle;
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *ent;
#endif

	retval = -ENODEV;
	if (pci_enable_device(dev) < 0) {
		err("couldn't enable PCI device");
		goto err_enable_device;
	}

	if (!dev->irq) {
		err("found UHCI device with no IRQ assigned. check BIOS settings!");
		goto err_invalid_irq;
	}

	if (!pci_dma_supported(dev, 0xFFFFFFFF)) {
		err("PCI subsystem doesn't support 32 bit addressing?");
		goto err_pci_dma_supported;
	}

	retval = -EBUSY;
	if (!request_region(io_addr, io_size, "usb-uhci")) {
		err("couldn't allocate I/O range %x - %x", io_addr,
			io_addr + io_size - 1);
		goto err_request_region;
	}

	pci_set_master(dev);

#ifndef __sparc__
	sprintf(buf, "%d", dev->irq);
#else
	bufp = __irq_itoa(dev->irq);
#endif
	printk(KERN_INFO __FILE__ ": USB UHCI at I/O 0x%x, IRQ %s\n",
		io_addr, bufp);

	if (pci_set_dma_mask(dev, 0xFFFFFFFF)) {
		err("couldn't set PCI dma mask");
		retval = -ENODEV;
		goto err_pci_set_dma_mask;
	}

	uhci = kmalloc(sizeof(*uhci), GFP_KERNEL);
	if (!uhci) {
		err("couldn't allocate uhci structure");
		retval = -ENOMEM;
		goto err_alloc_uhci;
	}

	uhci->dev = dev;
	uhci->irq = dev->irq;
	uhci->io_addr = io_addr;
	uhci->io_size = io_size;
	pci_set_drvdata(dev, uhci);

#ifdef CONFIG_PROC_FS
	uhci->num = uhci_num++;

	sprintf(buf, "hc%d", uhci->num);

	ent = create_proc_entry(buf, S_IFREG|S_IRUGO|S_IWUSR, uhci_proc_root);
	if (!ent) {
		err("couldn't create uhci proc entry");
		retval = -ENOMEM;
		goto err_create_proc_entry;
	}

	ent->data = uhci;
	ent->proc_fops = &uhci_proc_operations;
	ent->size = 0;
	uhci->proc_entry = ent;
#endif

	/* Reset here so we don't get any interrupts from an old setup */
	/*  or broken setup */
	reset_hc(uhci);

	uhci->fsbr = 0;
	uhci->fsbrtimeout = 0;

	uhci->is_suspended = 0;

	spin_lock_init(&uhci->qh_remove_list_lock);
	INIT_LIST_HEAD(&uhci->qh_remove_list);

	spin_lock_init(&uhci->urb_remove_list_lock);
	INIT_LIST_HEAD(&uhci->urb_remove_list);

	spin_lock_init(&uhci->urb_list_lock);
	INIT_LIST_HEAD(&uhci->urb_list);

	spin_lock_init(&uhci->complete_list_lock);
	INIT_LIST_HEAD(&uhci->complete_list);

	spin_lock_init(&uhci->frame_list_lock);

	/* We need exactly one page (per UHCI specs), how convenient */
	/* We assume that one page is atleast 4k (1024 frames * 4 bytes) */
#if PAGE_SIZE < (4 * 1024)
#error PAGE_SIZE is not atleast 4k
#endif
	uhci->fl = pci_alloc_consistent(uhci->dev, sizeof(*uhci->fl), &dma_handle);
	if (!uhci->fl) {
		err("unable to allocate consistent memory for frame list");
		goto err_alloc_fl;
	}

	memset((void *)uhci->fl, 0, sizeof(*uhci->fl));

	uhci->fl->dma_handle = dma_handle;

	uhci->td_pool = pci_pool_create("uhci_td", uhci->dev,
		sizeof(struct uhci_td), 16, 0, GFP_DMA | GFP_ATOMIC);
	if (!uhci->td_pool) {
		err("unable to create td pci_pool");
		goto err_create_td_pool;
	}

	uhci->qh_pool = pci_pool_create("uhci_qh", uhci->dev,
		sizeof(struct uhci_qh), 16, 0, GFP_DMA | GFP_ATOMIC);
	if (!uhci->qh_pool) {
		err("unable to create qh pci_pool");
		goto err_create_qh_pool;
	}

	bus = usb_alloc_bus(&uhci_device_operations);
	if (!bus) {
		err("unable to allocate bus");
		goto err_alloc_bus;
	}

	uhci->bus = bus;
	bus->bus_name = dev->slot_name;
	bus->hcpriv = uhci;

	usb_register_bus(uhci->bus);

	/* Initialize the root hub */

	/* UHCI specs says devices must have 2 ports, but goes on to say */
	/*  they may have more but give no way to determine how many they */
	/*  have. However, according to the UHCI spec, Bit 7 is always set */
	/*  to 1. So we try to use this to our advantage */
	for (port = 0; port < (uhci->io_size - 0x10) / 2; port++) {
		unsigned int portstatus;

		portstatus = inw(uhci->io_addr + 0x10 + (port * 2));
		if (!(portstatus & 0x0080))
			break;
	}
	if (debug)
		info("detected %d ports", port);

	/* This is experimental so anything less than 2 or greater than 8 is */
	/*  something weird and we'll ignore it */
	if (port < 2 || port > 8) {
		info("port count misdetected? forcing to 2 ports");
		port = 2;
	}

	uhci->rh.numports = port;

	uhci->bus->root_hub = uhci->rh.dev = usb_alloc_dev(NULL, uhci->bus);
	if (!uhci->rh.dev) {
		err("unable to allocate root hub");
		goto err_alloc_root_hub;
	}

	uhci->skeltd[0] = uhci_alloc_td(uhci, uhci->rh.dev);
	if (!uhci->skeltd[0]) {
		err("unable to allocate TD 0");
		goto err_alloc_skeltd;
	}

	/*
	 * 9 Interrupt queues; link int2 to int1, int4 to int2, etc
	 * then link int1 to control and control to bulk
	 */
	for (i = 1; i < 9; i++) {
		struct uhci_td *td;

		td = uhci->skeltd[i] = uhci_alloc_td(uhci, uhci->rh.dev);
		if (!td) {
			err("unable to allocate TD %d", i);
			goto err_alloc_skeltd;
		}

		uhci_fill_td(td, 0, (UHCI_NULL_DATA_SIZE << 21) | (0x7f << 8) | USB_PID_IN, 0);
		td->link = uhci->skeltd[i - 1]->dma_handle;
	}

	uhci->skel_term_td = uhci_alloc_td(uhci, uhci->rh.dev);
	if (!uhci->skel_term_td) {
		err("unable to allocate skel TD term");
		goto err_alloc_skeltd;
	}

	for (i = 0; i < UHCI_NUM_SKELQH; i++) {
		uhci->skelqh[i] = uhci_alloc_qh(uhci, uhci->rh.dev);
		if (!uhci->skelqh[i]) {
			err("unable to allocate QH %d", i);
			goto err_alloc_skelqh;
		}
	}

	uhci_fill_td(uhci->skel_int1_td, 0, (UHCI_NULL_DATA_SIZE << 21) | (0x7f << 8) | USB_PID_IN, 0);
	uhci->skel_int1_td->link = uhci->skel_ls_control_qh->dma_handle | UHCI_PTR_QH;

	uhci->skel_ls_control_qh->link = uhci->skel_hs_control_qh->dma_handle | UHCI_PTR_QH;
	uhci->skel_ls_control_qh->element = UHCI_PTR_TERM;

	uhci->skel_hs_control_qh->link = uhci->skel_bulk_qh->dma_handle | UHCI_PTR_QH;
	uhci->skel_hs_control_qh->element = UHCI_PTR_TERM;

	uhci->skel_bulk_qh->link = uhci->skel_term_qh->dma_handle | UHCI_PTR_QH;
	uhci->skel_bulk_qh->element = UHCI_PTR_TERM;

	/* This dummy TD is to work around a bug in Intel PIIX controllers */
	uhci_fill_td(uhci->skel_term_td, 0, (UHCI_NULL_DATA_SIZE << 21) | (0x7f << 8) | USB_PID_IN, 0);
	uhci->skel_term_td->link = uhci->skel_term_td->dma_handle;

	uhci->skel_term_qh->link = UHCI_PTR_TERM;
	uhci->skel_term_qh->element = uhci->skel_term_td->dma_handle;

	/*
	 * Fill the frame list: make all entries point to
	 * the proper interrupt queue.
	 *
	 * This is probably silly, but it's a simple way to
	 * scatter the interrupt queues in a way that gives
	 * us a reasonable dynamic range for irq latencies.
	 */
	for (i = 0; i < UHCI_NUMFRAMES; i++) {
		int irq = 0;

		if (i & 1) {
			irq++;
			if (i & 2) {
				irq++;
				if (i & 4) { 
					irq++;
					if (i & 8) { 
						irq++;
						if (i & 16) {
							irq++;
							if (i & 32) {
								irq++;
								if (i & 64)
									irq++;
							}
						}
					}
				}
			}
		}

		/* Only place we don't use the frame list routines */
		uhci->fl->frame[i] =  uhci->skeltd[irq]->dma_handle;
	}

	start_hc(uhci);

	if (request_irq(dev->irq, uhci_interrupt, SA_SHIRQ, "usb-uhci", uhci))
		goto err_request_irq;

	/* disable legacy emulation */
	pci_write_config_word(uhci->dev, USBLEGSUP, USBLEGSUP_DEFAULT);

	usb_connect(uhci->rh.dev);

	if (usb_new_device(uhci->rh.dev) != 0) {
		err("unable to start root hub");
		retval = -ENOMEM;
		goto err_start_root_hub;
	}

	return 0;

/*
 * error exits:
 */
err_start_root_hub:
	free_irq(uhci->irq, uhci);
	uhci->irq = -1;

err_request_irq:
	for (i = 0; i < UHCI_NUM_SKELQH; i++)
		if (uhci->skelqh[i]) {
			uhci_free_qh(uhci, uhci->skelqh[i]);
			uhci->skelqh[i] = NULL;
		}

err_alloc_skelqh:
	for (i = 0; i < UHCI_NUM_SKELTD; i++)
		if (uhci->skeltd[i]) {
			uhci_free_td(uhci, uhci->skeltd[i]);
			uhci->skeltd[i] = NULL;
		}

err_alloc_skeltd:
	usb_free_dev(uhci->rh.dev);
	uhci->rh.dev = NULL;

err_alloc_root_hub:
	usb_free_bus(uhci->bus);
	uhci->bus = NULL;

err_alloc_bus:
	pci_pool_destroy(uhci->qh_pool);
	uhci->qh_pool = NULL;

err_create_qh_pool:
	pci_pool_destroy(uhci->td_pool);
	uhci->td_pool = NULL;

err_create_td_pool:
	pci_free_consistent(uhci->dev, sizeof(*uhci->fl), uhci->fl, uhci->fl->dma_handle);
	uhci->fl = NULL;

err_alloc_fl:
#ifdef CONFIG_PROC_FS
	remove_proc_entry(buf, uhci_proc_root);
	uhci->proc_entry = NULL;

err_create_proc_entry:
	free_uhci(uhci);
#endif

err_alloc_uhci:

err_pci_set_dma_mask:
	release_region(io_addr, io_size);

err_request_region:

err_pci_dma_supported:

err_invalid_irq:

err_enable_device:

	return retval;
}

static int __devinit uhci_pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int i;

	/* Search for the IO base address.. */
	for (i = 0; i < 6; i++) {
		unsigned int io_addr = pci_resource_start(dev, i);
		unsigned int io_size = pci_resource_len(dev, i);

		/* IO address? */
		if (!(pci_resource_flags(dev, i) & IORESOURCE_IO))
			continue;

		return alloc_uhci(dev, io_addr, io_size);
	}

	return -ENODEV;
}

static void __devexit uhci_pci_remove(struct pci_dev *dev)
{
	struct uhci *uhci = pci_get_drvdata(dev);

	if (uhci->bus->root_hub)
		usb_disconnect(&uhci->bus->root_hub);

	usb_deregister_bus(uhci->bus);

	/*
	 * At this point, we're guaranteed that no new connects can be made
	 * to this bus since there are no more parents
	 */
	uhci_free_pending_qhs(uhci);
	uhci_remove_pending_qhs(uhci);

	reset_hc(uhci);
	release_region(uhci->io_addr, uhci->io_size);

	uhci_free_pending_qhs(uhci);

	release_uhci(uhci);
}

#ifdef CONFIG_PM
static int uhci_pci_suspend(struct pci_dev *dev, u32 state)
{
	suspend_hc((struct uhci *) pci_get_drvdata(dev));
	return 0;
}

static int uhci_pci_resume(struct pci_dev *dev)
{
	reset_hc((struct uhci *) pci_get_drvdata(dev));
	start_hc((struct uhci *) pci_get_drvdata(dev));
	return 0;
}
#endif

static const struct pci_device_id __devinitdata uhci_pci_ids[] = { {

	/* handle any USB UHCI controller */
	class: 		((PCI_CLASS_SERIAL_USB << 8) | 0x00),
	class_mask: 	~0,

	/* no matter who makes it */
	vendor:		PCI_ANY_ID,
	device:		PCI_ANY_ID,
	subvendor:	PCI_ANY_ID,
	subdevice:	PCI_ANY_ID,

	}, { /* end: all zeroes */ }
};

MODULE_DEVICE_TABLE(pci, uhci_pci_ids);

static struct pci_driver uhci_pci_driver = {
	name:		"usb-uhci",
	id_table:	uhci_pci_ids,

	probe:		uhci_pci_probe,
	remove:		__devexit_p(uhci_pci_remove),

#ifdef	CONFIG_PM
	suspend:	uhci_pci_suspend,
	resume:		uhci_pci_resume,
#endif	/* PM */
};

 
static int __init uhci_hcd_init(void)
{
	int retval = -ENOMEM;

	info(DRIVER_DESC " " DRIVER_VERSION);

	if (debug) {
		errbuf = kmalloc(ERRBUF_LEN, GFP_KERNEL);
		if (!errbuf)
			goto errbuf_failed;
	}

#ifdef CONFIG_PROC_FS
	uhci_proc_root = create_proc_entry("driver/uhci", S_IFDIR, 0);
	if (!uhci_proc_root)
		goto proc_failed;
#endif

	uhci_up_cachep = kmem_cache_create("uhci_urb_priv",
		sizeof(struct urb_priv), 0, 0, NULL, NULL);
	if (!uhci_up_cachep)
		goto up_failed;

	retval = pci_module_init(&uhci_pci_driver);
	if (retval)
		goto init_failed;

	return 0;

init_failed:
	if (kmem_cache_destroy(uhci_up_cachep))
		printk(KERN_INFO "uhci: not all urb_priv's were freed\n");

up_failed:

#ifdef CONFIG_PROC_FS
	remove_proc_entry("driver/uhci", 0);

proc_failed:
#endif
	if (errbuf)
		kfree(errbuf);

errbuf_failed:

	return retval;
}

static void __exit uhci_hcd_cleanup(void) 
{
	pci_unregister_driver(&uhci_pci_driver);
	
	if (kmem_cache_destroy(uhci_up_cachep))
		printk(KERN_INFO "uhci: not all urb_priv's were freed\n");

#ifdef CONFIG_PROC_FS
	remove_proc_entry("driver/uhci", 0);
#endif

	if (errbuf)
		kfree(errbuf);
}

module_init(uhci_hcd_init);
module_exit(uhci_hcd_cleanup);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

