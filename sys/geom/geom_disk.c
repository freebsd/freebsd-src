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
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/stdint.h>
#include <machine/md_var.h>


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

DECLARE_GEOM_CLASS(g_disk_class, g_disk);

static void __inline
g_disk_lock_giant(struct disk *dp)
{
	if (dp->d_flags & DISKFLAG_NOGIANT)
		return;
	mtx_lock(&Giant);
}

static void __inline
g_disk_unlock_giant(struct disk *dp)
{
	if (dp->d_flags & DISKFLAG_NOGIANT)
		return;
	mtx_unlock(&Giant);
}

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
	error = 0;
	if ((pp->acr + pp->acw + pp->ace) == 0 && (r + w + e) > 0) {
		if (dp->d_open != NULL) {
			g_disk_lock_giant(dp);
			error = dp->d_open(dev, FREAD | FWRITE, 0, NULL);
			if (error != 0)
				printf("Opened disk %s -> %d\n",
				    pp->name, error);
			g_disk_unlock_giant(dp);
		}
		pp->mediasize = dp->d_mediasize;
		pp->sectorsize = dp->d_sectorsize;
		dp->d_flags |= DISKFLAG_OPEN;
	} else if ((pp->acr + pp->acw + pp->ace) > 0 && (r + w + e) == 0) {
		if (dp->d_close != NULL) {
			g_disk_lock_giant(dp);
			error = dp->d_close(dev, FREAD | FWRITE, 0, NULL);
			if (error != 0)
				printf("Closed disk %s -> %d\n",
				    pp->name, error);
			g_disk_unlock_giant(dp);
		}
		dp->d_flags &= ~DISKFLAG_OPEN;
	}
	return (error);
}

static void
g_disk_kerneldump(struct bio *bp, struct disk *dp)
{ 
	int error;
	struct g_kerneldump *gkd;
	struct dumperinfo di;
	struct g_geom *gp;

	gkd = (struct g_kerneldump*)bp->bio_data;
	gp = bp->bio_to->geom;
	g_trace(G_T_TOPOLOGY, "g_disk_kernedump(%s, %jd, %jd)",
		gp->name, (intmax_t)gkd->offset, (intmax_t)gkd->length);
	di.dumper = dp->d_dump;
	di.priv = dp->d_dev;
	di.blocksize = dp->d_sectorsize;
	di.mediaoffset = gkd->offset;
	di.mediasize = gkd->length;
	error = set_dumper(&di);
	g_io_deliver(bp, error);
}

static void
g_disk_done(struct bio *bp)
{
	struct disk *dp;

	dp = bp->bio_disk;
	bp->bio_completed = bp->bio_length - bp->bio_resid;
	if (!(dp->d_flags & DISKFLAG_NOGIANT)) {
		DROP_GIANT();
		g_std_done(bp);
		PICKUP_GIANT();
	} else {
		g_std_done(bp);
	}
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
	error = EJUSTRETURN;
	switch(bp->bio_cmd) {
	case BIO_DELETE:
		if (!(dp->d_flags & DISKFLAG_CANDELETE)) {
			error = 0;
			break;
		}
		/* fall-through */
	case BIO_READ:
	case BIO_WRITE:
		bp2 = g_clone_bio(bp);
		bp2->bio_done = g_disk_done;
		bp2->bio_blkno = bp2->bio_offset >> DEV_BSHIFT;
		bp2->bio_pblkno = bp2->bio_offset / dp->d_sectorsize;
		bp2->bio_bcount = bp2->bio_length;
		bp2->bio_dev = dev;
		bp2->bio_disk = dp;
		g_disk_lock_giant(dp);
		dp->d_strategy(bp2);
		g_disk_unlock_giant(dp);
		break;
	case BIO_GETATTR:
		if (g_handleattr_int(bp, "GEOM::fwsectors", dp->d_fwsectors))
			break;
		else if (g_handleattr_int(bp, "GEOM::fwheads", dp->d_fwheads))
			break;
		else if (g_handleattr_off_t(bp, "GEOM::frontstuff", 0))
			break;
		else if (!strcmp(bp->bio_attribute, "GEOM::kerneldump"))
			g_disk_kerneldump(bp, dp);
		else if (dp->d_ioctl != NULL &&
		    !strcmp(bp->bio_attribute, "GEOM::ioctl") &&
		    bp->bio_length == sizeof *gio) {
			gio = (struct g_ioctl *)bp->bio_data;
			gio->func = dp->d_ioctl;
			gio->dev =  dp->d_dev;
			error = EDIRIOCTL;
		} else 
			error = ENOIOCTL;
		break;
	case BIO_SETATTR:
		if (!strcmp(bp->bio_attribute, "GEOM::ioctl") &&
		    bp->bio_length == sizeof *gio) {
			gio = (struct g_ioctl *)bp->bio_data;
			gio->func = devsw(dp->d_dev)->d_ioctl;
			gio->dev =  dp->d_dev;
			error = EDIRIOCTL;
		} else 
			error = ENOIOCTL;
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	if (error != EJUSTRETURN)
		g_io_deliver(bp, error);
	return;
}

static void
g_disk_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp, struct g_consumer *cp, struct g_provider *pp)
{
	struct disk *dp;

	dp = gp->softc;
	if (indent == NULL) {
		sbuf_printf(sb, " hd %u", dp->d_fwheads);
		sbuf_printf(sb, " sc %u", dp->d_fwsectors);
		return;
	}
	if (pp != NULL) {
		sbuf_printf(sb, "%s<fwheads>%u</fwheads>\n",
		    indent, dp->d_fwheads);
		sbuf_printf(sb, "%s<fwsectors>%u</fwsectors>\n",
		    indent, dp->d_fwsectors);
	}
}

static void
g_disk_create(void *arg)
{
	struct g_geom *gp;
	struct g_provider *pp;
	dev_t dev;

	g_topology_assert();
	dev = arg;
	gp = g_new_geomf(&g_disk_class, dev->si_name);
	gp->start = g_disk_start;
	gp->access = g_disk_access;
	gp->softc = dev->si_disk;
	gp->dumpconf = g_disk_dumpconf;
	dev->si_disk->d_softc = gp;
	pp = g_new_providerf(gp, "%s", gp->name);
	pp->mediasize = dev->si_disk->d_mediasize;
	pp->sectorsize = dev->si_disk->d_sectorsize;
	g_error_provider(pp, 0);
	if (bootverbose)
		printf("GEOM: new disk %s\n", gp->name);
}



dev_t
disk_create(int unit, struct disk *dp, int flags, struct cdevsw *cdevsw, void * unused __unused)
{
	dev_t dev;

	dev = g_malloc(sizeof *dev, M_ZERO);
	dp->d_dev = dev;
	dp->d_flags = flags;
	dp->d_devsw = cdevsw;
	dev->si_devsw = cdevsw;
	if (cdevsw != NULL) {
		dp->d_open = cdevsw->d_open;
		dp->d_close = cdevsw->d_close;
		dp->d_strategy = cdevsw->d_strategy;
		dp->d_ioctl = cdevsw->d_ioctl;
		dp->d_dump = (dumper_t *)cdevsw->d_dump;
		dp->d_name = cdevsw->d_name;
	} 
	KASSERT(dp->d_strategy != NULL, ("disk_create need d_strategy"));
	KASSERT(dp->d_name != NULL, ("disk_create need d_name"));
	KASSERT(*dp->d_name != 0, ("disk_create need d_name"));
	dev->si_disk = dp;
	dev->si_udev = 0x10002; /* XXX: Needed ? */
	sprintf(dev->si_name, "%s%d", dp->d_name, unit);
	g_call_me(g_disk_create, dev);
	return (dev);
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
	gp->softc = NULL;
	g_orphan_provider(LIST_FIRST(&gp->provider), ENXIO);
}

static void
g_kern_disks(void *p)
{
	struct sbuf *sb;
	struct g_geom *gp;
	char *sp;

	sb = p;
	sp = "";
	g_topology_assert();
	LIST_FOREACH(gp, &g_disk_class.geom, geom) {
		sbuf_printf(sb, "%s%s", sp, gp->name);
		sp = " ";
	}
	sbuf_finish(sb);
	wakeup(sb);
}

static int
sysctl_disks(SYSCTL_HANDLER_ARGS)
{
	int error;
	struct sbuf *sb;

	sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	sbuf_clear(sb);
	g_call_me(g_kern_disks, sb);
	do {
		tsleep(sb, PZERO, "kern.disks", hz);
	} while(!sbuf_done(sb));
	error = SYSCTL_OUT(req, sbuf_data(sb), sbuf_len(sb) + 1);
	sbuf_delete(sb);
	return error;
}
 
SYSCTL_PROC(_kern, OID_AUTO, disks, CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NOLOCK, 0, 0, 
    sysctl_disks, "A", "names of available disks");

