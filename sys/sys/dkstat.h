/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)dkstat.h	8.2 (Berkeley) 1/21/94
 * $Id: dkstat.h,v 1.8 1997/02/22 09:45:08 peter Exp $
 */

#ifndef _SYS_DKSTAT_H_
#define _SYS_DKSTAT_H_ 1

#define	CP_USER		0
#define	CP_NICE		1
#define	CP_SYS		2
#define	CP_INTR		3
#define	CP_IDLE		4
#define	CPUSTATES	5

#define	DK_NDRIVE	8
#define DK_NAMELEN	8
#ifdef KERNEL
extern long dk_seek[DK_NDRIVE];	/* # seeks */
extern long dk_wds[DK_NDRIVE];	/* # blocks of 32*16-bit words transferred */
extern long dk_wpms[DK_NDRIVE];	/* transfer rate in 16-bit words per second */
extern long dk_xfer[DK_NDRIVE];	/* # transfers */
extern char dk_names[DK_NDRIVE][DK_NAMELEN]; /* name of drive */

extern int dk_busy;		/* bit field of busy drives */
extern int dk_ndrive;		/* number of installed drives */

extern long tk_cancc;
extern long tk_nin;
extern long tk_nout;
extern long tk_rawcc;
#endif

#endif /* _SYS_DKSTAT_H_ */
