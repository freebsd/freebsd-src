/*-
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2016 Broadcom, All Rights Reserved.
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

#include <sys/cdefs.h>
#ifndef _BNXT_H
#define _BNXT_H

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/bitstring.h>

#include <machine/bus.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/iflib.h>
#include <linux/types.h>

#include "hsi_struct_def.h"
#include "bnxt_dcb.h"
#include "bnxt_auxbus_compat.h"

#define DFLT_HWRM_CMD_TIMEOUT		500

/* PCI IDs */
#define BROADCOM_VENDOR_ID	0x14E4

#define BCM57301	0x16c8
#define BCM57302	0x16c9
#define BCM57304	0x16ca
#define BCM57311	0x16ce
#define BCM57312	0x16cf
#define BCM57314	0x16df
#define BCM57402	0x16d0
#define BCM57402_NPAR	0x16d4
#define BCM57404	0x16d1
#define BCM57404_NPAR	0x16e7
#define BCM57406	0x16d2
#define BCM57406_NPAR	0x16e8
#define BCM57407	0x16d5
#define BCM57407_NPAR	0x16ea
#define BCM57407_SFP	0x16e9
#define BCM57412	0x16d6
#define BCM57412_NPAR1	0x16de
#define BCM57412_NPAR2	0x16eb
#define BCM57414	0x16d7
#define BCM57414_NPAR1	0x16ec
#define BCM57414_NPAR2	0x16ed
#define BCM57416	0x16d8
#define BCM57416_NPAR1	0x16ee
#define BCM57416_NPAR2	0x16ef
#define BCM57416_SFP	0x16e3
#define BCM57417	0x16d9
#define BCM57417_NPAR1	0x16c0
#define BCM57417_NPAR2	0x16cc
#define BCM57417_SFP	0x16e2
#define BCM57454	0x1614
#define BCM58700	0x16cd
#define BCM57508  	0x1750
#define BCM57504  	0x1751
#define BCM57504_NPAR	0x1801
#define BCM57502  	0x1752
#define BCM57608  	0x1760
#define BCM57604  	0x1761
#define BCM57602  	0x1762
#define BCM57601  	0x1763
#define NETXTREME_C_VF1	0x16cb
#define NETXTREME_C_VF2	0x16e1
#define NETXTREME_C_VF3	0x16e5
#define NETXTREME_E_VF1	0x16c1
#define NETXTREME_E_VF2	0x16d3
#define NETXTREME_E_VF3	0x16dc

#define EVENT_DATA1_RESET_NOTIFY_FATAL(data1)				\
	(((data1) &							\
	  HWRM_ASYNC_EVENT_CMPL_RESET_NOTIFY_EVENT_DATA1_REASON_CODE_MASK) ==\
	 HWRM_ASYNC_EVENT_CMPL_RESET_NOTIFY_EVENT_DATA1_REASON_CODE_FW_EXCEPTION_FATAL)

#define BNXT_EVENT_ERROR_REPORT_TYPE(data1)						\
	(((data1) &									\
	  HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_BASE_EVENT_DATA1_ERROR_TYPE_MASK)  >>	\
	 HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_BASE_EVENT_DATA1_ERROR_TYPE_SFT)

#define BNXT_EVENT_INVALID_SIGNAL_DATA(data2)						\
	(((data2) &									\
	  HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_INVALID_SIGNAL_EVENT_DATA2_PIN_ID_MASK) >>	\
	 HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_INVALID_SIGNAL_EVENT_DATA2_PIN_ID_SFT)

#define BNXT_EVENT_DBR_EPOCH(data)										\
	(((data) & HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_DOORBELL_DROP_THRESHOLD_EVENT_DATA1_EPOCH_MASK) >>	\
	 HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_DOORBELL_DROP_THRESHOLD_EVENT_DATA1_EPOCH_SFT)

#define BNXT_EVENT_THERMAL_THRESHOLD_TEMP(data2)						\
	(((data2) &										\
	  HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_THERMAL_EVENT_DATA2_THRESHOLD_TEMP_MASK) >>	\
	 HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_THERMAL_EVENT_DATA2_THRESHOLD_TEMP_SFT)

#define EVENT_DATA2_NVM_ERR_ADDR(data2)						\
	(((data2) &								\
	  HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_NVM_EVENT_DATA2_ERR_ADDR_MASK) >>	\
	 HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_NVM_EVENT_DATA2_ERR_ADDR_SFT)

#define EVENT_DATA1_THERMAL_THRESHOLD_DIR_INCREASING(data1)					\
	(((data1) &										\
	  HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_THERMAL_EVENT_DATA1_TRANSITION_DIR) ==		\
	 HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_THERMAL_EVENT_DATA1_TRANSITION_DIR_INCREASING)

#define EVENT_DATA1_NVM_ERR_TYPE_WRITE(data1)						\
	(((data1) &									\
	  HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_NVM_EVENT_DATA1_NVM_ERR_TYPE_MASK) ==	\
	 HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_NVM_EVENT_DATA1_NVM_ERR_TYPE_WRITE)

#define EVENT_DATA1_NVM_ERR_TYPE_ERASE(data1)						\
	(((data1) &									\
	  HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_NVM_EVENT_DATA1_NVM_ERR_TYPE_MASK) ==	\
	 HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_NVM_EVENT_DATA1_NVM_ERR_TYPE_ERASE)

#define EVENT_DATA1_THERMAL_THRESHOLD_TYPE(data1)			\
	((data1) & HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_THERMAL_EVENT_DATA1_THRESHOLD_TYPE_MASK)

#define BNXT_EVENT_THERMAL_CURRENT_TEMP(data2)				\
	((data2) & HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_THERMAL_EVENT_DATA2_CURRENT_TEMP_MASK)

#define EVENT_DATA1_RESET_NOTIFY_FW_ACTIVATION(data1)                   \
	(((data1) &                                                     \
	  HWRM_ASYNC_EVENT_CMPL_RESET_NOTIFY_EVENT_DATA1_REASON_CODE_MASK) ==\
	HWRM_ASYNC_EVENT_CMPL_RESET_NOTIFY_EVENT_DATA1_REASON_CODE_FW_ACTIVATION)

#define EVENT_DATA2_RESET_NOTIFY_FW_STATUS_CODE(data2)                  \
	((data2) &                                                      \
	HWRM_ASYNC_EVENT_CMPL_RESET_NOTIFY_EVENT_DATA2_FW_STATUS_CODE_MASK)

#define EVENT_DATA1_RECOVERY_ENABLED(data1)				\
	!!((data1) &							\
	   HWRM_ASYNC_EVENT_CMPL_ERROR_RECOVERY_EVENT_DATA1_FLAGS_RECOVERY_ENABLED)

#define EVENT_DATA1_RECOVERY_MASTER_FUNC(data1)				\
	!!((data1) &							\
	   HWRM_ASYNC_EVENT_CMPL_ERROR_RECOVERY_EVENT_DATA1_FLAGS_MASTER_FUNC)

#define INVALID_STATS_CTX_ID     -1

/* Maximum numbers of RX and TX descriptors. iflib requires this to be a power
 * of two. The hardware has no particular limitation. */
#define BNXT_MAX_RXD	((INT32_MAX >> 1) + 1)
#define BNXT_MAX_TXD	((INT32_MAX >> 1) + 1)

#define CSUM_OFFLOAD		(CSUM_IP_TSO|CSUM_IP6_TSO|CSUM_IP| \
				 CSUM_IP_UDP|CSUM_IP_TCP|CSUM_IP_SCTP| \
				 CSUM_IP6_UDP|CSUM_IP6_TCP|CSUM_IP6_SCTP)

#define BNXT_MAX_MTU	9600

#define BNXT_RSS_HASH_TYPE_TCPV4	0
#define BNXT_RSS_HASH_TYPE_UDPV4	1
#define BNXT_RSS_HASH_TYPE_IPV4		2
#define BNXT_RSS_HASH_TYPE_TCPV6	3
#define BNXT_RSS_HASH_TYPE_UDPV6	4
#define BNXT_RSS_HASH_TYPE_IPV6		5
#define BNXT_GET_RSS_PROFILE_ID(rss_hash_type) ((rss_hash_type >> 1) & 0x1F)

#define BNXT_NO_MORE_WOL_FILTERS	0xFFFF
#define bnxt_wol_supported(softc)	(!((softc)->flags & BNXT_FLAG_VF) && \
					  ((softc)->flags & BNXT_FLAG_WOL_CAP ))
/* 64-bit doorbell */
#define DBR_INDEX_MASK					0x0000000000ffffffULL
#define DBR_PI_LO_MASK					0xff000000UL
#define DBR_PI_LO_SFT					24
#define DBR_EPOCH_MASK					0x01000000UL
#define DBR_EPOCH_SFT					24
#define DBR_TOGGLE_MASK					0x06000000UL
#define DBR_TOGGLE_SFT					25
#define DBR_XID_MASK					0x000fffff00000000ULL
#define DBR_XID_SFT					32
#define DBR_PI_HI_MASK					0xf0000000000000ULL
#define DBR_PI_HI_SFT					52
#define DBR_PATH_L2					(0x1ULL << 56)
#define DBR_VALID					(0x1ULL << 58)
#define DBR_TYPE_SQ					(0x0ULL << 60)
#define DBR_TYPE_RQ					(0x1ULL << 60)
#define DBR_TYPE_SRQ					(0x2ULL << 60)
#define DBR_TYPE_SRQ_ARM				(0x3ULL << 60)
#define DBR_TYPE_CQ					(0x4ULL << 60)
#define DBR_TYPE_CQ_ARMSE				(0x5ULL << 60)
#define DBR_TYPE_CQ_ARMALL				(0x6ULL << 60)
#define DBR_TYPE_CQ_ARMENA				(0x7ULL << 60)
#define DBR_TYPE_SRQ_ARMENA				(0x8ULL << 60)
#define DBR_TYPE_CQ_CUTOFF_ACK				(0x9ULL << 60)
#define DBR_TYPE_NQ					(0xaULL << 60)
#define DBR_TYPE_NQ_ARM					(0xbULL << 60)
#define DBR_TYPE_PUSH_START				(0xcULL << 60)
#define DBR_TYPE_PUSH_END				(0xdULL << 60)
#define DBR_TYPE_NQ_MASK				(0xeULL << 60)
#define DBR_TYPE_NULL					(0xfULL << 60)

#define BNXT_MAX_L2_QUEUES				128
#define BNXT_ROCE_IRQ_COUNT				9

#define BNXT_MAX_NUM_QUEUES (BNXT_MAX_L2_QUEUES + BNXT_ROCE_IRQ_COUNT)

/* Completion related defines */
#define CMP_VALID(cmp, v_bit) \
	((!!(((struct cmpl_base *)(cmp))->info3_v & htole32(CMPL_BASE_V))) == !!(v_bit) )

/* Chip class phase 5 */
#define BNXT_CHIP_P5(sc) ((sc->flags & BNXT_FLAG_CHIP_P5))

/* Chip class phase 7 */
#define BNXT_CHIP_P7(sc) ((sc->flags & BNXT_FLAG_CHIP_P7))

/* Chip class phase 5 plus */
#define BNXT_CHIP_P5_PLUS(sc)                   \
	(BNXT_CHIP_P5(sc) || BNXT_CHIP_P7(sc))

#define DB_PF_OFFSET_P5                                 0x10000
#define DB_VF_OFFSET_P5                                 0x4000
#define NQ_VALID(cmp, v_bit) \
	((!!(((nq_cn_t *)(cmp))->v & htole32(NQ_CN_V))) == !!(v_bit) )

#ifndef DIV_ROUND_UP
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#endif
#ifndef roundup
#define roundup(x, y) ((((x) + ((y) - 1)) / (y)) * (y))
#endif

#define NEXT_CP_CONS_V(ring, cons, v_bit) do {				    \
	if (__predict_false(++(cons) == (ring)->ring_size))		    \
		((cons) = 0, (v_bit) = !v_bit);				    \
} while (0)

#define RING_NEXT(ring, idx) (__predict_false(idx + 1 == (ring)->ring_size) ? \
								0 : idx + 1)

#define CMPL_PREFETCH_NEXT(cpr, idx)					    \
	__builtin_prefetch(&((struct cmpl_base *)(cpr)->ring.vaddr)[((idx) +\
	    (CACHE_LINE_SIZE / sizeof(struct cmpl_base))) &		    \
	    ((cpr)->ring.ring_size - 1)])

/* Lock macros */
#define BNXT_HWRM_LOCK_INIT(_softc, _name) \
    mtx_init(&(_softc)->hwrm_lock, _name, "BNXT HWRM Lock", MTX_DEF)
#define BNXT_HWRM_LOCK(_softc)		mtx_lock(&(_softc)->hwrm_lock)
#define BNXT_HWRM_UNLOCK(_softc)	mtx_unlock(&(_softc)->hwrm_lock)
#define BNXT_HWRM_LOCK_DESTROY(_softc)	mtx_destroy(&(_softc)->hwrm_lock)
#define BNXT_HWRM_LOCK_ASSERT(_softc)	mtx_assert(&(_softc)->hwrm_lock,    \
    MA_OWNED)
#define BNXT_IS_FLOW_CTRL_CHANGED(link_info)				    \
	((link_info->last_flow_ctrl.tx != link_info->flow_ctrl.tx) ||       \
         (link_info->last_flow_ctrl.rx != link_info->flow_ctrl.rx) ||       \
	 (link_info->last_flow_ctrl.autoneg != link_info->flow_ctrl.autoneg))

/* Chip info */
#define BNXT_TSO_SIZE	UINT16_MAX

#define min_t(type, x, y) ({                    \
        type __min1 = (x);                      \
        type __min2 = (y);                      \
        __min1 < __min2 ? __min1 : __min2; })

#define max_t(type, x, y) ({                    \
        type __max1 = (x);                      \
        type __max2 = (y);                      \
        __max1 > __max2 ? __max1 : __max2; })

#define clamp_t(type, _x, min, max)     min_t(type, max_t(type, _x, min), max)

#define BNXT_IFMEDIA_ADD(supported, fw_speed, ifm_speed) do {			\
	if ((supported) & HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_ ## fw_speed)	\
		ifmedia_add(softc->media, IFM_ETHER | (ifm_speed), 0, NULL);	\
} while(0)

#define BNXT_MIN_FRAME_SIZE	52	/* Frames must be padded to this size for some A0 chips */

#define BNXT_RX_STATS_EXT_OFFSET(counter)		\
	(offsetof(struct rx_port_stats_ext, counter) / 8)

#define BNXT_RX_STATS_EXT_NUM_LEGACY			\
	BNXT_RX_STATS_EXT_OFFSET(rx_fec_corrected_blocks)

#define BNXT_TX_STATS_EXT_OFFSET(counter)		\
	(offsetof(struct tx_port_stats_ext, counter) / 8)

extern const char bnxt_driver_version[];
typedef void (*bnxt_doorbell_tx)(void *, uint16_t idx);
typedef void (*bnxt_doorbell_rx)(void *, uint16_t idx);
typedef void (*bnxt_doorbell_rx_cq)(void *, bool);
typedef void (*bnxt_doorbell_tx_cq)(void *, bool);
typedef void (*bnxt_doorbell_nq)(void *, bool);

typedef struct bnxt_doorbell_ops {
        bnxt_doorbell_tx bnxt_db_tx;
        bnxt_doorbell_rx bnxt_db_rx;
        bnxt_doorbell_rx_cq bnxt_db_rx_cq;
        bnxt_doorbell_tx_cq bnxt_db_tx_cq;
        bnxt_doorbell_nq bnxt_db_nq;
} bnxt_dooorbell_ops_t;
/* NVRAM access */
enum bnxt_nvm_directory_type {
	BNX_DIR_TYPE_UNUSED = 0,
	BNX_DIR_TYPE_PKG_LOG = 1,
	BNX_DIR_TYPE_UPDATE = 2,
	BNX_DIR_TYPE_CHIMP_PATCH = 3,
	BNX_DIR_TYPE_BOOTCODE = 4,
	BNX_DIR_TYPE_VPD = 5,
	BNX_DIR_TYPE_EXP_ROM_MBA = 6,
	BNX_DIR_TYPE_AVS = 7,
	BNX_DIR_TYPE_PCIE = 8,
	BNX_DIR_TYPE_PORT_MACRO = 9,
	BNX_DIR_TYPE_APE_FW = 10,
	BNX_DIR_TYPE_APE_PATCH = 11,
	BNX_DIR_TYPE_KONG_FW = 12,
	BNX_DIR_TYPE_KONG_PATCH = 13,
	BNX_DIR_TYPE_BONO_FW = 14,
	BNX_DIR_TYPE_BONO_PATCH = 15,
	BNX_DIR_TYPE_TANG_FW = 16,
	BNX_DIR_TYPE_TANG_PATCH = 17,
	BNX_DIR_TYPE_BOOTCODE_2 = 18,
	BNX_DIR_TYPE_CCM = 19,
	BNX_DIR_TYPE_PCI_CFG = 20,
	BNX_DIR_TYPE_TSCF_UCODE = 21,
	BNX_DIR_TYPE_ISCSI_BOOT = 22,
	BNX_DIR_TYPE_ISCSI_BOOT_IPV6 = 24,
	BNX_DIR_TYPE_ISCSI_BOOT_IPV4N6 = 25,
	BNX_DIR_TYPE_ISCSI_BOOT_CFG6 = 26,
	BNX_DIR_TYPE_EXT_PHY = 27,
	BNX_DIR_TYPE_SHARED_CFG = 40,
	BNX_DIR_TYPE_PORT_CFG = 41,
	BNX_DIR_TYPE_FUNC_CFG = 42,
	BNX_DIR_TYPE_MGMT_CFG = 48,
	BNX_DIR_TYPE_MGMT_DATA = 49,
	BNX_DIR_TYPE_MGMT_WEB_DATA = 50,
	BNX_DIR_TYPE_MGMT_WEB_META = 51,
	BNX_DIR_TYPE_MGMT_EVENT_LOG = 52,
	BNX_DIR_TYPE_MGMT_AUDIT_LOG = 53
};

enum bnxnvm_pkglog_field_index {
	BNX_PKG_LOG_FIELD_IDX_INSTALLED_TIMESTAMP	= 0,
	BNX_PKG_LOG_FIELD_IDX_PKG_DESCRIPTION		= 1,
	BNX_PKG_LOG_FIELD_IDX_PKG_VERSION		= 2,
	BNX_PKG_LOG_FIELD_IDX_PKG_TIMESTAMP		= 3,
	BNX_PKG_LOG_FIELD_IDX_PKG_CHECKSUM		= 4,
	BNX_PKG_LOG_FIELD_IDX_INSTALLED_ITEMS		= 5,
	BNX_PKG_LOG_FIELD_IDX_INSTALLED_MASK		= 6
};

#define BNX_DIR_ORDINAL_FIRST		0
#define BNX_DIR_EXT_NONE		0

struct bnxt_bar_info {
	struct resource		*res;
	bus_space_tag_t		tag;
	bus_space_handle_t	handle;
	bus_size_t		size;
	int			rid;
};

struct bnxt_flow_ctrl {
	bool rx;
	bool tx;
	bool autoneg;
};

struct bnxt_link_info {
	uint8_t		media_type;
	uint8_t		transceiver;
	uint8_t		phy_addr;
	uint8_t		phy_link_status;
	uint8_t		wire_speed;
	uint8_t		loop_back;
	uint8_t		link_up;
	uint8_t		last_link_up;
	uint8_t		duplex;
	uint8_t		last_duplex;
	uint8_t		last_phy_type;
	struct bnxt_flow_ctrl   flow_ctrl;
	struct bnxt_flow_ctrl   last_flow_ctrl;
	uint8_t		duplex_setting;
	uint8_t		auto_mode;
#define PHY_VER_LEN		3
	uint8_t		phy_ver[PHY_VER_LEN];
	uint8_t		phy_type;
#define BNXT_PHY_STATE_ENABLED		0
#define BNXT_PHY_STATE_DISABLED		1
	uint8_t		phy_state;

	uint16_t	link_speed;
	uint16_t	support_speeds;
	uint16_t	support_speeds2;
	uint16_t	support_pam4_speeds;
	uint16_t	auto_link_speeds;
	uint16_t	auto_link_speeds2;
	uint16_t	auto_pam4_link_speeds;
	uint16_t	force_link_speed;
	uint16_t	force_link_speeds2;
	uint16_t	force_pam4_link_speed;

	bool		force_pam4_speed;
	bool		force_speed2_nrz;
	bool		force_pam4_56_speed2;
	bool		force_pam4_112_speed2;

	uint16_t	advertising;
	uint16_t	advertising_pam4;

	uint32_t	preemphasis;
	uint16_t	support_auto_speeds;
	uint16_t	support_force_speeds;
	uint16_t	support_pam4_auto_speeds;
	uint16_t	support_pam4_force_speeds;
	uint16_t	support_auto_speeds2;
	uint16_t	support_force_speeds2;
#define BNXT_SIG_MODE_NRZ	HWRM_PORT_PHY_QCFG_OUTPUT_SIGNAL_MODE_NRZ
#define BNXT_SIG_MODE_PAM4	HWRM_PORT_PHY_QCFG_OUTPUT_SIGNAL_MODE_PAM4
#define BNXT_SIG_MODE_PAM4_112 HWRM_PORT_PHY_QCFG_OUTPUT_SIGNAL_MODE_PAM4_112
	uint8_t		req_signal_mode;

	uint8_t		active_fec_sig_mode;
	uint8_t		sig_mode;

	/* copy of requested setting */
	uint8_t		autoneg;
#define BNXT_AUTONEG_SPEED	1
#define BNXT_AUTONEG_FLOW_CTRL	2
	uint8_t		req_duplex;
	uint16_t	req_link_speed;
	uint8_t		module_status;
	struct hwrm_port_phy_qcfg_output    phy_qcfg_resp;
};

enum bnxt_phy_type {
	BNXT_MEDIA_CR = 0,
	BNXT_MEDIA_LR,
	BNXT_MEDIA_SR,
	BNXT_MEDIA_ER,
	BNXT_MEDIA_KR,
	BNXT_MEDIA_AC,
	BNXT_MEDIA_BASECX,
	BNXT_MEDIA_BASET,
	BNXT_MEDIA_BASEKX,
	BNXT_MEDIA_BASESGMII,
	BNXT_MEDIA_END
};

enum bnxt_cp_type {
	BNXT_DEFAULT,
	BNXT_TX,
	BNXT_RX,
	BNXT_SHARED
};

struct bnxt_queue_info {
	uint8_t		queue_id;
	uint8_t		queue_profile;
};

struct bnxt_func_info {
	uint32_t	fw_fid;
	uint8_t		mac_addr[ETHER_ADDR_LEN];
	uint16_t	max_rsscos_ctxs;
	uint16_t	max_cp_rings;
	uint16_t	max_tx_rings;
	uint16_t	max_rx_rings;
	uint16_t	max_hw_ring_grps;
	uint16_t	max_irqs;
	uint16_t	max_l2_ctxs;
	uint16_t	max_vnics;
	uint16_t	max_stat_ctxs;
};

struct bnxt_pf_info {
#define BNXT_FIRST_PF_FID	1
#define BNXT_FIRST_VF_FID	128
	uint8_t		port_id;
	uint32_t	first_vf_id;
	uint16_t	active_vfs;
	uint16_t	max_vfs;
	uint32_t	max_encap_records;
	uint32_t	max_decap_records;
	uint32_t	max_tx_em_flows;
	uint32_t	max_tx_wm_flows;
	uint32_t	max_rx_em_flows;
	uint32_t	max_rx_wm_flows;
	unsigned long	*vf_event_bmap;
	uint16_t	hwrm_cmd_req_pages;
	void		*hwrm_cmd_req_addr[4];
	bus_addr_t	hwrm_cmd_req_dma_addr[4];
};

struct bnxt_vf_info {
	uint16_t	fw_fid;
	uint8_t		mac_addr[ETHER_ADDR_LEN];
	uint16_t	max_rsscos_ctxs;
	uint16_t	max_cp_rings;
	uint16_t	max_tx_rings;
	uint16_t	max_rx_rings;
	uint16_t	max_hw_ring_grps;
	uint16_t	max_l2_ctxs;
	uint16_t	max_irqs;
	uint16_t	max_vnics;
	uint16_t	max_stat_ctxs;
	uint32_t	vlan;
#define BNXT_VF_QOS		0x1
#define BNXT_VF_SPOOFCHK	0x2
#define BNXT_VF_LINK_FORCED	0x4
#define BNXT_VF_LINK_UP		0x8
	uint32_t	flags;
	uint32_t	func_flags; /* func cfg flags */
	uint32_t	min_tx_rate;
	uint32_t	max_tx_rate;
	void		*hwrm_cmd_req_addr;
	bus_addr_t	hwrm_cmd_req_dma_addr;
};

#define BNXT_PF(softc)		(!((softc)->flags & BNXT_FLAG_VF))
#define BNXT_VF(softc)		((softc)->flags & BNXT_FLAG_VF)

struct bnxt_vlan_tag {
	SLIST_ENTRY(bnxt_vlan_tag) next;
	uint64_t	filter_id;
	uint16_t	tag;
};

struct bnxt_vnic_info {
	uint16_t	id;
	uint16_t	def_ring_grp;
	uint16_t	cos_rule;
	uint16_t	lb_rule;
	uint16_t	mru;

	uint32_t	rx_mask;
	struct iflib_dma_info mc_list;
	int		mc_list_count;
#define BNXT_MAX_MC_ADDRS		16

	uint32_t	flags;
#define BNXT_VNIC_FLAG_DEFAULT		0x01
#define BNXT_VNIC_FLAG_BD_STALL		0x02
#define BNXT_VNIC_FLAG_VLAN_STRIP	0x04

	uint64_t	filter_id;

	uint16_t	rss_id;
	uint32_t	rss_hash_type;
	uint8_t		rss_hash_key[HW_HASH_KEY_SIZE];
	struct iflib_dma_info rss_hash_key_tbl;
	struct iflib_dma_info	rss_grp_tbl;
	SLIST_HEAD(vlan_head, bnxt_vlan_tag) vlan_tags;
	struct iflib_dma_info vlan_tag_list;
};

struct bnxt_grp_info {
	uint16_t	stats_ctx;
	uint16_t	grp_id;
	uint16_t	rx_ring_id;
	uint16_t	cp_ring_id;
	uint16_t	ag_ring_id;
};

#define	EPOCH_ARR_SZ	4096

struct bnxt_ring {
	uint64_t		paddr;
	vm_offset_t		doorbell;
	caddr_t			vaddr;
	struct bnxt_softc	*softc;
	uint32_t		ring_size;	/* Must be a power of two */
	uint16_t		id;		/* Logical ID */
	uint16_t		phys_id;
	uint16_t		idx;
	struct bnxt_full_tpa_start *tpa_start;
	union {
		u64             db_key64;
		u32             db_key32;
	};
	uint32_t                db_ring_mask;
	uint32_t                db_epoch_mask;
	uint8_t                 db_epoch_shift;

	uint64_t		epoch_arr[EPOCH_ARR_SZ];
	bool                    epoch_bit;

};

struct bnxt_cp_ring {
	struct bnxt_ring	ring;
	struct if_irq		irq;
	uint32_t		cons;
	uint32_t		raw_cons;
	bool			v_bit;		/* Value of valid bit */
	struct ctx_hw_stats	*stats;
	uint32_t		stats_ctx_id;
	uint32_t		last_idx;	/* Used by RX rings only
						 * set to the last read pidx
						 */
	uint64_t 		int_count;
	uint8_t			toggle;
	uint8_t			type;
#define Q_TYPE_TX		1
#define Q_TYPE_RX		2
};

struct bnxt_full_tpa_start {
	struct rx_tpa_start_cmpl low;
	struct rx_tpa_start_cmpl_hi high;
};

/* All the version information for the part */
#define BNXT_VERSTR_SIZE	(3*3+2+1)	/* ie: "255.255.255\0" */
#define BNXT_NAME_SIZE		17
#define FW_VER_STR_LEN          32
#define BC_HWRM_STR_LEN         21
struct bnxt_ver_info {
	uint8_t		hwrm_if_major;
	uint8_t		hwrm_if_minor;
	uint8_t		hwrm_if_update;
	char		hwrm_if_ver[BNXT_VERSTR_SIZE];
	char		driver_hwrm_if_ver[BNXT_VERSTR_SIZE];
	char		mgmt_fw_ver[FW_VER_STR_LEN];
	char		netctrl_fw_ver[FW_VER_STR_LEN];
	char		roce_fw_ver[FW_VER_STR_LEN];
	char		fw_ver_str[FW_VER_STR_LEN];
	char		phy_ver[BNXT_VERSTR_SIZE];
	char		pkg_ver[64];

	char		hwrm_fw_name[BNXT_NAME_SIZE];
	char		mgmt_fw_name[BNXT_NAME_SIZE];
	char		netctrl_fw_name[BNXT_NAME_SIZE];
	char		roce_fw_name[BNXT_NAME_SIZE];
	char		phy_vendor[BNXT_NAME_SIZE];
	char		phy_partnumber[BNXT_NAME_SIZE];

	uint16_t	chip_num;
	uint8_t		chip_rev;
	uint8_t		chip_metal;
	uint8_t		chip_bond_id;
	uint8_t		chip_type;

	uint8_t		hwrm_min_major;
	uint8_t		hwrm_min_minor;
	uint8_t		hwrm_min_update;
	uint64_t	fw_ver_code;
#define BNXT_FW_VER_CODE(maj, min, bld, rsv)			\
	((uint64_t)(maj) << 48 | (uint64_t)(min) << 32 | (uint64_t)(bld) << 16 | (rsv))
#define BNXT_FW_MAJ(softc)	((softc)->ver_info->fw_ver_code >> 48)
#define BNXT_FW_MIN(softc)	(((softc)->ver_info->fw_ver_code >> 32) & 0xffff)
#define BNXT_FW_BLD(softc)	(((softc)->ver_info->fw_ver_code >> 16) & 0xffff)
#define BNXT_FW_RSV(softc)	(((softc)->ver_info->fw_ver_code) & 0xffff)

	struct sysctl_ctx_list	ver_ctx;
	struct sysctl_oid	*ver_oid;
};

struct bnxt_nvram_info {
	uint16_t	mfg_id;
	uint16_t	device_id;
	uint32_t	sector_size;
	uint32_t	size;
	uint32_t	reserved_size;
	uint32_t	available_size;

	struct sysctl_ctx_list	nvm_ctx;
	struct sysctl_oid	*nvm_oid;
};

struct bnxt_func_qcfg {
	uint16_t alloc_completion_rings;
	uint16_t alloc_tx_rings;
	uint16_t alloc_rx_rings;
	uint16_t alloc_vnics;
};

struct bnxt_hw_lro {
	uint16_t enable;
	uint16_t is_mode_gro;
	uint16_t max_agg_segs;
	uint16_t max_aggs;
	uint32_t min_agg_len;
};

/* The hardware supports certain page sizes.  Use the supported page sizes
 * to allocate the rings.
 */
#if (PAGE_SHIFT < 12)
#define BNXT_PAGE_SHIFT 12
#elif (PAGE_SHIFT <= 13)
#define BNXT_PAGE_SHIFT PAGE_SHIFT
#elif (PAGE_SHIFT < 16)
#define BNXT_PAGE_SHIFT 13
#else
#define BNXT_PAGE_SHIFT 16
#endif

#define BNXT_PAGE_SIZE  (1 << BNXT_PAGE_SHIFT)

#define MAX_CTX_PAGES	(BNXT_PAGE_SIZE / 8)
#define MAX_CTX_TOTAL_PAGES	(MAX_CTX_PAGES * MAX_CTX_PAGES)

struct bnxt_ring_mem_info {
	int			nr_pages;
	int			page_size;
	uint16_t		flags;
#define BNXT_RMEM_VALID_PTE_FLAG        1
#define BNXT_RMEM_RING_PTE_FLAG         2
#define BNXT_RMEM_USE_FULL_PAGE_FLAG    4
	uint16_t		depth;
	struct bnxt_ctx_mem_type	*ctx_mem;

	struct iflib_dma_info	*pg_arr;
	struct iflib_dma_info	pg_tbl;

	int			vmem_size;
	void			**vmem;
};

struct bnxt_ctx_pg_info {
	uint32_t		entries;
	uint32_t		nr_pages;
	struct iflib_dma_info   ctx_arr[MAX_CTX_PAGES];
	struct bnxt_ring_mem_info ring_mem;
	struct bnxt_ctx_pg_info **ctx_pg_tbl;
};

#define BNXT_MAX_TQM_SP_RINGS		1
#define BNXT_MAX_TQM_FP_LEGACY_RINGS	8
#define BNXT_MAX_TQM_FP_RINGS		9
#define BNXT_MAX_TQM_LEGACY_RINGS	\
	(BNXT_MAX_TQM_SP_RINGS + BNXT_MAX_TQM_FP_LEGACY_RINGS)
#define BNXT_MAX_TQM_RINGS		\
	(BNXT_MAX_TQM_SP_RINGS + BNXT_MAX_TQM_FP_RINGS)

#define BNXT_BACKING_STORE_CFG_LEGACY_LEN	256
#define BNXT_BACKING_STORE_CFG_LEN		\
	sizeof(struct hwrm_func_backing_store_cfg_input)

#define BNXT_SET_CTX_PAGE_ATTR(attr)					\
do {									\
	if (BNXT_PAGE_SIZE == 0x2000)					\
		attr = HWRM_FUNC_BACKING_STORE_CFG_INPUT_SRQ_PG_SIZE_PG_8K;	\
	else if (BNXT_PAGE_SIZE == 0x10000)				\
		attr = HWRM_FUNC_BACKING_STORE_CFG_INPUT_SRQ_PG_SIZE_PG_64K;	\
	else								\
		attr = HWRM_FUNC_BACKING_STORE_CFG_INPUT_SRQ_PG_SIZE_PG_4K;	\
} while (0)

struct bnxt_ctx_mem_type {
	u16	type;
	u16	entry_size;
	u32	flags;
#define BNXT_CTX_MEM_TYPE_VALID HWRM_FUNC_BACKING_STORE_QCAPS_V2_OUTPUT_FLAGS_TYPE_VALID
	u32	instance_bmap;
	u8	init_value;
	u8	entry_multiple;
	u16	init_offset;
#define	BNXT_CTX_INIT_INVALID_OFFSET	0xffff
	u32	max_entries;
	u32	min_entries;
	u8	last:1;
	u8	mem_valid:1;
	u8	split_entry_cnt;
#define BNXT_MAX_SPLIT_ENTRY	4
	union {
		struct {
			u32	qp_l2_entries;
			u32	qp_qp1_entries;
		};
		u32	srq_l2_entries;
		u32	cq_l2_entries;
		u32	vnic_entries;
		struct {
			u32	mrav_av_entries;
			u32	mrav_num_entries_units;
		};
		u32	split[BNXT_MAX_SPLIT_ENTRY];
	};
	struct bnxt_ctx_pg_info	*pg_info;
};

#define BNXT_CTX_QP	HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_TYPE_QP
#define BNXT_CTX_SRQ	HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_TYPE_SRQ
#define BNXT_CTX_CQ	HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_TYPE_CQ
#define BNXT_CTX_VNIC	HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_TYPE_VNIC
#define BNXT_CTX_STAT	HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_TYPE_STAT
#define BNXT_CTX_STQM	HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_TYPE_SP_TQM_RING
#define BNXT_CTX_FTQM	HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_TYPE_FP_TQM_RING
#define BNXT_CTX_MRAV	HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_TYPE_MRAV
#define BNXT_CTX_TIM	HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_TYPE_TIM
#define BNXT_CTX_TKC	HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_TYPE_TKC
#define BNXT_CTX_RKC	HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_TYPE_RKC
#define BNXT_CTX_MTQM	HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_TYPE_MP_TQM_RING
#define BNXT_CTX_SQDBS	HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_TYPE_SQ_DB_SHADOW
#define BNXT_CTX_RQDBS	HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_TYPE_RQ_DB_SHADOW
#define BNXT_CTX_SRQDBS	HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_TYPE_SRQ_DB_SHADOW
#define BNXT_CTX_CQDBS	HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_TYPE_CQ_DB_SHADOW
#define BNXT_CTX_QTKC	HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_TYPE_QUIC_TKC
#define BNXT_CTX_QRKC	HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_TYPE_QUIC_RKC
#define BNXT_CTX_MAX	(BNXT_CTX_TIM + 1)
#define BNXT_CTX_L2_MAX (BNXT_CTX_FTQM + 1)

#define BNXT_CTX_V2_MAX (HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_TYPE_ROCE_HWRM_TRACE + 1)
#define BNXT_CTX_SRT_TRACE		HWRM_FUNC_BACKING_STORE_QCFG_V2_OUTPUT_TYPE_SRT_TRACE
#define BNXT_CTX_ROCE_HWRM_TRACE	HWRM_FUNC_BACKING_STORE_CFG_V2_INPUT_TYPE_ROCE_HWRM_TRACE
#define BNXT_CTX_INV	((u16)-1)

struct bnxt_ctx_mem_info {
	u8	tqm_fp_rings_count;

	u32	flags;
	#define BNXT_CTX_FLAG_INITED	0x01
	struct bnxt_ctx_mem_type	ctx_arr[BNXT_CTX_V2_MAX];
};

struct bnxt_hw_resc {
	uint16_t	min_rsscos_ctxs;
	uint16_t	max_rsscos_ctxs;
	uint16_t	min_cp_rings;
	uint16_t	max_cp_rings;
	uint16_t	resv_cp_rings;
	uint16_t	min_tx_rings;
	uint16_t	max_tx_rings;
	uint16_t	resv_tx_rings;
	uint16_t	max_tx_sch_inputs;
	uint16_t	min_rx_rings;
	uint16_t	max_rx_rings;
	uint16_t	resv_rx_rings;
	uint16_t	min_hw_ring_grps;
	uint16_t	max_hw_ring_grps;
	uint16_t	resv_hw_ring_grps;
	uint16_t	min_l2_ctxs;
	uint16_t	max_l2_ctxs;
	uint16_t	min_vnics;
	uint16_t	max_vnics;
	uint16_t	resv_vnics;
	uint16_t	min_stat_ctxs;
	uint16_t	max_stat_ctxs;
	uint16_t	resv_stat_ctxs;
	uint16_t	max_nqs;
	uint16_t	max_irqs;
	uint16_t	resv_irqs;
};

enum bnxt_type_ets {
	BNXT_TYPE_ETS_TSA = 0,
	BNXT_TYPE_ETS_PRI2TC,
	BNXT_TYPE_ETS_TCBW,
	BNXT_TYPE_ETS_MAX
};

static const char *const BNXT_ETS_TYPE_STR[] = {
	"tsa",
	"pri2tc",
	"tcbw",
};

static const char *const BNXT_ETS_HELP_STR[] = {
	"X is 1 (strict),  0 (ets)",
	"TC values for pri 0 to 7",
	"TC BW values for pri 0 to 7, Sum should be 100",
};

#define BNXT_HWRM_MAX_REQ_LEN		(softc->hwrm_max_req_len)

struct bnxt_softc_list {
	SLIST_ENTRY(bnxt_softc_list) next;
	struct bnxt_softc *softc;
};

#ifndef BIT_ULL
#define BIT_ULL(nr)		(1ULL << (nr))
#endif

struct bnxt_aux_dev {
	struct auxiliary_device aux_dev;
	struct bnxt_en_dev *edev;
	int id;
};

struct bnxt_msix_tbl {
	uint32_t entry;
	uint32_t vector;
};

enum bnxt_health_severity {
	SEVERITY_NORMAL = 0,
	SEVERITY_WARNING,
	SEVERITY_RECOVERABLE,
	SEVERITY_FATAL,
};

enum bnxt_health_remedy {
	REMEDY_DEVLINK_RECOVER,
	REMEDY_POWER_CYCLE_DEVICE,
	REMEDY_POWER_CYCLE_HOST,
	REMEDY_FW_UPDATE,
	REMEDY_HW_REPLACE,
};

struct bnxt_fw_health {
	u32 flags;
	u32 polling_dsecs;
	u32 master_func_wait_dsecs;
	u32 normal_func_wait_dsecs;
	u32 post_reset_wait_dsecs;
	u32 post_reset_max_wait_dsecs;
	u32 regs[4];
	u32 mapped_regs[4];
#define BNXT_FW_HEALTH_REG		0
#define BNXT_FW_HEARTBEAT_REG		1
#define BNXT_FW_RESET_CNT_REG		2
#define BNXT_FW_RESET_INPROG_REG	3
	u32 fw_reset_inprog_reg_mask;
	u32 last_fw_heartbeat;
	u32 last_fw_reset_cnt;
	u8 enabled:1;
	u8 primary:1;
	u8 status_reliable:1;
	u8 resets_reliable:1;
	u8 tmr_multiplier;
	u8 tmr_counter;
	u8 fw_reset_seq_cnt;
	u32 fw_reset_seq_regs[16];
	u32 fw_reset_seq_vals[16];
	u32 fw_reset_seq_delay_msec[16];
	u32 echo_req_data1;
	u32 echo_req_data2;
	struct devlink_health_reporter	*fw_reporter;
	struct mutex lock;
	enum bnxt_health_severity severity;
	enum bnxt_health_remedy remedy;
	u32 arrests;
	u32 discoveries;
	u32 survivals;
	u32 fatalities;
	u32 diagnoses;
};

#define BNXT_FW_HEALTH_REG_TYPE_MASK	3
#define BNXT_FW_HEALTH_REG_TYPE_CFG	0
#define BNXT_FW_HEALTH_REG_TYPE_GRC	1
#define BNXT_FW_HEALTH_REG_TYPE_BAR0	2
#define BNXT_FW_HEALTH_REG_TYPE_BAR1	3

#define BNXT_FW_HEALTH_REG_TYPE(reg)	((reg) & BNXT_FW_HEALTH_REG_TYPE_MASK)
#define BNXT_FW_HEALTH_REG_OFF(reg)	((reg) & ~BNXT_FW_HEALTH_REG_TYPE_MASK)

#define BNXT_FW_HEALTH_WIN_BASE		0x3000
#define BNXT_FW_HEALTH_WIN_MAP_OFF	8

#define BNXT_FW_HEALTH_WIN_OFF(reg)	(BNXT_FW_HEALTH_WIN_BASE +	\
					 ((reg) & BNXT_GRC_OFFSET_MASK))

#define BNXT_FW_STATUS_HEALTH_MSK	0xffff
#define BNXT_FW_STATUS_HEALTHY		0x8000
#define BNXT_FW_STATUS_SHUTDOWN		0x100000
#define BNXT_FW_STATUS_RECOVERING	0x400000

#define BNXT_FW_IS_HEALTHY(sts)		(((sts) & BNXT_FW_STATUS_HEALTH_MSK) ==\
					 BNXT_FW_STATUS_HEALTHY)

#define BNXT_FW_IS_BOOTING(sts)		(((sts) & BNXT_FW_STATUS_HEALTH_MSK) < \
					 BNXT_FW_STATUS_HEALTHY)

#define BNXT_FW_IS_ERR(sts)		(((sts) & BNXT_FW_STATUS_HEALTH_MSK) > \
					 BNXT_FW_STATUS_HEALTHY)

#define BNXT_FW_IS_RECOVERING(sts)	(BNXT_FW_IS_ERR(sts) &&		       \
					 ((sts) & BNXT_FW_STATUS_RECOVERING))

#define BNXT_FW_RETRY			5
#define BNXT_FW_IF_RETRY		10
#define BNXT_FW_SLOT_RESET_RETRY	4

#define BNXT_GRCPF_REG_CHIMP_COMM		0x0
#define BNXT_GRCPF_REG_CHIMP_COMM_TRIGGER	0x100
#define BNXT_GRCPF_REG_WINDOW_BASE_OUT		0x400
#define BNXT_GRCPF_REG_SYNC_TIME		0x480
#define BNXT_GRCPF_REG_SYNC_TIME_ADJ		0x488
#define BNXT_GRCPF_REG_SYNC_TIME_ADJ_PER_MSK	0xffffffUL
#define BNXT_GRCPF_REG_SYNC_TIME_ADJ_PER_SFT	0
#define BNXT_GRCPF_REG_SYNC_TIME_ADJ_VAL_MSK	0x1f000000UL
#define BNXT_GRCPF_REG_SYNC_TIME_ADJ_VAL_SFT	24
#define BNXT_GRCPF_REG_SYNC_TIME_ADJ_SIGN_MSK	0x20000000UL
#define BNXT_GRCPF_REG_SYNC_TIME_ADJ_SIGN_SFT	29

#define BNXT_GRC_REG_STATUS_P5			0x520

#define BNXT_GRCPF_REG_KONG_COMM		0xA00
#define BNXT_GRCPF_REG_KONG_COMM_TRIGGER	0xB00

#define BNXT_CAG_REG_LEGACY_INT_STATUS		0x4014
#define BNXT_CAG_REG_BASE			0x300000

#define BNXT_GRC_REG_CHIP_NUM			0x48
#define BNXT_GRC_REG_BASE			0x260000

#define BNXT_TS_REG_TIMESYNC_TS0_LOWER		0x640180c
#define BNXT_TS_REG_TIMESYNC_TS0_UPPER		0x6401810

#define BNXT_GRC_BASE_MASK			0xfffff000
#define BNXT_GRC_OFFSET_MASK			0x00000ffc

#define NQE_CN_TYPE(type)	((type) & NQ_CN_TYPE_MASK)
#define NQE_CN_TOGGLE(type)	(((type) & NQ_CN_TOGGLE_MASK) >>        \
				 NQ_CN_TOGGLE_SFT)

#define DB_EPOCH(ring, idx)	(((idx) & (ring)->db_epoch_mask) <<       \
				 ((ring)->db_epoch_shift))

#define DB_TOGGLE(tgl)		((tgl) << DBR_TOGGLE_SFT)

#define DB_RING_IDX_CMP(ring, idx)    (((idx) & (ring)->db_ring_mask) |         \
				       DB_EPOCH(ring, idx))

#define DB_RING_IDX(ring, idx, bit)    (((idx) & (ring)->db_ring_mask) | \
                                       ((bit) << (24)))

struct bnxt_softc {
	device_t	dev;
	if_ctx_t	ctx;
	if_softc_ctx_t	scctx;
	if_shared_ctx_t	sctx;
	if_t ifp;
	uint32_t	domain;
	uint32_t	bus;
	uint32_t	slot;
	uint32_t	function;
	uint32_t	dev_fn;
	struct ifmedia	*media;
	struct bnxt_ctx_mem_info *ctx_mem;
	struct bnxt_hw_resc hw_resc;
	struct bnxt_softc_list list;

	struct bnxt_bar_info	hwrm_bar;
	struct bnxt_bar_info	doorbell_bar;
	struct bnxt_link_info	link_info;
#define BNXT_FLAG_VF				0x0001
#define BNXT_FLAG_NPAR				0x0002
#define BNXT_FLAG_WOL_CAP			0x0004
#define BNXT_FLAG_SHORT_CMD			0x0008
#define BNXT_FLAG_FW_CAP_NEW_RM			0x0010
#define BNXT_FLAG_CHIP_P5			0x0020
#define BNXT_FLAG_TPA				0x0040
#define BNXT_FLAG_FW_CAP_EXT_STATS		0x0080
#define BNXT_FLAG_MULTI_HOST			0x0100
#define BNXT_FLAG_MULTI_ROOT			0x0200
#define BNXT_FLAG_ROCEV1_CAP			0x0400
#define BNXT_FLAG_ROCEV2_CAP			0x0800
#define BNXT_FLAG_ROCE_CAP			(BNXT_FLAG_ROCEV1_CAP | BNXT_FLAG_ROCEV2_CAP)
#define BNXT_FLAG_CHIP_P7			0x1000
	uint32_t		flags;
#define BNXT_STATE_LINK_CHANGE  (0)
#define BNXT_STATE_MAX		(BNXT_STATE_LINK_CHANGE + 1)
	bitstr_t 		*state_bv;

	uint32_t		total_irqs;
	struct bnxt_msix_tbl	*irq_tbl;

	struct bnxt_func_info	func;
	struct bnxt_func_qcfg	fn_qcfg;
	struct bnxt_pf_info	pf;
	struct bnxt_vf_info	vf;

	uint16_t		hwrm_cmd_seq;
	uint32_t		hwrm_cmd_timeo;	/* milliseconds */
	struct iflib_dma_info	hwrm_cmd_resp;
	struct iflib_dma_info	hwrm_short_cmd_req_addr;
	/* Interrupt info for HWRM */
	struct if_irq		irq;
	struct mtx		hwrm_lock;
	uint16_t		hwrm_max_req_len;
	uint16_t		hwrm_max_ext_req_len;
	uint32_t		hwrm_spec_code;

#define BNXT_MAX_QUEUE	8
	uint8_t			max_tc;
	uint8_t			max_lltc;
	struct bnxt_queue_info  tx_q_info[BNXT_MAX_QUEUE];
	struct bnxt_queue_info  rx_q_info[BNXT_MAX_QUEUE];
	uint8_t			tc_to_qidx[BNXT_MAX_QUEUE];
	uint8_t			tx_q_ids[BNXT_MAX_QUEUE];
	uint8_t			rx_q_ids[BNXT_MAX_QUEUE];
	uint8_t			tx_max_q;
	uint8_t			rx_max_q;
	uint8_t			is_asym_q;

	struct bnxt_ieee_ets	*ieee_ets;
	struct bnxt_ieee_pfc    *ieee_pfc;
	uint8_t			dcbx_cap;
	uint8_t			default_pri;
	uint8_t			max_dscp_value;

	uint64_t		admin_ticks;
	struct iflib_dma_info	hw_rx_port_stats;
	struct iflib_dma_info	hw_tx_port_stats;
	struct rx_port_stats	*rx_port_stats;
	struct tx_port_stats	*tx_port_stats;

	struct iflib_dma_info	hw_tx_port_stats_ext;
	struct iflib_dma_info	hw_rx_port_stats_ext;
	struct tx_port_stats_ext *tx_port_stats_ext;
	struct rx_port_stats_ext *rx_port_stats_ext;

	uint16_t		fw_rx_stats_ext_size;
	uint16_t		fw_tx_stats_ext_size;
	uint16_t		hw_ring_stats_size;

	uint8_t			tx_pri2cos_idx[8];
	uint8_t			rx_pri2cos_idx[8];
	bool			pri2cos_valid;

	uint64_t		tx_bytes_pri[8];
	uint64_t		tx_packets_pri[8];
	uint64_t		rx_bytes_pri[8];
	uint64_t		rx_packets_pri[8];

	uint8_t			port_count;
	int			num_cp_rings;

	struct bnxt_cp_ring	*nq_rings;

	struct bnxt_ring	*tx_rings;
	struct bnxt_cp_ring	*tx_cp_rings;
	struct iflib_dma_info	tx_stats[BNXT_MAX_NUM_QUEUES];
	int			ntxqsets;

	struct bnxt_vnic_info	vnic_info;
	struct bnxt_ring	*ag_rings;
	struct bnxt_ring	*rx_rings;
	struct bnxt_cp_ring	*rx_cp_rings;
	struct bnxt_grp_info	*grp_info;
	struct iflib_dma_info	rx_stats[BNXT_MAX_NUM_QUEUES];
	int			nrxqsets;
	uint16_t		rx_buf_size;

	struct bnxt_cp_ring	def_cp_ring;
	struct bnxt_cp_ring	def_nq_ring;
	struct iflib_dma_info	def_cp_ring_mem;
	struct iflib_dma_info	def_nq_ring_mem;
	struct task		def_cp_task;
	int			db_size;
	int			legacy_db_size;
	struct bnxt_doorbell_ops db_ops;

	struct sysctl_ctx_list	hw_stats;
	struct sysctl_oid	*hw_stats_oid;
	struct sysctl_ctx_list	hw_lro_ctx;
	struct sysctl_oid	*hw_lro_oid;
	struct sysctl_ctx_list	flow_ctrl_ctx;
	struct sysctl_oid	*flow_ctrl_oid;
	struct sysctl_ctx_list	dcb_ctx;
	struct sysctl_oid	*dcb_oid;

	struct bnxt_ver_info	*ver_info;
	struct bnxt_nvram_info	*nvm_info;
	bool wol;
	bool is_dev_init;
	struct bnxt_hw_lro	hw_lro;
	uint8_t wol_filter_id;
	uint16_t		rx_coal_usecs;
	uint16_t		rx_coal_usecs_irq;
	uint16_t               	rx_coal_frames;
	uint16_t               	rx_coal_frames_irq;
	uint16_t               	tx_coal_usecs;
	uint16_t               	tx_coal_usecs_irq;
	uint16_t               	tx_coal_frames;
	uint16_t		tx_coal_frames_irq;

#define BNXT_USEC_TO_COAL_TIMER(x)      ((x) * 25 / 2)
#define BNXT_DEF_STATS_COAL_TICKS        1000000
#define BNXT_MIN_STATS_COAL_TICKS         250000
#define BNXT_MAX_STATS_COAL_TICKS        1000000

	uint64_t		fw_cap;
	#define BNXT_FW_CAP_SHORT_CMD			BIT_ULL(0)
	#define BNXT_FW_CAP_LLDP_AGENT			BIT_ULL(1)
	#define BNXT_FW_CAP_DCBX_AGENT			BIT_ULL(2)
	#define BNXT_FW_CAP_NEW_RM			BIT_ULL(3)
	#define BNXT_FW_CAP_IF_CHANGE			BIT_ULL(4)
	#define BNXT_FW_CAP_LINK_ADMIN			BIT_ULL(5)
	#define BNXT_FW_CAP_VF_RES_MIN_GUARANTEED	BIT_ULL(6)
	#define BNXT_FW_CAP_KONG_MB_CHNL		BIT_ULL(7)
	#define BNXT_FW_CAP_ADMIN_MTU			BIT_ULL(8)
	#define BNXT_FW_CAP_ADMIN_PF			BIT_ULL(9)
	#define BNXT_FW_CAP_OVS_64BIT_HANDLE		BIT_ULL(10)
	#define BNXT_FW_CAP_TRUSTED_VF			BIT_ULL(11)
	#define BNXT_FW_CAP_VF_VNIC_NOTIFY		BIT_ULL(12)
	#define BNXT_FW_CAP_ERROR_RECOVERY		BIT_ULL(13)
	#define BNXT_FW_CAP_PKG_VER			BIT_ULL(14)
	#define BNXT_FW_CAP_CFA_ADV_FLOW		BIT_ULL(15)
	#define BNXT_FW_CAP_CFA_RFS_RING_TBL_IDX_V2	BIT_ULL(16)
	#define BNXT_FW_CAP_PCIE_STATS_SUPPORTED	BIT_ULL(17)
	#define BNXT_FW_CAP_EXT_STATS_SUPPORTED		BIT_ULL(18)
	#define BNXT_FW_CAP_SECURE_MODE			BIT_ULL(19)
	#define BNXT_FW_CAP_ERR_RECOVER_RELOAD		BIT_ULL(20)
	#define BNXT_FW_CAP_HOT_RESET			BIT_ULL(21)
	#define BNXT_FW_CAP_CRASHDUMP			BIT_ULL(23)
	#define BNXT_FW_CAP_VLAN_RX_STRIP		BIT_ULL(24)
	#define BNXT_FW_CAP_VLAN_TX_INSERT		BIT_ULL(25)
	#define BNXT_FW_CAP_EXT_HW_STATS_SUPPORTED	BIT_ULL(26)
	#define BNXT_FW_CAP_CFA_EEM			BIT_ULL(27)
	#define BNXT_FW_CAP_DBG_QCAPS			BIT_ULL(29)
	#define BNXT_FW_CAP_RING_MONITOR		BIT_ULL(30)
	#define BNXT_FW_CAP_ECN_STATS			BIT_ULL(31)
	#define BNXT_FW_CAP_TRUFLOW			BIT_ULL(32)
	#define BNXT_FW_CAP_VF_CFG_FOR_PF		BIT_ULL(33)
	#define BNXT_FW_CAP_PTP_PPS			BIT_ULL(34)
	#define BNXT_FW_CAP_HOT_RESET_IF		BIT_ULL(35)
	#define BNXT_FW_CAP_LIVEPATCH			BIT_ULL(36)
	#define BNXT_FW_CAP_NPAR_1_2			BIT_ULL(37)
	#define BNXT_FW_CAP_RSS_HASH_TYPE_DELTA		BIT_ULL(38)
	#define BNXT_FW_CAP_PTP_RTC			BIT_ULL(39)
	#define	BNXT_FW_CAP_TRUFLOW_EN			BIT_ULL(40)
	#define BNXT_TRUFLOW_EN(bp)	((bp)->fw_cap & BNXT_FW_CAP_TRUFLOW_EN)
	#define BNXT_FW_CAP_RX_ALL_PKT_TS		BIT_ULL(41)
	#define BNXT_FW_CAP_BACKING_STORE_V2		BIT_ULL(42)
	#define BNXT_FW_CAP_DBR_SUPPORTED		BIT_ULL(43)
	#define BNXT_FW_CAP_GENERIC_STATS		BIT_ULL(44)
	#define BNXT_FW_CAP_DBR_PACING_SUPPORTED	BIT_ULL(45)
	#define BNXT_FW_CAP_PTP_PTM			BIT_ULL(46)
	#define BNXT_FW_CAP_CFA_NTUPLE_RX_EXT_IP_PROTO	BIT_ULL(47)
	#define BNXT_FW_CAP_ENABLE_RDMA_SRIOV		BIT_ULL(48)
	#define BNXT_FW_CAP_RSS_TCAM			BIT_ULL(49)
	uint32_t		lpi_tmr_lo;
	uint32_t		lpi_tmr_hi;
	/* copied from flags and flags2 in hwrm_port_phy_qcaps_output */
	uint16_t		phy_flags;
#define BNXT_PHY_FL_EEE_CAP             HWRM_PORT_PHY_QCAPS_OUTPUT_FLAGS_EEE_SUPPORTED
#define BNXT_PHY_FL_EXT_LPBK            HWRM_PORT_PHY_QCAPS_OUTPUT_FLAGS_EXTERNAL_LPBK_SUPPORTED
#define BNXT_PHY_FL_AN_PHY_LPBK         HWRM_PORT_PHY_QCAPS_OUTPUT_FLAGS_AUTONEG_LPBK_SUPPORTED
#define BNXT_PHY_FL_SHARED_PORT_CFG     HWRM_PORT_PHY_QCAPS_OUTPUT_FLAGS_SHARED_PHY_CFG_SUPPORTED
#define BNXT_PHY_FL_PORT_STATS_NO_RESET HWRM_PORT_PHY_QCAPS_OUTPUT_FLAGS_CUMULATIVE_COUNTERS_ON_RESET
#define BNXT_PHY_FL_NO_PHY_LPBK         HWRM_PORT_PHY_QCAPS_OUTPUT_FLAGS_LOCAL_LPBK_NOT_SUPPORTED
#define BNXT_PHY_FL_FW_MANAGED_LKDN     HWRM_PORT_PHY_QCAPS_OUTPUT_FLAGS_FW_MANAGED_LINK_DOWN
#define BNXT_PHY_FL_NO_FCS              HWRM_PORT_PHY_QCAPS_OUTPUT_FLAGS_NO_FCS
#define BNXT_PHY_FL_NO_PAUSE            (HWRM_PORT_PHY_QCAPS_OUTPUT_FLAGS2_PAUSE_UNSUPPORTED << 8)
#define BNXT_PHY_FL_NO_PFC              (HWRM_PORT_PHY_QCAPS_OUTPUT_FLAGS2_PFC_UNSUPPORTED << 8)
#define BNXT_PHY_FL_BANK_SEL            (HWRM_PORT_PHY_QCAPS_OUTPUT_FLAGS2_BANK_ADDR_SUPPORTED << 8)
	struct bnxt_aux_dev     *aux_dev;
	struct net_device	*net_dev;
	struct mtx		en_ops_lock;
	uint8_t			port_partition_type;
	struct bnxt_en_dev	*edev;
	unsigned long		state;
#define BNXT_STATE_OPEN			0
#define BNXT_STATE_IN_SP_TASK		1
#define BNXT_STATE_READ_STATS		2
#define BNXT_STATE_FW_RESET_DET 	3
#define BNXT_STATE_IN_FW_RESET		4
#define BNXT_STATE_ABORT_ERR		5
#define BNXT_STATE_FW_FATAL_COND	6
#define BNXT_STATE_DRV_REGISTERED	7
#define BNXT_STATE_PCI_CHANNEL_IO_FROZEN	8
#define BNXT_STATE_NAPI_DISABLED	9
#define BNXT_STATE_L2_FILTER_RETRY	10
#define BNXT_STATE_FW_ACTIVATE		11
#define BNXT_STATE_RECOVER		12
#define BNXT_STATE_FW_NON_FATAL_COND	13
#define BNXT_STATE_FW_ACTIVATE_RESET	14
#define BNXT_STATE_HALF_OPEN		15
#define BNXT_NO_FW_ACCESS(bp)		\
	test_bit(BNXT_STATE_FW_FATAL_COND, &(bp)->state)
	struct pci_dev			*pdev;

	struct work_struct	sp_task;
	unsigned long		sp_event;
#define BNXT_RX_MASK_SP_EVENT		0
#define BNXT_RX_NTP_FLTR_SP_EVENT	1
#define BNXT_LINK_CHNG_SP_EVENT		2
#define BNXT_HWRM_EXEC_FWD_REQ_SP_EVENT	3
#define BNXT_VXLAN_ADD_PORT_SP_EVENT	4
#define BNXT_VXLAN_DEL_PORT_SP_EVENT	5
#define BNXT_RESET_TASK_SP_EVENT	6
#define BNXT_RST_RING_SP_EVENT		7
#define BNXT_HWRM_PF_UNLOAD_SP_EVENT	8
#define BNXT_PERIODIC_STATS_SP_EVENT	9
#define BNXT_HWRM_PORT_MODULE_SP_EVENT	10
#define BNXT_RESET_TASK_SILENT_SP_EVENT	11
#define BNXT_GENEVE_ADD_PORT_SP_EVENT	12
#define BNXT_GENEVE_DEL_PORT_SP_EVENT	13
#define BNXT_LINK_SPEED_CHNG_SP_EVENT	14
#define BNXT_FLOW_STATS_SP_EVENT	15
#define BNXT_UPDATE_PHY_SP_EVENT	16
#define BNXT_RING_COAL_NOW_SP_EVENT	17
#define BNXT_FW_RESET_NOTIFY_SP_EVENT	18
#define BNXT_FW_EXCEPTION_SP_EVENT	19
#define BNXT_VF_VNIC_CHANGE_SP_EVENT	20
#define BNXT_LINK_CFG_CHANGE_SP_EVENT	21
#define BNXT_PTP_CURRENT_TIME_EVENT	22
#define BNXT_FW_ECHO_REQUEST_SP_EVENT	23
#define BNXT_VF_CFG_CHNG_SP_EVENT	24

	struct delayed_work	fw_reset_task;
	int			fw_reset_state;
#define BNXT_FW_RESET_STATE_POLL_VF	1
#define BNXT_FW_RESET_STATE_RESET_FW	2
#define BNXT_FW_RESET_STATE_ENABLE_DEV	3
#define BNXT_FW_RESET_STATE_POLL_FW	4
#define BNXT_FW_RESET_STATE_OPENING	5
#define BNXT_FW_RESET_STATE_POLL_FW_DOWN	6
	u16			fw_reset_min_dsecs;
#define BNXT_DFLT_FW_RST_MIN_DSECS	20
	u16			fw_reset_max_dsecs;
#define BNXT_DFLT_FW_RST_MAX_DSECS	60
	unsigned long		fw_reset_timestamp;

	struct bnxt_fw_health	*fw_health;
};

struct bnxt_filter_info {
	STAILQ_ENTRY(bnxt_filter_info) next;
	uint64_t	fw_l2_filter_id;
#define INVALID_MAC_INDEX ((uint16_t)-1)
	uint16_t	mac_index;

	/* Filter Characteristics */
	uint32_t	flags;
	uint32_t	enables;
	uint8_t		l2_addr[ETHER_ADDR_LEN];
	uint8_t		l2_addr_mask[ETHER_ADDR_LEN];
	uint16_t	l2_ovlan;
	uint16_t	l2_ovlan_mask;
	uint16_t	l2_ivlan;
	uint16_t	l2_ivlan_mask;
	uint8_t		t_l2_addr[ETHER_ADDR_LEN];
	uint8_t		t_l2_addr_mask[ETHER_ADDR_LEN];
	uint16_t	t_l2_ovlan;
	uint16_t	t_l2_ovlan_mask;
	uint16_t	t_l2_ivlan;
	uint16_t	t_l2_ivlan_mask;
	uint8_t		tunnel_type;
	uint16_t	mirror_vnic_id;
	uint32_t	vni;
	uint8_t		pri_hint;
	uint64_t	l2_filter_id_hint;
};

#define I2C_DEV_ADDR_A0                 0xa0
#define BNXT_MAX_PHY_I2C_RESP_SIZE      64

/* Function declarations */
void bnxt_report_link(struct bnxt_softc *softc);
bool bnxt_check_hwrm_version(struct bnxt_softc *softc);
struct bnxt_softc *bnxt_find_dev(uint32_t domain, uint32_t bus, uint32_t dev_fn, char *name);
int bnxt_read_sfp_module_eeprom_info(struct bnxt_softc *bp, uint16_t i2c_addr,
    uint16_t page_number, uint8_t bank, bool bank_sel_en, uint16_t start_addr,
    uint16_t data_length, uint8_t *buf);
void bnxt_dcb_init(struct bnxt_softc *softc);
void bnxt_dcb_free(struct bnxt_softc *softc);
uint8_t bnxt_dcb_setdcbx(struct bnxt_softc *softc, uint8_t mode);
uint8_t bnxt_dcb_getdcbx(struct bnxt_softc *softc);
int bnxt_dcb_ieee_getets(struct bnxt_softc *softc, struct bnxt_ieee_ets *ets);
int bnxt_dcb_ieee_setets(struct bnxt_softc *softc, struct bnxt_ieee_ets *ets);
uint8_t get_phy_type(struct bnxt_softc *softc);
int bnxt_dcb_ieee_getpfc(struct bnxt_softc *softc, struct bnxt_ieee_pfc *pfc);
int bnxt_dcb_ieee_setpfc(struct bnxt_softc *softc, struct bnxt_ieee_pfc *pfc);
int bnxt_dcb_ieee_setapp(struct bnxt_softc *softc, struct bnxt_dcb_app *app);
int bnxt_dcb_ieee_delapp(struct bnxt_softc *softc, struct bnxt_dcb_app *app);
int bnxt_dcb_ieee_listapp(struct bnxt_softc *softc, struct bnxt_dcb_app *app,
    size_t nitems, int *num_inputs);

#endif /* _BNXT_H */
