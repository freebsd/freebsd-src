/*-
 * Copyright (c) 2012-2015 Solarflare Communications Inc.
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

#include "efsys.h"
#include "efx.h"
#include "efx_types.h"
#include "efx_regs.h"
#include "efx_impl.h"
#if EFSYS_OPT_MON_STATS
#include "mcdi_mon.h"
#endif

#if EFSYS_OPT_HUNTINGTON

#if EFSYS_OPT_QSTATS
#define	EFX_EV_QSTAT_INCR(_eep, _stat)					\
	do {								\
		(_eep)->ee_stat[_stat]++;				\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)
#else
#define	EFX_EV_QSTAT_INCR(_eep, _stat)
#endif


static	__checkReturn	boolean_t
hunt_ev_rx(
	__in		efx_evq_t *eep,
	__in		efx_qword_t *eqp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg);

static	__checkReturn	boolean_t
hunt_ev_tx(
	__in		efx_evq_t *eep,
	__in		efx_qword_t *eqp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg);

static	__checkReturn	boolean_t
hunt_ev_driver(
	__in		efx_evq_t *eep,
	__in		efx_qword_t *eqp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg);

static	__checkReturn	boolean_t
hunt_ev_drv_gen(
	__in		efx_evq_t *eep,
	__in		efx_qword_t *eqp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg);

static	__checkReturn	boolean_t
hunt_ev_mcdi(
	__in		efx_evq_t *eep,
	__in		efx_qword_t *eqp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg);


static	__checkReturn	efx_rc_t
efx_mcdi_init_evq(
	__in		efx_nic_t *enp,
	__in		unsigned int instance,
	__in		efsys_mem_t *esmp,
	__in		size_t nevs,
	__in		uint32_t irq,
	__out_opt	uint32_t *irqp)
{
	efx_mcdi_req_t req;
	uint8_t payload[
	    MAX(MC_CMD_INIT_EVQ_IN_LEN(EFX_EVQ_NBUFS(EFX_EVQ_MAXNEVS)),
		MC_CMD_INIT_EVQ_OUT_LEN)];
	efx_qword_t *dma_addr;
	uint64_t addr;
	int npages;
	int i;
	int supports_rx_batching;
	efx_rc_t rc;

	npages = EFX_EVQ_NBUFS(nevs);
	if (MC_CMD_INIT_EVQ_IN_LEN(npages) > MC_CMD_INIT_EVQ_IN_LENMAX) {
		rc = EINVAL;
		goto fail1;
	}

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_INIT_EVQ;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_INIT_EVQ_IN_LEN(npages);
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_INIT_EVQ_OUT_LEN;

	MCDI_IN_SET_DWORD(req, INIT_EVQ_IN_SIZE, nevs);
	MCDI_IN_SET_DWORD(req, INIT_EVQ_IN_INSTANCE, instance);
	MCDI_IN_SET_DWORD(req, INIT_EVQ_IN_IRQ_NUM, irq);

	/*
	 * On Huntington RX and TX event batching can only be requested
	 * together (even if the datapath firmware doesn't actually support RX
	 * batching).
	 * Cut through is incompatible with RX batching and so enabling cut
	 * through disables RX batching (but it does not affect TX batching).
	 *
	 * So always enable RX and TX event batching, and enable cut through
	 * if RX event batching isn't supported (i.e. on low latency firmware).
	 */
	supports_rx_batching = enp->en_nic_cfg.enc_rx_batching_enabled ? 1 : 0;
	MCDI_IN_POPULATE_DWORD_6(req, INIT_EVQ_IN_FLAGS,
	    INIT_EVQ_IN_FLAG_INTERRUPTING, 1,
	    INIT_EVQ_IN_FLAG_RPTR_DOS, 0,
	    INIT_EVQ_IN_FLAG_INT_ARMD, 0,
	    INIT_EVQ_IN_FLAG_CUT_THRU, !supports_rx_batching,
	    INIT_EVQ_IN_FLAG_RX_MERGE, 1,
	    INIT_EVQ_IN_FLAG_TX_MERGE, 1);

	MCDI_IN_SET_DWORD(req, INIT_EVQ_IN_TMR_MODE,
	    MC_CMD_INIT_EVQ_IN_TMR_MODE_DIS);
	MCDI_IN_SET_DWORD(req, INIT_EVQ_IN_TMR_LOAD, 0);
	MCDI_IN_SET_DWORD(req, INIT_EVQ_IN_TMR_RELOAD, 0);

	MCDI_IN_SET_DWORD(req, INIT_EVQ_IN_COUNT_MODE,
	    MC_CMD_INIT_EVQ_IN_COUNT_MODE_DIS);
	MCDI_IN_SET_DWORD(req, INIT_EVQ_IN_COUNT_THRSHLD, 0);

	dma_addr = MCDI_IN2(req, efx_qword_t, INIT_EVQ_IN_DMA_ADDR);
	addr = EFSYS_MEM_ADDR(esmp);

	for (i = 0; i < npages; i++) {
		EFX_POPULATE_QWORD_2(*dma_addr,
		    EFX_DWORD_1, (uint32_t)(addr >> 32),
		    EFX_DWORD_0, (uint32_t)(addr & 0xffffffff));

		dma_addr++;
		addr += EFX_BUF_SIZE;
	}

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	if (req.emr_out_length_used < MC_CMD_INIT_EVQ_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail3;
	}

	if (irqp != NULL)
		*irqp = MCDI_OUT_DWORD(req, INIT_EVQ_OUT_IRQ);

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn	efx_rc_t
efx_mcdi_fini_evq(
	__in		efx_nic_t *enp,
	__in		uint32_t instance)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_FINI_EVQ_IN_LEN,
			    MC_CMD_FINI_EVQ_OUT_LEN)];
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_FINI_EVQ;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_FINI_EVQ_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_FINI_EVQ_OUT_LEN;

	MCDI_IN_SET_DWORD(req, FINI_EVQ_IN_INSTANCE, instance);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}



	__checkReturn	efx_rc_t
hunt_ev_init(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
	return (0);
}

			void
hunt_ev_fini(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
}

	__checkReturn	efx_rc_t
hunt_ev_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		efsys_mem_t *esmp,
	__in		size_t n,
	__in		uint32_t id,
	__in		efx_evq_t *eep)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint32_t irq;
	efx_rc_t rc;

	_NOTE(ARGUNUSED(id))	/* buftbl id managed by MC */
	EFX_STATIC_ASSERT(ISP2(EFX_EVQ_MAXNEVS));
	EFX_STATIC_ASSERT(ISP2(EFX_EVQ_MINNEVS));

	if (!ISP2(n) || (n < EFX_EVQ_MINNEVS) || (n > EFX_EVQ_MAXNEVS)) {
		rc = EINVAL;
		goto fail1;
	}

	if (index >= encp->enc_evq_limit) {
		rc = EINVAL;
		goto fail2;
	}

	/* Set up the handler table */
	eep->ee_rx	= hunt_ev_rx;
	eep->ee_tx	= hunt_ev_tx;
	eep->ee_driver	= hunt_ev_driver;
	eep->ee_drv_gen	= hunt_ev_drv_gen;
	eep->ee_mcdi	= hunt_ev_mcdi;

	/*
	 * Set up the event queue
	 * NOTE: ignore the returned IRQ param as firmware does not set it.
	 */
	irq = index;	/* INIT_EVQ expects function-relative vector number */
	if ((rc = efx_mcdi_init_evq(enp, index, esmp, n, irq, NULL)) != 0)
		goto fail3;

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
hunt_ev_qdestroy(
	__in		efx_evq_t *eep)
{
	efx_nic_t *enp = eep->ee_enp;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

	(void) efx_mcdi_fini_evq(eep->ee_enp, eep->ee_index);
}

	__checkReturn	efx_rc_t
hunt_ev_qprime(
	__in		efx_evq_t *eep,
	__in		unsigned int count)
{
	efx_nic_t *enp = eep->ee_enp;
	uint32_t rptr;
	efx_dword_t dword;

	rptr = count & eep->ee_mask;

	if (enp->en_nic_cfg.enc_bug35388_workaround) {
		EFX_STATIC_ASSERT(EFX_EVQ_MINNEVS >
		    (1 << ERF_DD_EVQ_IND_RPTR_WIDTH));
		EFX_STATIC_ASSERT(EFX_EVQ_MAXNEVS <
		    (1 << 2 * ERF_DD_EVQ_IND_RPTR_WIDTH));

		EFX_POPULATE_DWORD_2(dword,
		    ERF_DD_EVQ_IND_RPTR_FLAGS,
		    EFE_DD_EVQ_IND_RPTR_FLAGS_HIGH,
		    ERF_DD_EVQ_IND_RPTR,
		    (rptr >> ERF_DD_EVQ_IND_RPTR_WIDTH));
		EFX_BAR_TBL_WRITED(enp, ER_DD_EVQ_INDIRECT, eep->ee_index,
		    &dword, B_FALSE);

		EFX_POPULATE_DWORD_2(dword,
		    ERF_DD_EVQ_IND_RPTR_FLAGS,
		    EFE_DD_EVQ_IND_RPTR_FLAGS_LOW,
		    ERF_DD_EVQ_IND_RPTR,
		    rptr & ((1 << ERF_DD_EVQ_IND_RPTR_WIDTH) - 1));
		EFX_BAR_TBL_WRITED(enp, ER_DD_EVQ_INDIRECT, eep->ee_index,
		    &dword, B_FALSE);
	} else {
		EFX_POPULATE_DWORD_1(dword, ERF_DZ_EVQ_RPTR, rptr);
		EFX_BAR_TBL_WRITED(enp, ER_DZ_EVQ_RPTR_REG, eep->ee_index,
		    &dword, B_FALSE);
	}

	return (0);
}

static	__checkReturn	efx_rc_t
efx_mcdi_driver_event(
	__in		efx_nic_t *enp,
	__in		uint32_t evq,
	__in		efx_qword_t data)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_DRIVER_EVENT_IN_LEN,
			    MC_CMD_DRIVER_EVENT_OUT_LEN)];
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_DRIVER_EVENT;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_DRIVER_EVENT_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_DRIVER_EVENT_OUT_LEN;

	MCDI_IN_SET_DWORD(req, DRIVER_EVENT_IN_EVQ, evq);

	MCDI_IN_SET_DWORD(req, DRIVER_EVENT_IN_DATA_LO,
	    EFX_QWORD_FIELD(data, EFX_DWORD_0));
	MCDI_IN_SET_DWORD(req, DRIVER_EVENT_IN_DATA_HI,
	    EFX_QWORD_FIELD(data, EFX_DWORD_1));

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
hunt_ev_qpost(
	__in	efx_evq_t *eep,
	__in	uint16_t data)
{
	efx_nic_t *enp = eep->ee_enp;
	efx_qword_t event;

	EFX_POPULATE_QWORD_3(event,
	    ESF_DZ_DRV_CODE, ESE_DZ_EV_CODE_DRV_GEN_EV,
	    ESF_DZ_DRV_SUB_CODE, 0,
	    ESF_DZ_DRV_SUB_DATA_DW0, (uint32_t)data);

	(void) efx_mcdi_driver_event(enp, eep->ee_index, event);
}

	__checkReturn	efx_rc_t
hunt_ev_qmoderate(
	__in		efx_evq_t *eep,
	__in		unsigned int us)
{
	efx_nic_t *enp = eep->ee_enp;
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_dword_t dword;
	uint32_t timer_val, mode;
	efx_rc_t rc;

	if (us > encp->enc_evq_timer_max_us) {
		rc = EINVAL;
		goto fail1;
	}

	/* If the value is zero then disable the timer */
	if (us == 0) {
		timer_val = 0;
		mode = FFE_CZ_TIMER_MODE_DIS;
	} else {
		/* Calculate the timer value in quanta */
		timer_val = us * 1000 / encp->enc_evq_timer_quantum_ns;

		/* Moderation value is base 0 so we need to deduct 1 */
		if (timer_val > 0)
			timer_val--;

		mode = FFE_CZ_TIMER_MODE_INT_HLDOFF;
	}

	if (encp->enc_bug35388_workaround) {
		EFX_POPULATE_DWORD_3(dword,
		    ERF_DD_EVQ_IND_TIMER_FLAGS,
		    EFE_DD_EVQ_IND_TIMER_FLAGS,
		    ERF_DD_EVQ_IND_TIMER_MODE, mode,
		    ERF_DD_EVQ_IND_TIMER_VAL, timer_val);
		EFX_BAR_TBL_WRITED(enp, ER_DD_EVQ_INDIRECT,
		    eep->ee_index, &dword, 0);
	} else {
		EFX_POPULATE_DWORD_2(dword,
		    FRF_CZ_TC_TIMER_MODE, mode,
		    FRF_CZ_TC_TIMER_VAL, timer_val);
		EFX_BAR_TBL_WRITED(enp, FR_BZ_TIMER_COMMAND_REGP0,
		    eep->ee_index, &dword, 0);
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


#if EFSYS_OPT_QSTATS
			void
hunt_ev_qstats_update(
	__in				efx_evq_t *eep,
	__inout_ecount(EV_NQSTATS)	efsys_stat_t *stat)
{
	/*
	 * TBD: Consider a common Siena/Huntington function.  The code is
	 * essentially identical.
	 */
	unsigned int id;

	for (id = 0; id < EV_NQSTATS; id++) {
		efsys_stat_t *essp = &stat[id];

		EFSYS_STAT_INCR(essp, eep->ee_stat[id]);
		eep->ee_stat[id] = 0;
	}
}
#endif /* EFSYS_OPT_QSTATS */


static	__checkReturn	boolean_t
hunt_ev_rx(
	__in		efx_evq_t *eep,
	__in		efx_qword_t *eqp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg)
{
	efx_nic_t *enp = eep->ee_enp;
	uint32_t size;
	boolean_t parse_err;
	uint32_t label;
	uint32_t mcast;
	uint32_t eth_base_class;
	uint32_t eth_tag_class;
	uint32_t l3_class;
	uint32_t l4_class;
	uint32_t next_read_lbits;
	boolean_t soft1, soft2;
	uint16_t flags;
	boolean_t should_abort;
	efx_evq_rxq_state_t *eersp;
	unsigned int desc_count;
	unsigned int last_used_id;

	EFX_EV_QSTAT_INCR(eep, EV_RX);

	/* Discard events after RXQ/TXQ errors */
	if (enp->en_reset_flags & (EFX_RESET_RXQ_ERR | EFX_RESET_TXQ_ERR))
		return (B_FALSE);

	/*
	 * FIXME: likely to be incomplete, incorrect and inefficient.
	 * Improvements in all three areas are required.
	 */

	if (EFX_QWORD_FIELD(*eqp, ESF_DZ_RX_DROP_EVENT) != 0) {
		/* Drop this event */
		return (B_FALSE);
	}
	flags = 0;

	size = EFX_QWORD_FIELD(*eqp, ESF_DZ_RX_BYTES);

	if (EFX_QWORD_FIELD(*eqp, ESF_DZ_RX_CONT) != 0) {
		/*
		 * FIXME: There is not yet any driver that supports scatter on
		 * Huntington.  Scatter support is required for OSX.
		 */
		EFSYS_ASSERT(0);
		flags |= EFX_PKT_CONT;
	}

	parse_err = (EFX_QWORD_FIELD(*eqp, ESF_DZ_RX_PARSE_INCOMPLETE) != 0);
	label = EFX_QWORD_FIELD(*eqp, ESF_DZ_RX_QLABEL);

	if (EFX_QWORD_FIELD(*eqp, ESF_DZ_RX_ECRC_ERR) != 0) {
		/* Ethernet frame CRC bad */
		flags |= EFX_DISCARD;
	}
	if (EFX_QWORD_FIELD(*eqp, ESF_DZ_RX_CRC0_ERR) != 0) {
		/* IP+TCP, bad CRC in iSCSI header */
		flags |= EFX_DISCARD;
	}
	if (EFX_QWORD_FIELD(*eqp, ESF_DZ_RX_CRC1_ERR) != 0) {
		/* IP+TCP, bad CRC in iSCSI payload or FCoE or FCoIP */
		flags |= EFX_DISCARD;
	}
	if (EFX_QWORD_FIELD(*eqp, ESF_DZ_RX_ECC_ERR) != 0) {
		/* ECC memory error */
		flags |= EFX_DISCARD;
	}

	/* FIXME: do we need soft bits from RXDP firmware ? */
	soft1 = (EFX_QWORD_FIELD(*eqp, ESF_DZ_RX_EV_SOFT1) != 0);
	soft2 = (EFX_QWORD_FIELD(*eqp, ESF_DZ_RX_EV_SOFT2) != 0);

	mcast = EFX_QWORD_FIELD(*eqp, ESF_DZ_RX_MAC_CLASS);
	if (mcast == ESE_DZ_MAC_CLASS_UCAST)
		flags |= EFX_PKT_UNICAST;

	eth_base_class = EFX_QWORD_FIELD(*eqp, ESF_DZ_RX_ETH_BASE_CLASS);
	eth_tag_class = EFX_QWORD_FIELD(*eqp, ESF_DZ_RX_ETH_TAG_CLASS);
	l3_class = EFX_QWORD_FIELD(*eqp, ESF_DZ_RX_L3_CLASS);
	l4_class = EFX_QWORD_FIELD(*eqp, ESF_DZ_RX_L4_CLASS);

	/* bottom 4 bits of incremented index (not last desc consumed) */
	next_read_lbits = EFX_QWORD_FIELD(*eqp, ESF_DZ_RX_DSC_PTR_LBITS);

	/* Increment the count of descriptors read */
	eersp = &eep->ee_rxq_state[label];
	desc_count = (next_read_lbits - eersp->eers_rx_read_ptr) &
	    EFX_MASK32(ESF_DZ_RX_DSC_PTR_LBITS);
	eersp->eers_rx_read_ptr += desc_count;

	/*
	 * FIXME: add error checking to make sure this a batched event.
	 * This could also be an aborted scatter, see Bug36629.
	 */
	if (desc_count > 1) {
		EFX_EV_QSTAT_INCR(eep, EV_RX_BATCH);
		flags |= EFX_PKT_PREFIX_LEN;
	}

	/* Calculate the index of the the last descriptor consumed */
	last_used_id = (eersp->eers_rx_read_ptr - 1) & eersp->eers_rx_mask;

	/* EFX_QWORD_FIELD(*eqp, ESF_DZ_RX_OVERRIDE_HOLDOFF); */

	switch (eth_base_class) {
	case ESE_DZ_ETH_BASE_CLASS_LLC_SNAP:
	case ESE_DZ_ETH_BASE_CLASS_LLC:
	case ESE_DZ_ETH_BASE_CLASS_ETH2:
	default:
		break;
	}

	switch (eth_tag_class) {
	case ESE_DZ_ETH_TAG_CLASS_RSVD7:
	case ESE_DZ_ETH_TAG_CLASS_RSVD6:
	case ESE_DZ_ETH_TAG_CLASS_RSVD5:
	case ESE_DZ_ETH_TAG_CLASS_RSVD4:
		break;

	case ESE_DZ_ETH_TAG_CLASS_RSVD3: /* Triple tagged */
	case ESE_DZ_ETH_TAG_CLASS_VLAN2: /* Double tagged */
	case ESE_DZ_ETH_TAG_CLASS_VLAN1: /* VLAN tagged */
		flags |= EFX_PKT_VLAN_TAGGED;
		break;

	case ESE_DZ_ETH_TAG_CLASS_NONE:
	default:
		break;
	}

	switch (l3_class) {
	case ESE_DZ_L3_CLASS_RSVD7: /* Used by firmware for packet overrun */
		parse_err = B_TRUE;
		flags |= EFX_DISCARD;
		break;

	case ESE_DZ_L3_CLASS_ARP:
	case ESE_DZ_L3_CLASS_FCOE:
		break;

	case ESE_DZ_L3_CLASS_IP6_FRAG:
	case ESE_DZ_L3_CLASS_IP6:
		flags |= EFX_PKT_IPV6;
		break;

	case ESE_DZ_L3_CLASS_IP4_FRAG:
	case ESE_DZ_L3_CLASS_IP4:
		flags |= EFX_PKT_IPV4;
		if (EFX_QWORD_FIELD(*eqp, ESF_DZ_RX_IPCKSUM_ERR) == 0)
			flags |= EFX_CKSUM_IPV4;
		break;

	case ESE_DZ_L3_CLASS_UNKNOWN:
	default:
		break;
	}

	switch (l4_class) {
	case ESE_DZ_L4_CLASS_RSVD7:
	case ESE_DZ_L4_CLASS_RSVD6:
	case ESE_DZ_L4_CLASS_RSVD5:
	case ESE_DZ_L4_CLASS_RSVD4:
	case ESE_DZ_L4_CLASS_RSVD3:
		break;

	case ESE_DZ_L4_CLASS_UDP:
		flags |= EFX_PKT_UDP;
		if (EFX_QWORD_FIELD(*eqp, ESF_DZ_RX_TCPUDP_CKSUM_ERR) == 0)
			flags |= EFX_CKSUM_TCPUDP;
		break;

	case ESE_DZ_L4_CLASS_TCP:
		flags |= EFX_PKT_TCP;
		if (EFX_QWORD_FIELD(*eqp, ESF_DZ_RX_TCPUDP_CKSUM_ERR) == 0)
			flags |= EFX_CKSUM_TCPUDP;
		break;

	case ESE_DZ_L4_CLASS_UNKNOWN:
	default:
		break;
	}

	/* If we're not discarding the packet then it is ok */
	if (~flags & EFX_DISCARD)
		EFX_EV_QSTAT_INCR(eep, EV_RX_OK);

	EFSYS_ASSERT(eecp->eec_rx != NULL);
	should_abort = eecp->eec_rx(arg, label, last_used_id, size, flags);

	return (should_abort);
}

static	__checkReturn	boolean_t
hunt_ev_tx(
	__in		efx_evq_t *eep,
	__in		efx_qword_t *eqp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg)
{
	efx_nic_t *enp = eep->ee_enp;
	uint32_t id;
	uint32_t label;
	boolean_t should_abort;

	EFX_EV_QSTAT_INCR(eep, EV_TX);

	/* Discard events after RXQ/TXQ errors */
	if (enp->en_reset_flags & (EFX_RESET_RXQ_ERR | EFX_RESET_TXQ_ERR))
		return (B_FALSE);

	if (EFX_QWORD_FIELD(*eqp, ESF_DZ_TX_DROP_EVENT) != 0) {
		/* Drop this event */
		return (B_FALSE);
	}

	/* Per-packet TX completion (was per-descriptor for Falcon/Siena) */
	id = EFX_QWORD_FIELD(*eqp, ESF_DZ_TX_DESCR_INDX);
	label = EFX_QWORD_FIELD(*eqp, ESF_DZ_TX_QLABEL);

	EFSYS_PROBE2(tx_complete, uint32_t, label, uint32_t, id);

	EFSYS_ASSERT(eecp->eec_tx != NULL);
	should_abort = eecp->eec_tx(arg, label, id);

	return (should_abort);
}

static	__checkReturn	boolean_t
hunt_ev_driver(
	__in		efx_evq_t *eep,
	__in		efx_qword_t *eqp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg)
{
	unsigned int code;
	boolean_t should_abort;

	EFX_EV_QSTAT_INCR(eep, EV_DRIVER);
	should_abort = B_FALSE;

	code = EFX_QWORD_FIELD(*eqp, ESF_DZ_DRV_SUB_CODE);
	switch (code) {
	case ESE_DZ_DRV_TIMER_EV: {
		uint32_t id;

		id = EFX_QWORD_FIELD(*eqp, ESF_DZ_DRV_TMR_ID);

		EFSYS_ASSERT(eecp->eec_timer != NULL);
		should_abort = eecp->eec_timer(arg, id);
		break;
	}

	case ESE_DZ_DRV_WAKE_UP_EV: {
		uint32_t id;

		id = EFX_QWORD_FIELD(*eqp, ESF_DZ_DRV_EVQ_ID);

		EFSYS_ASSERT(eecp->eec_wake_up != NULL);
		should_abort = eecp->eec_wake_up(arg, id);
		break;
	}

	case ESE_DZ_DRV_START_UP_EV:
		EFSYS_ASSERT(eecp->eec_initialized != NULL);
		should_abort = eecp->eec_initialized(arg);
		break;

	default:
		EFSYS_PROBE3(bad_event, unsigned int, eep->ee_index,
		    uint32_t, EFX_QWORD_FIELD(*eqp, EFX_DWORD_1),
		    uint32_t, EFX_QWORD_FIELD(*eqp, EFX_DWORD_0));
		break;
	}

	return (should_abort);
}

static	__checkReturn	boolean_t
hunt_ev_drv_gen(
	__in		efx_evq_t *eep,
	__in		efx_qword_t *eqp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg)
{
	uint32_t data;
	boolean_t should_abort;

	EFX_EV_QSTAT_INCR(eep, EV_DRV_GEN);
	should_abort = B_FALSE;

	data = EFX_QWORD_FIELD(*eqp, ESF_DZ_DRV_SUB_DATA_DW0);
	if (data >= ((uint32_t)1 << 16)) {
		EFSYS_PROBE3(bad_event, unsigned int, eep->ee_index,
		    uint32_t, EFX_QWORD_FIELD(*eqp, EFX_DWORD_1),
		    uint32_t, EFX_QWORD_FIELD(*eqp, EFX_DWORD_0));

		return (B_TRUE);
	}

	EFSYS_ASSERT(eecp->eec_software != NULL);
	should_abort = eecp->eec_software(arg, (uint16_t)data);

	return (should_abort);
}

static	__checkReturn	boolean_t
hunt_ev_mcdi(
	__in		efx_evq_t *eep,
	__in		efx_qword_t *eqp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg)
{
	efx_nic_t *enp = eep->ee_enp;
	unsigned code;
	boolean_t should_abort = B_FALSE;

	EFX_EV_QSTAT_INCR(eep, EV_MCDI_RESPONSE);

	code = EFX_QWORD_FIELD(*eqp, MCDI_EVENT_CODE);
	switch (code) {
	case MCDI_EVENT_CODE_BADSSERT:
		efx_mcdi_ev_death(enp, EINTR);
		break;

	case MCDI_EVENT_CODE_CMDDONE:
		efx_mcdi_ev_cpl(enp,
		    MCDI_EV_FIELD(eqp, CMDDONE_SEQ),
		    MCDI_EV_FIELD(eqp, CMDDONE_DATALEN),
		    MCDI_EV_FIELD(eqp, CMDDONE_ERRNO));
		break;

	case MCDI_EVENT_CODE_LINKCHANGE: {
		efx_link_mode_t link_mode;

		hunt_phy_link_ev(enp, eqp, &link_mode);
		should_abort = eecp->eec_link_change(arg, link_mode);
		break;
	}

	case MCDI_EVENT_CODE_SENSOREVT: {
#if EFSYS_OPT_MON_STATS
		efx_mon_stat_t id;
		efx_mon_stat_value_t value;
		efx_rc_t rc;

		/* Decode monitor stat for MCDI sensor (if supported) */
		if ((rc = mcdi_mon_ev(enp, eqp, &id, &value)) == 0) {
			/* Report monitor stat change */
			should_abort = eecp->eec_monitor(arg, id, value);
		} else if (rc == ENOTSUP) {
			should_abort = eecp->eec_exception(arg,
				EFX_EXCEPTION_UNKNOWN_SENSOREVT,
				MCDI_EV_FIELD(eqp, DATA));
		} else {
			EFSYS_ASSERT(rc == ENODEV);	/* Wrong port */
		}
#endif
		break;
	}

	case MCDI_EVENT_CODE_SCHEDERR:
		/* Informational only */
		break;

	case MCDI_EVENT_CODE_REBOOT:
		/* Falcon/Siena only (should not been seen with Huntington). */
		efx_mcdi_ev_death(enp, EIO);
		break;

	case MCDI_EVENT_CODE_MC_REBOOT:
		/* MC_REBOOT event is used for Huntington (EF10) and later. */
		efx_mcdi_ev_death(enp, EIO);
		break;

	case MCDI_EVENT_CODE_MAC_STATS_DMA:
#if EFSYS_OPT_MAC_STATS
		if (eecp->eec_mac_stats != NULL) {
			eecp->eec_mac_stats(arg,
			    MCDI_EV_FIELD(eqp, MAC_STATS_DMA_GENERATION));
		}
#endif
		break;

	case MCDI_EVENT_CODE_FWALERT: {
		uint32_t reason = MCDI_EV_FIELD(eqp, FWALERT_REASON);

		if (reason == MCDI_EVENT_FWALERT_REASON_SRAM_ACCESS)
			should_abort = eecp->eec_exception(arg,
				EFX_EXCEPTION_FWALERT_SRAM,
				MCDI_EV_FIELD(eqp, FWALERT_DATA));
		else
			should_abort = eecp->eec_exception(arg,
				EFX_EXCEPTION_UNKNOWN_FWALERT,
				MCDI_EV_FIELD(eqp, DATA));
		break;
	}

	case MCDI_EVENT_CODE_TX_ERR: {
		/*
		 * After a TXQ error is detected, firmware sends a TX_ERR event.
		 * This may be followed by TX completions (which we discard),
		 * and then finally by a TX_FLUSH event. Firmware destroys the
		 * TXQ automatically after sending the TX_FLUSH event.
		 */
		enp->en_reset_flags |= EFX_RESET_TXQ_ERR;

		EFSYS_PROBE1(tx_descq_err, uint32_t, MCDI_EV_FIELD(eqp, DATA));

		/* Inform the driver that a reset is required. */
		eecp->eec_exception(arg, EFX_EXCEPTION_TX_ERROR,
		    MCDI_EV_FIELD(eqp, TX_ERR_DATA));
		break;
	}

	case MCDI_EVENT_CODE_TX_FLUSH: {
		uint32_t txq_index = MCDI_EV_FIELD(eqp, TX_FLUSH_TXQ);

		/*
		 * EF10 firmware sends two TX_FLUSH events: one to the txq's
		 * event queue, and one to evq 0 (with TX_FLUSH_TO_DRIVER set).
		 * We want to wait for all completions, so ignore the events
		 * with TX_FLUSH_TO_DRIVER.
		 */
		if (MCDI_EV_FIELD(eqp, TX_FLUSH_TO_DRIVER) != 0) {
			should_abort = B_FALSE;
			break;
		}

		EFX_EV_QSTAT_INCR(eep, EV_DRIVER_TX_DESCQ_FLS_DONE);

		EFSYS_PROBE1(tx_descq_fls_done, uint32_t, txq_index);

		EFSYS_ASSERT(eecp->eec_txq_flush_done != NULL);
		should_abort = eecp->eec_txq_flush_done(arg, txq_index);
		break;
	}

	case MCDI_EVENT_CODE_RX_ERR: {
		/*
		 * After an RXQ error is detected, firmware sends an RX_ERR
		 * event. This may be followed by RX events (which we discard),
		 * and then finally by an RX_FLUSH event. Firmware destroys the
		 * RXQ automatically after sending the RX_FLUSH event.
		 */
		enp->en_reset_flags |= EFX_RESET_RXQ_ERR;

		EFSYS_PROBE1(rx_descq_err, uint32_t, MCDI_EV_FIELD(eqp, DATA));

		/* Inform the driver that a reset is required. */
		eecp->eec_exception(arg, EFX_EXCEPTION_RX_ERROR,
		    MCDI_EV_FIELD(eqp, RX_ERR_DATA));
		break;
	}

	case MCDI_EVENT_CODE_RX_FLUSH: {
		uint32_t rxq_index = MCDI_EV_FIELD(eqp, RX_FLUSH_RXQ);

		/*
		 * EF10 firmware sends two RX_FLUSH events: one to the rxq's
		 * event queue, and one to evq 0 (with RX_FLUSH_TO_DRIVER set).
		 * We want to wait for all completions, so ignore the events
		 * with RX_FLUSH_TO_DRIVER.
		 */
		if (MCDI_EV_FIELD(eqp, RX_FLUSH_TO_DRIVER) != 0) {
			should_abort = B_FALSE;
			break;
		}

		EFX_EV_QSTAT_INCR(eep, EV_DRIVER_RX_DESCQ_FLS_DONE);

		EFSYS_PROBE1(rx_descq_fls_done, uint32_t, rxq_index);

		EFSYS_ASSERT(eecp->eec_rxq_flush_done != NULL);
		should_abort = eecp->eec_rxq_flush_done(arg, rxq_index);
		break;
	}

	default:
		EFSYS_PROBE3(bad_event, unsigned int, eep->ee_index,
		    uint32_t, EFX_QWORD_FIELD(*eqp, EFX_DWORD_1),
		    uint32_t, EFX_QWORD_FIELD(*eqp, EFX_DWORD_0));
		break;
	}

	return (should_abort);
}

		void
hunt_ev_rxlabel_init(
	__in		efx_evq_t *eep,
	__in		efx_rxq_t *erp,
	__in		unsigned int label)
{
	efx_evq_rxq_state_t *eersp;

	EFSYS_ASSERT3U(label, <, EFX_ARRAY_SIZE(eep->ee_rxq_state));
	eersp = &eep->ee_rxq_state[label];

	EFSYS_ASSERT3U(eersp->eers_rx_mask, ==, 0);

	eersp->eers_rx_read_ptr = 0;
	eersp->eers_rx_mask = erp->er_mask;
}

		void
hunt_ev_rxlabel_fini(
	__in		efx_evq_t *eep,
	__in		unsigned int label)
{
	efx_evq_rxq_state_t *eersp;

	EFSYS_ASSERT3U(label, <, EFX_ARRAY_SIZE(eep->ee_rxq_state));
	eersp = &eep->ee_rxq_state[label];

	EFSYS_ASSERT3U(eersp->eers_rx_mask, !=, 0);

	eersp->eers_rx_read_ptr = 0;
	eersp->eers_rx_mask = 0;
}

#endif	/* EFSYS_OPT_HUNTINGTON */
