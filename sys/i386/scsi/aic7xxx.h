/*
 * Interface to the generic driver for the aic7xxx based adaptec
 * SCSI controllers.  This is used to implement product specific
 * probe and attach routines.
 *
 * Copyright (c) 1994, 1995, 1996, 1997 Justin T. Gibbs.
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
 *	$Id$
 */

#ifndef _AIC7XXX_H_
#define _AIC7XXX_H_

#if defined(__FreeBSD__)
#include "ahc.h"                /* for NAHC from config */
#include "opt_aic7xxx.h"	/* for config options */
#endif

#if defined(__NetBSD__)
/*
 * convert FreeBSD's <sys/queue.h> symbols to NetBSD's
 */
#define	STAILQ_ENTRY		SIMPLEQ_ENTRY
#define	STAILQ_HEAD		SIMPLEQ_HEAD
#define	STAILQ_INIT		SIMPLEQ_INIT
#define	STAILQ_INSERT_HEAD	SIMPLEQ_INSERT_HEAD
#define	STAILQ_INSERT_TAIL	SIMPLEQ_INSERT_TAIL
#define	STAILQ_REMOVE_HEAD(head, field)	\
	SIMPLEQ_REMOVE_HEAD(head, (head)->sqh_first, field)
#define	stqh_first		sqh_first
#define	stqe_next		sqe_next
#endif

#define	AHC_NSEG	256	/* number of dma segments supported */

#define AHC_SCB_MAX	255	/*
				 * Up to 255 SCBs on some types of aic7xxx
				 * based boards.  The aic7870 have 16 internal
				 * SCBs, but external SRAM bumps this to 255.
				 * The aic7770 family have only 4, and the 
				 * aic7850 has only 3.
				 */


typedef u_int32_t physaddr;
#if defined(__FreeBSD__)
extern u_long ahc_unit;
#endif

struct ahc_dma_seg {
	physaddr	addr;
	u_int32_t	len;
};

typedef enum {
	AHC_NONE	= 0x0000,
	AHC_ULTRA	= 0x0001,	/* Supports 20MHz Transfers */
	AHC_WIDE  	= 0x0002,	/* Wide Channel */
	AHC_TWIN	= 0x0008,	/* Twin Channel */
	AHC_AIC7770	= 0x0010,
	AHC_AIC7850	= 0x0020,
	AHC_AIC7860	= 0x0021,	/* ULTRA version of the aic7850 */
	AHC_AIC7870	= 0x0040,
	AHC_AIC7880	= 0x0041,
	AHC_AIC78X0	= 0x0060,	/* PCI Based Controller */
	AHC_274		= 0x0110,	/* EISA Based Controller */
	AHC_284		= 0x0210,	/* VL/ISA Based Controller */
	AHC_294AU	= 0x0421,	/* aic7860 based '2940' */
	AHC_294		= 0x0440,	/* PCI Based Controller */
	AHC_294U	= 0x0441,	/* ULTRA PCI Based Controller */
	AHC_394		= 0x0840,	/* Twin Channel PCI Controller */
	AHC_394U	= 0x0841,	/* ULTRA, Twin Channel PCI Controller */
	AHC_398		= 0x1040,	/* Multi Channel PCI RAID Controller */
	AHC_398U	= 0x1041,	/* ULTRA, Multi Channel PCI
					 * RAID Controller
					 */
	AHC_39X		= 0x1800	/* Multi Channel PCI Adapter */
}ahc_type;

typedef enum {
	AHC_FNONE		= 0x00,
	AHC_INIT		= 0x01,
	AHC_RUNNING		= 0x02,
	AHC_PAGESCBS		= 0x04,	/* Enable SCB paging */
	AHC_CHANNEL_B_PRIMARY	= 0x08,	/*
					 * On twin channel adapters, probe
					 * channel B first since it is the
					 * primary bus.
					 */
	AHC_USEDEFAULTS		= 0x10,	/*
					 * For cards without an seeprom
					 * or a BIOS to initialize the chip's
					 * SRAM, we use the default target
					 * settings.
					 */
	AHC_CHNLB		= 0x20,	/* 
					 * Second controller on 3940/398X 
					 * Also encodes the offset in the
					 * SEEPROM for CHNLB info (32)
					 */
	AHC_CHNLC               = 0x40  /*
					 * Third controller on 3985
					 * Also encodes the offset in the
					 * SEEPROM for CHNLC info (64)
					 */
} ahc_flag;

typedef enum {
	SCB_FREE		= 0x0000,
	SCB_ACTIVE		= 0x0001,
	SCB_ABORTED		= 0x0002,
	SCB_DEVICE_RESET	= 0x0004,
	SCB_IMMED		= 0x0008,
	SCB_SENSE		= 0x0010,
	SCB_TIMEDOUT		= 0x0020,
	SCB_QUEUED_FOR_DONE	= 0x0040,
	SCB_PAGED_OUT		= 0x0080,
	SCB_WAITINGQ		= 0x0100,
	SCB_ASSIGNEDQ		= 0x0200,
	SCB_SENTORDEREDTAG	= 0x0400,
	SCB_MSGOUT_SDTR		= 0x0800,
	SCB_MSGOUT_WDTR		= 0x1000,
	SCB_ABORT		= 0x2000
} scb_flag;

/*
 * The driver keeps up to MAX_SCB scb structures per card in memory.  The SCB
 * consists of a "hardware SCB" mirroring the fields availible on the card
 * and additional information the kernel stores for each transaction.
 */
struct hardware_scb {
/*0*/   u_int8_t  control;
/*1*/	u_int8_t  tcl;		/* 4/1/3 bits */
/*2*/	u_int8_t  status;
/*3*/	u_int8_t  SG_segment_count;
/*4*/	physaddr  SG_list_pointer;
/*8*/	u_int8_t  residual_SG_segment_count;
/*9*/	u_int8_t  residual_data_count[3];
/*12*/	physaddr  data;
/*16*/	u_int32_t datalen;		/* Really only three bits, but its
					 * faster to treat it as a long on
					 * a quad boundary.
					 */
/*20*/	physaddr  cmdpointer;
/*24*/	u_int8_t  cmdlen;
/*25*/	u_int8_t  tag;			/* Index into our kernel SCB array.
					 * Also used as the tag for tagged I/O
					 */
#define SCB_PIO_TRANSFER_SIZE	26 	/* amount we need to upload/download
					 * via PIO to initialize a transaction.
					 */
/*26*/	u_int8_t  next;			/* Used for threading SCBs in the
					 * "Waiting for Selection" and
					 * "Disconnected SCB" lists down
					 * in the sequencer.
					 */
/*27*/	u_int8_t  prev;

/*28*/	u_int32_t pad;			/*
					 * Unused by the kernel, but we require
					 * the padding so that the array of
					 * hardware SCBs is alligned on 32 byte
					 * boundaries so the sequencer can
					 * index them easily.
					 */
};

struct scb {
	struct	hardware_scb	*hscb;
	STAILQ_ENTRY(scb)	links;	 /* for chaining */
	struct	scsi_xfer	*xs;	 /* the scsi_xfer for this cmd */
	scb_flag		flags;
	struct	ahc_dma_seg 	*ahc_dma;/* Pointer to SG segments */
	struct	scsi_sense	sense_cmd;
	u_int8_t		position;/* Position in card's scbarray */
};

struct scb_data {
	struct	hardware_scb	*hscbs;	    /* Array of hardware SCBs */
	struct	scb *scbarray[AHC_SCB_MAX]; /* Array of kernel SCBs */
	STAILQ_HEAD(, scb) free_scbs;	/*
					 * Pool of SCBs ready to be assigned
					 * commands to execute.
					 */
	u_int8_t	numscbs;
	u_int8_t	maxhscbs;	/* Number of SCBs on the card */
	u_int8_t	maxscbs;	/*
					 * Max SCBs we allocate total including
					 * any that will force us to page SCBs
					 */
};

struct ahc_busreset_args {
	struct	ahc_softc *ahc;
	char	bus;
};

struct ahc_softc {
#if defined(__FreeBSD__)
	int	unit;
#elif defined(__NetBSD__)
	struct device sc_dev;
	void	*sc_ih;
	bus_chipset_tag_t sc_bc;
	bus_io_handle_t sc_ioh;
#endif
	ahc_type type;
	ahc_flag flags;
#if defined(__FreeBSD__)
	u_int32_t	baseport;
#endif
	volatile u_int8_t *maddr;
	struct	scb_data *scb_data;
	struct	ahc_busreset_args	busreset_args;
	struct	ahc_busreset_args	busreset_args_b;
	struct	scsi_link sc_link;
	struct	scsi_link sc_link_b;	/* Second bus for Twin channel cards */
	STAILQ_HEAD(, scb) waiting_scbs;/*
					 * SCBs waiting ready to go but
					 * waiting for space in the QINFIFO.
					 */
	u_int8_t	activescbs;
	u_int16_t	needsdtr_orig;	/* Targets we initiate sync neg with */
	u_int16_t	needwdtr_orig;	/* Targets we initiate wide neg with */
	u_int16_t	needsdtr;	/* Current list of negotiated targets */
	u_int16_t	needwdtr;	/* Current list of negotiated targets */
	u_int16_t	sdtrpending;	/* Pending SDTR to these targets */
	u_int16_t	wdtrpending;	/* Pending WDTR to these targets */
	u_int16_t	tagenable;	/* Targets that can handle tags */
	u_int16_t	orderedtag;	/* Targets to use ordered tag on */
	u_int16_t	discenable;	/* Targets allowed to disconnect */
	u_int8_t	our_id;		/* our scsi id */
	u_int8_t	our_id_b;	/* B channel scsi id */
	u_int8_t	qcntmask;	/*
					 * Mask of valid registers in the
					 * Q*CNT registers.
					 */
	u_int8_t	qfullcount;	/*
					 * The maximum number of entries
					 * storable in the Q*FIFOs.
					 */
	u_int8_t	curqincnt;	/*
					 * The current value we "think" the
					 * QINCNT has.  The reason it is
					 * "think" is that this is a cached
					 * value that is only updated when
					 * curqincount == qfullcount to reduce
					 * the amount of accesses made to the
					 * card.
					 */
	u_int8_t	unpause;
	u_int8_t	pause;
	u_int8_t	in_timeout;
	u_int8_t	in_reset;
#define CHANNEL_A_RESET		0x01
#define CHANNEL_B_RESET		0x02
};

struct full_ahc_softc {
	struct ahc_softc softc;
	struct scb_data  scb_data_storage;
};

/* #define AHC_DEBUG */
#ifdef AHC_DEBUG
/* Different debugging levels used when AHC_DEBUG is defined */
#define AHC_SHOWMISC	0x0001
#define AHC_SHOWCMDS	0x0002
#define AHC_SHOWSCBS	0x0004
#define AHC_SHOWABORTS	0x0008
#define AHC_SHOWSENSE	0x0010
#define AHC_SHOWSCBCNT	0x0020

extern int ahc_debug; /* Initialized in i386/scsi/aic7xxx.c */
#endif

#if defined(__FreeBSD__)

char *ahc_name __P((struct ahc_softc *ahc));

struct ahc_softc *ahc_alloc __P((int unit, u_int32_t io_base,
				 vm_offset_t maddr, ahc_type type,
				 ahc_flag flags, struct scb_data *scb_data));
#elif defined(__NetBSD__)

#define	ahc_name(ahc)	(ahc)->sc_dev.dv_xname

void ahc_construct __P((struct ahc_softc *ahc, bus_chipset_tag_t bc, bus_io_handle_t ioh, ahc_type type, ahc_flag flags));
#endif
void ahc_reset __P((struct ahc_softc *ahc));
void ahc_free __P((struct ahc_softc *));
int ahc_init __P((struct ahc_softc *));
int ahc_attach __P((struct ahc_softc *));
#if defined(__FreeBSD__)
void ahc_intr __P((void *arg));
#elif defined(__NetBSD__)
int ahc_intr __P((void *arg));
#endif

#if defined(__FreeBSD__)
static __inline u_int8_t ahc_inb __P((struct ahc_softc *ahc, u_int32_t port));
static __inline void ahc_outb __P((struct ahc_softc *ahc, u_int32_t port,
				   u_int8_t val));
static __inline void ahc_outsb __P((struct ahc_softc *ahc, u_int32_t port,
				     u_int8_t *valp, size_t size));

static __inline u_int8_t
ahc_inb(ahc, port)
	struct ahc_softc *ahc;
	u_int32_t port;
{
	if (ahc->maddr != NULL)
		return ahc->maddr[port];
	else
		return inb(ahc->baseport + port);
}

static __inline void
ahc_outb(ahc, port, val)
	struct ahc_softc *ahc;
	u_int32_t port;
	u_int8_t val;
{
	if (ahc->maddr != NULL)
		ahc->maddr[port] = val;
	else
		outb(ahc->baseport + port, val);
}

static __inline void
ahc_outsb(ahc, port, valp, size)
	struct ahc_softc *ahc;
	u_int32_t port;
	u_int8_t *valp;
	size_t size;
{
	if (ahc->maddr != NULL) {
		__asm __volatile("
			cld;
		1:	lodsb;
			movb %%al,(%0);
			loop 1b"			:
							:
			"r" ((ahc)->maddr + (port)),
			"S" ((valp)), "c" ((size))	:
			"%esi", "%ecx", "%eax");
	} else
		outsb(ahc->baseport + port, valp, size);
}
#elif defined(__NetBSD__)
#define	ahc_inb(ahc, port)	\
	bus_io_read_1((ahc)->sc_bc, (ahc)->sc_ioh, port)
#define	ahc_outb(ahc, port, val)	\
	bus_io_write_1((ahc)->sc_bc, (ahc)->sc_ioh, port, val)
#define	ahc_outsb(ahc, port, valp, size)	\
	bus_io_write_multi_1((ahc)->sc_bc, (ahc)->sc_ioh, port, valp, size)
#endif

#endif  /* _AIC7XXX_H_ */
