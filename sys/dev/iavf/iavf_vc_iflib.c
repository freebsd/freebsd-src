/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2021, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file iavf_vc_iflib.c
 * @brief iflib-specific Virtchnl interface functions
 *
 * Contains functions implementing the virtchnl interface for communicating
 * with the PF driver. This file contains definitions specific to the iflib
 * driver implementation.
 */

#include "iavf_iflib.h"
#include "iavf_vc_common.h"

/**
 * iavf_configure_queues - Configure queues
 * @sc: device softc
 *
 * Request that the PF set up our queues.
 *
 * @returns zero on success, or an error code on failure.
 */
int
iavf_configure_queues(struct iavf_sc *sc)
{
	device_t dev = sc->dev;
	struct iavf_vsi *vsi = &sc->vsi;
	if_softc_ctx_t		scctx = iflib_get_softc_ctx(vsi->ctx);
	struct iavf_tx_queue	*tx_que = vsi->tx_queues;
	struct iavf_rx_queue	*rx_que = vsi->rx_queues;
	struct tx_ring		*txr;
	struct rx_ring		*rxr;
	int			len, pairs;

	struct virtchnl_vsi_queue_config_info *vqci;
	struct virtchnl_queue_pair_info *vqpi;

	/* XXX: Linux PF driver wants matching ids in each tx/rx struct, so both TX/RX
	 * queues of a pair need to be configured */
	pairs = max(vsi->num_tx_queues, vsi->num_rx_queues);
	len = sizeof(struct virtchnl_vsi_queue_config_info) +
		       (sizeof(struct virtchnl_queue_pair_info) * pairs);
	vqci = malloc(len, M_IAVF, M_NOWAIT | M_ZERO);
	if (!vqci) {
		device_printf(dev, "%s: unable to allocate memory\n", __func__);
		return (ENOMEM);
	}
	vqci->vsi_id = sc->vsi_res->vsi_id;
	vqci->num_queue_pairs = pairs;
	vqpi = vqci->qpair;
	/* Size check is not needed here - HW max is 16 queue pairs, and we
	 * can fit info for 31 of them into the AQ buffer before it overflows.
	 */
	// TODO: the above is wrong now; X722 VFs can have 256 queues
	for (int i = 0; i < pairs; i++, tx_que++, rx_que++, vqpi++) {
		txr = &tx_que->txr;
		rxr = &rx_que->rxr;

		vqpi->txq.vsi_id = vqci->vsi_id;
		vqpi->txq.queue_id = i;
		vqpi->txq.ring_len = scctx->isc_ntxd[0];
		vqpi->txq.dma_ring_addr = txr->tx_paddr;
		/* Enable Head writeback */
		if (!vsi->enable_head_writeback) {
			vqpi->txq.headwb_enabled = 0;
			vqpi->txq.dma_headwb_addr = 0;
		} else {
			vqpi->txq.headwb_enabled = 1;
			vqpi->txq.dma_headwb_addr = txr->tx_paddr +
			    sizeof(struct iavf_tx_desc) * scctx->isc_ntxd[0];
		}

		vqpi->rxq.vsi_id = vqci->vsi_id;
		vqpi->rxq.queue_id = i;
		vqpi->rxq.ring_len = scctx->isc_nrxd[0];
		vqpi->rxq.dma_ring_addr = rxr->rx_paddr;
		vqpi->rxq.max_pkt_size = scctx->isc_max_frame_size;
		vqpi->rxq.databuffer_size = rxr->mbuf_sz;
		vqpi->rxq.splithdr_enabled = 0;
	}

	iavf_send_pf_msg(sc, VIRTCHNL_OP_CONFIG_VSI_QUEUES,
			   (u8 *)vqci, len);
	free(vqci, M_IAVF);

	return (0);
}

/**
 * iavf_map_queues - Map queues to interrupt vectors
 * @sc: device softc
 *
 * Request that the PF map queues to interrupt vectors. Misc causes, including
 * admin queue, are always mapped to vector 0.
 *
 * @returns zero on success, or an error code on failure.
 */
int
iavf_map_queues(struct iavf_sc *sc)
{
	struct virtchnl_irq_map_info *vm;
	int			i, q, len;
	struct iavf_vsi		*vsi = &sc->vsi;
	struct iavf_rx_queue	*rx_que = vsi->rx_queues;
	if_softc_ctx_t		scctx = vsi->shared;
	device_t		dev = sc->dev;

	// XXX: What happens if we only get 1 MSI-X vector?
	MPASS(scctx->isc_vectors > 1);

	/* How many queue vectors, adminq uses one */
	// XXX: How do we know how many interrupt vectors we have?
	q = scctx->isc_vectors - 1;

	len = sizeof(struct virtchnl_irq_map_info) +
	      (scctx->isc_vectors * sizeof(struct virtchnl_vector_map));
	vm = malloc(len, M_IAVF, M_NOWAIT);
	if (!vm) {
		device_printf(dev, "%s: unable to allocate memory\n", __func__);
		return (ENOMEM);
	}

	vm->num_vectors = scctx->isc_vectors;
	/* Queue vectors first */
	for (i = 0; i < q; i++, rx_que++) {
		vm->vecmap[i].vsi_id = sc->vsi_res->vsi_id;
		vm->vecmap[i].vector_id = i + 1; /* first is adminq */
		// TODO: Re-examine this
		vm->vecmap[i].txq_map = (1 << rx_que->rxr.me);
		vm->vecmap[i].rxq_map = (1 << rx_que->rxr.me);
		vm->vecmap[i].rxitr_idx = 0;
		vm->vecmap[i].txitr_idx = 1;
	}

	/* Misc vector last - this is only for AdminQ messages */
	vm->vecmap[i].vsi_id = sc->vsi_res->vsi_id;
	vm->vecmap[i].vector_id = 0;
	vm->vecmap[i].txq_map = 0;
	vm->vecmap[i].rxq_map = 0;
	vm->vecmap[i].rxitr_idx = 0;
	vm->vecmap[i].txitr_idx = 0;

	iavf_send_pf_msg(sc, VIRTCHNL_OP_CONFIG_IRQ_MAP,
	    (u8 *)vm, len);
	free(vm, M_IAVF);

	return (0);
}
