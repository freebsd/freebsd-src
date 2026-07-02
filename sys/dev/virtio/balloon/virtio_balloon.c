/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011, Bryan Venteicher <bryanv@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Driver for VirtIO memory balloon devices. */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/endian.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sglist.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/eventhandler.h>

#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/virtio/virtio.h>
#include <dev/virtio/virtqueue.h>
#include <dev/virtio/balloon/virtio_balloon.h>

#include "virtio_if.h"

struct vtballoon_softc {
	device_t		 vtballoon_dev;
	struct mtx		 vtballoon_mtx;
	uint64_t		 vtballoon_features;
	uint32_t		 vtballoon_flags;
#define VTBALLOON_FLAG_DETACH	 0x01
#define VTBALLOON_FLAG_GUEST_LOWMEM	 0x02
#define VTBALLOON_FLAG_WANTSIZE	 0x04

	struct virtqueue	*vtballoon_inflate_vq;
	struct virtqueue	*vtballoon_deflate_vq;
	struct virtqueue	*vtballoon_stats_vq;

	struct virtio_balloon_stat vtballoon_stats[VIRTIO_BALLOON_S_NR];

	uint32_t		 vtballoon_desired_npages;
	uint32_t		 vtballoon_current_npages;
	TAILQ_HEAD(,vm_page)	 vtballoon_pages;

	struct thread		*vtballoon_td;
	uint32_t		*vtballoon_page_frames;
	int			 vtballoon_timeout;
	int			 vtballoon_debug;
	uint64_t		 vtballoon_guest_lowmem_count;
	uint64_t		 vtballoon_guest_lowmem_pages;

	eventhandler_tag	 vtballoon_guest_lowmem_tag;
};

static struct virtio_feature_desc vtballoon_feature_desc[] = {
	{ VIRTIO_BALLOON_F_MUST_TELL_HOST,	"MustTellHost"	},
	{ VIRTIO_BALLOON_F_STATS_VQ,		"StatsVq"	},
	{ VIRTIO_BALLOON_F_DEFLATE_ON_OOM,	"DeflateOnOOM"	},
	{ VIRTIO_BALLOON_F_FREE_PAGE_HINT,	"FreePageHint"	},
	{ VIRTIO_BALLOON_F_PAGE_POISON,		"PagePoison"	},
	{ VIRTIO_BALLOON_F_PAGE_REPORTING,	"PageReporting"	},

	{ 0, NULL }
};

static int	vtballoon_probe(device_t);
static int	vtballoon_attach(device_t);
static int	vtballoon_detach(device_t);
static int	vtballoon_config_change(device_t);

static int	vtballoon_negotiate_features(struct vtballoon_softc *);
static int	vtballoon_setup_features(struct vtballoon_softc *);
static int	vtballoon_alloc_virtqueues(struct vtballoon_softc *);

static void	vtballoon_vq_intr(void *);
static void	vtballoon_stats_vq_intr(void *);
static int	vtballoon_update_stats(struct vtballoon_softc *);

static void	vtballoon_inflate(struct vtballoon_softc *, int);
static void	vtballoon_deflate(struct vtballoon_softc *, int);

static void	vtballoon_send_page_frames(struct vtballoon_softc *,
		    struct virtqueue *, int);

static void	vtballoon_pop(struct vtballoon_softc *);
static void	vtballoon_stop(struct vtballoon_softc *);

static vm_page_t
		vtballoon_alloc_page(struct vtballoon_softc *);
static void	vtballoon_free_page(struct vtballoon_softc *, vm_page_t);

static int	vtballoon_sleep(struct vtballoon_softc *);
static void	vtballoon_thread(void *);
static void	vtballoon_guest_lowmem(void *, int);
static void	vtballoon_setup_sysctl(struct vtballoon_softc *);

#define vtballoon_modern(_sc) \
    (((_sc)->vtballoon_features & VIRTIO_F_VERSION_1) != 0)

/* Features desired/implemented by this driver. */
#define VTBALLOON_FEATURES		\
    (VIRTIO_BALLOON_F_MUST_TELL_HOST |	\
     VIRTIO_BALLOON_F_STATS_VQ |	\
     VIRTIO_BALLOON_F_DEFLATE_ON_OOM)

/* Timeout between retries when the balloon needs inflating. */
#define VTBALLOON_LOWMEM_TIMEOUT	hz

/*
 * Maximum number of pages we'll request to inflate or deflate
 * the balloon in one virtqueue request. Both Linux and NetBSD
 * have settled on 256, doing up to 1MB at a time.
 */
#define VTBALLOON_PAGES_PER_REQUEST	256

/* Must be able to fix all pages frames in one page (segment). */
CTASSERT(VTBALLOON_PAGES_PER_REQUEST * sizeof(uint32_t) <= PAGE_SIZE);

#define VTBALLOON_MTX(_sc)		&(_sc)->vtballoon_mtx
#define VTBALLOON_LOCK_INIT(_sc, _name)	mtx_init(VTBALLOON_MTX((_sc)), _name, \
					    "VirtIO Balloon Lock", MTX_DEF)
#define VTBALLOON_LOCK(_sc)		mtx_lock(VTBALLOON_MTX((_sc)))
#define VTBALLOON_UNLOCK(_sc)		mtx_unlock(VTBALLOON_MTX((_sc)))
#define VTBALLOON_LOCK_DESTROY(_sc)	mtx_destroy(VTBALLOON_MTX((_sc)))

static device_method_t vtballoon_methods[] = {
	/* Device methods. */
	DEVMETHOD(device_probe,		vtballoon_probe),
	DEVMETHOD(device_attach,	vtballoon_attach),
	DEVMETHOD(device_detach,	vtballoon_detach),

	/* VirtIO methods. */
	DEVMETHOD(virtio_config_change, vtballoon_config_change),

	DEVMETHOD_END
};

static driver_t vtballoon_driver = {
	"vtballoon",
	vtballoon_methods,
	sizeof(struct vtballoon_softc)
};

VIRTIO_DRIVER_MODULE(virtio_balloon, vtballoon_driver, 0, 0);
MODULE_VERSION(virtio_balloon, 1);
MODULE_DEPEND(virtio_balloon, virtio, 1, 1, 1);

VIRTIO_SIMPLE_PNPINFO(virtio_balloon, VIRTIO_ID_BALLOON,
    "VirtIO Balloon Adapter");

static int
vtballoon_probe(device_t dev)
{
	return (VIRTIO_SIMPLE_PROBE(dev, virtio_balloon));
}

static int
vtballoon_attach(device_t dev)
{
	struct vtballoon_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->vtballoon_dev = dev;
	virtio_set_feature_desc(dev, vtballoon_feature_desc);

	VTBALLOON_LOCK_INIT(sc, device_get_nameunit(dev));
	TAILQ_INIT(&sc->vtballoon_pages);

	vtballoon_setup_sysctl(sc);

	error = vtballoon_setup_features(sc);
	if (error) {
		device_printf(dev, "cannot setup features\n");
		goto fail;
	}

	sc->vtballoon_page_frames = malloc(VTBALLOON_PAGES_PER_REQUEST *
	    sizeof(uint32_t), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->vtballoon_page_frames == NULL) {
		error = ENOMEM;
		device_printf(dev,
		    "cannot allocate page frame request array\n");
		goto fail;
	}

	error = vtballoon_alloc_virtqueues(sc);
	if (error) {
		device_printf(dev, "cannot allocate virtqueues\n");
		goto fail;
	}

	error = virtio_setup_intr(dev, INTR_TYPE_MISC);
	if (error) {
		device_printf(dev, "cannot setup virtqueue interrupts\n");
		goto fail;
	}

	error = kthread_add(vtballoon_thread, sc, NULL, &sc->vtballoon_td,
	    0, 0, "virtio_balloon");
	if (error) {
		device_printf(dev, "cannot create balloon kthread\n");
		goto fail;
	}

	/*
	 * Prime the resize flag so the thread evaluates the host's
	 * desired balloon size on first wakeup.  This handles the
	 * case where the host set num_pages before the guest booted.
	 */
	sc->vtballoon_flags |= VTBALLOON_FLAG_WANTSIZE;

	virtqueue_enable_intr(sc->vtballoon_inflate_vq);
	virtqueue_enable_intr(sc->vtballoon_deflate_vq);

	/*
	 * Register for guest low-memory events.  vm_lowmem is raised by the
	 * FreeBSD guest when its own VM subsystem is short of pages; it is not
	 * a host request to reclaim memory.  Deflating the balloon returns
	 * pages to the guest before OOM handling.
	 */
	if (virtio_with_feature(dev, VIRTIO_BALLOON_F_DEFLATE_ON_OOM)) {
		sc->vtballoon_guest_lowmem_tag = EVENTHANDLER_REGISTER(vm_lowmem,
		    vtballoon_guest_lowmem, sc, EVENTHANDLER_PRI_FIRST);
	}

	/*
	 * Prime the stats virtqueue with one buffer so the hypervisor
	 * can use it to request guest memory statistics.
	 */
	if (sc->vtballoon_stats_vq != NULL) {
		struct sglist sg;
		struct sglist_seg segs[1];
		int nstats, error __diagused;

		nstats = vtballoon_update_stats(sc);

		sglist_init(&sg, 1, segs);
		error = sglist_append(&sg, sc->vtballoon_stats,
		    sizeof(struct virtio_balloon_stat) * nstats);
		KASSERT(error == 0, ("error adding stats to sglist"));

		error = virtqueue_enqueue(sc->vtballoon_stats_vq,
		    sc, &sg, 1, 0);
		KASSERT(error == 0,
		    ("error enqueuing stats to virtqueue"));
		virtqueue_notify(sc->vtballoon_stats_vq);
		virtqueue_enable_intr(sc->vtballoon_stats_vq);
	}

fail:
	if (error)
		vtballoon_detach(dev);

	return (error);
}

static int
vtballoon_detach(device_t dev)
{
	struct vtballoon_softc *sc;

	sc = device_get_softc(dev);

	if (sc->vtballoon_guest_lowmem_tag != NULL) {
		EVENTHANDLER_DEREGISTER(vm_lowmem,
		    sc->vtballoon_guest_lowmem_tag);
		sc->vtballoon_guest_lowmem_tag = NULL;
	}

	if (sc->vtballoon_td != NULL) {
		VTBALLOON_LOCK(sc);
		sc->vtballoon_flags |= VTBALLOON_FLAG_DETACH;
		wakeup_one(sc);
		msleep(sc->vtballoon_td, VTBALLOON_MTX(sc), 0, "vtbdth", 0);
		VTBALLOON_UNLOCK(sc);

		sc->vtballoon_td = NULL;
	}

	if (device_is_attached(dev)) {
		vtballoon_pop(sc);
		vtballoon_stop(sc);
	}

	if (sc->vtballoon_page_frames != NULL) {
		free(sc->vtballoon_page_frames, M_DEVBUF);
		sc->vtballoon_page_frames = NULL;
	}

	VTBALLOON_LOCK_DESTROY(sc);

	return (0);
}

static int
vtballoon_config_change(device_t dev)
{
	struct vtballoon_softc *sc;

	sc = device_get_softc(dev);

	VTBALLOON_LOCK(sc);
	sc->vtballoon_flags |= VTBALLOON_FLAG_WANTSIZE;
	wakeup_one(sc);
	VTBALLOON_UNLOCK(sc);

	return (1);
}

static int
vtballoon_negotiate_features(struct vtballoon_softc *sc)
{
	device_t dev;
	uint64_t features;

	dev = sc->vtballoon_dev;
	features = VTBALLOON_FEATURES;

	sc->vtballoon_features = virtio_negotiate_features(dev, features);
	return (virtio_finalize_features(dev));
}

static int
vtballoon_setup_features(struct vtballoon_softc *sc)
{
	int error;

	error = vtballoon_negotiate_features(sc);
	if (error)
		return (error);

	return (0);
}

static int
vtballoon_alloc_virtqueues(struct vtballoon_softc *sc)
{
	device_t dev;
	struct vq_alloc_info vq_info[3];
	int nvqs;

	dev = sc->vtballoon_dev;
	nvqs = 2;

	VQ_ALLOC_INFO_INIT(&vq_info[0], 0, vtballoon_vq_intr, sc,
	    &sc->vtballoon_inflate_vq, "%s inflate", device_get_nameunit(dev));

	VQ_ALLOC_INFO_INIT(&vq_info[1], 0, vtballoon_vq_intr, sc,
	    &sc->vtballoon_deflate_vq, "%s deflate", device_get_nameunit(dev));

	if (virtio_with_feature(dev, VIRTIO_BALLOON_F_STATS_VQ)) {
		VQ_ALLOC_INFO_INIT(&vq_info[2], 0,
		    vtballoon_stats_vq_intr, sc,
		    &sc->vtballoon_stats_vq, "%s stats",
		    device_get_nameunit(dev));
		nvqs = 3;
	}

	return (virtio_alloc_virtqueues(dev, nvqs, vq_info));
}

static void
vtballoon_vq_intr(void *xsc)
{
	struct vtballoon_softc *sc;

	sc = xsc;

	VTBALLOON_LOCK(sc);
	wakeup_one(sc);
	VTBALLOON_UNLOCK(sc);
}

static void
vtballoon_inflate(struct vtballoon_softc *sc, int npages)
{
	struct virtqueue *vq;
	vm_page_t m;
	int i;

	vq = sc->vtballoon_inflate_vq;

	if (npages > VTBALLOON_PAGES_PER_REQUEST)
		npages = VTBALLOON_PAGES_PER_REQUEST;

	for (i = 0; i < npages; i++) {
		if ((m = vtballoon_alloc_page(sc)) == NULL) {
			sc->vtballoon_timeout = VTBALLOON_LOWMEM_TIMEOUT;
			break;
		}

		sc->vtballoon_page_frames[i] =
		    VM_PAGE_TO_PHYS(m) >> VIRTIO_BALLOON_PFN_SHIFT;

		KASSERT(m->a.queue == PQ_NONE,
		    ("%s: allocated page %p on queue", __func__, m));
		TAILQ_INSERT_TAIL(&sc->vtballoon_pages, m, plinks.q);
	}

	if (i > 0)
		vtballoon_send_page_frames(sc, vq, i);
}

static void
vtballoon_deflate(struct vtballoon_softc *sc, int npages)
{
	TAILQ_HEAD(, vm_page) free_pages;
	struct virtqueue *vq;
	vm_page_t m;
	int i;

	vq = sc->vtballoon_deflate_vq;
	TAILQ_INIT(&free_pages);

	if (npages > VTBALLOON_PAGES_PER_REQUEST)
		npages = VTBALLOON_PAGES_PER_REQUEST;

	for (i = 0; i < npages; i++) {
		m = TAILQ_FIRST(&sc->vtballoon_pages);
		KASSERT(m != NULL, ("%s: no more pages to deflate", __func__));

		sc->vtballoon_page_frames[i] =
		    VM_PAGE_TO_PHYS(m) >> VIRTIO_BALLOON_PFN_SHIFT;

		TAILQ_REMOVE(&sc->vtballoon_pages, m, plinks.q);
		TAILQ_INSERT_TAIL(&free_pages, m, plinks.q);
	}

	if (i > 0) {
		/* Always tell host first before freeing the pages. */
		vtballoon_send_page_frames(sc, vq, i);

		while ((m = TAILQ_FIRST(&free_pages)) != NULL) {
			TAILQ_REMOVE(&free_pages, m, plinks.q);
			vtballoon_free_page(sc, m);
		}
	}

	KASSERT((TAILQ_EMPTY(&sc->vtballoon_pages) &&
	    sc->vtballoon_current_npages == 0) ||
	    (!TAILQ_EMPTY(&sc->vtballoon_pages) &&
	    sc->vtballoon_current_npages != 0),
	    ("%s: bogus page count %d", __func__,
	    sc->vtballoon_current_npages));
}

static void
vtballoon_send_page_frames(struct vtballoon_softc *sc, struct virtqueue *vq,
    int npages)
{
	struct sglist sg;
	struct sglist_seg segs[1];
	void *c;
	int error __diagused;

	sglist_init(&sg, 1, segs);

	error = sglist_append(&sg, sc->vtballoon_page_frames,
	    npages * sizeof(uint32_t));
	KASSERT(error == 0, ("error adding page frames to sglist"));

	error = virtqueue_enqueue(vq, vq, &sg, 1, 0);
	KASSERT(error == 0, ("error enqueuing page frames to virtqueue"));
	virtqueue_notify(vq);

	/*
	 * Inflate and deflate operations are done synchronously. The
	 * interrupt handler will wake us up.
	 */
	VTBALLOON_LOCK(sc);
	while ((c = virtqueue_dequeue(vq, NULL)) == NULL)
		msleep(sc, VTBALLOON_MTX(sc), 0, "vtbspf", 0);
	virtqueue_enable_intr(vq);
	VTBALLOON_UNLOCK(sc);

	KASSERT(c == vq, ("unexpected balloon operation response"));
}

static void
vtballoon_pop(struct vtballoon_softc *sc)
{

	while (!TAILQ_EMPTY(&sc->vtballoon_pages))
		vtballoon_deflate(sc, sc->vtballoon_current_npages);
}

static void
vtballoon_stop(struct vtballoon_softc *sc)
{

	virtqueue_disable_intr(sc->vtballoon_inflate_vq);
	virtqueue_disable_intr(sc->vtballoon_deflate_vq);
	if (sc->vtballoon_stats_vq != NULL)
		virtqueue_disable_intr(sc->vtballoon_stats_vq);

	virtio_stop(sc->vtballoon_dev);
}

static vm_page_t
vtballoon_alloc_page(struct vtballoon_softc *sc)
{
	vm_page_t m;

	m = vm_page_alloc_noobj(VM_ALLOC_NODUMP);
	if (m != NULL)
		sc->vtballoon_current_npages++;

	return (m);
}

static void
vtballoon_free_page(struct vtballoon_softc *sc, vm_page_t m)
{

	vm_page_free(m);
	sc->vtballoon_current_npages--;
}

static uint32_t
vtballoon_desired_size(struct vtballoon_softc *sc)
{
	uint32_t desired;

	desired = virtio_read_dev_config_4(sc->vtballoon_dev,
	    offsetof(struct virtio_balloon_config, num_pages));

	if (vtballoon_modern(sc))
		return (desired);
	else
		return (le32toh(desired));
}

static void
vtballoon_update_size(struct vtballoon_softc *sc)
{
	uint32_t npages;

	npages = sc->vtballoon_current_npages;
	if (!vtballoon_modern(sc))
		npages = htole32(npages);

	virtio_write_dev_config_4(sc->vtballoon_dev,
	    offsetof(struct virtio_balloon_config, actual), npages);
}

static int
vtballoon_sleep(struct vtballoon_softc *sc)
{
	int rc, timeout;
	uint32_t current, desired;

	rc = 0;
	current = sc->vtballoon_current_npages;

	VTBALLOON_LOCK(sc);
	for (;;) {
		if (sc->vtballoon_flags & VTBALLOON_FLAG_DETACH) {
			rc = 1;
			break;
		}

		/* Guest low-memory event: break out immediately to deflate. */
		if (sc->vtballoon_flags & VTBALLOON_FLAG_GUEST_LOWMEM)
			break;

		/*
		 * Only evaluate the host's desired balloon size when a
		 * config change interrupt has been received.  This
		 * prevents the thread from re-inflating pages that were
		 * just freed in response to a guest low-memory event.
		 */
		if (sc->vtballoon_flags & VTBALLOON_FLAG_WANTSIZE) {
			desired = vtballoon_desired_size(sc);
			sc->vtballoon_desired_npages = desired;

			/*
			 * If given, use non-zero timeout on the first
			 * time through the loop.  On subsequent times,
			 * timeout will be zero so we will reevaluate
			 * the desired size of the balloon and break
			 * out to retry if needed.
			 */
			timeout = sc->vtballoon_timeout;
			sc->vtballoon_timeout = 0;

			if (current > desired)
				break;
			if (current < desired && timeout == 0)
				break;
			if (current < desired) {
				/* Retry after timeout. */
				msleep(sc, VTBALLOON_MTX(sc), 0,
				    "vtbslp", timeout);
				continue;
			}

			/* Target reached; clear the flag. */
			sc->vtballoon_flags &= ~VTBALLOON_FLAG_WANTSIZE;
		}

		msleep(sc, VTBALLOON_MTX(sc), 0, "vtbslp", 0);
	}
	VTBALLOON_UNLOCK(sc);

	return (rc);
}

/*
 * Maximum number of pages to deflate in response to a guest low-memory event.
 * The virtio spec does not prescribe a specific amount; we use a fixed
 * page count so that the relief scales naturally with page size.
 */
#define VTBALLOON_GUEST_LOWMEM_DEFLATE_PAGES	256

static void
vtballoon_thread(void *xsc)
{
	struct vtballoon_softc *sc;
	uint32_t current, desired;
	int guest_lowmem;

	sc = xsc;

	for (;;) {
		if (vtballoon_sleep(sc) != 0)
			break;

		/*
		 * Check and clear the guest low-memory flag.  Also clear any
		 * pending host resize request so that we do not
		 * immediately re-inflate the pages just released to
		 * the guest VM subsystem.  The host will send a new
		 * config change interrupt if it still requires a
		 * larger balloon.
		 */
		VTBALLOON_LOCK(sc);
		guest_lowmem = sc->vtballoon_flags &
		    VTBALLOON_FLAG_GUEST_LOWMEM;
		if (guest_lowmem)
			sc->vtballoon_flags &=
			    ~(VTBALLOON_FLAG_GUEST_LOWMEM |
			    VTBALLOON_FLAG_WANTSIZE);
		VTBALLOON_UNLOCK(sc);

		if (guest_lowmem && sc->vtballoon_current_npages > 0) {
			/*
			 * Guest low memory: deflate up to
			 * VTBALLOON_GUEST_LOWMEM_DEFLATE_PAGES regardless of
			 * the host's desired balloon size.  This returns pages
			 * to the guest VM subsystem before the OOM killer is
			 * invoked.
			 */
			desired = sc->vtballoon_current_npages -
			    MIN(sc->vtballoon_current_npages,
			    VTBALLOON_GUEST_LOWMEM_DEFLATE_PAGES);

			sc->vtballoon_guest_lowmem_count++;
			sc->vtballoon_guest_lowmem_pages +=
			    sc->vtballoon_current_npages - desired;

			if (__predict_false(sc->vtballoon_debug > 0))
				device_printf(sc->vtballoon_dev,
				    "guest low memory, deflating balloon by "
				    "%d pages (current: %d)\n",
				    sc->vtballoon_current_npages - desired,
				    sc->vtballoon_current_npages);

			while (sc->vtballoon_current_npages > desired)
				vtballoon_deflate(sc,
				    sc->vtballoon_current_npages - desired);

			vtballoon_update_size(sc);
			continue;
		}

		current = sc->vtballoon_current_npages;
		desired = sc->vtballoon_desired_npages;

		if (desired != current) {
			if (desired > current)
				vtballoon_inflate(sc, desired - current);
			else
				vtballoon_deflate(sc, current - desired);

			vtballoon_update_size(sc);
		}
	}

	kthread_exit();
}

/*
 * Estimate FreeBSD VM headroom without dipping below the page daemon's free
 * and inactive targets.  Do not count active or laundry pages: active pages
 * are the guest's working set, and laundry pages may require writeback or
 * swap before reuse.
 */
static uint64_t
vtballoon_mem_available(void)
{
	int64_t available;

	available = (int64_t)vm_free_count() + vm_inactive_count();
	available -= (int64_t)vm_cnt.v_free_target + vm_cnt.v_inactive_target;

	if (available < 0)
		available = 0;
	return ((uint64_t)available * PAGE_SIZE);
}

static int
vtballoon_update_stats(struct vtballoon_softc *sc)
{
	struct virtio_balloon_stat *stat;
	int idx;
	bool modern;

	idx = 0;
	modern = vtballoon_modern(sc);
	stat = sc->vtballoon_stats;

#define VTBALLOON_SET_STAT(tag_, val_) do {			\
	stat[idx].tag = modern ? htole16(tag_) : (tag_);	\
	stat[idx].val = modern ? htole64(val_) : (val_);	\
	idx++;							\
} while (0)

	VTBALLOON_SET_STAT(VIRTIO_BALLOON_S_MEMTOT,
	    (uint64_t)vm_cnt.v_page_count * PAGE_SIZE);
	VTBALLOON_SET_STAT(VIRTIO_BALLOON_S_MEMFREE,
	    (uint64_t)vm_free_count() * PAGE_SIZE);
	VTBALLOON_SET_STAT(VIRTIO_BALLOON_S_AVAIL,
	    vtballoon_mem_available());
	VTBALLOON_SET_STAT(VIRTIO_BALLOON_S_SWAP_IN,
	    VM_CNT_FETCH(v_swappgsin) * PAGE_SIZE);
	VTBALLOON_SET_STAT(VIRTIO_BALLOON_S_SWAP_OUT,
	    VM_CNT_FETCH(v_swappgsout) * PAGE_SIZE);
	VTBALLOON_SET_STAT(VIRTIO_BALLOON_S_MINFLT,
	    VM_CNT_FETCH(v_vm_faults));
	VTBALLOON_SET_STAT(VIRTIO_BALLOON_S_MAJFLT,
	    VM_CNT_FETCH(v_io_faults));
	/*
	 * Map CACHES to the inactive page queue.  The virtio spec defines
	 * this as "disk caches" (Linux: page cache + reclaimable slab).
	 * FreeBSD's inactive queue is the closest approximation -- it
	 * contains pages eligible for reclaim, both file-backed and
	 * anonymous, similar in spirit to Linux's reclaimable memory.
	 */
	VTBALLOON_SET_STAT(VIRTIO_BALLOON_S_CACHES,
	    (uint64_t)vm_inactive_count() * PAGE_SIZE);

#undef VTBALLOON_SET_STAT

	return (idx);
}

/*
 * The stats virtqueue works in reverse: the host initiates a request by
 * returning a buffer we previously placed in the virtqueue. We then
 * refill the virtqueue with fresh statistics.
 */
static void
vtballoon_stats_vq_intr(void *xsc)
{
	struct vtballoon_softc *sc;
	struct virtqueue *vq;
	struct sglist sg;
	struct sglist_seg segs[1];
	int nstats, error __diagused;

	sc = xsc;
	vq = sc->vtballoon_stats_vq;

	/* Retrieve the buffer that the host returned to us. */
	if (virtqueue_dequeue(vq, NULL) == NULL)
		return;

	/* Collect fresh statistics and re-enqueue. */
	nstats = vtballoon_update_stats(sc);

	sglist_init(&sg, 1, segs);
	error = sglist_append(&sg, sc->vtballoon_stats,
	    sizeof(struct virtio_balloon_stat) * nstats);
	KASSERT(error == 0, ("error adding stats to sglist"));

	error = virtqueue_enqueue(vq, sc, &sg, 1, 0);
	KASSERT(error == 0, ("error enqueuing stats to virtqueue"));
	virtqueue_notify(vq);
	virtqueue_enable_intr(vq);
}

/*
 * Handler for the guest vm_lowmem event.  The FreeBSD guest's pagedaemon
 * fires this when the guest is short of physical pages; it is unrelated to
 * the host asking the guest to give memory back.  We respond by waking the
 * balloon kthread to deflate pages back to the guest, giving the VM subsystem
 * more free pages before it resorts to the OOM killer.
 *
 * This callback runs in the pagedaemon context, so we must not block.
 * We simply set a flag and wake the kthread, following the same pattern as
 * vtballoon_vq_intr() and vtballoon_config_change().
 */
static void
vtballoon_guest_lowmem(void *arg, int flags)
{
	struct vtballoon_softc *sc;

	sc = arg;

	/* Only respond to physical page shortage. */
	if ((flags & VM_LOW_PAGES) == 0)
		return;

	/* Nothing to deflate. */
	if (sc->vtballoon_current_npages == 0)
		return;

	VTBALLOON_LOCK(sc);
	sc->vtballoon_flags |= VTBALLOON_FLAG_GUEST_LOWMEM;
	wakeup_one(sc);
	VTBALLOON_UNLOCK(sc);
}

static void
vtballoon_setup_sysctl(struct vtballoon_softc *sc)
{
	device_t dev;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	struct sysctl_oid_list *child;

	dev = sc->vtballoon_dev;
	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);
	child = SYSCTL_CHILDREN(tree);

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "desired",
	    CTLFLAG_RD, &sc->vtballoon_desired_npages, sizeof(uint32_t),
	    "Desired balloon size in pages");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "current",
	    CTLFLAG_RD, &sc->vtballoon_current_npages, sizeof(uint32_t),
	    "Current balloon size in pages");

	SYSCTL_ADD_INT(ctx, child, OID_AUTO, "debug",
	    CTLFLAG_RWTUN, &sc->vtballoon_debug, 0,
	    "Debug level");

	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "guest_lowmem_deflations",
	    CTLFLAG_RD, &sc->vtballoon_guest_lowmem_count,
	    "Number of guest low-memory deflation events");

	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "guest_lowmem_pages_freed",
	    CTLFLAG_RD, &sc->vtballoon_guest_lowmem_pages,
	    "Total pages freed by guest low-memory deflation");
}
