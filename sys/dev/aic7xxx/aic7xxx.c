/*
 * Generic driver for the aic7xxx based adaptec SCSI controllers
 * Product specific probe and attach routines can be found in:
 * i386/eisa/ahc_eisa.c	27/284X and aic7770 motherboard controllers
 * pci/ahc_pci.c	3985, 3980, 3940, 2940, aic7895, aic7890,
 *			aic7880, aic7870, aic7860, and aic7850 controllers
 *
 * Copyright (c) 1994, 1995, 1996, 1997, 1998 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Where this Software is combined with software released under the terms of 
 * the GNU Public License ("GPL") and the terms of the GPL would require the 
 * combined work to also be released under the terms of the GPL, the terms
 * and conditions of this License will apply in addition to those of the
 * GPL with the exception of any terms or conditions of this License that
 * conflict with, or are expressly prohibited by, the GPL.
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
 *      $Id: aic7xxx.c,v 1.3 1998/09/18 03:42:16 gibbs Exp $
 */
/*
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
 * SCB is DMAed from the host.  If the MK_MESSAGE control bit is set
 * in the control byte of the SCB while it was disconnected, the sequencer
 * will assert ATN and attempt to issue a message to the host.
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

#include <opt_aic7xxx.h>

#include <pci.h>
#include <stddef.h>	/* For offsetof */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

#if NPCI > 0
#include <machine/bus_memio.h>
#endif
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/clock.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <dev/aic7xxx/aic7xxx.h>
#include <dev/aic7xxx/sequencer.h>

#include <aic7xxx_reg.h>
#include <aic7xxx_seq.h>

#include <sys/kernel.h>

#ifndef AHC_TMODE_ENABLE
#define AHC_TMODE_ENABLE 0
#endif

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define ALL_TARGETS (~0)
#define ALL_LUNS (~0) 
#define ALL_CHANNELS '\0'

#define	SIM_IS_SCSIBUS_B(ahc, sim)	\
	(sim == ahc->sim_b)
#define	SCB_IS_SCSIBUS_B(scb)	\
	(((scb)->hscb->tcl & SELBUSB) != 0)
#define	SCB_TARGET(scb)	\
	(((scb)->hscb->tcl & TID) >> 4)
#define	SCB_CHANNEL(scb) \
	(SCB_IS_SCSIBUS_B(scb) ? 'B' : 'A')
#define	SCB_LUN(scb)	\
	((scb)->hscb->tcl & LID)
#define SCB_TARGET_OFFSET(scb)		\
	(SCB_TARGET(scb) + (SCB_IS_SCSIBUS_B(scb) ? 8 : 0))
#define SCB_TARGET_MASK(scb)		\
	(0x01 << (SCB_TARGET_OFFSET(scb)))

#define ccb_scb_ptr spriv_ptr0
#define ccb_ahc_ptr spriv_ptr1

struct ahc_devinfo {
	int	  target_offset;
	u_int16_t target_mask;
	u_int8_t  target;
	char	  channel;
};

typedef enum {
	SEARCH_COMPLETE,
	SEARCH_COUNT,
	SEARCH_REMOVE
} ahc_search_action;

u_long ahc_unit = 0;

#ifdef AHC_DEBUG
static int     ahc_debug = AHC_DEBUG;
#endif

#if NPCI > 0
void ahc_pci_intr(struct ahc_softc *ahc);
#endif

static void	ahc_dump_targcmd(struct target_cmd *cmd);
static void	ahc_shutdown(int howto, void *arg);
static void	ahcminphys(struct buf *bp);
static cam_status
		ahc_find_tmode_devs(struct ahc_softc *ahc,
				    struct cam_sim *sim, union ccb *ccb,
				    struct tmode_tstate **tstate,
				    struct tmode_lstate **lstate,
				    int notfound_failure);
static void	ahc_action(struct cam_sim *sim, union ccb *ccb);
static void	ahc_async(void *callback_arg, u_int32_t code,
			  struct cam_path *path, void *arg);
static void	ahc_execute_scb(void *arg, bus_dma_segment_t *dm_segs,
				int nsegments, int error);
static void	ahc_poll(struct cam_sim *sim);
static void	ahc_setup_data(struct ahc_softc *ahc,
			       struct ccb_scsiio *csio, struct scb *scb);
static void	ahc_freeze_devq(struct ahc_softc *ahc, struct cam_path *path);
static struct scb *
                ahc_get_scb(struct ahc_softc *ahc);
static void     ahc_free_scb(struct ahc_softc *ahc, struct scb *scb);
static struct scb *
		ahc_alloc_scb(struct ahc_softc *ahc);
static void	ahc_fetch_devinfo(struct ahc_softc *ahc,
				  struct ahc_devinfo *devinfo);
static void	ahc_compile_devinfo(struct ahc_devinfo *devinfo,
				    u_int target, char channel);
static u_int	ahc_abort_wscb(struct ahc_softc *ahc, u_int scbpos, u_int prev);
static void	ahc_done(struct ahc_softc *ahc, struct scb *scbp);
static void	ahc_handle_target_cmd(struct ahc_softc *ahc);
static void 	ahc_handle_seqint(struct ahc_softc *ahc, u_int intstat);
static void	ahc_handle_scsiint(struct ahc_softc *ahc, u_int intstat);
static void	ahc_handle_reqinit(struct ahc_softc *ahc,
				   struct scb *scb);
static int	ahc_parse_msg(struct ahc_softc *ahc, struct scb *scb,
			      struct ahc_devinfo *devinfo);
static void	ahc_handle_devreset(struct ahc_softc *ahc, int target,
				    char channel, cam_status status,
				    ac_code acode, char *message,
				    int verbose_only);
static void	ahc_loadseq(struct ahc_softc *ahc);
static int	ahc_check_patch(struct ahc_softc *ahc,
				struct patch **start_patch,
				int start_instr, int *skip_addr);
static void	ahc_download_instr(struct ahc_softc *ahc,
				   int instrptr, u_int8_t *dconsts);
static int	ahc_match_scb(struct scb *scb, int target, char channel,
			      int lun, u_int tag);
#ifdef AHC_DEBUG
static void	ahc_print_scb(struct scb *scb);
#endif
static u_int	ahc_find_scb(struct ahc_softc *ahc, struct scb *scb);
static int	ahc_search_qinfifo(struct ahc_softc *ahc, int target,
				   char channel, int lun, u_int tag,
				   u_int32_t status, ahc_search_action action);
static int	ahc_reset_channel(struct ahc_softc *ahc, char channel,
				  int initiate_reset);
static int	ahc_abort_scbs(struct ahc_softc *ahc, int target,
			       char channel, int lun, u_int tag,
			       u_int32_t status);
static int	ahc_search_disc_list(struct ahc_softc *ahc, int target,
				     char channel, int lun, u_int tag);
static u_int	ahc_rem_scb_from_disc_list(struct ahc_softc *ahc,
					   u_int prev, u_int scbptr);
static void	ahc_add_curscb_to_free_list(struct ahc_softc *ahc);
static void	ahc_clear_intstat(struct ahc_softc *ahc);
static void	ahc_reset_current_bus(struct ahc_softc *ahc);
static struct ahc_syncrate *
		ahc_find_syncrate(struct ahc_softc *ahc, u_int *period,
				  u_int maxsync);
static u_int	ahc_find_period(struct ahc_softc *ahc, u_int scsirate,
				u_int maxsync);
static void	ahc_validate_offset(struct ahc_softc *ahc,
				    struct ahc_syncrate *syncrate,
				    u_int *offset, int wide); 
static void	ahc_set_syncrate(struct ahc_softc *ahc,
				 struct ahc_devinfo *devinfo,
				 struct cam_path *path,
				 struct ahc_syncrate *syncrate,
				 u_int period, u_int offset, u_int type);
static void	ahc_set_width(struct ahc_softc *ahc,
			      struct ahc_devinfo *devinfo,
			      struct cam_path *path, u_int width, u_int type);
static void	ahc_construct_sdtr(struct ahc_softc *ahc,
				   u_int period, u_int offset);
 
static void	ahc_construct_wdtr(struct ahc_softc *ahc, u_int bus_width);

static void	ahc_calc_residual(struct scb *scb);

static void	ahc_update_pending_syncrates(struct ahc_softc *ahc);

static void	ahc_set_recoveryscb(struct ahc_softc *ahc, struct scb *scb);

static timeout_t
		ahc_timeout;
static __inline void pause_sequencer(struct ahc_softc *ahc);
static __inline void unpause_sequencer(struct ahc_softc *ahc,
				       int unpause_always);
static __inline void restart_sequencer(struct ahc_softc *ahc);
static __inline u_int ahc_index_busy_tcl(struct ahc_softc *ahc,
					 u_int tcl, int unbusy);
 
static __inline void	 ahc_busy_tcl(struct ahc_softc *ahc, struct scb *scb);

static __inline void	   ahc_freeze_ccb(union ccb* ccb);
static __inline cam_status ahc_ccb_status(union ccb* ccb);
static __inline void	   ahc_set_ccb_status(union ccb* ccb,
					      cam_status status);

static __inline u_int32_t
ahc_hscb_busaddr(struct ahc_softc *ahc, u_int index)
{
	return (ahc->hscb_busaddr + (sizeof(struct hardware_scb) * index));
}

#define AHC_BUSRESET_DELAY	25	/* Reset delay in us */

static __inline void
pause_sequencer(struct ahc_softc *ahc)
{
	ahc_outb(ahc, HCNTRL, ahc->pause);

	/*
	 * Since the sequencer can disable pausing in a critical section, we
	 * must loop until it actually stops.
	 */
	while ((ahc_inb(ahc, HCNTRL) & PAUSE) == 0)
		;
}

static __inline void
unpause_sequencer(struct ahc_softc *ahc, int unpause_always)
{
	if ((ahc->flags & AHC_HANDLING_REQINITS) == 0
	 && (unpause_always
	  || (ahc_inb(ahc, INTSTAT) & (SCSIINT | SEQINT | BRKADRINT)) == 0))
		ahc_outb(ahc, HCNTRL, ahc->unpause);
}

/*
 * Restart the sequencer program from address zero
 */
static __inline void
restart_sequencer(struct ahc_softc *ahc)
{
	pause_sequencer(ahc);
	ahc_outb(ahc, SEQCTL, FASTMODE|SEQRESET);
	unpause_sequencer(ahc, /*unpause_always*/TRUE);
}

static __inline u_int
ahc_index_busy_tcl(struct ahc_softc *ahc, u_int tcl, int unbusy)
{
	u_int scbid;

	scbid = ahc->untagged_scbs[tcl];
	if (unbusy)
		ahc->untagged_scbs[tcl] = SCB_LIST_NULL;

	return (scbid);
}

static __inline void
ahc_busy_tcl(struct ahc_softc *ahc, struct scb *scb)
{
	ahc->untagged_scbs[scb->hscb->tcl] = scb->hscb->tag;
}

static __inline void
ahc_freeze_ccb(union ccb* ccb)
{
	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) == 0) {
		ccb->ccb_h.status |= CAM_DEV_QFRZN;
		xpt_freeze_devq(ccb->ccb_h.path, /*count*/1);
	}
}

static __inline cam_status
ahc_ccb_status(union ccb* ccb)
{
	return (ccb->ccb_h.status & CAM_STATUS_MASK);
}

static __inline void
ahc_set_ccb_status(union ccb* ccb, cam_status status)
{
	ccb->ccb_h.status &= ~CAM_STATUS_MASK;
	ccb->ccb_h.status |= status;
}

char *
ahc_name(struct ahc_softc *ahc)
{
	static char name[10];

	sprintf(name, "ahc%d", ahc->unit);
	return (name);
}

#ifdef  AHC_DEBUG
static void
ahc_print_scb(struct scb *scb)
{
	struct hardware_scb *hscb = scb->hscb;

	printf("scb:%p control:0x%x tcl:0x%x cmdlen:%d cmdpointer:0x%lx\n",
		scb,
		hscb->control,
		hscb->tcl,
		hscb->cmdlen,
		hscb->cmdpointer );
	printf("        datlen:%d data:0x%lx segs:0x%x segp:0x%lx\n",
		hscb->datalen,
		hscb->data,
		hscb->SG_count,
		hscb->SG_pointer);
	printf("	sg_addr:%lx sg_len:%ld\n",
		scb->ahc_dma[0].addr,
		scb->ahc_dma[0].len);
	printf("	cdb:%x %x %x %x %x %x %x %x %x %x %x %x\n",
		hscb->cmdstore[0], hscb->cmdstore[1], hscb->cmdstore[2],
		hscb->cmdstore[3], hscb->cmdstore[4], hscb->cmdstore[5],
		hscb->cmdstore[6], hscb->cmdstore[7], hscb->cmdstore[8],
		hscb->cmdstore[9], hscb->cmdstore[10], hscb->cmdstore[11]);
}
#endif

static struct {
        u_int8_t errno;
	char *errmesg;
} hard_error[] = {
	{ ILLHADDR,	"Illegal Host Access" },
	{ ILLSADDR,	"Illegal Sequencer Address referrenced" },
	{ ILLOPCODE,	"Illegal Opcode in sequencer program" },
	{ SQPARERR,	"Sequencer Parity Error" },
	{ DPARERR,	"Data-path Parity Error" },
	{ MPARERR,	"Scratch or SCB Memory Parity Error" },
	{ PCIERRSTAT,	"PCI Error detected" },
	{ CIOPARERR,	"CIOBUS Parity Error" },
};


/*
 * Valid SCSIRATE values.  (p. 3-17)
 * Provides a mapping of tranfer periods in ns to the proper value to
 * stick in the scsiscfr reg to use that transfer rate.
 */
#define AHC_SYNCRATE_ULTRA2	0
#define AHC_SYNCRATE_ULTRA	2
#define AHC_SYNCRATE_FAST	5
static struct ahc_syncrate ahc_syncrates[] = {
	/* ultra2  fast/ultra  period	rate */
	{ 0x13,   0x000,	10,	"40.0"	},
	{ 0x14,   0x000,	11,	"33.0"	},
	{ 0x15,   0x100,	12,	"20.0"	},
	{ 0x16,   0x110,	15,	"16.0"	},
	{ 0x17,   0x120,	18,	"13.4"	},
	{ 0x18,   0x000,	25,	"10.0"	},
	{ 0x19,   0x010,	31,	"8.0"	},
	{ 0x1a,   0x020,	37,	"6.67"	},
	{ 0x1b,   0x030,	43,	"5.7"	},
	{ 0x10,   0x040,	50,	"5.0"	},
	{ 0x00,   0x050,	56,	"4.4"	},
	{ 0x00,   0x060,	62,	"4.0"	},
	{ 0x00,   0x070,	68,	"3.6"	},
	{ 0x00,   0x000,	0,	NULL	}
};

/*
 * Allocate a controller structure for a new device and initialize it.
 */
struct ahc_softc *
ahc_alloc(int unit, u_int32_t iobase, vm_offset_t maddr, ahc_chip chip,
	  ahc_feature features, ahc_flag flags, struct scb_data *scb_data)
{
	/*
	 * find unit and check we have that many defined
	 */
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
	if (scb_data == NULL) {
		struct full_ahc_softc* full_softc = (struct full_ahc_softc*)ahc;
		ahc->scb_data = &full_softc->scb_data_storage;
		STAILQ_INIT(&ahc->scb_data->free_scbs);
	} else
		ahc->scb_data = scb_data;
	LIST_INIT(&ahc->pending_ccbs);
	ahc->unit = unit;

	/*
	 * XXX This should be done by the bus specific probe stubs with
	 *     the bus layer providing the bsh and tag.  Unfortunately,
	 *     we need to clean up how we configure things before this
	 *     can happen.
	 */
	if (maddr != NULL) {
		ahc->tag = I386_BUS_SPACE_MEM;
		ahc->bsh = (bus_space_handle_t)maddr;
	} else {
		ahc->tag = I386_BUS_SPACE_IO;
		ahc->bsh = (bus_space_handle_t)iobase;
	}
	ahc->chip = chip;
	ahc->features = features;
	ahc->flags = flags;
	ahc->unpause = (ahc_inb(ahc, HCNTRL) & IRQMS) | INTEN;
	ahc->pause = ahc->unpause | PAUSE;

	return (ahc);
}

void
ahc_free(ahc)
	struct ahc_softc *ahc;
{
	free(ahc, M_DEVBUF);
	return;
}

int
ahc_reset(struct ahc_softc *ahc)
{
        u_int	hcntrl;
	u_int	sblkctl;
	int	wait;
	
	ahc_outb(ahc, HCNTRL, CHIPRST | ahc->pause);
	/*
	 * Ensure that the reset has finished
	 */
	wait = 1000;
	while (--wait && !(ahc_inb(ahc, HCNTRL) & CHIPRSTACK))
		DELAY(1000);
	if (wait == 0) {
		printf("%s: WARNING - Failed chip reset!  "
		       "Trying to initialize anyway.\n", ahc_name(ahc));
	}
	ahc_outb(ahc, HCNTRL, ahc->pause);

	/* Determine channel configuration */
	sblkctl = ahc_inb(ahc, SBLKCTL) & (SELBUSB|SELWIDE);
	/* No Twin Channel PCI cards */
	if ((ahc->chip & AHC_PCI) != 0)
		sblkctl &= ~SELBUSB;
	switch (sblkctl) {
	case 0:
		/* Single Narrow Channel */
		break;
	case 2:
		/* Wide Channel */
		ahc->features |= AHC_WIDE;
		break;
	case 8:
		/* Twin Channel */
		ahc->features |= AHC_TWIN;
		break;
	default:
		printf(" Unsupported adapter type.  Ignoring\n");
		return(-1);
	}
	return (0);
}

/*
 * Look up the valid period to SCSIRATE conversion in our table.
 * Return the period and offset that should be sent to the target
 * if this was the beginning of an SDTR.
 */
static struct ahc_syncrate *
ahc_find_syncrate(struct ahc_softc *ahc, u_int *period, u_int maxsync)
{
	struct ahc_syncrate *syncrate;

	syncrate = &ahc_syncrates[maxsync];
	while ((syncrate->rate != NULL)
	    && ((ahc->features & AHC_ULTRA2) == 0
	     || (syncrate->sxfr_ultra2 != 0))) {

		if (*period <= syncrate->period) {
			/*
			 * When responding to a target that requests
			 * sync, the requested rate may fall between
			 * two rates that we can output, but still be
			 * a rate that we can receive.  Because of this,
			 * we want to respond to the target with
			 * the same rate that it sent to us even
			 * if the period we use to send data to it
			 * is lower.  Only lower the response period
			 * if we must.
			 */
			if (syncrate == &ahc_syncrates[maxsync]) {
				*period = syncrate->period;
			}
			break;
		}
		syncrate++;
	}

	if ((*period == 0)
	 || (syncrate->rate == NULL)
	 || ((ahc->features & AHC_ULTRA2) != 0
	  && (syncrate->sxfr_ultra2 == 0))) {
		/* Use asynchronous transfers. */
		*period = 0;
		syncrate = NULL;
	}
	return (syncrate);
}

static u_int
ahc_find_period(struct ahc_softc *ahc, u_int scsirate, u_int maxsync)
{
	struct ahc_syncrate *syncrate;

	if ((ahc->features & AHC_ULTRA2) != 0) {
		scsirate &= SXFR_ULTRA2;
	} else  {
		scsirate &= SXFR;
	}

	syncrate = &ahc_syncrates[maxsync];
	while (syncrate->rate != NULL) {

		if ((ahc->features & AHC_ULTRA2) != 0) {
			if (syncrate->sxfr_ultra2 == 0)
				break;
			else if (scsirate == syncrate->sxfr_ultra2)
				return (syncrate->period);
		} else if (scsirate == (syncrate->sxfr & ~ULTRA_SXFR)) {
				return (syncrate->period);
		}
		syncrate++;
	}
	return (0); /* async */
}

static void
ahc_validate_offset(struct ahc_softc *ahc, struct ahc_syncrate *syncrate,
		    u_int *offset, int wide)
{
	u_int maxoffset;

	/* Limit offset to what we can do */
	if (syncrate == NULL) {
		maxoffset = 0;
	} else if ((ahc->features & AHC_ULTRA2) != 0) {
		maxoffset = MAX_OFFSET_ULTRA2;
	} else {
		if (wide)
			maxoffset = MAX_OFFSET_16BIT;
		else
			maxoffset = MAX_OFFSET_8BIT;
	}
	*offset = MIN(*offset, maxoffset);
}

static void
ahc_set_syncrate(struct ahc_softc *ahc, struct ahc_devinfo *devinfo,
		 struct cam_path *path, struct ahc_syncrate *syncrate,
		 u_int period, u_int offset, u_int type)
{
	u_int old_period;
	u_int old_offset;

	if (syncrate == NULL) {
		period = 0;
		offset = 0;
	}

	old_period = ahc->transinfo[devinfo->target_offset].current.period;
	old_offset = ahc->transinfo[devinfo->target_offset].current.offset;

	if ((type & AHC_TRANS_CUR) != 0
	 && (old_period != period || old_offset != offset)) {
		struct	ccb_trans_settings neg;
		u_int	scsirate;

		scsirate = ahc->transinfo[devinfo->target_offset].scsirate;
		if ((ahc->features & AHC_ULTRA2) != 0) {

			scsirate &= ~SXFR_ULTRA2;

			if (syncrate != NULL) {
				scsirate |= syncrate->sxfr_ultra2;
			}

			if ((type & AHC_TRANS_ACTIVE) == AHC_TRANS_ACTIVE) {
				ahc_outb(ahc, SCSIOFFSET, offset);
			}
			ahc_outb(ahc, TARG_OFFSET + devinfo->target_offset,
				 offset);
		} else {

			scsirate &= ~(SXFR|SOFS);
			/*
			 * Ensure Ultra mode is set properly for
			 * this target.
			 */
			ahc->ultraenb &= ~devinfo->target_mask;
			if (syncrate != NULL) {
				if (syncrate->sxfr & ULTRA_SXFR) {
					ahc->ultraenb |= devinfo->target_mask;
				}
				scsirate |= syncrate->sxfr & SXFR;
				scsirate |= offset & SOFS;
			}
			if ((type & AHC_TRANS_ACTIVE) == AHC_TRANS_ACTIVE) {
				u_int sxfrctl0;

				sxfrctl0 = ahc_inb(ahc, SXFRCTL0);
				sxfrctl0 &= ~FAST20;
				if (ahc->ultraenb & devinfo->target_mask)
					sxfrctl0 |= FAST20;
				ahc_outb(ahc, SXFRCTL0, sxfrctl0);
			}
		}
		if ((type & AHC_TRANS_ACTIVE) == AHC_TRANS_ACTIVE)
			ahc_outb(ahc, SCSIRATE, scsirate);

		ahc->transinfo[devinfo->target_offset].scsirate = scsirate;
		ahc->transinfo[devinfo->target_offset].current.period = period;
		ahc->transinfo[devinfo->target_offset].current.offset = offset;

		/* Update the syncrates in any pending scbs */
		ahc_update_pending_syncrates(ahc);

		/*
		 * Tell the SCSI layer about the
		 * new transfer parameters.
		 */
		neg.sync_period = period;
		neg.sync_offset = offset;
		neg.valid = CCB_TRANS_SYNC_RATE_VALID
			  | CCB_TRANS_SYNC_OFFSET_VALID;
		xpt_setup_ccb(&neg.ccb_h, path, /*priority*/1);
		xpt_async(AC_TRANSFER_NEG, path, &neg);
		if (bootverbose) {
			if (neg.sync_offset != 0) {
				printf("%s: target %d synchronous at %sMHz, "
				       "offset = 0x%x\n", ahc_name(ahc),
				       devinfo->target, syncrate->rate, offset);
			} else {
				printf("%s: target %d using "
				       "asynchronous transfers\n",
				       ahc_name(ahc), devinfo->target);
			}
		}
	}

	if ((type & AHC_TRANS_GOAL) != 0) {
		ahc->transinfo[devinfo->target_offset].goal.period = period;
		ahc->transinfo[devinfo->target_offset].goal.offset = offset;
	}

	if ((type & AHC_TRANS_USER) != 0) {
		ahc->transinfo[devinfo->target_offset].user.period = period;
		ahc->transinfo[devinfo->target_offset].user.offset = offset;
	}
}

static void
ahc_set_width(struct ahc_softc *ahc, struct ahc_devinfo *devinfo,
	      struct cam_path *path, u_int width, u_int type)
{
	u_int	 oldwidth;

	oldwidth = ahc->transinfo[devinfo->target_offset].current.width;

	if ((type & AHC_TRANS_CUR) != 0 && oldwidth != width) {
		struct	ccb_trans_settings neg;
		u_int	scsirate;

		scsirate =  ahc->transinfo[devinfo->target_offset].scsirate;
		scsirate &= ~WIDEXFER;
		if (width == MSG_EXT_WDTR_BUS_16_BIT)
			scsirate |= WIDEXFER;

		ahc->transinfo[devinfo->target_offset].scsirate = scsirate;

		if ((type & AHC_TRANS_ACTIVE) == AHC_TRANS_ACTIVE)
			ahc_outb(ahc, SCSIRATE, scsirate);

		ahc->transinfo[devinfo->target_offset].current.width = width;

		/* Tell the SCSI layer about the new transfer params */
		neg.bus_width = width;
		neg.valid = CCB_TRANS_BUS_WIDTH_VALID;
		xpt_setup_ccb(&neg.ccb_h, path, /*priority*/1);
		xpt_async(AC_TRANSFER_NEG, path, &neg);
		if (bootverbose) {
			printf("%s: target %d using %dbit transfers\n",
			       ahc_name(ahc), devinfo->target,
			       8 * (0x01 << neg.bus_width));
		}
	}
	if ((type & AHC_TRANS_GOAL) != 0) {
		ahc->transinfo[devinfo->target_offset].goal.width = width;
	}
	if ((type & AHC_TRANS_USER) != 0) {
		ahc->transinfo[devinfo->target_offset].user.width = width;
	}
}

/*
 * Attach all the sub-devices we can find
 */
int
ahc_attach(struct ahc_softc *ahc)
{
	struct ccb_setasync csa;
	struct cam_devq *devq;
	int bus_id;

	/*
	 * Create the device queue for our SIM.
	 */
	devq = cam_simq_alloc(ahc->scb_data->maxscbs);
	if (devq == NULL)
		return (0);

	/*
	 * Construct our SIM entry
	 */
	ahc->sim = cam_sim_alloc(ahc_action, ahc_poll, "ahc", ahc, ahc->unit,
				 1, ahc->scb_data->maxscbs, devq);
	if (ahc->sim == NULL) {
		cam_simq_free(devq);
		return (0);
	}
	bus_id = (ahc->flags & AHC_CHANNEL_B_PRIMARY) ? 1 : 0;
	
	if (xpt_bus_register(ahc->sim, bus_id) != CAM_SUCCESS) {
		cam_sim_free(ahc->sim, /*free_devq*/TRUE);
		return (0);
	}
	
	if (xpt_create_path(&ahc->path, /*periph*/NULL,
			    cam_sim_path(ahc->sim), CAM_TARGET_WILDCARD,
			    CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(ahc->sim));
		cam_sim_free(ahc->sim, /*free_devq*/TRUE);
		return (0);
	}
		
	xpt_setup_ccb(&csa.ccb_h, ahc->path, /*priority*/5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = AC_LOST_DEVICE;
	csa.callback = ahc_async;
	csa.callback_arg = ahc->sim;
	xpt_action((union ccb *)&csa);

	if (ahc->features & AHC_TWIN) {
		ahc->sim_b = cam_sim_alloc(ahc_action, ahc_poll, "ahc",
					   ahc, ahc->unit, 1,
					   ahc->scb_data->maxscbs, devq);

		if (ahc->sim_b == NULL) {
			printf("ahc_attach: Unable to attach second "
			       "bus due to resource shortage");
			/*
			 * Must return success or the first bus
			 * won't get attached either.
			 */
			return (1);
		}
		
		bus_id = (ahc->flags & AHC_CHANNEL_B_PRIMARY) ? 0 : 1;
		if (xpt_bus_register(ahc->sim_b, bus_id) != CAM_SUCCESS) {
			printf("ahc_attach: Unable to attach second "
			       "bus due to resource shortage");
			/*
			 * We do not want to destroy the device queue
			 * because the first bus is using it.
			 */
			cam_sim_free(ahc->sim_b, /*free_devq*/FALSE);
			ahc->sim_b = NULL;
			return (1);
		}

		if (xpt_create_path(&ahc->path_b, /*periph*/NULL,
				    cam_sim_path(ahc->sim_b),
				    CAM_TARGET_WILDCARD,
				    CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
			xpt_bus_deregister(cam_sim_path(ahc->sim_b));
			cam_sim_free(ahc->sim_b, /*free_devq*/FALSE);
			ahc->sim_b = NULL;
			return (1);
		}
		xpt_setup_ccb(&csa.ccb_h, ahc->path_b, /*priority*/5);
		csa.ccb_h.func_code = XPT_SASYNC_CB;
		csa.event_enable = AC_LOST_DEVICE;
		csa.callback = ahc_async;
		csa.callback_arg = ahc->sim_b;
		xpt_action((union ccb *)&csa);
	}
	return (1);
}

static void
ahc_fetch_devinfo(struct ahc_softc *ahc, struct ahc_devinfo *devinfo)
{
	u_int saved_tcl;

	saved_tcl = ahc_inb(ahc, SAVED_TCL);
	ahc_compile_devinfo(devinfo, (saved_tcl >> 4) & 0x0f,
			    (saved_tcl & SELBUSB) ? 'B': 'A');
}

static void
ahc_compile_devinfo(struct ahc_devinfo *devinfo, u_int target, char channel)
{
	devinfo->target = target;
	devinfo->target_offset = target;
	devinfo->channel = channel;
	if (channel == 'B')
		devinfo->target_offset += 8;
	devinfo->target_mask = (0x01 << devinfo->target_offset);
}

/*
 * Catch an interrupt from the adapter
 */
void
ahc_intr(void *arg)
{
	struct	ahc_softc *ahc;
	u_int	intstat;

	ahc = (struct ahc_softc *)arg; 

	intstat = ahc_inb(ahc, INTSTAT);

	/*
	 * Any interrupts to process?
	 */
#if NPCI > 0
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
		struct scb *scb;
		u_int  scb_index;

		ahc_outb(ahc, CLRINT, CLRCMDINT);
		while (ahc->qoutfifo[ahc->qoutfifonext] != SCB_LIST_NULL) {
			scb_index = ahc->qoutfifo[ahc->qoutfifonext];
			ahc->qoutfifo[ahc->qoutfifonext++] = SCB_LIST_NULL;

			if (scb_index == TARGET_CMD_CMPLT
			 && (ahc->flags & AHC_TARGETMODE) != 0) {
				ahc_handle_target_cmd(ahc);
				continue;
			}

			scb = ahc->scb_data->scbarray[scb_index];
			if (!scb || !(scb->flags & SCB_ACTIVE)) {
				printf("%s: WARNING no command for scb %d "
				       "(cmdcmplt)\nQOUTPOS = %d\n",
				       ahc_name(ahc), scb_index,
				       ahc->qoutfifonext - 1);
				continue;
			}

			/*
			 * Save off the residual
			 * if there is one.
			 */
			if (scb->hscb->residual_SG_count != 0)
				ahc_calc_residual(scb);
			ahc_done(ahc, scb);
		}
	}
	if (intstat & BRKADRINT) {
		/*
		 * We upset the sequencer :-(
		 * Lookup the error message
		 */
		int i, error, num_errors;

		error = ahc_inb(ahc, ERROR);
		num_errors =  sizeof(hard_error)/sizeof(hard_error[0]);
		for (i = 0; error != 1 && i < num_errors; i++)
			error >>= 1;
		panic("%s: brkadrint, %s at seqaddr = 0x%x\n",
		      ahc_name(ahc), hard_error[i].errmesg,
		      ahc_inb(ahc, SEQADDR0) |
		      (ahc_inb(ahc, SEQADDR1) << 8));

		/* Tell everyone that this HBA is no longer availible */
		ahc_abort_scbs(ahc, ALL_TARGETS, ALL_CHANNELS,
			       ALL_LUNS, SCB_LIST_NULL, CAM_NO_HBA);
	}
	if (intstat & SEQINT)
		ahc_handle_seqint(ahc, intstat);

	if (intstat & SCSIINT)
		ahc_handle_scsiint(ahc, intstat);
}

static void
ahc_handle_target_cmd(struct ahc_softc *ahc)
{
	struct	  tmode_tstate *tstate;
	struct	  tmode_lstate *lstate;
	struct	  ccb_accept_tio *atio;
	struct	  target_cmd *cmd;
	u_int8_t *byte;
	int	  initiator;
	int	  target;
	int	  lun;

	cmd = &ahc->targetcmds[ahc->next_targetcmd];
	ahc->next_targetcmd++;
	if (ahc->next_targetcmd >= ahc->num_targetcmds)
		ahc->next_targetcmd = 0;

	initiator = cmd->icl >> 4;
	target = cmd->targ_id;
	lun    = (cmd->identify & MSG_IDENTIFY_LUNMASK);

	xpt_print_path(ahc->path);
	printf("Received Target Command (%d:%d:%d)\n",
	       initiator, target, lun);
	ahc_dump_targcmd(cmd);

	byte = cmd->bytes;
	tstate = ahc->enabled_targets[target];
	lstate = NULL;
	if (tstate != NULL && lun < 8)
		lstate = tstate->enabled_luns[lun];

	/*
	 * XXX Need to have a default TMODE devce that attaches to luns
	 *     that wouldn't otherwise be enabled and returns the proper
	 *     inquiry information.  After all, we don't want to duplicate
	 *     this code in each driver.  For now, simply drop it on the
	 *     floor.
	 */
	if (lstate == NULL) {
		printf("Incoming Command on disabled lun\n");
		return;
	}

	atio = (struct ccb_accept_tio*)SLIST_FIRST(&lstate->accept_tios);
	/* XXX Should reconnect and return BUSY status */
	if (atio == NULL) {
		printf("No ATIOs for incoming command\n");
		return;
	}

	/*
	 * Package it up and send it off to
	 * whomever has this lun enabled.
	 */
	atio->init_id = initiator;
	if (byte[0] != 0xFF) {
		/* Tag was included */
		atio->tag_action = *byte++;
		atio->tag_id = *byte++;
		atio->ccb_h.flags = CAM_TAG_ACTION_VALID;
	} else {
		byte++;
		atio->ccb_h.flags = 0;
	}

	/* Okay.  Now determine the cdb size based on the command code */
	switch (*byte >> CMD_GROUP_CODE_SHIFT) {
	case 0:
		atio->cdb_len = 6;
		break;
	case 1:
	case 2:
		atio->cdb_len = 10;
		break;
	case 4:
		atio->cdb_len = 16;
		break;
	case 5:
		atio->cdb_len = 12;
		break;
	case 3:
	default:
		/* Only copy the opcode. */
		atio->cdb_len = 1;
		printf("Reserved or VU command code type encountered\n");
		break;
	}
	bcopy(byte, atio->cdb_io.cdb_bytes, atio->cdb_len);

	SLIST_REMOVE_HEAD(&lstate->accept_tios, sim_links.sle);
	atio->ccb_h.status |= CAM_CDB_RECVD;

	if ((cmd->identify & MSG_IDENTIFY_DISCFLAG) == 0) {
		/*
		 * We weren't allowed to disconnect.
		 * We're hanging on the bus until a
		 * continue target I/O comes in response
		 * to this accept tio.
		 */
		xpt_print_path(atio->ccb_h.path);
		printf("Incoming Command did not disconnect %p\n", lstate);
		ahc->pending_device = lstate;
	}
	xpt_done((union ccb*)atio);
}

static void
ahc_handle_seqint(struct ahc_softc *ahc, u_int intstat)
{
	struct scb *scb;
	struct ahc_devinfo devinfo;
	
	ahc_fetch_devinfo(ahc, &devinfo);

	/*
	 * Clear the upper byte that holds SEQINT status
	 * codes and clear the SEQINT bit. We will unpause
	 * the sequencer, if appropriate, after servicing
	 * the request.
	 */
	ahc_outb(ahc, CLRINT, CLRSEQINT);
	switch (intstat & SEQINT_MASK) {
	case NO_MATCH:
	{
		/* Ensure we don't leave the selection hardware on */
		ahc_outb(ahc, SCSISEQ,
			 ahc_inb(ahc, SCSISEQ) & (ENSELI|ENRSELI|ENAUTOATNP));

		printf("%s:%c:%d: no active SCB for reconnecting "
		       "target - issuing BUS DEVICE RESET\n",
		       ahc_name(ahc), devinfo.channel, devinfo.target);
		printf("SAVED_TCL == 0x%x, ARG_1 == 0x%x, SEQ_FLAGS == 0x%x\n",
		       ahc_inb(ahc, SAVED_TCL), ahc_inb(ahc, ARG_1),
		       ahc_inb(ahc, SEQ_FLAGS));
		break;
	}
	case SEND_REJECT: 
	{
		u_int rejbyte = ahc_inb(ahc, ACCUM);
		printf("%s:%c:%d: Warning - unknown message received from "
		       "target (0x%x).  Rejecting\n", 
		       ahc_name(ahc), devinfo.channel, devinfo.target, rejbyte);
		break; 
	}
	case NO_IDENT: 
	{
		/*
		 * The reconnecting target either did not send an identify
		 * message, or did, but we didn't find and SCB to match and
		 * before it could respond to our ATN/abort, it hit a dataphase.
		 * The only safe thing to do is to blow it away with a bus
		 * reset.
		 */
		int found;

		printf("%s:%c:%d: Target did not send an IDENTIFY message. "
		       "LASTPHASE = 0x%x, SAVED_TCL == 0x%x\n",
		       ahc_name(ahc), devinfo.channel, devinfo.target,
		       ahc_inb(ahc, LASTPHASE), ahc_inb(ahc, SAVED_TCL));
		found = ahc_reset_channel(ahc, devinfo.channel, 
					  /*initiate reset*/TRUE);
		printf("%s: Issued Channel %c Bus Reset. "
		       "%d SCBs aborted\n", ahc_name(ahc), devinfo.channel,
		       found);
		break;
	}
	case BAD_PHASE:
		if (ahc_inb(ahc, LASTPHASE) == P_BUSFREE) {
			printf("%s:%c:%d: Missed busfree.\n", ahc_name(ahc),
			       devinfo.channel, devinfo.target);
			restart_sequencer(ahc);
			return;
		} else {
			printf("%s:%c:%d: unknown scsi bus phase.  Attempting "
			       "to continue\n", ahc_name(ahc), devinfo.channel,
			       devinfo.target);
		}
		break; 
	case EXTENDED_MSG:
	{
		ahc->msg_type = MSG_TYPE_INITIATOR_MSGIN;
		ahc->msg_len = 0; 
		ahc->msg_index = 0;

		/*      
		 * To actually receive the message, simply turn on
		 * REQINIT interrupts and let our interrupt handler
		 * do the rest (REQINIT should already be true).
		 */     
		ahc_outb(ahc, SIMODE1, ahc_inb(ahc, SIMODE1) | ENREQINIT);
		ahc->flags |= AHC_HANDLING_REQINITS;
		return;
	}
	case REJECT_MSG:
	{
		/*
		 * What we care about here is if we had an
		 * outstanding SDTR or WDTR message for this
		 * target.  If we did, this is a signal that
		 * the target is refusing negotiation.
		 */
		u_int scb_index;
		u_int last_msg;

		scb_index = ahc_inb(ahc, SCB_TAG);
		scb = ahc->scb_data->scbarray[scb_index];

		last_msg = ahc_inb(ahc, LAST_MSG);

		if ((last_msg == MSG_IDENTIFYFLAG)
		 && (scb->hscb->control & MSG_SIMPLE_Q_TAG) != 0) {
			struct	ccb_trans_settings neg;

			printf("%s:%c:%d: refuses tagged commands.  Performing "
			       "non-tagged I/O\n", ahc_name(ahc),
			       devinfo.channel, devinfo.target);
			
			ahc->tagenable &= ~devinfo.target_mask;
			neg.flags = 0;
			neg.valid = CCB_TRANS_TQ_VALID;
			xpt_setup_ccb(&neg.ccb_h, scb->ccb->ccb_h.path,
				      /*priority*/1);
			xpt_async(AC_TRANSFER_NEG, scb->ccb->ccb_h.path, &neg);
			/*
			 * Resend the identify for this CCB as the target
			 * may believe that the selection is invalid otherwise.
			 */
			ahc_outb(ahc, SCB_CONTROL, ahc_inb(ahc, SCB_CONTROL)
						  & ~MSG_SIMPLE_Q_TAG);
		 	scb->hscb->control &= ~MSG_SIMPLE_Q_TAG;
			scb->ccb->ccb_h.flags &= ~CAM_TAG_ACTION_VALID;
			ahc_outb(ahc, MSG_OUT, MSG_IDENTIFYFLAG);
			ahc_outb(ahc, SCSISIGO, ahc_inb(ahc, SCSISIGO) | ATNO);

			/*
			 * Requeue all tagged commands for this target
			 * currently in our posession so they can be
			 * converted to untagged commands.
			 */
			ahc_search_qinfifo(ahc, SCB_TARGET(scb),
					   SCB_CHANNEL(scb),
					   SCB_LUN(scb),
					   /*tag*/SCB_LIST_NULL,
					   CAM_REQUEUE_REQ,
					   SEARCH_COMPLETE);
		} else if ((last_msg == MSG_IDENTIFYFLAG
			 || last_msg == HOST_MSG)
		        && (scb->flags & SCB_MSGOUT_WDTR) != 0) {
			struct ahc_target_tinfo *tinfo;

			/* note 8bit xfers and clear flag */
			printf("%s:%c:%d: refuses WIDE negotiation.  Using "
			       "8bit transfers\n", ahc_name(ahc),
			       devinfo.channel, devinfo.target);
			scb->flags &= ~SCB_MSGOUT_BITS;
			ahc->wdtrpending &= ~devinfo.target_mask;
			ahc_set_width(ahc, &devinfo, scb->ccb->ccb_h.path,
				      MSG_EXT_WDTR_BUS_8_BIT,
				      AHC_TRANS_ACTIVE|AHC_TRANS_GOAL);
			ahc_set_syncrate(ahc, &devinfo, scb->ccb->ccb_h.path,
					 /*syncrate*/NULL, /*period*/0,
					 /*offset*/0, AHC_TRANS_ACTIVE);
			tinfo = &ahc->transinfo[devinfo.target_offset];
			if (tinfo->goal.period) {
				/* Start the sync negotiation */
				ahc->sdtrpending |= devinfo.target_mask;
				scb->flags |= SCB_MSGOUT_SDTR;
				ahc_outb(ahc, MSG_OUT, HOST_MSG);
				ahc_outb(ahc, SCSISIGO,
					 ahc_inb(ahc, SCSISIGO) | ATNO);
			}
		} else if ((last_msg == MSG_IDENTIFYFLAG
			 || last_msg == HOST_MSG)
		        && (scb->flags & SCB_MSGOUT_SDTR) != 0) {

			/* note asynch xfers and clear flag */
			ahc_set_syncrate(ahc, &devinfo, scb->ccb->ccb_h.path,
					 /*syncrate*/NULL, /*period*/0,
					 /*offset*/0,
					 AHC_TRANS_ACTIVE|AHC_TRANS_GOAL);
			scb->flags &= ~SCB_MSGOUT_BITS;
 			ahc->sdtrpending &= ~devinfo.target_mask;
			printf("%s:%c:%d: refuses synchronous negotiation. "
			       "Using asynchronous transfers\n",
			       ahc_name(ahc),
			       devinfo.channel, devinfo.target);
		} else {
			/*
			 * Otherwise, we ignore it.
			 */
#ifdef AHC_DEBUG
			if (ahc_debug & AHC_SHOWMISC)
				printf("%s:%c:%d: Message reject -- ignored\n",
				       ahc_name(ahc), devinfo.channel,
				       devinfo.target);
#endif
			break;
		}
		break;
	}
	case BAD_STATUS:
	{
		u_int  scb_index;
		struct hardware_scb *hscb;
		struct ccb_scsiio *csio;
		/*
		 * The sequencer will notify us when a command
		 * has an error that would be of interest to
		 * the kernel.  This allows us to leave the sequencer
		 * running in the common case of command completes
		 * without error.  The sequencer will already have
		 * dma'd the SCB back up to us, so we can reference
		 * the in kernel copy directly.
		 */
		scb_index = ahc_inb(ahc, SCB_TAG);
		scb = ahc->scb_data->scbarray[scb_index];
		hscb = scb->hscb; 

		/*
		 * Set the default return value to 0 (don't
		 * send sense).  The sense code will change
		 * this if needed.
		 */
		ahc_outb(ahc, RETURN_1, 0);
		if (!(scb && (scb->flags & SCB_ACTIVE))) {
			printf("%s:%c:%d: ahc_intr - referenced scb "
			       "not valid during seqint 0x%x scb(%d)\n",
			       ahc_name(ahc), devinfo.channel,
			       devinfo.target, intstat, scb_index);
			goto unpause;
		}

		/* Don't want to clobber the original sense code */
		if ((scb->flags & SCB_SENSE) != 0) {
			/*
			 * Clear the SCB_SENSE Flag and have
			 * the sequencer do a normal command
			 * complete.
			 */
			scb->flags &= ~SCB_SENSE;
			ahc_set_ccb_status(scb->ccb, CAM_AUTOSENSE_FAIL);
			break;
		}
		ahc_set_ccb_status(scb->ccb, CAM_SCSI_STATUS_ERROR);
		csio = &scb->ccb->csio;
		csio->scsi_status = hscb->status;
		switch (hscb->status) {
		case SCSI_STATUS_OK:
			printf("%s: Interrupted for staus of 0???\n",
			       ahc_name(ahc));
			break;
		case SCSI_STATUS_CMD_TERMINATED:
		case SCSI_STATUS_CHECK_COND:
#ifdef AHC_DEBUG
			if (ahc_debug & AHC_SHOWSENSE) {
				xpt_print_path(csio->ccb_h.path);
				printf("SCB %d: requests Check Status\n",
				       scb->hscb->tag);
			}
#endif

			if ((csio->ccb_h.flags & CAM_DIS_AUTOSENSE) == 0) {
				struct ahc_dma_seg *sg = scb->ahc_dma;
				struct scsi_sense *sc =
					(struct scsi_sense *)(&hscb->cmdstore);
				struct ahc_target_tinfo *tinfo;

				/*
				 * Save off the residual if there is one.
				 */
				if (hscb->residual_SG_count != 0)
					ahc_calc_residual(scb);

#ifdef AHC_DEBUG
				if (ahc_debug & AHC_SHOWSENSE) {
					xpt_print_path(csio->ccb_h.path);
					printf("Sending Sense\n");
				}
#endif
				/*
				 * bzero from the sense data before having
				 * the drive fill it.  The SCSI spec mandates
				 * that any untransfered data should be
				 * assumed to be zero.
				 */				
				bzero(&csio->sense_data,
				      sizeof(csio->sense_data));
				sc->opcode = REQUEST_SENSE;
				sc->byte2 =  SCB_LUN(scb) << 5;
				sc->unused[0] = 0;
				sc->unused[1] = 0;
				sc->length = csio->sense_len;
				sc->control = 0;

				sg->addr = vtophys(&csio->sense_data);
				sg->len = csio->sense_len;

				/*
				 * Would be nice to preserve DISCENB here,
				 * but due to the way we page SCBs, we can't.
				 */
				hscb->control = 0;
				/*
				 * This request sense could be because the
				 * the device lost power or in some other
				 * way has lost our transfer negotiations.
				 * Renegotiate if appropriate.
				 */
				ahc_set_width(ahc, &devinfo,
					      scb->ccb->ccb_h.path,
					      MSG_EXT_WDTR_BUS_8_BIT,
					      AHC_TRANS_CUR);
				ahc_set_syncrate(ahc, &devinfo,
						 scb->ccb->ccb_h.path,
						 /*syncrate*/NULL, /*period*/0,
						 /*offset*/0, AHC_TRANS_CUR);
				scb->flags &= ~SCB_MSGOUT_BITS;
				tinfo = &ahc->transinfo[devinfo.target_offset];
				if (tinfo->goal.width) {
					ahc->wdtrpending |= devinfo.target_mask;
					hscb->control |= MK_MESSAGE;
					scb->flags |= SCB_MSGOUT_WDTR;
				} else if (tinfo->goal.period) {
					ahc->sdtrpending |= devinfo.target_mask;
					hscb->control |= MK_MESSAGE;
					scb->flags |= SCB_MSGOUT_SDTR;
				}
				hscb->status = 0;
				hscb->SG_count = 1;
				hscb->SG_pointer = scb->ahc_dmaphys;
				hscb->data = sg->addr; 
				hscb->datalen = sg->len;
				hscb->cmdpointer = hscb->cmdstore_busaddr;
				hscb->cmdlen = sizeof(*sc);
				scb->sg_count = hscb->SG_count;
				scb->flags |= SCB_SENSE;
				/*
				 * Ensure the target is busy since this
				 * will be an untagged request.
				 */
				ahc_busy_tcl(ahc, scb);
				ahc_outb(ahc, RETURN_1, SEND_SENSE);

				/*
				 * Ensure we have enough time to actually
				 * retrieve the sense.
				 */
				untimeout(ahc_timeout, (caddr_t)scb,
					  scb->ccb->ccb_h.timeout_ch);
				scb->ccb->ccb_h.timeout_ch =
				    timeout(ahc_timeout, (caddr_t)scb, 5 * hz);
				/* Freeze the queue while the sense occurs. */
				ahc_freeze_devq(ahc, scb->ccb->ccb_h.path);
				ahc_freeze_ccb(scb->ccb);
				break;
			}
			break;
		case SCSI_STATUS_BUSY:
		case SCSI_STATUS_QUEUE_FULL:
			/*
			 * Requeue any transactions that haven't been
			 * sent yet.
			 */
			ahc_freeze_devq(ahc, scb->ccb->ccb_h.path);
			ahc_freeze_ccb(scb->ccb);
			break;
		}
		break;
	}
	case TARGET_SYNC_CMD:
	{
		/*
		 * We've already processed the command.  If the command
		 * is still pending, don't unpause the sequencer until
		 * it returns.
		 */
		xpt_print_path(ahc->path);
		printf("Saw a target sync cmd\n");
		if (ahc->pending_device != NULL) {
			printf("	Pending device too.\n");
			return;
		}
		break;
	}
	case TARGET_MSG_HELP:
	{
		/*
		 * XXX Handle BDR, Abort, Abort Tag, and transfer negotiations.
		 */
		restart_sequencer(ahc);
		return;
	}
	case AWAITING_MSG:
	{
		int	scb_index;

		scb_index = ahc_inb(ahc, SCB_TAG);
		scb = ahc->scb_data->scbarray[scb_index];

		/*              
		 * To facilitate adding multiple messages together,
		 * each routine should increment the index and len
		 * variables instead of setting them explicitly.
		 */             
		ahc->msg_index = 0;
		ahc->msg_len = 0;

		/*
		 * This SCB had MK_MESSAGE set in its control byte or
		 * we have explicitly set HOST_MSG in MSG_OUT,
		 * informing the sequencer that we want to send a
		 * special message to this target.
		 */
		if ((scb->flags & SCB_DEVICE_RESET) == 0
		 && ahc_inb(ahc, MSG_OUT) == MSG_IDENTIFYFLAG
		 && (scb->hscb->control & TAG_ENB) != 0) {
			ahc->msg_buf[ahc->msg_index++] =
			    scb->ccb->csio.tag_action;
			ahc->msg_buf[ahc->msg_index++] =
			    scb->hscb->tag;
			ahc->msg_len += 2;
		}

		if (scb->flags & SCB_DEVICE_RESET) {
			ahc->msg_buf[ahc->msg_index++] = MSG_BUS_DEV_RESET;
			ahc->msg_len++;
                        xpt_print_path(scb->ccb->ccb_h.path);
			printf("Bus Device Reset Message Sent\n");
		} else if (scb->flags & SCB_ABORT) {
			if ((scb->hscb->control & TAG_ENB) != 0)
				ahc->msg_buf[ahc->msg_index++] = MSG_ABORT_TAG;
			else
				ahc->msg_buf[ahc->msg_index++] = MSG_ABORT;
			ahc->msg_len++;
			xpt_print_path(scb->ccb->ccb_h.path);
			printf("Abort Message Sent\n");
		} else if (scb->flags & SCB_MSGOUT_WDTR) {
			struct	 ahc_target_tinfo *tinfo;

			tinfo = &ahc->transinfo[devinfo.target_offset];
			ahc_construct_wdtr(ahc, tinfo->goal.width);
		} else if (scb->flags & SCB_MSGOUT_SDTR) {
			struct	ahc_target_tinfo *tinfo;
			u_int	period;
			u_int	maxsync;

			/*
			 * Now that the target is actually selected, we
			 * can further refine our sync rate based on the
			 * output transceiver mode.
			 */
			if ((ahc->features & AHC_ULTRA2) != 0) {
				if ((ahc_inb(ahc, SBLKCTL) & ENAB40) != 0
				 && (ahc_inb(ahc, SSTAT2) & EXP_ACTIVE) == 0) {
					maxsync = AHC_SYNCRATE_ULTRA2;
				} else {
					maxsync = AHC_SYNCRATE_ULTRA;
				}
			} else if ((ahc->features & AHC_ULTRA) != 0) {
				maxsync = AHC_SYNCRATE_ULTRA;
			} else {
				maxsync = AHC_SYNCRATE_FAST;
			}
			tinfo = &ahc->transinfo[devinfo.target_offset];
			period = tinfo->goal.period;
			ahc_find_syncrate(ahc, &period, maxsync);
			ahc_construct_sdtr(ahc, period, tinfo->goal.offset);
		} else {
			printf("ahc_intr: AWAITING_MSG for an SCB that "
			       "does not have a waiting message");
			panic("SCB = %d, SCB Control = %x, MSG_OUT = %x "
			      "SCB flags = %x", scb_index, scb->hscb->control,
			      ahc_inb(ahc, MSG_OUT), scb->flags);
		}

		/*
		 * Record the fact that we attempted to send a message.
		 */
		scb->flags |= SCB_MSGOUT_SENT;

		/*
		 * To actually send the message, simply turn on
		 * REQINIT interrupts and let our interrupt handler
		 * do the rest (REQINIT should already be true).
		 */
		ahc->msg_index = 0;
		ahc->msg_type = MSG_TYPE_INITIATOR_MSGOUT;
		ahc->flags |= AHC_HANDLING_REQINITS;
		ahc_outb(ahc, SIMODE1, ahc_inb(ahc, SIMODE1) | ENREQINIT);

		return;
	}
	case DATA_OVERRUN:
	{
		/*
		 * When the sequencer detects an overrun, it
		 * places the controller in "BITBUCKET" mode
		 * and allows the target to complete its transfer.
		 * Unfortunately, none of the counters get updated
		 * when the controller is in this mode, so we have
		 * no way of knowing how large the overrun was.
		 */
		u_int scbindex = ahc_inb(ahc, SCB_TAG);
		u_int lastphase = ahc_inb(ahc, LASTPHASE);
		int i;

		scb = ahc->scb_data->scbarray[scbindex];
		xpt_print_path(scb->ccb->ccb_h.path);
		printf("data overrun detected in %s phase."
		       "  Tag == 0x%x.\n",
		       lastphase == P_DATAIN ? "Data-In" : "Data-Out",
		       scb->hscb->tag);
		xpt_print_path(scb->ccb->ccb_h.path);		
		printf("%s seen Data Phase.  Length = %d.  NumSGs = %d.\n",
		       ahc_inb(ahc, SEQ_FLAGS) & DPHASE ? "Have" : "Haven't",
		       scb->ccb->csio.dxfer_len, scb->sg_count);
		for (i = 0; i < scb->sg_count - 1; i++) {
			printf("sg[%d] - Addr 0x%x : Length %d\n",
			       i,
			       scb->ahc_dma[i].addr,
			       scb->ahc_dma[i].len);
		}
		/*
		 * Set this and it will take affect when the
		 * target does a command complete.
		 */
		ahc_freeze_devq(ahc, scb->ccb->ccb_h.path);
		ahc_set_ccb_status(scb->ccb, CAM_DATA_RUN_ERR);
		ahc_freeze_ccb(scb->ccb);
		break;
	}
	case TRACEPOINT:
	{
		printf("TRACEPOINT: RETURN_2 = %d\n", ahc_inb(ahc, RETURN_2));
#if 0
		printf("SSTAT1 == 0x%x\n", ahc_inb(ahc, SSTAT1));
		printf("SSTAT0 == 0x%x\n", ahc_inb(ahc, SSTAT0));
		printf(", SCSISIGI == 0x%x\n", ahc_inb(ahc, SCSISIGI));
		printf("TRACEPOINT: CCHCNT = %d, SG_COUNT = %d\n",
		       ahc_inb(ahc, CCHCNT), ahc_inb(ahc, SG_COUNT));
		printf("TRACEPOINT: SCB_TAG = %d\n", ahc_inb(ahc, SCB_TAG));
		printf("TRACEPOINT1: CCHADDR = %d, CCHCNT = %d, SCBPTR = %d\n",
		       ahc_inb(ahc, CCHADDR)
		    | (ahc_inb(ahc, CCHADDR+1) << 8)
		    | (ahc_inb(ahc, CCHADDR+2) << 16)
		    | (ahc_inb(ahc, CCHADDR+3) << 24),
		       ahc_inb(ahc, CCHCNT)
		    | (ahc_inb(ahc, CCHCNT+1) << 8)
		    | (ahc_inb(ahc, CCHCNT+2) << 16),
		       ahc_inb(ahc, SCBPTR));
		printf("TRACEPOINT: WAITING_SCBH = %d\n", ahc_inb(ahc, WAITING_SCBH));
		printf("TRACEPOINT: SCB_TAG = %d\n", ahc_inb(ahc, SCB_TAG));
#endif
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
		       intstat, ahc_inb(ahc, SCSISIGI));
		break;
	}
	
unpause:
	/*
	 *  The sequencer is paused immediately on
	 *  a SEQINT, so we should restart it when
	 *  we're done.
	 */
	unpause_sequencer(ahc, /*unpause_always*/TRUE);
}

static void
ahc_handle_scsiint(struct ahc_softc *ahc, u_int intstat)
{
	u_int	scb_index;
	u_int	status;
	struct	scb *scb;

	scb_index = ahc_inb(ahc, SCB_TAG);
	status = ahc_inb(ahc, SSTAT1);

	if (scb_index < ahc->scb_data->numscbs) {
		scb = ahc->scb_data->scbarray[scb_index];
		if ((scb->flags & SCB_ACTIVE) == 0)
			scb = NULL;
	} else
		scb = NULL;

	if ((status & SCSIRSTI) != 0) {
		char channel;
		channel = 'A';
		if ((ahc->features & AHC_TWIN) != 0
		 && ((ahc_inb(ahc, SBLKCTL) & SELBUSB) != 0))
			channel = 'B';
		printf("%s: Someone reset channel %c\n",
			ahc_name(ahc), channel);
		ahc_reset_channel(ahc, channel, /* Initiate Reset */FALSE);
	} else if ((status & BUSFREE) != 0 && (status & SELTO) == 0) {
		/*
		 * First look at what phase we were last in.
		 * If its message out, chances are pretty good
		 * that the busfree was in response to one of
		 * our abort requests.
		 */
		u_int lastphase = ahc_inb(ahc, LASTPHASE);
		u_int saved_tcl = ahc_inb(ahc, SAVED_TCL);
		u_int target = (saved_tcl >> 4) & 0x0f;
		char channel = saved_tcl & SELBUSB ? 'B': 'A';
		int printerror = 1;

		ahc_outb(ahc, SCSISEQ,
			 ahc_inb(ahc, SCSISEQ) & (ENSELI|ENRSELI|ENAUTOATNP));
		if (lastphase == P_MESGOUT) {
			u_int message;
			u_int tag;

			message = ahc_inb(ahc, SINDEX);

			tag = SCB_LIST_NULL;
			switch (message) {
			case MSG_ABORT_TAG:
				tag = scb->hscb->tag;
				/* FALLTRHOUGH */
			case MSG_ABORT:
				xpt_print_path(scb->ccb->ccb_h.path);
				printf("SCB %d - Abort %s Completed.\n",
				       scb->hscb->tag, tag == SCB_LIST_NULL ?
				       "" : "Tag");
				if ((scb->flags & SCB_RECOVERY_SCB) != 0) {
					ahc_set_ccb_status(scb->ccb,
							   CAM_REQ_ABORTED);
					ahc_done(ahc, scb);
				}
				printerror = 0;
				break;
			case MSG_BUS_DEV_RESET:
				ahc_handle_devreset(ahc, target, channel,
						    CAM_BDR_SENT, AC_SENT_BDR,
						    "Bus Device Reset",
						    /*verbose_only*/FALSE);
				printerror = 0;
				break;
			default:
				break;
			}
		}
		if (printerror != 0) {
			if (scb != NULL) {
				u_int tag;

				if ((scb->hscb->control & TAG_ENB) != 0)
					tag = scb->hscb->tag;
				else
					tag = SCB_LIST_NULL;
				ahc_abort_scbs(ahc, target, channel,
					       SCB_LUN(scb), tag,
					       CAM_UNEXP_BUSFREE);
			} else {
				ahc_abort_scbs(ahc, target, channel,
					       ALL_LUNS, SCB_LIST_NULL,
					       CAM_UNEXP_BUSFREE);
				printf("%s: ", ahc_name(ahc));
			}
			printf("Unexpected busfree.  LASTPHASE == 0x%x\n"
			       "SEQADDR == 0x%x\n",
			       lastphase, ahc_inb(ahc, SEQADDR0)
				| (ahc_inb(ahc, SEQADDR1) << 8));
		}
		ahc_outb(ahc, MSG_OUT, MSG_NOOP);
		ahc_outb(ahc, SIMODE1,
			 ahc_inb(ahc, SIMODE1) & ~(ENBUSFREE|ENREQINIT));
		ahc->flags &= ~AHC_HANDLING_REQINITS;
		ahc_outb(ahc, CLRSINT1, CLRBUSFREE);
		ahc_outb(ahc, CLRINT, CLRSCSIINT);
		restart_sequencer(ahc);
	} else if ((status & SELTO) != 0) {
		u_int scbptr;
		u_int nextscb;

		scbptr = ahc_inb(ahc, WAITING_SCBH);
		ahc_outb(ahc, SCBPTR, scbptr);
		scb_index = ahc_inb(ahc, SCB_TAG);

		if (scb_index < ahc->scb_data->numscbs) {
			scb = ahc->scb_data->scbarray[scb_index];
			if ((scb->flags & SCB_ACTIVE) == 0)
				scb = NULL;
		} else
			scb = NULL;

		if (scb == NULL) {
			printf("%s: ahc_intr - referenced scb not "
			       "valid during SELTO scb(%d, %d)\n",
			       ahc_name(ahc), scbptr, scb_index);
		} else {
			/*
			 * Clear any pending messages for the timed out
			 * target.
			 */
			ahc_outb(ahc, MSG_OUT, MSG_NOOP);
			ahc_handle_devreset(ahc, SCB_TARGET(scb),
					    SCB_CHANNEL(scb), CAM_SEL_TIMEOUT,
					    /*ac_code*/0, "Selection Timeout",
					    /*verbose_only*/TRUE);
		}
		/* Stop the selection */
		ahc_outb(ahc, SCSISEQ, 0);

		ahc_outb(ahc, SIMODE1,
			 ahc_inb(ahc, SIMODE1) & ~ENREQINIT);
		ahc->flags &= ~AHC_HANDLING_REQINITS;

		ahc_outb(ahc, CLRSINT1, CLRSELTIMEO|CLRBUSFREE);

		ahc_outb(ahc, CLRINT, CLRSCSIINT);

		restart_sequencer(ahc);
	} else if (scb == NULL) {
		printf("%s: ahc_intr - referenced scb not "
		       "valid during scsiint 0x%x scb(%d)\n"
		       "SIMODE0 = 0x%x, SIMODE1 = 0x%x, SSTAT0 = 0x%x\n"
		       "SEQADDR = 0x%x\n", ahc_name(ahc),
			status, scb_index, ahc_inb(ahc, SIMODE0),
			ahc_inb(ahc, SIMODE1), ahc_inb(ahc, SSTAT0),
			ahc_inb(ahc, SEQADDR0) | (ahc_inb(ahc, SEQADDR1) << 8));
		ahc_outb(ahc, CLRSINT1, status);
		ahc_outb(ahc, CLRINT, CLRSCSIINT);
		unpause_sequencer(ahc, /*unpause_always*/TRUE);
		scb = NULL;
	} else if ((status & SCSIPERR) != 0) {
		/*
		 * Determine the bus phase and
		 * queue an appropriate message
		 */
		char *phase;
		u_int mesg_out = MSG_NOOP;
		u_int lastphase = ahc_inb(ahc, LASTPHASE);

		xpt_print_path(scb->ccb->ccb_h.path);

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

		printf("SEQADDR == 0x%x\n", ahc_inb(ahc, SEQADDR0)
				 | (ahc_inb(ahc, SEQADDR1) << 8));

		printf("SCSIRATE == 0x%x\n", ahc_inb(ahc, SCSIRATE));

		/*
		 * We've set the hardware to assert ATN if we   
		 * get a parity error on "in" phases, so all we  
		 * need to do is stuff the message buffer with
		 * the appropriate message.  "In" phases have set
		 * mesg_out to something other than MSG_NOP.
		 */
		if (mesg_out != MSG_NOOP) {
			ahc_outb(ahc, MSG_OUT, mesg_out);
		}
		ahc_outb(ahc, CLRSINT1, CLRSCSIPERR);
		ahc_outb(ahc, CLRINT, CLRSCSIINT);
		unpause_sequencer(ahc, /*unpause_always*/TRUE);
	} else if ((status & REQINIT) != 0
		&& (ahc->flags & AHC_HANDLING_REQINITS) != 0) {
		ahc_handle_reqinit(ahc, scb);
	} else {
		xpt_print_path(scb->ccb->ccb_h.path);
		printf("Unknown SCSIINT. Status = 0x%x\n", status);
		ahc_outb(ahc, CLRSINT1, status);
		ahc_outb(ahc, CLRINT, CLRSCSIINT);
		unpause_sequencer(ahc, /*unpause_always*/TRUE);
	}
}

static void
ahc_handle_reqinit(struct ahc_softc *ahc, struct scb *scb)
{ 
	struct	ahc_devinfo devinfo;
	u_int	simode1;

	ahc_fetch_devinfo(ahc, &devinfo);

	switch (ahc->msg_type) {
	case MSG_TYPE_INITIATOR_MSGOUT:
	{
		int	lastbyte;
		int	phasemis;
		u_int	bus_phase;

		if (ahc->msg_len == 0)
			panic("REQINIT interrupt with no active message");

		lastbyte = (ahc->msg_index == ahc->msg_len - 1);
		bus_phase = ahc_inb(ahc, SCSISIGI) & PHASE_MASK;
		phasemis = bus_phase != P_MESGOUT;

		if (lastbyte || phasemis) {
			/* Time to end our message session */
			ahc->msg_len = 0;
			ahc->msg_type = MSG_TYPE_NONE;
			simode1 = ahc_inb(ahc, SIMODE1) & ~ENREQINIT;	
			ahc_outb(ahc, SIMODE1, simode1);
			ahc_outb(ahc, CLRINT, CLRSCSIINT);
			ahc->flags &= ~AHC_HANDLING_REQINITS;

			if (phasemis == 0) {
				ahc_outb(ahc, SINDEX,
					 ahc->msg_buf[ahc->msg_index]);
				ahc_outb(ahc, RETURN_1, 0);
			} else {
				ahc_outb(ahc, RETURN_1, MSGOUT_PHASEMIS);
			}

			unpause_sequencer(ahc, /* unpause_always */TRUE);
		} else {
			/*
			 * Clear our interrupt status and present the byte
			 * on the bus, but don't unpause the sequencer.
			 */
			ahc_outb(ahc, CLRSINT1, CLRREQINIT);
			ahc_outb(ahc, CLRINT, CLRSCSIINT);
			ahc_outb(ahc, SCSIDATL, ahc->msg_buf[ahc->msg_index++]);
		}
		break;
	}
	case MSG_TYPE_INITIATOR_MSGIN:
	{
		int	phasemis;
		int	done;

		phasemis = (ahc_inb(ahc, SCSISIGI) & PHASE_MASK) != P_MESGIN;

		if (phasemis == 0) {

			ahc->msg_len++;
			/* Pull the byte in without acking it */
			ahc->msg_buf[ahc->msg_index] = ahc_inb(ahc, SCSIBUSL);
			done = ahc_parse_msg(ahc, scb, &devinfo);
			/* Ack the byte */
			ahc_outb(ahc, CLRSINT1, CLRREQINIT);
			ahc_outb(ahc, CLRINT, CLRSCSIINT);
			ahc_inb(ahc, SCSIDATL);
			ahc->msg_index++;
		}
		if (phasemis || done) {
			/* Time to end our message session */
			ahc->msg_len = 0;
			ahc->msg_type = MSG_TYPE_NONE;
			simode1 = ahc_inb(ahc, SIMODE1) & ~ENREQINIT;	
			ahc->flags &= ~AHC_HANDLING_REQINITS;
			ahc_outb(ahc, SIMODE1, simode1);
			ahc_outb(ahc, CLRINT, CLRSCSIINT);
			unpause_sequencer(ahc, /* unpause_always */TRUE);
		}
		break;
	}
	default:
		panic("Unknown REQINIT message type");
	}
}

static int
ahc_parse_msg(struct ahc_softc *ahc, struct scb *scb,
	      struct ahc_devinfo *devinfo)
{
	int	 reject;
	int	 done;
	u_int	 targ_scsirate;

	done = FALSE;
	reject = FALSE;
	targ_scsirate = ahc->transinfo[devinfo->target_offset].scsirate;
	/*
	 * Parse as much of the message as is availible,
	 * rejecting it if we don't support it.  When
	 * the entire message is availible and has been
	 * handled, return TRUE indicating that we have
	 * parsed an entire message.
	 */
	if (ahc->msg_buf[0] != MSG_EXTENDED) {
		reject = TRUE;
	}

	/*
	 * Just accept the length byte outright and perform
	 * more checking once we know the message type.
	 */
	if (!reject && (ahc->msg_len > 2)) {
		switch (ahc->msg_buf[2]) {
		case MSG_EXT_SDTR:
		{
			struct	 ahc_syncrate *syncrate;
			u_int	 period;
			u_int	 offset;
			u_int	 saved_offset;
			u_int	 maxsync;
			
			if (ahc->msg_buf[1] != MSG_EXT_SDTR_LEN) {
				reject = TRUE;
				break;
			}

			/*
			 * Wait until we have both args before validating
			 * and acting on this message.
			 */
			if (ahc->msg_len < (MSG_EXT_SDTR_LEN + /*preamble*/2))
				break;

			period = ahc->msg_buf[3];
			saved_offset = offset = ahc->msg_buf[4];
			if ((ahc->features & AHC_ULTRA2) != 0) {
				if ((ahc_inb(ahc, SBLKCTL) & ENAB40) != 0
				 && (ahc_inb(ahc, SSTAT2) & EXP_ACTIVE) == 0) {
					maxsync = AHC_SYNCRATE_ULTRA2;
				} else {
					maxsync = AHC_SYNCRATE_ULTRA;
				}
			} else if ((ahc->features & AHC_ULTRA) != 0) {
				maxsync = AHC_SYNCRATE_ULTRA;
			} else {
				maxsync = AHC_SYNCRATE_FAST;
			}
			syncrate = ahc_find_syncrate(ahc, &period, maxsync);
			ahc_validate_offset(ahc, syncrate, &offset,
					    targ_scsirate & WIDEXFER);
			ahc_set_syncrate(ahc, devinfo, scb->ccb->ccb_h.path,
					 syncrate, period, offset,
					 AHC_TRANS_ACTIVE|AHC_TRANS_GOAL);

			/*
			 * See if we initiated Sync Negotiation
			 * and didn't have to fall down to async
			 * transfers.
			 */
			if ((scb->flags & (SCB_MSGOUT_SDTR|SCB_MSGOUT_SENT))
			 == (SCB_MSGOUT_SDTR|SCB_MSGOUT_SENT)) {
				/* We started it */
				if (saved_offset != offset) {
					/* Went too low - force async */
					reject = TRUE;
				}
				scb->flags &= ~SCB_MSGOUT_BITS;
				ahc->sdtrpending &= ~devinfo->target_mask;
			} else {
				/*
				 * Send our own SDTR in reply
				 */
				scb->flags &= ~SCB_MSGOUT_BITS;
				scb->flags |= SCB_MSGOUT_SDTR;
				ahc->sdtrpending |= devinfo->target_mask;
				xpt_print_path(scb->ccb->ccb_h.path);
				printf("Sending SDTR!!\n");
				ahc_outb(ahc, MSG_OUT, HOST_MSG);
				ahc_outb(ahc, SCSISIGO,
					 ahc_inb(ahc, SCSISIGO) | ATNO);
			}
			done = TRUE;
			break;
		}
		case MSG_EXT_WDTR:
		{
			struct	ccb_trans_settings neg;
			u_int	bus_width;

			if (ahc->msg_buf[1] != MSG_EXT_WDTR_LEN) {
				reject = TRUE;
				break;
			}

			/*
			 * Wait until we have our arg before validating
			 * and acting on this message.
			 */
			if (ahc->msg_len < (MSG_EXT_WDTR_LEN + /*preamble*/2))
				break;

			bus_width = ahc->msg_buf[3];
			if ((scb->flags & (SCB_MSGOUT_WDTR|SCB_MSGOUT_SENT))
			 == (SCB_MSGOUT_WDTR|SCB_MSGOUT_SENT)) {
				/*
				 * Don't send a WDTR back to the
				 * target, since we asked first.
				 */
				switch (bus_width){
				default:
					/*
					 * How can we do anything greater
					 * than 16bit transfers on a 16bit
					 * bus?
					 */
					reject = TRUE;
					printf("%s: target %d requested %dBit "
					       "transfers.  Rejecting...\n",
					       ahc_name(ahc), devinfo->target,
					       8 * (0x01 << bus_width));
					/* FALLTHROUGH */
				case MSG_EXT_WDTR_BUS_8_BIT:
					bus_width = MSG_EXT_WDTR_BUS_8_BIT;
					break;
				case MSG_EXT_WDTR_BUS_16_BIT:
					break;
				}
				scb->flags &= ~SCB_MSGOUT_WDTR;
				ahc->wdtrpending &= ~devinfo->target_mask;
			} else {
				/*
				 * Send our own WDTR in reply
				 */
				printf("Sending WDTR!\n");
				scb->flags &= ~SCB_MSGOUT_BITS;
				scb->flags |= SCB_MSGOUT_WDTR;
				switch (bus_width) {
				default:
					if (ahc->features & AHC_WIDE) {
						/* Respond Wide */
						bus_width =
						    MSG_EXT_WDTR_BUS_16_BIT;
						break;
					}
					/* FALLTHROUGH */
				case MSG_EXT_WDTR_BUS_8_BIT:
					bus_width = MSG_EXT_WDTR_BUS_8_BIT;
					break;
				}
				ahc_outb(ahc, MSG_OUT, HOST_MSG);
				ahc_outb(ahc, SCSISIGO,
					 ahc_inb(ahc, SCSISIGO) | ATNO);
				ahc->wdtrpending |= devinfo->target_mask;
			}
			ahc_set_width(ahc, devinfo, scb->ccb->ccb_h.path,
				      bus_width,
				      AHC_TRANS_ACTIVE|AHC_TRANS_GOAL);

			/* After a wide message, we are async */
			ahc_set_syncrate(ahc, devinfo, scb->ccb->ccb_h.path,
					 /*syncrate*/NULL, /*period*/0,
					 /*offset*/0, AHC_TRANS_ACTIVE);
			if ((ahc->wdtrpending & devinfo->target_mask) == 0
			 && (reject == 0)) {
				struct ahc_target_tinfo *tinfo;

				scb->flags &= ~SCB_MSGOUT_WDTR;
				tinfo = &ahc->transinfo[devinfo->target_offset];
				if (tinfo->goal.period) {
					/* Start the sync negotiation */
					ahc->sdtrpending |=
					    devinfo->target_mask;
					scb->flags |= SCB_MSGOUT_SDTR;
					ahc_outb(ahc, MSG_OUT, HOST_MSG);
					ahc_outb(ahc, SCSISIGO,
						 ahc_inb(ahc, SCSISIGO) | ATNO);
				}
			}
			done = TRUE;
			break;
		}
		default:
			/* Unknown extended message.  Reject it. */
			reject = TRUE;
			break;
		}
	}

	if (reject) {
		/*
		 * Assert attention and setup to
		 * reject the message.
		 */
		ahc_outb(ahc, MSG_OUT, MSG_MESSAGE_REJECT);
		ahc_outb(ahc, SCSISIGO, ahc_inb(ahc, SCSISIGO) | ATNO);
		done = TRUE;
	}
	return (done);
}

static void
ahc_handle_devreset(struct ahc_softc *ahc, int target, char channel,
		    cam_status status, ac_code acode, char *message,
		    int verbose_only)
{
	struct ahc_devinfo devinfo;
	struct cam_path *path;
	path_id_t path_id;
	u_int16_t targ_mask;
	u_int	  targ_scsirate;
	int scratch_offset = target;
	int found;
	int error;

	ahc_compile_devinfo(&devinfo, target, channel);

	if (channel == 'B')
		path_id = cam_sim_path(ahc->sim_b);
	else 
		path_id = cam_sim_path(ahc->sim);

	error = xpt_create_path(&path, /*periph*/NULL, path_id, target,
				CAM_LUN_WILDCARD);
	/*
	 * Go back to async/narrow transfers and renegotiate.
	 */
	if (error == CAM_REQ_CMP) {
		ahc_set_width(ahc, &devinfo, path, MSG_EXT_WDTR_BUS_8_BIT,
			      AHC_TRANS_CUR);
		ahc_set_syncrate(ahc, &devinfo, path, /*syncrate*/NULL,
				 /*period*/0, /*offset*/0, AHC_TRANS_CUR);
	}
	found = ahc_abort_scbs(ahc, target, channel, ALL_LUNS,
			       SCB_LIST_NULL, status);
	
	if (error == CAM_REQ_CMP && acode != 0)
		xpt_async(AC_SENT_BDR, path, NULL);

	if (error == CAM_REQ_CMP)
		xpt_free_path(path);

	if (message != NULL
	 && (verbose_only == 0 || bootverbose != 0))
		printf("%s: %s on %c:%d. %d SCBs aborted\n", ahc_name(ahc),
		       message, channel, target, found);
}
/*
 * We have an scb which has been processed by the
 * adaptor, now we look to see how the operation
 * went.
 */
static void
ahc_done(struct ahc_softc *ahc, struct scb *scb)
{
	union ccb *ccb;

	CAM_DEBUG(scb->ccb->ccb_h.path, CAM_DEBUG_TRACE,
		  ("ahc_done - scb %d\n", scb->hscb->tag));

	ccb = scb->ccb;
	LIST_REMOVE(&ccb->ccb_h, sim_links.le);

	untimeout(ahc_timeout, (caddr_t)scb, ccb->ccb_h.timeout_ch);

	if ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
		bus_dmasync_op_t op;

		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
			op = BUS_DMASYNC_POSTREAD;
		else
			op = BUS_DMASYNC_POSTWRITE;
		bus_dmamap_sync(ahc->dmat, scb->dmamap, op);
		bus_dmamap_unload(ahc->dmat, scb->dmamap);
	}

	/*
	 * Unbusy this target/channel/lun.
	 * XXX if we are holding two commands per lun, 
	 *     send the next command.
	 */
	ahc_index_busy_tcl(ahc, scb->hscb->tcl, /*unbusy*/TRUE);

	if (ccb->ccb_h.func_code == XPT_CONT_TARGET_IO) {
		xpt_print_path(ccb->ccb_h.path);
		printf("CONT_TARGET_IO complete\n");
		ccb->ccb_h.status = CAM_REQ_CMP;
		ahc_free_scb(ahc, scb);
		xpt_done(ccb);
		return;
	}

	/*
	 * If the recovery SCB completes, we have to be
	 * out of our timeout.
	 */
	if ((scb->flags & SCB_RECOVERY_SCB) != 0) {

		struct	ccb_hdr *ccbh;

		/*
		 * We were able to complete the command successfully,
		 * so reinstate the timeouts for all other pending
		 * commands.
		 */
		ccbh = ahc->pending_ccbs.lh_first;
		while (ccbh != NULL) {
			struct scb *pending_scb;

			pending_scb = (struct scb *)ccbh->ccb_scb_ptr;
			ccbh->timeout_ch = 
			    timeout(ahc_timeout, pending_scb,
				    (ccbh->timeout * hz)/1000);
			ccbh = LIST_NEXT(ccbh, sim_links.le);
		}

		/*
		 * Ensure that we didn't put a second instance of this
		 * SCB into the QINFIFO.
		 */
		ahc_search_qinfifo(ahc, SCB_TARGET(scb), SCB_CHANNEL(scb),
				   SCB_LUN(scb), scb->hscb->tag, /*status*/0,
				   SEARCH_REMOVE);
		if (ahc_ccb_status(ccb) == CAM_BDR_SENT)
			ahc_set_ccb_status(ccb, CAM_CMD_TIMEOUT);
		xpt_print_path(ccb->ccb_h.path);
		printf("no longer in timeout, status = %x\n",
		       ccb->ccb_h.status);
	}

	if ((scb->flags & (SCB_MSGOUT_WDTR|SCB_MSGOUT_SDTR)) != 0) {
		/*
		 * Turn off the pending flags for any DTR messages
		 * regardless of whether they completed successfully 
		 * or not.  This ensures that we don't have lingering
		 * state after we abort an SCB.
		 */
		u_int16_t mask;

		mask = (0x01 <<	(SCB_TARGET(scb)
				 | (SCB_IS_SCSIBUS_B(scb) ? SELBUSB : 0)));
		if (scb->flags & SCB_MSGOUT_WDTR)
			ahc->wdtrpending &= ~mask;
		if (scb->flags & SCB_MSGOUT_SDTR)
			ahc->sdtrpending &= ~mask;
	}
	/* Don't clobber any existing error state */
	if (ahc_ccb_status(ccb) == CAM_REQ_INPROG) {
		ccb->ccb_h.status |= CAM_REQ_CMP;
	} else if ((scb->flags & SCB_SENSE) != 0) {
		/* We performed autosense retrieval */
		scb->ccb->ccb_h.status |= CAM_AUTOSNS_VALID;
	}
	ccb->ccb_h.status &= ~CAM_SIM_QUEUED;
	ahc_free_scb(ahc, scb);
	xpt_done(ccb);
}

/*
 * Determine the number of SCBs available on the controller
 */
int
ahc_probe_scbs(struct ahc_softc *ahc) {
	int i;

	for (i = 0; i < AHC_SCB_MAX; i++) {
		ahc_outb(ahc, SCBPTR, i);
		ahc_outb(ahc, SCB_CONTROL, i);
		if (ahc_inb(ahc, SCB_CONTROL) != i)
			break;
		ahc_outb(ahc, SCBPTR, 0);
		if (ahc_inb(ahc, SCB_CONTROL) != 0)
			break;
	}

	return (i);
}

/*
 * Start the board, ready for normal operation
 */
int
ahc_init(struct ahc_softc *ahc)
{
	int	max_targ = 15;
	int	i;
	int	term;
	u_int	scsi_conf, sxfrctl1;

#ifdef AHC_PRINT_SRAM
	printf("Scratch Ram:");
	for (i = 0x20; i < 0x5f; i++) {
		if (((i % 8) == 0) && (i != 0)) {
			printf ("\n              ");
		}
		printf (" 0x%x", ahc_inb(ahc, i));
	}
	if ((ahc->features & AHC_MORE_SRAM) != 0) {
		for (i = 0x70; i < 0x7f; i++) {
			if (((i % 8) == 0) && (i != 0)) {
				printf ("\n              ");
			}
			printf (" 0x%x", ahc_inb(ahc, i));
		}
	}
	printf ("\n");
#endif

	/*
	 * Assume we have a board at this stage and it has been reset.
	 */
	if ((ahc->flags & AHC_USEDEFAULTS) != 0) {
		ahc->our_id = ahc->our_id_b = 7;
	}
	
	/*
	 * XXX Would be better to use a per device flag, but PCI and EISA
	 *     devices don't have them yet.
	 */
	if ((AHC_TMODE_ENABLE & (0x01 << ahc->unit)) != 0)
		ahc->flags |= AHC_TARGETMODE;

	if ((ahc->features & AHC_TWIN) != 0) {
 		printf("Twin Channel, A SCSI Id=%d, B SCSI Id=%d, ",
		       ahc->our_id, ahc->our_id_b);
	} else {
		if ((ahc->features & AHC_WIDE) != 0) {
			printf("Wide ");
		} else {
			printf("Single ");
		}
		printf("Channel %c, SCSI Id=%d, ", ahc->channel, ahc->our_id);
	}

	ahc_outb(ahc, SEQ_FLAGS, 0);

	/* Determine the number of SCBs and initialize them */

	if (ahc->scb_data->maxhscbs == 0) {
		ahc->scb_data->maxhscbs = ahc_probe_scbs(ahc);
		/* SCB 0 heads the free list */
		ahc_outb(ahc, FREE_SCBH, 0);
		for (i = 0; i < ahc->scb_data->maxhscbs; i++) {
			ahc_outb(ahc, SCBPTR, i);

			/* Clear the control byte. */
			ahc_outb(ahc, SCB_CONTROL, 0);

			/* Set the next pointer */
			ahc_outb(ahc, SCB_NEXT, i+1);

			/* Make the tag number invalid */
			ahc_outb(ahc, SCB_TAG, SCB_LIST_NULL);
		}

		/* Make that the last SCB terminates the free list */
		ahc_outb(ahc, SCBPTR, i-1);
		ahc_outb(ahc, SCB_NEXT, SCB_LIST_NULL);

		/* Ensure we clear the 0 SCB's control byte. */
		ahc_outb(ahc, SCBPTR, 0);
		ahc_outb(ahc, SCB_CONTROL, 0);

		ahc->scb_data->maxhscbs = i;
	}

	if (ahc->scb_data->maxhscbs == 0)
		panic("%s: No SCB space found", ahc_name(ahc));

	if (ahc->scb_data->maxhscbs < AHC_SCB_MAX) {
		ahc->flags |= AHC_PAGESCBS;
		ahc->scb_data->maxscbs = AHC_SCB_MAX;
		if ((ahc->flags & AHC_TARGETMODE) != 0) {
			/* Steal one slot for TMODE commands */
			ahc->scb_data->maxscbs--;
		}
		printf("%d/%d SCBs\n", ahc->scb_data->maxhscbs,
		       ahc->scb_data->maxscbs);
	} else {
		ahc->scb_data->maxscbs = ahc->scb_data->maxhscbs;
		if ((ahc->flags & AHC_TARGETMODE) != 0) {
			/* Steal one slot for TMODE commands */
			ahc->scb_data->maxscbs--;
		}
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
	if (ahc->features & AHC_TWIN) {

		/*
		 * The device is gated to channel B after a chip reset,
		 * so set those values first
		 */
		term = (ahc->flags & AHC_TERM_ENB_B) != 0 ? STPWEN : 0;
		if ((ahc->features & AHC_ULTRA2) != 0)
			ahc_outb(ahc, SCSIID_ULTRA2, ahc->our_id_b);
		else
			ahc_outb(ahc, SCSIID, ahc->our_id_b);
		scsi_conf = ahc_inb(ahc, SCSICONF + 1);
		sxfrctl1 = ahc_inb(ahc, SXFRCTL1);
		ahc_outb(ahc, SXFRCTL1, (scsi_conf & (ENSPCHK|STIMESEL))
					|term
					|ENSTIMER|ACTNEGEN);
		ahc_outb(ahc, SIMODE1, ENSELTIMO|ENSCSIRST|ENSCSIPERR);
		ahc_outb(ahc, SXFRCTL0, DFON|SPIOEN);

		if (scsi_conf & RESET_SCSI) {
			/* Reset the bus */
			if (bootverbose)
				printf("%s: Resetting Channel B\n",
				       ahc_name(ahc));
			ahc_reset_current_bus(ahc);
		}

		/* Select Channel A */
		ahc_outb(ahc, SBLKCTL, ahc_inb(ahc, SBLKCTL) & ~SELBUSB);
	}
	term = (ahc->flags & AHC_TERM_ENB_A) != 0 ? STPWEN : 0;
	if ((ahc->features & AHC_ULTRA2) != 0)
		ahc_outb(ahc, SCSIID_ULTRA2, ahc->our_id);
	else
		ahc_outb(ahc, SCSIID, ahc->our_id);
	scsi_conf = ahc_inb(ahc, SCSICONF);
	sxfrctl1 = ahc_inb(ahc, SXFRCTL1);
	ahc_outb(ahc, SXFRCTL1, (scsi_conf & (ENSPCHK|STIMESEL))
				|term
				|ENSTIMER|ACTNEGEN);
	ahc_outb(ahc, SIMODE1, ENSELTIMO|ENSCSIRST|ENSCSIPERR);
	ahc_outb(ahc, SXFRCTL0, DFON|SPIOEN);

	if ((ahc->features & AHC_ULTRA2) != 0) {
		/* Wait for our transceiver status to settle */
		i = 1000000;
		while (--i && ((ahc_inb(ahc, SBLKCTL) & (ENAB40|ENAB20)) == 0))
			DELAY(100);

		if (i == 0)
			panic("%s: Transceiver state never settled\n",
			      ahc_name(ahc)); 
	}

	if (scsi_conf & RESET_SCSI) {
		/* Reset the bus */
		if (bootverbose)
			printf("%s: Resetting Channel %c\n", ahc_name(ahc),
			       ahc->channel);

		ahc_reset_current_bus(ahc);
	}

	/*
	 * Look at the information that board initialization or
	 * the board bios has left us.  In the lower four bits of each
	 * target's scratch space any value other than 0 indicates
	 * that we should initiate synchronous transfers.  If it's zero,
	 * the user or the BIOS has decided to disable synchronous
	 * negotiation to that target so we don't activate the needsdtr
	 * flag.
	 */
	ahc->ultraenb = 0;	
	ahc->tagenable = ALL_TARGETS;

	/* Grab the disconnection disable table and invert it for our needs */
	if (ahc->flags & AHC_USEDEFAULTS) {
		printf("%s: Host Adapter Bios disabled.  Using default SCSI "
			"device parameters\n", ahc_name(ahc));
		ahc->flags |= AHC_EXTENDED_TRANS_A|AHC_EXTENDED_TRANS_B;
		ahc->discenable = ALL_TARGETS;
		if ((ahc->features & AHC_ULTRA) != 0)
			ahc->ultraenb = 0xffff;
	} else {
		ahc->discenable = ~((ahc_inb(ahc, DISC_DSB + 1) << 8)
				   | ahc_inb(ahc, DISC_DSB));
		if ((ahc->features & (AHC_ULTRA|AHC_ULTRA2)) != 0)
			ahc->ultraenb = (ahc_inb(ahc, ULTRA_ENB + 1) << 8)
				      | ahc_inb(ahc, ULTRA_ENB);
	}

	if ((ahc->features & (AHC_WIDE|AHC_TWIN)) == 0)
		max_targ = 7;

	for (i = 0; i <= max_targ; i++) {
		struct ahc_target_tinfo *transinfo;

		transinfo = &ahc->transinfo[i];
		/* Default to async narrow across the board */
		bzero(transinfo, sizeof(*transinfo));
		if (ahc->flags & AHC_USEDEFAULTS) {
			if ((ahc->features & AHC_WIDE) != 0)
				transinfo->user.width = MSG_EXT_WDTR_BUS_16_BIT;

			/*
			 * These will be truncated when we determine the
			 * connection type we have with the target.
			 */
			transinfo->user.period = ahc_syncrates->period;
			transinfo->user.offset = ~0;
		} else {
			u_int scsirate;
			u_int16_t mask;

			/* Take the settings leftover in scratch RAM. */
			scsirate = ahc_inb(ahc, TARG_SCSIRATE + i);
			mask = (0x01 << i);
			if ((ahc->features & AHC_ULTRA2) != 0) {
				u_int offset;

				if ((scsirate & SOFS) == 0x0F) {
					/*
					 * Haven't negotiated yet,
					 * so the format is different.
					 */
					scsirate = (scsirate & SXFR) >> 4
						 | (ahc->ultraenb & mask)
						  ? 0x18 : 0x10
						 | (scsirate & WIDEXFER);
					offset = MAX_OFFSET_ULTRA2;
				} else
					offset = ahc_inb(ahc, TARG_OFFSET + i);
				ahc_find_period(ahc, scsirate,
						AHC_SYNCRATE_ULTRA2);
				if (offset == 0)
					transinfo->user.period = 0;
				else
					transinfo->user.offset = ~0;
			} else if ((scsirate & SOFS) != 0) {
				transinfo->user.period = 
				    ahc_find_period(ahc, scsirate,
						    (ahc->ultraenb & mask)
						   ? AHC_SYNCRATE_ULTRA
						   : AHC_SYNCRATE_FAST);
				if ((scsirate & SOFS) != 0
				 && transinfo->user.period != 0) {
					transinfo->user.offset = ~0;
				}
			}
			if ((scsirate & WIDEXFER) != 0
			 && (ahc->features & AHC_WIDE) != 0) {
				transinfo->user.width = MSG_EXT_WDTR_BUS_16_BIT;
			}
			
		}
	}
	ahc->sdtrpending = 0;
	ahc->wdtrpending = 0;

#ifdef AHC_DEBUG
	if (ahc_debug & AHC_SHOWMISC)
		printf("NEEDSDTR == 0x%x\nNEEDWDTR == 0x%x\n"
		       "DISCENABLE == 0x%x\nULTRAENB == 0x%x\n",
		       ahc->needsdtr_orig, ahc->needwdtr_orig,
		       ahc->discenable, ahc->ultraenb);
#endif
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

		array_size = ahc->scb_data->maxscbs*sizeof(struct hardware_scb);
		if (array_size > PAGE_SIZE) {
			ahc->scb_data->hscbs = (struct hardware_scb *)
					contigmalloc(array_size, M_DEVBUF,
						     M_NOWAIT, 0ul, 0xffffffff,
						     PAGE_SIZE, 0x10000);
		} else {
			ahc->scb_data->hscbs = (struct hardware_scb *)
				     malloc(array_size, M_DEVBUF, M_NOWAIT);
		}
	
		if (ahc->scb_data->hscbs == NULL) {
			printf("%s: unable to allocate hardware SCB array.  "
			       "Failing attach\n", ahc_name(ahc));
			return (-1);
		}
		/* At least the control byte of each hscb needs to be zeroed */
		bzero(ahc->scb_data->hscbs, array_size);
	}

	if ((ahc->flags & AHC_TARGETMODE) != 0) {
		size_t array_size;

		ahc->num_targetcmds = 32;
		array_size = ahc->num_targetcmds * sizeof(struct target_cmd);
		ahc->targetcmds = malloc(array_size, M_DEVBUF, M_NOWAIT);

		if (ahc->targetcmds == NULL) {
			printf("%s: unable to allocate targetcmd array.  "
			       "Failing attach\n", ahc_name(ahc));
			return (-1);
		}

		bzero(ahc->targetcmds, array_size);
		ahc_outb(ahc, TMODE_CMDADDR_NEXT, 0);
	}

	/*
	 * Tell the sequencer where it can find the our arrays in memory.
	 */
	{
		u_int32_t physaddr;

		/* Tell the sequencer where it can find the hscb array. */
		physaddr = vtophys(ahc->scb_data->hscbs);
		ahc_outb(ahc, HSCB_ADDR, physaddr & 0xFF);
		ahc_outb(ahc, HSCB_ADDR + 1, (physaddr >> 8) & 0xFF);
		ahc_outb(ahc, HSCB_ADDR + 2, (physaddr >> 16) & 0xFF);
		ahc_outb(ahc, HSCB_ADDR + 3, (physaddr >> 24) & 0xFF);
		ahc->hscb_busaddr = physaddr;

		physaddr = vtophys(ahc->qoutfifo);
		ahc_outb(ahc, SCBID_ADDR, physaddr & 0xFF);
		ahc_outb(ahc, SCBID_ADDR + 1, (physaddr >> 8) & 0xFF);
		ahc_outb(ahc, SCBID_ADDR + 2, (physaddr >> 16) & 0xFF);
		ahc_outb(ahc, SCBID_ADDR + 3, (physaddr >> 24) & 0xFF);

		if ((ahc->flags & AHC_TARGETMODE) != 0) {
			physaddr = vtophys(ahc->targetcmds);
			ahc_outb(ahc, TMODE_CMDADDR, physaddr & 0xFF);
			ahc_outb(ahc, TMODE_CMDADDR + 1,
				 (physaddr >> 8) & 0xFF);
			ahc_outb(ahc, TMODE_CMDADDR + 2,
				 (physaddr >> 16) & 0xFF);
			ahc_outb(ahc, TMODE_CMDADDR + 3,
				 (physaddr >> 24) & 0xFF);

			ahc_outb(ahc, CMDSIZE_TABLE, 5);
			ahc_outb(ahc, CMDSIZE_TABLE + 1, 9);
			ahc_outb(ahc, CMDSIZE_TABLE + 2, 9);
			ahc_outb(ahc, CMDSIZE_TABLE + 3, 0);
			ahc_outb(ahc, CMDSIZE_TABLE + 4, 15);
			ahc_outb(ahc, CMDSIZE_TABLE + 5, 11);
			ahc_outb(ahc, CMDSIZE_TABLE + 6, 0);
			ahc_outb(ahc, CMDSIZE_TABLE + 7, 0);
		}
		
		/* There are no untagged SCBs active yet. */
		for (i = 0; i < sizeof(ahc->untagged_scbs); i++) {
			ahc->untagged_scbs[i] = SCB_LIST_NULL;
		}
		for (i = 0; i < sizeof(ahc->qoutfifo); i++) {
			ahc->qoutfifo[i] = SCB_LIST_NULL;
		}
	}	

	/* Our Q FIFOs are empty. */
	ahc_outb(ahc, KERNEL_QINPOS, 0);
	ahc_outb(ahc, QINPOS, 0);
	ahc_outb(ahc, QOUTPOS, 0);

	/*
	 * Use the built in queue management registers
	 * if they are available.
	 */
	if ((ahc->features & AHC_QUEUE_REGS) != 0) {
		ahc_outb(ahc, QOFF_CTLSTA, SCB_QSIZE_256);
		ahc_outb(ahc, SDSCB_QOFF, 0);
		ahc_outb(ahc, SNSCB_QOFF, 0);
		ahc_outb(ahc, HNSCB_QOFF, 0);
	}


	/* We don't have any waiting selections */
	ahc_outb(ahc, WAITING_SCBH, SCB_LIST_NULL);

	/* Our disconnection list is empty too */
	ahc_outb(ahc, DISCONNECTED_SCBH, SCB_LIST_NULL);

	/* Message out buffer starts empty */
	ahc_outb(ahc, MSG_OUT, MSG_NOOP);

	/*
	 * Load the Sequencer program and Enable the adapter
	 * in "fast" mode.
         */
	if (bootverbose)
		printf("%s: Downloading Sequencer Program...",
		       ahc_name(ahc));

	ahc_loadseq(ahc);

	/* We have to wait until after any system dumps... */
	at_shutdown(ahc_shutdown, ahc, SHUTDOWN_FINAL);

	return (0);
}

static void
ahcminphys(struct buf *bp)
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

static cam_status
ahc_find_tmode_devs(struct ahc_softc *ahc, struct cam_sim *sim, union ccb *ccb,
		    struct tmode_tstate **tstate, struct tmode_lstate **lstate,
		    int notfound_failure)
{
	int our_id;

	/*
	 * If we are not configured for target mode, someone
	 * is really confused to be sending this to us.
	 */
	if ((ahc->flags & AHC_TARGETMODE) == 0)
		return (CAM_REQ_INVALID);

	/* Range check target and lun */
	if (cam_sim_bus(sim) == 0)
		our_id = ahc->our_id;
	else
		our_id = ahc->our_id_b;
	if (ccb->ccb_h.target_id > ((ahc->features & AHC_WIDE) ? 15 : 7)
	 || ((ahc->features & AHC_MULTI_TID) == 0
	  && (ccb->ccb_h.target_id != our_id)))
		return (CAM_TID_INVALID);

	if (ccb->ccb_h.target_lun > 8)
		return (CAM_LUN_INVALID);

	*tstate = ahc->enabled_targets[ccb->ccb_h.target_id];
	*lstate = NULL;
	if (*tstate != NULL)
		*lstate = (*tstate)->enabled_luns[ccb->ccb_h.target_lun];

	if (notfound_failure != 0 && *lstate == NULL)
		return (CAM_PATH_INVALID);

	return (CAM_REQ_CMP);
}

static void
ahc_action(struct cam_sim *sim, union ccb *ccb)
{
	struct	  ahc_softc *ahc;
	struct	  tmode_lstate *lstate;
	int	  target_id;
	int	  s;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("ahc_action\n"));
	
	ahc = (struct ahc_softc *)cam_sim_softc(sim);

	target_id = ccb->ccb_h.target_id;
	
	switch (ccb->ccb_h.func_code) {
	/* Common cases first */
	case XPT_ACCEPT_TARGET_IO:	/* Accept Host Target Mode CDB */
	case XPT_CONT_TARGET_IO:/* Continue Host Target I/O Connection*/
	{
		struct	   tmode_tstate *tstate;
		cam_status status;

		status = ahc_find_tmode_devs(ahc, sim, ccb, &tstate,
					     &lstate, TRUE);

		if (status != CAM_REQ_CMP) {
			ccb->ccb_h.status = status;
			xpt_done(ccb);
			break;
		}
		if (ccb->ccb_h.func_code == XPT_ACCEPT_TARGET_IO) {
			SLIST_INSERT_HEAD(&lstate->accept_tios, &ccb->ccb_h,
					  sim_links.sle);
			ccb->ccb_h.status = CAM_REQ_INPROG;
			break;
		}

		/*
		 * The target_id represents the target we attempt to
		 * select.  In target mode, this is the initiator of
		 * the original command.
		 */
		target_id = ccb->csio.init_id;
		xpt_print_path(ccb->ccb_h.path);
		printf("Sending a continue TIO\n");
		/* FALLTHROUGH */
	}
	case XPT_SCSI_IO:	/* Execute the requested I/O operation */
	case XPT_RESET_DEV:	/* Bus Device Reset the specified SCSI device */
	{
		struct	   scb *scb;
		struct	   hardware_scb *hscb;	
		struct	   ahc_target_tinfo *tinfo;
		u_int16_t  mask;

		/*
		 * get an scb to use.
		 */
		if ((scb = ahc_get_scb(ahc)) == NULL) {
			int s;
	
			s = splcam();
			ahc->flags |= AHC_RESOURCE_SHORTAGE;
			splx(s);
			xpt_freeze_simq(ahc->sim, /*count*/1);
			ahc_set_ccb_status(ccb, CAM_REQUEUE_REQ);
			xpt_done(ccb);
			return;
		}
		
		hscb = scb->hscb;
		
		CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_SUBTRACE,
			  ("start scb(%p)\n", scb));
		scb->ccb = ccb;
		/*
		 * So we can find the SCB when an abort is requested
		 */
		ccb->ccb_h.ccb_scb_ptr = scb;
		ccb->ccb_h.ccb_ahc_ptr = ahc;

		/*
		 * Put all the arguments for the xfer in the scb
		 */
		hscb->tcl = ((target_id << 4) & 0xF0)
			| (SIM_IS_SCSIBUS_B(ahc, sim) ? SELBUSB : 0)
			| (ccb->ccb_h.target_lun & 0x07);

		mask = SCB_TARGET_MASK(scb);
		tinfo = &ahc->transinfo[SCB_TARGET_OFFSET(scb)];

		hscb->scsirate = tinfo->scsirate;
		hscb->scsioffset = tinfo->current.offset;
		if ((ahc->ultraenb & mask) != 0)
			hscb->control |= ULTRAENB;
		
		if ((ahc->discenable & mask) != 0
		 && (ccb->ccb_h.flags & CAM_DIS_DISCONNECT) == 0)
			hscb->control |= DISCENB;
		
		if (ccb->ccb_h.func_code == XPT_RESET_DEV) {
			hscb->cmdpointer = NULL;
			scb->flags |= SCB_DEVICE_RESET;
			hscb->control |= MK_MESSAGE;
			ahc_execute_scb(scb, NULL, 0, 0);
		} else {
			if (ccb->ccb_h.func_code == XPT_SCSI_IO) {
				if (tinfo->current.width != tinfo->goal.width) {
					if ((ahc->wdtrpending & mask) == 0) {
						ahc->wdtrpending |= mask;
						hscb->control |= MK_MESSAGE;
						scb->flags |= SCB_MSGOUT_WDTR;
					}
				} else if ((tinfo->current.period
					 != tinfo->goal.period)
					&& (ahc->sdtrpending & mask) == 0) {
					ahc->sdtrpending |= mask;
					hscb->control |= MK_MESSAGE;		
					scb->flags |= SCB_MSGOUT_SDTR;
				}
			} else {
				if (ahc->pending_device == lstate) {
					scb->flags |= SCB_TARGET_IMMEDIATE;
					ahc->pending_device = NULL;
				}
				hscb->control |= TARGET_SCB;
				hscb->cmdpointer = IDENTIFY_SEEN;
				if ((ccb->ccb_h.flags & CAM_SEND_STATUS) != 0) {
					hscb->cmdpointer |= SPHASE_PENDING;
					hscb->status = ccb->csio.scsi_status;
				}

				/* Overloaded with tag ID */
				hscb->cmdlen = ccb->csio.tag_id;
				/*
				 * Overloaded with our target ID to
				 * use for reselection.
				 */
				hscb->next = ccb->ccb_h.target_id;
			}
			if (ccb->ccb_h.flags & CAM_TAG_ACTION_VALID)
				hscb->control |= ccb->csio.tag_action;
			
			ahc_setup_data(ahc, &ccb->csio, scb);
		}
		break;
	}
	case XPT_NOTIFY_ACK:
	case XPT_IMMED_NOTIFY:
	{
		struct	   tmode_tstate *tstate;
		struct	   tmode_lstate *lstate;
		cam_status status;

		status = ahc_find_tmode_devs(ahc, sim, ccb, &tstate,
					     &lstate, TRUE);

		if (status != CAM_REQ_CMP) {
			ccb->ccb_h.status = status;
			xpt_done(ccb);
			break;
		}
		if (ccb->ccb_h.func_code == XPT_NOTIFY_ACK) {
			/* Clear notification state */
		}
		SLIST_INSERT_HEAD(&lstate->immed_notifies, &ccb->ccb_h,
				  sim_links.sle);
		ccb->ccb_h.status = CAM_REQ_INPROG;
		break;
	}
	case XPT_EN_LUN:		/* Enable LUN as a target */
	{
		struct	   tmode_tstate *tstate;
		struct	   tmode_lstate *lstate;
		struct	   ccb_en_lun *cel;
		cam_status status;
		int	   target;
		int	   lun;

		status = ahc_find_tmode_devs(ahc, sim, ccb, &tstate, &lstate,
					     /* notfound_failure*/FALSE);

		if (status != CAM_REQ_CMP) {
			ccb->ccb_h.status = status;
			xpt_done(ccb);
			break;
		}
			
		cel = &ccb->cel;
		target = ccb->ccb_h.target_id;
		lun = ccb->ccb_h.target_lun;
		if (cel->enable != 0) {
			/* Are we already enabled?? */
			if (lstate != NULL) {
				ccb->ccb_h.status = CAM_LUN_ALRDY_ENA;
				xpt_done(ccb);
				break;
			}

			if (cel->grp6_len != 0
			 || cel->grp7_len != 0) {
				/*
				 * Don't (yet?) support vendor
				 * specific commands.
				 */
				ccb->ccb_h.status = CAM_REQ_INVALID;
				xpt_done(ccb);
				break;
			}

			/*
			 * Seems to be okay.
			 * Setup our data structures.
			 */
			if (tstate == NULL) {
				tstate = malloc(sizeof(*tstate),
						M_DEVBUF, M_NOWAIT);
				if (tstate == NULL) {
					ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
					xpt_done(ccb);
					break;
				}
				bzero(tstate, sizeof(*tstate));
				ahc->enabled_targets[target] = tstate;
			}
			lstate = malloc(sizeof(*lstate), M_DEVBUF, M_NOWAIT);
			if (lstate == NULL) {
				ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
				xpt_done(ccb);
				break;
			}
			bzero(lstate, sizeof(*lstate));
			SLIST_INIT(&lstate->accept_tios);
			SLIST_INIT(&lstate->immed_notifies);
			tstate->enabled_luns[lun] = lstate;
			if ((ahc->features & AHC_MULTI_TID) != 0) {
				u_int16_t targid_mask;

				pause_sequencer(ahc);
				targid_mask = ahc_inb(ahc, TARGID)
					    | (ahc_inb(ahc, TARGID + 1) << 8);

				targid_mask |= (0x01 << target);
				ahc_outb(ahc, TARGID, targid_mask);
				ahc_outb(ahc, TARGID+1, (targid_mask >> 8));
				unpause_sequencer(ahc, /*always?*/FALSE);
			}
			ccb->ccb_h.status = CAM_REQ_CMP;
			xpt_print_path(ccb->ccb_h.path);
			printf("Lun now enabled for target mode\n");
			xpt_done(ccb);
			break;
		} else {
			/* XXX Fully Implement Disable */
			if (lstate == NULL) {
				ccb->ccb_h.status = CAM_LUN_INVALID;
				xpt_done(ccb);
				break;
			}
			ccb->ccb_h.status = CAM_REQ_CMP;
			xpt_done(ccb);
			break;
		}
		break;
	}
	case XPT_ABORT:			/* Abort the specified CCB */
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	case XPT_SET_TRAN_SETTINGS:
	{
		struct	  ahc_devinfo devinfo;
		struct	  ccb_trans_settings *cts;
		struct	  ahc_target_tinfo *tinfo;
		u_int	  update_type;
		int	  s;

		cts = &ccb->cts;
		ahc_compile_devinfo(&devinfo, cts->ccb_h.target_id,
				    SIM_IS_SCSIBUS_B(ahc, sim) ? 'B' : 'A');
		tinfo = &ahc->transinfo[devinfo.target_offset];
		update_type = 0;
		if ((cts->flags & CCB_TRANS_CURRENT_SETTINGS) != 0)
			update_type |= AHC_TRANS_GOAL;
		if ((cts->flags & CCB_TRANS_USER_SETTINGS) != 0)
			update_type |= AHC_TRANS_USER;
		
		s = splcam();

		if ((cts->valid & CCB_TRANS_DISC_VALID) != 0) {
			if ((cts->flags & CCB_TRANS_DISC_ENB) != 0)
				ahc->discenable |= devinfo.target_mask;
			else
				ahc->discenable &= ~devinfo.target_mask;
		}
		
		if ((cts->valid & CCB_TRANS_TQ_VALID) != 0) {
			if ((cts->flags & CCB_TRANS_TAG_ENB) != 0)
				ahc->tagenable |= devinfo.target_mask;
			else
				ahc->tagenable &= ~devinfo.target_mask;
		}	

		if ((cts->valid & CCB_TRANS_BUS_WIDTH_VALID) != 0) {
			switch (cts->bus_width) {
			case MSG_EXT_WDTR_BUS_16_BIT:
				if ((ahc->features & AHC_WIDE) != 0)
					break;
				/* FALLTHROUGH to 8bit */
			case MSG_EXT_WDTR_BUS_32_BIT:
			case MSG_EXT_WDTR_BUS_8_BIT:
			default:
				cts->bus_width = MSG_EXT_WDTR_BUS_8_BIT;
				break;
			}
			if ((update_type & AHC_TRANS_GOAL) != 0)
				tinfo->goal.width = cts->bus_width;
			if ((update_type & AHC_TRANS_USER) != 0)
				tinfo->user.width = cts->bus_width;
		}

		if ((cts->valid &  CCB_TRANS_SYNC_RATE_VALID) != 0) {
			struct ahc_syncrate *syncrate;
			u_int maxsync;

			if ((ahc->features & AHC_ULTRA2) != 0)
				maxsync = AHC_SYNCRATE_ULTRA2;
			else if ((ahc->features & AHC_ULTRA) != 0)
				maxsync = AHC_SYNCRATE_ULTRA;
			else
				maxsync = AHC_SYNCRATE_FAST;

			if ((cts->valid & CCB_TRANS_SYNC_OFFSET_VALID) != 0) {
				if (cts->sync_offset != 0)
					cts->sync_offset = ~0;
			} else {
				cts->sync_offset = 0;
			}

			syncrate = ahc_find_syncrate(ahc, &cts->sync_period,
						     maxsync);
			ahc_validate_offset(ahc, syncrate, &cts->sync_offset,
					    tinfo->goal.width);

			/* We use a period of 0 to represent async */
			if (cts->sync_offset == 0)
				cts->sync_period = 0;

			if ((update_type & AHC_TRANS_GOAL) != 0) {
				tinfo->goal.period = cts->sync_period;
				tinfo->goal.offset = cts->sync_offset;
			}
			if ((update_type & AHC_TRANS_USER) != 0) {
				tinfo->user.period = cts->sync_period;
				tinfo->user.offset = cts->sync_offset;
			}
		}
		splx(s);
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_GET_TRAN_SETTINGS:
	/* Get default/user set transfer settings for the target */
	{
		struct	  ahc_devinfo devinfo;
		struct	  ccb_trans_settings *cts;
		struct	  ahc_target_tinfo *targ_info;
		struct	  ahc_transinfo *tinfo;
		int	  s;

		cts = &ccb->cts;
		ahc_compile_devinfo(&devinfo, cts->ccb_h.target_id,
				    SIM_IS_SCSIBUS_B(ahc, sim) ? 'B' : 'A');
		targ_info = &ahc->transinfo[devinfo.target_offset];
		
		if ((cts->flags & CCB_TRANS_CURRENT_SETTINGS) != 0)
			tinfo = &targ_info->current;
		else
			tinfo = &targ_info->user;
		
		s = splcam();

		cts->flags &= ~(CCB_TRANS_DISC_ENB|CCB_TRANS_TAG_ENB);
		if ((ahc->discenable & devinfo.target_mask) != 0)
			cts->flags |= CCB_TRANS_DISC_ENB;

		if ((ahc->tagenable & devinfo.target_mask) != 0)
			cts->flags |= CCB_TRANS_TAG_ENB;

		cts->sync_period = tinfo->period;
		cts->sync_offset = tinfo->offset;
		cts->bus_width = tinfo->width;
		
		splx(s);

		cts->valid = CCB_TRANS_SYNC_RATE_VALID
			   | CCB_TRANS_SYNC_OFFSET_VALID
			   | CCB_TRANS_BUS_WIDTH_VALID
			   | CCB_TRANS_DISC_VALID
			   | CCB_TRANS_TQ_VALID;

		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_CALC_GEOMETRY:
	{
		struct	  ccb_calc_geometry *ccg;
		u_int32_t size_mb;
		u_int32_t secs_per_cylinder;
		int	  extended;

		ccg = &ccb->ccg;
		size_mb = ccg->volume_size
			/ ((1024L * 1024L) / ccg->block_size);
		extended = SIM_IS_SCSIBUS_B(ahc, sim)
			? ahc->flags & AHC_EXTENDED_TRANS_B
			: ahc->flags & AHC_EXTENDED_TRANS_A;
		
		if (size_mb > 1024 && extended) {
			ccg->heads = 255;
			ccg->secs_per_track = 63;
		} else {
			ccg->heads = 64;
			ccg->secs_per_track = 32;
		}
		secs_per_cylinder = ccg->heads * ccg->secs_per_track;
		ccg->cylinders = ccg->volume_size / secs_per_cylinder;
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_RESET_BUS:		/* Reset the specified SCSI bus */
	{
		struct cam_path *path;
		char channel;
		int  found;
		
		s = splcam();
		if (SIM_IS_SCSIBUS_B(ahc, sim)) {
			channel = 'B';
			path = ahc->path_b;
		} else {
			channel = 'A';
			path = ahc->path;
		}
		found = ahc_reset_channel(ahc, channel, /*initiate reset*/TRUE);
		splx(s);
		if (bootverbose) {
			xpt_print_path(path);
			printf("SCSI bus reset delivered. "
			       "%d SCBs aborted.\n", found);
		}
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_TERM_IO:		/* Terminate the I/O process */
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	case XPT_PATH_INQ:		/* Path routing inquiry */
	{
		struct ccb_pathinq *cpi = &ccb->cpi;
		
		cpi->version_num = 1; /* XXX??? */
		cpi->hba_inquiry = PI_SDTR_ABLE|PI_TAG_ABLE;
		if ((ahc->features & AHC_WIDE) != 0)
			cpi->hba_inquiry |= PI_WIDE_16;
		if ((ahc->flags & AHC_TARGETMODE) != 0) {
			cpi->target_sprt = PIT_PROCESSOR
					 | PIT_DISCONNECT
					 | PIT_TERM_IO;
		} else {
			cpi->target_sprt = 0;
		}
		cpi->hba_misc = 0;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = (ahc->features & AHC_WIDE) ? 15 : 7;
		cpi->max_lun = 7;
		if (SIM_IS_SCSIBUS_B(ahc, sim))
			cpi->initiator_id = ahc->our_id_b;
		else
			cpi->initiator_id = ahc->our_id;
		cpi->bus_id = cam_sim_bus(sim);
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "Adaptec", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	default:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
}

static void
ahc_async(void *callback_arg, u_int32_t code, struct cam_path *path, void *arg)
{
	struct ahc_softc *ahc;
	struct cam_sim *sim;

	sim = (struct cam_sim *)callback_arg;
	ahc = (struct ahc_softc *)cam_sim_softc(sim);
	switch (code) {
	case AC_LOST_DEVICE:
	{
		struct	ahc_devinfo devinfo;

		ahc_compile_devinfo(&devinfo, xpt_path_target_id(path),
				    SIM_IS_SCSIBUS_B(ahc, sim) ? 'B' : 'A');

		/*
		 * Revert to async/narrow transfers
		 * for the next device.
		 */
		pause_sequencer(ahc);
		ahc_set_width(ahc, &devinfo, path, MSG_EXT_WDTR_BUS_8_BIT,
			      AHC_TRANS_GOAL|AHC_TRANS_CUR);
		ahc_set_syncrate(ahc, &devinfo, path, /*syncrate*/NULL,
				 /*period*/0, /*offset*/0,
				 AHC_TRANS_GOAL|AHC_TRANS_CUR);
		unpause_sequencer(ahc, /*unpause always*/FALSE);
		break;
	}
	default:
		break;
	}
}

static void
ahc_execute_scb(void *arg, bus_dma_segment_t *dm_segs, int nsegments,
		int error)
{
	struct	 scb *scb;
	union	 ccb *ccb;
	struct	 ahc_softc *ahc;
	int	 s;

	scb = (struct scb *)arg;
	ccb = scb->ccb;
	ahc = (struct ahc_softc *)ccb->ccb_h.ccb_ahc_ptr;

	if (nsegments != 0) {
		struct	  ahc_dma_seg *sg;
		bus_dma_segment_t *end_seg;
		bus_dmasync_op_t op;

		end_seg = dm_segs + nsegments;

		/* Copy the first SG into the data pointer area */
		scb->hscb->SG_pointer = scb->ahc_dmaphys;
		scb->hscb->data = dm_segs->ds_addr;
		scb->hscb->datalen = dm_segs->ds_len;
		dm_segs++;

		/* Copy the remaining segments into our SG list */
		sg = scb->ahc_dma;
		while (dm_segs < end_seg) {
			sg->addr = dm_segs->ds_addr;
			sg->len = dm_segs->ds_len;
			sg++;
			dm_segs++;
		}

		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
			op = BUS_DMASYNC_PREREAD;
		else
			op = BUS_DMASYNC_PREWRITE;

		bus_dmamap_sync(ahc->dmat, scb->dmamap, op);
	} else {
		scb->hscb->SG_pointer = 0;
		scb->hscb->data = 0;
		scb->hscb->datalen = 0;
	}
	
	scb->sg_count = scb->hscb->SG_count = nsegments;

	s = splcam();

	/*
	 * Last time we need to check if this SCB needs to
	 * be aborted.
	 */
	if (ahc_ccb_status(ccb) != CAM_REQ_INPROG) {
		if (nsegments != 0)
			bus_dmamap_unload(ahc->dmat, scb->dmamap);
		ahc_free_scb(ahc, scb);
		xpt_done(ccb);
		splx(s);
		return;
	}

	/* Busy this tcl if we are untagged */
	if ((scb->hscb->control & TAG_ENB) == 0)
		ahc_busy_tcl(ahc, scb);
		
	LIST_INSERT_HEAD(&ahc->pending_ccbs, &ccb->ccb_h,
			 sim_links.le);

	scb->flags |= SCB_ACTIVE;
	ccb->ccb_h.status |= CAM_SIM_QUEUED;

	ccb->ccb_h.timeout_ch =
	    timeout(ahc_timeout, (caddr_t)scb,
		    (ccb->ccb_h.timeout * hz) / 1000);

	if ((scb->flags & SCB_TARGET_IMMEDIATE) != 0) {
		xpt_print_path(ccb->ccb_h.path);
		printf("Returning an immediate CTIO\n");
		if ((ahc->flags & AHC_PAGESCBS) == 0)
			ahc_outb(ahc, SCBPTR, scb->hscb->tag);
		ahc_outb(ahc, SCB_TAG, scb->hscb->tag);
		unpause_sequencer(ahc, /*unpause_always*/TRUE);
	} else {

		ahc->qinfifo[ahc->qinfifonext++] = scb->hscb->tag;

		if ((ahc->features & AHC_QUEUE_REGS) != 0) {
			ahc_outb(ahc, HNSCB_QOFF, ahc->qinfifonext);
		} else {
			pause_sequencer(ahc);
			ahc_outb(ahc, KERNEL_QINPOS, ahc->qinfifonext);
			unpause_sequencer(ahc, /*unpause_always*/FALSE);
		}
	}

	splx(s);
}

static void
ahc_poll(struct cam_sim *sim)
{
	ahc_intr(cam_sim_softc(sim));
}

static void
ahc_setup_data(struct ahc_softc *ahc, struct ccb_scsiio *csio,
	       struct scb *scb)
{
	struct hardware_scb *hscb;
	struct ccb_hdr *ccb_h;
	
	hscb = scb->hscb;
	ccb_h = &csio->ccb_h;
	
	if (ccb_h->func_code == XPT_SCSI_IO) {
		hscb->cmdlen = csio->cdb_len;
		if ((ccb_h->flags & CAM_CDB_POINTER) != 0) {
			if ((ccb_h->flags & CAM_CDB_PHYS) == 0)
				if (hscb->cmdlen <= 16) {
					memcpy(hscb->cmdstore, 
					       csio->cdb_io.cdb_ptr,
					       hscb->cmdlen);
					hscb->cmdpointer = 
					    hscb->cmdstore_busaddr;
				} else 
					hscb->cmdpointer =
					    vtophys(csio->cdb_io.cdb_ptr);
			else
				hscb->cmdpointer =
					(u_int32_t)csio->cdb_io.cdb_ptr;
		} else {
			/*
			 * CCB CDB Data Storage area is only 16 bytes
			 * so no additional testing is required
			 */
			memcpy(hscb->cmdstore, csio->cdb_io.cdb_bytes,
			       hscb->cmdlen);
			hscb->cmdpointer = hscb->cmdstore_busaddr;
		}
	}
		
	/* Only use S/G if there is a transfer */
	if ((ccb_h->flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
		if ((ccb_h->flags & CAM_SCATTER_VALID) == 0) {
			/* We've been given a pointer to a single buffer */
			if ((ccb_h->flags & CAM_DATA_PHYS) == 0) {
				int s;
				int error;

				s = splsoftvm();
				error = bus_dmamap_load(ahc->dmat,
							scb->dmamap,
							csio->data_ptr,
							csio->dxfer_len,
							ahc_execute_scb,
							scb, /*flags*/0);
				if (error == EINPROGRESS) {
					/*
					 * So as to maintain ordering,
					 * freeze the controller queue
					 * until our mapping is
					 * returned.
					 */
					xpt_freeze_simq(ahc->sim,
							/*count*/1);
					scb->ccb->ccb_h.status |=
					    CAM_RELEASE_SIMQ;
				}
				splx(s);
			} else {
				struct bus_dma_segment seg;

				/* Pointer to physical buffer */
				if (csio->dxfer_len > AHC_MAXTRANSFER_SIZE)
					panic("ahc_setup_data - Transfer size "
					      "larger than can device max");

				seg.ds_addr = (bus_addr_t)csio->data_ptr;
				seg.ds_len = csio->dxfer_len;
				ahc_execute_scb(scb, &seg, 1, 0);
			}
		} else {
			struct bus_dma_segment *segs;

			if ((ccb_h->flags & CAM_DATA_PHYS) != 0)
				panic("ahc_setup_data - Physical segment "
				      "pointers unsupported");

			if ((ccb_h->flags & CAM_SG_LIST_PHYS) == 0)
				panic("ahc_setup_data - Virtual segment "
				      "addresses unsupported");

			/* Just use the segments provided */
			segs = (struct bus_dma_segment *)csio->data_ptr;
			ahc_execute_scb(scb, segs, csio->sglist_cnt, 0);
		}
		if (ccb_h->func_code == XPT_CONT_TARGET_IO) {
			hscb->cmdpointer |= DPHASE_PENDING;
			if ((ccb_h->flags & CAM_DIR_MASK) == CAM_DIR_IN) 
				hscb->cmdpointer |= (TARGET_DATA_IN << 8);
		}
	} else {
		ahc_execute_scb(scb, NULL, 0, 0);
	}
}

static void
ahc_freeze_devq(struct ahc_softc *ahc, struct cam_path *path)
{
	int	target;
	char	channel;
	int	lun;

	target = xpt_path_target_id(path);
	lun = xpt_path_lun_id(path);
	channel = xpt_path_sim(path)->bus_id == 0 ? 'A' : 'B';
	
	ahc_search_qinfifo(ahc, target, channel, lun,
			   /*tag*/SCB_LIST_NULL, CAM_REQUEUE_REQ,
			   SEARCH_COMPLETE);
}

/*
 * An scb (and hence an scb entry on the board) is put onto the
 * free list.
 */
static void
ahc_free_scb(struct ahc_softc *ahc, struct scb *scb)
{       
	struct hardware_scb *hscb;
	int opri;

	hscb = scb->hscb;

	opri = splcam();

	if ((ahc->flags & AHC_RESOURCE_SHORTAGE) != 0
	 && (scb->ccb->ccb_h.status & CAM_RELEASE_SIMQ) == 0) {
		scb->ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
		ahc->flags &= ~AHC_RESOURCE_SHORTAGE;
	}

	/* Clean up for the next user */
	scb->flags = SCB_FREE;
	hscb->control = 0;
	hscb->status = 0;

	STAILQ_INSERT_HEAD(&ahc->scb_data->free_scbs, scb, links);
	splx(opri);
}

/*
 * Get a free scb, either one already assigned to a hardware slot
 * on the adapter or one that will require an SCB to be paged out before
 * use. If there are none, see if we can allocate a new SCB.  Otherwise
 * either return an error or sleep.
 */
static struct scb *
ahc_get_scb(struct ahc_softc *ahc)
{
	struct scb *scbp;
	int opri;

	opri = splcam();
	if ((scbp = STAILQ_FIRST(&ahc->scb_data->free_scbs))) {
		STAILQ_REMOVE_HEAD(&ahc->scb_data->free_scbs, links);
	} else if (ahc->scb_data->numscbs < ahc->scb_data->maxscbs) {
		scbp = ahc_alloc_scb(ahc);
		if (scbp == NULL)
			printf("%s: Can't malloc SCB\n", ahc_name(ahc));
	}

	splx(opri);

	return (scbp);
}


static struct scb *
ahc_alloc_scb(struct ahc_softc *ahc)
{
	static	struct ahc_dma_seg *next_sg_array = NULL;
	static	int sg_arrays_free = 0;
	struct	scb *newscb;
	int	error;

	newscb = (struct scb *) malloc(sizeof(struct scb), M_DEVBUF, M_NOWAIT);
	if (newscb != NULL) {
		bzero(newscb, sizeof(struct scb));
		error = bus_dmamap_create(ahc->dmat, /*flags*/0,
					  &newscb->dmamap);
		if (error != 0)
			printf("%s: Unable to allocate SCB dmamap - error %d\n",
			       ahc_name(ahc), error);
			
		if (error == 0 && next_sg_array == NULL) {
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
		if (error == 0 && next_sg_array != NULL) {
			struct hardware_scb *hscb;

			newscb->ahc_dma = next_sg_array;
			newscb->ahc_dmaphys = vtophys(next_sg_array);
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
			hscb->residual_SG_count = 0;
			hscb->cmdstore_busaddr = 
				ahc_hscb_busaddr(ahc, hscb->tag)
			      + offsetof(struct hardware_scb, cmdstore);	
			/*
			 * Place in the scbarray
			 * Never is removed.
			 */
			ahc->scb_data->scbarray[hscb->tag] = newscb;
			ahc->scb_data->numscbs++;
		} else {
			free(newscb, M_DEVBUF);
			newscb = NULL;
		}
	}
	return newscb;
}

static void
ahc_loadseq(struct ahc_softc *ahc)
{
	struct patch *cur_patch;
	int i;
	int downloaded;
	int skip_addr;
	u_int8_t download_consts[4];

	/* Setup downloadable constant table */
	download_consts[TMODE_NUMCMDS] = ahc->num_targetcmds;

	cur_patch = patches;
	downloaded = 0;
	skip_addr = 0;
	ahc_outb(ahc, SEQCTL, PERRORDIS|FAILDIS|FASTMODE|LOADRAM);
	ahc_outb(ahc, SEQADDR0, 0);
	ahc_outb(ahc, SEQADDR1, 0);

	for (i = 0; i < sizeof(seqprog)/4; i++) {
		if (ahc_check_patch(ahc, &cur_patch, i, &skip_addr) == 0) {
			/*
			 * Don't download this instruction as it
			 * is in a patch that was removed.
			 */
                        continue;
		}
		ahc_download_instr(ahc, i, download_consts);
		downloaded++;
	}
	ahc_outb(ahc, SEQCTL, PERRORDIS|FAILDIS|FASTMODE);
	restart_sequencer(ahc);

	if (bootverbose)
		printf(" %d instructions downloaded\n", downloaded);
}

static int
ahc_check_patch(struct ahc_softc *ahc, struct patch **start_patch,
		int start_instr, int *skip_addr)
{
	struct	patch *cur_patch;
	struct	patch *last_patch;
	int	patch_index;
	int	num_patches;

	num_patches = sizeof(patches)/sizeof(struct patch);
	last_patch = &patches[num_patches];
	cur_patch = *start_patch;

	while (cur_patch < last_patch && start_instr == cur_patch->begin) {

		if (cur_patch->patch_func(ahc) == 0) {
			int skip;

			/* Start rejecting code */
			*skip_addr = start_instr + cur_patch->skip_instr;
			cur_patch += cur_patch->skip_patch;
		} else {
			/* Accepted this patch.  Advance to the next
			 * one and wait for our intruction pointer to
			 * hit this point.
			 */
			cur_patch++;
		}
	}

	*start_patch = cur_patch;
	if (start_instr < *skip_addr)
		/* Still skipping */
		return (0);

	return (1);
}

static void
ahc_download_instr(struct ahc_softc *ahc, int instrptr, u_int8_t *dconsts)
{
	union	ins_formats instr;
	struct	ins_format1 *fmt1_ins;
	struct	ins_format3 *fmt3_ins;
	int	fmt3;
	u_int	opcode;

	/* Structure copy */
	instr = *(union ins_formats*)&seqprog[instrptr * 4];

	fmt1_ins = &instr.format1;
	fmt3_ins = NULL;

	/* Pull the opcode */
	opcode = instr.format1.opcode;
	switch (opcode) {
	case AIC_OP_JMP:
	case AIC_OP_JC:
	case AIC_OP_JNC:
	case AIC_OP_CALL:
	case AIC_OP_JNE:
	case AIC_OP_JNZ:
	case AIC_OP_JE:
	case AIC_OP_JZ:
	{
		struct patch *cur_patch;
		int address_offset;
		u_int address;
		int skip_addr;
		int i;

		fmt3_ins = &instr.format3;
		address_offset = 0;
		address = fmt3_ins->address;
		cur_patch = patches;
		skip_addr = 0;

		for (i = 0; i < address;) {

			ahc_check_patch(ahc, &cur_patch, i, &skip_addr);

			if (skip_addr > i) {
				int end_addr;

				end_addr = MIN(address, skip_addr);
				address_offset += end_addr - i;
				i = skip_addr;
			} else {
				i++;
			}
		}
		address -= address_offset;
		fmt3_ins->address = address;
		/* FALLTHROUGH */
	}
	case AIC_OP_OR:
	case AIC_OP_AND:
	case AIC_OP_XOR:
	case AIC_OP_ADD:
	case AIC_OP_ADC:
	case AIC_OP_BMOV:
		if (fmt1_ins->parity != 0) {
			fmt1_ins->immediate = dconsts[fmt1_ins->immediate];
		}
		fmt1_ins->parity = 0;
		/* FALLTHROUGH */
	case AIC_OP_ROL:
		if ((ahc->features & AHC_ULTRA2) != 0) {
			int i, count;

			/* Calculate odd parity for the instruction */
			for (i = 0, count = 0; i < 31; i++) {
				u_int32_t mask;

				mask = 0x01 << i;
				if ((instr.integer & mask) != 0)
					count++;
			}
			if ((count & 0x01) == 0)
				instr.format1.parity = 1;
		} else {
			/* Compress the instruction for older sequencers */
			if (fmt3_ins != NULL) {
				instr.integer =
					fmt3_ins->immediate
				      | (fmt3_ins->source << 8)
				      | (fmt3_ins->address << 16)
				      |	(fmt3_ins->opcode << 25);
			} else {
				instr.integer =
					fmt1_ins->immediate
				      | (fmt1_ins->source << 8)
				      | (fmt1_ins->destination << 16)
				      |	(fmt1_ins->ret << 24)
				      |	(fmt1_ins->opcode << 25);
			}
		}
		ahc_outsb(ahc, SEQRAM, instr.bytes, 4);
		break;
	default:
		panic("Unknown opcode encountered in seq program");
		break;
	}
}

static void
ahc_set_recoveryscb(struct ahc_softc *ahc, struct scb *scb) {

	if ((scb->flags & SCB_RECOVERY_SCB) == 0) {
		struct ccb_hdr *ccbh;

		scb->flags |= SCB_RECOVERY_SCB;

		/*
		 * Take all queued, but not sent SCBs out of the equation.
		 * Also ensure that no new CCBs are queued to us while we
		 * try to fix this problem.
		 */
		if ((scb->ccb->ccb_h.status & CAM_RELEASE_SIMQ) == 0) {
			xpt_freeze_simq(ahc->sim, /*count*/1);
			scb->ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
		}

		/*
		 * Go through all of our pending SCBs and remove
		 * any scheduled timeouts for them.  We will reschedule
		 * them after we've successfully fixed this problem.
		 */
		ccbh = ahc->pending_ccbs.lh_first;
		while (ccbh != NULL) {
			struct scb *pending_scb;

			pending_scb = (struct scb *)ccbh->ccb_scb_ptr;
			untimeout(ahc_timeout, pending_scb, ccbh->timeout_ch);
			ccbh = ccbh->sim_links.le.le_next;
		}
	}
}

static void
ahc_timeout(void *arg)
{
	struct	scb *scb;
	struct	ahc_softc *ahc;
	int	s, found;
	u_int	bus_state;
	int	target;
	int	lun;
	char	channel;

	scb = (struct scb *)arg; 
	ahc = (struct ahc_softc *)scb->ccb->ccb_h.ccb_ahc_ptr;

	s = splcam();

	/*
	 * Ensure that the card doesn't do anything
	 * behind our back.  Also make sure that we
	 * didn't "just" miss an interrupt that would
	 * affect this timeout.
	 */
	do {
		ahc_intr(ahc);
		pause_sequencer(ahc);
	} while (ahc_inb(ahc, INTSTAT) & INT_PEND);

	if ((scb->flags & SCB_ACTIVE) == 0) {
		/* Previous timeout took care of me already */
		printf("Timedout SCB handled by another timeout\n");
		unpause_sequencer(ahc, /*unpause_always*/TRUE);
		splx(s);
		return;
	}

	target = SCB_TARGET(scb);
	channel = SCB_CHANNEL(scb);
	lun = SCB_LUN(scb);

	xpt_print_path(scb->ccb->ccb_h.path);
	printf("SCB 0x%x - timed out ", scb->hscb->tag);
	/*
	 * Take a snapshot of the bus state and print out
	 * some information so we can track down driver bugs.
	 */
	bus_state = ahc_inb(ahc, LASTPHASE);

	switch(bus_state)
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
	case P_BUSFREE:
		printf("while idle, LASTPHASE == 0x%x",
			bus_state);
		break;
	default:
		/* 
		 * We aren't in a valid phase, so assume we're
		 * idle.
		 */
		printf("invalid phase, LASTPHASE == 0x%x",
			bus_state);
		bus_state = P_BUSFREE;
		break;
	}

	printf(", SCSISIGI == 0x%x\n", ahc_inb(ahc, SCSISIGI));

	printf("SEQADDR == 0x%x\n", ahc_inb(ahc, SEQADDR0)
				 | (ahc_inb(ahc, SEQADDR1) << 8));
	printf("SSTAT1 == 0x%x\n", ahc_inb(ahc, SSTAT1));
#if 0
	printf("SCSIRATE == 0x%x\n", ahc_inb(ahc, SCSIRATE));
	printf("CCSCBCTL == 0x%x\n", ahc_inb(ahc, CCSCBCTL));
	printf("CCSCBCNT == 0x%x\n", ahc_inb(ahc, CCSCBCNT));
	printf("DFCNTRL == 0x%x\n", ahc_inb(ahc, DFCNTRL));
	printf("DFSTATUS == 0x%x\n", ahc_inb(ahc, DFSTATUS));
	printf("CCHCNT == 0x%x\n", ahc_inb(ahc, CCHCNT));
#endif
	/* Decide our course of action */
	if (scb->flags & SCB_DEVICE_RESET) {
		/*
		 * Been down this road before.
		 * Do a full bus reset.
		 */
bus_reset:
		ahc_set_ccb_status(scb->ccb, CAM_CMD_TIMEOUT);
		found = ahc_reset_channel(ahc, channel, /*Initiate Reset*/TRUE);
		printf("%s: Issued Channel %c Bus Reset. "
		       "%d SCBs aborted\n", ahc_name(ahc), channel, found);
	} else {
		/*
		 * Send a Bus Device Reset message:
		 * The target that is holding up the bus may not
		 * be the same as the one that triggered this timeout
		 * (different commands have different timeout lengths).
		 * Our strategy here is to queue a BDR message
		 * to the timed out target if the bus is idle.
		 * Otherwise, if we have an active target we stuff the
		 * message buffer with a BDR message and assert ATN
		 * in the hopes that the target will let go of the bus
		 * and go to the mesgout phase.  If this fails, we'll
		 * get another timeout 2 seconds later which will attempt
		 * a bus reset.
		 */
		u_int active_scb_index;

		active_scb_index = ahc_inb(ahc, SCB_TAG);

		if (bus_state != P_BUSFREE 
		  && (active_scb_index < ahc->scb_data->numscbs)) {
			struct scb *active_scb;

			/*
			 * If the active SCB is not from our device,
			 * assume that another device is hogging the bus
			 * and wait for it's timeout to expire before
			 * taking additional action.
			 */ 
			active_scb = ahc->scb_data->scbarray[active_scb_index];
			if (active_scb->hscb->tcl != scb->hscb->tcl
			 && (scb->flags & SCB_OTHERTCL_TIMEOUT) == 0) {
				struct	ccb_hdr *ccbh;
				u_int	newtimeout;

				scb->flags |= SCB_OTHERTCL_TIMEOUT;
				newtimeout = MAX(active_scb->ccb->ccb_h.timeout,
						 scb->ccb->ccb_h.timeout);
				ccbh = &scb->ccb->ccb_h;
				scb->ccb->ccb_h.timeout_ch =
				    timeout(ahc_timeout, scb,
					    (newtimeout * hz) / 1000);
				splx(s);
				return;
			}
			ahc_set_recoveryscb(ahc, active_scb);
			ahc_outb(ahc, MSG_OUT, MSG_BUS_DEV_RESET);
			ahc_outb(ahc, SCSISIGO, bus_state|ATNO);
			xpt_print_path(active_scb->ccb->ccb_h.path);
			printf("BDR message in message buffer\n");
			active_scb->flags |=  SCB_DEVICE_RESET;
			active_scb->ccb->ccb_h.timeout_ch =
			    timeout(ahc_timeout, (caddr_t)active_scb, 2 * hz);
			unpause_sequencer(ahc, /*unpause_always*/TRUE);
		} else {
			int	 disconnected;

			if (ahc_search_qinfifo(ahc, target, channel, lun,
					       scb->hscb->tag, /*status*/0,
					       SEARCH_COUNT) > 0) {
				disconnected = FALSE;
			} else {
				disconnected = TRUE;
			}

			if (disconnected) {

				ahc_set_recoveryscb(ahc, scb);
				/*
				 * Simply set the MK_MESSAGE control bit.
				 */
				scb->hscb->control |= MK_MESSAGE;
				scb->flags |= SCB_QUEUED_MSG
					   |  SCB_DEVICE_RESET;

				/*
				 * Remove this SCB from the disconnected
				 * list so that a reconnect at this point
				 * causes a BDR.
				 */
				ahc_search_disc_list(ahc, target, channel, lun,
						     scb->hscb->tag);
				ahc_index_busy_tcl(ahc, scb->hscb->tcl,
						   /*unbusy*/TRUE);

				/*
				 * Actually re-queue this SCB in case we can
				 * select the device before it reconnects.
				 * Clear out any entries in the QINFIFO first
				 * so we are the next SCB for this target
				 * to run.
				 */
				ahc_search_qinfifo(ahc, SCB_TARGET(scb),
						   channel, SCB_LUN(scb),
						   SCB_LIST_NULL,
						   CAM_REQUEUE_REQ,
						   SEARCH_COMPLETE);
				xpt_print_path(scb->ccb->ccb_h.path);
				printf("Queuing a BDR SCB\n");
				ahc->qinfifo[ahc->qinfifonext++] =
				    scb->hscb->tag;
				if ((ahc->features & AHC_QUEUE_REGS) != 0) {
					ahc_outb(ahc, HNSCB_QOFF,
						 ahc->qinfifonext);
				} else {
					ahc_outb(ahc, KERNEL_QINPOS,
						 ahc->qinfifonext);
				}
				scb->ccb->ccb_h.timeout_ch =
				    timeout(ahc_timeout, (caddr_t)scb, 2 * hz);
				unpause_sequencer(ahc, /*unpause_always*/FALSE);
			} else {
				/* Go "immediatly" to the bus reset */
				/* This shouldn't happen */
				ahc_set_recoveryscb(ahc, scb);
				xpt_print_path(scb->ccb->ccb_h.path);
				printf("SCB %d: Immediate reset.  "
					"Flags = 0x%x\n", scb->hscb->tag,
					scb->flags);
				goto bus_reset;
			}
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
static u_int
ahc_find_scb(struct ahc_softc *ahc, struct scb *scb)
{
	u_int8_t saved_scbptr;
	u_int8_t curindex;

	saved_scbptr = ahc_inb(ahc, SCBPTR);
	for (curindex = 0; curindex < ahc->scb_data->maxhscbs; curindex++) {
		ahc_outb(ahc, SCBPTR, curindex);
		if (ahc_inb(ahc, SCB_TAG) == scb->hscb->tag)
			break;
	}
	ahc_outb(ahc, SCBPTR, saved_scbptr);
	if (curindex >= ahc->scb_data->maxhscbs)
		curindex = SCB_LIST_NULL;

	return curindex;
}

static int
ahc_search_qinfifo(struct ahc_softc *ahc, int target, char channel,
		   int lun, u_int tag, u_int32_t status,
		   ahc_search_action action)
{
	struct	 scb *scbp;
	u_int8_t qinpos;
	u_int8_t qintail;
	int	 found;

	qinpos = ahc_inb(ahc, QINPOS);
	qintail = ahc->qinfifonext;
	found = 0;

	/*
	 * Start with an empty queue.  Entries that are not chosen
	 * for removal will be re-added to the queue as we go.
	 */
	ahc->qinfifonext = qinpos;

	while (qinpos != qintail) {
		scbp = ahc->scb_data->scbarray[ahc->qinfifo[qinpos]];
		if (ahc_match_scb(scbp, target, channel, lun, tag)) {
			/*
			 * We found an scb that needs to be removed.
			 */
			switch (action) {
			case SEARCH_COMPLETE:
				if (ahc_ccb_status(scbp->ccb) == CAM_REQ_INPROG)
					ahc_set_ccb_status(scbp->ccb, status);
				ahc_freeze_ccb(scbp->ccb);
				ahc_done(ahc, scbp);
				break;
			case SEARCH_COUNT:
				ahc->qinfifo[ahc->qinfifonext++] =
				    scbp->hscb->tag;
				break;
			case SEARCH_REMOVE:
				break;
			}
			found++;
		} else {
			ahc->qinfifo[ahc->qinfifonext++] = scbp->hscb->tag;
		}
		qinpos++;
	}

	if ((ahc->features & AHC_QUEUE_REGS) != 0) {
		ahc_outb(ahc, HNSCB_QOFF, ahc->qinfifonext);
	} else {
		ahc_outb(ahc, KERNEL_QINPOS, ahc->qinfifonext);
	}

	return (found);
}


/*
 * Abort all SCBs that match the given description (target/channel/lun/tag),
 * setting their status to the passed in status if the status has not already
 * been modified from CAM_REQ_INPROG.  This routine assumes that the sequencer
 * is paused before it is called.
 */
static int
ahc_abort_scbs(struct ahc_softc *ahc, int target, char channel,
	       int lun, u_int tag, u_int32_t status)
{
	struct	scb *scbp;
	u_int	active_scb;
	int	i;
	int	found;

	/* restore this when we're done */
	active_scb = ahc_inb(ahc, SCBPTR);

	found = ahc_search_qinfifo(ahc, target, channel, lun, tag,
				   CAM_REQUEUE_REQ, SEARCH_COMPLETE);

	/*
	 * Search waiting for selection list.
	 */
	{
		u_int8_t next, prev;

		next = ahc_inb(ahc, WAITING_SCBH);  /* Start at head of list. */
		prev = SCB_LIST_NULL;

		while (next != SCB_LIST_NULL) {
			u_int8_t scb_index;

			ahc_outb(ahc, SCBPTR, next);
			scb_index = ahc_inb(ahc, SCB_TAG);
			if (scb_index >= ahc->scb_data->numscbs) {
				panic("Waiting List inconsistency. "
				      "SCB index == %d, yet numscbs == %d.",
				      scb_index, ahc->scb_data->numscbs);
			}
			scbp = ahc->scb_data->scbarray[scb_index];
			if (ahc_match_scb(scbp, target, channel, lun, tag)) {

				next = ahc_abort_wscb(ahc, next, prev);
			} else {
				
				prev = next;
				next = ahc_inb(ahc, SCB_NEXT);
			}
		}
	}
	/*
	 * Go through the disconnected list and remove any entries we
	 * have queued for completion, 0'ing their control byte too.
	 */
	ahc_search_disc_list(ahc, target, channel, lun, tag);

	/*
	 * Go through the hardware SCB array looking for commands that
	 * were active but not on any list.
	 */
	for(i = 0; i < ahc->scb_data->maxhscbs; i++) {
		u_int8_t scbid;

		ahc_outb(ahc, SCBPTR, i);
		scbid = ahc_inb(ahc, SCB_TAG);
		if (scbid < ahc->scb_data->numscbs) {
			scbp = ahc->scb_data->scbarray[scbid];
			if (ahc_match_scb(scbp, target, channel, lun, tag)) {
				ahc_add_curscb_to_free_list(ahc);
                        }
		}
	}
	/*
	 * Go through the pending CCB list and look for
	 * commands for this target that are still active.
	 * These are other tagged commands that were
	 * disconnected when the reset occured.
	 */
	{
		struct ccb_hdr *ccb_h;


		ccb_h = ahc->pending_ccbs.lh_first;

		while (ccb_h != NULL) {
			scbp = (struct scb *)ccb_h->ccb_scb_ptr;
			ccb_h = ccb_h->sim_links.le.le_next;
			if (ahc_match_scb(scbp, target, channel, lun, tag)) {
				if (ahc_ccb_status(scbp->ccb) == CAM_REQ_INPROG)
					ahc_set_ccb_status(scbp->ccb, status);
				ahc_freeze_ccb(scbp->ccb);
				ahc_done(ahc, scbp);
				found++;
			}
		}
	}
	ahc_outb(ahc, SCBPTR, active_scb);
	return found;
}

static int
ahc_search_disc_list(struct ahc_softc *ahc, int target, char channel,
		     int lun, u_int tag)
{
	struct	scb *scbp;
	u_int	next;
	u_int	prev;
	u_int	count;
	u_int	active_scb;

	count = 0;
	next = ahc_inb(ahc, DISCONNECTED_SCBH);
	prev = SCB_LIST_NULL;

	/* restore this when we're done */
	active_scb = ahc_inb(ahc, SCBPTR);

	while (next != SCB_LIST_NULL) {
		u_int scb_index;

		ahc_outb(ahc, SCBPTR, next);
		scb_index = ahc_inb(ahc, SCB_TAG);
		if (scb_index >= ahc->scb_data->numscbs) {
			panic("Disconnected List inconsistency. "
			      "SCB index == %d, yet numscbs == %d.",
			      scb_index, ahc->scb_data->numscbs);
		}
		scbp = ahc->scb_data->scbarray[scb_index];
		if (ahc_match_scb(scbp, target, channel, lun, tag)) {
			next = ahc_rem_scb_from_disc_list(ahc, prev,
							  next);
			count++;
		} else {
			prev = next;
			next = ahc_inb(ahc, SCB_NEXT);
		}
	}
	ahc_outb(ahc, SCBPTR, active_scb);
	return (count);
}

static u_int
ahc_rem_scb_from_disc_list(struct ahc_softc *ahc, u_int prev, u_int scbptr)
{
	u_int next;

	ahc_outb(ahc, SCBPTR, scbptr);
	next = ahc_inb(ahc, SCB_NEXT);

	ahc_outb(ahc, SCB_CONTROL, 0);

	ahc_add_curscb_to_free_list(ahc);

	if (prev != SCB_LIST_NULL) {
		ahc_outb(ahc, SCBPTR, prev);
		ahc_outb(ahc, SCB_NEXT, next);
	} else
		ahc_outb(ahc, DISCONNECTED_SCBH, next);

	return next;
}

static void
ahc_add_curscb_to_free_list(struct ahc_softc *ahc)
{
	/* Invalidate the tag so that ahc_find_scb doesn't think it's active */
	ahc_outb(ahc, SCB_TAG, SCB_LIST_NULL);

	ahc_outb(ahc, SCB_NEXT, ahc_inb(ahc, FREE_SCBH));
	ahc_outb(ahc, FREE_SCBH, ahc_inb(ahc, SCBPTR));
}

/*
 * Manipulate the waiting for selection list and return the
 * scb that follows the one that we remove.
 */
static u_int
ahc_abort_wscb(struct ahc_softc *ahc, u_int scbpos, u_int prev)
{       
	u_int curscb, next;

	/*
	 * Select the SCB we want to abort and
	 * pull the next pointer out of it.
	 */
	curscb = ahc_inb(ahc, SCBPTR);
	ahc_outb(ahc, SCBPTR, scbpos);
	next = ahc_inb(ahc, SCB_NEXT);

	/* Clear the necessary fields */
	ahc_outb(ahc, SCB_CONTROL, 0);

	ahc_add_curscb_to_free_list(ahc);

	/* update the waiting list */
	if (prev == SCB_LIST_NULL) {
		/* First in the list */
		ahc_outb(ahc, WAITING_SCBH, next); 

		/*
		 * Ensure we aren't attempting to perform
		 * selection for this entry.
		 */
		ahc_outb(ahc, SCSISEQ, (ahc_inb(ahc, SCSISEQ) & ~ENSELO));
	} else {
		/*
		 * Select the scb that pointed to us 
		 * and update its next pointer.
		 */
		ahc_outb(ahc, SCBPTR, prev);
		ahc_outb(ahc, SCB_NEXT, next);
	}

	/*
	 * Point us back at the original scb position.
	 */
	ahc_outb(ahc, SCBPTR, curscb);
	return next;
}

static void
ahc_clear_intstat(struct ahc_softc *ahc)
{
	/* Clear any interrupt conditions this may have caused */
	ahc_outb(ahc, CLRSINT0, CLRSELDO|CLRSELDI|CLRSELINGO);
	ahc_outb(ahc, CLRSINT1, CLRSELTIMEO|CLRATNO|CLRSCSIRSTI
				|CLRBUSFREE|CLRSCSIPERR|CLRPHASECHG|
				CLRREQINIT);
	ahc_outb(ahc, CLRINT, CLRSCSIINT);
}

static void
ahc_reset_current_bus(struct ahc_softc *ahc)
{
	u_int8_t scsiseq;

	ahc_outb(ahc, SIMODE1, ahc_inb(ahc, SIMODE1) & ~ENSCSIRST);
	scsiseq = ahc_inb(ahc, SCSISEQ);
	ahc_outb(ahc, SCSISEQ, scsiseq | SCSIRSTO);
	DELAY(AHC_BUSRESET_DELAY);
	/* Turn off the bus reset */
	ahc_outb(ahc, SCSISEQ, scsiseq & ~SCSIRSTO);

	ahc_clear_intstat(ahc);

	/* Re-enable reset interrupts */
	ahc_outb(ahc, SIMODE1, ahc_inb(ahc, SIMODE1) | ENSCSIRST);
}

static int
ahc_reset_channel(struct ahc_softc *ahc, char channel, int initiate_reset)
{
	u_int	  target, max_target;
	int	  found;
	u_int8_t  sblkctl;
	char	  cur_channel;
	struct	  cam_path *path;

	pause_sequencer(ahc);
	/*
	 * Clean up all the state information for the
	 * pending transactions on this bus.
	 */
	found = ahc_abort_scbs(ahc, ALL_TARGETS, channel, ALL_LUNS,
			       SCB_LIST_NULL, CAM_SCSI_BUS_RESET);
	path = channel == 'B' ? ahc->path_b : ahc->path;

	/* Notify the XPT that a bus reset occurred */
	xpt_async(AC_BUS_RESET, path, NULL);

	/*
	 * Revert to async/narrow transfers until we renegotiate.
	 */
	max_target = (ahc->features & AHC_WIDE) ? 15 : 7;
	for (target = 0; target <= max_target; target++) {
		struct ahc_devinfo devinfo;

		ahc_compile_devinfo(&devinfo, target, channel);
		ahc_set_width(ahc, &devinfo, path, MSG_EXT_WDTR_BUS_8_BIT,
			      AHC_TRANS_CUR);
		ahc_set_syncrate(ahc, &devinfo, path, /*syncrate*/NULL,
				 /*period*/0, /*offset*/0, AHC_TRANS_CUR);
	}

	/*
	 * Reset the bus if we are initiating this reset and
	 * restart/unpause the sequencer
	 */
	sblkctl = ahc_inb(ahc, SBLKCTL);
	cur_channel = 'A';
	if ((ahc->features & AHC_TWIN) != 0
	 && ((sblkctl & SELBUSB) != 0))
	    cur_channel = 'B';
	if (cur_channel != channel) {
		/* Case 1: Command for another bus is active
		 * Stealthily reset the other bus without
		 * upsetting the current bus.
		 */
		ahc_outb(ahc, SBLKCTL, sblkctl ^ SELBUSB);
		ahc_outb(ahc, SIMODE1,
			 ahc_inb(ahc, SIMODE1) & ~(ENBUSFREE|ENREQINIT));
		ahc_outb(ahc, SCSISEQ,
			 ahc_inb(ahc, SCSISEQ) & (ENSELI|ENRSELI|ENAUTOATNP));
		if (initiate_reset)
			ahc_reset_current_bus(ahc);
		ahc_clear_intstat(ahc);
		ahc_outb(ahc, SBLKCTL, sblkctl);
		unpause_sequencer(ahc, /*unpause_always*/FALSE);
	} else {
		/* Case 2: A command from this bus is active or we're idle */
		ahc_outb(ahc, SIMODE1,
			 ahc_inb(ahc, SIMODE1) & ~(ENBUSFREE|ENREQINIT));
		ahc->flags &= ~AHC_HANDLING_REQINITS;
		ahc->msg_type = MSG_TYPE_NONE;
		ahc_outb(ahc, SCSISEQ,
			 ahc_inb(ahc, SCSISEQ) & (ENSELI|ENRSELI|ENAUTOATNP));
		if (initiate_reset)
			ahc_reset_current_bus(ahc);
		ahc_clear_intstat(ahc);
		restart_sequencer(ahc);
	}
	return found;
}

static int
ahc_match_scb (struct scb *scb, int target, char channel, int lun, u_int tag)
{
	int targ = SCB_TARGET(scb);
	char chan = SCB_CHANNEL(scb);
	int slun = SCB_LUN(scb);
	int match;

	match = ((chan == channel) || (channel == ALL_CHANNELS));
	if (match != 0)
		match = ((targ == target) || (target == ALL_TARGETS));
	if (match != 0)
		match = ((lun == slun) || (lun == ALL_LUNS));
	if (match != 0)
		match = ((tag == scb->hscb->tag) || (tag == SCB_LIST_NULL));

	return match;
}

static void
ahc_construct_sdtr(struct ahc_softc *ahc, u_int period, u_int offset)
{
	ahc->msg_buf[ahc->msg_index++] = MSG_EXTENDED;
	ahc->msg_buf[ahc->msg_index++] = MSG_EXT_SDTR_LEN;
	ahc->msg_buf[ahc->msg_index++] = MSG_EXT_SDTR;
	ahc->msg_buf[ahc->msg_index++] = period;
	ahc->msg_buf[ahc->msg_index++] = offset;
	ahc->msg_len += 5;
}

static void
ahc_construct_wdtr(struct ahc_softc *ahc, u_int bus_width)
{
	ahc->msg_buf[ahc->msg_index++] = MSG_EXTENDED;
	ahc->msg_buf[ahc->msg_index++] = MSG_EXT_WDTR_LEN;
	ahc->msg_buf[ahc->msg_index++] = MSG_EXT_WDTR;
	ahc->msg_buf[ahc->msg_index++] = bus_width;
	ahc->msg_len += 4;
}

static void
ahc_calc_residual(struct scb *scb)
{
	struct	hardware_scb *hscb;

	hscb = scb->hscb;

	/*
	 * If the disconnected flag is still set, this is bogus
	 * residual information left over from a sequencer
	 * pagin/pageout, so ignore this case.
	 */
	if ((scb->hscb->control & DISCONNECTED) == 0) {
		u_int32_t resid;
		int	  resid_sgs;
		int	  sg;
		
		/*
		 * Remainder of the SG where the transfer
		 * stopped.
		 */
		resid = (hscb->residual_data_count[2] << 16)
		      |	(hscb->residual_data_count[1] <<8)
		      |	(hscb->residual_data_count[0]);

		/*
		 * Add up the contents of all residual
		 * SG segments that are after the SG where
		 * the transfer stopped.
		 */
		resid_sgs = scb->hscb->residual_SG_count - 1/*current*/;
		sg = scb->sg_count - resid_sgs - 1/*first SG*/;
		while (resid_sgs > 0) {

			resid += scb->ahc_dma[sg].len;
			sg++;
			resid_sgs--;
		}
		if ((scb->flags & SCB_SENSE) == 0) {

			scb->ccb->csio.resid = resid;
		} else {

			scb->ccb->csio.sense_resid = resid;
		}
	}

	/*
	 * Clean out the residual information in this SCB for its
	 * next consumer.
	 */
	hscb->residual_data_count[0] = 0;
	hscb->residual_data_count[1] = 0;
	hscb->residual_data_count[2] = 0;
	hscb->residual_SG_count = 0;

#ifdef AHC_DEBUG
	if (ahc_debug & AHC_SHOWMISC) {
		sc_print_addr(xs->sc_link);
		printf("Handled Residual of %ld bytes\n" ,xs->resid);
	}
#endif
}

static void
ahc_update_pending_syncrates(struct ahc_softc *ahc)
{
	/*
	 * Traverse the pending SCB list and ensure that all of the
	 * SCBs there have the proper settings.
	 */
	struct	ccb_hdr *ccbh;
	int	pending_ccb_count;
	int	i;
	u_int	saved_scbptr;

	/*
	 * We were able to complete the command successfully,
	 * so reinstate the timeouts for all other pending
	 * commands.
	 */
	ccbh = LIST_FIRST(&ahc->pending_ccbs);
	pending_ccb_count = 0;
	while (ccbh != NULL) {
		struct scb *pending_scb;
		struct hardware_scb *pending_hscb;
		struct ahc_target_tinfo *tinfo;
		struct ahc_devinfo devinfo;

		pending_scb = (struct scb *)ccbh->ccb_scb_ptr;
		pending_hscb = pending_scb->hscb;
		ahc_compile_devinfo(&devinfo, SCB_TARGET(pending_scb),
				    SCB_CHANNEL(pending_scb));
		tinfo = &ahc->transinfo[devinfo.target_offset];
		pending_hscb->control &= ~ULTRAENB;
		if ((ahc->ultraenb & devinfo.target_mask) != 0)
			pending_hscb->control |= ULTRAENB;
		pending_hscb->scsirate = tinfo->scsirate;
		pending_hscb->scsioffset = tinfo->current.offset;
		pending_ccb_count++;
		ccbh = LIST_NEXT(ccbh, sim_links.le);
	}

	if (pending_ccb_count == 0)
		return;

	saved_scbptr = ahc_inb(ahc, SCBPTR);
	/* Ensure that the hscbs down on the card match the new information */
	for (i = 0; i < ahc->scb_data->maxhscbs; i++) {
		u_int scb_tag;

		ahc_outb(ahc, SCBPTR, i);
		scb_tag = ahc_inb(ahc, SCB_TAG);
		if (scb_tag != SCB_LIST_NULL) {
			struct	scb *pending_scb;
			struct	hardware_scb *pending_hscb;
			struct	ahc_target_tinfo *tinfo;
			struct	ahc_devinfo devinfo;
			u_int	control;

			pending_scb = ahc->scb_data->scbarray[scb_tag];
			pending_hscb = pending_scb->hscb;
			ahc_compile_devinfo(&devinfo, SCB_TARGET(pending_scb),
					    SCB_CHANNEL(pending_scb));
			tinfo = &ahc->transinfo[devinfo.target_offset];
			control = ahc_inb(ahc, SCB_CONTROL);
			control &= ~ULTRAENB;
			if ((ahc->ultraenb & devinfo.target_mask) != 0)
				control |= ULTRAENB;
			ahc_outb(ahc, SCB_CONTROL, control);
			ahc_outb(ahc, SCB_SCSIRATE, tinfo->scsirate);
			ahc_outb(ahc, SCB_SCSIOFFSET, tinfo->current.offset);
		}
	}
	ahc_outb(ahc, SCBPTR, saved_scbptr);
}

static void
ahc_dump_targcmd(struct target_cmd *cmd)
{
	u_int8_t *byte;
	u_int8_t *last_byte;
	int	  initiator;
	int	  target;
	int	  lun;
	int i;

	byte = &cmd->icl;
	/* Debugging info for received commands */
	last_byte = &cmd[1].icl;

	i = 0;
	while (byte < last_byte) {
		if (i == 0)
			printf("\t");
		printf("%#x", *byte++);
		i++;
		if (i == 8) {
			printf("\n");
			i = 0;
		} else {
			printf(", ");
		}
	}
}

static void
ahc_shutdown(int howto, void *arg)
{
	struct	ahc_softc *ahc;
	int	i;

	ahc = (struct ahc_softc *)arg;

	ahc_reset(ahc);
	ahc_outb(ahc, SCSISEQ, 0);
	ahc_outb(ahc, SXFRCTL0, 0);
	ahc_outb(ahc, DSPCISTATUS, 0);

	for (i = TARG_SCSIRATE; i < HA_274_BIOSCTRL; i++)
		ahc_outb(ahc, i, 0);
}
