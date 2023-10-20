/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Arm Ltd
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

/* Driver for VirtIO SCMI device. */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/sglist.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>

#include <dev/virtio/virtio.h>
#include <dev/virtio/virtqueue.h>
#include <dev/virtio/scmi/virtio_scmi.h>

struct vtscmi_pdu {
	enum vtscmi_chan	chan;
	struct sglist		sg;
	struct sglist_seg	segs[2];
	void			*buf;
	SLIST_ENTRY(vtscmi_pdu)	next;
};

struct vtscmi_queue {
	device_t				dev;
	int					vq_id;
	unsigned int				vq_sz;
	struct virtqueue			*vq;
	struct mtx				vq_mtx;
	struct vtscmi_pdu			*pdus;
	SLIST_HEAD(pdus_head, vtscmi_pdu)	p_head;
	struct mtx				p_mtx;
	virtio_scmi_rx_callback_t		*rx_callback;
	void					*priv;
};

struct vtscmi_softc {
	device_t	vtscmi_dev;
	uint64_t	vtscmi_features;
	uint8_t		vtscmi_vqs_cnt;
	struct vtscmi_queue	vtscmi_queues[VIRTIO_SCMI_CHAN_MAX];
	bool		has_p2a;
	bool		has_shared;
};

static device_t vtscmi_dev;

static int vtscmi_modevent(module_t, int, void *);

static int	vtscmi_probe(device_t);
static int	vtscmi_attach(device_t);
static int	vtscmi_detach(device_t);
static int	vtscmi_shutdown(device_t);
static int	vtscmi_negotiate_features(struct vtscmi_softc *);
static int	vtscmi_setup_features(struct vtscmi_softc *);
static void	vtscmi_vq_intr(void *);
static int	vtscmi_alloc_virtqueues(struct vtscmi_softc *);
static int	vtscmi_alloc_queues(struct vtscmi_softc *);
static void	vtscmi_free_queues(struct vtscmi_softc *);
static void	*virtio_scmi_pdu_get(struct vtscmi_queue *, void *,
    unsigned int, unsigned int);
static void	virtio_scmi_pdu_put(device_t, struct vtscmi_pdu *);

static struct virtio_feature_desc vtscmi_feature_desc[] = {
	{ VIRTIO_SCMI_F_P2A_CHANNELS, "P2AChannel" },
	{ VIRTIO_SCMI_F_SHARED_MEMORY, "SharedMem" },
	{ 0, NULL }
};

static device_method_t vtscmi_methods[] = {
	/* Device methods. */
	DEVMETHOD(device_probe,		vtscmi_probe),
	DEVMETHOD(device_attach,	vtscmi_attach),
	DEVMETHOD(device_detach,	vtscmi_detach),
	DEVMETHOD(device_shutdown,	vtscmi_shutdown),

	DEVMETHOD_END
};

static driver_t vtscmi_driver = {
	"vtscmi",
	vtscmi_methods,
	sizeof(struct vtscmi_softc)
};

VIRTIO_DRIVER_MODULE(virtio_scmi, vtscmi_driver, vtscmi_modevent, NULL);
MODULE_VERSION(virtio_scmi, 1);
MODULE_DEPEND(virtio_scmi, virtio, 1, 1, 1);

VIRTIO_SIMPLE_PNPINFO(virtio_scmi, VIRTIO_ID_SCMI, "VirtIO SCMI Adapter");

static int
vtscmi_modevent(module_t mod, int type, void *unused)
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
vtscmi_probe(device_t dev)
{
	return (VIRTIO_SIMPLE_PROBE(dev, virtio_scmi));
}

static int
vtscmi_attach(device_t dev)
{
	struct vtscmi_softc *sc;
	int error;

	/* Only one SCMI device per-agent */
	if (vtscmi_dev != NULL)
		return (EEXIST);

	sc = device_get_softc(dev);
	sc->vtscmi_dev = dev;

	virtio_set_feature_desc(dev, vtscmi_feature_desc);
	error = vtscmi_setup_features(sc);
	if (error) {
		device_printf(dev, "cannot setup features\n");
		goto fail;
	}

	error = vtscmi_alloc_virtqueues(sc);
	if (error) {
		device_printf(dev, "cannot allocate virtqueues\n");
		goto fail;
	}

	error = vtscmi_alloc_queues(sc);
	if (error) {
		device_printf(dev, "cannot allocate queues\n");
		goto fail;
	}

	error = virtio_setup_intr(dev, INTR_TYPE_MISC);
	if (error) {
		device_printf(dev, "cannot setup intr\n");
		vtscmi_free_queues(sc);
		goto fail;
	}

	/* Save unique device */
	vtscmi_dev = sc->vtscmi_dev;

fail:

	return (error);
}

static int
vtscmi_detach(device_t dev)
{
	struct vtscmi_softc *sc;

	sc = device_get_softc(dev);

	/* These also disable related interrupts */
	virtio_scmi_channel_callback_set(dev, VIRTIO_SCMI_CHAN_A2P, NULL, NULL);
	virtio_scmi_channel_callback_set(dev, VIRTIO_SCMI_CHAN_P2A, NULL, NULL);

	virtio_stop(dev);

	vtscmi_free_queues(sc);

	return (0);
}

static int
vtscmi_shutdown(device_t dev)
{

	return (0);
}

static int
vtscmi_negotiate_features(struct vtscmi_softc *sc)
{
	device_t dev;
	uint64_t features;

	dev = sc->vtscmi_dev;
	/* We still don't support shared mem (stats)...so don't advertise it */
	features = VIRTIO_SCMI_F_P2A_CHANNELS;

	sc->vtscmi_features = virtio_negotiate_features(dev, features);
	return (virtio_finalize_features(dev));
}

static int
vtscmi_setup_features(struct vtscmi_softc *sc)
{
	device_t dev;
	int error;

	dev = sc->vtscmi_dev;
	error = vtscmi_negotiate_features(sc);
	if (error)
		return (error);

	if (virtio_with_feature(dev, VIRTIO_SCMI_F_P2A_CHANNELS))
		sc->has_p2a = true;
	if (virtio_with_feature(dev, VIRTIO_SCMI_F_SHARED_MEMORY))
		sc->has_shared = true;

	device_printf(dev, "Platform %s P2A channel.\n",
	    sc->has_p2a ? "supports" : "does NOT support");

	return (0);
}

static int
vtscmi_alloc_queues(struct vtscmi_softc *sc)
{
	int idx;

	for (idx = VIRTIO_SCMI_CHAN_A2P; idx < VIRTIO_SCMI_CHAN_MAX; idx++) {
		int i, vq_sz;
		struct vtscmi_queue *q;
		struct vtscmi_pdu *pdu;

		if (idx == VIRTIO_SCMI_CHAN_P2A && !sc->has_p2a)
			continue;

		q = &sc->vtscmi_queues[idx];
		q->dev = sc->vtscmi_dev;
		q->vq_id = idx;
		vq_sz = virtqueue_size(q->vq);
		q->vq_sz = idx != VIRTIO_SCMI_CHAN_A2P ? vq_sz : vq_sz / 2;

		q->pdus = mallocarray(q->vq_sz, sizeof(*pdu), M_DEVBUF,
		    M_ZERO | M_WAITOK);

		SLIST_INIT(&q->p_head);
		for (i = 0, pdu = q->pdus; i < q->vq_sz; i++, pdu++) {
			pdu->chan = idx;
			//XXX Maybe one seg redndant for P2A
			sglist_init(&pdu->sg,
			    idx == VIRTIO_SCMI_CHAN_A2P ? 2 : 1, pdu->segs);
			SLIST_INSERT_HEAD(&q->p_head, pdu, next);
		}

		mtx_init(&q->p_mtx, "vtscmi_pdus", "VTSCMI", MTX_SPIN);
		mtx_init(&q->vq_mtx, "vtscmi_vq", "VTSCMI", MTX_SPIN);
	}

	return (0);
}

static void
vtscmi_free_queues(struct vtscmi_softc *sc)
{
	int idx;

	for (idx = VIRTIO_SCMI_CHAN_A2P; idx < VIRTIO_SCMI_CHAN_MAX; idx++) {
		struct vtscmi_queue *q;

		if (idx == VIRTIO_SCMI_CHAN_P2A && !sc->has_p2a)
			continue;

		q = &sc->vtscmi_queues[idx];
		if (q->vq_sz == 0)
			continue;

		free(q->pdus, M_DEVBUF);
		mtx_destroy(&q->p_mtx);
		mtx_destroy(&q->vq_mtx);
	}
}

static void
vtscmi_vq_intr(void *arg)
{
	struct vtscmi_queue *q = arg;

	/*
	 * TODO
	 * - consider pressure on RX by msg floods
	 *   + Does it need a taskqueue_ like virtio/net to postpone processing
	 *     under pressure ? (SCMI is low_freq compared to network though)
	 */
	for (;;) {
		struct vtscmi_pdu *pdu;
		uint32_t rx_len;

		mtx_lock_spin(&q->vq_mtx);
		pdu = virtqueue_dequeue(q->vq, &rx_len);
		mtx_unlock_spin(&q->vq_mtx);
		if (!pdu)
			return;

		if (q->rx_callback)
			q->rx_callback(pdu->buf, rx_len, q->priv);

		/* Note that this only frees the PDU, NOT the buffer itself */
		virtio_scmi_pdu_put(q->dev, pdu);
	}
}

static int
vtscmi_alloc_virtqueues(struct vtscmi_softc *sc)
{
	device_t dev;
	struct vq_alloc_info vq_info[VIRTIO_SCMI_CHAN_MAX];

	dev = sc->vtscmi_dev;
	sc->vtscmi_vqs_cnt = sc->has_p2a ? 2 : 1;

	VQ_ALLOC_INFO_INIT(&vq_info[VIRTIO_SCMI_CHAN_A2P], 0,
			   vtscmi_vq_intr,
			   &sc->vtscmi_queues[VIRTIO_SCMI_CHAN_A2P],
			   &sc->vtscmi_queues[VIRTIO_SCMI_CHAN_A2P].vq,
			   "%s cmdq", device_get_nameunit(dev));

	if (sc->has_p2a) {
		VQ_ALLOC_INFO_INIT(&vq_info[VIRTIO_SCMI_CHAN_P2A], 0,
				   vtscmi_vq_intr,
				   &sc->vtscmi_queues[VIRTIO_SCMI_CHAN_P2A],
				   &sc->vtscmi_queues[VIRTIO_SCMI_CHAN_P2A].vq,
				   "%s evtq", device_get_nameunit(dev));
	}

	return (virtio_alloc_virtqueues(dev, sc->vtscmi_vqs_cnt, vq_info));
}

static void *
virtio_scmi_pdu_get(struct vtscmi_queue *q, void *buf, unsigned int tx_len,
    unsigned int rx_len)
{
	struct vtscmi_pdu *pdu = NULL;

	if (rx_len == 0)
		return (NULL);

	mtx_lock_spin(&q->p_mtx);
	if (!SLIST_EMPTY(&q->p_head)) {
		pdu = SLIST_FIRST(&q->p_head);
		SLIST_REMOVE_HEAD(&q->p_head, next);
	}
	mtx_unlock_spin(&q->p_mtx);

	if (pdu == NULL) {
		device_printf(q->dev, "Cannnot allocate PDU.\n");
		return (NULL);
	}

	/*Save msg buffer for easy access */
	pdu->buf = buf;
	if (tx_len != 0)
		sglist_append(&pdu->sg, pdu->buf, tx_len);
	sglist_append(&pdu->sg, pdu->buf, rx_len);

	return (pdu);
}

static void
virtio_scmi_pdu_put(device_t dev, struct vtscmi_pdu *pdu)
{
	struct vtscmi_softc *sc;
	struct vtscmi_queue *q;

	if (pdu == NULL)
		return;

	sc = device_get_softc(dev);
	q = &sc->vtscmi_queues[pdu->chan];

	sglist_reset(&pdu->sg);

	mtx_lock_spin(&q->p_mtx);
	SLIST_INSERT_HEAD(&q->p_head, pdu, next);
	mtx_unlock_spin(&q->p_mtx);
}

device_t
virtio_scmi_transport_get(void)
{
	return (vtscmi_dev);
}

int
virtio_scmi_channel_size_get(device_t dev, enum vtscmi_chan chan)
{
	struct vtscmi_softc *sc;

	sc = device_get_softc(dev);
	if (chan >= sc->vtscmi_vqs_cnt)
		return (0);

	return (sc->vtscmi_queues[chan].vq_sz);
}

int
virtio_scmi_channel_callback_set(device_t dev, enum vtscmi_chan chan,
    virtio_scmi_rx_callback_t *cb, void *priv)
{
	struct vtscmi_softc *sc;

	sc = device_get_softc(dev);
	if (chan >= sc->vtscmi_vqs_cnt)
		return (1);

	if (cb == NULL)
		virtqueue_disable_intr(sc->vtscmi_queues[chan].vq);

	sc->vtscmi_queues[chan].rx_callback = cb;
	sc->vtscmi_queues[chan].priv = priv;

	/* Enable Interrupt on VQ once the callback is set */
	if (cb != NULL)
		/*
		 * TODO
		 * Does this need a taskqueue_ task to process already pending
		 * messages ?
		 */
		virtqueue_enable_intr(sc->vtscmi_queues[chan].vq);

	device_printf(dev, "%sabled interrupts on VQ[%d].\n",
	    cb ? "En" : "Dis", chan);

	return (0);
}

int
virtio_scmi_message_enqueue(device_t dev, enum vtscmi_chan chan,
    void *buf, unsigned int tx_len, unsigned int rx_len)
{
	struct vtscmi_softc *sc;
	struct vtscmi_pdu *pdu;
	struct vtscmi_queue *q;
	int ret;

	sc = device_get_softc(dev);
	if (chan >= sc->vtscmi_vqs_cnt)
		return (1);

	q = &sc->vtscmi_queues[chan];
	pdu = virtio_scmi_pdu_get(q, buf, tx_len, rx_len);
	if (pdu == NULL)
		return (ENXIO);

	mtx_lock_spin(&q->vq_mtx);
	ret = virtqueue_enqueue(q->vq, pdu, &pdu->sg,
	    chan == VIRTIO_SCMI_CHAN_A2P ? 1 : 0, 1);
	if (ret == 0)
		virtqueue_notify(q->vq);
	mtx_unlock_spin(&q->vq_mtx);

	return (ret);
}

void *
virtio_scmi_message_poll(device_t dev, uint32_t *rx_len)
{
	struct vtscmi_softc *sc;
	struct vtscmi_queue *q;
	struct vtscmi_pdu *pdu;
	void *buf = NULL;

	sc = device_get_softc(dev);

	q = &sc->vtscmi_queues[VIRTIO_SCMI_CHAN_A2P];

	mtx_lock_spin(&q->vq_mtx);
	/* Not using virtqueue_poll since has no configurable timeout */
	pdu = virtqueue_dequeue(q->vq, rx_len);
	mtx_unlock_spin(&q->vq_mtx);
	if (pdu != NULL) {
		buf = pdu->buf;
		virtio_scmi_pdu_put(dev, pdu);
	}

	return (buf);
}
