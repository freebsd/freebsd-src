/*-
 * Copyright (c) 1982, 1986, 1993, 1994
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
 *	@(#)dkbad.h	8.2 (Berkeley) 7/10/94
 * $FreeBSD$
 */

#ifndef _SYS_DKBAD_H_
#define	_SYS_DKBAD_H_

/*
 * Definitions needed to perform bad sector revectoring ala DEC STD 144.
 *
 * The bad sector information is located in the first 5 even numbered
 * sectors of the last track of the disk pack.  There are five identical
 * copies of the information, described by the dkbad structure.
 *
 * Replacement sectors are allocated starting with the first sector before
 * the bad sector information and working backwards towards the beginning of
 * the disk.  A maximum of 126 bad sectors are supported.  The position of
 * the bad sector in the bad sector table determines which replacement sector
 * it corresponds to.
 *
 * The bad sector information and replacement sectors are conventionally
 * only accessible through the 'c' file system partition of the disk.  If
 * that partition is used for a file system, the user is responsible for
 * making sure that it does not overlap the bad sector information or any
 * replacement sectors.
 */

#define	DKBAD_MAGIC	0x4321		/* normal value for bt_flag */
#define	DKBAD_MAXBAD	126		/* maximum bad sectors supported */
#define	DKBAD_NOCYL	0xffff		/* cylinder to mark end of disk table */
#define	DKBAD_NOTRKSEC	0xffff		/* track/sector to mark end */

struct dkbad {
	int32_t   bt_csn;		/* cartridge serial number */
	u_int16_t bt_mbz;		/* unused; should be 0 */
	u_int16_t bt_flag;		/* -1 => alignment cartridge */
	struct bt_bad {
		u_int16_t bt_cyl;	/* cylinder number of bad sector */
		u_int16_t bt_trksec;	/* track and sector number */
	} bt_bad[DKBAD_MAXBAD];
};

#define	ECC	0
#define	SSE	1
#define	BSE	2
#define	CONT	3

#ifdef KERNEL
#define	DKBAD_NOSECT	(-1)		/* sector to mark end of core table */

struct dkbad_intern {
	daddr_t	bi_maxspare;		/* last spare sector */
	u_int	bi_nbad;		/* actual dimension of bi_badsect[] */
	long	bi_bad[DKBAD_MAXBAD + 1];	/* actually usually less */
};

struct disklabel;

struct dkbad_intern *internbad144 __P((struct dkbad *btp,
				       struct disklabel *lp));
char	*readbad144 __P((dev_t dev, void (*strat)(struct buf *bp),
			 struct disklabel *lp, struct dkbad *btp));
daddr_t	transbad144 __P((struct dkbad_intern *bip, daddr_t blkno));
#endif

#endif /* !_SYS_DKBAD_H_ */
