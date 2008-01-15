/*-
 * Copyright (c) 1987, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *	@(#)disklabel.h	8.2 (Berkeley) 7/10/94
 * $FreeBSD: src/sys/sys/diskpc98.h,v 1.103 2005/05/08 11:30:26 nyan Exp $
 */

#ifndef _SYS_DISKPC98_H_
#define	_SYS_DISKPC98_H_

#include <sys/ioccom.h>

#define	DOSBBSECTOR	0	/* DOS boot block relative sector number */
#define	DOSPARTOFF	0
#define	DOSPARTSIZE	32
#define	NDOSPART	16
#define	DOSMAGICOFFSET	510
#define	DOSMAGIC	0xAA55

#define	DOSMID_386BSD	(0x14|0x80)	/* 386BSD | bootable */
#define	DOSSID_386BSD	(0x44|0x80)	/* 386BSD | active */
#define	DOSPTYP_386BSD	(DOSSID_386BSD << 8 | DOSMID_386BSD)

struct pc98_partition {
    	unsigned char	dp_mid;
	unsigned char	dp_sid;
	unsigned char	dp_dum1;
	unsigned char	dp_dum2;
	unsigned char	dp_ipl_sct;
	unsigned char	dp_ipl_head;
	unsigned short	dp_ipl_cyl;
	unsigned char	dp_ssect;	/* starting sector */
	unsigned char	dp_shd;		/* starting head */
	unsigned short	dp_scyl;	/* starting cylinder */
	unsigned char	dp_esect;	/* end sector */
	unsigned char	dp_ehd;		/* end head */
	unsigned short	dp_ecyl;	/* end cylinder */
	unsigned char	dp_name[16];
};
#ifdef CTASSERT
CTASSERT(sizeof (struct pc98_partition) == DOSPARTSIZE);
#endif

void pc98_partition_dec(void const *pp, struct pc98_partition *d);
void pc98_partition_enc(void *pp, struct pc98_partition *d);

#define DIOCSPC98	_IOW('M', 129, u_char[8192])

#endif /* !_SYS_DISKPC98_H_ */
