/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016-2019 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include "opt_platform.h"
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/kobj.h>
#include <sys/malloc.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/bus.h>

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#include <dev/xdma/xdma.h>

#include <xdma_if.h>

/*
 * Multiple xDMA controllers may work with single DMA device,
 * so we have global lock for physical channel management.
 */
static struct mtx xdma_mtx;

#define	XDMA_LOCK()			mtx_lock(&xdma_mtx)
#define	XDMA_UNLOCK()			mtx_unlock(&xdma_mtx)
#define	XDMA_ASSERT_LOCKED()		mtx_assert(&xdma_mtx, MA_OWNED)

#define	FDT_REG_CELLS	4

/*
 * Allocate virtual xDMA channel.
 */
xdma_channel_t *
xdma_channel_alloc(xdma_controller_t *xdma, uint32_t caps)
{
	xdma_channel_t *xchan;
	int ret;

	xchan = malloc(sizeof(xdma_channel_t), M_XDMA, M_WAITOK | M_ZERO);
	xchan->xdma = xdma;
	xchan->caps = caps;

	XDMA_LOCK();

	/* Request a real channel from hardware driver. */
	ret = XDMA_CHANNEL_ALLOC(xdma->dma_dev, xchan);
	if (ret != 0) {
		device_printf(xdma->dev,
		    "%s: Can't request hardware channel.\n", __func__);
		XDMA_UNLOCK();
		free(xchan, M_XDMA);

		return (NULL);
	}

	TAILQ_INIT(&xchan->ie_handlers);

	mtx_init(&xchan->mtx_lock, "xDMA chan", NULL, MTX_DEF);
	mtx_init(&xchan->mtx_qin_lock, "xDMA qin", NULL, MTX_DEF);
	mtx_init(&xchan->mtx_qout_lock, "xDMA qout", NULL, MTX_DEF);
	mtx_init(&xchan->mtx_bank_lock, "xDMA bank", NULL, MTX_DEF);
	mtx_init(&xchan->mtx_proc_lock, "xDMA proc", NULL, MTX_DEF);

	TAILQ_INIT(&xchan->bank);
	TAILQ_INIT(&xchan->queue_in);
	TAILQ_INIT(&xchan->queue_out);
	TAILQ_INIT(&xchan->processing);

	TAILQ_INSERT_TAIL(&xdma->channels, xchan, xchan_next);

	XDMA_UNLOCK();

	return (xchan);
}

int
xdma_channel_free(xdma_channel_t *xchan)
{
	xdma_controller_t *xdma;
	int err;

	xdma = xchan->xdma;
	KASSERT(xdma != NULL, ("xdma is NULL"));

	XDMA_LOCK();

	/* Free the real DMA channel. */
	err = XDMA_CHANNEL_FREE(xdma->dma_dev, xchan);
	if (err != 0) {
		device_printf(xdma->dev,
		    "%s: Can't free real hw channel.\n", __func__);
		XDMA_UNLOCK();
		return (-1);
	}

	if (xchan->flags & XCHAN_TYPE_SG)
		xdma_channel_free_sg(xchan);

	xdma_teardown_all_intr(xchan);

	mtx_destroy(&xchan->mtx_lock);
	mtx_destroy(&xchan->mtx_qin_lock);
	mtx_destroy(&xchan->mtx_qout_lock);
	mtx_destroy(&xchan->mtx_bank_lock);
	mtx_destroy(&xchan->mtx_proc_lock);

	TAILQ_REMOVE(&xdma->channels, xchan, xchan_next);

	free(xchan, M_XDMA);

	XDMA_UNLOCK();

	return (0);
}

int
xdma_setup_intr(xdma_channel_t *xchan,
    int (*cb)(void *, xdma_transfer_status_t *),
    void *arg, void **ihandler)
{
	struct xdma_intr_handler *ih;
	xdma_controller_t *xdma;

	xdma = xchan->xdma;
	KASSERT(xdma != NULL, ("xdma is NULL"));

	/* Sanity check. */
	if (cb == NULL) {
		device_printf(xdma->dev,
		    "%s: Can't setup interrupt handler.\n",
		    __func__);

		return (-1);
	}

	ih = malloc(sizeof(struct xdma_intr_handler),
	    M_XDMA, M_WAITOK | M_ZERO);
	ih->cb = cb;
	ih->cb_user = arg;

	XCHAN_LOCK(xchan);
	TAILQ_INSERT_TAIL(&xchan->ie_handlers, ih, ih_next);
	XCHAN_UNLOCK(xchan);

	if (ihandler != NULL)
		*ihandler = ih;

	return (0);
}

int
xdma_teardown_intr(xdma_channel_t *xchan, struct xdma_intr_handler *ih)
{
	xdma_controller_t *xdma;

	xdma = xchan->xdma;
	KASSERT(xdma != NULL, ("xdma is NULL"));

	/* Sanity check. */
	if (ih == NULL) {
		device_printf(xdma->dev,
		    "%s: Can't teardown interrupt.\n", __func__);
		return (-1);
	}

	TAILQ_REMOVE(&xchan->ie_handlers, ih, ih_next);
	free(ih, M_XDMA);

	return (0);
}

int
xdma_teardown_all_intr(xdma_channel_t *xchan)
{
	struct xdma_intr_handler *ih_tmp;
	struct xdma_intr_handler *ih;
	xdma_controller_t *xdma;

	xdma = xchan->xdma;
	KASSERT(xdma != NULL, ("xdma is NULL"));

	TAILQ_FOREACH_SAFE(ih, &xchan->ie_handlers, ih_next, ih_tmp) {
		TAILQ_REMOVE(&xchan->ie_handlers, ih, ih_next);
		free(ih, M_XDMA);
	}

	return (0);
}

int
xdma_request(xdma_channel_t *xchan, struct xdma_request *req)
{
	xdma_controller_t *xdma;
	int ret;

	xdma = xchan->xdma;

	KASSERT(xdma != NULL, ("xdma is NULL"));

	XCHAN_LOCK(xchan);
	ret = XDMA_CHANNEL_REQUEST(xdma->dma_dev, xchan, req);
	if (ret != 0) {
		device_printf(xdma->dev,
		    "%s: Can't request a transfer.\n", __func__);
		XCHAN_UNLOCK(xchan);

		return (-1);
	}
	XCHAN_UNLOCK(xchan);

	return (0);
}

int
xdma_control(xdma_channel_t *xchan, enum xdma_command cmd)
{
	xdma_controller_t *xdma;
	int ret;

	xdma = xchan->xdma;
	KASSERT(xdma != NULL, ("xdma is NULL"));

	ret = XDMA_CHANNEL_CONTROL(xdma->dma_dev, xchan, cmd);
	if (ret != 0) {
		device_printf(xdma->dev,
		    "%s: Can't process command.\n", __func__);
		return (-1);
	}

	return (0);
}

void
xdma_callback(xdma_channel_t *xchan, xdma_transfer_status_t *status)
{
	struct xdma_intr_handler *ih_tmp;
	struct xdma_intr_handler *ih;
	xdma_controller_t *xdma;

	xdma = xchan->xdma;
	KASSERT(xdma != NULL, ("xdma is NULL"));

	TAILQ_FOREACH_SAFE(ih, &xchan->ie_handlers, ih_next, ih_tmp)
		if (ih->cb != NULL)
			ih->cb(ih->cb_user, status);

	if (xchan->flags & XCHAN_TYPE_SG)
		xdma_queue_submit(xchan);
}

#ifdef FDT
/*
 * Notify the DMA driver we have machine-dependent data in FDT.
 */
static int
xdma_ofw_md_data(xdma_controller_t *xdma, pcell_t *cells, int ncells)
{
	uint32_t ret;

	ret = XDMA_OFW_MD_DATA(xdma->dma_dev,
	    cells, ncells, (void **)&xdma->data);

	return (ret);
}

static int
xdma_handle_mem_node(vmem_t *vmem, phandle_t memory)
{
	pcell_t reg[FDT_REG_CELLS * FDT_MEM_REGIONS];
	pcell_t *regp;
	int addr_cells, size_cells;
	int i, reg_len, ret, tuple_size, tuples;
	u_long mem_start, mem_size;

	if ((ret = fdt_addrsize_cells(OF_parent(memory), &addr_cells,
	    &size_cells)) != 0)
		return (ret);

	if (addr_cells > 2)
		return (ERANGE);

	tuple_size = sizeof(pcell_t) * (addr_cells + size_cells);
	reg_len = OF_getproplen(memory, "reg");
	if (reg_len <= 0 || reg_len > sizeof(reg))
		return (ERANGE);

	if (OF_getprop(memory, "reg", reg, reg_len) <= 0)
		return (ENXIO);

	tuples = reg_len / tuple_size;
	regp = (pcell_t *)&reg;
	for (i = 0; i < tuples; i++) {
		ret = fdt_data_to_res(regp, addr_cells, size_cells,
		    &mem_start, &mem_size);
		if (ret != 0)
			return (ret);

		vmem_add(vmem, mem_start, mem_size, 0);
		regp += addr_cells + size_cells;
	}

	return (0);
}

vmem_t *
xdma_get_memory(device_t dev)
{
	phandle_t mem_node, node;
	pcell_t mem_handle;
	vmem_t *vmem;

	node = ofw_bus_get_node(dev);
	if (node <= 0) {
		device_printf(dev,
		    "%s called on not ofw based device.\n", __func__);
		return (NULL);
	}

	if (!OF_hasprop(node, "memory-region"))
		return (NULL);

	if (OF_getencprop(node, "memory-region", (void *)&mem_handle,
	    sizeof(mem_handle)) <= 0)
		return (NULL);

	vmem = vmem_create("xDMA vmem", 0, 0, PAGE_SIZE,
	    PAGE_SIZE, M_BESTFIT | M_WAITOK);
	if (vmem == NULL)
		return (NULL);

	mem_node = OF_node_from_xref(mem_handle);
	if (xdma_handle_mem_node(vmem, mem_node) != 0) {
		vmem_destroy(vmem);
		return (NULL);
	}

	return (vmem);
}

void
xdma_put_memory(vmem_t *vmem)
{

	vmem_destroy(vmem);
}

void
xchan_set_memory(xdma_channel_t *xchan, vmem_t *vmem)
{

	xchan->vmem = vmem;
}

/*
 * Allocate xdma controller.
 */
xdma_controller_t *
xdma_ofw_get(device_t dev, const char *prop)
{
	phandle_t node, parent;
	xdma_controller_t *xdma;
	device_t dma_dev;
	pcell_t *cells;
	int ncells;
	int error;
	int ndmas;
	int idx;

	node = ofw_bus_get_node(dev);
	if (node <= 0)
		device_printf(dev,
		    "%s called on not ofw based device.\n", __func__);

	error = ofw_bus_parse_xref_list_get_length(node,
	    "dmas", "#dma-cells", &ndmas);
	if (error) {
		device_printf(dev,
		    "%s can't get dmas list.\n", __func__);
		return (NULL);
	}

	if (ndmas == 0) {
		device_printf(dev,
		    "%s dmas list is empty.\n", __func__);
		return (NULL);
	}

	error = ofw_bus_find_string_index(node, "dma-names", prop, &idx);
	if (error != 0) {
		device_printf(dev,
		    "%s can't find string index.\n", __func__);
		return (NULL);
	}

	error = ofw_bus_parse_xref_list_alloc(node, "dmas", "#dma-cells",
	    idx, &parent, &ncells, &cells);
	if (error != 0) {
		device_printf(dev,
		    "%s can't get dma device xref.\n", __func__);
		return (NULL);
	}

	dma_dev = OF_device_from_xref(parent);
	if (dma_dev == NULL) {
		device_printf(dev,
		    "%s can't get dma device.\n", __func__);
		return (NULL);
	}

	xdma = malloc(sizeof(struct xdma_controller),
	    M_XDMA, M_WAITOK | M_ZERO);
	xdma->dev = dev;
	xdma->dma_dev = dma_dev;

	TAILQ_INIT(&xdma->channels);

	xdma_ofw_md_data(xdma, cells, ncells);
	free(cells, M_OFWPROP);

	return (xdma);
}
#endif

/*
 * Free xDMA controller object.
 */
int
xdma_put(xdma_controller_t *xdma)
{

	XDMA_LOCK();

	/* Ensure no channels allocated. */
	if (!TAILQ_EMPTY(&xdma->channels)) {
		device_printf(xdma->dev, "%s: Can't free xDMA\n", __func__);
		return (-1);
	}

	free(xdma->data, M_DEVBUF);
	free(xdma, M_XDMA);

	XDMA_UNLOCK();

	return (0);
}

static void
xdma_init(void)
{

	mtx_init(&xdma_mtx, "xDMA", NULL, MTX_DEF);
}

SYSINIT(xdma, SI_SUB_DRIVERS, SI_ORDER_FIRST, xdma_init, NULL);
