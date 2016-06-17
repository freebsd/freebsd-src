/* $Id: tpqic02.h,v 1.5 1996/12/14 23:01:38 root Exp root $
 *
 * Include file for QIC-02 driver for Linux.
 *
 * Copyright (c) 1992--1995 by H. H. Bergman. All rights reserved.
 *
 * ******* USER CONFIG SECTION BELOW (Near line 70) *******
 */

#ifndef _LINUX_TPQIC02_H
#define _LINUX_TPQIC02_H

#include <linux/config.h>

#if CONFIG_QIC02_TAPE || CONFIG_QIC02_TAPE_MODULE

/* need to have QIC02_TAPE_DRIVE and QIC02_TAPE_IFC expand to something */
#include <linux/mtio.h>


/* Make QIC02_TAPE_IFC expand to something.
 *
 * The only difference between WANGTEK and EVEREX is in the 
 * handling of the DMA channel 3.
 * Note that the driver maps EVEREX to WANGTEK internally for speed
 * reasons. Externally WANGTEK==1, EVEREX==2, ARCHIVE==3.
 * These must correspond to the values used in qic02config(1).
 *
 * Support for Mountain controllers was added by Erik Jacobson
 * and severely hacked by me.   -- hhb
 * 
 * Support for Emerald controllers by Alan Bain <afrb2@chiark.chu.cam.ac.uk>
 * with more hacks by me.   -- hhb
 */
#define WANGTEK		1		   /* don't know about Wangtek QIC-36 */
#define EVEREX		(WANGTEK+1)  /* I heard *some* of these are identical */
#define EVEREX_811V	EVEREX			      /* With TEAC MT 2ST 45D */
#define EVEREX_831V	EVEREX
#define ARCHIVE		3
#define ARCHIVE_SC400	ARCHIVE	       /* rumoured to be from the pre-SMD-age */
#define ARCHIVE_SC402	ARCHIVE		       /* don't know much about SC400 */
#define ARCHIVE_SC499	ARCHIVE       /* SC402 and SC499R should be identical */

#define MOUNTAIN	5		       /* Mountain Computer Interface */
#define EMERALD		6		       /* Emerald Interface card */



#define QIC02_TAPE_PORT_RANGE 	8	 /* number of IO locations to reserve */


/*********** START OF USER CONFIGURABLE SECTION ************/

/* Tape configuration: Select DRIVE, IFC, PORT, IRQ and DMA below.
 * Runtime (re)configuration is not supported yet.
 *
 * Tape drive configuration:	(MT_IS* constants are defined in mtio.h)
 *
 * QIC02_TAPE_DRIVE = MT_ISWT5150
 *	- Wangtek 5150, format: up to QIC-150.
 * QIC02_TAPE_DRIVE = MT_ISQIC02_ALL_FEATURES
 *	- Enables some optional QIC02 commands that some drives may lack.
 *	  It is provided so you can check which are supported by your drive.
 *	  Refer to tpqic02.h for others.
 *
 * Supported interface cards: QIC02_TAPE_IFC =
 *	WANGTEK,
 *	ARCHIVE_SC402, ARCHIVE_SC499.	(both same programming interface)
 *
 * Make sure you have the I/O ports/DMA channels 
 * and IRQ stuff configured properly!
 * NOTE: There may be other device drivers using the same major
 *       number. This must be avoided. Check for timer.h conflicts too.
 *
 * If you have an EVEREX EV-831 card and you are using DMA channel 3,
 * you will probably have to ``#define QIC02_TAPE_DMA3_FIX'' below.
 */

/* CONFIG_QIC02_DYNCONF can be defined in autoconf.h, by `make config' */

/*** #undef CONFIG_QIC02_DYNCONF ***/

#ifndef CONFIG_QIC02_DYNCONF

#define QIC02_TAPE_DRIVE	MT_ISQIC02_ALL_FEATURES	 /* drive type */
/* #define QIC02_TAPE_DRIVE	MT_ISWT5150 */
/* #define QIC02_TAPE_DRIVE	MT_ISARCHIVE_5945L2 */
/* #define QIC02_TAPE_DRIVE	MT_ISTEAC_MT2ST */
/* #define QIC02_TAPE_DRIVE	MT_ISARCHIVE_2150L */
/* #define QIC02_TAPE_DRIVE	MT_ISARCHIVESC499 */

/* Either WANGTEK, ARCHIVE or MOUNTAIN. Not EVEREX. 
 * If you have an EVEREX, use WANGTEK and try the DMA3_FIX below.
 */
#define QIC02_TAPE_IFC		WANGTEK	/* interface card type */
/* #define QIC02_TAPE_IFC		ARCHIVE */
/* #define QIC02_TAPE_IFC		MOUNTAIN */

#define QIC02_TAPE_PORT 	0x300	/* controller port address */
#define QIC02_TAPE_IRQ		5	/* For IRQ2, use 9 here, others normal. */
#define QIC02_TAPE_DMA		1	/* either 1 or 3, because 2 is used by the floppy */

/* If DMA3 doesn't work, but DMA1 does, and you have a 
 * Wangtek/Everex card, you can try #define-ing the flag
 * below. Note that you should also change the DACK jumper
 * for Wangtek/Everex cards when changing the DMA channel.
 */
#undef QIC02_TAPE_DMA3_FIX

/************ END OF USER CONFIGURABLE SECTION *************/

/* I put the stuff above in config.in, but a few recompiles, to
 * verify different configurations, and several days later I decided
 * to change it back again.
 */



/* NOTE: TP_HAVE_DENS should distinguish between available densities (?)
 * NOTE: Drive select is not implemented -- I have only one tape streamer,
 *	 so I'm unable and unmotivated to test and implement that. ;-) ;-)
 */
#if QIC02_TAPE_DRIVE == MT_ISWT5150
#define TP_HAVE_DENS	1
#define TP_HAVE_BSF	0	/* nope */
#define TP_HAVE_FSR	0	/* nope */
#define TP_HAVE_BSR	0	/* nope */
#define TP_HAVE_EOD	0	/* most of the time */
#define TP_HAVE_SEEK	0
#define TP_HAVE_TELL	0
#define TP_HAVE_RAS1	1
#define TP_HAVE_RAS2	1

#elif QIC02_TAPE_DRIVE == MT_ISARCHIVESC499	/* Archive SC-499 QIC-36 controller */
#define TP_HAVE_DENS	1	/* can do set density (QIC-11 / QIC-24) */
#define TP_HAVE_BSF	0
#define TP_HAVE_FSR	1	/* can skip one block forwards */
#define TP_HAVE_BSR	1	/* can skip one block backwards */
#define TP_HAVE_EOD	1	/* can seek to end of recorded data */
#define TP_HAVE_SEEK	0
#define TP_HAVE_TELL	0
#define TP_HAVE_RAS1	1	/* can run selftest 1 */
#define TP_HAVE_RAS2	1	/* can run selftest 2 */
/* These last two selftests shouldn't be used yet! */

#elif (QIC02_TAPE_DRIVE == MT_ISARCHIVE_2060L) || (QIC02_TAPE_DRIVE == MT_ISARCHIVE_2150L)
#define TP_HAVE_DENS	1	/* can do set density (QIC-24 / QIC-120 / QIC-150) */
#define TP_HAVE_BSF	0
#define TP_HAVE_FSR	1	/* can skip one block forwards */
#define TP_HAVE_BSR	1	/* can skip one block backwards */
#define TP_HAVE_EOD	1	/* can seek to end of recorded data */
#define TP_HAVE_TELL	1	/* can read current block address */
#define TP_HAVE_SEEK	1	/* can seek to block */
#define TP_HAVE_RAS1	1	/* can run selftest 1 */
#define TP_HAVE_RAS2	1	/* can run selftest 2 */
/* These last two selftests shouldn't be used yet! */

#elif QIC02_TAPE_DRIVE == MT_ISARCHIVE_5945L2
/* can anyone verify this entry?? */
#define TP_HAVE_DENS	1	/* can do set density?? (QIC-24??) */
#define TP_HAVE_BSF	0
#define TP_HAVE_FSR	1	/* can skip one block forwards */
#define TP_HAVE_BSR	1	/* can skip one block backwards */
#define TP_HAVE_EOD	1	/* can seek to end of recorded data */
#define TP_HAVE_TELL	1	/* can read current block address */
#define TP_HAVE_SEEK	1	/* can seek to block */
#define TP_HAVE_RAS1	1	/* can run selftest 1 */
#define TP_HAVE_RAS2	1	/* can run selftest 2 */
/* These last two selftests shouldn't be used yet! */

#elif QIC02_TAPE_DRIVE == MT_ISTEAC_MT2ST
/* can anyone verify this entry?? */
#define TP_HAVE_DENS	0	/* cannot do set density?? (QIC-150?) */
#define TP_HAVE_BSF	0
#define TP_HAVE_FSR	1	/* can skip one block forwards */
#define TP_HAVE_BSR	1	/* can skip one block backwards */
#define TP_HAVE_EOD	1	/* can seek to end of recorded data */
#define TP_HAVE_SEEK	1	/* can seek to block */
#define TP_HAVE_TELL	1	/* can read current block address */
#define TP_HAVE_RAS1	1	/* can run selftest 1 */
#define TP_HAVE_RAS2	1	/* can run selftest 2 */
/* These last two selftests shouldn't be used yet! */

#elif QIC02_TAPE_DRIVE == MT_ISQIC02_ALL_FEATURES
#define TP_HAVE_DENS	1	/* can do set density */
#define TP_HAVE_BSF	1	/* can search filemark backwards */
#define TP_HAVE_FSR	1	/* can skip one block forwards */
#define TP_HAVE_BSR	1	/* can skip one block backwards */
#define TP_HAVE_EOD	1	/* can seek to end of recorded data */
#define TP_HAVE_SEEK	1	/* seek to block address */
#define TP_HAVE_TELL	1	/* tell current block address */
#define TP_HAVE_RAS1	1	/* can run selftest 1 */
#define TP_HAVE_RAS2	1	/* can run selftest 2 */
/* These last two selftests shouldn't be used yet! */


#else
#error No QIC-02 tape drive type defined!
/* If your drive is not listed above, first try the 'ALL_FEATURES',
 * to see what commands are supported, then create your own entry in
 * the list above. You may want to mail it to me, so that I can include
 * it in the next release.
 */
#endif

#endif /* !CONFIG_QIC02_DYNCONF */


/* WANGTEK interface card specifics */
#define WT_QIC02_STAT_PORT	(QIC02_TAPE_PORT)
#define WT_QIC02_CTL_PORT	(QIC02_TAPE_PORT)
#define WT_QIC02_CMD_PORT	(QIC02_TAPE_PORT+1)
#define WT_QIC02_DATA_PORT	(QIC02_TAPE_PORT+1)

/* status register bits (Active LOW!) */
#define WT_QIC02_STAT_POLARITY	0
#define WT_QIC02_STAT_READY	0x01
#define WT_QIC02_STAT_EXCEPTION	0x02
#define WT_QIC02_STAT_MASK	(WT_QIC02_STAT_READY|WT_QIC02_STAT_EXCEPTION)

#define WT_QIC02_STAT_RESETMASK	0x07
#define WT_QIC02_STAT_RESETVAL	(WT_QIC02_STAT_RESETMASK & ~WT_QIC02_STAT_EXCEPTION)

/* controller register (QIC02_CTL_PORT) bits */
#define WT_QIC02_CTL_RESET	0x02
#define WT_QIC02_CTL_REQUEST	0x04
#define WT_CTL_ONLINE		0x01
#define WT_CTL_CMDOFF		0xC0 

#define WT_CTL_DMA3		0x10			  /* enable dma chan3 */
#define WT_CTL_DMA1		0x08	         /* enable dma chan1 or chan2 */

/* EMERALD interface card specifics
 * Much like Wangtek, only different polarity and bit locations
 */
#define EMR_QIC02_STAT_PORT	(QIC02_TAPE_PORT)
#define EMR_QIC02_CTL_PORT	(QIC02_TAPE_PORT)
#define EMR_QIC02_CMD_PORT	(QIC02_TAPE_PORT+1)
#define EMR_QIC02_DATA_PORT	(QIC02_TAPE_PORT+1)

/* status register bits (Active High!) */
#define EMR_QIC02_STAT_POLARITY		1
#define EMR_QIC02_STAT_READY		0x01
#define EMR_QIC02_STAT_EXCEPTION	0x02
#define EMR_QIC02_STAT_MASK	(EMR_QIC02_STAT_READY|EMR_QIC02_STAT_EXCEPTION)

#define EMR_QIC02_STAT_RESETMASK	0x07
#define EMR_QIC02_STAT_RESETVAL	(EMR_QIC02_STAT_RESETMASK & ~EMR_QIC02_STAT_EXCEPTION)

/* controller register (QIC02_CTL_PORT) bits */
#define EMR_QIC02_CTL_RESET	0x02
#define EMR_QIC02_CTL_REQUEST	0x04
#define EMR_CTL_ONLINE		0x01
#define EMR_CTL_CMDOFF		0xC0 

#define EMR_CTL_DMA3		0x10			  /* enable dma chan3 */
#define EMR_CTL_DMA1		0x08	         /* enable dma chan1 or chan2 */



/* ARCHIVE interface card specifics */
#define AR_QIC02_STAT_PORT	(QIC02_TAPE_PORT+1)
#define AR_QIC02_CTL_PORT	(QIC02_TAPE_PORT+1)
#define AR_QIC02_CMD_PORT	(QIC02_TAPE_PORT)
#define AR_QIC02_DATA_PORT	(QIC02_TAPE_PORT)

#define AR_START_DMA_PORT	(QIC02_TAPE_PORT+2)
#define AR_RESET_DMA_PORT	(QIC02_TAPE_PORT+3)

/* STAT port bits */
#define AR_QIC02_STAT_POLARITY	0
#define AR_STAT_IRQF		0x80	/* active high, interrupt request flag */
#define AR_QIC02_STAT_READY	0x40	/* active low */
#define AR_QIC02_STAT_EXCEPTION	0x20	/* active low */
#define AR_QIC02_STAT_MASK	(AR_QIC02_STAT_READY|AR_QIC02_STAT_EXCEPTION)
#define AR_STAT_DMADONE		0x10	/* active high, DMA done */
#define AR_STAT_DIRC		0x08	/* active high, direction */

#define AR_QIC02_STAT_RESETMASK	0x70	/* check RDY,EXC,DMADONE */
#define AR_QIC02_STAT_RESETVAL	((AR_QIC02_STAT_RESETMASK & ~AR_STAT_IRQF & ~AR_QIC02_STAT_EXCEPTION) | AR_STAT_DMADONE)

/* CTL port bits */
#define AR_QIC02_CTL_RESET	0x80	/* drive reset */
#define AR_QIC02_CTL_REQUEST	0x40	/* notify of new command */
#define AR_CTL_IEN		0x20	/* interrupt enable */
#define AR_CTL_DNIEN		0x10	/* done-interrupt enable */
  /* Note: All of these bits are cleared automatically when writing to
   * AR_RESET_DMA_PORT. So AR_CTL_IEN and AR_CTL_DNIEN must be
   * reprogrammed before the write to AR_START_DMA_PORT.
   */


/* MOUNTAIN interface specifics */
#define MTN_QIC02_STAT_PORT	(QIC02_TAPE_PORT+1)
#define MTN_QIC02_CTL_PORT	(QIC02_TAPE_PORT+1)
#define MTN_QIC02_CMD_PORT	(QIC02_TAPE_PORT)
#define MTN_QIC02_DATA_PORT	(QIC02_TAPE_PORT)

#define MTN_W_SELECT_DMA_PORT	(QIC02_TAPE_PORT+2)
#define MTN_R_DESELECT_DMA_PORT	(QIC02_TAPE_PORT+2)
#define MTN_W_DMA_WRITE_PORT	(QIC02_TAPE_PORT+3)

/* STAT port bits */
#define MTN_QIC02_STAT_POLARITY	 0
#define MTN_QIC02_STAT_READY	 0x02	/* active low */
#define MTN_QIC02_STAT_EXCEPTION 0x04	/* active low */
#define MTN_QIC02_STAT_MASK	 (MTN_QIC02_STAT_READY|MTN_QIC02_STAT_EXCEPTION)
#define MTN_STAT_DMADONE	 0x01	/* active high, DMA done */

#define MTN_QIC02_STAT_RESETMASK 0x07	/* check RDY,EXC,DMADONE */
#define MTN_QIC02_STAT_RESETVAL	 ((MTN_QIC02_STAT_RESETMASK & ~MTN_QIC02_STAT_EXCEPTION) | MTN_STAT_DMADONE)

/* CTL port bits */
#define MTN_QIC02_CTL_RESET_NOT	 0x80	/* drive reset, active low */
#define MTN_QIC02_CTL_RESET	 0x80	/* Fodder #definition to keep gcc happy */

#define MTN_QIC02_CTL_ONLINE	 0x40	/* Put drive on line  */
#define MTN_QIC02_CTL_REQUEST	 0x20	/* notify of new command */
#define MTN_QIC02_CTL_IRQ_DRIVER 0x10	/* Enable IRQ tristate driver */
#define MTN_QIC02_CTL_DMA_DRIVER 0x08	/* Enable DMA tristate driver */
#define MTN_CTL_EXC_IEN		 0x04	/* Exception interrupt enable */
#define MTN_CTL_RDY_IEN		 0x02	/* Ready interrupt enable */
#define MTN_CTL_DNIEN		 0x01	/* done-interrupt enable */

#define MTN_CTL_ONLINE		(MTN_QIC02_CTL_RESET_NOT | MTN_QIC02_CTL_IRQ_DRIVER | MTN_QIC02_CTL_DMA_DRIVER)


#ifndef CONFIG_QIC02_DYNCONF

# define QIC02_TAPE_DEBUG	(qic02_tape_debug)

# if QIC02_TAPE_IFC == WANGTEK	
#  define QIC02_STAT_POLARITY	WT_QIC02_STAT_POLARITY
#  define QIC02_STAT_PORT	WT_QIC02_STAT_PORT
#  define QIC02_CTL_PORT	WT_QIC02_CTL_PORT
#  define QIC02_CMD_PORT	WT_QIC02_CMD_PORT
#  define QIC02_DATA_PORT	WT_QIC02_DATA_PORT

#  define QIC02_STAT_READY	WT_QIC02_STAT_READY
#  define QIC02_STAT_EXCEPTION	WT_QIC02_STAT_EXCEPTION
#  define QIC02_STAT_MASK	WT_QIC02_STAT_MASK
#  define QIC02_STAT_RESETMASK	WT_QIC02_STAT_RESETMASK
#  define QIC02_STAT_RESETVAL	WT_QIC02_STAT_RESETVAL

#  define QIC02_CTL_RESET	WT_QIC02_CTL_RESET
#  define QIC02_CTL_REQUEST	WT_QIC02_CTL_REQUEST

#  if QIC02_TAPE_DMA == 3
#   ifdef QIC02_TAPE_DMA3_FIX
#    define WT_CTL_DMA		WT_CTL_DMA1
#   else
#    define WT_CTL_DMA		WT_CTL_DMA3
#   endif
#  elif QIC02_TAPE_DMA == 1
#    define WT_CTL_DMA		WT_CTL_DMA1
#  else
#   error Unsupported or incorrect DMA configuration.
#  endif

# elif QIC02_TAPE_IFC == EMERALD
#  define QIC02_STAT_POLARITY	EMR_QIC02_STAT_POLARITY
#  define QIC02_STAT_PORT	EMR_QIC02_STAT_PORT
#  define QIC02_CTL_PORT	EMR_QIC02_CTL_PORT
#  define QIC02_CMD_PORT	EMR_QIC02_CMD_PORT
#  define QIC02_DATA_PORT	EMR_QIC02_DATA_PORT

#  define QIC02_STAT_READY	EMR_QIC02_STAT_READY
#  define QIC02_STAT_EXCEPTION	EMR_QIC02_STAT_EXCEPTION
#  define QIC02_STAT_MASK	EMR_QIC02_STAT_MASK
#  define QIC02_STAT_RESETMASK	EMR_QIC02_STAT_RESETMASK
#  define QIC02_STAT_RESETVAL	EMR_QIC02_STAT_RESETVAL

#  define QIC02_CTL_RESET	EMR_QIC02_CTL_RESET
#  define QIC02_CTL_REQUEST	EMR_QIC02_CTL_REQUEST

#  if QIC02_TAPE_DMA == 3
#   ifdef QIC02_TAPE_DMA3_FIX
#    define EMR_CTL_DMA		EMR_CTL_DMA1
#   else
#    define EMR_CTL_DMA		EMR_CTL_DMA3
#   endif
#  elif QIC02_TAPE_DMA == 1
#    define EMR_CTL_DMA		EMR_CTL_DMA1
#  else
#   error Unsupported or incorrect DMA configuration.
#  endif

# elif QIC02_TAPE_IFC == ARCHIVE
#  define QIC02_STAT_POLARITY	AR_QIC02_STAT_POLARITY
#  define QIC02_STAT_PORT	AR_QIC02_STAT_PORT
#  define QIC02_CTL_PORT	AR_QIC02_CTL_PORT
#  define QIC02_CMD_PORT	AR_QIC02_CMD_PORT
#  define QIC02_DATA_PORT	AR_QIC02_DATA_PORT

#  define QIC02_STAT_READY	AR_QIC02_STAT_READY
#  define QIC02_STAT_EXCEPTION	AR_QIC02_STAT_EXCEPTION
#  define QIC02_STAT_MASK	AR_QIC02_STAT_MASK
#  define QIC02_STAT_RESETMASK	AR_QIC02_STAT_RESETMASK
#  define QIC02_STAT_RESETVAL	AR_QIC02_STAT_RESETVAL

#  define QIC02_CTL_RESET	AR_QIC02_CTL_RESET
#  define QIC02_CTL_REQUEST	AR_QIC02_CTL_REQUEST

#  if QIC02_TAPE_DMA > 3	/* channel 2 is used by the floppy driver */
#   error DMA channels other than 1 and 3 are not supported.
#  endif

# elif QIC02_TAPE_IFC == MOUNTAIN
#  define QIC02_STAT_POLARITY	MTN_QIC02_STAT_POLARITY
#  define QIC02_STAT_PORT	MTN_QIC02_STAT_PORT
#  define QIC02_CTL_PORT	MTN_QIC02_CTL_PORT
#  define QIC02_CMD_PORT	MTN_QIC02_CMD_PORT
#  define QIC02_DATA_PORT	MTN_QIC02_DATA_PORT

#  define QIC02_STAT_READY	MTN_QIC02_STAT_READY
#  define QIC02_STAT_EXCEPTION	MTN_QIC02_STAT_EXCEPTION
#  define QIC02_STAT_MASK	MTN_QIC02_STAT_MASK
#  define QIC02_STAT_RESETMASK	MTN_QIC02_STAT_RESETMASK
#  define QIC02_STAT_RESETVAL	MTN_QIC02_STAT_RESETVAL

#  define QIC02_CTL_RESET	MTN_QIC02_CTL_RESET
#  define QIC02_CTL_REQUEST	MTN_QIC02_CTL_REQUEST

#  if QIC02_TAPE_DMA > 3	/* channel 2 is used by the floppy driver */
#   error DMA channels other than 1 and 3 are not supported.
#  endif

# else
#  error No valid interface card specified!
# endif /* QIC02_TAPE_IFC */


  /* An ugly hack to make sure WT_CTL_DMA is defined even for the
   * static, non-Wangtek case. The alternative was even worse.
   */ 
# ifndef WT_CTL_DMA
#  define WT_CTL_DMA		WT_CTL_DMA1
# endif

/*******************/

#else /* !CONFIG_QIC02_DYNCONF */

/* Now the runtime config version, using variables instead of constants.
 *
 * qic02_tape_dynconf is R/O for the kernel, set from userspace.
 * qic02_tape_ccb is private to the driver, R/W.
 */

# define QIC02_TAPE_DRIVE	(qic02_tape_dynconf.mt_type)
# define QIC02_TAPE_IFC		(qic02_tape_ccb.ifc_type)
# define QIC02_TAPE_IRQ		(qic02_tape_dynconf.irqnr)
# define QIC02_TAPE_DMA		(qic02_tape_dynconf.dmanr)
# define QIC02_TAPE_PORT	(qic02_tape_dynconf.port)
# define WT_CTL_DMA		(qic02_tape_ccb.dma_enable_value)
# define QIC02_TAPE_DEBUG	(qic02_tape_dynconf.debug)

# define QIC02_STAT_PORT	(qic02_tape_ccb.port_stat)
# define QIC02_CTL_PORT 	(qic02_tape_ccb.port_ctl)
# define QIC02_CMD_PORT 	(qic02_tape_ccb.port_cmd)
# define QIC02_DATA_PORT 	(qic02_tape_ccb.port_data)

# define QIC02_STAT_POLARITY	(qic02_tape_ccb.stat_polarity)
# define QIC02_STAT_READY	(qic02_tape_ccb.stat_ready)
# define QIC02_STAT_EXCEPTION	(qic02_tape_ccb.stat_exception)
# define QIC02_STAT_MASK	(qic02_tape_ccb.stat_mask)

# define QIC02_STAT_RESETMASK	(qic02_tape_ccb.stat_resetmask)
# define QIC02_STAT_RESETVAL	(qic02_tape_ccb.stat_resetval)

# define QIC02_CTL_RESET	(qic02_tape_ccb.ctl_reset)
# define QIC02_CTL_REQUEST	(qic02_tape_ccb.ctl_request)

# define TP_HAVE_DENS		(qic02_tape_dynconf.have_dens)
# define TP_HAVE_BSF		(qic02_tape_dynconf.have_bsf)
# define TP_HAVE_FSR		(qic02_tape_dynconf.have_fsr)
# define TP_HAVE_BSR		(qic02_tape_dynconf.have_bsr)
# define TP_HAVE_EOD		(qic02_tape_dynconf.have_eod)
# define TP_HAVE_SEEK		(qic02_tape_dynconf.have_seek)
# define TP_HAVE_TELL		(qic02_tape_dynconf.have_tell)
# define TP_HAVE_RAS1		(qic02_tape_dynconf.have_ras1)
# define TP_HAVE_RAS2		(qic02_tape_dynconf.have_ras2)

#endif /* CONFIG_QIC02_DYNCONF */


/* "Vendor Unique" codes */
/* Archive seek & tell stuff */
#define AR_QCMDV_TELL_BLK	0xAE	/* read current block address */
#define AR_QCMDV_SEEK_BLK	0xAD	/* seek to specific block */
#define AR_SEEK_BUF_SIZE	3	/* address is 3 bytes */



/*
 * Misc common stuff
 */

/* Standard QIC-02 commands -- rev F.  All QIC-02 drives must support these */
#define QCMD_SEL_1	0x01		/* select drive 1 */
#define QCMD_SEL_2	0x02		/* select drive 2 */
#define QCMD_SEL_3	0x04		/* select drive 3 */
#define QCMD_SEL_4	0x08		/* select drive 4 */
#define	QCMD_REWIND	0x21		/* rewind tape */
#define QCMD_ERASE	0x22		/* erase tape */
#define QCMD_RETEN	0x24		/* retension tape */
#define	QCMD_WRT_DATA	0x40		/* write data */
#define	QCMD_WRT_FM	0x60		/* write file mark */
#define	QCMD_RD_DATA	0x80		/* read data */
#define	QCMD_RD_FM	0xA0		/* read file mark (forward direction) */
#define	QCMD_RD_STAT	0xC0		/* read status */

/* Other (optional/vendor unique) commands */
 /* Density commands are only valid when TP_BOM is set! */
#define QCMD_DENS_11	0x26		/* QIC-11 */
#define QCMD_DENS_24	0x27		/* QIC-24: 9 track 60MB */
#define QCMD_DENS_120	0x28		/* QIC-120: 15 track 120MB */
#define QCMD_DENS_150	0x29		/* QIC-150: 18 track 150MB */
#define QCMD_DENS_300	0x2A		/* QIC-300/QIC-2100 */
#define QCMD_DENS_600	0x2B		/* QIC-600/QIC-2200 */
/* don't know about QIC-1000 and QIC-1350 */

#define	QCMD_WRTNU_DATA	0x40		/* write data, no underruns, insert filler. */
#define QCMD_SPACE_FWD	0x81		/* skip next block */
#define QCMD_SPACE_BCK	0x89		/* move tape head one block back -- very useful! */
#define QCMD_RD_FM_BCK	0xA8		/* read filemark (backwards) */
#define QCMD_SEEK_EOD	0xA3		/* skip to EOD */
#define	QCMD_RD_STAT_X1	0xC1		/* read extended status 1 */
#define	QCMD_RD_STAT_X2	0xC4		/* read extended status 2 */
#define	QCMD_RD_STAT_X3	0xE0		/* read extended status 3 */
#define QCMD_SELF_TST1	0xC2		/* run self test 1 (nondestructive) */
#define QCMD_SELF_TST2	0xCA		/* run self test 2 (destructive) */



/* Optional, QFA (Quick File Access) commands.
 * Not all drives support this, but those that do could use these commands
 * to implement semi-non-sequential access. `mt fsf` would benefit from this.
 * QFA divides the tape into 2 partitions, a data and a directory partition,
 * causing some incompatibility problems wrt std QIC-02 data exchange.
 * It would be useful to cache the directory info, but that might be tricky
 * to do in kernel-space. [Size constraints.]
 * Refer to the QIC-02 specs, appendix A for more information.
 * I have no idea how other *nix variants implement QFA.
 * I have no idea which drives support QFA and which don't.
 */
#define QFA_ENABLE	0x2D		/* enter QFA mode, give @ BOT only */
#define QFA_DATA	0x20		/* select data partition */
#define QFA_DIR		0x23		/* select directory partition */
#define QFA_RD_POS	0xCF		/* read position+status bytes */
#define QFA_SEEK_EOD	0xA1		/* seek EOD within current partition */
#define QFA_SEEK_BLK	0xAF		/* seek to a block within current partition */




/*
 * Debugging flags
 */
#define TPQD_SENSE_TEXT	0x0001
#define TPQD_SENSE_CNTS 0x0002
#define TPQD_REWIND	0x0004
#define TPQD_TERM_CYCLE	0x0008
#define TPQD_IOCTLS	0x0010
#define TPQD_DMAX	0x0020
#define TPQD_BLKSZ	0x0040
#define TPQD_MISC	0x0080

#define TPQD_DEBUG	0x0100

#define TPQD_DIAGS	0x1000

#define TPQD_ALWAYS	0x8000

#define TPQD_DEFAULT_FLAGS	0x00fc


#define TPQDBG(f)	((QIC02_TAPE_DEBUG) & (TPQD_##f))


/* Minor device codes for tapes:
 * |7|6|5|4|3|2|1|0|
 *  | \ | / \ | / |_____ 1=rewind on close, 0=no rewind on close
 *  |  \|/    |_________ Density: 000=none, 001=QIC-11, 010=24, 011=120,
 *  |   |                100=QIC-150, 101..111 reserved.
 *  |   |_______________ Reserved for unit numbers.
 *  |___________________ Reserved for diagnostics during debugging.
 */

#define	TP_REWCLOSE(d)	((MINOR(d)&0x01) == 1)	   		/* rewind bit */
			   /* rewind is only done if data has been transferred */
#define	TP_DENS(dev)	((MINOR(dev) >> 1) & 0x07) 	      /* tape density */
#define TP_UNIT(dev)	((MINOR(dev) >> 4) & 0x07)	       /* unit number */

/* print excessive diagnostics */
#define TP_DIAGS(dev)	(QIC02_TAPE_DEBUG & TPQD_DIAGS)

/* status codes returned by a WTS_RDSTAT call */
struct tpstatus {	/* sizeof(short)==2), LSB first */
	unsigned short	exs;	/* Drive exception flags */
	unsigned short	dec;	/* data error count: nr of blocks rewritten/soft read errors */
	unsigned short	urc;	/* underrun count: nr of times streaming was interrupted */
};
#define TPSTATSIZE	sizeof(struct tpstatus)


/* defines for tpstatus.exs -- taken from 386BSD wt driver */
#define	TP_POR		0x100	/* Power on or reset occurred */
#define	TP_EOR		0x200	/* REServed for end of RECORDED media */
#define	TP_PAR		0x400	/* REServed for bus parity */
#define	TP_BOM		0x800	/* Beginning of media */
#define	TP_MBD		0x1000	/* Marginal block detected */
#define	TP_NDT		0x2000	/* No data detected */
#define	TP_ILL		0x4000	/* Illegal command */
#define	TP_ST1		0x8000	/* Status byte 1 flag */
#define	TP_FIL		0x01	/* File mark detected */
#define	TP_BNL		0x02	/* Bad block not located */
#define	TP_UDA		0x04	/* Unrecoverable data error */
#define	TP_EOM		0x08	/* End of media */
#define	TP_WRP		0x10	/* Write protected cartridge */
#define	TP_USL		0x20	/* Unselected drive */
#define	TP_CNI		0x40	/* Cartridge not in place */
#define	TP_ST0		0x80	/* Status byte 0 flag */

#define REPORT_ERR0	(TP_CNI|TP_USL|TP_WRP|TP_EOM|TP_UDA|TP_BNL|TP_FIL)
#define REPORT_ERR1	(TP_ILL|TP_NDT|TP_MBD|TP_PAR)


/* exception numbers */
#define EXC_UNKNOWN	0	/* (extra) Unknown exception code */
#define EXC_NDRV	1	/* No drive */
#define EXC_NCART	2	/* No cartridge */
#define EXC_WP		3	/* Write protected */
#define EXC_EOM		4	/* EOM */
#define EXC_RWA		5	/* read/write abort */
#define EXC_XBAD	6	/* read error, bad block transferred */
#define EXC_XFILLER	7	/* read error, filler block transferred */
#define EXC_NDT		8	/* read error, no data */
#define EXC_NDTEOM	9	/* read error, no data & EOM */
#define EXC_NDTBOM	10	/* read error, no data & BOM */
#define EXC_FM		11	/* Read a filemark */
#define EXC_ILL		12	/* Illegal command */
#define EXC_POR		13	/* Power on/reset */
#define EXC_MARGINAL	14	/* Marginal block detected */
#define EXC_EOR		15	/* (extra, for SEEKEOD) End Of Recorded data reached */
#define EXC_BOM		16	/* (extra) BOM reached */


#define TAPE_NOTIFY_TIMEOUT	1000000

/* internal function return codes */
#define TE_OK	0		/* everything is fine */
#define TE_EX	1		/* exception detected */
#define TE_ERR	2		/* some error */
#define TE_NS	3		/* can't read status */
#define TE_TIM	4		/* timed out */
#define TE_DEAD	5		/* tape drive doesn't respond */
#define TE_END	6		/******** Archive hack *****/

/* timeout timer values -- check these! */
#define TIM_S	(4*HZ)		/* 4 seconds (normal cmds) */
#define TIM_M	(30*HZ)		/* 30 seconds (write FM) */
#define TIM_R	(8*60*HZ)	/* 8 minutes (retensioning) */
#define TIM_F	(2*3600*HZ)	/* est. 1.2hr for full tape read/write+2 retens */

#define TIMERON(t)	mod_timer(&tp_timer, jiffies + (t))
#define TIMEROFF	del_timer_sync(&tp_timer);
#define TIMERCONT	add_timer(&tp_timer);


typedef char flag;
#define NO	0	/* NO must be 0 */
#define YES	1	/* YES must be != 0 */


#ifdef TDEBUG
# define TPQDEB(s)	s
# define TPQPUTS(s)	tpqputs(s)
#else
# define TPQDEB(s)
# define TPQPUTS(s)
#endif


/* NR_BLK_BUF is a `tuneable parameter'. If you're really low on
 * kernel space, you could decrease it to 1, or if you got a very
 * slow machine, you could increase it up to 127 blocks. Less kernel
 * buffer blocks result in more context-switching.
 */
#define NR_BLK_BUF	20				    /* max 127 blocks */
#define TAPE_BLKSIZE	512		  /* streamer tape block size (fixed) */
#define TPQBUF_SIZE	(TAPE_BLKSIZE*NR_BLK_BUF)	       /* buffer size */


#define BLOCKS_BEYOND_EW	2	/* nr of blocks after Early Warning hole */
#define BOGUS_IRQ		32009


/* This is internal data, filled in based on the ifc_type field given
 * by the user. Everex is mapped to Wangtek with a different
 * `dma_enable_value', if dmanr==3.
 */
struct qic02_ccb {
	long	ifc_type;

	unsigned short	port_stat;	/* Status port address */
	unsigned short	port_ctl;	/* Control port address */
	unsigned short	port_cmd;	/* Command port address */
	unsigned short	port_data;	/* Data port address */

	/* status register bits */
	unsigned short	stat_polarity;	/* invert status bits or not */
	unsigned short	stat_ready;	/* drive ready */
	unsigned short	stat_exception;	/* drive signals exception */
	unsigned short	stat_mask;
	unsigned short	stat_resetmask;
	unsigned short	stat_resetval;

	/* control register bits */
	unsigned short	ctl_reset;	/* reset drive */
	unsigned short	ctl_request;	/* latch command */
	
	/* This is used to change the DMA3 behaviour */
	unsigned short	dma_enable_value;
};

#if MODULE
static int qic02_tape_init(void);
#else
extern int qic02_tape_init(void);			  /* for mem.c */
#endif



#endif /* CONFIG_QIC02_TAPE */

#endif /* _LINUX_TPQIC02_H */

