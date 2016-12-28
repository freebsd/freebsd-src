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
efx_mcdi_get_rxdp_config(
	__in		efx_nic_t *enp,
	__out		uint32_t *end_paddingp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_GET_RXDP_CONFIG_IN_LEN,
			    MC_CMD_GET_RXDP_CONFIG_OUT_LEN)];
	uint32_t end_padding;
	efx_rc_t rc;

	memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_GET_RXDP_CONFIG;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_RXDP_CONFIG_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_RXDP_CONFIG_OUT_LEN;

	efx_mcdi_execute(enp, &req);
	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (MCDI_OUT_DWORD_FIELD(req, GET_RXDP_CONFIG_OUT_DATA,
				    GET_RXDP_CONFIG_OUT_PAD_HOST_DMA) == 0) {
		/* RX DMA end padding is disabled */
		end_padding = 0;
	} else {
		switch (MCDI_OUT_DWORD_FIELD(req, GET_RXDP_CONFIG_OUT_DATA,
					    GET_RXDP_CONFIG_OUT_PAD_HOST_LEN)) {
		case MC_CMD_SET_RXDP_CONFIG_IN_PAD_HOST_64:
			end_padding = 64;
			break;
		case MC_CMD_SET_RXDP_CONFIG_IN_PAD_HOST_128:
			end_padding = 128;
			break;
		case MC_CMD_SET_RXDP_CONFIG_IN_PAD_HOST_256:
			end_padding = 256;
			break;
		default:
			rc = ENOTSUP;
			goto fail2;
		}
	}

	*end_paddingp = end_padding;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

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
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint8_t mac_addr[6] = { 0 };
	uint32_t board_type = 0;
	ef10_link_state_t els;
	efx_port_t *epp = &(enp->en_port);
	uint32_t port;
	uint32_t pf;
	uint32_t vf;
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

	if ((rc = efx_mcdi_get_port_assignment(enp, &port)) != 0)
		goto fail1;

	/*
	 * NOTE: The MCDI protocol numbers ports from zero.
	 * The common code MCDI interface numbers ports from one.
	 */
	emip->emi_port = port + 1;

	if ((rc = ef10_external_port_mapping(enp, port,
		    &encp->enc_external_port)) != 0)
		goto fail2;

	/*
	 * Get PCIe function number from firmware (used for
	 * per-function privilege and dynamic config info).
	 *  - PCIe PF: pf = PF number, vf = 0xffff.
	 *  - PCIe VF: pf = parent PF, vf = VF number.
	 */
	if ((rc = efx_mcdi_get_function_info(enp, &pf, &vf)) != 0)
		goto fail3;

	encp->enc_pf = pf;
	encp->enc_vf = vf;

	/* MAC address for this function */
	if (EFX_PCI_FUNCTION_IS_PF(encp)) {
		rc = efx_mcdi_get_mac_address_pf(enp, mac_addr);
#if EFSYS_OPT_ALLOW_UNCONFIGURED_NIC
		/* Disable static config checking for Medford NICs, ONLY
		 * for manufacturing test and setup at the factory, to
		 * allow the static config to be installed.
		 */
#else /* EFSYS_OPT_ALLOW_UNCONFIGURED_NIC */
		if ((rc == 0) && (mac_addr[0] & 0x02)) {
			/*
			 * If the static config does not include a global MAC
			 * address pool then the board may return a locally
			 * administered MAC address (this should only happen on
			 * incorrectly programmed boards).
			 */
			rc = EINVAL;
		}
#endif /* EFSYS_OPT_ALLOW_UNCONFIGURED_NIC */
	} else {
		rc = efx_mcdi_get_mac_address_vf(enp, mac_addr);
	}
	if (rc != 0)
		goto fail4;

	EFX_MAC_ADDR_COPY(encp->enc_mac_addr, mac_addr);

	/* Board configuration */
	rc = efx_mcdi_get_board_cfg(enp, &board_type, NULL, NULL);
	if (rc != 0) {
		/* Unprivileged functions may not be able to read board cfg */
		if (rc == EACCES)
			board_type = 0;
		else
			goto fail5;
	}

	encp->enc_board_type = board_type;
	encp->enc_clk_mult = 1; /* not used for Medford */

	/* Fill out fields in enp->en_port and enp->en_nic_cfg from MCDI */
	if ((rc = efx_mcdi_get_phy_cfg(enp)) != 0)
		goto fail6;

	/* Obtain the default PHY advertised capabilities */
	if ((rc = ef10_phy_get_link(enp, &els)) != 0)
		goto fail7;
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
		 * Interrupt testing does not work for VFs. See bug50084.
		 * FIXME: Does this still  apply to Medford?
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
		goto fail8;

	/* Get clock frequencies (in MHz). */
	if ((rc = efx_mcdi_get_clock(enp, &sysclk, &dpcpu_clk)) != 0)
		goto fail9;

	/*
	 * The Medford timer quantum is 1536 dpcpu_clk cycles, documented for
	 * the EV_TMR_VAL field of EV_TIMER_TBL. Scale for MHz and ns units.
	 */
	encp->enc_evq_timer_quantum_ns = 1536000UL / dpcpu_clk; /* 1536 cycles */
	encp->enc_evq_timer_max_us = (encp->enc_evq_timer_quantum_ns <<
		    FRF_CZ_TC_TIMER_VAL_WIDTH) / 1000;

	/* Check capabilities of running datapath firmware */
	if ((rc = ef10_get_datapath_caps(enp)) != 0)
		goto fail10;

	/* Alignment for receive packet DMA buffers */
	encp->enc_rx_buf_align_start = 1;

	/* Get the RX DMA end padding alignment configuration */
	if ((rc = efx_mcdi_get_rxdp_config(enp, &end_padding)) != 0) {
		if (rc != EACCES)
			goto fail11;

		/* Assume largest tail padding size supported by hardware */
		end_padding = 256;
	}
	encp->enc_rx_buf_align_end = end_padding;

	/* Alignment for WPTR updates */
	encp->enc_rx_push_align = EF10_RX_WPTR_ALIGN;

	/*
	 * Set resource limits for MC_CMD_ALLOC_VIS. Note that we cannot use
	 * MC_CMD_GET_RESOURCE_LIMITS here as that reports the available
	 * resources (allocated to this PCIe function), which is zero until
	 * after we have allocated VIs.
	 */
	encp->enc_evq_limit = 1024;
	encp->enc_rxq_limit = EFX_RXQ_LIMIT_TARGET;
	encp->enc_txq_limit = EFX_TXQ_LIMIT_TARGET;

	encp->enc_buftbl_limit = 0xFFFFFFFF;

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
		goto fail12;
	encp->enc_privilege_mask = mask;

	/* Get interrupt vector limits */
	if ((rc = efx_mcdi_get_vector_cfg(enp, &base, &nvec, NULL)) != 0) {
		if (EFX_PCI_FUNCTION_IS_PF(encp))
			goto fail13;

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
		goto fail14;
	encp->enc_required_pcie_bandwidth_mbps = bandwidth;
	encp->enc_max_pcie_link_gen = EFX_PCIE_LINK_SPEED_GEN3;

	return (0);

fail14:
	EFSYS_PROBE(fail14);
fail13:
	EFSYS_PROBE(fail13);
fail12:
	EFSYS_PROBE(fail12);
fail11:
	EFSYS_PROBE(fail11);
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
