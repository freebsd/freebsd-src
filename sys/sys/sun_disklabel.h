/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)sun_disklabel.h	8.1 (Berkeley) 6/11/93
 *	$NetBSD: disklabel.h,v 1.2 1998/08/22 14:55:28 mrg Exp $
 *
 * $FreeBSD$ 
 */

/*
 * SunOS disk label layout (only relevant portions discovered here).
 */

#define	SUN_DKMAGIC	55998

/* geometry info */
struct sun_dkgeom {
	u_short	sdkc_ncylinders;	/* data cylinders */
	u_short	sdkc_acylinders;	/* alternate cylinders */
	u_short	sdkc_xxx1;
	u_short	sdkc_ntracks;		/* tracks per cylinder */
	u_short	sdkc_xxx2;
	u_short	sdkc_nsectors;		/* sectors per track */
	u_short	sdkc_interleave;	/* interleave factor */
	u_short	sdkc_xxx3;
	u_short	sdkc_xxx4;
	u_short	sdkc_sparespercyl;	/* spare sectors per cylinder */
	u_short	sdkc_rpm;		/* rotational speed */
	u_short	sdkc_pcylinders;	/* physical cylinders */
	u_short	sdkc_xxx5[7];
};

/* controller info */
struct sun_dkctlr {
	int	sdkc_addr;		/* controller address */
	short	sdkc_unit;		/* unit (slave) address */
	short	sdkc_type;		/* controller type */
	short	sdkc_flags;		/* flags */
};

/* partition info */
struct sun_dkpart {
	u_int32_t	sdkp_cyloffset;		/* starting cylinder */
	u_int32_t	sdkp_nsectors;		/* number of sectors */
};

struct sun_disklabel {			/* total size = 512 bytes */
	char		sl_text[128];
	char		sl_xxx1[292];
	u_int16_t	sl_rpm;		/* rotational speed */
	u_int16_t	sl_pcylinders;		/* number of physical cyls */
	u_int16_t	sl_sparespercyl;	/* spare sectors per cylinder */
	char		sl_xxx3[4];
	u_int16_t	sl_interleave;		/* interleave factor */
	u_int16_t	sl_ncylinders;		/* data cylinders */
	u_int16_t	sl_acylinders;		/* alternate cylinders */
	u_int16_t	sl_ntracks;		/* tracks per cylinder */
	u_int16_t	sl_nsectors;		/* sectors per track */
	char		sl_xxx4[4];
	struct sun_dkpart sl_part[8];	/* partition layout */
	u_int16_t	sl_magic;		/* == SUN_DKMAGIC */
	u_int16_t	sl_cksum;		/* xor checksum of all shorts */
};

CTASSERT(sizeof (struct sun_disklabel) == 512);
