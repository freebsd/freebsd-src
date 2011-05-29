/*-
 * Copyright (c) 2004-2006 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bio.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/eventhandler.h>
#include <vm/uma.h>
#include <geom/geom.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/sched.h>
#include <geom/mirror/g_mirror.h>

FEATURE(geom_mirror, "GEOM mirroring support");

static MALLOC_DEFINE(M_MIRROR, "mirror_data", "GEOM_MIRROR Data");

SYSCTL_DECL(_kern_geom);
SYSCTL_NODE(_kern_geom, OID_AUTO, mirror, CTLFLAG_RW, 0, "GEOM_MIRROR stuff");
u_int g_mirror_debug = 0;
TUNABLE_INT("kern.geom.mirror.debug", &g_mirror_debug);
SYSCTL_UINT(_kern_geom_mirror, OID_AUTO, debug, CTLFLAG_RW, &g_mirror_debug, 0,
    "Debug level");
static u_int g_mirror_timeout = 4;
TUNABLE_INT("kern.geom.mirror.timeout", &g_mirror_timeout);
SYSCTL_UINT(_kern_geom_mirror, OID_AUTO, timeout, CTLFLAG_RW, &g_mirror_timeout,
    0, "Time to wait on all mirror components");
static u_int g_mirror_idletime = 5;
TUNABLE_INT("kern.geom.mirror.idletime", &g_mirror_idletime);
SYSCTL_UINT(_kern_geom_mirror, OID_AUTO, idletime, CTLFLAG_RW,
    &g_mirror_idletime, 0, "Mark components as clean when idling");
static u_int g_mirror_disconnect_on_failure = 1;
TUNABLE_INT("kern.geom.mirror.disconnect_on_failure",
    &g_mirror_disconnect_on_failure);
SYSCTL_UINT(_kern_geom_mirror, OID_AUTO, disconnect_on_failure, CTLFLAG_RW,
    &g_mirror_disconnect_on_failure, 0, "Disconnect component on I/O failure.");
static u_int g_mirror_syncreqs = 2;
TUNABLE_INT("kern.geom.mirror.sync_requests", &g_mirror_syncreqs);
SYSCTL_UINT(_kern_geom_mirror, OID_AUTO, sync_requests, CTLFLAG_RDTUN,
    &g_mirror_syncreqs, 0, "Parallel synchronization I/O requests.");

#define	MSLEEP(ident, mtx, priority, wmesg, timeout)	do {		\
	G_MIRROR_DEBUG(4, "%s: Sleeping %p.", __func__, (ident));	\
	msleep((ident), (mtx), (priority), (wmesg), (timeout));		\
	G_MIRROR_DEBUG(4, "%s: Woken up %p.", __func__, (ident));	\
} while (0)

static eventhandler_tag g_mirror_pre_sync = NULL;

static int g_mirror_destroy_geom(struct gctl_req *req, struct g_class *mp,
    struct g_geom *gp);
static g_taste_t g_mirror_taste;
static void g_mirror_init(struct g_class *mp);
static void g_mirror_fini(struct g_class *mp);

struct g_class g_mirror_class = {
	.name = G_MIRROR_CLASS_NAME,
	.version = G_VERSION,
	.ctlreq = g_mirror_config,
	.taste = g_mirror_taste,
	.destroy_geom = g_mirror_destroy_geom,
	.init = g_mirror_init,
	.fini = g_mirror_fini
};


static void g_mirror_destroy_provider(struct g_mirror_softc *sc);
static int g_mirror_update_disk(struct g_mirror_disk *disk, u_int state);
static void g_mirror_update_device(struct g_mirror_softc *sc, boolean_t force);
static void g_mirror_dumpconf(struct sbuf *sb, const char *indent,
    struct g_geom *gp, struct g_consumer *cp, struct g_provider *pp);
static void g_mirror_sync_stop(struct g_mirror_disk *disk, int type);
static void g_mirror_register_request(struct bio *bp);
static void g_mirror_sync_release(struct g_mirror_softc *sc);


static const char *
g_mirror_disk_state2str(int state)
{

	switch (state) {
	case G_MIRROR_DISK_STATE_NONE:
		return ("NONE");
	case G_MIRROR_DISK_STATE_NEW:
		return ("NEW");
	case G_MIRROR_DISK_STATE_ACTIVE:
		return ("ACTIVE");
	case G_MIRROR_DISK_STATE_STALE:
		return ("STALE");
	case G_MIRROR_DISK_STATE_SYNCHRONIZING:
		return ("SYNCHRONIZING");
	case G_MIRROR_DISK_STATE_DISCONNECTED:
		return ("DISCONNECTED");
	case G_MIRROR_DISK_STATE_DESTROY:
		return ("DESTROY");
	default:
		return ("INVALID");
	}
}

static const char *
g_mirror_device_state2str(int state)
{

	switch (state) {
	case G_MIRROR_DEVICE_STATE_STARTING:
		return ("STARTING");
	case G_MIRROR_DEVICE_STATE_RUNNING:
		return ("RUNNING");
	default:
		return ("INVALID");
	}
}

static const char *
g_mirror_get_diskname(struct g_mirror_disk *disk)
{

	if (disk->d_consumer == NULL || disk->d_consumer->provider == NULL)
		return ("[unknown]");
	return (disk->d_name);
}

/*
 * --- Events handling functions ---
 * Events in geom_mirror are used to maintain disks and device status
 * from one thread to simplify locking.
 */
static void
g_mirror_event_free(struct g_mirror_event *ep)
{

	free(ep, M_MIRROR);
}

int
g_mirror_event_send(void *arg, int state, int flags)
{
	struct g_mirror_softc *sc;
	struct g_mirror_disk *disk;
	struct g_mirror_event *ep;
	int error;

	ep = malloc(sizeof(*ep), M_MIRROR, M_WAITOK);
	G_MIRROR_DEBUG(4, "%s: Sending event %p.", __func__, ep);
	if ((flags & G_MIRROR_EVENT_DEVICE) != 0) {
		disk = NULL;
		sc = arg;
	} else {
		disk = arg;
		sc = disk->d_softc;
	}
	ep->e_disk = disk;
	ep->e_state = state;
	ep->e_flags = flags;
	ep->e_error = 0;
	mtx_lock(&sc->sc_events_mtx);
	TAILQ_INSERT_TAIL(&sc->sc_events, ep, e_next);
	mtx_unlock(&sc->sc_events_mtx);
	G_MIRROR_DEBUG(4, "%s: Waking up %p.", __func__, sc);
	mtx_lock(&sc->sc_queue_mtx);
	wakeup(sc);
	mtx_unlock(&sc->sc_queue_mtx);
	if ((flags & G_MIRROR_EVENT_DONTWAIT) != 0)
		return (0);
	sx_assert(&sc->sc_lock, SX_XLOCKED);
	G_MIRROR_DEBUG(4, "%s: Sleeping %p.", __func__, ep);
	sx_xunlock(&sc->sc_lock);
	while ((ep->e_flags & G_MIRROR_EVENT_DONE) == 0) {
		mtx_lock(&sc->sc_events_mtx);
		MSLEEP(ep, &sc->sc_events_mtx, PRIBIO | PDROP, "m:event",
		    hz * 5);
	}
	error = ep->e_error;
	g_mirror_event_free(ep);
	sx_xlock(&sc->sc_lock);
	return (error);
}

static struct g_mirror_event *
g_mirror_event_get(struct g_mirror_softc *sc)
{
	struct g_mirror_event *ep;

	mtx_lock(&sc->sc_events_mtx);
	ep = TAILQ_FIRST(&sc->sc_events);
	mtx_unlock(&sc->sc_events_mtx);
	return (ep);
}

static void
g_mirror_event_remove(struct g_mirror_softc *sc, struct g_mirror_event *ep)
{

	mtx_lock(&sc->sc_events_mtx);
	TAILQ_REMOVE(&sc->sc_events, ep, e_next);
	mtx_unlock(&sc->sc_events_mtx);
}

static void
g_mirror_event_cancel(struct g_mirror_disk *disk)
{
	struct g_mirror_softc *sc;
	struct g_mirror_event *ep, *tmpep;

	sc = disk->d_softc;
	sx_assert(&sc->sc_lock, SX_XLOCKED);

	mtx_lock(&sc->sc_events_mtx);
	TAILQ_FOREACH_SAFE(ep, &sc->sc_events, e_next, tmpep) {
		if ((ep->e_flags & G_MIRROR_EVENT_DEVICE) != 0)
			continue;
		if (ep->e_disk != disk)
			continue;
		TAILQ_REMOVE(&sc->sc_events, ep, e_next);
		if ((ep->e_flags & G_MIRROR_EVENT_DONTWAIT) != 0)
			g_mirror_event_free(ep);
		else {
			ep->e_error = ECANCELED;
			wakeup(ep);
		}
	}
	mtx_unlock(&sc->sc_events_mtx);
}

/*
 * Return the number of disks in given state.
 * If state is equal to -1, count all connected disks.
 */
u_int
g_mirror_ndisks(struct g_mirror_softc *sc, int state)
{
	struct g_mirror_disk *disk;
	u_int n = 0;

	sx_assert(&sc->sc_lock, SX_LOCKED);

	LIST_FOREACH(disk, &sc->sc_disks, d_next) {
		if (state == -1 || disk->d_state == state)
			n++;
	}
	return (n);
}

/*
 * Find a disk in mirror by its disk ID.
 */
static struct g_mirror_disk *
g_mirror_id2disk(struct g_mirror_softc *sc, uint32_t id)
{
	struct g_mirror_disk *disk;

	sx_assert(&sc->sc_lock, SX_XLOCKED);

	LIST_FOREACH(disk, &sc->sc_disks, d_next) {
		if (disk->d_id == id)
			return (disk);
	}
	return (NULL);
}

static u_int
g_mirror_nrequests(struct g_mirror_softc *sc, struct g_consumer *cp)
{
	struct bio *bp;
	u_int nreqs = 0;

	mtx_lock(&sc->sc_queue_mtx);
	TAILQ_FOREACH(bp, &sc->sc_queue.queue, bio_queue) {
		if (bp->bio_from == cp)
			nreqs++;
	}
	mtx_unlock(&sc->sc_queue_mtx);
	return (nreqs);
}

static int
g_mirror_is_busy(struct g_mirror_softc *sc, struct g_consumer *cp)
{

	if (cp->index > 0) {
		G_MIRROR_DEBUG(2,
		    "I/O requests for %s exist, can't destroy it now.",
		    cp->provider->name);
		return (1);
	}
	if (g_mirror_nrequests(sc, cp) > 0) {
		G_MIRROR_DEBUG(2,
		    "I/O requests for %s in queue, can't destroy it now.",
		    cp->provider->name);
		return (1);
	}
	return (0);
}

static void
g_mirror_destroy_consumer(void *arg, int flags __unused)
{
	struct g_consumer *cp;

	g_topology_assert();

	cp = arg;
	G_MIRROR_DEBUG(1, "Consumer %s destroyed.", cp->provider->name);
	g_detach(cp);
	g_destroy_consumer(cp);
}

static void
g_mirror_kill_consumer(struct g_mirror_softc *sc, struct g_consumer *cp)
{
	struct g_provider *pp;
	int retaste_wait;

	g_topology_assert();

	cp->private = NULL;
	if (g_mirror_is_busy(sc, cp))
		return;
	pp = cp->provider;
	retaste_wait = 0;
	if (cp->acw == 1) {
		if ((pp->geom->flags & G_GEOM_WITHER) == 0)
			retaste_wait = 1;
	}
	G_MIRROR_DEBUG(2, "Access %s r%dw%de%d = %d", pp->name, -cp->acr,
	    -cp->acw, -cp->ace, 0);
	if (cp->acr > 0 || cp->acw > 0 || cp->ace > 0)
		g_access(cp, -cp->acr, -cp->acw, -cp->ace);
	if (retaste_wait) {
		/*
		 * After retaste event was send (inside g_access()), we can send
		 * event to detach and destroy consumer.
		 * A class, which has consumer to the given provider connected
		 * will not receive retaste event for the provider.
		 * This is the way how I ignore retaste events when I close
		 * consumers opened for write: I detach and destroy consumer
		 * after retaste event is sent.
		 */
		g_post_event(g_mirror_destroy_consumer, cp, M_WAITOK, NULL);
		return;
	}
	G_MIRROR_DEBUG(1, "Consumer %s destroyed.", pp->name);
	g_detach(cp);
	g_destroy_consumer(cp);
}

static int
g_mirror_connect_disk(struct g_mirror_disk *disk, struct g_provider *pp)
{
	struct g_consumer *cp;
	int error;

	g_topology_assert_not();
	KASSERT(disk->d_consumer == NULL,
	    ("Disk already connected (device %s).", disk->d_softc->sc_name));

	g_topology_lock();
	cp = g_new_consumer(disk->d_softc->sc_geom);
	error = g_attach(cp, pp);
	if (error != 0) {
		g_destroy_consumer(cp);
		g_topology_unlock();
		return (error);
	}
	error = g_access(cp, 1, 1, 1);
	if (error != 0) {
		g_detach(cp);
		g_destroy_consumer(cp);
		g_topology_unlock();
		G_MIRROR_DEBUG(0, "Cannot open consumer %s (error=%d).",
		    pp->name, error);
		return (error);
	}
	g_topology_unlock();
	disk->d_consumer = cp;
	disk->d_consumer->private = disk;
	disk->d_consumer->index = 0;

	G_MIRROR_DEBUG(2, "Disk %s connected.", g_mirror_get_diskname(disk));
	return (0);
}

static void
g_mirror_disconnect_consumer(struct g_mirror_softc *sc, struct g_consumer *cp)
{

	g_topology_assert();

	if (cp == NULL)
		return;
	if (cp->provider != NULL)
		g_mirror_kill_consumer(sc, cp);
	else
		g_destroy_consumer(cp);
}

/*
 * Initialize disk. This means allocate memory, create consumer, attach it
 * to the provider and open access (r1w1e1) to it.
 */
static struct g_mirror_disk *
g_mirror_init_disk(struct g_mirror_softc *sc, struct g_provider *pp,
    struct g_mirror_metadata *md, int *errorp)
{
	struct g_mirror_disk *disk;
	int error;

	disk = malloc(sizeof(*disk), M_MIRROR, M_NOWAIT | M_ZERO);
	if (disk == NULL) {
		error = ENOMEM;
		goto fail;
	}
	disk->d_softc = sc;
	error = g_mirror_connect_disk(disk, pp);
	if (error != 0)
		goto fail;
	disk->d_id = md->md_did;
	disk->d_state = G_MIRROR_DISK_STATE_NONE;
	disk->d_priority = md->md_priority;
	disk->d_flags = md->md_dflags;
	if (md->md_provider[0] != '\0')
		disk->d_flags |= G_MIRROR_DISK_FLAG_HARDCODED;
	disk->d_sync.ds_consumer = NULL;
	disk->d_sync.ds_offset = md->md_sync_offset;
	disk->d_sync.ds_offset_done = md->md_sync_offset;
	disk->d_genid = md->md_genid;
	disk->d_sync.ds_syncid = md->md_syncid;
	if (errorp != NULL)
		*errorp = 0;
	return (disk);
fail:
	if (errorp != NULL)
		*errorp = error;
	if (disk != NULL)
		free(disk, M_MIRROR);
	return (NULL);
}

static void
g_mirror_destroy_disk(struct g_mirror_disk *disk)
{
	struct g_mirror_softc *sc;

	g_topology_assert_not();
	sc = disk->d_softc;
	sx_assert(&sc->sc_lock, SX_XLOCKED);

	LIST_REMOVE(disk, d_next);
	g_mirror_event_cancel(disk);
	if (sc->sc_hint == disk)
		sc->sc_hint = NULL;
	switch (disk->d_state) {
	case G_MIRROR_DISK_STATE_SYNCHRONIZING:
		g_mirror_sync_stop(disk, 1);
		/* FALLTHROUGH */
	case G_MIRROR_DISK_STATE_NEW:
	case G_MIRROR_DISK_STATE_STALE:
	case G_MIRROR_DISK_STATE_ACTIVE:
		g_topology_lock();
		g_mirror_disconnect_consumer(sc, disk->d_consumer);
		g_topology_unlock();
		free(disk, M_MIRROR);
		break;
	default:
		KASSERT(0 == 1, ("Wrong disk state (%s, %s).",
		    g_mirror_get_diskname(disk),
		    g_mirror_disk_state2str(disk->d_state)));
	}
}

static void
g_mirror_destroy_device(struct g_mirror_softc *sc)
{
	struct g_mirror_disk *disk;
	struct g_mirror_event *ep;
	struct g_geom *gp;
	struct g_consumer *cp, *tmpcp;

	g_topology_assert_not();
	sx_assert(&sc->sc_lock, SX_XLOCKED);

	gp = sc->sc_geom;
	if (sc->sc_provider != NULL)
		g_mirror_destroy_provider(sc);
	for (disk = LIST_FIRST(&sc->sc_disks); disk != NULL;
	    disk = LIST_FIRST(&sc->sc_disks)) {
		disk->d_flags &= ~G_MIRROR_DISK_FLAG_DIRTY;
		g_mirror_update_metadata(disk);
		g_mirror_destroy_disk(disk);
	}
	while ((ep = g_mirror_event_get(sc)) != NULL) {
		g_mirror_event_remove(sc, ep);
		if ((ep->e_flags & G_MIRROR_EVENT_DONTWAIT) != 0)
			g_mirror_event_free(ep);
		else {
			ep->e_error = ECANCELED;
			ep->e_flags |= G_MIRROR_EVENT_DONE;
			G_MIRROR_DEBUG(4, "%s: Waking up %p.", __func__, ep);
			mtx_lock(&sc->sc_events_mtx);
			wakeup(ep);
			mtx_unlock(&sc->sc_events_mtx);
		}
	}
	callout_drain(&sc->sc_callout);

	g_topology_lock();
	LIST_FOREACH_SAFE(cp, &sc->sc_sync.ds_geom->consumer, consumer, tmpcp) {
		g_mirror_disconnect_consumer(sc, cp);
	}
	g_wither_geom(sc->sc_sync.ds_geom, ENXIO);
	G_MIRROR_DEBUG(0, "Device %s destroyed.", gp->name);
	g_wither_geom(gp, ENXIO);
	g_topology_unlock();
	mtx_destroy(&sc->sc_queue_mtx);
	mtx_destroy(&sc->sc_events_mtx);
	sx_xunlock(&sc->sc_lock);
	sx_destroy(&sc->sc_lock);
}

static void
g_mirror_orphan(struct g_consumer *cp)
{
	struct g_mirror_disk *disk;

	g_topology_assert();

	disk = cp->private;
	if (disk == NULL)
		return;
	disk->d_softc->sc_bump_id |= G_MIRROR_BUMP_SYNCID;
	g_mirror_event_send(disk, G_MIRROR_DISK_STATE_DISCONNECTED,
	    G_MIRROR_EVENT_DONTWAIT);
}

/*
 * Function should return the next active disk on the list.
 * It is possible that it will be the same disk as given.
 * If there are no active disks on list, NULL is returned.
 */
static __inline struct g_mirror_disk *
g_mirror_find_next(struct g_mirror_softc *sc, struct g_mirror_disk *disk)
{
	struct g_mirror_disk *dp;

	for (dp = LIST_NEXT(disk, d_next); dp != disk;
	    dp = LIST_NEXT(dp, d_next)) {
		if (dp == NULL)
			dp = LIST_FIRST(&sc->sc_disks);
		if (dp->d_state == G_MIRROR_DISK_STATE_ACTIVE)
			break;
	}
	if (dp->d_state != G_MIRROR_DISK_STATE_ACTIVE)
		return (NULL);
	return (dp);
}

static struct g_mirror_disk *
g_mirror_get_disk(struct g_mirror_softc *sc)
{
	struct g_mirror_disk *disk;

	if (sc->sc_hint == NULL) {
		sc->sc_hint = LIST_FIRST(&sc->sc_disks);
		if (sc->sc_hint == NULL)
			return (NULL);
	}
	disk = sc->sc_hint;
	if (disk->d_state != G_MIRROR_DISK_STATE_ACTIVE) {
		disk = g_mirror_find_next(sc, disk);
		if (disk == NULL)
			return (NULL);
	}
	sc->sc_hint = g_mirror_find_next(sc, disk);
	return (disk);
}

static int
g_mirror_write_metadata(struct g_mirror_disk *disk,
    struct g_mirror_metadata *md)
{
	struct g_mirror_softc *sc;
	struct g_consumer *cp;
	off_t offset, length;
	u_char *sector;
	int error = 0;

	g_topology_assert_not();
	sc = disk->d_softc;
	sx_assert(&sc->sc_lock, SX_LOCKED);

	cp = disk->d_consumer;
	KASSERT(cp != NULL, ("NULL consumer (%s).", sc->sc_name));
	KASSERT(cp->provider != NULL, ("NULL provider (%s).", sc->sc_name));
	KASSERT(cp->acr >= 1 && cp->acw >= 1 && cp->ace >= 1,
	    ("Consumer %s closed? (r%dw%de%d).", cp->provider->name, cp->acr,
	    cp->acw, cp->ace));
	length = cp->provider->sectorsize;
	offset = cp->provider->mediasize - length;
	sector = malloc((size_t)length, M_MIRROR, M_WAITOK | M_ZERO);
	if (md != NULL)
		mirror_metadata_encode(md, sector);
	error = g_write_data(cp, offset, sector, length);
	free(sector, M_MIRROR);
	if (error != 0) {
		if ((disk->d_flags & G_MIRROR_DISK_FLAG_BROKEN) == 0) {
			disk->d_flags |= G_MIRROR_DISK_FLAG_BROKEN;
			G_MIRROR_DEBUG(0, "Cannot write metadata on %s "
			    "(device=%s, error=%d).",
			    g_mirror_get_diskname(disk), sc->sc_name, error);
		} else {
			G_MIRROR_DEBUG(1, "Cannot write metadata on %s "
			    "(device=%s, error=%d).",
			    g_mirror_get_diskname(disk), sc->sc_name, error);
		}
		if (g_mirror_disconnect_on_failure &&
		    g_mirror_ndisks(sc, G_MIRROR_DISK_STATE_ACTIVE) > 1) {
			sc->sc_bump_id |= G_MIRROR_BUMP_GENID;
			g_mirror_event_send(disk,
			    G_MIRROR_DISK_STATE_DISCONNECTED,
			    G_MIRROR_EVENT_DONTWAIT);
		}
	}
	return (error);
}

static int
g_mirror_clear_metadata(struct g_mirror_disk *disk)
{
	int error;

	g_topology_assert_not();
	sx_assert(&disk->d_softc->sc_lock, SX_LOCKED);

	error = g_mirror_write_metadata(disk, NULL);
	if (error == 0) {
		G_MIRROR_DEBUG(2, "Metadata on %s cleared.",
		    g_mirror_get_diskname(disk));
	} else {
		G_MIRROR_DEBUG(0,
		    "Cannot clear metadata on disk %s (error=%d).",
		    g_mirror_get_diskname(disk), error);
	}
	return (error);
}

void
g_mirror_fill_metadata(struct g_mirror_softc *sc, struct g_mirror_disk *disk,
    struct g_mirror_metadata *md)
{

	strlcpy(md->md_magic, G_MIRROR_MAGIC, sizeof(md->md_magic));
	md->md_version = G_MIRROR_VERSION;
	strlcpy(md->md_name, sc->sc_name, sizeof(md->md_name));
	md->md_mid = sc->sc_id;
	md->md_all = sc->sc_ndisks;
	md->md_slice = sc->sc_slice;
	md->md_balance = sc->sc_balance;
	md->md_genid = sc->sc_genid;
	md->md_mediasize = sc->sc_mediasize;
	md->md_sectorsize = sc->sc_sectorsize;
	md->md_mflags = (sc->sc_flags & G_MIRROR_DEVICE_FLAG_MASK);
	bzero(md->md_provider, sizeof(md->md_provider));
	if (disk == NULL) {
		md->md_did = arc4random();
		md->md_priority = 0;
		md->md_syncid = 0;
		md->md_dflags = 0;
		md->md_sync_offset = 0;
		md->md_provsize = 0;
	} else {
		md->md_did = disk->d_id;
		md->md_priority = disk->d_priority;
		md->md_syncid = disk->d_sync.ds_syncid;
		md->md_dflags = (disk->d_flags & G_MIRROR_DISK_FLAG_MASK);
		if (disk->d_state == G_MIRROR_DISK_STATE_SYNCHRONIZING)
			md->md_sync_offset = disk->d_sync.ds_offset_done;
		else
			md->md_sync_offset = 0;
		if ((disk->d_flags & G_MIRROR_DISK_FLAG_HARDCODED) != 0) {
			strlcpy(md->md_provider,
			    disk->d_consumer->provider->name,
			    sizeof(md->md_provider));
		}
		md->md_provsize = disk->d_consumer->provider->mediasize;
	}
}

void
g_mirror_update_metadata(struct g_mirror_disk *disk)
{
	struct g_mirror_softc *sc;
	struct g_mirror_metadata md;
	int error;

	g_topology_assert_not();
	sc = disk->d_softc;
	sx_assert(&sc->sc_lock, SX_LOCKED);

	g_mirror_fill_metadata(sc, disk, &md);
	error = g_mirror_write_metadata(disk, &md);
	if (error == 0) {
		G_MIRROR_DEBUG(2, "Metadata on %s updated.",
		    g_mirror_get_diskname(disk));
	} else {
		G_MIRROR_DEBUG(0,
		    "Cannot update metadata on disk %s (error=%d).",
		    g_mirror_get_diskname(disk), error);
	}
}

static void
g_mirror_bump_syncid(struct g_mirror_softc *sc)
{
	struct g_mirror_disk *disk;

	g_topology_assert_not();
	sx_assert(&sc->sc_lock, SX_XLOCKED);
	KASSERT(g_mirror_ndisks(sc, G_MIRROR_DISK_STATE_ACTIVE) > 0,
	    ("%s called with no active disks (device=%s).", __func__,
	    sc->sc_name));

	sc->sc_syncid++;
	G_MIRROR_DEBUG(1, "Device %s: syncid bumped to %u.", sc->sc_name,
	    sc->sc_syncid);
	LIST_FOREACH(disk, &sc->sc_disks, d_next) {
		if (disk->d_state == G_MIRROR_DISK_STATE_ACTIVE ||
		    disk->d_state == G_MIRROR_DISK_STATE_SYNCHRONIZING) {
			disk->d_sync.ds_syncid = sc->sc_syncid;
			g_mirror_update_metadata(disk);
		}
	}
}

static void
g_mirror_bump_genid(struct g_mirror_softc *sc)
{
	struct g_mirror_disk *disk;

	g_topology_assert_not();
	sx_assert(&sc->sc_lock, SX_XLOCKED);
	KASSERT(g_mirror_ndisks(sc, G_MIRROR_DISK_STATE_ACTIVE) > 0,
	    ("%s called with no active disks (device=%s).", __func__,
	    sc->sc_name));

	sc->sc_genid++;
	G_MIRROR_DEBUG(1, "Device %s: genid bumped to %u.", sc->sc_name,
	    sc->sc_genid);
	LIST_FOREACH(disk, &sc->sc_disks, d_next) {
		if (disk->d_state == G_MIRROR_DISK_STATE_ACTIVE ||
		    disk->d_state == G_MIRROR_DISK_STATE_SYNCHRONIZING) {
			disk->d_genid = sc->sc_genid;
			g_mirror_update_metadata(disk);
		}
	}
}

static int
g_mirror_idle(struct g_mirror_softc *sc, int acw)
{
	struct g_mirror_disk *disk;
	int timeout;

	g_topology_assert_not();
	sx_assert(&sc->sc_lock, SX_XLOCKED);

	if (sc->sc_provider == NULL)
		return (0);
	if ((sc->sc_flags & G_MIRROR_DEVICE_FLAG_NOFAILSYNC) != 0)
		return (0);
	if (sc->sc_idle)
		return (0);
	if (sc->sc_writes > 0)
		return (0);
	if (acw > 0 || (acw == -1 && sc->sc_provider->acw > 0)) {
		timeout = g_mirror_idletime - (time_uptime - sc->sc_last_write);
		if (timeout > 0)
			return (timeout);
	}
	sc->sc_idle = 1;
	LIST_FOREACH(disk, &sc->sc_disks, d_next) {
		if (disk->d_state != G_MIRROR_DISK_STATE_ACTIVE)
			continue;
		G_MIRROR_DEBUG(1, "Disk %s (device %s) marked as clean.",
		    g_mirror_get_diskname(disk), sc->sc_name);
		disk->d_flags &= ~G_MIRROR_DISK_FLAG_DIRTY;
		g_mirror_update_metadata(disk);
	}
	return (0);
}

static void
g_mirror_unidle(struct g_mirror_softc *sc)
{
	struct g_mirror_disk *disk;

	g_topology_assert_not();
	sx_assert(&sc->sc_lock, SX_XLOCKED);

	if ((sc->sc_flags & G_MIRROR_DEVICE_FLAG_NOFAILSYNC) != 0)
		return;
	sc->sc_idle = 0;
	sc->sc_last_write = time_uptime;
	LIST_FOREACH(disk, &sc->sc_disks, d_next) {
		if (disk->d_state != G_MIRROR_DISK_STATE_ACTIVE)
			continue;
		G_MIRROR_DEBUG(1, "Disk %s (device %s) marked as dirty.",
		    g_mirror_get_diskname(disk), sc->sc_name);
		disk->d_flags |= G_MIRROR_DISK_FLAG_DIRTY;
		g_mirror_update_metadata(disk);
	}
}

static void
g_mirror_done(struct bio *bp)
{
	struct g_mirror_softc *sc;

	sc = bp->bio_from->geom->softc;
	bp->bio_cflags = G_MIRROR_BIO_FLAG_REGULAR;
	mtx_lock(&sc->sc_queue_mtx);
	bioq_disksort(&sc->sc_queue, bp);
	mtx_unlock(&sc->sc_queue_mtx);
	wakeup(sc);
}

static void
g_mirror_regular_request(struct bio *bp)
{
	struct g_mirror_softc *sc;
	struct g_mirror_disk *disk;
	struct bio *pbp;

	g_topology_assert_not();

	pbp = bp->bio_parent;
	sc = pbp->bio_to->geom->softc;
	bp->bio_from->index--;
	if (bp->bio_cmd == BIO_WRITE)
		sc->sc_writes--;
	disk = bp->bio_from->private;
	if (disk == NULL) {
		g_topology_lock();
		g_mirror_kill_consumer(sc, bp->bio_from);
		g_topology_unlock();
	}

	pbp->bio_inbed++;
	KASSERT(pbp->bio_inbed <= pbp->bio_children,
	    ("bio_inbed (%u) is bigger than bio_children (%u).", pbp->bio_inbed,
	    pbp->bio_children));
	if (bp->bio_error == 0 && pbp->bio_error == 0) {
		G_MIRROR_LOGREQ(3, bp, "Request delivered.");
		g_destroy_bio(bp);
		if (pbp->bio_children == pbp->bio_inbed) {
			G_MIRROR_LOGREQ(3, pbp, "Request delivered.");
			pbp->bio_completed = pbp->bio_length;
			if (pbp->bio_cmd == BIO_WRITE) {
				bioq_remove(&sc->sc_inflight, pbp);
				/* Release delayed sync requests if possible. */
				g_mirror_sync_release(sc);
			}
			g_io_deliver(pbp, pbp->bio_error);
		}
		return;
	} else if (bp->bio_error != 0) {
		if (pbp->bio_error == 0)
			pbp->bio_error = bp->bio_error;
		if (disk != NULL) {
			if ((disk->d_flags & G_MIRROR_DISK_FLAG_BROKEN) == 0) {
				disk->d_flags |= G_MIRROR_DISK_FLAG_BROKEN;
				G_MIRROR_LOGREQ(0, bp,
				    "Request failed (error=%d).",
				    bp->bio_error);
			} else {
				G_MIRROR_LOGREQ(1, bp,
				    "Request failed (error=%d).",
				    bp->bio_error);
			}
			if (g_mirror_disconnect_on_failure &&
			    g_mirror_ndisks(sc, G_MIRROR_DISK_STATE_ACTIVE) > 1)
			{
				sc->sc_bump_id |= G_MIRROR_BUMP_GENID;
				g_mirror_event_send(disk,
				    G_MIRROR_DISK_STATE_DISCONNECTED,
				    G_MIRROR_EVENT_DONTWAIT);
			}
		}
		switch (pbp->bio_cmd) {
		case BIO_DELETE:
		case BIO_WRITE:
			pbp->bio_inbed--;
			pbp->bio_children--;
			break;
		}
	}
	g_destroy_bio(bp);

	switch (pbp->bio_cmd) {
	case BIO_READ:
		if (pbp->bio_inbed < pbp->bio_children)
			break;
		if (g_mirror_ndisks(sc, G_MIRROR_DISK_STATE_ACTIVE) == 1)
			g_io_deliver(pbp, pbp->bio_error);
		else {
			pbp->bio_error = 0;
			mtx_lock(&sc->sc_queue_mtx);
			bioq_disksort(&sc->sc_queue, pbp);
			mtx_unlock(&sc->sc_queue_mtx);
			G_MIRROR_DEBUG(4, "%s: Waking up %p.", __func__, sc);
			wakeup(sc);
		}
		break;
	case BIO_DELETE:
	case BIO_WRITE:
		if (pbp->bio_children == 0) {
			/*
			 * All requests failed.
			 */
		} else if (pbp->bio_inbed < pbp->bio_children) {
			/* Do nothing. */
			break;
		} else if (pbp->bio_children == pbp->bio_inbed) {
			/* Some requests succeeded. */
			pbp->bio_error = 0;
			pbp->bio_completed = pbp->bio_length;
		}
		bioq_remove(&sc->sc_inflight, pbp);
		/* Release delayed sync requests if possible. */
		g_mirror_sync_release(sc);
		g_io_deliver(pbp, pbp->bio_error);
		break;
	default:
		KASSERT(1 == 0, ("Invalid request: %u.", pbp->bio_cmd));
		break;
	}
}

static void
g_mirror_sync_done(struct bio *bp)
{
	struct g_mirror_softc *sc;

	G_MIRROR_LOGREQ(3, bp, "Synchronization request delivered.");
	sc = bp->bio_from->geom->softc;
	bp->bio_cflags = G_MIRROR_BIO_FLAG_SYNC;
	mtx_lock(&sc->sc_queue_mtx);
	bioq_disksort(&sc->sc_queue, bp);
	mtx_unlock(&sc->sc_queue_mtx);
	wakeup(sc);
}

static void
g_mirror_kernel_dump(struct bio *bp)
{
	struct g_mirror_softc *sc;
	struct g_mirror_disk *disk;
	struct bio *cbp;
	struct g_kerneldump *gkd;

	/*
	 * We configure dumping to the first component, because this component
	 * will be used for reading with 'prefer' balance algorithm.
	 * If the component with the higest priority is currently disconnected
	 * we will not be able to read the dump after the reboot if it will be
	 * connected and synchronized later. Can we do something better?
	 */
	sc = bp->bio_to->geom->softc;
	disk = LIST_FIRST(&sc->sc_disks);

	gkd = (struct g_kerneldump *)bp->bio_data;
	if (gkd->length > bp->bio_to->mediasize)
		gkd->length = bp->bio_to->mediasize;
	cbp = g_clone_bio(bp);
	if (cbp == NULL) {
		g_io_deliver(bp, ENOMEM);
		return;
	}
	cbp->bio_done = g_std_done;
	g_io_request(cbp, disk->d_consumer);
	G_MIRROR_DEBUG(1, "Kernel dump will go to %s.",
	    g_mirror_get_diskname(disk));
}

static void
g_mirror_flush(struct g_mirror_softc *sc, struct bio *bp)
{
	struct bio_queue_head queue;
	struct g_mirror_disk *disk;
	struct g_consumer *cp;
	struct bio *cbp;

	bioq_init(&queue);
	LIST_FOREACH(disk, &sc->sc_disks, d_next) {
		if (disk->d_state != G_MIRROR_DISK_STATE_ACTIVE)
			continue;
		cbp = g_clone_bio(bp);
		if (cbp == NULL) {
			for (cbp = bioq_first(&queue); cbp != NULL;
			    cbp = bioq_first(&queue)) {
				bioq_remove(&queue, cbp);
				g_destroy_bio(cbp);
			}
			if (bp->bio_error == 0)
				bp->bio_error = ENOMEM;
			g_io_deliver(bp, bp->bio_error);
			return;
		}
		bioq_insert_tail(&queue, cbp);
		cbp->bio_done = g_std_done;
		cbp->bio_caller1 = disk;
		cbp->bio_to = disk->d_consumer->provider;
	}
	for (cbp = bioq_first(&queue); cbp != NULL; cbp = bioq_first(&queue)) {
		bioq_remove(&queue, cbp);
		G_MIRROR_LOGREQ(3, cbp, "Sending request.");
		disk = cbp->bio_caller1;
		cbp->bio_caller1 = NULL;
		cp = disk->d_consumer;
		KASSERT(cp->acr >= 1 && cp->acw >= 1 && cp->ace >= 1,
		    ("Consumer %s not opened (r%dw%de%d).", cp->provider->name,
		    cp->acr, cp->acw, cp->ace));
		g_io_request(cbp, disk->d_consumer);
	}
}

static void
g_mirror_start(struct bio *bp)
{
	struct g_mirror_softc *sc;

	sc = bp->bio_to->geom->softc;
	/*
	 * If sc == NULL or there are no valid disks, provider's error
	 * should be set and g_mirror_start() should not be called at all.
	 */
	KASSERT(sc != NULL && sc->sc_state == G_MIRROR_DEVICE_STATE_RUNNING,
	    ("Provider's error should be set (error=%d)(mirror=%s).",
	    bp->bio_to->error, bp->bio_to->name));
	G_MIRROR_LOGREQ(3, bp, "Request received.");

	switch (bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_DELETE:
		break;
	case BIO_FLUSH:
		g_mirror_flush(sc, bp);
		return;
	case BIO_GETATTR:
		if (strcmp("GEOM::kerneldump", bp->bio_attribute) == 0) {
			g_mirror_kernel_dump(bp);
			return;
		}
		/* FALLTHROUGH */
	default:
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}
	mtx_lock(&sc->sc_queue_mtx);
	bioq_disksort(&sc->sc_queue, bp);
	mtx_unlock(&sc->sc_queue_mtx);
	G_MIRROR_DEBUG(4, "%s: Waking up %p.", __func__, sc);
	wakeup(sc);
}

/*
 * Return TRUE if the given request is colliding with a in-progress
 * synchronization request.
 */
static int
g_mirror_sync_collision(struct g_mirror_softc *sc, struct bio *bp)
{
	struct g_mirror_disk *disk;
	struct bio *sbp;
	off_t rstart, rend, sstart, send;
	int i;

	if (sc->sc_sync.ds_ndisks == 0)
		return (0);
	rstart = bp->bio_offset;
	rend = bp->bio_offset + bp->bio_length;
	LIST_FOREACH(disk, &sc->sc_disks, d_next) {
		if (disk->d_state != G_MIRROR_DISK_STATE_SYNCHRONIZING)
			continue;
		for (i = 0; i < g_mirror_syncreqs; i++) {
			sbp = disk->d_sync.ds_bios[i];
			if (sbp == NULL)
				continue;
			sstart = sbp->bio_offset;
			send = sbp->bio_offset + sbp->bio_length;
			if (rend > sstart && rstart < send)
				return (1);
		}
	}
	return (0);
}

/*
 * Return TRUE if the given sync request is colliding with a in-progress regular
 * request.
 */
static int
g_mirror_regular_collision(struct g_mirror_softc *sc, struct bio *sbp)
{
	off_t rstart, rend, sstart, send;
	struct bio *bp;

	if (sc->sc_sync.ds_ndisks == 0)
		return (0);
	sstart = sbp->bio_offset;
	send = sbp->bio_offset + sbp->bio_length;
	TAILQ_FOREACH(bp, &sc->sc_inflight.queue, bio_queue) {
		rstart = bp->bio_offset;
		rend = bp->bio_offset + bp->bio_length;
		if (rend > sstart && rstart < send)
			return (1);
	}
	return (0);
}

/*
 * Puts request onto delayed queue.
 */
static void
g_mirror_regular_delay(struct g_mirror_softc *sc, struct bio *bp)
{

	G_MIRROR_LOGREQ(2, bp, "Delaying request.");
	bioq_insert_head(&sc->sc_regular_delayed, bp);
}

/*
 * Puts synchronization request onto delayed queue.
 */
static void
g_mirror_sync_delay(struct g_mirror_softc *sc, struct bio *bp)
{

	G_MIRROR_LOGREQ(2, bp, "Delaying synchronization request.");
	bioq_insert_tail(&sc->sc_sync_delayed, bp);
}

/*
 * Releases delayed regular requests which don't collide anymore with sync
 * requests.
 */
static void
g_mirror_regular_release(struct g_mirror_softc *sc)
{
	struct bio *bp, *bp2;

	TAILQ_FOREACH_SAFE(bp, &sc->sc_regular_delayed.queue, bio_queue, bp2) {
		if (g_mirror_sync_collision(sc, bp))
			continue;
		bioq_remove(&sc->sc_regular_delayed, bp);
		G_MIRROR_LOGREQ(2, bp, "Releasing delayed request (%p).", bp);
		mtx_lock(&sc->sc_queue_mtx);
		bioq_insert_head(&sc->sc_queue, bp);
#if 0
		/*
		 * wakeup() is not needed, because this function is called from
		 * the worker thread.
		 */
		wakeup(&sc->sc_queue);
#endif
		mtx_unlock(&sc->sc_queue_mtx);
	}
}

/*
 * Releases delayed sync requests which don't collide anymore with regular
 * requests.
 */
static void
g_mirror_sync_release(struct g_mirror_softc *sc)
{
	struct bio *bp, *bp2;

	TAILQ_FOREACH_SAFE(bp, &sc->sc_sync_delayed.queue, bio_queue, bp2) {
		if (g_mirror_regular_collision(sc, bp))
			continue;
		bioq_remove(&sc->sc_sync_delayed, bp);
		G_MIRROR_LOGREQ(2, bp,
		    "Releasing delayed synchronization request.");
		g_io_request(bp, bp->bio_from);
	}
}

/*
 * Handle synchronization requests.
 * Every synchronization request is two-steps process: first, READ request is
 * send to active provider and then WRITE request (with read data) to the provider
 * beeing synchronized. When WRITE is finished, new synchronization request is
 * send.
 */
static void
g_mirror_sync_request(struct bio *bp)
{
	struct g_mirror_softc *sc;
	struct g_mirror_disk *disk;

	bp->bio_from->index--;
	sc = bp->bio_from->geom->softc;
	disk = bp->bio_from->private;
	if (disk == NULL) {
		sx_xunlock(&sc->sc_lock); /* Avoid recursion on sc_lock. */
		g_topology_lock();
		g_mirror_kill_consumer(sc, bp->bio_from);
		g_topology_unlock();
		free(bp->bio_data, M_MIRROR);
		g_destroy_bio(bp);
		sx_xlock(&sc->sc_lock);
		return;
	}

	/*
	 * Synchronization request.
	 */
	switch (bp->bio_cmd) {
	case BIO_READ:
	    {
		struct g_consumer *cp;

		if (bp->bio_error != 0) {
			G_MIRROR_LOGREQ(0, bp,
			    "Synchronization request failed (error=%d).",
			    bp->bio_error);
			g_destroy_bio(bp);
			return;
		}
		G_MIRROR_LOGREQ(3, bp,
		    "Synchronization request half-finished.");
		bp->bio_cmd = BIO_WRITE;
		bp->bio_cflags = 0;
		cp = disk->d_consumer;
		KASSERT(cp->acr >= 1 && cp->acw >= 1 && cp->ace >= 1,
		    ("Consumer %s not opened (r%dw%de%d).", cp->provider->name,
		    cp->acr, cp->acw, cp->ace));
		cp->index++;
		g_io_request(bp, cp);
		return;
	    }
	case BIO_WRITE:
	    {
		struct g_mirror_disk_sync *sync;
		off_t offset;
		void *data;
		int i;

		if (bp->bio_error != 0) {
			G_MIRROR_LOGREQ(0, bp,
			    "Synchronization request failed (error=%d).",
			    bp->bio_error);
			g_destroy_bio(bp);
			sc->sc_bump_id |= G_MIRROR_BUMP_GENID;
			g_mirror_event_send(disk,
			    G_MIRROR_DISK_STATE_DISCONNECTED,
			    G_MIRROR_EVENT_DONTWAIT);
			return;
		}
		G_MIRROR_LOGREQ(3, bp, "Synchronization request finished.");
		sync = &disk->d_sync;
		if (sync->ds_offset == sc->sc_mediasize ||
		    sync->ds_consumer == NULL ||
		    (sc->sc_flags & G_MIRROR_DEVICE_FLAG_DESTROY) != 0) {
			/* Don't send more synchronization requests. */
			sync->ds_inflight--;
			if (sync->ds_bios != NULL) {
				i = (int)(uintptr_t)bp->bio_caller1;
				sync->ds_bios[i] = NULL;
			}
			free(bp->bio_data, M_MIRROR);
			g_destroy_bio(bp);
			if (sync->ds_inflight > 0)
				return;
			if (sync->ds_consumer == NULL ||
			    (sc->sc_flags & G_MIRROR_DEVICE_FLAG_DESTROY) != 0) {
				return;
			}
			/* Disk up-to-date, activate it. */
			g_mirror_event_send(disk, G_MIRROR_DISK_STATE_ACTIVE,
			    G_MIRROR_EVENT_DONTWAIT);
			return;
		}

		/* Send next synchronization request. */
		data = bp->bio_data;
		bzero(bp, sizeof(*bp));
		bp->bio_cmd = BIO_READ;
		bp->bio_offset = sync->ds_offset;
		bp->bio_length = MIN(MAXPHYS, sc->sc_mediasize - bp->bio_offset);
		sync->ds_offset += bp->bio_length;
		bp->bio_done = g_mirror_sync_done;
		bp->bio_data = data;
		bp->bio_from = sync->ds_consumer;
		bp->bio_to = sc->sc_provider;
		G_MIRROR_LOGREQ(3, bp, "Sending synchronization request.");
		sync->ds_consumer->index++;
		/*
		 * Delay the request if it is colliding with a regular request.
		 */
		if (g_mirror_regular_collision(sc, bp))
			g_mirror_sync_delay(sc, bp);
		else
			g_io_request(bp, sync->ds_consumer);

		/* Release delayed requests if possible. */
		g_mirror_regular_release(sc);

		/* Find the smallest offset */
		offset = sc->sc_mediasize;
		for (i = 0; i < g_mirror_syncreqs; i++) {
			bp = sync->ds_bios[i];
			if (bp->bio_offset < offset)
				offset = bp->bio_offset;
		}
		if (sync->ds_offset_done + (MAXPHYS * 100) < offset) {
			/* Update offset_done on every 100 blocks. */
			sync->ds_offset_done = offset;
			g_mirror_update_metadata(disk);
		}
		return;
	    }
	default:
		KASSERT(1 == 0, ("Invalid command here: %u (device=%s)",
		    bp->bio_cmd, sc->sc_name));
		break;
	}
}

static void
g_mirror_request_prefer(struct g_mirror_softc *sc, struct bio *bp)
{
	struct g_mirror_disk *disk;
	struct g_consumer *cp;
	struct bio *cbp;

	LIST_FOREACH(disk, &sc->sc_disks, d_next) {
		if (disk->d_state == G_MIRROR_DISK_STATE_ACTIVE)
			break;
	}
	if (disk == NULL) {
		if (bp->bio_error == 0)
			bp->bio_error = ENXIO;
		g_io_deliver(bp, bp->bio_error);
		return;
	}
	cbp = g_clone_bio(bp);
	if (cbp == NULL) {
		if (bp->bio_error == 0)
			bp->bio_error = ENOMEM;
		g_io_deliver(bp, bp->bio_error);
		return;
	}
	/*
	 * Fill in the component buf structure.
	 */
	cp = disk->d_consumer;
	cbp->bio_done = g_mirror_done;
	cbp->bio_to = cp->provider;
	G_MIRROR_LOGREQ(3, cbp, "Sending request.");
	KASSERT(cp->acr >= 1 && cp->acw >= 1 && cp->ace >= 1,
	    ("Consumer %s not opened (r%dw%de%d).", cp->provider->name, cp->acr,
	    cp->acw, cp->ace));
	cp->index++;
	g_io_request(cbp, cp);
}

static void
g_mirror_request_round_robin(struct g_mirror_softc *sc, struct bio *bp)
{
	struct g_mirror_disk *disk;
	struct g_consumer *cp;
	struct bio *cbp;

	disk = g_mirror_get_disk(sc);
	if (disk == NULL) {
		if (bp->bio_error == 0)
			bp->bio_error = ENXIO;
		g_io_deliver(bp, bp->bio_error);
		return;
	}
	cbp = g_clone_bio(bp);
	if (cbp == NULL) {
		if (bp->bio_error == 0)
			bp->bio_error = ENOMEM;
		g_io_deliver(bp, bp->bio_error);
		return;
	}
	/*
	 * Fill in the component buf structure.
	 */
	cp = disk->d_consumer;
	cbp->bio_done = g_mirror_done;
	cbp->bio_to = cp->provider;
	G_MIRROR_LOGREQ(3, cbp, "Sending request.");
	KASSERT(cp->acr >= 1 && cp->acw >= 1 && cp->ace >= 1,
	    ("Consumer %s not opened (r%dw%de%d).", cp->provider->name, cp->acr,
	    cp->acw, cp->ace));
	cp->index++;
	g_io_request(cbp, cp);
}

#define TRACK_SIZE  (1 * 1024 * 1024)
#define LOAD_SCALE	256
#define ABS(x)		(((x) >= 0) ? (x) : (-(x)))

static void
g_mirror_request_load(struct g_mirror_softc *sc, struct bio *bp)
{
	struct g_mirror_disk *disk, *dp;
	struct g_consumer *cp;
	struct bio *cbp;
	int prio, best;

	/* Find a disk with the smallest load. */
	disk = NULL;
	best = INT_MAX;
	LIST_FOREACH(dp, &sc->sc_disks, d_next) {
		if (dp->d_state != G_MIRROR_DISK_STATE_ACTIVE)
			continue;
		prio = dp->load;
		/* If disk head is precisely in position - highly prefer it. */
		if (dp->d_last_offset == bp->bio_offset)
			prio -= 2 * LOAD_SCALE;
		else
		/* If disk head is close to position - prefer it. */
		if (ABS(dp->d_last_offset - bp->bio_offset) < TRACK_SIZE)
			prio -= 1 * LOAD_SCALE;
		if (prio <= best) {
			disk = dp;
			best = prio;
		}
	}
	KASSERT(disk != NULL, ("NULL disk for %s.", sc->sc_name));
	cbp = g_clone_bio(bp);
	if (cbp == NULL) {
		if (bp->bio_error == 0)
			bp->bio_error = ENOMEM;
		g_io_deliver(bp, bp->bio_error);
		return;
	}
	/*
	 * Fill in the component buf structure.
	 */
	cp = disk->d_consumer;
	cbp->bio_done = g_mirror_done;
	cbp->bio_to = cp->provider;
	G_MIRROR_LOGREQ(3, cbp, "Sending request.");
	KASSERT(cp->acr >= 1 && cp->acw >= 1 && cp->ace >= 1,
	    ("Consumer %s not opened (r%dw%de%d).", cp->provider->name, cp->acr,
	    cp->acw, cp->ace));
	cp->index++;
	/* Remember last head position */
	disk->d_last_offset = bp->bio_offset + bp->bio_length;
	/* Update loads. */
	LIST_FOREACH(dp, &sc->sc_disks, d_next) {
		dp->load = (dp->d_consumer->index * LOAD_SCALE +
		    dp->load * 7) / 8;
	}
	g_io_request(cbp, cp);
}

static void
g_mirror_request_split(struct g_mirror_softc *sc, struct bio *bp)
{
	struct bio_queue_head queue;
	struct g_mirror_disk *disk;
	struct g_consumer *cp;
	struct bio *cbp;
	off_t left, mod, offset, slice;
	u_char *data;
	u_int ndisks;

	if (bp->bio_length <= sc->sc_slice) {
		g_mirror_request_round_robin(sc, bp);
		return;
	}
	ndisks = g_mirror_ndisks(sc, G_MIRROR_DISK_STATE_ACTIVE);
	slice = bp->bio_length / ndisks;
	mod = slice % sc->sc_provider->sectorsize;
	if (mod != 0)
		slice += sc->sc_provider->sectorsize - mod;
	/*
	 * Allocate all bios before sending any request, so we can
	 * return ENOMEM in nice and clean way.
	 */
	left = bp->bio_length;
	offset = bp->bio_offset;
	data = bp->bio_data;
	bioq_init(&queue);
	LIST_FOREACH(disk, &sc->sc_disks, d_next) {
		if (disk->d_state != G_MIRROR_DISK_STATE_ACTIVE)
			continue;
		cbp = g_clone_bio(bp);
		if (cbp == NULL) {
			for (cbp = bioq_first(&queue); cbp != NULL;
			    cbp = bioq_first(&queue)) {
				bioq_remove(&queue, cbp);
				g_destroy_bio(cbp);
			}
			if (bp->bio_error == 0)
				bp->bio_error = ENOMEM;
			g_io_deliver(bp, bp->bio_error);
			return;
		}
		bioq_insert_tail(&queue, cbp);
		cbp->bio_done = g_mirror_done;
		cbp->bio_caller1 = disk;
		cbp->bio_to = disk->d_consumer->provider;
		cbp->bio_offset = offset;
		cbp->bio_data = data;
		cbp->bio_length = MIN(left, slice);
		left -= cbp->bio_length;
		if (left == 0)
			break;
		offset += cbp->bio_length;
		data += cbp->bio_length;
	}
	for (cbp = bioq_first(&queue); cbp != NULL; cbp = bioq_first(&queue)) {
		bioq_remove(&queue, cbp);
		G_MIRROR_LOGREQ(3, cbp, "Sending request.");
		disk = cbp->bio_caller1;
		cbp->bio_caller1 = NULL;
		cp = disk->d_consumer;
		KASSERT(cp->acr >= 1 && cp->acw >= 1 && cp->ace >= 1,
		    ("Consumer %s not opened (r%dw%de%d).", cp->provider->name,
		    cp->acr, cp->acw, cp->ace));
		disk->d_consumer->index++;
		g_io_request(cbp, disk->d_consumer);
	}
}

static void
g_mirror_register_request(struct bio *bp)
{
	struct g_mirror_softc *sc;

	sc = bp->bio_to->geom->softc;
	switch (bp->bio_cmd) {
	case BIO_READ:
		switch (sc->sc_balance) {
		case G_MIRROR_BALANCE_LOAD:
			g_mirror_request_load(sc, bp);
			break;
		case G_MIRROR_BALANCE_PREFER:
			g_mirror_request_prefer(sc, bp);
			break;
		case G_MIRROR_BALANCE_ROUND_ROBIN:
			g_mirror_request_round_robin(sc, bp);
			break;
		case G_MIRROR_BALANCE_SPLIT:
			g_mirror_request_split(sc, bp);
			break;
		}
		return;
	case BIO_WRITE:
	case BIO_DELETE:
	    {
		struct g_mirror_disk *disk;
		struct g_mirror_disk_sync *sync;
		struct bio_queue_head queue;
		struct g_consumer *cp;
		struct bio *cbp;

		/*
		 * Delay the request if it is colliding with a synchronization
		 * request.
		 */
		if (g_mirror_sync_collision(sc, bp)) {
			g_mirror_regular_delay(sc, bp);
			return;
		}

		if (sc->sc_idle)
			g_mirror_unidle(sc);
		else
			sc->sc_last_write = time_uptime;

		/*
		 * Allocate all bios before sending any request, so we can
		 * return ENOMEM in nice and clean way.
		 */
		bioq_init(&queue);
		LIST_FOREACH(disk, &sc->sc_disks, d_next) {
			sync = &disk->d_sync;
			switch (disk->d_state) {
			case G_MIRROR_DISK_STATE_ACTIVE:
				break;
			case G_MIRROR_DISK_STATE_SYNCHRONIZING:
				if (bp->bio_offset >= sync->ds_offset)
					continue;
				break;
			default:
				continue;
			}
			cbp = g_clone_bio(bp);
			if (cbp == NULL) {
				for (cbp = bioq_first(&queue); cbp != NULL;
				    cbp = bioq_first(&queue)) {
					bioq_remove(&queue, cbp);
					g_destroy_bio(cbp);
				}
				if (bp->bio_error == 0)
					bp->bio_error = ENOMEM;
				g_io_deliver(bp, bp->bio_error);
				return;
			}
			bioq_insert_tail(&queue, cbp);
			cbp->bio_done = g_mirror_done;
			cp = disk->d_consumer;
			cbp->bio_caller1 = cp;
			cbp->bio_to = cp->provider;
			KASSERT(cp->acr >= 1 && cp->acw >= 1 && cp->ace >= 1,
			    ("Consumer %s not opened (r%dw%de%d).",
			    cp->provider->name, cp->acr, cp->acw, cp->ace));
		}
		for (cbp = bioq_first(&queue); cbp != NULL;
		    cbp = bioq_first(&queue)) {
			bioq_remove(&queue, cbp);
			G_MIRROR_LOGREQ(3, cbp, "Sending request.");
			cp = cbp->bio_caller1;
			cbp->bio_caller1 = NULL;
			cp->index++;
			sc->sc_writes++;
			g_io_request(cbp, cp);
		}
		/*
		 * Put request onto inflight queue, so we can check if new
		 * synchronization requests don't collide with it.
		 */
		bioq_insert_tail(&sc->sc_inflight, bp);
		/*
		 * Bump syncid on first write.
		 */
		if ((sc->sc_bump_id & G_MIRROR_BUMP_SYNCID) != 0) {
			sc->sc_bump_id &= ~G_MIRROR_BUMP_SYNCID;
			g_mirror_bump_syncid(sc);
		}
		return;
	    }
	default:
		KASSERT(1 == 0, ("Invalid command here: %u (device=%s)",
		    bp->bio_cmd, sc->sc_name));
		break;
	}
}

static int
g_mirror_can_destroy(struct g_mirror_softc *sc)
{
	struct g_geom *gp;
	struct g_consumer *cp;

	g_topology_assert();
	gp = sc->sc_geom;
	if (gp->softc == NULL)
		return (1);
	LIST_FOREACH(cp, &gp->consumer, consumer) {
		if (g_mirror_is_busy(sc, cp))
			return (0);
	}
	gp = sc->sc_sync.ds_geom;
	LIST_FOREACH(cp, &gp->consumer, consumer) {
		if (g_mirror_is_busy(sc, cp))
			return (0);
	}
	G_MIRROR_DEBUG(2, "No I/O requests for %s, it can be destroyed.",
	    sc->sc_name);
	return (1);
}

static int
g_mirror_try_destroy(struct g_mirror_softc *sc)
{

	if (sc->sc_rootmount != NULL) {
		G_MIRROR_DEBUG(1, "root_mount_rel[%u] %p", __LINE__,
		    sc->sc_rootmount);
		root_mount_rel(sc->sc_rootmount);
		sc->sc_rootmount = NULL;
	}
	g_topology_lock();
	if (!g_mirror_can_destroy(sc)) {
		g_topology_unlock();
		return (0);
	}
	sc->sc_geom->softc = NULL;
	sc->sc_sync.ds_geom->softc = NULL;
	if ((sc->sc_flags & G_MIRROR_DEVICE_FLAG_WAIT) != 0) {
		g_topology_unlock();
		G_MIRROR_DEBUG(4, "%s: Waking up %p.", __func__,
		    &sc->sc_worker);
		/* Unlock sc_lock here, as it can be destroyed after wakeup. */
		sx_xunlock(&sc->sc_lock);
		wakeup(&sc->sc_worker);
		sc->sc_worker = NULL;
	} else {
		g_topology_unlock();
		g_mirror_destroy_device(sc);
		free(sc, M_MIRROR);
	}
	return (1);
}

/*
 * Worker thread.
 */
static void
g_mirror_worker(void *arg)
{
	struct g_mirror_softc *sc;
	struct g_mirror_event *ep;
	struct bio *bp;
	int timeout;

	sc = arg;
	thread_lock(curthread);
	sched_prio(curthread, PRIBIO);
	thread_unlock(curthread);

	sx_xlock(&sc->sc_lock);
	for (;;) {
		G_MIRROR_DEBUG(5, "%s: Let's see...", __func__);
		/*
		 * First take a look at events.
		 * This is important to handle events before any I/O requests.
		 */
		ep = g_mirror_event_get(sc);
		if (ep != NULL) {
			g_mirror_event_remove(sc, ep);
			if ((ep->e_flags & G_MIRROR_EVENT_DEVICE) != 0) {
				/* Update only device status. */
				G_MIRROR_DEBUG(3,
				    "Running event for device %s.",
				    sc->sc_name);
				ep->e_error = 0;
				g_mirror_update_device(sc, 1);
			} else {
				/* Update disk status. */
				G_MIRROR_DEBUG(3, "Running event for disk %s.",
				     g_mirror_get_diskname(ep->e_disk));
				ep->e_error = g_mirror_update_disk(ep->e_disk,
				    ep->e_state);
				if (ep->e_error == 0)
					g_mirror_update_device(sc, 0);
			}
			if ((ep->e_flags & G_MIRROR_EVENT_DONTWAIT) != 0) {
				KASSERT(ep->e_error == 0,
				    ("Error cannot be handled."));
				g_mirror_event_free(ep);
			} else {
				ep->e_flags |= G_MIRROR_EVENT_DONE;
				G_MIRROR_DEBUG(4, "%s: Waking up %p.", __func__,
				    ep);
				mtx_lock(&sc->sc_events_mtx);
				wakeup(ep);
				mtx_unlock(&sc->sc_events_mtx);
			}
			if ((sc->sc_flags &
			    G_MIRROR_DEVICE_FLAG_DESTROY) != 0) {
				if (g_mirror_try_destroy(sc)) {
					curthread->td_pflags &= ~TDP_GEOM;
					G_MIRROR_DEBUG(1, "Thread exiting.");
					kproc_exit(0);
				}
			}
			G_MIRROR_DEBUG(5, "%s: I'm here 1.", __func__);
			continue;
		}
		/*
		 * Check if we can mark array as CLEAN and if we can't take
		 * how much seconds should we wait.
		 */
		timeout = g_mirror_idle(sc, -1);
		/*
		 * Now I/O requests.
		 */
		/* Get first request from the queue. */
		mtx_lock(&sc->sc_queue_mtx);
		bp = bioq_first(&sc->sc_queue);
		if (bp == NULL) {
			if ((sc->sc_flags &
			    G_MIRROR_DEVICE_FLAG_DESTROY) != 0) {
				mtx_unlock(&sc->sc_queue_mtx);
				if (g_mirror_try_destroy(sc)) {
					curthread->td_pflags &= ~TDP_GEOM;
					G_MIRROR_DEBUG(1, "Thread exiting.");
					kproc_exit(0);
				}
				mtx_lock(&sc->sc_queue_mtx);
			}
			sx_xunlock(&sc->sc_lock);
			/*
			 * XXX: We can miss an event here, because an event
			 *      can be added without sx-device-lock and without
			 *      mtx-queue-lock. Maybe I should just stop using
			 *      dedicated mutex for events synchronization and
			 *      stick with the queue lock?
			 *      The event will hang here until next I/O request
			 *      or next event is received.
			 */
			MSLEEP(sc, &sc->sc_queue_mtx, PRIBIO | PDROP, "m:w1",
			    timeout * hz);
			sx_xlock(&sc->sc_lock);
			G_MIRROR_DEBUG(5, "%s: I'm here 4.", __func__);
			continue;
		}
		bioq_remove(&sc->sc_queue, bp);
		mtx_unlock(&sc->sc_queue_mtx);

		if (bp->bio_from->geom == sc->sc_sync.ds_geom &&
		    (bp->bio_cflags & G_MIRROR_BIO_FLAG_SYNC) != 0) {
			g_mirror_sync_request(bp);	/* READ */
		} else if (bp->bio_to != sc->sc_provider) {
			if ((bp->bio_cflags & G_MIRROR_BIO_FLAG_REGULAR) != 0)
				g_mirror_regular_request(bp);
			else if ((bp->bio_cflags & G_MIRROR_BIO_FLAG_SYNC) != 0)
				g_mirror_sync_request(bp);	/* WRITE */
			else {
				KASSERT(0,
				    ("Invalid request cflags=0x%hhx to=%s.",
				    bp->bio_cflags, bp->bio_to->name));
			}
		} else {
			g_mirror_register_request(bp);
		}
		G_MIRROR_DEBUG(5, "%s: I'm here 9.", __func__);
	}
}

static void
g_mirror_update_idle(struct g_mirror_softc *sc, struct g_mirror_disk *disk)
{

	sx_assert(&sc->sc_lock, SX_LOCKED);

	if ((sc->sc_flags & G_MIRROR_DEVICE_FLAG_NOFAILSYNC) != 0)
		return;
	if (!sc->sc_idle && (disk->d_flags & G_MIRROR_DISK_FLAG_DIRTY) == 0) {
		G_MIRROR_DEBUG(1, "Disk %s (device %s) marked as dirty.",
		    g_mirror_get_diskname(disk), sc->sc_name);
		disk->d_flags |= G_MIRROR_DISK_FLAG_DIRTY;
	} else if (sc->sc_idle &&
	    (disk->d_flags & G_MIRROR_DISK_FLAG_DIRTY) != 0) {
		G_MIRROR_DEBUG(1, "Disk %s (device %s) marked as clean.",
		    g_mirror_get_diskname(disk), sc->sc_name);
		disk->d_flags &= ~G_MIRROR_DISK_FLAG_DIRTY;
	}
}

static void
g_mirror_sync_start(struct g_mirror_disk *disk)
{
	struct g_mirror_softc *sc;
	struct g_consumer *cp;
	struct bio *bp;
	int error, i;

	g_topology_assert_not();
	sc = disk->d_softc;
	sx_assert(&sc->sc_lock, SX_LOCKED);

	KASSERT(disk->d_state == G_MIRROR_DISK_STATE_SYNCHRONIZING,
	    ("Disk %s is not marked for synchronization.",
	    g_mirror_get_diskname(disk)));
	KASSERT(sc->sc_state == G_MIRROR_DEVICE_STATE_RUNNING,
	    ("Device not in RUNNING state (%s, %u).", sc->sc_name,
	    sc->sc_state));

	sx_xunlock(&sc->sc_lock);
	g_topology_lock();
	cp = g_new_consumer(sc->sc_sync.ds_geom);
	error = g_attach(cp, sc->sc_provider);
	KASSERT(error == 0,
	    ("Cannot attach to %s (error=%d).", sc->sc_name, error));
	error = g_access(cp, 1, 0, 0);
	KASSERT(error == 0, ("Cannot open %s (error=%d).", sc->sc_name, error));
	g_topology_unlock();
	sx_xlock(&sc->sc_lock);

	G_MIRROR_DEBUG(0, "Device %s: rebuilding provider %s.", sc->sc_name,
	    g_mirror_get_diskname(disk));
	if ((sc->sc_flags & G_MIRROR_DEVICE_FLAG_NOFAILSYNC) == 0)
		disk->d_flags |= G_MIRROR_DISK_FLAG_DIRTY;
	KASSERT(disk->d_sync.ds_consumer == NULL,
	    ("Sync consumer already exists (device=%s, disk=%s).",
	    sc->sc_name, g_mirror_get_diskname(disk)));

	disk->d_sync.ds_consumer = cp;
	disk->d_sync.ds_consumer->private = disk;
	disk->d_sync.ds_consumer->index = 0;

	/*
	 * Allocate memory for synchronization bios and initialize them.
	 */
	disk->d_sync.ds_bios = malloc(sizeof(struct bio *) * g_mirror_syncreqs,
	    M_MIRROR, M_WAITOK);
	for (i = 0; i < g_mirror_syncreqs; i++) {
		bp = g_alloc_bio();
		disk->d_sync.ds_bios[i] = bp;
		bp->bio_parent = NULL;
		bp->bio_cmd = BIO_READ;
		bp->bio_data = malloc(MAXPHYS, M_MIRROR, M_WAITOK);
		bp->bio_cflags = 0;
		bp->bio_offset = disk->d_sync.ds_offset;
		bp->bio_length = MIN(MAXPHYS, sc->sc_mediasize - bp->bio_offset);
		disk->d_sync.ds_offset += bp->bio_length;
		bp->bio_done = g_mirror_sync_done;
		bp->bio_from = disk->d_sync.ds_consumer;
		bp->bio_to = sc->sc_provider;
		bp->bio_caller1 = (void *)(uintptr_t)i;
	}

	/* Increase the number of disks in SYNCHRONIZING state. */
	sc->sc_sync.ds_ndisks++;
	/* Set the number of in-flight synchronization requests. */
	disk->d_sync.ds_inflight = g_mirror_syncreqs;

	/*
	 * Fire off first synchronization requests.
	 */
	for (i = 0; i < g_mirror_syncreqs; i++) {
		bp = disk->d_sync.ds_bios[i];
		G_MIRROR_LOGREQ(3, bp, "Sending synchronization request.");
		disk->d_sync.ds_consumer->index++;
		/*
		 * Delay the request if it is colliding with a regular request.
		 */
		if (g_mirror_regular_collision(sc, bp))
			g_mirror_sync_delay(sc, bp);
		else
			g_io_request(bp, disk->d_sync.ds_consumer);
	}
}

/*
 * Stop synchronization process.
 * type: 0 - synchronization finished
 *       1 - synchronization stopped
 */
static void
g_mirror_sync_stop(struct g_mirror_disk *disk, int type)
{
	struct g_mirror_softc *sc;
	struct g_consumer *cp;

	g_topology_assert_not();
	sc = disk->d_softc;
	sx_assert(&sc->sc_lock, SX_LOCKED);

	KASSERT(disk->d_state == G_MIRROR_DISK_STATE_SYNCHRONIZING,
	    ("Wrong disk state (%s, %s).", g_mirror_get_diskname(disk),
	    g_mirror_disk_state2str(disk->d_state)));
	if (disk->d_sync.ds_consumer == NULL)
		return;

	if (type == 0) {
		G_MIRROR_DEBUG(0, "Device %s: rebuilding provider %s finished.",
		    sc->sc_name, g_mirror_get_diskname(disk));
	} else /* if (type == 1) */ {
		G_MIRROR_DEBUG(0, "Device %s: rebuilding provider %s stopped.",
		    sc->sc_name, g_mirror_get_diskname(disk));
	}
	free(disk->d_sync.ds_bios, M_MIRROR);
	disk->d_sync.ds_bios = NULL;
	cp = disk->d_sync.ds_consumer;
	disk->d_sync.ds_consumer = NULL;
	disk->d_flags &= ~G_MIRROR_DISK_FLAG_DIRTY;
	sc->sc_sync.ds_ndisks--;
	sx_xunlock(&sc->sc_lock); /* Avoid recursion on sc_lock. */
	g_topology_lock();
	g_mirror_kill_consumer(sc, cp);
	g_topology_unlock();
	sx_xlock(&sc->sc_lock);
}

static void
g_mirror_launch_provider(struct g_mirror_softc *sc)
{
	struct g_mirror_disk *disk;
	struct g_provider *pp;

	sx_assert(&sc->sc_lock, SX_LOCKED);

	g_topology_lock();
	pp = g_new_providerf(sc->sc_geom, "mirror/%s", sc->sc_name);
	pp->mediasize = sc->sc_mediasize;
	pp->sectorsize = sc->sc_sectorsize;
	pp->stripesize = 0;
	pp->stripeoffset = 0;
	LIST_FOREACH(disk, &sc->sc_disks, d_next) {
		if (disk->d_consumer && disk->d_consumer->provider &&
		    disk->d_consumer->provider->stripesize > pp->stripesize) {
			pp->stripesize = disk->d_consumer->provider->stripesize;
			pp->stripeoffset = disk->d_consumer->provider->stripeoffset;
		}
	}
	sc->sc_provider = pp;
	g_error_provider(pp, 0);
	g_topology_unlock();
	G_MIRROR_DEBUG(0, "Device %s launched (%u/%u).", pp->name,
	    g_mirror_ndisks(sc, G_MIRROR_DISK_STATE_ACTIVE), sc->sc_ndisks);
	LIST_FOREACH(disk, &sc->sc_disks, d_next) {
		if (disk->d_state == G_MIRROR_DISK_STATE_SYNCHRONIZING)
			g_mirror_sync_start(disk);
	}
}

static void
g_mirror_destroy_provider(struct g_mirror_softc *sc)
{
	struct g_mirror_disk *disk;
	struct bio *bp;

	g_topology_assert_not();
	KASSERT(sc->sc_provider != NULL, ("NULL provider (device=%s).",
	    sc->sc_name));

	g_topology_lock();
	g_error_provider(sc->sc_provider, ENXIO);
	mtx_lock(&sc->sc_queue_mtx);
	while ((bp = bioq_first(&sc->sc_queue)) != NULL) {
		bioq_remove(&sc->sc_queue, bp);
		g_io_deliver(bp, ENXIO);
	}
	mtx_unlock(&sc->sc_queue_mtx);
	G_MIRROR_DEBUG(0, "Device %s: provider %s destroyed.", sc->sc_name,
	    sc->sc_provider->name);
	sc->sc_provider->flags |= G_PF_WITHER;
	g_orphan_provider(sc->sc_provider, ENXIO);
	g_topology_unlock();
	sc->sc_provider = NULL;
	LIST_FOREACH(disk, &sc->sc_disks, d_next) {
		if (disk->d_state == G_MIRROR_DISK_STATE_SYNCHRONIZING)
			g_mirror_sync_stop(disk, 1);
	}
}

static void
g_mirror_go(void *arg)
{
	struct g_mirror_softc *sc;

	sc = arg;
	G_MIRROR_DEBUG(0, "Force device %s start due to timeout.", sc->sc_name);
	g_mirror_event_send(sc, 0,
	    G_MIRROR_EVENT_DONTWAIT | G_MIRROR_EVENT_DEVICE);
}

static u_int
g_mirror_determine_state(struct g_mirror_disk *disk)
{
	struct g_mirror_softc *sc;
	u_int state;

	sc = disk->d_softc;
	if (sc->sc_syncid == disk->d_sync.ds_syncid) {
		if ((disk->d_flags &
		    G_MIRROR_DISK_FLAG_SYNCHRONIZING) == 0) {
			/* Disk does not need synchronization. */
			state = G_MIRROR_DISK_STATE_ACTIVE;
		} else {
			if ((sc->sc_flags &
			     G_MIRROR_DEVICE_FLAG_NOAUTOSYNC) == 0 ||
			    (disk->d_flags &
			     G_MIRROR_DISK_FLAG_FORCE_SYNC) != 0) {
				/*
				 * We can start synchronization from
				 * the stored offset.
				 */
				state = G_MIRROR_DISK_STATE_SYNCHRONIZING;
			} else {
				state = G_MIRROR_DISK_STATE_STALE;
			}
		}
	} else if (disk->d_sync.ds_syncid < sc->sc_syncid) {
		/*
		 * Reset all synchronization data for this disk,
		 * because if it even was synchronized, it was
		 * synchronized to disks with different syncid.
		 */
		disk->d_flags |= G_MIRROR_DISK_FLAG_SYNCHRONIZING;
		disk->d_sync.ds_offset = 0;
		disk->d_sync.ds_offset_done = 0;
		disk->d_sync.ds_syncid = sc->sc_syncid;
		if ((sc->sc_flags & G_MIRROR_DEVICE_FLAG_NOAUTOSYNC) == 0 ||
		    (disk->d_flags & G_MIRROR_DISK_FLAG_FORCE_SYNC) != 0) {
			state = G_MIRROR_DISK_STATE_SYNCHRONIZING;
		} else {
			state = G_MIRROR_DISK_STATE_STALE;
		}
	} else /* if (sc->sc_syncid < disk->d_sync.ds_syncid) */ {
		/*
		 * Not good, NOT GOOD!
		 * It means that mirror was started on stale disks
		 * and more fresh disk just arrive.
		 * If there were writes, mirror is broken, sorry.
		 * I think the best choice here is don't touch
		 * this disk and inform the user loudly.
		 */
		G_MIRROR_DEBUG(0, "Device %s was started before the freshest "
		    "disk (%s) arrives!! It will not be connected to the "
		    "running device.", sc->sc_name,
		    g_mirror_get_diskname(disk));
		g_mirror_destroy_disk(disk);
		state = G_MIRROR_DISK_STATE_NONE;
		/* Return immediately, because disk was destroyed. */
		return (state);
	}
	G_MIRROR_DEBUG(3, "State for %s disk: %s.",
	    g_mirror_get_diskname(disk), g_mirror_disk_state2str(state));
	return (state);
}

/*
 * Update device state.
 */
static void
g_mirror_update_device(struct g_mirror_softc *sc, boolean_t force)
{
	struct g_mirror_disk *disk;
	u_int state;

	sx_assert(&sc->sc_lock, SX_XLOCKED);

	switch (sc->sc_state) {
	case G_MIRROR_DEVICE_STATE_STARTING:
	    {
		struct g_mirror_disk *pdisk, *tdisk;
		u_int dirty, ndisks, genid, syncid;

		KASSERT(sc->sc_provider == NULL,
		    ("Non-NULL provider in STARTING state (%s).", sc->sc_name));
		/*
		 * Are we ready? We are, if all disks are connected or
		 * if we have any disks and 'force' is true.
		 */
		ndisks = g_mirror_ndisks(sc, -1);
		if (sc->sc_ndisks == ndisks || (force && ndisks > 0)) {
			;
		} else if (ndisks == 0) {
			/*
			 * Disks went down in starting phase, so destroy
			 * device.
			 */
			callout_drain(&sc->sc_callout);
			sc->sc_flags |= G_MIRROR_DEVICE_FLAG_DESTROY;
			G_MIRROR_DEBUG(1, "root_mount_rel[%u] %p", __LINE__,
			    sc->sc_rootmount);
			root_mount_rel(sc->sc_rootmount);
			sc->sc_rootmount = NULL;
			return;
		} else {
			return;
		}

		/*
		 * Activate all disks with the biggest syncid.
		 */
		if (force) {
			/*
			 * If 'force' is true, we have been called due to
			 * timeout, so don't bother canceling timeout.
			 */
			ndisks = 0;
			LIST_FOREACH(disk, &sc->sc_disks, d_next) {
				if ((disk->d_flags &
				    G_MIRROR_DISK_FLAG_SYNCHRONIZING) == 0) {
					ndisks++;
				}
			}
			if (ndisks == 0) {
				/* No valid disks found, destroy device. */
				sc->sc_flags |= G_MIRROR_DEVICE_FLAG_DESTROY;
				G_MIRROR_DEBUG(1, "root_mount_rel[%u] %p",
				    __LINE__, sc->sc_rootmount);
				root_mount_rel(sc->sc_rootmount);
				sc->sc_rootmount = NULL;
				return;
			}
		} else {
			/* Cancel timeout. */
			callout_drain(&sc->sc_callout);
		}

		/*
		 * Find the biggest genid.
		 */
		genid = 0;
		LIST_FOREACH(disk, &sc->sc_disks, d_next) {
			if (disk->d_genid > genid)
				genid = disk->d_genid;
		}
		sc->sc_genid = genid;
		/*
		 * Remove all disks without the biggest genid.
		 */
		LIST_FOREACH_SAFE(disk, &sc->sc_disks, d_next, tdisk) {
			if (disk->d_genid < genid) {
				G_MIRROR_DEBUG(0,
				    "Component %s (device %s) broken, skipping.",
				    g_mirror_get_diskname(disk), sc->sc_name);
				g_mirror_destroy_disk(disk);
			}
		}

		/*
		 * Find the biggest syncid.
		 */
		syncid = 0;
		LIST_FOREACH(disk, &sc->sc_disks, d_next) {
			if (disk->d_sync.ds_syncid > syncid)
				syncid = disk->d_sync.ds_syncid;
		}

		/*
		 * Here we need to look for dirty disks and if all disks
		 * with the biggest syncid are dirty, we have to choose
		 * one with the biggest priority and rebuild the rest.
		 */
		/*
		 * Find the number of dirty disks with the biggest syncid.
		 * Find the number of disks with the biggest syncid.
		 * While here, find a disk with the biggest priority.
		 */
		dirty = ndisks = 0;
		pdisk = NULL;
		LIST_FOREACH(disk, &sc->sc_disks, d_next) {
			if (disk->d_sync.ds_syncid != syncid)
				continue;
			if ((disk->d_flags &
			    G_MIRROR_DISK_FLAG_SYNCHRONIZING) != 0) {
				continue;
			}
			ndisks++;
			if ((disk->d_flags & G_MIRROR_DISK_FLAG_DIRTY) != 0) {
				dirty++;
				if (pdisk == NULL ||
				    pdisk->d_priority < disk->d_priority) {
					pdisk = disk;
				}
			}
		}
		if (dirty == 0) {
			/* No dirty disks at all, great. */
		} else if (dirty == ndisks) {
			/*
			 * Force synchronization for all dirty disks except one
			 * with the biggest priority.
			 */
			KASSERT(pdisk != NULL, ("pdisk == NULL"));
			G_MIRROR_DEBUG(1, "Using disk %s (device %s) as a "
			    "master disk for synchronization.",
			    g_mirror_get_diskname(pdisk), sc->sc_name);
			LIST_FOREACH(disk, &sc->sc_disks, d_next) {
				if (disk->d_sync.ds_syncid != syncid)
					continue;
				if ((disk->d_flags &
				    G_MIRROR_DISK_FLAG_SYNCHRONIZING) != 0) {
					continue;
				}
				KASSERT((disk->d_flags &
				    G_MIRROR_DISK_FLAG_DIRTY) != 0,
				    ("Disk %s isn't marked as dirty.",
				    g_mirror_get_diskname(disk)));
				/* Skip the disk with the biggest priority. */
				if (disk == pdisk)
					continue;
				disk->d_sync.ds_syncid = 0;
			}
		} else if (dirty < ndisks) {
			/*
			 * Force synchronization for all dirty disks.
			 * We have some non-dirty disks.
			 */
			LIST_FOREACH(disk, &sc->sc_disks, d_next) {
				if (disk->d_sync.ds_syncid != syncid)
					continue;
				if ((disk->d_flags &
				    G_MIRROR_DISK_FLAG_SYNCHRONIZING) != 0) {
					continue;
				}
				if ((disk->d_flags &
				    G_MIRROR_DISK_FLAG_DIRTY) == 0) {
					continue;
				}
				disk->d_sync.ds_syncid = 0;
			}
		}

		/* Reset hint. */
		sc->sc_hint = NULL;
		sc->sc_syncid = syncid;
		if (force) {
			/* Remember to bump syncid on first write. */
			sc->sc_bump_id |= G_MIRROR_BUMP_SYNCID;
		}
		state = G_MIRROR_DEVICE_STATE_RUNNING;
		G_MIRROR_DEBUG(1, "Device %s state changed from %s to %s.",
		    sc->sc_name, g_mirror_device_state2str(sc->sc_state),
		    g_mirror_device_state2str(state));
		sc->sc_state = state;
		LIST_FOREACH(disk, &sc->sc_disks, d_next) {
			state = g_mirror_determine_state(disk);
			g_mirror_event_send(disk, state,
			    G_MIRROR_EVENT_DONTWAIT);
			if (state == G_MIRROR_DISK_STATE_STALE)
				sc->sc_bump_id |= G_MIRROR_BUMP_SYNCID;
		}
		break;
	    }
	case G_MIRROR_DEVICE_STATE_RUNNING:
		if (g_mirror_ndisks(sc, G_MIRROR_DISK_STATE_ACTIVE) == 0 &&
		    g_mirror_ndisks(sc, G_MIRROR_DISK_STATE_NEW) == 0) {
			/*
			 * No active disks or no disks at all,
			 * so destroy device.
			 */
			if (sc->sc_provider != NULL)
				g_mirror_destroy_provider(sc);
			sc->sc_flags |= G_MIRROR_DEVICE_FLAG_DESTROY;
			break;
		} else if (g_mirror_ndisks(sc,
		    G_MIRROR_DISK_STATE_ACTIVE) > 0 &&
		    g_mirror_ndisks(sc, G_MIRROR_DISK_STATE_NEW) == 0) {
			/*
			 * We have active disks, launch provider if it doesn't
			 * exist.
			 */
			if (sc->sc_provider == NULL)
				g_mirror_launch_provider(sc);
			if (sc->sc_rootmount != NULL) {
				G_MIRROR_DEBUG(1, "root_mount_rel[%u] %p",
				    __LINE__, sc->sc_rootmount);
				root_mount_rel(sc->sc_rootmount);
				sc->sc_rootmount = NULL;
			}
		}
		/*
		 * Genid should be bumped immediately, so do it here.
		 */
		if ((sc->sc_bump_id & G_MIRROR_BUMP_GENID) != 0) {
			sc->sc_bump_id &= ~G_MIRROR_BUMP_GENID;
			g_mirror_bump_genid(sc);
		}
		break;
	default:
		KASSERT(1 == 0, ("Wrong device state (%s, %s).",
		    sc->sc_name, g_mirror_device_state2str(sc->sc_state)));
		break;
	}
}

/*
 * Update disk state and device state if needed.
 */
#define	DISK_STATE_CHANGED()	G_MIRROR_DEBUG(1,			\
	"Disk %s state changed from %s to %s (device %s).",		\
	g_mirror_get_diskname(disk),					\
	g_mirror_disk_state2str(disk->d_state),				\
	g_mirror_disk_state2str(state), sc->sc_name)
static int
g_mirror_update_disk(struct g_mirror_disk *disk, u_int state)
{
	struct g_mirror_softc *sc;

	sc = disk->d_softc;
	sx_assert(&sc->sc_lock, SX_XLOCKED);

again:
	G_MIRROR_DEBUG(3, "Changing disk %s state from %s to %s.",
	    g_mirror_get_diskname(disk), g_mirror_disk_state2str(disk->d_state),
	    g_mirror_disk_state2str(state));
	switch (state) {
	case G_MIRROR_DISK_STATE_NEW:
		/*
		 * Possible scenarios:
		 * 1. New disk arrive.
		 */
		/* Previous state should be NONE. */
		KASSERT(disk->d_state == G_MIRROR_DISK_STATE_NONE,
		    ("Wrong disk state (%s, %s).", g_mirror_get_diskname(disk),
		    g_mirror_disk_state2str(disk->d_state)));
		DISK_STATE_CHANGED();

		disk->d_state = state;
		if (LIST_EMPTY(&sc->sc_disks))
			LIST_INSERT_HEAD(&sc->sc_disks, disk, d_next);
		else {
			struct g_mirror_disk *dp;

			LIST_FOREACH(dp, &sc->sc_disks, d_next) {
				if (disk->d_priority >= dp->d_priority) {
					LIST_INSERT_BEFORE(dp, disk, d_next);
					dp = NULL;
					break;
				}
				if (LIST_NEXT(dp, d_next) == NULL)
					break;
			}
			if (dp != NULL)
				LIST_INSERT_AFTER(dp, disk, d_next);
		}
		G_MIRROR_DEBUG(1, "Device %s: provider %s detected.",
		    sc->sc_name, g_mirror_get_diskname(disk));
		if (sc->sc_state == G_MIRROR_DEVICE_STATE_STARTING)
			break;
		KASSERT(sc->sc_state == G_MIRROR_DEVICE_STATE_RUNNING,
		    ("Wrong device state (%s, %s, %s, %s).", sc->sc_name,
		    g_mirror_device_state2str(sc->sc_state),
		    g_mirror_get_diskname(disk),
		    g_mirror_disk_state2str(disk->d_state)));
		state = g_mirror_determine_state(disk);
		if (state != G_MIRROR_DISK_STATE_NONE)
			goto again;
		break;
	case G_MIRROR_DISK_STATE_ACTIVE:
		/*
		 * Possible scenarios:
		 * 1. New disk does not need synchronization.
		 * 2. Synchronization process finished successfully.
		 */
		KASSERT(sc->sc_state == G_MIRROR_DEVICE_STATE_RUNNING,
		    ("Wrong device state (%s, %s, %s, %s).", sc->sc_name,
		    g_mirror_device_state2str(sc->sc_state),
		    g_mirror_get_diskname(disk),
		    g_mirror_disk_state2str(disk->d_state)));
		/* Previous state should be NEW or SYNCHRONIZING. */
		KASSERT(disk->d_state == G_MIRROR_DISK_STATE_NEW ||
		    disk->d_state == G_MIRROR_DISK_STATE_SYNCHRONIZING,
		    ("Wrong disk state (%s, %s).", g_mirror_get_diskname(disk),
		    g_mirror_disk_state2str(disk->d_state)));
		DISK_STATE_CHANGED();

		if (disk->d_state == G_MIRROR_DISK_STATE_SYNCHRONIZING) {
			disk->d_flags &= ~G_MIRROR_DISK_FLAG_SYNCHRONIZING;
			disk->d_flags &= ~G_MIRROR_DISK_FLAG_FORCE_SYNC;
			g_mirror_sync_stop(disk, 0);
		}
		disk->d_state = state;
		disk->d_sync.ds_offset = 0;
		disk->d_sync.ds_offset_done = 0;
		g_mirror_update_idle(sc, disk);
		g_mirror_update_metadata(disk);
		G_MIRROR_DEBUG(1, "Device %s: provider %s activated.",
		    sc->sc_name, g_mirror_get_diskname(disk));
		break;
	case G_MIRROR_DISK_STATE_STALE:
		/*
		 * Possible scenarios:
		 * 1. Stale disk was connected.
		 */
		/* Previous state should be NEW. */
		KASSERT(disk->d_state == G_MIRROR_DISK_STATE_NEW,
		    ("Wrong disk state (%s, %s).", g_mirror_get_diskname(disk),
		    g_mirror_disk_state2str(disk->d_state)));
		KASSERT(sc->sc_state == G_MIRROR_DEVICE_STATE_RUNNING,
		    ("Wrong device state (%s, %s, %s, %s).", sc->sc_name,
		    g_mirror_device_state2str(sc->sc_state),
		    g_mirror_get_diskname(disk),
		    g_mirror_disk_state2str(disk->d_state)));
		/*
		 * STALE state is only possible if device is marked
		 * NOAUTOSYNC.
		 */
		KASSERT((sc->sc_flags & G_MIRROR_DEVICE_FLAG_NOAUTOSYNC) != 0,
		    ("Wrong device state (%s, %s, %s, %s).", sc->sc_name,
		    g_mirror_device_state2str(sc->sc_state),
		    g_mirror_get_diskname(disk),
		    g_mirror_disk_state2str(disk->d_state)));
		DISK_STATE_CHANGED();

		disk->d_flags &= ~G_MIRROR_DISK_FLAG_DIRTY;
		disk->d_state = state;
		g_mirror_update_metadata(disk);
		G_MIRROR_DEBUG(0, "Device %s: provider %s is stale.",
		    sc->sc_name, g_mirror_get_diskname(disk));
		break;
	case G_MIRROR_DISK_STATE_SYNCHRONIZING:
		/*
		 * Possible scenarios:
		 * 1. Disk which needs synchronization was connected.
		 */
		/* Previous state should be NEW. */
		KASSERT(disk->d_state == G_MIRROR_DISK_STATE_NEW,
		    ("Wrong disk state (%s, %s).", g_mirror_get_diskname(disk),
		    g_mirror_disk_state2str(disk->d_state)));
		KASSERT(sc->sc_state == G_MIRROR_DEVICE_STATE_RUNNING,
		    ("Wrong device state (%s, %s, %s, %s).", sc->sc_name,
		    g_mirror_device_state2str(sc->sc_state),
		    g_mirror_get_diskname(disk),
		    g_mirror_disk_state2str(disk->d_state)));
		DISK_STATE_CHANGED();

		if (disk->d_state == G_MIRROR_DISK_STATE_NEW)
			disk->d_flags &= ~G_MIRROR_DISK_FLAG_DIRTY;
		disk->d_state = state;
		if (sc->sc_provider != NULL) {
			g_mirror_sync_start(disk);
			g_mirror_update_metadata(disk);
		}
		break;
	case G_MIRROR_DISK_STATE_DISCONNECTED:
		/*
		 * Possible scenarios:
		 * 1. Device wasn't running yet, but disk disappear.
		 * 2. Disk was active and disapppear.
		 * 3. Disk disappear during synchronization process.
		 */
		if (sc->sc_state == G_MIRROR_DEVICE_STATE_RUNNING) {
			/*
			 * Previous state should be ACTIVE, STALE or
			 * SYNCHRONIZING.
			 */
			KASSERT(disk->d_state == G_MIRROR_DISK_STATE_ACTIVE ||
			    disk->d_state == G_MIRROR_DISK_STATE_STALE ||
			    disk->d_state == G_MIRROR_DISK_STATE_SYNCHRONIZING,
			    ("Wrong disk state (%s, %s).",
			    g_mirror_get_diskname(disk),
			    g_mirror_disk_state2str(disk->d_state)));
		} else if (sc->sc_state == G_MIRROR_DEVICE_STATE_STARTING) {
			/* Previous state should be NEW. */
			KASSERT(disk->d_state == G_MIRROR_DISK_STATE_NEW,
			    ("Wrong disk state (%s, %s).",
			    g_mirror_get_diskname(disk),
			    g_mirror_disk_state2str(disk->d_state)));
			/*
			 * Reset bumping syncid if disk disappeared in STARTING
			 * state.
			 */
			if ((sc->sc_bump_id & G_MIRROR_BUMP_SYNCID) != 0)
				sc->sc_bump_id &= ~G_MIRROR_BUMP_SYNCID;
#ifdef	INVARIANTS
		} else {
			KASSERT(1 == 0, ("Wrong device state (%s, %s, %s, %s).",
			    sc->sc_name,
			    g_mirror_device_state2str(sc->sc_state),
			    g_mirror_get_diskname(disk),
			    g_mirror_disk_state2str(disk->d_state)));
#endif
		}
		DISK_STATE_CHANGED();
		G_MIRROR_DEBUG(0, "Device %s: provider %s disconnected.",
		    sc->sc_name, g_mirror_get_diskname(disk));

		g_mirror_destroy_disk(disk);
		break;
	case G_MIRROR_DISK_STATE_DESTROY:
	    {
		int error;

		error = g_mirror_clear_metadata(disk);
		if (error != 0)
			return (error);
		DISK_STATE_CHANGED();
		G_MIRROR_DEBUG(0, "Device %s: provider %s destroyed.",
		    sc->sc_name, g_mirror_get_diskname(disk));

		g_mirror_destroy_disk(disk);
		sc->sc_ndisks--;
		LIST_FOREACH(disk, &sc->sc_disks, d_next) {
			g_mirror_update_metadata(disk);
		}
		break;
	    }
	default:
		KASSERT(1 == 0, ("Unknown state (%u).", state));
		break;
	}
	return (0);
}
#undef	DISK_STATE_CHANGED

int
g_mirror_read_metadata(struct g_consumer *cp, struct g_mirror_metadata *md)
{
	struct g_provider *pp;
	u_char *buf;
	int error;

	g_topology_assert();

	error = g_access(cp, 1, 0, 0);
	if (error != 0)
		return (error);
	pp = cp->provider;
	g_topology_unlock();
	/* Metadata are stored on last sector. */
	buf = g_read_data(cp, pp->mediasize - pp->sectorsize, pp->sectorsize,
	    &error);
	g_topology_lock();
	g_access(cp, -1, 0, 0);
	if (buf == NULL) {
		G_MIRROR_DEBUG(1, "Cannot read metadata from %s (error=%d).",
		    cp->provider->name, error);
		return (error);
	}

	/* Decode metadata. */
	error = mirror_metadata_decode(buf, md);
	g_free(buf);
	if (strcmp(md->md_magic, G_MIRROR_MAGIC) != 0)
		return (EINVAL);
	if (md->md_version > G_MIRROR_VERSION) {
		G_MIRROR_DEBUG(0,
		    "Kernel module is too old to handle metadata from %s.",
		    cp->provider->name);
		return (EINVAL);
	}
	if (error != 0) {
		G_MIRROR_DEBUG(1, "MD5 metadata hash mismatch for provider %s.",
		    cp->provider->name);
		return (error);
	}

	return (0);
}

static int
g_mirror_check_metadata(struct g_mirror_softc *sc, struct g_provider *pp,
    struct g_mirror_metadata *md)
{

	if (g_mirror_id2disk(sc, md->md_did) != NULL) {
		G_MIRROR_DEBUG(1, "Disk %s (id=%u) already exists, skipping.",
		    pp->name, md->md_did);
		return (EEXIST);
	}
	if (md->md_all != sc->sc_ndisks) {
		G_MIRROR_DEBUG(1,
		    "Invalid '%s' field on disk %s (device %s), skipping.",
		    "md_all", pp->name, sc->sc_name);
		return (EINVAL);
	}
	if (md->md_slice != sc->sc_slice) {
		G_MIRROR_DEBUG(1,
		    "Invalid '%s' field on disk %s (device %s), skipping.",
		    "md_slice", pp->name, sc->sc_name);
		return (EINVAL);
	}
	if (md->md_balance != sc->sc_balance) {
		G_MIRROR_DEBUG(1,
		    "Invalid '%s' field on disk %s (device %s), skipping.",
		    "md_balance", pp->name, sc->sc_name);
		return (EINVAL);
	}
	if (md->md_mediasize != sc->sc_mediasize) {
		G_MIRROR_DEBUG(1,
		    "Invalid '%s' field on disk %s (device %s), skipping.",
		    "md_mediasize", pp->name, sc->sc_name);
		return (EINVAL);
	}
	if (sc->sc_mediasize > pp->mediasize) {
		G_MIRROR_DEBUG(1,
		    "Invalid size of disk %s (device %s), skipping.", pp->name,
		    sc->sc_name);
		return (EINVAL);
	}
	if (md->md_sectorsize != sc->sc_sectorsize) {
		G_MIRROR_DEBUG(1,
		    "Invalid '%s' field on disk %s (device %s), skipping.",
		    "md_sectorsize", pp->name, sc->sc_name);
		return (EINVAL);
	}
	if ((sc->sc_sectorsize % pp->sectorsize) != 0) {
		G_MIRROR_DEBUG(1,
		    "Invalid sector size of disk %s (device %s), skipping.",
		    pp->name, sc->sc_name);
		return (EINVAL);
	}
	if ((md->md_mflags & ~G_MIRROR_DEVICE_FLAG_MASK) != 0) {
		G_MIRROR_DEBUG(1,
		    "Invalid device flags on disk %s (device %s), skipping.",
		    pp->name, sc->sc_name);
		return (EINVAL);
	}
	if ((md->md_dflags & ~G_MIRROR_DISK_FLAG_MASK) != 0) {
		G_MIRROR_DEBUG(1,
		    "Invalid disk flags on disk %s (device %s), skipping.",
		    pp->name, sc->sc_name);
		return (EINVAL);
	}
	return (0);
}

int
g_mirror_add_disk(struct g_mirror_softc *sc, struct g_provider *pp,
    struct g_mirror_metadata *md)
{
	struct g_mirror_disk *disk;
	int error;

	g_topology_assert_not();
	G_MIRROR_DEBUG(2, "Adding disk %s.", pp->name);

	error = g_mirror_check_metadata(sc, pp, md);
	if (error != 0)
		return (error);
	if (sc->sc_state == G_MIRROR_DEVICE_STATE_RUNNING &&
	    md->md_genid < sc->sc_genid) {
		G_MIRROR_DEBUG(0, "Component %s (device %s) broken, skipping.",
		    pp->name, sc->sc_name);
		return (EINVAL);
	}
	disk = g_mirror_init_disk(sc, pp, md, &error);
	if (disk == NULL)
		return (error);
	error = g_mirror_event_send(disk, G_MIRROR_DISK_STATE_NEW,
	    G_MIRROR_EVENT_WAIT);
	if (error != 0)
		return (error);
	if (md->md_version < G_MIRROR_VERSION) {
		G_MIRROR_DEBUG(0, "Upgrading metadata on %s (v%d->v%d).",
		    pp->name, md->md_version, G_MIRROR_VERSION);
		g_mirror_update_metadata(disk);
	}
	return (0);
}

static void
g_mirror_destroy_delayed(void *arg, int flag)
{
	struct g_mirror_softc *sc;
	int error;

	if (flag == EV_CANCEL) {
		G_MIRROR_DEBUG(1, "Destroying canceled.");
		return;
	}
	sc = arg;
	g_topology_unlock();
	sx_xlock(&sc->sc_lock);
	KASSERT((sc->sc_flags & G_MIRROR_DEVICE_FLAG_DESTROY) == 0,
	    ("DESTROY flag set on %s.", sc->sc_name));
	KASSERT((sc->sc_flags & G_MIRROR_DEVICE_FLAG_DESTROYING) != 0,
	    ("DESTROYING flag not set on %s.", sc->sc_name));
	G_MIRROR_DEBUG(1, "Destroying %s (delayed).", sc->sc_name);
	error = g_mirror_destroy(sc, G_MIRROR_DESTROY_SOFT);
	if (error != 0) {
		G_MIRROR_DEBUG(0, "Cannot destroy %s.", sc->sc_name);
		sx_xunlock(&sc->sc_lock);
	}
	g_topology_lock();
}

static int
g_mirror_access(struct g_provider *pp, int acr, int acw, int ace)
{
	struct g_mirror_softc *sc;
	int dcr, dcw, dce, error = 0;

	g_topology_assert();
	G_MIRROR_DEBUG(2, "Access request for %s: r%dw%de%d.", pp->name, acr,
	    acw, ace);

	sc = pp->geom->softc;
	if (sc == NULL && acr <= 0 && acw <= 0 && ace <= 0)
		return (0);
	KASSERT(sc != NULL, ("NULL softc (provider=%s).", pp->name));

	dcr = pp->acr + acr;
	dcw = pp->acw + acw;
	dce = pp->ace + ace;

	g_topology_unlock();
	sx_xlock(&sc->sc_lock);
	if ((sc->sc_flags & G_MIRROR_DEVICE_FLAG_DESTROY) != 0 ||
	    LIST_EMPTY(&sc->sc_disks)) {
		if (acr > 0 || acw > 0 || ace > 0)
			error = ENXIO;
		goto end;
	}
	if (dcw == 0 && !sc->sc_idle)
		g_mirror_idle(sc, dcw);
	if ((sc->sc_flags & G_MIRROR_DEVICE_FLAG_DESTROYING) != 0) {
		if (acr > 0 || acw > 0 || ace > 0) {
			error = ENXIO;
			goto end;
		}
		if (dcr == 0 && dcw == 0 && dce == 0) {
			g_post_event(g_mirror_destroy_delayed, sc, M_WAITOK,
			    sc, NULL);
		}
	}
end:
	sx_xunlock(&sc->sc_lock);
	g_topology_lock();
	return (error);
}

static struct g_geom *
g_mirror_create(struct g_class *mp, const struct g_mirror_metadata *md)
{
	struct g_mirror_softc *sc;
	struct g_geom *gp;
	int error, timeout;

	g_topology_assert();
	G_MIRROR_DEBUG(1, "Creating device %s (id=%u).", md->md_name,
	    md->md_mid);

	/* One disk is minimum. */
	if (md->md_all < 1)
		return (NULL);
	/*
	 * Action geom.
	 */
	gp = g_new_geomf(mp, "%s", md->md_name);
	sc = malloc(sizeof(*sc), M_MIRROR, M_WAITOK | M_ZERO);
	gp->start = g_mirror_start;
	gp->orphan = g_mirror_orphan;
	gp->access = g_mirror_access;
	gp->dumpconf = g_mirror_dumpconf;

	sc->sc_id = md->md_mid;
	sc->sc_slice = md->md_slice;
	sc->sc_balance = md->md_balance;
	sc->sc_mediasize = md->md_mediasize;
	sc->sc_sectorsize = md->md_sectorsize;
	sc->sc_ndisks = md->md_all;
	sc->sc_flags = md->md_mflags;
	sc->sc_bump_id = 0;
	sc->sc_idle = 1;
	sc->sc_last_write = time_uptime;
	sc->sc_writes = 0;
	sx_init(&sc->sc_lock, "gmirror:lock");
	bioq_init(&sc->sc_queue);
	mtx_init(&sc->sc_queue_mtx, "gmirror:queue", NULL, MTX_DEF);
	bioq_init(&sc->sc_regular_delayed);
	bioq_init(&sc->sc_inflight);
	bioq_init(&sc->sc_sync_delayed);
	LIST_INIT(&sc->sc_disks);
	TAILQ_INIT(&sc->sc_events);
	mtx_init(&sc->sc_events_mtx, "gmirror:events", NULL, MTX_DEF);
	callout_init(&sc->sc_callout, CALLOUT_MPSAFE);
	sc->sc_state = G_MIRROR_DEVICE_STATE_STARTING;
	gp->softc = sc;
	sc->sc_geom = gp;
	sc->sc_provider = NULL;
	/*
	 * Synchronization geom.
	 */
	gp = g_new_geomf(mp, "%s.sync", md->md_name);
	gp->softc = sc;
	gp->orphan = g_mirror_orphan;
	sc->sc_sync.ds_geom = gp;
	sc->sc_sync.ds_ndisks = 0;
	error = kproc_create(g_mirror_worker, sc, &sc->sc_worker, 0, 0,
	    "g_mirror %s", md->md_name);
	if (error != 0) {
		G_MIRROR_DEBUG(1, "Cannot create kernel thread for %s.",
		    sc->sc_name);
		g_destroy_geom(sc->sc_sync.ds_geom);
		mtx_destroy(&sc->sc_events_mtx);
		mtx_destroy(&sc->sc_queue_mtx);
		sx_destroy(&sc->sc_lock);
		g_destroy_geom(sc->sc_geom);
		free(sc, M_MIRROR);
		return (NULL);
	}

	G_MIRROR_DEBUG(1, "Device %s created (%u components, id=%u).",
	    sc->sc_name, sc->sc_ndisks, sc->sc_id);

	sc->sc_rootmount = root_mount_hold("GMIRROR");
	G_MIRROR_DEBUG(1, "root_mount_hold %p", sc->sc_rootmount);
	/*
	 * Run timeout.
	 */
	timeout = g_mirror_timeout * hz;
	callout_reset(&sc->sc_callout, timeout, g_mirror_go, sc);
	return (sc->sc_geom);
}

int
g_mirror_destroy(struct g_mirror_softc *sc, int how)
{
	struct g_mirror_disk *disk;
	struct g_provider *pp;

	g_topology_assert_not();
	if (sc == NULL)
		return (ENXIO);
	sx_assert(&sc->sc_lock, SX_XLOCKED);

	pp = sc->sc_provider;
	if (pp != NULL && (pp->acr != 0 || pp->acw != 0 || pp->ace != 0)) {
		switch (how) {
		case G_MIRROR_DESTROY_SOFT:
			G_MIRROR_DEBUG(1,
			    "Device %s is still open (r%dw%de%d).", pp->name,
			    pp->acr, pp->acw, pp->ace);
			return (EBUSY);
		case G_MIRROR_DESTROY_DELAYED:
			G_MIRROR_DEBUG(1,
			    "Device %s will be destroyed on last close.",
			    pp->name);
			LIST_FOREACH(disk, &sc->sc_disks, d_next) {
				if (disk->d_state ==
				    G_MIRROR_DISK_STATE_SYNCHRONIZING) {
					g_mirror_sync_stop(disk, 1);
				}
			}
			sc->sc_flags |= G_MIRROR_DEVICE_FLAG_DESTROYING;
			return (EBUSY);
		case G_MIRROR_DESTROY_HARD:
			G_MIRROR_DEBUG(1, "Device %s is still open, so it "
			    "can't be definitely removed.", pp->name);
		}
	}

	g_topology_lock();
	if (sc->sc_geom->softc == NULL) {
		g_topology_unlock();
		return (0);
	}
	sc->sc_geom->softc = NULL;
	sc->sc_sync.ds_geom->softc = NULL;
	g_topology_unlock();

	sc->sc_flags |= G_MIRROR_DEVICE_FLAG_DESTROY;
	sc->sc_flags |= G_MIRROR_DEVICE_FLAG_WAIT;
	G_MIRROR_DEBUG(4, "%s: Waking up %p.", __func__, sc);
	sx_xunlock(&sc->sc_lock);
	mtx_lock(&sc->sc_queue_mtx);
	wakeup(sc);
	mtx_unlock(&sc->sc_queue_mtx);
	G_MIRROR_DEBUG(4, "%s: Sleeping %p.", __func__, &sc->sc_worker);
	while (sc->sc_worker != NULL)
		tsleep(&sc->sc_worker, PRIBIO, "m:destroy", hz / 5);
	G_MIRROR_DEBUG(4, "%s: Woken up %p.", __func__, &sc->sc_worker);
	sx_xlock(&sc->sc_lock);
	g_mirror_destroy_device(sc);
	free(sc, M_MIRROR);
	return (0);
}

static void
g_mirror_taste_orphan(struct g_consumer *cp)
{

	KASSERT(1 == 0, ("%s called while tasting %s.", __func__,
	    cp->provider->name));
}

static struct g_geom *
g_mirror_taste(struct g_class *mp, struct g_provider *pp, int flags __unused)
{
	struct g_mirror_metadata md;
	struct g_mirror_softc *sc;
	struct g_consumer *cp;
	struct g_geom *gp;
	int error;

	g_topology_assert();
	g_trace(G_T_TOPOLOGY, "%s(%s, %s)", __func__, mp->name, pp->name);
	G_MIRROR_DEBUG(2, "Tasting %s.", pp->name);

	gp = g_new_geomf(mp, "mirror:taste");
	/*
	 * This orphan function should be never called.
	 */
	gp->orphan = g_mirror_taste_orphan;
	cp = g_new_consumer(gp);
	g_attach(cp, pp);
	error = g_mirror_read_metadata(cp, &md);
	g_detach(cp);
	g_destroy_consumer(cp);
	g_destroy_geom(gp);
	if (error != 0)
		return (NULL);
	gp = NULL;

	if (md.md_provider[0] != '\0' &&
	    !g_compare_names(md.md_provider, pp->name))
		return (NULL);
	if (md.md_provsize != 0 && md.md_provsize != pp->mediasize)
		return (NULL);
	if ((md.md_dflags & G_MIRROR_DISK_FLAG_INACTIVE) != 0) {
		G_MIRROR_DEBUG(0,
		    "Device %s: provider %s marked as inactive, skipping.",
		    md.md_name, pp->name);
		return (NULL);
	}
	if (g_mirror_debug >= 2)
		mirror_metadata_dump(&md);

	/*
	 * Let's check if device already exists.
	 */
	sc = NULL;
	LIST_FOREACH(gp, &mp->geom, geom) {
		sc = gp->softc;
		if (sc == NULL)
			continue;
		if (sc->sc_sync.ds_geom == gp)
			continue;
		if (strcmp(md.md_name, sc->sc_name) != 0)
			continue;
		if (md.md_mid != sc->sc_id) {
			G_MIRROR_DEBUG(0, "Device %s already configured.",
			    sc->sc_name);
			return (NULL);
		}
		break;
	}
	if (gp == NULL) {
		gp = g_mirror_create(mp, &md);
		if (gp == NULL) {
			G_MIRROR_DEBUG(0, "Cannot create device %s.",
			    md.md_name);
			return (NULL);
		}
		sc = gp->softc;
	}
	G_MIRROR_DEBUG(1, "Adding disk %s to %s.", pp->name, gp->name);
	g_topology_unlock();
	sx_xlock(&sc->sc_lock);
	error = g_mirror_add_disk(sc, pp, &md);
	if (error != 0) {
		G_MIRROR_DEBUG(0, "Cannot add disk %s to %s (error=%d).",
		    pp->name, gp->name, error);
		if (LIST_EMPTY(&sc->sc_disks)) {
			g_cancel_event(sc);
			g_mirror_destroy(sc, G_MIRROR_DESTROY_HARD);
			g_topology_lock();
			return (NULL);
		}
		gp = NULL;
	}
	sx_xunlock(&sc->sc_lock);
	g_topology_lock();
	return (gp);
}

static int
g_mirror_destroy_geom(struct gctl_req *req __unused,
    struct g_class *mp __unused, struct g_geom *gp)
{
	struct g_mirror_softc *sc;
	int error;

	g_topology_unlock();
	sc = gp->softc;
	sx_xlock(&sc->sc_lock);
	g_cancel_event(sc);
	error = g_mirror_destroy(gp->softc, G_MIRROR_DESTROY_SOFT);
	if (error != 0)
		sx_xunlock(&sc->sc_lock);
	g_topology_lock();
	return (error);
}

static void
g_mirror_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp,
    struct g_consumer *cp, struct g_provider *pp)
{
	struct g_mirror_softc *sc;

	g_topology_assert();

	sc = gp->softc;
	if (sc == NULL)
		return;
	/* Skip synchronization geom. */
	if (gp == sc->sc_sync.ds_geom)
		return;
	if (pp != NULL) {
		/* Nothing here. */
	} else if (cp != NULL) {
		struct g_mirror_disk *disk;

		disk = cp->private;
		if (disk == NULL)
			return;
		g_topology_unlock();
		sx_xlock(&sc->sc_lock);
		sbuf_printf(sb, "%s<ID>%u</ID>\n", indent, (u_int)disk->d_id);
		if (disk->d_state == G_MIRROR_DISK_STATE_SYNCHRONIZING) {
			sbuf_printf(sb, "%s<Synchronized>", indent);
			if (disk->d_sync.ds_offset == 0)
				sbuf_printf(sb, "0%%");
			else {
				sbuf_printf(sb, "%u%%",
				    (u_int)((disk->d_sync.ds_offset * 100) /
				    sc->sc_provider->mediasize));
			}
			sbuf_printf(sb, "</Synchronized>\n");
		}
		sbuf_printf(sb, "%s<SyncID>%u</SyncID>\n", indent,
		    disk->d_sync.ds_syncid);
		sbuf_printf(sb, "%s<GenID>%u</GenID>\n", indent,
		    disk->d_genid);
		sbuf_printf(sb, "%s<Flags>", indent);
		if (disk->d_flags == 0)
			sbuf_printf(sb, "NONE");
		else {
			int first = 1;

#define	ADD_FLAG(flag, name)	do {					\
	if ((disk->d_flags & (flag)) != 0) {				\
		if (!first)						\
			sbuf_printf(sb, ", ");				\
		else							\
			first = 0;					\
		sbuf_printf(sb, name);					\
	}								\
} while (0)
			ADD_FLAG(G_MIRROR_DISK_FLAG_DIRTY, "DIRTY");
			ADD_FLAG(G_MIRROR_DISK_FLAG_HARDCODED, "HARDCODED");
			ADD_FLAG(G_MIRROR_DISK_FLAG_INACTIVE, "INACTIVE");
			ADD_FLAG(G_MIRROR_DISK_FLAG_SYNCHRONIZING,
			    "SYNCHRONIZING");
			ADD_FLAG(G_MIRROR_DISK_FLAG_FORCE_SYNC, "FORCE_SYNC");
			ADD_FLAG(G_MIRROR_DISK_FLAG_BROKEN, "BROKEN");
#undef	ADD_FLAG
		}
		sbuf_printf(sb, "</Flags>\n");
		sbuf_printf(sb, "%s<Priority>%u</Priority>\n", indent,
		    disk->d_priority);
		sbuf_printf(sb, "%s<State>%s</State>\n", indent,
		    g_mirror_disk_state2str(disk->d_state));
		sx_xunlock(&sc->sc_lock);
		g_topology_lock();
	} else {
		g_topology_unlock();
		sx_xlock(&sc->sc_lock);
		sbuf_printf(sb, "%s<ID>%u</ID>\n", indent, (u_int)sc->sc_id);
		sbuf_printf(sb, "%s<SyncID>%u</SyncID>\n", indent, sc->sc_syncid);
		sbuf_printf(sb, "%s<GenID>%u</GenID>\n", indent, sc->sc_genid);
		sbuf_printf(sb, "%s<Flags>", indent);
		if (sc->sc_flags == 0)
			sbuf_printf(sb, "NONE");
		else {
			int first = 1;

#define	ADD_FLAG(flag, name)	do {					\
	if ((sc->sc_flags & (flag)) != 0) {				\
		if (!first)						\
			sbuf_printf(sb, ", ");				\
		else							\
			first = 0;					\
		sbuf_printf(sb, name);					\
	}								\
} while (0)
			ADD_FLAG(G_MIRROR_DEVICE_FLAG_NOFAILSYNC, "NOFAILSYNC");
			ADD_FLAG(G_MIRROR_DEVICE_FLAG_NOAUTOSYNC, "NOAUTOSYNC");
#undef	ADD_FLAG
		}
		sbuf_printf(sb, "</Flags>\n");
		sbuf_printf(sb, "%s<Slice>%u</Slice>\n", indent,
		    (u_int)sc->sc_slice);
		sbuf_printf(sb, "%s<Balance>%s</Balance>\n", indent,
		    balance_name(sc->sc_balance));
		sbuf_printf(sb, "%s<Components>%u</Components>\n", indent,
		    sc->sc_ndisks);
		sbuf_printf(sb, "%s<State>", indent);
		if (sc->sc_state == G_MIRROR_DEVICE_STATE_STARTING)
			sbuf_printf(sb, "%s", "STARTING");
		else if (sc->sc_ndisks ==
		    g_mirror_ndisks(sc, G_MIRROR_DISK_STATE_ACTIVE))
			sbuf_printf(sb, "%s", "COMPLETE");
		else
			sbuf_printf(sb, "%s", "DEGRADED");
		sbuf_printf(sb, "</State>\n");
		sx_xunlock(&sc->sc_lock);
		g_topology_lock();
	}
}

static void
g_mirror_shutdown_pre_sync(void *arg, int howto)
{
	struct g_class *mp;
	struct g_geom *gp, *gp2;
	struct g_mirror_softc *sc;
	int error;

	mp = arg;
	DROP_GIANT();
	g_topology_lock();
	LIST_FOREACH_SAFE(gp, &mp->geom, geom, gp2) {
		if ((sc = gp->softc) == NULL)
			continue;
		/* Skip synchronization geom. */
		if (gp == sc->sc_sync.ds_geom)
			continue;
		g_topology_unlock();
		sx_xlock(&sc->sc_lock);
		g_cancel_event(sc);
		error = g_mirror_destroy(sc, G_MIRROR_DESTROY_DELAYED);
		if (error != 0)
			sx_xunlock(&sc->sc_lock);
		g_topology_lock();
	}
	g_topology_unlock();
	PICKUP_GIANT();
}

static void
g_mirror_init(struct g_class *mp)
{

	g_mirror_pre_sync = EVENTHANDLER_REGISTER(shutdown_pre_sync,
	    g_mirror_shutdown_pre_sync, mp, SHUTDOWN_PRI_FIRST);
	if (g_mirror_pre_sync == NULL)
		G_MIRROR_DEBUG(0, "Warning! Cannot register shutdown event.");
}

static void
g_mirror_fini(struct g_class *mp)
{

	if (g_mirror_pre_sync != NULL)
		EVENTHANDLER_DEREGISTER(shutdown_pre_sync, g_mirror_pre_sync);
}

DECLARE_GEOM_CLASS(g_mirror_class, g_mirror);
