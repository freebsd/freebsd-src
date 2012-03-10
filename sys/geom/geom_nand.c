/*-
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * Copyright (c) 2010 Semihalf
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
 * From FreeBSD: src/sys/geom/geom_disk.c,v 1.110.2.2 2009/09/15 11:23:59 pjd
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
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/devicestat.h>
#include <sys/sbuf.h>
#include <machine/md_var.h>

#include <sys/lock.h>
#include <sys/mutex.h>
#include <geom/geom.h>
#include <geom/geom_nand.h>
#include <geom/geom_int.h>

static struct mtx g_gnand_done_mtx;

static g_access_t g_gnand_access;
static g_init_t g_gnand_init;
static g_fini_t g_gnand_fini;
static g_start_t g_gnand_start;
static g_ioctl_t g_gnand_ioctl;
static g_dumpconf_t g_gnand_dumpconf;

static struct g_class g_gnand_class = {
	.name = "GNAND",
	.version = G_VERSION,
	.init = g_gnand_init,
	.fini = g_gnand_fini,
	.start = g_gnand_start,
	.access = g_gnand_access,
	.ioctl = g_gnand_ioctl,
	.dumpconf = g_gnand_dumpconf,
};

static void
g_gnand_init(struct g_class *mp __unused)
{

	mtx_init(&g_gnand_done_mtx, "g_gnand_done", NULL, MTX_DEF);
}

static void
g_gnand_fini(struct g_class *mp __unused)
{

	mtx_destroy(&g_gnand_done_mtx);
}

DECLARE_GEOM_CLASS(g_gnand_class, g_gnand);

static void __inline
g_gnand_lock_giant(struct gnand *dp)
{
	if (dp->d_flags & DISKFLAG_NEEDSGIANT)
		mtx_lock(&Giant);
}

static void __inline
g_gnand_unlock_giant(struct gnand *dp)
{
	if (dp->d_flags & DISKFLAG_NEEDSGIANT)
		mtx_unlock(&Giant);
}

static int
g_gnand_access(struct g_provider *pp, int r, int w, int e)
{
	struct gnand *dp;
	int error;

	g_trace(G_T_ACCESS, "g_gnand_access(%s, %d, %d, %d)",
	    pp->name, r, w, e);
	g_topology_assert();
	dp = pp->geom->softc;
	if (dp == NULL || dp->d_destroyed) {
		/*
		 * Allow decreasing access count even if gnand is not
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
			g_gnand_lock_giant(dp);
			error = dp->d_open(dp);
			if (bootverbose && error != 0)
				printf("Opened gnand %s -> %d\n",
				    pp->name, error);
			g_gnand_unlock_giant(dp);
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
			g_gnand_lock_giant(dp);
			error = dp->d_close(dp);
			if (error != 0)
				printf("Closed gnand %s -> %d\n",
				    pp->name, error);
			g_gnand_unlock_giant(dp);
		}
		dp->d_flags &= ~DISKFLAG_OPEN;
	}
	return (error);
}

static void
g_gnand_done(struct bio *bp)
{
	struct bio *bp2;
	struct gnand *dp;

	/* See "notes" for why we need a mutex here */
	/* XXX: will witness accept a mix of Giant/unGiant drivers here ? */
	mtx_lock(&g_gnand_done_mtx);
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
	mtx_unlock(&g_gnand_done_mtx);
}

static int
g_gnand_ioctl(struct g_provider *pp, u_long cmd, void * data, int fflag,
    struct thread *td)
{
	struct g_geom *gp;
	struct gnand *dp;
	int error;

	gp = pp->geom;
	dp = gp->softc;

	if (dp->d_ioctl == NULL)
		return (ENOIOCTL);
	g_gnand_lock_giant(dp);
	error = dp->d_ioctl(dp, cmd, data, fflag, td);
	g_gnand_unlock_giant(dp);
	return (error);
}

static void
g_gnand_start(struct bio *bp)
{
	struct bio *bp2, *bp3;
	struct gnand *dp;
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
	case BIO_READ:
	case BIO_READOOB:
	case BIO_WRITE:
	case BIO_WRITEOOB:
		g_trace(G_T_BIO,
		    "g_gnand_start(%p) offset %jd length %jd data %p cmd %d",
		    bp, (intmax_t)bp->bio_offset, (intmax_t)bp->bio_length,
		    bp->bio_data, bp->bio_cmd);
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
			bp2->bio_done = g_gnand_done;
			bp2->bio_pblkno = bp2->bio_offset / dp->d_sectorsize;
			bp2->bio_bcount = bp2->bio_length;
			bp2->bio_nand = dp;
			devstat_start_transaction_bio(dp->d_devstat, bp2);
			g_gnand_lock_giant(dp);
			dp->d_strategy(bp2);
			g_gnand_unlock_giant(dp);
			bp2 = bp3;
			bp3 = NULL;
		} while (bp2 != NULL);
		break;
	case BIO_GETATTR:
		if (g_handleattr_int(bp, "NAND::oobsize", dp->d_oobsize))
			break;
		else if (g_handleattr(bp, "NAND::device", &(dp->d_dev),
		    sizeof(device_t)))
			break;
		else if (g_handleattr_int(bp, "NAND::pagesize", dp->d_pagesize))
			break;
		else if (g_handleattr_int(bp, "NAND::blocksize", dp->d_maxsize))
			break;
		else if (g_handleattr_off_t(bp, "GEOM::frontstuff", 0))
			break;
		else if (g_handleattr_str(bp, "GEOM::ident", dp->d_ident))
			break;
		else
			error = ENOIOCTL;
		break;
	case BIO_FLUSH:
			g_io_deliver(bp, ENODEV);
			return;
	default:
		error = EOPNOTSUPP;
		break;
	}
	if (error != EJUSTRETURN)
		g_io_deliver(bp, error);
	return;
}

static void
g_gnand_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp,
    struct g_consumer *cp, struct g_provider *pp)
{
	struct gnand *dp;

	dp = gp->softc;
	if (dp == NULL)
		return;
	if (indent == NULL) {
		sbuf_printf(sb, " page %u", dp->d_pagesize);
		sbuf_printf(sb, " oob %u", dp->d_oobsize);
		return;
	}
	if (pp != NULL) {
		sbuf_printf(sb, "%s<pagesize>%u</pagesize>\n",
		    indent, dp->d_pagesize);
		sbuf_printf(sb, "%s<oobsize>%u</oobsize>\n",
		    indent, dp->d_oobsize);
		sbuf_printf(sb, "%s<blocksize>%u</blocksize>\n",
		    indent, dp->d_maxsize);
	}
}

static void
g_gnand_create(void *arg, int flag)
{
	struct g_geom *gp;
	struct g_provider *pp;
	struct gnand *dp;

	if (flag == EV_CANCEL)
		return;
	g_topology_assert();
	dp = arg;
	gp = g_new_geomf(&g_gnand_class, "%s%d", dp->d_name, dp->d_unit);
	gp->softc = dp;
	pp = g_new_providerf(gp, "%s", gp->name);
	pp->mediasize = dp->d_mediasize;
	pp->sectorsize = dp->d_sectorsize;
	if (dp->d_flags & DISKFLAG_CANDELETE)
		pp->flags |= G_PF_CANDELETE;
	pp->stripeoffset = dp->d_stripeoffset;
	pp->stripesize = dp->d_stripesize;
	if (bootverbose)
		printf("GEOM: new gnand %s\n", gp->name);
	dp->d_geom = gp;
	g_error_provider(pp, 0);
}

static void
g_gnand_destroy(void *ptr, int flag)
{
	struct gnand *dp;
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
 * We only allow [a-zA-Z0-9-_@#%.:] characters, the rest is converted to 'x<HH>'.
 */
static void
g_gnand_ident_adjust(char *ident, size_t size)
{
	char newid[DISK_IDENT_SIZE], tmp[4];
	size_t len;
	char *p;

	bzero(newid, sizeof(newid));
	len = 0;
	for (p = ident; *p != '\0' && len < sizeof(newid) - 1; p++) {
		switch (*p) {
		default:
			if ((*p < 'a' || *p > 'z') &&
			    (*p < 'A' || *p > 'Z') &&
			    (*p < '0' || *p > '9')) {
				snprintf(tmp, sizeof(tmp), "x%02hhx", *p);
				strlcat(newid, tmp, sizeof(newid));
				len += 3;
				break;
			}
			/* FALLTHROUGH */
		case '-':
		case '_':
		case '@':
		case '#':
		case '%':
		case '.':
		case ':':
			newid[len++] = *p;
			break;
		}
	}
	bzero(ident, size);
	strlcpy(ident, newid, size);
}

struct gnand *
gnand_alloc()
{
	struct gnand *dp;

	dp = g_malloc(sizeof *dp, M_WAITOK | M_ZERO);
	return (dp);
}

void
gnand_create(struct gnand *dp)
{

	KASSERT(dp->d_strategy != NULL, ("gnand_create need d_strategy"));
	KASSERT(dp->d_name != NULL, ("gnand_create need d_name"));
	KASSERT(*dp->d_name != 0, ("gnand_create need d_name"));
	KASSERT(strlen(dp->d_name) < SPECNAMELEN - 4, ("gnand name too long"));
	if (dp->d_devstat == NULL)
		dp->d_devstat = devstat_new_entry(dp->d_name, dp->d_unit,
		    dp->d_sectorsize, DEVSTAT_ALL_SUPPORTED,
		    DEVSTAT_TYPE_DIRECT, DEVSTAT_PRIORITY_MAX);
	dp->d_geom = NULL;
	g_gnand_ident_adjust(dp->d_ident, sizeof(dp->d_ident));
	g_post_event(g_gnand_create, dp, M_WAITOK, dp, NULL);
}

void
gnand_destroy(struct gnand *dp)
{

	g_cancel_event(dp);
	dp->d_destroyed = 1;
	if (dp->d_devstat != NULL)
		devstat_remove_entry(dp->d_devstat);
	g_post_event(g_gnand_destroy, dp, M_WAITOK, NULL);
}

void
gnand_gone(struct gnand *dp)
{
	struct g_geom *gp;
	struct g_provider *pp;

	gp = dp->d_geom;
	if (gp != NULL)
		LIST_FOREACH(pp, &gp->provider, provider)
			g_wither_provider(pp, ENXIO);
}

static void
g_kern_gnands(void *p, int flag __unused)
{
	struct sbuf *sb;
	struct g_geom *gp;
	char *sp;

	sb = p;
	sp = "";
	g_topology_assert();
	LIST_FOREACH(gp, &g_gnand_class.geom, geom) {
		sbuf_printf(sb, "%s%s", sp, gp->name);
		sp = " ";
	}
	sbuf_finish(sb);
}

static int
sysctl_gnands(SYSCTL_HANDLER_ARGS)
{
	int error;
	struct sbuf *sb;

	sb = sbuf_new_auto();
	g_waitfor_event(g_kern_gnands, sb, M_WAITOK, NULL);
	error = SYSCTL_OUT(req, sbuf_data(sb), sbuf_len(sb) + 1);
	sbuf_delete(sb);
	return (error);
}

SYSCTL_PROC(_kern, OID_AUTO, gnands, CTLTYPE_STRING | CTLFLAG_RD |
    CTLFLAG_MPSAFE, 0, 0, sysctl_gnands, "A", "names of available gnands");
