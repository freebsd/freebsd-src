/*	$FreeBSD$ */
/*	$NetBSD: direntry.h,v 1.7 1994/08/21 18:43:54 ws Exp $	*/

/*-
 * Copyright (C) 1994 Wolfgang Solfrank.
 * Copyright (C) 1994 TooLs GmbH.
 * All rights reserved.
 * Original code by Paul Popelka (paulp@uts.amdahl.com) (see below).
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 * You can do anything you want with this software, just don't say you wrote
 * it, and don't remove this notice.
 *
 * This software is provided "as is".
 *
 * The author supplies this software to be publicly redistributed on the
 * understanding that the author is not responsible for the correct
 * functioning of this software in any circumstances and is not liable for
 * any damages caused by this software.
 *
 * October 1992
 */

/*
 * Structure of a dos directory entry.
 */
struct direntry {
	u_char deName[8];	/* filename, blank filled */
#define	SLOT_EMPTY	0x00	/* slot has never been used */
#define	SLOT_E5		0x05	/* the real value is 0xe5 */
#define	SLOT_DELETED	0xe5	/* file in this slot deleted */
	u_char deExtension[3];	/* extension, blank filled */
	u_char deAttributes;	/* file attributes */
#define	ATTR_NORMAL	0x00	/* normal file */
#define	ATTR_READONLY	0x01	/* file is readonly */
#define	ATTR_HIDDEN	0x02	/* file is hidden */
#define	ATTR_SYSTEM	0x04	/* file is a system file */
#define	ATTR_VOLUME	0x08	/* entry is a volume label */
#define	ATTR_DIRECTORY	0x10	/* entry is a directory name */
#define	ATTR_ARCHIVE	0x20	/* file is new or modified */
	u_char deReserved[10];	/* reserved */
	u_char deTime[2];	/* create/last update time */
	u_char deDate[2];	/* create/last update date */
	u_char deStartCluster[2]; /* starting cluster of file */
	u_char deFileSize[4];	/* size of file in bytes */
};

/*
 * This is the format of the contents of the deTime field in the direntry
 * structure.
 * We don't use bitfields because we don't know how compilers for
 * arbitrary machines will lay them out.
 */
#define DT_2SECONDS_MASK	0x1F	/* seconds divided by 2 */
#define DT_2SECONDS_SHIFT	0
#define DT_MINUTES_MASK		0x7E0	/* minutes */
#define DT_MINUTES_SHIFT	5
#define DT_HOURS_MASK		0xF800	/* hours */
#define DT_HOURS_SHIFT		11

/*
 * This is the format of the contents of the deDate field in the direntry
 * structure.
 */
#define DD_DAY_MASK		0x1F	/* day of month */
#define DD_DAY_SHIFT		0
#define DD_MONTH_MASK		0x1E0	/* month */
#define DD_MONTH_SHIFT		5
#define DD_YEAR_MASK		0xFE00	/* year - 1980 */
#define DD_YEAR_SHIFT		9

#ifdef KERNEL
void unix2dostime __P((struct timespec * tsp, u_short * ddp, u_short * dtp));
void dos2unixtime __P((u_short dd, u_short dt, struct timespec * tsp));
int dos2unixfn __P((u_char dn[11], u_char * un));
void unix2dosfn __P((u_char * un, u_char dn[11], int unlen));
#endif	/* KERNEL */
