/*
 * Interface to the generic driver for the aic7xxx based adaptec
 * SCSI controllers.  This is used to implement product specific
 * probe and attach routines.
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
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    Justin T. Gibbs.
 * 4. Modifications may be freely made to this file if the above conditions
 *    are met.
 *
 *	$Id: aic7xxx.h,v 1.23 1996/03/31 03:15:31 gibbs Exp $
 */

#ifndef _AIC7XXX_H_
#define _AIC7XXX_H_

#include "ahc.h"                /* for NAHC from config */

#define	AHC_NSEG	256	/* number of dma segments supported */

#define AHC_SCB_MAX	255	/*
				 * Up to 255 SCBs on some types of aic7xxx
				 * based boards.  The aic7870 have 16 internal
				 * SCBs, but external SRAM bumps this to 255.
				 * The aic7770 family have only 4, and the 
				 * aic7850 has only 3.
				 */

/* #define AHCDEBUG */

extern int bootverbose;
typedef unsigned long int physaddr;
extern u_long ahc_unit;

struct ahc_dma_seg {
        physaddr addr;
            long len;
};

typedef enum {
	AHC_NONE	= 0x000,
	AHC_ULTRA	= 0x001,	/* Supports 20MHz Transfers */
	AHC_WIDE  	= 0x002,	/* Wide Channel */
	AHC_TWIN	= 0x008,	/* Twin Channel */
	AHC_AIC7770	= 0x010,
	AHC_AIC7850	= 0x020,
	AHC_AIC7870	= 0x040,
	AHC_AIC7880	= 0x041,
	AHC_AIC78X0	= 0x060,	/* PCI Based Controller */
	AHC_274		= 0x110,	/* EISA Based Controller */
	AHC_284		= 0x210,	/* VL/ISA Based Controller */
	AHC_294		= 0x440,	/* PCI Based Controller */
	AHC_294U	= 0x441,	/* ULTRA PCI Based Controller */
	AHC_394		= 0x840,	/* Twin Channel PCI Controller */
	AHC_394U	= 0x841,	/* Twin, ULTRA Channel PCI Controller */
}ahc_type;

typedef enum {
	AHC_FNONE	= 0x00,
	AHC_INIT	= 0x01,
	AHC_RUNNING	= 0x02,
	AHC_USEDEFAULTS = 0x04,		/*
					 * For cards without an seeprom
					 * or a BIOS to initialize the chip's
					 * SRAM, we use the default chip and
					 * target settings.
					 */
	AHC_EXTSCB	= 0x10,		/* External SCBs present */
	AHC_CHNLB	= 0x20,		/* 
					 * Second controller on 3940 
					 * Also encodes the offset in the
					 * SEEPROM for CHNLB info (32)
					 */
}ahc_flag;

/*
 * The driver keeps up to MAX_SCB scb structures per card in memory.  Only the
 * first 26 bytes of the structure need to be transfered to the card during
 * normal operation.  The remaining fields (next_waiting and host_scb) are
 * initialized the first time an SCB is allocated in get_scb().  The fields
 * starting at byte 32 are used for kernel level bookkeeping.  
 */
struct scb {
/* ------------    Begin hardware supported fields    ---------------- */
/*0*/   u_char control;
/*1*/	u_char target_channel_lun;		/* 4/1/3 bits */
/*2*/	u_char target_status;
/*3*/	u_char SG_segment_count;
/*4*/	physaddr SG_list_pointer;
/*8*/	u_char residual_SG_segment_count;
/*9*/	u_char residual_data_count[3];
/*12*/	physaddr data;
/*16*/  u_long datalen;			/* Really only three bits, but its
					 * faster to treat it as a long on
					 * a quad boundary.
					 */
/*20*/	physaddr cmdpointer;
/*24*/	u_char cmdlen;
#define SCB_PIO_TRANSFER_SIZE	25 	/* amount we need to upload/download
					 * via PIO to initialize a transaction.
					 */
/*25*/	u_char next_waiting;		/* Used to thread SCBs awaiting
					 * selection
					 */
/*-----------------end of hardware supported fields----------------*/
	SLIST_ENTRY(scb)	next;	/* in free list */
	struct scsi_xfer *xs;	/* the scsi_xfer for this cmd */
	int	flags;
#define	SCB_FREE		0x00
#define	SCB_ACTIVE		0x01
#define	SCB_ABORTED		0x02
#define	SCB_DEVICE_RESET	0x04
#define	SCB_IMMED		0x08
#define	SCB_SENSE		0x10
#define	SCB_TIMEDOUT		0x20
#define	SCB_QUEUED_FOR_DONE	0x40
	int	position;	/* Position in scbarray */
	struct ahc_dma_seg ahc_dma[AHC_NSEG] __attribute__ ((packed));
	struct scsi_sense sense_cmd;	/* SCSI command block */
};

struct ahc_data {
	int	unit;
	ahc_type type;
	ahc_flag flags;
	u_long	baseport;
	struct	scb *scbarray[AHC_SCB_MAX]; /* Mirror boards scbarray */
	SLIST_HEAD(, scb) free_scb;
	int	our_id;			/* our scsi id */
	int	our_id_b;		/* B channel scsi id */
	int	vect;
	struct	scb *immed_ecb;		/* an outstanding immediate command */
	struct	scsi_link sc_link;
	struct	scsi_link sc_link_b;	/* Second bus for Twin channel cards */
	u_short	needsdtr_orig;		/* Targets we initiate sync neg with */
	u_short	needwdtr_orig;		/* Targets we initiate wide neg with */
	u_short	needsdtr;		/* Current list of negotiated targets */
	u_short needwdtr;		/* Current list of negotiated targets */
	u_short sdtrpending;		/* Pending SDTR to these targets */
	u_short wdtrpending;		/* Pending WDTR to these targets */
	u_short	tagenable;		/* Targets that can handle tagqueing */
	u_short	discenable;		/* Targets allowed to disconnect */
	int	numscbs;
	int	activescbs;
	u_char	maxscbs;
	u_char	qcntmask;
	u_char	unpause;
	u_char	pause;
	u_char	in_timeout;
};

/* Different debugging levels used when AHC_DEBUG is defined */
#define AHC_SHOWMISC	0x0001
#define AHC_SHOWCMDS	0x0002
#define AHC_SHOWSCBS	0x0004
#define AHC_SHOWABORTS	0x0008
#define AHC_SHOWSENSE	0x0010
#define AHC_SHOWSCBCNT	0x0020
/* #define AHC_DEBUG */

extern int ahc_debug; /* Initialized in i386/scsi/aic7xxx.c */

void ahc_reset __P((u_long iobase));
struct ahc_data *ahc_alloc __P((int unit, u_long io_base, ahc_type type, ahc_flag flags));
void ahc_free __P((struct ahc_data *));
int ahc_init __P((struct ahc_data *));
int ahc_attach __P((struct ahc_data *));
void ahc_intr __P((void *arg));

#endif  /* _AIC7XXX_H_ */
