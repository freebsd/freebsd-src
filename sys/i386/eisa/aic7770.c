/*
 * Driver for the 27/284X series adaptec SCSI controllers written by 
 * Justin T. Gibbs.  Much of this driver was taken from Julian Elischer's
 * 1742 driver, so it bears his copyright.
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
 *      $Id: aic7770.c,v 1.6 1994/11/25 22:25:15 ats Exp $
 */
/*
 * TODO:
 * 	Add support for dual and wide busses
 *	Implement Target Mode
 *	Implement Tagged Queuing
 * 	Add target reset capabilities
 *	Test the check SCSI sense code
 *	Write a message abort procedure for use in ahc_timeout
 *	Add support for the 294X series cards
 *
 *	This driver is very stable, and seems to offer performance
 *	comprable to the 1742 FreeBSD driver.  The only timeouts
 *	I have ever experienced were due to critical driver bugs
 *	where an abort wouldn't have helped me anyway. So I haven't
 *	written code to actually search the QINFIFO and/or kill an
 *	active command.  Same goes for target reset.
 */

#define AHC_SCB_MAX	16	/* 
				 * Up to 16 SCBs on some types of aic7xxx based 
				 * boards.  The aic7770 family only have 4
				 */

#include "ahc.h"		/* for NAHC from config */

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <machine/cpufunc.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <sys/devconf.h>

#define AHC_NSEG        256     /* number of dma segments supported */
#define PAGESIZ 4096  

/* #define AHCDEBUG */

typedef unsigned long int physaddr;

#include <sys/kernel.h>
#define KVTOPHYS(x)   vtophys(x)

typedef enum {
        AHC_274,    /* Single Channel */
        AHC_274T,   /* Twin Channel */
        AHC_274W,   /* Wide Channel */
        AHC_284,    /* VL Single Channel */
        AHC_284T,   /* VL Twin Channel */
        AHC_284W,   /* VL Wide Channel - Do these exist?? */
}ahc_type;

int     ahcprobe();
int     ahcprobe1 __P((struct isa_device *dev, ahc_type type));
int     ahc_attach();
int     ahc_init __P((int unit));
void    ahc_loadseq __P((int port));     
int     ahcintr();
int32   ahc_scsi_cmd();
timeout_t ahc_timeout;
void    ahc_done();
struct  scb *ahc_get_scb __P((int unit, int flags));
void    ahc_free_scb();
void    ahcminphys();
struct  scb *ahc_scb_phys_kv();
u_int32 ahc_adapter_info();

#define MAX_SLOTS        16     /* max slots on the EISA bus */
static  ahc_slot = 0;           /* slot last board was found in */
static  ahc_unit = 0;

/* Different debugging levels - only one so-far */
#define AHC_SHOWMISC 0x0001
int     ahc_debug = AHC_SHOWMISC;
/*
 * Standard EISA Host ID regs  (Offset from slot base)
 */

#define HID0            0xC80   /* 0,1: msb of ID2, 2-7: ID1      */
#define HID1            0xC81   /* 0-4: ID3, 5-7: LSB ID2         */
#define HID2            0xC82   /* product, 0=174[20] 1 = 1744    */
#define HID3            0xC83   /* firmware revision              */

/**** bit definitions for SCSIDEF ****/
#define	HSCSIID	0x07		/* our SCSI ID */

typedef struct 
{
  ahc_type type;
  unsigned char id; /* The Last EISA Host ID reg */
} ahc_sig;

#define CHAR1(B1,B2) (((B1>>2) & 0x1F) | '@')
#define CHAR2(B1,B2) (((B1<<3) & 0x18) | ((B2>>5) & 0x7)|'@')
#define CHAR3(B1,B2) ((B2 & 0x1F) | '@')

struct isa_driver ahcdriver = {ahcprobe, ahc_attach, "ahc"};

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

static struct kern_devconf kdc_ahc[NAHC] = { {
        0, 0, 0,                /* filled in by dev_attach */
	"ahc", 0, { MDDT_ISA, 0, "bio" },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN,
	&kdc_isa0,              /* parent */
	0,                      /* parentdata */
	DC_BUSY,                /* host adapters are always ``in use'' */
	"Adaptec aic7770 based SCSI host adapter"
} };

static inline void
ahc_registerdev(struct isa_device *id)
{
        if(id->id_unit)
		kdc_ahc[id->id_unit] = kdc_ahc[0];
	kdc_ahc[id->id_unit].kdc_unit = id->id_unit;
	kdc_ahc[id->id_unit].kdc_parentdata = id;
	dev_attach(&kdc_ahc[id->id_unit]);
}


/*
 * All of these should be in a separate header file shared by the sequencer
 * code and the kernel level driver.  The only catch is that we would need to 
 * add an additional 0xc00 offset when using them in the kernel driver.  The 
 * aic7770 assembler must be modified to allow include files as well.  All 
 * page numbers refer to the Adaptec AIC-7770 Data Book availible from 
 * Adaptec's Technical Documents Department 1-800-634-2766
 */

/* -------------------- AIC-7770 offset definitions ----------------------- */

/* 
 * SCSI Sequence Control (p. 3-11).  
 * Each bit, when set starts a specific SCSI sequence on the bus
 */
#define SCSISEQ			0xc00
#define		TEMODEO		0x80
#define		ENSELO		0x40
#define		ENSELI		0x20
#define		ENRSELI		0x10
#define		ENAUTOATNO	0x08
#define		ENAUTOATNI	0x04
#define		ENAUTOATNP	0x02
#define		SCSIRSTO	0x01

/*
 * SCSI Control Signal Read Register (p. 3-15). 
 * Reads the actual state of the SCSI bus pins
 */
#define SCSISIGI		0xc03
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
#define SCSISIGO		0xc03
#define		CDO		0x80
#define		IOO		0x40
#define		MSGO		0x20
#define		ATNO		0x10
#define		SELO		0x08
#define		BSYO		0x04
#define		REQO		0x02
#define		ACKO		0x01

/*
 * SCSI ID (p. 3-18).
 * Contains the ID of the board and the current target on the
 * selected channel
 */
#define SCSIID			0xc05
#define		TID		0xf0		/* Target ID mask */
#define		OID		0x0f		/* Our ID mask */

/*
 * SCSI Status 0 (p. 3-21)
 * Contains one set of SCSI Interrupt codes
 * These are most likely of interest to the sequencer
 */
#define SSTAT0			0xc0b
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
#define CLRSINT1		0xc0c
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
#define SSTAT1			0xc0c
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
#define SELID			0xc19
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
#define SBLKCTL			0xc1f
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
#define SEQCTL			0xc60
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
#define SEQRAM			0xc61

/*
 * Sequencer Address Registers (p. 3-35) 
 * Only the first bit of SEQADDR1 holds addressing information
 */
#define SEQADDR0		0xc62
#define SEQADDR1		0xc63
#define 	SEQADDR1_MASK	0x01

/*
 * Accumulator
 * We cheat by passing arguments in the Accumulator up to the kernel driver
 */
#define ACCUM			0xc64

/*
 * Board Control (p. 3-43)
 */
#define BCTL			0xc84
/*   RSVD			0xf0 */
#define		ACE		0x08	/* Support for external processors */
/*   RSVD			0x06 */
#define		ENABLE		0x01

/*
 * Host Control (p. 3-47) R/W
 * Overal host control of the device.  
 */
#define HCNTRL			0xc87
/*    UNUSED			0x80 */
#define		POWRDN		0x40
/*    UNUSED			0x20 */
#define		SWINT		0x10
#define		IRQMS		0x08
#define		PAUSE		0x04
#define		INTEN		0x02
#define		CHIPRST		0x01
#define		REQ_PAUSE	IRQMS | PAUSE | INTEN
#define		UNPAUSE_274X	IRQMS | INTEN
#define		UNPAUSE_284X	INTEN

/*
 * SCB Pointer (p. 3-49)
 * Gate one of the four SCBs into the SCBARRAY window.
 */
#define SCBPTR			0xc90

/*
 * Interrupt Status (p. 3-50)
 * Status for system interrupts
 */
#define INTSTAT			0xc91		
#define		SEQINT_MASK	0xf0		/* SEQINT Status Codes */
#define			BAD_PHASE	0x00
#define			MSG_REJECT	0x10
#define			NO_IDENT	0x20
#define			NO_MATCH	0x30
#define			TRANS_RATE	0x40
#define			BAD_STATUS	0x50
#define 	BRKADRINT 0x08
#define		SCSIINT	  0x04
#define		CMDCMPLT  0x02
#define		SEQINT    0x01
#define		INT_PEND  SEQINT | SCSIINT | CMDCMPLT  /* For polling */

/*
 * Hard Error (p. 3-53)
 * Reporting of catastrophic errors.  You usually cannot recover from 
 * these without a full board reset.
 */
#define ERROR			0xc92
/*    UNUSED			0xf0 */
#define		PARERR		0x08
#define		ILLOPCODE	0x04
#define		ILLSADDR	0x02
#define		ILLHADDR	0x01

/*
 * Clear Interrupt Status (p. 3-52)
 */
#define CLRINT			0xc92
#define		CLRBRKADRINT	0x08
#define		CLRINTSTAT     0x04   /* UNDOCUMENTED - must be unpaused */
#define		CLRCMDINT 	0x02
#define		CLRSEQINT 	0x01

/*
 * SCB Auto Increment (p. 3-59)
 * Byte offset into the SCB Array and an optional bit to allow auto 
 * incrementing of the address during download and upload operations
 */
#define SCBCNT			0xc9a
#define		SCBAUTO		0x80
#define		SCBCNT_MASK	0x1f

/*
 * Queue In FIFO (p. 3-60)
 * Input queue for queued SCBs (commands that the seqencer has yet to start)
 */
#define QINFIFO			0xc9b

/*
 * Queue In Count (p. 3-60)
 * Number of queued SCBs
 */
#define QINCNT			0xc9c

/*
 * Queue Out FIFO (p. 3-61)
 * Queue of SCBs that have completed and await the host
 */
#define QOUTFIFO		0xc9d

/*
 * Queue Out Count (p. 3-61)
 * Number of queued SCBs in the Out FIFO
 */
#define QOUTCNT			0xc9e

#define SCBARRAY		0xca0

/* ---------------- END AIC-7770 Register Definitions ----------------- */

/* ---------------------- Scratch RAM Offsets ------------------------- */
/* These offsets are either to values that are initialized by the board's
 * BIOS or are specified by the Linux sequencer code.  If I can figure out
 * how to read the EISA configuration info at probe time, the cards could
 * be run without BIOS support installed
 */

/*
 * The sequencer will stick the frist byte of any rejected message here so
 * we can see what is getting thrown away.
 */
#define HA_REJBYTE		0xc31

/*
 * Pending message flag
 */
#define HA_MSG_FLAGS		0xc35

/*
 * Length of pending message
 */
#define HA_MSG_LEN		0xc36

/*
 * message body
 */
#define HA_MSG_START		0xc37	/* outgoing message body */

/*
 * These are offsets into the card's scratch ram.  Some of the values are
 * specified in the AHA2742 technical reference manual and are initialized 
 * by the BIOS at boot time.
 */
#define HA_ARG_1		0xc4c
#define HA_ARG_2		0xc4d
#define HA_RETURN_1		0xc4c

#define HA_SIGSTATE		0xc4e
#define HA_NEEDSDTR		0xc4f

#define HA_SCSICONF		0xc5a
#define INTDEF			0xc5c
#define HA_HOSTCONF		0xc5d
#define HA_SCBCOUNT		0xc56
#define ACTIVE_A		0xc57
#define MSG_ABORT               0x06

/*
 * Since the sequencer can disable pausing in a critical section, we
 * must loop until it actually stops. 
 * XXX Should add a timeout in here!!
 */     
#define PAUSE_SEQUENCER(ahc)      \
        outb(HCNTRL + ahc->baseport, REQ_PAUSE);   \
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
                        outb( SEQCTL + ahc->baseport, SEQRESET );     \
                } while (inw(SEQADDR0 + ahc->baseport) != 0);     \
                                                        \
                UNPAUSE_SEQUENCER(ahc);                   


struct ahc_dma_seg {  
        physaddr addr;
            long len;  
};

/*
 * The driver keeps up to four scb structures per card in memory.  Only the
 * first 26 bytes of the structure are valid for the hardware, the rest used
 * for driver level bookeeping.  The "__attribute ((packed))" tags ensure that
 * gcc does not attempt to pad the long ints in the structure to word 
 * boundaries since the first 26 bytes of this structure must have the correct
 * offsets for the hardware to find them.  The driver should be further 
 * optimized so that we only have to download the first 14 bytes since as long 
 * as we always use S/G, the last fields should be zero anyway.  Its mostly a 
 * matter of looking through the sequencer code and ensuring that those fields 
 * are cleared or loaded with a valid value before being read.
 */
struct scb {
/* ------------    Begin hardware supported fields    ---------------- */
/*1*/	u_char control;
#define SCB_REJ_MDP 0x80		       /* Reject MDP message */
#define SCB_DCE 0x40				/* Disconnect enable */ 
#define SCB_TE 0x20                             /* Tag enable */
#define SCB_WAITING 0x06
#define SCB_DIS 0x04
#define SCB_TAG_TYPE 0x3
#define      SIMPLE_QUEUE 0x0
#define      HEAD_QUEUE 0x1
#define      OR_QUEUE 0x2
/*2*/	u_char target_channel_lun;		/* 4/1/3 bits */
/*3*/	u_char SG_segment_count;
/*7*/	physaddr SG_list_pointer	__attribute__ ((packed));
/*11*/	physaddr cmdpointer		__attribute__ ((packed));
/*12*/	u_char cmdlen;
/*14*/	u_char RESERVED[2];			/* must be zero */
/*15*/	u_char target_status;
/*18*/	u_char residual_data_count[3];
/*19*/	u_char residual_SG_segment_count;
/*23*/	physaddr data			__attribute__ ((packed));
/*26*/	u_char datalen[3];
#define SCB_SIZE 26			/* amount to actually download */
#if 0
	/*
	 *  No real point in transferring this to the
	 *  SCB registers.
	 */
	unsigned char RESERVED[6];
#endif
        /*-----------------end of hardware supported fields----------------*/
	struct scb *next;       /* in free list */
        struct scsi_xfer *xs;   /* the scsi_xfer for this cmd */
        int     flags;
	int	position;	/* Position in scbarray */
#define SCB_FREE        0
#define SCB_ACTIVE      1
#define SCB_ABORTED     2
#define SCB_IMMED       4
#define SCB_IMMED_FAIL  8
#define SCB_SENSE	16
        struct ahc_dma_seg ahc_dma[AHC_NSEG] __attribute__ ((packed));
        struct scsi_sense sense_cmd;  /* SCSI command block */
};

struct ahc_data {
        ahc_type type;
        int      flags; 
#define AHC_INIT        0x01;
        int      baseport;
        struct   scb *scbarray[AHC_SCB_MAX]; /* Mirror boards scbarray */
        struct   scb *free_scb;
        int      our_id;                /* our scsi id */
        int      vect;
        struct   scb *immed_ecb;        /* an outstanding immediete command */
        struct   scsi_link sc_link;
        int      numscbs;
	u_char	 maxscbs;
        int      unpause;
}      *ahcdata[NAHC];

#ifdef  AHCDEBUG
void
ahc_print_scb(scb)
        struct scb *scb;
{
        printf("scb:%x control:%x tcl:%x cmdlen:%d cmdpointer:%x\n"
            ,scb
	    ,scb->control
	    ,scb->target_channel_lun
            ,scb->cmdlen
            ,scb->cmdpointer );    
        printf("        datlen:%d data:%x res:%x segs:%x segp:%x\n"
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
	int port = ahc->baseport;
        PAUSE_SEQUENCER(ahc);
        cur_scb_offset = inb(SCBPTR + port);
	UNPAUSE_SEQUENCER(ahc);
	ahc_print_scb(ahc->scbarray[cur_scb_offset]);
}

#define         PARERR          0x08
#define         ILLOPCODE       0x04
#define         ILLSADDR        0x02
#define         ILLHADDR        0x01

#endif

static struct {
        u_char errno;            
	char *errmesg;
} hard_error[] = {
	ILLHADDR,  "Illegal Host Access",
	ILLSADDR,  "Illegal Sequencer Address referrenced",
	ILLOPCODE, "Illegal Opcode in sequencer program", 
	PARERR,    "Sequencer Ram Parity Error",
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
	0x00,	100,	"10.0",
	0x10,	125,	"8.0",
	0x20,	150,	"6.67",
	0x30,	175,	"5.7",
	0x40,	200,	"5.0",
	0x50,	225,	"4.4",
	0x60,	250,	"4.0",
	0x70,	275,	"3.6"
};

static int ahc_num_syncrates =
	sizeof(ahc_syncrates) / sizeof(ahc_syncrates[0]);

int
ahcprobe(struct isa_device *dev)
{       
        int     port;
	int	i;
        u_char  sig_id[4];

	ahc_sig valid_ids[] = {
	/* Entries of other tested adaptors should be added here */
		  AHC_274, 0x71,  /*274x, Card*/
		  AHC_274, 0x70,  /*274x, Motherboard*/
		  AHC_284, 0x56,  /*284x, BIOS enabled*/
		  AHC_284, 0x57,  /*284x, BIOS disabled*/
	};


        ahc_slot++;
        while (ahc_slot <= MAX_SLOTS) {
                port = 0x1000 * ahc_slot;
		for( i = 0; i < sizeof(sig_id); i++ )
		{
		       /* 
			* An outb is required to prime these registers on
			* VL cards
			*/
		        outb( port + HID0, HID0 + i );
                        sig_id[i] = inb(port + HID0 + i);
		}
                if (sig_id[0] == 0xff) {
                        ahc_slot++;
                        continue;
                }
		/* Check manufacturer's ID. */
		if ((CHAR1(sig_id[0], sig_id[1]) == 'A')
		    && (CHAR2(sig_id[0], sig_id[1]) == 'D')
		    && (CHAR3(sig_id[0], sig_id[1]) == 'P')
		    && (sig_id[2] == 0x77)) {
			for( i = 0; i < sizeof(valid_ids)/sizeof(ahc_sig); i++)
                        	if ( sig_id[3] == valid_ids[i].id ) {
		                        dev->id_iobase = port;
               			        return ahcprobe1(dev, valid_ids[i].type); 
                   		}
		}
                ahc_slot++;
        }
        return 0;
}

/*
 * Check if the device can be found at the port given
 * and if so, determine configuration and set it up for further work.
 * As an argument, takes the isa_device structure from
 * autoconf.c.
 */

int
ahcprobe1(dev, type)
        struct isa_device *dev;
	ahc_type type;
{

        /*
         * find unit and check we have that many defined
         */

        int     unit = dev->id_unit;
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
        ahc->baseport = dev->id_iobase;
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

        /*
         * If it's there, put in it's interrupt vectors
         */

        dev->id_irq = (1 << ahc->vect);
        dev->id_drq = -1;       /* use EISA dma */

        ahc_unit++;
        return IO_EISASIZE;
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
#ifdef AHCDEBUG
		        printf("ahc%d: target %d synchronous at %sMb/s\n",
				unit, target, ahc_syncrates[i].rate );
#endif /* AHCDEBUG */
                        return;
                }
        }
	/* Default to asyncronous transfer */
        *scsirate = 0;
#ifdef AHCDEBUG
	printf("ahc%d: target %d using asyncronous transfers\n",
		unit, target );
#endif /* AHCDEBUG */

}


/*
 * Attach all the sub-devices we can find
 */     
int
ahc_attach(dev)
        struct isa_device *dev;
{
        int     unit = dev->id_unit;
        struct ahc_data *ahc = ahcdata[unit]; 
    
        /*
         * fill in the prototype scsi_link.
         */
        ahc->sc_link.adapter_unit = unit;
        ahc->sc_link.adapter_targ = ahc->our_id;
        ahc->sc_link.adapter = &ahc_switch;
        ahc->sc_link.device = &ahc_dev;
	ahc->sc_link.flags = DEBUGLEVEL;

	/* 
	 * Here, we should really fill in up to two different sc_links,
	 * making use of the extra fields in the sc_link structure so 
	 * we can know which channel any requests are for.  Then its just
	 * a matter of doing a scsi_attachdevs to both instead of the one.
	 * This should be done when we get or write sequencer code that 
	 * supports more than one channel. XXX
	 */

        ahc_registerdev(dev);

        /*
         * ask the adapter what subunits are present
         */     
        scsi_attachdevs(&(ahc->sc_link)); 

        return 1;
}

void 
ahc_send_scb( ahc, scb )
        struct ahc_data *ahc;
        struct scb *scb;
{               
        int old_scbptr;
        int base = ahc->baseport;
         
        PAUSE_SEQUENCER(ahc);
        
        old_scbptr = inb(SCBPTR + base);
        outb(SCBPTR + base, scb->position);
                        
        outb(SCBCNT + base, SCBAUTO); 

	outsb(SCBARRAY + base, scb, SCB_SIZE);

        outb(SCBCNT + base, 0);

        outb(QINFIFO + base, scb->position);
        outb(SCBPTR + base, old_scbptr);

        UNPAUSE_SEQUENCER(ahc);
}

static  
void ahc_getscb(base, scb)
	int base;
	struct scb *scb;
{             
        outb(SCBCNT + base, 0x80);     /* SCBAUTO */
                
	insb(SCBARRAY + base, scb, SCB_SIZE);
                        
        outb(SCBCNT + base, 0);
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
        int	port = ahc->baseport;
	struct scb *scb = NULL;
	struct scsi_xfer *xs = NULL; 

        intstat = inb(INTSTAT + port);
 
        if (intstat & BRKADRINT) { 
		/* We upset the sequencer :-( */

		/* Lookup the error message */
		int i, error = inb(ERROR + port);
		int num_errors =  sizeof(hard_error)/sizeof(hard_error[0]);
		for(i = 0; error != 1 && i < num_errors; i++)
			error >>= 1;
                panic("ahc%d: brkadrint, %s at seqaddr = 0x%x\n",
		      unit, hard_error[i].errmesg, inw(SEQADDR0 + port));
        }
        if (intstat & SEQINT) { 
                unsigned char transfer, offset, rate;

                switch (intstat & SEQINT_MASK) {
                    case BAD_PHASE:
                        panic("ahc%d: unknown scsi bus phase.  "
			      "Attempting to continue\n", unit);  
                        break; 
                    case MSG_REJECT: 
                        printf("ahc%d: Warning - " 
                              "message reject, message type: 0x%x\n", unit,
                              inb(HA_REJBYTE + port));
                        break; 
                    case NO_IDENT: 
                        panic("ahc%d: No IDENTIFY message from reconnecting "
			      "target %d\n",
                              unit, (inb(SELID + port) >> 4) & 0xf);
			break;
                    case NO_MATCH:
			{
				u_char active;
				int target = (inb(SELID + port) >> 4) & 0x4;
				printf("ahc%d: no active SCB for reconnecting "
				       "target %d - issuing ABORT\n",
					unit, target);
				active = inb(HA_SCBCOUNT + port);
				DELAY(10000);
				active = inb(ACTIVE_A + port);
				active &= ~(0x01 << target);
				outb(ACTIVE_A + port, active);
				outb(CLRSINT1 + port, CLRSELTIMEO);
	                        RESTART_SEQUENCER(ahc);
                        	break;
			}
                    case TRANS_RATE:
                        /* 
			 * Help the sequencer to translate the negotiated
                         * transfer rate.  Transfer is 1/4 the period
			 * in ns as is returned by the sync negotiation
			 * message.  So, we must multiply by four
                         */
                        transfer = inb(HA_ARG_1 + port) << 2;
			/* The bottom half of SCSIXFER*/
                        offset = inb(HA_ARG_2 + port);
                        ahc_scsirate(&rate, transfer, offset, unit,
				inb(SCSIID + port) >> 0x4);
                        outb(HA_RETURN_1 + port, rate);
                        break;
                    case BAD_STATUS:   
			{
			  int	scb_index, saved_scb_index;
			
                          /* The sequencer will notify us when a command
                           * has an error that would be of interest to
                           * the kernel.  This allows us to leave the sequencer
                           * running in the common case of command completes
                           * without error.
                           */

  			  scb_index = inb(SCBPTR + port);
                          scb = ahc->scbarray[scb_index];
		 	  if (!scb || !(scb->flags & SCB_ACTIVE)) {
                              printf("ahc%d: ahcintr - referenced scb not "
				   "valid during seqint 0x%x scb(%d)\n", 
				   unit, intstat, scb_index);
			      goto clear;
			  }

			  xs = scb->xs;

                          ahc_getscb(port, scb);

#ifdef AHCDEBUG
                          if(xs->sc_link->target == DEBUGTARG)
                              ahc_print_scb(scb);
#endif
                          xs->status = scb->target_status;
                          xs->resid = ((scb->residual_data_count[2] << 16) |
                                    (scb->residual_data_count[1] << 8)  |
                                    scb->residual_data_count[0]);
                          switch(scb->target_status){
                                case SCSI_OK:
				    printf("ahc%d: Interrupted for staus of "
					   "0???\n", unit);
                                    break;
                                case SCSI_CHECK:
#ifdef AHCDEBUG
				    printf("ahc%d: SCSI Check requested\n", unit);
#endif
				   /*Priliminary code for requesting Sense */
				   /* Enable at your own risk */
#if STILL_NEEDS_TESTING
				    if((xs->error == XS_NOERROR) && 
					 !(scb->flags & SCB_SENSE))
				    {
					struct ahc_dma_seg *sg = scb->ahc_dma;
					struct scsi_sense *sc = &(scb->sense_cmd);
					int scbsave[AHC_SCB_MAX], i;
					int queued = inb(QINCNT + port);
#ifdef AHCDEBUG
					printf("SENDING SENSE.\n");
#endif
					bzero(scb, SCB_SIZE);
					scb->flags |= SCB_SENSE;
					xs->error = XS_SENSE;
					sc->op_code = REQUEST_SENSE;
					sc->byte2 =  xs->sc_link->lun << 5;
					sc->length = sizeof(struct scsi_sense_data);
					scb->cmdlen = sizeof(*sc);
					scb->cmdpointer = KVTOPHYS(sc);
					scb->SG_segment_count = 1;
					scb->SG_list_pointer = KVTOPHYS(sg);
					sg->addr = KVTOPHYS(&xs->sense);
					sg->len = sizeof(struct scsi_sense_data);
					/*
					 * Reinsert us at head of
					 * queue
					 */
				        outb(SCBCNT + port, 0x80); 
				        outsb(SCBARRAY + port, scb, SCB_SIZE);
				        outb(SCBCNT + port, 0);

				        for (i = 0; i < queued; i++) 
				            scbsave[i] = inb(QINFIFO + port);
					
					outb(QINFIFO + port, scb->position);
 
        				for (i = 0; i < queued; i++)
			                    outb(QINFIFO + port, scbsave[i]);

					/* New lease on life */
	                                untimeout(ahc_timeout, (caddr_t)scb);
					timeout(ahc_timeout, (caddr_t)scb, 
					        (xs->timeout * hz) / 1000);
					
					goto clear;
				    }
#endif
				    xs->error = XS_DRIVER_STUFFUP;
                                    break;
                                case SCSI_BUSY:
                                    xs->error = XS_BUSY;
				    printf("ahc%d: Target Busy\n", unit);
                                    break;
                                default:
#ifdef  AHCDEBUG              
                                    if (ahc_debug & AHC_SHOWMISC) 
			     	    {
                                        printf("unexpected targ_status: %x\n",
                                            scb->target_status);
                                     }
#endif /*AHCDEBUG */
                                    xs->error = XS_DRIVER_STUFFUP;
                                    break;
                          }
                          untimeout(ahc_timeout, (caddr_t)scb);
                          ahc_done(unit, scb);
		  	  break;
		      }
                    default:  
                        printf("ahc: seqint, "
                              "intstat = 0x%x, scsisigi = 0x%x\n",
                              intstat, inb(SCSISIGI + port));
                        break;
                }             
                              
                /*            
                 * Clear the upper byte that holds SEQINT status
                 * codes and clear the SEQINT bit.
                 */               
clear:                                  
                outb(CLRINT + port, CLRSEQINT);
                                 
                /*            
                 *  The sequencer is paused immediately on
                 *  a SEQINT, so we should restart it when
                 *  we leave this section. 
                 */                        
                 UNPAUSE_SEQUENCER(ahc);
           }                


        if (intstat & SCSIINT) { 

                int scb_index = inb(SCBPTR + port);
                status = inb(SSTAT1 + port);

                scb = ahc->scbarray[scb_index];
                if (!scb || scb->flags != SCB_ACTIVE) {
			printf("ahc%d: ahcintr - referenced scb not "
			       "valid during scsiint 0x%x scb(%d)\n",
				unit, status, scb_index);
                        outb(CLRSINT1 + port, status);
                        UNPAUSE_SEQUENCER(ahc);
                        outb(CLRINT + port, CLRINTSTAT);
			scb = NULL;
			goto cmdcomplete;
                }
		xs = scb->xs;

                if (status & SELTO) { 
			u_char active;
                        outb(SCSISEQ + port, 0);
                        xs->error = XS_TIMEOUT;
			/* 
			 * Clear any pending messages for the timed out
			 * target, and mark the target as free
			 */
                        outb(HA_MSG_FLAGS + port, 0);
			active = inb(ACTIVE_A + port);
			active &= ~(0x01 << xs->sc_link->target);
			outb(ACTIVE_A + port, active);
        
                        outb(CLRSINT1 + port, CLRSELTIMEO);
                        RESTART_SEQUENCER(ahc);
                         
                        outb(CLRINT + port, CLRINTSTAT);
                }       
                        
                if (status & SCSIPERR) { 
                        printf("ahc%d: parity error on channel A "
			       "target %d, lun %d\n",
			       unit,
                               xs->sc_link->target,
                               xs->sc_link->lun);
                        xs->error = XS_DRIVER_STUFFUP;
      
                        outb(CLRSINT1 + port, CLRSCSIPERR);
                        UNPAUSE_SEQUENCER(ahc);
                         
                        outb(CLRINT + port, CLRINTSTAT);
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
                      outb(CLRSINT1 + port, BUSFREE);    /* CLRBUSFREE */
#endif
		}

		else {
                      printf("ahc%d: Unknown SCSIINT. Status = 0x%x\n", 
			     unit, status);
                      outb(CLRSINT1 + port, status);
                      UNPAUSE_SEQUENCER(ahc);
                      outb(CLRINT + port, CLRINTSTAT);
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
                int   scb_index, saved_scb_index;
                         
                do {    
                        scb_index = inb(QOUTFIFO + port);
                	scb = ahc->scbarray[scb_index];
                        if (!scb || !(scb->flags & SCB_ACTIVE)) {
                                printf("ahc%d: WARNING "
                                       "no command for scb %d (cmdcmplt)\n",
					unit, scb_index);
                        	outb(CLRINT + port, CLRCMDINT);      
                                continue;
                        }  
                               
                        outb(CLRINT+ port, CLRCMDINT);      
                        untimeout(ahc_timeout, (caddr_t)scb);
                        ahc_done(unit, scb);
                        
                } while (inb(QOUTCNT + port));
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
        if ((xs->flags & SCSI_ERR_OK) && !(xs->error == XS_SENSE)) {  
		/* All went correctly  OR errors expected */
                xs->error = 0;
        }
        xs->flags |= ITSDONE;
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
	int     port = ahc->baseport;
	int     intdef;

	/*
	 * Assume we have a board at this stage
	 * Find out the configured interupt and the card type.
	 */

#ifdef AHCDEBUG
	printf("ahc%d: scb %d bytes; SCB_SIZE %d bytes, ahc_dma %d bytes\n", 
		unit, sizeof(struct scb), SCB_SIZE, sizeof(struct ahc_dma_seg));
#endif /* AHCDEBUG */
	printf("ahc%d: reading board settings\n", unit);

	outb(HCNTRL + port, CHIPRST);
	switch( ahc->type ) {
	   case AHC_274:
		printf("ahc%d: 274x", unit);
 		ahc->unpause = UNPAUSE_274X;
		ahc->maxscbs = 0x4;
		break;
	   case AHC_284:
		printf("ahc%d: 284x", unit);
		ahc->unpause = UNPAUSE_284X;
		ahc->maxscbs = 0x4;
		break;
	   default:
	};

        /* Determine channel configuration. */
        switch ( inb(SBLKCTL + port) ) {
            case 0:
                printf(" Single Channel, ");
                break;
            case 2:
                printf(" Wide SCSI configuration - Unsupported\n");
		ahc->type += 2;
                return(-1);
                break;
            case 8:
                printf(" Twin Channel - ignoring channel B, ");
		ahc->type += 1;
                break;
            default:
                printf(" Unsupported adapter type.  Ignoring\n");
                return(-1);
        }

	/* Number of SCBs that will be used.  Supposedly some newer rev
	 * aic7770s have more than four so maybe we can detect this in
	 * the future.
	 */
	printf("%d SCBs, ", ahc->maxscbs);

	intdef = inb(INTDEF + port);
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
	printf("int=%d, ", ahc->vect);

	/* who are we on the scsi bus? */
	ahc->our_id = (inb(HA_SCSICONF + port) & HSCSIID);
	printf("SCSI Id=%d\n", ahc->our_id);

	/*
	 * Load the Sequencer program and Enable the adapter.
	 * Place the aic7770 in fastmode which makes a big
	 * difference when doing many small block transfers.
         */
	
        printf("ahc%d: Downloading Sequencer Program\n", unit);
	ahc_loadseq(port);
        outb(SEQCTL + port, FASTMODE);
	outb(BCTL + port, ENABLE); 

	/* Reset the SCSI bus.  Is this necessary? */
        outb(SCSISEQ + port, SCSIRSTO);
        DELAY(500);
        outb(SCSISEQ + port, 0);

	/*
	 * Attempt syncronous negotiation for all targets.
	 * Clear the pending messages flag
	 */
        outb( HA_NEEDSDTR + port, 0xff );
        outb( HA_MSG_FLAGS + port, 0);
	outb(HA_SCBCOUNT + port, ahc->maxscbs);
	outb( ACTIVE_A + port, 0 );

        UNPAUSE_SEQUENCER(ahc);

	/*
	 * Note that we are going and return (to probe)
	 */
	ahc->flags |= AHC_INIT;
	return (0);
}

void
ahcminphys(bp)
        struct buf *bp; 
{
/* Even though the card can transfer up to 16megs per command
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
		/* AR: Needs Implementation */
		printf("ahc0: SCSI_RESET called.\n");
        }       
        /*
         * Put all the arguments for the xfer in the scb
         */     
        
	/* Note, Linux sequencer code does not support extra channels */
	scb->target_channel_lun = ((xs->sc_link->target << 4) & 0xF0) | 
				  xs->sc_link->lun & 0x7;
        scb->cmdlen = xs->cmdlen;
	scb->cmdpointer = KVTOPHYS(xs->cmd);
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
#ifdef AHCDEBUG
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
			printf("Abort called.  Someone implement me please!\n");
                        xs->error = XS_DRIVER_STUFFUP;
                        return (HAD_ERROR);
                } 
        } while (!(xs->flags & ITSDONE));  /* something (?) else finished */               
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

        scb->next = ahc->free_scb;
        ahc->free_scb = scb;
        scb->flags = SCB_FREE;
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

void ahc_loadseq(port)
	int port;
{
        static unsigned char seqprog[] = {
#               include <gnu/misc/aic7770/aic7770_seq.h>
        };
 
        outb(SEQCTL + port, PERRORDIS|SEQRESET|LOADRAM);

	outsb(SEQRAM + port, seqprog, sizeof(seqprog));

        outb(SEQCTL + port, 0);
        do {
		/* XXX Need a timer here? */
                outb(SEQCTL + port, SEQRESET);
                 
        } while (inw(SEQADDR0 + port) != 0); 
}

/*              
 * Function to poll for command completion when in poll mode
 */     
int 
ahc_poll(int unit, int wait)
{                               /* in msec  */
        struct ahc_data *ahc = ahcdata[unit];  
        int     port = ahc->baseport;
        int     stport = INTSTAT + port;  

      retry:
        while (--wait) {
                if (inb(stport) & INT_PEND)
                        break;
                DELAY(1000);  
        } if (wait == 0) {
                printf("ahc%d: board not responding\n", unit);
                return (EIO);
        }
        ahcintr(unit);          
        return (0);
}

void
ahc_timeout(void *arg1)
{               
        struct scb *scb = (struct scb *)arg1;
        int     unit, cur_scb_offset, port;
        struct ahc_data *ahc;
        int     s = splbio();
         
        unit = scb->xs->sc_link->adapter_unit;
        ahc = ahcdata[unit];
	port = ahc->baseport;
        printf("ahc%d: target %d, lun %d (%s%d) timed out ", unit
            ,scb->xs->sc_link->target
            ,scb->xs->sc_link->lun 
            ,scb->xs->sc_link->device->name
            ,scb->xs->sc_link->dev_unit);
#if 0
#ifdef  AHCDEBUG
        if (ahc_debug & AHC_SHOWMISC)
                ahc_print_active_scb(unit);
#endif /*AHCDEBUG */
#endif

        /*
         * If it's immediate, don't try abort it
         */
        if (scb->flags & SCB_IMMED) {
                scb->xs->retries = 0;   /* I MEAN IT ! */
                scb->flags |= SCB_IMMED_FAIL;
                ahc_done(unit, scb);
                splx(s);
                return;
        }
        /*
         * If it has been through before, then
         * a previous abort has failed, don't
         * try abort again
         */
        if (scb->flags == SCB_ABORTED) {
                /*
                 * abort timed out
                 */
                printf("AGAIN");
                scb->xs->retries = 0;   /* I MEAN IT ! */
                ahc_done(unit, scb);
        } else {                /* abort the operation that has timed out */
                printf("Abort unsupported!!!\n");
        }
        splx(s);
}

