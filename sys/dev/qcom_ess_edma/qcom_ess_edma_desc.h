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

#ifndef	__QCOM_ESS_EDMA_DESC_H__
#define	__QCOM_ESS_EDMA_DESC_H__

extern	int qcom_ess_edma_desc_ring_setup(struct qcom_ess_edma_softc *sc,
	    struct qcom_ess_edma_desc_ring *ring,
	    char *label,
	    int count,
	    int sw_desc_size,
	    int hw_desc_size,
	    int num_segments,
	    int buffer_alignment);
extern	int qcom_ess_edma_desc_ring_free(struct qcom_ess_edma_softc *sc,
	    struct qcom_ess_edma_desc_ring *ring);
extern	void * qcom_ess_edma_desc_ring_get_sw_desc(
	    struct qcom_ess_edma_softc *sc,
	    struct qcom_ess_edma_desc_ring *ring,
	    uint16_t index);
extern	void * qcom_ess_edma_desc_ring_get_hw_desc(
	    struct qcom_ess_edma_softc *sc,
	    struct qcom_ess_edma_desc_ring *ring,
	    uint16_t index);
extern	int qcom_ess_edma_desc_ring_flush_preupdate(
	    struct qcom_ess_edma_softc *sc,
	    struct qcom_ess_edma_desc_ring *ring);
extern	int qcom_ess_edma_desc_ring_flush_postupdate(
	    struct qcom_ess_edma_softc *sc,
	    struct qcom_ess_edma_desc_ring *ring);
extern	int qcom_ess_edma_desc_ring_get_num_available(
	    struct qcom_ess_edma_softc *sc,
	    struct qcom_ess_edma_desc_ring *ring);

#endif	/* __QCOM_ESS_EDMA_DESC_H__ */
