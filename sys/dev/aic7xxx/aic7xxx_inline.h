/*
 * Inline routines shareable across OS platforms.
 *
 * Copyright (c) 1994, 1995, 1996, 1997, 1998, 1999, 2000 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU Public License ("GPL").
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: //depot/src/aic7xxx/aic7xxx_inline.h#5 $
 *
 * $FreeBSD$
 */

#ifndef _AIC7XXX_INLINE_H_
#define _AIC7XXX_INLINE_H_

/************************* Sequencer Execution Control ************************/
static __inline int  sequencer_paused(struct ahc_softc *ahc);
static __inline void ahc_pause_bug_fix(struct ahc_softc *ahc);
static __inline void pause_sequencer(struct ahc_softc *ahc);
static __inline void unpause_sequencer(struct ahc_softc *ahc);

/*
 * Work around any chip bugs related to halting sequencer execution.
 * On Ultra2 controllers, we must clear the CIOBUS stretch signal by
 * reading a register that will set this signal and deassert it.
 * Without this workaround, if the chip is paused, by an interrupt or
 * manual pause while accessing scb ram, accesses to certain registers
 * will hang the system (infinite pci retries).
 */
static __inline void
ahc_pause_bug_fix(struct ahc_softc *ahc)
{
	if ((ahc->features & AHC_ULTRA2) != 0)
		(void)ahc_inb(ahc, CCSCBCTL);
}

/*
 * Determine whether the sequencer has halted code execution.
 * Returns non-zero status if the sequencer is stopped.
 */
static __inline int
sequencer_paused(struct ahc_softc *ahc)
{
	return ((ahc_inb(ahc, HCNTRL) & PAUSE) != 0);
}

/*
 * Request that the sequencer stop and wait, indefinitely, for it
 * to stop.  The sequencer will only acknowledge that it is paused
 * once it has reached an instruction boundary and PAUSEDIS is
 * cleared in the SEQCTL register.  The sequencer may use PAUSEDIS
 * for critical sections.
 */
static __inline void
pause_sequencer(struct ahc_softc *ahc)
{
	ahc_outb(ahc, HCNTRL, ahc->pause);

	/*
	 * Since the sequencer can disable pausing in a critical section, we
	 * must loop until it actually stops.
	 */
	while (sequencer_paused(ahc) == 0)
		;

	ahc_pause_bug_fix(ahc);
}

/*
 * Allow the sequencer to continue program execution.
 * We check here to ensure that no additional interrupt
 * sources that would cause the sequencer to halt have been
 * asserted.  If, for example, a SCSI bus reset is detected
 * while we are fielding a different, pausing, interrupt type,
 * we don't want to release the sequencer before going back
 * into our interrupt handler and dealing with this new
 * condition.
 */
static __inline void
unpause_sequencer(struct ahc_softc *ahc)
{
	if ((ahc_inb(ahc, INTSTAT) & (SCSIINT | SEQINT | BRKADRINT)) == 0)
		ahc_outb(ahc, HCNTRL, ahc->unpause);
}

/*********************** Untagged Transaction Routines ************************/
u_int			ahc_index_busy_tcl(struct ahc_softc *ahc,
					   u_int tcl, int unbusy);
static __inline void	ahc_freeze_untagged_queues(struct ahc_softc *ahc);
static __inline void	ahc_release_untagged_queues(struct ahc_softc *ahc);

/*
 * Block our completion routine from starting the next untagged
 * transaction for this target or target lun.
 */
static __inline void
ahc_freeze_untagged_queues(struct ahc_softc *ahc)
{
	if ((ahc->features & AHC_SCB_BTT) == 0)
		ahc->untagged_queue_lock++;
}

/*
 * Allow the next untagged transaction for this target or target lun
 * to be executed.  We use a counting semaphore to allow the lock
 * to be acquired recursively.  Once the count drops to zero, the
 * transaction queues will be run.
 */
static __inline void
ahc_release_untagged_queues(struct ahc_softc *ahc)
{
	if ((ahc->features & AHC_SCB_BTT) == 0) {
		ahc->untagged_queue_lock--;
		if (ahc->untagged_queue_lock == 0)
			ahc_run_untagged_queues(ahc);
	}
}

/************************** Memory mapping routines ***************************/
static __inline struct ahc_dma_seg *
			ahc_sg_bus_to_virt(struct scb *scb,
					   uint32_t sg_busaddr);
static __inline uint32_t
			ahc_sg_virt_to_bus(struct scb *scb,
					   struct ahc_dma_seg *sg);
static __inline uint32_t
			ahc_hscb_busaddr(struct ahc_softc *ahc, u_int index);

static __inline struct ahc_dma_seg *
ahc_sg_bus_to_virt(struct scb *scb, uint32_t sg_busaddr)
{
	int sg_index;

	sg_index = (sg_busaddr - scb->sg_list_phys)/sizeof(struct ahc_dma_seg);
	/* sg_list_phys points to entry 1, not 0 */
	sg_index++;

	return (&scb->sg_list[sg_index]);
}

static __inline uint32_t
ahc_sg_virt_to_bus(struct scb *scb, struct ahc_dma_seg *sg)
{
	int sg_index;

	/* sg_list_phys points to entry 1, not 0 */
	sg_index = sg - &scb->sg_list[1];

	return (scb->sg_list_phys + (sg_index * sizeof(*scb->sg_list)));
}

static __inline uint32_t
ahc_hscb_busaddr(struct ahc_softc *ahc, u_int index)
{
	return (ahc->scb_data->hscb_busaddr
		+ (sizeof(struct hardware_scb) * index));
}

/******************************** Debugging ***********************************/
static __inline char *ahc_name(struct ahc_softc *ahc);

static __inline char *
ahc_name(struct ahc_softc *ahc)
{
	return (ahc->name);
}

/*********************** Miscelaneous Support Functions ***********************/

static __inline int	ahc_check_residual(struct scb *scb);
static __inline struct ahc_initiator_tinfo *
			ahc_fetch_transinfo(struct ahc_softc *ahc,
					    char channel, u_int our_id,
					    u_int remote_id,
					    struct tmode_tstate **tstate);
static __inline struct scb*
			ahc_get_scb(struct ahc_softc *ahc);
static __inline void	ahc_free_scb(struct ahc_softc *ahc, struct scb *scb);
static __inline void	ahc_queue_scb(struct ahc_softc *ahc, struct scb *scb);

/*
 * Determine whether the sequencer reported a residual
 * for this SCB/transaction.
 */
static __inline int
ahc_check_residual(struct scb *scb)
{
	struct status_pkt *sp;

	sp = &scb->hscb->shared_data.status;
	if ((scb->hscb->sgptr & SG_RESID_VALID) != 0)
		return (1);
	return (0);
}

/*
 * Return pointers to the transfer negotiation information
 * for the specified our_id/remote_id pair.
 */
static __inline struct ahc_initiator_tinfo *
ahc_fetch_transinfo(struct ahc_softc *ahc, char channel, u_int our_id,
		    u_int remote_id, struct tmode_tstate **tstate)
{
	/*
	 * Transfer data structures are stored from the perspective
	 * of the target role.  Since the parameters for a connection
	 * in the initiator role to a given target are the same as
	 * when the roles are reversed, we pretend we are the target.
	 */
	if (channel == 'B')
		our_id += 8;
	*tstate = ahc->enabled_targets[our_id];
	return (&(*tstate)->transinfo[remote_id]);
}

/*
 * Get a free scb. If there are none, see if we can allocate a new SCB.
 */
static __inline struct scb *
ahc_get_scb(struct ahc_softc *ahc)
{
	struct scb *scb;

	if ((scb = SLIST_FIRST(&ahc->scb_data->free_scbs)) == NULL) {
		ahc_alloc_scbs(ahc);
		scb = SLIST_FIRST(&ahc->scb_data->free_scbs);
		if (scb == NULL)
			return (NULL);
	}
	SLIST_REMOVE_HEAD(&ahc->scb_data->free_scbs, links.sle);
	return (scb);
}

/*
 * Return an SCB resource to the free list.
 */
static __inline void
ahc_free_scb(struct ahc_softc *ahc, struct scb *scb)
{       
	struct hardware_scb *hscb;

	hscb = scb->hscb;
	/* Clean up for the next user */
	ahc->scb_data->scbindex[hscb->tag] = NULL;
	scb->flags = SCB_FREE;
	hscb->control = 0;

	SLIST_INSERT_HEAD(&ahc->scb_data->free_scbs, scb, links.sle);

	/* Notify the OSM that a resource is now available. */
	ahc_platform_scb_free(ahc, scb);
}

static __inline struct scb *
ahc_lookup_scb(struct ahc_softc *ahc, u_int tag)
{
	return (ahc->scb_data->scbindex[tag]);

}

/*
 * Tell the sequencer about a new transaction to execute.
 */
static __inline void
ahc_queue_scb(struct ahc_softc *ahc, struct scb *scb)
{
	struct hardware_scb *q_hscb;
	u_int  saved_tag;

	/*
	 * Our queuing method is a bit tricky.  The card
	 * knows in advance which HSCB to download, and we
	 * can't disappoint it.  To achieve this, the next
	 * SCB to download is saved off in ahc->next_queued_scb.
	 * When we are called to queue "an arbitrary scb",
	 * we copy the contents of the incoming HSCB to the one
	 * the sequencer knows about, swap HSCB pointers and
	 * finally assigne the SCB to the tag indexed location
	 * in the scb_array.  This makes sure that we can still
	 * locate the correct SCB by SCB_TAG.
	 *
	 * Start by copying the payload without perterbing
	 * the tag number.  Also set the hscb id for the next
	 * SCB to download.
	 */
	q_hscb = ahc->next_queued_scb->hscb;
	saved_tag = q_hscb->tag;
	memcpy(q_hscb, scb->hscb, sizeof(*scb->hscb));
	if ((scb->flags & SCB_CDB32_PTR) != 0) {
		q_hscb->shared_data.cdb_ptr =
		    ahc_hscb_busaddr(ahc, q_hscb->tag)
		  + offsetof(struct hardware_scb, cdb32);	
	}
	q_hscb->tag = saved_tag;
	q_hscb->next = scb->hscb->tag;

	/* Now swap HSCB pointers. */
	ahc->next_queued_scb->hscb = scb->hscb;
	scb->hscb = q_hscb;

	/* Now define the mapping from tag to SCB in the scbindex */
	ahc->scb_data->scbindex[scb->hscb->tag] = scb;

	if (scb->hscb->tag == SCB_LIST_NULL
	 || scb->hscb->next == SCB_LIST_NULL)
		panic("Attempt to queue invalid SCB tag %x:%x\n",
		      scb->hscb->tag, scb->hscb->next);

	/*
	 * Keep a history of SCBs we've downloaded in the qinfifo.
	 */
	ahc->qinfifo[ahc->qinfifonext++] = scb->hscb->tag;
	if ((ahc->features & AHC_QUEUE_REGS) != 0) {
		ahc_outb(ahc, HNSCB_QOFF, ahc->qinfifonext);
	} else {
		if ((ahc->features & AHC_AUTOPAUSE) == 0)
			pause_sequencer(ahc);
		ahc_outb(ahc, KERNEL_QINPOS, ahc->qinfifonext);
		if ((ahc->features & AHC_AUTOPAUSE) == 0)
			unpause_sequencer(ahc);
	}
}

/************************** Interrupt Processing ******************************/
static __inline void ahc_intr(struct ahc_softc *ahc);

/*
 * Catch an interrupt from the adapter
 */
static __inline void
ahc_intr(struct ahc_softc *ahc)
{
	u_int	intstat;

	intstat = ahc_inb(ahc, INTSTAT);

	/*
	 * Any interrupts to process?
	 */
#if AHC_PCI_CONFIG > 0
	if ((intstat & INT_PEND) == 0) {
		if ((ahc->chip & AHC_PCI) != 0
		 && (ahc->unsolicited_ints > 500)) {
			if ((ahc_inb(ahc, ERROR) & PCIERRSTAT) != 0)
				ahc_pci_intr(ahc);
			ahc->unsolicited_ints = 0;
		} else {
			ahc->unsolicited_ints++;
		}
		return;
	} else {
		ahc->unsolicited_ints = 0;
	}
#else
	if ((intstat & INT_PEND) == 0)
		return;
#endif

	if (intstat & CMDCMPLT) {
		ahc_outb(ahc, CLRINT, CLRCMDINT);
		/*
		 * Ensure that the chip sees that we've cleared
		 * this interrupt before we walk the output fifo.
		 * Otherwise, we may, due to posted bus writes,
		 * clear the interrupt after we finish the scan,
		 * and after the sequencer has added new entries
		 * and asserted the interrupt again.
		 */
		ahc_flush_device_writes(ahc);
		ahc_run_qoutfifo(ahc);
#ifdef AHC_TARGET_MODE
		if ((ahc->flags & AHC_TARGETROLE) != 0)
			ahc_run_tqinfifo(ahc, /*paused*/FALSE);
#endif
	}
	if (intstat & BRKADRINT)
		ahc_handle_brkadrint(ahc);

	if ((intstat & (SEQINT|SCSIINT)) != 0)
		ahc_pause_bug_fix(ahc);

	if ((intstat & SEQINT) != 0)
		ahc_handle_seqint(ahc, intstat);

	if ((intstat & SCSIINT) != 0)
		ahc_handle_scsiint(ahc, intstat);
}

#endif  /* _AIC7XXX_INLINE_H_ */
