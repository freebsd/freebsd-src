/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)wdreg.h	7.1 (Berkeley) 5/9/91
 *	$Id: wxreg.h,v 1.1 1993/10/26 22:26:39 nate Exp $
 */

/*
 * Disk Controller register definitions.
 */
#define	wd_data		0x0		/* data register (R/W - 16 bits) */
#define wd_error	0x1		/* error register (R) */
#define	wd_precomp	wd_error	/* write precompensation (W) */
#define	wd_seccnt	0x2		/* sector count (R/W) */
#define	wd_sector	0x3		/* first sector number (R/W) */
#define	wd_cyl_lo	0x4		/* cylinder address, low byte (R/W) */
#define	wd_cyl_hi	0x5		/* cylinder address, high byte (R/W)*/
#define	wd_sdh		0x6		/* sector size/drive/head (R/W)*/
#define	wd_command	0x7		/* command register (W)	 */
#define	wd_status wd_command		/* immediate status (R)	 */

#define	wd_altsts	0x206	 /*alternate fixed disk status(via 1015) (R)*/
#define	wd_ctlr		0x206	 /*fixed disk controller control(via 1015) (W)*/
#define  WDCTL_4BIT	 0x8	/* use four head bits (wd1003) */
#define  WDCTL_RST	 0x4	/* reset the controller */
#define  WDCTL_IDS	 0x2	/* disable controller interrupts */
#define	wd_digin	0x207	 /* disk controller input(via 1015) (R)*/

/*
 * Status Bits.
 */
#define	WDCS_BUSY	0x80		/* Controller busy bit. */
#define	WDCS_READY	0x40		/* Selected drive is ready */
#define	WDCS_WRTFLT	0x20		/* Write fault */
#define	WDCS_SEEKCMPLT	0x10		/* Seek complete */
#define	WDCS_DRQ	0x08		/* Data request bit. */
#define	WDCS_ECCCOR	0x04		/* ECC correction made in data */
#define	WDCS_INDEX	0x02		/* Index pulse from selected drive */
#define	WDCS_ERR	0x01		/* Error detect bit. */

#define WDCS_BITS	"\020\010busy\006rdy\006wrtflt\005seekdone\004drq\003ecc_cor\002index\001err"

#define WDERR_BITS	"\020\010badblk\007uncorr\006id_crc\005no_id\003abort\002tr000\001no_dam"

/*
 * Commands for Disk Controller.
 */
#define	WDCC_RESTORE	0x10		/* disk restore code -- resets cntlr */

#define	WDCC_READ	0x20		/* disk read code */
#define	WDCC_WRITE	0x30		/* disk write code */
#define	 WDCC__LONG	 0x02		 /* modifier -- access ecc bytes */
#define	 WDCC__NORETRY	 0x01		 /* modifier -- no retrys */

#define	WDCC_FORMAT	0x50		/* disk format code */
#define	WDCC_DIAGNOSE	0x90		/* controller diagnostic */
#define	WDCC_IDC	0x91		/* initialize drive command */

#define	WDCC_EXTDCMD	0xE0		/* send extended command */
#define	WDCC_READP	0xEC		/* read parameters from controller */
#define	WDCC_CACHEC	0xEF		/* cache control */

#define	WD_STEP		0		/* winchester- default 35us step */

#define	WDSD_IBM	0xa0		/* forced to 512 byte sector, ecc */


#ifdef KERNEL
/*
 * read parameters command returns this:
 */
struct wdparams {
	/* drive info */
	short	wdp_config;		/* general configuration */
	short	wdp_fixedcyl;		/* number of non-removable cylinders */
	short	wdp_removcyl;		/* number of removable cylinders */
	short	wdp_heads;		/* number of heads */
	short	wdp_unfbytespertrk;	/* number of unformatted bytes/track */
	short	wdp_unfbytes;		/* number of unformatted bytes/sector */
	short	wdp_sectors;		/* number of sectors */
	short	wdp_minisg;		/* minimum bytes in inter-sector gap*/
	short	wdp_minplo;		/* minimum bytes in postamble */
	short	wdp_vendstat;		/* number of words of vendor status */
	/* controller info */
	char	wdp_cnsn[20];		/* controller serial number */
	short	wdp_cntype;		/* controller type */
#define	WDTYPE_SINGLEPORTSECTOR	1	 /* single port, single sector buffer */
#define	WDTYPE_DUALPORTMULTI	2	 /* dual port, multiple sector buffer */
#define	WDTYPE_DUALPORTMULTICACHE 3	 /* above plus track cache */
	short	wdp_cnsbsz;		/* sector buffer size, in sectors */
	short	wdp_necc;		/* ecc bytes appended */
	char	wdp_rev[8];		/* firmware revision */
	char	wdp_model[40];		/* model name */
	short	wdp_nsecperint;		/* sectors per interrupt */
	short	wdp_usedmovsd;		/* can use double word read/write? */
};

/*
 * wd driver entry points
 */
void wdstrategy(struct buf *bp);
void wdintr(int unit);
int wdopen(dev_t dev, int flags, int fmt, struct proc *p);
int wdclose(dev_t dev, int flags, int fmt);
int wdioctl(dev_t dev, int cmd, caddr_t addr, int flag);
#ifdef	B_FORMAT
int wdformat(struct buf *bp);
#endif
int wdsize(dev_t dev);
int wddump(dev_t dev);

#endif KERNEL
