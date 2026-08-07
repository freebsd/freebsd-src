/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kyle Crenshaw
 * All rights reserved.
 */

#ifndef _USBC_PD_MSG_H_
#define _USBC_PD_MSG_H_

#include <dev/iicbus/usb/usbc/usbc_pd.h>

/*
 * Hardware-independent helpers for building and parsing PD messages.
 * The TCPC chip driver puts the resulting header + payload into its
 * TX FIFO using whatever framing the chip requires (BMC tokens for
 * FUSB302, or a raw packet for ITE TCPCI-class chips).
 */

struct usbc_pd_msg {
	uint16_t	hdr;
	uint8_t		ndo;		/* number of data objects, 0..7 */
	uint32_t	data[7];
};

/*
 * Build a PD message header (no data objects).
 *
 * type:	control or data message type (USBC_PD_CTRL_* / USBC_PD_DATA_*)
 * ndo:		number of 32-bit data objects (0 for control messages)
 * msg_id:	rolling 3-bit MessageID counter, caller-managed
 * power_role:	source/sink (USBC_PD_ROLE_*)
 * data_role:	UFP/DFP (USBC_PD_DATA_*)
 * rev:		PD revision (USBC_PD_REV_*)
 */
uint16_t	usbc_pd_build_header(uint8_t type, uint8_t ndo, uint8_t msg_id,
		    enum usbc_pd_power_role power_role,
		    enum usbc_pd_data_role data_role,
		    enum usbc_pd_rev rev);

/*
 * Build a Fixed Supply Source PDO at the given voltage and max current.
 * Caller selects flags from USBC_PD_PDO_FIXED_* (USB_COMMS, DUAL_ROLE,
 * UNCHUNKED_EXT, etc) and ORs them into the result.
 */
uint32_t	usbc_pd_build_fixed_pdo(uint16_t voltage_mv,
		    uint16_t max_current_ma, uint32_t flags);

/*
 * Build a Fixed Supply Sink Request data object (RDO).
 *
 * pdo_index:	1-based index of the source PDO being selected
 * op_current_ma:	operating current the sink will draw
 * max_current_ma:	maximum current the sink may draw
 * flags:	bitmap of additional RDO flags (giveback, capability mismatch,
 *		USB comms, no suspend, etc — see PD 6.4.2)
 */
#define	USBC_PD_RDO_GIVEBACK		(1u << 27)
#define	USBC_PD_RDO_CAP_MISMATCH	(1u << 26)
#define	USBC_PD_RDO_USB_COMMS_CAPABLE	(1u << 25)
#define	USBC_PD_RDO_NO_USB_SUSPEND	(1u << 24)
#define	USBC_PD_RDO_UNCHUNKED_EXT	(1u << 23)

uint32_t	usbc_pd_build_request(uint8_t pdo_index, uint16_t op_current_ma,
		    uint16_t max_current_ma, uint32_t flags);

/*
 * Convenience: build a complete control message (header only, no data).
 * Returns true on success.
 */
bool		usbc_pd_msg_ctrl(struct usbc_pd_msg *msg, uint8_t ctrl_type,
		    uint8_t msg_id, enum usbc_pd_power_role pr,
		    enum usbc_pd_data_role dr, enum usbc_pd_rev rev);

/*
 * Convenience: build a complete data message with the given PDO/RDO/etc
 * payload.  Returns true if the payload fits in 7 data objects.
 */
bool		usbc_pd_msg_data(struct usbc_pd_msg *msg, uint8_t data_type,
		    const uint32_t *data, uint8_t ndo, uint8_t msg_id,
		    enum usbc_pd_power_role pr, enum usbc_pd_data_role dr,
		    enum usbc_pd_rev rev);

/*
 * Vendor Defined Message (VDM) — PD 6.4.4.
 *
 * A VDM is a Data Message of type USBC_PD_DATA_VENDOR with the first
 * data object being the VDM Header.  Layout (32 bits):
 *
 *   31:16  SVID                    Standard or Vendor ID
 *   15     Type                    1 = Structured VDM, 0 = Unstructured
 *   14:13  Structured VDM Version  0=1.0, 1=2.0, 2=2.1
 *   12:11  reserved
 *   10:8   Object Position         1..7 (used by Enter/Exit Mode)
 *    7:6   Command Type            0=REQ, 1=ACK, 2=NAK, 3=BUSY
 *    5     reserved
 *    4:0   Command                 1..6 generic + 16..31 SVID-specific
 */
#define	USBC_VDM_HDR_SVID(h)		(((h) >> 16) & 0xffff)
#define	USBC_VDM_HDR_STRUCTURED		(1u << 15)
#define	USBC_VDM_HDR_VER_SHIFT		13
#define	USBC_VDM_HDR_OBJPOS_SHIFT	8
#define	USBC_VDM_HDR_OBJPOS_MASK	(0x7u << 8)
#define	USBC_VDM_HDR_GET_OBJPOS(h)	(((h) >> 8) & 0x7u)
#define	USBC_VDM_HDR_CMDTYPE_SHIFT	6
#define	USBC_VDM_HDR_CMDTYPE_MASK	(0x3u << 6)
#define	USBC_VDM_HDR_GET_CMDTYPE(h)	(((h) >> 6) & 0x3u)
#define	USBC_VDM_HDR_GET_CMD(h)		((h) & 0x1fu)

/* Command Type values */
#define	USBC_VDM_CMDTYPE_REQ		0u
#define	USBC_VDM_CMDTYPE_ACK		1u
#define	USBC_VDM_CMDTYPE_NAK		2u
#define	USBC_VDM_CMDTYPE_BUSY		3u

/* Generic Structured VDM commands (PD 6.4.4.2) */
#define	USBC_VDM_CMD_DISCOVER_IDENTITY	1u
#define	USBC_VDM_CMD_DISCOVER_SVIDS	2u
#define	USBC_VDM_CMD_DISCOVER_MODES	3u
#define	USBC_VDM_CMD_ENTER_MODE		4u
#define	USBC_VDM_CMD_EXIT_MODE		5u
#define	USBC_VDM_CMD_ATTENTION		6u

/* SVIDs of interest */
#define	USBC_VDM_SVID_PD		0xff00u	/* Standard PD ID */
#define	USBC_VDM_SVID_DP		0xff01u	/* DisplayPort Alt Mode */

/* DisplayPort-specific VDM commands (under SVID 0xff01, PD DP Alt Mode 2.0) */
#define	USBC_DP_CMD_STATUS_UPDATE	0x10u
#define	USBC_DP_CMD_CONFIGURE		0x11u

/*
 * DisplayPort Status VDO bits (response to DP_Status_Update or
 * Attention with DP-payload, per DP Alt Mode 2.0 §4.2):
 *
 *   bit 0   DFP_D Connected
 *   bit 1   UFP_D Connected
 *   bit 2   Power Low
 *   bit 3   Enabled
 *   bit 4   Multi-Function Preferred
 *   bit 5   USB Configuration Request
 *   bit 6   Exit DP Mode Request
 *   bit 7   HPD State
 *   bit 8   IRQ_HPD
 */
#define	USBC_DP_STATUS_DFP_D_CONN	(1u << 0)
#define	USBC_DP_STATUS_UFP_D_CONN	(1u << 1)
#define	USBC_DP_STATUS_POWER_LOW	(1u << 2)
#define	USBC_DP_STATUS_ENABLED		(1u << 3)
#define	USBC_DP_STATUS_MF_PREF		(1u << 4)
#define	USBC_DP_STATUS_USB_REQ		(1u << 5)
#define	USBC_DP_STATUS_EXIT_DP		(1u << 6)
#define	USBC_DP_STATUS_HPD		(1u << 7)
#define	USBC_DP_STATUS_IRQ_HPD		(1u << 8)

/* Build a Structured VDM header. */
uint32_t	usbc_pd_build_vdm_header(uint16_t svid, uint8_t cmd,
		    uint8_t cmd_type, uint8_t obj_pos);

/*
 * Compose a complete VDM data message into msg.  vdo_count is the
 * number of additional VDOs (after the VDM header itself) that follow
 * — Discover_Identity REQ has 0; Enter_Mode REQ has 0; DP_Configure
 * REQ has 1; Discover_Identity ACK has up to 4 (ID Header + Cert Stat
 * + Product VDO + AMA VDO).  Returns true if vdo_count <= 6 (1 header
 * + 6 VDOs = 7 data objects max in a PD message).
 */
bool		usbc_pd_msg_vdm(struct usbc_pd_msg *msg, uint16_t svid,
		    uint8_t cmd, uint8_t cmd_type, uint8_t obj_pos,
		    const uint32_t *vdos, uint8_t vdo_count, uint8_t msg_id,
		    enum usbc_pd_power_role pr, enum usbc_pd_data_role dr,
		    enum usbc_pd_rev rev);

#endif /* !_USBC_PD_MSG_H_ */
