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
 *	$Id: diskslice.h,v 1.10 1995/05/30 08:14:19 rgrimes Exp $
 */

#ifndef _SYS_DISKSLICE_H_
#define	_SYS_DISKSLICE_H_

#include <sys/ioccom.h>

#define	BASE_SLICE		2
#define	COMPATIBILITY_SLICE	0
#define	DIOCGSLICEINFO		_IOR('d', 111, struct diskslices)
#define	DIOCSYNCSLICEINFO	_IOW('d', 112, int)
#define	MAX_SLICES		32
#define	WHOLE_DISK_SLICE	1

/* upcoming change from julian.. early warning of probable form */
#if 1
struct	diskslice {
	u_long	ds_offset;		/* starting sector */
	u_long	ds_size;		/* number of sectors */
	int	ds_type;		/* (foreign) slice type */
	struct dkbad_intern *ds_bad;	/* bad sector table, if any */
	struct disklabel *ds_label;	/* BSD label, if any */
	void	*ds_bdev;		/* devfs token for whole slice */
	void	*ds_cdev;		/* devfs token for raw whole slice */
#ifdef MAXPARTITIONS			/* XXX don't depend on dislabel.h */
#if MAXPARTITIONS !=	8		/* but check consistency if possible */
#error "inconsistent MAXPARTITIONS"
#endif
#else
#define	MAXPARTITIONS	8
#endif
	void	*ds_bdevs[MAXPARTITIONS];	/* XXX s.b. in label */
	void	*ds_cdevs[MAXPARTITIONS];	/* XXX s.b. in label */
	u_char	ds_bopenmask;		/* bdevs open */
	u_char	ds_copenmask;		/* cdevs open */
	u_char	ds_openmask;		/* [bc]devs open */
	u_char	ds_wlabel;		/* nonzero if label is writable */
};

#else
/* switch table for slice handlers (sample only) */
struct	slice_switch (
	int	(*slice_load)();
	int	(*slice_check();
	int	(*slice_gone)();
	/*
	 * etc.
	 * each  routine is called with the address of the private data
	 * and the minor number..
	 * Other arguments as needed
	 */
};

struct	diskslice {
	u_long	ds_offset;		/* starting sector */
	u_long	ds_size;		/* number of sectors */
	int	ds_type;		/* (foreign) slice type */
	struct dkbad_intern *ds_bad;	/* bad sector table, if any */
	void	*ds_date;		/* Slice type specific data */
	struct slice_switch *switch;	/* switch table for type handler */
	u_char	ds_bopenmask;		/* bdevs open */
	u_char	ds_copenmask;		/* cdevs open */
	u_char	ds_openmask;		/* [bc]devs open */
	u_char	ds_wlabel;		/* nonzero if label is writable */
};
#endif

struct diskslices {
	struct bdevsw *dss_bdevsw;	/* for containing device */
	struct cdevsw *dss_cdevsw;	/* for containing device */
	int	dss_first_bsd_slice;	/* COMPATIBILTY_SLICE is mapped here */
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
void	dsgone __P((struct diskslices **sspp));
int	dsinit __P((char *dname, dev_t dev, d_strategy_t *strat,
		    struct disklabel *lp, struct diskslices **sspp));
int	dsioctl __P((char *dname, dev_t dev, int cmd, caddr_t data, int flags,
		     struct diskslices **sspp, d_strategy_t *strat,
		     ds_setgeom_t *setgeom));
int	dsisopen __P((struct diskslices *ssp));
char	*dsname __P((char *dname, int unit, int slice, int part,
		     char *partname));
int	dsopen __P((char *dname, dev_t dev, int mode, struct diskslices **sspp,
		    struct disklabel *lp, d_strategy_t *strat,
		    ds_setgeom_t *setgeom, struct bdevsw *bdevsw,
		    struct cdevsw *cdevsw));
int	dssize __P((dev_t dev, struct diskslices **sspp, d_open_t dopen,
		    d_close_t dclose));

#endif /* KERNEL */

#endif /* !_SYS_DISKSLICE_H_ */
