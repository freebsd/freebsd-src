/*
 * Interface to the generic driver for the aic7xxx based adaptec 
 * SCSI controllers.  This is used to implement product specific 
 * probe and attach routines.
 *
 * Copyright (c) 1994, 1995 Justin T. Gibbs.
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
 *	$Id: aic7xxx.h,v 1.4 1995/02/22 01:43:25 gibbs Exp $
 */

#ifndef _AIC7XXX_H_
#define _AIC7XXX_H_

#include "ahc.h"                /* for NAHC from config */

#define	AHC_NSEG	256	/* number of dma segments supported */

#define AHC_SCB_MAX	16	/*
				 * Up to 16 SCBs on some types of aic7xxx based
				 * boards.  The aic7770 family only have 4
				 */

/* #define AHCDEBUG */

typedef unsigned long int physaddr;
extern int  ahc_unit;

struct ahc_dma_seg {
        physaddr addr;
            long len;
};
 
typedef u_char ahc_type;
#define	AHC_NONE	0x00
#define	AHC_WIDE	0x02	/* Wide Channel */
#define AHC_TWIN	0x08	/* Twin Channel */
#define	AHC_274		0x10	/* EISA Based Controller */
#define	AHC_284		0x20	/* VL/ISA Based Controller */
#define	AHC_AIC7870	0x40	/* PCI Based Controller */
#define	AHC_294		0xc0	/* PCI Based Controller */

/*
 * The driver keeps up to MAX_SCB scb structures per card in memory.  Only the
 * first 26 bytes of the structure are valid for the hardware, the rest used
 * for driver level bookeeping.  The "__attribute ((packed))" tags ensure that
 * gcc does not attempt to pad the long ints in the structure to word
 * boundaries since the first 26 bytes of this structure must have the correct
 * offsets for the hardware to find them.  The driver is further optimized
 * so that we only have to download the first 19 bytes since as long
 * as we always use S/G, the last fields should be zero anyway.  
 */
struct scb {
/* ------------    Begin hardware supported fields    ---------------- */
/*1*/   u_char control;
#define	SCB_NEEDWDTR 0x80			/* Initiate Wide Negotiation */
#define SCB_NEEDSDTR 0x40			/* Initiate Sync Negotiation */
#define	SCB_TE	     0x20			/* Tag enable */
#define	SCB_NEEDDMA  0x08			/* SCB needs to be DMA'd from
						 * from host memory
						 */
#define	SCB_DIS 0x04
#define	SCB_TAG_TYPE 0x3
#define		SIMPLE_QUEUE 0x0
#define		HEAD_QUEUE 0x1
#define		OR_QUEUE 0x2
/*2*/	u_char target_channel_lun;		/* 4/1/3 bits */
/*3*/	u_char SG_segment_count;
/*7*/	physaddr SG_list_pointer	__attribute__ ((packed));
/*11*/	physaddr cmdpointer		__attribute__ ((packed));
/*12*/	u_char cmdlen;
/*14*/	u_char RESERVED[2];			/* must be zero */
/*15*/	u_char target_status;
/*18*/	u_char residual_data_count[3];
/*19*/	u_char residual_SG_segment_count;
#define	SCB_DOWN_SIZE 19		/* amount to actually download */
/*23*/	physaddr data			 __attribute__ ((packed));
/*26*/  u_char datalen[3];
#define SCB_UP_SIZE 26			/* amount to actually upload */
/*30*/	physaddr host_scb			 __attribute__ ((packed));
#if 0
	/*
	 *  No real point in transferring this to the
	 *  SCB registers.
	*/
	unsigned char RESERVED[2];
#endif
	/*-----------------end of hardware supported fields----------------*/
	struct scb *next;	/* in free list */
	struct scsi_xfer *xs;	/* the scsi_xfer for this cmd */
	int	flags;
#define	SCB_FREE	0x00
#define	SCB_ACTIVE	0x01
#define	SCB_ABORTED	0x02
#define	SCB_IMMED	0x04
#define	SCB_IMMED_FAIL	0x08
#define	SCB_SENSE	0x10
	int	position;	/* Position in scbarray */
	struct ahc_dma_seg ahc_dma[AHC_NSEG] __attribute__ ((packed));
	struct scsi_sense sense_cmd;	/* SCSI command block */
};
 
struct ahc_data {
	ahc_type type;
	int      flags;
#define AHC_INIT	0x01
#define AHC_RUNNING	0x02
	u_long	baseport;
	struct	scb *scbarray[AHC_SCB_MAX]; /* Mirror boards scbarray */
	struct	scb *free_scb;
	int	our_id;			/* our scsi id */
	int	our_id_b;		/* B channel scsi id */
	int	vect;
	struct	scb *immed_ecb;		/* an outstanding immediete command */
	struct	scsi_link sc_link;
	struct	scsi_link sc_link_b;	/* Second bus for Twin channel cards */
	u_short	needsdtr_orig;		/* Targets we initiate sync neg with */
	u_short	needwdtr_orig;		/* Targets we initiate wide neg with */
	u_short	needsdtr;		/* Current list of negotiated targets */
	u_short needwdtr;		/* Current list of negotiated targets */
	u_short sdtrpending;		/* Pending SDTR to these targets */
	u_short wdtrpending;		/* Pending WDTR to these targets */
	u_short	tagenable;		/* Targets that can handle tagqueing */
	int	numscbs;
	u_char	maxscbs;
	u_char	unpause;
	u_char	pause;
};

extern struct ahc_data *ahcdata[NAHC];

int ahcprobe __P((int unit, u_long io_base, ahc_type type));
int ahc_attach __P((int unit));
int ahcintr();

#endif  /* _AIC7XXX_H_ */
