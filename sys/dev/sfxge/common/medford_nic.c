/*-
 * Copyright (c) 2015-2016 Solarflare Communications Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the FreeBSD Project.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "efx.h"
#include "efx_impl.h"


#if EFSYS_OPT_MEDFORD

static	__checkReturn	efx_rc_t
medford_nic_get_required_pcie_bandwidth(
	__in		efx_nic_t *enp,
	__out		uint32_t *bandwidth_mbpsp)
{
	uint32_t port_modes;
	uint32_t current_mode;
	uint32_t bandwidth;
	efx_rc_t rc;

	if ((rc = efx_mcdi_get_port_modes(enp, &port_modes,
				    &current_mode)) != 0) {
		/* No port mode info available. */
		bandwidth = 0;
		goto out;
	}

	if ((rc = ef10_nic_get_port_mode_bandwidth(current_mode,
						    &bandwidth)) != 0)
		goto fail1;

out:
	*bandwidth_mbpsp = bandwidth;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
medford_board_cfg(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint32_t board_type = 0;
	ef10_link_state_t els;
	efx_port_t *epp = &(enp->en_port);
	uint32_t mask;
	uint32_t sysclk, dpcpu_clk;
	uint32_t base, nvec;
	uint32_t end_padding;
	uint32_t bandwidth;
	efx_rc_t rc;

	/*
	 * FIXME: Likely to be incomplete and incorrect.
	 * Parts of this should be shared with Huntington.
	 */

	/* Medford has a fixed 8Kbyte VI window size */
	EFX_STATIC_ASSERT(ER_DZ_EVQ_RPTR_REG_STEP	== 8192);
	EFX_STATIC_ASSERT(ER_DZ_EVQ_TMR_REG_STEP	== 8192);
	EFX_STATIC_ASSERT(ER_DZ_RX_DESC_UPD_REG_STEP	== 8192);
	EFX_STATIC_ASSERT(ER_DZ_TX_DESC_UPD_REG_STEP	== 8192);
	EFX_STATIC_ASSERT(ER_DZ_TX_PIOBUF_STEP		== 8192);

	EFX_STATIC_ASSERT(1U << EFX_VI_WINDOW_SHIFT_8K	== 8192);
	encp->enc_vi_window_shift = EFX_VI_WINDOW_SHIFT_8K;

	/* Board configuration */
	rc = efx_mcdi_get_board_cfg(enp, &board_type, NULL, NULL);
	if (rc != 0) {
		/* Unprivileged functions may not be able to read board cfg */
		if (rc == EACCES)
			board_type = 0;
		else
			goto fail1;
	}

	encp->enc_board_type = board_type;
	encp->enc_clk_mult = 1; /* not used for Medford */

	/* Fill out fields in enp->en_port and enp->en_nic_cfg from MCDI */
	if ((rc = efx_mcdi_get_phy_cfg(enp)) != 0)
		goto fail2;

	/* Obtain the default PHY advertised capabilities */
	if ((rc = ef10_phy_get_link(enp, &els)) != 0)
		goto fail3;
	epp->ep_default_adv_cap_mask = els.els_adv_cap_mask;
	epp->ep_adv_cap_mask = els.els_adv_cap_mask;

	/*
	 * Enable firmware workarounds for hardware errata.
	 * Expected responses are:
	 *  - 0 (zero):
	 *	Success: workaround enabled or disabled as requested.
	 *  - MC_CMD_ERR_ENOSYS (reported as ENOTSUP):
	 *	Firmware does not support the MC_CMD_WORKAROUND request.
	 *	(assume that the workaround is not supported).
	 *  - MC_CMD_ERR_ENOENT (reported as ENOENT):
	 *	Firmware does not support the requested workaround.
	 *  - MC_CMD_ERR_EPERM  (reported as EACCES):
	 *	Unprivileged function cannot enable/disable workarounds.
	 *
	 * See efx_mcdi_request_errcode() for MCDI error translations.
	 */


	if (EFX_PCI_FUNCTION_IS_VF(encp)) {
		/*
		 * Interrupt testing does not work for VFs. See bug50084 and
		 * bug71432 comment 21.
		 */
		encp->enc_bug41750_workaround = B_TRUE;
	}

	/* Chained multicast is always enabled on Medford */
	encp->enc_bug26807_workaround = B_TRUE;

	/*
	 * If the bug61265 workaround is enabled, then interrupt holdoff timers
	 * cannot be controlled by timer table writes, so MCDI must be used
	 * (timer table writes can still be used for wakeup timers).
	 */
	rc = efx_mcdi_set_workaround(enp, MC_CMD_WORKAROUND_BUG61265, B_TRUE,
	    NULL);
	if ((rc == 0) || (rc == EACCES))
		encp->enc_bug61265_workaround = B_TRUE;
	else if ((rc == ENOTSUP) || (rc == ENOENT))
		encp->enc_bug61265_workaround = B_FALSE;
	else
		goto fail4;

	/* Get clock frequencies (in MHz). */
	if ((rc = efx_mcdi_get_clock(enp, &sysclk, &dpcpu_clk)) != 0)
		goto fail5;

	/*
	 * The Medford timer quantum is 1536 dpcpu_clk cycles, documented for
	 * the EV_TMR_VAL field of EV_TIMER_TBL. Scale for MHz and ns units.
	 */
	encp->enc_evq_timer_quantum_ns = 1536000UL / dpcpu_clk; /* 1536 cycles */
	encp->enc_evq_timer_max_us = (encp->enc_evq_timer_quantum_ns <<
		    FRF_CZ_TC_TIMER_VAL_WIDTH) / 1000;

	/* Check capabilities of running datapath firmware */
	if ((rc = ef10_get_datapath_caps(enp)) != 0)
		goto fail6;

	/* Alignment for receive packet DMA buffers */
	encp->enc_rx_buf_align_start = 1;

	/* Get the RX DMA end padding alignment configuration */
	if ((rc = efx_mcdi_get_rxdp_config(enp, &end_padding)) != 0) {
		if (rc != EACCES)
			goto fail7;

		/* Assume largest tail padding size supported by hardware */
		end_padding = 256;
	}
	encp->enc_rx_buf_align_end = end_padding;

	/* Alignment for WPTR updates */
	encp->enc_rx_push_align = EF10_RX_WPTR_ALIGN;

	/*
	 * Maximum number of exclusive RSS contexts which can be allocated. The
	 * hardware supports 64, but 6 are reserved for shared contexts. They
	 * are a global resource so not all may be available.
	 */
	encp->enc_rx_scale_max_exclusive_contexts = 58;

	encp->enc_tx_dma_desc_size_max = EFX_MASK32(ESF_DZ_RX_KER_BYTE_CNT);
	/* No boundary crossing limits */
	encp->enc_tx_dma_desc_boundary = 0;

	/*
	 * Set resource limits for MC_CMD_ALLOC_VIS. Note that we cannot use
	 * MC_CMD_GET_RESOURCE_LIMITS here as that reports the available
	 * resources (allocated to this PCIe function), which is zero until
	 * after we have allocated VIs.
	 */
	encp->enc_evq_limit = 1024;
	encp->enc_rxq_limit = EFX_RXQ_LIMIT_TARGET;
	encp->enc_txq_limit = EFX_TXQ_LIMIT_TARGET;

	/*
	 * The maximum supported transmit queue size is 2048. TXQs with 4096
	 * descriptors are not supported as the top bit is used for vfifo
	 * stuffing.
	 */
	encp->enc_txq_max_ndescs = 2048;

	encp->enc_buftbl_limit = 0xFFFFFFFF;

	EFX_STATIC_ASSERT(MEDFORD_PIOBUF_NBUFS <= EF10_MAX_PIOBUF_NBUFS);
	encp->enc_piobuf_limit = MEDFORD_PIOBUF_NBUFS;
	encp->enc_piobuf_size = MEDFORD_PIOBUF_SIZE;
	encp->enc_piobuf_min_alloc_size = MEDFORD_MIN_PIO_ALLOC_SIZE;

	/*
	 * Get the current privilege mask. Note that this may be modified
	 * dynamically, so this value is informational only. DO NOT use
	 * the privilege mask to check for sufficient privileges, as that
	 * can result in time-of-check/time-of-use bugs.
	 */
	if ((rc = ef10_get_privilege_mask(enp, &mask)) != 0)
		goto fail8;
	encp->enc_privilege_mask = mask;

	/* Get interrupt vector limits */
	if ((rc = efx_mcdi_get_vector_cfg(enp, &base, &nvec, NULL)) != 0) {
		if (EFX_PCI_FUNCTION_IS_PF(encp))
			goto fail9;

		/* Ignore error (cannot query vector limits from a VF). */
		base = 0;
		nvec = 1024;
	}
	encp->enc_intr_vec_base = base;
	encp->enc_intr_limit = nvec;

	/*
	 * Maximum number of bytes into the frame the TCP header can start for
	 * firmware assisted TSO to work.
	 */
	encp->enc_tx_tso_tcp_header_offset_limit = EF10_TCP_HEADER_OFFSET_LIMIT;

	/*
	 * Medford stores a single global copy of VPD, not per-PF as on
	 * Huntington.
	 */
	encp->enc_vpd_is_global = B_TRUE;

	rc = medford_nic_get_required_pcie_bandwidth(enp, &bandwidth);
	if (rc != 0)
		goto fail10;
	encp->enc_required_pcie_bandwidth_mbps = bandwidth;
	encp->enc_max_pcie_link_gen = EFX_PCIE_LINK_SPEED_GEN3;

	return (0);

fail10:
	EFSYS_PROBE(fail10);
fail9:
	EFSYS_PROBE(fail9);
fail8:
	EFSYS_PROBE(fail8);
fail7:
	EFSYS_PROBE(fail7);
fail6:
	EFSYS_PROBE(fail6);
fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_MEDFORD */
