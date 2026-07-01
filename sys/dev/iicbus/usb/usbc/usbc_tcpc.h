/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kyle Crenshaw
 * All rights reserved.
 */

#ifndef _USBC_TCPC_H_
#define _USBC_TCPC_H_

#include <sys/param.h>
#include <sys/types.h>
#include <sys/bus.h>

#include <dev/iicbus/usb/usbc/usbc_pd.h>
#include <dev/iicbus/usb/usbc/usbc_pd_msg.h>

/*
 * Type-C Port Controller (TCPC) abstraction.  This is the interface
 * between a chip driver (FUSB302, ITE TCPCI, etc.) and the
 * hardware-independent PD policy state machine.
 *
 * A chip driver implements struct usbc_tcpc_ops and registers itself
 * with the policy layer; the policy calls into the ops to drive the
 * physical port.  The ops mirror USB-PD R3.2 chapter 4 (TCPM/TCPC
 * functional split) and intentionally avoid chip-specific naming.
 */

struct usbc_tcpc;

/* CC line state as observed by the TCPC */
enum usbc_tcpc_cc_state {
	USBC_TCPC_CC_OPEN	= 0,
	USBC_TCPC_CC_RA		= 1,	/* powered cable / accessory */
	USBC_TCPC_CC_RD		= 2,	/* sink termination present */
	USBC_TCPC_CC_RP_USB	= 3,	/* source advertising USB default */
	USBC_TCPC_CC_RP_1_5A	= 4,
	USBC_TCPC_CC_RP_3_0A	= 5,
};

/* TCPC power role configuration */
enum usbc_tcpc_role {
	USBC_TCPC_ROLE_SNK	= 0,	/* sink-only (Rd on both CC) */
	USBC_TCPC_ROLE_SRC	= 1,	/* source-only (Rp on both CC) */
	USBC_TCPC_ROLE_DRP	= 2,	/* dual-role, toggles between */
};

/* Events the TCPC raises to the policy layer */
enum usbc_tcpc_event {
	USBC_TCPC_EV_CC_CHANGE	= 1u << 0,
	USBC_TCPC_EV_VBUS_CHANGE	= 1u << 1,
	USBC_TCPC_EV_RX		= 1u << 2,
	USBC_TCPC_EV_TX_OK	= 1u << 3,
	USBC_TCPC_EV_TX_FAIL	= 1u << 4,
	USBC_TCPC_EV_HARD_RESET	= 1u << 5,
};

/*
 * Operations a TCPC chip driver must implement.  Keep these small,
 * synchronous, and side-effect free where possible — the policy
 * state machine calls them from its event handler.
 */
struct usbc_tcpc_ops {
	/* Configure source-side Rp current advertisement */
	int	(*set_rp)(struct usbc_tcpc *, enum usbc_pd_rp_value);

	/* Configure port role: src/snk/drp */
	int	(*set_role)(struct usbc_tcpc *, enum usbc_tcpc_role);

	/* Pin the active CC orientation (called once attach is debounced) */
	int	(*set_polarity)(struct usbc_tcpc *, enum usbc_pd_polarity);

	/* Enable/disable VCONN sourcing on the inactive CC pin */
	int	(*set_vconn)(struct usbc_tcpc *, bool enable);

	/* Enable/disable PD message reception on the active CC */
	int	(*set_rx_enable)(struct usbc_tcpc *, bool enable);

	/* Read most-recent CC1/CC2 state */
	int	(*get_cc)(struct usbc_tcpc *,
		    enum usbc_tcpc_cc_state *cc1,
		    enum usbc_tcpc_cc_state *cc2);

	/* Read whether VBUS is present (vSafe5V or above) */
	int	(*get_vbus_present)(struct usbc_tcpc *, bool *present);

	/* Enable/disable VBUS source output */
	int	(*set_vbus_source)(struct usbc_tcpc *, bool enable);

	/*
	 * Transmit a single PD message.  The chip's hardware handles BMC
	 * encoding, CRC32, retry up to N_RETRIES, and GoodCRC reception.
	 * Returns 0 on successful enqueue; the chip will subsequently
	 * raise USBC_TCPC_EV_TX_OK or USBC_TCPC_EV_TX_FAIL.
	 */
	int	(*transmit)(struct usbc_tcpc *, enum usbc_pd_sop sop,
		    const struct usbc_pd_msg *msg);

	/*
	 * Receive the next queued PD message.  Returns 0 and fills msg on
	 * success, ENOENT if the RX queue is empty, or an errno on bus
	 * error.
	 */
	int	(*receive)(struct usbc_tcpc *, enum usbc_pd_sop *sop,
		    struct usbc_pd_msg *msg);

	/* Issue a hard-reset transmission */
	int	(*send_hard_reset)(struct usbc_tcpc *);
};

/*
 * Chip drivers initialize this structure and pass it to the policy
 * layer when registering.  Policy code keeps a pointer back into the
 * chip via softc and uses ops to drive everything.
 */
struct usbc_tcpc {
	device_t			dev;
	const struct usbc_tcpc_ops	*ops;
	void				*softc;	/* opaque chip-private state */
};

/*
 * Helpers for chip drivers to raise events into the policy layer.
 * Implementation lives in usbc_pd_policy.c (when that file is added).
 */
void	usbc_tcpc_raise_event(struct usbc_tcpc *, uint32_t event_mask);

#endif /* !_USBC_TCPC_H_ */
