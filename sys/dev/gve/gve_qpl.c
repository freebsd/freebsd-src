/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Google LLC
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
#include <sys/malloc.h>

#include "gve.h"
#include "gve_adminq.h"

static MALLOC_DEFINE(M_GVE_QPL, "gve qpl", "gve qpl allocations");

static uint32_t
gve_num_tx_qpls(struct gve_priv *priv)
{
	if (priv->queue_format != GVE_GQI_QPL_FORMAT)
		return (0);

	return (priv->tx_cfg.max_queues);
}

static uint32_t
gve_num_rx_qpls(struct gve_priv *priv)
{
	if (priv->queue_format != GVE_GQI_QPL_FORMAT)
		return (0);

	return (priv->rx_cfg.max_queues);
}

static void
gve_free_qpl(struct gve_priv *priv, uint32_t id)
{
	struct gve_queue_page_list *qpl = &priv->qpls[id];
	int i;

	for (i = 0; i < qpl->num_dmas; i++) {
		gve_dmamap_destroy(&qpl->dmas[i]);
	}

	if (qpl->kva) {
		pmap_qremove(qpl->kva, qpl->num_pages);
		kva_free(qpl->kva, PAGE_SIZE * qpl->num_pages);
	}

	for (i = 0; i < qpl->num_pages; i++) {
		/*
		 * Free the page only if this is the last ref.
		 * Tx pages are known to have no other refs at
		 * this point, but Rx pages might still be in
		 * use by the networking stack, see gve_mextadd_free.
		 */
		if (vm_page_unwire_noq(qpl->pages[i])) {
			if (!qpl->kva) {
				pmap_qremove((vm_offset_t)qpl->dmas[i].cpu_addr, 1);
				kva_free((vm_offset_t)qpl->dmas[i].cpu_addr, PAGE_SIZE);
			}
			vm_page_free(qpl->pages[i]);
		}

		priv->num_registered_pages--;
	}

	if (qpl->pages != NULL)
		free(qpl->pages, M_GVE_QPL);

	if (qpl->dmas != NULL)
		free(qpl->dmas, M_GVE_QPL);
}

static int
gve_alloc_qpl(struct gve_priv *priv, uint32_t id, int npages, bool single_kva)
{
	struct gve_queue_page_list *qpl = &priv->qpls[id];
	int err;
	int i;

	if (npages + priv->num_registered_pages > priv->max_registered_pages) {
		device_printf(priv->dev, "Reached max number of registered pages %lu > %lu\n",
		    npages + priv->num_registered_pages,
		    priv->max_registered_pages);
		return (EINVAL);
	}

	qpl->id = id;
	qpl->num_pages = 0;
	qpl->num_dmas = 0;

	qpl->dmas = malloc(npages * sizeof(*qpl->dmas), M_GVE_QPL,
	    M_WAITOK | M_ZERO);

	qpl->pages = malloc(npages * sizeof(*qpl->pages), M_GVE_QPL,
	    M_WAITOK | M_ZERO);

	qpl->kva = 0;
	if (single_kva) {
		qpl->kva = kva_alloc(PAGE_SIZE * npages);
		if (!qpl->kva) {
			device_printf(priv->dev, "Failed to create the single kva for QPL %d\n", id);
			err = ENOMEM;
			goto abort;
		}
	}

	for (i = 0; i < npages; i++) {
		qpl->pages[i] = vm_page_alloc_noobj(VM_ALLOC_WIRED |
						    VM_ALLOC_WAITOK |
						    VM_ALLOC_ZERO);

		if (!single_kva) {
			qpl->dmas[i].cpu_addr = (void *)kva_alloc(PAGE_SIZE);
			if (!qpl->dmas[i].cpu_addr) {
				device_printf(priv->dev, "Failed to create kva for page %d in QPL %d", i, id);
				err = ENOMEM;
				goto abort;
			}
			pmap_qenter((vm_offset_t)qpl->dmas[i].cpu_addr, &(qpl->pages[i]), 1);
		} else
			qpl->dmas[i].cpu_addr = (void *)(qpl->kva + (PAGE_SIZE * i));


		qpl->num_pages++;
	}

	if (single_kva)
		pmap_qenter(qpl->kva, qpl->pages, npages);

	for (i = 0; i < npages; i++) {
		err = gve_dmamap_create(priv, /*size=*/PAGE_SIZE, /*align=*/PAGE_SIZE,
		    &qpl->dmas[i]);
		if (err != 0) {
			device_printf(priv->dev, "Failed to dma-map page %d in QPL %d\n", i, id);
			goto abort;
		}

		qpl->num_dmas++;
		priv->num_registered_pages++;
	}

	return (0);

abort:
	gve_free_qpl(priv, id);
	return (err);
}

void
gve_free_qpls(struct gve_priv *priv)
{
	int num_qpls = gve_num_tx_qpls(priv) + gve_num_rx_qpls(priv);
	int i;

	if (num_qpls == 0)
		return;

	if (priv->qpls != NULL) {
		for (i = 0; i < num_qpls; i++)
			gve_free_qpl(priv, i);
		free(priv->qpls, M_GVE_QPL);
	}
}

int gve_alloc_qpls(struct gve_priv *priv)
{
	int num_qpls = gve_num_tx_qpls(priv) + gve_num_rx_qpls(priv);
	int err;
	int i;

	if (num_qpls == 0)
		return (0);

	priv->qpls = malloc(num_qpls * sizeof(*priv->qpls), M_GVE_QPL,
	    M_WAITOK | M_ZERO);

	for (i = 0; i < gve_num_tx_qpls(priv); i++) {
		err = gve_alloc_qpl(priv, i, priv->tx_desc_cnt / GVE_QPL_DIVISOR,
		    /*single_kva=*/true);
		if (err != 0)
			goto abort;
	}

	for (; i < num_qpls; i++) {
		err = gve_alloc_qpl(priv, i, priv->rx_desc_cnt, /*single_kva=*/false);
		if (err != 0)
			goto abort;
	}

	return (0);

abort:
	gve_free_qpls(priv);
	return (err);
}

static int
gve_unregister_n_qpls(struct gve_priv *priv, int n)
{
	int err;
	int i;

	for (i = 0; i < n; i++) {
		err = gve_adminq_unregister_page_list(priv, priv->qpls[i].id);
		if (err != 0) {
			device_printf(priv->dev,
			    "Failed to unregister qpl %d, err: %d\n",
			    priv->qpls[i].id, err);
		}
	}

	if (err != 0)
		return (err);

	return (0);
}

int
gve_register_qpls(struct gve_priv *priv)
{
	int num_qpls = gve_num_tx_qpls(priv) + gve_num_rx_qpls(priv);
	int err;
	int i;

	if (gve_get_state_flag(priv, GVE_STATE_FLAG_QPLREG_OK))
		return (0);

	for (i = 0; i < num_qpls; i++) {
		err = gve_adminq_register_page_list(priv, &priv->qpls[i]);
		if (err != 0) {
			device_printf(priv->dev,
			    "Failed to register qpl %d, err: %d\n",
			    priv->qpls[i].id, err);
			goto abort;
		}
	}

	gve_set_state_flag(priv, GVE_STATE_FLAG_QPLREG_OK);
	return (0);

abort:
	gve_unregister_n_qpls(priv, i);
	return (err);
}

int
gve_unregister_qpls(struct gve_priv *priv)
{
	int num_qpls = gve_num_tx_qpls(priv) + gve_num_rx_qpls(priv);
	int err;

	if (!gve_get_state_flag(priv, GVE_STATE_FLAG_QPLREG_OK))
		return (0);

	err = gve_unregister_n_qpls(priv, num_qpls);
	if (err != 0)
		return (err);

	gve_clear_state_flag(priv, GVE_STATE_FLAG_QPLREG_OK);
	return (0);
}
