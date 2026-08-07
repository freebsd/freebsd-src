/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kyle Crenshaw
 * All rights reserved.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>

#include <dev/iicbus/usb/usbc/usbc_pd.h>
#include <dev/iicbus/usb/usbc/usbc_pd_msg.h>

/*
 * usbc_pd_build_header
 *
 * Pack a USB-PD message header (16 bits, PD spec 6.2.1) from its
 * constituent fields.  Layout written here matches the wire format
 * the BMC encoder on a TCPC chip expects: type in bits [4:0], data
 * role in bit 5, spec rev in bits [7:6], power role in bit 8,
 * MessageID in bits [11:9], and number of data objects in bits
 * [14:12].  Bit 15 (extended-message flag) is left clear; callers
 * doing extended messages would OR in USBC_PD_HDR_EXTENDED.
 *
 * The resulting uint16_t is bus-byte-order-neutral: chip drivers that
 * write it byte-by-byte to a TX FIFO must split it into LSB then MSB,
 * which is what every existing chip driver in tree already does.
 */
uint16_t
usbc_pd_build_header(uint8_t type, uint8_t ndo, uint8_t msg_id,
    enum usbc_pd_power_role power_role, enum usbc_pd_data_role data_role,
    enum usbc_pd_rev rev)
{
	uint16_t h = 0;

	h |= (type & USBC_PD_HDR_TYPE_MASK);
	h |= ((uint16_t)(ndo & 0x7) << USBC_PD_HDR_NDO_SHIFT);
	h |= ((uint16_t)(msg_id & 0x7) << USBC_PD_HDR_MSGID_SHIFT);
	h |= ((uint16_t)(rev & 0x3) << USBC_PD_HDR_REV_SHIFT);

	if (power_role == USBC_PD_ROLE_SOURCE)
		h |= USBC_PD_HDR_POWER_ROLE;
	if (data_role == USBC_PD_DATA_DFP)
		h |= USBC_PD_HDR_DATA_ROLE;

	return (h);
}

/*
 * usbc_pd_build_fixed_pdo
 *
 * Construct a Fixed Supply Power Data Object (PD spec 6.4.1.2.3).
 * Voltage is encoded in 50 mV units in bits [19:10] and max current in
 * 10 mA units in bits [9:0].  `flags` is the bitmap of fixed-supply
 * capability bits (USB_COMMS, DUAL_ROLE, UNCHUNKED_EXT, etc) ORed in
 * by the caller, since which flags to advertise is a policy decision
 * the protocol layer cannot decide on its own.
 *
 * A 5V@900mA, USB-comms-capable PDO — the minimum valid Source PDO
 * for a port that wants to claim USB host capability — is built as:
 *   usbc_pd_build_fixed_pdo(5000, 900, USBC_PD_PDO_FIXED_USB_COMMS)
 */
uint32_t
usbc_pd_build_fixed_pdo(uint16_t voltage_mv, uint16_t max_current_ma,
    uint32_t flags)
{
	uint32_t pdo;

	pdo = USBC_PD_PDO_TYPE_FIXED;
	pdo |= USBC_PD_PDO_FIXED_VOLTAGE_MV(voltage_mv);
	pdo |= USBC_PD_PDO_FIXED_CURRENT_MA(max_current_ma);
	pdo |= flags;
	return (pdo);
}

/*
 * usbc_pd_build_request
 *
 * Build a Sink Request Data Object (RDO, PD spec 6.4.2) selecting one
 * of the source's advertised PDOs.  pdo_index is 1-based per spec and
 * gets clamped into the legal [1..7] range.
 *
 * op_current_ma is what the sink intends to draw under normal
 * operation; max_current_ma is its absolute upper limit (including
 * inrush).  Both go into the RDO in 10 mA units.  Caller adds RDO
 * flags (giveback, capability mismatch, USB-comms capable, no-suspend,
 * unchunked extended messages) via the `flags` argument; we don't pick
 * those defaults here because they are policy decisions tied to the
 * sink's device profile, not the message format.
 */
uint32_t
usbc_pd_build_request(uint8_t pdo_index, uint16_t op_current_ma,
    uint16_t max_current_ma, uint32_t flags)
{
	uint32_t rdo;

	if (pdo_index < 1)
		pdo_index = 1;
	if (pdo_index > 7)
		pdo_index = 7;

	rdo = ((uint32_t)pdo_index << 28);
	rdo |= flags;
	rdo |= ((uint32_t)(op_current_ma / 10) & 0x3ff) << 10;
	rdo |= ((uint32_t)(max_current_ma / 10) & 0x3ff);
	return (rdo);
}

/*
 * usbc_pd_msg_ctrl
 *
 * Convenience constructor for a complete control message (no data
 * objects, header only).  Used by the policy layer to build messages
 * like Accept, Reject, PS_RDY, Soft_Reset, Get_Snk_Cap — anything
 * where the message type alone carries the meaning.  ndo is fixed at
 * 0 to satisfy the wire-format invariant for control messages.
 *
 * Returns false if msg is NULL; otherwise unconditionally succeeds.
 */
bool
usbc_pd_msg_ctrl(struct usbc_pd_msg *msg, uint8_t ctrl_type, uint8_t msg_id,
    enum usbc_pd_power_role pr, enum usbc_pd_data_role dr,
    enum usbc_pd_rev rev)
{
	if (msg == NULL)
		return (false);

	msg->hdr = usbc_pd_build_header(ctrl_type, 0, msg_id, pr, dr, rev);
	msg->ndo = 0;
	return (true);
}

/*
 * usbc_pd_msg_data
 *
 * Convenience constructor for a complete data message: header + 1..7
 * 32-bit data objects.  Used for Source_Capabilities (PDOs), Request
 * (RDO), Sink_Capabilities, BIST, and Vendor messages.
 *
 * Returns false on any of: NULL msg or data, ndo == 0 (data messages
 * MUST carry at least one object), or ndo > 7 (the wire format only
 * has 3 bits for the count).  On success, copies ndo*4 bytes from
 * data[] into msg->data[] and stamps the header with the right ndo
 * field.
 */
bool
usbc_pd_msg_data(struct usbc_pd_msg *msg, uint8_t data_type,
    const uint32_t *data, uint8_t ndo, uint8_t msg_id,
    enum usbc_pd_power_role pr, enum usbc_pd_data_role dr,
    enum usbc_pd_rev rev)
{
	if (msg == NULL || ndo == 0 || ndo > 7 || data == NULL)
		return (false);

	msg->hdr = usbc_pd_build_header(data_type, ndo, msg_id, pr, dr, rev);
	msg->ndo = ndo;
	memcpy(msg->data, data, ndo * sizeof(uint32_t));
	return (true);
}

/*
 * usbc_pd_build_vdm_header
 *
 * Build the 32-bit Structured VDM header per PD 6.4.4.2.1.  Caller
 * supplies the SVID, command, command type (REQ/ACK/NAK/BUSY), and
 * object position (1..7 for Enter/Exit Mode, 0 elsewhere).  Always
 * sets the Structured VDM flag and Version=2.0.
 */
uint32_t
usbc_pd_build_vdm_header(uint16_t svid, uint8_t cmd, uint8_t cmd_type,
    uint8_t obj_pos)
{
	uint32_t h;

	h = (uint32_t)svid << 16;
	h |= USBC_VDM_HDR_STRUCTURED;
	h |= (1u << USBC_VDM_HDR_VER_SHIFT);	/* Version 2.0 */
	h |= ((uint32_t)(obj_pos & 0x7u)) << USBC_VDM_HDR_OBJPOS_SHIFT;
	h |= ((uint32_t)(cmd_type & 0x3u)) << USBC_VDM_HDR_CMDTYPE_SHIFT;
	h |= cmd & 0x1fu;
	return (h);
}

/*
 * usbc_pd_msg_vdm
 *
 * Compose a Structured VDM as a Vendor data message.  Payload is
 * [VDM_header, vdo[0], vdo[1], ...]; vdo_count is the number of VDOs
 * after the header (1 + vdo_count must fit in 7 data objects).
 */
bool
usbc_pd_msg_vdm(struct usbc_pd_msg *msg, uint16_t svid, uint8_t cmd,
    uint8_t cmd_type, uint8_t obj_pos, const uint32_t *vdos,
    uint8_t vdo_count, uint8_t msg_id, enum usbc_pd_power_role pr,
    enum usbc_pd_data_role dr, enum usbc_pd_rev rev)
{
	uint32_t payload[7];
	uint8_t i;

	if (msg == NULL || vdo_count > 6 ||
	    (vdo_count > 0 && vdos == NULL))
		return (false);

	payload[0] = usbc_pd_build_vdm_header(svid, cmd, cmd_type, obj_pos);
	for (i = 0; i < vdo_count; i++)
		payload[i + 1] = vdos[i];

	return (usbc_pd_msg_data(msg, USBC_PD_DATA_VENDOR, payload,
	    1 + vdo_count, msg_id, pr, dr, rev));
}
