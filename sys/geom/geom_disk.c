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
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/devicestat.h>
#include <machine/md_var.h>

#include <sys/lock.h>
#include <sys/mutex.h>
#include <geom/geom.h>
#include <geom/geom_disk.h>
#include <geom/geom_int.h>

static struct mtx g_disk_done_mtx;

static g_access_t g_disk_access;

struct g_class g_disk_class = {
	.name = "DISK",
	G_CLASS_INITIALIZER
};

static void
g_disk_init(void)
{
	mtx_unlock(&Giant);
	g_add_class(&g_disk_class);
	mtx_init(&g_disk_done_mtx, "g_disk_done", MTX_DEF, 0);
	mtx_lock(&Giant);
}

DECLARE_GEOM_CLASS_INIT(g_disk_class, g_disk, g_disk_init);

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
	int error;

	g_trace(G_T_ACCESS, "g_disk_access(%s, %d, %d, %d)",
	    pp->name, r, w, e);
	g_topology_assert();
	r += pp->acr;
	w += pp->acw;
	e += pp->ace;
	dp = pp->geom->softc;
	if (dp == NULL)
		return (ENXIO);
	error = 0;
	if ((pp->acr + pp->acw + pp->ace) == 0 && (r + w + e) > 0) {
		if (dp->d_open != NULL) {
			g_disk_lock_giant(dp);
			error = dp->d_open(dp);
			if (error != 0)
				printf("Opened disk %s -> %d\n",
				    pp->name, error);
			g_disk_unlock_giant(dp);
		}
		pp->mediasize = dp->d_mediasize;
		pp->sectorsize = dp->d_sectorsize;
		dp->d_flags |= DISKFLAG_OPEN;
		if (dp->d_maxsize == 0) {
			printf("WARNING: Disk drive %s%d has no d_maxsize\n",
			    dp->d_name, dp->d_unit);
			dp->d_maxsize = DFLTPHYS;
		}
	} else if ((pp->acr + pp->acw + pp->ace) > 0 && (r + w + e) == 0) {
		if (dp->d_close != NULL) {
			g_disk_lock_giant(dp);
			error = dp->d_close(dp);
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
	di.priv = dp;
	di.blocksize = dp->d_sectorsize;
	di.mediaoffset = gkd->offset;
	di.mediasize = gkd->length;
	error = set_dumper(&di);
	g_io_deliver(bp, error);
}

static void
g_disk_done(struct bio *bp)
{
	struct bio *bp2;
	struct disk *dp;

	/* See "notes" for why we need a mutex here */
	/* XXX: will witness accept a mix of Giant/unGiant drivers here ? */
	mtx_lock(&g_disk_done_mtx);
	bp->bio_completed = bp->bio_length - bp->bio_resid;

	bp2 = bp->bio_parent;
	dp = bp2->bio_to->geom->softc;
	if (bp2->bio_error == 0)
		bp2->bio_error = bp->bio_error;
	bp2->bio_completed += bp->bio_completed;
	g_destroy_bio(bp);
	bp2->bio_inbed++;
	if (bp2->bio_children == bp2->bio_inbed) {
		bp2->bio_resid = bp2->bio_bcount - bp2->bio_completed;
		devstat_end_transaction_bio(dp->d_devstat, bp2);
		g_io_deliver(bp2, bp2->bio_error);
	}
	mtx_unlock(&g_disk_done_mtx);
}

static void
g_disk_start(struct bio *bp)
{
	struct bio *bp2, *bp3;
	struct disk *dp;
	struct g_ioctl *gio;
	int error;
	off_t off;

	dp = bp->bio_to->geom->softc;
	if (dp == NULL)
		g_io_deliver(bp, ENXIO);
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
		off = 0;
		bp3 = NULL;
		bp2 = g_clone_bio(bp);
		if (bp2 == NULL) {
			error = ENOMEM;
			break;
		}
		devstat_start_transaction_bio(dp->d_devstat, bp);
		do {
			bp2->bio_offset += off;
			bp2->bio_length -= off;
			bp2->bio_data += off;
			if (bp2->bio_length > dp->d_maxsize) {
				/*
				 * XXX: If we have a stripesize we should really
				 * use it here.
				 */
				bp2->bio_length = dp->d_maxsize;
				off += dp->d_maxsize;
				/*
				 * To avoid a race, we need to grab the next bio
				 * before we schedule this one.  See "notes".
				 */
				bp3 = g_clone_bio(bp);
				if (bp3 == NULL)
					bp->bio_error = ENOMEM;
			}
			bp2->bio_done = g_disk_done;
			bp2->bio_blkno = bp2->bio_offset >> DEV_BSHIFT;
			bp2->bio_pblkno = bp2->bio_offset / dp->d_sectorsize;
			bp2->bio_bcount = bp2->bio_length;
			bp2->bio_disk = dp;
			g_disk_lock_giant(dp);
			dp->d_strategy(bp2);
			g_disk_unlock_giant(dp);
			bp2 = bp3;
			bp3 = NULL;
		} while (bp2 != NULL);
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
		else if ((g_debugflags & G_F_DISKIOCTL) &&
		    (dp->d_ioctl != NULL) &&
		    !strcmp(bp->bio_attribute, "GEOM::ioctl") &&
		    bp->bio_length == sizeof *gio) {
			gio = (struct g_ioctl *)bp->bio_data;
			gio->dev =  dp;
			gio->func = (d_ioctl_t *)(dp->d_ioctl);
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
g_disk_create(void *arg, int flag)
{
	struct g_geom *gp;
	struct g_provider *pp;
	struct disk *dp;

	if (flag == EV_CANCEL)
		return;
	g_topology_assert();
	dp = arg;
	gp = g_new_geomf(&g_disk_class, "%s%d", dp->d_name, dp->d_unit);
	gp->start = g_disk_start;
	gp->access = g_disk_access;
	gp->softc = dp;
	gp->dumpconf = g_disk_dumpconf;
	pp = g_new_providerf(gp, "%s", gp->name);
	pp->mediasize = dp->d_mediasize;
	pp->sectorsize = dp->d_sectorsize;
	if (dp->d_flags & DISKFLAG_CANDELETE)
		pp->flags |= G_PF_CANDELETE;
	pp->stripeoffset = dp->d_stripeoffset;
	pp->stripesize = dp->d_stripesize;
	if (bootverbose)
		printf("GEOM: new disk %s\n", gp->name);
	dp->d_geom = gp;
	g_error_provider(pp, 0);
}



void
disk_create(int unit, struct disk *dp, int flags, void *unused __unused, void * unused2 __unused)
{

	dp->d_unit = unit;
	dp->d_flags = flags;
	KASSERT(dp->d_strategy != NULL, ("disk_create need d_strategy"));
	KASSERT(dp->d_name != NULL, ("disk_create need d_name"));
	KASSERT(*dp->d_name != 0, ("disk_create need d_name"));
	KASSERT(strlen(dp->d_name) < SPECNAMELEN - 4, ("disk name too long"));
	dp->d_devstat = devstat_new_entry(dp->d_name, dp->d_unit,
	    dp->d_sectorsize, DEVSTAT_ALL_SUPPORTED,
	    DEVSTAT_TYPE_DIRECT, DEVSTAT_PRIORITY_MAX);
	dp->d_geom = NULL;
	g_post_event(g_disk_create, dp, M_WAITOK, dp, NULL);
}

/*
 * XXX: There is a race if disk_destroy() is called while the g_disk_create()
 * XXX: event is running.  I belive the current result is that disk_destroy()
 * XXX: actually doesn't do anything.  Considering that the driver owns the
 * XXX: struct disk and is likely to free it in a few moments, this can
 * XXX: hardly be said to be optimal.  To what extent we can sleep in
 * XXX: disk_create() and disk_destroy() is currently undefined (but generally
 * XXX: undesirable) so any solution seems to involve an intrusive decision.
 */

static void
disk_destroy_event(void *ptr, int flag)
{

	g_topology_assert();
	g_wither_geom(ptr, ENXIO);
}

void
disk_destroy(struct disk *dp)
{
	struct g_geom *gp;

	g_cancel_event(dp);
	gp = dp->d_geom;
	if (gp == NULL)
		return;
	gp->softc = NULL;
	devstat_remove_entry(dp->d_devstat);
	g_post_event(disk_destroy_event, gp, M_WAITOK, NULL, NULL);
}

static void
g_kern_disks(void *p, int flag __unused)
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
}

static int
sysctl_disks(SYSCTL_HANDLER_ARGS)
{
	int error;
	struct sbuf *sb;

	sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	sbuf_clear(sb);
	g_waitfor_event(g_kern_disks, sb, M_WAITOK, NULL);
	error = SYSCTL_OUT(req, sbuf_data(sb), sbuf_len(sb) + 1);
	sbuf_delete(sb);
	return error;
}
 
SYSCTL_PROC(_kern, OID_AUTO, disks, CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NOLOCK, 0, 0, 
    sysctl_disks, "A", "names of available disks");

