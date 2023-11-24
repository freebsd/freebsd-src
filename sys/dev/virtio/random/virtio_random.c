/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013, Bryan Venteicher <bryanv@FreeBSD.org>
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

/* Driver for VirtIO entropy device. */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sglist.h>
#include <sys/random.h>
#include <sys/stdatomic.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>

#include <dev/random/randomdev.h>
#include <dev/random/random_harvestq.h>
#include <dev/virtio/virtio.h>
#include <dev/virtio/virtqueue.h>

struct vtrnd_softc {
	device_t		 vtrnd_dev;
	uint64_t		 vtrnd_features;
	struct virtqueue	*vtrnd_vq;
	eventhandler_tag	 eh;
	bool			 inactive;
	struct sglist		 *vtrnd_sg;
	uint32_t		 *vtrnd_value;
};

static int	vtrnd_modevent(module_t, int, void *);

static int	vtrnd_probe(device_t);
static int	vtrnd_attach(device_t);
static int	vtrnd_detach(device_t);
static int	vtrnd_shutdown(device_t);

static int	vtrnd_negotiate_features(struct vtrnd_softc *);
static int	vtrnd_setup_features(struct vtrnd_softc *);
static int	vtrnd_alloc_virtqueue(struct vtrnd_softc *);
static int	vtrnd_harvest(struct vtrnd_softc *, void *, size_t *);
static void	vtrnd_enqueue(struct vtrnd_softc *sc);
static unsigned	vtrnd_read(void *, unsigned);

#define VTRND_FEATURES	0

static struct virtio_feature_desc vtrnd_feature_desc[] = {
	{ 0, NULL }
};

static struct random_source random_vtrnd = {
	.rs_ident = "VirtIO Entropy Adapter",
	.rs_source = RANDOM_PURE_VIRTIO,
	.rs_read = vtrnd_read,
};

/* Kludge for API limitations of random(4). */
static _Atomic(struct vtrnd_softc *) g_vtrnd_softc;

static device_method_t vtrnd_methods[] = {
	/* Device methods. */
	DEVMETHOD(device_probe,		vtrnd_probe),
	DEVMETHOD(device_attach,	vtrnd_attach),
	DEVMETHOD(device_detach,	vtrnd_detach),
	DEVMETHOD(device_shutdown,	vtrnd_shutdown),

	DEVMETHOD_END
};

static driver_t vtrnd_driver = {
	"vtrnd",
	vtrnd_methods,
	sizeof(struct vtrnd_softc)
};

VIRTIO_DRIVER_MODULE(virtio_random, vtrnd_driver, vtrnd_modevent, NULL);
MODULE_VERSION(virtio_random, 1);
MODULE_DEPEND(virtio_random, virtio, 1, 1, 1);
MODULE_DEPEND(virtio_random, random_device, 1, 1, 1);

VIRTIO_SIMPLE_PNPINFO(virtio_random, VIRTIO_ID_ENTROPY,
    "VirtIO Entropy Adapter");

static int
vtrnd_modevent(module_t mod, int type, void *unused)
{
	int error;

	switch (type) {
	case MOD_LOAD:
	case MOD_QUIESCE:
	case MOD_UNLOAD:
	case MOD_SHUTDOWN:
		error = 0;
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

static int
vtrnd_probe(device_t dev)
{
	return (VIRTIO_SIMPLE_PROBE(dev, virtio_random));
}

static int
vtrnd_attach(device_t dev)
{
	struct vtrnd_softc *sc, *exp;
	size_t len;
	int error;

	sc = device_get_softc(dev);
	sc->vtrnd_dev = dev;
	virtio_set_feature_desc(dev, vtrnd_feature_desc);

	len = sizeof(*sc->vtrnd_value) * HARVESTSIZE;
	sc->vtrnd_value = malloc_aligned(len, len, M_DEVBUF, M_WAITOK);
	sc->vtrnd_sg = sglist_build(sc->vtrnd_value, len, M_WAITOK);

	error = vtrnd_setup_features(sc);
	if (error) {
		device_printf(dev, "cannot setup features\n");
		goto fail;
	}

	error = vtrnd_alloc_virtqueue(sc);
	if (error) {
		device_printf(dev, "cannot allocate virtqueue\n");
		goto fail;
	}

	exp = NULL;
	if (!atomic_compare_exchange_strong_explicit(&g_vtrnd_softc, &exp, sc,
	    memory_order_release, memory_order_acquire)) {
		error = EEXIST;
		goto fail;
	}

	sc->eh = EVENTHANDLER_REGISTER(shutdown_post_sync,
		vtrnd_shutdown, dev, SHUTDOWN_PRI_LAST + 1); /* ??? */
	if (sc->eh == NULL) {
		device_printf(dev, "Shutdown event registration failed\n");
		error = ENXIO;
		goto fail;
	}

	sc->inactive = false;
	random_source_register(&random_vtrnd);

	vtrnd_enqueue(sc);

fail:
	if (error)
		vtrnd_detach(dev);

	return (error);
}

static int
vtrnd_detach(device_t dev)
{
	struct vtrnd_softc *sc;
	uint32_t rdlen;

	sc = device_get_softc(dev);
	KASSERT(
	    atomic_load_explicit(&g_vtrnd_softc, memory_order_acquire) == sc,
	    ("only one global instance at a time"));

	sc->inactive = true;
	if (sc->eh != NULL) {
		EVENTHANDLER_DEREGISTER(shutdown_post_sync, sc->eh);
		sc->eh = NULL;
	}
	random_source_deregister(&random_vtrnd);

	/* clear the queue */
	virtqueue_poll(sc->vtrnd_vq, &rdlen);

	atomic_store_explicit(&g_vtrnd_softc, NULL, memory_order_release);
	sglist_free(sc->vtrnd_sg);
	zfree(sc->vtrnd_value, M_DEVBUF);
	return (0);
}

static int
vtrnd_shutdown(device_t dev)
{
	struct vtrnd_softc *sc;

	sc = device_get_softc(dev);
	sc->inactive = true;

	return(0);
}

static int
vtrnd_negotiate_features(struct vtrnd_softc *sc)
{
	device_t dev;
	uint64_t features;

	dev = sc->vtrnd_dev;
	features = VTRND_FEATURES;

	sc->vtrnd_features = virtio_negotiate_features(dev, features);
	return (virtio_finalize_features(dev));
}

static int
vtrnd_setup_features(struct vtrnd_softc *sc)
{
	int error;

	error = vtrnd_negotiate_features(sc);
	if (error)
		return (error);

	return (0);
}

static int
vtrnd_alloc_virtqueue(struct vtrnd_softc *sc)
{
	device_t dev;
	struct vq_alloc_info vq_info;

	dev = sc->vtrnd_dev;

	VQ_ALLOC_INFO_INIT(&vq_info, 0, NULL, sc, &sc->vtrnd_vq,
	    "%s request", device_get_nameunit(dev));

	return (virtio_alloc_virtqueues(dev, 1, &vq_info));
}

static void
vtrnd_enqueue(struct vtrnd_softc *sc)
{
	struct virtqueue *vq;
	int error __diagused;

	vq = sc->vtrnd_vq;

	KASSERT(virtqueue_empty(vq), ("%s: non-empty queue", __func__));

	error = virtqueue_enqueue(vq, sc, sc->vtrnd_sg, 0, 1);
	KASSERT(error == 0, ("%s: virtqueue_enqueue returned error: %d",
	    __func__, error));

	virtqueue_notify(vq);
}

static int
vtrnd_harvest(struct vtrnd_softc *sc, void *buf, size_t *sz)
{
	struct virtqueue *vq;
	void *cookie;
	uint32_t rdlen;

	if (sc->inactive)
		return (EDEADLK);

	vq = sc->vtrnd_vq;

	cookie = virtqueue_dequeue(vq, &rdlen);
	if (cookie == NULL)
		return (EAGAIN);
	KASSERT(cookie == sc, ("%s: cookie mismatch", __func__));

	*sz = MIN(rdlen, *sz);
	memcpy(buf, sc->vtrnd_value, *sz);

	vtrnd_enqueue(sc);

	return (0);
}

static unsigned
vtrnd_read(void *buf, unsigned usz)
{
	struct vtrnd_softc *sc;
	size_t sz;
	int error;

	sc = g_vtrnd_softc;
	if (sc == NULL)
		return (0);

	sz = usz;
	error = vtrnd_harvest(sc, buf, &sz);
	if (error != 0)
		return (0);

	return (sz);
}
