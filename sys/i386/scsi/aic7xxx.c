/*
 * Generic driver for the aic7xxx based adaptec SCSI controllers
 * Copyright (c) 1994, 1995 Justin T. Gibbs.  
 * All rights reserved.
 *
 * Product specific probe and attach routines can be found in:
 * i386/isa/aic7770.c	27/284X and aic7770 motherboard controllers
 * /pci/aic7870.c	294x and aic7870 motherboard controllers
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
 *      $Id: aic7xxx.c,v 1.18 1995/03/31 13:54:40 gibbs Exp $
 */
/*
 * TODO:
 * 	Add target reset capabilities
 *	Implement Target Mode
 *
 *	This driver is very stable, and seems to offer performance
 *	comprable to the 1742 FreeBSD driver.  I have not experienced
 *	any timeouts since the timeout code was written, so in that 
 *	sense, it is untested.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <machine/clock.h>
#include <i386/scsi/aic7xxx.h>

#define PAGESIZ 4096  

#define MAX_TAGS 4;

#include <sys/kernel.h>
#define KVTOPHYS(x)   vtophys(x)

struct ahc_data *ahcdata[NAHC];

int     ahc_init __P((int unit));
void    ahc_loadseq __P((u_long iobase));     
int32   ahc_scsi_cmd();
timeout_t ahc_timeout;
void    ahc_done();
struct  scb *ahc_get_scb __P((int unit, int flags));
void    ahc_free_scb();
void	ahc_abort_scb __P((int unit, struct ahc_data *ahc, struct scb *scb));
void    ahcminphys();
struct  scb *ahc_scb_phys_kv();
int	ahc_poll __P((int unit, int wait));
u_int32 ahc_adapter_info();

int  ahc_unit = 0;

/* Different debugging levels */
#define AHC_SHOWMISC 0x0001
#define AHC_SHOWCMDS 0x0002
#define AHC_SHOWSCBS 0x0004
/*#define AHC_DEBUG*/
int     ahc_debug = AHC_SHOWMISC;

/**** bit definitions for SCSIDEF ****/
#define	HSCSIID		0x07		/* our SCSI ID */
#define HWSCSIID	0x0f		/* our SCSI ID if Wide Bus */

struct scsi_adapter ahc_switch =
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
struct scsi_device ahc_dev =
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
 * All of these should be in a separate header file shared by the sequencer
 * code and the kernel level driver.  The only catch is that we would need to 
 * add an additional 0xc00 offset when using them in the kernel driver.  The 
 * aic7770 assembler must be modified to allow include files as well.  All 
 * page numbers refer to the Adaptec AIC-7770 Data Book availible from 
 * Adaptec's Technical Documents Department 1-800-934-2766
 */

/* -------------------- AIC-7770 offset definitions ----------------------- */

/* 
 * SCSI Sequence Control (p. 3-11).  
 * Each bit, when set starts a specific SCSI sequence on the bus
 */
#define SCSISEQ			0xc00ul
#define		TEMODEO		0x80
#define		ENSELO		0x40
#define		ENSELI		0x20
#define		ENRSELI		0x10
#define		ENAUTOATNO	0x08
#define		ENAUTOATNI	0x04
#define		ENAUTOATNP	0x02
#define		SCSIRSTO	0x01

/*
 * SCSI Transfer Control 1 Register (pp. 3-14,15).
 * Controls the SCSI module data path.
 */
#define	SXFRCTL1		0xc02ul
#define		BITBUCKET	0x80
#define		SWRAPEN		0x40
#define		ENSPCHK		0x20
#define		STIMESEL	0x18
#define		ENSTIMER	0x04
#define		ACTNEGEN	0x02
#define		STPWEN		0x01	/* Powered Termination */

/*
 * SCSI Interrrupt Mode 1 (pp. 3-28,29).
 * Set bits in this register enable the corresponding
 * interrupt source.
 */
#define	SIMODE1			0xc11ul
#define		ENSELTIMO	0x80
#define		ENATNTARG	0x40
#define		ENSCSIRST	0x20
#define		ENPHASEMIS	0x10
#define		ENBUSFREE	0x08
#define		ENSCSIPERR	0x04
#define		ENPHASECHG	0x02
#define		ENREQINIT	0x01

/*
 * SCSI Control Signal Read Register (p. 3-15). 
 * Reads the actual state of the SCSI bus pins
 */
#define SCSISIGI		0xc03ul
#define		CDI		0x80
#define		IOI		0x40
#define		MSGI		0x20
#define		ATNI		0x10
#define		SELI		0x08
#define		BSYI		0x04
#define		REQI		0x02
#define		ACKI		0x01

/*
 * SCSI Contol Signal Write Register (p. 3-16). 
 * Writing to this register modifies the control signals on the bus.  Only 
 * those signals that are allowed in the current mode (Initiator/Target) are
 * asserted.
 */
#define SCSISIGO		0xc03ul
#define		CDO		0x80
#define		IOO		0x40
#define		MSGO		0x20
#define		ATNO		0x10
#define		SELO		0x08
#define		BSYO		0x04
#define		REQO		0x02
#define		ACKO		0x01

/* XXX document this thing */
#define SCSIRATE		0xc04ul

/*
 * SCSI ID (p. 3-18).
 * Contains the ID of the board and the current target on the
 * selected channel
 */
#define SCSIID			0xc05ul
#define		TID		0xf0		/* Target ID mask */
#define		OID		0x0f		/* Our ID mask */

/*
 * SCSI Status 0 (p. 3-21)
 * Contains one set of SCSI Interrupt codes
 * These are most likely of interest to the sequencer
 */
#define SSTAT0			0xc0bul
#define		TARGET		0x80		/* Board is a target */
#define		SELDO		0x40		/* Selection Done */
#define		SELDI		0x20		/* Board has been selected */
#define		SELINGO		0x10		/* Selection In Progress */
#define		SWRAP		0x08		/* 24bit counter wrap */
#define		SDONE		0x04		/* STCNT = 0x000000 */
#define		SPIORDY		0x02		/* SCSI PIO Ready */
#define		DMADONE		0x01		/* DMA transfer completed */

/*
 * Clear SCSI Interrupt 1 (p. 3-23)
 * Writing a 1 to a bit clears the associated SCSI Interrupt in SSTAT1.
 */ 
#define CLRSINT1		0xc0cul
#define		CLRSELTIMEO	0x80
#define		CLRATNO		0x40
#define		CLRSCSIRSTI	0x20
/*  UNUSED			0x10 */
#define		CLRBUSFREE	0x08
#define		CLRSCSIPERR	0x04
#define		CLRPHASECHG	0x02
#define		CLRREQINIT	0x01

/*
 * SCSI Status 1 (p. 3-24)
 * These interrupt bits are of interest to the kernel driver
 */
#define SSTAT1			0xc0cul
#define		SELTO		0x80
#define		ATNTARG 	0x40
#define		SCSIRSTI	0x20
#define		PHASEMIS	0x10
#define		BUSFREE		0x08
#define		SCSIPERR	0x04
#define		PHASECHG	0x02
#define		REQINIT		0x01

/*
 * Selection/Reselection ID (p. 3-31)
 * Upper four bits are the device id.  The ONEBIT is set when the re/selecting
 * device did not set its own ID.
 */
#define SELID			0xc19ul
#define		SELID_MASK	0xf0
#define		ONEBIT		0x08
/*  UNUSED			0x07 */

/*
 * SCSI Block Control (p. 3-32)
 * Controls Bus type and channel selection.  In a twin channel configuration
 * addresses 0x00-0x1e are gated to the appropriate channel based on this
 * register.  SELWIDE allows for the coexistence of 8bit and 16bit devices
 * on a wide bus.
 */
#define SBLKCTL			0xc1ful
/*  UNUSED			0xc0 */
#define		AUTOFLUSHDIS	0x20
/*  UNUSED			0x10 */
#define		SELBUSB		0x08
/*  UNUSED			0x04 */
#define		SELWIDE		0x02 
/*  UNUSED			0x01 */

/*
 * Sequencer Control (p. 3-33)
 * Error detection mode and speed configuration
 */
#define SEQCTL			0xc60ul
#define		PERRORDIS	0x80
#define		PAUSEDIS	0x40
#define		FAILDIS		0x20
#define 	FASTMODE	0x10
#define		BRKADRINTEN	0x08
#define		STEP		0x04
#define		SEQRESET	0x02
#define		LOADRAM		0x01

/*
 * Sequencer RAM Data (p. 3-34)
 * Single byte window into the Scratch Ram area starting at the address
 * specified by SEQADDR0 and SEQADDR1.  To write a full word, simply write
 * four bytes in sucessesion.  The SEQADDRs will increment after the most
 * significant byte is written
 */
#define SEQRAM			0xc61ul

/*
 * Sequencer Address Registers (p. 3-35) 
 * Only the first bit of SEQADDR1 holds addressing information
 */
#define SEQADDR0		0xc62ul
#define SEQADDR1		0xc63ul
#define 	SEQADDR1_MASK	0x01

/*
 * Accumulator
 * We cheat by passing arguments in the Accumulator up to the kernel driver
 */
#define ACCUM			0xc64ul

#define SINDEX			0xc65ul

/*
 * Board Control (p. 3-43)
 */
#define BCTL			0xc84ul
/*   RSVD			0xf0 */
#define		ACE		0x08	/* Support for external processors */
/*   RSVD			0x06 */
#define		ENABLE		0x01

/*
 * Host Control (p. 3-47) R/W
 * Overal host control of the device.  
 */
#define HCNTRL			0xc87ul
/*    UNUSED			0x80 */
#define		POWRDN		0x40
/*    UNUSED			0x20 */
#define		SWINT		0x10
#define		IRQMS		0x08
#define		PAUSE		0x04
#define		INTEN		0x02
#define		CHIPRST		0x01

/*
 * SCB Pointer (p. 3-49)
 * Gate one of the four SCBs into the SCBARRAY window.
 */
#define SCBPTR			0xc90ul

/*
 * Interrupt Status (p. 3-50)
 * Status for system interrupts
 */
#define INTSTAT			0xc91ul
#define		SEQINT_MASK	0xf0		/* SEQINT Status Codes */
#define			BAD_PHASE	0x00
#define			SEND_REJECT	0x10
#define			NO_IDENT	0x20
#define			NO_MATCH	0x30
#define			MSG_SDTR	0x40
#define			MSG_WDTR	0x50
#define			MSG_REJECT	0x60
#define			BAD_STATUS	0x70
#define			RESIDUAL	0x80
#define			ABORT_TAG	0x90
#define 	BRKADRINT 0x08
#define		SCSIINT	  0x04
#define		CMDCMPLT  0x02
#define		SEQINT    0x01
#define		INT_PEND  (BRKADRINT | SEQINT | SCSIINT | CMDCMPLT)

/*
 * Hard Error (p. 3-53)
 * Reporting of catastrophic errors.  You usually cannot recover from 
 * these without a full board reset.
 */
#define ERROR			0xc92ul
/*    UNUSED			0xf0 */
#define		PARERR		0x08
#define		ILLOPCODE	0x04
#define		ILLSADDR	0x02
#define		ILLHADDR	0x01

/*
 * Clear Interrupt Status (p. 3-52)
 */
#define CLRINT			0xc92ul
#define		CLRBRKADRINT	0x08
#define		CLRSCSIINT      0x04
#define		CLRCMDINT 	0x02
#define		CLRSEQINT 	0x01

/*
 * SCB Auto Increment (p. 3-59)
 * Byte offset into the SCB Array and an optional bit to allow auto 
 * incrementing of the address during download and upload operations
 */
#define SCBCNT			0xc9aul
#define		SCBAUTO		0x80
#define		SCBCNT_MASK	0x1f

/*
 * Queue In FIFO (p. 3-60)
 * Input queue for queued SCBs (commands that the seqencer has yet to start)
 */
#define QINFIFO			0xc9bul

/*
 * Queue In Count (p. 3-60)
 * Number of queued SCBs
 */
#define QINCNT			0xc9cul

/*
 * Queue Out FIFO (p. 3-61)
 * Queue of SCBs that have completed and await the host
 */
#define QOUTFIFO		0xc9dul

/*
 * Queue Out Count (p. 3-61)
 * Number of queued SCBs in the Out FIFO
 */
#define QOUTCNT			0xc9eul

#define SCBARRAY		0xca0ul

/* ---------------- END AIC-7770 Register Definitions ----------------- */

/* --------------------- AIC-7870-only definitions -------------------- */
        
#define DSPCISTATUS		0xc86ul

/* ---------------------- Scratch RAM Offsets ------------------------- */
/* These offsets are either to values that are initialized by the board's
 * BIOS or are specified by the Linux sequencer code.  If I can figure out
 * how to read the EISA configuration info at probe time, the cards could
 * be run without BIOS support installed
 */

/*
 * 1 byte per target starting at this address for configuration values
 */
#define HA_TARG_SCRATCH		0xc20ul

/*
 * The sequencer will stick the frist byte of any rejected message here so
 * we can see what is getting thrown away.
 */
#define HA_REJBYTE		0xc31ul

/*
 * Length of pending message
 */
#define HA_MSG_LEN		0xc34ul

/*
 * message body
 */
#define HA_MSG_START		0xc35ul	/* outgoing message body */

/*
 * These are offsets into the card's scratch ram.  Some of the values are
 * specified in the AHA2742 technical reference manual and are initialized 
 * by the BIOS at boot time.
 */
#define HA_ARG_1		0xc4aul
#define HA_RETURN_1		0xc4aul
#define		SEND_WDTR	0x80
#define		SEND_SDTR	0x80

#define HA_SIGSTATE		0xc4bul

#define HA_SCBCOUNT		0xc52ul
#define HA_FLAGS		0xc53ul
#define		SINGLE_BUS	0x00
#define		TWIN_BUS	0x01
#define		WIDE_BUS	0x02
#define		SENSE		0x10
#define		ACTIVE_MSG	0x20
#define		IDENTIFY_SEEN	0x40
#define		RESELECTING	0x80

#define	HA_ACTIVE0		0xc54ul
#define	HA_ACTIVE1		0xc55ul
#define	SAVED_TCL		0xc56ul

#define HA_SCSICONF		0xc5aul
#define INTDEF			0xc5cul
#define HA_HOSTCONF		0xc5dul

#define MSG_ABORT               0x06
#define	BUS_8_BIT		0x00
#define BUS_16_BIT		0x01
#define BUS_32_BIT		0x02

/*
 * Since the sequencer can disable pausing in a critical section, we
 * must loop until it actually stops. 
 * XXX Should add a timeout in here??
 */     
#define PAUSE_SEQUENCER(ahc)      \
        outb(HCNTRL + ahc->baseport, ahc->pause);   \
				\
        while ((inb(HCNTRL + ahc->baseport) & PAUSE) == 0)             \
                        ;                                               

#define UNPAUSE_SEQUENCER(ahc)    \
        outb( HCNTRL + ahc->baseport, ahc->unpause ) 

/*
 * Restart the sequencer program from address zero
 */
#define RESTART_SEQUENCER(ahc)    \
                do {                                    \
                        outb( SEQCTL + ahc->baseport, SEQRESET|FASTMODE );     \
                } while (inb(SEQADDR0 + ahc->baseport) != 0 &&   \
			 inb(SEQADDR1 + ahc->baseport != 0));     \
                                                        \
                UNPAUSE_SEQUENCER(ahc);                   

#ifdef  AHC_DEBUG
void
ahc_print_scb(scb)
        struct scb *scb;
{
        printf("scb:0x%x control:0x%x tcl:0x%x cmdlen:%d cmdpointer:0x%x\n"
            ,scb
	    ,scb->control
	    ,scb->target_channel_lun
            ,scb->cmdlen
            ,scb->cmdpointer );    
        printf("        datlen:%d data:0x%x res:0x%x segs:0x%x segp:0x%x\n"
            ,scb->datalen[2] << 16 | scb->datalen[1] << 8 | scb->datalen[0]
            ,scb->data
	    ,scb->RESERVED[1] << 8 | scb->RESERVED[0] 
            ,scb->SG_segment_count
            ,scb->SG_list_pointer);
	printf("	sg_addr:%x sg_len:%d\n"
	    ,scb->ahc_dma[0].addr
	    ,scb->ahc_dma[0].len);
	printf("	size:%d\n"
	    ,(int)&(scb->next) - (int)scb);
}

void                  
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
	short period; /* in ns */
	char *rate;
} ahc_syncrates[] = {
	{ 0x00, 100, "10.0"  },
	{ 0x10, 125,  "8.0"  },
	{ 0x20, 150,  "6.67" },
	{ 0x30, 175,  "5.7"  },
	{ 0x40, 200,  "5.0"  },
	{ 0x50, 225,  "4.4"  },
	{ 0x60, 250,  "4.0"  },
	{ 0x70, 275,  "3.6"  }
};

static int ahc_num_syncrates =
	sizeof(ahc_syncrates) / sizeof(ahc_syncrates[0]);

/*
 * Check if the device can be found at the port given
 * and if so, determine configuration and set it up for further work.
 */

int
ahcprobe(unit, iobase, type)
	int unit;
	u_long iobase;
	ahc_type type;
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
                return 0;
        }
        ahc = malloc(sizeof(struct ahc_data), M_TEMP, M_NOWAIT);
        if (!ahc) {
                printf("ahc%d: cannot malloc!\n", unit);
                return 0;
        }
        bzero(ahc, sizeof(struct ahc_data));
        ahcdata[unit] = ahc;
        ahc->baseport = iobase;
	ahc->type = type;

        /*
         * Try to initialize a unit at this location
         * reset the AIC-7770, read its registers,
         * and fill in the dev structure accordingly
         */

        if (ahc_init(unit) != 0) {
                ahcdata[unit] = NULL;
                free(ahc, M_TEMP);
                return (0);
        }

        return (1);
}


/*
 * Look up the valid period to SCSIRATE conversion in our table.
 */
static
void ahc_scsirate(scsirate, period, offset, unit, target )
	u_char	*scsirate;
	u_char	period, offset;
	int	unit, target;
{
        int i;

        for (i = 0; i < ahc_num_syncrates; i++) {
   
                if ((ahc_syncrates[i].period - period) >= 0) {
                        *scsirate = (ahc_syncrates[i].sxfr) | (offset & 0x0f);
		        printf("ahc%d: target %d synchronous at %sMB/s, "
			       "offset = 0x%x\n",
				unit, target, ahc_syncrates[i].rate, offset );
#ifdef AHC_DEBUG
#endif /* AHC_DEBUG */
                        return;
                }
        }
	/* Default to asyncronous transfer */
        *scsirate = 0;
	printf("ahc%d: target %d using asyncronous transfers\n",
		unit, target );
#ifdef AHC_DEBUG
#endif /* AHC_DEBUG */

}


/*
 * Attach all the sub-devices we can find
 */     
int
ahc_attach(unit)
	int unit;
{
        struct ahc_data *ahc = ahcdata[unit]; 

        /*
         * fill in the prototype scsi_link.
         */
        ahc->sc_link.adapter_unit = unit;
        ahc->sc_link.adapter_targ = ahc->our_id;
        ahc->sc_link.adapter = &ahc_switch;
	ahc->sc_link.opennings = 2;
        ahc->sc_link.device = &ahc_dev;
	ahc->sc_link.flags = DEBUGLEVEL;
	ahc->sc_link.fordriver = 0;

        /*
         * ask the adapter what subunits are present
         */     
	printf("ahc%d: Probing channel A\n", unit);
        scsi_attachdevs(&(ahc->sc_link)); 

	if(ahc->type & AHC_TWIN) {
		/* Configure the second scsi bus */
		ahc->sc_link_b = ahc->sc_link;
		ahc->sc_link_b.fordriver = (void *)0x0008;
		printf("ahc%d: Probing Channel B\n", unit);
		scsi_attachdevs(&(ahc->sc_link_b));		
	}	

        return 1;
}

void 
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
                
	insb(SCBARRAY + iobase, scb, SCB_UP_SIZE);
                        
        outb(SCBCNT + iobase, 0);
}

/*              
 * Catch an interrupt from the adaptor
 */     
int
ahcintr(unit)
	int	unit;
{
	int     intstat;
	u_char	status;
        struct ahc_data *ahc = ahcdata[unit]; 
        u_long	iobase = ahc->baseport;
	struct scb *scb = NULL;
	struct scsi_xfer *xs = NULL; 

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
		      unit, hard_error[i].errmesg, 
		      (inb(SEQADDR1 + iobase) << 8) | 
		      inb(SEQADDR0 + iobase));
        }
        if (intstat & SEQINT) { 
                unsigned char transfer;

                switch (intstat & SEQINT_MASK) {
                    case BAD_PHASE:
                        panic("ahc%d: unknown scsi bus phase.  "
			      "Attempting to continue\n", unit);  
                        break; 
                    case SEND_REJECT: 
                        printf("ahc%d: Warning - " 
                              "message reject, message type: 0x%x\n", unit,
                              inb(HA_REJBYTE + iobase));
                        break; 
                    case NO_IDENT: 
                        panic("ahc%d: No IDENTIFY message from reconnecting "
			      "target %d at seqaddr = 0x%lx "
			      "SAVED_TCL == 0x%x\n",
                              unit, (inb(SELID + iobase) >> 4) & 0xf,
			      (inb(SEQADDR1 + iobase) << 8) | 
			      inb(SEQADDR0 + iobase), 
			      inb(SAVED_TCL + iobase));
			break;
                    case NO_MATCH:
			{
				u_char active;
				int active_port = HA_ACTIVE0 + iobase;
				int tcl = inb(SCBARRAY+1 + iobase);
				int target = (tcl >> 4) & 0x0f;
				printf("ahc%d: no active SCB for reconnecting "
				    "target %d, channel %c - issuing ABORT\n",
				    unit, target, tcl & 0x08 ? 'B' : 'A');
				printf("SAVED_TCL == 0x%x\n",
					inb(SAVED_TCL + iobase));
				if( tcl & 0x88 ) {
					/* Second channel stores its info 
					 * in byte two of HA_ACTIVE
					 */
					active_port++;
				}
				active = inb(active_port);
				active &= ~(0x01 << (target & 0x07));
				outb(SCBARRAY + iobase, SCB_NEEDDMA);
				outb(active_port, active);
				outb(CLRSINT1 + iobase, CLRSELTIMEO);
	                        RESTART_SEQUENCER(ahc);
                        	break;
			}
                    case MSG_SDTR:
			{
				u_char scsi_id, offset, rate, targ_scratch;
	                        /* 
				 * Help the sequencer to translate the 
				 * negotiated transfer rate.  Transfer is 
				 * 1/4 the period in ns as is returned by 
				 * the sync negotiation message.  So, we must 
				 * multiply by four
				 */
	                        transfer = inb(HA_ARG_1 + iobase) << 2;
				/* The bottom half of SCSIXFER*/
				offset = inb(ACCUM + iobase);
				scsi_id = inb(SCSIID + iobase) >> 0x4;
				ahc_scsirate(&rate, transfer, offset, unit,
					scsi_id);
				if(inb(SBLKCTL + iobase) & 0x08)
					/* B channel */
					scsi_id += 8;
				targ_scratch = inb(HA_TARG_SCRATCH + iobase 
						   + scsi_id);
				/* Preserve the WideXfer flag */
				rate |= targ_scratch & 0x80;	
				outb(HA_TARG_SCRATCH + iobase + scsi_id, rate);
				outb(SCSIRATE + iobase, rate); 
				/* See if we initiated Sync Negotiation */
				if(ahc->sdtrpending & (0x01 << scsi_id))
				{
					/*
					 * Don't send an SDTR back to
					 * the target
					 */
					outb(HA_RETURN_1 + iobase, 0);
				}
				else{
					/*
					 * Send our own SDTR in reply
					 */
#ifdef AHC_DEBUG
					if(ahc_debug & AHC_SHOWMISC)
						printf("Sending SDTR!!\n");
#endif
					outb(HA_RETURN_1 + iobase, SEND_SDTR);
				}
				/* 
				 * Negate the flags
				 */
				ahc->needsdtr &= ~(0x01 << scsi_id);
				ahc->sdtrpending &= ~(0x01 << scsi_id);
	                        break;
			}
                    case MSG_WDTR:
			{
				u_char scsi_id, scratch, bus_width;

				bus_width = inb(ACCUM + iobase);
				scsi_id = inb(SCSIID + iobase) >> 0x4;

				if(inb(SBLKCTL + iobase) & 0x08)
					/* B channel */
					scsi_id += 8;

				scratch = inb(HA_TARG_SCRATCH + iobase 
					      + scsi_id);

				if(ahc->wdtrpending & (0x01 << scsi_id))
				{
					/* 
					 * Don't send a WDTR back to the 
					 * target, since we asked first.
					 */
					outb(HA_RETURN_1 + iobase, 0);
					switch(bus_width)
					{
						case BUS_8_BIT:
							scratch &= 0x7f;
							break;
						case BUS_16_BIT:
		        				printf("ahc%d: target "
							       "%d using 16Bit "
							       "transfers\n",
								unit, scsi_id);
							scratch |= 0x88;	
							scratch &= 0xf8;
							break;
					}
				}
				else {
					/*
					 * Send our own WDTR in reply
					 */
					printf("Will Send WDTR!!\n");
					switch(bus_width)
					{
						case BUS_8_BIT:
							scratch &= 0x7f;
							break;
						case BUS_32_BIT:
							/* Negotiate 16_BITS */
							bus_width = BUS_16_BIT;
						case BUS_16_BIT:
		        				printf("ahc%d: target "
							       "%d using 16Bit "
							       "transfers\n",
								unit, scsi_id);
							scratch |= 0x88;	
							scratch &= 0xf8;
							break;
					}
					outb(HA_RETURN_1 + iobase, 
						bus_width | SEND_WDTR);
				}
				ahc->needwdtr &= ~(0x01 << scsi_id);
				ahc->wdtrpending &= ~(0x01 << scsi_id);
				outb(HA_TARG_SCRATCH + iobase + scsi_id, scratch);
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
				u_char scsi_id = inb(SCSIID + iobase) >> 0x4;
				u_short mask;

				if(inb(SBLKCTL + iobase) & 0x08)
					/* B channel */
					scsi_id += 8;

				targ_scratch = inb(HA_TARG_SCRATCH + iobase
						   + scsi_id);

				mask = (0x01 << scsi_id);
				if(ahc->wdtrpending & mask){
					/* note 8bit xfers and clear flag */
					targ_scratch &= 0x7f;
					ahc->needwdtr &= ~mask;
					ahc->wdtrpending &= ~mask;
        				printf("ahc%d: target %d refusing "
					       "WIDE negotiation.  Using "
					       "8bit transfers\n",
						unit, scsi_id);
				}
				else if(ahc->sdtrpending & mask){
					/* note asynch xfers and clear flag */
					targ_scratch &= 0xf0;
					ahc->needsdtr &= ~mask;
					ahc->sdtrpending &= ~mask;
        				printf("ahc%d: target %d refusing "
					       "syncronous negotiation.  Using "
					       "asyncronous transfers\n",
						unit, scsi_id);
				}
				else {
					/*
					 * Otherwise, we ignore it.
					 */
#ifdef AHC_DEBUG
					if(ahc_debug & AHC_SHOWMISC)
						printf("Message reject -- "
						       "ignored\n");
#endif
					break;
				}
				outb(HA_TARG_SCRATCH + iobase + scsi_id,
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
		 	  if (!scb || !(scb->flags & SCB_ACTIVE)) {
                              printf("ahc%d: ahcintr - referenced scb not "
				   "valid during seqint 0x%x scb(%d)\n", 
				   unit, intstat, scb_index);
			      goto clear;
			}

			xs = scb->xs;

			ahc_getscb(iobase, scb);

#ifdef AHC_DEBUG
			if(xs->sc_link->target == DEBUGTARG)
				ahc_print_scb(scb);
#endif
			xs->status = scb->target_status;
			switch(scb->target_status){
			    case SCSI_OK:
				printf("ahc%d: Interrupted for staus of"
					" 0???\n", unit);
				break;
			    case SCSI_CHECK:
#ifdef AHC_DEBUG
				printf("ahc%d: target %d, lun %d (%s%d) "
					"requests Check Status\n", unit
					,xs->sc_link->target
					,xs->sc_link->lun
					,xs->sc_link->device->name
					,xs->sc_link->dev_unit);
#endif

				if((xs->error == XS_NOERROR) && 
				    !(scb->flags & SCB_SENSE)) {
					u_char flags;
					struct ahc_dma_seg *sg = scb->ahc_dma;
					struct scsi_sense *sc = &(scb->sense_cmd);
					u_char control = scb->control;
					u_char tcl = scb->target_channel_lun;
#ifdef AHC_DEBUG
					printf("ahc%d: target %d, lun %d "
						"(%s%d) Sending Sense\n", unit
						,xs->sc_link->target
						,xs->sc_link->lun
						,xs->sc_link->device->name
						,xs->sc_link->dev_unit);
#endif
					bzero(scb, SCB_DOWN_SIZE);
					scb->flags |= SCB_SENSE;
					scb->control = control & SCB_TE;
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

					outb(SCBCNT + iobase, 0x80);
					outsb(SCBARRAY+iobase,scb,SCB_DOWN_SIZE);
					outb(SCBCNT + iobase, 0);

					flags = inb(HA_FLAGS + iobase);
					/* 
					 * Have the sequencer handle the sense
					 * request
					 */
					outb(HA_FLAGS + iobase, flags | SENSE);
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
				printf("ahc%d: Target Busy\n", unit);
				break;
			    case SCSI_QUEUE_FULL:
				/*
				 * The upper level SCSI code will eventually
				 * handle this properly.
				 */
				printf("ahc%d: Queue Full\n", unit);
				xs->error = XS_BUSY;
				break;
			    default:
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
			/*
			 * Don't clobber valid resid info with
			 * a resid coming from a check sense
			 * operation.
			 */
			if(!(scb->flags & SCB_SENSE))
			    scb->xs->resid = (inb(iobase+SCBARRAY+17) << 16) |
					     (inb(iobase+SCBARRAY+16) << 8) |
					      inb(iobase+SCBARRAY+15);
			break;
		  }
		  case ABORT_TAG:
		  {
                        int   scb_index;
			scb_index = inb(SCBPTR + iobase);
			scb = ahc->scbarray[scb_index];
			/*
			 * We didn't recieve a valid tag back from 
			 * the target on a reconnect.
			 */
			printf("ahc%d: invalid tag recieved on channel %c "  
			       "target %d, lun %d -- sending ABORT_TAG\n",
			       unit,
			       ((u_long)xs->sc_link->fordriver & 0x08)? 'B':'A',
			       xs->sc_link->target,
			       xs->sc_link->lun);
			scb->xs->error = XS_DRIVER_STUFFUP;
			untimeout(ahc_timeout, (caddr_t)scb);
			ahc_done(unit, scb);
			break;
		  }
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
				unit, status, scb_index);
                        outb(CLRSINT1 + iobase, status);
                        UNPAUSE_SEQUENCER(ahc);
                        outb(CLRINT + iobase, CLRSCSIINT);
			scb = NULL;
			goto cmdcomplete;
                }
		xs = scb->xs;

		if (status & SELTO) { 
			u_char active;
			u_char flags;
			u_long active_port = HA_ACTIVE0 + iobase;
                        outb(SCSISEQ + iobase, 0);
                        xs->error = XS_TIMEOUT;
			/* 
			 * Clear any pending messages for the timed out
			 * target, and mark the target as free
			 */
			flags = inb( HA_FLAGS + iobase );
                        outb(HA_FLAGS + iobase, flags & ~ACTIVE_MSG);

			if (scb->target_channel_lun & 0x88)
				active_port++;

			active = inb(active_port) &
				~(0x01 << (xs->sc_link->target & 0x07));
			outb(active_port, active);

			outb(SCBARRAY + iobase, SCB_NEEDDMA);

                        outb(CLRSINT1 + iobase, CLRSELTIMEO);

                        RESTART_SEQUENCER(ahc);
                         
                        outb(CLRINT + iobase, CLRSCSIINT);
                }       
                        
                if (status & SCSIPERR) { 
                        printf("ahc%d: parity error on channel %c "
			       "target %d, lun %d\n",
			       unit,
			       ((u_long)xs->sc_link->fordriver & 0x08)? 'B':'A',
                               xs->sc_link->target,
                               xs->sc_link->lun);
                        xs->error = XS_DRIVER_STUFFUP;
      
                        outb(CLRSINT1 + iobase, CLRSCSIPERR);
                        UNPAUSE_SEQUENCER(ahc);
                         
                        outb(CLRINT + iobase, CLRSCSIINT);
			scb = NULL;
                }       
                if (status & BUSFREE) {
#if 0
                     /* 
		      * Has seen busfree since selection, i.e.
                      * a "spurious" selection. Shouldn't happen.
                      */
                      printf("ahc: unexpected busfree\n"); 
                      xs->error = XS_DRIVER_STUFFUP; 
                      outb(CLRSINT1 + iobase, BUSFREE);    /* CLRBUSFREE */
#endif
		}
		else {
                      printf("ahc%d: Unknown SCSIINT. Status = 0x%x\n", 
			     unit, status);
                      outb(CLRSINT1 + iobase, status);
                      UNPAUSE_SEQUENCER(ahc);
                      outb(CLRINT + iobase, CLRSCSIINT);
		      scb = NULL;
                }
		if(scb != NULL) {
		    /* We want to process the command */
                    untimeout(ahc_timeout, (caddr_t)scb);
                    ahc_done(unit, scb);
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
					unit, scb_index, inb(QOUTCNT + iobase));
                        	outb(CLRINT + iobase, CLRCMDINT);      
                                continue;
                        }  
                               
                        outb(CLRINT + iobase, CLRCMDINT);      
                        untimeout(ahc_timeout, (caddr_t)scb);
                        ahc_done(unit, scb);
                        
                } while (inb(QOUTCNT + iobase));
        }               
	return 1;
}

/*
 * We have a scb which has been processed by the
 * adaptor, now we look to see how the operation
 * went.
 */
void
ahc_done(unit, scb)
	int unit;
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
	if(xs->cmd->opcode == 0x12 && xs->error == XS_NOERROR) 
	{
		struct ahc_data *ahc = ahcdata[unit];
		struct scsi_inquiry_data *inq_data;
		u_short mask = 0x01 << (xs->sc_link->target |
				(scb->target_channel_lun & 0x08));
		/*
		 * Sneak a look at the results of the SCSI Inquiry
		 * command and see if we can do Tagged queing.  This
		 * should really be done by the higher level drivers.
		 */
		inq_data = (struct scsi_inquiry_data *)xs->data;
		if(((inq_data->device & SID_TYPE) == 0) 
		    && (inq_data->flags & SID_CmdQue)
		    && !(ahc->tagenable & mask))
		{
			/*
			 * Disk type device and can tag
			 */
		        printf("ahc%d: target %d Tagged Queuing Device\n",
				unit, xs->sc_link->target);
			ahc->tagenable |= mask;
#ifdef QUEUE_FULL_SUPPORTED
			xs->sc_link->opennings += 2; */
#endif
		}
	}
        ahc_free_scb(unit, scb, xs->flags);
        scsi_done(xs);  
}

/*
 * Start the board, ready for normal operation
 */
int
ahc_init(unit)
	int      unit;
{
	struct  ahc_data *ahc = ahcdata[unit];
	u_long	iobase = ahc->baseport;
	u_char	scsi_conf, sblkctl;
	int     intdef, i, max_targ = 16, wait;

	/*
	 * Assume we have a board at this stage
	 * Find out the configured interupt and the card type.
	 */

#ifdef AHC_DEBUG
	printf("ahc%d: scb %d bytes; SCB_SIZE %d bytes, ahc_dma %d bytes\n", 
		unit, sizeof(struct scb), SCB_DOWN_SIZE, 
		sizeof(struct ahc_dma_seg));
#endif /* AHC_DEBUG */
	printf("ahc%d: reading board settings\n", unit);

	/* Save the IRQ type before we do a chip reset */

	ahc->unpause = (inb(HCNTRL + iobase) & IRQMS) | INTEN;
	ahc->pause = (inb(HCNTRL + iobase) & IRQMS) | INTEN | PAUSE;
	outb(HCNTRL + iobase, CHIPRST);
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
		printf("ahc%d: Failed chip reset - probe failed!\n", unit);
		return(1);
	}
	switch( ahc->type ) {
	   case AHC_274:
		printf("ahc%d: 274x ", unit);
		ahc->maxscbs = 0x4;
		break;
	   case AHC_284:
		printf("ahc%d: 284x ", unit);
		ahc->maxscbs = 0x4;
		break;
	   case AHC_AIC7870:
	   case AHC_294:
		if( ahc->type == AHC_AIC7870)
			printf("ahc%d: aic7870 ", unit);
		else
			printf("ahc%d: 294x ", unit);
		ahc->maxscbs = 0x10;
		#define DFTHRESH        3
		outb(DSPCISTATUS + iobase, DFTHRESH << 6);
		/* XXX Hard coded SCSI ID for now */
		outb(HA_SCSICONF + iobase, 0x07 | (DFTHRESH << 6));
		/* In case we are a wide card */
		outb(HA_SCSICONF + 1 + iobase, 0x07);
		break;
	   default:
	};

        /* Determine channel configuration and who we are on the scsi bus. */
        switch ( (sblkctl = inb(SBLKCTL + iobase) & 0x0f) ) {
            case 0:
		ahc->our_id = (inb(HA_SCSICONF + iobase) & HSCSIID);
                printf("Single Channel, SCSI Id=%d, ", ahc->our_id);
		outb(HA_FLAGS + iobase, SINGLE_BUS);
                break;
            case 2:
		ahc->our_id = (inb(HA_SCSICONF + 1 + iobase) & HWSCSIID);
                printf("Wide Channel, SCSI Id=%d, ", ahc->our_id);
		ahc->type |= AHC_WIDE;
		outb(HA_FLAGS + iobase, WIDE_BUS);
                break;
            case 8:
		ahc->our_id = (inb(HA_SCSICONF + iobase) & HSCSIID);
		ahc->our_id_b = (inb(HA_SCSICONF + 1 + iobase) & HSCSIID);
                printf("Twin Channel, A SCSI Id=%d, B SCSI Id=%d, ",
			ahc->our_id, ahc->our_id_b);
		ahc->type |= AHC_TWIN;
		outb(HA_FLAGS + iobase, TWIN_BUS);
                break;
            default:
                printf(" Unsupported adapter type.  Ignoring\n");
                return(-1);
        }
	/*
	 * Take the bus led out of diagnostic mode
	 */
	outb(SBLKCTL + iobase, sblkctl);
	/* 
	 * Number of SCBs that will be used. Rev E aic7770s and
	 * aic7870s have 16.  The rest have 4.
	 */
	if(!(ahc->type & AHC_AIC7870))
	{
		/* 
		 * See if we have a Rev E or higher
		 * aic7770.  If so, use 16 SCBs.
		 * Anything below a Rev E will have a
		 * R/O autoflush disable configuration bit.
		 */
		u_char sblkctl_orig;
		sblkctl_orig = inb(SBLKCTL + iobase);
		sblkctl = sblkctl_orig ^ AUTOFLUSHDIS;
		outb(SBLKCTL + iobase, sblkctl);
		sblkctl = inb(SBLKCTL + iobase);
		if(sblkctl != sblkctl_orig)
		{
			printf("aic7770 >= Rev E, ");
			ahc->maxscbs = 0x10;
			/*
			 * Ensure autoflush is enabled
			 */
			sblkctl &= ~AUTOFLUSHDIS;
			outb(SBLKCTL + iobase, sblkctl);
		}
		else
			printf("aic7770 <= Rev C, ");
	}
	else
		printf("aic7870, ");
	printf("%d SCBs\n", ahc->maxscbs);
	if(!(ahc->type & AHC_AIC7870)){
	/* 
	 * The 294x cards are PCI, so we get their interrupt from the PCI
	 * BIOS. 
	 */

		intdef = inb(INTDEF + iobase);
		switch (intdef & 0xf) {
		case 9:
			ahc->vect = 9;
			break;
		case 10:
			ahc->vect = 10;
			break;
		case 11:
			ahc->vect = 11;
			break;
		case 12:
			ahc->vect = 12;
			break;
		case 14:
			ahc->vect = 14;
			break;
		case 15:
			ahc->vect = 15;
			break;
		default:
			printf("illegal irq setting\n");
			return (EIO);
		}
	}

	/* Set the SCSI Id, SXFRCTL1, and SIMODE1, for both channes */
	if( ahc->type & AHC_TWIN)
	{
		/* 
		 * The device is gated to channel B after a chip reset,
		 * so set those values first
		 */
		outb(SCSIID + iobase, ahc->our_id_b);
		scsi_conf = inb(HA_SCSICONF + 1 + iobase) & (ENSPCHK|STIMESEL);
		outb(SXFRCTL1 + iobase, scsi_conf|ENSTIMER|ACTNEGEN|STPWEN);
		outb(SIMODE1 + iobase, ENSELTIMO|ENSCSIPERR);
		/* Select Channel A */
		outb(SBLKCTL + iobase, 0);
	}
	outb(SCSIID + iobase, ahc->our_id);
	scsi_conf = inb(HA_SCSICONF + iobase) & (ENSPCHK|STIMESEL);
	outb(SXFRCTL1 + iobase, scsi_conf|ENSTIMER|ACTNEGEN|STPWEN);
	outb(SIMODE1 + iobase, ENSELTIMO|ENSCSIPERR);

	/*
	 * Look at the information that board initialization or
	 * the board bios has left us.  In the lower four bits of each
	 * target's scratch space any value other than 0 indicates
	 * that we should initiate syncronous transfers.  If it's zero, 
	 * the user or the BIOS has decided to disable syncronous 
	 * negotiation to that target so we don't activate the needsdr
	 * flag.
	 */
	ahc->needsdtr_orig = 0;
	ahc->needwdtr_orig = 0;
	if(!(ahc->type & AHC_WIDE))
		max_targ = 8;

	for(i = 0; i < max_targ; i++){
		u_char target_settings = inb(HA_TARG_SCRATCH + i + iobase);
		if(target_settings & 0x0f){
			ahc->needsdtr_orig |= (0x01 << i);
			/* Default to a asyncronous transfers (0 offset) */
			target_settings &= 0xf0;
		}
		if(target_settings & 0x80){
			ahc->needwdtr_orig |= (0x01 << i);
			/*
			 * We'll set the Wide flag when we
			 * are successful with Wide negotiation,
			 * so turn it off for now so we aren't
			 * confused.
			 */
			target_settings &= 0x7f;
		}
		outb(HA_TARG_SCRATCH+i+iobase,target_settings);
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
	printf("NEEDSDTR == 0x%x\nNEEDWDTR == 0x%x\n", ahc->needsdtr,
		ahc->needwdtr);
#endif
	/*
	 * Set the number of availible SCBs
	 */
	outb(HA_SCBCOUNT + iobase, ahc->maxscbs);

	/* We don't have any busy targets right now */
	outb( HA_ACTIVE0 + iobase, 0 );
	outb( HA_ACTIVE1 + iobase, 0 );

	/*
	 * Load the Sequencer program and Enable the adapter.
	 * Place the aic7770 in fastmode which makes a big
	 * difference when doing many small block transfers.
         */
	
        printf("ahc%d: Downloading Sequencer Program...", unit);
	ahc_loadseq(iobase);
	printf("Done\n");

        outb(SEQCTL + iobase, FASTMODE);
	if (!(ahc->type & AHC_AIC7870))
		outb(BCTL + iobase, ENABLE); 

	/* Reset the bus */
	outb(SCSISEQ + iobase, SCSIRSTO); 
	DELAY(1000);
	outb(SCSISEQ + iobase, 0);

        UNPAUSE_SEQUENCER(ahc);

	/*
	 * Note that we are going and return (to probe)
	 */
	ahc->flags = AHC_INIT;
	return (0);
}

void
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
int32
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
        if (xs->bp)
                flags |= (SCSI_NOSLEEP);        /* just to be sure */
        if (flags & ITSDONE) {
                printf("ahc%d: Already done?", unit);  
                xs->flags &= ~ITSDONE;  
        }
        if (!(flags & INUSE)) {
                printf("ahc%d: Not in use?", unit);
                xs->flags |= INUSE;
        }
        if (!(scb = ahc_get_scb(unit, flags))) {
                xs->error = XS_DRIVER_STUFFUP;
                return (TRY_AGAIN_LATER);
        }
        SC_DEBUG(xs->sc_link, SDEV_DB3, ("start scb(%x)\n", scb));
        scb->xs = xs;
        if (flags & SCSI_RESET) {
		/* XXX: Needs Implementation */
		printf("ahc0: SCSI_RESET called.\n");
        }       
        /*
         * Put all the arguments for the xfer in the scb
         */     

	if(ahc->tagenable & mask)
		scb->control |= SCB_TE;
	if((ahc->needwdtr & mask) && !(ahc->wdtrpending & mask))     
	{
		scb->control |= SCB_NEEDWDTR;
		ahc->wdtrpending |= mask;
	}
	if((ahc->needsdtr & mask) && !(ahc->sdtrpending & mask))
	{
		scb->control |= SCB_NEEDSDTR;
		ahc->sdtrpending |= mask;
	}
	scb->target_channel_lun = ((xs->sc_link->target << 4) & 0xF0) | 
				  ((u_long)xs->sc_link->fordriver & 0x08) |
				  (xs->sc_link->lun & 0x07);
        scb->cmdlen = xs->cmdlen;
	scb->cmdpointer = KVTOPHYS(xs->cmd);
	xs->resid = 0;
        if (xs->datalen) {      /* should use S/G only if not zero length */
                scb->SG_list_pointer = KVTOPHYS(scb->ahc_dma); 
                sg = scb->ahc_dma;
                seg = 0;
                {        
                        /*
                         * Set up the scatter gather block
                         */     
                        SC_DEBUG(xs->sc_link, SDEV_DB4,
                            ("%d @0x%x:- ", xs->datalen, xs->data));
                        datalen = xs->datalen;
                        thiskv = (int) xs->data;
                        thisphys = KVTOPHYS(thiskv);
        
                        while ((datalen) && (seg < AHC_NSEG)) {
                                bytes_this_seg = 0;

                                /* put in the base address */
                                sg->addr = thisphys;

                                SC_DEBUGN(xs->sc_link, SDEV_DB4, ("0x%x", thisphys));   
        
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
                SC_DEBUGN(xs->sc_link, SDEV_DB4, ("\n"));
                if (datalen) { /* there's still data, must have run out of segs! */                    
                        printf("ahc_scsi_cmd%d: more than %d DMA segs\n",
                            unit, AHC_NSEG);
                        xs->error = XS_DRIVER_STUFFUP;
                        ahc_free_scb(unit, scb, flags);
                        return (HAD_ERROR);
                }
        } 
	/*  else No data xfer, use non S/G values 
	 *  the SG_segment_count and SG_list_pointer are pre-zeroed, so 
	 *  we don't have to do anything
	 */

        /*                               
         * Usually return SUCCESSFULLY QUEUED
         */
#ifdef AHC_DEBUG
        if(xs->sc_link->target == DEBUGTARG)
		ahc_print_scb(scb);
#endif
        if (!(flags & SCSI_NOMASK)) {   
                s = splbio();           
		ahc_send_scb(ahc, scb);
	        timeout(ahc_timeout, (caddr_t)scb, (xs->timeout * hz) / 1000);
                splx(s);                
                SC_DEBUG(xs->sc_link, SDEV_DB3, ("cmd_sent\n"));
                return (SUCCESSFULLY_QUEUED);
        }                               
        /*                              
         * If we can't use interrupts, poll on completion
         */                             
        ahc_send_scb(ahc, scb);
        SC_DEBUG(xs->sc_link, SDEV_DB3, ("cmd_wait\n"));
        do {                            
                if (ahc_poll(unit, xs->timeout)) {
                        if (!(xs->flags & SCSI_SILENT))
                                printf("cmd fail\n");
			printf("cmd fail\n");
			ahc_abort_scb(unit,ahc,scb);
                        return (HAD_ERROR);
                } 
        } while (!(xs->flags & ITSDONE));  /* a non command complete intr */               
        if (xs->error) {
                return (HAD_ERROR);
        }       
        return (COMPLETE);  
}                       


/*      
 * Return some information to the caller about
 * the adapter and it's capabilities.  
 */     
u_int32
ahc_adapter_info(unit)
        int     unit;
{
        return (2);         /* 2 outstanding requests at a time per device */   
}   

/*
 * A scb (and hence an scb entry on the board is put onto the
 * free list.
 */
void
ahc_free_scb(unit, scb, flags)
        int     unit, flags;
        struct  scb *scb;
{
        unsigned int opri = 0;
        struct ahc_data *ahc = ahcdata[unit];

        if (!(flags & SCSI_NOMASK))
                opri = splbio();

        scb->flags = SCB_FREE;
        scb->next = ahc->free_scb;
        ahc->free_scb = scb;
        /*
         * If there were none, wake abybody waiting for
         * one to come free, starting with queued entries
         */
        if (!scb->next) {
                wakeup((caddr_t)&ahc->free_scb);
        }
        if (!(flags & SCSI_NOMASK))
                splx(opri);
}
 
/*
 * Get a free scb
 * If there are none, see if we can allocate a
 * new one.  Otherwise either return an error or sleep
 */
struct scb *
ahc_get_scb(unit, flags)
        int     unit, flags;
{
        struct ahc_data *ahc = ahcdata[unit];
        unsigned opri = 0;
        struct scb *scbp;
	int position;

        if (!(flags & SCSI_NOMASK))
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
				 * reference.
				 */ 
				scbp->control = SCB_NEEDDMA;
				scbp->host_scb = scbaddr;
				PAUSE_SEQUENCER(ahc);
				curscb = inb(SCBPTR + iobase);
				outb(SCBPTR + iobase, scbp->position);
				outb(SCBCNT + iobase, 0x80);
				outsb(SCBARRAY+iobase,scbp,30);
				outb(SCBCNT + iobase, 0);
				outb(SCBPTR + iobase, curscb);
				UNPAUSE_SEQUENCER(ahc);
				scbp->control = 0;

                        } else {
                                printf("ahc%d: Can't malloc SCB\n", unit);
                        } goto gottit;
                } else {
                        if (!(flags & SCSI_NOSLEEP)) {
                                tsleep((caddr_t)&ahc->free_scb, PRIBIO,
                                    "ahcscb", 0);
                        }
                }
        } if (scbp) {
                /* Get SCB from from free list */
                ahc->free_scb = scbp->next;
		/* preserve the position */
		position = scbp->position;
                bzero(scbp, sizeof(struct scb));
                scbp->flags = SCB_ACTIVE;
		scbp->position = position;
        }
gottit: if (!(flags & SCSI_NOMASK))
                splx(opri);

        return (scbp);
}

void ahc_loadseq(iobase)
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

/*              
 * Function to poll for command completion when in poll mode
 */     
int 
ahc_poll(int unit, int wait)
{                               /* in msec  */
        struct	ahc_data *ahc = ahcdata[unit];  
        u_long	iobase = ahc->baseport;
        u_long	stport = INTSTAT + iobase;  

        while (--wait) {
                DELAY(1000);
                if (inb(stport) & INT_PEND)
                        break;
        } if (wait == 0) {
                printf("ahc%d: board not responding\n", unit);
                return (EIO);
        }
	ahcintr(unit);
        return (0);
}

void
ahc_abort_scb( unit, ahc, scb )
	int unit;
        struct ahc_data *ahc;
        struct scb *scb;
{
	u_long iobase = ahc->baseport;
	int found = 0;
	int active_scb;
	u_char flags;
	u_char scb_control;

	PAUSE_SEQUENCER(ahc);
	/*
	 * Case 1: In the QINFIFO
	 */
	{
		int saved_queue[AHC_SCB_MAX];
		int i;
		int queued = inb(QINCNT + iobase);

		for( i = 0; i < (queued - found); i++){
			saved_queue[i] = inb(QINFIFO + iobase);
			if( saved_queue[i] == scb->position ){
				i--;
				found = 1;
			}
		}
		/* Re-insert entries back into the queue */
		for( queued = 0; queued < i; queued++ )
			outb(QINFIFO + iobase, saved_queue[queued]);
			
		if( found ){
			goto done;
		}
	}

	active_scb = inb(SCBPTR + iobase);
	/*
	 * Case 2: Not the active command
	 */
	if( active_scb != scb->position ){
		/*
		 * Select the SCB we want to abort
		 * and turn off the disconnected bit.
		 * the driver will then abort the command
		 * and notify us of the abort.
		 */
		outb(SCBPTR + iobase, scb->position);
		scb_control = inb(SCBARRAY + iobase);
		scb_control &= ~SCB_DIS;
		outb(SCBARRAY + iobase, scb_control);
		outb(SCBPTR + iobase, active_scb);
		goto done;
	}
	scb_control = inb(SCBARRAY + iobase);
	scb_control &= ~SCB_DIS;
	if( scb_control & SCB_DIS ) {
		scb_control &= ~SCB_DIS;
		outb(SCBARRAY + iobase, scb_control);
		goto done;
	}
	/*
	 * Case 3: Currently active command
	 */
	if ( (flags = inb(HA_FLAGS + iobase)) & ACTIVE_MSG) {
		/* 
		 * If there's a message in progress, 
		 * reset the bus and have all devices renegotiate.
		 */
		if(scb->target_channel_lun & 0x08){
			ahc->needsdtr |= (ahc->needsdtr_orig & 0xff00);
			ahc->sdtrpending &= 0x00ff;
			outb(HA_ACTIVE1, 0);
		}
		else if (ahc->type & AHC_WIDE){
			ahc->needsdtr = ahc->needsdtr_orig;
			ahc->needwdtr = ahc->needwdtr_orig;
			ahc->sdtrpending = 0;
			ahc->wdtrpending = 0;
			outb(HA_ACTIVE0, 0);
			outb(HA_ACTIVE1, 0);
		}
		else{
			ahc->needsdtr |= (ahc->needsdtr_orig & 0x00ff);
			ahc->sdtrpending &= 0xff00;
			outb(HA_ACTIVE0, 0);
		}

		/* Reset the bus */
		outb(SCSISEQ + iobase, SCSIRSTO); 
		DELAY(1000);
		outb(SCSISEQ + iobase, 0);
		goto done;
        }        

	/* 
	 * Otherwise, set up an abort message and have the sequencer
	 * clean up
	 */
        outb(HA_FLAGS + iobase, flags | ACTIVE_MSG);
        outb(HA_MSG_LEN + iobase, 1);
        outb(HA_MSG_START + iobase, MSG_ABORT);
 
        outb(SCSISIGO + iobase, inb(HA_SIGSTATE + iobase) | 0x10);

done:
	scb->flags |= SCB_ABORTED;
	UNPAUSE_SEQUENCER(ahc);
	ahc_done(unit, scb);
	return;
}

void
ahc_timeout(void *arg1)
{               
        struct scb *scb = (struct scb *)arg1;
        int     unit;
        struct ahc_data *ahc;
        int     s = splbio();

        unit = scb->xs->sc_link->adapter_unit;
        ahc = ahcdata[unit];
        printf("ahc%d: target %d, lun %d (%s%d) timed out\n", unit
            ,scb->xs->sc_link->target
            ,scb->xs->sc_link->lun 
            ,scb->xs->sc_link->device->name
            ,scb->xs->sc_link->dev_unit);
#ifdef  AHC_DEBUG
#ifdef	SCSIDEBUG
	if (ahc_debug & AHC_SHOWCMDS) {
		show_scsi_cmd(scb->xs); 
	}
#endif
        if (ahc_debug & AHC_SHOWSCBS)
                ahc_print_active_scb(unit);
#endif /*AHC_DEBUG */

        /*
         * If it's immediate, don't try to abort it
         */
        if (scb->flags & SCB_IMMED) {
                scb->xs->retries = 0;   /* I MEAN IT ! */
                scb->flags |= SCB_IMMED_FAIL;
                ahc_done(unit, scb);
                splx(s);
                return;
        }
        /* abort the operation that has timed out */
	printf("\n");
	ahc_abort_scb( unit, ahc, scb );
        splx(s);
}

