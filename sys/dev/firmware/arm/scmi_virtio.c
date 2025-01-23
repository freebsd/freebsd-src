/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Arm Ltd
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/fdt/simplebus.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/virtio/scmi/virtio_scmi.h>

#include "scmi.h"
#include "scmi_protocols.h"

#define SCMI_VIRTIO_POLLING_INTERVAL_MS	2

struct scmi_virtio_softc {
	struct scmi_softc	base;
	device_t		virtio_dev;
	int			cmdq_sz;
	int			evtq_sz;
	void			*p2a_pool;
};

static void	scmi_virtio_callback(void *, unsigned int, void *);
static void	*scmi_virtio_p2a_pool_init(device_t, unsigned int);
static int	scmi_virtio_transport_init(device_t);
static void	scmi_virtio_transport_cleanup(device_t);
static int	scmi_virtio_xfer_msg(device_t, struct scmi_msg *);
static int	scmi_virtio_poll_msg(device_t, struct scmi_msg *, unsigned int);
static void	scmi_virtio_clear_channel(device_t, void *);
static int	scmi_virtio_probe(device_t);
static int	scmi_virtio_attach(device_t);

static void
scmi_virtio_callback(void *msg, unsigned int len, void *priv)
{
	struct scmi_virtio_softc *sc;
	uint32_t hdr;

	sc = priv;

	if (msg == NULL || len < sizeof(hdr)) {
		device_printf(sc->virtio_dev, "Ignoring malformed message.\n");
		return;
	}

	hdr = le32toh(*((uint32_t *)msg));
	scmi_rx_irq_callback(sc->base.dev, msg, hdr, len);
}

static void *
scmi_virtio_p2a_pool_init(device_t dev, unsigned int max_msg)
{
	struct scmi_virtio_softc *sc;
	unsigned int max_msg_sz;
	void *pool;
	uint8_t *buf;
	int i;

	sc = device_get_softc(dev);
	max_msg_sz = SCMI_MAX_MSG_SIZE(&sc->base);
	pool = mallocarray(max_msg, max_msg_sz, M_DEVBUF, M_ZERO | M_WAITOK);

	for (i = 0, buf = pool; i < max_msg; i++, buf += max_msg_sz) {
		/* Feed platform with pre-allocated P2A buffers */
		virtio_scmi_message_enqueue(sc->virtio_dev,
		    VIRTIO_SCMI_CHAN_P2A, buf, 0, max_msg_sz);
	}

	device_printf(dev,
	    "Fed %d initial P2A buffers to platform.\n", max_msg);

	return (pool);
}

static void
scmi_virtio_clear_channel(device_t dev, void *msg)
{
	struct scmi_virtio_softc *sc;

	sc = device_get_softc(dev);
	virtio_scmi_message_enqueue(sc->virtio_dev, VIRTIO_SCMI_CHAN_P2A,
	    msg, 0, SCMI_MAX_MSG_SIZE(&sc->base));
}

static int
scmi_virtio_transport_init(device_t dev)
{
	struct scmi_virtio_softc *sc;
	int ret;

	sc = device_get_softc(dev);

	sc->cmdq_sz = virtio_scmi_channel_size_get(sc->virtio_dev,
	    VIRTIO_SCMI_CHAN_A2P);
	sc->evtq_sz = virtio_scmi_channel_size_get(sc->virtio_dev,
	    VIRTIO_SCMI_CHAN_P2A);

	if (!sc->cmdq_sz) {
		device_printf(dev,
		    "VirtIO cmdq virtqueue not found. Aborting.\n");
		return (ENXIO);
	}

	/*
	 * P2A buffers are owned by the platform initially; allocate a feed an
	 * appropriate number of buffers.
	 */
	if (sc->evtq_sz != 0) {
		sc->p2a_pool = scmi_virtio_p2a_pool_init(dev, sc->evtq_sz);
		if (sc->p2a_pool == NULL)
			return (ENOMEM);
	}

	/* Note that setting a callback also enables that VQ interrupts */
	ret = virtio_scmi_channel_callback_set(sc->virtio_dev,
	    VIRTIO_SCMI_CHAN_A2P, scmi_virtio_callback, sc);
	if (ret) {
		device_printf(dev, "Failed to set VirtIO cmdq callback.\n");
		return (ENXIO);
	}

	device_printf(dev,
	    "VirtIO cmdq virtqueue configured - cmdq_sz:%d\n", sc->cmdq_sz);

	/* P2A channel is optional */
	if (sc->evtq_sz) {
		ret = virtio_scmi_channel_callback_set(sc->virtio_dev,
		    VIRTIO_SCMI_CHAN_P2A, scmi_virtio_callback, sc);
		if (ret == 0) {
			device_printf(dev,
			    "VirtIO evtq virtqueue configured - evtq_sz:%d\n",
			    sc->evtq_sz);
		} else {
			device_printf(dev,
			    "Failed to set VirtIO evtq callback.Skip.\n");
			sc->evtq_sz = 0;
		}
	}

	sc->base.trs_desc.reply_timo_ms = 100;

	return (0);
}

static void
scmi_virtio_transport_cleanup(device_t dev)
{
	struct scmi_virtio_softc *sc;

	sc = device_get_softc(dev);

	if (sc->evtq_sz != 0) {
		virtio_scmi_channel_callback_set(sc->virtio_dev,
		    VIRTIO_SCMI_CHAN_P2A, NULL, NULL);
		free(sc->p2a_pool, M_DEVBUF);
	}

	virtio_scmi_channel_callback_set(sc->virtio_dev,
	    VIRTIO_SCMI_CHAN_A2P, NULL, NULL);
}

static int
scmi_virtio_xfer_msg(device_t dev, struct scmi_msg *msg)
{
	struct scmi_virtio_softc *sc;

	sc = device_get_softc(dev);

	return (virtio_scmi_message_enqueue(sc->virtio_dev,
	    VIRTIO_SCMI_CHAN_A2P, &msg->hdr, msg->tx_len, msg->rx_len));
}

static int
scmi_virtio_poll_msg(device_t dev, struct scmi_msg *msg, unsigned int tmo_ms)
{
	struct scmi_virtio_softc *sc;
	device_t vdev;
	int tmo_loops;

	sc = device_get_softc(dev);
	vdev = sc->virtio_dev;

	tmo_loops = tmo_ms / SCMI_VIRTIO_POLLING_INTERVAL_MS;
	while (tmo_loops-- && atomic_load_acq_int(&msg->poll_done) == 0) {
		struct scmi_msg *rx_msg;
		void *rx_buf;
		uint32_t rx_len;

		rx_buf = virtio_scmi_message_poll(vdev, &rx_len);
		if (rx_buf == NULL) {
			DELAY(SCMI_VIRTIO_POLLING_INTERVAL_MS * 1000);
			continue;
		}

		rx_msg = hdr_to_msg(rx_buf);
		/* Complete the polling on any poll path */
		if (rx_msg->polling)
			atomic_store_rel_int(&rx_msg->poll_done, 1);

		if (__predict_true(rx_msg == msg))
			break;

		/*
		 * Polling returned an unexpected message: either a message
		 * polled by some other thread of execution or a message not
		 * supposed to be polled.
		 */
		device_printf(dev, "POLLED OoO HDR:|%08X| - polling:%d\n",
		    rx_msg->hdr, rx_msg->polling);

		if (!rx_msg->polling)
			scmi_rx_irq_callback(sc->base.dev, rx_msg, rx_msg->hdr, rx_len);
	}

	return (tmo_loops > 0 ? 0 : ETIMEDOUT);
}

static int
scmi_virtio_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "arm,scmi-virtio"))
		return (ENXIO);

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	device_set_desc(dev, "ARM SCMI VirtIO Transport driver");

	return (BUS_PROBE_DEFAULT);
}

static int
scmi_virtio_attach(device_t dev)
{
	struct scmi_virtio_softc *sc;

	sc = device_get_softc(dev);
	sc->virtio_dev = virtio_scmi_transport_get();
	if (sc->virtio_dev == NULL)
		return (1);

	/* When attach fails there is nothing to cleanup*/
	return (scmi_attach(dev));
}

static device_method_t scmi_virtio_methods[] = {
	DEVMETHOD(device_probe,		scmi_virtio_probe),
	DEVMETHOD(device_attach,	scmi_virtio_attach),

	/* SCMI interface */
	DEVMETHOD(scmi_transport_init,		scmi_virtio_transport_init),
	DEVMETHOD(scmi_transport_cleanup,	scmi_virtio_transport_cleanup),
	DEVMETHOD(scmi_xfer_msg,		scmi_virtio_xfer_msg),
	DEVMETHOD(scmi_poll_msg,		scmi_virtio_poll_msg),
	DEVMETHOD(scmi_clear_channel,		scmi_virtio_clear_channel),

	DEVMETHOD_END
};

DEFINE_CLASS_1(scmi_virtio, scmi_virtio_driver, scmi_virtio_methods,
    sizeof(struct scmi_virtio_softc), scmi_driver);

/* Needs to be after the mmio_sram driver */
DRIVER_MODULE(scmi_virtio, simplebus, scmi_virtio_driver, 0, 0);
MODULE_VERSION(scmi_virtio, 1);
