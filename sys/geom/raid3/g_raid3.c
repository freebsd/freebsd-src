/*-
 * Copyright (c) 2004-2005 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
#include <geom/raid3/g_raid3.h>


static MALLOC_DEFINE(M_RAID3, "raid3_data", "GEOM_RAID3 Data");

SYSCTL_DECL(_kern_geom);
SYSCTL_NODE(_kern_geom, OID_AUTO, raid3, CTLFLAG_RW, 0, "GEOM_RAID3 stuff");
u_int g_raid3_debug = 0;
TUNABLE_INT("kern.geom.raid3.debug", &g_raid3_debug);
SYSCTL_UINT(_kern_geom_raid3, OID_AUTO, debug, CTLFLAG_RW, &g_raid3_debug, 0,
    "Debug level");
static u_int g_raid3_timeout = 4;
TUNABLE_INT("kern.geom.raid3.timeout", &g_raid3_timeout);
SYSCTL_UINT(_kern_geom_raid3, OID_AUTO, timeout, CTLFLAG_RW, &g_raid3_timeout,
    0, "Time to wait on all raid3 components");
static u_int g_raid3_idletime = 5;
TUNABLE_INT("kern.geom.raid3.idletime", &g_raid3_idletime);
SYSCTL_UINT(_kern_geom_raid3, OID_AUTO, idletime, CTLFLAG_RW,
    &g_raid3_idletime, 0, "Mark components as clean when idling");
static u_int g_raid3_reqs_per_sync = 5;
SYSCTL_UINT(_kern_geom_raid3, OID_AUTO, reqs_per_sync, CTLFLAG_RW,
    &g_raid3_reqs_per_sync, 0,
    "Number of regular I/O requests per synchronization request");
static u_int g_raid3_syncs_per_sec = 1000;
SYSCTL_UINT(_kern_geom_raid3, OID_AUTO, syncs_per_sec, CTLFLAG_RW,
    &g_raid3_syncs_per_sec, 0,
    "Number of synchronizations requests per second");

static u_int g_raid3_n64k = 50;
TUNABLE_INT("kern.geom.raid3.n64k", &g_raid3_n64k);
SYSCTL_UINT(_kern_geom_raid3, OID_AUTO, n64k, CTLFLAG_RD, &g_raid3_n64k, 0,
    "Maximum number of 64kB allocations");
static u_int g_raid3_n16k = 200;
TUNABLE_INT("kern.geom.raid3.n16k", &g_raid3_n16k);
SYSCTL_UINT(_kern_geom_raid3, OID_AUTO, n16k, CTLFLAG_RD, &g_raid3_n16k, 0,
    "Maximum number of 16kB allocations");
static u_int g_raid3_n4k = 1200;
TUNABLE_INT("kern.geom.raid3.n4k", &g_raid3_n4k);
SYSCTL_UINT(_kern_geom_raid3, OID_AUTO, n4k, CTLFLAG_RD, &g_raid3_n4k, 0,
    "Maximum number of 4kB allocations");

SYSCTL_NODE(_kern_geom_raid3, OID_AUTO, stat, CTLFLAG_RW, 0,
    "GEOM_RAID3 statistics");
static u_int g_raid3_parity_mismatch = 0;
SYSCTL_UINT(_kern_geom_raid3_stat, OID_AUTO, parity_mismatch, CTLFLAG_RD,
    &g_raid3_parity_mismatch, 0, "Number of failures in VERIFY mode");
static u_int g_raid3_64k_requested = 0;
SYSCTL_UINT(_kern_geom_raid3_stat, OID_AUTO, 64k_requested, CTLFLAG_RD,
    &g_raid3_64k_requested, 0, "Number of requested 64kB allocations");
static u_int g_raid3_64k_failed = 0;
SYSCTL_UINT(_kern_geom_raid3_stat, OID_AUTO, 64k_failed, CTLFLAG_RD,
    &g_raid3_64k_failed, 0, "Number of failed 64kB allocations");
static u_int g_raid3_16k_requested = 0;
SYSCTL_UINT(_kern_geom_raid3_stat, OID_AUTO, 16k_requested, CTLFLAG_RD,
    &g_raid3_16k_requested, 0, "Number of requested 16kB allocations");
static u_int g_raid3_16k_failed = 0;
SYSCTL_UINT(_kern_geom_raid3_stat, OID_AUTO, 16k_failed, CTLFLAG_RD,
    &g_raid3_16k_failed, 0, "Number of failed 16kB allocations");
static u_int g_raid3_4k_requested = 0;
SYSCTL_UINT(_kern_geom_raid3_stat, OID_AUTO, 4k_requested, CTLFLAG_RD,
    &g_raid3_4k_requested, 0, "Number of requested 4kB allocations");
static u_int g_raid3_4k_failed = 0;
SYSCTL_UINT(_kern_geom_raid3_stat, OID_AUTO, 4k_failed, CTLFLAG_RD,
    &g_raid3_4k_failed, 0, "Number of failed 4kB allocations");

#define	MSLEEP(ident, mtx, priority, wmesg, timeout)	do {		\
	G_RAID3_DEBUG(4, "%s: Sleeping %p.", __func__, (ident));	\
	msleep((ident), (mtx), (priority), (wmesg), (timeout));		\
	G_RAID3_DEBUG(4, "%s: Woken up %p.", __func__, (ident));	\
} while (0)

static eventhandler_tag g_raid3_ehtag = NULL;

static int g_raid3_destroy_geom(struct gctl_req *req, struct g_class *mp,
    struct g_geom *gp);
static g_taste_t g_raid3_taste;
static void g_raid3_init(struct g_class *mp);
static void g_raid3_fini(struct g_class *mp);

struct g_class g_raid3_class = {
	.name = G_RAID3_CLASS_NAME,
	.version = G_VERSION,
	.ctlreq = g_raid3_config,
	.taste = g_raid3_taste,
	.destroy_geom = g_raid3_destroy_geom,
	.init = g_raid3_init,
	.fini = g_raid3_fini
};


static void g_raid3_destroy_provider(struct g_raid3_softc *sc);
static int g_raid3_update_disk(struct g_raid3_disk *disk, u_int state);
static void g_raid3_update_device(struct g_raid3_softc *sc, boolean_t force);
static void g_raid3_dumpconf(struct sbuf *sb, const char *indent,
    struct g_geom *gp, struct g_consumer *cp, struct g_provider *pp);
static void g_raid3_sync_stop(struct g_raid3_softc *sc, int type);


static const char *
g_raid3_disk_state2str(int state)
{

	switch (state) {
	case G_RAID3_DISK_STATE_NODISK:
		return ("NODISK");
	case G_RAID3_DISK_STATE_NONE:
		return ("NONE");
	case G_RAID3_DISK_STATE_NEW:
		return ("NEW");
	case G_RAID3_DISK_STATE_ACTIVE:
		return ("ACTIVE");
	case G_RAID3_DISK_STATE_STALE:
		return ("STALE");
	case G_RAID3_DISK_STATE_SYNCHRONIZING:
		return ("SYNCHRONIZING");
	case G_RAID3_DISK_STATE_DISCONNECTED:
		return ("DISCONNECTED");
	default:
		return ("INVALID");
	}
}

static const char *
g_raid3_device_state2str(int state)
{

	switch (state) {
	case G_RAID3_DEVICE_STATE_STARTING:
		return ("STARTING");
	case G_RAID3_DEVICE_STATE_DEGRADED:
		return ("DEGRADED");
	case G_RAID3_DEVICE_STATE_COMPLETE:
		return ("COMPLETE");
	default:
		return ("INVALID");
	}
}

const char *
g_raid3_get_diskname(struct g_raid3_disk *disk)
{

	if (disk->d_consumer == NULL || disk->d_consumer->provider == NULL)
		return ("[unknown]");
	return (disk->d_name);
}

#define	g_raid3_xor(src1, src2, dst, size)				\
	_g_raid3_xor((uint64_t *)(src1), (uint64_t *)(src2),		\
	    (uint64_t *)(dst), (size_t)size)
static void
_g_raid3_xor(uint64_t *src1, uint64_t *src2, uint64_t *dst, size_t size)
{

	KASSERT((size % 128) == 0, ("Invalid size: %zu.", size));
	for (; size > 0; size -= 128) {
		*dst++ = (*src1++) ^ (*src2++);
		*dst++ = (*src1++) ^ (*src2++);
		*dst++ = (*src1++) ^ (*src2++);
		*dst++ = (*src1++) ^ (*src2++);
		*dst++ = (*src1++) ^ (*src2++);
		*dst++ = (*src1++) ^ (*src2++);
		*dst++ = (*src1++) ^ (*src2++);
		*dst++ = (*src1++) ^ (*src2++);
		*dst++ = (*src1++) ^ (*src2++);
		*dst++ = (*src1++) ^ (*src2++);
		*dst++ = (*src1++) ^ (*src2++);
		*dst++ = (*src1++) ^ (*src2++);
		*dst++ = (*src1++) ^ (*src2++);
		*dst++ = (*src1++) ^ (*src2++);
		*dst++ = (*src1++) ^ (*src2++);
		*dst++ = (*src1++) ^ (*src2++);
	}
}

static int
g_raid3_is_zero(struct bio *bp)
{
	static const uint64_t zeros[] = {
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	u_char *addr;
	ssize_t size;

	size = bp->bio_length;
	addr = (u_char *)bp->bio_data;
	for (; size > 0; size -= sizeof(zeros), addr += sizeof(zeros)) {
		if (bcmp(addr, zeros, sizeof(zeros)) != 0)
			return (0);
	}
	return (1);
}

/*
 * --- Events handling functions ---
 * Events in geom_raid3 are used to maintain disks and device status
 * from one thread to simplify locking.
 */
static void
g_raid3_event_free(struct g_raid3_event *ep)
{

	free(ep, M_RAID3);
}

int
g_raid3_event_send(void *arg, int state, int flags)
{
	struct g_raid3_softc *sc;
	struct g_raid3_disk *disk;
	struct g_raid3_event *ep;
	int error;

	ep = malloc(sizeof(*ep), M_RAID3, M_WAITOK);
	G_RAID3_DEBUG(4, "%s: Sending event %p.", __func__, ep);
	if ((flags & G_RAID3_EVENT_DEVICE) != 0) {
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
	G_RAID3_DEBUG(4, "%s: Waking up %p.", __func__, sc);
	mtx_lock(&sc->sc_queue_mtx);
	wakeup(sc);
	wakeup(&sc->sc_queue);
	mtx_unlock(&sc->sc_queue_mtx);
	if ((flags & G_RAID3_EVENT_DONTWAIT) != 0)
		return (0);
	g_topology_assert();
	G_RAID3_DEBUG(4, "%s: Sleeping %p.", __func__, ep);
	g_topology_unlock();
	while ((ep->e_flags & G_RAID3_EVENT_DONE) == 0) {
		mtx_lock(&sc->sc_events_mtx);
		MSLEEP(ep, &sc->sc_events_mtx, PRIBIO | PDROP, "r3:event",
		    hz * 5);
	}
	/* Don't even try to use 'sc' here, because it could be already dead. */
	g_topology_lock();
	error = ep->e_error;
	g_raid3_event_free(ep);
	return (error);
}

static struct g_raid3_event *
g_raid3_event_get(struct g_raid3_softc *sc)
{
	struct g_raid3_event *ep;

	mtx_lock(&sc->sc_events_mtx);
	ep = TAILQ_FIRST(&sc->sc_events);
	mtx_unlock(&sc->sc_events_mtx);
	return (ep);
}

static void
g_raid3_event_remove(struct g_raid3_softc *sc, struct g_raid3_event *ep)
{

	mtx_lock(&sc->sc_events_mtx);
	TAILQ_REMOVE(&sc->sc_events, ep, e_next);
	mtx_unlock(&sc->sc_events_mtx);
}

static void
g_raid3_event_cancel(struct g_raid3_disk *disk)
{
	struct g_raid3_softc *sc;
	struct g_raid3_event *ep, *tmpep;

	g_topology_assert();

	sc = disk->d_softc;
	mtx_lock(&sc->sc_events_mtx);
	TAILQ_FOREACH_SAFE(ep, &sc->sc_events, e_next, tmpep) {
		if ((ep->e_flags & G_RAID3_EVENT_DEVICE) != 0)
			continue;
		if (ep->e_disk != disk)
			continue;
		TAILQ_REMOVE(&sc->sc_events, ep, e_next);
		if ((ep->e_flags & G_RAID3_EVENT_DONTWAIT) != 0)
			g_raid3_event_free(ep);
		else {
			ep->e_error = ECANCELED;
			wakeup(ep);
		}
	}
	mtx_unlock(&sc->sc_events_mtx);
}

/*
 * Return the number of disks in the given state.
 * If state is equal to -1, count all connected disks.
 */
u_int
g_raid3_ndisks(struct g_raid3_softc *sc, int state)
{
	struct g_raid3_disk *disk;
	u_int n, ndisks;

	for (n = ndisks = 0; n < sc->sc_ndisks; n++) {
		disk = &sc->sc_disks[n];
		if (disk->d_state == G_RAID3_DISK_STATE_NODISK)
			continue;
		if (state == -1 || disk->d_state == state)
			ndisks++;
	}
	return (ndisks);
}

static u_int
g_raid3_nrequests(struct g_raid3_softc *sc, struct g_consumer *cp)
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
g_raid3_is_busy(struct g_raid3_softc *sc, struct g_consumer *cp)
{

	if (cp->index > 0) {
		G_RAID3_DEBUG(2,
		    "I/O requests for %s exist, can't destroy it now.",
		    cp->provider->name);
		return (1);
	}
	if (g_raid3_nrequests(sc, cp) > 0) {
		G_RAID3_DEBUG(2,
		    "I/O requests for %s in queue, can't destroy it now.",
		    cp->provider->name);
		return (1);
	}
	return (0);
}

static void
g_raid3_destroy_consumer(void *arg, int flags __unused)
{
	struct g_consumer *cp;

	cp = arg;
	G_RAID3_DEBUG(1, "Consumer %s destroyed.", cp->provider->name);
	g_detach(cp);
	g_destroy_consumer(cp);
}

static void
g_raid3_kill_consumer(struct g_raid3_softc *sc, struct g_consumer *cp)
{
	struct g_provider *pp;
	int retaste_wait;

	g_topology_assert();

	cp->private = NULL;
	if (g_raid3_is_busy(sc, cp))
		return;
	G_RAID3_DEBUG(2, "Consumer %s destroyed.", cp->provider->name);
	pp = cp->provider;
	retaste_wait = 0;
	if (cp->acw == 1) {
		if ((pp->geom->flags & G_GEOM_WITHER) == 0)
			retaste_wait = 1;
	}
	G_RAID3_DEBUG(2, "Access %s r%dw%de%d = %d", pp->name, -cp->acr,
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
		g_post_event(g_raid3_destroy_consumer, cp, M_WAITOK, NULL);
		return;
	}
	G_RAID3_DEBUG(1, "Consumer %s destroyed.", pp->name);
	g_detach(cp);
	g_destroy_consumer(cp);
}

static int
g_raid3_connect_disk(struct g_raid3_disk *disk, struct g_provider *pp)
{
	struct g_consumer *cp;
	int error;

	g_topology_assert();
	KASSERT(disk->d_consumer == NULL,
	    ("Disk already connected (device %s).", disk->d_softc->sc_name));

	cp = g_new_consumer(disk->d_softc->sc_geom);
	error = g_attach(cp, pp);
	if (error != 0) {
		g_destroy_consumer(cp);
		return (error);
	}
	error = g_access(cp, 1, 1, 1);
	if (error != 0) {
		g_detach(cp);
		g_destroy_consumer(cp);
		G_RAID3_DEBUG(0, "Cannot open consumer %s (error=%d).",
		    pp->name, error);
		return (error);
	}
	disk->d_consumer = cp;
	disk->d_consumer->private = disk;
	disk->d_consumer->index = 0;
	G_RAID3_DEBUG(2, "Disk %s connected.", g_raid3_get_diskname(disk));
	return (0);
}

static void
g_raid3_disconnect_consumer(struct g_raid3_softc *sc, struct g_consumer *cp)
{

	g_topology_assert();

	if (cp == NULL)
		return;
	if (cp->provider != NULL)
		g_raid3_kill_consumer(sc, cp);
	else
		g_destroy_consumer(cp);
}

/*
 * Initialize disk. This means allocate memory, create consumer, attach it
 * to the provider and open access (r1w1e1) to it.
 */
static struct g_raid3_disk *
g_raid3_init_disk(struct g_raid3_softc *sc, struct g_provider *pp,
    struct g_raid3_metadata *md, int *errorp)
{
	struct g_raid3_disk *disk;
	int error;

	disk = &sc->sc_disks[md->md_no];
	error = g_raid3_connect_disk(disk, pp);
	if (error != 0) {
		if (errorp != NULL)
			*errorp = error;
		return (NULL);
	}
	disk->d_state = G_RAID3_DISK_STATE_NONE;
	disk->d_flags = md->md_dflags;
	if (md->md_provider[0] != '\0')
		disk->d_flags |= G_RAID3_DISK_FLAG_HARDCODED;
	disk->d_sync.ds_consumer = NULL;
	disk->d_sync.ds_offset = md->md_sync_offset;
	disk->d_sync.ds_offset_done = md->md_sync_offset;
	disk->d_sync.ds_resync = -1;
	disk->d_genid = md->md_genid;
	disk->d_sync.ds_syncid = md->md_syncid;
	if (errorp != NULL)
		*errorp = 0;
	return (disk);
}

static void
g_raid3_destroy_disk(struct g_raid3_disk *disk)
{
	struct g_raid3_softc *sc;

	g_topology_assert();

	if (disk->d_state == G_RAID3_DISK_STATE_NODISK)
		return;
	g_raid3_event_cancel(disk);
	sc = disk->d_softc;
	switch (disk->d_state) {
	case G_RAID3_DISK_STATE_SYNCHRONIZING:
		if (sc->sc_syncdisk != NULL)
			g_raid3_sync_stop(sc, 1);
		/* FALLTHROUGH */
	case G_RAID3_DISK_STATE_NEW:
	case G_RAID3_DISK_STATE_STALE:
	case G_RAID3_DISK_STATE_ACTIVE:
		g_raid3_disconnect_consumer(sc, disk->d_consumer);
		disk->d_consumer = NULL;
		break;
	default:
		KASSERT(0 == 1, ("Wrong disk state (%s, %s).",
		    g_raid3_get_diskname(disk),
		    g_raid3_disk_state2str(disk->d_state)));
	}
	disk->d_state = G_RAID3_DISK_STATE_NODISK;
}

static void
g_raid3_destroy_device(struct g_raid3_softc *sc)
{
	struct g_raid3_event *ep;
	struct g_raid3_disk *disk;
	struct g_geom *gp;
	struct g_consumer *cp;
	u_int n;

	g_topology_assert();

	gp = sc->sc_geom;
	if (sc->sc_provider != NULL)
		g_raid3_destroy_provider(sc);
	for (n = 0; n < sc->sc_ndisks; n++) {
		disk = &sc->sc_disks[n];
		if (disk->d_state != G_RAID3_DISK_STATE_NODISK) {
			disk->d_flags &= ~G_RAID3_DISK_FLAG_DIRTY;
			g_raid3_update_metadata(disk);
			g_raid3_destroy_disk(disk);
		}
	}
	while ((ep = g_raid3_event_get(sc)) != NULL) {
		g_raid3_event_remove(sc, ep);
		if ((ep->e_flags & G_RAID3_EVENT_DONTWAIT) != 0)
			g_raid3_event_free(ep);
		else {
			ep->e_error = ECANCELED;
			ep->e_flags |= G_RAID3_EVENT_DONE;
			G_RAID3_DEBUG(4, "%s: Waking up %p.", __func__, ep);
			mtx_lock(&sc->sc_events_mtx);
			wakeup(ep);
			mtx_unlock(&sc->sc_events_mtx);
		}
	}
	callout_drain(&sc->sc_callout);
	gp->softc = NULL;
	cp = LIST_FIRST(&sc->sc_sync.ds_geom->consumer);
	if (cp != NULL)
		g_raid3_disconnect_consumer(sc, cp);
	sc->sc_sync.ds_geom->softc = NULL;
	g_wither_geom(sc->sc_sync.ds_geom, ENXIO);
	uma_zdestroy(sc->sc_zone_64k); 
	uma_zdestroy(sc->sc_zone_16k); 
	uma_zdestroy(sc->sc_zone_4k); 
	mtx_destroy(&sc->sc_queue_mtx);
	mtx_destroy(&sc->sc_events_mtx);
	G_RAID3_DEBUG(0, "Device %s destroyed.", gp->name);
	g_wither_geom(gp, ENXIO);
}

static void
g_raid3_orphan(struct g_consumer *cp)
{
	struct g_raid3_disk *disk;

	g_topology_assert();

	disk = cp->private;
	if (disk == NULL)
		return;
	disk->d_softc->sc_bump_id = G_RAID3_BUMP_SYNCID;
	g_raid3_event_send(disk, G_RAID3_DISK_STATE_DISCONNECTED,
	    G_RAID3_EVENT_DONTWAIT);
}

static int
g_raid3_write_metadata(struct g_raid3_disk *disk, struct g_raid3_metadata *md)
{
	struct g_raid3_softc *sc;
	struct g_consumer *cp;
	off_t offset, length;
	u_char *sector;
	int error = 0;

	g_topology_assert();

	sc = disk->d_softc;
	cp = disk->d_consumer;
	KASSERT(cp != NULL, ("NULL consumer (%s).", sc->sc_name));
	KASSERT(cp->provider != NULL, ("NULL provider (%s).", sc->sc_name));
	KASSERT(cp->acr == 1 && cp->acw == 1 && cp->ace == 1,
	    ("Consumer %s closed? (r%dw%de%d).", cp->provider->name, cp->acr,
	    cp->acw, cp->ace));
	length = cp->provider->sectorsize;
	offset = cp->provider->mediasize - length;
	sector = malloc((size_t)length, M_RAID3, M_WAITOK | M_ZERO);
	if (md != NULL)
		raid3_metadata_encode(md, sector);
	g_topology_unlock();
	error = g_write_data(cp, offset, sector, length);
	g_topology_lock();
	free(sector, M_RAID3);
	if (error != 0) {
		disk->d_softc->sc_bump_id = G_RAID3_BUMP_GENID;
		g_raid3_event_send(disk, G_RAID3_DISK_STATE_DISCONNECTED,
		    G_RAID3_EVENT_DONTWAIT);
	}
	return (error);
}

int
g_raid3_clear_metadata(struct g_raid3_disk *disk)
{
	int error;

	g_topology_assert();
	error = g_raid3_write_metadata(disk, NULL);
	if (error == 0) {
		G_RAID3_DEBUG(2, "Metadata on %s cleared.",
		    g_raid3_get_diskname(disk));
	} else {
		G_RAID3_DEBUG(0,
		    "Cannot clear metadata on disk %s (error=%d).",
		    g_raid3_get_diskname(disk), error);
	}
	return (error);
}

void
g_raid3_fill_metadata(struct g_raid3_disk *disk, struct g_raid3_metadata *md)
{
	struct g_raid3_softc *sc;
	struct g_provider *pp;

	sc = disk->d_softc;
	strlcpy(md->md_magic, G_RAID3_MAGIC, sizeof(md->md_magic));
	md->md_version = G_RAID3_VERSION;
	strlcpy(md->md_name, sc->sc_name, sizeof(md->md_name));
	md->md_id = sc->sc_id;
	md->md_all = sc->sc_ndisks;
	md->md_genid = sc->sc_genid;
	md->md_mediasize = sc->sc_mediasize;
	md->md_sectorsize = sc->sc_sectorsize;
	md->md_mflags = (sc->sc_flags & G_RAID3_DEVICE_FLAG_MASK);
	md->md_no = disk->d_no;
	md->md_syncid = disk->d_sync.ds_syncid;
	md->md_dflags = (disk->d_flags & G_RAID3_DISK_FLAG_MASK);
	if (disk->d_state == G_RAID3_DISK_STATE_SYNCHRONIZING)
		md->md_sync_offset = disk->d_sync.ds_offset_done;
	else
		md->md_sync_offset = 0;
	if (disk->d_consumer != NULL && disk->d_consumer->provider != NULL)
		pp = disk->d_consumer->provider;
	else
		pp = NULL;
	if ((disk->d_flags & G_RAID3_DISK_FLAG_HARDCODED) != 0 && pp != NULL)
		strlcpy(md->md_provider, pp->name, sizeof(md->md_provider));
	else
		bzero(md->md_provider, sizeof(md->md_provider));
	if (pp != NULL)
		md->md_provsize = pp->mediasize;
	else
		md->md_provsize = 0;
}

void
g_raid3_update_metadata(struct g_raid3_disk *disk)
{
	struct g_raid3_metadata md;
	int error;

	g_topology_assert();
	g_raid3_fill_metadata(disk, &md);
	error = g_raid3_write_metadata(disk, &md);
	if (error == 0) {
		G_RAID3_DEBUG(2, "Metadata on %s updated.",
		    g_raid3_get_diskname(disk));
	} else {
		G_RAID3_DEBUG(0,
		    "Cannot update metadata on disk %s (error=%d).",
		    g_raid3_get_diskname(disk), error);
	}
}

static void
g_raid3_bump_syncid(struct g_raid3_softc *sc)
{
	struct g_raid3_disk *disk;
	u_int n;

	g_topology_assert();
	KASSERT(g_raid3_ndisks(sc, G_RAID3_DISK_STATE_ACTIVE) > 0,
	    ("%s called with no active disks (device=%s).", __func__,
	    sc->sc_name));

	sc->sc_syncid++;
	G_RAID3_DEBUG(1, "Device %s: syncid bumped to %u.", sc->sc_name,
	    sc->sc_syncid);
	for (n = 0; n < sc->sc_ndisks; n++) {
		disk = &sc->sc_disks[n];
		if (disk->d_state == G_RAID3_DISK_STATE_ACTIVE ||
		    disk->d_state == G_RAID3_DISK_STATE_SYNCHRONIZING) {
			disk->d_sync.ds_syncid = sc->sc_syncid;
			g_raid3_update_metadata(disk);
		}
	}
}

static void
g_raid3_bump_genid(struct g_raid3_softc *sc)
{
	struct g_raid3_disk *disk;
	u_int n;

	g_topology_assert();
	KASSERT(g_raid3_ndisks(sc, G_RAID3_DISK_STATE_ACTIVE) > 0,
	    ("%s called with no active disks (device=%s).", __func__,
	    sc->sc_name));

	sc->sc_genid++;
	G_RAID3_DEBUG(1, "Device %s: genid bumped to %u.", sc->sc_name,
	    sc->sc_genid);
	for (n = 0; n < sc->sc_ndisks; n++) {
		disk = &sc->sc_disks[n];
		if (disk->d_state == G_RAID3_DISK_STATE_ACTIVE ||
		    disk->d_state == G_RAID3_DISK_STATE_SYNCHRONIZING) {
			disk->d_genid = sc->sc_genid;
			g_raid3_update_metadata(disk);
		}
	}
}

static void
g_raid3_idle(struct g_raid3_softc *sc)
{
	struct g_raid3_disk *disk;
	u_int i;

	if (sc->sc_provider == NULL || sc->sc_provider->acw == 0)
		return;
	sc->sc_idle = 1;
	g_topology_lock();
	for (i = 0; i < sc->sc_ndisks; i++) {
		disk = &sc->sc_disks[i];
		if (disk->d_state != G_RAID3_DISK_STATE_ACTIVE)
			continue;
		G_RAID3_DEBUG(1, "Disk %s (device %s) marked as clean.",
		    g_raid3_get_diskname(disk), sc->sc_name);
		disk->d_flags &= ~G_RAID3_DISK_FLAG_DIRTY;
		g_raid3_update_metadata(disk);
	}
	g_topology_unlock();
}

static void
g_raid3_unidle(struct g_raid3_softc *sc)
{
	struct g_raid3_disk *disk;
	u_int i;

	sc->sc_idle = 0;
	g_topology_lock();
	for (i = 0; i < sc->sc_ndisks; i++) {
		disk = &sc->sc_disks[i];
		if (disk->d_state != G_RAID3_DISK_STATE_ACTIVE)
			continue;
		G_RAID3_DEBUG(1, "Disk %s (device %s) marked as dirty.",
		    g_raid3_get_diskname(disk), sc->sc_name);
		disk->d_flags |= G_RAID3_DISK_FLAG_DIRTY;
		g_raid3_update_metadata(disk);
	}
	g_topology_unlock();
}

/*      
 * Return 1 if we should check if RAID3 device is idling.
 */
static int      
g_raid3_check_idle(struct g_raid3_softc *sc)
{   
	struct g_raid3_disk *disk;
	u_int i;

	if (sc->sc_idle)
		return (0);
	if (sc->sc_provider != NULL && sc->sc_provider->acw == 0)
		return (0);
	/* 
	 * Check if there are no in-flight requests.
	 */	 
	for (i = 0; i < sc->sc_ndisks; i++) {
		disk = &sc->sc_disks[i];
		if (disk->d_state != G_RAID3_DISK_STATE_ACTIVE)
			continue;
		if (disk->d_consumer->index > 0)
			return (0);
	}	   
	return (1); 
}

/*
 * Treat bio_driver1 field in parent bio as list head and field bio_caller1
 * in child bio as pointer to the next element on the list.
 */
#define	G_RAID3_HEAD_BIO(pbp)	(pbp)->bio_driver1

#define	G_RAID3_NEXT_BIO(cbp)	(cbp)->bio_caller1

#define	G_RAID3_FOREACH_BIO(pbp, bp)					\
	for ((bp) = G_RAID3_HEAD_BIO(pbp); (bp) != NULL;		\
	    (bp) = G_RAID3_NEXT_BIO(bp))

#define	G_RAID3_FOREACH_SAFE_BIO(pbp, bp, tmpbp)			\
	for ((bp) = G_RAID3_HEAD_BIO(pbp);				\
	    (bp) != NULL && ((tmpbp) = G_RAID3_NEXT_BIO(bp), 1);	\
	    (bp) = (tmpbp))

static void
g_raid3_init_bio(struct bio *pbp)
{

	G_RAID3_HEAD_BIO(pbp) = NULL;
}

static void
g_raid3_remove_bio(struct bio *cbp)
{
	struct bio *pbp, *bp;

	pbp = cbp->bio_parent;
	if (G_RAID3_HEAD_BIO(pbp) == cbp)
		G_RAID3_HEAD_BIO(pbp) = G_RAID3_NEXT_BIO(cbp);
	else {
		G_RAID3_FOREACH_BIO(pbp, bp) {
			if (G_RAID3_NEXT_BIO(bp) == cbp) {
				G_RAID3_NEXT_BIO(bp) = G_RAID3_NEXT_BIO(cbp);
				break;
			}
		}
	}
	G_RAID3_NEXT_BIO(cbp) = NULL;
}

static void
g_raid3_replace_bio(struct bio *sbp, struct bio *dbp)
{
	struct bio *pbp, *bp;

	g_raid3_remove_bio(sbp);
	pbp = dbp->bio_parent;
	G_RAID3_NEXT_BIO(sbp) = G_RAID3_NEXT_BIO(dbp);
	if (G_RAID3_HEAD_BIO(pbp) == dbp)
		G_RAID3_HEAD_BIO(pbp) = sbp;
	else {
		G_RAID3_FOREACH_BIO(pbp, bp) {
			if (G_RAID3_NEXT_BIO(bp) == dbp) {
				G_RAID3_NEXT_BIO(bp) = sbp;
				break;
			}
		}
	}
	G_RAID3_NEXT_BIO(dbp) = NULL;
}

static void
g_raid3_destroy_bio(struct g_raid3_softc *sc, struct bio *cbp)
{
	struct bio *bp, *pbp;
	size_t size;

	pbp = cbp->bio_parent;
	pbp->bio_children--;
	KASSERT(cbp->bio_data != NULL, ("NULL bio_data"));
	size = pbp->bio_length / (sc->sc_ndisks - 1);
	if (size > 16384)
		uma_zfree(sc->sc_zone_64k, cbp->bio_data);
	else if (size > 4096)
		uma_zfree(sc->sc_zone_16k, cbp->bio_data);
	else
		uma_zfree(sc->sc_zone_4k, cbp->bio_data);
	if (G_RAID3_HEAD_BIO(pbp) == cbp) {
		G_RAID3_HEAD_BIO(pbp) = G_RAID3_NEXT_BIO(cbp);
		G_RAID3_NEXT_BIO(cbp) = NULL;
		g_destroy_bio(cbp);
	} else {
		G_RAID3_FOREACH_BIO(pbp, bp) {
			if (G_RAID3_NEXT_BIO(bp) == cbp)
				break;
		}
		if (bp != NULL) {
			KASSERT(G_RAID3_NEXT_BIO(bp) != NULL,
			    ("NULL bp->bio_driver1"));
			G_RAID3_NEXT_BIO(bp) = G_RAID3_NEXT_BIO(cbp);
			G_RAID3_NEXT_BIO(cbp) = NULL;
		}
		g_destroy_bio(cbp);
	}
}

static struct bio *
g_raid3_clone_bio(struct g_raid3_softc *sc, struct bio *pbp)
{
	struct bio *bp, *cbp;
	size_t size;

	cbp = g_clone_bio(pbp);
	if (cbp == NULL)
		return (NULL);
	size = pbp->bio_length / (sc->sc_ndisks - 1);
	if (size > 16384) {
		cbp->bio_data = uma_zalloc(sc->sc_zone_64k, M_NOWAIT);
		g_raid3_64k_requested++;
	} else if (size > 4096) {
		cbp->bio_data = uma_zalloc(sc->sc_zone_16k, M_NOWAIT);
		g_raid3_16k_requested++;
	} else {
		cbp->bio_data = uma_zalloc(sc->sc_zone_4k, M_NOWAIT);
		g_raid3_4k_requested++;
	}
	if (cbp->bio_data == NULL) {
		if (size > 16384)
			g_raid3_64k_failed++;
		else if (size > 4096)
			g_raid3_16k_failed++;
		else
			g_raid3_4k_failed++;
		pbp->bio_children--;
		g_destroy_bio(cbp);
		return (NULL);
	}
	G_RAID3_NEXT_BIO(cbp) = NULL;
	if (G_RAID3_HEAD_BIO(pbp) == NULL)
		G_RAID3_HEAD_BIO(pbp) = cbp;
	else {
		G_RAID3_FOREACH_BIO(pbp, bp) {
			if (G_RAID3_NEXT_BIO(bp) == NULL) {
				G_RAID3_NEXT_BIO(bp) = cbp;
				break;
			}
		}
	}
	return (cbp);
}

static void
g_raid3_scatter(struct bio *pbp)
{
	struct g_raid3_softc *sc;
	struct g_raid3_disk *disk;
	struct bio *bp, *cbp;
	off_t atom, cadd, padd, left;

	sc = pbp->bio_to->geom->softc;
	bp = NULL;
	if ((pbp->bio_pflags & G_RAID3_BIO_PFLAG_NOPARITY) == 0) {
		/*
		 * Find bio for which we should calculate data.
		 */
		G_RAID3_FOREACH_BIO(pbp, cbp) {
			if ((cbp->bio_cflags & G_RAID3_BIO_CFLAG_PARITY) != 0) {
				bp = cbp;
				break;
			}
		}
		KASSERT(bp != NULL, ("NULL parity bio."));
	}
	atom = sc->sc_sectorsize / (sc->sc_ndisks - 1);
	cadd = padd = 0;
	for (left = pbp->bio_length; left > 0; left -= sc->sc_sectorsize) {
		G_RAID3_FOREACH_BIO(pbp, cbp) {
			if (cbp == bp)
				continue;
			bcopy(pbp->bio_data + padd, cbp->bio_data + cadd, atom);
			padd += atom;
		}
		cadd += atom;
	}
	if ((pbp->bio_pflags & G_RAID3_BIO_PFLAG_NOPARITY) == 0) {
		struct bio *tmpbp;

		/*
		 * Calculate parity.
		 */
		bzero(bp->bio_data, bp->bio_length);
		G_RAID3_FOREACH_SAFE_BIO(pbp, cbp, tmpbp) {
			if (cbp == bp)
				continue;
			g_raid3_xor(cbp->bio_data, bp->bio_data, bp->bio_data,
			    bp->bio_length);
			if ((cbp->bio_cflags & G_RAID3_BIO_CFLAG_NODISK) != 0)
				g_raid3_destroy_bio(sc, cbp);
		}
	}
	G_RAID3_FOREACH_BIO(pbp, cbp) {
		struct g_consumer *cp;

		disk = cbp->bio_caller2;
		cp = disk->d_consumer;
		cbp->bio_to = cp->provider;
		G_RAID3_LOGREQ(3, cbp, "Sending request.");
		KASSERT(cp->acr == 1 && cp->acw == 1 && cp->ace == 1,
		    ("Consumer %s not opened (r%dw%de%d).", cp->provider->name,
		    cp->acr, cp->acw, cp->ace));
		cp->index++;
		g_io_request(cbp, cp);
	}
}

static void
g_raid3_gather(struct bio *pbp)
{
	struct g_raid3_softc *sc;
	struct g_raid3_disk *disk;
	struct bio *xbp, *fbp, *cbp;
	off_t atom, cadd, padd, left;

	sc = pbp->bio_to->geom->softc;
	/*
	 * Find bio for which we have to calculate data.
	 * While going through this path, check if all requests
	 * succeeded, if not, deny whole request.
	 * If we're in COMPLETE mode, we allow one request to fail,
	 * so if we find one, we're sending it to the parity consumer.
	 * If there are more failed requests, we deny whole request.
	 */
	xbp = fbp = NULL;
	G_RAID3_FOREACH_BIO(pbp, cbp) {
		if ((cbp->bio_cflags & G_RAID3_BIO_CFLAG_PARITY) != 0) {
			KASSERT(xbp == NULL, ("More than one parity bio."));
			xbp = cbp;
		}
		if (cbp->bio_error == 0)
			continue;
		/*
		 * Found failed request.
		 */
		G_RAID3_LOGREQ(0, cbp, "Request failed.");
		disk = cbp->bio_caller2;
		if (disk != NULL) {
			/*
			 * Actually this is pointless to bump genid,
			 * because whole device is fucked up.
			 */
			sc->sc_bump_id |= G_RAID3_BUMP_GENID;
			g_raid3_event_send(disk,
			    G_RAID3_DISK_STATE_DISCONNECTED,
			    G_RAID3_EVENT_DONTWAIT);
		}
		if (fbp == NULL) {
			if ((pbp->bio_pflags & G_RAID3_BIO_PFLAG_DEGRADED) != 0) {
				/*
				 * We are already in degraded mode, so we can't
				 * accept any failures.
				 */
				if (pbp->bio_error == 0)
					pbp->bio_error = fbp->bio_error;
			} else {
				fbp = cbp;
			}
		} else {
			/*
			 * Next failed request, that's too many.
			 */
			if (pbp->bio_error == 0)
				pbp->bio_error = fbp->bio_error;
		}
	}
	if (pbp->bio_error != 0)
		goto finish;
	if (fbp != NULL && (pbp->bio_pflags & G_RAID3_BIO_PFLAG_VERIFY) != 0) {
		pbp->bio_pflags &= ~G_RAID3_BIO_PFLAG_VERIFY;
		if (xbp != fbp)
			g_raid3_replace_bio(xbp, fbp);
		g_raid3_destroy_bio(sc, fbp);
	} else if (fbp != NULL) {
		struct g_consumer *cp;

		/*
		 * One request failed, so send the same request to
		 * the parity consumer.
		 */
		disk = pbp->bio_driver2;
		if (disk->d_state != G_RAID3_DISK_STATE_ACTIVE) {
			pbp->bio_error = fbp->bio_error;
			goto finish;
		}
		pbp->bio_pflags |= G_RAID3_BIO_PFLAG_DEGRADED;
		pbp->bio_inbed--;
		fbp->bio_flags &= ~(BIO_DONE | BIO_ERROR);
		if (disk->d_no == sc->sc_ndisks - 1)
			fbp->bio_cflags |= G_RAID3_BIO_CFLAG_PARITY;
		fbp->bio_error = 0;
		fbp->bio_completed = 0;
		fbp->bio_children = 0;
		fbp->bio_inbed = 0;
		cp = disk->d_consumer;
		fbp->bio_caller2 = disk;
		fbp->bio_to = cp->provider;
		G_RAID3_LOGREQ(3, fbp, "Sending request (recover).");
		KASSERT(cp->acr == 1 && cp->acw == 1 && cp->ace == 1,
		    ("Consumer %s not opened (r%dw%de%d).", cp->provider->name,
		    cp->acr, cp->acw, cp->ace));
		cp->index++;
		g_io_request(fbp, cp);
		return;
	}
	if (xbp != NULL) {
		/*
		 * Calculate parity.
		 */
		G_RAID3_FOREACH_BIO(pbp, cbp) {
			if ((cbp->bio_cflags & G_RAID3_BIO_CFLAG_PARITY) != 0)
				continue;
			g_raid3_xor(cbp->bio_data, xbp->bio_data, xbp->bio_data,
			    xbp->bio_length);
		}
		xbp->bio_cflags &= ~G_RAID3_BIO_CFLAG_PARITY;
		if ((pbp->bio_pflags & G_RAID3_BIO_PFLAG_VERIFY) != 0) {
			if (!g_raid3_is_zero(xbp)) {
				g_raid3_parity_mismatch++;
				pbp->bio_error = EIO;
				goto finish;
			}
			g_raid3_destroy_bio(sc, xbp);
		}
	}
	atom = sc->sc_sectorsize / (sc->sc_ndisks - 1);
	cadd = padd = 0;
	for (left = pbp->bio_length; left > 0; left -= sc->sc_sectorsize) {
		G_RAID3_FOREACH_BIO(pbp, cbp) {
			bcopy(cbp->bio_data + cadd, pbp->bio_data + padd, atom);
			pbp->bio_completed += atom;
			padd += atom;
		}
		cadd += atom;
	}
finish:
	if (pbp->bio_error == 0)
		G_RAID3_LOGREQ(3, pbp, "Request finished.");
	else {
		if ((pbp->bio_pflags & G_RAID3_BIO_PFLAG_VERIFY) != 0)
			G_RAID3_LOGREQ(1, pbp, "Verification error.");
		else
			G_RAID3_LOGREQ(0, pbp, "Request failed.");
	}
	pbp->bio_pflags &= ~G_RAID3_BIO_PFLAG_MASK;
	g_io_deliver(pbp, pbp->bio_error);
	while ((cbp = G_RAID3_HEAD_BIO(pbp)) != NULL)
		g_raid3_destroy_bio(sc, cbp);
}

static void
g_raid3_done(struct bio *bp)
{
	struct g_raid3_softc *sc;

	sc = bp->bio_from->geom->softc;
	bp->bio_cflags |= G_RAID3_BIO_CFLAG_REGULAR; 
	G_RAID3_LOGREQ(3, bp, "Regular request done (error=%d).", bp->bio_error);
	mtx_lock(&sc->sc_queue_mtx);
	bioq_insert_head(&sc->sc_queue, bp);
	wakeup(sc);
	wakeup(&sc->sc_queue);
	mtx_unlock(&sc->sc_queue_mtx);
}

static void
g_raid3_regular_request(struct bio *cbp)
{
	struct g_raid3_softc *sc;
	struct g_raid3_disk *disk;
	struct bio *pbp;

	g_topology_assert_not();

	cbp->bio_from->index--;
	pbp = cbp->bio_parent;
	sc = pbp->bio_to->geom->softc;
	disk = cbp->bio_from->private;
	if (disk == NULL) {
		g_topology_lock();
		g_raid3_kill_consumer(sc, cbp->bio_from);
		g_topology_unlock();
	}

	G_RAID3_LOGREQ(3, cbp, "Request finished.");
	pbp->bio_inbed++;
	KASSERT(pbp->bio_inbed <= pbp->bio_children,
	    ("bio_inbed (%u) is bigger than bio_children (%u).", pbp->bio_inbed,
	    pbp->bio_children));
	if (pbp->bio_inbed != pbp->bio_children)
		return;
	switch (pbp->bio_cmd) {
	case BIO_READ:
		g_raid3_gather(pbp);
		break;
	case BIO_WRITE:
	case BIO_DELETE:
	    {
		int error = 0;

		pbp->bio_completed = pbp->bio_length;
		while ((cbp = G_RAID3_HEAD_BIO(pbp)) != NULL) {
			if (cbp->bio_error != 0) {
				disk = cbp->bio_caller2;
				if (disk != NULL) {
					sc->sc_bump_id |= G_RAID3_BUMP_GENID;
					g_raid3_event_send(disk,
					    G_RAID3_DISK_STATE_DISCONNECTED,
					    G_RAID3_EVENT_DONTWAIT);
				}
				if (error == 0)
					error = cbp->bio_error;
				else if (pbp->bio_error == 0) {
					/*
					 * Next failed request, that's too many.
					 */
					pbp->bio_error = error;
				}
			}
			g_raid3_destroy_bio(sc, cbp);
		}
		if (pbp->bio_error == 0)
			G_RAID3_LOGREQ(3, pbp, "Request finished.");
		else
			G_RAID3_LOGREQ(0, pbp, "Request failed.");
		pbp->bio_pflags &= ~G_RAID3_BIO_PFLAG_DEGRADED;
		pbp->bio_pflags &= ~G_RAID3_BIO_PFLAG_NOPARITY;
		g_io_deliver(pbp, pbp->bio_error);
		break;
	    }
	}
}

static void
g_raid3_sync_done(struct bio *bp)
{
	struct g_raid3_softc *sc;

	G_RAID3_LOGREQ(3, bp, "Synchronization request delivered.");
	sc = bp->bio_from->geom->softc;
	bp->bio_cflags |= G_RAID3_BIO_CFLAG_SYNC;
	mtx_lock(&sc->sc_queue_mtx);
	bioq_insert_head(&sc->sc_queue, bp);
	wakeup(sc);
	wakeup(&sc->sc_queue);
	mtx_unlock(&sc->sc_queue_mtx);
}

static void
g_raid3_start(struct bio *bp)
{
	struct g_raid3_softc *sc;

	sc = bp->bio_to->geom->softc;
	/*
	 * If sc == NULL or there are no valid disks, provider's error
	 * should be set and g_raid3_start() should not be called at all.
	 */
	KASSERT(sc != NULL && (sc->sc_state == G_RAID3_DEVICE_STATE_DEGRADED ||
	    sc->sc_state == G_RAID3_DEVICE_STATE_COMPLETE),
	    ("Provider's error should be set (error=%d)(device=%s).",
	    bp->bio_to->error, bp->bio_to->name));
	G_RAID3_LOGREQ(3, bp, "Request received.");

	switch (bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_DELETE:
		break;
	case BIO_GETATTR:
	default:
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}
	mtx_lock(&sc->sc_queue_mtx);
	bioq_insert_tail(&sc->sc_queue, bp);
	G_RAID3_DEBUG(4, "%s: Waking up %p.", __func__, sc);
	wakeup(sc);
	mtx_unlock(&sc->sc_queue_mtx);
}

/*
 * Send one synchronization request.
 */
static void
g_raid3_sync_one(struct g_raid3_softc *sc)
{
	struct g_raid3_disk *disk;
	struct bio *bp;

	KASSERT(sc->sc_state == G_RAID3_DEVICE_STATE_DEGRADED,
	    ("Wrong device state (%s, %s).", sc->sc_name,
	    g_raid3_device_state2str(sc->sc_state)));
	disk = sc->sc_syncdisk;
	KASSERT(disk != NULL, ("No sync disk (%s).", sc->sc_name));
	KASSERT(disk->d_state == G_RAID3_DISK_STATE_SYNCHRONIZING,
	    ("Disk %s is not marked for synchronization.",
	    g_raid3_get_diskname(disk)));

	bp = g_new_bio();
	if (bp == NULL)
		return;
	bp->bio_parent = NULL;
	bp->bio_cmd = BIO_READ;
	bp->bio_offset = disk->d_sync.ds_offset * (sc->sc_ndisks - 1);
	bp->bio_length = MIN(MAXPHYS, sc->sc_mediasize - bp->bio_offset);
	bp->bio_cflags = 0;
	bp->bio_done = g_raid3_sync_done;
	bp->bio_data = disk->d_sync.ds_data;
	if (bp->bio_data == NULL) {
		g_destroy_bio(bp);
		return;
	}
	bp->bio_cflags = G_RAID3_BIO_CFLAG_REGSYNC;
	disk->d_sync.ds_offset += bp->bio_length / (sc->sc_ndisks - 1);
	bp->bio_to = sc->sc_provider;
	G_RAID3_LOGREQ(3, bp, "Sending synchronization request.");
	disk->d_sync.ds_consumer->index++;
	g_io_request(bp, disk->d_sync.ds_consumer);
}

static void
g_raid3_sync_request(struct bio *bp)
{
	struct g_raid3_softc *sc;
	struct g_raid3_disk *disk;

	bp->bio_from->index--;
	sc = bp->bio_from->geom->softc;
	disk = bp->bio_from->private;
	if (disk == NULL) {
		g_topology_lock();
		g_raid3_kill_consumer(sc, bp->bio_from);
		g_topology_unlock();
		g_destroy_bio(bp);
		return;
	}

	/*
	 * Synchronization request.
	 */
	switch (bp->bio_cmd) {
	case BIO_READ:
	    {
		struct g_consumer *cp;
		u_char *dst, *src;
		off_t left;
		u_int atom;

		if (bp->bio_error != 0) {
			G_RAID3_LOGREQ(0, bp,
			    "Synchronization request failed (error=%d).",
			    bp->bio_error);
			g_destroy_bio(bp);
			return;
		}
		G_RAID3_LOGREQ(3, bp, "Synchronization request finished.");
		atom = sc->sc_sectorsize / (sc->sc_ndisks - 1);
		dst = src = bp->bio_data;
		if (disk->d_no == sc->sc_ndisks - 1) {
			u_int n;

			/* Parity component. */
			for (left = bp->bio_length; left > 0;
			    left -= sc->sc_sectorsize) {
				bcopy(src, dst, atom);
				src += atom;
				for (n = 1; n < sc->sc_ndisks - 1; n++) {
					g_raid3_xor(src, dst, dst, atom);
					src += atom;
				}
				dst += atom;
			}
		} else {
			/* Regular component. */
			src += atom * disk->d_no;
			for (left = bp->bio_length; left > 0;
			    left -= sc->sc_sectorsize) {
				bcopy(src, dst, atom);
				src += sc->sc_sectorsize;
				dst += atom;
			}
		}
		bp->bio_offset /= sc->sc_ndisks - 1;
		bp->bio_length /= sc->sc_ndisks - 1;
		bp->bio_cmd = BIO_WRITE;
		bp->bio_cflags = 0;
		bp->bio_children = bp->bio_inbed = 0;
		cp = disk->d_consumer;
		KASSERT(cp->acr == 1 && cp->acw == 1 && cp->ace == 1,
		    ("Consumer %s not opened (r%dw%de%d).", cp->provider->name,
		    cp->acr, cp->acw, cp->ace));
		cp->index++;
		g_io_request(bp, cp);
		return;
	    }
	case BIO_WRITE:
	    {
		struct g_raid3_disk_sync *sync;

		if (bp->bio_error != 0) {
			G_RAID3_LOGREQ(0, bp,
			    "Synchronization request failed (error=%d).",
			    bp->bio_error);
			g_destroy_bio(bp);
			sc->sc_bump_id |= G_RAID3_BUMP_GENID;
			g_raid3_event_send(disk,
			    G_RAID3_DISK_STATE_DISCONNECTED,
			    G_RAID3_EVENT_DONTWAIT);
			return;
		}
		G_RAID3_LOGREQ(3, bp, "Synchronization request finished.");
		sync = &disk->d_sync;
		sync->ds_offset_done = bp->bio_offset + bp->bio_length;
		g_destroy_bio(bp);
		if (sync->ds_resync != -1)
			return;
		if (sync->ds_offset_done ==
		    sc->sc_mediasize / (sc->sc_ndisks - 1)) {
			/*
			 * Disk up-to-date, activate it.
			 */
			g_raid3_event_send(disk, G_RAID3_DISK_STATE_ACTIVE,
			    G_RAID3_EVENT_DONTWAIT);
			return;
		} else if (sync->ds_offset_done % (MAXPHYS * 100) == 0) {
			/*
			 * Update offset_done on every 100 blocks.
			 * XXX: This should be configurable.
			 */
			g_topology_lock();
			g_raid3_update_metadata(disk);
			g_topology_unlock();
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
g_raid3_register_request(struct bio *pbp)
{
	struct g_raid3_softc *sc;
	struct g_raid3_disk *disk;
	struct g_consumer *cp;
	struct bio *cbp;
	off_t offset, length;
	u_int n, ndisks;
	int round_robin, verify;

	ndisks = 0;
	sc = pbp->bio_to->geom->softc;
	if ((pbp->bio_cflags & G_RAID3_BIO_CFLAG_REGSYNC) != 0 &&
	    sc->sc_syncdisk == NULL) {
		g_io_deliver(pbp, EIO);
		return (0);
	}
	g_raid3_init_bio(pbp);
	length = pbp->bio_length / (sc->sc_ndisks - 1);
	offset = pbp->bio_offset / (sc->sc_ndisks - 1);
	round_robin = verify = 0;
	switch (pbp->bio_cmd) {
	case BIO_READ:
		if ((sc->sc_flags & G_RAID3_DEVICE_FLAG_VERIFY) != 0 &&
		    sc->sc_state == G_RAID3_DEVICE_STATE_COMPLETE) {
			pbp->bio_pflags |= G_RAID3_BIO_PFLAG_VERIFY;
			verify = 1;
			ndisks = sc->sc_ndisks;
		} else {
			verify = 0;
			ndisks = sc->sc_ndisks - 1;
		}
		if ((sc->sc_flags & G_RAID3_DEVICE_FLAG_ROUND_ROBIN) != 0 &&
		    sc->sc_state == G_RAID3_DEVICE_STATE_COMPLETE) {
			round_robin = 1;
		} else {
			round_robin = 0;
		}
		KASSERT(!round_robin || !verify,
		    ("ROUND-ROBIN and VERIFY are mutually exclusive."));
		pbp->bio_driver2 = &sc->sc_disks[sc->sc_ndisks - 1];
		break;
	case BIO_WRITE:
	case BIO_DELETE:
	    {
		struct g_raid3_disk_sync *sync;

		if (sc->sc_idle)
			g_raid3_unidle(sc);

		ndisks = sc->sc_ndisks;

		if (sc->sc_syncdisk == NULL)
			break;
		sync = &sc->sc_syncdisk->d_sync;
		if (offset >= sync->ds_offset)
			break;
		if (offset + length <= sync->ds_offset_done)
			break;
		if (offset >= sync->ds_resync && sync->ds_resync != -1)
			break;
		sync->ds_resync = offset - (offset % MAXPHYS);
		break;
	    }
	}
	for (n = 0; n < ndisks; n++) {
		disk = &sc->sc_disks[n];
		cbp = g_raid3_clone_bio(sc, pbp);
		if (cbp == NULL) {
			while ((cbp = G_RAID3_HEAD_BIO(pbp)) != NULL)
				g_raid3_destroy_bio(sc, cbp);
			/*
			 * To prevent deadlock, we must run back up
			 * with the ENOMEM for failed requests of any
			 * of our consumers.  Our own sync requests
			 * can stick around, as they are finite.
			 */
			if ((pbp->bio_cflags &
			    G_RAID3_BIO_CFLAG_REGULAR) != 0) {
				g_io_deliver(pbp, ENOMEM);
				return (0);
			}
			return (ENOMEM);
		}
		cbp->bio_offset = offset;
		cbp->bio_length = length;
		cbp->bio_done = g_raid3_done;
		switch (pbp->bio_cmd) {
		case BIO_READ:
			if (disk->d_state != G_RAID3_DISK_STATE_ACTIVE) {
				/*
				 * Replace invalid component with the parity
				 * component.
				 */
				disk = &sc->sc_disks[sc->sc_ndisks - 1];
				cbp->bio_cflags |= G_RAID3_BIO_CFLAG_PARITY;
				pbp->bio_pflags |= G_RAID3_BIO_PFLAG_DEGRADED;
			} else if (round_robin &&
			    disk->d_no == sc->sc_round_robin) {
				/*
				 * In round-robin mode skip one data component
				 * and use parity component when reading.
				 */
				pbp->bio_driver2 = disk;
				disk = &sc->sc_disks[sc->sc_ndisks - 1];
				cbp->bio_cflags |= G_RAID3_BIO_CFLAG_PARITY;
				sc->sc_round_robin++;
				round_robin = 0;
			} else if (verify && disk->d_no == sc->sc_ndisks - 1) {
				cbp->bio_cflags |= G_RAID3_BIO_CFLAG_PARITY;
			}
			break;
		case BIO_WRITE:
		case BIO_DELETE:
			if (disk->d_state == G_RAID3_DISK_STATE_ACTIVE ||
			    disk->d_state == G_RAID3_DISK_STATE_SYNCHRONIZING) {
				if (n == ndisks - 1) {
					/*
					 * Active parity component, mark it as such.
					 */
					cbp->bio_cflags |=
					    G_RAID3_BIO_CFLAG_PARITY;
				}
			} else {
				pbp->bio_pflags |= G_RAID3_BIO_PFLAG_DEGRADED;
				if (n == ndisks - 1) {
					/*
					 * Parity component is not connected,
					 * so destroy its request.
					 */
					pbp->bio_pflags |=
					    G_RAID3_BIO_PFLAG_NOPARITY;
					g_raid3_destroy_bio(sc, cbp);
					cbp = NULL;
				} else {
					cbp->bio_cflags |=
					    G_RAID3_BIO_CFLAG_NODISK;
					disk = NULL;
				}
			}
			break;
		}
		if (cbp != NULL)
			cbp->bio_caller2 = disk;
	}
	switch (pbp->bio_cmd) {
	case BIO_READ:
		if (round_robin) {
			/*
			 * If we are in round-robin mode and 'round_robin' is
			 * still 1, it means, that we skipped parity component
			 * for this read and must reset sc_round_robin field.
			 */
			sc->sc_round_robin = 0;
		}
		G_RAID3_FOREACH_BIO(pbp, cbp) {
			disk = cbp->bio_caller2;
			cp = disk->d_consumer;
			cbp->bio_to = cp->provider;
			G_RAID3_LOGREQ(3, cbp, "Sending request.");
			KASSERT(cp->acr == 1 && cp->acw == 1 && cp->ace == 1,
			    ("Consumer %s not opened (r%dw%de%d).",
			    cp->provider->name, cp->acr, cp->acw, cp->ace));
			cp->index++;
			g_io_request(cbp, cp);
		}
		break;
	case BIO_WRITE:
	case BIO_DELETE:
		/*
		 * Bump syncid on first write.
		 */
		if ((sc->sc_bump_id & G_RAID3_BUMP_SYNCID) != 0) {
			sc->sc_bump_id &= ~G_RAID3_BUMP_SYNCID;
			g_topology_lock();
			g_raid3_bump_syncid(sc);
			g_topology_unlock();
		}
		g_raid3_scatter(pbp);
		break;
	}
	return (0);
}

static int
g_raid3_can_destroy(struct g_raid3_softc *sc)
{
	struct g_geom *gp;
	struct g_consumer *cp;
 
	g_topology_assert();
	gp = sc->sc_geom;
	LIST_FOREACH(cp, &gp->consumer, consumer) {
		if (g_raid3_is_busy(sc, cp))
			return (0);
	}
	gp = sc->sc_sync.ds_geom;
	LIST_FOREACH(cp, &gp->consumer, consumer) {
		if (g_raid3_is_busy(sc, cp))
			return (0);
	}
	G_RAID3_DEBUG(2, "No I/O requests for %s, it can be destroyed.",
	    sc->sc_name);
	return (1);
}
 
static int
g_raid3_try_destroy(struct g_raid3_softc *sc)
{
 
	if (sc->sc_rootmount != NULL) {
		G_RAID3_DEBUG(1, "root_mount_rel[%u] %p", __LINE__,
		    sc->sc_rootmount);
		root_mount_rel(sc->sc_rootmount);
		sc->sc_rootmount = NULL;
	}

	g_topology_lock();
	if (!g_raid3_can_destroy(sc)) {
		g_topology_unlock();
		return (0);
	}
	if ((sc->sc_flags & G_RAID3_DEVICE_FLAG_WAIT) != 0) {
		g_topology_unlock();
		G_RAID3_DEBUG(4, "%s: Waking up %p.", __func__,
		    &sc->sc_worker);
		wakeup(&sc->sc_worker);
		sc->sc_worker = NULL;
	} else {
		g_raid3_destroy_device(sc);
		g_topology_unlock();
		free(sc->sc_disks, M_RAID3);
		free(sc, M_RAID3);
	}
	return (1);
}

/*
 * Worker thread.
 */
static void
g_raid3_worker(void *arg)
{
	struct g_raid3_softc *sc;
	struct g_raid3_disk *disk;
	struct g_raid3_disk_sync *sync;
	struct g_raid3_event *ep;
	struct bio *bp;
	u_int nreqs;

	sc = arg;
	mtx_lock_spin(&sched_lock);
	sched_prio(curthread, PRIBIO);
	mtx_unlock_spin(&sched_lock);

	nreqs = 0;
	for (;;) {
		G_RAID3_DEBUG(5, "%s: Let's see...", __func__);
		/*
		 * First take a look at events.
		 * This is important to handle events before any I/O requests.
		 */
		ep = g_raid3_event_get(sc);
		if (ep != NULL && g_topology_try_lock()) {
			g_raid3_event_remove(sc, ep);
			if ((ep->e_flags & G_RAID3_EVENT_DEVICE) != 0) {
				/* Update only device status. */
				G_RAID3_DEBUG(3,
				    "Running event for device %s.",
				    sc->sc_name);
				ep->e_error = 0;
				g_raid3_update_device(sc, 1);
			} else {
				/* Update disk status. */
				G_RAID3_DEBUG(3, "Running event for disk %s.",
				     g_raid3_get_diskname(ep->e_disk));
				ep->e_error = g_raid3_update_disk(ep->e_disk,
				    ep->e_state);
				if (ep->e_error == 0)
					g_raid3_update_device(sc, 0);
			}
			g_topology_unlock();
			if ((ep->e_flags & G_RAID3_EVENT_DONTWAIT) != 0) {
				KASSERT(ep->e_error == 0,
				    ("Error cannot be handled."));
				g_raid3_event_free(ep);
			} else {
				ep->e_flags |= G_RAID3_EVENT_DONE;
				G_RAID3_DEBUG(4, "%s: Waking up %p.", __func__,
				    ep);
				mtx_lock(&sc->sc_events_mtx);
				wakeup(ep);
				mtx_unlock(&sc->sc_events_mtx);
			}
			if ((sc->sc_flags &
			    G_RAID3_DEVICE_FLAG_DESTROY) != 0) {
				if (g_raid3_try_destroy(sc))
					kthread_exit(0);
			}
			G_RAID3_DEBUG(5, "%s: I'm here 1.", __func__);
			continue;
		}
		/*
		 * Now I/O requests.
		 */
		/* Get first request from the queue. */
		mtx_lock(&sc->sc_queue_mtx);
		bp = bioq_first(&sc->sc_queue);
		if (bp == NULL) {
			if (ep != NULL) {
				/*
				 * No I/O requests and topology lock was
				 * already held? Try again.
				 */
				mtx_unlock(&sc->sc_queue_mtx);
				tsleep(ep, PRIBIO, "r3:top1", hz / 5);
				continue;
			}
			if ((sc->sc_flags &
			    G_RAID3_DEVICE_FLAG_DESTROY) != 0) {
				mtx_unlock(&sc->sc_queue_mtx);
				if (g_raid3_try_destroy(sc))
					kthread_exit(0);
				mtx_lock(&sc->sc_queue_mtx);
			}
		}
		if (sc->sc_syncdisk != NULL &&
		    (bp == NULL || nreqs > g_raid3_reqs_per_sync)) {
			mtx_unlock(&sc->sc_queue_mtx);
			/*
			 * It is time for synchronization...
			 */
			nreqs = 0;
			disk = sc->sc_syncdisk;
			sync = &disk->d_sync;
			if (sync->ds_offset <
			    sc->sc_mediasize / (sc->sc_ndisks - 1) &&
			    sync->ds_offset == sync->ds_offset_done) {
				if (sync->ds_resync != -1) {
					sync->ds_offset = sync->ds_resync;
					sync->ds_offset_done = sync->ds_resync;
					sync->ds_resync = -1;
				}
				g_raid3_sync_one(sc);
			}
			G_RAID3_DEBUG(5, "%s: I'm here 2.", __func__);
			goto sleep;
		}
		if (bp == NULL) {
			if (g_raid3_check_idle(sc)) {
				u_int idletime;

				idletime = g_raid3_idletime;
				if (idletime == 0)
					idletime = 1;
				idletime *= hz;
				if (msleep(sc, &sc->sc_queue_mtx, PRIBIO | PDROP,
				    "r3:w1", idletime) == EWOULDBLOCK) {
					G_RAID3_DEBUG(5, "%s: I'm here 3.",
					    __func__);
					/*
					 * No I/O requests in 'idletime'
					 * seconds, so mark components as clean.
					 */
					g_raid3_idle(sc);
				}
				G_RAID3_DEBUG(5, "%s: I'm here 4.", __func__);
			} else {
				MSLEEP(sc, &sc->sc_queue_mtx, PRIBIO | PDROP,
				    "r3:w2", 0);
				G_RAID3_DEBUG(5, "%s: I'm here 5.", __func__);
			}
			continue;
		}
		nreqs++;
		bioq_remove(&sc->sc_queue, bp);
		mtx_unlock(&sc->sc_queue_mtx);

		if ((bp->bio_cflags & G_RAID3_BIO_CFLAG_REGULAR) != 0) {
			g_raid3_regular_request(bp);
		} else if ((bp->bio_cflags & G_RAID3_BIO_CFLAG_SYNC) != 0) {
			u_int timeout, sps;

			g_raid3_sync_request(bp);
sleep:
			sps = atomic_load_acq_int(&g_raid3_syncs_per_sec);
			if (sps == 0) {
				G_RAID3_DEBUG(5, "%s: I'm here 6.", __func__);
				continue;
			}
			if (ep != NULL) {
				/*
				 * We have some pending events, don't sleep now.
				 */
				G_RAID3_DEBUG(5, "%s: I'm here 7.", __func__);
				tsleep(ep, PRIBIO, "r3:top2", hz / 5);
				continue;
			}
			mtx_lock(&sc->sc_queue_mtx);
			if (bioq_first(&sc->sc_queue) != NULL) {
				mtx_unlock(&sc->sc_queue_mtx);
				G_RAID3_DEBUG(5, "%s: I'm here 8.", __func__);
				continue;
			}
			timeout = hz / sps;
			if (timeout == 0)
				timeout = 1;
			MSLEEP(sc, &sc->sc_queue_mtx, PRIBIO | PDROP, "r3:w2",
			    timeout);
		} else {
			if (g_raid3_register_request(bp) != 0) {
				mtx_lock(&sc->sc_queue_mtx);
				bioq_insert_tail(&sc->sc_queue, bp);
				MSLEEP(&sc->sc_queue, &sc->sc_queue_mtx,
				    PRIBIO | PDROP, "r3:lowmem", hz / 10);
			}
		}
		G_RAID3_DEBUG(5, "%s: I'm here 9.", __func__);
	}
}

/*
 * Open disk's consumer if needed.
 */
static void
g_raid3_update_access(struct g_raid3_disk *disk)
{
	struct g_provider *pp;

	g_topology_assert();

	pp = disk->d_softc->sc_provider;
	if (pp == NULL)
		return;
	if (pp->acw > 0) {
		if ((disk->d_flags & G_RAID3_DISK_FLAG_DIRTY) == 0) {
			G_RAID3_DEBUG(1, "Disk %s (device %s) marked as dirty.",
			    g_raid3_get_diskname(disk), disk->d_softc->sc_name);
			disk->d_flags |= G_RAID3_DISK_FLAG_DIRTY;
		}
	} else if (pp->acw == 0) {
		if ((disk->d_flags & G_RAID3_DISK_FLAG_DIRTY) != 0) {
			G_RAID3_DEBUG(1, "Disk %s (device %s) marked as clean.",
			    g_raid3_get_diskname(disk), disk->d_softc->sc_name);
			disk->d_flags &= ~G_RAID3_DISK_FLAG_DIRTY;
		}
	}
}

static void
g_raid3_sync_start(struct g_raid3_softc *sc)
{
	struct g_raid3_disk *disk;
	int error;
	u_int n;

	g_topology_assert();

	KASSERT(sc->sc_state == G_RAID3_DEVICE_STATE_DEGRADED,
	    ("Device not in DEGRADED state (%s, %u).", sc->sc_name,
	    sc->sc_state));
	KASSERT(sc->sc_syncdisk == NULL, ("Syncdisk is not NULL (%s, %u).",
	    sc->sc_name, sc->sc_state));
	disk = NULL;
	for (n = 0; n < sc->sc_ndisks; n++) {
		if (sc->sc_disks[n].d_state != G_RAID3_DISK_STATE_SYNCHRONIZING)
			continue;
		disk = &sc->sc_disks[n];
		break;
	}
	if (disk == NULL)
		return;

	G_RAID3_DEBUG(0, "Device %s: rebuilding provider %s.", sc->sc_name,
	    g_raid3_get_diskname(disk));
	disk->d_flags |= G_RAID3_DISK_FLAG_DIRTY;
	KASSERT(disk->d_sync.ds_consumer == NULL,
	    ("Sync consumer already exists (device=%s, disk=%s).",
	    sc->sc_name, g_raid3_get_diskname(disk)));
	disk->d_sync.ds_consumer = g_new_consumer(sc->sc_sync.ds_geom);
	disk->d_sync.ds_consumer->private = disk;
	disk->d_sync.ds_consumer->index = 0;
	error = g_attach(disk->d_sync.ds_consumer, disk->d_softc->sc_provider);
	KASSERT(error == 0, ("Cannot attach to %s (error=%d).",
	    disk->d_softc->sc_name, error));
	error = g_access(disk->d_sync.ds_consumer, 1, 0, 0);
	KASSERT(error == 0, ("Cannot open %s (error=%d).",
	    disk->d_softc->sc_name, error));
	disk->d_sync.ds_data = malloc(MAXPHYS, M_RAID3, M_WAITOK);
	sc->sc_syncdisk = disk;
}

/*
 * Stop synchronization process.
 * type: 0 - synchronization finished
 *       1 - synchronization stopped
 */
static void
g_raid3_sync_stop(struct g_raid3_softc *sc, int type)
{
	struct g_raid3_disk *disk;

	g_topology_assert();
	KASSERT(sc->sc_state == G_RAID3_DEVICE_STATE_DEGRADED,
	    ("Device not in DEGRADED state (%s, %u).", sc->sc_name,
	    sc->sc_state));
	disk = sc->sc_syncdisk;
	sc->sc_syncdisk = NULL;
	KASSERT(disk != NULL, ("No disk was synchronized (%s).", sc->sc_name));
	KASSERT(disk->d_state == G_RAID3_DISK_STATE_SYNCHRONIZING,
	    ("Wrong disk state (%s, %s).", g_raid3_get_diskname(disk),
	    g_raid3_disk_state2str(disk->d_state)));
	if (disk->d_sync.ds_consumer == NULL)
		return;

	if (type == 0) {
		G_RAID3_DEBUG(0, "Device %s: rebuilding provider %s finished.",
		    disk->d_softc->sc_name, g_raid3_get_diskname(disk));
	} else /* if (type == 1) */ {
		G_RAID3_DEBUG(0, "Device %s: rebuilding provider %s stopped.",
		    disk->d_softc->sc_name, g_raid3_get_diskname(disk));
	}
	g_raid3_kill_consumer(disk->d_softc, disk->d_sync.ds_consumer);
	free(disk->d_sync.ds_data, M_RAID3);
	disk->d_sync.ds_consumer = NULL;
	disk->d_flags &= ~G_RAID3_DISK_FLAG_DIRTY;
}

static void
g_raid3_launch_provider(struct g_raid3_softc *sc)
{
	struct g_provider *pp;

	g_topology_assert();

	pp = g_new_providerf(sc->sc_geom, "raid3/%s", sc->sc_name);
	pp->mediasize = sc->sc_mediasize;
	pp->sectorsize = sc->sc_sectorsize;
	sc->sc_provider = pp;
	g_error_provider(pp, 0);
	G_RAID3_DEBUG(0, "Device %s: provider %s launched.", sc->sc_name,
	    pp->name);
	if (sc->sc_state == G_RAID3_DEVICE_STATE_DEGRADED)
		g_raid3_sync_start(sc);
}

static void
g_raid3_destroy_provider(struct g_raid3_softc *sc)
{
	struct bio *bp;

	g_topology_assert();
	KASSERT(sc->sc_provider != NULL, ("NULL provider (device=%s).",
	    sc->sc_name));

	g_error_provider(sc->sc_provider, ENXIO);
	mtx_lock(&sc->sc_queue_mtx);
	while ((bp = bioq_first(&sc->sc_queue)) != NULL) {
		bioq_remove(&sc->sc_queue, bp);
		g_io_deliver(bp, ENXIO);
	}
	mtx_unlock(&sc->sc_queue_mtx);
	G_RAID3_DEBUG(0, "Device %s: provider %s destroyed.", sc->sc_name,
	    sc->sc_provider->name);
	sc->sc_provider->flags |= G_PF_WITHER;
	g_orphan_provider(sc->sc_provider, ENXIO);
	sc->sc_provider = NULL;
	if (sc->sc_syncdisk != NULL)
		g_raid3_sync_stop(sc, 1);
}

static void
g_raid3_go(void *arg)
{
	struct g_raid3_softc *sc;

	sc = arg;
	G_RAID3_DEBUG(0, "Force device %s start due to timeout.", sc->sc_name);
	g_raid3_event_send(sc, 0,
	    G_RAID3_EVENT_DONTWAIT | G_RAID3_EVENT_DEVICE);
}

static u_int
g_raid3_determine_state(struct g_raid3_disk *disk)
{
	struct g_raid3_softc *sc;
	u_int state;

	sc = disk->d_softc;
	if (sc->sc_syncid == disk->d_sync.ds_syncid) {
		if ((disk->d_flags &
		    G_RAID3_DISK_FLAG_SYNCHRONIZING) == 0) {
			/* Disk does not need synchronization. */
			state = G_RAID3_DISK_STATE_ACTIVE;
		} else {
			if ((sc->sc_flags &
			     G_RAID3_DEVICE_FLAG_NOAUTOSYNC) == 0  ||
			    (disk->d_flags &
			     G_RAID3_DISK_FLAG_FORCE_SYNC) != 0) {
				/*
				 * We can start synchronization from
				 * the stored offset.
				 */
				state = G_RAID3_DISK_STATE_SYNCHRONIZING;
			} else {
				state = G_RAID3_DISK_STATE_STALE;
			}
		}
	} else if (disk->d_sync.ds_syncid < sc->sc_syncid) {
		/*
		 * Reset all synchronization data for this disk,
		 * because if it even was synchronized, it was
		 * synchronized to disks with different syncid.
		 */
		disk->d_flags |= G_RAID3_DISK_FLAG_SYNCHRONIZING;
		disk->d_sync.ds_offset = 0;
		disk->d_sync.ds_offset_done = 0;
		disk->d_sync.ds_syncid = sc->sc_syncid;
		if ((sc->sc_flags & G_RAID3_DEVICE_FLAG_NOAUTOSYNC) == 0 ||
		    (disk->d_flags & G_RAID3_DISK_FLAG_FORCE_SYNC) != 0) {
			state = G_RAID3_DISK_STATE_SYNCHRONIZING;
		} else {
			state = G_RAID3_DISK_STATE_STALE;
		}
	} else /* if (sc->sc_syncid < disk->d_sync.ds_syncid) */ {
		/*
		 * Not good, NOT GOOD!
		 * It means that device was started on stale disks
		 * and more fresh disk just arrive.
		 * If there were writes, device is fucked up, sorry.
		 * I think the best choice here is don't touch
		 * this disk and inform the user laudly.
		 */
		G_RAID3_DEBUG(0, "Device %s was started before the freshest "
		    "disk (%s) arrives!! It will not be connected to the "
		    "running device.", sc->sc_name,
		    g_raid3_get_diskname(disk));
		g_raid3_destroy_disk(disk);
		state = G_RAID3_DISK_STATE_NONE;
		/* Return immediately, because disk was destroyed. */
		return (state);
	}
	G_RAID3_DEBUG(3, "State for %s disk: %s.",
	    g_raid3_get_diskname(disk), g_raid3_disk_state2str(state));
	return (state);
}

/*
 * Update device state.
 */
static void
g_raid3_update_device(struct g_raid3_softc *sc, boolean_t force)
{
	struct g_raid3_disk *disk;
	u_int state;

	g_topology_assert();

	switch (sc->sc_state) {
	case G_RAID3_DEVICE_STATE_STARTING:
	    {
		u_int n, ndirty, ndisks, genid, syncid;

		KASSERT(sc->sc_provider == NULL,
		    ("Non-NULL provider in STARTING state (%s).", sc->sc_name));
		/*
		 * Are we ready? We are, if all disks are connected or
		 * one disk is missing and 'force' is true.
		 */
		if (g_raid3_ndisks(sc, -1) + force == sc->sc_ndisks) {
			if (!force)
				callout_drain(&sc->sc_callout);
		} else {
			if (force) {
				/*
				 * Timeout expired, so destroy device.
				 */
				sc->sc_flags |= G_RAID3_DEVICE_FLAG_DESTROY;
				G_RAID3_DEBUG(1, "root_mount_rel[%u] %p",
				    __LINE__, sc->sc_rootmount);
				root_mount_rel(sc->sc_rootmount);
				sc->sc_rootmount = NULL;
			}
			return;
		}

		/*
		 * Find the biggest genid.
		 */
		genid = 0;
		for (n = 0; n < sc->sc_ndisks; n++) {
			disk = &sc->sc_disks[n];
			if (disk->d_state == G_RAID3_DISK_STATE_NODISK)
				continue;
			if (disk->d_genid > genid)
				genid = disk->d_genid;
		}
		sc->sc_genid = genid;
		/*
		 * Remove all disks without the biggest genid.
		 */
		for (n = 0; n < sc->sc_ndisks; n++) {
			disk = &sc->sc_disks[n];
			if (disk->d_state == G_RAID3_DISK_STATE_NODISK)
				continue;
			if (disk->d_genid < genid) {
				G_RAID3_DEBUG(0,
				    "Component %s (device %s) broken, skipping.",
				    g_raid3_get_diskname(disk), sc->sc_name);
				g_raid3_destroy_disk(disk);
			}
		}

		/*
		 * There must be at least 'sc->sc_ndisks - 1' components
		 * with the same syncid and without SYNCHRONIZING flag.
		 */

		/*
		 * Find the biggest syncid, number of valid components and
		 * number of dirty components.
		 */
		ndirty = ndisks = syncid = 0;
		for (n = 0; n < sc->sc_ndisks; n++) {
			disk = &sc->sc_disks[n];
			if (disk->d_state == G_RAID3_DISK_STATE_NODISK)
				continue;
			if ((disk->d_flags & G_RAID3_DISK_FLAG_DIRTY) != 0)
				ndirty++;
			if (disk->d_sync.ds_syncid > syncid) {
				syncid = disk->d_sync.ds_syncid;
				ndisks = 0;
			} else if (disk->d_sync.ds_syncid < syncid) {
				continue;
			}
			if ((disk->d_flags &
			    G_RAID3_DISK_FLAG_SYNCHRONIZING) != 0) {
				continue;
			}
			ndisks++;
		}
		/*
		 * Do we have enough valid components?
		 */
		if (ndisks + 1 < sc->sc_ndisks) {
			G_RAID3_DEBUG(0,
			    "Device %s is broken, too few valid components.",
			    sc->sc_name);
			sc->sc_flags |= G_RAID3_DEVICE_FLAG_DESTROY;
			return;
		}
		/*
		 * If there is one DIRTY component and all disks are present,
		 * mark it for synchronization. If there is more than one DIRTY
		 * component, mark parity component for synchronization.
		 */
		if (ndisks == sc->sc_ndisks && ndirty == 1) {
			for (n = 0; n < sc->sc_ndisks; n++) {
				disk = &sc->sc_disks[n];
				if ((disk->d_flags &
				    G_RAID3_DISK_FLAG_DIRTY) == 0) {
					continue;
				}
				disk->d_flags |=
				    G_RAID3_DISK_FLAG_SYNCHRONIZING; 
			}
		} else if (ndisks == sc->sc_ndisks && ndirty > 1) {
			disk = &sc->sc_disks[sc->sc_ndisks - 1];
			disk->d_flags |= G_RAID3_DISK_FLAG_SYNCHRONIZING; 
		}

		sc->sc_syncid = syncid;
		if (force) {
			/* Remember to bump syncid on first write. */
			sc->sc_bump_id |= G_RAID3_BUMP_SYNCID;
		}
		if (ndisks == sc->sc_ndisks)
			state = G_RAID3_DEVICE_STATE_COMPLETE;
		else /* if (ndisks == sc->sc_ndisks - 1) */
			state = G_RAID3_DEVICE_STATE_DEGRADED;
		G_RAID3_DEBUG(1, "Device %s state changed from %s to %s.",
		    sc->sc_name, g_raid3_device_state2str(sc->sc_state),
		    g_raid3_device_state2str(state));
		sc->sc_state = state;
		for (n = 0; n < sc->sc_ndisks; n++) {
			disk = &sc->sc_disks[n];
			if (disk->d_state == G_RAID3_DISK_STATE_NODISK)
				continue;
			state = g_raid3_determine_state(disk);
			g_raid3_event_send(disk, state, G_RAID3_EVENT_DONTWAIT);
			if (state == G_RAID3_DISK_STATE_STALE)
				sc->sc_bump_id |= G_RAID3_BUMP_SYNCID;
		}
		break;
	    }
	case G_RAID3_DEVICE_STATE_DEGRADED:
		/*
		 * Genid need to be bumped immediately, so do it here.
		 */
		if ((sc->sc_bump_id & G_RAID3_BUMP_GENID) != 0) {
			sc->sc_bump_id &= ~G_RAID3_BUMP_GENID;
			g_raid3_bump_genid(sc);
		}

		if (g_raid3_ndisks(sc, G_RAID3_DISK_STATE_NEW) > 0)
			return;
		if (g_raid3_ndisks(sc, G_RAID3_DISK_STATE_ACTIVE) <
		    sc->sc_ndisks - 1) {
			if (sc->sc_provider != NULL)
				g_raid3_destroy_provider(sc);
			sc->sc_flags |= G_RAID3_DEVICE_FLAG_DESTROY;
			return;
		}
		if (g_raid3_ndisks(sc, G_RAID3_DISK_STATE_ACTIVE) ==
		    sc->sc_ndisks) {
			state = G_RAID3_DEVICE_STATE_COMPLETE;
			G_RAID3_DEBUG(1,
			    "Device %s state changed from %s to %s.",
			    sc->sc_name, g_raid3_device_state2str(sc->sc_state),
			    g_raid3_device_state2str(state));
			sc->sc_state = state;
		}
		if (sc->sc_provider == NULL)
			g_raid3_launch_provider(sc);
		if (sc->sc_rootmount != NULL) {
			G_RAID3_DEBUG(1, "root_mount_rel[%u] %p", __LINE__,
			    sc->sc_rootmount);
			root_mount_rel(sc->sc_rootmount);
			sc->sc_rootmount = NULL;
		}
		break;
	case G_RAID3_DEVICE_STATE_COMPLETE:
		/*
		 * Genid need to be bumped immediately, so do it here.
		 */
		if ((sc->sc_bump_id & G_RAID3_BUMP_GENID) != 0) {
			sc->sc_bump_id &= ~G_RAID3_BUMP_GENID;
			g_raid3_bump_genid(sc);
		}

		if (g_raid3_ndisks(sc, G_RAID3_DISK_STATE_NEW) > 0)
			return;
		KASSERT(g_raid3_ndisks(sc, G_RAID3_DISK_STATE_ACTIVE) >=
		    sc->sc_ndisks - 1,
		    ("Too few ACTIVE components in COMPLETE state (device %s).",
		    sc->sc_name));
		if (g_raid3_ndisks(sc, G_RAID3_DISK_STATE_ACTIVE) ==
		    sc->sc_ndisks - 1) {
			state = G_RAID3_DEVICE_STATE_DEGRADED;
			G_RAID3_DEBUG(1,
			    "Device %s state changed from %s to %s.",
			    sc->sc_name, g_raid3_device_state2str(sc->sc_state),
			    g_raid3_device_state2str(state));
			sc->sc_state = state;
		}
		if (sc->sc_provider == NULL)
			g_raid3_launch_provider(sc);
		if (sc->sc_rootmount != NULL) {
			G_RAID3_DEBUG(1, "root_mount_rel[%u] %p", __LINE__,
			    sc->sc_rootmount);
			root_mount_rel(sc->sc_rootmount);
			sc->sc_rootmount = NULL;
		}
		break;
	default:
		KASSERT(1 == 0, ("Wrong device state (%s, %s).", sc->sc_name,
		    g_raid3_device_state2str(sc->sc_state)));
		break;
	}
}

/*
 * Update disk state and device state if needed.
 */
#define	DISK_STATE_CHANGED()	G_RAID3_DEBUG(1,			\
	"Disk %s state changed from %s to %s (device %s).",		\
	g_raid3_get_diskname(disk),					\
	g_raid3_disk_state2str(disk->d_state),				\
	g_raid3_disk_state2str(state), sc->sc_name)
static int
g_raid3_update_disk(struct g_raid3_disk *disk, u_int state)
{
	struct g_raid3_softc *sc;

	g_topology_assert();

	sc = disk->d_softc;
again:
	G_RAID3_DEBUG(3, "Changing disk %s state from %s to %s.",
	    g_raid3_get_diskname(disk), g_raid3_disk_state2str(disk->d_state),
	    g_raid3_disk_state2str(state));
	switch (state) {
	case G_RAID3_DISK_STATE_NEW:
		/*
		 * Possible scenarios:
		 * 1. New disk arrive.
		 */
		/* Previous state should be NONE. */
		KASSERT(disk->d_state == G_RAID3_DISK_STATE_NONE,
		    ("Wrong disk state (%s, %s).", g_raid3_get_diskname(disk),
		    g_raid3_disk_state2str(disk->d_state)));
		DISK_STATE_CHANGED();

		disk->d_state = state;
		G_RAID3_DEBUG(0, "Device %s: provider %s detected.",
		    sc->sc_name, g_raid3_get_diskname(disk));
		if (sc->sc_state == G_RAID3_DEVICE_STATE_STARTING)
			break;
		KASSERT(sc->sc_state == G_RAID3_DEVICE_STATE_DEGRADED ||
		    sc->sc_state == G_RAID3_DEVICE_STATE_COMPLETE,
		    ("Wrong device state (%s, %s, %s, %s).", sc->sc_name,
		    g_raid3_device_state2str(sc->sc_state),
		    g_raid3_get_diskname(disk),
		    g_raid3_disk_state2str(disk->d_state)));
		state = g_raid3_determine_state(disk);
		if (state != G_RAID3_DISK_STATE_NONE)
			goto again;
		break;
	case G_RAID3_DISK_STATE_ACTIVE:
		/*
		 * Possible scenarios:
		 * 1. New disk does not need synchronization.
		 * 2. Synchronization process finished successfully.
		 */
		KASSERT(sc->sc_state == G_RAID3_DEVICE_STATE_DEGRADED ||
		    sc->sc_state == G_RAID3_DEVICE_STATE_COMPLETE,
		    ("Wrong device state (%s, %s, %s, %s).", sc->sc_name,
		    g_raid3_device_state2str(sc->sc_state),
		    g_raid3_get_diskname(disk),
		    g_raid3_disk_state2str(disk->d_state)));
		/* Previous state should be NEW or SYNCHRONIZING. */
		KASSERT(disk->d_state == G_RAID3_DISK_STATE_NEW ||
		    disk->d_state == G_RAID3_DISK_STATE_SYNCHRONIZING,
		    ("Wrong disk state (%s, %s).", g_raid3_get_diskname(disk),
		    g_raid3_disk_state2str(disk->d_state)));
		DISK_STATE_CHANGED();

		if (disk->d_state == G_RAID3_DISK_STATE_NEW)
			disk->d_flags &= ~G_RAID3_DISK_FLAG_DIRTY;
		else if (disk->d_state == G_RAID3_DISK_STATE_SYNCHRONIZING) {
			disk->d_flags &= ~G_RAID3_DISK_FLAG_SYNCHRONIZING;
			disk->d_flags &= ~G_RAID3_DISK_FLAG_FORCE_SYNC;
			g_raid3_sync_stop(sc, 0);
		}
		disk->d_state = state;
		disk->d_sync.ds_offset = 0;
		disk->d_sync.ds_offset_done = 0;
		g_raid3_update_access(disk);
		g_raid3_update_metadata(disk);
		G_RAID3_DEBUG(0, "Device %s: provider %s activated.",
		    sc->sc_name, g_raid3_get_diskname(disk));
		break;
	case G_RAID3_DISK_STATE_STALE:
		/*
		 * Possible scenarios:
		 * 1. Stale disk was connected.
		 */
		/* Previous state should be NEW. */
		KASSERT(disk->d_state == G_RAID3_DISK_STATE_NEW,
		    ("Wrong disk state (%s, %s).", g_raid3_get_diskname(disk),
		    g_raid3_disk_state2str(disk->d_state)));
		KASSERT(sc->sc_state == G_RAID3_DEVICE_STATE_DEGRADED ||
		    sc->sc_state == G_RAID3_DEVICE_STATE_COMPLETE,
		    ("Wrong device state (%s, %s, %s, %s).", sc->sc_name,
		    g_raid3_device_state2str(sc->sc_state),
		    g_raid3_get_diskname(disk),
		    g_raid3_disk_state2str(disk->d_state)));
		/*
		 * STALE state is only possible if device is marked
		 * NOAUTOSYNC.
		 */
		KASSERT((sc->sc_flags & G_RAID3_DEVICE_FLAG_NOAUTOSYNC) != 0,
		    ("Wrong device state (%s, %s, %s, %s).", sc->sc_name,
		    g_raid3_device_state2str(sc->sc_state),
		    g_raid3_get_diskname(disk),
		    g_raid3_disk_state2str(disk->d_state)));
		DISK_STATE_CHANGED();

		disk->d_flags &= ~G_RAID3_DISK_FLAG_DIRTY;
		disk->d_state = state;
		g_raid3_update_metadata(disk);
		G_RAID3_DEBUG(0, "Device %s: provider %s is stale.",
		    sc->sc_name, g_raid3_get_diskname(disk));
		break;
	case G_RAID3_DISK_STATE_SYNCHRONIZING:
		/*
		 * Possible scenarios:
		 * 1. Disk which needs synchronization was connected.
		 */
		/* Previous state should be NEW. */
		KASSERT(disk->d_state == G_RAID3_DISK_STATE_NEW,
		    ("Wrong disk state (%s, %s).", g_raid3_get_diskname(disk),
		    g_raid3_disk_state2str(disk->d_state)));
		KASSERT(sc->sc_state == G_RAID3_DEVICE_STATE_DEGRADED ||
		    sc->sc_state == G_RAID3_DEVICE_STATE_COMPLETE,
		    ("Wrong device state (%s, %s, %s, %s).", sc->sc_name,
		    g_raid3_device_state2str(sc->sc_state),
		    g_raid3_get_diskname(disk),
		    g_raid3_disk_state2str(disk->d_state)));
		DISK_STATE_CHANGED();

		if (disk->d_state == G_RAID3_DISK_STATE_NEW)
			disk->d_flags &= ~G_RAID3_DISK_FLAG_DIRTY;
		disk->d_state = state;
		if (sc->sc_provider != NULL) {
			g_raid3_sync_start(sc);
			g_raid3_update_metadata(disk);
		}
		break;
	case G_RAID3_DISK_STATE_DISCONNECTED:
		/*
		 * Possible scenarios:
		 * 1. Device wasn't running yet, but disk disappear.
		 * 2. Disk was active and disapppear.
		 * 3. Disk disappear during synchronization process.
		 */
		if (sc->sc_state == G_RAID3_DEVICE_STATE_DEGRADED ||
		    sc->sc_state == G_RAID3_DEVICE_STATE_COMPLETE) {
			/*
			 * Previous state should be ACTIVE, STALE or
			 * SYNCHRONIZING.
			 */
			KASSERT(disk->d_state == G_RAID3_DISK_STATE_ACTIVE ||
			    disk->d_state == G_RAID3_DISK_STATE_STALE ||
			    disk->d_state == G_RAID3_DISK_STATE_SYNCHRONIZING,
			    ("Wrong disk state (%s, %s).",
			    g_raid3_get_diskname(disk),
			    g_raid3_disk_state2str(disk->d_state)));
		} else if (sc->sc_state == G_RAID3_DEVICE_STATE_STARTING) {
			/* Previous state should be NEW. */
			KASSERT(disk->d_state == G_RAID3_DISK_STATE_NEW,
			    ("Wrong disk state (%s, %s).",
			    g_raid3_get_diskname(disk),
			    g_raid3_disk_state2str(disk->d_state)));
			/*
			 * Reset bumping syncid if disk disappeared in STARTING
			 * state.
			 */
			if ((sc->sc_bump_id & G_RAID3_BUMP_SYNCID) != 0)
				sc->sc_bump_id &= ~G_RAID3_BUMP_SYNCID;
#ifdef	INVARIANTS
		} else {
			KASSERT(1 == 0, ("Wrong device state (%s, %s, %s, %s).",
			    sc->sc_name,
			    g_raid3_device_state2str(sc->sc_state),
			    g_raid3_get_diskname(disk),
			    g_raid3_disk_state2str(disk->d_state)));
#endif
		}
		DISK_STATE_CHANGED();
		G_RAID3_DEBUG(0, "Device %s: provider %s disconnected.",
		    sc->sc_name, g_raid3_get_diskname(disk));

		g_raid3_destroy_disk(disk);
		break;
	default:
		KASSERT(1 == 0, ("Unknown state (%u).", state));
		break;
	}
	return (0);
}
#undef	DISK_STATE_CHANGED

int
g_raid3_read_metadata(struct g_consumer *cp, struct g_raid3_metadata *md)
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
		G_RAID3_DEBUG(1, "Cannot read metadata from %s (error=%d).",
		    cp->provider->name, error);
		return (error);
	}

	/* Decode metadata. */
	error = raid3_metadata_decode(buf, md);
	g_free(buf);
	if (strcmp(md->md_magic, G_RAID3_MAGIC) != 0)
		return (EINVAL);
	if (md->md_version > G_RAID3_VERSION) {
		G_RAID3_DEBUG(0,
		    "Kernel module is too old to handle metadata from %s.",
		    cp->provider->name);
		return (EINVAL);
	}
	if (error != 0) {
		G_RAID3_DEBUG(1, "MD5 metadata hash mismatch for provider %s.",
		    cp->provider->name);
		return (error);
	}

	return (0);
}

static int
g_raid3_check_metadata(struct g_raid3_softc *sc, struct g_provider *pp,
    struct g_raid3_metadata *md)
{

	if (md->md_no >= sc->sc_ndisks) {
		G_RAID3_DEBUG(1, "Invalid disk %s number (no=%u), skipping.",
		    pp->name, md->md_no);
		return (EINVAL);
	}
	if (sc->sc_disks[md->md_no].d_state != G_RAID3_DISK_STATE_NODISK) {
		G_RAID3_DEBUG(1, "Disk %s (no=%u) already exists, skipping.",
		    pp->name, md->md_no);
		return (EEXIST);
	}
	if (md->md_all != sc->sc_ndisks) {
		G_RAID3_DEBUG(1,
		    "Invalid '%s' field on disk %s (device %s), skipping.",
		    "md_all", pp->name, sc->sc_name);
		return (EINVAL);
	}
	if (md->md_mediasize != sc->sc_mediasize) {
		G_RAID3_DEBUG(1,
		    "Invalid '%s' field on disk %s (device %s), skipping.",
		    "md_mediasize", pp->name, sc->sc_name);
		return (EINVAL);
	}
	if ((md->md_mediasize % (sc->sc_ndisks - 1)) != 0) {
		G_RAID3_DEBUG(1,
		    "Invalid '%s' field on disk %s (device %s), skipping.",
		    "md_mediasize", pp->name, sc->sc_name);
		return (EINVAL);
	}
	if ((sc->sc_mediasize / (sc->sc_ndisks - 1)) > pp->mediasize) {
		G_RAID3_DEBUG(1,
		    "Invalid size of disk %s (device %s), skipping.", pp->name,
		    sc->sc_name);
		return (EINVAL);
	}
	if ((md->md_sectorsize / pp->sectorsize) < sc->sc_ndisks - 1) {
		G_RAID3_DEBUG(1,
		    "Invalid '%s' field on disk %s (device %s), skipping.",
		    "md_sectorsize", pp->name, sc->sc_name);
		return (EINVAL);
	}
	if (md->md_sectorsize != sc->sc_sectorsize) {
		G_RAID3_DEBUG(1,
		    "Invalid '%s' field on disk %s (device %s), skipping.",
		    "md_sectorsize", pp->name, sc->sc_name);
		return (EINVAL);
	}
	if ((sc->sc_sectorsize % pp->sectorsize) != 0) {
		G_RAID3_DEBUG(1,
		    "Invalid sector size of disk %s (device %s), skipping.",
		    pp->name, sc->sc_name);
		return (EINVAL);
	}
	if ((md->md_mflags & ~G_RAID3_DEVICE_FLAG_MASK) != 0) {
		G_RAID3_DEBUG(1,
		    "Invalid device flags on disk %s (device %s), skipping.",
		    pp->name, sc->sc_name);
		return (EINVAL);
	}
	if ((md->md_mflags & G_RAID3_DEVICE_FLAG_VERIFY) != 0 &&
	    (md->md_mflags & G_RAID3_DEVICE_FLAG_ROUND_ROBIN) != 0) {
		/*
		 * VERIFY and ROUND-ROBIN options are mutally exclusive.
		 */
		G_RAID3_DEBUG(1, "Both VERIFY and ROUND-ROBIN flags exist on "
		    "disk %s (device %s), skipping.", pp->name, sc->sc_name);
		return (EINVAL);
	}
	if ((md->md_dflags & ~G_RAID3_DISK_FLAG_MASK) != 0) {
		G_RAID3_DEBUG(1,
		    "Invalid disk flags on disk %s (device %s), skipping.",
		    pp->name, sc->sc_name);
		return (EINVAL);
	}
	return (0);
}

int
g_raid3_add_disk(struct g_raid3_softc *sc, struct g_provider *pp,
    struct g_raid3_metadata *md)
{
	struct g_raid3_disk *disk;
	int error;

	g_topology_assert();
	G_RAID3_DEBUG(2, "Adding disk %s.", pp->name);

	error = g_raid3_check_metadata(sc, pp, md);
	if (error != 0)
		return (error);
	if (sc->sc_state != G_RAID3_DEVICE_STATE_STARTING &&
	    md->md_genid < sc->sc_genid) {
		G_RAID3_DEBUG(0, "Component %s (device %s) broken, skipping.",
		    pp->name, sc->sc_name);
		return (EINVAL);
	}
	disk = g_raid3_init_disk(sc, pp, md, &error);
	if (disk == NULL)
		return (error);
	error = g_raid3_event_send(disk, G_RAID3_DISK_STATE_NEW,
	    G_RAID3_EVENT_WAIT);
	if (error != 0)
		return (error);
	if (md->md_version < G_RAID3_VERSION) {
		G_RAID3_DEBUG(0, "Upgrading metadata on %s (v%d->v%d).",
		    pp->name, md->md_version, G_RAID3_VERSION);
		g_raid3_update_metadata(disk);
	}
	return (0);
}

static int
g_raid3_access(struct g_provider *pp, int acr, int acw, int ace)
{
	struct g_raid3_softc *sc;
	struct g_raid3_disk *disk;
	int dcr, dcw, dce;
	u_int n;

	g_topology_assert();
	G_RAID3_DEBUG(2, "Access request for %s: r%dw%de%d.", pp->name, acr,
	    acw, ace);

	dcr = pp->acr + acr;
	dcw = pp->acw + acw;
	dce = pp->ace + ace;

	sc = pp->geom->softc;
	if (sc == NULL ||
	    g_raid3_ndisks(sc, G_RAID3_DISK_STATE_ACTIVE) < sc->sc_ndisks - 1 ||
	    (sc->sc_flags & G_RAID3_DEVICE_FLAG_DESTROY) != 0) {
		if (acr <= 0 && acw <= 0 && ace <= 0)
			return (0);
		else
			return (ENXIO);
	}
	for (n = 0; n < sc->sc_ndisks; n++) {
		disk = &sc->sc_disks[n];
		if (disk->d_state != G_RAID3_DISK_STATE_ACTIVE)
			continue;
		/*
		 * Mark disk as dirty on open and unmark on close.
		 */
		if (pp->acw == 0 && dcw > 0) {
			G_RAID3_DEBUG(1, "Disk %s (device %s) marked as dirty.",
			    g_raid3_get_diskname(disk), sc->sc_name);
			disk->d_flags |= G_RAID3_DISK_FLAG_DIRTY;
			g_raid3_update_metadata(disk);
		} else if (pp->acw > 0 && dcw == 0) {
			G_RAID3_DEBUG(1, "Disk %s (device %s) marked as clean.",
			    g_raid3_get_diskname(disk), sc->sc_name);
			disk->d_flags &= ~G_RAID3_DISK_FLAG_DIRTY;
			g_raid3_update_metadata(disk);
		}
	}
	return (0);
}

static struct g_geom *
g_raid3_create(struct g_class *mp, const struct g_raid3_metadata *md)
{
	struct g_raid3_softc *sc;
	struct g_geom *gp;
	int error, timeout;
	u_int n;

	g_topology_assert();
	G_RAID3_DEBUG(1, "Creating device %s (id=%u).", md->md_name, md->md_id);

	/* One disk is minimum. */
	if (md->md_all < 1)
		return (NULL);
	/*
	 * Action geom.
	 */
	gp = g_new_geomf(mp, "%s", md->md_name);
	sc = malloc(sizeof(*sc), M_RAID3, M_WAITOK | M_ZERO);
	sc->sc_disks = malloc(sizeof(struct g_raid3_disk) * md->md_all, M_RAID3,
	    M_WAITOK | M_ZERO);
	gp->start = g_raid3_start;
	gp->orphan = g_raid3_orphan;
	gp->access = g_raid3_access;
	gp->dumpconf = g_raid3_dumpconf;

	sc->sc_id = md->md_id;
	sc->sc_mediasize = md->md_mediasize;
	sc->sc_sectorsize = md->md_sectorsize;
	sc->sc_ndisks = md->md_all;
	sc->sc_round_robin = 0;
	sc->sc_flags = md->md_mflags;
	sc->sc_bump_id = 0;
	sc->sc_idle = 0;
	for (n = 0; n < sc->sc_ndisks; n++) {
		sc->sc_disks[n].d_softc = sc;
		sc->sc_disks[n].d_no = n;
		sc->sc_disks[n].d_state = G_RAID3_DISK_STATE_NODISK;
	}
	bioq_init(&sc->sc_queue);
	mtx_init(&sc->sc_queue_mtx, "graid3:queue", NULL, MTX_DEF);
	TAILQ_INIT(&sc->sc_events);
	mtx_init(&sc->sc_events_mtx, "graid3:events", NULL, MTX_DEF);
	callout_init(&sc->sc_callout, CALLOUT_MPSAFE);
	sc->sc_state = G_RAID3_DEVICE_STATE_STARTING;
	gp->softc = sc;
	sc->sc_geom = gp;
	sc->sc_provider = NULL;
	/*
	 * Synchronization geom.
	 */
	gp = g_new_geomf(mp, "%s.sync", md->md_name);
	gp->softc = sc;
	gp->orphan = g_raid3_orphan;
	sc->sc_sync.ds_geom = gp;
	sc->sc_zone_64k = uma_zcreate("gr3:64k", 65536, NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);
	uma_zone_set_max(sc->sc_zone_64k, g_raid3_n64k);
	sc->sc_zone_16k = uma_zcreate("gr3:16k", 16384, NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);
	uma_zone_set_max(sc->sc_zone_64k, g_raid3_n16k);
	sc->sc_zone_4k = uma_zcreate("gr3:4k", 4096, NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);
	uma_zone_set_max(sc->sc_zone_4k, g_raid3_n4k);
	error = kthread_create(g_raid3_worker, sc, &sc->sc_worker, 0, 0,
	    "g_raid3 %s", md->md_name);
	if (error != 0) {
		G_RAID3_DEBUG(1, "Cannot create kernel thread for %s.",
		    sc->sc_name);
		uma_zdestroy(sc->sc_zone_64k); 
		uma_zdestroy(sc->sc_zone_16k); 
		uma_zdestroy(sc->sc_zone_4k); 
		g_destroy_geom(sc->sc_sync.ds_geom);
		mtx_destroy(&sc->sc_events_mtx);
		mtx_destroy(&sc->sc_queue_mtx);
		g_destroy_geom(sc->sc_geom);
		free(sc->sc_disks, M_RAID3);
		free(sc, M_RAID3);
		return (NULL);
	}

	G_RAID3_DEBUG(0, "Device %s created (id=%u).", sc->sc_name, sc->sc_id);

	sc->sc_rootmount = root_mount_hold("GRAID3");
	G_RAID3_DEBUG(1, "root_mount_hold %p", sc->sc_rootmount);

	/*
	 * Run timeout.
	 */
	timeout = atomic_load_acq_int(&g_raid3_timeout);
	callout_reset(&sc->sc_callout, timeout * hz, g_raid3_go, sc);
	return (sc->sc_geom);
}

int
g_raid3_destroy(struct g_raid3_softc *sc, boolean_t force)
{
	struct g_provider *pp;

	g_topology_assert();

	if (sc == NULL)
		return (ENXIO);
	pp = sc->sc_provider;
	if (pp != NULL && (pp->acr != 0 || pp->acw != 0 || pp->ace != 0)) {
		if (force) {
			G_RAID3_DEBUG(1, "Device %s is still open, so it "
			    "can't be definitely removed.", pp->name);
		} else {
			G_RAID3_DEBUG(1,
			    "Device %s is still open (r%dw%de%d).", pp->name,
			    pp->acr, pp->acw, pp->ace);
			return (EBUSY);
		}
	}

	sc->sc_flags |= G_RAID3_DEVICE_FLAG_DESTROY;
	sc->sc_flags |= G_RAID3_DEVICE_FLAG_WAIT;
	g_topology_unlock();
	G_RAID3_DEBUG(4, "%s: Waking up %p.", __func__, sc);
	mtx_lock(&sc->sc_queue_mtx);
	wakeup(sc);
	wakeup(&sc->sc_queue);
	mtx_unlock(&sc->sc_queue_mtx);
	G_RAID3_DEBUG(4, "%s: Sleeping %p.", __func__, &sc->sc_worker);
	while (sc->sc_worker != NULL)
		tsleep(&sc->sc_worker, PRIBIO, "r3:destroy", hz / 5);
	G_RAID3_DEBUG(4, "%s: Woken up %p.", __func__, &sc->sc_worker);
	g_topology_lock();
	g_raid3_destroy_device(sc);
	free(sc->sc_disks, M_RAID3);
	free(sc, M_RAID3);
	return (0);
}

static void
g_raid3_taste_orphan(struct g_consumer *cp)
{

	KASSERT(1 == 0, ("%s called while tasting %s.", __func__,
	    cp->provider->name));
}

static struct g_geom *
g_raid3_taste(struct g_class *mp, struct g_provider *pp, int flags __unused)
{
	struct g_raid3_metadata md;
	struct g_raid3_softc *sc;
	struct g_consumer *cp;
	struct g_geom *gp;
	int error;

	g_topology_assert();
	g_trace(G_T_TOPOLOGY, "%s(%s, %s)", __func__, mp->name, pp->name);
	G_RAID3_DEBUG(2, "Tasting %s.", pp->name);

	gp = g_new_geomf(mp, "raid3:taste");
	/* This orphan function should be never called. */
	gp->orphan = g_raid3_taste_orphan;
	cp = g_new_consumer(gp);
	g_attach(cp, pp);
	error = g_raid3_read_metadata(cp, &md);
	g_detach(cp);
	g_destroy_consumer(cp);
	g_destroy_geom(gp);
	if (error != 0)
		return (NULL);
	gp = NULL;

	if (md.md_provider[0] != '\0' && strcmp(md.md_provider, pp->name) != 0)
		return (NULL);
	if (md.md_provsize != 0 && md.md_provsize != pp->mediasize)
		return (NULL);
	if (g_raid3_debug >= 2)
		raid3_metadata_dump(&md);

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
		if (md.md_id != sc->sc_id) {
			G_RAID3_DEBUG(0, "Device %s already configured.",
			    sc->sc_name);
			return (NULL);
		}
		break;
	}
	if (gp == NULL) {
		gp = g_raid3_create(mp, &md);
		if (gp == NULL) {
			G_RAID3_DEBUG(0, "Cannot create device %s.",
			    md.md_name);
			return (NULL);
		}
		sc = gp->softc;
	}
	G_RAID3_DEBUG(1, "Adding disk %s to %s.", pp->name, gp->name);
	error = g_raid3_add_disk(sc, pp, &md);
	if (error != 0) {
		G_RAID3_DEBUG(0, "Cannot add disk %s to %s (error=%d).",
		    pp->name, gp->name, error);
		if (g_raid3_ndisks(sc, G_RAID3_DISK_STATE_NODISK) ==
		    sc->sc_ndisks) {
			g_raid3_destroy(sc, 1);
		}
		return (NULL);
	}
	return (gp);
}

static int
g_raid3_destroy_geom(struct gctl_req *req __unused, struct g_class *mp __unused,
    struct g_geom *gp)
{

	return (g_raid3_destroy(gp->softc, 0));
}

static void
g_raid3_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp,
    struct g_consumer *cp, struct g_provider *pp)
{
	struct g_raid3_softc *sc;

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
		struct g_raid3_disk *disk;

		disk = cp->private;
		if (disk == NULL)
			return;
		sbuf_printf(sb, "%s<Type>", indent);
		if (disk->d_no == sc->sc_ndisks - 1)
			sbuf_printf(sb, "PARITY");
		else
			sbuf_printf(sb, "DATA");
		sbuf_printf(sb, "</Type>\n");
		sbuf_printf(sb, "%s<Number>%u</Number>\n", indent,
		    (u_int)disk->d_no);
		if (disk->d_state == G_RAID3_DISK_STATE_SYNCHRONIZING) {
			sbuf_printf(sb, "%s<Synchronized>", indent);
			if (disk->d_sync.ds_offset_done == 0)
				sbuf_printf(sb, "0%%");
			else {
				sbuf_printf(sb, "%u%%",
				    (u_int)((disk->d_sync.ds_offset_done * 100) /
				    (sc->sc_mediasize / (sc->sc_ndisks - 1))));
			}
			sbuf_printf(sb, "</Synchronized>\n");
		}
		sbuf_printf(sb, "%s<SyncID>%u</SyncID>\n", indent,
		    disk->d_sync.ds_syncid);
		sbuf_printf(sb, "%s<GenID>%u</GenID>\n", indent, disk->d_genid);
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
			ADD_FLAG(G_RAID3_DISK_FLAG_DIRTY, "DIRTY");
			ADD_FLAG(G_RAID3_DISK_FLAG_HARDCODED, "HARDCODED");
			ADD_FLAG(G_RAID3_DISK_FLAG_SYNCHRONIZING,
			    "SYNCHRONIZING");
			ADD_FLAG(G_RAID3_DISK_FLAG_FORCE_SYNC, "FORCE_SYNC");
#undef	ADD_FLAG
		}
		sbuf_printf(sb, "</Flags>\n");
		sbuf_printf(sb, "%s<State>%s</State>\n", indent,
		    g_raid3_disk_state2str(disk->d_state));
	} else {
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
			ADD_FLAG(G_RAID3_DEVICE_FLAG_NOAUTOSYNC, "NOAUTOSYNC");
			ADD_FLAG(G_RAID3_DEVICE_FLAG_ROUND_ROBIN,
			    "ROUND-ROBIN");
			ADD_FLAG(G_RAID3_DEVICE_FLAG_VERIFY, "VERIFY");
#undef	ADD_FLAG
		}
		sbuf_printf(sb, "</Flags>\n");
		sbuf_printf(sb, "%s<Components>%u</Components>\n", indent,
		    sc->sc_ndisks);
		sbuf_printf(sb, "%s<State>%s</State>\n", indent,
		    g_raid3_device_state2str(sc->sc_state));
	}
}

static void
g_raid3_shutdown(void *arg, int howto)
{
	struct g_class *mp;
	struct g_geom *gp, *gp2;

	mp = arg;
	DROP_GIANT();
	g_topology_lock();
	LIST_FOREACH_SAFE(gp, &mp->geom, geom, gp2) {
		if (gp->softc == NULL)
			continue;
		g_raid3_destroy(gp->softc, 1);
	}
	g_topology_unlock();
	PICKUP_GIANT();
#if 0
	tsleep(&gp, PRIBIO, "r3:shutdown", hz * 20);
#endif
}

static void
g_raid3_init(struct g_class *mp)
{

	g_raid3_ehtag = EVENTHANDLER_REGISTER(shutdown_post_sync,
	    g_raid3_shutdown, mp, SHUTDOWN_PRI_FIRST);
	if (g_raid3_ehtag == NULL)
		G_RAID3_DEBUG(0, "Warning! Cannot register shutdown event.");
}

static void
g_raid3_fini(struct g_class *mp)
{

	if (g_raid3_ehtag == NULL)
		return;
	EVENTHANDLER_DEREGISTER(shutdown_post_sync, g_raid3_ehtag);
}

DECLARE_GEOM_CLASS(g_raid3_class, g_raid3);
