/*
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
 *      for use with "386BSD" and similar operating systems.
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
 *	$Id: mcdreg.h,v 1.1 1993/10/12 06:08:31 rgrimes Exp $
 */

#ifndef MCD_H
#define	MCD_H

#ifdef __GNUC__
#if __GNUC__ >= 2
#pragma pack(1)
#endif
#endif

typedef unsigned char	bcd_t;
#define	M_msf(msf)	msf[0]
#define	S_msf(msf)	msf[1]
#define	F_msf(msf)	msf[2]

/* io lines used */
#define	MCD_IO_BASE	0x300

#define	mcd_command	0
#define	mcd_status	0
#define	mcd_rdata	0

#define	mcd_reset	1
#define	mcd_xfer	1
#define	mcd_ctl2	2 /* XXX Is this right? */
#define	mcd_config	3

#define	MCD_MASK_DMA	0x07	/* bits 2-0 = DMA channel */
#define	MCD_MASK_IRQ	0x70	/* bits 6-4 = INT number */
				/* 001 = int 2,9 */
				/* 010 = int 3 */
				/* 011 = int 5 */
				/* 100 = int 10 */
				/* 101 = int 11 */
/* flags */
#define	STATUS_AVAIL	0xB
#define	DATA_AVAIL	0xF

/* New Flags */
#define M_STATUS_AVAIL       	0xFB 
#define M_DATA_AVAIL		0xFD

/* New Commands */
#define M_RESET		0x00

/* ports */
#define	MCD_DATA	0
#define	MCD_FLAGS	1
#define	MCD_DONT_KNOW	2	/* What are these two ports for??? */
#define	CHANNEL		3

/* Status bits */
#define	MCD_ST_DOOROPEN		0x80
#define	MCD_ST_DSKIN		0x40
#define	MCD_ST_DSKCHNG		0x20
#define	MCD_ST_BUSY		0x04
#define	MCD_ST_AUDIOBSY		0x02

/* commands known by the controller */
#define	MCD_CMDRESET		0x00
#define	MCD_CMDGETVOLINFO	0x10	/* gets mcd_volinfo */
#define	MCD_CMDGETQCHN		0x20	/* gets mcd_qchninfo */
#define	MCD_CMDGETSTAT		0x40	/* gets a byte of status */
#define	MCD_CMDSETMODE		0x50	/* set transmission mode, needs byte */
#define	MCD_MD_RAW		0x60
#define	MCD_MD_COOKED		0x01
#define	MCD_MD_TOC		0x05
#define	MCD_CMDSTOPAUDIO	0x70
#define	MCD_CMDGETVOLUME	0x8E	/* gets mcd_volume */
#define	MCD_CMDSETVOLUME	0xAE	/* sets mcd_volume */
#define	MCD_CMDREAD1		0xB0	/* read n sectors */
#define	MCD_CMDREAD2		0xC0	/* read from-to */
#define	MCD_CMDCONTINFO		0xDC	/* Get controller info */
#define	MCD_CMDEJECTDISK	0xF6
#define	MCD_CMDCLOSETRAY	0xF8
#define	MCD_CMDLOCKDRV		0xFE	/* needs byte */
#define	MCD_LK_UNLOCK	0x00
#define	MCD_LK_LOCK	0x01
#define	MCD_LK_TEST	0x02

struct mcd_volinfo {
	bcd_t	trk_low;
	bcd_t	trk_high;
	bcd_t	vol_msf[3];
	bcd_t	trk1_msf[3];
};

struct mcd_qchninfo {
	u_char	ctrl_adr;
	u_char	trk_no;
	u_char	idx_no;
	bcd_t	trk_size_msf[3];
	u_char	:8;
	bcd_t	hd_pos_msf[3];
};

struct mcd_volume {
	u_char	v0l;
	u_char	v0rs;
	u_char	v0r;
	u_char	v0ls;
};

struct mcd_read1 {
	bcd_t	start_msf[3];
	u_char	nsec[3];
};

struct mcd_read2 {
	bcd_t	start_msf[3];
	bcd_t	end_msf[3];
};
#endif /* MCD_H */
