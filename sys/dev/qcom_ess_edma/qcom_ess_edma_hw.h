/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Adrian Chadd <adrian@FreeBSD.org>.
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
 *
 * $FreeBSD$
 *
 */

#ifndef	__QCOM_ESS_EDMA_HW_H__
#define	__QCOM_ESS_EDMA_HW_H__

extern	int qcom_ess_edma_hw_reset(struct qcom_ess_edma_softc *sc);
extern	int qcom_ess_edma_hw_get_tx_intr_moderation(
	    struct qcom_ess_edma_softc *sc, uint32_t *usec);
extern	int qcom_ess_edma_hw_set_tx_intr_moderation(
	    struct qcom_ess_edma_softc *sc, uint32_t usec);
extern	int qcom_ess_edma_hw_set_rx_intr_moderation(
	    struct qcom_ess_edma_softc *sc, uint32_t usec);
extern	int qcom_ess_edma_hw_intr_disable(struct qcom_ess_edma_softc *sc);
extern	int qcom_ess_edma_hw_intr_rx_intr_set_enable(
	    struct qcom_ess_edma_softc *sc, int rxq, bool state);
extern	int qcom_ess_edma_hw_intr_tx_intr_set_enable(
	    struct qcom_ess_edma_softc *sc, int txq, bool state);
extern	int qcom_ess_edma_hw_intr_enable(struct qcom_ess_edma_softc *sc);
extern	int qcom_ess_edma_hw_intr_status_clear(
	    struct qcom_ess_edma_softc *sc);
extern	int qcom_ess_edma_hw_intr_rx_ack(struct qcom_ess_edma_softc *sc,
	    int rx_queue);
extern	int qcom_ess_edma_hw_intr_tx_ack(struct qcom_ess_edma_softc *sc,
	    int tx_queue);
extern	int qcom_ess_edma_hw_configure_rss_table(
	    struct qcom_ess_edma_softc *sc);
extern	int qcom_ess_edma_hw_configure_load_balance_table(
	    struct qcom_ess_edma_softc *sc);
extern	int qcom_ess_edma_hw_configure_tx_virtual_queue(
	    struct qcom_ess_edma_softc *sc);
extern	int qcom_ess_edma_hw_configure_default_axi_transaction_size(
	    struct qcom_ess_edma_softc *sc);
extern	int qcom_ess_edma_hw_stop_txrx_queues(struct qcom_ess_edma_softc *sc);
extern	int qcom_ess_edma_hw_stop(struct qcom_ess_edma_softc *sc);

extern	int qcom_ess_edma_hw_rfd_prod_index_update(
	    struct qcom_ess_edma_softc *sc, int queue, int idx);
extern	int qcom_ess_edma_hw_rfd_get_cons_index(
	    struct qcom_ess_edma_softc *sc, int queue);
extern	int qcom_ess_edma_hw_rfd_sw_cons_index_update(
	    struct qcom_ess_edma_softc *sc, int queue, int idx);

extern	int qcom_ess_edma_hw_setup(struct qcom_ess_edma_softc *sc);
extern	int qcom_ess_edma_hw_setup_tx(struct qcom_ess_edma_softc *sc);
extern	int qcom_ess_edma_hw_setup_rx(struct qcom_ess_edma_softc *sc);
extern	int qcom_ess_edma_hw_setup_txrx_desc_rings(
	    struct qcom_ess_edma_softc *sc);
extern	int qcom_ess_edma_hw_tx_enable(struct qcom_ess_edma_softc *sc);
extern	int qcom_ess_edma_hw_rx_enable(struct qcom_ess_edma_softc *sc);
extern	int qcom_ess_edma_hw_tx_read_tpd_cons_idx(
	    struct qcom_ess_edma_softc *sc, int queue_id, uint16_t *idx);
extern	int qcom_ess_edma_hw_tx_update_tpd_prod_idx(
	    struct qcom_ess_edma_softc *sc, int queue_id, uint16_t idx);
extern	int qcom_ess_edma_hw_tx_update_cons_idx(
	    struct qcom_ess_edma_softc *sc, int queue_id, uint16_t idx);

#endif	/* __QCOM_ESS_EDMA_VAR_H__ */
