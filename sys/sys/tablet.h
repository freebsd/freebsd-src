/*-
 * Copyright (c) 1985, 1986, 1993
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
 *	@(#)tablet.h	8.4 (Berkeley) 7/10/94
 * $FreeBSD$
 */

#ifndef	_SYS_TABLET_H_
#define	_SYS_TABLET_H_

/*
 * Tablet line discipline.
 */

#ifndef KERNEL
#include <sys/types.h>
#endif
#include <sys/ioccom.h>

/*
 * Reads on the tablet return one of the following structures, depending on
 * the underlying tablet type.  The first two are defined such that a read of
 * sizeof (gtcopos) on a non-gtco tablet will return meaningful info.  The
 * in-proximity bit is simulated where the tablet does not directly provide
 * the information.
 */
struct	tbpos {
	int32_t	xpos, ypos;	/* raw x-y coordinates */
	int16_t	status;		/* buttons/pen down */
#define	TBINPROX	0100000		/* pen in proximity of tablet */
	int16_t	scount;		/* sample count */
};

struct	gtcopos {
	int32_t	xpos, ypos;	/* raw x-y coordinates */
	int16_t	status;		/* as above */
	int16_t	scount;		/* sample count */
	int16_t	xtilt, ytilt;	/* raw tilt */
	int16_t	pressure;
	int16_t	pad;		/* pad to longword boundary */
};

struct	polpos {
	int16_t	p_x, p_y, p_z;	/* raw 3-space coordinates */
	int16_t	p_azi, p_pit, p_rol;	/* azimuth, pitch, and roll */
	int16_t	p_stat;		/* status, as above */
	char	p_key;		/* calculator input keyboard */
};

#define BIOSMODE	_IOW('b', 1, int)	/* set mode bit(s) */
#define BIOGMODE	_IOR('b', 2, int)	/* get mode bit(s) */
#define	TBMODE		0xfff0		/* mode bits: */
#define		TBPOINT		0x0010		/* single point */
#define		TBRUN		0x0000		/* runs contin. */
#define		TBSTOP		0x0020		/* shut-up */
#define		TBGO		0x0000		/* ~TBSTOP */
#define	TBTYPE		0x000f		/* tablet type: */
#define		TBUNUSED	0x0
#define		TBHITACHI	0x1		/* hitachi tablet */
#define		TBTIGER		0x2		/* hitachi tiger */
#define		TBGTCO		0x3		/* gtco */
#define		TBPOL		0x4		/* polhemus 3space */
#define		TBHDG		0x5		/* hdg-1111b, low res */
#define		TBHDGHIRES	0x6		/* hdg-1111b, high res */
#define		TBDIGI		0x7		/* gtco digi-pad, low res */
#define		TBDIGIHIRES	0x8		/* gtco digi-pad, high res */
#define BIOSTYPE	_IOW('b', 3, int)	/* set tablet type */
#define BIOGTYPE	_IOR('b', 4, int)	/* get tablet type*/

#endif /* !_SYS_TABLET_H_ */
