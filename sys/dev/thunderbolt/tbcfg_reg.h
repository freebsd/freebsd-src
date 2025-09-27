/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Scott Long
 * All rights reserved.
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
 * Thunderbolt3/USB4 config space register definitions
 *
 * $FreeBSD$
 */

#ifndef _TBCFG_REG_H
#define _TBCFG_REG_H

/* Config space read request, 6.4.2.3 */
struct tb_cfg_read {
	tb_route_t			route;
	uint32_t			addr_attrs;
#define TB_CFG_ADDR_SHIFT		0
#define TB_CFG_ADDR_MASK		GENMASK(12,0)
#define TB_CFG_SIZE_SHIFT		13
#define TB_CFG_SIZE_MASK		GENMASK(18,13)
#define TB_CFG_ADAPTER_SHIFT		19
#define TB_CFG_ADAPTER_MASK		GENMASK(24,19)
#define TB_CFG_CS_PATH			(0x00 << 25)
#define TB_CFG_CS_ADAPTER		(0x01 << 25)
#define TB_CFG_CS_ROUTER		(0x02 << 25)
#define TB_CFG_CS_COUNTERS		(0x03 << 25)
#define TB_CFG_SEQ_SHIFT		27
#define TB_CFG_SEQ_MASK			(28,27)
	uint32_t			crc;
};

/* Config space read request, 6.4.2.4 */
struct tb_cfg_read_resp {
	tb_route_t			route;
	uint32_t			addr_attrs;
	uint32_t			data[0];	/* Up to 60 dwords */
	/* uint32_t crc is at the end */
} __packed;

/* Config space write request, 6.4.2.5 */
struct tb_cfg_write {
	tb_route_t			route;
	uint32_t			addr_attrs;
	uint32_t			data[0];	/* Up to 60 dwords */
	/* uint32_t crc is at the end */
} __packed;

/* Config space write response, 6.4.2.6 */
struct tb_cfg_write_resp {
	tb_route_t			route;
	uint32_t			addr_attrs;
	uint32_t			crc;
} __packed;

/* Config space event, 6.4.2.7 */
struct tb_cfg_notify {
	tb_route_t			route;
	uint32_t			event_adap;
#define TB_CFG_EVENT_MASK		GENMASK(7,0)
#define GET_NOTIFY_EVENT(n)		((n)->event_adap & TB_CFG_EVENT_MASK)
#define TB_CFG_ERR_CONN			0x00
#define TB_CFG_ERR_LINK			0x01
#define TB_CFG_ERR_ADDR			0x02
#define TB_CFG_ERR_ADP			0x04
#define TB_CFG_ERR_ENUM			0x08
#define TB_CFG_ERR_NUA			0x09
#define TB_CFG_ERR_LEN			0x0b
#define TB_CFG_ERR_HEC			0x0c
#define TB_CFG_ERR_FC			0x0d
#define TB_CFG_ERR_PLUG			0x0e
#define TB_CFG_ERR_LOCK			0x0f
#define TB_CFG_HP_ACK			0x07
#define TB_CFG_DP_BW			0x20
#define TB_CFG_EVENT_ADAPTER_SHIFT	8
#define TB_CFG_EVENT_ADAPTER_MASK	GENMASK(13,8)
#define GET_NOTIFY_ADAPTER(n)		(((n)->event_adap & \
					TB_CFG_EVENT_ADAPTER_MASK) >> \
					TB_CFG_EVENT_ADAPTER_SHIFT)
#define TB_CFG_PG_NONE			0x00000000
#define TB_CFG_PG_PLUG			0x80000000
#define TB_CFG_PG_UNPLUG		0xc0000000
	uint32_t			crc;
} __packed;

/* Config space event acknowledgement, 6.4.2.8 */
struct tb_cfg_notify_ack {
	tb_route_t			route;
	uint32_t			crc;
} __packed;

/* Config space hot plug event, 6.4.2.10 */
struct tb_cfg_hotplug {
	tb_route_t			route;
	uint32_t			adapter_attrs;
#define TB_CFG_ADPT_MASK		GENMASK(5,0)
#define TB_CFG_UPG_PLUG			(0x0 << 31)
#define TB_CFG_UPG_UNPLUG		(0x1 << 31)
	uint32_t			crc;
} __packed;

/* Config space inter-domain request, 6.4.2.11 */
struct tb_cfg_xdomain {
	tb_route_t			route;
	uint32_t			data[0];
	/* uint32_t crc is at the end */
} __packed;

/* Config space inter-domain response, 6.4.2.12 */
struct tb_cfg_xdomain_resp {
	tb_route_t			route;
	uint32_t			data[0];
	/* uint32_t crc is at the end */
} __packed;

/* Config space router basic registers 8.2.1.1 */
struct tb_cfg_router {
	uint16_t			vendor_id;	/* ROUTER_CS_0 */
	uint16_t			product_id;
	uint32_t			router_cs_1;	/* ROUTER_CS_1 */
#define ROUTER_CS1_NEXT_CAP_MASK	GENMASK(7,0)
#define GET_ROUTER_CS_NEXT_CAP(r)	(r->router_cs_1 & \
					ROUTER_CS1_NEXT_CAP_MASK)
#define ROUTER_CS1_UPSTREAM_SHIFT	8
#define ROUTER_CS1_UPSTREAM_MASK	GENMASK(13,8)
#define GET_ROUTER_CS_UPSTREAM_ADAP(r)	((r->router_cs_1 & \
					ROUTER_CS1_UPSTREAM_MASK) >> \
					ROUTER_CS1_UPSTREAM_SHIFT)
#define ROUTER_CS1_MAX_SHIFT		14
#define ROUTER_CS1_MAX_MASK		GENMASK(19,14)
#define GET_ROUTER_CS_MAX_ADAP(r)	((r->router_cs_1 & \
					ROUTER_CS1_MAX_MASK) >> \
					ROUTER_CS1_MAX_SHIFT)
#define ROUTER_CS1_MAX_ADAPTERS		64
#define ROUTER_CS1_DEPTH_SHIFT		20
#define ROUTER_CS1_DEPTH_MASK		GENMASK(22,20)
#define GET_ROUTER_CS_DEPTH(r)		((r->router_cs_1 & \
					ROUTER_CS1_DEPTH_MASK) >> \
					ROUTER_CS1_DEPTH_SHIFT)
#define ROUTER_CS1_REVISION_SHIFT	24
#define ROUTER_CS1_REVISION_MASK	GENMASK(31,24)
#define GET_ROUTER_CS_REVISION		((r->router_cs_1 & \
					ROUTER_CS1_REVISION_MASK) >> \
					ROUTER_CS1_REVISION_SHIFT)
	uint32_t			topology_lo;	/* ROUTER_CS_2 */
	uint32_t			topology_hi;	/* ROUTER_CS_3 */
#define CFG_TOPOLOGY_VALID		(1 << 31)
	uint8_t				notification_timeout; /* ROUTER_CS_4 */
	uint8_t				cm_version;
#define CFG_CM_USB4			0x10
	uint8_t				rsrvd1;
	uint8_t				usb4_version;
#define CFG_USB4_V1_0			0x10
	uint32_t			flags_cs5;	/* ROUTER_CS_5 */
#define CFG_CS5_SLP			(1 << 0)
#define CFG_CS5_WOP			(1 << 1)
#define CFG_CS5_WOU			(1 << 2)
#define CFG_CS5_DP			(1 << 3)
#define CFG_CS5_C3S			(1 << 23)
#define CFG_CS5_PTO			(1 << 24)
#define CFG_CS5_UTO			(1 << 25)
#define CFG_CS5_HCO			(1 << 26)
#define CFG_CS5_CV			(1 << 31)
	uint32_t			flags_cs6;	/* ROUTER_CS_6 */
#define CFG_CS6_SLPR			(1 << 0)
#define CFG_CS6_TNS			(1 << 1)
#define CFG_CS6_WAKE_PCIE		(1 << 2)
#define CFG_CS6_WAKE_USB3		(1 << 3)
#define CFG_CS6_WAKE_DP			(1 << 4)
#define CFG_CS6_HCI			(1 << 18)
#define CFG_CS6_RR			(1 << 24)
#define CFG_CS6_CR			(1 << 25)
	uint32_t			uuid_hi;	/* ROUTER_CS_7 */
	uint32_t			uuid_lo;	/* ROUTER_CS_8 */
	uint32_t			data[16];	/* ROUTER_CS_9-24 */
	uint32_t			metadata;	/* ROUTER_CS_25 */
	uint32_t			opcode_status;	/* ROUTER_CS_26 */
/* TBD: Opcodes and status */
#define CFG_ONS				(1 << 30)
#define CFG_OV				(1 << 31)
} __packed;

#define TB_CFG_CAP_OFFSET_MAX		0xfff

/* Config space router capability header 8.2.1.3/8.2.1.4 */
struct tb_cfg_cap_hdr {
	uint8_t				next_cap;
	uint8_t				cap_id;
} __packed;

/* Config space router TMU registers 8.2.1.2 */
struct tb_cfg_cap_tmu {
	struct tb_cfg_cap_hdr		hdr;
#define TB_CFG_CAP_TMU			0x03
} __packed;

struct tb_cfg_vsc_cap {
	struct tb_cfg_cap_hdr		hdr;
#define TB_CFG_CAP_VSC			0x05
	uint8_t				vsc_id;
	uint8_t				len;
} __packed;

struct tb_cfg_vsec_cap {
	struct tb_cfg_cap_hdr		hdr;
#define TB_CFG_CAP_VSEC			0x05
	uint8_t				vsec_id;
	uint8_t				len;
	uint16_t			vsec_next_cap;
	uint16_t			vsec_len;
} __packed;

union tb_cfg_cap {
	struct tb_cfg_cap_hdr		hdr;
	struct tb_cfg_cap_tmu		tmu;
	struct tb_cfg_vsc_cap		vsc;
	struct tb_cfg_vsec_cap		vsec;
} __packed;

#define TB_CFG_VSC_PLUG		0x01	/* Hot Plug and DROM */

#define TB_CFG_VSEC_LC		0x06	/* Link Controller */
#define TB_LC_DESC		0x02	/* LC Descriptor fields */
#define TB_LC_DESC_NUM_LC_MASK	GENMASK(3, 0)
#define TB_LC_DESC_SIZE_SHIFT	8
#define TB_LC_DESC_SIZE_MASK	GENMASK(15, 8)
#define TB_LC_DESC_PORT_SHIFT	16
#define TB_LC_DESC_PORT_MASK	GENMASK(27, 16)
#define TB_LC_UUID		0x03
#define TB_LC_DP_SINK		0x10	/* Display Port config */
#define TB_LC_PORT_ATTR		0x8d	/* Port attributes */
#define TB_LC_PORT_ATTR_BE	(1 << 12)	/* Bonding enabled */
#define TB_LC_SX_CTRL		0x96	/* Sleep control */
#define TB_LC_SX_CTRL_WOC	(1 << 1)
#define TB_LC_SX_CTRL_WOD	(1 << 2)
#define TB_LC_SX_CTRL_WOU4	(1 << 5)
#define TB_LC_SX_CTRL_WOP	(1 << 6)
#define TB_LC_SX_CTRL_L1C	(1 << 16)
#define TB_LC_SX_CTRL_L1D	(1 << 17)
#define TB_LC_SX_CTRL_L2C	(1 << 20)
#define TB_LC_SX_CTRL_L2D	(1 << 21)
#define TB_LC_SX_CTRL_UFP	(1 << 30)
#define TB_LC_SX_CTRL_SLP	(1 << 31)
#define TB_LC_POWER		0x740

/* Config space adapter basic registers 8.2.2.1 */
struct tb_cfg_adapter {
	uint16_t			vendor_id;	/* ADP CS0 */
	uint16_t			product_id;
	uint32_t			adp_cs1;	/* ADP CS1 */
#define ADP_CS1_NEXT_CAP_MASK		GENMASK(7,0)
#define GET_ADP_CS_NEXT_CAP(a)		(a->adp_cs1 & \
					ADP_CS1_NEXT_CAP_MASK)
#define ADP_CS1_COUNTER_SHIFT		8
#define ADP_CS1_COUNTER_MASK		GENMASK(18,8)
#define GET_ADP_CS_MAX_COUNTERS(a)	((a->adp_cs1 & \
					ADP_CS1_COUNTER_MASK) >> \
					ADP_CS1_COUNTER_SHIFT)
#define CFG_COUNTER_CONFIG_FLAG		(1 << 19)
	uint32_t			adp_cs2;	/* ADP CS2 */
#define ADP_CS2_TYPE_MASK		GENMASK(23,0)
#define GET_ADP_CS_TYPE(a)		(a->adp_cs2 & ADP_CS2_TYPE_MASK)
#define ADP_CS2_UNSUPPORTED		0x000000
#define ADP_CS2_LANE			0x000001
#define ADP_CS2_HOSTIF			0x000002
#define ADP_CS2_PCIE_DFP		0x100101
#define ADP_CS2_PCIE_UFP		0x100102
#define ADP_CS2_DP_OUT			0x0e0102
#define ADP_CS2_DP_IN			0x0e0101
#define ADP_CS2_USB3_DFP		0x200101
#define ADP_CS2_USB3_UFP		0x200102
	uint32_t			adp_cs3;	/* ADP CS 3 */
#define ADP_CS3_ADP_NUM_SHIFT		20
#define ADP_CS3_ADP_NUM_MASK		GENMASK(25,20)
#define GET_ADP_CS_ADP_NUM(a)		((a->adp_cs3 & \
					ADP_CS3_ADP_NUM_MASK) >> \
					ADP_CS3_ADP_NUM_SHIFT)
#define CFG_ADP_HEC_ERROR		(1 << 29)
#define CFG_ADP_FC_ERROR		(1 << 30)
#define CFG_ADP_SBC			(1 << 31)
} __packed;

/* Config space lane adapter capability 8.2.2.3 */
struct tb_cfg_cap_lane {
	struct tb_cfg_cap_hdr	hdr;		/* LANE_ADP_CS_0 */
#define TB_CFG_CAP_LANE		0x01
	/* Supported link/width/power */
	uint16_t		supp_lwp;
#define CAP_LANE_LINK_MASK	GENMASK(3,0)
#define CAP_LANE_LINK_GEN3	0x0004
#define CAP_LANE_LINK_GEN2	0x0008
#define CAP_LANE_WIDTH_MASK	GENMASK(9,4)
#define CAP_LANE_WIDTH_1X	0x0010
#define CAP_LANE_WIDTH_2X	0x0020
#define CAP_LANE_POWER_CL0	0x0400
#define CAP_LANE_POWER_CL1	0x0800
#define CAP_LANE_POWER_CL2	0x1000
	/* Target link/width/power */
	uint16_t		targ_lwp;	/* LANE_ADP_CS_1 */
#define CAP_LANE_TARGET_GEN2	0x0008
#define CAP_LANE_TARGET_GEN3	0x000c
#define CAP_LANE_TARGET_SINGLE	0x0010
#define CAP_LANE_TARGET_DUAL	0x0030
#define CAP_LANE_DISABLE	0x4000
#define CAP_LANE_BONDING	0x8000
	/* Current link/width/state */
	uint16_t		current_lws;
/* Same definitions a supp_lwp for bits 0 - 9 */
#define CAP_LANE_STATE_SHIFT	10
#define CAP_LANE_STATE_MASK	GENMASK(13,10)
#define CAP_LANE_STATE_DISABLE	(0x0 << CAP_LANE_STATE_SHIFT)
#define CAP_LANE_STATE_TRAINING	(0x1 << CAP_LANE_STATE_SHIFT)
#define CAP_LANE_STATE_CL0	(0x2 << CAP_LANE_STATE_SHIFT)
#define CAP_LANE_STATE_TXCL0	(0x3 << CAP_LANE_STATE_SHIFT)
#define CAP_LANE_STATE_RXCL0	(0x4 << CAP_LANE_STATE_SHIFT)
#define CAP_LANE_STATE_CL1	(0x5 << CAP_LANE_STATE_SHIFT)
#define CAP_LANE_STATE_CL2	(0x6 << CAP_LANE_STATE_SHIFT)
#define CAP_LANE_STATE_CLD	(0x7 << CAP_LANE_STATE_SHIFT)
#define CAP_LANE_PMS		0x4000
	/* Logical Layer Errors */
	uint16_t		lle;		/* LANE_ADP_CS_2 */
#define CAP_LANE_LLE_MASK	GENMASK(6,0)
#define CAP_LANE_LLE_ALE	0x01
#define CAP_LANE_LLE_OSE	0x02
#define CAP_LANE_LLE_TE		0x04
#define CAP_LANE_LLE_EBE	0x08
#define CAP_LANE_LLE_DBE	0x10
#define CAP_LANE_LLE_RDE	0x20
#define CAP_LANE_LLE_RST	0x40
	uint16_t		lle_enable;
} __packed;

/* Config space path registers 8.2.3.1 */
struct tb_cfg_path {
} __packed;

/* Config space counter registers 8.2.4 */
struct tb_cfg_counters {
} __packed;

#endif /* _TBCFG_REG_H */
