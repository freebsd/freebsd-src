/*-
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Poul-Henning Kamp
 * and NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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

#include "opt_geom.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/bio.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/stdint.h>
#include <machine/md_var.h>
#include <sys/ctype.h>


#include <sys/lock.h>
#include <sys/mutex.h>
#include <geom/geom.h>

static g_access_t g_disk_access;

struct g_class g_disk_class = {
	"DISK",
	NULL,
	NULL,
	G_CLASS_INITIALIZER
};

static int
g_disk_access(struct g_provider *pp, int r, int w, int e)
{
	struct disk *dp;
	dev_t dev;
	int error;

	g_trace(G_T_ACCESS, "g_disk_access(%s, %d, %d, %d)",
	    pp->name, r, w, e);
	g_topology_assert();
	r += pp->acr;
	w += pp->acw;
	e += pp->ace;
	dp = pp->geom->softc;
	dev = dp->d_dev;
	if ((pp->acr + pp->acw + pp->ace) == 0 && (r + w + e) > 0) {
		mtx_lock(&Giant);
		error = devsw(dev)->d_open(dev, 3, 0, NULL);
		if (error != 0)
			printf("Opened disk %s -> %d\n", pp->name, error);
		mtx_unlock(&Giant);
	} else if ((pp->acr + pp->acw + pp->ace) > 0 && (r + w + e) == 0) {
		mtx_lock(&Giant);
		error = devsw(dev)->d_close(dev, 3, 0, NULL);
		if (error != 0)
			printf("Closed disk %s -> %d\n", pp->name, error);
		mtx_unlock(&Giant);
	} else {
		error = 0;
	}
        pp->mediasize =
	    dp->d_label.d_secsize * (off_t)dp->d_label.d_secperunit;
	return (error);
}

static void
g_disk_kerneldump(struct bio *bp, struct disk *dp)
{ 
	int error;
	struct g_kerneldump *gkd;
	struct dumperinfo di;

	gkd = (struct g_kerneldump*)bp->bio_data;
	printf("Kerneldump off=%jd len=%jd\n", (intmax_t)gkd->offset, (intmax_t)gkd->length);
	di.dumper = (dumper_t *)dp->d_devsw->d_dump;
	di.priv = dp->d_dev;
	di.blocksize = dp->d_label.d_secsize;
	di.mediaoffset = gkd->offset;
	di.mediasize = gkd->length;
	error = set_dumper(&di);
	g_io_fail(bp, error);
}

static void
g_disk_done(struct bio *bp)
{

	mtx_unlock(&Giant);
	bp->bio_completed = bp->bio_length - bp->bio_resid;
	g_std_done(bp);
	mtx_lock(&Giant);
}

static void
g_disk_start(struct bio *bp)
{
	struct bio *bp2;
	dev_t dev;
	struct disk *dp;
	struct g_ioctl *gio;
	int error;

	dp = bp->bio_to->geom->softc;
	dev = dp->d_dev;
	error = 0;
	switch(bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
		bp2 = g_clone_bio(bp);
		bp2->bio_done = g_disk_done;
		bp2->bio_blkno = bp2->bio_offset >> DEV_BSHIFT;
		bp2->bio_pblkno = bp2->bio_blkno;
		bp2->bio_bcount = bp2->bio_length;
		bp2->bio_dev = dev;
		mtx_lock(&Giant);
		devsw(dev)->d_strategy(bp2);
		mtx_unlock(&Giant);
		break;
	case BIO_GETATTR:
		if (g_handleattr_int(bp, "GEOM::sectorsize",
		    dp->d_label.d_secsize))
			break;
		else if (g_handleattr_int(bp, "GEOM::fwsectors",
		    dp->d_label.d_nsectors))
			break;
		else if (g_handleattr_int(bp, "GEOM::fwheads",
		    dp->d_label.d_ntracks))
			break;
		else if (g_handleattr_off_t(bp, "GEOM::mediasize",
		    dp->d_label.d_secsize * (off_t)dp->d_label.d_secperunit))
			break;
		else if (g_handleattr_off_t(bp, "GEOM::frontstuff", 0))
			break;
		else if (!strcmp(bp->bio_attribute, "GEOM::kerneldump"))
			g_disk_kerneldump(bp, dp);
		else if (!strcmp(bp->bio_attribute, "GEOM::ioctl") &&
		    bp->bio_length == sizeof *gio) {
			gio = (struct g_ioctl *)bp->bio_data;
			mtx_lock(&Giant);
			error = devsw(dev)->d_ioctl(dev, gio->cmd,
			    gio->data, gio->fflag, gio->td);
			mtx_unlock(&Giant);
		} else 
			error = ENOIOCTL;
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	if (error) {
		bp->bio_error = error;
		g_io_deliver(bp);
	}
	return;
}

dev_t
disk_create(int unit, struct disk *dp, int flags, struct cdevsw *cdevsw, struct cdevsw *proto)
{
	static int once;
	struct g_geom *gp;
	struct g_provider *pp;
	dev_t dev;

	mtx_unlock(&Giant);
	if (!once) {
		g_add_class(&g_disk_class);
		once++;
	}
	dev = g_malloc(sizeof *dev, M_WAITOK | M_ZERO);
	dp->d_dev = dev;
	dp->d_devsw = cdevsw;
	dev->si_devsw = cdevsw;
	dev->si_disk = dp;
	dev->si_udev = dkmakeminor(unit, WHOLE_DISK_SLICE, RAW_PART);
	g_topology_lock();
	gp = g_new_geomf(&g_disk_class, "%s%d", cdevsw->d_name, unit);
	strcpy(dev->si_name, gp->name);
	gp->start = g_disk_start;
	gp->access = g_disk_access;
	gp->softc = dp;
	dp->d_softc = gp;
	pp = g_new_providerf(gp, "%s", gp->name);
	g_error_provider(pp, 0);
	g_topology_unlock();
	mtx_lock(&Giant);
	return (dev);
}

void disk_dev_synth(dev_t dev);

void
disk_dev_synth(dev_t dev)
{

	return;
}

void
disk_destroy(dev_t dev)
{
	struct disk *dp;
	struct g_geom *gp;

	dp = dev->si_disk;
	gp = dp->d_softc;
	g_free(dev);
	gp->flags |= G_GEOM_WITHER;
	g_orphan_provider(LIST_FIRST(&gp->provider), ENXIO);
}

void
disk_invalidate (struct disk *disk)
{
}


SYSCTL_INT(_debug_sizeof, OID_AUTO, disklabel, CTLFLAG_RD,
    0, sizeof(struct disklabel), "sizeof(struct disklabel)");

SYSCTL_INT(_debug_sizeof, OID_AUTO, diskslices, CTLFLAG_RD,
    0, sizeof(struct diskslices), "sizeof(struct diskslices)");

SYSCTL_INT(_debug_sizeof, OID_AUTO, disk, CTLFLAG_RD,
    0, sizeof(struct disk), "sizeof(struct disk)");
