/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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

/* Xilinx AXI DMA controller driver. */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_platform.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_page.h>

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#include <dev/xdma/xdma.h>
#include <dev/xilinx/axidma.h>

#include "xdma_if.h"

#define AXIDMA_DEBUG
#undef AXIDMA_DEBUG

#ifdef AXIDMA_DEBUG
#define dprintf(fmt, ...)  printf(fmt, ##__VA_ARGS__)
#else
#define dprintf(fmt, ...)
#endif

#define	AXIDMA_NCHANNELS	2
#define	AXIDMA_DESCS_NUM	512
#define	AXIDMA_TX_CHAN		0
#define	AXIDMA_RX_CHAN		1

extern struct bus_space memmap_bus;

struct axidma_fdt_data {
	int id;
};

struct axidma_channel {
	struct axidma_softc	*sc;
	xdma_channel_t		*xchan;
	bool			used;
	int			idx_head;
	int			idx_tail;

	struct axidma_desc	**descs;
	vm_paddr_t		*descs_phys;
	uint32_t		descs_num;

	vm_size_t		mem_size;
	vm_offset_t		mem_paddr;
	vm_offset_t		mem_vaddr;

	uint32_t		descs_used_count;
};

struct axidma_softc {
	device_t		dev;
	struct resource		*res[3];
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	void			*ih[2];
	struct axidma_desc	desc;
	struct axidma_channel	channels[AXIDMA_NCHANNELS];
};

static struct resource_spec axidma_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		1,	RF_ACTIVE },
	{ -1, 0 }
};

#define	HWTYPE_NONE	0
#define	HWTYPE_STD	1

static struct ofw_compat_data compat_data[] = {
	{ "xlnx,eth-dma",	HWTYPE_STD },
	{ NULL,			HWTYPE_NONE },
};

static int axidma_probe(device_t dev);
static int axidma_attach(device_t dev);
static int axidma_detach(device_t dev);

static inline uint32_t
axidma_next_desc(struct axidma_channel *chan, uint32_t curidx)
{

	return ((curidx + 1) % chan->descs_num);
}

static void
axidma_intr(struct axidma_softc *sc,
    struct axidma_channel *chan)
{
	xdma_transfer_status_t status;
	xdma_transfer_status_t st;
	struct axidma_fdt_data *data;
	xdma_controller_t *xdma;
	struct axidma_desc *desc;
	struct xdma_channel *xchan;
	uint32_t tot_copied;
	int pending;
	int errors;

	xchan = chan->xchan;
	xdma = xchan->xdma;
	data = xdma->data;

	pending = READ4(sc, AXI_DMASR(data->id));
	WRITE4(sc, AXI_DMASR(data->id), pending);

	errors = (pending & (DMASR_DMAINTERR | DMASR_DMASLVERR
			| DMASR_DMADECOREERR | DMASR_SGINTERR
			| DMASR_SGSLVERR | DMASR_SGDECERR));

	dprintf("%s: AXI_DMASR %x\n", __func__,
	    READ4(sc, AXI_DMASR(data->id)));
	dprintf("%s: AXI_CURDESC %x\n", __func__,
	    READ4(sc, AXI_CURDESC(data->id)));
	dprintf("%s: AXI_TAILDESC %x\n", __func__,
	    READ4(sc, AXI_TAILDESC(data->id)));

	tot_copied = 0;

	while (chan->idx_tail != chan->idx_head) {
		desc = chan->descs[chan->idx_tail];
		if ((desc->status & BD_STATUS_CMPLT) == 0)
			break;

		st.error = errors;
		st.transferred = desc->status & BD_CONTROL_LEN_M;
		tot_copied += st.transferred;
		xchan_seg_done(xchan, &st);

		chan->idx_tail = axidma_next_desc(chan, chan->idx_tail);
		atomic_subtract_int(&chan->descs_used_count, 1);
	}

	/* Finish operation */
	status.error = errors;
	status.transferred = tot_copied;
	xdma_callback(chan->xchan, &status);
}

static void
axidma_intr_rx(void *arg)
{
	struct axidma_softc *sc;
	struct axidma_channel *chan;

	dprintf("%s\n", __func__);

	sc = arg;
	chan = &sc->channels[AXIDMA_RX_CHAN];

	axidma_intr(sc, chan);
}

static void
axidma_intr_tx(void *arg)
{
	struct axidma_softc *sc;
	struct axidma_channel *chan;

	dprintf("%s\n", __func__);

	sc = arg;
	chan = &sc->channels[AXIDMA_TX_CHAN];

	axidma_intr(sc, chan);
}

static int
axidma_reset(struct axidma_softc *sc, int chan_id)
{
	int timeout;

	WRITE4(sc, AXI_DMACR(chan_id), DMACR_RESET);

	timeout = 100;
	do {
		if ((READ4(sc, AXI_DMACR(chan_id)) & DMACR_RESET) == 0)
			break;
	} while (timeout--);

	dprintf("timeout %d\n", timeout);

	if (timeout == 0)
		return (-1);

	dprintf("%s: read control after reset: %x\n",
	    __func__, READ4(sc, AXI_DMACR(chan_id)));

	return (0);
}

static int
axidma_probe(device_t dev)
{
	int hwtype;

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	hwtype = ofw_bus_search_compatible(dev, compat_data)->ocd_data;
	if (hwtype == HWTYPE_NONE)
		return (ENXIO);

	device_set_desc(dev, "Xilinx AXI DMA");

	return (BUS_PROBE_DEFAULT);
}

static int
axidma_attach(device_t dev)
{
	struct axidma_softc *sc;
	phandle_t xref, node;
	int err;

	sc = device_get_softc(dev);
	sc->dev = dev;

	if (bus_alloc_resources(dev, axidma_spec, sc->res)) {
		device_printf(dev, "could not allocate resources.\n");
		return (ENXIO);
	}

	/* CSR memory interface */
	sc->bst = rman_get_bustag(sc->res[0]);
	sc->bsh = rman_get_bushandle(sc->res[0]);

	/* Setup interrupt handler */
	err = bus_setup_intr(dev, sc->res[1], INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, axidma_intr_tx, sc, &sc->ih[0]);
	if (err) {
		device_printf(dev, "Unable to alloc interrupt resource.\n");
		return (ENXIO);
	}

	/* Setup interrupt handler */
	err = bus_setup_intr(dev, sc->res[2], INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, axidma_intr_rx, sc, &sc->ih[1]);
	if (err) {
		device_printf(dev, "Unable to alloc interrupt resource.\n");
		return (ENXIO);
	}

	node = ofw_bus_get_node(dev);
	xref = OF_xref_from_node(node);
	OF_device_register_xref(xref, dev);

	return (0);
}

static int
axidma_detach(device_t dev)
{
	struct axidma_softc *sc;

	sc = device_get_softc(dev);

	bus_teardown_intr(dev, sc->res[1], sc->ih[0]);
	bus_teardown_intr(dev, sc->res[2], sc->ih[1]);
	bus_release_resources(dev, axidma_spec, sc->res);

	return (0);
}

static int
axidma_desc_free(struct axidma_softc *sc, struct axidma_channel *chan)
{
	struct xdma_channel *xchan;
	int nsegments;

	nsegments = chan->descs_num;
	xchan = chan->xchan;

	free(chan->descs, M_DEVBUF);
	free(chan->descs_phys, M_DEVBUF);

	pmap_kremove_device(chan->mem_vaddr, chan->mem_size);
	kva_free(chan->mem_vaddr, chan->mem_size);
	vmem_free(xchan->vmem, chan->mem_paddr, chan->mem_size);

	return (0);
}

static int
axidma_desc_alloc(struct axidma_softc *sc, struct xdma_channel *xchan,
    uint32_t desc_size)
{
	struct axidma_channel *chan;
	int nsegments;
	int i;

	chan = (struct axidma_channel *)xchan->chan;
	nsegments = chan->descs_num;

	chan->descs = malloc(nsegments * sizeof(struct axidma_desc *),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (chan->descs == NULL) {
		device_printf(sc->dev,
		    "%s: Can't allocate memory.\n", __func__);
		return (-1);
	}

	chan->descs_phys = malloc(nsegments * sizeof(bus_dma_segment_t),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	chan->mem_size = desc_size * nsegments;
	if (vmem_alloc(xchan->vmem, chan->mem_size, M_FIRSTFIT | M_NOWAIT,
	    &chan->mem_paddr)) {
		device_printf(sc->dev, "Failed to allocate memory.\n");
		return (-1);
	}
	chan->mem_vaddr = kva_alloc(chan->mem_size);
	pmap_kenter_device(chan->mem_vaddr, chan->mem_size, chan->mem_paddr);

	device_printf(sc->dev, "Allocated chunk %lx %d\n",
	    chan->mem_paddr, chan->mem_size);

	for (i = 0; i < nsegments; i++) {
		chan->descs[i] = (struct axidma_desc *)
		    ((uint64_t)chan->mem_vaddr + desc_size * i);
		chan->descs_phys[i] = chan->mem_paddr + desc_size * i;
	}

	return (0);
}

static int
axidma_channel_alloc(device_t dev, struct xdma_channel *xchan)
{
	xdma_controller_t *xdma;
	struct axidma_fdt_data *data;
	struct axidma_channel *chan;
	struct axidma_softc *sc;

	sc = device_get_softc(dev);

	if (xchan->caps & XCHAN_CAP_BUSDMA) {
		device_printf(sc->dev,
		    "Error: busdma operation is not implemented.");
		return (-1);
	}

	xdma = xchan->xdma;
	data = xdma->data;

	chan = &sc->channels[data->id];
	if (chan->used == false) {
		if (axidma_reset(sc, data->id) != 0)
			return (-1);
		chan->xchan = xchan;
		xchan->chan = (void *)chan;
		chan->sc = sc;
		chan->used = true;
		chan->idx_head = 0;
		chan->idx_tail = 0;
		chan->descs_used_count = 0;
		chan->descs_num = AXIDMA_DESCS_NUM;

		return (0);
	}

	return (-1);
}

static int
axidma_channel_free(device_t dev, struct xdma_channel *xchan)
{
	struct axidma_channel *chan;
	struct axidma_softc *sc;

	sc = device_get_softc(dev);

	chan = (struct axidma_channel *)xchan->chan;

	axidma_desc_free(sc, chan);

	chan->used = false;

	return (0);
}

static int
axidma_channel_capacity(device_t dev, xdma_channel_t *xchan,
    uint32_t *capacity)
{
	struct axidma_channel *chan;
	uint32_t c;

	chan = (struct axidma_channel *)xchan->chan;

	/* At least one descriptor must be left empty. */
	c = (chan->descs_num - chan->descs_used_count - 1);

	*capacity = c;

	return (0);
}

static int
axidma_channel_submit_sg(device_t dev, struct xdma_channel *xchan,
    struct xdma_sglist *sg, uint32_t sg_n)
{
	xdma_controller_t *xdma;
	struct axidma_fdt_data *data;
	struct axidma_channel *chan;
	struct axidma_desc *desc;
	struct axidma_softc *sc;
	uint32_t src_addr;
	uint32_t dst_addr;
	uint32_t addr;
	uint32_t len;
	uint32_t tmp;
	int i;
	int tail;

	dprintf("%s: sg_n %d\n", __func__, sg_n);

	sc = device_get_softc(dev);

	chan = (struct axidma_channel *)xchan->chan;
	xdma = xchan->xdma;
	data = xdma->data;

	if (sg_n == 0)
		return (0);

	tail = chan->idx_head;

	tmp = 0;

	for (i = 0; i < sg_n; i++) {
		src_addr = (uint32_t)sg[i].src_addr;
		dst_addr = (uint32_t)sg[i].dst_addr;
		len = (uint32_t)sg[i].len;

		dprintf("%s(%d): src %x dst %x len %d\n", __func__,
		    data->id, src_addr, dst_addr, len);

		desc = chan->descs[chan->idx_head];
		if (sg[i].direction == XDMA_MEM_TO_DEV)
			desc->phys = src_addr;
		else
			desc->phys = dst_addr;
		desc->status = 0;
		desc->control = len;
		if (sg[i].first == 1)
			desc->control |= BD_CONTROL_TXSOF;
		if (sg[i].last == 1)
			desc->control |= BD_CONTROL_TXEOF;

		tmp = chan->idx_head;

		atomic_add_int(&chan->descs_used_count, 1);
		chan->idx_head = axidma_next_desc(chan, chan->idx_head);
	}

	dprintf("%s(%d): _curdesc %x\n", __func__, data->id,
	    READ8(sc, AXI_CURDESC(data->id)));
	dprintf("%s(%d): _curdesc %x\n", __func__, data->id,
	    READ8(sc, AXI_CURDESC(data->id)));
	dprintf("%s(%d): status %x\n", __func__, data->id,
	    READ4(sc, AXI_DMASR(data->id)));

	addr = chan->descs_phys[tmp];
	WRITE8(sc, AXI_TAILDESC(data->id), addr);

	return (0);
}

static int
axidma_channel_prep_sg(device_t dev, struct xdma_channel *xchan)
{
	xdma_controller_t *xdma;
	struct axidma_fdt_data *data;
	struct axidma_channel *chan;
	struct axidma_desc *desc;
	struct axidma_softc *sc;
	uint32_t addr;
	uint32_t reg;
	int ret;
	int i;

	sc = device_get_softc(dev);

	chan = (struct axidma_channel *)xchan->chan;
	xdma = xchan->xdma;
	data = xdma->data;

	dprintf("%s(%d)\n", __func__, data->id);

	ret = axidma_desc_alloc(sc, xchan, sizeof(struct axidma_desc));
	if (ret != 0) {
		device_printf(sc->dev,
		    "%s: Can't allocate descriptors.\n", __func__);
		return (-1);
	}

	for (i = 0; i < chan->descs_num; i++) {
		desc = chan->descs[i];
		bzero(desc, sizeof(struct axidma_desc));

		if (i == (chan->descs_num - 1))
			desc->next = chan->descs_phys[0];
		else
			desc->next = chan->descs_phys[i + 1];
		desc->status = 0;
		desc->control = 0;

		dprintf("%s(%d): desc %d vaddr %lx next paddr %x\n", __func__,
		    data->id, i, (uint64_t)desc, le32toh(desc->next));
	}

	addr = chan->descs_phys[0];
	WRITE8(sc, AXI_CURDESC(data->id), addr);

	reg = READ4(sc, AXI_DMACR(data->id));
	reg |= DMACR_IOC_IRQEN | DMACR_DLY_IRQEN | DMACR_ERR_IRQEN;
	WRITE4(sc, AXI_DMACR(data->id), reg);
	reg |= DMACR_RS;
	WRITE4(sc, AXI_DMACR(data->id), reg);

	return (0);
}

static int
axidma_channel_control(device_t dev, xdma_channel_t *xchan, int cmd)
{
	struct axidma_channel *chan;
	struct axidma_softc *sc;

	sc = device_get_softc(dev);

	chan = (struct axidma_channel *)xchan->chan;

	switch (cmd) {
	case XDMA_CMD_BEGIN:
	case XDMA_CMD_TERMINATE:
	case XDMA_CMD_PAUSE:
		/* TODO: implement me */
		return (-1);
	}

	return (0);
}

#ifdef FDT
static int
axidma_ofw_md_data(device_t dev, pcell_t *cells, int ncells, void **ptr)
{
	struct axidma_fdt_data *data;

	if (ncells != 1)
		return (-1);

	data = malloc(sizeof(struct axidma_fdt_data),
	    M_DEVBUF, (M_WAITOK | M_ZERO));
	data->id = cells[0];

	*ptr = data;

	return (0);
}
#endif

static device_method_t axidma_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			axidma_probe),
	DEVMETHOD(device_attach,		axidma_attach),
	DEVMETHOD(device_detach,		axidma_detach),

	/* xDMA Interface */
	DEVMETHOD(xdma_channel_alloc,		axidma_channel_alloc),
	DEVMETHOD(xdma_channel_free,		axidma_channel_free),
	DEVMETHOD(xdma_channel_control,		axidma_channel_control),

	/* xDMA SG Interface */
	DEVMETHOD(xdma_channel_capacity,	axidma_channel_capacity),
	DEVMETHOD(xdma_channel_prep_sg,		axidma_channel_prep_sg),
	DEVMETHOD(xdma_channel_submit_sg,	axidma_channel_submit_sg),

#ifdef FDT
	DEVMETHOD(xdma_ofw_md_data,		axidma_ofw_md_data),
#endif

	DEVMETHOD_END
};

static driver_t axidma_driver = {
	"axidma",
	axidma_methods,
	sizeof(struct axidma_softc),
};

static devclass_t axidma_devclass;

EARLY_DRIVER_MODULE(axidma, simplebus, axidma_driver, axidma_devclass, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);
