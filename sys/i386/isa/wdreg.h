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
 * $FreeBSD: src/sys/i386/isa/wdreg.h,v 1.30 1999/12/29 04:33:16 peter Exp $
 */

/*
 * modified for PC9801 by F.Ukai
 *			Kyoto University Microcomputer Club (KMC)
 */

/*
 * Disk Controller register definitions.
 */
#ifdef PC98
#define	wd_data		0x0		/* data register (R/W - 16 bits) */
#define wd_error	0x2		/* error register (R) */
#define	wd_precomp	wd_error	/* write precompensation (W) */
#define wd_features	wd_error	/* features register (W) */
#define	wd_seccnt	0x4		/* sector count (R/W) */
#define	wd_sector	0x6		/* first sector number (R/W) */
#define	wd_cyl_lo	0x8		/* cylinder address, low byte (R/W) */
#define	wd_cyl_hi	0xa		/* cylinder address, high byte (R/W)*/
#define	wd_sdh		0xc		/* sector size/drive/head (R/W)*/
#define	wd_command	0xe		/* command register (W)	 */
#define	wd_status wd_command		/* immediate status (R)	 */

#define	wd_altsts_nec	0x10c	 /*alternate fixed disk status(via 1015) (R)*/
#define	wd_ctlr_nec	0x10c	 /*fixed disk controller control(via 1015) (W)*/
#define	wd_altsts_epson	0x3	 /*alternate fixed disk status(via 1015) (R)*/
#define	wd_ctlr_epson	0x3	 /*fixed disk controller control(via 1015) (W)*/
#define wd_altsts		wd_altsts_nec

#define  WDCTL_4BIT	 0x8	/* use four head bits (wd1003) */
#define  WDCTL_RST	 0x4	/* reset the controller */
#define  WDCTL_IDS	 0x2	/* disable controller interrupts */
#define	wd_digin	0x10e	 /* disk controller input(via 1015) (R)*/
#else	/* IBM-PC */
#define	wd_data		0x0		/* data register (R/W - 16 bits) */
#define wd_error	0x1		/* error register (R) */
#define	wd_precomp	wd_error	/* write precompensation (W) */
#define wd_features	wd_error	/* features register (W) */
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
#endif	/* PC98 */

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

#define WDCS_BITS	"\020\010busy\007rdy\006wrtflt\005seekdone\004drq\003ecc_cor\002index\001err"
#define WDERR_ABORT	0x04

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
#define WDCC_READ_MULTI	0xC4	/* read multiple */
#define WDCC_WRITE_MULTI	0xC5	/* write multiple */
#define WDCC_SET_MULTI 0xC6		/* set multiple count */
#define WDCC_READ_DMA	0xC8		/* read using DMA */
#define WDCC_WRITE_DMA	0xCA		/* write using DMA */


#define	WDCC_EXTDCMD	0xE0		/* send extended command */
#define	WDCC_READP	0xEC		/* read parameters from controller */
#define	WDCC_FEATURES	0xEF		/* features control */

#define WDCC_DEFECT     0xF0            /* read defect list */

#define	WDFEA_NORCACHE	0x55		/* read cache disable */
#define	WDFEA_RCACHE	0xAA		/* read cache enable */
#define WDFEA_NOWCACHE	0x82		/* write cache disable */
#define WDFEA_WCACHE	0x02		/* write cache enable */
#define WDFEA_SETXFER	0x03		/* set transfer mode */

#define	WD_STEP		0		/* winchester- default 35us step */

#define	WDSD_IBM	0xa0		/* forced to 512 byte sector, ecc */
#define	WDSD_LBA	0x40		/* use Logical Block Adressing */

#ifdef _KERNEL
/*
 * read parameters command returns this:
 */
struct wdparams {
	/*
	 * XXX partly based on DRAFT X3T13/1153D rev 14.  
	 * by the time you read this it will have changed.
	 * Offsets in words
	 * (as that's how they are usually presented in tables
	 * e.g. QUANTUM Product manuals)
	 */
	/* drive info */
	short	wdp_config;		/*0 general configuration bits */
	u_short	wdp_cylinders;		/*1 number of cylinders */
	short	wdp_reserved2;		/*2*/
	u_short	wdp_heads;		/*3 number of heads */
	short	wdp_unfbytespertrk;	/*4 number of unformatted bytes/track */
	short	wdp_unfbytes;		/*5 number of unformatted bytes/sec */
	u_short	wdp_sectors;		/*6 number of sectors per track */
	short	wdp_vendorunique[3];	/*7,8,9*/
	/* controller info */
	char	wdp_serial[20];		/*10-19 serial number */
	short	wdp_buffertype;		/*20 buffer type */
#define	WDTYPE_SINGLEPORTSECTOR	1	 /* single port, single sector buffer */
#define	WDTYPE_DUALPORTMULTI	2	 /* dual port, multiple sector buffer */
#define	WDTYPE_DUALPORTMULTICACHE 3	 /* above plus track cache */
	short	wdp_buffersize;		/*21 buffer size, in 512-byte units */
	short	wdp_necc;		/*22 ecc bytes appended */
	char	wdp_rev[8];		/*23-26 firmware revision */
	char	wdp_model[40];		/*27-46 model name */
	char	wdp_nsecperint;		/*47L sectors per interrupt */
	char	wdp_vendorunique1;	/*47H*/
	short	wdp_usedmovsd;		/*48 can use double word read/write? */
	char	wdp_vendorunique2;	/*49L*/
	char	wdp_capability;		/*49H various capability bits */
	short	wdp_cap_validate;	/*50 validation for above */
	char	wdp_vendorunique3;	/*51L*/
	char	wdp_opiomode;		/*51H PIO modes 0-2 */
	char	wdp_vendorunique4;	/*52*/
	char	wdp_odmamode;		/*52 old DMA modes, not in ATA-3 */
	short	wdp_atavalid;		/*53 validation for newer fields */
	short	wdp_currcyls;		/*54 */
	short	wdp_currheads;		/*55 */
	short	wdp_currsectors;	/*56 */
	short	wdp_currsize0;		/*57 CHS size*/
	short	wdp_currsize1;		/*58 CHS size*/
	char	wdp_currmultsect;	/*59L */
	char	wdp_multsectvalid;	/*59H */
	int	wdp_lbasize;		/*60,61*/
	short	wdp_dmasword;		/*62 obsolete in ATA-3 */
	short	wdp_dmamword;		/*63 multiword DMA modes */
	short	wdp_eidepiomodes;	/*64 advanced PIO modes */
	short	wdp_eidedmamin;		/*65 fastest possible DMA timing */
	short	wdp_eidedmanorm;	/*66 recommended DMA timing */
	short	wdp_eidepioblind;	/*67 fastest possible blind PIO */
	short	wdp_eidepioacked;	/*68 fastest possible IORDY PIO */
	short	wdp_reserved69;		/*69*/
	short	wdp_reserved70;		/*70*/
	short	wdp_reserved71;		/*71*/
	short	wdp_reserved72;		/*72*/
	short	wdp_reserved73;		/*73*/
	short	wdp_reserved74;		/*74*/
	short	wdp_queuelen;		/*75*/
	short	wdp_reserved76;		/*76*/
	short	wdp_reserved77;		/*77*/
	short	wdp_reserved78;		/*78*/
	short	wdp_reserved79;		/*79*/
	short	wdp_versmaj;		/*80*/
	short	wdp_versmin;		/*81*/
	short	wdp_featsupp1;		/*82*/
	short	wdp_featsupp2;		/*83*/
	short	wdp_featsupp3;		/*84*/
	short	wdp_featenab1;		/*85*/
	short	wdp_featenab2;		/*86*/
	short	wdp_featenab3;		/*87*/
	short	wdp_udmamode;		/*88 UltraDMA modes */
	short	wdp_erasetime;		/*89*/
	short	wdp_enherasetime;	/*90*/
	short	wdp_apmlevel;		/*91*/
	short	wdp_reserved92[34];	/*92*/
	short	wdp_rmvcap;		/*93*/
	short	wdp_securelevel;	/*94*/
};

/*
 * IDE DMA support.
 * This is based on what is needed for the IDE DMA function of the Intel
 * Triton chipset; hopefully it's general enough to be used for other
 * chipsets as well.
 *
 * To use this:
 *	For each drive which you might want to do DMA on, call wdd_candma()
 *	to get a cookie.  If it returns a null pointer, then the drive
 *	can't do DMA.  Then call wdd_dmainit() to initialize the controller
 *	and drive.  wdd_dmainit should leave PIO modes operational, though
 *	perhaps with suboptimal performance.
 *
 *	Check the transfer by calling wdd_dmaverify().  The cookie is what
 *	you got before; vaddr is the virtual address of the buffer to be
 *	written; len is the length of the buffer; and direction is either
 *	B_READ or B_WRITE. This function verifies that the DMA hardware is
 *	capable of handling the request you've made.
 *
 *	Setup the transfer by calling wdd_dmaprep().  This takes the same
 *	paramaters as wdd_dmaverify().
 *
 *	Send a read/write DMA command to the drive.
 *
 *	Call wdd_dmastart().
 *
 *	Wait for an interrupt.  Multi-sector transfers will only interrupt
 *	at the end of the transfer.
 *
 *	Call wdd_dmadone().  It will return the status as defined by the
 *	WDDS_* constants below.
 */
struct wddma {
	void	*(*wdd_candma)		/* returns a cookie if PCI */
		__P((int iobase_wd, int ctlr, int unit));
	int	(*wdd_dmaverify)	/* verify that request is DMA-able */
		__P((void *cookie, char *vaddr, u_long len, int direction));
	int	(*wdd_dmaprep)		/* prepare DMA hardware */
		__P((void *cookie, char *vaddr, u_long len, int direction));
	void	(*wdd_dmastart)		/* begin DMA transfer */
		__P((void *cookie));
	int	(*wdd_dmadone)		/* DMA transfer completed */
		__P((void *cookie));
	int	(*wdd_dmastatus)	/* return status of DMA */
		__P((void *cookie));
	int	(*wdd_dmainit)		/* initialize controller and drive */
		__P((void *cookie, 
		     struct wdparams *wp, 
		     int(wdcmd)__P((int mode, void *wdinfo)),
		     void *wdinfo));
	int	(*wdd_iobase)		/* returns iobase address */
		__P((void *cookie));
	int	(*wdd_altiobase)	/* returns altiobase address */
		__P((void *cookie));
};

/* logical status bits returned by wdd_dmastatus */
#define	WDDS_ACTIVE	0x0001
#define	WDDS_ERROR	0x0002
#define	WDDS_INTERRUPT	0x0004

#define WDDS_BITS	"\20\4interrupt\2error\1active"

/* defines for ATA timing modes */
#define WDDMA_GRPMASK	0xf8
#define WDDMA_MODEMASK	0x07
/* flow-controlled PIO modes */
#define	WDDMA_PIO	0x10
#define WDDMA_PIO3	0x10
#define WDDMA_PIO4	0x11
/* multi-word DMA timing modes */
#define	WDDMA_MDMA	0x20
#define	WDDMA_MDMA0	0x20
#define	WDDMA_MDMA1	0x21
#define	WDDMA_MDMA2	0x22

/* Ultra DMA timing modes */
#define	WDDMA_UDMA	0x40
#define	WDDMA_UDMA0	0x40
#define	WDDMA_UDMA1	0x41
#define	WDDMA_UDMA2	0x42

#define Q_CMD640B       0x00000001 /* CMD640B quirk: serialize IDE channels */
void	wdc_pci(int quirks);

extern struct wddma wddma[];

void	wdintr __P((void *unit));

#endif /* _KERNEL */
