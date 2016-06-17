/*
 * Copyright (c) 2001-2003 by David Brownell
 * Copyright (c) 2003 Michal Sojka, for high-speed iso transfers
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* this file is part of ehci-hcd.c */

/*-------------------------------------------------------------------------*/

/*
 * EHCI scheduled transaction support:  interrupt, iso, split iso
 * These are called "periodic" transactions in the EHCI spec.
 *
 * Note that for interrupt transfers, the QH/QTD manipulation is shared
 * with the "asynchronous" transaction support (control/bulk transfers).
 * The only real difference is in how interrupt transfers are scheduled.
 *
 * For ISO, we make an "iso_stream" head to serve the same role as a QH.
 * It keeps track of every ITD (or SITD) that's linked, and holds enough
 * pre-calculated schedule data to make appending to the queue be quick.
 */

static int ehci_get_frame (struct usb_hcd *hcd);

/*-------------------------------------------------------------------------*/

/*
 * periodic_next_shadow - return "next" pointer on shadow list
 * @periodic: host pointer to qh/itd/sitd
 * @tag: hardware tag for type of this record
 */
static union ehci_shadow *
periodic_next_shadow (union ehci_shadow *periodic, int tag)
{
	switch (tag) {
	case Q_TYPE_QH:
		return &periodic->qh->qh_next;
	case Q_TYPE_FSTN:
		return &periodic->fstn->fstn_next;
	case Q_TYPE_ITD:
		return &periodic->itd->itd_next;
#ifdef have_split_iso
	case Q_TYPE_SITD:
		return &periodic->sitd->sitd_next;
#endif /* have_split_iso */
	}
	dbg ("BAD shadow %p tag %d", periodic->ptr, tag);
	// BUG ();
	return 0;
}

/* returns true after successful unlink */
/* caller must hold ehci->lock */
static int periodic_unlink (struct ehci_hcd *ehci, unsigned frame, void *ptr)
{
	union ehci_shadow	*prev_p = &ehci->pshadow [frame];
	u32			*hw_p = &ehci->periodic [frame];
	union ehci_shadow	here = *prev_p;
	union ehci_shadow	*next_p;

	/* find predecessor of "ptr"; hw and shadow lists are in sync */
	while (here.ptr && here.ptr != ptr) {
		prev_p = periodic_next_shadow (prev_p, Q_NEXT_TYPE (*hw_p));
		hw_p = &here.qh->hw_next;
		here = *prev_p;
	}
	/* an interrupt entry (at list end) could have been shared */
	if (!here.ptr) {
		dbg ("entry %p no longer on frame [%d]", ptr, frame);
		return 0;
	}
	// vdbg ("periodic unlink %p from frame %d", ptr, frame);

	/* update hardware list ... HC may still know the old structure, so
	 * don't change hw_next until it'll have purged its cache
	 */
	next_p = periodic_next_shadow (&here, Q_NEXT_TYPE (*hw_p));
	*hw_p = here.qh->hw_next;

	/* unlink from shadow list; HCD won't see old structure again */
	*prev_p = *next_p;
	next_p->ptr = 0;

	return 1;
}

/* how many of the uframe's 125 usecs are allocated? */
static unsigned short
periodic_usecs (struct ehci_hcd *ehci, unsigned frame, unsigned uframe)
{
	u32			*hw_p = &ehci->periodic [frame];
	union ehci_shadow	*q = &ehci->pshadow [frame];
	unsigned		usecs = 0;

	while (q->ptr) {
		switch (Q_NEXT_TYPE (*hw_p)) {
		case Q_TYPE_QH:
			/* is it in the S-mask? */
			if (q->qh->hw_info2 & cpu_to_le32 (1 << uframe))
				usecs += q->qh->usecs;
			/* ... or C-mask? */
			if (q->qh->hw_info2 & cpu_to_le32 (1 << (8 + uframe)))
				usecs += q->qh->c_usecs;
			q = &q->qh->qh_next;
			break;
		case Q_TYPE_FSTN:
			/* for "save place" FSTNs, count the relevant INTR
			 * bandwidth from the previous frame
			 */
			if (q->fstn->hw_prev != EHCI_LIST_END) {
				dbg ("not counting FSTN bandwidth yet ...");
			}
			q = &q->fstn->fstn_next;
			break;
		case Q_TYPE_ITD:
			usecs += q->itd->usecs [uframe];
			q = &q->itd->itd_next;
			break;
#ifdef have_split_iso
		case Q_TYPE_SITD:
			temp = q->sitd->hw_fullspeed_ep &
				__constant_cpu_to_le32 (1 << 31);

			// FIXME:  this doesn't count data bytes right...

			/* is it in the S-mask?  (count SPLIT, DATA) */
			if (q->sitd->hw_uframe & cpu_to_le32 (1 << uframe)) {
				if (temp)
					usecs += HS_USECS (188);
				else
					usecs += HS_USECS (1);
			}

			/* ... C-mask?  (count CSPLIT, DATA) */
			if (q->sitd->hw_uframe &
					cpu_to_le32 (1 << (8 + uframe))) {
				if (temp)
					usecs += HS_USECS (0);
				else
					usecs += HS_USECS (188);
			}
			q = &q->sitd->sitd_next;
			break;
#endif /* have_split_iso */
		default:
			BUG ();
		}
	}
#ifdef	DEBUG
	if (usecs > 100)
		err ("overallocated uframe %d, periodic is %d usecs",
			frame * 8 + uframe, usecs);
#endif
	return usecs;
}

/*-------------------------------------------------------------------------*/

static int enable_periodic (struct ehci_hcd *ehci)
{
	u32	cmd;
	int	status;

	/* did clearing PSE did take effect yet?
	 * takes effect only at frame boundaries...
	 */
	status = handshake (&ehci->regs->status, STS_PSS, 0, 9 * 125);
	if (status != 0) {
		ehci->hcd.state = USB_STATE_HALT;
		return status;
	}

	cmd = readl (&ehci->regs->command) | CMD_PSE;
	writel (cmd, &ehci->regs->command);
	/* posted write ... PSS happens later */
	ehci->hcd.state = USB_STATE_RUNNING;

	/* make sure ehci_work scans these */
	ehci->next_uframe = readl (&ehci->regs->frame_index)
				% (ehci->periodic_size << 3);
	return 0;
}

static int disable_periodic (struct ehci_hcd *ehci)
{
	u32	cmd;
	int	status;

	/* did setting PSE not take effect yet?
	 * takes effect only at frame boundaries...
	 */
	status = handshake (&ehci->regs->status, STS_PSS, STS_PSS, 9 * 125);
	if (status != 0) {
		ehci->hcd.state = USB_STATE_HALT;
		return status;
	}

	cmd = readl (&ehci->regs->command) & ~CMD_PSE;
	writel (cmd, &ehci->regs->command);
	/* posted write ... */

	ehci->next_uframe = -1;
	return 0;
}

/*-------------------------------------------------------------------------*/

// FIXME microframe periods not yet handled

static void intr_deschedule (
	struct ehci_hcd	*ehci,
	struct ehci_qh	*qh,
	int		wait
) {
	int		status;
	unsigned	frame = qh->start;

	do {
		periodic_unlink (ehci, frame, qh);
		qh_put (ehci, qh);
		frame += qh->period;
	} while (frame < ehci->periodic_size);

	qh->qh_state = QH_STATE_UNLINK;
	qh->qh_next.ptr = 0;
	ehci->periodic_sched--;

	/* maybe turn off periodic schedule */
	if (!ehci->periodic_sched)
		status = disable_periodic (ehci);
	else {
		status = 0;
		vdbg ("periodic schedule still enabled");
	}

	/*
	 * If the hc may be looking at this qh, then delay a uframe
	 * (yeech!) to be sure it's done.
	 * No other threads may be mucking with this qh.
	 */
	if (((ehci_get_frame (&ehci->hcd) - frame) % qh->period) == 0) {
		if (wait) {
			udelay (125);
			qh->hw_next = EHCI_LIST_END;
		} else {
			/* we may not be IDLE yet, but if the qh is empty
			 * the race is very short.  then if qh also isn't
			 * rescheduled soon, it won't matter.  otherwise...
			 */
			vdbg ("intr_deschedule...");
		}
	} else
		qh->hw_next = EHCI_LIST_END;

	qh->qh_state = QH_STATE_IDLE;

	/* update per-qh bandwidth utilization (for usbfs) */
	hcd_to_bus (&ehci->hcd)->bandwidth_allocated -= 
		(qh->usecs + qh->c_usecs) / qh->period;

	dbg ("descheduled qh %p, period = %d frame = %d count = %d, urbs = %d",
		qh, qh->period, frame,
		atomic_read (&qh->refcount), ehci->periodic_sched);
}

static int check_period (
	struct ehci_hcd *ehci, 
	unsigned	frame,
	unsigned	uframe,
	unsigned	period,
	unsigned	usecs
) {
	/* complete split running into next frame?
	 * given FSTN support, we could sometimes check...
	 */
	if (uframe >= 8)
		return 0;

	/*
	 * 80% periodic == 100 usec/uframe available
	 * convert "usecs we need" to "max already claimed" 
	 */
	usecs = 100 - usecs;

	do {
		int	claimed;

// FIXME delete when intr_submit handles non-empty queues
// this gives us a one intr/frame limit (vs N/uframe)
// ... and also lets us avoid tracking split transactions
// that might collide at a given TT/hub.
		if (ehci->pshadow [frame].ptr)
			return 0;

		claimed = periodic_usecs (ehci, frame, uframe);
		if (claimed > usecs)
			return 0;

// FIXME update to handle sub-frame periods
	} while ((frame += period) < ehci->periodic_size);

	// success!
	return 1;
}

static int check_intr_schedule (
	struct ehci_hcd		*ehci, 
	unsigned		frame,
	unsigned		uframe,
	const struct ehci_qh	*qh,
	u32			*c_maskp
)
{
    	int		retval = -ENOSPC;

	if (!check_period (ehci, frame, uframe, qh->period, qh->usecs))
		goto done;
	if (!qh->c_usecs) {
		retval = 0;
		*c_maskp = cpu_to_le32 (0);
		goto done;
	}

	/* This is a split transaction; check the bandwidth available for
	 * the completion too.  Check both worst and best case gaps: worst
	 * case is SPLIT near uframe end, and CSPLIT near start ... best is
	 * vice versa.  Difference can be almost two uframe times, but we
	 * reserve unnecessary bandwidth (waste it) this way.  (Actually
	 * even better cases exist, like immediate device NAK.)
	 *
	 * FIXME don't even bother unless we know this TT is idle in that
	 * range of uframes ... for now, check_period() allows only one
	 * interrupt transfer per frame, so needn't check "TT busy" status
	 * when scheduling a split (QH, SITD, or FSTN).
	 *
	 * FIXME ehci 0.96 and above can use FSTNs
	 */
	if (!check_period (ehci, frame, uframe + qh->gap_uf + 1,
				qh->period, qh->c_usecs))
		goto done;
	if (!check_period (ehci, frame, uframe + qh->gap_uf,
				qh->period, qh->c_usecs))
		goto done;

	*c_maskp = cpu_to_le32 (0x03 << (8 + uframe + qh->gap_uf));
	retval = 0;
done:
	return retval;
}

static int qh_schedule (struct ehci_hcd *ehci, struct ehci_qh *qh)
{
	int 		status;
	unsigned	uframe;
	u32		c_mask;
	unsigned	frame;		/* 0..(qh->period - 1), or NO_FRAME */

	qh->hw_next = EHCI_LIST_END;
	frame = qh->start;

	/* reuse the previous schedule slots, if we can */
	if (frame < qh->period) {
		uframe = ffs (le32_to_cpup (&qh->hw_info2) & 0x00ff);
		status = check_intr_schedule (ehci, frame, --uframe,
				qh, &c_mask);
	} else {
		uframe = 0;
		c_mask = 0;
		status = -ENOSPC;
	}

	/* else scan the schedule to find a group of slots such that all
	 * uframes have enough periodic bandwidth available.
	 */
	if (status) {
		frame = qh->period - 1;
		do {
			for (uframe = 0; uframe < 8; uframe++) {
				status = check_intr_schedule (ehci,
						frame, uframe, qh,
						&c_mask);
				if (status == 0)
					break;
			}
		} while (status && frame--);
		if (status)
			goto done;
		qh->start = frame;

		/* reset S-frame and (maybe) C-frame masks */
		qh->hw_info2 &= ~0xffff;
		qh->hw_info2 |= cpu_to_le32 (1 << uframe) | c_mask;
	} else
		dbg ("reused previous qh %p schedule", qh);

	/* stuff into the periodic schedule */
	qh->qh_state = QH_STATE_LINKED;
	dbg ("scheduled qh %p usecs %d/%d period %d.0 starting %d.%d (gap %d)",
		qh, qh->usecs, qh->c_usecs,
		qh->period, frame, uframe, qh->gap_uf);
	do {
		if (unlikely (ehci->pshadow [frame].ptr != 0)) {

// FIXME -- just link toward the end, before any qh with a shorter period,
// AND accomodate it already having been linked here (after some other qh)
// AS WELL AS updating the schedule checking logic

			BUG ();
		} else {
			ehci->pshadow [frame].qh = qh_get (qh);
			ehci->periodic [frame] =
				QH_NEXT (qh->qh_dma);
		}
		wmb ();
		frame += qh->period;
	} while (frame < ehci->periodic_size);

	/* update per-qh bandwidth for usbfs */
	hcd_to_bus (&ehci->hcd)->bandwidth_allocated += 
		(qh->usecs + qh->c_usecs) / qh->period;

	/* maybe enable periodic schedule processing */
	if (!ehci->periodic_sched++)
		status = enable_periodic (ehci);
done:
	return status;
}

static int intr_submit (
	struct ehci_hcd		*ehci,
	struct urb		*urb,
	struct list_head	*qtd_list,
	int			mem_flags
) {
	unsigned		epnum;
	unsigned long		flags;
	struct ehci_qh		*qh;
	struct hcd_dev		*dev;
	int			is_input;
	int			status = 0;
	struct list_head	empty;

	/* get endpoint and transfer/schedule data */
	epnum = usb_pipeendpoint (urb->pipe);
	is_input = usb_pipein (urb->pipe);
	if (is_input)
		epnum |= 0x10;

	spin_lock_irqsave (&ehci->lock, flags);
	dev = (struct hcd_dev *)urb->dev->hcpriv;

	/* get qh and force any scheduling errors */
	INIT_LIST_HEAD (&empty);
	qh = qh_append_tds (ehci, urb, &empty, epnum, &dev->ep [epnum]);
	if (qh == 0) {
		status = -ENOMEM;
		goto done;
	}
	if (qh->qh_state == QH_STATE_IDLE) {
		if ((status = qh_schedule (ehci, qh)) != 0)
			goto done;
	}

	/* then queue the urb's tds to the qh */
	qh = qh_append_tds (ehci, urb, qtd_list, epnum, &dev->ep [epnum]);
	BUG_ON (qh == 0);

	/* ... update usbfs periodic stats */
	hcd_to_bus (&ehci->hcd)->bandwidth_int_reqs++;

done:
	spin_unlock_irqrestore (&ehci->lock, flags);
	if (status)
		qtd_list_free (ehci, urb, qtd_list);

	return status;
}

/*-------------------------------------------------------------------------*/

static inline struct ehci_iso_stream *
iso_stream_alloc (int mem_flags)
{
	struct ehci_iso_stream *stream;

	stream = kmalloc(sizeof *stream, mem_flags);
	if (likely (stream != 0)) {
		memset (stream, 0, sizeof(*stream));
		INIT_LIST_HEAD(&stream->itd_list);
		INIT_LIST_HEAD(&stream->free_itd_list);
		stream->next_uframe = -1;
		stream->refcount = 1;
	}
	return stream;
}

static inline void
iso_stream_init (
	struct ehci_iso_stream	*stream,
	struct usb_device	*dev,
	int			pipe,
	unsigned		interval
)
{
	u32			buf1;
	unsigned		epnum, maxp, multi;
	int			is_input;
	long			bandwidth;

	/*
	 * this might be a "high bandwidth" highspeed endpoint,
	 * as encoded in the ep descriptor's wMaxPacket field
	 */
	epnum = usb_pipeendpoint (pipe);
	is_input = usb_pipein (pipe) ? USB_DIR_IN : 0;
	if (is_input) {
		maxp = dev->epmaxpacketin [epnum];
		buf1 = (1 << 11);
	} else {
		maxp = dev->epmaxpacketout [epnum];
		buf1 = 0;
	}

	multi = hb_mult(maxp);
	maxp = max_packet(maxp);
	buf1 |= maxp;
	maxp *= multi;

	stream->dev = (struct hcd_dev *)dev->hcpriv;

	stream->bEndpointAddress = is_input | epnum;
	stream->interval = interval;
	stream->maxp = maxp;

	stream->buf0 = cpu_to_le32 ((epnum << 8) | dev->devnum);
	stream->buf1 = cpu_to_le32 (buf1);
	stream->buf2 = cpu_to_le32 (multi);

	/* usbfs wants to report the average usecs per frame tied up
	 * when transfers on this endpoint are scheduled ...
	 */
	stream->usecs = HS_USECS_ISO (maxp);
	bandwidth = stream->usecs * 8;
	bandwidth /= 1 << (interval - 1);
	stream->bandwidth = bandwidth;
}

static void
iso_stream_put(struct ehci_hcd *ehci, struct ehci_iso_stream *stream)
{
	stream->refcount--;

	/* free whenever just a dev->ep reference remains.
	 * not like a QH -- no persistent state (toggle, halt)
	 */
	if (stream->refcount == 1) {
		int is_in;

		// BUG_ON (!list_empty(&stream->itd_list));

		while (!list_empty (&stream->free_itd_list)) {
			struct ehci_itd	*itd;

			itd = list_entry (stream->free_itd_list.next,
				struct ehci_itd, itd_list);
			list_del (&itd->itd_list);
			pci_pool_free (ehci->itd_pool, itd, itd->itd_dma);
		}

		is_in = (stream->bEndpointAddress & USB_DIR_IN) ? 0x10 : 0;
		stream->bEndpointAddress &= 0x0f;
		stream->dev->ep [is_in + stream->bEndpointAddress] = 0;

		if (stream->rescheduled) {
			ehci_info (ehci, "ep%d%s-iso rescheduled "
				"%lu times in %lu seconds\n",
				stream->bEndpointAddress, is_in ? "in" : "out",
				stream->rescheduled,
				((jiffies - stream->start)/HZ)
				);
		}

		kfree(stream);
	}
}

static inline struct ehci_iso_stream *
iso_stream_get (struct ehci_iso_stream *stream)
{
	if (likely (stream != 0))
		stream->refcount++;
	return stream;
}

static struct ehci_iso_stream *
iso_stream_find (struct ehci_hcd *ehci, struct urb *urb)
{
	unsigned		epnum;
	struct hcd_dev		*dev;
	struct ehci_iso_stream	*stream;
	unsigned long		flags;

	epnum = usb_pipeendpoint (urb->pipe);
	if (usb_pipein(urb->pipe))
		epnum += 0x10;

	spin_lock_irqsave (&ehci->lock, flags);

	dev = (struct hcd_dev *)urb->dev->hcpriv;
	stream = dev->ep [epnum];

	if (unlikely (stream == 0)) {
		stream = iso_stream_alloc(GFP_ATOMIC);
		if (likely (stream != 0)) {
			/* dev->ep owns the initial refcount */
			dev->ep[epnum] = stream;
			iso_stream_init(stream, urb->dev, urb->pipe,
					urb->interval);
		}

	/* if dev->ep [epnum] is a QH, info1.maxpacket is nonzero */
	} else if (unlikely (stream->hw_info1 != 0)) {
		ehci_dbg (ehci, "dev %s ep%d%s, not iso??\n",
			urb->dev->devpath, epnum & 0x0f,
			(epnum & 0x10) ? "in" : "out");
		stream = 0;
	}

	/* caller guarantees an eventual matching iso_stream_put */
	stream = iso_stream_get (stream);

	spin_unlock_irqrestore (&ehci->lock, flags);
	return stream;
}

/*-------------------------------------------------------------------------*/

static inline struct ehci_itd_sched *
itd_sched_alloc (unsigned packets, int mem_flags)
{
	struct ehci_itd_sched	*itd_sched;
	int			size = sizeof *itd_sched;

	size += packets * sizeof (struct ehci_iso_uframe);
	itd_sched = kmalloc (size, mem_flags);
	if (likely (itd_sched != 0)) {
		memset(itd_sched, 0, size);
		INIT_LIST_HEAD (&itd_sched->itd_list);
	}
	return itd_sched;
}

static int
itd_sched_init (
	struct ehci_itd_sched	*itd_sched,
	struct ehci_iso_stream	*stream,
	struct urb		*urb
)
{
	unsigned	i;
	dma_addr_t	dma = urb->transfer_dma;

	/* how many uframes are needed for these transfers */
	itd_sched->span = urb->number_of_packets * stream->interval;

	/* figure out per-uframe itd fields that we'll need later
	 * when we fit new itds into the schedule.
	 */
	for (i = 0; i < urb->number_of_packets; i++) {
		struct ehci_iso_uframe	*uframe = &itd_sched->packet [i];
		unsigned		length;
		dma_addr_t		buf;
		u32			trans;

		length = urb->iso_frame_desc [i].length;
		buf = dma + urb->iso_frame_desc [i].offset;

		trans = EHCI_ISOC_ACTIVE;
		trans |= buf & 0x0fff;
		if (unlikely (((i + 1) == urb->number_of_packets))
				&& !(urb->transfer_flags & URB_NO_INTERRUPT))
			trans |= EHCI_ITD_IOC;
		trans |= length << 16;
		uframe->transaction = cpu_to_le32 (trans);

		/* might need to cross a buffer page within a td */
		uframe->bufp = (buf & ~(u64)0x0fff);
		buf += length;
		if (unlikely ((uframe->bufp != (buf & ~(u64)0x0fff))))
			uframe->cross = 1;
	}
	return 0;
}

static void
itd_sched_free (
	struct ehci_iso_stream	*stream,
	struct ehci_itd_sched	*itd_sched
)
{
	list_splice (&itd_sched->itd_list, &stream->free_itd_list);
	kfree (itd_sched);
}

static int
itd_urb_transaction (
	struct ehci_iso_stream	*stream,
	struct ehci_hcd		*ehci,
	struct urb		*urb,
	int			mem_flags
)
{
	struct ehci_itd		*itd;
	int			status;
	dma_addr_t		itd_dma;
	int			i;
	unsigned		num_itds;
	struct ehci_itd_sched	*itd_sched;

	itd_sched = itd_sched_alloc (urb->number_of_packets, mem_flags);
	if (unlikely (itd_sched == 0))
		return -ENOMEM;

	status = itd_sched_init (itd_sched, stream, urb);
	if (unlikely (status != 0))  {
		itd_sched_free (stream, itd_sched);
		return status;
	}

	if (urb->interval < 8)
		num_itds = 1 + (itd_sched->span + 7) / 8;
	else
		num_itds = urb->number_of_packets;

	/* allocate/init ITDs */
	for (i = 0; i < num_itds; i++) {

		/* free_itd_list.next might be cache-hot ... but maybe
		 * the HC caches it too. avoid that issue for now.
		 */

		/* prefer previously-allocated itds */
		if (likely (!list_empty(&stream->free_itd_list))) {
			itd = list_entry (stream->free_itd_list.prev,
					 struct ehci_itd, itd_list);
			list_del (&itd->itd_list);
			itd_dma = itd->itd_dma;
		} else
			itd = pci_pool_alloc (ehci->itd_pool, mem_flags,
					&itd_dma);

		if (unlikely (0 == itd)) {
			itd_sched_free (stream, itd_sched);
			return -ENOMEM;
		}
		memset (itd, 0, sizeof *itd);
		itd->itd_dma = itd_dma;
		list_add (&itd->itd_list, &itd_sched->itd_list);
	}

	/* temporarily store schedule info in hcpriv */
	urb->hcpriv = itd_sched;
	urb->error_count = 0;
	return 0;
}

/*
 * This scheduler plans almost as far into the future as it has actual
 * periodic schedule slots.  (Affected by TUNE_FLS, which defaults to
 * "as small as possible" to be cache-friendlier.)  That limits the size
 * transfers you can stream reliably; avoid more than 64 msec per urb.
 * Also avoid queue depths of less than ehci's worst irq latency (affected
 * by the per-urb URB_NO_INTERRUPT hint, the log2_irq_thresh module parameter,
 * and other factors); or more than about 230 msec total (for portability,
 * given EHCI_TUNE_FLS and the slop).  Or, write a smarter scheduler!
 */

#define SCHEDULE_SLOP	10	/* frames */

static int
itd_stream_schedule (
	struct ehci_hcd		*ehci,
	struct urb		*urb,
	struct ehci_iso_stream	*stream
)
{
	u32			now, start, end, max;
	int			status;
	unsigned		mod = ehci->periodic_size << 3;
	struct ehci_itd_sched	*itd_sched = urb->hcpriv;

	if (unlikely (itd_sched->span > (mod - 8 * SCHEDULE_SLOP))) {
		ehci_dbg (ehci, "iso request %p too long\n", urb);
		status = -EFBIG;
		goto fail;
	}

	now = readl (&ehci->regs->frame_index) % mod;

	/* when's the last uframe this urb could start? */
	max = now + mod;
	max -= itd_sched->span;
	max -= 8 * SCHEDULE_SLOP;

	/* typical case: reuse current schedule. stream is still active,
	 * and no gaps from host falling behind (irq delays etc)
	 */
	if (likely (!list_empty (&stream->itd_list))) {

		start = stream->next_uframe;
		if (start < now)
			start += mod;
		if (likely (start < max))
			goto ready;

		/* two cases:
		 * (a) we missed some uframes ... can reschedule
		 * (b) trying to overcommit the schedule
		 * FIXME (b) should be a hard failure
		 */
	}

	/* need to schedule; when's the next (u)frame we could start?
	 * this is bigger than ehci->i_thresh allows; scheduling itself
	 * isn't free, the slop should handle reasonably slow cpus.  it
	 * can also help high bandwidth if the dma and irq loads don't
	 * jump until after the queue is primed.
	 */
	start = SCHEDULE_SLOP * 8 + (now & ~0x07);
	end = start;

	ehci_vdbg (ehci, "%s schedule from %d (%d..%d), was %d\n",
			__FUNCTION__, now, start, max,
			stream->next_uframe);

	/* NOTE:  assumes URB_ISO_ASAP, to limit complexity/bugs */

	if (likely (max > (start + urb->interval)))
		max = start + urb->interval;

	/* hack:  account for itds already scheduled to this endpoint */
	if (unlikely (list_empty (&stream->itd_list)))
		end = max;

	/* within [start..max] find a uframe slot with enough bandwidth */
	end %= mod;
	do {
		unsigned	uframe;
		int		enough_space = 1;

		/* check schedule: enough space? */
		uframe = start;
		do {
			uframe %= mod;

			/* can't commit more than 80% periodic == 100 usec */
			if (periodic_usecs (ehci, uframe >> 3, uframe & 0x7)
					> (100 - stream->usecs)) {
				enough_space = 0;
				break;
			}

			/* we know urb->interval is 2^N uframes */
			uframe += urb->interval;
		} while (uframe != end);

		/* (re)schedule it here if there's enough bandwidth */
		if (enough_space) {
			start %= mod;
			if (unlikely (!list_empty (&stream->itd_list))) {
				/* host fell behind ... maybe irq latencies
				 * delayed this request queue for too long.
				 */
				stream->rescheduled++;
				pr_debug ("ehci %s devpath %d "
					"iso%d%s %d.%d skip %d.%d\n",
					ehci->pdev->slot_name,
					urb->dev->devpath,
					stream->bEndpointAddress & 0x0f,
					(stream->bEndpointAddress & USB_DIR_IN)
						? "in" : "out",
					stream->next_uframe >> 3,
					stream->next_uframe & 0x7,
					start >> 3, start & 0x7);
			}
			stream->next_uframe = start;
			goto ready;
		}

	} while (++start < max);

	/* no room in the schedule */
	ehci_dbg (ehci, "iso %ssched full %p (now %d end %d max %d)\n",
		list_empty (&stream->itd_list) ? "" : "re",
		urb, now, end, max);
	status = -ENOSPC;

fail:
	itd_sched_free (stream, itd_sched);
	urb->hcpriv = 0;
	return status;

ready:
	urb->start_frame = stream->next_uframe;
	return 0;
}

/*-------------------------------------------------------------------------*/

static inline void
itd_init (struct ehci_iso_stream *stream, struct ehci_itd *itd)
{
	int i;

	itd->hw_next = EHCI_LIST_END;
	itd->hw_bufp [0] = stream->buf0;
	itd->hw_bufp [1] = stream->buf1;
	itd->hw_bufp [2] = stream->buf2;

	for (i = 0; i < 8; i++)
		itd->index[i] = -1;

	/* All other fields are filled when scheduling */
}

static inline void
itd_patch (
	struct ehci_itd		*itd,
	struct ehci_itd_sched	*itd_sched,
	unsigned		index,
	u16			uframe,
	int			first
)
{
	struct ehci_iso_uframe	*uf = &itd_sched->packet [index];
	unsigned		pg = itd->pg;

	// BUG_ON (pg == 6 && uf->cross);

	uframe &= 0x07;
	itd->index [uframe] = index;

	itd->hw_transaction [uframe] = uf->transaction;
	itd->hw_transaction [uframe] |= cpu_to_le32 (pg << 12);
	itd->hw_bufp [pg] |= cpu_to_le32 (uf->bufp & ~(u32)0);
	itd->hw_bufp_hi [pg] |= cpu_to_le32 ((u32)(uf->bufp >> 32));

	/* iso_frame_desc[].offset must be strictly increasing */
	if (unlikely (!first && uf->cross)) {
		u64	bufp = uf->bufp + 4096;
		itd->pg = ++pg;
		itd->hw_bufp [pg] |= cpu_to_le32 (bufp & ~(u32)0);
		itd->hw_bufp_hi [pg] |= cpu_to_le32 ((u32)(bufp >> 32));
	}
}

static inline void
itd_link (struct ehci_hcd *ehci, unsigned frame, struct ehci_itd *itd)
{
	/* always prepend ITD/SITD ... only QH tree is order-sensitive */
	itd->itd_next = ehci->pshadow [frame];
	itd->hw_next = ehci->periodic [frame];
	ehci->pshadow [frame].itd = itd;
	itd->frame = frame;
	wmb ();
	ehci->periodic [frame] = cpu_to_le32 (itd->itd_dma) | Q_TYPE_ITD;
}

/* fit urb's itds into the selected schedule slot; activate as needed */
static int
itd_link_urb (
	struct ehci_hcd		*ehci,
	struct urb		*urb,
	unsigned		mod,
	struct ehci_iso_stream	*stream
)
{
	int			packet, first = 1;
	unsigned		next_uframe, uframe, frame;
	struct ehci_itd_sched	*itd_sched = urb->hcpriv;
	struct ehci_itd		*itd;

	next_uframe = stream->next_uframe % mod;

	if (unlikely (list_empty(&stream->itd_list))) {
		hcd_to_bus (&ehci->hcd)->bandwidth_allocated
				+= stream->bandwidth;
		ehci_vdbg (ehci,
			"schedule devp %s ep%d%s-iso period %d start %d.%d\n",
			urb->dev->devpath, stream->bEndpointAddress & 0x0f,
			(stream->bEndpointAddress & USB_DIR_IN) ? "in" : "out",
			urb->interval,
			next_uframe >> 3, next_uframe & 0x7);
		stream->start = jiffies;
	}
	hcd_to_bus (&ehci->hcd)->bandwidth_isoc_reqs++;

	/* fill iTDs uframe by uframe */
	for (packet = 0, itd = 0; packet < urb->number_of_packets; ) {
		if (itd == 0) {
			/* ASSERT:  we have all necessary itds */
			// BUG_ON (list_empty (&itd_sched->itd_list));

			/* ASSERT:  no itds for this endpoint in this uframe */

			itd = list_entry (itd_sched->itd_list.next,
					struct ehci_itd, itd_list);
			list_move_tail (&itd->itd_list, &stream->itd_list);
			itd->stream = iso_stream_get (stream);
			itd->urb = usb_get_urb (urb);
			first = 1;
			itd_init (stream, itd);
		}

		uframe = next_uframe & 0x07;
		frame = next_uframe >> 3;

		itd->usecs [uframe] = stream->usecs;
		itd_patch (itd, itd_sched, packet, uframe, first);
		first = 0;

		next_uframe += stream->interval;
		next_uframe %= mod;
		packet++;

		/* link completed itds into the schedule */
		if (((next_uframe >> 3) != frame)
				|| packet == urb->number_of_packets) {
			itd_link (ehci, frame % ehci->periodic_size, itd);
			itd = 0;
		}
	}
	stream->next_uframe = next_uframe;

	/* don't need that schedule data any more */
	itd_sched_free (stream, itd_sched);
	urb->hcpriv = 0;

	if (unlikely (!ehci->periodic_sched++))
		return enable_periodic (ehci);
	return 0;
}

#define	ISO_ERRS (EHCI_ISOC_BUF_ERR | EHCI_ISOC_BABBLE | EHCI_ISOC_XACTERR)

static unsigned
itd_complete (
	struct ehci_hcd	*ehci,
	struct ehci_itd	*itd,
	struct pt_regs	*regs
) {
	struct urb				*urb = itd->urb;
	struct usb_iso_packet_descriptor	*desc;
	u32					t;
	unsigned				uframe;
	int					urb_index = -1;
	struct ehci_iso_stream			*stream = itd->stream;
	struct usb_device			*dev;

	/* for each uframe with a packet */
	for (uframe = 0; uframe < 8; uframe++) {
		if (likely (itd->index[uframe] == -1))
			continue;
		urb_index = itd->index[uframe];
		desc = &urb->iso_frame_desc [urb_index];

		t = le32_to_cpup (&itd->hw_transaction [uframe]);
		itd->hw_transaction [uframe] = 0;

		/* report transfer status */
		if (unlikely (t & ISO_ERRS)) {
			urb->error_count++;
			if (t & EHCI_ISOC_BUF_ERR)
				desc->status = usb_pipein (urb->pipe)
					? -ENOSR  /* hc couldn't read */
					: -ECOMM; /* hc couldn't write */
			else if (t & EHCI_ISOC_BABBLE)
				desc->status = -EOVERFLOW;
			else /* (t & EHCI_ISOC_XACTERR) */
				desc->status = -EPROTO;

			/* HC need not update length with this error */
			if (!(t & EHCI_ISOC_BABBLE))
				desc->actual_length = EHCI_ITD_LENGTH (t);
		} else if (likely ((t & EHCI_ISOC_ACTIVE) == 0)) {
			desc->status = 0;
			desc->actual_length = EHCI_ITD_LENGTH (t);
		}
	}

	usb_put_urb (urb);
	itd->urb = 0;
	itd->stream = 0;
	list_move (&itd->itd_list, &stream->free_itd_list);
	iso_stream_put (ehci, stream);

	/* handle completion now? */
	if (likely ((urb_index + 1) != urb->number_of_packets))
		return 0;

	/* ASSERT: it's really the last itd for this urb
	list_for_each_entry (itd, &stream->itd_list, itd_list)
		BUG_ON (itd->urb == urb);
	 */

	/* give urb back to the driver ... can be out-of-order */
	//dev = usb_get_dev (urb->dev);
	dev = urb->dev;
	ehci_urb_done (ehci, urb, regs);
	urb = 0;

	/* defer stopping schedule; completion can submit */
	ehci->periodic_sched--;
	if (unlikely (!ehci->periodic_sched))
		(void) disable_periodic (ehci);
	hcd_to_bus (&ehci->hcd)->bandwidth_isoc_reqs--;

	if (unlikely (list_empty (&stream->itd_list))) {
		hcd_to_bus (&ehci->hcd)->bandwidth_allocated
				-= stream->bandwidth;
		ehci_vdbg (ehci,
			"deschedule devp %s ep%d%s-iso\n",
			dev->devpath, stream->bEndpointAddress & 0x0f,
			(stream->bEndpointAddress & USB_DIR_IN) ? "in" : "out");
	}
	iso_stream_put (ehci, stream);
	//usb_put_dev (dev);

	return 1;
}

/*-------------------------------------------------------------------------*/

static int itd_submit (struct ehci_hcd *ehci, struct urb *urb, int mem_flags)
{
	int			status = -EINVAL;
	unsigned long		flags;
	struct ehci_iso_stream	*stream;

	/* Get iso_stream head */
	stream = iso_stream_find (ehci, urb);
	if (unlikely (stream == 0)) {
		ehci_dbg (ehci, "can't get iso stream\n");
		return -ENOMEM;
	}
	if (unlikely (urb->interval != stream->interval)) {
		ehci_dbg (ehci, "can't change iso interval %d --> %d\n",
			stream->interval, urb->interval);
		goto done;
	}

#ifdef EHCI_URB_TRACE
	ehci_dbg (ehci,
		"%s %s urb %p ep%d%s len %d, %d pkts %d uframes [%p]\n",
		__FUNCTION__, urb->dev->devpath, urb,
		usb_pipeendpoint (urb->pipe),
		usb_pipein (urb->pipe) ? "in" : "out",
		urb->transfer_buffer_length,
		urb->number_of_packets, urb->interval,
		stream);
#endif

	/* allocate ITDs w/o locking anything */
	status = itd_urb_transaction (stream, ehci, urb, mem_flags);
	if (unlikely (status < 0)) {
		ehci_dbg (ehci, "can't init itds\n");
		goto done;
	}

	/* schedule ... need to lock */
	spin_lock_irqsave (&ehci->lock, flags);
	status = itd_stream_schedule (ehci, urb, stream);
 	if (likely (status == 0))
		itd_link_urb (ehci, urb, ehci->periodic_size << 3, stream);
	spin_unlock_irqrestore (&ehci->lock, flags);

done:
	if (unlikely (status < 0))
		iso_stream_put (ehci, stream);
	return status;
}

#ifdef have_split_iso

/*-------------------------------------------------------------------------*/

/*
 * "Split ISO TDs" ... used for USB 1.1 devices going through
 * the TTs in USB 2.0 hubs.
 *
 * FIXME not yet implemented
 */

#endif /* have_split_iso */

/*-------------------------------------------------------------------------*/

static void
scan_periodic (struct ehci_hcd *ehci, struct pt_regs *regs)
{
	unsigned	frame, clock, now_uframe, mod;
	unsigned	modified;

	mod = ehci->periodic_size << 3;

	/*
	 * When running, scan from last scan point up to "now"
	 * else clean up by scanning everything that's left.
	 * Touches as few pages as possible:  cache-friendly.
	 */
	now_uframe = ehci->next_uframe;
	if (HCD_IS_RUNNING (ehci->hcd.state))
		clock = readl (&ehci->regs->frame_index);
	else
		clock = now_uframe + mod - 1;
	clock %= mod;

	for (;;) {
		union ehci_shadow	q, *q_p;
		u32			type, *hw_p;
		unsigned		uframes;

		/* don't scan past the live uframe */
		frame = now_uframe >> 3;
		if (frame == (clock >> 3))
			uframes = now_uframe & 0x07;
		else {
			/* safe to scan the whole frame at once */
			now_uframe |= 0x07;
			uframes = 8;
		}

restart:
		/* scan each element in frame's queue for completions */
		q_p = &ehci->pshadow [frame];
		hw_p = &ehci->periodic [frame];
		q.ptr = q_p->ptr;
		type = Q_NEXT_TYPE (*hw_p);
		modified = 0;

		while (q.ptr != 0) {
			unsigned		uf;
			union ehci_shadow	temp;

			switch (type) {
			case Q_TYPE_QH:
				/* handle any completions */
				temp.qh = qh_get (q.qh);
				type = Q_NEXT_TYPE (q.qh->hw_next);
				q = q.qh->qh_next;
				modified = qh_completions (ehci, temp.qh, regs);
				if (unlikely (list_empty (&temp.qh->qtd_list)))
					intr_deschedule (ehci, temp.qh, 0);
				qh_put (ehci, temp.qh);
				break;
			case Q_TYPE_FSTN:
				/* for "save place" FSTNs, look at QH entries
				 * in the previous frame for completions.
				 */
				if (q.fstn->hw_prev != EHCI_LIST_END) {
					dbg ("ignoring completions from FSTNs");
				}
				type = Q_NEXT_TYPE (q.fstn->hw_next);
				q = q.fstn->fstn_next;
				break;
			case Q_TYPE_ITD:
				/* skip itds for later in the frame */
				rmb ();
				for (uf = uframes; uf < 8; uf++) {
					if (0 == (q.itd->hw_transaction [uf]
							& ISO_ACTIVE))
						continue;
					q_p = &q.itd->itd_next;
					hw_p = &q.itd->hw_next;
					type = Q_NEXT_TYPE (q.itd->hw_next);
					q = *q_p;
					break;
				}
				if (uf != 8)
					break;

				/* this one's ready ... HC won't cache the
				 * pointer for much longer, if at all.
				 */
				*q_p = q.itd->itd_next;
				*hw_p = q.itd->hw_next;
				type = Q_NEXT_TYPE (q.itd->hw_next);
				wmb();
				modified = itd_complete (ehci, q.itd, regs);
				q = *q_p;
				break;
#ifdef have_split_iso
			case Q_TYPE_SITD:
				// nyet!
#endif /* have_split_iso */
			default:
				dbg ("corrupt type %d frame %d shadow %p",
					type, frame, q.ptr);
				// BUG ();
				q.ptr = 0;
			}

			/* assume completion callbacks modify the queue */
			if (unlikely (modified))
				goto restart;
		}

		/* stop when we catch up to the HC */

		// FIXME:  this assumes we won't get lapped when
		// latencies climb; that should be rare, but...
		// detect it, and just go all the way around.
		// FLR might help detect this case, so long as latencies
		// don't exceed periodic_size msec (default 1.024 sec).

		// FIXME:  likewise assumes HC doesn't halt mid-scan

		if (now_uframe == clock) {
			unsigned	now;

			if (!HCD_IS_RUNNING (ehci->hcd.state))
				break;
			ehci->next_uframe = now_uframe;
			now = readl (&ehci->regs->frame_index) % mod;
			if (now_uframe == now)
				break;

			/* rescan the rest of this frame, then ... */
			clock = now;
		} else {
			now_uframe++;
			now_uframe %= mod;
		}
	} 
}
