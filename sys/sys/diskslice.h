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
 *	$Id:
 */

#ifndef _SYS_DISKSLICE_H_
#define	_SYS_DISKSLICE_H_

#define	MAX_SLICES		32
#define	WHOLE_DISK_SLICE	0

struct	diskslice {
	u_long	ds_offset;		/* starting sector */
	u_long	ds_size;		/* number of sectors */
	struct dkbad_intern *ds_bad;	/* bad sector table, if any */
	struct disklabel *ds_label;	/* BSD label, if any */
	u_char	ds_bopenmask;		/* bdevs open */
	u_char	ds_copenmask;		/* cdevs open */
	u_char	ds_openmask;		/* [bc]devs open */
	u_char	ds_wlabel;		/* nonzero if label is writable */
};

struct diskslices {
	u_int	dss_nslices;		/* actual dimension of dss_slices[] */
	struct diskslice
		dss_slices[MAX_SLICES];	/* actually usually less */
};

#ifdef KERNEL

#include <sys/conf.h>

#define	dsgetbad(dev, ssp)	(ssp->dss_slices[dkslice(dev)].ds_bad)
#define	dsgetlabel(dev, ssp)	(ssp->dss_slices[dkslice(dev)].ds_label)

struct buf;
struct disklabel;

typedef int ds_setgeom_t __P((struct disklabel *lp));

int	dscheck __P((struct buf *bp, struct diskslices *ssp));
void	dsclose __P((dev_t dev, int mode, struct diskslices *ssp));
int	dsinit __P((char *dname, dev_t dev, d_strategy_t *strat,
		    struct disklabel *lp, struct diskslices **sspp));
int	dsioctl __P((dev_t dev, int cmd, caddr_t data, int flags,
		     struct diskslices *ssp, d_strategy_t *strat,
		     ds_setgeom_t *setgeom));
int	dsopen __P((char *dname, dev_t dev, int mode, struct diskslices **sspp,
		    struct disklabel *lp, d_strategy_t *strat,
		    ds_setgeom_t *setgeom));
int	dswlabel __P((dev_t dev, struct diskslices *ssp, int wlabel));

#endif /* KERNEL */

#endif /* !_SYS_DISKSLICE_H_ */
