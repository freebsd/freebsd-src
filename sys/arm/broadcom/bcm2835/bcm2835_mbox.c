/*-
 * Copyright (c) 2012 Oleksandr Tymoshenko <gonzo@freebsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/sema.h>
#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/broadcom/bcm2835/bcm2835_mbox.h>
#include <arm/broadcom/bcm2835/bcm2835_mbox_prop.h>
#include <arm/broadcom/bcm2835/bcm2835_vcbus.h>

#include "mbox_if.h"

#define	REG_READ	0x00
#define	REG_POL		0x10
#define	REG_SENDER	0x14
#define	REG_STATUS	0x18
#define		STATUS_FULL	0x80000000
#define		STATUS_EMPTY	0x40000000
#define	REG_CONFIG	0x1C
#define		CONFIG_DATA_IRQ	0x00000001
#define	REG_WRITE	0x20 /* This is Mailbox 1 address */

#define	MBOX_MSG(chan, data)	(((data) & ~0xf) | ((chan) & 0xf))
#define	MBOX_CHAN(msg)		((msg) & 0xf)
#define	MBOX_DATA(msg)		((msg) & ~0xf)

#define	MBOX_LOCK(sc)	do {	\
	mtx_lock(&(sc)->lock);	\
} while(0)

#define	MBOX_UNLOCK(sc)	do {		\
	mtx_unlock(&(sc)->lock);	\
} while(0)

#ifdef  DEBUG
#define dprintf(fmt, args...) printf(fmt, ##args)
#else
#define dprintf(fmt, args...)
#endif

struct bcm_mbox_softc {
	struct mtx		lock;
	struct resource *	mem_res;
	struct resource *	irq_res;
	void*			intr_hl;
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	int			msg[BCM2835_MBOX_CHANS];
	struct sema		sema[BCM2835_MBOX_CHANS];
};

#define	mbox_read_4(sc, reg)		\
    bus_space_read_4((sc)->bst, (sc)->bsh, reg)
#define	mbox_write_4(sc, reg, val)		\
    bus_space_write_4((sc)->bst, (sc)->bsh, reg, val)

static int
bcm_mbox_read_msg(struct bcm_mbox_softc *sc, int *ochan)
{
	uint32_t data;
	uint32_t msg;
	int chan;

	msg = mbox_read_4(sc, REG_READ);
	dprintf("bcm_mbox_intr: raw data %08x\n", msg);
	chan = MBOX_CHAN(msg);
	data = MBOX_DATA(msg);
	if (sc->msg[chan]) {
		printf("bcm_mbox_intr: channel %d oveflow\n", chan);
		return (1);
	}
	dprintf("bcm_mbox_intr: chan %d, data %08x\n", chan, data);
	sc->msg[chan] = msg;

	if (ochan != NULL)
		*ochan = chan;

	return (0);
}

static void
bcm_mbox_intr(void *arg)
{
	struct bcm_mbox_softc *sc = arg;
	int chan;

	while (!(mbox_read_4(sc, REG_STATUS) & STATUS_EMPTY))
		if (bcm_mbox_read_msg(sc, &chan) == 0)
			sema_post(&sc->sema[chan]);
}

static int
bcm_mbox_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "broadcom,bcm2835-mbox")) {
		device_set_desc(dev, "BCM2835 VideoCore Mailbox");
		return(BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
bcm_mbox_attach(device_t dev)
{
	struct bcm_mbox_softc *sc = device_get_softc(dev);
	int i;
	int rid = 0;

	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "could not allocate memory resource\n");
		return (ENXIO);
	}

	sc->bst = rman_get_bustag(sc->mem_res);
	sc->bsh = rman_get_bushandle(sc->mem_res);

	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "could not allocate interrupt resource\n");
		return (ENXIO);
	}

	/* Setup and enable the timer */
	if (bus_setup_intr(dev, sc->irq_res, INTR_MPSAFE | INTR_TYPE_MISC, 
	    NULL, bcm_mbox_intr, sc, &sc->intr_hl) != 0) {
		bus_release_resource(dev, SYS_RES_IRQ, rid, sc->irq_res);
		device_printf(dev, "Unable to setup the clock irq handler.\n");
		return (ENXIO);
	}

	mtx_init(&sc->lock, "vcio mbox", NULL, MTX_DEF);
	for (i = 0; i < BCM2835_MBOX_CHANS; i++) {
		sc->msg[i] = 0;
		sema_init(&sc->sema[i], 0, "mbox");
	}

	/* Read all pending messages */
	while ((mbox_read_4(sc, REG_STATUS) & STATUS_EMPTY) == 0)
		(void)mbox_read_4(sc, REG_READ);

	mbox_write_4(sc, REG_CONFIG, CONFIG_DATA_IRQ);

	return (0);
}

/* 
 * Mailbox API
 */
static int
bcm_mbox_write(device_t dev, int chan, uint32_t data)
{
	int limit = 1000;
	struct bcm_mbox_softc *sc = device_get_softc(dev);

	dprintf("bcm_mbox_write: chan %d, data %08x\n", chan, data);
	MBOX_LOCK(sc);
	while ((mbox_read_4(sc, REG_STATUS) & STATUS_FULL) && --limit)
		DELAY(5);
	if (limit == 0) {
		printf("bcm_mbox_write: STATUS_FULL stuck");
		MBOX_UNLOCK(sc);
		return (EAGAIN);
	}
	mbox_write_4(sc, REG_WRITE, MBOX_MSG(chan, data));
	MBOX_UNLOCK(sc);

	return (0);
}

static int
bcm_mbox_read(device_t dev, int chan, uint32_t *data)
{
	struct bcm_mbox_softc *sc = device_get_softc(dev);
	int err, read_chan;

	dprintf("bcm_mbox_read: chan %d\n", chan);

	err = 0;
	MBOX_LOCK(sc);
	if (!cold) {
		while (sema_trywait(&sc->sema[chan]) == 0) {
			/* do not unlock sc while waiting for the mbox */
			if (sema_timedwait(&sc->sema[chan], 10*hz) == 0)
				break;
			printf("timeout sema for chan %d\n", chan);
		}
	} else {
		do {
			/* Wait for a message */
			while ((mbox_read_4(sc, REG_STATUS) & STATUS_EMPTY))
				;
			/* Read the message */
			if (bcm_mbox_read_msg(sc, &read_chan) != 0) {
				err = EINVAL;
				goto out;
			}
		} while (read_chan != chan);
	}
	/*
	 *  get data from intr handler, the same channel is never coming
	 *  because of holding sc lock.
	 */
	*data = MBOX_DATA(sc->msg[chan]);
	sc->msg[chan] = 0;
out:
	MBOX_UNLOCK(sc);
	dprintf("bcm_mbox_read: chan %d, data %08x\n", chan, *data);

	return (err);
}

static device_method_t bcm_mbox_methods[] = {
	DEVMETHOD(device_probe,		bcm_mbox_probe),
	DEVMETHOD(device_attach,	bcm_mbox_attach),

	DEVMETHOD(mbox_read,		bcm_mbox_read),
	DEVMETHOD(mbox_write,		bcm_mbox_write),

	DEVMETHOD_END
};

static driver_t bcm_mbox_driver = {
	"mbox",
	bcm_mbox_methods,
	sizeof(struct bcm_mbox_softc),
};

static devclass_t bcm_mbox_devclass;

DRIVER_MODULE(mbox, simplebus, bcm_mbox_driver, bcm_mbox_devclass, 0, 0);

static void
bcm2835_mbox_dma_cb(void *arg, bus_dma_segment_t *segs, int nseg, int err)
{
	bus_addr_t *addr;

	if (err)
		return;
	addr = (bus_addr_t *)arg;
	*addr = PHYS_TO_VCBUS(segs[0].ds_addr);
}

static void *
bcm2835_mbox_init_dma(device_t dev, size_t len, bus_dma_tag_t *tag,
    bus_dmamap_t *map, bus_addr_t *phys)
{
	void *buf;
	int err;

	err = bus_dma_tag_create(bus_get_dma_tag(dev), 16, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    len, 1, len, 0, NULL, NULL, tag);
	if (err != 0) {
		device_printf(dev, "can't create DMA tag\n");
		return (NULL);
	}

	err = bus_dmamem_alloc(*tag, &buf, 0, map);
	if (err != 0) {
		bus_dma_tag_destroy(*tag);
		device_printf(dev, "can't allocate dmamem\n");
		return (NULL);
	}

	err = bus_dmamap_load(*tag, *map, buf,
	    sizeof(struct msg_set_power_state), bcm2835_mbox_dma_cb,
	    phys, 0);
	if (err != 0) {
		bus_dmamem_free(*tag, buf, *map);
		bus_dma_tag_destroy(*tag);
		device_printf(dev, "can't load DMA map\n");
		return (NULL);
	}

	return (buf);
}

int
bcm2835_mbox_set_power_state(device_t dev, uint32_t device_id, boolean_t on)
{
	struct msg_set_power_state *msg;
	bus_dma_tag_t msg_tag;
	bus_dmamap_t msg_map;
	bus_addr_t msg_phys;
	uint32_t reg;
	device_t mbox;

	/* get mbox device */
	mbox = devclass_get_device(devclass_find("mbox"), 0);
	if (mbox == NULL) {
		device_printf(dev, "can't find mbox\n");
		return (ENXIO);
	}

	/* Allocate memory for the message */
	msg = bcm2835_mbox_init_dma(dev, sizeof(*msg), &msg_tag, &msg_map,
	    &msg_phys);
	if (msg == NULL)
		return (ENOMEM);

	memset(msg, 0, sizeof(*msg));
	msg->hdr.buf_size = sizeof(*msg);
	msg->hdr.code = BCM2835_MBOX_CODE_REQ;
	msg->tag_hdr.tag = BCM2835_MBOX_TAG_SET_POWER_STATE;
	msg->tag_hdr.val_buf_size = sizeof(msg->body);
	msg->tag_hdr.val_len = sizeof(msg->body.req);
	msg->body.req.device_id = device_id;
	msg->body.req.state = (on ? BCM2835_MBOX_POWER_ON : 0) |
	    BCM2835_MBOX_POWER_WAIT;
	msg->end_tag = 0;

	bus_dmamap_sync(msg_tag, msg_map,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	MBOX_WRITE(mbox, BCM2835_MBOX_CHAN_PROP, (uint32_t)msg_phys);
	MBOX_READ(mbox, BCM2835_MBOX_CHAN_PROP, &reg);

	bus_dmamap_unload(msg_tag, msg_map);
	bus_dmamem_free(msg_tag, msg, msg_map);
	bus_dma_tag_destroy(msg_tag);

	return (0);
}

int
bcm2835_mbox_get_clock_rate(device_t dev, uint32_t clock_id, uint32_t *hz)
{
	struct msg_get_clock_rate *msg;
	bus_dma_tag_t msg_tag;
	bus_dmamap_t msg_map;
	bus_addr_t msg_phys;
	uint32_t reg;
	device_t mbox;

	/* get mbox device */
	mbox = devclass_get_device(devclass_find("mbox"), 0);
	if (mbox == NULL) {
		device_printf(dev, "can't find mbox\n");
		return (ENXIO);
	}

	/* Allocate memory for the message */
	msg = bcm2835_mbox_init_dma(dev, sizeof(*msg), &msg_tag, &msg_map,
	    &msg_phys);
	if (msg == NULL)
		return (ENOMEM);

	memset(msg, 0, sizeof(*msg));
	msg->hdr.buf_size = sizeof(*msg);
	msg->hdr.code = BCM2835_MBOX_CODE_REQ;
	msg->tag_hdr.tag = BCM2835_MBOX_TAG_GET_CLOCK_RATE;
	msg->tag_hdr.val_buf_size = sizeof(msg->body);
	msg->tag_hdr.val_len = sizeof(msg->body.req);
	msg->body.req.clock_id = clock_id;
	msg->end_tag = 0;

	bus_dmamap_sync(msg_tag, msg_map, BUS_DMASYNC_PREWRITE);
	MBOX_WRITE(mbox, BCM2835_MBOX_CHAN_PROP, (uint32_t)msg_phys);
	bus_dmamap_sync(msg_tag, msg_map, BUS_DMASYNC_POSTWRITE);

	bus_dmamap_sync(msg_tag, msg_map, BUS_DMASYNC_PREREAD);
	MBOX_READ(mbox, BCM2835_MBOX_CHAN_PROP, &reg);
	bus_dmamap_sync(msg_tag, msg_map, BUS_DMASYNC_POSTREAD);

	*hz = msg->body.resp.rate_hz;

	bus_dmamap_unload(msg_tag, msg_map);
	bus_dmamem_free(msg_tag, msg, msg_map);
	bus_dma_tag_destroy(msg_tag);

	return (0);
}
