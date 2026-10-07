/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kyle Crenshaw
 * All rights reserved.
 */

#ifndef _USBC_PD_POLICY_H_
#define _USBC_PD_POLICY_H_

#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>

#include <dev/iicbus/usb/usbc/usbc_pd.h>
#include <dev/iicbus/usb/usbc/usbc_pd_msg.h>
#include <dev/iicbus/usb/usbc/usbc_tcpc.h>

/*
 * USB-PD R3.2 policy state machine.
 *
 * Mirrors the structure of Linux's drivers/usb/typec/tcpm/tcpm.c but
 * is reimplemented in FreeBSD-native style: BSD-licensed, BSD softc,
 * native callouts and taskqueues, no linuxkpi.
 *
 * The policy machine sits between:
 *   - the TCPC chip driver (struct usbc_tcpc_ops, e.g. fusb302), which
 *     handles the byte-level CC / VBUS / message transport, AND
 *   - the higher-level altmode and class consumers (DP altmode, the
 *     /dev/typec0 cdev, etc.), which observe role/state transitions.
 *
 * It owns the per-port USB-PD spec state machine: who's source, who's
 * sink, when to send Source_Capabilities, how to handle GoodCRC /
 * Accept / Reject, when to do a hard reset, etc.
 */

struct usbc_pd_policy;
struct usbc_pd_port_caps;

/*
 * Top-level state.  Mirrors the per-section grouping in PD spec
 * chapter 8 (Type-C Port Manager State Diagrams) so cross-referencing
 * with the spec is easy.  Not every Linux state is present; this is
 * the subset we need for SRC/SNK/DRP with PD 2.0 + 3.0 + DP altmode.
 *
 * State transitions happen only inside usbc_pd_policy_run() in
 * response to events delivered via usbc_pd_policy_event().  External
 * callers never write the state field directly.
 */
enum usbc_pd_state {
	/* --- Universal --- */
	USBC_PD_S_INVALID		= 0,
	USBC_PD_S_DISABLED,			/* port-disable manual override */
	USBC_PD_S_ERROR_RECOVERY,		/* ToggleI -> attach again */
	USBC_PD_S_PORT_RESET,
	USBC_PD_S_PORT_RESET_WAIT_OFF,

	/* --- Type-C attach states --- */
	USBC_PD_S_UNATTACHED_SNK,
	USBC_PD_S_UNATTACHED_SRC,
	USBC_PD_S_ATTACH_WAIT_SNK,
	USBC_PD_S_ATTACH_WAIT_SRC,
	USBC_PD_S_TRY_SNK,
	USBC_PD_S_TRY_SRC,
	USBC_PD_S_TRY_WAIT_SNK,
	USBC_PD_S_TRY_WAIT_SRC,
	USBC_PD_S_AUDIO_ACCESSORY,
	USBC_PD_S_DEBUG_ACCESSORY,
	USBC_PD_S_ORIENTED_DEBUG_ACCESSORY_SNK,
	USBC_PD_S_ORIENTED_DEBUG_ACCESSORY_SRC,

	/* --- Source role: PD bring-up --- */
	USBC_PD_S_SRC_ATTACHED,
	USBC_PD_S_SRC_STARTUP,
	USBC_PD_S_SRC_SEND_CAPABILITIES,	/* Send Source_Capabilities */
	USBC_PD_S_SRC_SEND_CAPABILITIES_TIMEOUT,
	USBC_PD_S_SRC_NEGOTIATE_CAPABILITIES,	/* Got Request, evaluate */
	USBC_PD_S_SRC_TRANSITION_SUPPLY,	/* Send Accept, wait tSrcTransition */
	USBC_PD_S_SRC_READY,
	USBC_PD_S_SRC_WAIT_NEW_CAPABILITIES,

	/* --- Sink role: PD bring-up --- */
	USBC_PD_S_SNK_ATTACHED,
	USBC_PD_S_SNK_STARTUP,
	USBC_PD_S_SNK_DISCOVERY,
	USBC_PD_S_SNK_WAIT_CAPABILITIES,	/* Waiting for Source_Capabilities */
	USBC_PD_S_SNK_NEGOTIATE_CAPABILITIES,	/* Pick a PDO, build RDO */
	USBC_PD_S_SNK_TRANSITION_SINK,
	USBC_PD_S_SNK_TRANSITION_SINK_VBUS,
	USBC_PD_S_SNK_READY,

	/* --- Hard reset --- */
	USBC_PD_S_HARD_RESET_SEND,
	USBC_PD_S_HARD_RESET_START,
	USBC_PD_S_SRC_HARD_RESET_VBUS_OFF,
	USBC_PD_S_SRC_HARD_RESET_VBUS_ON,
	USBC_PD_S_SNK_HARD_RESET_SINK_OFF,
	USBC_PD_S_SNK_HARD_RESET_WAIT_VBUS,
	USBC_PD_S_SNK_HARD_RESET_SINK_ON,

	/* --- Soft reset --- */
	USBC_PD_S_SOFT_RESET,
	USBC_PD_S_SOFT_RESET_SEND,

	/* --- Role swaps --- */
	USBC_PD_S_DR_SWAP_SEND,
	USBC_PD_S_DR_SWAP_ACCEPT,
	USBC_PD_S_DR_SWAP_CHANGE_DR,
	USBC_PD_S_PR_SWAP_SEND,
	USBC_PD_S_PR_SWAP_ACCEPT,
	USBC_PD_S_PR_SWAP_SRC_SNK_TRANSITION_OFF,
	USBC_PD_S_PR_SWAP_SRC_SNK_SOURCE_OFF,
	USBC_PD_S_PR_SWAP_SRC_SNK_SINK_ON,
	USBC_PD_S_PR_SWAP_SNK_SRC_SINK_OFF,
	USBC_PD_S_PR_SWAP_SNK_SRC_SOURCE_ON,
	USBC_PD_S_VCONN_SWAP_SEND,
	USBC_PD_S_VCONN_SWAP_ACCEPT,
	USBC_PD_S_VCONN_SWAP_WAIT_FOR_VCONN,
	USBC_PD_S_VCONN_SWAP_TURN_ON_VCONN,
	USBC_PD_S_VCONN_SWAP_TURN_OFF_VCONN,

	/* --- VDM (Vendor Defined Message) handling --- */
	USBC_PD_S_VDM_SEND_DISCOVER_IDENTITY,
	USBC_PD_S_VDM_SEND_DISCOVER_SVIDS,
	USBC_PD_S_VDM_SEND_DISCOVER_MODES,
	USBC_PD_S_VDM_SEND_ENTER_MODE,
	USBC_PD_S_VDM_SEND_DP_STATUS,
	USBC_PD_S_VDM_SEND_DP_CONFIGURE,
	USBC_PD_S_VDM_DP_READY,
	USBC_PD_S_VDM_RESPONSE_BUSY,

	USBC_PD_S__COUNT
};

/*
 * Events delivered to the state machine.  The chip driver and timers
 * raise these via usbc_pd_policy_event(); the SM consumes them in
 * usbc_pd_policy_run().  Multiple events may fold together in one
 * run() invocation if they arrive between dispatches.
 */
enum usbc_pd_event {
	USBC_PD_E_NONE			= 0,
	USBC_PD_E_CC_CHANGE		= 1u << 0,
	USBC_PD_E_VBUS_CHANGE		= 1u << 1,
	USBC_PD_E_RX			= 1u << 2,	/* PD message available */
	USBC_PD_E_TX_OK			= 1u << 3,
	USBC_PD_E_TX_FAIL		= 1u << 4,	/* RetryFail / no GoodCRC */
	USBC_PD_E_TIMER			= 1u << 5,
	USBC_PD_E_HARD_RESET_RX		= 1u << 6,
	USBC_PD_E_HARD_RESET_TX_OK	= 1u << 7,
	USBC_PD_E_PORT_DISABLE		= 1u << 8,
	USBC_PD_E_PORT_ENABLE		= 1u << 9,
};

/*
 * Spec-named timers (PD 6.5).  Each has a duration constant in
 * usbc_pd.h; the policy machine arms one at a time and reacts to
 * USBC_PD_E_TIMER when it fires.  The chip driver does not see
 * these -- timers live entirely inside the policy module.
 */
enum usbc_pd_timer {
	USBC_PD_T_NONE = 0,
	USBC_PD_T_CC_DEBOUNCE,
	USBC_PD_T_PD_DEBOUNCE,
	USBC_PD_T_TYPEC_SEND_SOURCE_CAP,
	USBC_PD_T_SENDER_RESPONSE,
	USBC_PD_T_PS_HARD_RESET,
	USBC_PD_T_PS_SOURCE_OFF,
	USBC_PD_T_PS_SOURCE_ON,
	USBC_PD_T_PS_TRANSITION,
	USBC_PD_T_NO_RESPONSE,
	USBC_PD_T_VBUS_ON,
	USBC_PD_T_VBUS_OFF,
	USBC_PD_T_TYPEC_SINK_WAIT_CAP,
	USBC_PD_T_TYPEC_TRY,
	USBC_PD_T_VDM_SENDER_RESPONSE,
};

/*
 * Static configuration the consumer hands the policy at init.
 * Encodes role preference (SRC / SNK / DRP), what we'll advertise
 * as a source, what we accept as a sink, and feature flags.
 */
struct usbc_pd_port_caps {
	enum usbc_pd_data_role	default_data_role;
	enum usbc_pd_power_role	default_power_role;
	bool			supports_drp;	/* dual-role-power capable */
	bool			supports_dr_swap;
	bool			supports_pr_swap;
	bool			supports_vconn_swap;
	enum usbc_pd_rev	advertise_rev;	/* PD 2.0 or 3.0 */

	/* Source-side capabilities advertised in Source_Capabilities. */
	uint8_t		nr_src_pdos;
	uint32_t	src_pdos[7];

	/* Sink-side capabilities advertised in Sink_Capabilities. */
	uint8_t		nr_snk_pdos;
	uint32_t	snk_pdos[7];
};

/*
 * Public API.  Consumers (chip drivers, altmode code, sysctl
 * handlers) interact with the policy machine only through these
 * functions; struct usbc_pd_policy is opaque.
 */

/* Allocate and initialize a policy instance bound to a TCPC. */
struct usbc_pd_policy *usbc_pd_policy_alloc(struct usbc_tcpc *tcpc,
		    const struct usbc_pd_port_caps *caps);

/* Free a previously-allocated policy instance. */
void	usbc_pd_policy_free(struct usbc_pd_policy *p);

/* Queue an event for the policy machine; safe from any context. */
void	usbc_pd_policy_event(struct usbc_pd_policy *p,
		    enum usbc_pd_event e);

/* Run the state machine; consumes queued events until quiescent. */
void	usbc_pd_policy_run(struct usbc_pd_policy *p);

/* Read-only state accessor for the typec class device / sysctls. */
enum usbc_pd_state usbc_pd_policy_state(const struct usbc_pd_policy *p);

#endif /* !_USBC_PD_POLICY_H_ */
