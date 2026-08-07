/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kyle Crenshaw
 * All rights reserved.
 */

#ifndef _USBC_PD_H_
#define _USBC_PD_H_

#include <sys/param.h>
#include <sys/types.h>

/*
 * USB Power Delivery R3.2 base types, enums, and timing constants
 * shared across the message, protocol, and policy layers.  Hardware
 * independent: this header may be included by a chip driver, by the
 * policy state machine, or by altmode helpers.
 */

/* SOP* packet types per USB PD 6.6 */
enum usbc_pd_sop {
	USBC_PD_SOP        = 0,	/* port-to-port */
	USBC_PD_SOP_PRIME  = 1,	/* port-to-cable plug (near end) */
	USBC_PD_SOP_DPRIME = 2,	/* port-to-cable plug (far end) */
	USBC_PD_SOP_DEBUG_PRIME  = 3,
	USBC_PD_SOP_DEBUG_DPRIME = 4,
};

/* Power roles per PD 6.2.1.1.4 */
enum usbc_pd_power_role {
	USBC_PD_ROLE_SINK   = 0,
	USBC_PD_ROLE_SOURCE = 1,
};

/* Data roles per PD 6.2.1.1.6 */
enum usbc_pd_data_role {
	USBC_PD_DATA_UFP = 0,
	USBC_PD_DATA_DFP = 1,
};

/* PD spec revision per PD 6.2.1.1.5 */
enum usbc_pd_rev {
	USBC_PD_REV_1_0 = 0,
	USBC_PD_REV_2_0 = 1,
	USBC_PD_REV_3_0 = 2,
	/* PD 3.1+ uses extended fields, encoded as REV_3_0 in the
	 * 2-bit message header field. */
};

/* CC line orientation as detected by the TCPC */
enum usbc_pd_polarity {
	USBC_PD_POLARITY_CC1 = 0,
	USBC_PD_POLARITY_CC2 = 1,
};

/* Source Rp current advertisement per Type-C Table 4-27 */
enum usbc_pd_rp_value {
	USBC_PD_RP_USB_DEFAULT = 0,	/* 80uA, 500/900mA */
	USBC_PD_RP_1_5A        = 1,	/* 180uA, 1.5A@5V */
	USBC_PD_RP_3_0A        = 2,	/* 330uA, 3.0A@5V */
};

/*
 * PD message header (16 bits) per PD 6.2.1
 * bit 15:    Extended (1 = extended message)
 * bit 14:12: Number of Data Objects (0..7)
 * bit 11: 9: MessageID (rolling counter)
 * bit  8:    Port Power Role / Cable Plug
 * bit  7: 6: Specification Revision
 * bit  5:    Port Data Role
 * bit  4: 0: Message Type
 */
#define	USBC_PD_HDR_EXTENDED		(1u << 15)
#define	USBC_PD_HDR_NDO_SHIFT		12
#define	USBC_PD_HDR_NDO_MASK		(0x7u << 12)
#define	USBC_PD_HDR_MSGID_SHIFT		9
#define	USBC_PD_HDR_MSGID_MASK		(0x7u << 9)
#define	USBC_PD_HDR_POWER_ROLE		(1u << 8)
#define	USBC_PD_HDR_REV_SHIFT		6
#define	USBC_PD_HDR_REV_MASK		(0x3u << 6)
#define	USBC_PD_HDR_DATA_ROLE		(1u << 5)
#define	USBC_PD_HDR_TYPE_MASK		0x1fu

#define	USBC_PD_HDR_GET_NDO(h)		(((h) & USBC_PD_HDR_NDO_MASK) >> 12)
#define	USBC_PD_HDR_GET_MSGID(h)	(((h) & USBC_PD_HDR_MSGID_MASK) >> 9)
#define	USBC_PD_HDR_GET_REV(h)		(((h) & USBC_PD_HDR_REV_MASK) >> 6)
#define	USBC_PD_HDR_GET_TYPE(h)		((h) & USBC_PD_HDR_TYPE_MASK)

/* Control message types (NDO=0) per PD 6.3 */
#define	USBC_PD_CTRL_GOOD_CRC		0x01
#define	USBC_PD_CTRL_GOTO_MIN		0x02
#define	USBC_PD_CTRL_ACCEPT		0x03
#define	USBC_PD_CTRL_REJECT		0x04
#define	USBC_PD_CTRL_PING		0x05
#define	USBC_PD_CTRL_PS_RDY		0x06
#define	USBC_PD_CTRL_GET_SRC_CAP	0x07
#define	USBC_PD_CTRL_GET_SNK_CAP	0x08
#define	USBC_PD_CTRL_DR_SWAP		0x09
#define	USBC_PD_CTRL_PR_SWAP		0x0a
#define	USBC_PD_CTRL_VCONN_SWAP		0x0b
#define	USBC_PD_CTRL_WAIT		0x0c
#define	USBC_PD_CTRL_SOFT_RESET		0x0d
#define	USBC_PD_CTRL_NOT_SUPPORTED	0x10
#define	USBC_PD_CTRL_GET_SRC_CAP_EXT	0x11
#define	USBC_PD_CTRL_GET_STATUS		0x12

/* Data message types (NDO>0) per PD 6.4 */
#define	USBC_PD_DATA_SRC_CAP		0x01
#define	USBC_PD_DATA_REQUEST		0x02
#define	USBC_PD_DATA_BIST		0x03
#define	USBC_PD_DATA_SNK_CAP		0x04
#define	USBC_PD_DATA_BATTERY_STATUS	0x05
#define	USBC_PD_DATA_ALERT		0x06
#define	USBC_PD_DATA_VENDOR		0x0f

/*
 * Power Data Object (32 bits) per PD 6.4.1
 * Fixed Supply PDO bits [31:30] = 00:
 * bit 31:30 type (00 = Fixed)
 * bit 29    Dual-Role Power
 * bit 28    USB Suspend Supported
 * bit 27    Unconstrained Power
 * bit 26    USB Communications Capable
 * bit 25    Dual-Role Data
 * bit 24    Unchunked Extended Messages Supported (PD 3.0)
 * bit 23:22 Fast Role Swap (PD 3.0)
 * bit 21:20 Reserved
 * bit 19:10 Voltage in 50mV units
 * bit  9: 0 Maximum Current in 10mA units
 */
#define	USBC_PD_PDO_TYPE_FIXED		(0u << 30)
#define	USBC_PD_PDO_TYPE_BATTERY	(1u << 30)
#define	USBC_PD_PDO_TYPE_VARIABLE	(2u << 30)
#define	USBC_PD_PDO_TYPE_AUGMENTED	(3u << 30)
#define	USBC_PD_PDO_TYPE_MASK		(3u << 30)

#define	USBC_PD_PDO_FIXED_DUAL_ROLE_PWR		(1u << 29)
#define	USBC_PD_PDO_FIXED_USB_SUSPEND		(1u << 28)
#define	USBC_PD_PDO_FIXED_UNCONSTRAINED		(1u << 27)
#define	USBC_PD_PDO_FIXED_USB_COMMS		(1u << 26)
#define	USBC_PD_PDO_FIXED_DUAL_ROLE_DATA	(1u << 25)
#define	USBC_PD_PDO_FIXED_UNCHUNKED_EXT		(1u << 24)

#define	USBC_PD_PDO_FIXED_VOLTAGE_SHIFT		10
#define	USBC_PD_PDO_FIXED_VOLTAGE_MASK		(0x3ffu << 10)
#define	USBC_PD_PDO_FIXED_CURRENT_MASK		0x3ffu

#define	USBC_PD_PDO_FIXED_VOLTAGE_MV(mv)	\
	((((uint32_t)(mv) / 50) & 0x3ffu) << USBC_PD_PDO_FIXED_VOLTAGE_SHIFT)
#define	USBC_PD_PDO_FIXED_CURRENT_MA(ma)	\
	(((uint32_t)(ma) / 10) & USBC_PD_PDO_FIXED_CURRENT_MASK)

#define	USBC_PD_PDO_FIXED_GET_VOLTAGE_MV(p)	\
	((((p) & USBC_PD_PDO_FIXED_VOLTAGE_MASK) >> 10) * 50)
#define	USBC_PD_PDO_FIXED_GET_CURRENT_MA(p)	\
	(((p) & USBC_PD_PDO_FIXED_CURRENT_MASK) * 10)

/*
 * PD timing constants per PD Table 6-68 (R3.2).  These are the
 * spec-mandated values; chip drivers and policy state machines
 * should reference these names rather than re-inventing literals.
 */
#define	USBC_PD_T_CC_DEBOUNCE_MS	100	/* 100..200 ms */
#define	USBC_PD_T_PD_DEBOUNCE_MS	15	/* 10..20  ms */
#define	USBC_PD_T_VBUS_ON_MS		275	/* tVBUSON, max */
#define	USBC_PD_T_VBUS_OFF_MS		650	/* tVBUSOFF, max */
#define	USBC_PD_T_SENDER_RESPONSE_MS	27	/* 24..30  ms */
#define	USBC_PD_T_SRC_TRANSITION_MS	30	/* 25..35  ms */
#define	USBC_PD_T_NO_RESPONSE_MS	5000	/* 4.5..5.5 s */
#define	USBC_PD_T_TYPEC_SEND_SRCCAP_MS	150	/* 100..200 ms */
#define	USBC_PD_T_TYPEC_SINK_WAIT_CAP_MS 350	/* 310..620 ms */
#define	USBC_PD_T_PS_TRANSITION_MS	500	/* 450..550 ms */
#define	USBC_PD_T_PS_HARD_RESET_MS	30	/* 25..35  ms */
#define	USBC_PD_T_SAFE_5V_MS		275	/* tSafe5V, max */
#define	USBC_PD_N_CAPS_COUNT		50
#define	USBC_PD_N_HARD_RESET_COUNT	2	/* spec nHardResetCount */

#endif /* !_USBC_PD_H_ */
