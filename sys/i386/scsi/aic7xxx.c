/*
 * Generic driver for the aic7xxx based adaptec SCSI controllers
 * Product specific probe and attach routines can be found in:
 * i386/eisa/aic7770.c	27/284X and aic7770 motherboard controllers
 * pci/aic7870.c	3940, 2940, aic7870 and aic7850 controllers
 *
 * Copyright (c) 1994, 1995, 1996 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *      $Id: aic7xxx.c,v 1.29.2.15 1996/05/21 23:33:52 dima Exp $
 */
/*
 * TODO:
 *	Implement Target Mode
 *
 * A few notes on how SCB paging works...
 *
 * SCB paging takes advantage of the fact that devices stay disconnected
 * from the bus a relatively long time and that while they're disconnected,
 * having the SCBs for that device down on the host adapter is of little use.
 * Instead we copy the SCB back up into kernel memory and reuse the SCB slot
 * on the card to schedule another transaction.  This can be a real payoff
 * when doing random I/O to tagged queueing devices since there are more 
 * transactions active at once for the device to sort for optimal seek
 * reduction. The algorithm goes like this...
 *
 * At the sequencer level:
 * 1) Disconnected SCBs are threaded onto a doubly linked list, headed by
 *    DISCONNECTED_SCBH using the SCB_NEXT and SCB_PREV fields.  The most
 *    recently disconnected device is always at the head.
 *
 * 2) The SCB has an added field SCB_TAG that corresponds to the kernel
 *    SCB number (ie 0-254).
 *
 * 3) When a command is queued, the hardware index of the SCB it was downloaded
 *    into is placed into the QINFIFO for easy indexing by the sequencer.
 *
 * 4) The tag field is used as the tag for tagged-queueing, for determining
 *    the related kernel SCB, and is the value put into the QOUTFIFO
 *    so the kernel doesn't have to upload the SCB to determine the kernel SCB
 *    that completed on command completes.
 *
 * 5) When a reconnect occurs, the sequencer must scan the SCB array (even
 *    in the tag case) looking for the appropriate SCB and if it can't find
 *    it, it interrupts the kernel so it can page the SCB in.
 *
 * 6) If the sequencer is successful in finding the SCB, it removes it from
 *    the doubly linked list of disconnected SCBS.
 *
 * At the kernel level:
 * 1) There are four queues that a kernel SCB may reside on:
 *	free_scbs - SCBs that are not in use and have a hardware slot assigned
 *		    to them.
 *      page_scbs - SCBs that are not in use and need to have a hardware slot
 *		    assigned to them (i.e. they will most likely cause a page
 *		    out event).
 *	waiting_scbs - SCBs that are active, don't have an assigned hardware
 *		    slot assigned to them and are waiting for either a
 *		    disconnection or a command complete to free up a slot.
 *	assigned_scbs - SCBs that were in the waiting_scbs queue, but were
 *		    assigned a slot by ahc_free_scb.
 *
 * 2) When a new request comes in, an SCB is allocated from the free_scbs or
 *    page_scbs queue with preference to SCBs on the free_scbs queue.
 *
 * 3) If there are no free slots (we retrieved the SCB off of the page_scbs
 *    queue), the SCB is inserted onto the tail of the waiting_scbs list and
 *    we attempt to run this queue down.
 *
 * 4) ahc_run_waiing_queues() looks at both the assigned_scbs and waiting_scbs
 *    queues.  In the case of the assigned_scbs, the commands are immediately
 *    downloaded and started.  For waiting_scbs, we page in all that we can
 *    ensuring we don't create a resource deadlock (see comments in
 *    ahc_run_waing_queues()).
 *
 * 5) After we handle a bunch of command completes, we also try running the
 *    queues since many SCBs may have disconnected since the last command
 *    was started and we have at least one free slot on the card.
 *
 * 6) ahc_free_scb looks at the waiting_scbs queue for a transaction
 *    requiring a slot and moves it to the assigned_scbs queue if it
 *    finds one.  Otherwise it puts the current SCB onto the free_scbs
 *    queue for later use.
 *
 * 7) The driver handles page-in requests from the sequencer in response to
 *    the NO_MATCH sequencer interrupt.  For tagged commands, the approprite
 *    SCB is easily found since the tag is a direct index into our kernel SCB
 *    array.  For non-tagged commands, we keep a separate array of 16 pointers
 *    that point to the single possible SCB that was paged out for that target.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <machine/clock.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <i386/scsi/aic7xxx.h>

#include <dev/aic7xxx/aic7xxx_reg.h>

#include <sys/kernel.h>
#define KVTOPHYS(x)   vtophys(x)

#define MIN(a,b) ((a < b) ? a : b)
#define ALL_TARGETS -1

u_long ahc_unit = 0;

#ifdef AHC_DEBUG
static int     ahc_debug = AHC_SHOWSENSE;
#endif

/**** bit definitions for SCSIDEF ****/
#define	HSCSIID		0x07		/* our SCSI ID */
#define HWSCSIID	0x0f		/* our SCSI ID if Wide Bus */

static void	 ahcminphys __P((struct buf *bp));
static int32_t	 ahc_scsi_cmd __P((struct scsi_xfer *xs));

static struct scsi_adapter ahc_switch =
{
        ahc_scsi_cmd,
        ahcminphys,
        0,
        0,
        0,
        "ahc",
        { 0, 0 }
};

/* the below structure is so we have a default dev struct for our link struct */
static struct scsi_device ahc_dev =
{
    NULL,                       /* Use default error handler */
    NULL,                       /* have a queue, served by this */
    NULL,                       /* have no async handler */
    NULL,                       /* Use default 'done' routine */
    "ahc",
    0,
    { 0, 0 }
};

/*
 * Since the sequencer can disable pausing in a critical section, we
 * must loop until it actually stops.
 * XXX Should add a timeout in here??
 */
#define PAUSE_SEQUENCER(ahc)					\
	outb(HCNTRL + ahc->baseport, ahc->pause);		\
								\
	while ((inb(HCNTRL + ahc->baseport) & PAUSE) == 0)	\
		;

#define UNPAUSE_SEQUENCER(ahc)					\
	outb( HCNTRL + ahc->baseport, ahc->unpause )

/*
 * Restart the sequencer program from address zero
 */
#define RESTART_SEQUENCER(ahc)						\
	do {								\
		outb( SEQCTL + ahc->baseport, SEQRESET|FASTMODE );	\
	} while (inb(SEQADDR0 + ahc->baseport) != 0 &&			\
		 inb(SEQADDR1 + ahc->baseport) != 0);			\
									\
	UNPAUSE_SEQUENCER(ahc);

static u_char	ahc_abort_wscb __P((struct ahc_data *ahc, struct scb *scbp,
				    u_char prev, u_long iobase,
				    u_char timedout_scb, u_int32_t xs_error));
static void	ahc_add_waiting_scb __P((u_long iobase, struct scb *scb));
static void	ahc_done __P((struct ahc_data *ahc, struct scb *scbp));
static void	ahc_free_scb __P((struct ahc_data *ahc, struct scb *scb,
				  int flags));
static inline void ahc_send_scb __P((struct ahc_data *ahc, struct scb *scb));
static inline void ahc_fetch_scb __P((struct ahc_data *ahc, struct scb *scb));
static inline void ahc_page_scb __P((struct ahc_data *ahc, struct scb *out_scb,
				struct scb *in_scb));
static inline void ahc_run_waiting_queues __P((struct ahc_data *ahc));
static struct scb *
		ahc_get_scb __P((struct ahc_data *ahc, int flags));
static void	ahc_loadseq __P((u_long iobase));
static int	ahc_match_scb __P((struct scb *scb, int target, char channel));
static int	ahc_poll __P((struct ahc_data *ahc, int wait));
#ifdef AHC_DEBUG
static void	ahc_print_scb __P((struct scb *scb));
#endif
static int	ahc_reset_channel __P((struct ahc_data *ahc, char channel,
				       u_char timedout_scb, u_int32_t xs_error,
				       u_char initiate_reset));
static int	ahc_reset_device __P((struct ahc_data *ahc, int target,
				      char channel, u_char timedout_scb,
				      u_int32_t xs_error));
static void	ahc_reset_current_bus __P((u_long iobase));
static void	ahc_run_done_queue __P((struct ahc_data *ahc));
static void	ahc_scsirate __P((struct ahc_data* ahc, u_char *scsirate,
				  int period, int offset, char channel,
				  int target));
static timeout_t
		ahc_timeout;
static void	ahc_busy_target __P((int target, char channel,
				     u_long iobase));
static void	ahc_unbusy_target __P((int target, char channel,
				       u_long iobase));

#ifdef  AHC_DEBUG
static void
ahc_print_scb(scb)
        struct scb *scb;
{
        printf("scb:%p control:0x%x tcl:0x%x cmdlen:%d cmdpointer:0x%lx\n"
	    ,scb
	    ,scb->control
	    ,scb->tcl
	    ,scb->cmdlen
	    ,scb->cmdpointer );
        printf("        datlen:%d data:0x%lx segs:0x%x segp:0x%lx\n"
	    ,scb->datalen
	    ,scb->data
	    ,scb->SG_segment_count
	    ,scb->SG_list_pointer);
	printf("	sg_addr:%lx sg_len:%ld\n"
	    ,scb->ahc_dma[0].addr
	    ,scb->ahc_dma[0].len);
}

#endif

static struct {
        u_char errno;
	char *errmesg;
} hard_error[] = {
	{ ILLHADDR,  "Illegal Host Access" },
	{ ILLSADDR,  "Illegal Sequencer Address referrenced" },
	{ ILLOPCODE, "Illegal Opcode in sequencer program" },
	{ PARERR,    "Sequencer Ram Parity Error" }
};


/*
 * Valid SCSIRATE values.  (p. 3-17)
 * Provides a mapping of tranfer periods in ns to the proper value to
 * stick in the scsiscfr reg to use that transfer rate.
 */
static struct {
	short sxfr;
	/* Rates in Ultra mode have bit 8 of sxfr set */
#define		ULTRA_SXFR 0x100
	short period; /* in ns */
	char *rate;
} ahc_syncrates[] = {
	{ 0x100,  50, "20.0"  },
	{ 0x110,  62, "16.0"  },
	{ 0x120,  75, "13.4"  },
	{ 0x000, 100, "10.0"  },
	{ 0x010, 125,  "8.0"  },
	{ 0x020, 150,  "6.67" },
	{ 0x030, 175,  "5.7"  },
	{ 0x040, 200,  "5.0"  },
	{ 0x050, 225,  "4.4"  },
	{ 0x060, 250,  "4.0"  },
	{ 0x070, 275,  "3.6"  }
};

static int ahc_num_syncrates =
	sizeof(ahc_syncrates) / sizeof(ahc_syncrates[0]);

/*
 * Allocate a controller structures for a new device and initialize it.
 * ahc_reset should be called before now since we assume that the card
 * is paused.
 *
 */
struct ahc_data *
ahc_alloc(unit, iobase, type, flags)
	int unit;
	u_long iobase;
	ahc_type type;
	ahc_flag flags;
{

	/*
	 * find unit and check we have that many defined
	 */

	struct  ahc_data *ahc;

	/*
	 * Allocate a storage area for us
	 */

	ahc = malloc(sizeof(struct ahc_data), M_TEMP, M_NOWAIT);
	if (!ahc) {
		printf("ahc%d: cannot malloc!\n", unit);
		return NULL;
	}
	bzero(ahc, sizeof(struct ahc_data));
	STAILQ_INIT(&ahc->free_scbs);
	STAILQ_INIT(&ahc->page_scbs);
	STAILQ_INIT(&ahc->waiting_scbs);
	STAILQ_INIT(&ahc->assigned_scbs);
	ahc->unit = unit;
	ahc->baseport = iobase;
	ahc->type = type;
	ahc->flags = flags;
	ahc->unpause = (inb(HCNTRL + iobase) & IRQMS) | INTEN;
	ahc->pause = ahc->unpause | PAUSE;

	return (ahc);
}

void
ahc_free(ahc)
	struct ahc_data *ahc;
{
	free(ahc, M_DEVBUF);
	return;
}

void
ahc_reset(iobase)
	u_long iobase;
{
        u_char hcntrl;
	int wait;

	/* Retain the IRQ type accross the chip reset */
	hcntrl = (inb(HCNTRL + iobase) & IRQMS) | INTEN;

	outb(HCNTRL + iobase, CHIPRST | PAUSE);
	/*
	 * Ensure that the reset has finished
	 */
	wait = 1000;
	while (--wait && !(inb(HCNTRL + iobase) & CHIPRSTACK))
		DELAY(1000);
	if(wait == 0) {
		printf("ahc at 0x%lx: WARNING - Failed chip reset!  "
		       "Trying to initialize anyway.\n", iobase);
	}
	outb(HCNTRL + iobase, hcntrl | PAUSE);
}

/*
 * Look up the valid period to SCSIRATE conversion in our table.
 */
static void
ahc_scsirate(ahc, scsirate, period, offset, channel, target )
	struct	ahc_data *ahc;
	u_char	*scsirate;
	short	period;
	u_char	offset;
	char	channel;
	int	target;
{
	int i;

	for (i = 0; i < ahc_num_syncrates; i++) {
		u_char ultra_enb;
		u_char sxfrctl0;
		u_long ultra_enb_addr;

		if ((ahc_syncrates[i].period - period) >= 0) {
			/*
			 * Watch out for Ultra speeds when ultra is not
			 * enabled and vice-versa.
			 */
			if(!(ahc->type & AHC_ULTRA) 
			 && (ahc_syncrates[i].sxfr & ULTRA_SXFR)) {
				/*
				 * This should only happen if the
				 * drive is the first to negotiate
				 * and chooses a high rate.  We'll
				 * just move down the table util
				 * we hit a non ultra speed.
				 */
				continue;
			}
			*scsirate = (ahc_syncrates[i].sxfr) | (offset & 0x0f);
			/*
			 * Ensure Ultra mode is set properly for
			 * this target.
			 */

			ultra_enb_addr = ahc->baseport + ULTRA_ENB;
			if(channel == 'B' || target > 7)
				ultra_enb_addr++;
			ultra_enb = inb(ultra_enb_addr);	
			sxfrctl0 = inb(SXFRCTL0 + ahc->baseport);
			if (ahc_syncrates[i].sxfr & ULTRA_SXFR) {
				ultra_enb |= 0x01 << (target & 0x07);
				sxfrctl0 |= ULTRAEN;
			}
			else {
				ultra_enb &= ~(0x01 << (target & 0x07));
				sxfrctl0 &= ~ULTRAEN;
			}
			outb(ultra_enb_addr, ultra_enb);
			outb(SXFRCTL0 + ahc->baseport, sxfrctl0);
			
			if(bootverbose) {
				printf("ahc%d: target %d synchronous at %sMHz,"
				       " offset = 0x%x\n", ahc->unit, target,
					ahc_syncrates[i].rate, offset );
			}
			return;
		}
	}
	/* Default to asyncronous transfers.  Also reject this SDTR request. */
	*scsirate = 0;
	if(bootverbose) {
		printf("ahc%d: target %d using asyncronous transfers\n",
			ahc->unit, target );
	}
}


/*
 * Attach all the sub-devices we can find
 */
int
ahc_attach(ahc)
	struct ahc_data *ahc;
{
	struct scsibus_data *scbus;

	/*
	 * fill in the prototype scsi_links.
	 */
	ahc->sc_link.adapter_unit = ahc->unit;
	ahc->sc_link.adapter_targ = ahc->our_id;
	ahc->sc_link.adapter_softc = ahc;
	ahc->sc_link.adapter = &ahc_switch;
	ahc->sc_link.opennings = 2;
	ahc->sc_link.device = &ahc_dev;
	ahc->sc_link.flags = DEBUGLEVEL;
	ahc->sc_link.fordriver = 0;

	if(ahc->type & AHC_TWIN) {
		/* Configure the second scsi bus */
		ahc->sc_link_b = ahc->sc_link;
		ahc->sc_link_b.adapter_targ = ahc->our_id_b;
		ahc->sc_link_b.adapter_bus = 1;
		ahc->sc_link_b.fordriver = (void *)SELBUSB;
	}

	/*
	 * Prepare the scsibus_data area for the upperlevel
	 * scsi code.
	 */
	scbus = scsi_alloc_bus();
	if(!scbus) 
		return 0;
	scbus->adapter_link = (ahc->flags & AHC_CHANNEL_B_PRIMARY) ?
				&ahc->sc_link_b : &ahc->sc_link;
	if(ahc->type & AHC_WIDE)
		scbus->maxtarg = 15;
	
	/*
	 * ask the adapter what subunits are present
	 */
	if(bootverbose)
		printf("ahc%d: Probing channel %c\n", ahc->unit,
			(ahc->flags & AHC_CHANNEL_B_PRIMARY) ? 'B' : 'A');
	scsi_attachdevs(scbus);
	scbus = NULL;	/* Upper-level SCSI code owns this now */

	if(ahc->type & AHC_TWIN) {
		scbus =  scsi_alloc_bus();
		if(!scbus) 
			return 0;
		scbus->adapter_link = (ahc->flags & AHC_CHANNEL_B_PRIMARY) ? 
					&ahc->sc_link : &ahc->sc_link_b;
		if(ahc->type & AHC_WIDE)
			scbus->maxtarg = 15;
		if(bootverbose)
			printf("ahc%d: Probing Channel %c\n", ahc->unit,
			       (ahc->flags & AHC_CHANNEL_B_PRIMARY) ? 'A': 'B');
		scsi_attachdevs(scbus);
		scbus = NULL;	/* Upper-level SCSI code owns this now */
	}
	return 1;
}

/*
 * Send an SCB down to the card via PIO.
 * We assume that the proper SCB is already selected in SCBPTR. 
 */
static inline void
ahc_send_scb(ahc, scb)
        struct	ahc_data *ahc;
        struct	scb *scb;
{
	u_long iobase = ahc->baseport;

	outb(SCBCNT + iobase, SCBAUTO);
	if( ahc->type == AHC_284 )
		/* Can only do 8bit PIO */
		outsb(SCBARRAY+iobase, scb, SCB_PIO_TRANSFER_SIZE);
	else
		outsl(SCBARRAY+iobase, scb, 
		      (SCB_PIO_TRANSFER_SIZE + 3) / 4);
	outb(SCBCNT + iobase, 0); 
}

/*
 * Retrieve an SCB from the card via PIO.
 * We assume that the proper SCB is already selected in SCBPTR.
 */
static inline void
ahc_fetch_scb(ahc, scb)
	struct	ahc_data *ahc;
	struct	scb *scb;
{
	u_long	iobase = ahc->baseport;

	outb(SCBCNT + iobase, 0x80);     /* SCBAUTO */

	/* Can only do 8bit PIO for reads */
	insb(SCBARRAY+iobase, scb, SCB_PIO_TRANSFER_SIZE);

	outb(SCBCNT + iobase, 0);
}

/*
 * Swap in_scbp for out_scbp down in the cards SCB array.
 * We assume that the SCB for out_scbp is already selected in SCBPTR.
 */
static inline void
ahc_page_scb(ahc, out_scbp, in_scbp)
	struct ahc_data *ahc;
	struct scb *out_scbp;
	struct scb *in_scbp;
{
	/* Page-out */
	ahc_fetch_scb(ahc, out_scbp);
	out_scbp->flags |= SCB_PAGED_OUT;
	if(!(out_scbp->control & TAG_ENB))
	{
		/* Stick in non-tagged array */
		int index =  (out_scbp->tcl >> 4)
			   | (out_scbp->tcl & SELBUSB);
		ahc->pagedout_ntscbs[index] = out_scbp;
	}

	/* Page-in */
	in_scbp->position = out_scbp->position;
	out_scbp->position = SCB_LIST_NULL;
	ahc_send_scb(ahc, in_scbp);
	in_scbp->flags &= ~SCB_PAGED_OUT;
}

static inline void
ahc_run_waiting_queues(ahc)
	struct ahc_data *ahc;
{
	struct scb* scb;
	u_char cur_scb;
	u_long iobase = ahc->baseport;

	if(!(ahc->assigned_scbs.stqh_first || ahc->waiting_scbs.stqh_first))
		return;

	PAUSE_SEQUENCER(ahc);
	cur_scb = inb(SCBPTR + iobase);

	/*
	 * First handle SCBs that are waiting but have been
	 * assigned a slot.
	 */
	while((scb = ahc->assigned_scbs.stqh_first) != NULL) {
		STAILQ_REMOVE_HEAD(&ahc->assigned_scbs, links);
		outb(SCBPTR + iobase, scb->position);
		ahc_send_scb(ahc, scb);

		/* Mark this as an active command */
		scb->flags = SCB_ACTIVE;

		outb(QINFIFO + iobase, scb->position);
		if (!(scb->xs->flags & SCSI_NOMASK)) {
			timeout(ahc_timeout, (caddr_t)scb,
				(scb->xs->timeout * hz) / 1000);
		}
		SC_DEBUG(scb->xs->sc_link, SDEV_DB3, ("cmd_sent\n"));
	}
	/* Now deal with SCBs that require paging */
	if((scb = ahc->waiting_scbs.stqh_first) != NULL) {
		u_char disc_scb = inb(DISCONNECTED_SCBH + iobase);
		u_char active = inb(FLAGS+iobase) & (SELECTED|IDENTIFY_SEEN);
		int count = 0;

		do {
			u_char next_scb;

			/* Attempt to page this SCB in */
			if(disc_scb == SCB_LIST_NULL)
				break;

			/*
			 * Advance disc_scb to the next on in the
			 * list.
			 */
			outb(SCBPTR + iobase, disc_scb);
			next_scb = inb(SCB_NEXT + iobase); 

			/*
			 * We have to be careful about when we allow
			 * an SCB to be paged out.  There must always
			 * be at least one slot availible for a
			 * reconnecting target in case it references
			 * an SCB that has been paged out.  Our
			 * heuristic is that either the disconnected
			 * list has at least two entries in it or
			 * there is one entry and the sequencer is
			 * activily working on an SCB which implies that
			 * it will either complete or disconnect before
			 * another reconnection can occur.
			 */
			if((next_scb != SCB_LIST_NULL) || active)
			{
				u_char out_scbi;
				struct scb* out_scbp;

				STAILQ_REMOVE_HEAD(&ahc->waiting_scbs, links);

				/*
				 * Find the in-core SCB for the one
				 * we're paging out.
				 */
				out_scbi = inb(SCB_TAG + iobase); 
				out_scbp = ahc->scbarray[out_scbi];

				/* Do the page out */
				ahc_page_scb(ahc, out_scbp, scb);

				/* Mark this as an active command */
				scb->flags = SCB_ACTIVE;

				/* Queue the command */
				outb(QINFIFO + iobase, scb->position);
				if (!(scb->xs->flags & SCSI_NOMASK)) {
					timeout(ahc_timeout, (caddr_t)scb,
						(scb->xs->timeout * hz) / 1000);
				}
				SC_DEBUG(scb->xs->sc_link, SDEV_DB3,
					("cmd_paged-in\n"));
				count++;

				/* Advance to the next disconnected SCB */
				disc_scb = next_scb;
			}
			else
				break;
		} while((scb = ahc->waiting_scbs.stqh_first) != NULL);

		if(count) {
			/* 
			 * Update the head of the disconnected list.
			 */
			outb(DISCONNECTED_SCBH + iobase, disc_scb);
			if(disc_scb != SCB_LIST_NULL) {
				outb(SCBPTR + iobase, disc_scb);
				outb(SCB_PREV + iobase, SCB_LIST_NULL);
			}
		}
	}
	/* Restore old position */
	outb(SCBPTR + iobase, cur_scb);
	UNPAUSE_SEQUENCER(ahc);
}

/*
 * Add this SCB to the head of the "waiting for selection" list.
 */
static
void ahc_add_waiting_scb (iobase, scb)
	u_long iobase;
	struct scb *scb;
{
	u_char next; 
	u_char curscb;

	curscb = inb(SCBPTR + iobase);
	next = inb(WAITING_SCBH + iobase);

	outb(SCBPTR+iobase, scb->position);
	outb(SCB_NEXT+iobase, next);
	outb(WAITING_SCBH + iobase, scb->position);

	outb(SCBPTR + iobase, curscb);
}

/*
 * Catch an interrupt from the adapter
 */
void
ahc_intr(arg)
        void *arg;
{
	int     intstat;
	u_char	status;
        u_long	iobase;
	struct scb *scb = NULL;
	struct scsi_xfer *xs = NULL;
	struct ahc_data *ahc = (struct ahc_data *)arg;

	iobase = ahc->baseport;
        intstat = inb(INTSTAT + iobase);
	/*
	 * Is this interrupt for me? or for
	 * someone who is sharing my interrupt
	 */
	if (!(intstat & INT_PEND))
		return;

        if (intstat & BRKADRINT) {
		/* We upset the sequencer :-( */

		/* Lookup the error message */
		int i, error = inb(ERROR + iobase);
		int num_errors =  sizeof(hard_error)/sizeof(hard_error[0]);
		for(i = 0; error != 1 && i < num_errors; i++)
			error >>= 1;
                panic("ahc%d: brkadrint, %s at seqaddr = 0x%x\n",
		      ahc->unit, hard_error[i].errmesg,
		      (inb(SEQADDR1 + iobase) << 8) |
		      inb(SEQADDR0 + iobase));
        }
        if (intstat & SEQINT) { 
		/*
		 * This code isn't used by the SCB page-in code.  It
		 * should probably be moved to cut out the extra
		 * inb.
		 */
		u_short targ_mask;
		u_char target = (inb(SCSIID + iobase) >> 4) & 0x0f;
		u_char scratch_offset = target;
		char channel = 
			inb(SBLKCTL + iobase) & SELBUSB ? 'B': 'A';

		if (channel == 'B')
			scratch_offset += 8;
		targ_mask = (0x01 << scratch_offset); 
		
                switch (intstat & SEQINT_MASK) {
                    case BAD_PHASE:
                        printf("ahc%d:%c:%d: unknown scsi bus phase.  "
			      "Attempting to continue\n",
			      ahc->unit, channel, target);  
                        break; 
                    case SEND_REJECT: 
			{
				u_char rejbyte = inb(REJBYTE + iobase);
				if(( rejbyte & 0xf0) == 0x20) {
					/* Tagged Message */
					printf("\nahc%d:%c:%d: Tagged message "
						"received without identify. "
						"Disabling tagged commands "
						"for this target.\n", 
						ahc->unit, channel, target);
					ahc->tagenable &= ~targ_mask;
				}
				else
					printf("ahc%d:%c:%d: Warning - "
					       "unknown message recieved from "
					       "target (0x%x - 0x%x).  Rejecting\n", 
						ahc->unit, channel, target,
						rejbyte, inb(REJBYTE_EXT + iobase));
				break; 
			}
                    case NO_IDENT: 
                        panic("ahc%d:%c:%d: Target did not send an IDENTIFY "
			      "message. SAVED_TCL == 0x%x\n",
                              ahc->unit, channel, target,
			      inb(SAVED_TCL + iobase));
			break;
                    case NO_MATCH:
			if(ahc->flags & AHC_PAGESCBS) {
				/* SCB Page-in request */
				u_char tag;
				u_char next;
				u_char disc_scb;
				struct scb *outscb;
				u_char arg_1 = inb(ARG_1 + iobase);
				if(arg_1 == SCB_LIST_NULL) {
					/* Non-tagged command */
					int index = target |
						(channel == 'B' ? SELBUSB : 0);
					scb = ahc->pagedout_ntscbs[index];
				}
				else
					scb = ahc->scbarray[arg_1];

				/*
				 * Now to pick the SCB to page out.
				 * Either take a free SCB, an assigned SCB,
				 * an SCB that just completed, the first
				 * one on the disconnected SCB list, or
				 * as a last resort a queued SCB.
				 */
				if(ahc->free_scbs.stqh_first) {
					outscb = ahc->free_scbs.stqh_first; 
					STAILQ_REMOVE_HEAD(&ahc->free_scbs,
							   links);
					scb->position = outscb->position;
					outscb->position = SCB_LIST_NULL;
					STAILQ_INSERT_HEAD(&ahc->page_scbs,
							   outscb, links);
					outb(SCBPTR + iobase, scb->position);
					ahc_send_scb(ahc, scb);
					scb->flags &= ~SCB_PAGED_OUT;
					goto pagein_done;
				}
				if(ahc->assigned_scbs.stqh_first) {
					outscb = ahc->assigned_scbs.stqh_first; 
					STAILQ_REMOVE_HEAD(&ahc->assigned_scbs,
							   links);
					scb->position = outscb->position;
					outscb->position = SCB_LIST_NULL;
					STAILQ_INSERT_HEAD(&ahc->waiting_scbs,
							   outscb, links);
					outscb->flags = SCB_WAITINGQ;
					outb(SCBPTR + iobase, scb->position);
					ahc_send_scb(ahc, scb);
					scb->flags &= ~SCB_PAGED_OUT;
					goto pagein_done;
				}
				if(intstat & CMDCMPLT) {
					int   scb_index;

					outb(CLRINT + iobase, CLRCMDINT);
					scb_index = inb(QOUTFIFO + iobase);
					if(!(inb(QOUTCNT + iobase) & ahc->qcntmask))
						intstat &= ~CMDCMPLT;

					outscb = ahc->scbarray[scb_index];
					if (!outscb || !(outscb->flags & SCB_ACTIVE)) {
						printf("ahc%d: WARNING "
						       "no command for scb %d (cmdcmplt)\n",
							ahc->unit, scb_index );
						/* Fall through in hopes of finding another SCB */
					}
					else {
						scb->position = outscb->position;
						outscb->position = SCB_LIST_NULL;
						outb(SCBPTR + iobase, scb->position);
						ahc_send_scb(ahc, scb);
						scb->flags &= ~SCB_PAGED_OUT;
						untimeout(ahc_timeout, (caddr_t)outscb);
						ahc_done(ahc, outscb);
						goto pagein_done;
					}
				}
				disc_scb = inb(DISCONNECTED_SCBH + iobase);
				if(disc_scb != SCB_LIST_NULL) {
					outb(SCBPTR + iobase, disc_scb);
					tag = inb(SCB_TAG + iobase); 
					outscb = ahc->scbarray[tag];
					next = inb(SCB_NEXT + iobase);
					if(next != SCB_LIST_NULL) {
						outb(SCBPTR + iobase, next);
						outb(SCB_PREV + iobase,
						     SCB_LIST_NULL);
						outb(SCBPTR + iobase, disc_scb);
					}
					outb(DISCONNECTED_SCBH + iobase, next);
					ahc_page_scb(ahc, outscb, scb);
				}
				else if(inb(QINCNT + iobase) & ahc->qcntmask) {
					/* Pull one of our queued commands as a last resort */
					disc_scb = inb(QINFIFO + iobase);
					outb(SCBPTR + iobase, disc_scb);
					tag = inb(SCB_TAG + iobase);
					outscb = ahc->scbarray[tag];
					if((outscb->control & 0x23) != TAG_ENB) {
						/*
						 * This is not a simple tagged command
						 * so its position in the queue
						 * matters.  Take the command at the
						 * end of the queue instead.
						 */
						int i;
						u_char saved_queue[AHC_SCB_MAX];
						u_char queued = inb(QINCNT + iobase) & ahc->qcntmask;

						/* Count the command we removed already */
						saved_queue[0] = disc_scb;
						queued++;

						/* Empty the input queue */
						for (i = 1; i < queued; i++) 
							saved_queue[i] = inb(QINFIFO + iobase);

						/* Put everyone back put the last entry */
						queued--;
						for (i = 0; i < queued; i++)
							outb (QINFIFO + iobase, saved_queue[i]);

						outb(SCBPTR + iobase, saved_queue[queued]);
						tag = inb(SCB_TAG + iobase);
						outscb = ahc->scbarray[tag];
					}	
					untimeout(ahc_timeout, (caddr_t)outscb);
					scb->position = outscb->position;
					outscb->position = SCB_LIST_NULL;
					STAILQ_INSERT_HEAD(&ahc->waiting_scbs,
							   outscb, links);
					outscb->flags = SCB_WAITINGQ;
					ahc_send_scb(ahc, scb);
					scb->flags &= ~SCB_PAGED_OUT;
				}
				else
					panic("Page-in request with no candidates");
pagein_done:
				outb(RETURN_1 + iobase, SCB_PAGEDIN);
			}
			else {
				printf("ahc%d:%c:%d: no active SCB for "
				       "reconnecting target - "
				       "issuing ABORT\n", ahc->unit, channel, 
				       target);
				printf("SAVED_TCL == 0x%x\n",
					inb(SAVED_TCL + iobase));
				ahc_unbusy_target(target, channel, iobase);
				outb(SCB_CONTROL + iobase, 0);
				outb(CLRSINT1 + iobase, CLRSELTIMEO);
				outb(RETURN_1 + iobase, 0);
			}
			break;
                    case SDTR_MSG:
			{
				short period;
				u_char offset, rate;
				u_char targ_scratch;
				u_char maxoffset;
	                        /* 
				 * Help the sequencer to translate the 
				 * negotiated transfer rate.  Transfer is 
				 * 1/4 the period in ns as is returned by 
				 * the sync negotiation message.  So, we must 
				 * multiply by four
				 */
	                        period = inb(ARG_1 + iobase) << 2;
				offset = inb(ACCUM + iobase);
				targ_scratch = inb(TARG_SCRATCH + iobase 
						   + scratch_offset);
				if(targ_scratch & WIDEXFER)
					maxoffset = 0x08;
				else
					maxoffset = 0x0f;
				ahc_scsirate(ahc, &rate, period,
					     MIN(offset, maxoffset),
					     channel, target);
				/* Preserve the WideXfer flag */
				targ_scratch = rate | (targ_scratch & WIDEXFER);
				outb(TARG_SCRATCH + iobase + scratch_offset,
				     targ_scratch);
				outb(SCSIRATE + iobase, targ_scratch); 
				if( (targ_scratch & 0x0f) == 0 ) 
				{
					/*
					 * The requested rate was so low
					 * that asyncronous transfers are
					 * faster (not to mention the
					 * controller won't support them),
					 * so we issue a message reject to
					 * ensure we go to asyncronous
					 * transfers.
					 */
					outb(RETURN_1 + iobase, SEND_REJ);
				}
				/* See if we initiated Sync Negotiation */
				else if(ahc->sdtrpending & targ_mask)
				{
					/*
					 * Don't send an SDTR back to
					 * the target
					 */
					outb(RETURN_1 + iobase, 0);
				}
				else{
					/*
					 * Send our own SDTR in reply
					 */
#ifdef AHC_DEBUG
					if(ahc_debug & AHC_SHOWMISC)
						printf("Sending SDTR!!\n");
#endif
					outb(RETURN_1 + iobase, SEND_SDTR);
				}
				/*
				 * Negate the flags
				 */
				ahc->needsdtr &= ~targ_mask;
				ahc->sdtrpending &= ~targ_mask;
	                        break;
			}
                    case WDTR_MSG:
			{
				u_char scratch, bus_width;

				bus_width = inb(ARG_1 + iobase);

				scratch = inb(TARG_SCRATCH + iobase 
					      + scratch_offset);

				if(ahc->wdtrpending & targ_mask)
				{
					/*
					 * Don't send a WDTR back to the
					 * target, since we asked first.
					 */
					outb(RETURN_1 + iobase, 0);
					switch(bus_width)
					{
						case BUS_8_BIT:
						    scratch &= 0x7f;
						    break;
						case BUS_16_BIT:
						    if(bootverbose)
		        				printf("ahc%d: target "
							       "%d using 16Bit "
							       "transfers\n",
								ahc->unit,
								target);
						    scratch |= 0x80;	
						    break;
						case BUS_32_BIT:
						    /*
						     * How can we do 32bit
						     * transfers on a 16bit
						     * bus?
						     */
						    outb(RETURN_1 + iobase,
							 SEND_REJ);
		        			    printf("ahc%d: target "
						           "%d requested 32Bit "
						           "transfers.  "
							   "Rejecting...\n",
							   ahc->unit, target);
						    break;
						default:
						    break;
					}
				}
				else {
					/*
					 * Send our own WDTR in reply
					 */
					switch(bus_width)
					{
						case BUS_8_BIT:
							scratch &= 0x7f;
							break;
						case BUS_32_BIT:
						case BUS_16_BIT:
						    if(ahc->type & AHC_WIDE) {
							/* Negotiate 16_BITS */
							bus_width = BUS_16_BIT;
							if(bootverbose)
							    printf("ahc%d: "
								"target %d "
								"using 16Bit "
							        "transfers\n",
								ahc->unit,
								target);
						 	scratch |= 0x80;	
						    }
						    else
							bus_width = BUS_8_BIT;
						    break;
						default:
						    break;
					}
					outb(RETURN_1 + iobase,
						bus_width | SEND_WDTR);
				}
				ahc->needwdtr &= ~targ_mask;
				ahc->wdtrpending &= ~targ_mask;
				outb(TARG_SCRATCH + iobase + scratch_offset, 
				     scratch);
				outb(SCSIRATE + iobase, scratch); 
	                        break;
			}
		    case REJECT_MSG:
			{
				/*
				 * What we care about here is if we had an
				 * outstanding SDTR or WDTR message for this
				 * target.  If we did, this is a signal that
				 * the target is refusing negotiation.
				 */

				u_char targ_scratch;

				targ_scratch = inb(TARG_SCRATCH + iobase
						   + scratch_offset);

				if(ahc->wdtrpending & targ_mask){
					/* note 8bit xfers and clear flag */
					targ_scratch &= 0x7f;
					ahc->needwdtr &= ~targ_mask;
					ahc->wdtrpending &= ~targ_mask;
        				printf("ahc%d:%c:%d: refuses "
					       "WIDE negotiation.  Using "
					       "8bit transfers\n",
						ahc->unit, channel, target);
				}
				else if(ahc->sdtrpending & targ_mask){
					/* note asynch xfers and clear flag */
					targ_scratch &= 0xf0;
					ahc->needsdtr &= ~targ_mask;
					ahc->sdtrpending &= ~targ_mask;
        				printf("ahc%d:%c:%d: refuses "
					       "syncronous negotiation.  Using "
					       "asyncronous transfers\n",
						ahc->unit, channel, target);
				}
				else {
					/*
					 * Otherwise, we ignore it.
					 */
#ifdef AHC_DEBUG
					if(ahc_debug & AHC_SHOWMISC)
						printf("ahc%d:%c:%d: Message 
							reject -- ignored\n",
							ahc->unit, channel,
							target);
#endif
					break;
				}
				outb(TARG_SCRATCH + iobase + scratch_offset,
				     targ_scratch);
				outb(SCSIRATE + iobase, targ_scratch);
				break;
			}
                    case BAD_STATUS:
			{
			  int	scb_index;

			  /* The sequencer will notify us when a command
			   * has an error that would be of interest to
			   * the kernel.  This allows us to leave the sequencer
			   * running in the common case of command completes
			   * without error.
			   */

  			  scb_index = inb(SCB_TAG + iobase);
			  scb = ahc->scbarray[scb_index];

			  /*
			   * Set the default return value to 0 (don't
			   * send sense).  The sense code will change
			   * this if needed and this reduces code
			   * duplication.
			   */
			  outb(RETURN_1 + iobase, 0);
		 	  if (!(scb && (scb->flags & SCB_ACTIVE))) {
				printf("ahc%d:%c:%d: ahc_intr - referenced scb "
				       "not valid during seqint 0x%x scb(%d)\n",
				       ahc->unit, channel, target, intstat,
				       scb_index);
			      goto clear;
			  }

			  xs = scb->xs;

			  scb->status = inb(SCB_TARGET_STATUS + iobase);

#ifdef AHC_DEBUG
			  if((ahc_debug & AHC_SHOWSCBS)
			    && xs->sc_link->target == DEBUGTARG)
				ahc_print_scb(scb);
#endif
			  xs->status = scb->status;
			  switch(scb->status){
			    case SCSI_OK:
				printf("ahc%d: Interrupted for staus of"
					" 0???\n", ahc->unit);
				break;
			    case SCSI_CHECK:
#ifdef AHC_DEBUG
				if(ahc_debug & AHC_SHOWSENSE)
				{
					sc_print_addr(xs->sc_link);
					printf("requests Check Status\n");
				}
#endif

				if((xs->error == XS_NOERROR) &&
				    !(scb->flags & SCB_SENSE)) {
					struct ahc_dma_seg *sg = scb->ahc_dma;
					struct scsi_sense *sc = &(scb->sense_cmd);
#ifdef AHC_DEBUG
					if(ahc_debug & AHC_SHOWSENSE)
					{
						sc_print_addr(xs->sc_link);
						printf("Sending Sense\n");
					}
#endif
					sc->op_code = REQUEST_SENSE;
					sc->byte2 =  xs->sc_link->lun << 5;
					sc->length = sizeof(struct scsi_sense_data);
					sc->control = 0;

					sg->addr = KVTOPHYS(&xs->sense);
					sg->len = sizeof(struct scsi_sense_data);

					scb->control &= DISCENB;
					scb->status = 0;
					scb->SG_segment_count = 1;
					scb->SG_list_pointer = KVTOPHYS(sg);
			                scb->data = sg->addr; 
					scb->datalen = sg->len;
					scb->cmdpointer = KVTOPHYS(sc);
					scb->cmdlen = sizeof(*sc);

					scb->flags |= SCB_SENSE;
					ahc_send_scb(ahc, scb);
					/*
					 * Ensure that the target is "BUSY"
					 * so we don't get overlapping 
					 * commands if we happen to be doing
					 * tagged I/O.
					 */
					ahc_busy_target(target,channel,iobase);

					/*
					 * Make us the next command to run
					 */
					ahc_add_waiting_scb(iobase, scb);
					outb(RETURN_1 + iobase, SEND_SENSE);
					break;
				}
				/*
				 * Clear the SCB_SENSE Flag and have
				 * the sequencer do a normal command
				 * complete with either a "DRIVER_STUFFUP"
				 * error or whatever other error condition
				 * we already had.
				 */
				scb->flags &= ~SCB_SENSE;
				if(xs->error == XS_NOERROR)
					xs->error = XS_DRIVER_STUFFUP;
				break;
			    case SCSI_BUSY:
				xs->error = XS_BUSY;
				sc_print_addr(xs->sc_link);
				printf("Target Busy\n");
				break;
			    case SCSI_QUEUE_FULL:
				/*
				 * The upper level SCSI code will eventually
				 * handle this properly.
				 */
				sc_print_addr(xs->sc_link);
				printf("Queue Full\n");
				scb->flags = SCB_ASSIGNEDQ;
				STAILQ_INSERT_TAIL(&ahc->assigned_scbs,
						   scb, links);
				break;
			    default:
				sc_print_addr(xs->sc_link);
				printf("unexpected targ_status: %x\n",
					scb->status);
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
			break;
		  }
		  case RESIDUAL:
		  {
			int   scb_index;
			scb_index = inb(SCB_TAG + iobase);
			scb = ahc->scbarray[scb_index];
			xs = scb->xs;
			/*
			 * Don't clobber valid resid info with
			 * a resid coming from a check sense
			 * operation.
			 */
			if(!(scb->flags & SCB_SENSE)) {
				int resid_sgs;

				/*
				 * Remainder of the SG where the transfer
				 * stopped.
				 */
				xs->resid =
					(inb(iobase+SCB_RESID_DCNT2)<<16) |
					(inb(iobase+SCB_RESID_DCNT1)<<8)  |
					 inb(iobase+SCB_RESID_DCNT0);

				/*
				 * Add up the contents of all residual
				 * SG segments that are after the SG where
				 * the transfer stopped.
				 */
				resid_sgs = inb(SCB_RESID_SGCNT + iobase) - 1;
				while(resid_sgs > 0) {
					int sg;

					sg = scb->SG_segment_count - resid_sgs;
					xs->resid += scb->ahc_dma[sg].len;
					resid_sgs--;
				}

				xs->flags |= SCSI_RESID_VALID;
#ifdef AHC_DEBUG
				if(ahc_debug & AHC_SHOWMISC) {
					sc_print_addr(xs->sc_link);
					printf("Handled Residual of %ld bytes\n"
						,xs->resid);
				}
#endif
			}
			break;
		  }
		  case ABORT_TAG:
		  {
			int   scb_index;
			scb_index = inb(SCB_TAG + iobase);
			scb = ahc->scbarray[scb_index];
			xs = scb->xs;
			/*
			 * We didn't recieve a valid tag back from
			 * the target on a reconnect.
			 */
			sc_print_addr(xs->sc_link);
			printf("invalid tag recieved -- sending ABORT_TAG\n");
			xs->error = XS_DRIVER_STUFFUP;
			untimeout(ahc_timeout, (caddr_t)scb);
			ahc_done(ahc, scb);
			break;
		  }
		  case AWAITING_MSG:
		  {
			int   scb_index;
			scb_index = inb(SCB_TAG + iobase);
			scb = ahc->scbarray[scb_index];
			/*
			 * This SCB had a zero length command, informing
			 * the sequencer that we wanted to send a special
			 * message to this target.  We only do this for
			 * BUS_DEVICE_RESET messages currently.
			 */
			if(scb->flags & SCB_DEVICE_RESET)
			{
				outb(MSG0 + iobase,
					MSG_BUS_DEVICE_RESET);
				outb(MSG_LEN + iobase, 1);
				printf("Bus Device Reset Message Sent\n");
			}
			else
				panic("ahc_intr: AWAITING_MSG for an SCB that "
					"does not have a waiting message");
			break;
		  }
		  case IMMEDDONE:
		  {
			/*
			 * Take care of device reset messages
			 */
			u_char scbindex = inb(SCB_TAG + iobase);
			scb = ahc->scbarray[scbindex];
			if(scb->flags & SCB_DEVICE_RESET) {
				u_char targ_scratch;
				int found;
				/*
				 * Go back to async/narrow transfers and
				 * renegotiate.
				 */
				ahc_unbusy_target(target, channel, iobase);
				ahc->needsdtr |= ahc->needsdtr_orig & targ_mask;
				ahc->needwdtr |= ahc->needwdtr_orig & targ_mask;
				ahc->sdtrpending &= ~targ_mask;
				ahc->wdtrpending &= ~targ_mask;
				targ_scratch = inb(TARG_SCRATCH + iobase 
							+ scratch_offset);
				targ_scratch &= SXFR;
				outb(TARG_SCRATCH + iobase + scratch_offset,
					targ_scratch);
				found = ahc_reset_device(ahc, target,
						channel, SCB_LIST_NULL, 
						XS_NOERROR);
				sc_print_addr(scb->xs->sc_link);
				printf("Bus Device Reset delivered. "
					"%d SCBs aborted\n", found);
				ahc->in_timeout = FALSE;
				ahc_run_done_queue(ahc);
			}
			else
				panic("ahc_intr: Immediate complete for "
				      "unknown operation.");
			break;
		  }
#if NOT_YET
		  /* XXX Fill these in later */
		  case MESG_BUFFER_BUSY:
			break;
		  case MSGIN_PHASEMIS:
			break;
#endif
		  default:
			printf("ahc: seqint, "
			       "intstat == 0x%x, scsisigi = 0x%x\n",
			       intstat, inb(SCSISIGI + iobase));
			break;
		}
clear:
		/*
		 * Clear the upper byte that holds SEQINT status
		 * codes and clear the SEQINT bit.
		 */
		outb(CLRINT + iobase, CLRSEQINT);

		/*
		 *  The sequencer is paused immediately on
		 *  a SEQINT, so we should restart it when
		 *  we leave this section.
		 */
		UNPAUSE_SEQUENCER(ahc);
	   }


	   if (intstat & SCSIINT) {

		int scb_index = inb(SCB_TAG + iobase);
		status = inb(SSTAT1 + iobase);
		scb = ahc->scbarray[scb_index];

		if (status & SCSIRSTI) {
			char channel;
			channel = inb(SBLKCTL + iobase);
			channel = channel & SELBUSB ? 'B' : 'A';
			printf("ahc%d: Someone reset channel %c\n",
				ahc->unit, channel);
			ahc_reset_channel(ahc, 
					  channel,
					  SCB_LIST_NULL,
					  XS_BUSY,
					  /* Initiate Reset */FALSE);
			scb = NULL;
		}
		else if (!(scb && (scb->flags & SCB_ACTIVE))){
			printf("ahc%d: ahc_intr - referenced scb not "
			       "valid during scsiint 0x%x scb(%d)\n",
				ahc->unit, status, scb_index);
			outb(CLRSINT1 + iobase, status);
			UNPAUSE_SEQUENCER(ahc);
			outb(CLRINT + iobase, CLRSCSIINT);
			scb = NULL;
		}
		else if (status & SCSIPERR) {
			/*
			 * Determine the bus phase and
			 * queue an appropriate message
			 */
			char	*phase;
			u_char	mesg_out = MSG_NOP;
			u_char	lastphase = inb(LASTPHASE + iobase);

			xs = scb->xs;
			sc_print_addr(xs->sc_link);

			switch(lastphase) {
				case P_DATAOUT:
					phase = "Data-Out";
					break;
				case P_DATAIN:
					phase = "Data-In";
					mesg_out = MSG_INITIATOR_DET_ERROR;
					break;
				case P_COMMAND:
					phase = "Command";
					break;
				case P_MESGOUT:
					phase = "Message-Out";
					break;
				case P_STATUS:
					phase = "Status";
					mesg_out = MSG_INITIATOR_DET_ERROR;
					break;
				case P_MESGIN:
					phase = "Message-In";
					mesg_out = MSG_MSG_PARITY_ERROR;
					break;
				default:
					phase = "unknown";
					break;
			}
                        printf("parity error during %s phase.\n", phase);

			/*
			 * We've set the hardware to assert ATN if we
			 * get a parity error on "in" phases, so all we
			 * need to do is stuff the message buffer with
			 * the appropriate message.  In phases have set
			 * mesg_out to something other than MSG_NOP.
			 */
			if(mesg_out != MSG_NOP) {
				outb(MSG0 + iobase, mesg_out);
				outb(MSG_LEN + iobase, 1);
			}
			else
				/*
				 * Should we allow the target to make
				 * this decision for us?
				 */
				xs->error = XS_DRIVER_STUFFUP;
		}
		else if (status & SELTO) {
			u_char waiting;
			u_char flags;

			xs = scb->xs;
			xs->error = XS_SELTIMEOUT;
			/*
			 * Clear any pending messages for the timed out
			 * target, and mark the target as free
			 */
			flags = inb(FLAGS + iobase);
			outb(MSG_LEN + iobase, 0);
			ahc_unbusy_target(xs->sc_link->target,
			 	((long)xs->sc_link->fordriver & SELBUSB)
				 	? 'B' : 'A',
				 iobase);

			outb(SCB_CONTROL + iobase, 0);

			outb(CLRSINT1 + iobase, CLRSELTIMEO);

			outb(CLRINT + iobase, CLRSCSIINT);

			/* Shift the waiting for selection queue forward */
			waiting = inb(WAITING_SCBH + iobase);
			outb(SCBPTR + iobase, waiting);
			waiting = inb(SCB_NEXT + iobase);
			outb(WAITING_SCBH + iobase, waiting);

			RESTART_SEQUENCER(ahc);
		}       
		else if (!(status & BUSFREE)) {
		      sc_print_addr(xs->sc_link);
		      printf("Unknown SCSIINT. Status = 0x%x\n", status);
		      outb(CLRSINT1 + iobase, status);
		      UNPAUSE_SEQUENCER(ahc);
		      outb(CLRINT + iobase, CLRSCSIINT);
		      scb = NULL;
		}
		if(scb != NULL) {
		    /* We want to process the command */
		    untimeout(ahc_timeout, (caddr_t)scb);
		    ahc_done(ahc, scb);
		}
	}
	if (intstat & CMDCMPLT) {
		int   scb_index;

		do {
			scb_index = inb(QOUTFIFO + iobase);
			scb = ahc->scbarray[scb_index];
			if (!scb || !(scb->flags & SCB_ACTIVE)) {
				printf("ahc%d: WARNING "
				       "no command for scb %d (cmdcmplt)\n"
				       "QOUTCNT == %d\n",
					ahc->unit, scb_index,
					inb(QOUTCNT + iobase));
				outb(CLRINT + iobase, CLRCMDINT);
				continue;
			}
			outb(CLRINT + iobase, CLRCMDINT);
			untimeout(ahc_timeout, (caddr_t)scb);
			ahc_done(ahc, scb);

		} while (inb(QOUTCNT + iobase) & ahc->qcntmask);

		ahc_run_waiting_queues(ahc);
	}
}

/*
 * We have a scb which has been processed by the
 * adaptor, now we look to see how the operation
 * went.
 */
static void
ahc_done(ahc, scb)
	struct ahc_data *ahc;
	struct scb *scb;
{
	struct scsi_xfer *xs = scb->xs;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("ahc_done\n"));
	/*
	 * Put the results of the operation
	 * into the xfer and call whoever started it
	 */
	if(scb->flags & SCB_SENSE)
		xs->error = XS_SENSE;
	if(scb->flags & SCB_SENTORDEREDTAG)
		ahc->in_timeout = FALSE;
	if ((xs->flags & SCSI_ERR_OK) && !(xs->error == XS_SENSE)) {
		/* All went correctly  OR errors expected */
		xs->error = XS_NOERROR;
	}
	xs->flags |= ITSDONE;
#ifdef AHC_TAGENABLE
	if(xs->cmd->opcode == 0x12 && xs->error == XS_NOERROR)
	{
		struct scsi_inquiry_data *inq_data;
		u_short mask = 0x01 << (xs->sc_link->target |
				(scb->tcl & 0x08));
		/*
		 * Sneak a look at the results of the SCSI Inquiry
		 * command and see if we can do Tagged queing.  This
		 * should really be done by the higher level drivers.
		 */
		inq_data = (struct scsi_inquiry_data *)xs->data;
		if((inq_data->flags & SID_CmdQue) && !(ahc->tagenable & mask))
		{
		        printf("ahc%d: target %d Tagged Queuing Device\n",
				ahc->unit, xs->sc_link->target);
			ahc->tagenable |= mask;
			if(ahc->maxhscbs >= 16 || (ahc->flags & AHC_PAGESCBS)) {
				/* Default to 8 tags */
				xs->sc_link->opennings += 6;
			}
			else
			{
				/*
				 * Default to 4 tags on whimpy
				 * cards that don't have much SCB
				 * space and can't page.  This prevents
				 * a single device from hogging all
				 * slots.  We should really have a better
				 * way of providing fairness.
				 */
				xs->sc_link->opennings += 2;
			}
		}
	}
#endif
	ahc_free_scb(ahc, scb, xs->flags);
	scsi_done(xs);
}

/*
 * Start the board, ready for normal operation
 */
int
ahc_init(ahc)
	struct  ahc_data *ahc;
{
	u_long	iobase = ahc->baseport;
	u_char	scsi_conf, sblkctl, i;
	u_short	ultraenable = 0;
	int     max_targ = 15;
	/*
	 * Assume we have a board at this stage and it has been reset.
	 */

	/* Handle the SCBPAGING option */
#ifndef AHC_SCBPAGING_ENABLE
	ahc->flags &= ~AHC_PAGESCBS;
#endif

	/* Determine channel configuration and who we are on the scsi bus. */
	switch ( (sblkctl = inb(SBLKCTL + iobase) & 0x0a) ) {
	    case 0:
		ahc->our_id = (inb(SCSICONF + iobase) & HSCSIID);
		ahc->flags &= ~AHC_CHANNEL_B_PRIMARY;
		if(ahc->type == AHC_394)
			printf("Channel %c, SCSI Id=%d, ", 
				ahc->flags & AHC_CHNLB ? 'B' : 'A',
				ahc->our_id);
		else
			printf("Single Channel, SCSI Id=%d, ", ahc->our_id);
		outb(FLAGS + iobase, SINGLE_BUS | (ahc->flags & AHC_PAGESCBS));
		break;
	    case 2:
		ahc->our_id = (inb(SCSICONF + 1 + iobase) & HWSCSIID);
		ahc->flags &= ~AHC_CHANNEL_B_PRIMARY;
		if(ahc->type == AHC_394)
			printf("Wide Channel %c, SCSI Id=%d, ", 
				ahc->flags & AHC_CHNLB ? 'B' : 'A',
				ahc->our_id);
		else
			printf("Wide Channel, SCSI Id=%d, ", ahc->our_id);
		ahc->type |= AHC_WIDE;
		outb(FLAGS + iobase, WIDE_BUS | (ahc->flags & AHC_PAGESCBS));
		break;
	    case 8:
		ahc->our_id = (inb(SCSICONF + iobase) & HSCSIID);
		ahc->our_id_b = (inb(SCSICONF + 1 + iobase) & HSCSIID);
		printf("Twin Channel, A SCSI Id=%d, B SCSI Id=%d, ",
			ahc->our_id, ahc->our_id_b);
		ahc->type |= AHC_TWIN;
		outb(FLAGS + iobase, TWIN_BUS | (ahc->flags & AHC_PAGESCBS));
		break;
	    default:
		printf(" Unsupported adapter type.  Ignoring\n");
		return(-1);
	}

	/* Determine the number of SCBs */

	{
		outb(SCBPTR + iobase, 0);
		outb(SCB_CONTROL + iobase, 0);
		for(i = 1; i < AHC_SCB_MAX; i++) {
			outb(SCBPTR + iobase, i);
			outb(SCB_CONTROL + iobase, i);
			if(inb(SCB_CONTROL + iobase) != i)
				break;
			outb(SCBPTR + iobase, 0);
			if(inb(SCB_CONTROL + iobase) != 0)
				break;
			/* Clear the control byte. */
			outb(SCBPTR + iobase, i);
			outb(SCB_CONTROL + iobase, 0);

			ahc->qcntmask |= i;     /* Update the count mask. */
		}

		/* Ensure we clear the 0 SCB's control byte. */
		outb(SCBPTR + iobase, 0);
		outb(SCB_CONTROL + iobase, 0);

		ahc->qcntmask |= i;
		ahc->maxhscbs = i;
	}

	if((ahc->maxhscbs < AHC_SCB_MAX) && (ahc->flags & AHC_PAGESCBS))
		ahc->maxscbs = AHC_SCB_MAX;
	else {
		ahc->maxscbs = ahc->maxhscbs;
		ahc->flags &= ~AHC_PAGESCBS;
	}

	printf("%d SCBs\n", ahc->maxhscbs);

#ifdef AHC_DEBUG
	if(ahc_debug & AHC_SHOWMISC) {
		struct scb	test;
		printf("ahc%d: hardware scb %ld bytes; kernel scb; "
		       "ahc_dma %d bytes\n",
			ahc->unit, (u_long)&(test.next) - (u_long)(&test),
			sizeof(test),
			sizeof(struct ahc_dma_seg));
	}
#endif /* AHC_DEBUG */

	/* Set the SCSI Id, SXFRCTL0, SXFRCTL1, and SIMODE1, for both channels*/
	if(ahc->type & AHC_TWIN)
	{
		/*
		 * The device is gated to channel B after a chip reset,
		 * so set those values first
		 */
		outb(SCSIID + iobase, ahc->our_id_b);
		scsi_conf = inb(SCSICONF + 1 + iobase);
		outb(SXFRCTL1 + iobase, (scsi_conf & (ENSPCHK|STIMESEL))
					| ENSTIMER|ACTNEGEN|STPWEN);
		outb(SIMODE1 + iobase, ENSELTIMO|ENSCSIRST|ENSCSIPERR);
		if(ahc->type & AHC_ULTRA)
			outb(SXFRCTL0 + iobase, DFON|SPIOEN|ULTRAEN);
		else
			outb(SXFRCTL0 + iobase, DFON|SPIOEN);

		if(scsi_conf & RESET_SCSI) {
			/* Reset the bus */
			if(bootverbose)
				printf("Reseting Channel B\n");
			outb(SCSISEQ + iobase, SCSIRSTO);
			DELAY(1000);
			outb(SCSISEQ + iobase, 0);

			/* Ensure we don't get a RSTI interrupt from this */
			outb(CLRSINT1 + iobase, CLRSCSIRSTI);
			outb(CLRINT + iobase, CLRSCSIINT);
		}

		/* Select Channel A */
		outb(SBLKCTL + iobase, 0);
	}
	outb(SCSIID + iobase, ahc->our_id);
	scsi_conf = inb(SCSICONF + iobase);
	outb(SXFRCTL1 + iobase, (scsi_conf & (ENSPCHK|STIMESEL))
				| ENSTIMER|ACTNEGEN|STPWEN);
	outb(SIMODE1 + iobase, ENSELTIMO|ENSCSIRST|ENSCSIPERR);
	if(ahc->type & AHC_ULTRA)
		outb(SXFRCTL0 + iobase, DFON|SPIOEN|ULTRAEN);
	else
		outb(SXFRCTL0 + iobase, DFON|SPIOEN);

	if(scsi_conf & RESET_SCSI) {
		/* Reset the bus */
		if(bootverbose)
			printf("Reseting Channel A\n");

		outb(SCSISEQ + iobase, SCSIRSTO);
		DELAY(1000);
		outb(SCSISEQ + iobase, 0);

		/* Ensure we don't get a RSTI interrupt from this */
		outb(CLRSINT1 + iobase, CLRSCSIRSTI);
		outb(CLRINT + iobase, CLRSCSIINT);
	}

	/*
	 * Look at the information that board initialization or
	 * the board bios has left us.  In the lower four bits of each
	 * target's scratch space any value other than 0 indicates
	 * that we should initiate syncronous transfers.  If it's zero,
	 * the user or the BIOS has decided to disable syncronous
	 * negotiation to that target so we don't activate the needsdtr
	 * flag.
	 */
	ahc->needsdtr_orig = 0;
	ahc->needwdtr_orig = 0;

	/* Grab the disconnection disable table and invert it for our needs */
	if(ahc->flags & AHC_USEDEFAULTS) {
		printf("ahc%d: Host Adapter Bios disabled.  Using default SCSI "
			"device parameters\n", ahc->unit);
		ahc->discenable = 0xff;
	}
	else
		ahc->discenable = ~((inb(DISC_DSB + iobase + 1) << 8)
				   | inb(DISC_DSB + iobase));

	if(!(ahc->type & (AHC_WIDE|AHC_TWIN)))
		max_targ = 7;

	for(i = 0; i <= max_targ; i++){
		u_char target_settings;
		if (ahc->flags & AHC_USEDEFAULTS) {
			target_settings = 0; /* 10MHz */
			ahc->needsdtr_orig |= (0x01 << i);
			ahc->needwdtr_orig |= (0x01 << i);
		}
		else {
			/* Take the settings leftover in scratch RAM. */
			target_settings = inb(TARG_SCRATCH + i + iobase);

			if(target_settings & 0x0f){
				ahc->needsdtr_orig |= (0x01 << i);
				/*Default to a asyncronous transfers(0 offset)*/
				target_settings &= 0xf0;
			}
			if(target_settings & 0x80){
				ahc->needwdtr_orig |= (0x01 << i);
				/*
				 * We'll set the Wide flag when we
				 * are successful with Wide negotiation.
				 * Turn it off for now so we aren't
				 * confused.
				 */
				target_settings &= 0x7f;
			}
			if(ahc->type & AHC_ULTRA) {
				/*
				 * Enable Ultra for any target that
				 * has a valid ultra syncrate setting.
				 */
				u_char rate = target_settings & 0x70;
				if( rate == 0x00 || rate == 0x10 ||
				    rate == 0x20 )
					ultraenable |= (0x01 << i);
			}
		}
		outb(TARG_SCRATCH+i+iobase,target_settings);
	}
	/*
	 * If we are not a WIDE device, forget WDTR.  This
	 * makes the driver work on some cards that don't
	 * leave these fields cleared when the BIOS is not
	 * installed.
	 */
	if(!(ahc->type & AHC_WIDE))
		ahc->needwdtr_orig = 0;
	ahc->needsdtr = ahc->needsdtr_orig;
	ahc->needwdtr = ahc->needwdtr_orig;
	ahc->sdtrpending = 0;
	ahc->wdtrpending = 0;
	ahc->tagenable = 0;
	ahc->orderedtag = 0;

	outb(ULTRA_ENB + iobase, ultraenable & 0xff);
	outb(ULTRA_ENB + 1 + iobase, (ultraenable >> 8) & 0xff);

#ifdef AHC_DEBUG
	/* How did we do? */
	if(ahc_debug & AHC_SHOWMISC)
		printf("NEEDSDTR == 0x%x\nNEEDWDTR == 0x%x\n"
			"DISCENABLE == 0x%x\n", ahc->needsdtr, 
			ahc->needwdtr, ahc->discenable);
#endif
	/*
	 * Set the number of availible SCBs
	 */
	outb(SCBCOUNT + iobase, ahc->maxhscbs);

	/*
	 * 2's compliment of maximum tag value
	 */
	i = ahc->maxscbs;
	outb(COMP_SCBCOUNT + iobase, -i & 0xff);

	/*
	 * QCount mask to deal with broken aic7850s that
	 * sporatically get garbage in the upper bits of
	 * their QCount registers.
	 */
	outb(QCNTMASK + iobase, ahc->qcntmask);

	/* We don't have any busy targets right now */
	outb(ACTIVE_A + iobase, 0);
	outb(ACTIVE_B + iobase, 0);

	/* We don't have any waiting selections */
	outb(WAITING_SCBH + iobase, SCB_LIST_NULL);

	/* Our disconnection list is empty too */
	outb(DISCONNECTED_SCBH + iobase, SCB_LIST_NULL);

	/* Message out buffer starts empty */
	outb(MSG_LEN + iobase, 0x00);

	/*
	 * Load the Sequencer program and Enable the adapter
	 * in "fast" mode.
         */
	if(bootverbose)
		printf("ahc%d: Downloading Sequencer Program...", ahc->unit);

	ahc_loadseq(iobase);

	if(bootverbose)
		printf("Done\n");

        outb(SEQCTL + iobase, FASTMODE);

        UNPAUSE_SEQUENCER(ahc);

	/*
	 * Note that we are going and return (to probe)
	 */
	ahc->flags |= AHC_INIT;
	return (0);
}

static void
ahcminphys(bp)
        struct buf *bp;
{
/*
 * Even though the card can transfer up to 16megs per command
 * we are limited by the number of segments in the dma segment
 * list that we can hold.  The worst case is that all pages are
 * discontinuous physically, hense the "page per segment" limit
 * enforced here.
 */
        if (bp->b_bcount > ((AHC_NSEG - 1) * PAGE_SIZE)) {
                bp->b_bcount = ((AHC_NSEG - 1) * PAGE_SIZE);
        }
}

/*
 * start a scsi operation given the command and
 * the data address, target, and lun all of which
 * are stored in the scsi_xfer struct
 */
static int32_t
ahc_scsi_cmd(xs)
        struct scsi_xfer *xs;
{
        struct	scb *scb;
        struct	ahc_dma_seg *sg;
        int     seg;            /* scatter gather seg being worked on */
        int     thiskv;
        physaddr thisphys, nextphys;
        int     bytes_this_seg, bytes_this_page, datalen, flags;
        struct	ahc_data *ahc;
	u_short	mask;
        int     s;

	ahc = (struct ahc_data *)xs->sc_link->adapter_softc;
	mask  = (0x01 << (xs->sc_link->target
				| ((u_long)xs->sc_link->fordriver & 0x08)));
        SC_DEBUG(xs->sc_link, SDEV_DB2, ("ahc_scsi_cmd\n"));
        /*
         * get an scb to use. If the transfer
         * is from a buf (possibly from interrupt time)
         * then we can't allow it to sleep
         */
        flags = xs->flags;
        if (flags & ITSDONE) {
                printf("ahc%d: Already done?", ahc->unit);
                xs->flags &= ~ITSDONE;
        }
        if (!(flags & INUSE)) {
                printf("ahc%d: Not in use?", ahc->unit);
                xs->flags |= INUSE;
        }
        if (!(scb = ahc_get_scb(ahc, flags))) {
                xs->error = XS_DRIVER_STUFFUP;
                return (TRY_AGAIN_LATER);
        }
        SC_DEBUG(xs->sc_link, SDEV_DB3, ("start scb(%p)\n", scb));
        scb->xs = xs;
        if (flags & SCSI_RESET)
		scb->flags |= SCB_DEVICE_RESET|SCB_IMMED;
        /*
         * Put all the arguments for the xfer in the scb
         */

	if(ahc->tagenable & mask) {
		scb->control |= TAG_ENB;
		if(ahc->orderedtag & mask) {
			printf("Ordered Tag sent\n");
			scb->control |= 0x02;
			ahc->orderedtag &= ~mask;
		}
	}
	if(ahc->discenable & mask)
		scb->control |= DISCENB;
	if((ahc->needwdtr & mask) && !(ahc->wdtrpending & mask))
	{
		scb->control |= NEEDWDTR;
		ahc->wdtrpending |= mask;
	}
	else if((ahc->needsdtr & mask) && !(ahc->sdtrpending & mask))
	{
		scb->control |= NEEDSDTR;
		ahc->sdtrpending |= mask;
	}
	scb->tcl = ((xs->sc_link->target << 4) & 0xF0) |
				  ((u_long)xs->sc_link->fordriver & 0x08) |
				  (xs->sc_link->lun & 0x07);
	scb->cmdlen = xs->cmdlen;
	scb->cmdpointer = KVTOPHYS(xs->cmd);
	xs->resid = 0;
	xs->status = 0;
	if (xs->datalen) {      /* should use S/G only if not zero length */
		scb->SG_list_pointer = KVTOPHYS(scb->ahc_dma);
		sg = scb->ahc_dma;
		seg = 0;
		/*
		 * Set up the scatter gather block
		 */
		SC_DEBUG(xs->sc_link, SDEV_DB4,
			 ("%ld @%p:- ", xs->datalen, xs->data));
		datalen = xs->datalen;
		thiskv = (int) xs->data;
		thisphys = KVTOPHYS(thiskv);

		while ((datalen) && (seg < AHC_NSEG)) {
			bytes_this_seg = 0;

			/* put in the base address */
			sg->addr = thisphys;

			SC_DEBUGN(xs->sc_link, SDEV_DB4, ("0x%lx", thisphys));

			/* do it at least once */
			nextphys = thisphys;
			while ((datalen) && (thisphys == nextphys)) {
				/*
				 * This page is contiguous (physically)
				 * with the the last, just extend the
				 * length
				 */
				/* how far to the end of the page */
				nextphys = (thisphys & (~(PAGE_SIZE- 1)))
					   + PAGE_SIZE;
				bytes_this_page = nextphys - thisphys;
				/**** or the data ****/
				bytes_this_page = min(bytes_this_page ,datalen);
				bytes_this_seg += bytes_this_page;
				datalen -= bytes_this_page;

				/* get more ready for the next page */
				thiskv = (thiskv & (~(PAGE_SIZE - 1)))
					 + PAGE_SIZE;
				if (datalen)
					thisphys = KVTOPHYS(thiskv);
			}
			/*
			 * next page isn't contiguous, finish the seg
			 */
			SC_DEBUGN(xs->sc_link, SDEV_DB4,
					("(0x%x)", bytes_this_seg));
			sg->len = bytes_this_seg;
			sg++;
			seg++;
		}
		scb->SG_segment_count = seg;

		/* Copy the first SG into the data pointer area */
		scb->data = scb->ahc_dma->addr;
		scb->datalen = scb->ahc_dma->len;
		SC_DEBUGN(xs->sc_link, SDEV_DB4, ("\n"));
		if (datalen) { 
			/* there's still data, must have run out of segs! */
			printf("ahc_scsi_cmd%d: more than %d DMA segs\n",
				ahc->unit, AHC_NSEG);
			xs->error = XS_DRIVER_STUFFUP;
			ahc_free_scb(ahc, scb, flags);
			return (COMPLETE);
		}
	}
	else {
		/*
		 * No data xfer, use non S/G values
	 	 */
		scb->SG_segment_count = 0;
		scb->SG_list_pointer = 0;
		scb->data = 0;
		scb->datalen = 0;
	}

#ifdef AHC_DEBUG
	if((ahc_debug & AHC_SHOWSCBS) && (xs->sc_link->target == DEBUGTARG))
		ahc_print_scb(scb);
#endif
	s = splbio();

	if( scb->position != SCB_LIST_NULL )
	{
		/* We already have a valid slot */
		u_long iobase = ahc->baseport;
		u_char curscb;

		PAUSE_SEQUENCER(ahc);
		curscb = inb(SCBPTR + iobase);
		outb(SCBPTR + iobase, scb->position);
		ahc_send_scb(ahc, scb);
		outb(SCBPTR + iobase, curscb);
		outb(QINFIFO + iobase, scb->position);
		UNPAUSE_SEQUENCER(ahc);
		scb->flags = SCB_ACTIVE;
		if (!(flags & SCSI_NOMASK)) {
			timeout(ahc_timeout, (caddr_t)scb,
				(xs->timeout * hz) / 1000);
		}
		SC_DEBUG(xs->sc_link, SDEV_DB3, ("cmd_sent\n"));
	}
	else {
		scb->flags = SCB_WAITINGQ;
		STAILQ_INSERT_TAIL(&ahc->waiting_scbs, scb, links);
		ahc_run_waiting_queues(ahc);
	}
	if (!(flags & SCSI_NOMASK)) {
		splx(s);
		return (SUCCESSFULLY_QUEUED);
	}
	/*
	 * If we can't use interrupts, poll for completion
	 */
	SC_DEBUG(xs->sc_link, SDEV_DB3, ("cmd_poll\n"));
	do {
		if (ahc_poll(ahc, xs->timeout)) {
			if (!(xs->flags & SCSI_SILENT))
				printf("cmd fail\n");
			ahc_timeout(scb);
			break;
		}
	} while (!(xs->flags & ITSDONE));  /* a non command complete intr */
	splx(s); 
	return (COMPLETE);
}


/*
 * A scb (and hence an scb entry on the board is put onto the
 * free list.
 */
static void
ahc_free_scb(ahc, scb, flags)
        struct	ahc_data *ahc;
        int     flags;
        struct  scb *scb;
{
	struct scb *wscb;
	unsigned int opri;

	opri = splbio();

	scb->flags = SCB_FREE;
	if(scb->position == SCB_LIST_NULL) {
		STAILQ_INSERT_HEAD(&ahc->page_scbs, scb, links);
		if(!scb->links.stqe_next && !ahc->free_scbs.stqh_first)
			/*
			 * If there were no SCBs availible, wake anybody waiting
			 * for one to come free.
			 */
			wakeup((caddr_t)&ahc->free_scbs);
	}
	/*
	 * If there are any SCBS on the waiting queue,
	 * assign the slot of this "freed" SCB to the first
	 * one.  We'll run the waiting queues after all command
	 * completes for a particular interrupt are completed
	 * or when we start another command.
	 */
	else if((wscb = ahc->waiting_scbs.stqh_first) != NULL) {
		wscb->position = scb->position;
		STAILQ_REMOVE_HEAD(&ahc->waiting_scbs, links);
		STAILQ_INSERT_HEAD(&ahc->assigned_scbs, wscb, links);
		wscb->flags = SCB_ASSIGNEDQ;

		/* 
		 * The "freed" SCB will need to be assigned a slot
		 * before being used, so put it in the page_scbs
		 * queue.
		 */
		scb->position = SCB_LIST_NULL;
		STAILQ_INSERT_HEAD(&ahc->page_scbs, scb, links);
		if(!scb->links.stqe_next && !ahc->free_scbs.stqh_first)
			/*
			 * If there were no SCBs availible, wake anybody waiting
			 * for one to come free.
			 */
			wakeup((caddr_t)&ahc->free_scbs);
	}
	else {
		STAILQ_INSERT_HEAD(&ahc->free_scbs, scb, links);
#ifdef AHC_DEBUG
		ahc->activescbs--;
#endif
		if(!scb->links.stqe_next && !ahc->page_scbs.stqh_first)
			/*
			 * If there were no SCBs availible, wake anybody waiting
			 * for one to come free.
			 */
			wakeup((caddr_t)&ahc->free_scbs);
	}
	splx(opri);
}

/*
 * Get a free scb, either one already assigned to a hardware slot
 * on the adapter or one that will require an SCB to be paged out before
 * use. If there are none, see if we can allocate a new SCB.  Otherwise
 * either return an error or sleep.
 */
static struct scb *
ahc_get_scb(ahc, flags)
        struct	ahc_data *ahc;
        int	flags;
{
	unsigned opri;
	struct scb *scbp;

	opri = splbio();
	/*
	 * If we can and have to, sleep waiting for one to come free
	 * but only if we can't allocate a new one.
	 */
	while (1) {
		if((scbp = ahc->free_scbs.stqh_first)) {
			STAILQ_REMOVE_HEAD(&ahc->free_scbs, links);
		}
		else if((scbp = ahc->page_scbs.stqh_first)) {
			STAILQ_REMOVE_HEAD(&ahc->page_scbs, links);
		}
		else if (ahc->numscbs < ahc->maxscbs) {
			scbp = (struct scb *) malloc(sizeof(struct scb),
				M_TEMP, M_NOWAIT);
			if (scbp) {
				bzero(scbp, sizeof(struct scb));
				scbp->tag = ahc->numscbs;
				if( ahc->numscbs < ahc->maxhscbs )
					scbp->position = ahc->numscbs;
				else
					scbp->position = SCB_LIST_NULL;
				ahc->numscbs++;
				/*
				 * Place in the scbarray
				 * Never is removed.
				 */
				ahc->scbarray[scbp->tag] = scbp;
			}
			else {
				printf("ahc%d: Can't malloc SCB\n", ahc->unit);
			}
		}
		else {
			if (!(flags & SCSI_NOSLEEP)) {
				tsleep((caddr_t)&ahc->free_scbs, PRIBIO,
					"ahcscb", 0);
				continue;
			}
		}
		break;
	}

	if (scbp) {
		scbp->control = 0;
		scbp->status = 0;
		scbp->flags = 0;
#ifdef AHC_DEBUG
		ahc->activescbs++;
		if((ahc_debug & AHC_SHOWSCBCNT)
		  && (ahc->activescbs == ahc->maxhscbs))
			printf("ahc%d: Max SCBs active\n", ahc->unit);
#endif
	}

	splx(opri);

	return (scbp);
}

static void ahc_loadseq(iobase)
	u_long iobase;
{
        static unsigned char seqprog[] = {
#               include "aic7xxx_seq.h"
	};

	outb(SEQCTL + iobase, PERRORDIS|SEQRESET|LOADRAM);

	outsb(SEQRAM + iobase, seqprog, sizeof(seqprog));

	outb(SEQCTL + iobase, FASTMODE|SEQRESET);
	do {
		outb(SEQCTL + iobase, SEQRESET|FASTMODE);

	} while (inb(SEQADDR0 + iobase) != 0 &&
		 inb(SEQADDR1 + iobase) != 0);
}

/*
 * Function to poll for command completion when
 * interrupts are disabled (crash dumps)
 */
static int
ahc_poll(ahc, wait)
	struct	ahc_data *ahc;
	int	wait; /* in msec */
{
	u_long	iobase = ahc->baseport;
	u_long	stport = INTSTAT + iobase;

	while (--wait) {
		DELAY(1000);
		if (inb(stport) & INT_PEND)
			break;
	} if (wait == 0) {
		printf("ahc%d: board is not responding\n", ahc->unit);
		return (EIO);
	}
	ahc_intr((void *)ahc);
	return (0);
}

static void
ahc_timeout(arg)
	void	*arg;
{
	struct	scb *scb = (struct scb *)arg;
	struct	ahc_data *ahc;
	int	s, found;
	u_char	bus_state;
	u_long	iobase;
	char	channel;

	s = splbio();

	if (!(scb->flags & SCB_ACTIVE)) {
		/* Previous timeout took care of me already */
		splx(s);
		return;
	}

	ahc = (struct ahc_data *)scb->xs->sc_link->adapter_softc;

	if (ahc->in_timeout) {
		/*
		 * Some other SCB has started a recovery operation
		 * and is still working on cleaning things up.
		 */
		if (scb->flags & SCB_TIMEDOUT) {
			/*
			 * This SCB has been here before and is not the
			 * recovery SCB. Cut our losses and panic.  Its
			 * better to do this than trash a filesystem.
			 */
			panic("ahc%d: Timed-out command times out "
				"again\n", ahc->unit);
		}
		else if (!(scb->flags & SCB_ABORTED))
		{
			/*
			 * This is not the SCB that started this timeout
			 * processing.  Give this scb another lifetime so
			 * that it can continue once we deal with the
			 * timeout.
			 */
			scb->flags |= SCB_TIMEDOUT;
			timeout(ahc_timeout, (caddr_t)scb, 
				(scb->xs->timeout * hz) / 1000);
			splx(s);
			return;
		}
	}
	ahc->in_timeout = TRUE;

	/*      
	 * Ensure that the card doesn't do anything
	 * behind our back.
	 */
	PAUSE_SEQUENCER(ahc);

	sc_print_addr(scb->xs->sc_link);
	printf("timed out ");
	/*
	 * Take a snapshot of the bus state and print out
	 * some information so we can track down driver bugs.
	 */
	iobase = ahc->baseport;
	bus_state = inb(iobase + LASTPHASE);

	switch(bus_state & PHASE_MASK)
	{
		case P_DATAOUT:
			printf("in dataout phase");
			break;
		case P_DATAIN:
			printf("in datain phase");
			break;
		case P_COMMAND:
			printf("in command phase");
			break;
		case P_MESGOUT:
			printf("in message out phase");
			break;
		case P_STATUS:
			printf("in status phase");
			break;
		case P_MESGIN:
			printf("in message in phase");
			break;
		default:
			printf("while idle, LASTPHASE == 0x%x",
				bus_state);
			/* 
			 * We aren't in a valid phase, so assume we're
			 * idle.
			 */
			bus_state = 0;
			break;
	}

	printf(", SCSISIGI == 0x%x\n", inb(iobase + SCSISIGI));

	/* Decide our course of action */

	if(scb->flags & SCB_ABORTED)
	{
		/*
		 * Been down this road before.
		 * Do a full bus reset.
		 */
		char channel = (scb->tcl & SELBUSB)
			   ? 'B': 'A';	
		found = ahc_reset_channel(ahc, channel, scb->tag,
					  XS_TIMEOUT, /*Initiate Reset*/TRUE);
		printf("ahc%d: Issued Channel %c Bus Reset #1. "
		       "%d SCBs aborted\n", ahc->unit, channel, found);
		ahc->in_timeout = FALSE;
	}
	else if(scb->control & TAG_ENB) {
		/*
		 * We could be starving this command
		 * try sending an ordered tag command
		 * to the target we come from.
		 */
		scb->flags |= SCB_ABORTED|SCB_SENTORDEREDTAG;
		ahc->orderedtag |= 0xFF;
		timeout(ahc_timeout, (caddr_t)scb, (5 * hz));
		UNPAUSE_SEQUENCER(ahc);
		printf("Ordered Tag queued\n");
		goto done;
	}
	else {
		/*
		 * Send a Bus Device Reset Message:
		 * The target that is holding up the bus may not
		 * be the same as the one that triggered this timeout
		 * (different commands have different timeout lengths).
		 * It is also impossible to get a message to a target
		 * if we are in a "frozen" data transfer phase.  Our
		 * strategy here is to queue a bus device reset message
		 * to the timed out target if it is disconnected.
		 * Otherwise, if we have an active target we stuff the
		 * message buffer with a bus device reset message and
		 * assert ATN in the hopes that the target will let go
		 * of the bus and finally disconnect.  If this fails,
		 * we'll get another timeout 2 seconds later which will
		 * cause a bus reset.
		 *
		 * XXX If the SCB is paged out, we simply reset the
		 *     bus.  We should probably queue a new command
		 *     instead.
		 */

		/* Test to see if scb is disconnected */
		if( !(scb->flags & SCB_PAGED_OUT ) ){
			u_char active_scb;
			struct scb *active_scbp;

			active_scb = inb(SCBPTR + iobase);
			active_scbp = ahc->scbarray[inb(SCB_TAG + iobase)];
			outb(SCBPTR + iobase, scb->position);

			if(inb(SCB_CONTROL + iobase) & DISCONNECTED) {
				if(ahc->flags & AHC_PAGESCBS) {
					/*
					 * Pull this SCB out of the 
					 * disconnected list.
					 */
					u_char prev = inb(SCB_PREV + iobase);
					u_char next = inb(SCB_NEXT + iobase);
					if(prev == SCB_LIST_NULL) {
						/* At the head */
						outb(DISCONNECTED_SCBH + iobase,
						     next );
					}
					else {
						outb(SCBPTR + iobase, prev);
						outb(SCB_NEXT + iobase, next);
						if(next != SCB_LIST_NULL) {
							outb(SCBPTR + iobase,
							     next);
							outb(SCB_PREV + iobase,
							     prev);
						}
						outb(SCBPTR + iobase,
						     scb->position);
					}
				}
				scb->flags |= SCB_DEVICE_RESET|SCB_ABORTED;
				scb->control &= DISCENB;
				scb->cmdlen = 0;
				scb->SG_segment_count = 0;
				scb->SG_list_pointer = 0;
				scb->data = 0;
				scb->datalen = 0;
				ahc_send_scb(ahc, scb);
				ahc_add_waiting_scb(iobase, scb);
				timeout(ahc_timeout, (caddr_t)scb, (2 * hz));
				sc_print_addr(scb->xs->sc_link);
				printf("BUS DEVICE RESET message queued.\n");
				outb(SCBPTR + iobase, active_scb);
				UNPAUSE_SEQUENCER(ahc);
				goto done;
			}
			/* Is the active SCB really active? */
			else if((active_scbp->flags & SCB_ACTIVE) && bus_state){
				outb(MSG_LEN + iobase, 1);
				outb(MSG0 + iobase, MSG_BUS_DEVICE_RESET);
				outb(SCSISIGO + iobase, bus_state|ATNO);
				sc_print_addr(active_scbp->xs->sc_link);
				printf("asserted ATN - device reset in "
				       "message buffer\n");
				active_scbp->flags |=   SCB_DEVICE_RESET
						      | SCB_ABORTED;
				if(active_scbp != scb) {
					untimeout(ahc_timeout, 
						  (caddr_t)active_scbp);
					/* Give scb a new lease on life */
					timeout(ahc_timeout, (caddr_t)scb, 
						(scb->xs->timeout * hz) / 1000);
				}
				timeout(ahc_timeout, (caddr_t)active_scbp, 
					(2 * hz));
				outb(SCBPTR + iobase, active_scb);
				UNPAUSE_SEQUENCER(ahc);
				goto done;
			}
		}
		/*
		 * No active target or a paged out SCB.
		 * Try reseting the bus.
		 */
		channel = (scb->tcl & SELBUSB) ? 'B': 'A';	
		found = ahc_reset_channel(ahc, channel, scb->tag, 
					  XS_TIMEOUT,
					  /*Initiate Reset*/TRUE);
		printf("ahc%d: Issued Channel %c Bus Reset #2. "
			"%d SCBs aborted\n", ahc->unit, channel,
			found);
		ahc->in_timeout = FALSE;
	}
done:
	splx(s);
}


/*
 * The device at the given target/channel has been reset.  Abort 
 * all active and queued scbs for that target/channel. 
 */
static int
ahc_reset_device(ahc, target, channel, timedout_scb, xs_error)
	struct ahc_data *ahc;
	int target;
	char channel;
	u_char timedout_scb;
	u_int32_t xs_error;
{
	u_long iobase = ahc->baseport;
        struct scb *scbp;
	u_char active_scb;
	int i = 0;
	int found = 0;

	/* restore this when we're done */
	active_scb = inb(SCBPTR + iobase);

	/*
	 * Search the QINFIFO.
	 */
	{
		u_char saved_queue[AHC_SCB_MAX];
		u_char queued = inb(QINCNT + iobase) & ahc->qcntmask;

		for (i = 0; i < (queued - found); i++) {
			saved_queue[i] = inb(QINFIFO + iobase);
			outb(SCBPTR + iobase, saved_queue[i]);
			scbp = ahc->scbarray[inb(SCB_TAG + iobase)];
			if (ahc_match_scb (scbp, target, channel)){
				/*
				 * We found an scb that needs to be aborted.
				 */
				scbp->flags = SCB_ABORTED|SCB_QUEUED_FOR_DONE;
				scbp->xs->error |= xs_error;
				if(scbp->position != timedout_scb)
					untimeout(ahc_timeout, (caddr_t)scbp);
				outb(SCB_CONTROL + iobase, 0);
				i--;
				found++;
			}
		}
		/* Now put the saved scbs back. */
		for (queued = 0; queued < i; queued++) {
			outb (QINFIFO + iobase, saved_queue[queued]);
		}
	}

	/*
	 * Search waiting for selection list.
	 */
	{
		u_char next, prev;

		next = inb(WAITING_SCBH + iobase);  /* Start at head of list. */
		prev = SCB_LIST_NULL;

		while (next != SCB_LIST_NULL) {
			outb(SCBPTR + iobase, next);
			scbp = ahc->scbarray[inb(SCB_TAG + iobase)];
			/*
			 * Select the SCB.
			 */
			if (ahc_match_scb(scbp, target, channel)) {
				next = ahc_abort_wscb(ahc, scbp, prev,
						iobase, timedout_scb, xs_error);
				found++;
			}
			else {
				prev = next;
				next = inb(SCB_NEXT + iobase);
			}
		}
	}
	/*
	 * Go through the entire SCB array now and look for 
	 * commands for this target that are active.  These
	 * are other (most likely tagged) commands that 
	 * were disconnected when the reset occured.
	 */
	for(i = 0; i < ahc->numscbs; i++) {
		scbp = ahc->scbarray[i];
		if((scbp->flags & SCB_ACTIVE)
		  && ahc_match_scb(scbp, target, channel)) {
			/* Ensure the target is "free" */
			ahc_unbusy_target(target, channel, iobase);
			if( !(scbp->flags & SCB_PAGED_OUT) )
			{
				outb(SCBPTR + iobase, scbp->position);
				outb(SCB_CONTROL + iobase, 0);
			}
			scbp->flags = SCB_ABORTED|SCB_QUEUED_FOR_DONE;
			scbp->xs->error |= xs_error;
			if(scbp->tag != timedout_scb)
				untimeout(ahc_timeout, (caddr_t)scbp);
			found++;
		}
	}			
	outb(SCBPTR + iobase, active_scb);
	return found;
}

/*
 * Manipulate the waiting for selection list and return the
 * scb that follows the one that we remove.
 */
static u_char
ahc_abort_wscb (ahc, scbp, prev, iobase, timedout_scb, xs_error)
	struct ahc_data *ahc;
        struct scb *scbp;
	u_char prev;
        u_long iobase;
	u_char timedout_scb;
	u_int32_t xs_error;
{       
	u_char curscbp, next;
	int target = ((scbp->tcl >> 4) & 0x0f);
	char channel = (scbp->tcl & SELBUSB) ? 'B' : 'A';
	/*
	 * Select the SCB we want to abort and
	 * pull the next pointer out of it.
	 */
	curscbp = inb(SCBPTR + iobase);
	outb(SCBPTR + iobase, scbp->position);
	next = inb(SCB_NEXT + iobase);

	/* Clear the necessary fields */
	outb(SCB_CONTROL + iobase, 0);
	outb(SCB_NEXT + iobase, SCB_LIST_NULL);
	ahc_unbusy_target(target, channel, iobase);

	/* update the waiting list */
	if( prev == SCB_LIST_NULL ) 
		/* First in the list */
		outb(WAITING_SCBH + iobase, next); 
	else {
		/*
		 * Select the scb that pointed to us 
		 * and update its next pointer.
		 */
		outb(SCBPTR + iobase, prev);
		outb(SCB_NEXT + iobase, next);
	}
	/*
	 * Point us back at the original scb position
	 * and inform the SCSI system that the command
	 * has been aborted.
	 */
	outb(SCBPTR + iobase, curscbp);
	scbp->flags = SCB_ABORTED|SCB_QUEUED_FOR_DONE;
	scbp->xs->error |= xs_error;
	if(scbp->tag != timedout_scb)
		untimeout(ahc_timeout, (caddr_t)scbp);
	return next;
}

static void
ahc_busy_target(target, channel, iobase)
	u_char target;
	char   channel;
	u_long iobase;
{
	u_char active;
	u_long active_port = ACTIVE_A + iobase;
	if(target > 0x07 || channel == 'B') {
		/* 
		 * targets on the Second channel or
		 * above id 7 store info in byte two 
		 * of HA_ACTIVE
		 */
		active_port++;
	}
	active = inb(active_port);
	active |= (0x01 << (target & 0x07));
	outb(active_port, active);
}

static void
ahc_unbusy_target(target, channel, iobase)
	u_char target;
	char   channel;
	u_long iobase;
{
	u_char active;
	u_long active_port = ACTIVE_A + iobase;
	if(target > 0x07 || channel == 'B') {
		/* 
		 * targets on the Second channel or
		 * above id 7 store info in byte two 
		 * of HA_ACTIVE
		 */
		active_port++;
	}
	active = inb(active_port);
	active &= ~(0x01 << (target & 0x07));
	outb(active_port, active);
}

static void
ahc_reset_current_bus(iobase)
	u_long iobase;
{
	outb(SCSISEQ + iobase, SCSIRSTO);
	DELAY(1000);
	outb(SCSISEQ + iobase, 0);
}

static int
ahc_reset_channel(ahc, channel, timedout_scb, xs_error, initiate_reset)
	struct ahc_data *ahc;
	char   channel;
	u_char timedout_scb;
	u_int32_t xs_error;
	u_char initiate_reset;
{
	u_long iobase = ahc->baseport;
	u_char sblkctl;
	char cur_channel;
	u_long offset, offset_max;
	int found;

	/*
	 * Clean up all the state information for the
	 * pending transactions on this bus.
	 */
	found = ahc_reset_device(ahc, ALL_TARGETS, channel, 
				 timedout_scb, xs_error);
	if(channel == 'B'){
		ahc->needsdtr |= (ahc->needsdtr_orig & 0xff00);
		ahc->sdtrpending &= 0x00ff;
		outb(ACTIVE_B + iobase, 0);
		offset = TARG_SCRATCH + iobase + 8;
		offset_max = TARG_SCRATCH + iobase + 16;
	}
	else if (ahc->type & AHC_WIDE){
		ahc->needsdtr = ahc->needsdtr_orig;
		ahc->needwdtr = ahc->needwdtr_orig;
		ahc->sdtrpending = 0;
		ahc->wdtrpending = 0;
		outb(ACTIVE_A + iobase, 0);
		outb(ACTIVE_B + iobase, 0);
		offset = TARG_SCRATCH + iobase;
		offset_max = TARG_SCRATCH + iobase + 16;
	}
	else{
		ahc->needsdtr |= (ahc->needsdtr_orig & 0x00ff);
		ahc->sdtrpending &= 0xff00;
		outb(ACTIVE_A + iobase, 0);
		offset = TARG_SCRATCH + iobase;
		offset_max = TARG_SCRATCH + iobase + 8;
	}
	for(;offset < offset_max;offset++) {
		/*
		 * Revert to async/narrow transfers
		 * until we renegotiate.
		 */
		u_char targ_scratch;
		targ_scratch = inb(offset);
		targ_scratch &= SXFR;
		outb(offset, targ_scratch);
	}

	/*
	 * Reset the bus if we are initiating this reset and
	 * restart/unpause the sequencer
	 */
	/* Case 1: Command for another bus is active */
	sblkctl = inb(SBLKCTL + iobase);
	cur_channel = (sblkctl & SELBUSB) ? 'B' : 'A';
	if(cur_channel != channel)
	{
		/*
		 * Stealthily reset the other bus
		 * without upsetting the current bus
		 */
		outb(SBLKCTL + iobase, sblkctl ^ SELBUSB);
		if( initiate_reset )
		{
			ahc_reset_current_bus(iobase);
		}
		outb(CLRSINT1 + iobase, CLRSCSIRSTI|CLRSELTIMEO);
		outb(CLRINT + iobase, CLRSCSIINT);
		outb(SBLKCTL + iobase, sblkctl);
		UNPAUSE_SEQUENCER(ahc);
	}
	/* Case 2: A command from this bus is active or we're idle */ 
	else {
		if( initiate_reset )
		{
			ahc_reset_current_bus(iobase);
		}
		outb(CLRSINT1 + iobase, CLRSCSIRSTI|CLRSELTIMEO);
		outb(CLRINT + iobase, CLRSCSIINT);
		RESTART_SEQUENCER(ahc);
	}
	ahc_run_done_queue(ahc);
	return found;
}

void
ahc_run_done_queue(ahc)
	struct ahc_data *ahc;
{
	int i;
	struct scb *scbp;
	
	for(i = 0; i < ahc->numscbs; i++) {
		scbp = ahc->scbarray[i];
		if(scbp->flags & SCB_QUEUED_FOR_DONE) 
			ahc_done(ahc, scbp);
	}
}
	
static int
ahc_match_scb (scb, target, channel)
        struct scb *scb;
        int target;
	char channel;
{
	int targ = (scb->tcl >> 4) & 0x0f;
	char chan = (scb->tcl & SELBUSB) ? 'B' : 'A';

	if (target == ALL_TARGETS) 
		return (chan == channel);
	else
		return ((chan == channel) && (targ == target));
}
