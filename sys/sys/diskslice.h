/*-
 * Copyright (c) 1994 Bruce D. Evans.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_SYS_DISKSLICE_H_
#define	_SYS_DISKSLICE_H_

#ifndef _KERNEL
#include <sys/types.h>
#endif
#include <sys/ioccom.h>

#define	BASE_SLICE		2
#define	COMPATIBILITY_SLICE	0
#define	DIOCGSLICEINFO		_IOR('d', 111, struct diskslices)
#define	DIOCSYNCSLICEINFO	_IOW('d', 112, int)
#define	MAX_SLICES		32
#define	WHOLE_DISK_SLICE	1

struct	diskslice {
	u_long	ds_offset;		/* starting sector */
	u_long	ds_size;		/* number of sectors */
	int	ds_type;		/* (foreign) slice type */
#ifdef PC98
	int	ds_subtype;		/* sub slice type */
	u_char	ds_name[16];		/* slice name */
#endif
	struct disklabel *ds_label;	/* BSD label, if any */
	u_char	ds_openmask;		/* devs open */
	u_char	ds_wlabel;		/* nonzero if label is writable */
};

struct diskslices {
	struct cdevsw *dss_cdevsw;	/* for containing device */
	int	dss_first_bsd_slice;	/* COMPATIBILITY_SLICE is mapped here */
	u_int	dss_nslices;		/* actual dimension of dss_slices[] */
	u_int	dss_oflags;		/* copy of flags for "first" open */
	int	dss_secmult;		/* block to sector multiplier */
	int	dss_secshift;		/* block to sector shift (or -1) */
	int	dss_secsize;		/* sector size */
	struct diskslice
		dss_slices[MAX_SLICES];	/* actually usually less */
};

#ifdef _KERNEL

/* Flags for dsopen(). */
#define	DSO_NOLABELS	1
#define	DSO_ONESLICE	2

#define	dsgetlabel(dev, ssp)	(ssp->dss_slices[dkslice(dev)].ds_label)

struct bio;
struct disklabel;

int	dscheck __P((struct bio *bp, struct diskslices *ssp));
void	dsclose __P((dev_t dev, int mode, struct diskslices *ssp));
void	dsgone __P((struct diskslices **sspp));
int	dsinit __P((dev_t dev, struct disklabel *lp,
		     struct diskslices **sspp));
int	dsioctl __P((dev_t dev, u_long cmd, caddr_t data,
		     int flags, struct diskslices **sspp));
int	dsisopen __P((struct diskslices *ssp));
struct diskslices *dsmakeslicestruct __P((int nslices, struct disklabel *lp));
char	*dsname __P((dev_t dev, int unit, int slice, int part,
		     char *partname));
int	dsopen __P((dev_t dev, int mode, u_int flags,
		    struct diskslices **sspp, struct disklabel *lp));
int	dssize __P((dev_t dev, struct diskslices **sspp));

#endif /* _KERNEL */

#endif /* !_SYS_DISKSLICE_H_ */
