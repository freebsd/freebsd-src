/*
 * Copyright (c) 1994 Charles Hannum.
 * Copyright (c) 1994 Jarle Greipsland
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jarle Greipsland
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * $Id: aic6360.c,v 1.3 1996/09/03 10:23:23 asami Exp $
 *
 * Acknowledgements: Many of the algorithms used in this driver are
 * inspired by the work of Julian Elischer (julian@tfs.com) and
 * Charles Hannum (mycroft@duality.gnu.ai.mit.edu).  Thanks a million!
 *
 * Converted from NetBSD to FreeBSD by Jim Babb
 */

/* TODO list:
 * 1) Get the DMA stuff working.
 * 2) Get the iov/uio stuff working. Is this a good thing ???
 * 3) Get the synch stuff working.
 * 4) Rewrite it to use malloc for the acb structs instead of static alloc.?
 */

/*
 * PC-9801-100/AHA-1030P support by URATA S.
 */

/*
 * A few customizable items:
 */

/* The SCSI ID of the host adapter/computer */
#ifndef AIC_SCSI_HOSTID
#define AIC_SCSI_HOSTID 7
#endif

/* Use doubleword transfers to/from SCSI chip.  Note: This requires
 * motherboard support.  Basicly, some motherboard chipsets are able to
 * split a 32 bit I/O operation into two 16 bit I/O operations,
 * transparently to the processor.  This speeds up some things, notably long
 * data transfers.
 */
#define AIC_USE_DWORDS 0

/* Allow disconnects?  Was mainly used in an early phase of the driver when
 * the message system was very flaky.  Should go away soon.
 */
#define AIC_ALLOW_DISCONNECT	1

/* Synchronous data transfers? (does not work yet!) XXX */
#define AIC_USE_SYNCHRONOUS	0 	/* Enable/disable (1/0) */
#define AIC_SYNC_PERIOD 	200
#define AIC_SYNC_REQ_ACK_OFS 	8

/* Max attempts made to transmit a message */
#define AIC_MSG_MAX_ATTEMPT	3 /* Not used now XXX */

/* Use DMA (else we do programmed I/O using string instructions) (not yet!)*/
#define AIC_USE_EISA_DMA	0
#define AIC_USE_ISA_DMA		0

/* How to behave on the (E)ISA bus when/if DMAing (on<<4) + off in us */
#define EISA_BRST_TIM ((15<<4) + 1)	/* 15us on, 1us off */

/* Some spin loop parameters (essentially how long to wait some places)
 * The problem(?) is that sometimes we expect either to be able to transmit a
 * byte or to get a new one from the SCSI bus pretty soon.  In order to avoid
 * returning from the interrupt just to get yanked back for the next byte we
 * may spin in the interrupt routine waiting for this byte to come.  How long?
 * This is really (SCSI) device and processor dependent.  Tuneable, I guess.
 */
#define AIC_MSGI_SPIN	1 	/* Will spinwait upto ?ms for a new msg byte */
#define AIC_MSGO_SPIN	1

/* Include debug functions?  At the end of this file there are a bunch of
 * functions that will print out various information regarding queued SCSI
 * commands, driver state and chip contents.  You can call them from the
 * kernel debugger.  If you set AIC_DEBUG to 0 they are not included (the
 * kernel uses less memory) but you lose the debugging facilities.
 */
#define AIC_DEBUG 0

/* End of customizable parameters */

#if AIC_USE_EISA_DMA || AIC_USE_ISA_DMA
#error "I said not yet! Start paying attention... grumble"
#endif

#include "opt_ddb.h"
#include <aic.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <machine/clock.h>
#include <i386/isa/isa_device.h>

#include <sys/kernel.h>

/* Definitions, most of them has turned out to be unneccesary, but here they
 * are anyway.
 */

/*
 * Generic SCSI messages. For now we reject most of them.
 */
/* Messages (1 byte) */		     /* I/T M(andatory) or (O)ptional */
#define MSG_CMDCOMPLETE		0x00 /* M/M */
#define MSG_EXTENDED		0x01 /* O/O */
#define MSG_SAVEDATAPOINTER	0x02 /* O/O */
#define MSG_RESTOREPOINTERS	0x03 /* O/O */
#define MSG_DISCONNECT		0x04 /* O/O */
#define MSG_INITIATOR_DET_ERR	0x05 /* M/M */
#define MSG_ABORT		0x06 /* O/M */
#define MSG_MESSAGE_REJECT	0x07 /* M/M */
#define MSG_NOOP		0x08 /* M/M */
#define MSG_PARITY_ERR		0x09 /* M/M */
#define MSG_LINK_CMD_COMPLETE	0x0a /* O/O */
#define MSG_LINK_CMD_COMPLETEF	0x0b /* O/O */
#define MSG_BUS_DEV_RESET	0x0c /* O/M */
#define MSG_ABORT_TAG		0x0d /* O/O */
#define MSG_CLEAR_QUEUE		0x0e /* O/O */
#define MSG_INIT_RECOVERY	0x0f /* O/O */
#define MSG_REL_RECOVERY	0x10 /* O/O */
#define MSG_TERM_IO_PROC	0x11 /* O/O */

/* Messages (2 byte) */
#define MSG_SIMPLE_Q_TAG	0x20 /* O/O */
#define MSG_HEAD_OF_Q_TAG	0x21 /* O/O */
#define MSG_ORDERED_Q_TAG	0x22 /* O/O */
#define MSG_IGN_WIDE_RESIDUE	0x23 /* O/O */

/* Identify message */
#define MSG_IDENTIFY(lun) ((AIC_ALLOW_DISCONNECT ? 0xc0 : 0x80)|((lun) & 0x7))
#define MSG_ISIDENT(m)		((m) & 0x80)

/* Extended messages (opcode) */
#define MSG_EXT_SDTR		0x01

/* SCSI Status codes */
#define ST_GOOD			0x00
#define ST_CHKCOND		0x02
#define ST_CONDMET		0x04
#define ST_BUSY			0x08
#define ST_INTERMED		0x10
#define ST_INTERMED_CONDMET	0x14
#define ST_RESERVATION_CONFLICT	0x18
#define ST_CMD_TERM		0x22
#define ST_QUEUE_FULL		0x28

#define ST_MASK			0x3e /* bit 0,6,7 is reserved */

/* AIC6360 definitions */
#ifdef PC98
#define SCSISEQ		(iobase + 0x00) /* SCSI sequence control */
#define SXFRCTL0	(iobase + 0x02) /* SCSI transfer control 0 */
#define SXFRCTL1	(iobase + 0x04) /* SCSI transfer control 1 */
#define SCSISIGI	(iobase + 0x06) /* SCSI signal in */
#define SCSISIGO	(iobase + 0x06) /* SCSI signal out */
#define SCSIRATE	(iobase + 0x08) /* SCSI rate control */
#define SCSIID		(iobase + 0x0a) /* SCSI ID */
#define SELID		(iobase + 0x0a) /* Selection/Reselection ID */
#define SCSIDAT		(iobase + 0x0c) /* SCSI Latched Data */
#define SCSIBUS		(iobase + 0x0e) /* SCSI Data Bus*/
#define STCNT0		(iobase + 0x10) /* SCSI transfer count */
#define STCNT1		(iobase + 0x12)
#define STCNT2		(iobase + 0x14)
#define CLRSINT0	(iobase + 0x16) /* Clear SCSI interrupts 0 */
#define SSTAT0		(iobase + 0x16) /* SCSI interrupt status 0 */
#define CLRSINT1	(iobase + 0x18) /* Clear SCSI interrupts 1 */
#define SSTAT1		(iobase + 0x18) /* SCSI status 1 */
#define SSTAT2		(iobase + 0x1a) /* SCSI status 2 */
#define SCSITEST	(iobase + 0x1c) /* SCSI test control */
#define SSTAT3		(iobase + 0x1c) /* SCSI status 3 */
#define CLRSERR		(iobase + 0x1e) /* Clear SCSI errors */
#define SSTAT4		(iobase + 0x1e) /* SCSI status 4 */
#define SIMODE0		(iobase + 0x20) /* SCSI interrupt mode 0 */
#define SIMODE1		(iobase + 0x22) /* SCSI interrupt mode 1 */
#define DMACNTRL0	(iobase + 0x24) /* DMA control 0 */
#define DMACNTRL1	(iobase + 0x26) /* DMA control 1 */
#define DMASTAT		(iobase + 0x28) /* DMA status */
#define FIFOSTAT	(iobase + 0x2a) /* FIFO status */
#define DMADATA		(iobase + 0x2c) /* DMA data */
#define DMADATAL	(iobase + 0x2c) /* DMA data low byte */
#define DMADATAH	(iobase + 0x2e) /* DMA data high byte */
#define BRSTCNTRL	(iobase + 0x30) /* Burst Control */
#define DMADATALONG	(iobase + 0x30)
#define PORTA		(iobase + 0x34) /* Port A */
#define PORTB		(iobase + 0x36) /* Port B */
#define REV		(iobase + 0x38) /* Revision (001 for 6360) */
#define STACK		(iobase + 0x3a) /* Stack */
#define TEST		(iobase + 0x3c) /* Test register */
#define ID		(iobase + 0x3e) /* ID register */
#else
#define SCSISEQ		(iobase + 0x00) /* SCSI sequence control */
#define SXFRCTL0	(iobase + 0x01) /* SCSI transfer control 0 */
#define SXFRCTL1	(iobase + 0x02) /* SCSI transfer control 1 */
#define SCSISIGI	(iobase + 0x03) /* SCSI signal in */
#define SCSISIGO	(iobase + 0x03) /* SCSI signal out */
#define SCSIRATE	(iobase + 0x04) /* SCSI rate control */
#define SCSIID		(iobase + 0x05) /* SCSI ID */
#define SELID		(iobase + 0x05) /* Selection/Reselection ID */
#define SCSIDAT		(iobase + 0x06) /* SCSI Latched Data */
#define SCSIBUS		(iobase + 0x07) /* SCSI Data Bus*/
#define STCNT0		(iobase + 0x08) /* SCSI transfer count */
#define STCNT1		(iobase + 0x09)
#define STCNT2		(iobase + 0x0a)
#define CLRSINT0	(iobase + 0x0b) /* Clear SCSI interrupts 0 */
#define SSTAT0		(iobase + 0x0b) /* SCSI interrupt status 0 */
#define CLRSINT1	(iobase + 0x0c) /* Clear SCSI interrupts 1 */
#define SSTAT1		(iobase + 0x0c) /* SCSI status 1 */
#define SSTAT2		(iobase + 0x0d) /* SCSI status 2 */
#define SCSITEST	(iobase + 0x0e) /* SCSI test control */
#define SSTAT3		(iobase + 0x0e) /* SCSI status 3 */
#define CLRSERR		(iobase + 0x0f) /* Clear SCSI errors */
#define SSTAT4		(iobase + 0x0f) /* SCSI status 4 */
#define SIMODE0		(iobase + 0x10) /* SCSI interrupt mode 0 */
#define SIMODE1		(iobase + 0x11) /* SCSI interrupt mode 1 */
#define DMACNTRL0	(iobase + 0x12) /* DMA control 0 */
#define DMACNTRL1	(iobase + 0x13) /* DMA control 1 */
#define DMASTAT		(iobase + 0x14) /* DMA status */
#define FIFOSTAT	(iobase + 0x15) /* FIFO status */
#define DMADATA		(iobase + 0x16) /* DMA data */
#define DMADATAL	(iobase + 0x16) /* DMA data low byte */
#define DMADATAH	(iobase + 0x17) /* DMA data high byte */
#define BRSTCNTRL	(iobase + 0x18) /* Burst Control */
#define DMADATALONG	(iobase + 0x18)
#define PORTA		(iobase + 0x1a) /* Port A */
#define PORTB		(iobase + 0x1b) /* Port B */
#define REV		(iobase + 0x1c) /* Revision (001 for 6360) */
#define STACK		(iobase + 0x1d) /* Stack */
#define TEST		(iobase + 0x1e) /* Test register */
#define ID		(iobase + 0x1f) /* ID register */
#endif

#define IDSTRING "(C)1991ADAPTECAIC6360           "

/* What all the bits do */

/* SCSISEQ */
#define TEMODEO		0x80
#define ENSELO		0x40
#define ENSELI		0x20
#define ENRESELI	0x10
#define ENAUTOATNO	0x08
#define ENAUTOATNI	0x04
#define ENAUTOATNP	0x02
#define SCSIRSTO	0x01

/* SXFRCTL0 */
#define SCSIEN		0x80
#define DMAEN		0x40
#define CHEN		0x20
#define CLRSTCNT	0x10
#define SPIOEN		0x08
#define CLRCH		0x02

/* SXFRCTL1 */
#define BITBUCKET	0x80
#define SWRAPEN		0x40
#define ENSPCHK		0x20
#define STIMESEL1	0x10
#define STIMESEL0	0x08
#define STIMO_256ms	0x00
#define STIMO_128ms	0x08
#define STIMO_64ms	0x10
#define STIMO_32ms	0x18
#define ENSTIMER	0x04
#define BYTEALIGN	0x02

/* SCSISIGI */
#define CDI		0x80
#define IOI		0x40
#define MSGI		0x20
#define ATNI		0x10
#define SELI		0x08
#define BSYI		0x04
#define REQI		0x02
#define ACKI		0x01

/* Important! The 3 most significant bits of this register, in initiator mode,
 * represents the "expected" SCSI bus phase and can be used to trigger phase
 * mismatch and phase change interrupts.  But more important:  If there is a
 * phase mismatch the chip will not transfer any data!  This is actually a nice
 * feature as it gives us a bit more control over what is happening when we are
 * bursting data (in) through the FIFOs and the phase suddenly changes from
 * DATA IN to STATUS or MESSAGE IN.  The transfer will stop and wait for the
 * proper phase to be set in this register instead of dumping the bits into the
 * FIFOs.
 */
/* SCSISIGO */
#define CDO		0x80
#define CDEXP		(CDO)
#define IOO		0x40
#define IOEXP		(IOO)
#define MSGO		0x20
#define MSGEXP		(MSGO)
#define ATNO		0x10
#define SELO		0x08
#define BSYO		0x04
#define REQO		0x02
#define ACKO		0x01

/* Information transfer phases */
#define PH_DOUT		(0)
#define PH_DIN		(IOI)
#define PH_CMD		(CDI)
#define PH_STAT		(CDI|IOI)
#define PH_MSGO		(MSGI|CDI)
#define PH_MSGI		(MSGI|CDI|IOI)

#define PH_MASK		0xe0

/* Some pseudo phases for getphase()*/
#define PH_BUSFREE	0x100	/* (Re)Selection no longer valid */
#define PH_INVALID	0x101	/* (Re)Selection valid, but no REQ yet */
#define PH_PSBIT	0x100	/* "pseudo" bit */

/* SCSIRATE */
#define SXFR2		0x40
#define SXFR1		0x20
#define SXFR0		0x10
#define SOFS3		0x08
#define SOFS2		0x04
#define SOFS1		0x02
#define SOFS0		0x01

/* SCSI ID */
#define OID2		0x40
#define OID1		0x20
#define OID0		0x10
#define OID_S		4	/* shift value */
#define TID2		0x04
#define TID1		0x02
#define TID0		0x01
#define SCSI_ID_MASK	0x7

/* SCSI selection/reselection ID (both target *and* initiator) */
#define SELID7		0x80
#define SELID6		0x40
#define SELID5		0x20
#define SELID4		0x10
#define SELID3		0x08
#define SELID2		0x04
#define SELID1		0x02
#define SELID0		0x01

/* CLRSINT0                      Clears what? (interrupt and/or status bit) */
#define SETSDONE	0x80
#define CLRSELDO	0x40	/* I */
#define CLRSELDI	0x20	/* I+ */
#define CLRSELINGO	0x10	/* I */
#define CLRSWRAP	0x08	/* I+S */
#define CLRSDONE	0x04	/* I+S */
#define CLRSPIORDY	0x02	/* I */
#define CLRDMADONE	0x01	/* I */

/* SSTAT0                          Howto clear */
#define TARGET		0x80
#define SELDO		0x40	/* Selfclearing */
#define SELDI		0x20	/* Selfclearing when CLRSELDI is set */
#define SELINGO		0x10	/* Selfclearing */
#define SWRAP		0x08	/* CLRSWAP */
#define SDONE		0x04	/* Not used in initiator mode */
#define SPIORDY		0x02	/* Selfclearing (op on SCSIDAT) */
#define DMADONE		0x01	/* Selfclearing (all FIFOs empty & T/C */

/* CLRSINT1                      Clears what? */
#define CLRSELTIMO	0x80	/* I+S */
#define CLRATNO		0x40
#define CLRSCSIRSTI	0x20	/* I+S */
#define CLRBUSFREE	0x08	/* I+S */
#define CLRSCSIPERR	0x04	/* I+S */
#define CLRPHASECHG	0x02	/* I+S */
#define CLRREQINIT	0x01	/* I+S */

/* SSTAT1                       How to clear?  When set?*/
#define SELTO		0x80	/* C		select out timeout */
#define ATNTARG		0x40	/* Not used in initiator mode */
#define SCSIRSTI	0x20	/* C		RST asserted */
#define PHASEMIS	0x10	/* Selfclearing */
#define BUSFREE		0x08	/* C		bus free condition */
#define SCSIPERR	0x04	/* C		parity error on inbound data */
#define PHASECHG	0x02	/* C	     phase in SCSISIGI doesn't match */
#define REQINIT		0x01	/* C or ACK	asserting edge of REQ */

/* SSTAT2 */
#define SOFFSET		0x20
#define SEMPTY		0x10
#define SFULL		0x08
#define SFCNT2		0x04
#define SFCNT1		0x02
#define SFCNT0		0x01

/* SCSITEST */
#define SCTESTU		0x08
#define SCTESTD		0x04
#define STCTEST		0x01

/* SSTAT3 */
#define SCSICNT3	0x80
#define SCSICNT2	0x40
#define SCSICNT1	0x20
#define SCSICNT0	0x10
#define OFFCNT3		0x08
#define OFFCNT2		0x04
#define OFFCNT1		0x02
#define OFFCNT0		0x01

/* CLRSERR */
#define CLRSYNCERR	0x04
#define CLRFWERR	0x02
#define CLRFRERR	0x01

/* SSTAT4 */
#define SYNCERR		0x04
#define FWERR		0x02
#define FRERR		0x01

/* SIMODE0 */
#define ENSELDO		0x40
#define ENSELDI		0x20
#define ENSELINGO	0x10
#define	ENSWRAP		0x08
#define ENSDONE		0x04
#define ENSPIORDY	0x02
#define ENDMADONE	0x01

/* SIMODE1 */
#define ENSELTIMO	0x80
#define ENATNTARG	0x40
#define ENSCSIRST	0x20
#define ENPHASEMIS	0x10
#define ENBUSFREE	0x08
#define ENSCSIPERR	0x04
#define ENPHASECHG	0x02
#define ENREQINIT	0x01

/* DMACNTRL0 */
#define ENDMA		0x80
#define B8MODE		0x40
#define DMA		0x20
#define DWORDPIO	0x10
#define WRITE		0x08
#define INTEN		0x04
#define RSTFIFO		0x02
#define SWINT		0x01

/* DMACNTRL1 */
#define PWRDWN		0x80
#define ENSTK32		0x40
#define STK4		0x10
#define STK3		0x08
#define STK2		0x04
#define STK1		0x02
#define STK0		0x01

/* DMASTAT */
#define ATDONE		0x80
#define WORDRDY		0x40
#define INTSTAT		0x20
#define DFIFOFULL	0x10
#define DFIFOEMP	0x08
#define DFIFOHF		0x04
#define DWORDRDY	0x02

/* BRSTCNTRL */
#define BON3		0x80
#define BON2		0x40
#define BON1		0x20
#define BON0		0x10
#define BOFF3		0x08
#define BOFF2		0x04
#define BOFF1		0x02
#define BOFF0		0x01

/* TEST */
#define BOFFTMR		0x40
#define BONTMR		0x20
#define STCNTH		0x10
#define STCNTM		0x08
#define STCNTL		0x04
#define SCSIBLK		0x02
#define DMABLK		0x01


#define orreg(reg, val)   outb((reg), inb(reg)| (val))
#define andreg(reg, val)  outb((reg), inb(reg)& (val))
#define nandreg(reg, val) outb((reg), inb(reg)&~(val))



#ifdef DDB
#define	fatal_if_no_DDB()
#else
#define	fatal_if_no_DDB() panic("panic for historical reasons")
#endif

typedef u_long physaddr;

struct aic_dma_seg {
	physaddr	addr;
	long		len;
};

#define DELAYCOUNT	16

#define FUDGE(X)	((X)>>1) 	/* get 1 ms spincount */
#define MINIFUDGE(X)	((X)>>4) 	/* get (approx) 125us spincount */
#define AIC_NSEG	16
#define NUM_CONCURRENT	7	/* Only one per target for now */

/*
 * ACB. Holds additional information for each SCSI command Comments: We
 * need a separate scsi command block because we may need to overwrite it
 * with a request sense command.  Basicly, we refrain from fiddling with
 * the scsi_xfer struct (except do the expected updating of return values).
 * We'll generally update: xs->{flags,resid,error,sense,status} and
 * occasionally xs->retries.
 */

struct acb {
	TAILQ_ENTRY(acb) chain;
	struct scsi_xfer *xs;	/* SCSI xfer ctrl block from above */
	int		flags;	/* Status */
#define ACB_FREE	0x00
#define ACB_ACTIVE	0x01
#define ACB_DONE	0x04
#define ACB_CHKSENSE	0x08
/*	struct aic_dma_seg dma[AIC_NSEG]; */  /* Physical addresses+len */
	struct scsi_generic cmd;  /* SCSI command block */
	int	 clen;
	char	*daddr;		/* Saved data pointer */
	int	 dleft;		/* Residue */
	int 	 stat;		/* SCSI status byte */
};

/*
 * Some info about each (possible) target on the SCSI bus.  This should
 * probably have been a "per target+lunit" structure, but we'll leave it at
 * this for now.  Is there a way to reliably hook it up to sc->fordriver??
 */
struct aic_tinfo {
	int	cmds;		/* #commands processed */
	int	dconns;		/* #disconnects */
	int	touts;		/* #timeouts */
	int	perrs;		/* #parity errors */
	int	senses;		/* #request sense commands sent */
	ushort	lubusy;		/* What local units/subr. are busy? */
	u_char  flags;
#define NEED_TO_RESET	0x01	/* Should send a BUS_DEV_RESET */
#define DO_NEGOTIATE	0x02	/* (Re)Negotiate synchronous options */
#define TARGET_BUSY	0x04	/* Target is busy, i.e. cmd in progress */
	u_char  persgst;	/* Period suggestion */
	u_char  offsgst;	/* Offset suggestion */
	u_char  syncdata;	/* True negotiated synch parameters */
};

/* Register a linenumber (for debugging) */
#if AIC_DEBUG
#define LOGLINE(p) \
	do {					\
		p->history[p->hp] = __LINE__;	\
		p->hp = ++p->hp % AIC_HSIZE;	\
	} while (0)
#else
#define LOGLINE(p)
#endif

static struct aic_data { /* One of these per adapter */
	u_short		iobase;		/* Base I/O port */
	struct scsi_link sc_link;	/* prototype for subdevs */
	int		aic_int;	/* IRQ on the EISA bus */
	int		aic_dma;	/* DRQ on the EISA bus */
	/* Lists of command blocks */
	TAILQ_HEAD(acb_list, acb) free_list, ready_list, nexus_list;
	struct acb *nexus;	/* current command */
	/* Command blocks and target info */
	struct acb acb[NUM_CONCURRENT];
	struct aic_tinfo tinfo[8];
	/* Data about the current nexus (updated for every cmd switch) */
	u_char	*dp;		/* Current data pointer */
	int	 dleft;		/* Data left to transfer */
	/* Adapter state */
	short	 phase;		/* Copy of what bus phase we are in */
	short	 prevphase;	/* Copy of what bus phase we were in */
	short	 state;		/* State applicable to the adapter */
#define AIC_IDLE	0x01
#define AIC_TMP_UNAVAIL	0x02	/* Don't accept SCSI commands */
#define AIC_SELECTING	0x03	/* SCSI command is arbiting  */
#define AIC_RESELECTED	0x04	/* Has been reselected */
#define AIC_HASNEXUS	0x05	/* Actively using the SCSI bus */
#define AIC_CLEANING	0x06
	short	 flags;
#define AIC_DROP_MSGI	0x01	/* Discard all msgs (parity err detected) */
#define AIC_DOINGDMA	0x02	/* The FIFO data path is active! */
#define AIC_BUSFREE_OK	0x04	/* Bus free phase is OK. */
#define AIC_SYNCHNEGO	0x08	/* Synch negotiation in progress. */
#define AIC_BLOCKED	0x10	/* Don't schedule new scsi bus operations */
	/* Debugging stuff */
#define AIC_HSIZE 8
	short	history[AIC_HSIZE]; /* Store line numbers here. */
	short	hp;
	u_char	progress;	/* Set if interrupt has achieved progress */
	/* Message stuff */
	u_char	msgpriq;	/* One or more messages to send (encoded) */
	u_char	msgout;		/* What message is on its way out? */
#define SEND_DEV_RESET		0x01
#define SEND_PARITY_ERROR	0x02
#define SEND_ABORT		0x04
#define SEND_REJECT		0x08
#define SEND_INIT_DET_ERR	0x10
#define SEND_IDENTIFY  		0x20
#define SEND_SDTR		0x40
#define AIC_MAX_MSG_LEN 8
	u_char  omess[AIC_MAX_MSG_LEN];	/* Scratch area for messages */
	u_char	*omp;		/* Message pointer (for multibyte messages) */
	u_char  omlen;
	u_char	imess[AIC_MAX_MSG_LEN + 1];
	u_char	*imp;		/* Message pointer (for multibyte messages) */
	u_char	imlen;
} *aicdata[NAIC];

#define AIC_SHOWACBS 0x01
#define AIC_SHOWINTS 0x02
#define AIC_SHOWCMDS 0x04
#define AIC_SHOWMISC 0x08
#define AIC_SHOWTRAC 0x10
#define AIC_SHOWSTART 0x20
static int aic_debug = 0; /* AIC_SHOWSTART|AIC_SHOWMISC|AIC_SHOWTRAC; */

#if AIC_DEBUG
#define AIC_ACBS(str)  do {if (aic_debug & AIC_SHOWACBS) printf str;} while (0)
#define AIC_MISC(str)  do {if (aic_debug & AIC_SHOWMISC) printf str;} while (0)
#define AIC_INTS(str)  do {if (aic_debug & AIC_SHOWINTS) printf str;} while (0)
#define AIC_TRACE(str) do {if (aic_debug & AIC_SHOWTRAC) printf str;} while (0)
#define AIC_CMDS(str)  do {if (aic_debug & AIC_SHOWCMDS) printf str;} while (0)
#define AIC_START(str) do {if (aic_debug & AIC_SHOWSTART) printf str;}while (0)
#else
#define AIC_ACBS(str)
#define AIC_MISC(str)
#define AIC_INTS(str)
#define AIC_TRACE(str)
#define AIC_CMDS(str)
#define AIC_START(str)
#endif

static int	aicprobe	__P((struct isa_device *));
static int	aicattach	__P((struct isa_device *));
static void	aic_minphys	__P((struct buf *));
static u_int32_t	aic_adapter_info __P((int));
static void 	aic_init	__P((struct aic_data *));
static int 	aic_find	__P((struct aic_data *));
static void	aic_done	__P((struct acb *));
static void	aic_dataout	__P((struct aic_data *aic));
static void	aic_datain	__P((struct aic_data *aic));
static int32_t	aic_scsi_cmd	__P((struct scsi_xfer *));
static int	aic_poll	__P((struct aic_data *aic, struct acb *));
void	aic_add_timeout __P((struct acb *, int));
void	aic_remove_timeout __P((struct acb *));
static	void	aic6360_reset	__P((struct aic_data *aic));
static	u_short	aicphase	__P((struct aic_data *aic));
static	void	aic_msgin	__P((struct aic_data *aic));
static	void	aic_msgout	__P((struct aic_data *aic));
static timeout_t aic_timeout;
static void	aic_sched	__P((struct aic_data *));
static void	aic_scsi_reset	__P((struct aic_data *));
#if AIC_DEBUG
void	aic_print_active_acb	__P((void));
void	aic_dump6360		__P((void));
void	aic_dump_driver		__P((void));
#endif

/* Linkup to the rest of the kernel */
struct isa_driver aicdriver = {
    aicprobe, aicattach, "aic"
};

static int aicunit = 0;

static struct scsi_adapter aic_switch = {
	aic_scsi_cmd,
	aic_minphys,
	0,
	0,
	aic_adapter_info,
	"aic"
	,0 , 0
};

static struct scsi_device aic_dev = {
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
	"aic",
	0
};


/*
 * INITIALIZATION ROUTINES (probe, attach ++)
 */

/*
 * aicprobe: probe for AIC6360 SCSI-controller
 * returns non-zero value if a controller is found.
 */
static int
aicprobe(dev)
	struct isa_device *dev;
{
	int	unit = aicunit;
	struct aic_data *aic;

	if (unit >= NAIC) {
		printf("aic%d: unit number too high\n", unit);
		return 0;
	}
	dev->id_unit = unit;
	/*
	* Allocate a storage area for us
	*/
	if (aicdata[unit]) {
	       printf("aic%d: memory already allocated\n", unit);
	       return 0;
	}
	aic = malloc(sizeof(struct aic_data), M_TEMP, M_NOWAIT);
	if (!aic) {
	       printf("aic%d: cannot malloc!\n", unit);
	       return 0;
	}
	bzero(aic, sizeof(struct aic_data));
	aicdata[unit] = aic;
	aic->iobase = dev->id_iobase;

	if (aic_find(aic) != 0) {
		aicdata[unit] = NULL;
		free(aic, M_TEMP);
		return 0;
	}
	aicunit++;
#ifdef PC98
	return 0x40;
#else
	return 0x20;
#endif
}

/* Do the real search-for-device.
 * Prerequisite: aic->iobase should be set to the proper value
 */
static int
aic_find(aic)
	struct aic_data *aic;
{
	u_short iobase = aic->iobase;
	char chip_id[sizeof(IDSTRING)];	/* For chips that support it */
	int i;

	/* Remove aic6360 from possible powerdown mode */
	outb(DMACNTRL0, 0);

	/* Thanks to mark@aggregate.com for the new method for detecting
	 * whether the chip is present or not.  Bonus: may also work for
	 * the AIC-6260!
 	 */
	AIC_TRACE(("aic: probing for aic-chip at port 0x%x\n",(int)iobase));
 	/*
 	 * Linux also init's the stack to 1-16 and then clears it,
     	 *  6260's don't appear to have an ID reg - mpg
 	 */
	/* Push the sequence 0,1,..,15 on the stack */
#define STSIZE 16
	outb(DMACNTRL1, 0);	/* Reset stack pointer */
	for (i = 0; i < STSIZE; i++)
		outb(STACK, i);

	/* See if we can pull out the same sequence */
	outb(DMACNTRL1, 0);
 	for (i = 0; i < STSIZE && inb(STACK) == i; i++)
		;
	if (i != STSIZE) {
		AIC_START(("STACK futzed at %d.\n", i));
		return ENXIO;
	}

	/* See if we can pull the id string out of the ID register,
	 * now only used for informational purposes.
	 */
	bzero(chip_id, sizeof(chip_id));
	insb(ID, chip_id, sizeof(IDSTRING)-1);
	AIC_START(("AIC found at 0x%x ", (int)aic->iobase));
	AIC_START(("ID: %s ",chip_id));
	AIC_START(("chip revision %d\n",(int)inb(REV)));
	return 0;
}


/*
 * Attach the AIC6360, fill out some high and low level data structures
 */
static int
aicattach(dev)
	struct isa_device *dev;
{
	int unit = dev->id_unit;
	struct aic_data *aic = aicdata[unit];
	struct scsibus_data *scbus;

	AIC_TRACE(("aicattach\n"));
	aic->state = 0;
	aic_scsi_reset(aic);
	aic_init(aic);	/* Init chip and driver */

	/*
	 * Fill in the prototype scsi_link
	 */
	aic->sc_link.adapter_unit = unit;
	aic->sc_link.adapter_targ = AIC_SCSI_HOSTID;
	aic->sc_link.adapter_softc = aic;
	aic->sc_link.adapter = &aic_switch;
	aic->sc_link.device = &aic_dev;

	/*
	 * Prepare the scsibus_data area for the upperlevel
	 * scsi code.
	 */
	scbus = scsi_alloc_bus();
	if(!scbus)
		return 0;
	scbus->adapter_link = &aic->sc_link;

	/*
	 * ask the adapter what subunits are present
	 */
	scsi_attachdevs(scbus);

	return 1;
}


/* Initialize AIC6360 chip itself
 * The following conditions should hold:
 * aicprobe should have succeeded, i.e. the iobase address in aic_data must
 * be valid.
 */
static void
aic6360_reset(aic)
	struct aic_data *aic;
{
	u_short iobase = aic->iobase;

	outb(SCSITEST, 0);	/* Doc. recommends to clear these two */
	outb(TEST, 0);		/* registers before operations commence */

	/* Reset SCSI-FIFO and abort any transfers */
	outb(SXFRCTL0, CHEN|CLRCH|CLRSTCNT);

	/* Reset DMA-FIFO */
	outb(DMACNTRL0, RSTFIFO);
	outb(DMACNTRL1, 0);

	outb(SCSISEQ, 0);	/* Disable all selection features */
	outb(SXFRCTL1, 0);

	outb(SIMODE0, 0x00);		/* Disable some interrupts */
	outb(CLRSINT0, 0x7f);	/* Clear a slew of interrupts */

	outb(SIMODE1, 0x00);		/* Disable some more interrupts */
	outb(CLRSINT1, 0xef);	/* Clear another slew of interrupts */

	outb(SCSIRATE, 0);	/* Disable synchronous transfers */

	outb(CLRSERR, 0x07);	/* Haven't seen ant errors (yet) */

	outb(SCSIID, AIC_SCSI_HOSTID << OID_S); /* Set our SCSI-ID */
	outb(BRSTCNTRL, EISA_BRST_TIM);
}

/* Pull the SCSI RST line for 500 us */
static void
aic_scsi_reset(aic)
	struct aic_data *aic;
{
	u_short iobase = aic->iobase;

	outb(SCSISEQ, SCSIRSTO);
	DELAY(500);
	outb(SCSISEQ, 0);
	DELAY(50);
}

/*
 * Initialize aic SCSI driver, also (conditonally) reset the SCSI bus.
 * The reinitialization is still buggy (e.g. on SCSI resets).
 */
static void
aic_init(aic)
	struct aic_data *aic;
{
	u_short iobase = aic->iobase;
	struct acb *acb;
	int r;

				/* Reset the SCSI-bus itself */
	aic_scsi_reset(aic);

	aic6360_reset(aic);	/* Clean up our own hardware */

/*XXX*/	/* If not the first time (probably a reset condition),
	 * we should clean queues with active commands
	 */
	if (aic->state == 0) {	/* First time through */
		TAILQ_INIT(&aic->ready_list);
		TAILQ_INIT(&aic->nexus_list);
		TAILQ_INIT(&aic->free_list);
		aic->nexus = 0;
		acb = aic->acb;
		bzero(acb, sizeof(aic->acb));
		for (r = 0; r < sizeof(aic->acb) / sizeof(*acb); r++) {
			TAILQ_INSERT_TAIL(&aic->free_list, acb, chain);
			acb++;
		}
		bzero(&aic->tinfo, sizeof(aic->tinfo));
	} else {
		aic->state = AIC_CLEANING;
		if (aic->nexus != NULL) {
			aic->nexus->xs->error = XS_DRIVER_STUFFUP;
			untimeout(aic_timeout, (caddr_t)aic->nexus);
			aic_done(aic->nexus);
		}
		aic->nexus = NULL;
		while (acb = aic->nexus_list.tqh_first) {
			acb->xs->error = XS_DRIVER_STUFFUP;
			untimeout(aic_timeout, (caddr_t)acb);
			aic_done(acb);
		}
	}

	aic->phase = aic->prevphase = PH_INVALID;
	aic->hp = 0;
	for (r = 0; r < 7; r++) {
		struct aic_tinfo *tp = &aic->tinfo[r];
		tp->flags = AIC_USE_SYNCHRONOUS ? DO_NEGOTIATE : 0;
		tp->flags |= NEED_TO_RESET;
		tp->persgst = AIC_SYNC_PERIOD;
		tp->offsgst = AIC_SYNC_REQ_ACK_OFS;
		tp->syncdata = 0;
	}
	aic->state = AIC_IDLE;
	outb(DMACNTRL0, INTEN);
	return;
}

/*
 * DRIVER FUNCTIONS CALLABLE FROM HIGHER LEVEL DRIVERS
 */

/*
 * Expected sequence:
 * 1) Command inserted into ready list
 * 2) Command selected for execution
 * 3) Command won arbitration and has selected target device
 * 4) Send message out (identify message, eventually also sync.negotiations)
 * 5) Send command
 * 5a) Receive disconnect message, disconnect.
 * 5b) Reselected by target
 * 5c) Receive identify message from target.
 * 6) Send or receive data
 * 7) Receive status
 * 8) Receive message (command complete etc.)
 * 9) If status == SCSI_CHECK construct a synthetic request sense SCSI cmd.
 *    Repeat 2-8 (no disconnects please...)
 */

/*
 * Start a SCSI-command
 * This function is called by the higher level SCSI-driver to queue/run
 * SCSI-commands.
 */
static int32_t
aic_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc = xs->sc_link;
	struct aic_data *aic;
	struct acb 	*acb;
	int s = 0;
	int flags;

	aic = (struct aic_data *)sc->adapter_softc;
	SC_DEBUG(sc, SDEV_DB2, ("aic_scsi_cmd\n"));
	AIC_TRACE(("aic_scsi_cmd\n"));
	AIC_MISC(("[0x%x, %d]->%d ", (int)xs->cmd->opcode, xs->cmdlen,
		  sc->target));

	flags = xs->flags;

	/* Get a aic command block */
	if (!(flags & SCSI_NOMASK)) {
		/* Critical region */
		s = splbio();
		acb = aic->free_list.tqh_first;
		if (acb) {
			TAILQ_REMOVE(&aic->free_list, acb, chain);
		}
		splx(s);
	} else {
		acb = aic->free_list.tqh_first;
		if (acb) {
			TAILQ_REMOVE(&aic->free_list, acb, chain);
		}
	}

	if (acb == NULL) {
		xs->error = XS_DRIVER_STUFFUP;
		AIC_MISC(("TRY_AGAIN_LATER"));
		return TRY_AGAIN_LATER;
	}

	/* Initialize acb */
	acb->flags = ACB_ACTIVE;
	acb->xs = xs;
	bcopy(xs->cmd, &acb->cmd, xs->cmdlen);
	acb->clen = xs->cmdlen;
	acb->daddr = xs->data;
	acb->dleft = xs->datalen;
	acb->stat = 0;

	if (!(flags & SCSI_NOMASK))
		s = splbio();

	TAILQ_INSERT_TAIL(&aic->ready_list, acb, chain);
	timeout(aic_timeout, (caddr_t)acb, (xs->timeout*hz)/1000);

	if (aic->state == AIC_IDLE)
		aic_sched(aic);

	if (!(flags & SCSI_NOMASK)) { /* Almost done. Wait outside */
		splx(s);
		AIC_MISC(("SUCCESSFULLY_QUEUED"));
		return SUCCESSFULLY_QUEUED;
	}

	/* Not allowed to use interrupts, use polling instead */
	return aic_poll(aic, acb);
}

/*
 * Adjust transfer size in buffer structure
 */
static void
aic_minphys(bp)
	struct buf *bp;
{

	AIC_TRACE(("aic_minphys\n"));
	if (bp->b_bcount > (AIC_NSEG << PAGE_SHIFT))
		bp->b_bcount = (AIC_NSEG << PAGE_SHIFT);
}


static u_int32_t
aic_adapter_info(unit)
	int	unit;
{

	AIC_TRACE(("aic_adapter_info\n"));
	return (2);		/* One outstanding command per target */
}

/*
 * Used when interrupt driven I/O isn't allowed, e.g. during boot.
 */
static int
aic_poll(aic, acb)
	struct aic_data *aic;
	struct acb *acb;
{
	register u_short iobase = aic->iobase;
	struct scsi_xfer *xs = acb->xs;
	int count = xs->timeout * 10;

	AIC_TRACE(("aic_poll\n"));
	while (count) {
		if (inb(DMASTAT) & INTSTAT)
			aicintr(xs->sc_link->adapter_unit);
		if (xs->flags & ITSDONE)
			break;
		DELAY(100);
		count--;
	}
	if (count == 0) {
		AIC_MISC(("aic_poll: timeout"));
		aic_timeout((caddr_t)acb);
	}
	if (xs->error)
		return HAD_ERROR;
	return COMPLETE;
}

/* LOW LEVEL SCSI UTILITIES */

/* Determine the SCSI bus phase, return either a real SCSI bus phase or some
 * pseudo phase we use to detect certain exceptions.  This one is a bit tricky.
 * The bits we peek at:
 * CDI, MSGI and DI is the 3 SCSI signals determining the bus phase.
 * These should be qualified by REQI high and ACKI low.
 * Also peek at SSTAT0[SELDO|SELDI] to detect a passing BUSFREE condition.
 * No longer detect SCSI RESET or PERR here.  They are tested for separately
 * in the interrupt handler.
 * Note: If an exception occur at some critical time during the phase
 * determination we'll most likely return something wildly erronous....
 */
static inline u_short
aicphase(aic)
	struct aic_data *aic;
{
	register u_short iobase = aic->iobase;
	register u_char sstat0, sstat1, scsisig;

	sstat1 = inb(SSTAT1);	/* Look for REQINIT (REQ asserted) */
	scsisig = inb(SCSISIGI); /* Get the SCSI bus signals */
	sstat0 = inb(SSTAT0);	/* Get the selection valid status bits */

	if (!(inb(SSTAT0) & (SELDO|SELDI))) /* Selection became invalid? */
		return PH_BUSFREE;

	/* Selection is still valid */
	if (!(sstat1 & REQINIT)) 		/* REQ not asserted ? */
		return PH_INVALID;

	/* REQ is asserted, (and ACK is not) */
	return scsisig & PH_MASK;
}


/* Schedule a scsi operation.  This has now been pulled out of the interrupt
 * handler so that we may call it from aic_scsi_cmd and aic_done.  This may
 * save us an unecessary interrupt just to get things going.  Should only be
 * called when state == AIC_IDLE and at bio pl.
 */
static void
aic_sched(aic)
	register struct aic_data *aic;
{
	struct scsi_link *sc;
	struct acb *acb;
	u_short iobase = aic->iobase;
	int t;
	u_char simode0, simode1, scsiseq;

	AIC_TRACE(("aic_sched\n"));
	simode0 = ENSELDI;
	simode1 = ENSCSIRST|ENSCSIPERR|ENREQINIT;
	scsiseq = ENRESELI;
	/*
	 * Find first acb in rdy queue that is for a target/lunit
	 * combinations that is not busy.
	 */
	outb(CLRSINT1, CLRSELTIMO|CLRBUSFREE|CLRSCSIPERR);
	for (acb = aic->ready_list.tqh_first; acb; acb = acb->chain.tqe_next) {
		sc = acb->xs->sc_link;
		t = sc->target;
		if (!(aic->tinfo[t].lubusy & (1 << sc->lun))) {
			TAILQ_REMOVE(&aic->ready_list, acb, chain);
			aic->nexus = acb;
			aic->state = AIC_SELECTING;
			/*
			 * Start selection process. Always enable
			 * reselections.  Note: we don't have a nexus yet, so
			 * cannot set aic->state = AIC_HASNEXUS.
			 */
			simode0 = ENSELDI|ENSELDO;
			simode1 = ENSCSIRST|ENSCSIPERR|
				  ENREQINIT|ENSELTIMO;
			scsiseq = ENRESELI|ENSELO|ENAUTOATNO;
			outb(SCSIID, AIC_SCSI_HOSTID << OID_S | t);
			outb(SXFRCTL1, STIMO_256ms|ENSTIMER);
			outb(CLRSINT0, CLRSELDO);
			break;
		}
#if AIC_DEBUG
		else
			AIC_MISC(("%d:%d busy\n", t, sc->lun));
#endif
	}
	AIC_MISC(("%sselecting\n",scsiseq&ENSELO?"":"re"));
	outb(SIMODE0, simode0);
	outb(SIMODE1, simode1);
	outb(SCSISEQ, scsiseq);
}


/*
 * POST PROCESSING OF SCSI_CMD (usually current)
 */
static void
aic_done(acb)
	struct acb *acb;
{
	struct scsi_xfer *xs = acb->xs;
	struct scsi_link *sc = xs->sc_link;
	struct aic_data *aic = (struct aic_data *)sc->adapter_softc;

	AIC_TRACE(("aic_done "));

	/*
	 * Now, if we've come here with no error code, i.e. we've kept the
	 * initial XS_NOERROR, and the status code signals that we should
	 * check sense, we'll need to set up a request sense cmd block and
	 * push the command back into the ready queue *before* any other
	 * commands for this target/lunit, else we lose the sense info.
	 * We don't support chk sense conditions for the request sense cmd.
	 */
	if (xs->error == XS_NOERROR && !(acb->flags & ACB_CHKSENSE)) {
		if ((acb->stat & ST_MASK)==SCSI_CHECK) {
			struct scsi_sense *ss = (void *)&acb->cmd;
			AIC_MISC(("requesting sense "));
			/* First, save the return values */
			xs->resid = acb->dleft;
			xs->status = acb->stat;
			/* Next, setup a request sense command block */
			bzero(ss, sizeof(*ss));
			ss->op_code = REQUEST_SENSE;
			ss->byte2 = sc->lun << 5;
			ss->length = sizeof(struct scsi_sense_data);
			acb->clen = sizeof(*ss);
			acb->daddr = (char *)&xs->sense;
			acb->dleft = sizeof(struct scsi_sense_data);
			acb->flags = ACB_ACTIVE|ACB_CHKSENSE;
			TAILQ_INSERT_HEAD(&aic->ready_list, acb, chain);
			aic->tinfo[sc->target].lubusy &= ~(1<<sc->lun);
			aic->tinfo[sc->target].senses++;
			if (aic->nexus == acb) {
				aic->nexus = NULL;
				aic->state = AIC_IDLE;
				aic_sched(aic);
			}
			return;
		}
	}

	if (xs->flags & SCSI_ERR_OK) {
		xs->resid = 0;
		xs->error = XS_NOERROR;
	} else if (xs->error == XS_NOERROR && (acb->flags & ACB_CHKSENSE)) {
		xs->error = XS_SENSE;
	} else {
		xs->resid = acb->dleft;
	}
	xs->flags |= ITSDONE;

#if AIC_DEBUG
	if (aic_debug & AIC_SHOWMISC) {
		printf("err=0x%02x ",xs->error);
		if (xs->error == XS_SENSE)
			printf("sense=%2x\n", xs->sense.error_code);
	}
	if ((xs->resid || xs->error > XS_SENSE) && aic_debug & AIC_SHOWMISC) {
		if (xs->resid)
			printf("aic_done: resid=%d\n", xs->resid);
		if (xs->error)
			printf("aic_done: error=%d\n", xs->error);
	}
#endif

	/*
	 * Remove the ACB from whatever queue it's on.  We have to do a bit of
	 * a hack to figure out which queue it's on.  Note that it is *not*
	 * necessary to cdr down the ready queue, but we must cdr down the
	 * nexus queue and see if it's there, so we can mark the unit as no
	 * longer busy.  This code is sickening, but it works.
	 */
	if (acb == aic->nexus) {
		aic->state = AIC_IDLE;
		aic->tinfo[sc->target].lubusy &= ~(1<<sc->lun);
		aic_sched(aic);
	} else if (aic->ready_list.tqh_last == &acb->chain.tqe_next) {
		TAILQ_REMOVE(&aic->ready_list, acb, chain);
	} else {
		register struct acb *acb2;
		for (acb2 = aic->nexus_list.tqh_first; acb2;
		    acb2 = acb2->chain.tqe_next)
			if (acb2 == acb) {
				TAILQ_REMOVE(&aic->nexus_list, acb, chain);
				aic->tinfo[sc->target].lubusy &= ~(1<<sc->lun);
				/* XXXX Should we call aic_sched() here? */
				break;
			}
		if (acb2)
			;
		else if (acb->chain.tqe_next) {
			TAILQ_REMOVE(&aic->ready_list, acb, chain);
		} else {
			printf("aic%d: can't find matching acb\n", 
				xs->sc_link->adapter_unit);
			Debugger("aic6360");
			fatal_if_no_DDB();
		}
	}
	/* Put it on the free list. */
	acb->flags = ACB_FREE;
	TAILQ_INSERT_HEAD(&aic->free_list, acb, chain);

	aic->tinfo[sc->target].cmds++;
	scsi_done(xs);
	return;
}

/*
 * INTERRUPT/PROTOCOL ENGINE
 */

/* The message system:
 * This is a revamped message system that now should easier accomodate new
 * messages, if necessary.
 * Currently we accept these messages:
 * IDENTIFY (when reselecting)
 * COMMAND COMPLETE # (expect bus free after messages marked #)
 * NOOP
 * MESSAGE REJECT
 * SYNCHRONOUS DATA TRANSFER REQUEST
 * SAVE DATA POINTER
 * RESTORE POINTERS
 * DISCONNECT #
 *
 * We may send these messages in prioritized order:
 * BUS DEVICE RESET #		if SCSI_RESET & xs->flags (or in weird sits.)
 * MESSAGE PARITY ERROR		par. err. during MSGI
 * MESSAGE REJECT		If we get a message we don't know how to handle
 * ABORT #			send on errors
 * INITIATOR DETECTED ERROR	also on errors (SCSI2) (during info xfer)
 * IDENTIFY			At the start of each transfer
 * SYNCHRONOUS DATA TRANSFER REQUEST	if appropriate
 * NOOP				if nothing else fits the bill ...
 */

#define aic_sched_msgout(m) \
	do {				\
		orreg(SCSISIGO, ATNO);	\
		aic->msgpriq |= (m);	\
	} while (0)

#define IS1BYTEMSG(m) (((m) != 1 && (m) < 0x20) || (m) >= 0x80)
#define IS2BYTEMSG(m) (((m) & 0xf0) == 0x20)
#define ISEXTMSG(m) ((m) == 1)
/* Precondition:
 * The SCSI bus is already in the MSGI phase and there is a message byte
 * on the bus, along with an asserted REQ signal.
 */
static void
aic_msgin(aic)
	register struct aic_data *aic;
{
	register u_short iobase = aic->iobase;
	int spincount, extlen;
	u_char sstat1;

	AIC_TRACE(("aic_msgin "));
	outb(SCSISIGO, PH_MSGI);
	/* Prepare for a new message.  A message should (according to the SCSI
	 * standard) be transmitted in one single message_in phase.
	 * If we have been in some other phase, then this is a new message.
	 */
	if (aic->prevphase != PH_MSGI) {
		aic->flags &= ~AIC_DROP_MSGI;
		aic->imlen = 0;
	}
	/*
	 * Read a whole message but the last byte.  If we shall reject the
	 * message, we shall have to do it, by asserting ATNO, during the
	 * message transfer phase itself.
	 */
	for (;;) {
		sstat1 = inb(SSTAT1);
		/* If parity errors just dump everything on the floor, also
		 * a parity error automatically sets ATNO
		 */
		if (sstat1 & SCSIPERR) {
			aic_sched_msgout(SEND_PARITY_ERROR);
			aic->flags |= AIC_DROP_MSGI;
		}
		/*
		 * If we're going to reject the message, don't bother storing
		 * the incoming bytes.  But still, we need to ACK them.
		 */
		if (!(aic->flags & AIC_DROP_MSGI)) {
			/* Get next message byte */
			aic->imess[aic->imlen] = inb(SCSIDAT);
			/*
			 * This testing is suboptimal, but most messages will
			 * be of the one byte variety, so it should not effect
			 * performance significantly.
			 */
			if (IS1BYTEMSG(aic->imess[0]))
				break;
			if (IS2BYTEMSG(aic->imess[0]) && aic->imlen == 1)
				break;
			if (ISEXTMSG(aic->imess[0]) && aic->imlen > 0) {
				if (aic->imlen == AIC_MAX_MSG_LEN) {
					aic->flags |= AIC_DROP_MSGI;
					aic_sched_msgout(SEND_REJECT);
				}
				extlen = aic->imess[1] ? aic->imess[1] : 256;
				if (aic->imlen == extlen + 2)
					break; /* Got it all */
			}
		}
		/* If we reach this spot we're either:
		 * a) in the middle of a multi-byte message or
		 * b) we're dropping bytes
		 */
		outb(SXFRCTL0, CHEN|SPIOEN);
		inb(SCSIDAT); /* Really read it (ACK it, that is) */
		outb(SXFRCTL0, CHEN);
		aic->imlen++;

		/*
		 * We expect the bytes in a multibyte message to arrive
		 * relatively close in time, a few microseconds apart.
		 * Therefore we will spinwait for some small amount of time
		 * waiting for the next byte.
		 */
		spincount = DELAYCOUNT * AIC_MSGI_SPIN;
		LOGLINE(aic);
		while (spincount-- && !((sstat1 = inb(SSTAT1)) & REQINIT))
			;
		if (spincount == -1 || sstat1 & (PHASEMIS|BUSFREE))
			return;
	}
	/* Now we should have a complete message (1 byte, 2 byte and moderately
	 * long extended messages).  We only handle extended messages which
	 * total length is shorter than AIC_MAX_MSG_LEN.  Longer messages will
	 * be amputated.  (Return XS_BOBBITT ?)
	 */
	if (aic->state == AIC_HASNEXUS) {
		struct acb *acb = aic->nexus;
		struct aic_tinfo *ti = &aic->tinfo[acb->xs->sc_link->target];
		int offs, per, rate;

		outb(SIMODE1, ENSCSIRST|ENPHASEMIS|ENBUSFREE|ENSCSIPERR);
		switch (aic->imess[0]) {
		case MSG_CMDCOMPLETE:
			if (!acb) {
				aic_sched_msgout(SEND_ABORT);
				printf("aic: CMDCOMPLETE but no command?\n");
				break;
			}
			if (aic->dleft < 0) {
				struct scsi_link *sc = acb->xs->sc_link;
				printf("aic: %d extra bytes from %d:%d\n",
				    -aic->dleft, sc->target, sc->lun);
				acb->dleft = 0;
			}
			acb->xs->resid = acb->dleft = aic->dleft;
			aic->flags |= AIC_BUSFREE_OK;
			untimeout(aic_timeout, (caddr_t)acb);
			aic_done(acb);
			break;
		case MSG_MESSAGE_REJECT:
			if (aic_debug & AIC_SHOWMISC)
				printf("aic: our msg rejected by target\n");
			if (aic->flags & AIC_SYNCHNEGO) {
				ti->syncdata = 0;
				ti->persgst = ti->offsgst = 0;
				aic->flags &= ~AIC_SYNCHNEGO;
				ti->flags &= ~DO_NEGOTIATE;
			}
			/* Not all targets understand INITIATOR_DETECTED_ERR */
			if (aic->msgout == SEND_INIT_DET_ERR)
				aic_sched_msgout(SEND_ABORT);
			break;
		case MSG_NOOP:	/* Will do! Immediately, sir!*/
			break;	/* Hah, that was easy! */
		case MSG_DISCONNECT:
			if (!acb) {
				aic_sched_msgout(SEND_ABORT);
				printf("aic: nothing to DISCONNECT\n");
				break;
			}
			ti->dconns++;
			TAILQ_INSERT_HEAD(&aic->nexus_list, acb, chain);
			acb = aic->nexus = NULL;
			aic->state = AIC_IDLE;
			aic->flags |= AIC_BUSFREE_OK;
			break;
		case MSG_SAVEDATAPOINTER:
			if (!acb) {
				aic_sched_msgout(SEND_ABORT);
				printf("aic: no DATAPOINTERs to save\n");
				break;
			}
			acb->dleft = aic->dleft;
			acb->daddr = aic->dp;
			break;
		case MSG_RESTOREPOINTERS:
			if (!acb) {
				aic_sched_msgout(SEND_ABORT);
				printf("aic: no DATAPOINTERs to restore\n");
				break;
			}
			aic->dp = acb->daddr;
			aic->dleft = acb->dleft;
			break;
		case MSG_EXTENDED:
			switch (aic->imess[2]) {
			case MSG_EXT_SDTR:
				per = aic->imess[3] * 4;
				rate = (per + 49 - 100)/50;
				offs = aic->imess[4];
				if (offs == 0)
					ti->syncdata = 0;
				else if (rate > 7) {
					/* Too slow for aic6360. Do asynch
					 * instead.  Renegotiate the deal.
					 */
					ti->persgst = 0;
					ti->offsgst = 0;
					aic_sched_msgout(SEND_SDTR);
				} else {
					rate = rate<<4 | offs;
					ti->syncdata = rate;
				}
				break;
			default: /* Extended messages we don't handle */
				aic_sched_msgout(SEND_REJECT);
				break;
			}
			break;
		default:
			aic_sched_msgout(SEND_REJECT);
			break;
		}
	} else if (aic->state == AIC_RESELECTED) {
		struct scsi_link *sc;
		struct acb *acb;
		u_char selid, lunit;
		/*
		 * Which target is reselecting us? (The ID bit really)
		 */
		selid = inb(SELID) & ~(1<<AIC_SCSI_HOSTID);
		if (MSG_ISIDENT(aic->imess[0])) { 	/* Identify? */
			AIC_MISC(("searching "));
			/* Search wait queue for disconnected cmd
			 * The list should be short, so I haven't bothered with
			 * any more sophisticated structures than a simple
			 * singly linked list.
			 */
			lunit = aic->imess[0] & 0x07;
			for (acb = aic->nexus_list.tqh_first; acb;
			    acb = acb->chain.tqe_next) {
				sc = acb->xs->sc_link;
				if (sc->lun == lunit &&
				    selid == (1<<sc->target)) {
					TAILQ_REMOVE(&aic->nexus_list, acb,
					    chain);
					break;
				}
			}
			if (!acb) { /* Invalid reselection! */
				aic_sched_msgout(SEND_ABORT);
				printf("aic: invalid reselect (idbit=0x%2x)\n",
				    selid);
			} else { /* Reestablish nexus */
				/* Setup driver data structures and
				 * do an implicit RESTORE POINTERS
				 */
				aic->nexus = acb;
				aic->dp = acb->daddr;
				aic->dleft = acb->dleft;
				aic->tinfo[sc->target].lubusy |= (1<<sc->lun);
				outb(SCSIRATE,aic->tinfo[sc->target].syncdata);
				AIC_MISC(("... found acb"));
				aic->state = AIC_HASNEXUS;
			}
		} else {
			printf("aic: bogus reselect (no IDENTIFY) %0x2x\n",
			    selid);
			aic_sched_msgout(SEND_DEV_RESET);
		}
	} else { /* Neither AIC_HASNEXUS nor AIC_RESELECTED! */
		printf("aic: unexpected message in; will send DEV_RESET\n");
		aic_sched_msgout(SEND_DEV_RESET);
	}
	/* Must not forget to ACK the last message byte ... */
	outb(SXFRCTL0, CHEN|SPIOEN);
	inb(SCSIDAT);
	outb(SXFRCTL0, CHEN);
	outb(SIMODE1, ENSCSIRST|ENBUSFREE|ENSCSIPERR|ENREQINIT);
}


/* The message out (and in) stuff is a bit complicated:
 * If the target requests another message (sequence) without
 * having changed phase in between it really asks for a
 * retransmit, probably due to parity error(s).
 * The following messages can be sent:
 * IDENTIFY	   @ These 3 stems from scsi command activity
 * BUS_DEV_RESET   @
 * IDENTIFY + SDTR @
 * MESSAGE_REJECT if MSGI doesn't make sense
 * MESSAGE_PARITY_ERROR if MSGI spots a parity error
 * NOOP if asked for a message and there's nothing to send
 */
static void
aic_msgout(aic)
	register struct aic_data *aic;
{
	register u_short iobase = aic->iobase;
	struct aic_tinfo *ti;
	struct acb *acb;

	/* First determine what to send. If we haven't seen a
	 * phasechange this is a retransmission request.
	 */
	outb(SCSISIGO, PH_MSGO);
	if (aic->prevphase != PH_MSGO) { /* NOT a retransmit */
		/* Pick up highest priority message */
		aic->msgout = aic->msgpriq & -aic->msgpriq; /* What message? */
		aic->omlen = 1;	/* "Default" message len */
		switch (aic->msgout) {
		case SEND_SDTR:	/* Also implies an IDENTIFY message */
			acb = aic->nexus;
			ti = &aic->tinfo[acb->xs->sc_link->target];
			aic->omess[1] = MSG_EXTENDED;
			aic->omess[2] = 3;
			aic->omess[3] = MSG_EXT_SDTR;
			aic->omess[4] = ti->persgst >> 2;
			aic->omess[5] = ti->offsgst;
			aic->omlen = 6;
			/* Fallthrough! */
		case SEND_IDENTIFY:
			if (aic->state != AIC_HASNEXUS) {
				printf("aic at line %d: no nexus", __LINE__);
				Debugger("aic6360");
				fatal_if_no_DDB();
			}
			acb = aic->nexus;
			aic->omess[0] = MSG_IDENTIFY(acb->xs->sc_link->lun);
			break;
		case SEND_DEV_RESET:
			aic->omess[0] = MSG_BUS_DEV_RESET;
			aic->flags |= AIC_BUSFREE_OK;
			break;
		case SEND_PARITY_ERROR:
			aic->omess[0] = MSG_PARITY_ERR;
			break;
		case SEND_ABORT:
			aic->omess[0] = MSG_ABORT;
			aic->flags |= AIC_BUSFREE_OK;
			break;
		case SEND_INIT_DET_ERR:
			aic->omess[0] = MSG_INITIATOR_DET_ERR;
			break;
		case SEND_REJECT:
			aic->omess[0] = MSG_MESSAGE_REJECT;
			break;
		default:
			aic->omess[0] = MSG_NOOP;
			break;
		}
		aic->omp = aic->omess;
	} else if (aic->omp == &aic->omess[aic->omlen]) {
		/* Have sent the message at least once, this is a retransmit.
		 */
		AIC_MISC(("retransmitting "));
		if (aic->omlen > 1)
			outb(SCSISIGO, PH_MSGO|ATNO);
	}
	/* else, we're in the middle of a multi-byte message */
	outb(SXFRCTL0, CHEN|SPIOEN);
	outb(DMACNTRL0, INTEN|RSTFIFO);
	outb(SIMODE1, ENSCSIRST|ENBUSFREE|ENSCSIPERR|ENREQINIT);
	do {
		LOGLINE(aic);
		do {
			aic->phase = aicphase(aic);
		} while (aic->phase == PH_INVALID);
		if (aic->phase != PH_MSGO)
			/* Target left MSGO, possibly to reject our
			 * message
			 */
			break;
		/* Clear ATN before last byte */
		if (aic->omp == &aic->omess[aic->omlen-1])
			outb(CLRSINT1, CLRATNO);
		outb(SCSIDAT, *aic->omp++);	/* Send MSG */
		LOGLINE(aic);
		while (inb(SCSISIGI) & ACKO)
			;
	} while (aic->omp != &aic->omess[aic->omlen]);
	aic->progress = aic->omp != aic->omess;
	/* We get here in two ways:
	 * a) phase != MSGO.  Target is probably going to reject our message
	 * b) aic->omp == &aic->omess[aic->omlen], i.e. the message has been
	 *    transmitted correctly and accepted by the target.
	 */
	if (aic->phase == PH_MSGO) {	/* Message accepted by target! */
		aic->msgpriq &= ~aic->msgout;
		aic->msgout = 0;
	}
	outb(SXFRCTL0, CHEN);	/* Disable SPIO */
	outb(SIMODE0, 0); /* Setup interrupts before leaving */
	outb(SIMODE1, ENSCSIRST|ENBUSFREE|ENSCSIPERR|ENREQINIT);
	/* Enabled ints: SCSIPERR, SCSIRSTI (unexpected)
	 * 		 REQINIT (expected) BUSFREE (possibly expected)
	 */
}

/* aic_dataout: perform a data transfer using the FIFO datapath in the aic6360
 * Precondition: The SCSI bus should be in the DOUT phase, with REQ asserted
 * and ACK deasserted (i.e. waiting for a data byte)
 * This new revision has been optimized (I tried) to make the common case fast,
 * and the rarer cases (as a result) somewhat more comlex
 */
static void
aic_dataout(aic)
	register struct aic_data *aic;
{
	register u_short iobase = aic->iobase;
	register u_char dmastat;
	int amount, olddleft = aic->dleft;
#define DOUTAMOUNT 128		/* Full FIFO */

	/* Enable DATA OUT transfers */
	outb(SCSISIGO, PH_DOUT);
	outb(CLRSINT1, CLRPHASECHG);
	/* Clear FIFOs and counters */
	outb(SXFRCTL0, CHEN|CLRSTCNT|CLRCH);
	outb(DMACNTRL0, WRITE|INTEN|RSTFIFO);
	/* Enable FIFOs */
	outb(SXFRCTL0, SCSIEN|DMAEN|CHEN);
	outb(DMACNTRL0, ENDMA|DWORDPIO|WRITE|INTEN);

	/* Setup to detect:
	 * PHASEMIS & PHASECHG: target has left the DOUT phase
	 * SCSIRST: something just pulled the RST line.
	 * BUSFREE: target has unexpectedly left the DOUT phase
	 */
	outb(SIMODE1, ENPHASEMIS|ENSCSIRST|ENBUSFREE|ENPHASECHG);

	/* I have tried to make the main loop as tight as possible.  This
	 * means that some of the code following the loop is a bit more
	 * complex than otherwise.
	 */
	while (aic->dleft) {
		int xfer;

		LOGLINE(aic);

		for (;;) {
			dmastat = inb(DMASTAT);
			if (dmastat & DFIFOEMP)
				break;
			if (dmastat & INTSTAT)
				goto phasechange;
		}

		xfer = min(DOUTAMOUNT, aic->dleft);

#if AIC_USE_DWORDS
		if (xfer >= 12) {
			outsl(DMADATALONG, aic->dp, xfer/4);
			aic->dleft -= xfer & ~3;
			aic->dp += xfer & ~3;
			xfer &= 3;
		}
#else
		if (xfer >= 8) {
			outsw(DMADATA, aic->dp, xfer/2);
			aic->dleft -= xfer & ~1;
			aic->dp += xfer & ~1;
			xfer &= 1;
		}
#endif

		if (xfer) {
			outb(DMACNTRL0, ENDMA|B8MODE|INTEN);
			outsb(DMADATA, aic->dp, xfer);
			aic->dleft -= xfer;
			aic->dp += xfer;
			outb(DMACNTRL0, ENDMA|DWORDPIO|INTEN);
		}
	}

	/* See the bytes off chip */
	for (;;) {
		dmastat = inb(DMASTAT);
		if ((dmastat & DFIFOEMP) && (inb(SSTAT2) & SEMPTY))
			break;
		if (dmastat & INTSTAT)
			goto phasechange;
	}

phasechange:
	/* We now have the data off chip.  */
	outb(SXFRCTL0, CHEN);

	if (dmastat & INTSTAT) { /* Some sort of phasechange */
		register u_char sstat2;
		/* Stop transfers, do some accounting */
		amount = inb(FIFOSTAT);
		sstat2 = inb(SSTAT2);
		if ((sstat2 & 7) == 0)
			amount += sstat2 & SFULL ? 8 : 0;
		else
			amount += sstat2 & 7;
		aic->dleft += amount;
		aic->dp -= amount;
		AIC_MISC(("+%d ", amount));
	}

	outb(DMACNTRL0, RSTFIFO|INTEN);
	LOGLINE(aic);
	while (inb(SXFRCTL0) & SCSIEN)
		;
	outb(SIMODE1, ENSCSIRST|ENBUSFREE|ENSCSIPERR|ENREQINIT);
	/* Enabled ints: BUSFREE, SCSIPERR, SCSIRSTI (unexpected)
	 * 		 REQINIT (expected)
	 */
	aic->progress = olddleft != aic->dleft;
	return;
}

/* aic_datain: perform data transfers using the FIFO datapath in the aic6360
 * Precondition: The SCSI bus should be in the DIN phase, with REQ asserted
 * and ACK deasserted (i.e. at least one byte is ready).
 * For now, uses a pretty dumb algorithm, hangs around until all data has been
 * transferred.  This, is OK for fast targets, but not so smart for slow
 * targets which don't disconnect or for huge transfers.
 */
static void
aic_datain(aic)
	register struct aic_data *aic;
{
	register u_short iobase = aic->iobase;
	register u_char dmastat;
	int olddleft = aic->dleft;
#define DINAMOUNT 128		/* Default amount of data to transfer */

	/* Enable DATA IN transfers */
	outb(SCSISIGO, PH_DIN);
	outb(CLRSINT1, CLRPHASECHG);
	/* Clear FIFOs and counters */
	outb(SXFRCTL0, CHEN|CLRSTCNT|CLRCH);
	outb(DMACNTRL0, INTEN|RSTFIFO);
	/* Enable FIFOs */
	outb(SXFRCTL0, SCSIEN|DMAEN|CHEN);
	outb(DMACNTRL0, ENDMA|DWORDPIO|INTEN);

	outb(SIMODE1, ENSCSIRST|ENPHASEMIS|ENBUSFREE|ENPHASECHG);

	/* We leave this loop if one or more of the following is true:
	 * a) phase != PH_DIN && FIFOs are empty
	 * b) SCSIRSTI is set (a reset has occurred) or busfree is detected.
	 */
	while (aic->dleft) {
		int done = 0;
		int xfer;

		LOGLINE(aic);

		/* Wait for fifo half full or phase mismatch */
		for (;;) {
			dmastat = inb(DMASTAT);
			if (dmastat & (DFIFOFULL|INTSTAT))
				break;
		}

		if (dmastat & DFIFOFULL)
			xfer = DINAMOUNT;
		else {
			while ((inb(SSTAT2) & SEMPTY) == 0)
				;
			xfer = inb(FIFOSTAT);
			done = 1;
		}

		xfer = min(xfer, aic->dleft);

#if AIC_USE_DWORDS
		if (xfer >= 12) {
			insl(DMADATALONG, aic->dp, xfer/4);
			aic->dleft -= xfer & ~3;
			aic->dp += xfer & ~3;
			xfer &= 3;
		}
#else
		if (xfer >= 8) {
			insw(DMADATA, aic->dp, xfer/2);
			aic->dleft -= xfer & ~1;
			aic->dp += xfer & ~1;
			xfer &= 1;
		}
#endif

		if (xfer) {
			outb(DMACNTRL0, ENDMA|B8MODE|INTEN);
			insb(DMADATA, aic->dp, xfer);
			aic->dleft -= xfer;
			aic->dp += xfer;
			outb(DMACNTRL0, ENDMA|DWORDPIO|INTEN);
		}

		if (done)
			break;
	}

#if 0
	if (aic->dleft)
		printf("residual of %d\n", aic->dleft);
#endif

	aic->progress = olddleft != aic->dleft;
	/* Some SCSI-devices are rude enough to transfer more data than what
	 * was requested, e.g. 2048 bytes from a CD-ROM instead of the
	 * requested 512.  Test for progress, i.e. real transfers.  If no real
	 * transfers have been performed (acb->dleft is probably already zero)
	 * and the FIFO is not empty, waste some bytes....
	 */
	if (!aic->progress) {
		int extra = 0;
		LOGLINE(aic);

		for (;;) {
			dmastat = inb(DMASTAT);
			if (dmastat & DFIFOEMP)
				break;
			(void) inb(DMADATA); /* Throw it away */
			extra++;
		}

		AIC_MISC(("aic: %d extra bytes from %d:%d\n", extra,
		    acb->xs->sc_link->target, acb->xs->sc_link->lun));
		aic->progress = extra;
	}

	/* Stop the FIFO data path */
	outb(SXFRCTL0, CHEN);

	outb(DMACNTRL0, RSTFIFO|INTEN);
	/* Come back when REQ is set again */
	outb(SIMODE1, ENSCSIRST|ENBUSFREE|ENSCSIPERR|ENREQINIT);
	LOGLINE(aic);
}


/*
 * This is the workhorse routine of the driver.
 * Deficiencies (for now):
 * 1) always uses programmed I/O
 * 2) doesn't support synchronous transfers properly (yet)
 */

void
aicintr(int unit)
{
	struct aic_data *aic = aicdata[unit];
	register struct acb *acb;
	register struct scsi_link *sc;
	register u_short iobase = aic->iobase;
	struct aic_tinfo *ti;
	u_char sstat0, sstat1,  sstat2, sxfrctl0;


	LOGLINE(aic);
	/* Clear INTEN.  This is important if we're running with edge
	 * triggered interrupts as we don't guarantee that all interrupts will
	 * be served during one single invocation of this routine, i.e. we may
	 * need another edge.
	 */
	outb(DMACNTRL0, 0);
	AIC_TRACE(("aicintr\n"));

	/*
	 * 1st check for abnormal conditions, such as reset or parity errors
	 */
	sstat1 = inb(SSTAT1);
	AIC_MISC(("s1:0x%02x ", sstat1));
	if (sstat1 & (SCSIRSTI|SCSIPERR)) {
		if (sstat1 & SCSIRSTI) {
			printf("aic: reset in -- reinitializing....\n");
			aic_init(aic); /* Restart everything */
			LOGLINE(aic);
			outb(DMACNTRL0, INTEN);
			return;
		} else {
			printf("aic: SCSI bus parity error\n");
			outb(CLRSINT1, CLRSCSIPERR);
			if (aic->prevphase == PH_MSGI)
				aic_sched_msgout(SEND_PARITY_ERROR);
			else
				aic_sched_msgout(SEND_INIT_DET_ERR);
		}
	}

	/*
	 * If we're not already busy doing something test for the following
	 * conditions:
	 * 1) We have been reselected by something
	 * 2) We have selected something successfully
	 * 3) Our selection process has timed out
	 * 4) This is really a bus free interrupt just to get a new command
	 *    going?
	 * 5) Spurious interrupt?
	 */
	sstat0 = inb(SSTAT0);
	AIC_MISC(("s0:0x%02x ", sstat0));
	if (aic->state != AIC_HASNEXUS) { /* No nexus yet */
		if (sstat0 & SELDI) {
			LOGLINE(aic);
			/* We have been reselected. Things to do:
			 * a) If we're trying to select something ourselves
			 *    back off the current command.
			 * b) "Wait" for a message in phase (IDENTIFY)
			 * c) Call aic_msgin() to get the identify message and
			 *    retrieve the disconnected command from the wait
			 *    queue.
			 */
			AIC_MISC(("reselect "));
			/* If we're trying to select a target ourselves,
			 * push our command back into the rdy list.
			 */
			if (aic->state == AIC_SELECTING) {
				AIC_MISC(("backoff selector "));
				TAILQ_INSERT_HEAD(&aic->ready_list, aic->nexus,
				    chain);
				aic->nexus = NULL;
			}
			aic->state = AIC_RESELECTED;
			/* Clear interrupts, disable future selection stuff
			 * including select interrupts and timeouts
			 */
			outb(CLRSINT0, CLRSELDI);
			outb(SCSISEQ, 0);
			outb(SIMODE0, 0);
			/* Setup chip so we may detect spurious busfree
			 * conditions later.
			 */
			outb(CLRSINT1, CLRBUSFREE);
			outb(SIMODE1, ENSCSIRST|ENBUSFREE|
			     ENSCSIPERR|ENREQINIT);
			/* Now, we're expecting an IDENTIFY message. */
			aic->phase = aicphase(aic);
			if (aic->phase & PH_PSBIT) {
				LOGLINE(aic);
				outb(DMACNTRL0, INTEN);
				return;	/* Come back when REQ is set */
			}
			if (aic->phase == PH_MSGI)
				aic_msgin(aic);	/* Handle identify message */
			else {
				/* Things are seriously fucked up.
				 * Pull the brakes, i.e. RST
				 */
				printf("aic at line %d: target didn't identify\n", __LINE__);
				Debugger("aic6360");
				fatal_if_no_DDB();
				aic_init(aic);
				return;
			}
			if (aic->state != AIC_HASNEXUS) {/* IDENTIFY fail?! */
				printf("aic at line %d: identify failed\n",
				    __LINE__);
				aic_init(aic);
				return;
			} else {
				outb(SIMODE1,
				    ENSCSIRST|ENBUSFREE|ENSCSIPERR|ENREQINIT);
				/* Fallthrough to HASNEXUS part of aicintr */
			}
		} else if (sstat0 & SELDO) {
			LOGLINE(aic);
			/* We have selected a target. Things to do:
			 * a) Determine what message(s) to send.
			 * b) Verify that we're still selecting the target.
			 * c) Mark device as busy.
			 */
			acb = aic->nexus;
			if (!acb) {
				printf("aic at line %d: missing acb", __LINE__);
				Debugger("aic6360");
				fatal_if_no_DDB();
			}
			sc = acb->xs->sc_link;
			ti = &aic->tinfo[sc->target];
			if (acb->xs->flags & SCSI_RESET)
				aic->msgpriq = SEND_DEV_RESET;
			else if (ti->flags & DO_NEGOTIATE)
				aic->msgpriq = SEND_IDENTIFY|SEND_SDTR;
			else
				aic->msgpriq = SEND_IDENTIFY;
			/* Setup chip to enable later testing for busfree
			 * conditions
			 */
			outb(CLRSINT1, CLRBUSFREE);
			outb(SCSISEQ, 0); /* Stop selection stuff */
			nandreg(SIMODE0, ENSELDO); /* No more selectout ints */
			sstat0 = inb(SSTAT0);
			if (sstat0 & SELDO) { /* Still selected!? */
				outb(SIMODE0, 0);
				outb(SIMODE1, ENSCSIRST|ENSCSIPERR|
				     ENBUSFREE|ENREQINIT);
				aic->state = AIC_HASNEXUS;
				aic->flags = 0;
				aic->prevphase = PH_INVALID;
				aic->dp = acb->daddr;
				aic->dleft = acb->dleft;
				ti->lubusy |= (1<<sc->lun);
				AIC_MISC(("select ok "));
			} else {
				/* Has seen busfree since selection, i.e.
				 * a "spurious" selection. Shouldn't happen.
				 */
				printf("aic: unexpected busfree\n");
				acb->xs->error = XS_DRIVER_STUFFUP;
				untimeout(aic_timeout, (caddr_t)acb);
				aic_done(acb);
			}
			LOGLINE(aic);
			outb(DMACNTRL0, INTEN);
			return;
		} else if (sstat1 & SELTO) {
			/* Selection timed out. What to do:
			 * Disable selections out and fail the command with
			 * code XS_TIMEOUT.
			 */
			acb = aic->nexus;
			if (!acb) {
				printf("aic at line %d: missing acb", __LINE__);
				Debugger("aic6360");
				fatal_if_no_DDB();
			}
			outb(SCSISEQ, ENRESELI|ENAUTOATNP);
			outb(SXFRCTL1, 0);
			outb(CLRSINT1, CLRSELTIMO);
			aic->state = AIC_IDLE;
			acb->xs->error = XS_TIMEOUT;
			untimeout(aic_timeout, (caddr_t)acb);
			aic_done(acb);
			LOGLINE(aic);
			outb(DMACNTRL0, INTEN);
			return;
		} else {
			/* Assume a bus free interrupt.  What to do:
			 * Start selecting.
			 */
			if (aic->state == AIC_IDLE)
				aic_sched(aic);
#if AIC_DEBUG
			else
				AIC_MISC(("Extra aic6360 interrupt."));
#endif
			LOGLINE(aic);
			outb(DMACNTRL0, INTEN);
			return;
		}
	}
	/* Driver is now in state AIC_HASNEXUS, i.e. we have a current command
	 * working the SCSI bus.
	 */
	acb = aic->nexus;
	if (aic->state != AIC_HASNEXUS || acb == NULL) {
		printf("aic: no nexus!!\n");
		Debugger("aic6360");
		fatal_if_no_DDB();
	}

	/* What sort of transfer does the bus signal? */
	aic->phase = aicphase(aic);
	if (!(aic->phase & PH_PSBIT)) /* not a pseudo phase */
		outb(SCSISIGO, aic->phase);
	outb(CLRSINT1, CLRPHASECHG);
	/* These interrupts are enabled by default:
	 * SCSIRSTI, SCSIPERR, BUSFREE, REQINIT
	 */
	switch (aic->phase) {
	case PH_MSGO:
		LOGLINE(aic);
		if (aic_debug & AIC_SHOWMISC)
			printf("PH_MSGO ");
		aic_msgout(aic);
		aic->prevphase = PH_MSGO;
		/* Setup interrupts before leaving */
		outb(SIMODE0, 0);
		outb(SIMODE1, ENSCSIRST|ENBUSFREE|ENSCSIPERR|ENREQINIT);
		/* Enabled ints: SCSIPERR, SCSIRSTI (unexpected)
		 * 		 REQINIT (expected) BUSFREE (possibly expected)
		 */
		break;
	case PH_CMD:		/* CMD phase & REQ asserted */
		LOGLINE(aic);
		if (aic_debug & AIC_SHOWMISC)
			printf("PH_CMD 0x%02x (%d) ",
			       acb->cmd.opcode, acb->clen);
		outb(SCSISIGO, PH_CMD);
		/* Use FIFO for CMDs. Assumes that no cmd > 128 bytes. OK? */
		/* Clear hostFIFO and enable EISA-hostFIFO transfers */
		outb(DMACNTRL0, WRITE|RSTFIFO|INTEN);	/* 3(4) */
		/* Clear scsiFIFO and enable SCSI-interface
		   & hostFIFO-scsiFIFO transfers */
		outb(SXFRCTL0, CHEN|CLRCH|CLRSTCNT); 	/* 4 */
		outb(SXFRCTL0, SCSIEN|DMAEN|CHEN); 	/* 5 */
		outb(DMACNTRL0, ENDMA|WRITE|INTEN); 	/* 3+6 */
		/* What (polled) interrupts to enable */
		outb(SIMODE1, ENPHASEMIS|ENSCSIRST|ENBUSFREE|ENSCSIPERR);
		/* DFIFOEMP is set, FIFO (128 byte) is always big enough */
		outsw(DMADATA, (short *)&acb->cmd, acb->clen>>1);

		/* Wait for SCSI FIFO to drain */
		LOGLINE(aic);
		do {
			sstat2 = inb(SSTAT2);
		} while (!(sstat2 & SEMPTY) && !(inb(DMASTAT) & INTSTAT));
		if (!(inb(SSTAT2) & SEMPTY)) {
			printf("aic at line %d: SCSI-FIFO didn't drain\n",
			    __LINE__);
			Debugger("aic6360");
			fatal_if_no_DDB();
			acb->xs->error = XS_DRIVER_STUFFUP;
			untimeout(aic_timeout, (caddr_t)acb);
			aic_done(acb);
			aic_init(aic);
			return;
		}
		outb(SXFRCTL0, CHEN);	/* Clear SCSIEN & DMAEN */
		outb(SIMODE0, 0);
		outb(SIMODE1, ENSCSIRST|ENBUSFREE|ENSCSIPERR);
		LOGLINE(aic);
		do {
			sxfrctl0 = inb(SXFRCTL0);
		} while (sxfrctl0 & SCSIEN && !(inb(DMASTAT) & INTSTAT));
		if (sxfrctl0 & SCSIEN) {
			printf("aic at line %d: scsi xfer never finished\n",
			    __LINE__);
			Debugger("aic6360");
			fatal_if_no_DDB();
			acb->xs->error = XS_DRIVER_STUFFUP;
			untimeout(aic_timeout, (caddr_t)acb);
			aic_done(acb);
			aic_init(aic);
			return;
		}
		outb(SIMODE1, ENSCSIRST|ENBUSFREE|ENSCSIPERR|ENREQINIT);
		/* Enabled ints: BUSFREE, SCSIPERR, SCSIRSTI (unexpected)
		 * 		 REQINIT (expected)
		 */
		aic->prevphase = PH_CMD;
		break;
	case PH_DOUT:
		LOGLINE(aic);
		AIC_MISC(("PH_DOUT [%d] ",aic->dleft));
		aic_dataout(aic);
		aic->prevphase = PH_DOUT;
		break;
	case PH_MSGI:
		LOGLINE(aic);
		if (aic_debug & AIC_SHOWMISC)
			printf("PH_MSGI ");
		aic_msgin(aic);
		outb(SIMODE1, ENSCSIRST|ENBUSFREE|ENSCSIPERR|ENREQINIT);
		aic->prevphase = PH_MSGI;
		break;
	case PH_DIN:
		LOGLINE(aic);
		if (aic_debug & AIC_SHOWMISC)
			printf("PH_DIN ");
		aic_datain(aic);
		aic->prevphase = PH_DIN;
		break;
	case PH_STAT:
		LOGLINE(aic);
		if (aic_debug & AIC_SHOWMISC)
			printf("PH_STAT ");
		outb(SCSISIGO, PH_STAT);
		outb(SXFRCTL0, CHEN|SPIOEN);
		outb(DMACNTRL0, RSTFIFO|INTEN);
		outb(SIMODE1, ENSCSIRST|ENPHASEMIS|ENBUSFREE|ENSCSIPERR);
		acb->stat = inb(SCSIDAT);
		outb(SXFRCTL0, CHEN);
		if (aic_debug & AIC_SHOWMISC)
			printf("0x%02x ", acb->stat);
		outb(SIMODE1, ENSCSIRST|ENBUSFREE|ENSCSIPERR|ENREQINIT);
		aic->prevphase = PH_STAT;
		break;
	case PH_INVALID:
		LOGLINE(aic);
		break;
	case PH_BUSFREE:
		LOGLINE(aic);
		if (aic->flags & AIC_BUSFREE_OK) { /*It's fun the 1st time.. */
			aic->flags &= ~AIC_BUSFREE_OK;
		} else {
			printf("aic at line %d: unexpected busfree phase\n",
			    __LINE__);
			Debugger("aic6360");
			fatal_if_no_DDB();
		}
		break;
        default:
		printf("aic at line %d: bogus bus phase\n", __LINE__);
		Debugger("aic6360");
		fatal_if_no_DDB();
		break;
	}
	LOGLINE(aic);
	outb(DMACNTRL0, INTEN);
	return;
}

static void
aic_timeout(void *arg1) {
	int s = splbio();
	struct acb *acb = (struct acb *)arg1;
	int     unit;
	struct aic_data *aic;

	unit = acb->xs->sc_link->adapter_unit;
	aic = aicdata[unit];
	sc_print_addr(acb->xs->sc_link);
	acb->xs->error = XS_TIMEOUT;
	printf("timed out\n");

	aic_done(acb);
	splx(s);
}

#if AIC_DEBUG
/*
 * The following functions are mostly used for debugging purposes, either
 * directly called from the driver or from the kernel debugger.
 */

void
aic_show_scsi_cmd(acb)
	struct acb *acb;
{
	u_char  *b = (u_char *)&acb->cmd;
	struct scsi_link *sc = acb->xs->sc_link;
	int i;

	sc_print_addr(sc);
	if (!(acb->xs->flags & SCSI_RESET)) {
		for (i = 0; i < acb->clen; i++) {
			if (i)
				printf(",");
			printf("%x", b[i]);
		}
		printf("\n");
	} else
		printf("RESET\n");
}

void
aic_print_acb(acb)
	struct acb *acb;
{

	printf("acb@%x xs=%x flags=%x", acb, acb->xs, acb->flags);
	printf(" daddr=%x dleft=%d stat=%x\n",
	    (long)acb->daddr, acb->dleft, acb->stat);
	aic_show_scsi_cmd(acb);
}

void
aic_print_active_acb()
{
	struct acb *acb;
	struct aic_data *aic = aicdata[0];

	printf("ready list:\n");
	for (acb = aic->ready_list.tqh_first; acb; acb = acb->chain.tqe_next)
		aic_print_acb(acb);
	printf("nexus:\n");
	if (aic->nexus)
		aic_print_acb(aic->nexus);
	printf("nexus list:\n");
	for (acb = aic->nexus_list.tqh_first; acb; acb = acb->chain.tqe_next)
		aic_print_acb(acb);
}

void
aic_dump6360()
{
	u_short iobase = 0x340;

	printf("aic6360: SCSISEQ=%x SXFRCTL0=%x SXFRCTL1=%x SCSISIGI=%x\n",
	    inb(SCSISEQ), inb(SXFRCTL0), inb(SXFRCTL1), inb(SCSISIGI));
	printf("         SSTAT0=%x SSTAT1=%x SSTAT2=%x SSTAT3=%x SSTAT4=%x\n",
	    inb(SSTAT0), inb(SSTAT1), inb(SSTAT2), inb(SSTAT3), inb(SSTAT4));
	printf("         SIMODE0=%x SIMODE1=%x DMACNTRL0=%x DMACNTRL1=%x DMASTAT=%x\n",
	    inb(SIMODE0), inb(SIMODE1), inb(DMACNTRL0), inb(DMACNTRL1),
	    inb(DMASTAT));
	printf("         FIFOSTAT=%d SCSIBUS=0x%x\n",
	    inb(FIFOSTAT), inb(SCSIBUS));
}

void
aic_dump_driver()
{
	struct aic_data *aic = aicdata[0];
	struct aic_tinfo *ti;
	int i;

	printf("nexus=%x phase=%x prevphase=%x\n", aic->nexus, aic->phase,
	    aic->prevphase);
	printf("state=%x msgin=%x msgpriq=%x msgout=%x imlen=%d omlen=%d\n",
	    aic->state, aic->imess[0], aic->msgpriq, aic->msgout, aic->imlen,
	    aic->omlen);
	printf("history:");
	i = aic->hp;
	do {
		printf(" %d", aic->history[i]);
		i = (i + 1) % AIC_HSIZE;
	} while (i != aic->hp);
	printf("*\n");
	for (i = 0; i < 7; i++) {
		ti = &aic->tinfo[i];
		printf("tinfo%d: %d cmds %d disconnects %d timeouts",
		    i, ti->cmds, ti->dconns, ti->touts);
		printf(" %d senses flags=%x\n", ti->senses, ti->flags);
	}
}
#endif
