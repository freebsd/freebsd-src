/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023-2024 Google LLC
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "gve.h"
#include "gve_dqo.h"

uint32_t
gve_reg_bar_read_4(struct gve_priv *priv, bus_size_t offset)
{
	return (be32toh(bus_read_4(priv->reg_bar, offset)));
}

void
gve_reg_bar_write_4(struct gve_priv *priv, bus_size_t offset, uint32_t val)
{
	bus_write_4(priv->reg_bar, offset, htobe32(val));
}

void
gve_db_bar_write_4(struct gve_priv *priv, bus_size_t offset, uint32_t val)
{
	bus_write_4(priv->db_bar, offset, htobe32(val));
}

void
gve_db_bar_dqo_write_4(struct gve_priv *priv, bus_size_t offset, uint32_t val)
{
	bus_write_4(priv->db_bar, offset, val);
}

void
gve_alloc_counters(counter_u64_t *stat, int num_stats)
{
	int i;

	for (i = 0; i < num_stats; i++)
		stat[i] = counter_u64_alloc(M_WAITOK);
}

void
gve_free_counters(counter_u64_t *stat, int num_stats)
{
	int i;

	for (i = 0; i < num_stats; i++)
		counter_u64_free(stat[i]);
}

/* Currently assumes a single segment. */
static void
gve_dmamap_load_callback(void *arg, bus_dma_segment_t *segs, int nseg,
    int error)
{
	if (error == 0)
		*(bus_addr_t *) arg = segs[0].ds_addr;
}

int
gve_dma_alloc_coherent(struct gve_priv *priv, int size, int align,
    struct gve_dma_handle *dma)
{
	int err;
	device_t dev = priv->dev;

	err = bus_dma_tag_create(
	    bus_get_dma_tag(dev),	/* parent */
	    align, 0,			/* alignment, bounds */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    size,			/* maxsize */
	    1,				/* nsegments */
	    size,			/* maxsegsize */
	    BUS_DMA_ALLOCNOW,		/* flags */
	    NULL,			/* lockfunc */
	    NULL,			/* lockarg */
	    &dma->tag);
	if (err != 0) {
		device_printf(dev, "%s: bus_dma_tag_create failed: %d\n",
		    __func__, err);
		goto clear_tag;
	}

	err = bus_dmamem_alloc(dma->tag, (void **) &dma->cpu_addr,
	    BUS_DMA_WAITOK | BUS_DMA_COHERENT | BUS_DMA_ZERO,
	    &dma->map);
	if (err != 0) {
		device_printf(dev, "%s: bus_dmamem_alloc(%ju) failed: %d\n",
		    __func__, (uintmax_t)size, err);
		goto destroy_tag;
	}

	/* An address set by the callback will never be -1 */
	dma->bus_addr = (bus_addr_t)-1;
	err = bus_dmamap_load(dma->tag, dma->map, dma->cpu_addr, size,
	    gve_dmamap_load_callback, &dma->bus_addr, BUS_DMA_NOWAIT);
	if (err != 0 || dma->bus_addr == (bus_addr_t)-1) {
		device_printf(dev, "%s: bus_dmamap_load failed: %d\n", __func__, err);
		goto free_mem;
	}

	return (0);

free_mem:
	bus_dmamem_free(dma->tag, dma->cpu_addr, dma->map);
destroy_tag:
	bus_dma_tag_destroy(dma->tag);
clear_tag:
	dma->tag = NULL;

	return (err);
}

void
gve_dma_free_coherent(struct gve_dma_handle *dma)
{
	bus_dmamap_sync(dma->tag, dma->map, BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(dma->tag, dma->map);
	bus_dmamem_free(dma->tag, dma->cpu_addr, dma->map);
	bus_dma_tag_destroy(dma->tag);
}

int
gve_dmamap_create(struct gve_priv *priv, int size, int align,
    struct gve_dma_handle *dma)
{
	int err;
	device_t dev = priv->dev;

	err = bus_dma_tag_create(
	    bus_get_dma_tag(dev),	/* parent */
	    align, 0,			/* alignment, bounds */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    size,			/* maxsize */
	    1,				/* nsegments */
	    size,			/* maxsegsize */
	    BUS_DMA_ALLOCNOW,		/* flags */
	    NULL,			/* lockfunc */
	    NULL,			/* lockarg */
	    &dma->tag);
	if (err != 0) {
		device_printf(dev, "%s: bus_dma_tag_create failed: %d\n",
		    __func__, err);
		goto clear_tag;
	}

	err = bus_dmamap_create(dma->tag, BUS_DMA_COHERENT, &dma->map);
	if (err != 0) {
		device_printf(dev, "%s: bus_dmamap_create failed: %d\n",
		    __func__, err);
		goto destroy_tag;
	}

	/* An address set by the callback will never be -1 */
	dma->bus_addr = (bus_addr_t)-1;
	err = bus_dmamap_load(dma->tag, dma->map, dma->cpu_addr, size,
	    gve_dmamap_load_callback, &dma->bus_addr, BUS_DMA_WAITOK);
	if (err != 0 || dma->bus_addr == (bus_addr_t)-1) {
		device_printf(dev, "%s: bus_dmamap_load failed: %d\n",
		    __func__, err);
		goto destroy_map;
	}

	return (0);

destroy_map:
	bus_dmamap_destroy(dma->tag, dma->map);
destroy_tag:
	bus_dma_tag_destroy(dma->tag);
clear_tag:
	dma->tag = NULL;

	return (err);
}

void
gve_dmamap_destroy(struct gve_dma_handle *dma)
{
	bus_dmamap_sync(dma->tag, dma->map, BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(dma->tag, dma->map);
	bus_dmamap_destroy(dma->tag, dma->map);
	bus_dma_tag_destroy(dma->tag);
}

static int
gve_mgmnt_intr(void *arg)
{
	struct gve_priv *priv = arg;

	taskqueue_enqueue(priv->service_tq, &priv->service_task);
	return (FILTER_HANDLED);
}

void
gve_free_irqs(struct gve_priv *priv)
{
	struct gve_irq *irq;
	int num_irqs;
	int rid;
	int rc;
	int i;

	if (priv->irq_tbl == NULL) {
		device_printf(priv->dev, "No irq table, nothing to free\n");
		return;
	}

	num_irqs = priv->tx_cfg.num_queues + priv->rx_cfg.num_queues + 1;

	for (i = 0; i < num_irqs; i++) {
		irq = &priv->irq_tbl[i];
		if (irq->res == NULL)
			continue;

		rid = rman_get_rid(irq->res);

		rc = bus_teardown_intr(priv->dev, irq->res, irq->cookie);
		if (rc != 0)
			device_printf(priv->dev, "Failed to teardown irq num %d\n",
			    rid);

		rc = bus_release_resource(priv->dev, SYS_RES_IRQ,
		    rid, irq->res);
		if (rc != 0)
			device_printf(priv->dev, "Failed to release irq num %d\n",
			    rid);

		irq->res = NULL;
		irq->cookie = NULL;
	}

	free(priv->irq_tbl, M_GVE);
	priv->irq_tbl = NULL;

	/* Safe to call even if msix was never alloced */
	pci_release_msi(priv->dev);
}

int
gve_alloc_irqs(struct gve_priv *priv)
{
	int num_tx = priv->tx_cfg.num_queues;
	int num_rx = priv->rx_cfg.num_queues;
	int req_nvecs = num_tx + num_rx + 1;
	int got_nvecs = req_nvecs;
	struct gve_irq *irq;
	int i, j, m;
	int rid;
	int err;

	struct gve_ring_com *com;
	struct gve_rx_ring *rx;
	struct gve_tx_ring *tx;

	if (pci_alloc_msix(priv->dev, &got_nvecs) != 0) {
		device_printf(priv->dev, "Failed to acquire any msix vectors\n");
		err = ENXIO;
		goto abort;
	} else if (got_nvecs != req_nvecs) {
		device_printf(priv->dev, "Tried to acquire %d msix vectors, got only %d\n",
		    req_nvecs, got_nvecs);
		err = ENOSPC;
		goto abort;
        }

	if (bootverbose)
		device_printf(priv->dev, "Enabled MSIX with %d vectors\n", got_nvecs);

	priv->irq_tbl = malloc(sizeof(struct gve_irq) * req_nvecs, M_GVE,
	    M_WAITOK | M_ZERO);

	for (i = 0; i < num_tx; i++) {
		irq = &priv->irq_tbl[i];
		tx = &priv->tx[i];
		com = &tx->com;
		rid = i + 1;

		irq->res = bus_alloc_resource_any(priv->dev, SYS_RES_IRQ,
		    &rid, RF_ACTIVE);
		if (irq->res == NULL) {
			device_printf(priv->dev, "Failed to alloc irq %d for Tx queue %d\n",
			    rid, i);
			err = ENOMEM;
			goto abort;
		}

		err = bus_setup_intr(priv->dev, irq->res, INTR_TYPE_NET | INTR_MPSAFE,
		    gve_is_gqi(priv) ? gve_tx_intr : gve_tx_intr_dqo, NULL,
		    &priv->tx[i], &irq->cookie);
		if (err != 0) {
			device_printf(priv->dev, "Failed to setup irq %d for Tx queue %d, "
			    "err: %d\n", rid, i, err);
			goto abort;
		}

		bus_describe_intr(priv->dev, irq->res, irq->cookie, "tx%d", i);
		com->ntfy_id = i;
	}

	for (j = 0; j < num_rx; j++) {
		irq = &priv->irq_tbl[i + j];
		rx = &priv->rx[j];
		com = &rx->com;
		rid = i + j + 1;

		irq->res = bus_alloc_resource_any(priv->dev, SYS_RES_IRQ,
		    &rid, RF_ACTIVE);
		if (irq->res == NULL) {
			device_printf(priv->dev,
			    "Failed to alloc irq %d for Rx queue %d", rid, j);
			err = ENOMEM;
			goto abort;
		}

		err = bus_setup_intr(priv->dev, irq->res, INTR_TYPE_NET | INTR_MPSAFE,
		    gve_is_gqi(priv) ? gve_rx_intr : gve_rx_intr_dqo, NULL,
		    &priv->rx[j], &irq->cookie);
		if (err != 0) {
			device_printf(priv->dev, "Failed to setup irq %d for Rx queue %d, "
			    "err: %d\n", rid, j, err);
			goto abort;
		}

		bus_describe_intr(priv->dev, irq->res, irq->cookie, "rx%d", j);
		com->ntfy_id = i + j;
	}

	m = i + j;
	rid = m + 1;
	irq = &priv->irq_tbl[m];

	irq->res = bus_alloc_resource_any(priv->dev, SYS_RES_IRQ,
	    &rid, RF_ACTIVE);
	if (irq->res == NULL) {
		device_printf(priv->dev, "Failed to allocate irq %d for mgmnt queue\n", rid);
		err = ENOMEM;
		goto abort;
	}

	err = bus_setup_intr(priv->dev, irq->res, INTR_TYPE_NET | INTR_MPSAFE,
	    gve_mgmnt_intr, NULL, priv, &irq->cookie);
	if (err != 0) {
		device_printf(priv->dev, "Failed to setup irq %d for mgmnt queue, err: %d\n",
		    rid, err);
		goto abort;
	}

	bus_describe_intr(priv->dev, irq->res, irq->cookie, "mgmnt");

	return (0);

abort:
	gve_free_irqs(priv);
	return (err);
}

/*
 * Builds register value to write to DQO IRQ doorbell to enable with specified
 * ITR interval.
 */
static uint32_t
gve_setup_itr_interval_dqo(uint32_t interval_us)
{
	uint32_t result = GVE_ITR_ENABLE_BIT_DQO;

	/* Interval has 2us granularity. */
	interval_us >>= 1;

	interval_us &= GVE_ITR_INTERVAL_DQO_MASK;
	result |= (interval_us << GVE_ITR_INTERVAL_DQO_SHIFT);

	return (result);
}

void
gve_unmask_all_queue_irqs(struct gve_priv *priv)
{
	struct gve_tx_ring *tx;
	struct gve_rx_ring *rx;
	int idx;

	for (idx = 0; idx < priv->tx_cfg.num_queues; idx++) {
		tx = &priv->tx[idx];
		if (gve_is_gqi(priv))
			gve_db_bar_write_4(priv, tx->com.irq_db_offset, 0);
		else
			gve_db_bar_dqo_write_4(priv, tx->com.irq_db_offset,
			    gve_setup_itr_interval_dqo(GVE_TX_IRQ_RATELIMIT_US_DQO));
	}

	for (idx = 0; idx < priv->rx_cfg.num_queues; idx++) {
		rx = &priv->rx[idx];
		if (gve_is_gqi(priv))
			gve_db_bar_write_4(priv, rx->com.irq_db_offset, 0);
		else
			gve_db_bar_dqo_write_4(priv, rx->com.irq_db_offset,
			    gve_setup_itr_interval_dqo(GVE_RX_IRQ_RATELIMIT_US_DQO));
	}
}

void
gve_mask_all_queue_irqs(struct gve_priv *priv)
{
	for (int idx = 0; idx < priv->tx_cfg.num_queues; idx++) {
		struct gve_tx_ring *tx = &priv->tx[idx];
		gve_db_bar_write_4(priv, tx->com.irq_db_offset, GVE_IRQ_MASK);
	}
	for (int idx = 0; idx < priv->rx_cfg.num_queues; idx++) {
		struct gve_rx_ring *rx = &priv->rx[idx];
		gve_db_bar_write_4(priv, rx->com.irq_db_offset, GVE_IRQ_MASK);
	}
}
