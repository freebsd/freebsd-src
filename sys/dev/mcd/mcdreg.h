/*-
 * Copyright 1993 by Holger Veit (data part)
 * Copyright 1993 by Brian Moore (audio part)
 * Changes Copyright 1993 by Gary Clark II
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
 *	This software was developed by Holger Veit and Brian Moore
 *	for use with "386BSD" and similar operating systems.
 *    "Similar operating systems" includes mainly non-profit oriented
 *    systems for research and education, including but not restricted to
 *    "NetBSD", "FreeBSD", "Mach" (by CMU).
 * 4. Neither the name of the developer(s) nor the name "386BSD"
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file contains definitions for some cdrom control commands
 * and status codes. This info was "inherited" from the DOS MTMCDE.SYS
 * driver, and is thus not complete (and may even be wrong). Some day
 * the manufacturer or anyone else might provide better documentation,
 * so this file (and the driver) will then have a better quality.
 *
 * $FreeBSD: src/sys/dev/mcd/mcdreg.h,v 1.18 2005/03/02 21:33:24 joerg Exp $
 */

#ifndef MCD_H
#define	MCD_H

/* toc */
#define MCD_MAXTOCS	104	/* from the Linux driver */
#define MCD_LASTPLUS1	170	/* special toc entry */

typedef unsigned char	bcd_t;
#define	M_msf(msf)	msf[0]
#define	S_msf(msf)	msf[1]
#define	F_msf(msf)	msf[2]

/* io lines used */
#define	MCD_IO_BASE	0x300

#define	MCD_REG_COMMAND	0
#define	MCD_REG_STATUS	0
#define	MCD_REG_RDATA	0

#define	MCD_REG_RESET	1
#define	MCD_REG_CTL2	2	/* XXX Is this right? */
#define	MCD_REG_CONFIG	3

#define	MCD_MASK_DMA	0x07	/* bits 2-0 = DMA channel */
#define	MCD_MASK_IRQ	0x70	/* bits 6-4 = INT number */
				/* 001 = int 2,9 */
				/* 010 = int 3 */
				/* 011 = int 5 */
				/* 100 = int 10 */
				/* 101 = int 11 */
/* flags */
#define MFL_DATA_NOT_AVAIL      0x02
#define MFL_STATUS_NOT_AVAIL    0x04

/* New Commands */
#define M_RESET		0x00
#define M_PICKLE	0x04

/* ports */
#define	MCD_DATA	0
#define	MCD_FLAGS	1
#define	MCD_CTRL	2
#define	CHANNEL		3	/* XXX ??? */

/* Status bits */
#define	MCD_ST_DOOROPEN		0x80
#define	MCD_ST_DSKIN		0x40
#define	MCD_ST_DSKCHNG		0x20
#define	MCD_ST_SPINNING		0x10
#define	MCD_ST_AUDIODISK	0x08	/* Audio Disk is in */
#define	MCD_ST_BUSY		0x04
#define	MCD_ST_AUDIOBSY		0x02	/* Audio Disk is Playing */
#define	MCD_ST_CMDCHECK		0x01	/* Command error */

/* commands known by the controller */
#define	MCD_CMDRESET		0x00
#define	MCD_CMDGETVOLINFO	0x10	/* gets mcd_volinfo */
#define	MCD_CMDGETDISKINFO	0x11	/* gets	mcd_disk information */
#define	MCD_CMDGETQCHN		0x20	/* gets mcd_qchninfo */
#define	MCD_CMDGETSENSE		0x30	/* gets	sense info */
#define	MCD_CMDGETSTAT		0x40	/* gets a byte of status */

#define	MCD_CMDSETMODE		0x50	/* set transmission mode, needs byte */

#define	MCD_MDBIT_TESTMODE	0x80	/* 0 = DATALENGTH setting is valid */
#define	MCD_MDBIT_DATALENGTH	0x40	/* 0 = Read User Data Only */
					/* 1 = Read Raw	sectors	(2352 bytes) */

#define	MCDBLK	2048				/* for cooked mode */
#define	MCDRBLK	sizeof(struct mcd_rawsector)	/* for raw mode	*/

#define	MCD_MDBIT_ECCMODE	0x20	/* 0 = Use secondary correction	*/
					/* 1 = Don't use secondary ECC */
#define	MCD_MDBIT_SPINDOWN	0x08	/* 0 = Spin Up,	1 = Spin Down */
#define	MCD_MDBIT_GET_TOC	0x04	/* 0 = Get UPC on next GETQCHAN	*/
					/* 1 = Get TOC on GETQCHAN */
#define	MCD_MDBIT_MUTEDATA	0x01	/* 1 = Don't play back Data as audio */

#define	MCD_MD_RAW		(MCD_MDBIT_DATALENGTH|MCD_MDBIT_ECCMODE|MCD_MDBIT_MUTEDATA)
#define	MCD_MD_COOKED		(MCD_MDBIT_MUTEDATA)
#define	MCD_MD_TOC		(MCD_MDBIT_GET_TOC|MCD_MDBIT_MUTEDATA)

#define	MCD_CMDSTOPAUDIO	0x70
#define	MCD_CMDSTOPAUDIOTIME	0x80
#define	MCD_CMDGETVOLUME	0x8E	/* gets mcd_volume */
#define	MCD_CMDSETDRIVEMODE	0xA0	/* Set drive mode */
#define	MCD_READUPC		0xA2	/* Get UPC info	*/
#define	MCD_CMDSETVOLUME	0xAE	/* sets mcd_volume */
#define	MCD_CMDREAD1		0xB0	/* read n sectors */
#define	MCD_CMDSINGLESPEEDREAD	0xC0	/* read	from-to	*/
#define	MCD_CMDSTARTAUDIOMSF	0xC1	/* read	audio data */
#define	MCD_CMDDOUBLESPEEDREAD	0xC1	/* Read	lots of	data from the drive */
#define	MCD_CMDGETDRIVEMODE	0xC2	/* Get the drive mode */
#define	MCD_CMDREAD		0xC3	/* Read	data from the drive */
#define	MCD_CMDSETINTERLEAVE	0xC8	/* Adjust the interleave */
#define	MCD_CMDCONTINFO		0xDC	/* Get controller info */
#define	MCD_CMDSTOP		0xF0	/* Stop	everything */
#define	MCD_CMDEJECTDISK	0xF6
#define	MCD_CMDCLOSETRAY	0xF8

#define	MCD_CMDLOCKDRV		0xFE	/* needs byte */
#define	MCD_LK_UNLOCK	0x00
#define	MCD_LK_LOCK	0x01
#define	MCD_LK_TEST	0x02

/* DMA Enable Stuff */
#define	MCD_DMA_IRQFLAGS	0x10	/* Set data0 for IRQ click */

#define	MCD_DMA_PREIRQ		0x01	/* All of these	are for	*/
#define	MCD_DMA_POSTIRQ		0x02	/* MCD_DMA_IRQFLAG...	*/
#define	MCD_DMA_ERRIRQ		0x04	/*			*/

#define	MCD_DMA_TIMEOUT		0x08	/* Set data0 for DMA timeout */
#define	MCD_DMA_UPCFLAG		0x04	/* 1 = Next command will be READUPC */

#define	MCD_DMA_DMAMODE		0x02	/* 1 = Data uses DMA */
#define	MCD_DMA_TRANSFERLENGTH	0x01	/* data0 = MSB,	data1 =	LSB of block length */

struct mcd_dma_mode {
	u_char	dma_mode;
	u_char	data0;		/* If dma_mode & 0x10: Use IRQ settings	*/
	u_char	data1;		/* Used	if dma_mode & 0x01 */
} __packed;

struct mcd_volinfo {
	bcd_t	trk_low;
	bcd_t	trk_high;
	bcd_t	vol_msf[3];
	bcd_t	trk1_msf[3];
} __packed;

struct mcd_qchninfo {
	u_char  addr_type:4;
	u_char  control:4;
	u_char	trk_no;
	u_char	idx_no;
	bcd_t	trk_size_msf[3];
	u_char	:8;
	bcd_t	hd_pos_msf[3];
} __packed;

struct mcd_volume {
	u_char	v0l;
	u_char	v0rs;
	u_char	v0r;
	u_char	v0ls;
} __packed;

struct mcd_holdtime {
	u_char	units_of_ten_seconds;
			/* If this is 0, the default (12) is used */
} __packed;

struct mcd_read1 {
	bcd_t	start_msf[3];
	u_char	nsec[3];
} __packed;

struct mcd_read2 {
	bcd_t	start_msf[3];
	bcd_t	end_msf[3];
} __packed;

struct mcd_rawsector {
	u_char sync1[12];
	u_char header[4];
	u_char subheader1[4];
	u_char subheader2[4];
	u_char data[MCDBLK];
	u_char ecc_bits[280];
} __packed;

#endif /* MCD_H */
