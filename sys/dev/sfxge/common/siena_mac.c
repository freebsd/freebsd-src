/*-
 * Copyright 2009 Solarflare Communications Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
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

#include "efsys.h"
#include "efx.h"
#include "efx_impl.h"

#if EFSYS_OPT_SIENA

	__checkReturn	int
siena_mac_poll(
	__in		efx_nic_t *enp,
	__out		efx_link_mode_t *link_modep)
{
	efx_port_t *epp = &(enp->en_port);
	siena_link_state_t sls;
	int rc;

	if ((rc = siena_phy_get_link(enp, &sls)) != 0)
		goto fail1;

	epp->ep_adv_cap_mask = sls.sls_adv_cap_mask;
	epp->ep_fcntl = sls.sls_fcntl;

	*link_modep = sls.sls_link_mode;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	*link_modep = EFX_LINK_UNKNOWN;

	return (rc);
}

	__checkReturn	int
siena_mac_up(
	__in		efx_nic_t *enp,
	__out		boolean_t *mac_upp)
{
	siena_link_state_t sls;
	int rc;

	/*
	 * Because Siena doesn't *require* polling, we can't rely on
	 * siena_mac_poll() being executed to populate epp->ep_mac_up.
	 */
	if ((rc = siena_phy_get_link(enp, &sls)) != 0)
		goto fail1;

	*mac_upp = sls.sls_mac_up;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
siena_mac_reconfigure(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	uint8_t payload[MAX(MC_CMD_SET_MAC_IN_LEN,
			    MC_CMD_SET_MCAST_HASH_IN_LEN)];
	efx_mcdi_req_t req;
	unsigned int fcntl;
	int rc;

	req.emr_cmd = MC_CMD_SET_MAC;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_SET_MAC_IN_LEN;
	EFX_STATIC_ASSERT(MC_CMD_SET_MAC_OUT_LEN == 0);
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_IN_SET_DWORD(req, SET_MAC_IN_MTU, epp->ep_mac_pdu);
	MCDI_IN_SET_DWORD(req, SET_MAC_IN_DRAIN, epp->ep_mac_drain ? 1 : 0);
	EFX_MAC_ADDR_COPY(MCDI_IN2(req, uint8_t, SET_MAC_IN_ADDR),
			    epp->ep_mac_addr);
	MCDI_IN_POPULATE_DWORD_2(req, SET_MAC_IN_REJECT,
				    SET_MAC_IN_REJECT_UNCST, !epp->ep_unicst,
				    SET_MAC_IN_REJECT_BRDCST, !epp->ep_brdcst);

	if (epp->ep_fcntl_autoneg)
		/* efx_fcntl_set() has already set the phy capabilities */
		fcntl = MC_CMD_FCNTL_AUTO;
	else if (epp->ep_fcntl & EFX_FCNTL_RESPOND)
		fcntl = (epp->ep_fcntl & EFX_FCNTL_GENERATE)
			? MC_CMD_FCNTL_BIDIR
			: MC_CMD_FCNTL_RESPOND;
	else
		fcntl = MC_CMD_FCNTL_OFF;

	MCDI_IN_SET_DWORD(req, SET_MAC_IN_FCNTL, fcntl);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	/* Push multicast hash. Set the broadcast bit (0xff) appropriately */
	req.emr_cmd = MC_CMD_SET_MCAST_HASH;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_SET_MCAST_HASH_IN_LEN;
	EFX_STATIC_ASSERT(MC_CMD_SET_MCAST_HASH_OUT_LEN == 0);
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	memcpy(MCDI_IN2(req, uint8_t, SET_MCAST_HASH_IN_HASH0),
	    epp->ep_multicst_hash, sizeof (epp->ep_multicst_hash));
	if (epp->ep_brdcst)
		EFX_SET_OWORD_BIT(*MCDI_IN2(req, efx_oword_t,
		    SET_MCAST_HASH_IN_HASH1), 0x7f);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#if EFSYS_OPT_LOOPBACK

	__checkReturn	int
siena_mac_loopback_set(
	__in		efx_nic_t *enp,
	__in		efx_link_mode_t link_mode,
	__in		efx_loopback_type_t loopback_type)
{
	efx_port_t *epp = &(enp->en_port);
	efx_phy_ops_t *epop = epp->ep_epop;
	efx_loopback_type_t old_loopback_type;
	efx_link_mode_t old_loopback_link_mode;
	int rc;

	/* The PHY object handles this on Siena */
	old_loopback_type = epp->ep_loopback_type;
	old_loopback_link_mode = epp->ep_loopback_link_mode;
	epp->ep_loopback_type = loopback_type;
	epp->ep_loopback_link_mode = link_mode;

	if ((rc = epop->epo_reconfigure(enp)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE(fail2);

	epp->ep_loopback_type = old_loopback_type;
	epp->ep_loopback_link_mode = old_loopback_link_mode;

	return (rc);
}

#endif	/* EFSYS_OPT_LOOPBACK */

#if EFSYS_OPT_MAC_STATS

	__checkReturn			int
siena_mac_stats_clear(
	__in				efx_nic_t *enp)
{
	uint8_t payload[MC_CMD_MAC_STATS_IN_LEN];
	efx_mcdi_req_t req;
	int rc;

	req.emr_cmd = MC_CMD_MAC_STATS;
	req.emr_in_buf = payload;
	req.emr_in_length = sizeof (payload);
	EFX_STATIC_ASSERT(MC_CMD_MAC_STATS_OUT_DMA_LEN == 0);
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_IN_POPULATE_DWORD_3(req, MAC_STATS_IN_CMD,
				    MAC_STATS_IN_DMA, 0,
				    MAC_STATS_IN_CLEAR, 1,
				    MAC_STATS_IN_PERIODIC_CHANGE, 0);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn			int
siena_mac_stats_upload(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp)
{
	uint8_t payload[MC_CMD_MAC_STATS_IN_LEN];
	efx_mcdi_req_t req;
	size_t bytes;
	int rc;

	EFX_STATIC_ASSERT(MC_CMD_MAC_NSTATS * sizeof (uint64_t) <=
	    EFX_MAC_STATS_SIZE);

	bytes = MC_CMD_MAC_NSTATS * sizeof (uint64_t);

	req.emr_cmd = MC_CMD_MAC_STATS;
	req.emr_in_buf = payload;
	req.emr_in_length = sizeof (payload);
	EFX_STATIC_ASSERT(MC_CMD_MAC_STATS_OUT_DMA_LEN == 0);
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_IN_SET_DWORD(req, MAC_STATS_IN_DMA_ADDR_LO,
			    EFSYS_MEM_ADDR(esmp) & 0xffffffff);
	MCDI_IN_SET_DWORD(req, MAC_STATS_IN_DMA_ADDR_HI,
			    EFSYS_MEM_ADDR(esmp) >> 32);
	MCDI_IN_SET_DWORD(req, MAC_STATS_IN_DMA_LEN, bytes);

	/*
	 * The MC DMAs aggregate statistics for our convinience, so we can
	 * avoid having to pull the statistics buffer into the cache to
	 * maintain cumulative statistics.
	 */
	MCDI_IN_POPULATE_DWORD_3(req, MAC_STATS_IN_CMD,
				    MAC_STATS_IN_DMA, 1,
				    MAC_STATS_IN_CLEAR, 0,
				    MAC_STATS_IN_PERIODIC_CHANGE, 0);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn			int
siena_mac_stats_periodic(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__in				uint16_t period,
	__in				boolean_t events)
{
	uint8_t payload[MC_CMD_MAC_STATS_IN_LEN];
	efx_mcdi_req_t req;
	size_t bytes;
	int rc;

	bytes = MC_CMD_MAC_NSTATS * sizeof (uint64_t);

	req.emr_cmd = MC_CMD_MAC_STATS;
	req.emr_in_buf = payload;
	req.emr_in_length = sizeof (payload);
	EFX_STATIC_ASSERT(MC_CMD_MAC_STATS_OUT_DMA_LEN == 0);
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_IN_SET_DWORD(req, MAC_STATS_IN_DMA_ADDR_LO,
			    EFSYS_MEM_ADDR(esmp) & 0xffffffff);
	MCDI_IN_SET_DWORD(req, MAC_STATS_IN_DMA_ADDR_HI,
			    EFSYS_MEM_ADDR(esmp) >> 32);
	MCDI_IN_SET_DWORD(req, MAC_STATS_IN_DMA_LEN, bytes);

	/*
	 * The MC DMAs aggregate statistics for our convinience, so we can
	 * avoid having to pull the statistics buffer into the cache to
	 * maintain cumulative statistics.
	 */
	MCDI_IN_POPULATE_DWORD_6(req, MAC_STATS_IN_CMD,
			    MAC_STATS_IN_DMA, 0,
			    MAC_STATS_IN_CLEAR, 0,
			    MAC_STATS_IN_PERIODIC_CHANGE, 1,
			    MAC_STATS_IN_PERIODIC_ENABLE, period ? 1 : 0,
			    MAC_STATS_IN_PERIODIC_NOEVENT, events ? 0 : 1,
			    MAC_STATS_IN_PERIOD_MS, period);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}


#define	SIENA_MAC_STAT_READ(_esmp, _field, _eqp)			\
	EFSYS_MEM_READQ((_esmp), (_field) * sizeof (efx_qword_t), _eqp)

	__checkReturn			int
siena_mac_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__out_ecount(EFX_MAC_NSTATS)	efsys_stat_t *stat,
	__out_opt			uint32_t *generationp)
{
	efx_qword_t rx_pkts;
	efx_qword_t value;
	efx_qword_t generation_start;
	efx_qword_t generation_end;

	_NOTE(ARGUNUSED(enp))

	/* Read END first so we don't race with the MC */
	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_GENERATION_END,
			    &generation_end);
	EFSYS_MEM_READ_BARRIER();

	/* TX */
	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_TX_PKTS]), &value);
	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_CONTROL_PKTS, &value);
	EFSYS_STAT_SUBR_QWORD(&(stat[EFX_MAC_TX_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_PAUSE_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_TX_PAUSE_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_UNICAST_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_TX_UNICST_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_MULTICAST_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_TX_MULTICST_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_BROADCAST_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_TX_BRDCST_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_BYTES, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_TX_OCTETS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_LT64_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_TX_LE_64_PKTS]), &value);
	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_64_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_TX_LE_64_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_65_TO_127_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_TX_65_TO_127_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_128_TO_255_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_TX_128_TO_255_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_256_TO_511_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_TX_256_TO_511_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_512_TO_1023_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_TX_512_TO_1023_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_1024_TO_15XX_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_TX_1024_TO_15XX_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_15XX_TO_JUMBO_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_TX_GE_15XX_PKTS]), &value);
	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_GTJUMBO_PKTS, &value);
	EFSYS_STAT_INCR_QWORD(&(stat[EFX_MAC_TX_GE_15XX_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_BAD_FCS_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_TX_ERRORS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_SINGLE_COLLISION_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_TX_SGL_COL_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_MULTIPLE_COLLISION_PKTS,
			    &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_TX_MULT_COL_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_EXCESSIVE_COLLISION_PKTS,
			    &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_TX_EX_COL_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_LATE_COLLISION_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_TX_LATE_COL_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_DEFERRED_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_TX_DEF_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_TX_EXCESSIVE_DEFERRED_PKTS,
	    &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_TX_EX_DEF_PKTS]), &value);

	/* RX */
	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_BYTES, &rx_pkts);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_OCTETS]), &rx_pkts);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_UNICAST_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_UNICST_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_MULTICAST_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_MULTICST_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_BROADCAST_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_BRDCST_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_PAUSE_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_PAUSE_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_UNDERSIZE_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_LE_64_PKTS]), &value);
	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_64_PKTS, &value);
	EFSYS_STAT_INCR_QWORD(&(stat[EFX_MAC_RX_LE_64_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_65_TO_127_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_65_TO_127_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_128_TO_255_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_128_TO_255_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_256_TO_511_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_256_TO_511_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_512_TO_1023_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_512_TO_1023_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_1024_TO_15XX_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_1024_TO_15XX_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_15XX_TO_JUMBO_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_GE_15XX_PKTS]), &value);
	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_GTJUMBO_PKTS, &value);
	EFSYS_STAT_INCR_QWORD(&(stat[EFX_MAC_RX_GE_15XX_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_BAD_FCS_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_FCS_ERRORS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_OVERFLOW_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_DROP_EVENTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_FALSE_CARRIER_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_FALSE_CARRIER_ERRORS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_SYMBOL_ERROR_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_SYMBOL_ERRORS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_ALIGN_ERROR_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_ALIGN_ERRORS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_INTERNAL_ERROR_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_INTERNAL_ERRORS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_JABBER_PKTS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_JABBER_PKTS]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_LANES01_CHAR_ERR, &value);
	EFSYS_STAT_SET_DWORD(&(stat[EFX_MAC_RX_LANE0_CHAR_ERR]),
			    &(value.eq_dword[0]));
	EFSYS_STAT_SET_DWORD(&(stat[EFX_MAC_RX_LANE1_CHAR_ERR]),
			    &(value.eq_dword[1]));

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_LANES23_CHAR_ERR, &value);
	EFSYS_STAT_SET_DWORD(&(stat[EFX_MAC_RX_LANE2_CHAR_ERR]),
			    &(value.eq_dword[0]));
	EFSYS_STAT_SET_DWORD(&(stat[EFX_MAC_RX_LANE3_CHAR_ERR]),
			    &(value.eq_dword[1]));

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_LANES01_DISP_ERR, &value);
	EFSYS_STAT_SET_DWORD(&(stat[EFX_MAC_RX_LANE0_DISP_ERR]),
			    &(value.eq_dword[0]));
	EFSYS_STAT_SET_DWORD(&(stat[EFX_MAC_RX_LANE1_DISP_ERR]),
			    &(value.eq_dword[1]));

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_LANES23_DISP_ERR, &value);
	EFSYS_STAT_SET_DWORD(&(stat[EFX_MAC_RX_LANE2_DISP_ERR]),
			    &(value.eq_dword[0]));
	EFSYS_STAT_SET_DWORD(&(stat[EFX_MAC_RX_LANE3_DISP_ERR]),
			    &(value.eq_dword[1]));

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_MATCH_FAULT, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_MATCH_FAULT]), &value);

	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_RX_NODESC_DROPS, &value);
	EFSYS_STAT_SET_QWORD(&(stat[EFX_MAC_RX_NODESC_DROP_CNT]), &value);

	EFSYS_MEM_READ_BARRIER();
	SIENA_MAC_STAT_READ(esmp, MC_CMD_MAC_GENERATION_START,
			    &generation_start);

	/* Check that we didn't read the stats in the middle of a DMA */
	if (memcmp(&generation_start, &generation_end,
	    sizeof (generation_start)))
		return (EAGAIN);

	if (generationp)
		*generationp = EFX_QWORD_FIELD(generation_start, EFX_DWORD_0);

	return (0);
}

#endif	/* EFSYS_OPT_MAC_STATS */

#endif	/* EFSYS_OPT_SIENA */
