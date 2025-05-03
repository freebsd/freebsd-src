/*-
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2024 Broadcom, All Rights Reserved.
 * The term Broadcom refers to Broadcom Limited and/or its subsidiaries
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

#ifndef _BNXT_DCB_H
#define _BNXT_DCB_H

#define BNXT_IEEE_8021QAZ_MAX_TCS		8
#define BNXT_IEEE_8021QAZ_TSA_STRICT		0
#define BNXT_IEEE_8021QAZ_TSA_ETS		2
#define BNXT_IEEE_8021QAZ_TSA_VENDOR		255

#define BNXT_DCB_CAP_DCBX_HOST			0x01
#define BNXT_DCB_CAP_DCBX_LLD_MANAGED		0x02
#define BNXT_DCB_CAP_DCBX_VER_CEE		0x04
#define BNXT_DCB_CAP_DCBX_VER_IEEE		0x08
#define BNXT_DCB_CAP_DCBX_STATIC		0x10

#ifndef	__struct_group
#define __struct_group(TAG, NAME, ATTRS, MEMBERS...) \
	union { \
		struct { MEMBERS } ATTRS; \
		struct TAG { MEMBERS } ATTRS NAME; \
	}
#endif
#ifndef	struct_group_attr
#define struct_group_attr(NAME, ATTRS, MEMBERS...) \
	__struct_group(/* no tag */, NAME, ATTRS, MEMBERS)
#endif

struct bnxt_cos2bw_cfg {
	uint8_t			pad[3];
	struct_group_attr(cfg, __packed,
		uint8_t			queue_id;
		uint32_t		min_bw;
		uint32_t		max_bw;
#define BW_VALUE_UNIT_PERCENT1_100		(0x1UL << 29)
		uint8_t			tsa;
		uint8_t			pri_lvl;
		uint8_t			bw_weight;
	);
	uint8_t			unused;
};

struct bnxt_dscp2pri_entry {
	uint8_t	dscp;
	uint8_t	mask;
	uint8_t	pri;
};

struct bnxt_ieee_ets {
	uint8_t    willing;
	uint8_t    ets_cap;
	uint8_t    cbs;
	uint8_t    tc_tx_bw[BNXT_IEEE_8021QAZ_MAX_TCS];
	uint8_t    tc_rx_bw[BNXT_IEEE_8021QAZ_MAX_TCS];
	uint8_t    tc_tsa[BNXT_IEEE_8021QAZ_MAX_TCS];
	uint8_t    prio_tc[BNXT_IEEE_8021QAZ_MAX_TCS];
	uint8_t    tc_reco_bw[BNXT_IEEE_8021QAZ_MAX_TCS];
	uint8_t    tc_reco_tsa[BNXT_IEEE_8021QAZ_MAX_TCS];
	uint8_t    reco_prio_tc[BNXT_IEEE_8021QAZ_MAX_TCS];
} __attribute__ ((__packed__));

struct bnxt_ieee_pfc {
	uint8_t    pfc_cap;
	uint8_t    pfc_en;
	uint8_t    mbc;
	uint16_t   delay;
	uint64_t   requests[BNXT_IEEE_8021QAZ_MAX_TCS];
	uint64_t   indications[BNXT_IEEE_8021QAZ_MAX_TCS];
} __attribute__ ((__packed__));

struct bnxt_dcb_app {
	uint8_t    selector;
	uint8_t    priority;
	uint16_t   protocol;
} __attribute__ ((__packed__));

struct bnxt_eee {
	uint32_t   cmd;
	uint32_t   supported;
	uint32_t   advertised;
	uint32_t   lp_advertised;
	uint32_t   eee_active;
	uint32_t   eee_enabled;
	uint32_t   tx_lpi_enabled;
	uint32_t   tx_lpi_timer;
	uint32_t   reserved[2];
} __attribute__ ((__packed__));

#define BNXT_IEEE_8021QAZ_APP_SEL_ETHERTYPE	1
#define BNXT_IEEE_8021QAZ_APP_SEL_STREAM	2
#define BNXT_IEEE_8021QAZ_APP_SEL_DGRAM		3
#define BNXT_IEEE_8021QAZ_APP_SEL_ANY		4
#define BNXT_IEEE_8021QAZ_APP_SEL_DSCP       	5
#define ETH_P_ROCE 				0x8915
#define ROCE_V2_UDP_DPORT 			4791

#define BNXT_LLQ(q_profile)	\
	((q_profile) == HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID0_SERVICE_PROFILE_LOSSLESS_ROCE ||	\
	 (q_profile) == HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID0_SERVICE_PROFILE_LOSSLESS_NIC)
#define BNXT_CNPQ(q_profile)	\
	((q_profile) == HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID0_SERVICE_PROFILE_LOSSY_ROCE_CNP)

#define HWRM_STRUCT_DATA_SUBTYPE_HOST_OPERATIONAL	0x0300

#endif
