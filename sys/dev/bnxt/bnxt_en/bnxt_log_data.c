/*-
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2026 Broadcom Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "bnxt_log.h"
#include "bnxt_log_data.h"

static void
bnxt_log_drv_version(struct bnxt_softc *bp)
{
	bnxt_log_live(bp, BNXT_LOGGER_L2, "\n");

	bnxt_log_live(bp, BNXT_LOGGER_L2, "Interface: L2  driver version: %s\n",
		      bnxt_driver_version);
}

static void
bnxt_log_tx_sw_state(struct bnxt_softc *softc)
{
	int i;
	uint32_t prod, cons;

	for (i = 0; i < softc->ntxqsets; i++) {
		bnxt_hwrm_ring_info_get(softc,
		    HWRM_DBG_RING_INFO_GET_INPUT_RING_TYPE_TX, i, &prod, &cons);
		bnxt_log_live(softc, BNXT_LOGGER_L2,
		    "tx {fw_ring: %d prod: %x cons: %x}\n",
			      i, prod, cons);
	}
}

static void
bnxt_log_rx_sw_state(struct bnxt_softc *softc)
{
	int i;
	uint32_t prod, cons;

	for (i = 0; i < softc->nrxqsets; i++) {
		bnxt_hwrm_ring_info_get(softc,
		    HWRM_DBG_RING_INFO_GET_INPUT_RING_TYPE_RX, i, &prod, &cons);
		bnxt_log_live(softc, BNXT_LOGGER_L2,
		    "rx{fw_ring: %d prod: %x}\n", i, prod);
	}

}

void
bnxt_log_ring_states(struct bnxt_softc *bp)
{
	bnxt_log_drv_version(bp);
	bnxt_log_tx_sw_state(bp);
	bnxt_log_rx_sw_state(bp);
}
