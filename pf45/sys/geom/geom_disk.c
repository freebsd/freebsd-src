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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_geom.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/bio.h>
#include <sys/conf.h>
#include <sys/ctype.h>
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
static g_init_t g_disk_init;
static g_fini_t g_disk_fini;
static g_start_t g_disk_start;
static g_ioctl_t g_disk_ioctl;
static g_dumpconf_t g_disk_dumpconf;

static struct g_class g_disk_class = {
	.name = "DISK",
	.version = G_VERSION,
	.init = g_disk_init,
	.fini = g_disk_fini,
	.start = g_disk_start,
	.access = g_disk_access,
	.ioctl = g_disk_ioctl,
	.dumpconf = g_disk_dumpconf,
};

static void
g_disk_init(struct g_class *mp __unused)
{

	mtx_init(&g_disk_done_mtx, "g_disk_done", NULL, MTX_DEF);
}

static void
g_disk_fini(struct g_class *mp __unused)
{

	mtx_destroy(&g_disk_done_mtx);
}

DECLARE_GEOM_CLASS(g_disk_class, g_disk);

static void __inline
g_disk_lock_giant(struct disk *dp)
{
	if (dp->d_flags & DISKFLAG_NEEDSGIANT)
		mtx_lock(&Giant);
}

static void __inline
g_disk_unlock_giant(struct disk *dp)
{
	if (dp->d_flags & DISKFLAG_NEEDSGIANT)
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
	dp = pp->geom->softc;
	if (dp == NULL || dp->d_destroyed) {
		/*
		 * Allow decreasing access count even if disk is not
		 * avaliable anymore.
		 */
		if (r <= 0 && w <= 0 && e <= 0)
			return (0);
		return (ENXIO);
	}
	r += pp->acr;
	w += pp->acw;
	e += pp->ace;
	error = 0;
	if ((pp->acr + pp->acw + pp->ace) == 0 && (r + w + e) > 0) {
		if (dp->d_open != NULL) {
			g_disk_lock_giant(dp);
			error = dp->d_open(dp);
			if (bootverbose && error != 0)
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
	if (dp->d_dump == NULL) {
		g_io_deliver(bp, ENODEV);
		return;
	}
	di.dumper = dp->d_dump;
	di.priv = dp;
	di.blocksize = dp->d_sectorsize;
	di.maxiosize = dp->d_maxsize;
	di.mediaoffset = gkd->offset;
	if ((gkd->offset + gkd->length) > dp->d_mediasize)
		gkd->length = dp->d_mediasize - gkd->offset;
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
	if (bp2->bio_error == 0)
		bp2->bio_error = bp->bio_error;
	bp2->bio_completed += bp->bio_completed;
	if ((bp->bio_cmd & (BIO_READ|BIO_WRITE|BIO_DELETE)) &&
	    (dp = bp2->bio_to->geom->softc)) {
		devstat_end_transaction_bio(dp->d_devstat, bp);
	}
	g_destroy_bio(bp);
	bp2->bio_inbed++;
	if (bp2->bio_children == bp2->bio_inbed) {
		bp2->bio_resid = bp2->bio_bcount - bp2->bio_completed;
		g_io_deliver(bp2, bp2->bio_error);
	}
	mtx_unlock(&g_disk_done_mtx);
}

static int
g_disk_ioctl(struct g_provider *pp, u_long cmd, void * data, int fflag, struct thread *td)
{
	struct g_geom *gp;
	struct disk *dp;
	int error;

	gp = pp->geom;
	dp = gp->softc;

	if (dp->d_ioctl == NULL)
		return (ENOIOCTL);
	g_disk_lock_giant(dp);
	error = dp->d_ioctl(dp, cmd, data, fflag, td);
	g_disk_unlock_giant(dp);
	return(error);
}

static void
g_disk_start(struct bio *bp)
{
	struct bio *bp2, *bp3;
	struct disk *dp;
	int error;
	off_t off;

	dp = bp->bio_to->geom->softc;
	if (dp == NULL || dp->d_destroyed) {
		g_io_deliver(bp, ENXIO);
		return;
	}
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
			bp2->bio_pblkno = bp2->bio_offset / dp->d_sectorsize;
			bp2->bio_bcount = bp2->bio_length;
			bp2->bio_disk = dp;
			devstat_start_transaction_bio(dp->d_devstat, bp2);
			g_disk_lock_giant(dp);
			dp->d_strategy(bp2);
			g_disk_unlock_giant(dp);
			bp2 = bp3;
			bp3 = NULL;
		} while (bp2 != NULL);
		break;
	case BIO_GETATTR:
		if (g_handleattr_int(bp, "GEOM::candelete",
		    (dp->d_flags & DISKFLAG_CANDELETE) != 0))
			break;
		else if (g_handleattr_int(bp, "GEOM::fwsectors",
		    dp->d_fwsectors))
			break;
		else if (g_handleattr_int(bp, "GEOM::fwheads", dp->d_fwheads))
			break;
		else if (g_handleattr_off_t(bp, "GEOM::frontstuff", 0))
			break;
		else if (g_handleattr_str(bp, "GEOM::ident", dp->d_ident))
			break;
		else if (g_handleattr(bp, "GEOM::hba_vendor",
		    &dp->d_hba_vendor, 2))
			break;
		else if (g_handleattr(bp, "GEOM::hba_device",
		    &dp->d_hba_device, 2))
			break;
		else if (g_handleattr(bp, "GEOM::hba_subvendor",
		    &dp->d_hba_subvendor, 2))
			break;
		else if (g_handleattr(bp, "GEOM::hba_subdevice",
		    &dp->d_hba_subdevice, 2))
			break;
		else if (!strcmp(bp->bio_attribute, "GEOM::kerneldump"))
			g_disk_kerneldump(bp, dp);
		else 
			error = ENOIOCTL;
		break;
	case BIO_FLUSH:
		g_trace(G_T_TOPOLOGY, "g_disk_flushcache(%s)",
		    bp->bio_to->name);
		if (!(dp->d_flags & DISKFLAG_CANFLUSHCACHE)) {
			g_io_deliver(bp, ENODEV);
			return;
		}
		bp2 = g_clone_bio(bp);
		if (bp2 == NULL) {
			g_io_deliver(bp, ENOMEM);
			return;
		}
		bp2->bio_done = g_disk_done;
		bp2->bio_disk = dp;
		g_disk_lock_giant(dp);
		dp->d_strategy(bp2);
		g_disk_unlock_giant(dp);
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
	if (dp == NULL)
		return;
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
	gp->softc = dp;
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

static void
g_disk_destroy(void *ptr, int flag)
{
	struct disk *dp;
	struct g_geom *gp;

	g_topology_assert();
	dp = ptr;
	gp = dp->d_geom;
	if (gp != NULL) {
		gp->softc = NULL;
		g_wither_geom(gp, ENXIO);
	}
	g_free(dp);
}

/*
 * We only allow printable characters in disk ident,
 * the rest is converted to 'x<HH>'.
 */
static void
g_disk_ident_adjust(char *ident, size_t size)
{
	char *p, tmp[4], newid[DISK_IDENT_SIZE];

	newid[0] = '\0';
	for (p = ident; *p != '\0'; p++) {
		if (isprint(*p)) {
			tmp[0] = *p;
			tmp[1] = '\0';
		} else {
			snprintf(tmp, sizeof(tmp), "x%02hhx",
			    *(unsigned char *)p);
		}
		if (strlcat(newid, tmp, sizeof(newid)) >= sizeof(newid))
			break;
	}
	bzero(ident, size);
	strlcpy(ident, newid, size);
}

struct disk *
disk_alloc()
{
	struct disk *dp;

	dp = g_malloc(sizeof *dp, M_WAITOK | M_ZERO);
	return (dp);
}

void
disk_create(struct disk *dp, int version)
{
	if (version != DISK_VERSION_00 && version != DISK_VERSION_01) {
		printf("WARNING: Attempt to add disk %s%d %s",
		    dp->d_name, dp->d_unit,
		    " using incompatible ABI version of disk(9)\n");
		printf("WARNING: Ignoring disk %s%d\n",
		    dp->d_name, dp->d_unit);
		return;
	}
	KASSERT(dp->d_strategy != NULL, ("disk_create need d_strategy"));
	KASSERT(dp->d_name != NULL, ("disk_create need d_name"));
	KASSERT(*dp->d_name != 0, ("disk_create need d_name"));
	KASSERT(strlen(dp->d_name) < SPECNAMELEN - 4, ("disk name too long"));
	if (dp->d_devstat == NULL)
		dp->d_devstat = devstat_new_entry(dp->d_name, dp->d_unit,
		    dp->d_sectorsize, DEVSTAT_ALL_SUPPORTED,
		    DEVSTAT_TYPE_DIRECT, DEVSTAT_PRIORITY_MAX);
	dp->d_geom = NULL;
	g_disk_ident_adjust(dp->d_ident, sizeof(dp->d_ident));
	g_post_event(g_disk_create, dp, M_WAITOK, dp, NULL);
}

void
disk_destroy(struct disk *dp)
{

	g_cancel_event(dp);
	dp->d_destroyed = 1;
	if (dp->d_devstat != NULL)
		devstat_remove_entry(dp->d_devstat);
	g_post_event(g_disk_destroy, dp, M_WAITOK, NULL);
}

void
disk_gone(struct disk *dp)
{
	struct g_geom *gp;
	struct g_provider *pp;

	gp = dp->d_geom;
	if (gp != NULL)
		LIST_FOREACH(pp, &gp->provider, provider)
			g_wither_provider(pp, ENXIO);
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

	sb = sbuf_new_auto();
	g_waitfor_event(g_kern_disks, sb, M_WAITOK, NULL);
	error = SYSCTL_OUT(req, sbuf_data(sb), sbuf_len(sb) + 1);
	sbuf_delete(sb);
	return error;
}
 
SYSCTL_PROC(_kern, OID_AUTO, disks,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_disks, "A", "names of available disks");

