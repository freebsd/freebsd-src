/*
 * Generic driver for the aic7xxx based adaptec SCSI controllers
 * Product specific probe and attach routines can be found in:
 * i386/eisa/aic7770.c	27/284X and aic7770 motherboard controllers
 * pci/aic7870.c	3940, 2940, aic7880, aic7870, aic7860,
 *			and aic7850 controllers
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
 *      $Id: aic7xxx.c,v 1.82 1996/11/05 07:57:26 gibbs Exp $
 */
/*
 * TODO:
 *	Implement Target Mode
 *
 * A few notes on features of the driver.
 *
 * SCB paging takes advantage of the fact that devices stay disconnected
 * from the bus a relatively long time and that while they're disconnected,
 * having the SCBs for these transactions down on the host adapter is of
 * little use.  Instead of leaving this idle SCB down on the card we copy
 * it back up into kernel memory and reuse the SCB slot on the card to
 * schedule another transaction.  This can be a real payoff when doing random
 * I/O to tagged queueing devices since there are more transactions active at
 * once for the device to sort for optimal seek reduction. The algorithm goes
 * like this...
 *
 * The sequencer maintains two lists of its hardware SCBs.  The first is the
 * singly linked free list which tracks all SCBs that are not currently in
 * use.  The second is the doubly linked disconnected list which holds the
 * SCBs of transactions that are in the disconnected state sorted most
 * recently disconnected first.  When the kernel queues a transaction to
 * the card, a hardware SCB to "house" this transaction is retrieved from
 * either of these two lists.  If the SCB came from the disconnected list,
 * a check is made to see if any data transfer or SCB linking (more on linking
 * in a bit) information has been changed since it was copied from the host
 * and if so, DMAs the SCB back up before it can be used.  Once a hardware
 * SCB has been obtained, the SCB is DMAed from the host.  Before any work
 * can begin on this SCB, the sequencer must ensure that either the SCB is
 * for a tagged transaction or the target is not already working on another
 * non-tagged transaction.  If a conflict arises in the non-tagged case, the
 * sequencer finds the SCB for the active transactions and sets the SCB_LINKED
 * field in that SCB to this next SCB to execute.  To facilitate finding
 * active non-tagged SCBs, the last four bytes of up to the first four hardware
 * SCBs serve as a storage area for the currently active SCB ID for each
 * target.
 *
 * When a device reconnects, a search is made of the hardware SCBs to find
 * the SCB for this transaction.  If the search fails, a hardware SCB is
 * pulled from either the free or disconnected SCB list and the proper
 * SCB is DMAed from the host.  If the SCB_ABORTED control bit is set
 * in the control byte of the SCB while it was disconnected, the sequencer
 * will send an abort or abort tag message to the target during the
 * reconnection and signal the kernel that the abort was successfull.
 *
 * When a command completes, a check for non-zero status and residuals is
 * made.  If either of these conditions exists, the SCB is DMAed back up to
 * the host so that it can interpret this information.  Additionally, in the
 * case of bad status, the sequencer generates a special interrupt and pauses
 * itself.  This allows the host to setup a request sense command if it 
 * chooses for this target synchronously with the error so that sense
 * information isn't lost.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#if defined(__NetBSD__)
#include <sys/device.h>
#include <machine/bus.h>
#include <machine/intr.h>
#endif /* defined(__NetBSD__) */

#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_message.h>
#if defined(__NetBSD__)
#include <scsi/scsi_debug.h>
#endif
#include <scsi/scsiconf.h>

#if defined(__FreeBSD__)
#include <machine/clock.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#if defined(__FreeBSD__)
#include <i386/scsi/aic7xxx.h>

#include <dev/aic7xxx/aic7xxx_reg.h>
#endif /* defined(__FreeBSD__) */

#if defined(__NetBSD__)
#include <dev/ic/aic7xxxreg.h>
#include <dev/ic/aic7xxxvar.h>

#define bootverbose	1

#if DEBUGTARGET < 0	/* Negative numbrs for disabling cause warnings */
#define DEBUGTARGET	17
#endif
#endif /* defined(__NetBSD__) */

#include <sys/kernel.h>

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define ALL_TARGETS -1

#if defined(__FreeBSD__)
u_long ahc_unit = 0;
#endif

#ifdef AHC_DEBUG
static int     ahc_debug = AHC_DEBUG;
#endif

#ifdef AHC_BROKEN_CACHE
int ahc_broken_cache = 1;

/*
 * "wbinvd" cause writing back whole cache (both CPU internal & external)
 * to memory, so that the instruction takes a lot of time.
 * This makes machine slow.
 */
#define	INVALIDATE_CACHE()	__asm __volatile("wbinvd")
#endif

/**** bit definitions for SCSIDEF ****/
#define	HSCSIID		0x07		/* our SCSI ID */
#define HWSCSIID	0x0f		/* our SCSI ID if Wide Bus */

static void	ahcminphys __P((struct buf *bp));
static int32_t	ahc_scsi_cmd __P((struct scsi_xfer *xs));
static void	ahc_run_waiting_queue __P((struct ahc_softc *ahc));
static struct scb *
                ahc_get_scb __P((struct ahc_softc *ahc, u_int32_t flags));
static void     ahc_free_scb __P((struct ahc_softc *ahc, struct scb *scb));
static struct scb *
		ahc_alloc_scb __P((struct ahc_softc *ahc));
static inline void pause_sequencer __P((struct ahc_softc *ahc));
static inline void unpause_sequencer __P((struct ahc_softc *ahc,
					  int unpause_always));
static inline void restart_sequencer __P((struct ahc_softc *ahc));

static struct scsi_adapter ahc_switch =
{
	ahc_scsi_cmd,
	ahcminphys,
	NULL,
	NULL,
#if defined(__FreeBSD__)
	NULL,
	"ahc",
	{ 0, 0 }
#endif
};

static struct scsi_device ahc_dev =
{
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
#if defined(__FreeBSD__)
	"ahc",
	0,
	{ 0, 0 }
#endif
};

static inline void
pause_sequencer(ahc)
	struct ahc_softc *ahc;
{
	AHC_OUTB(ahc, HCNTRL, ahc->pause);

	/*
	 * Since the sequencer can disable pausing in a critical section, we
	 * must loop until it actually stops.
	 */
	while ((AHC_INB(ahc, HCNTRL) & PAUSE) == 0)
		;
}

static inline void
unpause_sequencer(ahc, unpause_always)
	struct ahc_softc *ahc;
	int unpause_always;
{
	if (unpause_always
	 || (AHC_INB(ahc, INTSTAT) & (SCSIINT | SEQINT | BRKADRINT)) == 0)
		AHC_OUTB(ahc, HCNTRL, ahc->unpause);
}

/*
 * Restart the sequencer program from address zero
 */
static inline void
restart_sequencer(ahc)
	struct ahc_softc *ahc;
{
	do {
		AHC_OUTB(ahc, SEQCTL, SEQRESET|FASTMODE);
	} while ((AHC_INB(ahc, SEQADDR0) != 0)
	      || (AHC_INB(ahc, SEQADDR1) != 0));

	unpause_sequencer(ahc, /*unpause_always*/TRUE);
}

#if defined(__FreeBSD__)
#define	IS_SCSIBUS_B(ahc, sc_link)	\
	(((u_int32_t)(sc_link)->fordriver) & SELBUSB)
#else /* NetBSD/OpenBSD */
#define	IS_SCSIBUS_B(ahc, sc_link)	\
	((sc_link)->scsibus == (ahc)->sc_link_b.scsibus)
#endif

static u_int8_t	ahc_abort_wscb __P((struct ahc_softc *ahc, struct scb *scbp,
				    u_int8_t scbpos, u_int8_t prev,
				    struct scb *timedout_scb,
				    u_int32_t xs_error));
static void	ahc_done __P((struct ahc_softc *ahc, struct scb *scbp));
static void 	ahc_handle_seqint __P((struct ahc_softc *ahc, u_int8_t intstat));
static void	ahc_handle_scsiint __P((struct ahc_softc *ahc,
					u_int8_t intstat));
static void	ahc_loadseq __P((struct ahc_softc *ahc));
static int	ahc_match_scb __P((struct scb *scb, int target, char channel));
static int	ahc_poll __P((struct ahc_softc *ahc, int wait));
#ifdef AHC_DEBUG
static void	ahc_print_scb __P((struct scb *scb));
#endif
static u_int8_t find_scb __P((struct ahc_softc *ahc, struct scb *scb));
static int	ahc_reset_channel __P((struct ahc_softc *ahc, char channel,
				       struct scb *timedout_scb,
				       u_int32_t xs_error,
				       int initiate_reset));
static int	ahc_reset_device __P((struct ahc_softc *ahc, int target,
				      char channel, struct scb *timedout_scb,
				      u_int32_t xs_error));
static void	ahc_reset_current_bus __P((struct ahc_softc *ahc));
static void	ahc_run_done_queue __P((struct ahc_softc *ahc));
static void	ahc_scsirate __P((struct ahc_softc* ahc, u_int8_t *scsirate,
				  u_int8_t *period, u_int8_t *offset,
				  char channel, int target));
#if defined(__FreeBSD__)
static timeout_t
		ahc_timeout;
#elif defined(__NetBSD__)
static void	ahc_timeout __P((void *));
#endif

static u_int8_t	ahc_unbusy_target __P((struct ahc_softc *ahc,
				       int target, char channel));

static void	ahc_construct_sdtr __P((struct ahc_softc *ahc, int start_byte,
					u_int8_t period, u_int8_t offset));

static void	ahc_construct_wdtr __P((struct ahc_softc *ahc, int start_byte,
					u_int8_t bus_width));

static void	ahc_calc_residual __P((struct scb *scb));

#if defined(__FreeBSD__)

char *ahc_name(ahc)
	struct ahc_softc *ahc;
{
	static char name[10];

	sprintf(name, "ahc%d", ahc->unit);
	return (name);
}

#elif defined(__NetBSD__)
struct cfdriver ahc_cd = {
	NULL, "ahc", DV_DULL
};
#endif

#ifdef  AHC_DEBUG
static void
ahc_print_scb(scb)
        struct scb *scb;
{
	printf("scb:%p control:0x%x tcl:0x%x cmdlen:%d cmdpointer:0x%lx\n",
		scb,
		scb->control,
		scb->tcl,
		scb->cmdlen,
		scb->cmdpointer );
	printf("        datlen:%d data:0x%lx segs:0x%x segp:0x%lx\n",
		scb->datalen,
		scb->data,
		scb->SG_segment_count,
		scb->SG_list_pointer);
	printf("	sg_addr:%lx sg_len:%ld\n",
		scb->ahc_dma[0].addr,
		scb->ahc_dma[0].len);
}

#endif

static struct {
        u_int8_t errno;
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
	int sxfr;
	/* Rates in Ultra mode have bit 8 of sxfr set */
#define		ULTRA_SXFR 0x100
	u_int8_t period; /* Period to send to SCSI target */
	char *rate;
} ahc_syncrates[] = {
	{ 0x100, 12, "20.0"  },
	{ 0x110, 15, "16.0"  },
	{ 0x120, 18, "13.4"  },
	{ 0x000, 25, "10.0"  },
	{ 0x010, 31,  "8.0"  },
	{ 0x020, 37,  "6.67" },
	{ 0x030, 43,  "5.7"  },
	{ 0x040, 50,  "5.0"  },
	{ 0x050, 56,  "4.4"  },
	{ 0x060, 62,  "4.0"  },
	{ 0x070, 68,  "3.6"  }
};

static int ahc_num_syncrates =
	sizeof(ahc_syncrates) / sizeof(ahc_syncrates[0]);

/*
 * Allocate a controller structures for a new device and initialize it.
 * ahc_reset should be called before now since we assume that the card
 * is paused.
 */
#if defined(__FreeBSD__)
struct ahc_softc *
ahc_alloc(unit, iobase, maddr, type, flags, scb_data)
	int unit;
	u_int32_t iobase;
#elif defined(__NetBSD__)
void
ahc_construct(ahc, bc, ioh, maddr, type, flags)
	struct  ahc_softc *ahc;
	bus_chipset_tag_t bc;
	bus_io_handle_t ioh;
#endif
	vm_offset_t maddr;
	ahc_type type;
	ahc_flag flags;
	struct	scb_data *scb_data;
{
	/*
	 * find unit and check we have that many defined
	 */
#if defined(__FreeBSD__)
	struct  ahc_softc *ahc;
	size_t	alloc_size;

	/*
	 * Allocate a storage area for us
	 */
	if (scb_data == NULL)
		/*
		 * We are not sharing SCB space with another controller
		 * so allocate our own SCB data space.
		 */
		alloc_size = sizeof(struct full_ahc_softc);
	else
		alloc_size = sizeof(struct ahc_softc);
	ahc = malloc(alloc_size, M_DEVBUF, M_NOWAIT);
	if (!ahc) {
		printf("ahc%d: cannot malloc!\n", unit);
		return NULL;
	}
	bzero(ahc, alloc_size);
#endif
	if (scb_data == NULL) {
		struct full_ahc_softc* full_softc = (struct full_ahc_softc*)ahc;
		ahc->scb_data = &full_softc->scb_data_storage;
		STAILQ_INIT(&ahc->scb_data->free_scbs);
	} else
		ahc->scb_data = scb_data;
	STAILQ_INIT(&ahc->waiting_scbs);
#if defined(__FreeBSD__)
	ahc->unit = unit;
#endif
#if defined(__FreeBSD__)
	ahc->baseport = iobase;
#elif defined(__NetBSD__)
	ahc->sc_bc = bc;
	ahc->sc_ioh = ioh;
#endif
	ahc->maddr = (volatile u_int8_t *)maddr;
	ahc->type = type;
	ahc->flags = flags;
	ahc->unpause = (AHC_INB(ahc, HCNTRL) & IRQMS) | INTEN;
	ahc->pause = ahc->unpause | PAUSE;

#if defined(__FreeBSD__)
	return (ahc);
#endif
}

void
ahc_free(ahc)
	struct ahc_softc *ahc;
{
#if defined(__FreeBSD__)
	free(ahc, M_DEVBUF);
	return;
#endif
}

void
#if defined(__FreeBSD__)
ahc_reset(iobase)
	u_int32_t iobase;
#elif defined(__NetBSD__)
ahc_reset(devname, bc, ioh)
	char *devname;
	bus_chipset_tag_t bc;
	bus_io_handle_t ioh;
#endif
{
        u_int8_t hcntrl;
	int wait;

	/* Retain the IRQ type accross the chip reset */
#if defined(__FreeBSD__)
	hcntrl = (inb(HCNTRL + iobase) & IRQMS) | INTEN;

	outb(HCNTRL + iobase, CHIPRST | PAUSE);
#elif defined(__NetBSD__)
	hcntrl = (bus_io_read_1(bc, ioh, HCNTRL) & IRQMS) | INTEN;

	bus_io_write_1(bc, ioh, HCNTRL, CHIPRST | PAUSE);
#endif
	/*
	 * Ensure that the reset has finished
	 */
	wait = 1000;
#if defined(__FreeBSD__)
	while (--wait && !(inb(HCNTRL + iobase) & CHIPRSTACK))
#elif defined(__NetBSD__)
	while (--wait && !(bus_io_read_1(bc, ioh, HCNTRL) & CHIPRSTACK))
#endif
		DELAY(1000);
	if (wait == 0) {
#if defined(__FreeBSD__)
		printf("ahc at 0x%lx: WARNING - Failed chip reset!  "
		       "Trying to initialize anyway.\n", iobase);
#elif defined(__NetBSD__)
		printf("%s: WARNING - Failed chip reset!  "
		       "Trying to initialize anyway.\n", devname);
#endif
	}
#if defined(__FreeBSD__)
	outb(HCNTRL + iobase, hcntrl | PAUSE);
#elif defined(__NetBSD__)
	bus_io_write_1(bc, ioh, HCNTRL, hcntrl | PAUSE);
#endif
}

/*
 * Look up the valid period to SCSIRATE conversion in our table.
 */
static void
ahc_scsirate(ahc, scsirate, period, offset, channel, target )
	struct	 ahc_softc *ahc;
	u_int8_t *scsirate;
	u_int8_t *period;
	u_int8_t *offset;
	char	 channel;
	int	 target;
{
	int i;
	u_int32_t ultra_enb_addr;
	u_int8_t  sxfrctl0;
	u_int8_t  ultra_enb;

	i = ahc_num_syncrates; /* Default to async */
	
	if (*period >= ahc_syncrates[0].period && *offset != 0) {
		for (i = 0; i < ahc_num_syncrates; i++) {

			if (*period <= ahc_syncrates[i].period) {
				/*
				 * Watch out for Ultra speeds when ultra is not
				 * enabled and vice-versa.
				 */
				if (!(ahc->type & AHC_ULTRA) 
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
				*scsirate = (ahc_syncrates[i].sxfr & 0xF0)
					  | (*offset & 0x0f);
				*period = ahc_syncrates[i].period;

				if (bootverbose) {
					printf("%s: target %d synchronous at "
					       "%sMHz, offset = 0x%x\n",
					        ahc_name(ahc), target,
						ahc_syncrates[i].rate, *offset );
				}
				break;
			}
		}
	}
	if (i >= ahc_num_syncrates) {
		/* Use asyncronous transfers. */
		*scsirate = 0;
		*period = 0;
		*offset = 0;
		if (bootverbose)
			printf("%s: target %d using asyncronous transfers\n",
			       ahc_name(ahc), target );
	}
	/*
	 * Ensure Ultra mode is set properly for
	 * this target.
	 */
	ultra_enb_addr = ULTRA_ENB;
	if (channel == 'B' || target > 7)
		ultra_enb_addr++;
	ultra_enb = AHC_INB(ahc, ultra_enb_addr);	
	sxfrctl0 = AHC_INB(ahc, SXFRCTL0);
	if (*scsirate != 0 && ahc_syncrates[i].sxfr & ULTRA_SXFR) {
		ultra_enb |= 0x01 << (target & 0x07);
		sxfrctl0 |= ULTRAEN;
	} else {
		ultra_enb &= ~(0x01 << (target & 0x07));
		sxfrctl0 &= ~ULTRAEN;
	}
	AHC_OUTB(ahc, ultra_enb_addr, ultra_enb);
	AHC_OUTB(ahc, SXFRCTL0, sxfrctl0);
}

#if defined(__NetBSD__)
int
ahcprint(aux, name)
	void *aux;
	char *name;
{
	if (name != NULL)
		printf("%s: scsibus ", name);
	return UNCONF;
}
#endif


/*
 * Attach all the sub-devices we can find
 */
int
ahc_attach(ahc)
	struct ahc_softc *ahc;
{
	struct scsibus_data *scbus;

#ifdef AHC_BROKEN_CACHE
	if (cpu_class == CPUCLASS_386)	/* doesn't have "wbinvd" instruction */
		ahc_broken_cache = 0;
#endif
	/*
	 * fill in the prototype scsi_links.
	 */
#if defined(__FreeBSD__)
	ahc->sc_link.adapter_unit = ahc->unit;
	ahc->sc_link.adapter_targ = ahc->our_id;
	ahc->sc_link.fordriver = 0;
#elif defined(__NetBSD__)
	ahc->sc_link.adapter_target = ahc->our_id;
#endif
	ahc->sc_link.adapter_softc = ahc;
	ahc->sc_link.adapter = &ahc_switch;
	ahc->sc_link.opennings = 2;
	ahc->sc_link.device = &ahc_dev;
	ahc->sc_link.flags = DEBUGLEVEL;

	if (ahc->type & AHC_TWIN) {
		/* Configure the second scsi bus */
		ahc->sc_link_b = ahc->sc_link;
#if defined(__FreeBSD__)
		ahc->sc_link_b.adapter_targ = ahc->our_id_b;
		ahc->sc_link_b.adapter_bus = 1;
		ahc->sc_link_b.fordriver = (void *)SELBUSB;
#elif defined(__NetBSD__)
		ahc->sc_link_b.adapter_target = ahc->our_id_b;
#endif
	}


#if defined(__FreeBSD__)
	/*
	 * Prepare the scsibus_data area for the upperlevel
	 * scsi code.
	 */
	scbus = scsi_alloc_bus();
	if(!scbus) 
		return 0;
	scbus->adapter_link = (ahc->flags & AHC_CHANNEL_B_PRIMARY) ?
				&ahc->sc_link_b : &ahc->sc_link;
	if (ahc->type & AHC_WIDE)
		scbus->maxtarg = 15;
	
	/*
	 * ask the adapter what subunits are present
	 */
	if (bootverbose)
		printf("ahc%d: Probing channel %c\n", ahc->unit,
			(ahc->flags & AHC_CHANNEL_B_PRIMARY) ? 'B' : 'A');
	scsi_attachdevs(scbus);
	scbus = NULL;	/* Upper-level SCSI code owns this now */

	if (ahc->type & AHC_TWIN) {
		scbus =  scsi_alloc_bus();
		if (!scbus) 
			return 0;
		scbus->adapter_link = (ahc->flags & AHC_CHANNEL_B_PRIMARY) ? 
					&ahc->sc_link : &ahc->sc_link_b;
		if (ahc->type & AHC_WIDE)
			scbus->maxtarg = 15;
		if (bootverbose)
			printf("ahc%d: Probing Channel %c\n", ahc->unit,
			       (ahc->flags & AHC_CHANNEL_B_PRIMARY) ? 'A': 'B');
		scsi_attachdevs(scbus);
		scbus = NULL;	/* Upper-level SCSI code owns this now */
	}
#elif defined(__NetBSD__)
	/*
	 * XXX - Update MI SCSI code
	 *
	 * if(ahc->type & AHC_WIDE)
	 *	max target of both channel A and B = 15;
	 */
	
	/*
	 * ask the adapter what subunits are present
	 */
	if ((ahc->flags & AHC_CHANNEL_B_PRIMARY) == 0) {
		/* make IS_SCSIBUS_B() == false, while probing channel A */
		ahc->sc_link_b.scsibus = 0xff;

		if (ahc->type & AHC_TWIN)
			printf("%s: Probing channel A\n", ahc_name(ahc));
		config_found((void *)ahc, &ahc->sc_link, ahcprint);
		if (ahc->type & AHC_TWIN) {
			printf("%s: Probing channel B\n", ahc_name(ahc));
			config_found((void *)ahc, &ahc->sc_link_b, ahcprint);
		}
	} else {
		/*
		 * if implementation of IS_SCSIBUS_B() is changed to use
		 * ahc->sc_link.scsibus, then "ahc->sc_link.scsibus = 0xff;"
		 * is needed, here.
		 */

		/* assert(ahc->type & AHC_TWIN); */
		printf("%s: Probing channel B\n", ahc_name(ahc));
		config_found((void *)ahc, &ahc->sc_link_b, ahcprint);
		printf("%s: Probing channel A\n", ahc_name(ahc));
		config_found((void *)ahc, &ahc->sc_link, ahcprint);
	}
#endif
	return 1;
}

/*
 * Catch an interrupt from the adapter
 */
#if defined(__FreeBSD__)
void
#elif defined (__NetBSD__)
int
#endif
ahc_intr(arg)
        void *arg;
{
	struct	 ahc_softc *ahc;
	u_int8_t intstat;

	ahc = (struct ahc_softc *)arg; 
	intstat = AHC_INB(ahc, INTSTAT);
	/*
	 * Is this interrupt for me? or for
	 * someone who is sharing my interrupt
	 */
	if (!(intstat & INT_PEND))
#if defined(__FreeBSD__)
		return;
#elif defined(__NetBSD__)
		return 0;
#endif

        if (intstat & BRKADRINT) {
		/*
		 * We upset the sequencer :-(
		 * Lookup the error message
		 */
		int i, error, num_errors;

		error = AHC_INB(ahc, ERROR);
		num_errors =  sizeof(hard_error)/sizeof(hard_error[0]);
		for (i = 0; error != 1 && i < num_errors; i++)
			error >>= 1;
                panic("%s: brkadrint, %s at seqaddr = 0x%x\n",
		      ahc_name(ahc), hard_error[i].errmesg,
		      (AHC_INB(ahc, SEQADDR1) << 8) |
		      AHC_INB(ahc, SEQADDR0));
        }
	if (intstat & SEQINT)
		ahc_handle_seqint(ahc, intstat);
	
	if (intstat & SCSIINT)
		ahc_handle_scsiint(ahc, intstat);

	if (intstat & CMDCMPLT) {
		struct	 scb *scb;
		u_int8_t scb_index;
		u_int8_t qoutcnt;
		int	 int_cleared;

		int_cleared = 0;
		while (qoutcnt = (AHC_INB(ahc, QOUTCNT) & ahc->qcntmask)) {
			
			for (; qoutcnt > 0; qoutcnt--) {
				scb_index = AHC_INB(ahc, QOUTFIFO);
				scb = ahc->scb_data->scbarray[scb_index];
				if (!scb || !(scb->flags & SCB_ACTIVE)) {
					printf("%s: WARNING "
					       "no command for scb %d "
					       "(cmdcmplt)\nQOUTCNT == %d\n",
						ahc_name(ahc), scb_index,
						qoutcnt);
					continue;
				}
				untimeout(ahc_timeout, (caddr_t)scb);
				/*
				 * Save off the residual if there is one.
				 */
				if (scb->hscb->residual_SG_segment_count != 0)
					ahc_calc_residual(scb);
				ahc_done(ahc, scb);
			}
			AHC_OUTB(ahc, CLRINT, CLRCMDINT);
			int_cleared++;
		} 

		if (int_cleared == 0)
			AHC_OUTB(ahc, CLRINT, CLRCMDINT);
	}

	if (ahc->waiting_scbs.stqh_first != NULL)
		ahc_run_waiting_queue(ahc);
#if defined(__NetBSD__)
	return 1;
#endif
}

static void
ahc_handle_seqint(ahc, intstat)
	struct ahc_softc *ahc;
	u_int8_t intstat;
{
	struct scb *scb;
	u_int16_t targ_mask;
	u_int8_t target = (AHC_INB(ahc, SCSIID) >> 4) & 0x0f;
	int scratch_offset = target;
	char channel = AHC_INB(ahc, SBLKCTL) & SELBUSB ? 'B': 'A';

	if (channel == 'B')
		scratch_offset += 8;
	targ_mask = (0x01 << scratch_offset); 

	switch (intstat & SEQINT_MASK) {
	case NO_MATCH:
	{
		/*
		 * This could be for a normal abort request.
		 * Figure out the SCB that we were trying to find
		 * and only give an error if we didn't ask for this
		 * to happen.
		 */
		u_int8_t scb_index;
		u_int8_t busy_scbid;

		busy_scbid = ahc_unbusy_target(ahc, target, channel);
		scb_index = AHC_INB(ahc, ARG_1);

		if (scb_index == SCB_LIST_NULL)
			/* Untagged Request */
			scb_index = busy_scbid;

		if (scb_index < ahc->scb_data->numscbs) {
			scb = ahc->scb_data->scbarray[busy_scbid];

			if (scb->hscb->control & ABORT_SCB) {
				sc_print_addr(scb->xs->sc_link);
				printf(" - SCB abort successfull\n");
				break;
			}
		}
		printf("%s:%c:%d: no active SCB for reconnecting "
		       "target - issuing ABORT\n",
		       ahc_name(ahc), channel, target);
		printf("SAVED_TCL == 0x%x\n",
		       AHC_INB(ahc, SAVED_TCL));
		break;
	}
	case NO_MATCH_BUSY:
	{
		/* The SCB that wanted to link in is in CUR_SCBID */
		u_int8_t scb_index;
		u_int8_t busy_scbindex;
		struct scb *busy_scb = NULL;

		scb_index = AHC_INB(ahc, CUR_SCBID);
		scb = ahc->scb_data->scbarray[scb_index];

		/* Find the busy SCB and unbusy this target */
		busy_scbindex = ahc_unbusy_target(ahc, scb->xs->sc_link->target,
						  channel);
		if (busy_scbindex == SCB_LIST_NULL)
			panic("%s:%c:%d: Target busy link failure, but "
			       "the target is not busy!\n",
			       ahc_name(ahc), channel, target);

		busy_scb = ahc->scb_data->scbarray[busy_scbindex];
		/* Busy SCB should be aborted */
		if ((busy_scb != NULL)
		 && (busy_scb->hscb->control & ABORT_SCB) == 0
		 && (busy_scb->hscb->control & SCB_ACTIVE) != 0) {
			panic("%s:%c:%d: Target busy link failure, but "
			       "busy SCB exists!\n",
			       ahc_name(ahc), channel, target);
		}

		if ((scb->hscb->control & ABORT_SCB) == 0) {
			/* We didn't want to abort this one too */
			AHC_OUTB(ahc, QINFIFO, scb_index);
		} else
			/* It's been aborted */
			ahc_done(ahc, scb);
		restart_sequencer(ahc);
	}
	case SEND_REJECT: 
	{
		u_int8_t rejbyte = AHC_INB(ahc, REJBYTE);
		printf("%s:%c:%d: Warning - unknown message recieved from "
		       "target (0x%x).  Rejecting\n", 
		       ahc_name(ahc), channel, target, rejbyte);
		break; 
	}
	case NO_IDENT: 
		panic("%s:%c:%d: Target did not send an IDENTIFY message. "
		      "SAVED_TCL == 0x%x\n",
		      ahc_name(ahc), channel, target,
		      AHC_INB(ahc, SAVED_TCL));
		break;
	case BAD_PHASE:
		printf("%s:%c:%d: unknown scsi bus phase.  Attempting to "
		       "continue\n", ahc_name(ahc), channel, target);	
		break; 
	case EXTENDED_MSG:
	{
		u_int8_t message_length;
		u_int8_t message_code;

		message_length = AHC_INB(ahc, MSGIN_EXT_LEN);
		message_code = AHC_INB(ahc, MSGIN_EXT_OPCODE);
		switch (message_code) {
		case MSG_EXT_SDTR:
		{
			u_int8_t period;
			u_int8_t offset;
			u_int8_t saved_offset;
			u_int8_t targ_scratch;
			u_int8_t maxoffset;
			u_int8_t rate;
			
			if (message_length != MSG_EXT_SDTR_LEN) {
				AHC_OUTB(ahc, RETURN_1, SEND_REJ);
				ahc->sdtrpending &= ~targ_mask;
				break;
			}
			period = AHC_INB(ahc, MSGIN_EXT_BYTE0);
			saved_offset = AHC_INB(ahc, MSGIN_EXT_BYTE1);
			targ_scratch = AHC_INB(ahc, TARG_SCRATCH
					       + scratch_offset);
			if (targ_scratch & WIDEXFER)
				maxoffset = MAX_OFFSET_16BIT;
			else
				maxoffset = MAX_OFFSET_8BIT;
			offset = MIN(saved_offset, maxoffset);
			ahc_scsirate(ahc, &rate, &period, &offset,
				     channel, target);
			/* Preserve the WideXfer flag */
			targ_scratch = rate | (targ_scratch & WIDEXFER);

			/*
			 * Update both the target scratch area and the
			 * current SCSIRATE.
			 */
			AHC_OUTB(ahc, TARG_SCRATCH + scratch_offset,
				 targ_scratch);
			AHC_OUTB(ahc, SCSIRATE, targ_scratch); 

			/*
			 * See if we initiated Sync Negotiation
			 * and didn't have to fall down to async
			 * transfers.
			 */
			if ((ahc->sdtrpending & targ_mask) != 0) {
				if (saved_offset == offset) {
					/*
					 * Don't send an SDTR back to
					 * the target
					 */
					AHC_OUTB(ahc, RETURN_1, 0);
				} else
					AHC_OUTB(ahc, RETURN_1, SEND_REJ);
			} else {
				/*
				 * Send our own SDTR in reply
				 */
				printf("Sending SDTR!!\n");
				ahc_construct_sdtr(ahc, /*start_byte*/0,
						   period, offset);
				AHC_OUTB(ahc, RETURN_1, SEND_MSG);
			}
			ahc->needsdtr &= ~targ_mask;
			ahc->sdtrpending &= ~targ_mask;
			break;
		}
		case MSG_EXT_WDTR:
		{
			u_int8_t scratch, bus_width;

			if (message_length != MSG_EXT_WDTR_LEN) {
				AHC_OUTB(ahc, RETURN_1, SEND_REJ);
				ahc->wdtrpending &= ~targ_mask;
				break;
			}

			bus_width = AHC_INB(ahc, MSGIN_EXT_BYTE0);
			scratch = AHC_INB(ahc, TARG_SCRATCH
					  + scratch_offset);

			if (ahc->wdtrpending & targ_mask) {
				/*
				 * Don't send a WDTR back to the
				 * target, since we asked first.
				 */
				AHC_OUTB(ahc, RETURN_1, 0);
				switch (bus_width){
				case BUS_8_BIT:
					scratch &= 0x7f;
					break;
				case BUS_16_BIT:
					if (bootverbose)
						printf("%s: target %d using "
						       "16Bit transfers\n",
						       ahc_name(ahc), target);
					scratch |= WIDEXFER;	
					break;
				case BUS_32_BIT:
					/*
					 * How can we do 32bit transfers
					 * on a 16bit bus?
					 */
					AHC_OUTB(ahc, RETURN_1, SEND_REJ);
					printf("%s: target %d requested 32Bit "
					       "transfers.  Rejecting...\n",
					       ahc_name(ahc), target);
					break;
				default:
					break;
				}
			} else {
				/*
				 * Send our own WDTR in reply
				 */
				switch (bus_width) {
				case BUS_8_BIT:
					scratch &= 0x7f;
					break;
				case BUS_32_BIT:
				case BUS_16_BIT:
					if (ahc->type & AHC_WIDE) {
						/* Negotiate 16_BITS */
						bus_width = BUS_16_BIT;
						if (bootverbose)
							printf("%s: target %d "
							       "using 16Bit "
							       "transfers\n",
							       ahc_name(ahc),
							       target);
						scratch |= WIDEXFER;	
					} else
						bus_width = BUS_8_BIT;
					break;
				default:
					break;
				}
				ahc_construct_wdtr(ahc, /*start_byte*/0,
						   bus_width);
				AHC_OUTB(ahc, RETURN_1, SEND_MSG);
			}
			
			ahc->needwdtr &= ~targ_mask;
			ahc->wdtrpending &= ~targ_mask;
			AHC_OUTB(ahc, TARG_SCRATCH + scratch_offset, scratch);
			AHC_OUTB(ahc, SCSIRATE, scratch); 
			break;
		}
		default:
			/* Unknown extended message.  Reject it. */
			AHC_OUTB(ahc, RETURN_1, SEND_REJ);
		}
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

		u_int8_t targ_scratch;

		targ_scratch = AHC_INB(ahc, TARG_SCRATCH
				       + scratch_offset);

		if (ahc->wdtrpending & targ_mask) {
			/* note 8bit xfers and clear flag */
			targ_scratch &= 0x7f;
			ahc->needwdtr &= ~targ_mask;
			ahc->wdtrpending &= ~targ_mask;
			printf("%s:%c:%d: refuses WIDE negotiation.  Using "
			       "8bit transfers\n", ahc_name(ahc),
			       channel, target);
		} else if (ahc->sdtrpending & targ_mask) {
			/* note asynch xfers and clear flag */
			targ_scratch &= 0xf0;
			ahc->needsdtr &= ~targ_mask;
			ahc->sdtrpending &= ~targ_mask;
			printf("%s:%c:%d: refuses syncronous negotiation. "
			       "Using asyncronous transfers\n",
			       ahc_name(ahc),
			       channel, target);
		} else {
			/*
			 * Otherwise, we ignore it.
			 */
#ifdef AHC_DEBUG
			if (ahc_debug & AHC_SHOWMISC)
				printf("%s:%c:%d: Message reject -- ignored\n",
				       ahc_name(ahc), channel, target);
#endif
			break;
		}
		AHC_OUTB(ahc, TARG_SCRATCH + scratch_offset, targ_scratch);
		AHC_OUTB(ahc, SCSIRATE, targ_scratch);
		break;
	}
	case BAD_STATUS:
	{
		u_int8_t scb_index;
		struct	 scsi_xfer *xs;
		struct	 hardware_scb *hscb;

		/*
		 * The sequencer will notify us when a command
		 * has an error that would be of interest to
		 * the kernel.  This allows us to leave the sequencer
		 * running in the common case of command completes
		 * without error.  The sequencer will already have
		 * dma'd the SCB back up to us, so we can reference
		 * the in kernel copy directly.
		 */

		scb_index = AHC_INB(ahc, SCB_TAG);
		scb = ahc->scb_data->scbarray[scb_index];
		hscb = scb->hscb; 

		/*
		 * Set the default return value to 0 (don't
		 * send sense).  The sense code will change
		 * this if needed and this reduces code
		 * duplication.
		 */
		AHC_OUTB(ahc, RETURN_1, 0);
		if (!(scb && (scb->flags & SCB_ACTIVE))) {
			printf("%s:%c:%d: ahc_intr - referenced scb "
			       "not valid during seqint 0x%x scb(%d)\n",
			       ahc_name(ahc),
			       channel, target, intstat,
			       scb_index);
			goto clear;
		}

		xs = scb->xs;

		xs->status = hscb->status;
		switch (hscb->status){
		case SCSI_OK:
			printf("%s: Interrupted for staus of"
			       " 0???\n", ahc_name(ahc));
			break;
		case SCSI_CHECK:
#ifdef AHC_DEBUG
			if (ahc_debug & AHC_SHOWSENSE) {
				sc_print_addr(xs->sc_link);
				printf("requests Check Status\n");
			}
#endif

			if ((xs->error == XS_NOERROR)
			 && !(scb->flags & SCB_SENSE)) {
				struct ahc_dma_seg *sg = scb->ahc_dma;
				struct scsi_sense *sc = &(scb->sense_cmd);

				/*
				 * Save off the residual if there is one.
				 */
				if (hscb->residual_SG_segment_count != 0)
					ahc_calc_residual(scb);
#ifdef AHC_DEBUG
				if (ahc_debug & AHC_SHOWSENSE) {
					sc_print_addr(xs->sc_link);
					printf("Sending Sense\n");
				}
#endif
#if defined(__FreeBSD__)
				sc->op_code = REQUEST_SENSE;
#elif defined(__NetBSD__)
				sc->opcode = REQUEST_SENSE;
#endif
				sc->byte2 =  xs->sc_link->lun << 5;
				sc->length = sizeof(struct scsi_sense_data);
				sc->control = 0;

				sg->addr = vtophys(&xs->sense);
				sg->len = sizeof(struct scsi_sense_data);

				hscb->control &= DISCENB;
				hscb->status = 0;
				hscb->SG_segment_count = 1;
				hscb->SG_list_pointer = vtophys(sg);
				hscb->data = sg->addr; 
				/* Maintain SCB_LINKED_NEXT */
				hscb->datalen &= 0xFF000000;
				hscb->datalen |= sg->len;
				hscb->cmdpointer = vtophys(sc);
				hscb->cmdlen = sizeof(*sc);

				scb->flags |= SCB_SENSE;
				AHC_OUTB(ahc, RETURN_1, SEND_SENSE);
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
			if (xs->error == XS_NOERROR)
				xs->error = XS_DRIVER_STUFFUP;
			break;
		case SCSI_BUSY:
			xs->error = XS_BUSY;
			sc_print_addr(xs->sc_link);
			printf("Target Busy\n");
			break;
		case SCSI_QUEUE_FULL:
			/*
			 * The upper level SCSI code will someday
			 * handle this properly.
			 */
			printf("Queue Full\n");
			xs->error = XS_BUSY;
			break;
		default:
			sc_print_addr(xs->sc_link);
			printf("unexpected targ_status: %x\n", hscb->status);
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
		break;
	}
	case ABORT_TAG:
	{
		u_int8_t scb_index;
		struct scsi_xfer *xs;

		scb_index = AHC_INB(ahc, SCB_TAG);
		scb = ahc->scb_data->scbarray[scb_index];
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

		scb_index = AHC_INB(ahc, SCB_TAG);
		scb = ahc->scb_data->scbarray[scb_index];
		/*
		 * This SCB had MK_MESSAGE set in its control byte,
		 * informing the sequencer that we wanted to send a
		 * special message to this target.
		 */
		if (scb->flags & SCB_DEVICE_RESET) {
			AHC_OUTB(ahc, MSG0,
				 MSG_BUS_DEV_RESET);
			AHC_OUTB(ahc, MSG_LEN, 1);
			printf("Bus Device Reset Message Sent\n");
		} else if (scb->flags & SCB_MSGOUT_WDTR) {
			ahc_construct_wdtr(ahc, AHC_INB(ahc, MSG_LEN),
					   BUS_16_BIT);
		} else if (scb->flags & SCB_MSGOUT_SDTR) {
			u_int8_t target_scratch;
			u_int8_t ultraenable;			
			int sxfr;
			int i;

			/* Pull the user defined setting */
			target_scratch = AHC_INB(ahc, TARG_SCRATCH
						 + scratch_offset);
			
			sxfr = target_scratch & SXFR;
			if (scratch_offset < 8)
				ultraenable = AHC_INB(ahc, ULTRA_ENB);
			else
				ultraenable = AHC_INB(ahc, ULTRA_ENB + 1);
			
			if (ultraenable & targ_mask)
				/* Want an ultra speed in the table */
				sxfr |= 0x100;
			
			for (i = 0; i < ahc_num_syncrates; i++)
				if (sxfr == ahc_syncrates[i].sxfr)
					break;
							
			ahc_construct_sdtr(ahc, AHC_INB(ahc, MSG_LEN),
					   ahc_syncrates[i].period,
					   (target_scratch & WIDEXFER) ?
					   MAX_OFFSET_16BIT : MAX_OFFSET_8BIT);
		} else	
			panic("ahc_intr: AWAITING_MSG for an SCB that "
			      "does not have a waiting message");
		break;
	}
	case IMMEDDONE:
	{
		/*
		 * Take care of device reset messages
		 */
		u_int8_t scbindex = AHC_INB(ahc, SCB_TAG);

		scb = ahc->scb_data->scbarray[scbindex];
		if (scb->flags & SCB_DEVICE_RESET) {
			u_int8_t targ_scratch;
			int found;
			/*
			 * Go back to async/narrow transfers and
			 * renegotiate.
			 */
			ahc_unbusy_target(ahc, target, channel);
			ahc->needsdtr |= ahc->needsdtr_orig & targ_mask;
			ahc->needwdtr |= ahc->needwdtr_orig & targ_mask;
			ahc->sdtrpending &= ~targ_mask;
			ahc->wdtrpending &= ~targ_mask;
			targ_scratch = AHC_INB(ahc, TARG_SCRATCH 
					       + scratch_offset);
			targ_scratch &= SXFR;
			AHC_OUTB(ahc, TARG_SCRATCH + scratch_offset,
				 targ_scratch);
			found = ahc_reset_device(ahc, target,
						 channel, NULL, 
						 XS_NOERROR);
			sc_print_addr(scb->xs->sc_link);
			printf("Bus Device Reset delivered. "
			       "%d SCBs aborted\n", found);
			ahc->in_timeout = FALSE;
			ahc_run_done_queue(ahc);
		} else
			panic("ahc_intr: Immediate complete for "
			      "unknown operation.");
		break;
	}
	case DATA_OVERRUN:
	{
		/*
		 * When the sequencer detects an overrun, it
		 * sets STCNT to 0x00ffffff and allows the
		 * target to complete its transfer in
		 * BITBUCKET mode.
		 */
		u_int8_t scbindex = AHC_INB(ahc, SCB_TAG);
		u_int32_t overrun;
		scb = ahc->scb_data->scbarray[scbindex];
		overrun = AHC_INB(ahc, STCNT0)
			| (AHC_INB(ahc, STCNT1) << 8)
			| (AHC_INB(ahc, STCNT2) << 16);
		overrun = 0x00ffffff - overrun;
		sc_print_addr(scb->xs->sc_link);
		printf("data overrun of %d bytes detected."
		       "  Forcing a retry.\n", overrun);
		/*
		 * Set this and it will take affect when the
		 * target does a command complete.
		 */
		scb->xs->error = XS_DRIVER_STUFFUP;
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
		printf("ahc_intr: seqint, "
		       "intstat == 0x%x, scsisigi = 0x%x\n",
		       intstat, AHC_INB(ahc, SCSISIGI));
		break;
	}
	
clear:
	/*
	 * Clear the upper byte that holds SEQINT status
	 * codes and clear the SEQINT bit.
	 */
	AHC_OUTB(ahc, CLRINT, CLRSEQINT);

	/*
	 *  The sequencer is paused immediately on
	 *  a SEQINT, so we should restart it when
	 *  we're done.
	 */
	unpause_sequencer(ahc, /*unpause_always*/TRUE);
}

static void
ahc_handle_scsiint(ahc, intstat)
	struct ahc_softc *ahc;
	u_int8_t intstat;
{
	u_int8_t scb_index;
	u_int8_t status;
	struct scb *scb;

	scb_index = AHC_INB(ahc, SCB_TAG);
	status = AHC_INB(ahc, SSTAT1);
	scb = ahc->scb_data->scbarray[scb_index];

	if (status & SCSIRSTI) {
		char channel;
		channel = (AHC_INB(ahc, SBLKCTL) & SELBUSB) ? 'B' : 'A';
		printf("%s: Someone reset channel %c\n",
			ahc_name(ahc), channel);
		ahc_reset_channel(ahc, 
				  channel,
				  NULL,
				  XS_BUSY,
				  /* Initiate Reset */FALSE);
		scb = NULL;
	} else if (!(scb && (scb->flags & SCB_ACTIVE))){
		printf("%s: ahc_intr - referenced scb not "
		       "valid during scsiint 0x%x scb(%d)\n",
			ahc_name(ahc), status, scb_index);
		AHC_OUTB(ahc, CLRSINT1, status);
		unpause_sequencer(ahc, /*unpause_always*/TRUE);
		AHC_OUTB(ahc, CLRINT, CLRSCSIINT);
		scb = NULL;
	} else if (status & SCSIPERR) {
		/*
		 * Determine the bus phase and
		 * queue an appropriate message
		 */
		char	 *phase;
		u_int8_t mesg_out = MSG_NOOP;
		u_int8_t lastphase = AHC_INB(ahc, LASTPHASE);
		struct	 scsi_xfer *xs;

		xs = scb->xs;
		sc_print_addr(xs->sc_link);

		switch (lastphase) {
		case P_DATAOUT:
			phase = "Data-Out";
			break;
		case P_DATAIN:
			phase = "Data-In";
			mesg_out = MSG_INITIATOR_DET_ERR;
			break;
		case P_COMMAND:
			phase = "Command";
			break;
		case P_MESGOUT:
			phase = "Message-Out";
			break;
		case P_STATUS:
			phase = "Status";
			mesg_out = MSG_INITIATOR_DET_ERR;
			break;
		case P_MESGIN:
			phase = "Message-In";
			mesg_out = MSG_PARITY_ERROR;
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
		 * the appropriate message.  "In" phases have set
		 * mesg_out to something other than MSG_NOP.
		 */
		if (mesg_out != MSG_NOOP) {
			AHC_OUTB(ahc, MSG0, mesg_out);
			AHC_OUTB(ahc, MSG_LEN, 1);
		} else
			/*
			 * Should we allow the target to make
			 * this decision for us?
			 */
			xs->error = XS_DRIVER_STUFFUP;
	} else if (status & SELTO) {
		struct scsi_xfer *xs;
		u_int8_t scbptr;
		u_int8_t nextscb;
		u_int8_t flags;

		xs = scb->xs;
		xs->error = XS_SELTIMEOUT;
		/*
		 * Clear any pending messages for the timed out
		 * target, and mark the target as free
		 */
		flags = AHC_INB(ahc, FLAGS);
		AHC_OUTB(ahc, MSG_LEN, 0);
		ahc_unbusy_target(ahc, xs->sc_link->target,
				  IS_SCSIBUS_B(ahc, xs->sc_link) ? 'B' : 'A');
		/* Stop the selection */
		AHC_OUTB(ahc, SCSISEQ, 0);

		AHC_OUTB(ahc, SCB_CONTROL, 0);

		AHC_OUTB(ahc, CLRSINT1, CLRSELTIMEO);

		AHC_OUTB(ahc, CLRINT, CLRSCSIINT);

		/* Shift the waiting Q forward. */
		scbptr = AHC_INB(ahc, WAITING_SCBH);
		AHC_OUTB(ahc, SCBPTR, scbptr);
		nextscb = AHC_INB(ahc, SCB_NEXT);
		AHC_OUTB(ahc, WAITING_SCBH, nextscb);

		/* Put this SCB back on the free list */
		nextscb = AHC_INB(ahc, FREE_SCBH);
		AHC_OUTB(ahc, SCB_NEXT, nextscb);
		AHC_OUTB(ahc, FREE_SCBH, scbptr);
		restart_sequencer(ahc);
	} else if (!(status & BUSFREE)) {
		sc_print_addr(scb->xs->sc_link);
		printf("Unknown SCSIINT. Status = 0x%x\n", status);
		AHC_OUTB(ahc, CLRSINT1, status);
		unpause_sequencer(ahc, /*unpause_always*/TRUE);
		AHC_OUTB(ahc, CLRINT, CLRSCSIINT);
		scb = NULL;
	}
	if (scb != NULL) {
		/* We want to process the command */
		untimeout(ahc_timeout, (caddr_t)scb);
		ahc_done(ahc, scb);
	}
}

	
/*
 * We have a scb which has been processed by the
 * adaptor, now we look to see how the operation
 * went.
 */
static void
ahc_done(ahc, scb)
	struct ahc_softc *ahc;
	struct scb *scb;
{
	struct scsi_xfer *xs = scb->xs;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("ahc_done\n"));
	/*
	 * Put the results of the operation
	 * into the xfer and call whoever started it
	 */
#if defined(__NetBSD__)
	if (xs->error != XS_NOERROR) {
		/* Don't override the error value. */
	} else if (scb->flags & SCB_ABORTED) {
		xs->error = XS_DRIVER_STUFFUP;
	} else
#endif
	if (scb->flags & SCB_SENSE)
		xs->error = XS_SENSE;
	if (scb->flags & SCB_SENTORDEREDTAG)
		ahc->in_timeout = FALSE;
#if defined(__FreeBSD__)
	if ((xs->flags & SCSI_ERR_OK) && !(xs->error == XS_SENSE)) {
		/* All went correctly  OR errors expected */
		xs->error = XS_NOERROR;
	}
#elif defined(__NetBSD__)
	/*
	 * Since NetBSD doesn't have error ignoring operation mode
	 * (SCSI_ERR_OK in FreeBSD), we don't have to care this case.
	 */
#endif
	xs->flags |= ITSDONE;
#ifdef AHC_TAGENABLE
	/*
	 * This functionality is provided by the generic SCSI layer 
	 * in FreeBSD 2.2.
	 */
	if (xs->cmd->opcode == INQUIRY && xs->error == XS_NOERROR) {
		struct scsi_inquiry_data *inq_data;
		u_int16_t mask = 0x01 << (xs->sc_link->target |
				          (scb->hscb->tcl & 0x08));
		/*
		 * Sneak a look at the results of the SCSI Inquiry
		 * command and see if we can do Tagged queing.  This
		 * should really be done by the higher level drivers.
		 */
		inq_data = (struct scsi_inquiry_data *)xs->data;
		if ((inq_data->flags & SID_CmdQue)
		 && !(ahc->tagenable & mask)) {
		        printf("%s: target %d Tagged Queuing Device\n",
				ahc_name(ahc), xs->sc_link->target);
			ahc->tagenable |= mask;
			if (ahc->scb_data->maxhscbs >= 16
			 || (ahc->flags & AHC_PAGESCBS)) {
				/* Default to 8 tags */
				xs->sc_link->opennings += 6;
			} else {
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
#endif /* AHC_TAGENABLE */
	ahc_free_scb(ahc, scb);
	scsi_done(xs);
}

/*
 * Start the board, ready for normal operation
 */
int
ahc_init(ahc)
	struct  ahc_softc *ahc;
{
	u_int8_t  scsi_conf, sblkctl, i;
	u_int16_t ultraenable = 0;
	int       max_targ = 15;
	/*
	 * Assume we have a board at this stage and it has been reset.
	 */

	/* Handle the SCBPAGING option */
#ifndef AHC_SCBPAGING_ENABLE
	ahc->flags &= ~AHC_PAGESCBS;
#endif

	/* Determine channel configuration and who we are on the scsi bus. */
	switch ((sblkctl = AHC_INB(ahc, SBLKCTL) & 0x0a)) {
	case 0:
		ahc->our_id = (AHC_INB(ahc, SCSICONF) & HSCSIID);
		ahc->flags &= ~AHC_CHANNEL_B_PRIMARY;
		if ((ahc->type & AHC_39X) != 0) {
			char channel = 'A';

			if ((ahc->flags & (AHC_CHNLB|AHC_CHNLC)) != 0)
				channel = ahc->flags & AHC_CHNLB ? 'B' : 'C';
			printf("Channel %c, SCSI Id=%d, ", channel,
				ahc->our_id);
		} else
			printf("Single Channel, SCSI Id=%d, ", ahc->our_id);
		AHC_OUTB(ahc, FLAGS, SINGLE_BUS | (ahc->flags & AHC_PAGESCBS));
		break;
	case 2:
		ahc->our_id = (AHC_INB(ahc, SCSICONF + 1) & HWSCSIID);
		ahc->flags &= ~AHC_CHANNEL_B_PRIMARY;
		if ((ahc->type & AHC_39X) != 0)  {
			char channel = 'A';

			if ((ahc->flags & (AHC_CHNLB|AHC_CHNLC)) != 0)
				channel = ahc->flags & AHC_CHNLB ? 'B' : 'C';
			printf("Wide Channel %c, SCSI Id=%d, ", channel,
				ahc->our_id);
		} else
			printf("Wide Channel, SCSI Id=%d, ", ahc->our_id);
		ahc->type |= AHC_WIDE;
		AHC_OUTB(ahc, FLAGS, WIDE_BUS | (ahc->flags & AHC_PAGESCBS));
		break;
	case 8:
		ahc->our_id = (AHC_INB(ahc, SCSICONF) & HSCSIID);
		ahc->our_id_b = (AHC_INB(ahc, SCSICONF + 1) & HSCSIID);
		printf("Twin Channel, A SCSI Id=%d, B SCSI Id=%d, ",
			ahc->our_id, ahc->our_id_b);
		ahc->type |= AHC_TWIN;
		AHC_OUTB(ahc, FLAGS, TWIN_BUS | (ahc->flags & AHC_PAGESCBS));
		break;
	default:
		printf(" Unsupported adapter type.  Ignoring\n");
		return(-1);
	}

	/* Determine the number of SCBs and initialize them */

	if (ahc->scb_data->maxhscbs == 0) {
		/* SCB 0 heads the free list */
		AHC_OUTB(ahc, FREE_SCBH, 0);
		for (i = 0; i < AHC_SCB_MAX; i++) {
			AHC_OUTB(ahc, SCBPTR, i);
			AHC_OUTB(ahc, SCB_CONTROL, i);
			if(AHC_INB(ahc, SCB_CONTROL) != i)
				break;
			AHC_OUTB(ahc, SCBPTR, 0);
			if(AHC_INB(ahc, SCB_CONTROL) != 0)
				break;
			AHC_OUTB(ahc, SCBPTR, i);

			/* Clear the control byte. */
			AHC_OUTB(ahc, SCB_CONTROL, 0);

			/* Set the next pointer */
			AHC_OUTB(ahc, SCB_NEXT, i+1);

			/* No Busy non-tagged targets yet */
			AHC_OUTB(ahc, SCB_ACTIVE0, SCB_LIST_NULL);
			AHC_OUTB(ahc, SCB_ACTIVE1, SCB_LIST_NULL);
			AHC_OUTB(ahc, SCB_ACTIVE2, SCB_LIST_NULL);
			AHC_OUTB(ahc, SCB_ACTIVE3, SCB_LIST_NULL);
		}

		/* Make that the last SCB terminates the free list */
		AHC_OUTB(ahc, SCBPTR, i-1);
		AHC_OUTB(ahc, SCB_NEXT, SCB_LIST_NULL);

		/* Ensure we clear the 0 SCB's control byte. */
		AHC_OUTB(ahc, SCBPTR, 0);
		AHC_OUTB(ahc, SCB_CONTROL, 0);

		ahc->scb_data->maxhscbs = i;
	}

	if ((ahc->scb_data->maxhscbs < AHC_SCB_MAX)
	 && (ahc->flags & AHC_PAGESCBS)) {
		ahc->scb_data->maxscbs = AHC_SCB_MAX;
		printf("%d/%d SCBs\n", ahc->scb_data->maxhscbs,
			ahc->scb_data->maxscbs);
	} else {
		ahc->scb_data->maxscbs = ahc->scb_data->maxhscbs;
		ahc->flags &= ~AHC_PAGESCBS;
		printf("%d SCBs\n", ahc->scb_data->maxhscbs);
	}

#ifdef AHC_DEBUG
	if (ahc_debug & AHC_SHOWMISC) {
		printf("%s: hardware scb %d bytes; kernel scb %d bytes; "
		       "ahc_dma %d bytes\n",
			ahc_name(ahc),
		        sizeof(struct hardware_scb),
			sizeof(struct scb),
			sizeof(struct ahc_dma_seg));
	}
#endif /* AHC_DEBUG */

	/* Set the SCSI Id, SXFRCTL0, SXFRCTL1, and SIMODE1, for both channels*/
	if (ahc->type & AHC_TWIN) {
		/*
		 * The device is gated to channel B after a chip reset,
		 * so set those values first
		 */
		AHC_OUTB(ahc, SCSIID, ahc->our_id_b);
		scsi_conf = AHC_INB(ahc, SCSICONF + 1);
		AHC_OUTB(ahc, SXFRCTL1, (scsi_conf & (ENSPCHK|STIMESEL))
					| ENSTIMER|ACTNEGEN|STPWEN);
		AHC_OUTB(ahc, SIMODE1, ENSELTIMO|ENSCSIRST|ENSCSIPERR);
		if (ahc->type & AHC_ULTRA)
			AHC_OUTB(ahc, SXFRCTL0, DFON|SPIOEN|ULTRAEN);
		else
			AHC_OUTB(ahc, SXFRCTL0, DFON|SPIOEN);

		if (scsi_conf & RESET_SCSI) {
			/* Reset the bus */
			if (bootverbose)
				printf("%s: Reseting Channel B\n",
				       ahc_name(ahc));
			AHC_OUTB(ahc, SCSISEQ, SCSIRSTO);
			DELAY(1000);
			AHC_OUTB(ahc, SCSISEQ, 0);

			/* Ensure we don't get a RSTI interrupt from this */
			AHC_OUTB(ahc, CLRSINT1, CLRSCSIRSTI);
			AHC_OUTB(ahc, CLRINT, CLRSCSIINT);
		}

		/* Select Channel A */
		AHC_OUTB(ahc, SBLKCTL, 0);
	}
	AHC_OUTB(ahc, SCSIID, ahc->our_id);
	scsi_conf = AHC_INB(ahc, SCSICONF);
	AHC_OUTB(ahc, SXFRCTL1, (scsi_conf & (ENSPCHK|STIMESEL))
				| ENSTIMER|ACTNEGEN|STPWEN);
	AHC_OUTB(ahc, SIMODE1, ENSELTIMO|ENSCSIRST|ENSCSIPERR);
	if (ahc->type & AHC_ULTRA)
		AHC_OUTB(ahc, SXFRCTL0, DFON|SPIOEN|ULTRAEN);
	else
		AHC_OUTB(ahc, SXFRCTL0, DFON|SPIOEN);

	if (scsi_conf & RESET_SCSI) {
		/* Reset the bus */
		if (bootverbose)
			printf("%s: Reseting Channel A\n", ahc_name(ahc));

		AHC_OUTB(ahc, SCSISEQ, SCSIRSTO);
		DELAY(1000);
		AHC_OUTB(ahc, SCSISEQ, 0);

		/* Ensure we don't get a RSTI interrupt from this */
		AHC_OUTB(ahc, CLRSINT1, CLRSCSIRSTI);
		AHC_OUTB(ahc, CLRINT, CLRSCSIINT);
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
	if (ahc->flags & AHC_USEDEFAULTS) {
		printf("%s: Host Adapter Bios disabled.  Using default SCSI "
			"device parameters\n", ahc_name(ahc));
		ahc->discenable = 0xff;
	} else
		ahc->discenable = ~((AHC_INB(ahc, DISC_DSB + 1) << 8)
				   | AHC_INB(ahc, DISC_DSB));

	if (!(ahc->type & (AHC_WIDE|AHC_TWIN)))
		max_targ = 7;

	for (i = 0; i <= max_targ; i++) {
		u_int8_t target_settings;
		if (ahc->flags & AHC_USEDEFAULTS) {
			target_settings = 0; /* 10MHz */
			ahc->needsdtr_orig |= (0x01 << i);
			ahc->needwdtr_orig |= (0x01 << i);
		} else {
			/* Take the settings leftover in scratch RAM. */
			target_settings = AHC_INB(ahc, TARG_SCRATCH + i);

			if (target_settings & 0x0f) {
				ahc->needsdtr_orig |= (0x01 << i);
				/*Default to a asyncronous transfers(0 offset)*/
				target_settings &= 0xf0;
			}
			if (target_settings & 0x80) {
				ahc->needwdtr_orig |= (0x01 << i);
				/*
				 * We'll set the Wide flag when we
				 * are successful with Wide negotiation.
				 * Turn it off for now so we aren't
				 * confused.
				 */
				target_settings &= 0x7f;
			}
			if (ahc->type & AHC_ULTRA) {
				/*
				 * Enable Ultra for any target that
				 * has a valid ultra syncrate setting.
				 */
				u_int8_t rate = target_settings & 0x70;
				if (rate == 0x00 || rate == 0x10 ||
				    rate == 0x20 || rate == 0x40) {
					if (rate == 0x40) {
						/* Treat 10MHz specially */
						target_settings &= ~0x70;
					} else
						ultraenable |= (0x01 << i);
				}
			}
		}
		AHC_OUTB(ahc, TARG_SCRATCH+i,target_settings);
	}
	/*
	 * If we are not a WIDE device, forget WDTR.  This
	 * makes the driver work on some cards that don't
	 * leave these fields cleared when the BIOS is not
	 * installed.
	 */
	if ((ahc->type & AHC_WIDE) == 0)
		ahc->needwdtr_orig = 0;
	ahc->needsdtr = ahc->needsdtr_orig;
	ahc->needwdtr = ahc->needwdtr_orig;
	ahc->sdtrpending = 0;
	ahc->wdtrpending = 0;
	ahc->tagenable = 0;
	ahc->orderedtag = 0;

	AHC_OUTB(ahc, ULTRA_ENB, ultraenable & 0xff);
	AHC_OUTB(ahc, ULTRA_ENB + 1, (ultraenable >> 8) & 0xff);

#ifdef AHC_DEBUG
	/* How did we do? */
	if (ahc_debug & AHC_SHOWMISC)
		printf("NEEDSDTR == 0x%x\nNEEDWDTR == 0x%x\n"
			"DISCENABLE == 0x%x\n", ahc->needsdtr, 
			ahc->needwdtr, ahc->discenable);
#endif
	/*
	 * Set the number of availible hardware SCBs
	 */
	AHC_OUTB(ahc, SCBCOUNT, ahc->scb_data->maxhscbs);

	/*
	 * 2's compliment of maximum tag value
	 */
	i = ahc->scb_data->maxscbs;
	AHC_OUTB(ahc, COMP_SCBCOUNT, -i & 0xff);

	/*
	 * Allocate enough "hardware scbs" to handle
	 * the maximum number of concurrent transactions
	 * we can have active.  We have to use contigmalloc
	 * if this array crosses a page boundary since the
	 * sequencer depends on this array being physically
	 * contiguous.
	 */
	if (ahc->scb_data->hscbs == NULL) {
		size_t array_size;
		u_int32_t hscb_physaddr;

		array_size = ahc->scb_data->maxscbs*sizeof(struct hardware_scb);
		if (array_size > PAGE_SIZE) {
			ahc->scb_data->hscbs = (struct hardware_scb *)
				     contigmalloc(array_size, M_DEVBUF,
				     M_NOWAIT, 0ul, 0xffffffff, PAGE_SIZE,
				     0x10000);
		} else {
			ahc->scb_data->hscbs = (struct hardware_scb *)
				     malloc(array_size, M_DEVBUF, M_NOWAIT);
		}
	
		if (ahc->scb_data->hscbs == NULL) {
			printf("%s: unable to allocate hardware SCB array.  "
			       "Failing attach\n");
			return (-1);
		}

		/* Tell the sequencer where it can find the hscb array. */
		hscb_physaddr = vtophys(ahc->scb_data->hscbs);
		AHC_OUTB(ahc, HSCB_ADDR0, hscb_physaddr & 0xFF);
		AHC_OUTB(ahc, HSCB_ADDR1, (hscb_physaddr >> 8)& 0xFF);
		AHC_OUTB(ahc, HSCB_ADDR2, (hscb_physaddr >> 16)& 0xFF);
		AHC_OUTB(ahc, HSCB_ADDR3, (hscb_physaddr >> 24)& 0xFF);
	}

	/*
	 * Q-Full-Count.  Some cards have more Q space
	 * then SCBs.
	 */
	if (ahc->type & AHC_AIC7770) {
		ahc->qfullcount = 4;
		ahc->qcntmask = 0x07;
	} else if (ahc->type & AHC_AIC7850) {
		ahc->qfullcount = 8;
		ahc->qcntmask = 0x0f;
	} else if (ahc->scb_data->maxhscbs == 255) {
		/* 7870/7880 with external SRAM */
		ahc->qfullcount = 255;
		ahc->qcntmask = 0xff;
	} else {
		/* 7870/7880 */
		ahc->qfullcount = 16;
		ahc->qcntmask = 0x1f;
	}

	/*
	 * QCount mask to deal with broken aic7850s that
	 * sporatically get garbage in the upper bits of
	 * their QCount registers.
	 *
	 * QFullCount to guard against overflowing the
	 * QINFIFO or QOUTFIFO when we are paging SCBs.
	 */
	AHC_OUTB(ahc, QCNTMASK, ahc->qcntmask);

	AHC_OUTB(ahc, QFULLCNT, ahc->qfullcount);

	/* We don't have any waiting selections */
	AHC_OUTB(ahc, WAITING_SCBH, SCB_LIST_NULL);

	/* Our disconnection list is empty too */
	AHC_OUTB(ahc, DISCONNECTED_SCBH, SCB_LIST_NULL);

	/* Message out buffer starts empty */
	AHC_OUTB(ahc, MSG_LEN, 0x00);

	/*
	 * Load the Sequencer program and Enable the adapter
	 * in "fast" mode.
         */
	if (bootverbose)
		printf("%s: Downloading Sequencer Program...",
		       ahc_name(ahc));

	ahc_loadseq(ahc);

	if (bootverbose)
		printf("Done\n");

	AHC_OUTB(ahc, SEQCTL, FASTMODE);

	unpause_sequencer(ahc, /*unpause_always*/TRUE);

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
#if defined(__NetBSD__)
	minphys(bp);
#endif
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
	struct	  scb *scb;
	struct	  hardware_scb *hscb;
	struct	  ahc_softc *ahc;
	u_int16_t mask;
	int	  flags;
	int	  s;

	ahc = (struct ahc_softc *)xs->sc_link->adapter_softc;
	mask = (0x01 << (xs->sc_link->target
		| (IS_SCSIBUS_B(ahc, xs->sc_link) ? SELBUSB : 0)));
	SC_DEBUG(xs->sc_link, SDEV_DB2, ("ahc_scsi_cmd\n"));
	flags = xs->flags;
	/*
	 * get an scb to use. If the transfer
	 * is from a buf (possibly from interrupt time)
	 * then we can't allow it to sleep
	 */
	if ((scb = ahc_get_scb(ahc, flags)) == NULL) {
		xs->error = XS_DRIVER_STUFFUP;
		return (TRY_AGAIN_LATER);
	}
	hscb = scb->hscb;
	SC_DEBUG(xs->sc_link, SDEV_DB3, ("start scb(%p)\n", scb));
	scb->xs = xs;

	/*
	 * Put all the arguments for the xfer in the scb
	 */
	if (ahc->discenable & mask)
		hscb->control |= DISCENB;
	
	if (flags & SCSI_RESET) {
		scb->flags |= SCB_DEVICE_RESET|SCB_IMMED;
		hscb->control |= MK_MESSAGE;		
	} else if ((ahc->needwdtr & mask) && !(ahc->wdtrpending & mask)) {
		ahc->wdtrpending |= mask;
		hscb->control |= MK_MESSAGE;
		scb->flags |= SCB_MSGOUT_WDTR;
	} else if((ahc->needsdtr & mask) && !(ahc->sdtrpending & mask)) {
		ahc->sdtrpending |= mask;
		hscb->control |= MK_MESSAGE;		
		scb->flags |= SCB_MSGOUT_SDTR;
	} else if (ahc->orderedtag & mask) {
		/* XXX this should be handled by the upper SCSI layer */
		printf("Ordered Tag sent\n");
		hscb->control |= MSG_ORDERED_Q_TAG;
		ahc->orderedtag &= ~mask;
	} else if (hscb->control & DISCENB) {
                if (ahc->tagenable & mask)
                        hscb->control |= TAG_ENB;
	}
	hscb->tcl = ((xs->sc_link->target << 4) & 0xF0)
		  | (IS_SCSIBUS_B(ahc,xs->sc_link)? SELBUSB : 0)
		  | (xs->sc_link->lun & 0x07);
	hscb->cmdlen = xs->cmdlen;
	hscb->cmdpointer = vtophys(xs->cmd);
	xs->resid = 0;
	xs->status = 0;

	/* Only use S/G if non-zero length */
	if (xs->datalen) {
		int		seg;
		u_int32_t	datalen;
		vm_offset_t	vaddr;
		u_int32_t	paddr;
		u_int32_t	nextpaddr;
		struct		ahc_dma_seg *sg;
	
		seg = 0;
		datalen = xs->datalen;
		vaddr = (vm_offset_t)xs->data;
		paddr = vtophys(vaddr);
		sg = scb->ahc_dma;
		hscb->SG_list_pointer = vtophys(sg);

		while ((datalen > 0) && (seg < AHC_NSEG)) {
			/* put in the base address and length */
			sg->addr = paddr;
			sg->len = 0;

			/* do it at least once */
			nextpaddr = paddr;

			while ((datalen > 0) && (paddr == nextpaddr)) {
				u_int32_t	size;
				/*
				 * This page is contiguous (physically)
				 * with the the last, just extend the
				 * length
				 */
				/* how far to the end of the page */
				nextpaddr = (paddr & (~PAGE_MASK)) + PAGE_SIZE;

				/*
				 * Compute the maximum size
				 */
				size = nextpaddr - paddr;
				if (size > datalen)
				        size = datalen;

				sg->len += size;
				vaddr   += size;
				datalen -= size;
				if (datalen > 0)
					paddr = vtophys(vaddr);
			}
			/*
			 * next page isn't contiguous, finish the seg
			 */
			seg++;
			sg++;
		}
		hscb->SG_segment_count = seg;

		/* Copy the first SG into the data pointer area */
		hscb->data = scb->ahc_dma->addr;
		hscb->datalen = scb->ahc_dma->len | (SCB_LIST_NULL << 24);
		if (datalen) { 
			/* there's still data, must have run out of segs! */
			printf("%s: ahc_scsi_cmd: more than %d DMA segs\n",
				ahc_name(ahc), AHC_NSEG);
			xs->error = XS_DRIVER_STUFFUP;
			ahc_free_scb(ahc, scb);
			return (COMPLETE);
		}
#ifdef AHC_BROKEN_CACHE
		if (ahc_broken_cache)
			INVALIDATE_CACHE();
#endif
	} else {
		/*
		 * No data xfer, use non S/G values
	 	 */
		hscb->SG_segment_count = 0;
		hscb->SG_list_pointer = 0;
		hscb->data = 0;
		hscb->datalen = (SCB_LIST_NULL << 24);
	}

#ifdef AHC_DEBUG
	if((ahc_debug & AHC_SHOWSCBS) && (xs->sc_link->target == DEBUGTARGET))
		ahc_print_scb(scb);
#endif
	s = splbio();

	STAILQ_INSERT_TAIL(&ahc->waiting_scbs, scb, links);

	scb->flags |= SCB_ACTIVE;

	ahc_run_waiting_queue(ahc);

	if ((flags & SCSI_NOMASK) == 0) {
		timeout(ahc_timeout, (caddr_t)scb, (xs->timeout * hz) / 1000);
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
	} while ((xs->flags & ITSDONE) == 0);  /* a non command complete intr */
	splx(s); 
	return (COMPLETE);
}

/*
 * Look for space in the QINFIFO and queue as many SCBs in the waiting
 * queue as possible.  Assumes that it is called at splbio().
 */
static void
ahc_run_waiting_queue(ahc)
	struct	ahc_softc *ahc;
{
	struct scb *scb;

	/*
	 * On aic78X0 chips, we rely on Auto Access Pause (AAP)
	 * instead of doing an explicit pause/unpause.
	 */
	if ((ahc->type & AHC_AIC78X0) == 0)
		pause_sequencer(ahc);

	while ((scb = ahc->waiting_scbs.stqh_first) != NULL) {

		if (ahc->curqincnt >= ahc->qfullcount) {
			ahc->curqincnt = AHC_INB(ahc, QINCNT) & ahc->qcntmask;
			if (ahc->curqincnt >= ahc->qfullcount)
				/* Still no space */
				break;
		}
		STAILQ_REMOVE_HEAD(&ahc->waiting_scbs, links);
		AHC_OUTB(ahc, QINFIFO, scb->hscb->tag);

		if ((ahc->flags & AHC_PAGESCBS) != 0)
			/*
			 * We only care about this statistic when paging
			 * since it is impossible to overflow the qinfifo
			 * in the non-paging case.
			 */
			ahc->curqincnt++;
	}
	if ((ahc->type & AHC_AIC78X0) == 0)
		unpause_sequencer(ahc, /*Unpause always*/FALSE);
}

/*
 * A scb (and hence an scb entry on the board is put onto the
 * free list.
 */
static void
ahc_free_scb(ahc, scb)
	struct	ahc_softc *ahc;
	struct	scb *scb;
{       
	struct hardware_scb *hscb;
	int opri;

	hscb = scb->hscb;

	opri = splbio();

	/* Clean up for the next user */
	scb->flags = SCB_FREE;
	hscb->control = 0;
	hscb->status = 0;

	STAILQ_INSERT_HEAD(&ahc->scb_data->free_scbs, scb, links);
	if (scb->links.stqe_next == NULL) {
		/*
		 * If there were no SCBs availible, wake anybody waiting
		 * for one to come free.
		 */
		wakeup((caddr_t)&ahc->scb_data->free_scbs);
	}
#ifdef AHC_DEBUG
	ahc->activescbs--;
#endif
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
	struct  ahc_softc *ahc;
	u_int32_t flags;
{
	struct scb *scbp;
	int opri;

	opri = splbio();
	/*
	 * If we can and have to, sleep waiting for one to come free
	 * but only if we can't allocate a new one.
	 */
	while (1) {
		if ((scbp = ahc->scb_data->free_scbs.stqh_first)) {
			STAILQ_REMOVE_HEAD(&ahc->scb_data->free_scbs, links);
		} else if(ahc->scb_data->numscbs < ahc->scb_data->maxscbs) {
			scbp = ahc_alloc_scb(ahc);
			if (scbp == NULL)
				printf("%s: Can't malloc SCB\n", ahc_name(ahc));
		} else if ((flags & SCSI_NOSLEEP) == 0) {
			tsleep((caddr_t)&ahc->scb_data->free_scbs, PRIBIO,
				"ahcscb", 0);
			continue;
		}
		break;
	}

#ifdef AHC_DEBUG
	if (scbp) {
		ahc->activescbs++;
		if((ahc_debug & AHC_SHOWSCBCNT)
		  && (ahc->activescbs == ahc->scb_data->maxhscbs))
			printf("%s: Max SCBs active\n", ahc_name(ahc));
	}
#endif

	splx(opri);

	return (scbp);
}


static struct scb *
ahc_alloc_scb(ahc)
	struct	ahc_softc *ahc;
{
	static	struct ahc_dma_seg *next_sg_array = NULL;
	static	int sg_arrays_free = 0;
	struct	scb *newscb;

	newscb = (struct scb *) malloc(sizeof(struct scb), M_DEVBUF, M_NOWAIT);
	if (newscb != NULL) {
		bzero(newscb, sizeof(struct scb));
		if (next_sg_array == NULL) {
			size_t	alloc_size = sizeof(struct ahc_dma_seg)
					     * AHC_NSEG; 
			sg_arrays_free = PAGE_SIZE / alloc_size;
			alloc_size *= sg_arrays_free;
			if (alloc_size == 0)
				panic("%s: SG list doesn't fit in a page",
				      ahc_name(ahc));
			next_sg_array = (struct ahc_dma_seg *)
					malloc(alloc_size, M_DEVBUF, M_NOWAIT);
		}
		if (next_sg_array != NULL) {
			struct hardware_scb *hscb;

			newscb->ahc_dma = next_sg_array;
			sg_arrays_free--;
			if (sg_arrays_free == 0)
				next_sg_array = NULL;
			else
				next_sg_array = &next_sg_array[AHC_NSEG];
			hscb = &ahc->scb_data->hscbs[ahc->scb_data->numscbs];
			newscb->hscb = hscb;
			hscb->control = 0;
			hscb->status = 0;
			hscb->tag = ahc->scb_data->numscbs;
			hscb->residual_data_count[2] = 0;
			hscb->residual_data_count[1] = 0;
			hscb->residual_data_count[0] = 0;
			hscb->residual_SG_segment_count = 0;
			ahc->scb_data->numscbs++;
			/*
			 * Place in the scbarray
			 * Never is removed.
			 */
			ahc->scb_data->scbarray[hscb->tag] = newscb;
		} else {
			free(newscb, M_DEVBUF);
			newscb = NULL;
		}
	}
	return newscb;
}

static void ahc_loadseq(ahc)
	struct ahc_softc *ahc;
{
        static u_char seqprog[] = {
#               include "aic7xxx_seq.h"
	};

	AHC_OUTB(ahc, SEQCTL, PERRORDIS|SEQRESET|LOADRAM);

	AHC_OUTSB(ahc, SEQRAM, seqprog, sizeof(seqprog));

	do {
		AHC_OUTB(ahc, SEQCTL, SEQRESET|FASTMODE);
	} while ((AHC_INB(ahc, SEQADDR0) != 0)
	      || (AHC_INB(ahc, SEQADDR1) != 0));
}

/*
 * Function to poll for command completion when
 * interrupts are disabled (crash dumps)
 */
static int
ahc_poll(ahc, wait)
	struct	ahc_softc *ahc;
	int	wait; /* in msec */
{
	while (--wait) {
		DELAY(1000);
		if (AHC_INB(ahc, INTSTAT) & INT_PEND)
			break;
	} if (wait == 0) {
		printf("%s: board is not responding\n", ahc_name(ahc));
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
	struct	ahc_softc *ahc;
	int	s, found;
	u_char	bus_state;

	s = splbio();

	if (!(scb->flags & SCB_ACTIVE)) {
		/* Previous timeout took care of me already */
		splx(s);
		return;
	}

	ahc = (struct ahc_softc *)scb->xs->sc_link->adapter_softc;

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
			panic("%s: Timed-out command times out "
				"again\n", ahc_name(ahc));
		} else if ((scb->flags & (SCB_ABORTED | SCB_DEVICE_RESET
					  | SCB_SENTORDEREDTAG)) == 0) {
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
	pause_sequencer(ahc);

	sc_print_addr(scb->xs->sc_link);
	printf("timed out ");
	/*
	 * Take a snapshot of the bus state and print out
	 * some information so we can track down driver bugs.
	 */
	bus_state = AHC_INB(ahc, LASTPHASE);

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

	printf(", SCSISIGI == 0x%x\n", AHC_INB(ahc, SCSISIGI));

	/* Decide our course of action */

	if (scb->flags & SCB_ABORTED) {
		/*
		 * Been down this road before.
		 * Do a full bus reset.
		 */
		char channel = (scb->hscb->tcl & SELBUSB)
			   ? 'B': 'A';	
		found = ahc_reset_channel(ahc, channel, scb,
					  XS_TIMEOUT, /*Initiate Reset*/TRUE);
		printf("%s: Issued Channel %c Bus Reset. "
		       "%d SCBs aborted\n", ahc_name(ahc), channel, found);
		ahc->in_timeout = FALSE;
	} else if ((scb->hscb->control & TAG_ENB) != 0
		&& (scb->flags & SCB_SENTORDEREDTAG) == 0) {
		/*
		 * We could be starving this command
		 * try sending an ordered tag command
		 * to the target we come from.
		 */
		scb->flags |= SCB_SENTORDEREDTAG;
		ahc->orderedtag |= 0xFF;
		timeout(ahc_timeout, (caddr_t)scb, (5 * hz));
		unpause_sequencer(ahc, /*unpause_always*/FALSE);
		printf("Ordered Tag queued\n");
	} else {
		/*
		 * Send an Abort Message:
		 * The target that is holding up the bus may not
		 * be the same as the one that triggered this timeout
		 * (different commands have different timeout lengths).
		 * Our strategy here is to queue an abort message
		 * to the timed out target if it is disconnected.
		 * Otherwise, if we have an active target we stuff the
		 * message buffer with an abort message and assert ATN
		 * in the hopes that the target will let go of the bus
		 * and go to the mesgout phase.  If this fails, we'll
		 * get another timeout 2 seconds later which will attempt
		 * a bus reset.
		 */
		u_int8_t saved_scbptr;
		u_int8_t active_scb_index;
		struct scb *active_scb;

		saved_scbptr = AHC_INB(ahc, SCBPTR);
		active_scb_index = AHC_INB(ahc, SCB_TAG);
		active_scb = ahc->scb_data->scbarray[active_scb_index];

		if (bus_state != 0) {
			/* Send the abort to the active SCB */
			AHC_OUTB(ahc, MSG_LEN, 1);
			AHC_OUTB(ahc, MSG0,
				 (active_scb->hscb->control & TAG_ENB) == 0 ?
				 MSG_ABORT : MSG_ABORT_TAG);
			AHC_OUTB(ahc, SCSISIGO, bus_state|ATNO);
			sc_print_addr(active_scb->xs->sc_link);
			printf("abort message in message buffer\n");
			active_scb->flags |= SCB_ABORTED;
			if (active_scb != scb) {
				untimeout(ahc_timeout, 
					  (caddr_t)active_scb);
				/* Give scb a new lease on life */
				timeout(ahc_timeout, (caddr_t)scb, 
					(scb->xs->timeout * hz) / 1000);
			}
			timeout(ahc_timeout, (caddr_t)active_scb, (2 * hz));
			unpause_sequencer(ahc, /*unpause_always*/FALSE);
		} else {
			u_int8_t hscb_index;
			int	 disconnected;

			disconnected = FALSE;
			hscb_index = find_scb(ahc, scb);
			if (hscb_index == SCB_LIST_NULL)
				disconnected = TRUE;
			else {
				AHC_OUTB(ahc, SCBPTR, hscb_index);
				if (AHC_INB(ahc, SCB_CONTROL) & DISCONNECTED)
					disconnected = TRUE;
			}

			scb->flags |= SCB_ABORTED;
			if (disconnected) {
				/* Simply set the ABORT_SCB control bit */
				scb->hscb->control |= ABORT_SCB;
				if (hscb_index != SCB_LIST_NULL)
					AHC_OUTB(ahc, SCB_CONTROL, ABORT_SCB);
				timeout(ahc_timeout, (caddr_t)scb, (2 * hz));
			}
			AHC_OUTB(ahc, SCBPTR, saved_scbptr);
			unpause_sequencer(ahc, /*unpause_always*/FALSE);
			if (!disconnected)
				/* Go "immediatly" to the bus reset */
				timeout(ahc_timeout, (caddr_t)scb, hz / 2);	
		}
	}
	splx(s);
}

/*
 * Look through the SCB array of the card and attempt to find the
 * hardware SCB that corresponds to the passed in SCB.  Return
 * SCB_LIST_NULL if unsuccessful.  This routine assumes that the
 * card is already paused.
 */
static u_int8_t
find_scb(ahc, scb)
	struct ahc_softc *ahc;
	struct scb *scb;
{
	u_int8_t saved_scbptr;
	u_int8_t curindex;

	saved_scbptr = AHC_INB(ahc, SCBPTR);
	curindex = 0;
	for (curindex = 0; curindex < ahc->scb_data->maxhscbs; curindex++) {
		AHC_OUTB(ahc, SCBPTR, curindex);
		if (AHC_INB(ahc, SCB_TAG) == scb->hscb->tag)
			break;
	}
	AHC_OUTB(ahc, SCBPTR, saved_scbptr);
	if (curindex > ahc->scb_data->maxhscbs)
		curindex = SCB_LIST_NULL;

	return curindex;
}

/*
 * The device at the given target/channel has been reset.  Abort 
 * all active and queued scbs for that target/channel. 
 */
static int
ahc_reset_device(ahc, target, channel, timedout_scb, xs_error)
	struct ahc_softc *ahc;
	int target;
	char channel;
	struct scb *timedout_scb;
	u_int32_t xs_error;
{
        struct scb *scbp;
	u_char active_scb;
	int i = 0;
	int found = 0;

	/* restore this when we're done */
	active_scb = AHC_INB(ahc, SCBPTR);

	/*
	 * Search the QINFIFO.
	 */
	{
		u_int8_t saved_queue[AHC_SCB_MAX];
		u_int8_t queued = AHC_INB(ahc, QINCNT) & ahc->qcntmask;

		for (i = 0; i < (queued - found); i++) {
			saved_queue[i] = AHC_INB(ahc, QINFIFO);
			scbp = ahc->scb_data->scbarray[saved_queue[i]];
			if (ahc_match_scb (scbp, target, channel)) {
				/*
				 * We found an scb that needs to be aborted.
				 */
				scbp->flags = SCB_ABORTED|SCB_QUEUED_FOR_DONE;
				scbp->xs->error |= xs_error;
				if(scbp != timedout_scb)
					untimeout(ahc_timeout, (caddr_t)scbp);
				i--;
				found++;
			}
		}
		/* Now put the saved scbs back. */
		for (queued = 0; queued < i; queued++) {
			AHC_OUTB(ahc, QINFIFO, saved_queue[queued]);
		}
	}

	/*
	 * Search waiting for selection list.
	 */
	{
		u_int8_t next, prev;

		next = AHC_INB(ahc, WAITING_SCBH);  /* Start at head of list. */
		prev = SCB_LIST_NULL;

		while (next != SCB_LIST_NULL) {
			AHC_OUTB(ahc, SCBPTR, next);
			scbp = ahc->scb_data->scbarray[AHC_INB(ahc, SCB_TAG)];
			if (ahc_match_scb(scbp, target, channel)) {
				next = ahc_abort_wscb(ahc, scbp, next, prev,
						      timedout_scb, xs_error);
				found++;
			} else {
				prev = next;
				next = AHC_INB(ahc, SCB_NEXT);
			}
		}
	}
	/*
	 * Go through the entire SCB array now and look for 
	 * commands for this target that are active.  These
	 * are other (most likely tagged) commands that 
	 * were disconnected when the reset occured.
	 */
	for (i = 0; i < ahc->scb_data->numscbs; i++) {
		scbp = ahc->scb_data->scbarray[i];
		if ((scbp->flags & SCB_ACTIVE)
		  && ahc_match_scb(scbp, target, channel)) {
			/* Ensure the target is "free" */
			ahc_unbusy_target(ahc, target, channel);
			scbp->flags = SCB_ABORTED|SCB_QUEUED_FOR_DONE;
			scbp->xs->error |= xs_error;
			if (scbp != timedout_scb)
				untimeout(ahc_timeout, (caddr_t)scbp);
			found++;
		}
	}			
	AHC_OUTB(ahc, SCBPTR, active_scb);
	return found;
}

/*
 * Manipulate the waiting for selection list and return the
 * scb that follows the one that we remove.
 */
static u_char
ahc_abort_wscb (ahc, scbp, scbpos, prev, timedout_scb, xs_error)
	struct	  ahc_softc *ahc;
	struct	  scb *scbp;
	u_int8_t  scbpos;
	u_int8_t  prev;
	struct	  scb *timedout_scb;
	u_int32_t xs_error;
{       
	u_int8_t curscb, next;
	int target = ((scbp->hscb->tcl >> 4) & 0x0f);
	char channel = (scbp->hscb->tcl & SELBUSB) ? 'B' : 'A';
	/*
	 * Select the SCB we want to abort and
	 * pull the next pointer out of it.
	 */
	curscb = AHC_INB(ahc, SCBPTR);
	AHC_OUTB(ahc, SCBPTR, scbpos);
	next = AHC_INB(ahc, SCB_NEXT);

	/* Clear the necessary fields */
	AHC_OUTB(ahc, SCB_CONTROL, 0);
	AHC_OUTB(ahc, SCB_NEXT, SCB_LIST_NULL);
	ahc_unbusy_target(ahc, target, channel);

	/* update the waiting list */
	if (prev == SCB_LIST_NULL) 
		/* First in the list */
		AHC_OUTB(ahc, WAITING_SCBH, next); 
	else {
		/*
		 * Select the scb that pointed to us 
		 * and update its next pointer.
		 */
		AHC_OUTB(ahc, SCBPTR, prev);
		AHC_OUTB(ahc, SCB_NEXT, next);
	}
	/*
	 * Point us back at the original scb position
	 * and inform the SCSI system that the command
	 * has been aborted.
	 */
	AHC_OUTB(ahc, SCBPTR, curscb);
	scbp->flags = SCB_ABORTED|SCB_QUEUED_FOR_DONE;
	scbp->xs->error |= xs_error;
	if (scbp != timedout_scb)
		untimeout(ahc_timeout, (caddr_t)scbp);
	return next;
}

static u_int8_t
ahc_unbusy_target(ahc, target, channel)
	struct	ahc_softc *ahc;
	int	target;
	char	channel;
{
	u_int8_t  active_scb;
	u_int8_t  info_scb;
	u_int8_t  busy_scbid;
	u_int32_t scb_offset;

	info_scb = target / 4;
	if (channel == 'B')
		info_scb += 2;
	active_scb = AHC_INB(ahc, SCBPTR);
	AHC_OUTB(ahc, SCBPTR, info_scb);
	scb_offset = SCB_ACTIVE0 + (target & 0x03);
	busy_scbid = AHC_INB(ahc, scb_offset);
	AHC_OUTB(ahc, scb_offset, SCB_LIST_NULL);
	AHC_OUTB(ahc, SCBPTR, active_scb);
	return busy_scbid;
}

static void
ahc_reset_current_bus(ahc)
	struct ahc_softc *ahc;
{
	AHC_OUTB(ahc, SCSISEQ, SCSIRSTO);
	DELAY(1000);
	AHC_OUTB(ahc, SCSISEQ, 0);
}

static int
ahc_reset_channel(ahc, channel, timedout_scb, xs_error, initiate_reset)
	struct	  ahc_softc *ahc;
	char	  channel;
	struct	  scb *timedout_scb;
	u_int32_t xs_error;
	int	  initiate_reset;
{
	u_int8_t sblkctl;
	char cur_channel;
	u_int32_t offset, offset_max;
	int found;
	int target;
	int maxtarget;

	maxtarget = 8;
	/*
	 * Clean up all the state information for the
	 * pending transactions on this bus.
	 */
	found = ahc_reset_device(ahc, ALL_TARGETS, channel, 
				 timedout_scb, xs_error);
	if (channel == 'B') {
		ahc->needsdtr |= (ahc->needsdtr_orig & 0xff00);
		ahc->sdtrpending &= 0x00ff;
		offset = TARG_SCRATCH + 8;
		offset_max = TARG_SCRATCH + 16;
	} else if (ahc->type & AHC_WIDE){
		ahc->needsdtr = ahc->needsdtr_orig;
		ahc->needwdtr = ahc->needwdtr_orig;
		ahc->sdtrpending = 0;
		ahc->wdtrpending = 0;
		maxtarget = 16;
		offset = TARG_SCRATCH;
		offset_max = TARG_SCRATCH + 16;
	} else {
		ahc->needsdtr |= (ahc->needsdtr_orig & 0x00ff);
		ahc->sdtrpending &= 0xff00;
		offset = TARG_SCRATCH;
		offset_max = TARG_SCRATCH + 8;
	}

	for (target = 0; target < maxtarget; target++)
		ahc_unbusy_target(ahc, target, channel);

	for (; offset < offset_max; offset++) {
		/*
		 * Revert to async/narrow transfers
		 * until we renegotiate.
		 */
		u_int8_t targ_scratch;

		targ_scratch = AHC_INB(ahc, offset);
		targ_scratch &= SXFR;
		AHC_OUTB(ahc, offset, targ_scratch);
	}

	/*
	 * Reset the bus if we are initiating this reset and
	 * restart/unpause the sequencer
	 */
	sblkctl = AHC_INB(ahc, SBLKCTL);
	cur_channel = (sblkctl & SELBUSB) ? 'B' : 'A';
	if (cur_channel != channel) {
		/* Case 1: Command for another bus is active
		 * Stealthily reset the other bus without
		 * upsetting the current bus.
		 */
		AHC_OUTB(ahc, SBLKCTL, sblkctl ^ SELBUSB);
		if (initiate_reset)
			ahc_reset_current_bus(ahc);
		AHC_OUTB(ahc, CLRSINT1, CLRSCSIRSTI|CLRSELTIMEO);
		AHC_OUTB(ahc, CLRINT, CLRSCSIINT);
		AHC_OUTB(ahc, SBLKCTL, sblkctl);
		unpause_sequencer(ahc, /*unpause_always*/TRUE);
	} else {
		/* Case 2: A command from this bus is active or we're idle */ 
		if (initiate_reset)
			ahc_reset_current_bus(ahc);
		AHC_OUTB(ahc, CLRSINT1, CLRSCSIRSTI|CLRSELTIMEO);
		AHC_OUTB(ahc, CLRINT, CLRSCSIINT);
		restart_sequencer(ahc);
	}
	ahc_run_done_queue(ahc);
	return found;
}

void
ahc_run_done_queue(ahc)
	struct ahc_softc *ahc;
{
	int i;
	struct scb *scbp;
	
	for (i = 0; i < ahc->scb_data->numscbs; i++) {
		scbp = ahc->scb_data->scbarray[i];
		if (scbp->flags & SCB_QUEUED_FOR_DONE) 
			ahc_done(ahc, scbp);
	}
}
	
static int
ahc_match_scb (scb, target, channel)
        struct scb *scb;
        int target;
	char channel;
{
	int targ = (scb->hscb->tcl >> 4) & 0x0f;
	char chan = (scb->hscb->tcl & SELBUSB) ? 'B' : 'A';

	if (target == ALL_TARGETS) 
		return (chan == channel);
	else
		return ((chan == channel) && (targ == target));
}

static void
ahc_construct_sdtr(ahc, start_byte, period, offset)
	struct ahc_softc *ahc;
	int start_byte;
	u_int8_t period;
	u_int8_t offset;
{
	AHC_OUTB(ahc, MSG0 + start_byte, MSG_EXTENDED);
	AHC_OUTB(ahc, MSG1 + start_byte, MSG_EXT_SDTR_LEN);
	AHC_OUTB(ahc, MSG2 + start_byte, MSG_EXT_SDTR);
	AHC_OUTB(ahc, MSG3 + start_byte, period);
	AHC_OUTB(ahc, MSG4 + start_byte, offset);
	AHC_OUTB(ahc, MSG_LEN, start_byte + 5);
}

static void
ahc_construct_wdtr(ahc, start_byte, bus_width)
	struct ahc_softc *ahc;
	int start_byte;
	u_int8_t bus_width;
{
	AHC_OUTB(ahc, MSG0 + start_byte, MSG_EXTENDED);
	AHC_OUTB(ahc, MSG1 + start_byte, MSG_EXT_WDTR_LEN);
	AHC_OUTB(ahc, MSG2 + start_byte, MSG_EXT_WDTR);
	AHC_OUTB(ahc, MSG3 + start_byte, bus_width);
	AHC_OUTB(ahc, MSG_LEN, start_byte + 4);
}

static void
ahc_calc_residual(scb)
	struct scb *scb;
{
	struct	scsi_xfer *xs;
	struct	hardware_scb *hscb;
	int	resid_sgs;

	xs = scb->xs;
	hscb = scb->hscb;

	if ((scb->flags & SCB_SENSE) == 0) {
		/*
		 * Remainder of the SG where the transfer
		 * stopped.
		 */
		xs->resid = (hscb->residual_data_count[2] <<16) |
			    (hscb->residual_data_count[1] <<8)  |
			    (hscb->residual_data_count[0]);

		/*
		 * Add up the contents of all residual
		 * SG segments that are after the SG where
		 * the transfer stopped.
		 */
		resid_sgs = hscb->residual_SG_segment_count - 1;
		while (resid_sgs > 0) {
			int sg;

			sg = hscb->SG_segment_count - resid_sgs;
			xs->resid += scb->ahc_dma[sg].len;
			resid_sgs--;
		}
#if defined(__FreeBSD__)
		xs->flags |= SCSI_RESID_VALID;
#elif defined(__NetBSD__)
		/* XXX - Update to do this right */
#endif
	}

	/*
	 * Clean out the residual information in this SCB for the
	 * next consumer of this SCB.
	 */
	hscb->residual_data_count[2] = 0;
	hscb->residual_data_count[1] = 0;
	hscb->residual_data_count[0] = 0;
	hscb->residual_SG_segment_count = 0;

#ifdef AHC_DEBUG
	if (ahc_debug & AHC_SHOWMISC) {
		sc_print_addr(xs->sc_link);
		printf("Handled Residual of %ld bytes\n" ,xs->resid);
	}
#endif
}
