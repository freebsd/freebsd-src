/*
 * Generic driver for the aic7xxx based adaptec SCSI controllers
 * Copyright (c) 1994, 1995 Justin T. Gibbs.
 * All rights reserved.
 *
 * Product specific probe and attach routines can be found in:
 * i386/isa/aic7770.c	27/284X and aic7770 motherboard controllers
 * /pci/aic7870.c	3940, 2940, aic7870 and aic7850 controllers
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
 *      $Id: aic7xxx.c,v 1.29.2.5 1995/09/21 02:11:16 davidg Exp $
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
#include <sys/user.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <machine/clock.h>
#include <i386/scsi/aic7xxx.h>
#include <i386/scsi/93cx6.h>

#define PAGESIZ 4096

#define MAX_TAGS 4;

#include <sys/kernel.h>
#define KVTOPHYS(x)   vtophys(x)

#define MIN(a,b) ((a < b) ? a : b)
#define ALL_TARGETS -1

struct ahc_data *ahcdata[NAHC];

int     ahc_init __P((int unit));
void    ahc_loadseq __P((u_long iobase));
int32   ahc_scsi_cmd();
timeout_t ahc_timeout;
void    ahc_done __P((int unit, struct scb *scbp));
struct  scb *ahc_get_scb __P((int unit, int flags));
void    ahc_free_scb();
void	ahc_scb_timeout __P((int unit, struct ahc_data *ahc, struct scb *scb));
u_char	ahc_abort_wscb __P((int unit, struct scb *scbp, u_char prev,
		 u_long iobase, u_char timedout_scb, u_int32 xs_error));
int	ahc_match_scb __P((struct scb *scb, int target, char channel));
int	ahc_reset_device __P((int unit, struct ahc_data *ahc, int target,
		 char channel, u_char timedout_scb, u_int32 xs_error));
void	ahc_reset_current_bus __P((u_long iobase));
int	ahc_reset_channel __P((int unit, struct ahc_data *ahc, char channel, 
		u_char timedout_scb, u_int32 xs_error));
void    ahcminphys();
void	ahc_unbusy_target __P((int target, char channel, u_long iobase));
struct  scb *ahc_scb_phys_kv();
int	ahc_poll __P((int unit, int wait));
u_int32 ahc_adapter_info();

int  ahc_unit = 0;

/* Different debugging levels */
#define AHC_SHOWMISC	0x0001
#define AHC_SHOWCMDS	0x0002
#define AHC_SHOWSCBS	0x0004
#define AHC_SHOWABORTS	0x0008
#define AHC_SHOWSENSE	0x0010
#define AHC_DEBUG 
int     ahc_debug = AHC_SHOWABORTS;

/**** bit definitions for SCSIDEF ****/
#define	HSCSIID		0x07		/* our SCSI ID */
#define HWSCSIID	0x0f		/* our SCSI ID if Wide Bus */

typedef enum {
	list_head,
	list_second,
	list_tail
}insert_t;

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
 * SCSI Transfer Control 0 Register (pp. 3-13).
 * Controls the SCSI module data path.
 */
#define	SXFRCTL0		0xc01ul
#define		DFON		0x80
#define		DFPEXP		0x40
#define		ULTRAEN		0x20
#define		CLRSTCNT	0x10
#define		SPIOEN		0x08
#define		SCAMEN		0x04
#define		CLRCHN		0x02
/*  UNUSED			0x01 */

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

/* 
 * SCSI Rate Control (p. 3-17).
 * Contents of this register determine the Synchronous SCSI data transfer
 * rate and the maximum synchronous Req/Ack offset.  An offset of 0 in the
 * SOFS (3:0) bits disables synchronous data transfers.  Any offset value
 * greater than 0 enables synchronous transfers.
 */
#define SCSIRATE		0xc04ul
#define WIDEXFER		0x80		/* Wide transfer control */
#define SXFR			0x70		/* Sync transfer rate */
#define SOFS			0x0f		/* Sync offset */

/*
 * SCSI ID (p. 3-18).
 * Contains the ID of the board and the current target on the
 * selected channel
 */
#define SCSIID			0xc05ul
#define		TID		0xf0		/* Target ID mask */
#define		OID		0x0f		/* Our ID mask */

/*
 * SCSI Transfer Count (pp. 3-19,20)
 * These registers count down the number of bytes transfered
 * across the SCSI bus.  The counter is decremented only once
 * the data has been safely transfered.  SDONE in SSTAT0 is
 * set when STCNT goes to 0
 */ 
#define STCNT			0xc08ul

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
 * SCSI/Host Address (p. 3-30)
 * These registers hold the host address for the byte about to be
 * transfered on the SCSI bus.  They are counted up in the same
 * manner as STCNT is counted down.  SHADDR should always be used
 * to determine the address of the last byte transfered since HADDR
 * can be squewed by write ahead.
 */
#define	SHADDR			0xc14ul

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
 * Bus On/Off Time (p. 3-44)
 */
#define BUSTIME			0xc85ul
#define		BOFF		0xf0
#define		BON		0x0f

/*
 * Bus Speed (p. 3-45)
 */
#define	BUSSPD			0xc86ul
#define		DFTHRSH		0xc0
#define		STBOFF		0x38
#define		STBON		0x07

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
 * Host Address (p. 3-48)
 * This register contains the address of the byte about
 * to be transfered across the host bus.
 */
#define HADDR			0xc88ul
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
#define			AWAITING_MSG	0xa0
#define			IMMEDDONE	0xb0
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

/*
 * Serial EEPROM Control (p. 4-92 in 7870 Databook)
 * Controls the reading and writing of an external serial 1-bit
 * EEPROM Device.  In order to access the serial EEPROM, you must
 * first set the SEEMS bit that generates a request to the memory
 * port for access to the serial EEPROM device.  When the memory
 * port is not busy servicing another request, it reconfigures
 * to allow access to the serial EEPROM.  When this happens, SEERDY
 * gets set high to verify that the memory port access has been
 * granted.  
 *
 * After successful arbitration for the memory port, the SEECS bit of 
 * the SEECTL register is connected to the chip select.  The SEECK, 
 * SEEDO, and SEEDI are connected to the clock, data out, and data in 
 * lines respectively.  The SEERDY bit of SEECTL is useful in that it 
 * gives us an 800 nsec timer.  After a write to the SEECTL register, 
 * the SEERDY goes high 800 nsec later.  The one exception to this is 
 * when we first request access to the memory port.  The SEERDY goes 
 * high to signify that access has been granted and, for this case, has 
 * no implied timing.
 *
 * See 93cx6.c for detailed information on the protocol necessary to 
 * read the serial EEPROM.
 */
#define SEECTL			0xc1eul
#define		EXTARBACK	0x80
#define		EXTARBREQ	0x40
#define		SEEMS		0x20
#define		SEERDY		0x10
#define		SEECS		0x08
#define		SEECK		0x04
#define		SEEDO		0x02
#define		SEEDI		0x01

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
 * Bit vector of targets that have disconnection disabled.
 */
#define	HA_DISC_DSB		0xc32ul

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
#define		SEND_SENSE	0x80
#define		SEND_WDTR	0x80
#define		SEND_SDTR	0x80
#define		SEND_REJ	0x40

#define	SG_COUNT		0xc4dul
#define	SG_NEXT			0xc4eul
#define HA_SIGSTATE		0xc4bul

#define HA_SCBCOUNT		0xc52ul
#define HA_FLAGS		0xc53ul
#define		SINGLE_BUS	0x00
#define		TWIN_BUS	0x01
#define		WIDE_BUS	0x02
#define		ACTIVE_MSG	0x20
#define		IDENTIFY_SEEN	0x40
#define		RESELECTING	0x80

#define	HA_ACTIVE0		0xc54ul
#define	HA_ACTIVE1		0xc55ul
#define	SAVED_TCL		0xc56ul
#define WAITING_SCBH		0xc57ul
#define WAITING_SCBT		0xc58ul

#define HA_SCSICONF		0xc5aul
#define INTDEF			0xc5cul
#define HA_HOSTCONF		0xc5dul

#define HA_274_BIOSCTRL		0xc5ful
#define BIOSMODE		0x30
#define BIOSDISABLED		0x30

#define MSG_ABORT		0x06
#define MSG_BUS_DEVICE_RESET	0x0c
#define	BUS_8_BIT		0x00
#define BUS_16_BIT		0x01
#define BUS_32_BIT		0x02

/*
 * Define the format of the SEEPROM registers (16 bits).
 *
 */

struct seeprom_config {

/*
 * SCSI ID Configuration Flags
 */
#define CFXFER		0x0007		/* synchronous transfer rate */
#define CFSYNCH		0x0008		/* enable synchronous transfer */
#define CFDISC		0x0010		/* enable disconnection */
#define CFWIDEB		0x0020		/* wide bus device */
/* UNUSED		0x00C0 */
#define CFSTART		0x0100		/* send start unit SCSI command */
#define CFINCBIOS	0x0200		/* include in BIOS scan */
#define CFRNFOUND	0x0400		/* report even if not found */
/* UNUSED		0xf800 */
  unsigned short device_flags[16];	/* words 0-15 */

/*
 * BIOS Control Bits
 */
#define CFSUPREM	0x0001		/* support all removeable drives */
#define CFSUPREMB	0x0002		/* support removeable drives for boot only */
#define CFBIOSEN	0x0004		/* BIOS enabled */
/* UNUSED		0x0008 */
#define CFSM2DRV	0x0010		/* support more than two drives */
/* UNUSED		0x0060 */
#define CFEXTEND	0x0080		/* extended translation enabled */
/* UNUSED		0xff00 */
  unsigned short bios_control;		/* word 16 */

/*
 * Host Adapter Control Bits
 */
/* UNUSED		0x0001 */
#define CFULTRAEN       0x0002          /* Ultra SCSI speed enable (Ultra cards) */
#define CFSTERM		0x0004		/* SCSI low byte termination (non-wide cards) */
#define CFWSTERM	0x0008		/* SCSI high byte termination (wide card) */
#define CFSPARITY	0x0010		/* SCSI parity */
/* UNUSED		0x0020 */
#define CFRESETB	0x0040		/* reset SCSI bus at IC initialization */
/* UNUSED		0xff80 */
  unsigned short adapter_control;	/* word 17 */

/*
 * Bus Release, Host Adapter ID
 */
#define CFSCSIID	0x000f		/* host adapter SCSI ID */
/* UNUSED		0x00f0 */
#define CFBRTIME	0xff00		/* bus release time */
 unsigned short brtime_id;		/* word 18 */

/*
 * Maximum targets
 */
#define CFMAXTARG	0x00ff	/* maximum targets */
/* UNUSED		0xff00 */
  unsigned short max_targets;		/* word 19 */

  unsigned short res_1[11];		/* words 20-30 */
  unsigned short checksum;		/* word 31 */

};


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
 * Check if the device can be found at the port given
 * and if so, determine configuration and set it up for further work.
 */

int
ahcprobe(unit, iobase, type, flags)
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
	ahc->flags = flags;

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
	struct ahc_data *ahc = ahcdata[unit];

        for (i = 0; i < ahc_num_syncrates; i++) {

                if ((ahc_syncrates[i].period - period) >= 0) {
			/*
			 * Watch out for Ultra speeds when ultra is not
			 * enabled and vice-versa.
			 */
			if (ahc->type & AHC_ULTRA) {
				if (!(ahc_syncrates[i].sxfr & ULTRA_SXFR)) {
					printf("ahc%d: target %d requests "
					       "%sMB/s transfers, but adapter "
					       "in Ultra mode can only sync at "
					       "10MB/s or above\n", unit, 
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
				printf("ahc%d: target %d synchronous at %sMB/s,"
				       " offset = 0x%x\n", unit, target,
					ahc_syncrates[i].rate, offset );
			}
                        return;
                }
        }
	/* Default to asyncronous transfers.  Also reject this SDTR request. */
	*scsirate = 0;
	if(bootverbose) {
		printf("ahc%d: target %d using asyncronous transfers\n",
			unit, target );
	}
}


/*
 * Attach all the sub-devices we can find
 */
int
ahc_attach(unit)
	int unit;
{
	struct ahc_data *ahc = ahcdata[unit];
	struct scsibus_data *scbus;

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
		printf("ahc%d: Probing channel A\n", unit);
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
			printf("ahc%d: Probing Channel B\n", unit);
		scsi_attachdevs(scbus);
		scbus = NULL;	/* Upper-level SCSI code owns this now */
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
	tail = inb(WAITING_SCBT + iobase);
	if(head == SCB_LIST_NULL) {
		/* List was empty */
		head = scb->position;
		tail = SCB_LIST_NULL;
	}
	else if (where == list_head) {
		outb(SCBPTR+iobase, scb->position);
		outb(SCBARRAY+iobase+30, head);
		head = scb->position;
	}
	else if(tail == SCB_LIST_NULL) {
		/* List had one element */
		tail = scb->position;
		outb(SCBPTR+iobase,head);
		outb(SCBARRAY+iobase+30,
		     tail);
	}
	else if(where == list_second) {
		u_char third_scb;
		outb(SCBPTR+iobase, head);
		third_scb = inb(SCBARRAY+iobase+30);
		outb(SCBARRAY+iobase+30,scb->position);
		outb(SCBPTR+iobase, scb->position);
		outb(SCBARRAY+iobase+30,third_scb);
	}
	else {
		outb(SCBPTR+iobase,tail);
		tail = scb->position;
		outb(SCBARRAY+iobase+30,
		     tail);
	}
	outb(WAITING_SCBH + iobase, head);
	outb(WAITING_SCBT + iobase, tail);
	outb(SCBPTR + iobase, curscb);
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
			      unit, channel, target);  
                        break; 
                    case SEND_REJECT: 
			{
				u_char rejbyte = inb(HA_REJBYTE + iobase);
				if(( rejbyte & 0xf0) == 0x20) {
					/* Tagged Message */
					printf("\nahc%d:%c:%d: Tagged message "
						"rejected.  Disabling tagged "
						"commands for this target.\n", 
						unit, channel, target);
					ahc->tagenable &= ~targ_mask;
				}
				else
					printf("ahc%d:%c:%d: Warning - message "
						"rejected by target: 0x%x\n", 
						unit, channel, target, rejbyte);
				break; 
			}
                    case NO_IDENT: 
                        panic("ahc%d:%c:%d: Target did not send an IDENTIFY "
			      "message. SAVED_TCL == 0x%x\n",
                              unit, channel, target,
			      inb(SAVED_TCL + iobase));
			break;
                    case NO_MATCH:
			{
				printf("ahc%d:%c:%d: no active SCB for "
				       "reconnecting target - "
				       "issuing ABORT\n", unit, channel, 
				       target);
				printf("SAVED_TCL == 0x%x\n",
					inb(SAVED_TCL + iobase));
				ahc_unbusy_target(target, channel, iobase);
				outb(SCBARRAY + iobase, SCB_NEEDDMA);
				outb(CLRSINT1 + iobase, CLRSELTIMEO);
	                        RESTART_SEQUENCER(ahc);
                        	break;
			}
                    case MSG_SDTR:
			{
				u_char period, offset, rate;
				u_char targ_scratch;
				u_char maxoffset;
	                        /* 
				 * Help the sequencer to translate the 
				 * negotiated transfer rate.  Transfer is 
				 * 1/4 the period in ns as is returned by 
				 * the sync negotiation message.  So, we must 
				 * multiply by four
				 */
	                        period = inb(HA_ARG_1 + iobase) << 2;
				offset = inb(ACCUM + iobase);
				targ_scratch = inb(HA_TARG_SCRATCH + iobase 
						   + scratch_offset);
				if(targ_scratch & WIDEXFER)
					maxoffset = 0x08;
				else
					maxoffset = 0x0f;
				ahc_scsirate(&rate, period, 
					MIN(offset,maxoffset), unit, target);
				/* Preserve the WideXfer flag */
				targ_scratch = rate | (targ_scratch & WIDEXFER);
				outb(HA_TARG_SCRATCH + iobase + scratch_offset,
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
					outb(HA_RETURN_1 + iobase, SEND_REJ);
				}
				/* See if we initiated Sync Negotiation */
				else if(ahc->sdtrpending & targ_mask)
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
				ahc->needsdtr &= ~targ_mask;
				ahc->sdtrpending &= ~targ_mask;
	                        break;
			}
                    case MSG_WDTR:
			{
				u_char scratch, bus_width;

				bus_width = inb(ACCUM + iobase);

				scratch = inb(HA_TARG_SCRATCH + iobase 
					      + scratch_offset);

				if(ahc->wdtrpending & targ_mask)
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
						    if(bootverbose)
		        				printf("ahc%d: target "
							       "%d using 16Bit "
							       "transfers\n",
								unit, target);
						    scratch |= 0x80;	
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
								unit, target);
						    scratch |= 0x80;	
						    break;
						default:
						    break;
					}
					outb(HA_RETURN_1 + iobase,
						bus_width | SEND_WDTR);
				}
				ahc->needwdtr &= ~targ_mask;
				ahc->wdtrpending &= ~targ_mask;
				outb(HA_TARG_SCRATCH + iobase + scratch_offset, 
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

				targ_scratch = inb(HA_TARG_SCRATCH + iobase
						   + scratch_offset);

				if(ahc->wdtrpending & targ_mask){
					/* note 8bit xfers and clear flag */
					targ_scratch &= 0x7f;
					ahc->needwdtr &= ~targ_mask;
					ahc->wdtrpending &= ~targ_mask;
        				printf("ahc%d:%c:%d: refuses "
					       "WIDE negotiation.  Using "
					       "8bit transfers\n",
						unit, channel, target);
				}
				else if(ahc->sdtrpending & targ_mask){
					/* note asynch xfers and clear flag */
					targ_scratch &= 0xf0;
					ahc->needsdtr &= ~targ_mask;
					ahc->sdtrpending &= ~targ_mask;
        				printf("ahc%d:%c:%d: refuses "
					       "syncronous negotiation.  Using "
					       "asyncronous transfers\n",
						unit, channel, target);
				}
				else {
					/*
					 * Otherwise, we ignore it.
					 */
#ifdef AHC_DEBUG
					if(ahc_debug & AHC_SHOWMISC)
						printf("ahc%d:%c:%d: Message 
							reject -- ignored\n",
							unit, channel, target);
#endif
					break;
				}
				outb(HA_TARG_SCRATCH + iobase + scratch_offset,
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
			   * send sense).  The sense code with change
			   * this if needed and this reduces code
			   * duplication.
			   */
			  outb(HA_RETURN_1 + iobase, 0);
		 	  if (!scb || !(scb->flags & SCB_ACTIVE)) {
                              printf("ahc%d:%c:%d: ahcintr - referenced scb "
				     "not valid during seqint 0x%x scb(%d)\n", 
				     unit, channel, target, intstat, scb_index);
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
					" 0???\n", unit);
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
					bzero(scb, SCB_DOWN_SIZE);
					scb->control |= control & SCB_DISCENB;
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
					scb->datalen[0] = 
						sg->len & 0xff;
					scb->datalen[1] = 
						(sg->len >> 8) & 0xff;
					scb->datalen[2] = 
						(sg->len >> 16) & 0xff;
					outb(SCBCNT + iobase, 0x80);
					outsb(SCBARRAY+iobase,scb,SCB_DOWN_SIZE);
					outb(SCBCNT + iobase, 0);
					outb(SCBARRAY+iobase+30,SCB_LIST_NULL);
					/*
					 * Ensure that the target is "BUSY"
					 * so we don't get overlapping 
					 * commands if we happen to be doing
					 * tagged I/O.
					 */
					active = inb(HA_ACTIVE0 + iobase)
					  | (inb(HA_ACTIVE1 + iobase) << 8);
					active |= targ_mask;
					outb(HA_ACTIVE0 + iobase,active & 0xff);
					outb(HA_ACTIVE1 + iobase, 
						(active >> 8) & 0xff);
					ahc_add_waiting_scb(iobase, scb, 
							    list_head);
					outb(HA_RETURN_1 + iobase, SEND_SENSE);
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
			    scb->xs->resid = (inb(iobase+SCBARRAY+17) << 16) |
					     (inb(iobase+SCBARRAY+16) << 8) |
					      inb(iobase+SCBARRAY+15);
				xs->flags |= SCSI_RESID_VALID;
#ifdef AHC_DEBUG
				if(ahc_debug & AHC_SHOWMISC) {
					sc_print_addr(xs->sc_link);
					printf("Handled Residual of %d bytes\n"
					       "SG_COUNT == %d\n",
						scb->xs->resid,
						inb(SCBARRAY+18 + iobase));
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
			ahc_done(unit, scb);
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
				outb(HA_MSG_START + iobase,
					MSG_BUS_DEVICE_RESET);
				outb(HA_MSG_LEN + iobase, 1);
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
				targ_scratch = inb(HA_TARG_SCRATCH + iobase 
							+ scratch_offset);
				targ_scratch &= SXFR;
				outb(HA_TARG_SCRATCH + iobase + scratch_offset,
					targ_scratch);
				found = ahc_reset_device(unit, ahc, target,
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
			u_char waiting;
			u_char flags;
                        outb(SCSISEQ + iobase, ENRSELI);
                        xs->error = XS_TIMEOUT;
			/*
			 * Clear any pending messages for the timed out
			 * target, and mark the target as free
			 */
			flags = inb( HA_FLAGS + iobase );
                        outb(HA_FLAGS + iobase,
				flags & ~ACTIVE_MSG);
			ahc_unbusy_target(xs->sc_link->target,
			 	((long)xs->sc_link->fordriver & SELBUSB)
				 	? 'B' : 'A',
				 iobase);

			outb(SCBARRAY + iobase, SCB_NEEDDMA);

                        outb(CLRSINT1 + iobase, CLRSELTIMEO);

                        outb(CLRINT + iobase, CLRSCSIINT);

			/* Shift the waiting for selection queue forward */
			waiting = inb(WAITING_SCBH + iobase);
			outb(SCBPTR + iobase, waiting);
			waiting = inb(SCBARRAY + iobase + 30);
			outb(WAITING_SCBH + iobase, waiting);

                        RESTART_SEQUENCER(ahc);
                }       
                        
                else if (status & SCSIPERR) { 
			sc_print_addr(xs->sc_link);
                        printf("parity error\n");
                        xs->error = XS_DRIVER_STUFFUP;

                        outb(CLRSINT1 + iobase, CLRSCSIPERR);
                        UNPAUSE_SEQUENCER(ahc);

                        outb(CLRINT + iobase, CLRSCSIINT);
			scb = NULL;
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


int
enable_seeprom(u_long   offset,
                  u_short  CS,   /* chip select */
                  u_short  CK,   /* clock */
                  u_short  DO,   /* data out */
                  u_short  DI,   /* data in */
                  u_short  RDY,  /* ready */
                  u_short  MS    /* mode select */)
{
	int wait;
	/*
	 * Request access of the memory port.  When access is
	 * granted, SEERDY will go high.  We use a 1 second
	 * timeout which should be near 1 second more than
	 * is needed.  Reason: after the chip reset, there
	 * should be no contention.
	 */
	outb(offset, MS);
	wait = 1000;  /* 1 second timeout in msec */
	while (--wait && ((inb(offset) & RDY) == 0)) {
		DELAY (1000);  /* delay 1 msec */
        }
	if ((inb(offset) & RDY) == 0) {
		outb (offset, 0); 
		return (0);
	}         
	return(1);
}

void
release_seeprom(u_long   offset,
                  u_short  CS,   /* chip select */
                  u_short  CK,   /* clock */
                  u_short  DO,   /* data out */
                  u_short  DI,   /* data in */
                  u_short  RDY,  /* ready */
                  u_short  MS    /* mode select */)
{
	/* Release access to the memory port and the serial EEPROM. */
	outb(offset, 0);
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
#ifdef AHC_TAGENABLE
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
#endif
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
	u_char	scsi_conf, sblkctl, i, host_id;
	int     intdef, max_targ = 15, wait, have_seeprom = 0;
	int	bios_disabled = 0;
	struct seeprom_config sc;
	/*
	 * Assume we have a board at this stage
	 * Find out the configured interupt and the card type.
	 */

#ifdef AHC_DEBUG
	if(ahc_debug & AHC_SHOWMISC)
		printf("ahc%d: scb %d bytes; ahc_dma %d bytes\n",
			unit, sizeof(struct scb), sizeof(struct ahc_dma_seg));
#endif /* AHC_DEBUG */
	if(bootverbose)
		printf("ahc%d: reading board settings\n", unit);

	/* Save the IRQ type before we do a chip reset */

	ahc->unpause = (inb(HCNTRL + iobase) & IRQMS) | INTEN;
	ahc->pause = ahc->unpause | PAUSE;
	outb(HCNTRL + iobase, CHIPRST | ahc->pause);
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
		printf("ahc%d: WARNING - Failed chip reset!  "
		       "Trying to initialize anyway.\n", unit);
		/* Forcibly clear CHIPRST */
		outb(HCNTRL + iobase, ahc->pause);
	}
	switch( ahc->type ) {
	   case AHC_AIC7770:
	   case AHC_274:
	   case AHC_284:
	   {
		u_char hostconf;
		if(ahc->type == AHC_274) {
			printf("ahc%d: 274x ", unit);
			if((inb(HA_274_BIOSCTRL + iobase) & BIOSMODE)
				== BIOSDISABLED)
				bios_disabled = 1;
		}
		else if(ahc->type == AHC_284)
		        printf("ahc%d: 284x ", unit);
		else 
		        printf("ahc%d: Motherboard ", unit);
		ahc->maxscbs = 0x4;
		/* Should we only do this for the 27/284x? */
 		/* Setup the FIFO threshold and the bus off time */
 		hostconf = inb(HA_HOSTCONF + iobase);
 		outb(BUSSPD + iobase, hostconf & DFTHRSH);
 		outb(BUSTIME + iobase, (hostconf << 2) & BOFF);
		break;
	   }
	   case AHC_AIC7850:
	   case AHC_AIC7870:
	   case AHC_AIC7880:
	   case AHC_394U:
	   case AHC_394:
	   case AHC_294U:
	   case AHC_294:
		host_id = 0x07;  /* default to SCSI ID 7 for 7850 */
		if (ahc->type & AHC_AIC7870) {
			unsigned short *scarray = (u_short *)&sc;
			unsigned short  checksum = 0;

			if(bootverbose)
				printf("ahc%d: Reading SEEPROM...", unit);
			have_seeprom = enable_seeprom (iobase + SEECTL,
				SEECS, SEECK, SEEDO, SEEDI, SEERDY, SEEMS);
			if (have_seeprom) {
				have_seeprom = read_seeprom (iobase + SEECTL, 
					(u_short *)&sc, ahc->flags & AHC_CHNLB,
					sizeof(sc)/2, SEECS, SEECK, SEEDO, 
					SEEDI, SEERDY, SEEMS);
				release_seeprom (iobase + SEECTL, SEECS, SEECK,
					SEEDO, SEEDI, SEERDY, SEEMS);
				if (have_seeprom) {
					/* Check checksum */
				    for (i = 0;i < (sizeof(sc)/2 - 1);i = i + 1)
					checksum = checksum + scarray[i];
				    if (checksum != sc.checksum) {
					printf ("checksum error");
					have_seeprom = 0;
				    }
				    else {
					if(bootverbose)
						printf("done.\n");
					host_id = (sc.brtime_id & CFSCSIID);
				    }
				}
			}
			if (!have_seeprom) {
				printf("\nahc%d: SEEPROM read failed, "
					"using leftover BIOS values\n", unit);
				host_id = 0x7;
			}
		}
		ahc->maxscbs = 0x10;
		if(ahc->type == AHC_394)
			printf("ahc%d: 3940 ", unit);
		else if(ahc->type == AHC_294)
			printf("ahc%d: 2940 ", unit);
		else if(ahc->type == AHC_AIC7850){
			printf("ahc%d: aic7850 ", unit);
			ahc->maxscbs = 0x03;
		}
		else
			printf("ahc%d: aic7870 ", unit);
		outb(DSPCISTATUS + iobase, 0xc0 /* DFTHRSH == 100% */);
		/*
		 * XXX Use SCSI ID from SEEPROM if we have it; otherwise
		 * its hardcoded to 7 until we can read it from NVRAM.
		 */
		outb(HA_SCSICONF + iobase, host_id | 0xc0 /* DFTHRSH = 100% */);
		/* In case we are a wide card */
		outb(HA_SCSICONF + 1 + iobase, host_id);
		break;
	   default:
	};
	if(ahc->type & AHC_ULTRA) {
		printf("Ultra ");
		if(have_seeprom) {
			/* Should we enable Ultra mode? */
			if(!(sc.adapter_control & CFULTRAEN))
				/* Treat it like a normal card */
				ahc->type &= ~AHC_ULTRA;
		}
	}
        /* Determine channel configuration and who we are on the scsi bus. */
        switch ( (sblkctl = inb(SBLKCTL + iobase) & 0x0a) ) {
            case 0:
		ahc->our_id = (inb(HA_SCSICONF + iobase) & HSCSIID);
		if(ahc->type == AHC_394)
			printf("Channel %c, SCSI Id=%d, ", 
				ahc->flags & AHC_CHNLB ? 'B' : 'A',
				ahc->our_id);
		else
			printf("Single Channel, SCSI Id=%d, ", ahc->our_id);
		outb(HA_FLAGS + iobase, SINGLE_BUS);
                break;
            case 2:
		ahc->our_id = (inb(HA_SCSICONF + 1 + iobase) & HWSCSIID);
		if(ahc->type == AHC_394)
			printf("Wide Channel %c, SCSI Id=%d, ", 
				ahc->flags & AHC_CHNLB ? 'B' : 'A',
				ahc->our_id);
		else
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
	 * Number of SCBs that will be used. Rev E aic7770s supposedly
	 * can do 255 concurrent commands.  Right now, we just ID the
	 * card until we can find out how this is done.
	 */
	if(!(ahc->type & AHC_AIC78X0))
	{
		/*
		 * See if we have a Rev E or higher
		 * aic7770. Anything below a Rev E will
		 * have a R/O autoflush disable configuration
		 * bit.
		 */
		u_char sblkctl_orig;
		sblkctl_orig = inb(SBLKCTL + iobase);
		sblkctl = sblkctl_orig ^ AUTOFLUSHDIS;
		outb(SBLKCTL + iobase, sblkctl);
		sblkctl = inb(SBLKCTL + iobase);
		if(sblkctl != sblkctl_orig)
		{
			printf("aic7770 >= Rev E, ");
			/*
			 * Ensure autoflush is enabled
			 */
			sblkctl &= ~AUTOFLUSHDIS;
			outb(SBLKCTL + iobase, sblkctl);
		}
		else
			printf("aic7770 <= Rev C, ");
	}
	else if(ahc->type & AHC_AIC7850)
		printf("aic7850, ");
	else
		printf("aic7870, ");
	if(ahc->flags & AHC_EXTSCB) {
		/*
		 * This adapter has external SCB memory.
		 * Walk the SCBs to determine how many there are.
		 */
		for(i = 0; i < AHC_SCB_MAX; i++) {
			outb(SCBPTR + iobase, i);
			outb(SCBARRAY + iobase, 0xaa);
			if(inb(SCBARRAY + iobase) == 0xaa){
				outb(SCBARRAY + iobase, 0x55);
				if(inb(SCBARRAY + iobase) == 0x55) {
					continue;
				}
			}
			break;
		}
		ahc->maxscbs = i;		
	}
	printf("%d SCBs\n", ahc->maxscbs);

	if(!(ahc->type & AHC_AIC78X0) && bootverbose) {
		if(ahc->pause & IRQMS)
			printf("ahc%d: Using Level Sensitive Interrupts\n",
				unit);
		else
			printf("ahc%d: Using Edge Triggered Interrupts\n",
				unit);
	}
	if(!(ahc->type & AHC_AIC78X0)){
	/*
	 * The AIC78X0 cards are PCI, so we get their interrupt from the PCI
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

	/* Set the SCSI Id, SXFRCTL0, SXFRCTL1, and SIMODE1, for both channels*/
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
	scsi_conf = inb(HA_SCSICONF + iobase) & (ENSPCHK|STIMESEL);
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
	 * negotiation to that target so we don't activate the needsdr
	 * flag.
	 */
	ahc->needsdtr_orig = 0;
	ahc->needwdtr_orig = 0;

	/* Grab the disconnection disable table and invert it for our needs */
	if(have_seeprom)
		ahc->discenable = 0;
	else if(bios_disabled){
		printf("ahc%d: Host Adapter Bios disabled.  Using default SCSI "
			"device parameters\n", unit);
		ahc->discenable = 0xff;
	}
	else
		ahc->discenable = ~(inw(HA_DISC_DSB + iobase));

	if(!(ahc->type & AHC_WIDE))
		max_targ = 7;

	for(i = 0; i <= max_targ; i++){
		u_char target_settings;
		if (have_seeprom) {
			target_settings = (sc.device_flags[i] & CFXFER) << 4;
			if (sc.device_flags[i] & CFSYNCH)
				ahc->needsdtr_orig |= (0x01 << i);
			if (sc.device_flags[i] & CFWIDEB)
				ahc->needwdtr_orig |= (0x01 << i);
			if (sc.device_flags[i] & CFDISC)
				ahc->discenable |= (0x01 << i);
		}
		else if (bios_disabled) {
			target_settings = 0; /* 10MHz */
			ahc->needsdtr_orig |= (0x01 << i);
			ahc->needwdtr_orig |= (0x01 << i);
		}
		else {
			/* Take the settings leftover in scratch RAM. */
			target_settings = inb(HA_TARG_SCRATCH + i + iobase);

			if(target_settings & 0x0f){
				ahc->needsdtr_orig |= (0x01 << i);
				/*Default to a asyncronous transfers(0 offset)*/
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

	/*
	 * Clear the control byte for every SCB so that the sequencer
	 * doesn't get confused and think that one of them is valid
	 */
	for(i = 0; i < ahc->maxscbs; i++) {
		outb(SCBPTR + iobase, i);
		outb(SCBARRAY + iobase, 0);
	}

#ifdef AHC_DEBUG
	if(ahc_debug & AHC_SHOWMISC)
		printf("NEEDSDTR == 0x%x\nNEEDWDTR == 0x%x\n"
			"DISCENABLE == 0x%x\n", ahc->needsdtr, 
			ahc->needwdtr, ahc->discenable);
#endif
	/*
	 * Set the number of availible SCBs
	 */
	outb(HA_SCBCOUNT + iobase, ahc->maxscbs);

	/* We don't have any busy targets right now */
	outb( HA_ACTIVE0 + iobase, 0 );
	outb( HA_ACTIVE1 + iobase, 0 );

	/* We don't have any waiting selections */
	outb( WAITING_SCBH + iobase, SCB_LIST_NULL );
	outb( WAITING_SCBT + iobase, SCB_LIST_NULL );
	/*
	 * Load the Sequencer program and Enable the adapter.
	 * Place the aic7xxx in fastmode which makes a big
	 * difference when doing many small block transfers.
         */

	if(bootverbose)
		printf("ahc%d: Downloading Sequencer Program...", unit);
	ahc_loadseq(iobase);
	if(bootverbose)
		printf("Done\n");

        outb(SEQCTL + iobase, FASTMODE);
	if (!(ahc->type & AHC_AIC78X0))
		outb(BCTL + iobase, ENABLE);

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
        SC_DEBUG(xs->sc_link, SDEV_DB3, ("start scb(%p)\n", scb));
        scb->xs = xs;
        if (flags & SCSI_RESET)
		scb->flags |= SCB_DEVICE_RESET|SCB_IMMED;
        /*
         * Put all the arguments for the xfer in the scb
         */

	if(ahc->tagenable & mask)
		scb->control |= SCB_TE;
	if(ahc->discenable & mask)
		scb->control |= SCB_DISCENB;
	if((ahc->needwdtr & mask) && !(ahc->wdtrpending & mask))
	{
		scb->control |= SCB_NEEDWDTR;
		ahc->wdtrpending |= mask;
	}
	else if((ahc->needsdtr & mask) && !(ahc->sdtrpending & mask))
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
		scb->datalen[0] = scb->ahc_dma->len & 0xff;
		scb->datalen[1] = (scb->ahc_dma->len >> 8) & 0xff;
		scb->datalen[2] = (scb->ahc_dma->len >> 16) & 0xff;
                SC_DEBUGN(xs->sc_link, SDEV_DB4, ("\n"));
                if (datalen) { 
			/* there's still data, must have run out of segs! */
                        printf("ahc_scsi_cmd%d: more than %d DMA segs\n",
                            unit, AHC_NSEG);
                        xs->error = XS_DRIVER_STUFFUP;
                        ahc_free_scb(unit, scb, flags);
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
		scb->datalen[0] = 0;
		scb->datalen[1] = 0;
		scb->datalen[2] = 0;
	}

        /*
         * Usually return SUCCESSFULLY QUEUED
         */
#ifdef AHC_DEBUG
        if((ahc_debug & AHC_SHOWSCBS) && (xs->sc_link->target == DEBUGTARG))
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
			ahc_scb_timeout(unit,ahc,scb);
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
        unsigned int opri;
        struct ahc_data *ahc = ahcdata[unit];

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
struct scb *
ahc_get_scb(unit, flags)
        int     unit, flags;
{
        struct ahc_data *ahc = ahcdata[unit];
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
				scbp->control = SCB_NEEDDMA;
				scbp->host_scb = scbaddr;
				scbp->next_waiting = SCB_LIST_NULL;
				PAUSE_SEQUENCER(ahc);
				curscb = inb(SCBPTR + iobase);
				outb(SCBPTR + iobase, scbp->position);
				outb(SCBCNT + iobase, 0x80);
				outsb(SCBARRAY+iobase,scbp,31);
				outb(SCBCNT + iobase, 0);
				outb(SCBPTR + iobase, curscb);
				UNPAUSE_SEQUENCER(ahc);
				scbp->control = 0;
                        } else {
                                printf("ahc%d: Can't malloc SCB\n", unit);
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
		if((ahc_debug & AHC_SHOWMISC)
		  && (ahc->activescbs == ahc->maxscbs))
			printf("ahc%d: Max SCBs active\n", unit);
#endif
        }

gottit:
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
ahc_scb_timeout(unit, ahc, scb)
	int unit;
        struct ahc_data *ahc;
        struct scb *scb;
{
	u_long iobase = ahc->baseport;
	int found = 0;
	u_char scb_control;
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
		found = ahc_reset_channel(unit, ahc, channel, scb->position,
					  XS_TIMEOUT);
#ifdef AHC_DEBUG
		if(ahc_debug & AHC_SHOWABORTS) 
			printf("ahc%d: Issued Channel %c Bus Reset #1. "
				"%d SCBs aborted\n", unit, channel, found);
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
		if(inb(SCBARRAY + iobase) & SCB_DIS) {
			scb->flags |= SCB_DEVICE_RESET|SCB_ABORTED;
			scb->SG_segment_count = 0;
			scb->SG_list_pointer = 0;
			scb->data = 0;
			scb->datalen[0] = 0;
			scb->datalen[1] = 0;
			scb->datalen[2] = 0;
			outb(SCBCNT + iobase, 0x80);
			outsb(SCBARRAY+iobase,scb,SCB_DOWN_SIZE);
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
		  && (control & SCB_NEEDDMA) == SCB_NEEDDMA) {

		    u_char flags = inb(HA_FLAGS + iobase);
		    if(flags & ACTIVE_MSG) {
			/*
			 * If we're in a message phase, tacking on 
			 * another message may confuse the target totally.
			 * The bus is probably wedged, so reset the
			 * channel.
			 */
			channel = (active_scbp->target_channel_lun & SELBUSB)
					? 'B': 'A';	
			ahc_reset_channel(unit, ahc, channel, scb->position, 
					  XS_TIMEOUT);
#ifdef AHC_DEBUG
			if(ahc_debug & AHC_SHOWABORTS) 
				printf("ahc%d: Issued Channel %c Bus Reset #2. "
					"%d SCBs aborted\n", unit, channel,
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
			outb(HA_FLAGS + iobase, flags | ACTIVE_MSG);
			outb(HA_MSG_LEN + iobase, 1);
			outb(HA_MSG_START + iobase, MSG_BUS_DEVICE_RESET);
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
			ahc_reset_channel(unit, ahc, channel, scb->position,
					  XS_TIMEOUT);
#ifdef AHC_DEBUG
			if(ahc_debug & AHC_SHOWABORTS) 
				printf("ahc%d: Issued Channel %c Bus Reset #3. "
					"%d SCBs aborted\n", unit, channel,
					found);
#endif
		}
	}
}

void
ahc_timeout(void *arg1)
{
	struct scb *scb = (struct scb *)arg1;
	int     unit;
	struct ahc_data *ahc;
	int     s;
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
		ahc_done(unit, scb);
	}
	else {
		/* abort the operation that has timed out */
		ahc_scb_timeout( unit, ahc, scb );
	}
	splx(s);
}


/*
 * The device at the given target/channel has been reset.  Abort 
 * all active and queued scbs for that target/channel. 
 */
int
ahc_reset_device(unit, ahc, target, channel, timedout_scb, xs_error)
	int unit;
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
				ahc_done (unit, scbp);
				outb(SCBPTR + iobase, scbp->position);
				outb(SCBARRAY + iobase, SCB_NEEDDMA);
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
				next = ahc_abort_wscb(unit, scbp, prev,
						iobase, timedout_scb, xs_error);
				found++;
			}
			else {
				outb(SCBPTR + iobase, scbp->position);
				prev = next;
				next = inb(SCBARRAY + iobase + 30);
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
			outb(SCBARRAY + iobase, SCB_NEEDDMA);
			scbp->flags |= SCB_ABORTED;
			scbp->xs->error |= xs_error;
			if(scbp->position != timedout_scb)
				untimeout(ahc_timeout, (caddr_t)scbp);
			ahc_done (unit, scbp);
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
u_char
ahc_abort_wscb (unit, scbp, prev, iobase, timedout_scb, xs_error)
	int unit;
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
	next = inb(SCBARRAY + iobase + 30);

	/* Clear the necessary fields */
	outb(SCBARRAY + iobase, SCB_NEEDDMA);
	outb(SCBARRAY + iobase + 30, SCB_LIST_NULL);
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
		outb(SCBARRAY + iobase + 30, next);
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
	ahc_done (unit, scbp);
	return next;
}

void
ahc_unbusy_target(target, channel, iobase)
	u_char target;
	char   channel;
	u_long iobase;
{
	u_char active;
	u_long active_port = HA_ACTIVE0 + iobase;
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

void
ahc_reset_current_bus(iobase)
	u_long iobase;
{
	outb(SCSISEQ + iobase, SCSIRSTO);
	DELAY(1000);
	outb(SCSISEQ + iobase, 0);
}

int
ahc_reset_channel(unit, ahc, channel, timedout_scb, xs_error)
	int unit;
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
	found = ahc_reset_device(unit, ahc, ALL_TARGETS, channel, 
				 timedout_scb, xs_error);
	if(channel == 'B'){
		ahc->needsdtr |= (ahc->needsdtr_orig & 0xff00);
		ahc->sdtrpending &= 0x00ff;
		outb(HA_ACTIVE1 + iobase, 0);
		offset = HA_TARG_SCRATCH + iobase + 8;
		offset_max = HA_TARG_SCRATCH + iobase + 16;
	}
	else if (ahc->type & AHC_WIDE){
		ahc->needsdtr = ahc->needsdtr_orig;
		ahc->needwdtr = ahc->needwdtr_orig;
		ahc->sdtrpending = 0;
		ahc->wdtrpending = 0;
		outb(HA_ACTIVE0 + iobase, 0);
		outb(HA_ACTIVE1 + iobase, 0);
		offset = HA_TARG_SCRATCH + iobase;
		offset_max = HA_TARG_SCRATCH + iobase + 16;
	}
	else{
		ahc->needsdtr |= (ahc->needsdtr_orig & 0x00ff);
		ahc->sdtrpending &= 0xff00;
		outb(HA_ACTIVE0 + iobase, 0);
		offset = HA_TARG_SCRATCH + iobase;
		offset_max = HA_TARG_SCRATCH + iobase + 8;
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

int
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
