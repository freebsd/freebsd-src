/* 
 * Universal Host Controller Interface driver for USB (take II).
 *
 * (c) 1999-2001 Georg Acher, acher@in.tum.de (executive slave) (base guitar)
 *               Deti Fliegl, deti@fliegl.de (executive slave) (lead voice)
 *               Thomas Sailer, sailer@ife.ee.ethz.ch (chief consultant) (cheer leader)
 *               Roman Weissgaerber, weissg@vienna.at (virt root hub) (studio porter)
 * (c) 2000      Yggdrasil Computing, Inc. (port of new PCI interface support
 *               from usb-ohci.c by Adam Richter, adam@yggdrasil.com).
 * (C) 2000      David Brownell, david-b@pacbell.net (usb-ohci.c)
 *          
 * HW-initalization based on material of
 *
 * (C) Copyright 1999 Linus Torvalds
 * (C) Copyright 1999 Johannes Erdfelt
 * (C) Copyright 1999 Randy Dunlap
 * (C) Copyright 1999 Gregory P. Smith
 *
 * $Id: usb-uhci.c,v 1.275 2002/01/19 20:57:33 acher Exp $
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/errno.h>
#include <linux/unistd.h>
#include <linux/interrupt.h>	/* for in_interrupt() */
#include <linux/init.h>
#include <linux/version.h>
#include <linux/pm.h>
#include <linux/timer.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>

/* This enables more detailed sanity checks in submit_iso */
//#define ISO_SANITY_CHECK

/* This enables debug printks */
#define DEBUG

/* This enables all symbols to be exported, to ease debugging oopses */
//#define DEBUG_SYMBOLS

/* This enables an extra UHCI slab for memory debugging */
#define DEBUG_SLAB

#define VERSTR "$Revision: 1.275 $ time " __TIME__ " " __DATE__

#include <linux/usb.h>
#include "usb-uhci.h"
#include "usb-uhci-debug.h"

#include "../hcd.h"

/*
 * Version Information
 */
#define DRIVER_VERSION "v1.275"
#define DRIVER_AUTHOR "Georg Acher, Deti Fliegl, Thomas Sailer, Roman Weissgaerber"
#define DRIVER_DESC "USB Universal Host Controller Interface driver"

#undef DEBUG
#undef dbg
#define dbg(format, arg...) do {} while (0)
#define DEBUG_SYMBOLS
#ifdef DEBUG_SYMBOLS
	#define _static
	#ifndef EXPORT_SYMTAB
		#define EXPORT_SYMTAB
	#endif
#else
	#define _static static
#endif

#define queue_dbg dbg //err
#define async_dbg dbg //err

#ifdef DEBUG_SLAB
	static kmem_cache_t *urb_priv_kmem;
#endif

#define SLAB_FLAG     (in_interrupt () || current->state != TASK_RUNNING ? SLAB_ATOMIC : SLAB_NOIO)
#define KMALLOC_FLAG  (in_interrupt () || current->state != TASK_RUNNING ? GFP_ATOMIC : GFP_NOIO)

/* CONFIG_USB_UHCI_HIGH_BANDWITH turns on Full Speed Bandwidth
 * Reclamation: feature that puts loop on descriptor loop when
 * there's some transfer going on. With FSBR, USB performance
 * is optimal, but PCI can be slowed down up-to 5 times, slowing down
 * system performance (eg. framebuffer devices).
 */
#define CONFIG_USB_UHCI_HIGH_BANDWIDTH

/* *_DEPTH_FIRST puts descriptor in depth-first mode. This has
 * somehow similar effect to FSBR (higher speed), but does not
 * slow PCI down. OTOH USB performace is slightly slower than
 * in FSBR case and single device could hog whole USB, starving
 * other devices.
 */
#define USE_CTRL_DEPTH_FIRST 0  // 0: Breadth first, 1: Depth first
#define USE_BULK_DEPTH_FIRST 0  // 0: Breadth first, 1: Depth first

/* Turning off both CONFIG_USB_UHCI_HIGH_BANDWITH and *_DEPTH_FIRST
 * will lead to <64KB/sec performance over USB for bulk transfers targeting
 * one device's endpoint. You probably do not want to do that.
 */

// stop bandwidth reclamation after (roughly) 50ms
#define IDLE_TIMEOUT  (HZ/20)

// Suppress HC interrupt error messages for 5s
#define ERROR_SUPPRESSION_TIME (HZ*5)

_static int rh_submit_urb (struct urb *urb);
_static int rh_unlink_urb (struct urb *urb);
_static int delete_qh (uhci_t *s, uhci_desc_t *qh);
_static int process_transfer (uhci_t *s, struct urb *urb, int mode);
_static int process_interrupt (uhci_t *s, struct urb *urb);
_static int process_iso (uhci_t *s, struct urb *urb, int force);

// How much URBs with ->next are walked
#define MAX_NEXT_COUNT 2048

static uhci_t *devs = NULL;

/* used by userspace UHCI data structure dumper */
uhci_t **uhci_devices = &devs;

/*-------------------------------------------------------------------*/
// Cleans up collected QHs, but not more than 100 in one go
void clean_descs(uhci_t *s, int force)
{
	struct list_head *q;
	uhci_desc_t *qh;
	int now=UHCI_GET_CURRENT_FRAME(s), n=0;

	q=s->free_desc.prev;

	while (q != &s->free_desc && (force || n<100)) {
		qh = list_entry (q, uhci_desc_t, horizontal);
		q=qh->horizontal.prev;

		if ((qh->last_used!=now) || force)
			delete_qh(s,qh);
		n++;
	}
}
/*-------------------------------------------------------------------*/
_static void uhci_switch_timer_int(uhci_t *s)
{

	if (!list_empty(&s->urb_unlinked))
		set_td_ioc(s->td1ms);
	else
		clr_td_ioc(s->td1ms);

	if (s->timeout_urbs)
		set_td_ioc(s->td32ms);
	else
		clr_td_ioc(s->td32ms);
	wmb();
}
/*-------------------------------------------------------------------*/
#ifdef CONFIG_USB_UHCI_HIGH_BANDWIDTH
_static void enable_desc_loop(uhci_t *s, struct urb *urb)
{
	unsigned long flags;

	if (urb->transfer_flags & USB_NO_FSBR)
		return;

	spin_lock_irqsave (&s->qh_lock, flags);
	s->chain_end->hw.qh.head&=cpu_to_le32(~UHCI_PTR_TERM);
	mb();
	s->loop_usage++;
	((urb_priv_t*)urb->hcpriv)->use_loop=1;
	spin_unlock_irqrestore (&s->qh_lock, flags);
}
/*-------------------------------------------------------------------*/
_static void disable_desc_loop(uhci_t *s, struct urb *urb)
{
	unsigned long flags;

	if (urb->transfer_flags & USB_NO_FSBR)
		return;

	spin_lock_irqsave (&s->qh_lock, flags);
	if (((urb_priv_t*)urb->hcpriv)->use_loop) {
		s->loop_usage--;

		if (!s->loop_usage) {
			s->chain_end->hw.qh.head|=cpu_to_le32(UHCI_PTR_TERM);
			mb();
		}
		((urb_priv_t*)urb->hcpriv)->use_loop=0;
	}
	spin_unlock_irqrestore (&s->qh_lock, flags);
}
#endif
/*-------------------------------------------------------------------*/
_static void queue_urb_unlocked (uhci_t *s, struct urb *urb)
{
	struct list_head *p=&urb->urb_list;
#ifdef CONFIG_USB_UHCI_HIGH_BANDWIDTH
	{
		int type;
		type=usb_pipetype (urb->pipe);

		if ((type == PIPE_BULK) || (type == PIPE_CONTROL))
			enable_desc_loop(s, urb);
	}
#endif
	urb->status = -EINPROGRESS;
	((urb_priv_t*)urb->hcpriv)->started=jiffies;
	list_add (p, &s->urb_list);
	if (urb->timeout)
		s->timeout_urbs++;
	uhci_switch_timer_int(s);
}
/*-------------------------------------------------------------------*/
_static void queue_urb (uhci_t *s, struct urb *urb)
{
	unsigned long flags=0;

	spin_lock_irqsave (&s->urb_list_lock, flags);
	queue_urb_unlocked(s,urb);
	spin_unlock_irqrestore (&s->urb_list_lock, flags);
}
/*-------------------------------------------------------------------*/
_static void dequeue_urb (uhci_t *s, struct urb *urb)
{
#ifdef CONFIG_USB_UHCI_HIGH_BANDWIDTH
	int type;

	type=usb_pipetype (urb->pipe);

	if ((type == PIPE_BULK) || (type == PIPE_CONTROL))
		disable_desc_loop(s, urb);
#endif

	list_del (&urb->urb_list);
	if (urb->timeout && s->timeout_urbs)
		s->timeout_urbs--;

}
/*-------------------------------------------------------------------*/
_static int alloc_td (uhci_t *s, uhci_desc_t ** new, int flags)
{
	dma_addr_t dma_handle;

	*new = pci_pool_alloc(s->desc_pool, GFP_DMA | GFP_ATOMIC, &dma_handle);
	if (!*new)
		return -ENOMEM;
	memset (*new, 0, sizeof (uhci_desc_t));
	(*new)->dma_addr = dma_handle;
	set_td_link((*new), UHCI_PTR_TERM | (flags & UHCI_PTR_BITS));	// last by default
	(*new)->type = TD_TYPE;
	mb();
	INIT_LIST_HEAD (&(*new)->vertical);
	INIT_LIST_HEAD (&(*new)->horizontal);
	
	return 0;
}
/*-------------------------------------------------------------------*/
// append a qh to td.link physically, the SW linkage is not affected
_static void append_qh(uhci_t *s, uhci_desc_t *td, uhci_desc_t* qh, int  flags)
{
	unsigned long xxx;
	
	spin_lock_irqsave (&s->td_lock, xxx);

	set_td_link(td, qh->dma_addr | (flags & UHCI_PTR_DEPTH) | UHCI_PTR_QH);
       
	mb();
	spin_unlock_irqrestore (&s->td_lock, xxx);
}
/*-------------------------------------------------------------------*/
/* insert td at last position in td-list of qh (vertical) */
_static int insert_td (uhci_t *s, uhci_desc_t *qh, uhci_desc_t* new, int flags)
{
	uhci_desc_t *prev;
	unsigned long xxx;
	
	spin_lock_irqsave (&s->td_lock, xxx);

	list_add_tail (&new->vertical, &qh->vertical);

	prev = list_entry (new->vertical.prev, uhci_desc_t, vertical);

	if (qh == prev ) {
		// virgin qh without any tds
		set_qh_element(qh, new->dma_addr | UHCI_PTR_TERM);
	}
	else {
		// already tds inserted, implicitely remove TERM bit of prev
		set_td_link(prev, new->dma_addr | (flags & UHCI_PTR_DEPTH));
	}
	mb();
	spin_unlock_irqrestore (&s->td_lock, xxx);
	
	return 0;
}
/*-------------------------------------------------------------------*/
/* insert new_td after td (horizontal) */
_static int insert_td_horizontal (uhci_t *s, uhci_desc_t *td, uhci_desc_t* new)
{
	uhci_desc_t *next;
	unsigned long flags;
	
	spin_lock_irqsave (&s->td_lock, flags);

	next = list_entry (td->horizontal.next, uhci_desc_t, horizontal);
	list_add (&new->horizontal, &td->horizontal);
	new->hw.td.link = td->hw.td.link;
	set_td_link(td, new->dma_addr);
	mb();
	spin_unlock_irqrestore (&s->td_lock, flags);	
	
	return 0;
}
/*-------------------------------------------------------------------*/
_static int unlink_td (uhci_t *s, uhci_desc_t *element, int phys_unlink)
{
	uhci_desc_t *next, *prev;
	int dir = 0;
	unsigned long flags;
	
	spin_lock_irqsave (&s->td_lock, flags);
	
	next = list_entry (element->vertical.next, uhci_desc_t, vertical);
	
	if (next == element) {
		dir = 1;
		prev = list_entry (element->horizontal.prev, uhci_desc_t, horizontal);
	}
	else 
		prev = list_entry (element->vertical.prev, uhci_desc_t, vertical);
	
	if (phys_unlink) {
		// really remove HW linking
		if (prev->type == TD_TYPE)
			prev->hw.td.link = element->hw.td.link;
		else
			prev->hw.qh.element = element->hw.td.link;
	}

	mb ();

	if (dir == 0)
		list_del (&element->vertical);
	else
		list_del (&element->horizontal);
	
	spin_unlock_irqrestore (&s->td_lock, flags);	
	
	return 0;
}

/*-------------------------------------------------------------------*/
_static int delete_desc (uhci_t *s, uhci_desc_t *element)
{
	pci_pool_free(s->desc_pool, element, element->dma_addr);
	return 0;
}
/*-------------------------------------------------------------------*/
// Allocates qh element
_static int alloc_qh (uhci_t *s, uhci_desc_t ** new)
{
	dma_addr_t dma_handle;

	*new = pci_pool_alloc(s->desc_pool, GFP_DMA | GFP_ATOMIC, &dma_handle);
	if (!*new)
		return -ENOMEM;
	memset (*new, 0, sizeof (uhci_desc_t));
	(*new)->dma_addr = dma_handle;
	set_qh_head(*new, UHCI_PTR_TERM);
	set_qh_element(*new, UHCI_PTR_TERM);
	(*new)->type = QH_TYPE;
	
	mb();
	INIT_LIST_HEAD (&(*new)->horizontal);
	INIT_LIST_HEAD (&(*new)->vertical);
	
	dbg("Allocated qh @ %p", *new);
	
	return 0;
}
/*-------------------------------------------------------------------*/
// inserts new qh before/after the qh at pos
// flags: 0: insert before pos, 1: insert after pos (for low speed transfers)
_static int insert_qh (uhci_t *s, uhci_desc_t *pos, uhci_desc_t *new, int order)
{
	uhci_desc_t *old;
	unsigned long flags;

	spin_lock_irqsave (&s->qh_lock, flags);

	if (!order) {
		// (OLD) (POS) -> (OLD) (NEW) (POS)
		old = list_entry (pos->horizontal.prev, uhci_desc_t, horizontal);
		list_add_tail (&new->horizontal, &pos->horizontal);
		set_qh_head(new, MAKE_QH_ADDR (pos)) ;
		if (!(old->hw.qh.head & cpu_to_le32(UHCI_PTR_TERM)))
			set_qh_head(old, MAKE_QH_ADDR (new)) ;
	}
	else {
		// (POS) (OLD) -> (POS) (NEW) (OLD)
		old = list_entry (pos->horizontal.next, uhci_desc_t, horizontal);
		list_add (&new->horizontal, &pos->horizontal);
		set_qh_head(new, MAKE_QH_ADDR (old));
		set_qh_head(pos, MAKE_QH_ADDR (new)) ;
	}

	mb ();
	
	spin_unlock_irqrestore (&s->qh_lock, flags);

	return 0;
}

/*-------------------------------------------------------------------*/
_static int unlink_qh (uhci_t *s, uhci_desc_t *element)
{
	uhci_desc_t  *prev;
	unsigned long flags;

	spin_lock_irqsave (&s->qh_lock, flags);
	
	prev = list_entry (element->horizontal.prev, uhci_desc_t, horizontal);
	prev->hw.qh.head = element->hw.qh.head;

	dbg("unlink qh %p, pqh %p, nxqh %p, to %08x", element, prev, 
	    list_entry (element->horizontal.next, uhci_desc_t, horizontal),le32_to_cpu(element->hw.qh.head) &~15);
	
	list_del(&element->horizontal);

	mb ();
	spin_unlock_irqrestore (&s->qh_lock, flags);
	
	return 0;
}
/*-------------------------------------------------------------------*/
_static int delete_qh (uhci_t *s, uhci_desc_t *qh)
{
	uhci_desc_t *td;
	struct list_head *p;
	
	list_del (&qh->horizontal);

	while ((p = qh->vertical.next) != &qh->vertical) {
		td = list_entry (p, uhci_desc_t, vertical);
		dbg("unlink td @ %p",td);
		unlink_td (s, td, 0); // no physical unlink
		delete_desc (s, td);
	}

	delete_desc (s, qh);
	
	return 0;
}
/*-------------------------------------------------------------------*/
_static void clean_td_chain (uhci_t *s, uhci_desc_t *td)
{
	struct list_head *p;
	uhci_desc_t *td1;

	if (!td)
		return;
	
	while ((p = td->horizontal.next) != &td->horizontal) {
		td1 = list_entry (p, uhci_desc_t, horizontal);
		delete_desc (s, td1);
	}
	
	delete_desc (s, td);
}

/*-------------------------------------------------------------------*/
_static void fill_td (uhci_desc_t *td, int status, int info, __u32 buffer)
{
	td->hw.td.status = cpu_to_le32(status);
	td->hw.td.info = cpu_to_le32(info);
	td->hw.td.buffer = cpu_to_le32(buffer);
}
/*-------------------------------------------------------------------*/
// Removes ALL qhs in chain (paranoia!)
_static void cleanup_skel (uhci_t *s)
{
	unsigned int n;
	uhci_desc_t *td;

	dbg("cleanup_skel");

	clean_descs(s,1);

	
	if (s->td32ms) {
	
		unlink_td(s,s->td32ms,1);
		delete_desc(s, s->td32ms);
	}

	for (n = 0; n < 8; n++) {
		td = s->int_chain[n];
		clean_td_chain (s, td);
	}

	if (s->iso_td) {
		for (n = 0; n < 1024; n++) {
			td = s->iso_td[n];
			clean_td_chain (s, td);
		}
		kfree (s->iso_td);
	}

	if (s->framelist)
		pci_free_consistent(s->uhci_pci, PAGE_SIZE,
				    s->framelist, s->framelist_dma);

	if (s->control_chain) {
		// completed init_skel?
		struct list_head *p;
		uhci_desc_t *qh, *qh1;

		qh = s->control_chain;
		while ((p = qh->horizontal.next) != &qh->horizontal) {
			qh1 = list_entry (p, uhci_desc_t, horizontal);
			delete_qh (s, qh1);
		}

		delete_qh (s, qh);
	}
	else {
		if (s->ls_control_chain)
			delete_desc (s, s->ls_control_chain);
		if (s->control_chain)
			delete_desc (s, s->control_chain);
		if (s->bulk_chain)
			delete_desc (s, s->bulk_chain);
		if (s->chain_end)
			delete_desc (s, s->chain_end);
	}

	if (s->desc_pool) {
		pci_pool_destroy(s->desc_pool);
		s->desc_pool = NULL;
	}

	dbg("cleanup_skel finished");	
}
/*-------------------------------------------------------------------*/
// allocates framelist and qh-skeletons
// only HW-links provide continous linking, SW-links stay in their domain (ISO/INT)
_static int init_skel (uhci_t *s)
{
	int n, ret;
	uhci_desc_t *qh, *td;
	
	dbg("init_skel");
	
	s->framelist = pci_alloc_consistent(s->uhci_pci, PAGE_SIZE,
					    &s->framelist_dma);

	if (!s->framelist)
		return -ENOMEM;

	memset (s->framelist, 0, 4096);

	dbg("creating descriptor pci_pool");

	s->desc_pool = pci_pool_create("uhci_desc", s->uhci_pci,
				       sizeof(uhci_desc_t), 16, 0,
				       GFP_DMA | GFP_ATOMIC);	
	if (!s->desc_pool)
		goto init_skel_cleanup;

	dbg("allocating iso desc pointer list");
	s->iso_td = (uhci_desc_t **) kmalloc (1024 * sizeof (uhci_desc_t*), GFP_KERNEL);
	
	if (!s->iso_td)
		goto init_skel_cleanup;

	s->ls_control_chain = NULL;
	s->control_chain = NULL;
	s->bulk_chain = NULL;
	s->chain_end = NULL;

	dbg("allocating iso descs");
	for (n = 0; n < 1024; n++) {
	 	// allocate skeleton iso/irq-tds
		if (alloc_td (s, &td, 0))
			goto init_skel_cleanup;

		s->iso_td[n] = td;
		s->framelist[n] = cpu_to_le32((__u32) td->dma_addr);
	}

	dbg("allocating qh: chain_end");
	if (alloc_qh (s, &qh))	
		goto init_skel_cleanup;
				
	s->chain_end = qh;

	if (alloc_td (s, &td, 0))
		goto init_skel_cleanup;
	
	fill_td (td, 0 * TD_CTRL_IOC, 0, 0); // generate 1ms interrupt (enabled on demand)
	insert_td (s, qh, td, 0);
	qh->hw.qh.element &= cpu_to_le32(~UHCI_PTR_TERM); // remove TERM bit
	s->td1ms=td;

	dbg("allocating qh: bulk_chain");
	if (alloc_qh (s, &qh))
		goto init_skel_cleanup;
	
	insert_qh (s, s->chain_end, qh, 0);
	s->bulk_chain = qh;

	dbg("allocating qh: control_chain");
	ret = alloc_qh (s, &qh);
	if (ret)
		goto init_skel_cleanup;
	
	insert_qh (s, s->bulk_chain, qh, 0);
	s->control_chain = qh;

#ifdef	CONFIG_USB_UHCI_HIGH_BANDWIDTH
	// disabled reclamation loop
	set_qh_head(s->chain_end, s->control_chain->dma_addr | UHCI_PTR_QH | UHCI_PTR_TERM);
#endif

	dbg("allocating qh: ls_control_chain");
	if (alloc_qh (s, &qh))
		goto init_skel_cleanup;
	
	insert_qh (s, s->control_chain, qh, 0);
	s->ls_control_chain = qh;

	for (n = 0; n < 8; n++)
		s->int_chain[n] = 0;

	dbg("allocating skeleton INT-TDs");
	
	for (n = 0; n < 8; n++) {
		uhci_desc_t *td;

		if (alloc_td (s, &td, 0))
			goto init_skel_cleanup;

		s->int_chain[n] = td;
		if (n == 0) {
			set_td_link(s->int_chain[0], s->ls_control_chain->dma_addr | UHCI_PTR_QH);
		}
		else {
			set_td_link(s->int_chain[n], s->int_chain[0]->dma_addr);
		}
	}

	dbg("Linking skeleton INT-TDs");
	
	for (n = 0; n < 1024; n++) {
		// link all iso-tds to the interrupt chains
		int m, o;
		dbg("framelist[%i]=%x",n,le32_to_cpu(s->framelist[n]));
		if ((n&127)==127) 
			((uhci_desc_t*) s->iso_td[n])->hw.td.link = cpu_to_le32(s->int_chain[0]->dma_addr);
		else 
			for (o = 1, m = 2; m <= 128; o++, m += m)
				if ((n & (m - 1)) == ((m - 1) / 2))
					set_td_link(((uhci_desc_t*) s->iso_td[n]), s->int_chain[o]->dma_addr);
	}

	if (alloc_td (s, &td, 0))
		goto init_skel_cleanup;
	
	fill_td (td, 0 * TD_CTRL_IOC, 0, 0); // generate 32ms interrupt (activated later)
	s->td32ms=td;

	insert_td_horizontal (s, s->int_chain[5], td);

	mb();
	//uhci_show_queue(s->control_chain);   
	dbg("init_skel exit");
	return 0;

      init_skel_cleanup:
	cleanup_skel (s);
	return -ENOMEM;
}

/*-------------------------------------------------------------------*/
//                         LOW LEVEL STUFF
//          assembles QHs und TDs for control, bulk and iso
/*-------------------------------------------------------------------*/
_static int uhci_submit_control_urb (struct urb *urb)
{
	uhci_desc_t *qh, *td;
	uhci_t *s = (uhci_t*) urb->dev->bus->hcpriv;
	urb_priv_t *urb_priv = urb->hcpriv;
	unsigned long destination, status;
	int maxsze = usb_maxpacket (urb->dev, urb->pipe, usb_pipeout (urb->pipe));
	unsigned long len;
	char *data;
	int depth_first=USE_CTRL_DEPTH_FIRST;  // UHCI descriptor chasing method

	dbg("uhci_submit_control start");
	if (alloc_qh (s, &qh))		// alloc qh for this request
		return -ENOMEM;

	if (alloc_td (s, &td, UHCI_PTR_DEPTH * depth_first))		// get td for setup stage
	{
		delete_qh (s, qh);
		return -ENOMEM;
	}

	/* The "pipe" thing contains the destination in bits 8--18 */
	destination = (urb->pipe & PIPE_DEVEP_MASK) | USB_PID_SETUP;

	/* 3 errors */
	status = (urb->pipe & TD_CTRL_LS) | TD_CTRL_ACTIVE |
		(urb->transfer_flags & USB_DISABLE_SPD ? 0 : TD_CTRL_SPD) | (3 << 27);

	/*  Build the TD for the control request, try forever, 8 bytes of data */
	fill_td (td, status, destination | (7 << 21), urb_priv->setup_packet_dma);

	insert_td (s, qh, td, 0);	// queue 'setup stage'-td in qh
#if 0
	{
		char *sp=urb->setup_packet;
		dbg("SETUP to pipe %x: %x %x %x %x %x %x %x %x", urb->pipe,
		    sp[0],sp[1],sp[2],sp[3],sp[4],sp[5],sp[6],sp[7]);
	}
	//uhci_show_td(td);
#endif

	len = urb->transfer_buffer_length;
	data = urb->transfer_buffer;

	/* If direction is "send", change the frame from SETUP (0x2D)
	   to OUT (0xE1). Else change it from SETUP to IN (0x69). */

	destination = (urb->pipe & PIPE_DEVEP_MASK) | (usb_pipeout (urb->pipe)?USB_PID_OUT:USB_PID_IN);

	while (len > 0) {
		int pktsze = len;

		if (alloc_td (s, &td, UHCI_PTR_DEPTH * depth_first))
			goto fail_unmap_enomem;

		if (pktsze > maxsze)
			pktsze = maxsze;

		destination ^= 1 << TD_TOKEN_TOGGLE;	// toggle DATA0/1

		// Status, pktsze bytes of data
		fill_td (td, status, destination | ((pktsze - 1) << 21),
			 urb_priv->transfer_buffer_dma + (data - (char *)urb->transfer_buffer));

		insert_td (s, qh, td, UHCI_PTR_DEPTH * depth_first);	// queue 'data stage'-td in qh

		data += pktsze;
		len -= pktsze;
	}

	/* Build the final TD for control status */
	/* It's only IN if the pipe is out AND we aren't expecting data */

	destination &= ~UHCI_PID;

	if (usb_pipeout (urb->pipe) || (urb->transfer_buffer_length == 0))
		destination |= USB_PID_IN;
	else
		destination |= USB_PID_OUT;

	destination |= 1 << TD_TOKEN_TOGGLE;	/* End in Data1 */

	if (alloc_td (s, &td, UHCI_PTR_DEPTH))
		goto fail_unmap_enomem;

	status &=~TD_CTRL_SPD;

	/* no limit on errors on final packet , 0 bytes of data */
	fill_td (td, status | TD_CTRL_IOC, destination | (UHCI_NULL_DATA_SIZE << 21),
		 0);

	insert_td (s, qh, td, UHCI_PTR_DEPTH * depth_first);	// queue status td

	list_add (&qh->desc_list, &urb_priv->desc_list);

	queue_urb (s, urb);	// queue before inserting in desc chain

	qh->hw.qh.element &= cpu_to_le32(~UHCI_PTR_TERM);

	//uhci_show_queue(qh);
	/* Start it up... put low speed first */
	if (urb->pipe & TD_CTRL_LS)
		insert_qh (s, s->control_chain, qh, 0);
	else
		insert_qh (s, s->bulk_chain, qh, 0);

	dbg("uhci_submit_control end");
	return 0;

fail_unmap_enomem:
	delete_qh(s, qh);
	return -ENOMEM;
}
/*-------------------------------------------------------------------*/
// For queued bulk transfers, two additional QH helpers are allocated (nqh, bqh)
// Due to the linking with other bulk urbs, it has to be locked with urb_list_lock!

_static int uhci_submit_bulk_urb (struct urb *urb, struct urb *bulk_urb)
{
	uhci_t *s = (uhci_t*) urb->dev->bus->hcpriv;
	urb_priv_t *urb_priv = urb->hcpriv, *upriv, *bpriv=NULL;
	uhci_desc_t *qh, *td, *nqh=NULL, *bqh=NULL, *first_td=NULL;
	unsigned long destination, status;
	char *data;
	unsigned int pipe = urb->pipe;
	int maxsze = usb_maxpacket (urb->dev, pipe, usb_pipeout (pipe));
	int info, len, last;
	int depth_first=USE_BULK_DEPTH_FIRST;  // UHCI descriptor chasing method

	if (usb_endpoint_halted (urb->dev, usb_pipeendpoint (pipe), usb_pipeout (pipe)))
		return -EPIPE;

	queue_dbg("uhci_submit_bulk_urb: urb %p, old %p, pipe %08x, len %i",
		  urb,bulk_urb,urb->pipe,urb->transfer_buffer_length);

	upriv = (urb_priv_t*)urb->hcpriv;

	if (!bulk_urb) {
		if (alloc_qh (s, &qh))		// get qh for this request
			return -ENOMEM;

		if (urb->transfer_flags & USB_QUEUE_BULK) {
			if (alloc_qh(s, &nqh)) // placeholder for clean unlink
			{
				delete_desc (s, qh);
				return -ENOMEM;
			}
			upriv->next_qh = nqh;
			queue_dbg("new next qh %p",nqh);
		}
	}
	else { 
		bpriv = (urb_priv_t*)bulk_urb->hcpriv;
		qh = bpriv->bottom_qh;  // re-use bottom qh and next qh
		nqh = bpriv->next_qh;
		upriv->next_qh=nqh;	
		upriv->prev_queued_urb=bulk_urb;
	}

	if (urb->transfer_flags & USB_QUEUE_BULK) {
		if (alloc_qh (s, &bqh))  // "bottom" QH
		{
			if (!bulk_urb) { 
				delete_desc(s, qh);
				delete_desc(s, nqh);
			}
			return -ENOMEM;
		}
		set_qh_element(bqh, UHCI_PTR_TERM);
		set_qh_head(bqh, nqh->dma_addr | UHCI_PTR_QH); // element
		upriv->bottom_qh = bqh;
	}
	queue_dbg("uhci_submit_bulk: qh %p bqh %p nqh %p",qh, bqh, nqh);

	/* The "pipe" thing contains the destination in bits 8--18. */
	destination = (pipe & PIPE_DEVEP_MASK) | usb_packetid (pipe);

	/* 3 errors */
	status = (pipe & TD_CTRL_LS) | TD_CTRL_ACTIVE |
		((urb->transfer_flags & USB_DISABLE_SPD) ? 0 : TD_CTRL_SPD) | (3 << 27);

	/* Build the TDs for the bulk request */
	len = urb->transfer_buffer_length;
	data = urb->transfer_buffer;
	
	do {					// TBD: Really allow zero-length packets?
		int pktsze = len;

		if (alloc_td (s, &td, UHCI_PTR_DEPTH * depth_first))
		{
			delete_qh (s, qh);
			return -ENOMEM;
		}

		if (pktsze > maxsze)
			pktsze = maxsze;

		// pktsze bytes of data 
		info = destination | (((pktsze - 1)&UHCI_NULL_DATA_SIZE) << 21) |
			(usb_gettoggle (urb->dev, usb_pipeendpoint (pipe), usb_pipeout (pipe)) << TD_TOKEN_TOGGLE);

		fill_td (td, status, info,
			 urb_priv->transfer_buffer_dma + (data - (char *)urb->transfer_buffer));

		data += pktsze;
		len -= pktsze;
		// Use USB_ZERO_PACKET to finish bulk OUTs always with a zero length packet
		last = (len == 0 && (usb_pipein(pipe) || pktsze < maxsze || !(urb->transfer_flags & USB_ZERO_PACKET)));

		if (last)
			set_td_ioc(td);	// last one generates INT

		insert_td (s, qh, td, UHCI_PTR_DEPTH * depth_first);
		if (!first_td)
			first_td=td;
		usb_dotoggle (urb->dev, usb_pipeendpoint (pipe), usb_pipeout (pipe));

	} while (!last);

	if (bulk_urb && bpriv)   // everything went OK, link with old bulk URB
		bpriv->next_queued_urb=urb;

	list_add (&qh->desc_list, &urb_priv->desc_list);

	if (urb->transfer_flags & USB_QUEUE_BULK)
		append_qh(s, td, bqh, UHCI_PTR_DEPTH * depth_first);

	queue_urb_unlocked (s, urb);
	
	if (urb->transfer_flags & USB_QUEUE_BULK)
		set_qh_element(qh, first_td->dma_addr);
	else
		qh->hw.qh.element &= cpu_to_le32(~UHCI_PTR_TERM);    // arm QH

	if (!bulk_urb) { 					// new bulk queue	
		if (urb->transfer_flags & USB_QUEUE_BULK) {
			spin_lock (&s->td_lock);		// both QHs in one go
			insert_qh (s, s->chain_end, qh, 0);	// Main QH
			insert_qh (s, s->chain_end, nqh, 0);	// Helper QH
			spin_unlock (&s->td_lock);
		}
		else
			insert_qh (s, s->chain_end, qh, 0);
	}
	
	//uhci_show_queue(s->bulk_chain);
	//dbg("uhci_submit_bulk_urb: exit\n");
	return 0;
}
/*-------------------------------------------------------------------*/
_static void uhci_clean_iso_step1(uhci_t *s, urb_priv_t *urb_priv)
{
	struct list_head *p;
	uhci_desc_t *td;

	for (p = urb_priv->desc_list.next; p != &urb_priv->desc_list; p = p->next) {
				td = list_entry (p, uhci_desc_t, desc_list);
				unlink_td (s, td, 1);
	}
}
/*-------------------------------------------------------------------*/
_static void uhci_clean_iso_step2(uhci_t *s, urb_priv_t *urb_priv)
{
	struct list_head *p;
	uhci_desc_t *td;

	while ((p = urb_priv->desc_list.next) != &urb_priv->desc_list) {
				td = list_entry (p, uhci_desc_t, desc_list);
				list_del (p);
				delete_desc (s, td);
	}
}
/*-------------------------------------------------------------------*/
/* mode: CLEAN_TRANSFER_NO_DELETION: unlink but no deletion mark (step 1 of async_unlink)
         CLEAN_TRANSFER_REGULAR: regular (unlink/delete-mark)
         CLEAN_TRANSFER_DELETION_MARK: deletion mark for QH (step 2 of async_unlink)
 looks a bit complicated because of all the bulk queueing goodies
*/

_static void uhci_clean_transfer (uhci_t *s, struct urb *urb, uhci_desc_t *qh, int mode)
{
	uhci_desc_t *bqh, *nqh, *prevqh, *prevtd;
	int now;
	urb_priv_t *priv=(urb_priv_t*)urb->hcpriv;

	now=UHCI_GET_CURRENT_FRAME(s);

	bqh=priv->bottom_qh;	
	
	if (!priv->next_queued_urb)  { // no more appended bulk queues

		queue_dbg("uhci_clean_transfer: No more bulks for urb %p, qh %p, bqh %p, nqh %p", urb, qh, bqh, priv->next_qh);	
	
		if (priv->prev_queued_urb && mode != CLEAN_TRANSFER_DELETION_MARK) {  // qh not top of the queue
				unsigned long flags; 
				urb_priv_t* ppriv=(urb_priv_t*)priv->prev_queued_urb->hcpriv;

				spin_lock_irqsave (&s->qh_lock, flags);
				prevqh = list_entry (ppriv->desc_list.next, uhci_desc_t, desc_list);
				prevtd = list_entry (prevqh->vertical.prev, uhci_desc_t, vertical);
				set_td_link(prevtd, priv->bottom_qh->dma_addr | UHCI_PTR_QH); // skip current qh
				mb();
				queue_dbg("uhci_clean_transfer: relink pqh %p, ptd %p",prevqh, prevtd);
				spin_unlock_irqrestore (&s->qh_lock, flags);

				ppriv->bottom_qh = priv->bottom_qh;
				ppriv->next_queued_urb = NULL;
			}
		else {   // queue is dead, qh is top of the queue
			
			if (mode != CLEAN_TRANSFER_DELETION_MARK) 				
				unlink_qh(s, qh); // remove qh from horizontal chain

			if (bqh) {  // remove remainings of bulk queue
				nqh=priv->next_qh;

				if (mode != CLEAN_TRANSFER_DELETION_MARK) 
					unlink_qh(s, nqh);  // remove nqh from horizontal chain
				
				if (mode != CLEAN_TRANSFER_NO_DELETION) {  // add helper QHs to free desc list
					nqh->last_used = bqh->last_used = now;
					list_add_tail (&nqh->horizontal, &s->free_desc);
					list_add_tail (&bqh->horizontal, &s->free_desc);
				}			
			}
		}
	}
	else { // there are queued urbs following
	
	  queue_dbg("uhci_clean_transfer: urb %p, prevurb %p, nexturb %p, qh %p, bqh %p, nqh %p",
		       urb, priv->prev_queued_urb,  priv->next_queued_urb, qh, bqh, priv->next_qh);	
       	
		if (mode != CLEAN_TRANSFER_DELETION_MARK) {	// no work for cleanup at unlink-completion
			struct urb *nurb;
			unsigned long flags;

			nurb = priv->next_queued_urb;
			spin_lock_irqsave (&s->qh_lock, flags);		

			if (!priv->prev_queued_urb) { // top QH
				
				prevqh = list_entry (qh->horizontal.prev, uhci_desc_t, horizontal);
				set_qh_head(prevqh, bqh->dma_addr | UHCI_PTR_QH);
				list_del (&qh->horizontal);  // remove this qh form horizontal chain
				list_add (&bqh->horizontal, &prevqh->horizontal); // insert next bqh in horizontal chain
			}
			else {		// intermediate QH
				urb_priv_t* ppriv=(urb_priv_t*)priv->prev_queued_urb->hcpriv;
				urb_priv_t* npriv=(urb_priv_t*)nurb->hcpriv;
				uhci_desc_t * bnqh;
				
				bnqh = list_entry (npriv->desc_list.next, uhci_desc_t, desc_list);
				ppriv->bottom_qh = bnqh;
				ppriv->next_queued_urb = nurb;				
				prevqh = list_entry (ppriv->desc_list.next, uhci_desc_t, desc_list);
				set_qh_head(prevqh, bqh->dma_addr | UHCI_PTR_QH);
			}

			mb();
			((urb_priv_t*)nurb->hcpriv)->prev_queued_urb=priv->prev_queued_urb;
			spin_unlock_irqrestore (&s->qh_lock, flags);
		}		
	}

	if (mode != CLEAN_TRANSFER_NO_DELETION) {
		qh->last_used = now;	
		list_add_tail (&qh->horizontal, &s->free_desc); // mark qh for later deletion/kfree
	}
}
/*-------------------------------------------------------------------*/
// Release bandwidth for Interrupt or Isoc. transfers 
_static void uhci_release_bandwidth(struct urb *urb)
{       
	if (urb->bandwidth) {
		switch (usb_pipetype(urb->pipe)) {
		case PIPE_INTERRUPT:
			usb_release_bandwidth (urb->dev, urb, 0);
			break;
		case PIPE_ISOCHRONOUS:
			usb_release_bandwidth (urb->dev, urb, 1);
			break;
		default:
			break;
		}
	}	
}

_static void uhci_urb_dma_sync(uhci_t *s, struct urb *urb, urb_priv_t *urb_priv)
{
	if (urb_priv->setup_packet_dma)
		pci_dma_sync_single(s->uhci_pci, urb_priv->setup_packet_dma,
				    sizeof(struct usb_ctrlrequest), PCI_DMA_TODEVICE);

	if (urb_priv->transfer_buffer_dma)
		pci_dma_sync_single(s->uhci_pci, urb_priv->transfer_buffer_dma,
				    urb->transfer_buffer_length,
				    usb_pipein(urb->pipe) ?
				    PCI_DMA_FROMDEVICE :
				    PCI_DMA_TODEVICE);
}

_static void uhci_urb_dma_unmap(uhci_t *s, struct urb *urb, urb_priv_t *urb_priv)
{
	if (urb_priv->setup_packet_dma) {
		pci_unmap_single(s->uhci_pci, urb_priv->setup_packet_dma,
				 sizeof(struct usb_ctrlrequest), PCI_DMA_TODEVICE);
		urb_priv->setup_packet_dma = 0;
	}
	if (urb_priv->transfer_buffer_dma) {
		pci_unmap_single(s->uhci_pci, urb_priv->transfer_buffer_dma,
				 urb->transfer_buffer_length,
				 usb_pipein(urb->pipe) ?
				 PCI_DMA_FROMDEVICE :
				 PCI_DMA_TODEVICE);
		urb_priv->transfer_buffer_dma = 0;
	}
}
/*-------------------------------------------------------------------*/
/* needs urb_list_lock!
   mode: UNLINK_ASYNC_STORE_URB: unlink and move URB into unlinked list
         UNLINK_ASYNC_DONT_STORE: unlink, don't move URB into unlinked list
*/
_static int uhci_unlink_urb_async (uhci_t *s,struct urb *urb, int mode)
{
	uhci_desc_t *qh;
	urb_priv_t *urb_priv;
	
	async_dbg("unlink_urb_async called %p",urb);

	if ((urb->status == -EINPROGRESS) ||
	    ((usb_pipetype (urb->pipe) ==  PIPE_INTERRUPT) && ((urb_priv_t*)urb->hcpriv)->flags))
	{
		((urb_priv_t*)urb->hcpriv)->started = ~0;  // mark
		dequeue_urb (s, urb);

		if (mode==UNLINK_ASYNC_STORE_URB)
			list_add_tail (&urb->urb_list, &s->urb_unlinked); // store urb

		uhci_switch_timer_int(s);
       		s->unlink_urb_done = 1;
		uhci_release_bandwidth(urb);

		urb->status = -ECONNABORTED;	// mark urb as "waiting to be killed"	
		urb_priv = (urb_priv_t*)urb->hcpriv;

		switch (usb_pipetype (urb->pipe)) {
		case PIPE_INTERRUPT:
			usb_dotoggle (urb->dev, usb_pipeendpoint (urb->pipe), usb_pipeout (urb->pipe));

		case PIPE_ISOCHRONOUS:
			uhci_clean_iso_step1 (s, urb_priv);
			break;

		case PIPE_BULK:
		case PIPE_CONTROL:
			qh = list_entry (urb_priv->desc_list.next, uhci_desc_t, desc_list);
			uhci_clean_transfer (s, urb, qh, CLEAN_TRANSFER_NO_DELETION);
			break;
		}
		((urb_priv_t*)urb->hcpriv)->started = UHCI_GET_CURRENT_FRAME(s);
		return -EINPROGRESS;  // completion will follow
	}		

	return 0;    // URB already dead
}
/*-------------------------------------------------------------------*/
// kills an urb by unlinking descriptors and waiting for at least one frame
_static int uhci_unlink_urb_sync (uhci_t *s, struct urb *urb)
{
	uhci_desc_t *qh;
	urb_priv_t *urb_priv;
	unsigned long flags=0;
	struct usb_device *usb_dev;

	spin_lock_irqsave (&s->urb_list_lock, flags);

	if (urb->status == -EINPROGRESS) {

		// move descriptors out of the running chains, dequeue urb
		uhci_unlink_urb_async(s, urb, UNLINK_ASYNC_DONT_STORE);

		urb_priv = urb->hcpriv;
		urb->status = -ENOENT;	// prevent from double deletion after unlock		
		spin_unlock_irqrestore (&s->urb_list_lock, flags);
		
		// cleanup the rest
		switch (usb_pipetype (urb->pipe)) {

		case PIPE_INTERRUPT:
		case PIPE_ISOCHRONOUS:
			uhci_wait_ms(1);
			uhci_clean_iso_step2(s, urb_priv);
			break;

		case PIPE_BULK:
		case PIPE_CONTROL:
			qh = list_entry (urb_priv->desc_list.next, uhci_desc_t, desc_list);
			uhci_clean_transfer(s, urb, qh, CLEAN_TRANSFER_DELETION_MARK);
			uhci_wait_ms(1);
		}
		urb->status = -ENOENT;	// mark urb as killed		
					
		uhci_urb_dma_unmap(s, urb, urb->hcpriv);

#ifdef DEBUG_SLAB
		kmem_cache_free (urb_priv_kmem, urb->hcpriv);
#else
		kfree (urb->hcpriv);
#endif
		usb_dev = urb->dev;
		if (urb->complete) {
			dbg("unlink_urb: calling completion");
			urb->dev = NULL;
			urb->complete ((struct urb *) urb);
		}
		usb_dec_dev_use (usb_dev);
	}
	else
		spin_unlock_irqrestore (&s->urb_list_lock, flags);

	return 0;
}
/*-------------------------------------------------------------------*/
// async unlink_urb completion/cleanup work
// has to be protected by urb_list_lock!
// features: if set in transfer_flags, the resulting status of the killed
// transaction is not overwritten

_static void uhci_cleanup_unlink(uhci_t *s, int force)
{
	struct list_head *q;
	struct urb *urb;
	struct usb_device *dev;
	int now, type;
	urb_priv_t *urb_priv;

	q=s->urb_unlinked.next;
	now=UHCI_GET_CURRENT_FRAME(s);

	while (q != &s->urb_unlinked) {

		urb = list_entry (q, struct urb, urb_list);

		urb_priv = (urb_priv_t*)urb->hcpriv;
		q = urb->urb_list.next;
		
		if (!urb_priv) // avoid crash when URB is corrupted
			break;
			
		if (force || ((urb_priv->started != ~0) && (urb_priv->started != now))) {
			async_dbg("async cleanup %p",urb);
			type=usb_pipetype (urb->pipe);

			switch (type) { // process descriptors
			case PIPE_CONTROL:
				process_transfer (s, urb, CLEAN_TRANSFER_DELETION_MARK);  // don't unlink (already done)
				break;
			case PIPE_BULK:
				if (!s->avoid_bulk.counter)
					process_transfer (s, urb, CLEAN_TRANSFER_DELETION_MARK); // don't unlink (already done)
				else
					continue;
				break;
			case PIPE_ISOCHRONOUS:
				process_iso (s, urb, PROCESS_ISO_FORCE); // force, don't unlink
				break;
			case PIPE_INTERRUPT:
				process_interrupt (s, urb);
				break;
			}

			if (!(urb->transfer_flags & USB_TIMEOUT_KILLED))
		  		urb->status = -ECONNRESET; // mark as asynchronously killed

			dev = urb->dev;	// completion may destroy all...
			urb_priv = urb->hcpriv;
			list_del (&urb->urb_list);
			
			uhci_urb_dma_sync(s, urb, urb_priv);
			if (urb->complete) {
				spin_unlock(&s->urb_list_lock);
				urb->dev = NULL;
				urb->complete ((struct urb *) urb);
				spin_lock(&s->urb_list_lock);
			}

			if (!(urb->transfer_flags & USB_TIMEOUT_KILLED))
				urb->status = -ENOENT;  // now the urb is really dead

			switch (type) {
			case PIPE_ISOCHRONOUS:
			case PIPE_INTERRUPT:
				uhci_clean_iso_step2(s, urb_priv);
				break;
			}
	
			uhci_urb_dma_unmap(s, urb, urb_priv);

			usb_dec_dev_use (dev);
#ifdef DEBUG_SLAB
			kmem_cache_free (urb_priv_kmem, urb_priv);
#else
			kfree (urb_priv);
#endif

		}
	}
}
 
/*-------------------------------------------------------------------*/
_static int uhci_unlink_urb (struct urb *urb)
{
	uhci_t *s;
	unsigned long flags=0;
	dbg("uhci_unlink_urb called for %p",urb);
	if (!urb || !urb->dev)		// you never know...
		return -EINVAL;
	
	s = (uhci_t*) urb->dev->bus->hcpriv;

	if (usb_pipedevice (urb->pipe) == s->rh.devnum)
		return rh_unlink_urb (urb);

	if (!urb->hcpriv)
		return -EINVAL;

	if (urb->transfer_flags & USB_ASYNC_UNLINK) {
		int ret;
       		spin_lock_irqsave (&s->urb_list_lock, flags);
       		
		uhci_release_bandwidth(urb);
		ret = uhci_unlink_urb_async(s, urb, UNLINK_ASYNC_STORE_URB);

		spin_unlock_irqrestore (&s->urb_list_lock, flags);	
		return ret;
	}
	else
		return uhci_unlink_urb_sync(s, urb);
}
/*-------------------------------------------------------------------*/
// In case of ASAP iso transfer, search the URB-list for already queued URBs
// for this EP and calculate the earliest start frame for the new
// URB (easy seamless URB continuation!)
_static int find_iso_limits (struct urb *urb, unsigned int *start, unsigned int *end)
{
	struct urb *u, *last_urb = NULL;
	uhci_t *s = (uhci_t*) urb->dev->bus->hcpriv;
	struct list_head *p;
	int ret=-1;
	unsigned long flags;
	
	spin_lock_irqsave (&s->urb_list_lock, flags);
	p=s->urb_list.prev;

	for (; p != &s->urb_list; p = p->prev) {
		u = list_entry (p, struct urb, urb_list);
		// look for pending URBs with identical pipe handle
		// works only because iso doesn't toggle the data bit!
		if ((urb->pipe == u->pipe) && (urb->dev == u->dev) && (u->status == -EINPROGRESS)) {
			if (!last_urb)
				*start = u->start_frame;
			last_urb = u;
		}
	}
	
	if (last_urb) {
		*end = (last_urb->start_frame + last_urb->number_of_packets) & 1023;
		ret=0;
	}
	
	spin_unlock_irqrestore(&s->urb_list_lock, flags);
	
	return ret;
}
/*-------------------------------------------------------------------*/
// adjust start_frame according to scheduling constraints (ASAP etc)

_static int iso_find_start (struct urb *urb)
{
	uhci_t *s = (uhci_t*) urb->dev->bus->hcpriv;
	unsigned int now;
	unsigned int start_limit = 0, stop_limit = 0, queued_size;
	int limits;

	now = UHCI_GET_CURRENT_FRAME (s) & 1023;

	if ((unsigned) urb->number_of_packets > 900)
		return -EFBIG;
	
	limits = find_iso_limits (urb, &start_limit, &stop_limit);
	queued_size = (stop_limit - start_limit) & 1023;

	if (urb->transfer_flags & USB_ISO_ASAP) {
		// first iso
		if (limits) {
			// 10ms setup should be enough //FIXME!
			urb->start_frame = (now + 10) & 1023;
		}
		else {
			urb->start_frame = stop_limit;		//seamless linkage

			if (((now - urb->start_frame) & 1023) <= (unsigned) urb->number_of_packets) {
				info("iso_find_start: gap in seamless isochronous scheduling");
				dbg("iso_find_start: now %u start_frame %u number_of_packets %u pipe 0x%08x",
					now, urb->start_frame, urb->number_of_packets, urb->pipe);
				urb->start_frame = (now + 5) & 1023;	// 5ms setup should be enough //FIXME!
			}
		}
	}
	else {
		urb->start_frame &= 1023;
		if (((now - urb->start_frame) & 1023) < (unsigned) urb->number_of_packets) {
			dbg("iso_find_start: now between start_frame and end");
			return -EAGAIN;
		}
	}

	/* check if either start_frame or start_frame+number_of_packets-1 lies between start_limit and stop_limit */
	if (limits)
		return 0;

	if (((urb->start_frame - start_limit) & 1023) < queued_size ||
	    ((urb->start_frame + urb->number_of_packets - 1 - start_limit) & 1023) < queued_size) {
		dbg("iso_find_start: start_frame %u number_of_packets %u start_limit %u stop_limit %u",
			urb->start_frame, urb->number_of_packets, start_limit, stop_limit);
		return -EAGAIN;
	}

	return 0;
}
/*-------------------------------------------------------------------*/
// submits USB interrupt (ie. polling ;-) 
// ASAP-flag set implicitely
// if period==0, the transfer is only done once

_static int uhci_submit_int_urb (struct urb *urb)
{
	uhci_t *s = (uhci_t*) urb->dev->bus->hcpriv;
	urb_priv_t *urb_priv = urb->hcpriv;
	int nint, n;
	uhci_desc_t *td;
	int status, destination;
	int info;
	unsigned int pipe = urb->pipe;

	if (urb->interval < 0 || urb->interval >= 256)
		return -EINVAL;

	if (urb->interval == 0)
		nint = 0;
	else {
		for (nint = 0, n = 1; nint <= 8; nint++, n += n)	// round interval down to 2^n
		 {
			if (urb->interval < n) {
				urb->interval = n / 2;
				break;
			}
		}
		nint--;
	}

	dbg("Rounded interval to %i, chain  %i", urb->interval, nint);

	urb->start_frame = UHCI_GET_CURRENT_FRAME (s) & 1023;	// remember start frame, just in case...

	urb->number_of_packets = 1;

	// INT allows only one packet
	if (urb->transfer_buffer_length > usb_maxpacket (urb->dev, pipe, usb_pipeout (pipe)))
		return -EINVAL;

	if (alloc_td (s, &td, UHCI_PTR_DEPTH))
		return -ENOMEM;

	status = (pipe & TD_CTRL_LS) | TD_CTRL_ACTIVE | TD_CTRL_IOC |
		(urb->transfer_flags & USB_DISABLE_SPD ? 0 : TD_CTRL_SPD) | (3 << 27);

	destination = (urb->pipe & PIPE_DEVEP_MASK) | usb_packetid (urb->pipe) |
		(((urb->transfer_buffer_length - 1) & 0x7ff) << 21);


	info = destination | (usb_gettoggle (urb->dev, usb_pipeendpoint (pipe), usb_pipeout (pipe)) << TD_TOKEN_TOGGLE);

	fill_td (td, status, info, urb_priv->transfer_buffer_dma);
	list_add_tail (&td->desc_list, &urb_priv->desc_list);

	queue_urb (s, urb);

	insert_td_horizontal (s, s->int_chain[nint], td);	// store in INT-TDs

	usb_dotoggle (urb->dev, usb_pipeendpoint (pipe), usb_pipeout (pipe));

	return 0;
}
/*-------------------------------------------------------------------*/
_static int uhci_submit_iso_urb (struct urb *urb)
{
	uhci_t *s = (uhci_t*) urb->dev->bus->hcpriv;
	urb_priv_t *urb_priv = urb->hcpriv;
#ifdef ISO_SANITY_CHECK
	int pipe=urb->pipe;
	int maxsze = usb_maxpacket (urb->dev, pipe, usb_pipeout (pipe));
#endif
	int n, ret, last=0;
	uhci_desc_t *td, **tdm;
	int status, destination;
	unsigned long flags;

	__save_flags(flags);
	__cli();		      // Disable IRQs to schedule all ISO-TDs in time
	ret = iso_find_start (urb);	// adjusts urb->start_frame for later use
	
	if (ret)
		goto err;

	tdm = (uhci_desc_t **) kmalloc (urb->number_of_packets * sizeof (uhci_desc_t*), KMALLOC_FLAG);

	if (!tdm) {
		ret = -ENOMEM;
		goto err;
	}

	memset(tdm, 0, urb->number_of_packets * sizeof (uhci_desc_t*));

	// First try to get all TDs. Cause: Removing already inserted TDs can only be done 
	// racefree in three steps: unlink TDs, wait one frame, delete TDs. 
	// So, this solutions seems simpler...

	for (n = 0; n < urb->number_of_packets; n++) {
		dbg("n:%d urb->iso_frame_desc[n].length:%d", n, urb->iso_frame_desc[n].length);
		if (!urb->iso_frame_desc[n].length)
			continue;  // allows ISO striping by setting length to zero in iso_descriptor


#ifdef ISO_SANITY_CHECK
		if(urb->iso_frame_desc[n].length > maxsze) {

			err("submit_iso: urb->iso_frame_desc[%d].length(%d)>%d",n , urb->iso_frame_desc[n].length, maxsze);
			ret=-EINVAL;		
		}
		else
#endif
		if (alloc_td (s, &td, UHCI_PTR_DEPTH)) {
			int i;	// Cleanup allocated TDs

			for (i = 0; i < n; n++)
				if (tdm[i])
					 delete_desc(s, tdm[i]);
			kfree (tdm);
			goto err;
		}
		last=n;
		tdm[n] = td;
	}

	status = TD_CTRL_ACTIVE | TD_CTRL_IOS;

	destination = (urb->pipe & PIPE_DEVEP_MASK) | usb_packetid (urb->pipe);

	// Queue all allocated TDs
	for (n = 0; n < urb->number_of_packets; n++) {
		td = tdm[n];
		if (!td)
			continue;
			
		if (n  == last) {
			status |= TD_CTRL_IOC;
			queue_urb (s, urb);
		}

		fill_td (td, status, destination | (((urb->iso_frame_desc[n].length - 1) & 0x7ff) << 21),
			 urb_priv->transfer_buffer_dma + urb->iso_frame_desc[n].offset);
		list_add_tail (&td->desc_list, &urb_priv->desc_list);
	
		insert_td_horizontal (s, s->iso_td[(urb->start_frame + n) & 1023], td);	// store in iso-tds
	}

	kfree (tdm);
	dbg("ISO-INT# %i, start %i, now %i", urb->number_of_packets, urb->start_frame, UHCI_GET_CURRENT_FRAME (s) & 1023);
	ret = 0;

      err:
	__restore_flags(flags);
	return ret;
}
/*-------------------------------------------------------------------*/
// returns: 0 (no transfer queued), urb* (this urb already queued)
 
_static struct urb* search_dev_ep (uhci_t *s, struct urb *urb)
{
	struct list_head *p;
	struct urb *tmp;
	unsigned int mask = usb_pipecontrol(urb->pipe) ? (~USB_DIR_IN) : (~0);

	dbg("search_dev_ep:");

	p=s->urb_list.next;

	for (; p != &s->urb_list; p = p->next) {
		tmp = list_entry (p, struct urb, urb_list);
		dbg("urb: %p", tmp);
		// we can accept this urb if it is not queued at this time 
		// or if non-iso transfer requests should be scheduled for the same device and pipe
		if ((!usb_pipeisoc(urb->pipe) && (tmp->dev == urb->dev) && !((tmp->pipe ^ urb->pipe) & mask)) ||
		    (urb == tmp)) {
			return tmp;	// found another urb already queued for processing
		}
	}

	return 0;
}
/*-------------------------------------------------------------------*/
_static int uhci_submit_urb (struct urb *urb)
{
	uhci_t *s;
	urb_priv_t *urb_priv;
	int ret = 0, type;
	unsigned long flags;
	struct urb *queued_urb=NULL;
	int bustime;
		
	if (!urb->dev || !urb->dev->bus)
		return -ENODEV;

	s = (uhci_t*) urb->dev->bus->hcpriv;
	//dbg("submit_urb: %p type %d",urb,usb_pipetype(urb->pipe));
	
	if (!s->running)
		return -ENODEV;
	
	type = usb_pipetype (urb->pipe);

	if (usb_pipedevice (urb->pipe) == s->rh.devnum)
		return rh_submit_urb (urb);	/* virtual root hub */

	// Sanity checks
	if (usb_maxpacket (urb->dev, urb->pipe, usb_pipeout (urb->pipe)) <= 0) {		
		err("uhci_submit_urb: pipesize for pipe %x is zero", urb->pipe);
		return -EMSGSIZE;
	}

	if (urb->transfer_buffer_length < 0 && type != PIPE_ISOCHRONOUS) {
		err("uhci_submit_urb: Negative transfer length for urb %p", urb);
		return -EINVAL;
	}

	usb_inc_dev_use (urb->dev);

	spin_lock_irqsave (&s->urb_list_lock, flags);

	queued_urb = search_dev_ep (s, urb); // returns already queued urb for that pipe

	if (queued_urb) {

		queue_dbg("found bulk urb %p\n", queued_urb);

		if (( type != PIPE_BULK) ||
		    ((type == PIPE_BULK) &&
		     (!(urb->transfer_flags & USB_QUEUE_BULK) || !(queued_urb->transfer_flags & USB_QUEUE_BULK)))) {
			spin_unlock_irqrestore (&s->urb_list_lock, flags);
			usb_dec_dev_use (urb->dev);
			err("ENXIO %08x, flags %x, urb %p, burb %p",urb->pipe,urb->transfer_flags,urb,queued_urb);
			return -ENXIO;	// urb already queued
		}
	}

#ifdef DEBUG_SLAB
	urb_priv = kmem_cache_alloc(urb_priv_kmem, SLAB_FLAG);
#else
	urb_priv = kmalloc (sizeof (urb_priv_t), KMALLOC_FLAG);
#endif
	if (!urb_priv) {
		usb_dec_dev_use (urb->dev);
		spin_unlock_irqrestore (&s->urb_list_lock, flags);
		return -ENOMEM;
	}

	memset(urb_priv, 0, sizeof(urb_priv_t));
	urb->hcpriv = urb_priv;
	INIT_LIST_HEAD (&urb_priv->desc_list);

	dbg("submit_urb: scheduling %p", urb);
	
	if (type == PIPE_CONTROL)
		urb_priv->setup_packet_dma = pci_map_single(s->uhci_pci, urb->setup_packet,
							    sizeof(struct usb_ctrlrequest), PCI_DMA_TODEVICE);

	if (urb->transfer_buffer_length)
		urb_priv->transfer_buffer_dma = pci_map_single(s->uhci_pci,
							       urb->transfer_buffer,
							       urb->transfer_buffer_length,
							       usb_pipein(urb->pipe) ?
							       PCI_DMA_FROMDEVICE :
							       PCI_DMA_TODEVICE);

	if (type == PIPE_BULK) {
	
		if (queued_urb) {
			while (((urb_priv_t*)queued_urb->hcpriv)->next_queued_urb)  // find last queued bulk
				queued_urb=((urb_priv_t*)queued_urb->hcpriv)->next_queued_urb;
			
			((urb_priv_t*)queued_urb->hcpriv)->next_queued_urb=urb;
		}
		atomic_inc (&s->avoid_bulk);
		ret = uhci_submit_bulk_urb (urb, queued_urb);
		atomic_dec (&s->avoid_bulk);
		spin_unlock_irqrestore (&s->urb_list_lock, flags);
	}
	else {
		spin_unlock_irqrestore (&s->urb_list_lock, flags);
		switch (type) {
		case PIPE_ISOCHRONOUS:			
			if (urb->bandwidth == 0) {      /* not yet checked/allocated */
				if (urb->number_of_packets <= 0) {
					ret = -EINVAL;
					break;
				}

				bustime = usb_check_bandwidth (urb->dev, urb);
				if (bustime < 0) 
					ret = bustime;
				else {
					ret = uhci_submit_iso_urb(urb);
					if (ret == 0)
						usb_claim_bandwidth (urb->dev, urb, bustime, 1);
				}
			} else {        /* bandwidth is already set */
				ret = uhci_submit_iso_urb(urb);
			}
			break;
		case PIPE_INTERRUPT:
			if (urb->bandwidth == 0) {      /* not yet checked/allocated */
				bustime = usb_check_bandwidth (urb->dev, urb);
				if (bustime < 0)
					ret = bustime;
				else {
					ret = uhci_submit_int_urb(urb);
					if (ret == 0)
						usb_claim_bandwidth (urb->dev, urb, bustime, 0);
				}
			} else {        /* bandwidth is already set */
				ret = uhci_submit_int_urb(urb);
			}
			break;
		case PIPE_CONTROL:
			ret = uhci_submit_control_urb (urb);
			break;
		default:
			ret = -EINVAL;
		}
	}

	dbg("submit_urb: scheduled with ret: %d", ret);
	
	if (ret != 0) {
		uhci_urb_dma_unmap(s, urb, urb_priv);
		usb_dec_dev_use (urb->dev);
#ifdef DEBUG_SLAB
		kmem_cache_free(urb_priv_kmem, urb_priv);
#else
		kfree (urb_priv);
#endif
		return ret;
	}

	return 0;
}

// Checks for URB timeout and removes bandwidth reclamation if URB idles too long
_static void uhci_check_timeouts(uhci_t *s)
{
	struct list_head *p,*p2;
	struct urb *urb;
	int type;	

	p = s->urb_list.prev;	

	while (p != &s->urb_list) {
		urb_priv_t *hcpriv;

		p2 = p;
		p = p->prev;
		urb = list_entry (p2, struct urb, urb_list);
		type = usb_pipetype (urb->pipe);

		hcpriv = (urb_priv_t*)urb->hcpriv;
		
		if ( urb->timeout && time_after(jiffies, hcpriv->started + urb->timeout)) {
			urb->transfer_flags |= USB_TIMEOUT_KILLED | USB_ASYNC_UNLINK;
			async_dbg("uhci_check_timeout: timeout for %p",urb);
			uhci_unlink_urb_async(s, urb, UNLINK_ASYNC_STORE_URB);
		}
#ifdef CONFIG_USB_UHCI_HIGH_BANDWIDTH
		else if (((type == PIPE_BULK) || (type == PIPE_CONTROL)) &&  
			 (hcpriv->use_loop) && time_after(jiffies, hcpriv->started + IDLE_TIMEOUT))
			disable_desc_loop(s, urb);
#endif

	}
	s->timeout_check=jiffies;
}

/*-------------------------------------------------------------------
 Virtual Root Hub
 -------------------------------------------------------------------*/

_static __u8 root_hub_dev_des[] =
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
_static __u8 root_hub_config_des[] =
{
	0x09,			/*  __u8  bLength; */
	0x02,			/*  __u8  bDescriptorType; Configuration */
	0x19,			/*  __u16 wTotalLength; */
	0x00,
	0x01,			/*  __u8  bNumInterfaces; */
	0x01,			/*  __u8  bConfigurationValue; */
	0x00,			/*  __u8  iConfiguration; */
	0x40,			/*  __u8  bmAttributes; 
				   Bit 7: Bus-powered, 6: Self-powered, 5 Remote-wakwup, 4..0: resvd */
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


_static __u8 root_hub_hub_des[] =
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

/*-------------------------------------------------------------------------*/
/* prepare Interrupt pipe transaction data; HUB INTERRUPT ENDPOINT */
_static int rh_send_irq (struct urb *urb)
{
	int len = 1;
	int i;
	uhci_t *uhci = urb->dev->bus->hcpriv;
	unsigned int io_addr = uhci->io_addr;
	__u16 data = 0;

	for (i = 0; i < uhci->rh.numports; i++) {
		data |= ((inw (io_addr + USBPORTSC1 + i * 2) & 0xa) > 0 ? (1 << (i + 1)) : 0);
		len = (i + 1) / 8 + 1;
	}

	*(__u16 *) urb->transfer_buffer = cpu_to_le16 (data);
	urb->actual_length = len;
	urb->status = 0;
	
	if ((data > 0) && (uhci->rh.send != 0)) {
		dbg("Root-Hub INT complete: port1: %x port2: %x data: %x",
		     inw (io_addr + USBPORTSC1), inw (io_addr + USBPORTSC2), data);
		urb->complete (urb);
	}
	return 0;
}

/*-------------------------------------------------------------------------*/
/* Virtual Root Hub INTs are polled by this timer every "intervall" ms */
_static int rh_init_int_timer (struct urb *urb);

_static void rh_int_timer_do (unsigned long ptr)
{
	int len;
	struct urb *urb = (struct urb*) ptr;
	uhci_t *uhci = urb->dev->bus->hcpriv;

	if (uhci->rh.send) {
		len = rh_send_irq (urb);
		if (len > 0) {
			urb->actual_length = len;
			if (urb->complete)
				urb->complete (urb);
		}
	}
	rh_init_int_timer (urb);
}

/*-------------------------------------------------------------------------*/
/* Root Hub INTs are polled by this timer, polling interval 20ms */

_static int rh_init_int_timer (struct urb *urb)
{
	uhci_t *uhci = urb->dev->bus->hcpriv;

	uhci->rh.interval = urb->interval;
	init_timer (&uhci->rh.rh_int_timer);
	uhci->rh.rh_int_timer.function = rh_int_timer_do;
	uhci->rh.rh_int_timer.data = (unsigned long) urb;
	uhci->rh.rh_int_timer.expires = jiffies + (HZ * 20) / 1000;
	add_timer (&uhci->rh.rh_int_timer);

	return 0;
}

/*-------------------------------------------------------------------------*/
#define OK(x) 			len = (x); break

#define CLR_RH_PORTSTAT(x) \
		status = inw(io_addr+USBPORTSC1+2*(wIndex-1)); \
		status = (status & 0xfff5) & ~(x); \
		outw(status, io_addr+USBPORTSC1+2*(wIndex-1))

#define SET_RH_PORTSTAT(x) \
		status = inw(io_addr+USBPORTSC1+2*(wIndex-1)); \
		status = (status & 0xfff5) | (x); \
		outw(status, io_addr+USBPORTSC1+2*(wIndex-1))


/*-------------------------------------------------------------------------*/
/****
 ** Root Hub Control Pipe
 *************************/


_static int rh_submit_urb (struct urb *urb)
{
	struct usb_device *usb_dev = urb->dev;
	uhci_t *uhci = usb_dev->bus->hcpriv;
	unsigned int pipe = urb->pipe;
	struct usb_ctrlrequest *cmd = (struct usb_ctrlrequest *) urb->setup_packet;
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

	if (usb_pipetype (pipe) == PIPE_INTERRUPT) {
		dbg("Root-Hub submit IRQ: every %d ms", urb->interval);
		uhci->rh.urb = urb;
		uhci->rh.send = 1;
		uhci->rh.interval = urb->interval;
		rh_init_int_timer (urb);

		return 0;
	}


	bmRType_bReq = cmd->bRequestType | cmd->bRequest << 8;
	wValue = le16_to_cpu (cmd->wValue);
	wIndex = le16_to_cpu (cmd->wIndex);
	wLength = le16_to_cpu (cmd->wLength);

	for (i = 0; i < 8; i++)
		uhci->rh.c_p_r[i] = 0;

	dbg("Root-Hub: adr: %2x cmd(%1x): %04x %04x %04x %04x",
	     uhci->rh.devnum, 8, bmRType_bReq, wValue, wIndex, wLength);

	switch (bmRType_bReq) {
		/* Request Destination:
		   without flags: Device, 
		   RH_INTERFACE: interface, 
		   RH_ENDPOINT: endpoint,
		   RH_CLASS means HUB here, 
		   RH_OTHER | RH_CLASS  almost ever means HUB_PORT here 
		 */

	case RH_GET_STATUS:
		*(__u16 *) data = cpu_to_le16 (1);
		OK (2);
	case RH_GET_STATUS | RH_INTERFACE:
		*(__u16 *) data = cpu_to_le16 (0);
		OK (2);
	case RH_GET_STATUS | RH_ENDPOINT:
		*(__u16 *) data = cpu_to_le16 (0);
		OK (2);
	case RH_GET_STATUS | RH_CLASS:
		*(__u32 *) data = cpu_to_le32 (0);
		OK (4);		/* hub power ** */
	case RH_GET_STATUS | RH_OTHER | RH_CLASS:
		status = inw (io_addr + USBPORTSC1 + 2 * (wIndex - 1));
		cstatus = ((status & USBPORTSC_CSC) >> (1 - 0)) |
			((status & USBPORTSC_PEC) >> (3 - 1)) |
			(uhci->rh.c_p_r[wIndex - 1] << (0 + 4));
		status = (status & USBPORTSC_CCS) |
			((status & USBPORTSC_PE) >> (2 - 1)) |
			((status & USBPORTSC_SUSP) >> (12 - 2)) |
			((status & USBPORTSC_PR) >> (9 - 4)) |
			(1 << 8) |	/* power on ** */
			((status & USBPORTSC_LSDA) << (-8 + 9));

		*(__u16 *) data = cpu_to_le16 (status);
		*(__u16 *) (data + 2) = cpu_to_le16 (cstatus);
		OK (4);

	case RH_CLEAR_FEATURE | RH_ENDPOINT:
		switch (wValue) {
		case (RH_ENDPOINT_STALL):
			OK (0);
		}
		break;

	case RH_CLEAR_FEATURE | RH_CLASS:
		switch (wValue) {
		case (RH_C_HUB_OVER_CURRENT):
			OK (0);	/* hub power over current ** */
		}
		break;

	case RH_CLEAR_FEATURE | RH_OTHER | RH_CLASS:
		switch (wValue) {
		case (RH_PORT_ENABLE):
			CLR_RH_PORTSTAT (USBPORTSC_PE);
			OK (0);
		case (RH_PORT_SUSPEND):
			CLR_RH_PORTSTAT (USBPORTSC_SUSP);
			OK (0);
		case (RH_PORT_POWER):
			OK (0);	/* port power ** */
		case (RH_C_PORT_CONNECTION):
			SET_RH_PORTSTAT (USBPORTSC_CSC);
			OK (0);
		case (RH_C_PORT_ENABLE):
			SET_RH_PORTSTAT (USBPORTSC_PEC);
			OK (0);
		case (RH_C_PORT_SUSPEND):
/*** WR_RH_PORTSTAT(RH_PS_PSSC); */
			OK (0);
		case (RH_C_PORT_OVER_CURRENT):
			OK (0);	/* port power over current ** */
		case (RH_C_PORT_RESET):
			uhci->rh.c_p_r[wIndex - 1] = 0;
			OK (0);
		}
		break;

	case RH_SET_FEATURE | RH_OTHER | RH_CLASS:
		switch (wValue) {
		case (RH_PORT_SUSPEND):
			SET_RH_PORTSTAT (USBPORTSC_SUSP);
			OK (0);
		case (RH_PORT_RESET):
			SET_RH_PORTSTAT (USBPORTSC_PR);
			uhci_wait_ms (10);
			uhci->rh.c_p_r[wIndex - 1] = 1;
			CLR_RH_PORTSTAT (USBPORTSC_PR);
			udelay (10);
			SET_RH_PORTSTAT (USBPORTSC_PE);
			uhci_wait_ms (10);
			SET_RH_PORTSTAT (0xa);
			OK (0);
		case (RH_PORT_POWER):
			OK (0);	/* port power ** */
		case (RH_PORT_ENABLE):
			SET_RH_PORTSTAT (USBPORTSC_PE);
			OK (0);
		}
		break;

	case RH_SET_ADDRESS:
		uhci->rh.devnum = wValue;
		OK (0);

	case RH_GET_DESCRIPTOR:
		switch ((wValue & 0xff00) >> 8) {
		case (0x01):	/* device descriptor */
			len = min_t(unsigned int, leni,
				  min_t(unsigned int,
				      sizeof (root_hub_dev_des), wLength));
			memcpy (data, root_hub_dev_des, len);
			OK (len);
		case (0x02):	/* configuration descriptor */
			len = min_t(unsigned int, leni,
				  min_t(unsigned int,
				      sizeof (root_hub_config_des), wLength));
			memcpy (data, root_hub_config_des, len);
			OK (len);
		case (0x03):	/* string descriptors */
			len = usb_root_hub_string (wValue & 0xff,
			        uhci->io_addr, "UHCI",
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
			  min_t(unsigned int, sizeof (root_hub_hub_des), wLength));
		memcpy (data, root_hub_hub_des, len);
		OK (len);

	case RH_GET_CONFIGURATION:
		*(__u8 *) data = 0x01;
		OK (1);

	case RH_SET_CONFIGURATION:
		OK (0);
	default:
		stat = -EPIPE;
	}

	dbg("Root-Hub stat port1: %x port2: %x",
	     inw (io_addr + USBPORTSC1), inw (io_addr + USBPORTSC2));

	urb->actual_length = len;
	urb->status = stat;
	urb->dev=NULL;
	if (urb->complete)
		urb->complete (urb);
	return 0;
}
/*-------------------------------------------------------------------------*/

_static int rh_unlink_urb (struct urb *urb)
{
	uhci_t *uhci = urb->dev->bus->hcpriv;

	if (uhci->rh.urb==urb) {
		dbg("Root-Hub unlink IRQ");
		uhci->rh.send = 0;
		del_timer (&uhci->rh.rh_int_timer);
	}
	return 0;
}
/*-------------------------------------------------------------------*/

/*
 * Map status to standard result codes
 *
 * <status> is (td->status & 0xFE0000) [a.k.a. uhci_status_bits(td->status)
 * <dir_out> is True for output TDs and False for input TDs.
 */
_static int uhci_map_status (int status, int dir_out)
{
	if (!status)
		return 0;
	if (status & TD_CTRL_BITSTUFF)	/* Bitstuff error */
		return -EPROTO;
	if (status & TD_CTRL_CRCTIMEO) {	/* CRC/Timeout */
		if (dir_out)
			return -ETIMEDOUT;
		else
			return -EILSEQ;
	}
	if (status & TD_CTRL_NAK)	/* NAK */
		return -ETIMEDOUT;
	if (status & TD_CTRL_BABBLE)	/* Babble */
		return -EOVERFLOW;
	if (status & TD_CTRL_DBUFERR)	/* Buffer error */
		return -ENOSR;
	if (status & TD_CTRL_STALLED)	/* Stalled */
		return -EPIPE;
	if (status & TD_CTRL_ACTIVE)	/* Active */
		return 0;

	return -EPROTO;
}

/*
 * Only the USB core should call uhci_alloc_dev and uhci_free_dev
 */
_static int uhci_alloc_dev (struct usb_device *usb_dev)
{
	return 0;
}

_static void uhci_unlink_urbs(uhci_t *s, struct usb_device *usb_dev, int remove_all)
{
	unsigned long flags;
	struct list_head *p;
	struct list_head *p2;
	struct urb *urb;

	spin_lock_irqsave (&s->urb_list_lock, flags);
	p = s->urb_list.prev;	
	while (p != &s->urb_list) {
		p2 = p;
		p = p->prev ;
		urb = list_entry (p2, struct urb, urb_list);
		dbg("urb: %p, dev %p, %p", urb, usb_dev,urb->dev);
		
		//urb->transfer_flags |=USB_ASYNC_UNLINK; 
			
		if (remove_all || (usb_dev == urb->dev)) {
			spin_unlock_irqrestore (&s->urb_list_lock, flags);
			warn("forced removing of queued URB %p due to disconnect",urb);
			uhci_unlink_urb(urb);
			urb->dev = NULL; // avoid further processing of this URB
			spin_lock_irqsave (&s->urb_list_lock, flags);
			p = s->urb_list.prev;	
		}
	}
	spin_unlock_irqrestore (&s->urb_list_lock, flags);
}

_static int uhci_free_dev (struct usb_device *usb_dev)
{
	uhci_t *s;
	

	if(!usb_dev || !usb_dev->bus || !usb_dev->bus->hcpriv)
		return -EINVAL;
	
	s=(uhci_t*) usb_dev->bus->hcpriv;	
	uhci_unlink_urbs(s, usb_dev, 0);

	return 0;
}

/*
 * uhci_get_current_frame_number()
 *
 * returns the current frame number for a USB bus/controller.
 */
_static int uhci_get_current_frame_number (struct usb_device *usb_dev)
{
	return UHCI_GET_CURRENT_FRAME ((uhci_t*) usb_dev->bus->hcpriv);
}

struct usb_operations uhci_device_operations =
{
	uhci_alloc_dev,
	uhci_free_dev,
	uhci_get_current_frame_number,
	uhci_submit_urb,
	uhci_unlink_urb
};

_static void correct_data_toggles(struct urb *urb)
{
	usb_settoggle (urb->dev, usb_pipeendpoint (urb->pipe), usb_pipeout (urb->pipe), 
		       !usb_gettoggle (urb->dev, usb_pipeendpoint (urb->pipe), usb_pipeout (urb->pipe)));

	while(urb) {
		urb_priv_t *priv=urb->hcpriv;		
		uhci_desc_t *qh = list_entry (priv->desc_list.next, uhci_desc_t, desc_list);
		struct list_head *p = qh->vertical.next;
		uhci_desc_t *td;
		dbg("URB to correct %p\n", urb);
	
		for (; p != &qh->vertical; p = p->next) {
			td = list_entry (p, uhci_desc_t, vertical);
			td->hw.td.info^=cpu_to_le32(1<<TD_TOKEN_TOGGLE);
		}
		urb=priv->next_queued_urb;
	}
}

/* 
 * For IN-control transfers, process_transfer gets a bit more complicated,
 * since there are devices that return less data (eg. strings) than they
 * have announced. This leads to a queue abort due to the short packet,
 * the status stage is not executed. If this happens, the status stage
 * is manually re-executed.
 * mode: PROCESS_TRANSFER_REGULAR: regular (unlink QH)
 *       PROCESS_TRANSFER_DONT_UNLINK: QHs already unlinked (for async unlink_urb)
 */

_static int process_transfer (uhci_t *s, struct urb *urb, int mode)
{
	int ret = 0;
	urb_priv_t *urb_priv = urb->hcpriv;
	struct list_head *qhl = urb_priv->desc_list.next;
	uhci_desc_t *qh = list_entry (qhl, uhci_desc_t, desc_list);
	struct list_head *p = qh->vertical.next;
	uhci_desc_t *desc= list_entry (urb_priv->desc_list.prev, uhci_desc_t, desc_list);
	uhci_desc_t *last_desc = list_entry (desc->vertical.prev, uhci_desc_t, vertical);
	int data_toggle = usb_gettoggle (urb->dev, usb_pipeendpoint (urb->pipe), usb_pipeout (urb->pipe));	// save initial data_toggle
	int maxlength; 	// extracted and remapped info from TD
	int actual_length;
	int status = 0;

	//dbg("process_transfer: urb %p, urb_priv %p, qh %p last_desc %p\n",urb,urb_priv, qh, last_desc);

	/* if the status phase has been retriggered and the
	   queue is empty or the last status-TD is inactive, the retriggered
	   status stage is completed
	 */

	if (urb_priv->flags && 
	    ((qh->hw.qh.element == cpu_to_le32(UHCI_PTR_TERM)) || !is_td_active(desc)))
		goto transfer_finished;

	urb->actual_length=0;

	for (; p != &qh->vertical; p = p->next) {
		desc = list_entry (p, uhci_desc_t, vertical);

		if (is_td_active(desc)) {	// do not process active TDs
			if (mode == CLEAN_TRANSFER_DELETION_MARK) // if called from async_unlink
				uhci_clean_transfer(s, urb, qh, CLEAN_TRANSFER_DELETION_MARK);
			return ret;
		}
	
		actual_length = uhci_actual_length(le32_to_cpu(desc->hw.td.status));		// extract transfer parameters from TD
		maxlength = (((le32_to_cpu(desc->hw.td.info) >> 21) & 0x7ff) + 1) & 0x7ff;
		status = uhci_map_status (uhci_status_bits (le32_to_cpu(desc->hw.td.status)), usb_pipeout (urb->pipe));

		if (status == -EPIPE) { 		// see if EP is stalled
			// set up stalled condition
			usb_endpoint_halt (urb->dev, usb_pipeendpoint (urb->pipe), usb_pipeout (urb->pipe));
		}

		if (status && (status != -EPIPE)) {	// if any error occurred stop processing of further TDs
			// only set ret if status returned an error
  is_error:
			ret = status;
			urb->error_count++;
			break;
		}
		else if ((le32_to_cpu(desc->hw.td.info) & 0xff) != USB_PID_SETUP)
			urb->actual_length += actual_length;

		// got less data than requested
		if ( (actual_length < maxlength)) {
			if (urb->transfer_flags & USB_DISABLE_SPD) {
				status = -EREMOTEIO;	// treat as real error
				dbg("process_transfer: SPD!!");
				break;	// exit after this TD because SP was detected
			}

			// short read during control-IN: re-start status stage
			if ((usb_pipetype (urb->pipe) == PIPE_CONTROL)) {
				if (uhci_packetid(le32_to_cpu(last_desc->hw.td.info)) == USB_PID_OUT) {
			
					set_qh_element(qh, last_desc->dma_addr);  // re-trigger status stage
					dbg("short packet during control transfer, retrigger status stage @ %p",last_desc);
					urb_priv->flags = 1; // mark as short control packet
					return 0;
				}
			}
			// all other cases: short read is OK
			data_toggle = uhci_toggle (le32_to_cpu(desc->hw.td.info));
			break;
		}
		else if (status)
			goto is_error;

		data_toggle = uhci_toggle (le32_to_cpu(desc->hw.td.info));
		queue_dbg("process_transfer: len:%d status:%x mapped:%x toggle:%d", actual_length, le32_to_cpu(desc->hw.td.status),status, data_toggle);      

	}

	if (usb_pipetype (urb->pipe) == PIPE_BULK ) {  /* toggle correction for short bulk transfers (nonqueued/queued) */

		urb_priv_t *priv=(urb_priv_t*)urb->hcpriv;
		struct urb *next_queued_urb=priv->next_queued_urb;

		if (next_queued_urb) {
			urb_priv_t *next_priv=(urb_priv_t*)next_queued_urb->hcpriv;
			uhci_desc_t *qh = list_entry (next_priv->desc_list.next, uhci_desc_t, desc_list);
			uhci_desc_t *first_td=list_entry (qh->vertical.next, uhci_desc_t, vertical);

			if (data_toggle == uhci_toggle (le32_to_cpu(first_td->hw.td.info))) {
				err("process_transfer: fixed toggle");
				correct_data_toggles(next_queued_urb);
			}						
		}
		else
			usb_settoggle (urb->dev, usb_pipeendpoint (urb->pipe), usb_pipeout (urb->pipe), !data_toggle);		
	}

 transfer_finished:
	
	uhci_clean_transfer(s, urb, qh, mode);

	urb->status = status;

#ifdef CONFIG_USB_UHCI_HIGH_BANDWIDTH	
	disable_desc_loop(s,urb);
#endif	

	queue_dbg("process_transfer: (end) urb %p, wanted len %d, len %d status %x err %d",
		urb,urb->transfer_buffer_length,urb->actual_length, urb->status, urb->error_count);
	return ret;
}

_static int process_interrupt (uhci_t *s, struct urb *urb)
{
	int ret = -EINPROGRESS;
	urb_priv_t *urb_priv = urb->hcpriv;
	struct list_head *p;
	uhci_desc_t *desc = list_entry (urb_priv->desc_list.prev, uhci_desc_t, desc_list);

	int actual_length;
	int status = 0;

	//dbg("urb contains interrupt request");

	// Maybe we allow more than one TD later ;-)
	while ((p = urb_priv->desc_list.next) != &urb_priv->desc_list) {

		desc = list_entry (p, uhci_desc_t, desc_list);

		if (is_td_active(desc)) {
			// do not process active TDs
			//dbg("TD ACT Status @%p %08x",desc,le32_to_cpu(desc->hw.td.status));
			break;
		}

		if (!(desc->hw.td.status & cpu_to_le32(TD_CTRL_IOC))) {
			// do not process one-shot TDs, no recycling
			break;
		}
		// extract transfer parameters from TD

		actual_length = uhci_actual_length(le32_to_cpu(desc->hw.td.status));
		status = uhci_map_status (uhci_status_bits (le32_to_cpu(desc->hw.td.status)), usb_pipeout (urb->pipe));

		// see if EP is stalled
		if (status == -EPIPE) {
			// set up stalled condition
			usb_endpoint_halt (urb->dev, usb_pipeendpoint (urb->pipe), usb_pipeout (urb->pipe));
		}

		// if any error occurred: ignore this td, and continue
		if (status != 0) {
			//uhci_show_td (desc);
			urb->error_count++;
			goto recycle;
		}
		else
			urb->actual_length = actual_length;

	recycle:
		uhci_urb_dma_sync(s, urb, urb->hcpriv);
		if (urb->complete) {
			//dbg("process_interrupt: calling completion, status %i",status);
			urb->status = status;
			((urb_priv_t*)urb->hcpriv)->flags=1; // if unlink_urb is called during completion

			spin_unlock(&s->urb_list_lock);
			
			urb->complete ((struct urb *) urb);
			
			spin_lock(&s->urb_list_lock);

			((urb_priv_t*)urb->hcpriv)->flags=0;		       			
		}
		
		if ((urb->status != -ECONNABORTED) && (urb->status != ECONNRESET) &&
			    (urb->status != -ENOENT)) {

			urb->status = -EINPROGRESS;

			// Recycle INT-TD if interval!=0, else mark TD as one-shot
			if (urb->interval) {
				
				desc->hw.td.info &= cpu_to_le32(~(1 << TD_TOKEN_TOGGLE));
				if (status==0) {
					((urb_priv_t*)urb->hcpriv)->started=jiffies;
					desc->hw.td.info |= cpu_to_le32((usb_gettoggle (urb->dev, usb_pipeendpoint (urb->pipe),
									    usb_pipeout (urb->pipe)) << TD_TOKEN_TOGGLE));
					usb_dotoggle (urb->dev, usb_pipeendpoint (urb->pipe), usb_pipeout (urb->pipe));
				} else {
					desc->hw.td.info |= cpu_to_le32((!usb_gettoggle (urb->dev, usb_pipeendpoint (urb->pipe),
									     usb_pipeout (urb->pipe)) << TD_TOKEN_TOGGLE));
				}
				desc->hw.td.status= cpu_to_le32((urb->pipe & TD_CTRL_LS) | TD_CTRL_ACTIVE | TD_CTRL_IOC |
					(urb->transfer_flags & USB_DISABLE_SPD ? 0 : TD_CTRL_SPD) | (3 << 27));
				mb();
			}
			else {
				uhci_unlink_urb_async(s, urb, UNLINK_ASYNC_STORE_URB);
				uhci_clean_iso_step2(s, urb_priv);
				// correct toggle after unlink
				usb_dotoggle (urb->dev, usb_pipeendpoint (urb->pipe), usb_pipeout (urb->pipe));
				clr_td_ioc(desc); // inactivate TD
			}
		}
	}

	return ret;
}

// mode: PROCESS_ISO_REGULAR: processing only for done TDs, unlink TDs
// mode: PROCESS_ISO_FORCE: force processing, don't unlink TDs (already unlinked)

_static int process_iso (uhci_t *s, struct urb *urb, int mode)
{
	int i;
	int ret = 0;
	urb_priv_t *urb_priv = urb->hcpriv;
	struct list_head *p = urb_priv->desc_list.next, *p_tmp;
	uhci_desc_t *desc = list_entry (urb_priv->desc_list.prev, uhci_desc_t, desc_list);

	dbg("urb contains iso request");
	if (is_td_active(desc) && mode==PROCESS_ISO_REGULAR)
		return -EXDEV;	// last TD not finished

	urb->error_count = 0;
	urb->actual_length = 0;
	urb->status = 0;
	dbg("process iso urb %p, %li, %i, %i, %i %08x",urb,jiffies,UHCI_GET_CURRENT_FRAME(s),
	    urb->number_of_packets,mode,le32_to_cpu(desc->hw.td.status));

	for (i = 0; p != &urb_priv->desc_list;  i++) {
		desc = list_entry (p, uhci_desc_t, desc_list);
		
		//uhci_show_td(desc);
		if (is_td_active(desc)) {
			// means we have completed the last TD, but not the TDs before
			desc->hw.td.status &= cpu_to_le32(~TD_CTRL_ACTIVE);
			dbg("TD still active (%x)- grrr. paranoia!", le32_to_cpu(desc->hw.td.status));
			ret = -EXDEV;
			urb->iso_frame_desc[i].status = ret;
			unlink_td (s, desc, 1);
			// FIXME: immediate deletion may be dangerous
			goto err;
		}

		if (mode == PROCESS_ISO_REGULAR)
			unlink_td (s, desc, 1);

		if (urb->number_of_packets <= i) {
			dbg("urb->number_of_packets (%d)<=(%d)", urb->number_of_packets, i);
			ret = -EINVAL;
			goto err;
		}

		urb->iso_frame_desc[i].actual_length = uhci_actual_length(le32_to_cpu(desc->hw.td.status));
		urb->iso_frame_desc[i].status = uhci_map_status (uhci_status_bits (le32_to_cpu(desc->hw.td.status)), usb_pipeout (urb->pipe));
		urb->actual_length += urb->iso_frame_desc[i].actual_length;

	      err:

		if (urb->iso_frame_desc[i].status != 0) {
			urb->error_count++;
			urb->status = urb->iso_frame_desc[i].status;
		}
		dbg("process_iso: %i: len:%d %08x status:%x",
		     i, urb->iso_frame_desc[i].actual_length, le32_to_cpu(desc->hw.td.status),urb->iso_frame_desc[i].status);

		p_tmp = p;
		p = p->next;
		list_del (p_tmp);
		delete_desc (s, desc);
	}
	
	dbg("process_iso: exit %i (%d), actual_len %i", i, ret,urb->actual_length);
	return ret;
}


_static int process_urb (uhci_t *s, struct list_head *p)
{
	int ret = 0;
	struct urb *urb;

	urb=list_entry (p, struct urb, urb_list);
	//dbg("process_urb: found queued urb: %p", urb);

	switch (usb_pipetype (urb->pipe)) {
	case PIPE_CONTROL:
		ret = process_transfer (s, urb, CLEAN_TRANSFER_REGULAR);
		break;
	case PIPE_BULK:
		if (!s->avoid_bulk.counter)
			ret = process_transfer (s, urb, CLEAN_TRANSFER_REGULAR);
		else
			return 0;
		break;
	case PIPE_ISOCHRONOUS:
		ret = process_iso (s, urb, PROCESS_ISO_REGULAR);
		break;
	case PIPE_INTERRUPT:
		ret = process_interrupt (s, urb);
		break;
	}

	if (urb->status != -EINPROGRESS) {
		urb_priv_t *urb_priv;
		struct usb_device *usb_dev;
		
		usb_dev=urb->dev;

		/* Release bandwidth for Interrupt or Iso transfers */
		if (urb->bandwidth) {
			if (usb_pipetype(urb->pipe)==PIPE_ISOCHRONOUS)
				usb_release_bandwidth (urb->dev, urb, 1);
			else if (usb_pipetype(urb->pipe)==PIPE_INTERRUPT && urb->interval)
				usb_release_bandwidth (urb->dev, urb, 0);
		}

		dbg("dequeued urb: %p", urb);
		dequeue_urb (s, urb);

		urb_priv = urb->hcpriv;

		uhci_urb_dma_unmap(s, urb, urb_priv);

#ifdef DEBUG_SLAB
		kmem_cache_free(urb_priv_kmem, urb_priv);
#else
		kfree (urb_priv);
#endif

		if ((usb_pipetype (urb->pipe) != PIPE_INTERRUPT)) {  // process_interrupt does completion on its own		
			struct urb *next_urb = urb->next;
			int is_ring = 0;
			int contains_killed = 0;
			int loop_count=0;
			
			if (next_urb) {
				// Find out if the URBs are linked to a ring
				while  (next_urb != NULL && next_urb != urb && loop_count < MAX_NEXT_COUNT) {
					if (next_urb->status == -ENOENT) {// killed URBs break ring structure & resubmission
						contains_killed = 1;
						break;
					}	
					next_urb = next_urb->next;
					loop_count++;
				}
				
				if (loop_count == MAX_NEXT_COUNT)
					err("process_urb: Too much linked URBs in ring detection!");

				if (next_urb == urb)
					is_ring=1;
			}			

			// Submit idle/non-killed URBs linked with urb->next
			// Stop before the current URB				
			
			next_urb = urb->next;	
			if (next_urb && !contains_killed) {
				int ret_submit;
				next_urb = urb->next;	
				
				loop_count=0;
				while (next_urb != NULL && next_urb != urb && loop_count < MAX_NEXT_COUNT) {
					if (next_urb->status != -EINPROGRESS) {
					
						if (next_urb->status == -ENOENT) 
							break;

						spin_unlock(&s->urb_list_lock);

						ret_submit=uhci_submit_urb(next_urb);
						spin_lock(&s->urb_list_lock);
						
						if (ret_submit)
							break;						
					}
					loop_count++;
					next_urb = next_urb->next;
				}
				if (loop_count == MAX_NEXT_COUNT)
					err("process_urb: Too much linked URBs in resubmission!");
			}

			// Completion
			if (urb->complete) {
				int was_unlinked = (urb->status == -ENOENT);
				urb->dev = NULL;
				spin_unlock(&s->urb_list_lock);

				urb->complete ((struct urb *) urb);

				// Re-submit the URB if ring-linked
				if (is_ring && !was_unlinked && !contains_killed) {
					urb->dev=usb_dev;
					uhci_submit_urb (urb);
				}
				spin_lock(&s->urb_list_lock);
			}
			
			usb_dec_dev_use (usb_dev);
		}
	}

	return ret;
}

_static void uhci_interrupt (int irq, void *__uhci, struct pt_regs *regs)
{
	uhci_t *s = __uhci;
	unsigned int io_addr = s->io_addr;
	unsigned short status;
	struct list_head *p, *p2;
	int restarts, work_done;
	
	/*
	 * Read the interrupt status, and write it back to clear the
	 * interrupt cause
	 */

	status = inw (io_addr + USBSTS);

	if (!status)		/* shared interrupt, not mine */
		return;

	dbg("interrupt");

	if (status != 1) {
		// Avoid too much error messages at a time
		if (time_after(jiffies, s->last_error_time + ERROR_SUPPRESSION_TIME)) {
			warn("interrupt, status %x, frame# %i", status, 
			     UHCI_GET_CURRENT_FRAME(s));
			s->last_error_time = jiffies;
		}
		
		// remove host controller halted state
		if ((status&0x20) && (s->running)) {
			err("Host controller halted, trying to restart.");
			outw (USBCMD_RS | inw(io_addr + USBCMD), io_addr + USBCMD);
		}
		//uhci_show_status (s);
	}
	/*
	 * traverse the list in *reverse* direction, because new entries
	 * may be added at the end.
	 * also, because process_urb may unlink the current urb,
	 * we need to advance the list before
	 * New: check for max. workload and restart count
	 */

	spin_lock (&s->urb_list_lock);

	restarts=0;
	work_done=0;

restart:
	s->unlink_urb_done=0;
	p = s->urb_list.prev;	

	while (p != &s->urb_list && (work_done < 1024)) {
		p2 = p;
		p = p->prev;

		process_urb (s, p2);
		
		work_done++;

		if (s->unlink_urb_done) {
			s->unlink_urb_done=0;
			restarts++;
			
			if (restarts<16)	// avoid endless restarts
				goto restart;
			else 
				break;
		}
	}
	if (time_after(jiffies, s->timeout_check + (HZ/30)))
		uhci_check_timeouts(s);

	clean_descs(s, CLEAN_NOT_FORCED);
	uhci_cleanup_unlink(s, CLEAN_NOT_FORCED);
	uhci_switch_timer_int(s);
							
	spin_unlock (&s->urb_list_lock);
	
	outw (status, io_addr + USBSTS);

	//dbg("uhci_interrupt: done");
}

_static void reset_hc (uhci_t *s)
{
	unsigned int io_addr = s->io_addr;

	s->apm_state = 0;
	/* Global reset for 50ms */
	outw (USBCMD_GRESET, io_addr + USBCMD);
	uhci_wait_ms (50);
	outw (0, io_addr + USBCMD);
	uhci_wait_ms (10);
}

_static void start_hc (uhci_t *s)
{
	unsigned int io_addr = s->io_addr;
	int timeout = 10;

	/*
	 * Reset the HC - this will force us to get a
	 * new notification of any already connected
	 * ports due to the virtual disconnect that it
	 * implies.
	 */
	outw (USBCMD_HCRESET, io_addr + USBCMD);

	while (inw (io_addr + USBCMD) & USBCMD_HCRESET) {
		if (!--timeout) {
			err("USBCMD_HCRESET timed out!");
			break;
		}
		udelay(1);
	}

	/* Turn on all interrupts */
	outw (USBINTR_TIMEOUT | USBINTR_RESUME | USBINTR_IOC | USBINTR_SP, io_addr + USBINTR);

	/* Start at frame 0 */
	outw (0, io_addr + USBFRNUM);
	outl (s->framelist_dma, io_addr + USBFLBASEADD);

	/* Run and mark it configured with a 64-byte max packet */
	outw (USBCMD_RS | USBCMD_CF | USBCMD_MAXP, io_addr + USBCMD);
	s->apm_state = 1;
	s->running = 1;
}

/* No  __devexit, since it maybe called from alloc_uhci() */
_static void
uhci_pci_remove (struct pci_dev *dev)
{
	uhci_t *s = pci_get_drvdata(dev);
	struct usb_device *root_hub = s->bus->root_hub;

	s->running = 0;		    // Don't allow submit_urb

	if (root_hub)
		usb_disconnect (&root_hub);

	reset_hc (s);
	wait_ms (1);

	uhci_unlink_urbs (s, 0, CLEAN_FORCED);  // Forced unlink of remaining URBs
	uhci_cleanup_unlink (s, CLEAN_FORCED);  // force cleanup of async killed URBs
	
	usb_deregister_bus (s->bus);

	release_region (s->io_addr, s->io_size);
	free_irq (s->irq, s);
	usb_free_bus (s->bus);
	cleanup_skel (s);
	kfree (s);
}

_static int __init uhci_start_usb (uhci_t *s)
{				/* start it up */
	/* connect the virtual root hub */
	struct usb_device *usb_dev;

	usb_dev = usb_alloc_dev (NULL, s->bus);
	if (!usb_dev)
		return -1;

	s->bus->root_hub = usb_dev;
	usb_connect (usb_dev);

	if (usb_new_device (usb_dev) != 0) {
		usb_free_dev (usb_dev);
		return -1;
	}

	return 0;
}

#ifdef CONFIG_PM
_static int
uhci_pci_suspend (struct pci_dev *dev, u32 state)
{
	reset_hc((uhci_t *) pci_get_drvdata(dev));
	return 0;
}

_static int
uhci_pci_resume (struct pci_dev *dev)
{
	start_hc((uhci_t *) pci_get_drvdata(dev));
	return 0;
}
#endif

_static int __devinit alloc_uhci (struct pci_dev *dev, int irq, unsigned int io_addr, unsigned int io_size)
{
	uhci_t *s;
	struct usb_bus *bus;
	char buf[8], *bufp = buf;

#ifndef __sparc__
	sprintf(buf, "%d", irq);
#else
	bufp = __irq_itoa(irq);
#endif
	printk(KERN_INFO __FILE__ ": USB UHCI at I/O 0x%x, IRQ %s\n",
		io_addr, bufp);

	s = kmalloc (sizeof (uhci_t), GFP_KERNEL);
	if (!s)
		return -1;

	memset (s, 0, sizeof (uhci_t));
	INIT_LIST_HEAD (&s->free_desc);
	INIT_LIST_HEAD (&s->urb_list);
	INIT_LIST_HEAD (&s->urb_unlinked);
	spin_lock_init (&s->urb_list_lock);
	spin_lock_init (&s->qh_lock);
	spin_lock_init (&s->td_lock);
	atomic_set(&s->avoid_bulk, 0);
	s->irq = -1;
	s->io_addr = io_addr;
	s->io_size = io_size;
	s->uhci_pci=dev;

	bus = usb_alloc_bus (&uhci_device_operations);
	if (!bus) {
		kfree (s);
		return -1;
	}

	s->bus = bus;
	bus->bus_name = dev->slot_name;
	bus->hcpriv = s;

	/* UHCI specs says devices must have 2 ports, but goes on to say */
	/* they may have more but give no way to determine how many they */
	/* have, so default to 2 */
	/* According to the UHCI spec, Bit 7 is always set to 1. So we try */
	/* to use this to our advantage */

	for (s->maxports = 0; s->maxports < (io_size - 0x10) / 2; s->maxports++) {
		unsigned int portstatus;

		portstatus = inw (io_addr + 0x10 + (s->maxports * 2));
		dbg("port %i, adr %x status %x", s->maxports,
			io_addr + 0x10 + (s->maxports * 2), portstatus);
		if (!(portstatus & 0x0080))
			break;
	}
	warn("Detected %d ports", s->maxports);

	/* This is experimental so anything less than 2 or greater than 8 is */
	/*  something weird and we'll ignore it */
	if (s->maxports < 2 || s->maxports > 8) {
		dbg("Port count misdetected, forcing to 2 ports");
		s->maxports = 2;
	}

	s->rh.numports = s->maxports;
	s->loop_usage=0;
	if (init_skel (s)) {
		usb_free_bus (bus);
		kfree(s);
		return -1;
	}

	request_region (s->io_addr, io_size, MODNAME);
	reset_hc (s);
	usb_register_bus (s->bus);

	start_hc (s);

	if (request_irq (irq, uhci_interrupt, SA_SHIRQ, MODNAME, s)) {
		err("request_irq %d failed!",irq);
		usb_free_bus (bus);
		reset_hc (s);
		release_region (s->io_addr, s->io_size);
		cleanup_skel(s);
		kfree(s);
		return -1;
	}

	/* Enable PIRQ */
	pci_write_config_word (dev, USBLEGSUP, USBLEGSUP_DEFAULT);

	s->irq = irq;

	if(uhci_start_usb (s) < 0) {
		uhci_pci_remove(dev);
		return -1;
	}

	//chain new uhci device into global list
	pci_set_drvdata(dev, s);
	devs=s;

	return 0;
}

_static int __devinit
uhci_pci_probe (struct pci_dev *dev, const struct pci_device_id *id)
{
	int i;

	if (pci_enable_device(dev) < 0)
		return -ENODEV;
		
	if (!dev->irq) {
		err("found UHCI device with no IRQ assigned. check BIOS settings!");
		return -ENODEV;
	}
	
	pci_set_master(dev);

	/* Search for the IO base address.. */
	for (i = 0; i < 6; i++) {

		unsigned int io_addr = pci_resource_start(dev, i);
		unsigned int io_size = pci_resource_len(dev, i);
		if (!(pci_resource_flags(dev,i) & IORESOURCE_IO))
			continue;

		/* Is it already in use? */
		if (check_region (io_addr, io_size))
			break;
		/* disable legacy emulation */
		pci_write_config_word (dev, USBLEGSUP, 0);
	
		return alloc_uhci(dev, dev->irq, io_addr, io_size);
	}
	return -ENODEV;
}

/*-------------------------------------------------------------------------*/

static const struct pci_device_id __devinitdata uhci_pci_ids [] = { {

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

MODULE_DEVICE_TABLE (pci, uhci_pci_ids);

static struct pci_driver uhci_pci_driver = {
	name:		"usb-uhci",
	id_table:	&uhci_pci_ids [0],

	probe:		uhci_pci_probe,
	remove:		uhci_pci_remove,

#ifdef	CONFIG_PM
	suspend:	uhci_pci_suspend,
	resume:		uhci_pci_resume,
#endif	/* PM */

};

/*-------------------------------------------------------------------------*/

static int __init uhci_hcd_init (void) 
{
	int retval;

#ifdef DEBUG_SLAB
	urb_priv_kmem = kmem_cache_create("urb_priv", sizeof(urb_priv_t), 0, SLAB_HWCACHE_ALIGN, NULL, NULL);
	
	if(!urb_priv_kmem) {
		err("kmem_cache_create for urb_priv_t failed (out of memory)");
		return -ENOMEM;
	}
#endif	
	info(VERSTR);

#ifdef CONFIG_USB_UHCI_HIGH_BANDWIDTH
	info("High bandwidth mode enabled");	
#endif

	retval = pci_module_init (&uhci_pci_driver);

#ifdef DEBUG_SLAB
	if (retval < 0 ) {
		if (kmem_cache_destroy(urb_priv_kmem))
			err("urb_priv_kmem remained");
	}
#endif
	
	info(DRIVER_VERSION ":" DRIVER_DESC);

	return retval;
}

static void __exit uhci_hcd_cleanup (void) 
{      
	pci_unregister_driver (&uhci_pci_driver);
	
#ifdef DEBUG_SLAB
	if(kmem_cache_destroy(urb_priv_kmem))
		err("urb_priv_kmem remained");
#endif
}

module_init (uhci_hcd_init);
module_exit (uhci_hcd_cleanup);


MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");
