/*
 * Generic driver for the aic7xxx based adaptec SCSI controllers
 * Copyright (c) 1994, 1995 Justin T. Gibbs.
 * All rights reserved.
 *
 * Product specific probe and attach routines can be found in:
 * i386/eisa/aic7770.c	27/284X and aic7770 motherboard controllers
 * pci/aic7870.c	3940, 2940, aic7870 and aic7850 controllers
 *
 * Portions of this driver are based on the FreeBSD 1742 Driver:
 *
 * Written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * commenced: Sun Sep 27 18:14:01 PDT 1992
 *
 *      $Id: aic7xxx.c,v 1.51 1996/01/03 06:32:10 gibbs Exp $
 */
/*
 * TODO:
 *	Implement Target Mode
 *
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

#define PAGESIZ 4096

#define MAX_TAGS 4;

#include <sys/kernel.h>
#define KVTOPHYS(x)   vtophys(x)

#define MIN(a,b) ((a < b) ? a : b)
#define ALL_TARGETS -1

struct ahc_data *ahcdata[NAHC];

u_long ahc_unit = 0;

static int     ahc_debug = AHC_SHOWABORTS|AHC_SHOWMISC;

/**** bit definitions for SCSIDEF ****/
#define	HSCSIID		0x07		/* our SCSI ID */
#define HWSCSIID	0x0f		/* our SCSI ID if Wide Bus */

typedef enum {
	list_head,
	list_second,
}insert_t;

static u_int32	ahc_adapter_info __P((int unit));
static void	ahcminphys __P((struct buf *bp));
static int32	ahc_scsi_cmd __P((struct scsi_xfer *xs));

static struct scsi_adapter ahc_switch =
{
        ahc_scsi_cmd,
        ahcminphys,
        0,
        0,
        ahc_adapter_info,
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
		 inb(SEQADDR1 + ahc->baseport != 0));			\
									\
	UNPAUSE_SEQUENCER(ahc);

static u_char	ahc_abort_wscb __P((struct ahc_data *ahc, struct scb *scbp,
				    u_char prev, u_long iobase,
				    u_char timedout_scb, u_int32 xs_error));
static void	ahc_add_waiting_scb __P((u_long iobase, struct scb *scb,
					 insert_t where));
static void	ahc_done __P((struct ahc_data *ahc, struct scb *scbp));
static void	ahc_free_scb __P((struct ahc_data *ahc, struct scb *scb,
				  int flags));
static void	ahc_getscb __P((u_long iobase, struct scb *scb));
static struct scb *
		ahc_get_scb __P((struct ahc_data *ahc, int flags));
static void	ahc_loadseq __P((u_long iobase));
static int	ahc_match_scb __P((struct scb *scb, int target, char channel));
#ifdef AHC_DEBUG
static void	ahc_print_active_scb __P((struct ahc_data *ahc));
static void	ahc_print_scb __P((struct scb *scb));
#endif
static int	ahc_reset_channel __P((struct ahc_data *ahc, char channel,
				       u_char timedout_scb, u_int32 xs_error));
static int	ahc_reset_device __P((struct ahc_data *ahc, int target,
				      char channel, u_char timedout_scb,
				      u_int32 xs_error));
static void	ahc_reset_current_bus __P((u_long iobase));
static void	ahc_scb_timeout __P((struct ahc_data *ahc, struct scb *scb));
static void	ahc_scsirate __P((struct ahc_data* ahc, u_char *scsirate,
				  int period, int offset, int target));
static void	ahc_send_scb __P((struct ahc_data *ahc, struct scb *scb));
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
	    ,scb->target_channel_lun
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

static void
ahc_print_active_scb(ahc)
        struct ahc_data *ahc;
{
	int cur_scb_offset;
	u_long iobase = ahc->baseport;
	PAUSE_SEQUENCER(ahc);
	cur_scb_offset = inb(SCBPTR + iobase);
	UNPAUSE_SEQUENCER(ahc);
	ahc_print_scb(ahc->scbarray[cur_scb_offset]);
}

#endif

#define         PARERR          0x08
#define         ILLOPCODE       0x04
#define         ILLSADDR        0x02
#define         ILLHADDR        0x01

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
	{ 0x140, 100, "10.0"  },
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
 * Sticking the ahc structure into the ahcdata array is an artifact of the
 * need to index by unit.  As soon as the upper level scsi code passes
 * pointers instead of units down to us, this will go away.
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

	if (unit >= NAHC) {
		printf("ahc: unit number (%d) too high\n", unit);
		return 0;
	}

	/*
	 * Allocate a storage area for us
	 */

	if (ahcdata[unit]) {
		printf("ahc%d: memory already allocated\n", unit);
		return NULL;
	}
	ahc = malloc(sizeof(struct ahc_data), M_TEMP, M_NOWAIT);
	if (!ahc) {
		printf("ahc%d: cannot malloc!\n", unit);
		return NULL;
	}
	bzero(ahc, sizeof(struct ahc_data));
        ahcdata[unit] = ahc;
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
	ahcdata[ahc->unit] = NULL;
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
	while (wait--) {
		DELAY(1000);
		if(!(inb(HCNTRL + iobase) & CHIPRST))
			break;
	}
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
ahc_scsirate(ahc, scsirate, period, offset, target )
	struct	ahc_data *ahc;
	u_char	*scsirate;
	short	period;
	u_char	offset;
	int	target;
{
	int i;

	for (i = 0; i < ahc_num_syncrates; i++) {

		if ((ahc_syncrates[i].period - period) >= 0) {
			/*
			 * Watch out for Ultra speeds when ultra is not
			 * enabled and vice-versa.
			 */
			if (ahc->type & AHC_ULTRA) {
				if (!(ahc_syncrates[i].sxfr & ULTRA_SXFR)) {
					printf("ahc%d: target %d requests "
					       "%sMHz transfers, but adapter "
					       "in Ultra mode can only sync at "
					       "10MHz or above\n", ahc->unit, 
					       target, ahc_syncrates[i].rate);
					break; /* Use Async */
				}
			}
			else {
				if (ahc_syncrates[i].sxfr & ULTRA_SXFR) {
					/*
					 * This should only happen if the
					 * drive is the first to negotiate
					 * and chooses a high rate.  We'll
					 * just move down the table util
					 * we hit a non ultra speed.
					 */
					continue;
				}
			}
			*scsirate = (ahc_syncrates[i].sxfr) | (offset & 0x0f);
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
	 * fill in the prototype scsi_link.
	 */
	ahc->sc_link.adapter_unit = ahc->unit;
	ahc->sc_link.adapter_targ = ahc->our_id;
	ahc->sc_link.adapter = &ahc_switch;
	ahc->sc_link.opennings = 2;
	ahc->sc_link.device = &ahc_dev;
	ahc->sc_link.flags = DEBUGLEVEL;
	ahc->sc_link.fordriver = 0;

	/*
	 * Prepare the scsibus_data area for the upperlevel
	 * scsi code.
	 */
	scbus = scsi_alloc_bus();
	if(!scbus) 
		return 0;
	scbus->adapter_link = &ahc->sc_link;
	if(ahc->type & AHC_WIDE)
		scbus->maxtarg = 15;
	
	/*
	 * ask the adapter what subunits are present
	 */
	if(bootverbose)
		printf("ahc%d: Probing channel A\n", ahc->unit);
	scsi_attachdevs(scbus);
	scbus = NULL;	/* Upper-level SCSI code owns this now */
	if(ahc->type & AHC_TWIN) {
		/* Configure the second scsi bus */
		ahc->sc_link_b = ahc->sc_link;
		ahc->sc_link_b.adapter_targ = ahc->our_id_b;
		ahc->sc_link_b.adapter_bus = 1;
		ahc->sc_link_b.fordriver = (void *)SELBUSB;
		scbus =  scsi_alloc_bus();
		if(!scbus) 
			return 0;
		scbus->adapter_link = &ahc->sc_link_b;
		if(ahc->type & AHC_WIDE)
			scbus->maxtarg = 15;
		if(bootverbose)
			printf("ahc%d: Probing Channel B\n", ahc->unit);
		scsi_attachdevs(scbus);
		scbus = NULL;	/* Upper-level SCSI code owns this now */
	}
	return 1;
}

static void
ahc_send_scb( ahc, scb )
        struct ahc_data *ahc;
        struct scb *scb;
{
        u_long iobase = ahc->baseport;

        PAUSE_SEQUENCER(ahc);
		outb(QINFIFO + iobase, scb->position);
        UNPAUSE_SEQUENCER(ahc);
}

static
void ahc_getscb(iobase, scb)
	u_long iobase;
	struct scb *scb;
{
        outb(SCBCNT + iobase, 0x80);     /* SCBAUTO */

	insb(SCBARRAY + iobase, scb, SCB_PIO_TRANSFER_SIZE);

        outb(SCBCNT + iobase, 0);
}

/*
 * Add this SCB to the "waiting for selection" list.
 */
static
void ahc_add_waiting_scb (iobase, scb, where)
	u_long iobase;
	struct scb *scb;
	insert_t where;
{
	u_char head, tail; 
	u_char curscb;

	curscb = inb(SCBPTR + iobase);
	head = inb(WAITING_SCBH + iobase);
	if(head == SCB_LIST_NULL) {
		/* List was empty */
		head = scb->position;
		tail = SCB_LIST_NULL;
	}
	else if (where == list_head) {
		outb(SCBPTR+iobase, scb->position);
		outb(SCB_NEXT_WAITING+iobase, head);
		head = scb->position;
	}
	else /*where == list_second*/ {
		u_char third_scb;
		outb(SCBPTR+iobase, head);
		third_scb = inb(SCB_NEXT_WAITING+iobase);
		outb(SCB_NEXT_WAITING+iobase,scb->position);
		outb(SCBPTR+iobase, scb->position);
		outb(SCB_NEXT_WAITING+iobase,third_scb);
	}
	outb(WAITING_SCBH + iobase, head);
	outb(SCBPTR + iobase, curscb);
}

/*
 * Catch an interrupt from the adaptor
 */
int
ahcintr(arg)
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
		return 0;

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
                        panic("ahc%d:%c:%d: unknown scsi bus phase.  "
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
					       "target (0x%x).  Rejecting\n", 
						ahc->unit, channel, target,
						rejbyte);
				break; 
			}
                    case NO_IDENT: 
                        panic("ahc%d:%c:%d: Target did not send an IDENTIFY "
			      "message. SAVED_TCL == 0x%x\n",
                              ahc->unit, channel, target,
			      inb(SAVED_TCL + iobase));
			break;
                    case NO_MATCH:
			{
				printf("ahc%d:%c:%d: no active SCB for "
				       "reconnecting target - "
				       "issuing ABORT\n", ahc->unit, channel, 
				       target);
				printf("SAVED_TCL == 0x%x\n",
					inb(SAVED_TCL + iobase));
				ahc_unbusy_target(target, channel, iobase);
				outb(SCBARRAY + iobase, NEEDDMA);
				outb(CLRSINT1 + iobase, CLRSELTIMEO);
	                        RESTART_SEQUENCER(ahc);
                        	break;
			}
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
					     MIN(offset,maxoffset),
					    target);
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
							/* Negotiate 16_BITS */
							bus_width = BUS_16_BIT;
						case BUS_16_BIT:
						    if(bootverbose)
		        				printf("ahc%d: target "
							       "%d using 16Bit "
							       "transfers\n",
								ahc->unit,
								target);
						    scratch |= 0x80;	
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
		    case MSG_REJECT:
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

  			  scb_index = inb(SCBPTR + iobase);
                          scb = ahc->scbarray[scb_index];

			  /*
			   * Set the default return value to 0 (don't
			   * send sense).  The sense code will change
			   * this if needed and this reduces code
			   * duplication.
			   */
			  outb(RETURN_1 + iobase, 0);
		 	  if (!scb || !(scb->flags & SCB_ACTIVE)) {
                              printf("ahc%d:%c:%d: ahcintr - referenced scb "
				     "not valid during seqint 0x%x scb(%d)\n", 
				     ahc->unit, channel, target, intstat,
				     scb_index);
			      goto clear;
			  }

			  xs = scb->xs;

			  ahc_getscb(iobase, scb);

#ifdef AHC_DEBUG
			  if((ahc_debug & AHC_SHOWSCBS)
			    && xs->sc_link->target == DEBUGTARG)
				ahc_print_scb(scb);
#endif
			  xs->status = scb->target_status;
			  switch(scb->target_status){
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
					u_char control = scb->control;
					u_short active;
					struct ahc_dma_seg *sg = scb->ahc_dma;
					struct scsi_sense *sc = &(scb->sense_cmd);
					u_char tcl = scb->target_channel_lun;
#ifdef AHC_DEBUG
					if(ahc_debug & AHC_SHOWSENSE)
					{
						sc_print_addr(xs->sc_link);
						printf("Sending Sense\n");
					}
#endif
					bzero(scb, SCB_PIO_TRANSFER_SIZE);
					scb->control |= control & DISCENB;
					scb->flags |= SCB_SENSE;
					sc->op_code = REQUEST_SENSE;
					sc->byte2 =  xs->sc_link->lun << 5;
					sc->length = sizeof(struct scsi_sense_data);
					sc->control = 0;

					sg->addr = KVTOPHYS(&xs->sense);
					sg->len = sizeof(struct scsi_sense_data);

					scb->target_channel_lun = tcl;
					scb->SG_segment_count = 1;
					scb->SG_list_pointer = KVTOPHYS(sg);
					scb->cmdpointer = KVTOPHYS(sc);
					scb->cmdlen = sizeof(*sc);

			                scb->data = sg->addr; 
					scb->datalen = sg->len;
					outb(SCBCNT + iobase, 0x80);
					outsb(SCBARRAY+iobase,scb,SCB_PIO_TRANSFER_SIZE);
					outb(SCBCNT + iobase, 0);
					outb(SCB_NEXT_WAITING+iobase,SCB_LIST_NULL);
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
					ahc_add_waiting_scb(iobase, scb, 
							    list_head);
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
				xs->error = XS_BUSY;
				break;
			    default:
				sc_print_addr(xs->sc_link);
				printf("unexpected targ_status: %x\n",
					scb->target_status);
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
			break;
		  }
		  case RESIDUAL:
		  {
			int   scb_index;
			scb_index = inb(SCBPTR + iobase);
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
				scb->xs->resid =
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
					scb->xs->resid += scb->ahc_dma[sg].len;
					resid_sgs--;
				}

				xs->flags |= SCSI_RESID_VALID;
#ifdef AHC_DEBUG
				if(ahc_debug & AHC_SHOWMISC) {
					sc_print_addr(xs->sc_link);
					printf("Handled Residual of %ld bytes\n"
						,scb->xs->resid);
				}
#endif
			}
			break;
		  }
		  case ABORT_TAG:
		  {
                        int   scb_index;
			scb_index = inb(SCBPTR + iobase);
			scb = ahc->scbarray[scb_index];
			xs = scb->xs;
			/*
			 * We didn't recieve a valid tag back from
			 * the target on a reconnect.
			 */
			sc_print_addr(xs->sc_link);
			printf("invalid tag recieved -- sending ABORT_TAG\n");
			scb->xs->error = XS_DRIVER_STUFFUP;
			untimeout(ahc_timeout, (caddr_t)scb);
			ahc_done(ahc, scb);
			break;
		  }
		  case AWAITING_MSG:
		  {
			int   scb_index;
			scb_index = inb(SCBPTR + iobase);
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
			}
			else
				panic("ahcintr: AWAITING_MSG for an SCB that"
					"does not have a waiting message");
			break;
		  }
		  case IMMEDDONE:
		  {
			/*
			 * Take care of device reset messages
			 */
			u_char scbindex = inb(SCBPTR + iobase);
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
#ifdef AHC_DEBUG
				if(ahc_debug & AHC_SHOWABORTS) {
					sc_print_addr(scb->xs->sc_link);
					printf("Bus Device Reset delivered. "
						"%d SCBs aborted\n", found);
				}
#endif
			}
			else
				panic("ahcintr: Immediate complete for "
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

                int scb_index = inb(SCBPTR + iobase);
                status = inb(SSTAT1 + iobase);

                scb = ahc->scbarray[scb_index];
                if (!scb || !(scb->flags & SCB_ACTIVE)) {
			printf("ahc%d: ahcintr - referenced scb not "
			       "valid during scsiint 0x%x scb(%d)\n",
				ahc->unit, status, scb_index);
                        outb(CLRSINT1 + iobase, status);
                        UNPAUSE_SEQUENCER(ahc);
                        outb(CLRINT + iobase, CLRSCSIINT);
			scb = NULL;
			goto cmdcomplete;
                }
		xs = scb->xs;

		if (status & SELTO) {
			u_char waiting;
			u_char flags;
                        outb(SCSISEQ + iobase, ENRSELI);
                        xs->error = XS_TIMEOUT;
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

			outb(SCBARRAY + iobase, NEEDDMA);

                        outb(CLRSINT1 + iobase, CLRSELTIMEO);

                        outb(CLRINT + iobase, CLRSCSIINT);

			/* Shift the waiting for selection queue forward */
			waiting = inb(WAITING_SCBH + iobase);
			outb(SCBPTR + iobase, waiting);
			waiting = inb(SCB_NEXT_WAITING + iobase);
			outb(WAITING_SCBH + iobase, waiting);

                        RESTART_SEQUENCER(ahc);
                }       
                        
                else if (status & SCSIPERR) { 
			/*
			 * Determine the bus phase and
			 * queue an appropriate message
			 */
			char	*phase;
			u_char	mesg_out = MSG_NOP;
			u_char	sigstate = inb(SIGSTATE + iobase);

			sc_print_addr(xs->sc_link);

			switch(sigstate) {
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
				scb->xs->error = XS_DRIVER_STUFFUP;

                        outb(CLRSINT1 + iobase, CLRSCSIPERR);
                        UNPAUSE_SEQUENCER(ahc);

                        outb(CLRINT + iobase, CLRSCSIINT);
			scb = NULL;	/* Don't ahc_done the scb */
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
cmdcomplete:
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

                } while (inb(QOUTCNT + iobase));
        }
	return 1;
}

void
ahc_eisa_intr(arg)
	void *arg;
{
	ahcintr(arg);
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
				(scb->target_channel_lun & 0x08));
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
#ifdef QUEUE_FULL_SUPPORTED
			xs->sc_link->opennings += 2;
#endif
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
	int     max_targ = 15;
	/*
	 * Assume we have a board at this stage and it has been reset.
	 */

        /* Determine channel configuration and who we are on the scsi bus. */
        switch ( (sblkctl = inb(SBLKCTL + iobase) & 0x0a) ) {
            case 0:
		ahc->our_id = (inb(SCSICONF + iobase) & HSCSIID);
		if(ahc->type == AHC_394)
			printf("Channel %c, SCSI Id=%d, ", 
				ahc->flags & AHC_CHNLB ? 'B' : 'A',
				ahc->our_id);
		else
			printf("Single Channel, SCSI Id=%d, ", ahc->our_id);
		outb(FLAGS + iobase, SINGLE_BUS);
                break;
            case 2:
		ahc->our_id = (inb(SCSICONF + 1 + iobase) & HWSCSIID);
		if(ahc->type == AHC_394)
			printf("Wide Channel %c, SCSI Id=%d, ", 
				ahc->flags & AHC_CHNLB ? 'B' : 'A',
				ahc->our_id);
		else
			printf("Wide Channel, SCSI Id=%d, ", ahc->our_id);
		ahc->type |= AHC_WIDE;
		outb(FLAGS + iobase, WIDE_BUS);
                break;
            case 8:
		ahc->our_id = (inb(SCSICONF + iobase) & HSCSIID);
		ahc->our_id_b = (inb(SCSICONF + 1 + iobase) & HSCSIID);
                printf("Twin Channel, A SCSI Id=%d, B SCSI Id=%d, ",
			ahc->our_id, ahc->our_id_b);
		ahc->type |= AHC_TWIN;
		outb(FLAGS + iobase, TWIN_BUS);
                break;
            default:
                printf(" Unsupported adapter type.  Ignoring\n");
                return(-1);
        }

	printf("%d SCBs\n", ahc->maxscbs);

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
		scsi_conf = inb(SCSICONF + 1 + iobase) & (ENSPCHK|STIMESEL);
		outb(SXFRCTL1 + iobase, scsi_conf|ENSTIMER|ACTNEGEN|STPWEN);
		outb(SIMODE1 + iobase, ENSELTIMO|ENSCSIPERR);
		if(ahc->type & AHC_ULTRA)
			outb(SXFRCTL0 + iobase, DFON|SPIOEN|ULTRAEN);
		else
			outb(SXFRCTL0 + iobase, DFON|SPIOEN);

		/* Reset the bus */
		outb(SCSISEQ + iobase, SCSIRSTO);
		DELAY(1000);
		outb(SCSISEQ + iobase, 0);

		/* Select Channel A */
		outb(SBLKCTL + iobase, 0);
	}
	outb(SCSIID + iobase, ahc->our_id);
	scsi_conf = inb(SCSICONF + iobase) & (ENSPCHK|STIMESEL);
	outb(SXFRCTL1 + iobase, scsi_conf|ENSTIMER|ACTNEGEN|STPWEN);
	outb(SIMODE1 + iobase, ENSELTIMO|ENSCSIPERR);
	if(ahc->type & AHC_ULTRA)
		outb(SXFRCTL0 + iobase, DFON|SPIOEN|ULTRAEN);
	else
		outb(SXFRCTL0 + iobase, DFON|SPIOEN);

	/* Reset the bus */
	outb(SCSISEQ + iobase, SCSIRSTO);
	DELAY(1000);
	outb(SCSISEQ + iobase, 0);

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

#ifdef AHC_DEBUG
	/* How did we do? */
	if(ahc_debug & AHC_SHOWMISC)
		printf("NEEDSDTR == 0x%x\nNEEDWDTR == 0x%x\n"
			"DISCENABLE == 0x%x\n", ahc->needsdtr, 
			ahc->needwdtr, ahc->discenable);
#endif
	/*
	 * Clear the control byte for every SCB so that the sequencer
	 * doesn't get confused and think that one of them is valid
	 */
	for(i = 0; i < ahc->maxscbs; i++) {
		outb(SCBPTR + iobase, i);
		outb(SCBARRAY + iobase, 0);
	}

	/*
	 * Set the number of availible SCBs
	 */
	outb(SCBCOUNT + iobase, ahc->maxscbs);

	/*
	 * 2s compliment of SCBCOUNT
	 */
	i = ahc->maxscbs;
	outb(COMP_SCBCOUNT + iobase, -i & 0xff);

	/* We don't have any busy targets right now */
	outb( ACTIVE_A + iobase, 0 );
	outb( ACTIVE_B + iobase, 0 );

	/* We don't have any waiting selections */
	outb( WAITING_SCBH + iobase, SCB_LIST_NULL );
	outb( WAITING_SCBT + iobase, SCB_LIST_NULL );

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
	ahc->flags = AHC_INIT;
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
        if (bp->b_bcount > ((AHC_NSEG - 1) * PAGESIZ)) {
                bp->b_bcount = ((AHC_NSEG - 1) * PAGESIZ);
        }
}

/*
 * start a scsi operation given the command and
 * the data address, target, and lun all of which
 * are stored in the scsi_xfer struct
 */
static int32
ahc_scsi_cmd(xs)
        struct scsi_xfer *xs;
{
        struct scb *scb = NULL;
        struct ahc_dma_seg *sg;
        int     seg;            /* scatter gather seg being worked on */
        int     thiskv;
        physaddr thisphys, nextphys;
        int     unit = xs->sc_link->adapter_unit;
	u_short	mask = (0x01 << (xs->sc_link->target
				| ((u_long)xs->sc_link->fordriver & 0x08)));
        int     bytes_this_seg, bytes_this_page, datalen, flags;
        struct ahc_data *ahc = ahcdata[unit];
        int     s;

        SC_DEBUG(xs->sc_link, SDEV_DB2, ("ahc_scsi_cmd\n"));
        /*
         * get an scb to use. If the transfer
         * is from a buf (possibly from interrupt time)
         * then we can't allow it to sleep
         */
        flags = xs->flags;
        if (flags & ITSDONE) {
                printf("ahc%d: Already done?", unit);
                xs->flags &= ~ITSDONE;
        }
        if (!(flags & INUSE)) {
                printf("ahc%d: Not in use?", unit);
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

	if(ahc->tagenable & mask)
		scb->control |= TAG_ENB;
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
	scb->target_channel_lun = ((xs->sc_link->target << 4) & 0xF0) |
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
                {
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

                                SC_DEBUGN(xs->sc_link, SDEV_DB4, ("0x%lx",
					thisphys));

                                /* do it at least once */
                                nextphys = thisphys;
                                while ((datalen) && (thisphys == nextphys)) {
					/*
					 * This page is contiguous (physically)
					 * with the the last, just extend the
					 * length
					 */
                                        /* how far to the end of the page */
                                        nextphys = (thisphys & (~(PAGESIZ - 1)))
                                            + PAGESIZ;
                                        bytes_this_page = nextphys - thisphys;
                                        /**** or the data ****/
                                        bytes_this_page = min(bytes_this_page
                                            ,datalen);
                                        bytes_this_seg += bytes_this_page;
                                        datalen -= bytes_this_page;

                                        /* get more ready for the next page */
                                        thiskv = (thiskv & (~(PAGESIZ - 1)))
                                            + PAGESIZ;
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
                } /*end of iov/kv decision */
                scb->SG_segment_count = seg;

		/* Copy the first SG into the data pointer area */
		scb->data = scb->ahc_dma->addr;
		scb->datalen = scb->ahc_dma->len;
                SC_DEBUGN(xs->sc_link, SDEV_DB4, ("\n"));
                if (datalen) { 
			/* there's still data, must have run out of segs! */
                        printf("ahc_scsi_cmd%d: more than %d DMA segs\n",
                            unit, AHC_NSEG);
                        xs->error = XS_DRIVER_STUFFUP;
                        ahc_free_scb(ahc, scb, flags);
                        return (HAD_ERROR);
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

        /*
         * Usually return SUCCESSFULLY QUEUED
         */
#ifdef AHC_DEBUG
        if((ahc_debug & AHC_SHOWSCBS) && (xs->sc_link->target == DEBUGTARG))
		ahc_print_scb(scb);
#endif
	s = splbio();
	ahc_send_scb(ahc, scb);
        timeout(ahc_timeout, (caddr_t)scb, (xs->timeout * hz) / 1000);
	splx(s);
	SC_DEBUG(xs->sc_link, SDEV_DB3, ("cmd_sent\n"));
	return (SUCCESSFULLY_QUEUED);
}


/*
 * Return some information to the caller about
 * the adapter and it's capabilities.
 */
static u_int32
ahc_adapter_info(unit)
        int     unit;
{
        return (2);         /* 2 outstanding requests at a time per device */
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
        unsigned int opri;

	opri = splbio();

        scb->flags = SCB_FREE;
        scb->next = ahc->free_scb;
        ahc->free_scb = scb;
#ifdef AHC_DEBUG
	ahc->activescbs--;
#endif
        /*
         * If there were none, wake abybody waiting for
         * one to come free, starting with queued entries
         */
        if (!scb->next) {
                wakeup((caddr_t)&ahc->free_scb);
        }
	splx(opri);
}

/*
 * Get a free scb
 * If there are none, see if we can allocate a
 * new one.  Otherwise either return an error or sleep
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
        while (!(scbp = ahc->free_scb)) {
                if (ahc->numscbs < ahc->maxscbs) {
                        scbp = (struct scb *) malloc(sizeof(struct scb),
                                M_TEMP, M_NOWAIT);
                        if (scbp) {
				physaddr scbaddr = KVTOPHYS(scbp);
				u_long iobase = ahc->baseport;
				u_char curscb;
				bzero(scbp, sizeof(struct scb));
				scbp->position = ahc->numscbs;
				ahc->numscbs++;
				scbp->flags = SCB_ACTIVE;
				/*
				 * Place in the scbarray
				 * Never is removed.  Position
				 * in ahc->scbarray is the scbarray
				 * position on the board we will
				 * load it into.
				 */
				ahc->scbarray[scbp->position] = scbp;

				/*
				 * Initialize the host memory location
				 * of this SCB down on the board and
				 * flag that it should be DMA's before
				 * reference.  Also set its psuedo
				 * next pointer (for use in the psuedo
				 * list of SCBs waiting for selection)
				 * to SCB_LIST_NULL.
				 */
				scbp->control = NEEDDMA;
				scbp->host_scb = scbaddr;
				scbp->next_waiting = SCB_LIST_NULL;
				PAUSE_SEQUENCER(ahc);
				curscb = inb(SCBPTR + iobase);
				outb(SCBPTR + iobase, scbp->position);
				outb(SCBCNT + iobase, 0x80);
				outsb(SCBARRAY+iobase,scbp,SCB_HARDWARE_SIZE);
				outb(SCBCNT + iobase, 0);
				outb(SCBPTR + iobase, curscb);
				UNPAUSE_SEQUENCER(ahc);
				scbp->control = 0;
                        } else {
                                printf("ahc%d: Can't malloc SCB\n", ahc->unit);
                        }
			break;
                } else {
                        if (!(flags & SCSI_NOSLEEP)) {
                                tsleep((caddr_t)&ahc->free_scb, PRIBIO,
                                    "ahcscb", 0);
				continue;
                        }
			break;
                }
        }

	if (scbp) {
                /* Get SCB from from free list */
                ahc->free_scb = scbp->next;
		scbp->control = 0;
                scbp->flags = SCB_ACTIVE;
#ifdef AHC_DEBUG
		ahc->activescbs++;
		if((ahc_debug & AHC_SHOWSCBCNT)
		  && (ahc->activescbs == ahc->maxscbs))
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
		 inb(SEQADDR1 + iobase != 0));
}

static void
ahc_scb_timeout(ahc, scb)
        struct ahc_data *ahc;
        struct scb *scb;
{
	u_long iobase = ahc->baseport;
	int found = 0;
	char channel = scb->target_channel_lun & SELBUSB ? 'B': 'A';

	/*
	 * Ensure that the card doesn't do anything
	 * behind our back.
	 */
	PAUSE_SEQUENCER(ahc);

	/*
	 * First, determine if we want to do a bus
	 * reset or simply a bus device reset.
	 * If this is the first time that a transaction
	 * has timed out, just schedule a bus device
	 * reset.  Otherwise, we reset the bus and
	 * abort all pending I/Os on that bus.
	 */
	if(scb->flags & SCB_ABORTED)
	{
		/*
		 * Been down this road before.
		 * Do a full bus reset.
		 */
		found = ahc_reset_channel(ahc, channel, scb->position,
					  XS_TIMEOUT);
#ifdef AHC_DEBUG
		if(ahc_debug & AHC_SHOWABORTS) 
			printf("ahc%d: Issued Channel %c Bus Reset #1. "
				"%d SCBs aborted\n", ahc->unit, channel, found);
#endif
	}
	else {
		/*
		 * Send a Bus Device Reset Message:
		 * The target we select to send the message to may
		 * be entirely different than the target pointed to
		 * by the scb that timed out.  If the command is
		 * in the QINFIFO or the waiting for selection list,
		 * its not tying up the bus and isn't responsible
		 * for the delay so we pick off the active command
		 * which should be the SCB selected by SCBPTR.  If
		 * its disconnected or active, we device reset the
		 * target scbp points to.  Although it may be that
		 * this target is not responsible for the delay, it
		 * may also be that we're timing out on a command that
		 * just takes too much time, so we try the bus device
		 * reset there first.
		 */
		u_char active_scb, control;
		struct scb *active_scbp;
		active_scb = inb(SCBPTR + iobase);
		active_scbp = ahc->scbarray[active_scb];
		control = inb(SCBARRAY + iobase);

		/* Test to see if scbp is disconnected */
		outb(SCBPTR + iobase, scb->position);
		if(inb(SCB_CONTROL + iobase) & DISCONNECTED) {
			scb->flags |= SCB_DEVICE_RESET|SCB_ABORTED;
			scb->SG_segment_count = 0;
			scb->SG_list_pointer = 0;
			scb->data = 0;
			scb->datalen = 0;
			outb(SCBCNT + iobase, 0x80);
			outsb(SCBARRAY+iobase,scb,SCB_PIO_TRANSFER_SIZE);
			outb(SCBCNT + iobase, 0);
			ahc_add_waiting_scb(iobase, scb, list_second);
			timeout(ahc_timeout, (caddr_t)scb, (2 * hz));
#ifdef AHC_DEBUG
			if(ahc_debug & AHC_SHOWABORTS) {
				sc_print_addr(scb->xs->sc_link);
				printf("BUS DEVICE RESET message queued.\n");
			}
#endif
			UNPAUSE_SEQUENCER(ahc);
		}
		/* Is the active SCB really active? */
		else if((active_scbp->flags & SCB_ACTIVE) 
		  && (control & NEEDDMA) == NEEDDMA) {
		    u_char msg_len = inb(MSG_LEN + iobase);
		    if(msg_len) {
			/*
			 * If we're in a message phase, tacking on 
			 * another message may confuse the target totally.
			 * The bus is probably wedged, so reset the
			 * channel.
			 */
			channel = (active_scbp->target_channel_lun & SELBUSB)
					? 'B': 'A';	
			ahc_reset_channel(ahc, channel, scb->position, 
					  XS_TIMEOUT);
#ifdef AHC_DEBUG
			if(ahc_debug & AHC_SHOWABORTS) 
				printf("ahc%d: Issued Channel %c Bus Reset #2. "
					"%d SCBs aborted\n", ahc->unit, channel,
					found);
#endif
		    }
		    else {
			/* 
			 * Load the message buffer and assert attention.
			 */
			active_scbp->flags |= SCB_DEVICE_RESET|SCB_ABORTED;
			if(active_scbp != scb)
				untimeout(ahc_timeout, (caddr_t)active_scbp);
			timeout(ahc_timeout, (caddr_t)active_scbp, (2 * hz));
			outb(MSG_LEN + iobase, 1);
			outb(MSG0 + iobase, MSG_BUS_DEVICE_RESET);
			if(active_scbp->target_channel_lun 
			   != scb->target_channel_lun) {
				/* Give scb a new lease on life */
				timeout(ahc_timeout, (caddr_t)scb, 
					(scb->xs->timeout * hz) / 1000);
			}
#ifdef AHC_DEBUG
			if(ahc_debug & AHC_SHOWABORTS) {
				sc_print_addr(active_scbp->xs->sc_link);
				printf("BUS DEVICE RESET message queued.\n");
			}
#endif
			UNPAUSE_SEQUENCER(ahc);
		    }
		}
		else {
			/*
			 * No active command to single out, so reset
			 * the bus for the timed out target.
			 */
			ahc_reset_channel(ahc, channel, scb->position,
					  XS_TIMEOUT);
#ifdef AHC_DEBUG
			if(ahc_debug & AHC_SHOWABORTS) 
				printf("ahc%d: Issued Channel %c Bus Reset #3. "
					"%d SCBs aborted\n", ahc->unit, channel,
					found);
#endif
		}
	}
}

static void
ahc_timeout(void *arg1)
{
	struct	scb *scb = (struct scb *)arg1;
	int	unit;
	struct	ahc_data *ahc;
	int	s;

	s = splhigh();

	if (!(scb->flags & SCB_ACTIVE)) {
		/* Previous timeout took care of me already */
		splx(s);
		return;
	}

	unit = scb->xs->sc_link->adapter_unit;
	ahc = ahcdata[unit];
	printf("ahc%d: target %d, lun %d (%s%d) timed out\n", unit
		,scb->xs->sc_link->target
		,scb->xs->sc_link->lun
		,scb->xs->sc_link->device->name
		,scb->xs->sc_link->dev_unit);
#ifdef SCSIDEBUG
	show_scsi_cmd(scb->xs);
#endif
#ifdef  AHC_DEBUG
	if (ahc_debug & AHC_SHOWSCBS)
		ahc_print_active_scb(ahc);
#endif /*AHC_DEBUG */

	/*
	 * If it's immediate, don't try to abort it
	 */
	if (scb->flags & SCB_IMMED) {
		scb->xs->retries = 0;   /* I MEAN IT ! */
		ahc_done(ahc, scb);
	}
	else {
		/* abort the operation that has timed out */
		ahc_scb_timeout(ahc, scb);
	}
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
	u_int32 xs_error;
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
		int saved_queue[AHC_SCB_MAX];
		int queued = inb(QINCNT + iobase);

		for (i = 0; i < (queued - found); i++) {
			saved_queue[i] = inb(QINFIFO + iobase);
			scbp = ahc->scbarray[saved_queue[i]];
			if (ahc_match_scb (scbp, target, channel)){
				/*
				 * We found an scb that needs to be aborted.
				 */
				scbp->flags |= SCB_ABORTED;
				scbp->xs->error |= xs_error;
				if(scbp->position != timedout_scb)
					untimeout(ahc_timeout, (caddr_t)scbp);
				ahc_done (ahc, scbp);
				outb(SCBPTR + iobase, scbp->position);
				outb(SCBARRAY + iobase, NEEDDMA);
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
			scbp = ahc->scbarray[next];
			/*
			 * Select the SCB.
			 */
			if (ahc_match_scb(scbp, target, channel)) {
				next = ahc_abort_wscb(ahc, scbp, prev,
						iobase, timedout_scb, xs_error);
				found++;
			}
			else {
				outb(SCBPTR + iobase, scbp->position);
				prev = next;
				next = inb(SCB_NEXT_WAITING + iobase);
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
			outb(SCBPTR + iobase, scbp->position);
			outb(SCBARRAY + iobase, NEEDDMA);
			scbp->flags |= SCB_ABORTED;
			scbp->xs->error |= xs_error;
			if(scbp->position != timedout_scb)
				untimeout(ahc_timeout, (caddr_t)scbp);
			ahc_done (ahc, scbp);
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
	u_int32 xs_error;
{       
	u_char curscbp, next;
	int target = ((scbp->target_channel_lun >> 4) & 0x0f);
	char channel = (scbp->target_channel_lun & SELBUSB) ? 'B' : 'A';
	/*
	 * Select the SCB we want to abort and
	 * pull the next pointer out of it.
	 */
	curscbp = inb(SCBPTR + iobase);
	outb(SCBPTR + iobase, scbp->position);
	next = inb(SCB_NEXT_WAITING + iobase);

	/* Clear the necessary fields */
	outb(SCBARRAY + iobase, NEEDDMA);
	outb(SCB_NEXT_WAITING + iobase, SCB_LIST_NULL);
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
		outb(SCB_NEXT_WAITING + iobase, next);
	}
	/* Update the tale pointer */
	if(inb(WAITING_SCBT + iobase) == scbp->position)
		outb(WAITING_SCBT + iobase, prev);

	/*
	 * Point us back at the original scb position
	 * and inform the SCSI system that the command
	 * has been aborted.
	 */
	outb(SCBPTR + iobase, curscbp);
	scbp->flags |= SCB_ABORTED;
	scbp->xs->error |= xs_error;
	if(scbp->position != timedout_scb)
		untimeout(ahc_timeout, (caddr_t)scbp);
	ahc_done (ahc, scbp);
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
ahc_reset_channel(ahc, channel, timedout_scb, xs_error)
	struct ahc_data *ahc;
	char   channel;
	u_char timedout_scb;
	u_int32 xs_error;
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
	 * Reset the bus and unpause/restart the controller
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
		ahc_reset_current_bus(iobase);
		outb(SBLKCTL + iobase, sblkctl);
		UNPAUSE_SEQUENCER(ahc);
	}
	/* Case 2: A command from this bus is active or we're idle */ 
	else {
		ahc_reset_current_bus(iobase);
		RESTART_SEQUENCER(ahc);
	}
	return found;
}

static int
ahc_match_scb (scb, target, channel)
        struct scb *scb;
        int target;
	char channel;
{
	int targ = (scb->target_channel_lun >> 4) & 0x0f;
	char chan = (scb->target_channel_lun & SELBUSB) ? 'B' : 'A';

	if (target == ALL_TARGETS) 
		return (chan == channel);
	else
		return ((chan == channel) && (targ == target));
}
