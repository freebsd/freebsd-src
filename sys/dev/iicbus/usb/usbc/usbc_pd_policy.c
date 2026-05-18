/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kyle Crenshaw
 * All rights reserved.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/callout.h>
#include <sys/taskqueue.h>

#include <dev/iicbus/usb/usbc/usbc_pd.h>
#include <dev/iicbus/usb/usbc/usbc_pd_msg.h>
#include <dev/iicbus/usb/usbc/usbc_pd_policy.h>
#include <dev/iicbus/usb/usbc/usbc_tcpc.h>

/*
 * USB-PD policy state machine -- runtime skeleton.
 *
 * This file owns the per-policy softc (struct usbc_pd_policy), the
 * dispatch plumbing (event queue + run loop + timer + lock), and the
 * lifecycle (alloc/free).  Per-state behavior lives in dispatch
 * tables and helper functions added in subsequent commits; this
 * skeleton transitions only between USBC_PD_S_INVALID and
 * USBC_PD_S_DISABLED.
 */

static MALLOC_DEFINE(M_USBC_PD_POLICY, "usbc_pd_policy",
    "USB-PD policy state machine");

/*
 * Internal softc.  Opaque to consumers (declared in the header as
 * struct usbc_pd_policy).  Not allocated on the stack -- always via
 * malloc(M_USBC_PD_POLICY) so the lock has a stable address.
 */
struct usbc_pd_policy {
	struct mtx		lk;
	struct usbc_tcpc	*tcpc;
	struct usbc_pd_port_caps caps;

	enum usbc_pd_state	state;
	enum usbc_pd_state	prev_state;

	/*
	 * Saved at the point a soft-reset path is entered, so the SM can
	 * tell whether to return to SRC_SEND_CAPABILITIES or
	 * SNK_WAIT_CAPABILITIES once the reset exchange completes.
	 */
	enum usbc_pd_state	pre_reset_state;

	/*
	 * VDM / DP Alt Mode state.  Tracks discovery responses from the
	 * partner and the latest DP_Status VDO.  dp_object_position is
	 * the 1-based mode index we Enter_Mode'd into; dp_status is the
	 * most recent DP_Status response (HPD bit etc.).  dp_ready is
	 * set after Enter_Mode ACK and goes false on detach.
	 */
	bool			dp_ready;
	uint8_t			dp_object_position;
	uint16_t		dp_partner_svids[8];
	uint8_t			dp_partner_svid_count;
	uint32_t		dp_partner_modes[6];
	uint8_t			dp_partner_mode_count;
	uint32_t		dp_status;
	uint32_t		dp_partner_id_hdr;
	uint32_t		dp_partner_cert_stat;
	uint32_t		dp_partner_product;

	/* Pending events folded together until run() consumes them. */
	uint32_t		pending_events;

	/* Active timer (if any).  USBC_PD_T_NONE means no timer armed. */
	enum usbc_pd_timer	active_timer;
	struct callout		timer_co;

	/* Outgoing message scratch space + monotonic MessageID. */
	struct usbc_pd_msg	tx;
	uint8_t			msg_id;

	/* SRC_SEND_CAPABILITIES retry counter (PD nCapsCount, max 50). */
	uint8_t			caps_counter;

	bool			running;	/* run() is on stack */
	bool			pending_run;	/* needs another iteration */
};

#define	POLICY_LOCK(p)		mtx_lock(&(p)->lk)
#define	POLICY_UNLOCK(p)	mtx_unlock(&(p)->lk)
#define	POLICY_ASSERT_LOCKED(p)	mtx_assert(&(p)->lk, MA_OWNED)

/* Forward declarations of helpers defined below. */
static void	usbc_pd_policy_timer_cb(void *);
static void	usbc_pd_policy_run_locked(struct usbc_pd_policy *p);
static void	usbc_pd_policy_set_state(struct usbc_pd_policy *p,
		    enum usbc_pd_state next);
static void	usbc_pd_policy_arm_timer(struct usbc_pd_policy *p,
		    enum usbc_pd_timer t, u_int ms);
static void	usbc_pd_policy_cancel_timer(struct usbc_pd_policy *p);
static void	usbc_pd_state_unattached_src(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_attach_wait_src(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_src_attached(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_src_startup(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_src_send_caps(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_src_negotiate_caps(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_src_transition_supply(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_src_ready(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_hard_reset_send(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_src_hard_reset_vbus_off(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_src_hard_reset_vbus_on(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_unattached_snk(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_attach_wait_snk(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_snk_attached(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_snk_startup(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_snk_discovery(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_snk_wait_caps(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_snk_negotiate_caps(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_snk_transition_sink(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_snk_ready(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_snk_hard_reset_sink_off(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_snk_hard_reset_wait_vbus(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_snk_hard_reset_sink_on(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_soft_reset(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_soft_reset_send(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_vdm_discover_identity(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_vdm_discover_svids(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_vdm_discover_modes(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_vdm_enter_mode(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_vdm_dp_status(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_vdm_dp_configure(struct usbc_pd_policy *p,
		    uint32_t events);
static void	usbc_pd_state_vdm_dp_ready(struct usbc_pd_policy *p,
		    uint32_t events);
static int	usbc_pd_send_vdm(struct usbc_pd_policy *p, uint16_t svid,
		    uint8_t cmd, uint8_t obj_pos,
		    const uint32_t *vdos, uint8_t vdo_count);
static void	usbc_pd_handle_attention(struct usbc_pd_policy *p,
		    const struct usbc_pd_msg *rx);
static int	usbc_pd_send_ctrl(struct usbc_pd_policy *p, uint8_t ctrl_type);
static int	usbc_pd_drain_rx(struct usbc_pd_policy *p,
		    struct usbc_pd_msg *out, enum usbc_pd_sop *sop_out);
static bool	usbc_pd_check_soft_reset_rx(struct usbc_pd_policy *p,
		    const struct usbc_pd_msg *rx, enum usbc_pd_sop sop);
static enum usbc_pd_state usbc_pd_caps_state_for_role(
		    const struct usbc_pd_policy *p);

/* Allocate and zero-init a policy bound to the given TCPC + caps. */
struct usbc_pd_policy *
usbc_pd_policy_alloc(struct usbc_tcpc *tcpc,
    const struct usbc_pd_port_caps *caps)
{
	struct usbc_pd_policy *p;

	if (tcpc == NULL || caps == NULL)
		return (NULL);

	p = malloc(sizeof(*p), M_USBC_PD_POLICY, M_WAITOK | M_ZERO);
	mtx_init(&p->lk, "usbc_pd", NULL, MTX_DEF);
	callout_init_mtx(&p->timer_co, &p->lk, 0);

	p->tcpc = tcpc;
	memcpy(&p->caps, caps, sizeof(p->caps));
	p->state = USBC_PD_S_DISABLED;
	p->prev_state = USBC_PD_S_INVALID;
	p->active_timer = USBC_PD_T_NONE;
	p->msg_id = 0;

	return (p);
}

/* Tear down a policy: cancel timers, drop the lock, free. */
void
usbc_pd_policy_free(struct usbc_pd_policy *p)
{

	if (p == NULL)
		return;

	POLICY_LOCK(p);
	callout_stop(&p->timer_co);
	POLICY_UNLOCK(p);

	callout_drain(&p->timer_co);
	mtx_destroy(&p->lk);
	free(p, M_USBC_PD_POLICY);
}

/* Queue events for the SM; safe from any context, including IRQ. */
void
usbc_pd_policy_event(struct usbc_pd_policy *p, enum usbc_pd_event e)
{

	if (p == NULL || e == USBC_PD_E_NONE)
		return;

	POLICY_LOCK(p);
	p->pending_events |= (uint32_t)e;
	if (p->running) {
		/* Caller is already in run(); it will see the new bits. */
		p->pending_run = true;
		POLICY_UNLOCK(p);
		return;
	}
	usbc_pd_policy_run_locked(p);
	POLICY_UNLOCK(p);
}

/* Public run() variant: take the lock and dispatch. */
void
usbc_pd_policy_run(struct usbc_pd_policy *p)
{

	if (p == NULL)
		return;
	POLICY_LOCK(p);
	usbc_pd_policy_run_locked(p);
	POLICY_UNLOCK(p);
}

/* Read-only state accessor (for sysctls / class device). */
enum usbc_pd_state
usbc_pd_policy_state(const struct usbc_pd_policy *p)
{

	if (p == NULL)
		return (USBC_PD_S_INVALID);
	return (p->state);	/* atomic read of a single int -- no lock needed */
}

/*
 * Internal: dispatch loop.  Repeatedly snapshot pending_events,
 * call the per-state handler with that event mask, and repeat until
 * either the state stops requesting another iteration or no new
 * events arrive.  Recursion-safe via the running/pending_run pair:
 * if event() is called while a state handler is executing, the new
 * event is folded in and a re-iteration is requested rather than a
 * nested run.
 */
static void
usbc_pd_policy_run_locked(struct usbc_pd_policy *p)
{
	uint32_t events;

	POLICY_ASSERT_LOCKED(p);

	if (p->running) {
		p->pending_run = true;
		return;
	}
	p->running = true;

	for (;;) {
		events = p->pending_events;
		p->pending_events = 0;
		p->pending_run = false;

		if (events == 0)
			break;

		/*
		 * Cross-cutting events handled before per-state dispatch:
		 * a hard-reset received from the partner forces us into
		 * the role-appropriate recovery chain regardless of
		 * current state.  Source: drop VBUS for tPSHardReset.
		 * Sink: detach the load and wait for VBUS to recycle.
		 */
		if ((events & USBC_PD_E_HARD_RESET_RX) &&
		    p->state >= USBC_PD_S_SRC_ATTACHED &&
		    p->state <= USBC_PD_S_SRC_WAIT_NEW_CAPABILITIES) {
			usbc_pd_policy_set_state(p,
			    USBC_PD_S_SRC_HARD_RESET_VBUS_OFF);
			continue;
		}
		if ((events & USBC_PD_E_HARD_RESET_RX) &&
		    p->state >= USBC_PD_S_SNK_ATTACHED &&
		    p->state <= USBC_PD_S_SNK_READY) {
			usbc_pd_policy_set_state(p,
			    USBC_PD_S_SNK_HARD_RESET_SINK_OFF);
			continue;
		}

		switch (p->state) {
		case USBC_PD_S_UNATTACHED_SRC:
			usbc_pd_state_unattached_src(p, events);
			break;
		case USBC_PD_S_ATTACH_WAIT_SRC:
			usbc_pd_state_attach_wait_src(p, events);
			break;
		case USBC_PD_S_SRC_ATTACHED:
			usbc_pd_state_src_attached(p, events);
			break;
		case USBC_PD_S_SRC_STARTUP:
			usbc_pd_state_src_startup(p, events);
			break;
		case USBC_PD_S_SRC_SEND_CAPABILITIES:
			usbc_pd_state_src_send_caps(p, events);
			break;
		case USBC_PD_S_SRC_NEGOTIATE_CAPABILITIES:
			usbc_pd_state_src_negotiate_caps(p, events);
			break;
		case USBC_PD_S_SRC_TRANSITION_SUPPLY:
			usbc_pd_state_src_transition_supply(p, events);
			break;
		case USBC_PD_S_SRC_READY:
			usbc_pd_state_src_ready(p, events);
			break;
		case USBC_PD_S_HARD_RESET_SEND:
			usbc_pd_state_hard_reset_send(p, events);
			break;
		case USBC_PD_S_SRC_HARD_RESET_VBUS_OFF:
			usbc_pd_state_src_hard_reset_vbus_off(p, events);
			break;
		case USBC_PD_S_SRC_HARD_RESET_VBUS_ON:
			usbc_pd_state_src_hard_reset_vbus_on(p, events);
			break;
		case USBC_PD_S_UNATTACHED_SNK:
			usbc_pd_state_unattached_snk(p, events);
			break;
		case USBC_PD_S_ATTACH_WAIT_SNK:
			usbc_pd_state_attach_wait_snk(p, events);
			break;
		case USBC_PD_S_SNK_ATTACHED:
			usbc_pd_state_snk_attached(p, events);
			break;
		case USBC_PD_S_SNK_STARTUP:
			usbc_pd_state_snk_startup(p, events);
			break;
		case USBC_PD_S_SNK_DISCOVERY:
			usbc_pd_state_snk_discovery(p, events);
			break;
		case USBC_PD_S_SNK_WAIT_CAPABILITIES:
			usbc_pd_state_snk_wait_caps(p, events);
			break;
		case USBC_PD_S_SNK_NEGOTIATE_CAPABILITIES:
			usbc_pd_state_snk_negotiate_caps(p, events);
			break;
		case USBC_PD_S_SNK_TRANSITION_SINK:
			usbc_pd_state_snk_transition_sink(p, events);
			break;
		case USBC_PD_S_SNK_READY:
			usbc_pd_state_snk_ready(p, events);
			break;
		case USBC_PD_S_SNK_HARD_RESET_SINK_OFF:
			usbc_pd_state_snk_hard_reset_sink_off(p, events);
			break;
		case USBC_PD_S_SNK_HARD_RESET_WAIT_VBUS:
			usbc_pd_state_snk_hard_reset_wait_vbus(p, events);
			break;
		case USBC_PD_S_SNK_HARD_RESET_SINK_ON:
			usbc_pd_state_snk_hard_reset_sink_on(p, events);
			break;
		case USBC_PD_S_SOFT_RESET:
			usbc_pd_state_soft_reset(p, events);
			break;
		case USBC_PD_S_SOFT_RESET_SEND:
			usbc_pd_state_soft_reset_send(p, events);
			break;
		case USBC_PD_S_VDM_SEND_DISCOVER_IDENTITY:
			usbc_pd_state_vdm_discover_identity(p, events);
			break;
		case USBC_PD_S_VDM_SEND_DISCOVER_SVIDS:
			usbc_pd_state_vdm_discover_svids(p, events);
			break;
		case USBC_PD_S_VDM_SEND_DISCOVER_MODES:
			usbc_pd_state_vdm_discover_modes(p, events);
			break;
		case USBC_PD_S_VDM_SEND_ENTER_MODE:
			usbc_pd_state_vdm_enter_mode(p, events);
			break;
		case USBC_PD_S_VDM_SEND_DP_STATUS:
			usbc_pd_state_vdm_dp_status(p, events);
			break;
		case USBC_PD_S_VDM_SEND_DP_CONFIGURE:
			usbc_pd_state_vdm_dp_configure(p, events);
			break;
		case USBC_PD_S_VDM_DP_READY:
			usbc_pd_state_vdm_dp_ready(p, events);
			break;
		case USBC_PD_S_DISABLED:
			/*
			 * Manual disable: stay until PORT_ENABLE.  Default
			 * unattached state follows the configured power role.
			 */
			if (events & USBC_PD_E_PORT_ENABLE) {
				enum usbc_pd_state next;

				next = (p->caps.default_power_role ==
				    USBC_PD_ROLE_SINK) ?
				    USBC_PD_S_UNATTACHED_SNK :
				    USBC_PD_S_UNATTACHED_SRC;
				usbc_pd_policy_set_state(p, next);
			}
			break;
		default:
			/* States not yet implemented just sink events. */
			break;
		}

		if (!p->pending_run && p->pending_events == 0)
			break;
	}

	p->running = false;
}

/* Callout body for the active PD timer.  Translates to USBC_PD_E_TIMER. */
static void
usbc_pd_policy_timer_cb(void *arg)
{
	struct usbc_pd_policy *p = arg;

	POLICY_ASSERT_LOCKED(p);
	p->active_timer = USBC_PD_T_NONE;
	p->pending_events |= USBC_PD_E_TIMER;
	usbc_pd_policy_run_locked(p);
}

/* State transition: cancel the active timer and update prev/state. */
static void
usbc_pd_policy_set_state(struct usbc_pd_policy *p, enum usbc_pd_state next)
{

	POLICY_ASSERT_LOCKED(p);
	if (next == p->state)
		return;
	usbc_pd_policy_cancel_timer(p);
	p->prev_state = p->state;
	p->state = next;
	/* Re-run so the new state's handler executes its entry actions. */
	p->pending_run = true;
}

/* Arm the named PD timer for `ms` milliseconds (overrides any active). */
static void
usbc_pd_policy_arm_timer(struct usbc_pd_policy *p, enum usbc_pd_timer t,
    u_int ms)
{
	sbintime_t sbt;

	POLICY_ASSERT_LOCKED(p);
	usbc_pd_policy_cancel_timer(p);
	p->active_timer = t;
	sbt = SBT_1MS * ms;
	callout_reset_sbt(&p->timer_co, sbt, 0,
	    usbc_pd_policy_timer_cb, p, 0);
}

/* Cancel any active PD timer; idempotent. */
static void
usbc_pd_policy_cancel_timer(struct usbc_pd_policy *p)
{

	POLICY_ASSERT_LOCKED(p);
	if (p->active_timer == USBC_PD_T_NONE)
		return;
	callout_stop(&p->timer_co);
	p->active_timer = USBC_PD_T_NONE;
}

/*
 * UNATTACHED_SRC: idle as a source.  Watch CC for an Rd termination
 * appearing (sink connected); when seen, debounce in ATTACH_WAIT_SRC.
 */
static void
usbc_pd_state_unattached_src(struct usbc_pd_policy *p, uint32_t events)
{
	enum usbc_tcpc_cc_state cc1, cc2;

	POLICY_ASSERT_LOCKED(p);

	if ((events & USBC_PD_E_CC_CHANGE) == 0)
		return;
	if (p->tcpc->ops->get_cc == NULL)
		return;
	if (p->tcpc->ops->get_cc(p->tcpc, &cc1, &cc2) != 0)
		return;
	if (cc1 == USBC_TCPC_CC_RD || cc2 == USBC_TCPC_CC_RD)
		usbc_pd_policy_set_state(p, USBC_PD_S_ATTACH_WAIT_SRC);
}

/*
 * ATTACH_WAIT_SRC: tCCDebounce expires before we commit to attach.
 * The chip latches Rd; we just delay long enough that a glitch
 * doesn't trigger the full PD bring-up.
 */
static void
usbc_pd_state_attach_wait_src(struct usbc_pd_policy *p, uint32_t events)
{
	enum usbc_tcpc_cc_state cc1, cc2;

	POLICY_ASSERT_LOCKED(p);

	/* (Re)arm tCCDebounce on entry or any CC bounce. */
	if (p->prev_state != USBC_PD_S_ATTACH_WAIT_SRC ||
	    (events & USBC_PD_E_CC_CHANGE)) {
		usbc_pd_policy_arm_timer(p, USBC_PD_T_CC_DEBOUNCE,
		    USBC_PD_T_CC_DEBOUNCE_MS);
		p->prev_state = USBC_PD_S_ATTACH_WAIT_SRC;
		return;
	}
	if ((events & USBC_PD_E_TIMER) == 0)
		return;

	if (p->tcpc->ops->get_cc == NULL ||
	    p->tcpc->ops->get_cc(p->tcpc, &cc1, &cc2) != 0) {
		usbc_pd_policy_set_state(p, USBC_PD_S_UNATTACHED_SRC);
		return;
	}
	if (cc1 == USBC_TCPC_CC_RD || cc2 == USBC_TCPC_CC_RD)
		usbc_pd_policy_set_state(p, USBC_PD_S_SRC_ATTACHED);
	else
		usbc_pd_policy_set_state(p, USBC_PD_S_UNATTACHED_SRC);
}

/*
 * SRC_ATTACHED: pin polarity, enable VBUS, enable VCONN, then chain
 * into SRC_STARTUP for PD bring-up.  Entry-driven; events ignored.
 */
static void
usbc_pd_state_src_attached(struct usbc_pd_policy *p, uint32_t events)
{
	enum usbc_tcpc_cc_state cc1, cc2;
	enum usbc_pd_polarity polarity;

	POLICY_ASSERT_LOCKED(p);

	(void)events;

	if (p->prev_state == USBC_PD_S_SRC_ATTACHED)
		return;
	p->prev_state = USBC_PD_S_SRC_ATTACHED;

	if (p->tcpc->ops->get_cc == NULL ||
	    p->tcpc->ops->get_cc(p->tcpc, &cc1, &cc2) != 0)
		return;
	polarity = (cc1 == USBC_TCPC_CC_RD) ? USBC_PD_POLARITY_CC1 :
	    USBC_PD_POLARITY_CC2;
	if (p->tcpc->ops->set_polarity != NULL)
		(void)p->tcpc->ops->set_polarity(p->tcpc, polarity);
	if (p->tcpc->ops->set_vbus_source != NULL)
		(void)p->tcpc->ops->set_vbus_source(p->tcpc, true);
	if (p->tcpc->ops->set_vconn != NULL)
		(void)p->tcpc->ops->set_vconn(p->tcpc, true);

	/* Hand off to the PD bring-up path. */
	usbc_pd_policy_set_state(p, USBC_PD_S_SRC_STARTUP);
}

/*
 * SRC_STARTUP: reset PD layer state and enable message reception,
 * then transition to SRC_SEND_CAPABILITIES so we'll start
 * advertising our PDOs to the partner.
 */
static void
usbc_pd_state_src_startup(struct usbc_pd_policy *p, uint32_t events)
{

	POLICY_ASSERT_LOCKED(p);

	(void)events;

	/* Per-attach reset of PD-layer state. */
	p->msg_id = 0;
	p->caps_counter = 0;

	/* Tell the chip we're ready to receive PD messages. */
	if (p->tcpc->ops->set_rx_enable != NULL)
		(void)p->tcpc->ops->set_rx_enable(p->tcpc, true);

	usbc_pd_policy_set_state(p, USBC_PD_S_SRC_SEND_CAPABILITIES);
}

/*
 * SRC_SEND_CAPABILITIES: transmit Source_Capabilities and wait for
 * Request.  On TX_FAIL we retry up to N_CAPS_COUNT times spaced by
 * tTypeCSendSourceCap; on TX_OK we wait for the partner's Request
 * (via tSenderResponse).  RX-of-Request handoff to
 * SRC_NEGOTIATE_CAPABILITIES lands in a later commit; for now this
 * state stops at "TX accepted, waiting for Request."
 */
static void
usbc_pd_state_src_send_caps(struct usbc_pd_policy *p, uint32_t events)
{
	struct usbc_pd_msg *msg;
	uint32_t pdos[7];
	uint8_t i;
	int err;

	POLICY_ASSERT_LOCKED(p);

	/* Entry: build and submit the Source_Capabilities message. */
	if (p->prev_state != USBC_PD_S_SRC_SEND_CAPABILITIES) {
		p->prev_state = USBC_PD_S_SRC_SEND_CAPABILITIES;
		if (p->caps.nr_src_pdos == 0 || p->caps.nr_src_pdos > 7) {
			usbc_pd_policy_set_state(p, USBC_PD_S_HARD_RESET_SEND);
			return;
		}
		for (i = 0; i < p->caps.nr_src_pdos; i++)
			pdos[i] = p->caps.src_pdos[i];
		msg = &p->tx;
		if (!usbc_pd_msg_data(msg, USBC_PD_DATA_SRC_CAP,
		    pdos, p->caps.nr_src_pdos, p->msg_id,
		    USBC_PD_ROLE_SOURCE, USBC_PD_DATA_DFP,
		    p->caps.advertise_rev)) {
			usbc_pd_policy_set_state(p, USBC_PD_S_HARD_RESET_SEND);
			return;
		}
		if (p->tcpc->ops->transmit == NULL) {
			usbc_pd_policy_set_state(p, USBC_PD_S_HARD_RESET_SEND);
			return;
		}
		err = p->tcpc->ops->transmit(p->tcpc, USBC_PD_SOP, msg);
		if (err != 0) {
			usbc_pd_policy_set_state(p, USBC_PD_S_HARD_RESET_SEND);
			return;
		}
		return;
	}

	/* TX completed successfully -- arm tSenderResponse for Request. */
	if (events & USBC_PD_E_TX_OK) {
		p->caps_counter = 0;
		p->msg_id = (p->msg_id + 1) & 0x7;
		usbc_pd_policy_arm_timer(p, USBC_PD_T_SENDER_RESPONSE,
		    USBC_PD_T_SENDER_RESPONSE_MS);
		return;
	}

	/* Partner sent a message -- if it's Request, negotiate. */
	if (events & USBC_PD_E_RX) {
		struct usbc_pd_msg rx;
		enum usbc_pd_sop sop;
		uint8_t type;

		if (usbc_pd_drain_rx(p, &rx, &sop) != 0)
			return;
		if (usbc_pd_check_soft_reset_rx(p, &rx, sop))
			return;
		if (sop != USBC_PD_SOP)
			return;	/* ignore SOP'/SOP'' for now */
		if (USBC_PD_HDR_GET_NDO(rx.hdr) == 0)
			return;	/* control msg, not a Request */
		type = USBC_PD_HDR_GET_TYPE(rx.hdr);
		if (type != USBC_PD_DATA_REQUEST)
			return;
		/*
		 * We've received a Request RDO.  Stash it and move to
		 * SRC_NEGOTIATE_CAPABILITIES.  The negotiate state will
		 * evaluate the requested PDO index against our advertised
		 * supply.  For now we copy the RDO into tx slot 0 so the
		 * negotiate handler can read it; a future commit moves
		 * this to a proper rx_msg field on the softc.
		 */
		p->tx.data[0] = rx.data[0];
		usbc_pd_policy_set_state(p,
		    USBC_PD_S_SRC_NEGOTIATE_CAPABILITIES);
		return;
	}

	/* TX failed -- retry up to N_CAPS_COUNT times. */
	if (events & USBC_PD_E_TX_FAIL) {
		p->caps_counter++;
		if (p->caps_counter >= USBC_PD_N_CAPS_COUNT) {
			/* Partner not PD-capable; for now bail out. */
			usbc_pd_policy_set_state(p, USBC_PD_S_DISABLED);
			return;
		}
		usbc_pd_policy_arm_timer(p, USBC_PD_T_TYPEC_SEND_SOURCE_CAP,
		    USBC_PD_T_TYPEC_SEND_SRCCAP_MS);
		return;
	}

	/* tTypeCSendSourceCap fired -- re-transmit. */
	if ((events & USBC_PD_E_TIMER) &&
	    p->active_timer == USBC_PD_T_NONE) {
		p->prev_state = USBC_PD_S_INVALID;	/* force re-entry */
		p->pending_run = true;
		return;
	}

	/* tSenderResponse fired with no Request received -- hard reset. */
	if (events & USBC_PD_E_TIMER)
		usbc_pd_policy_set_state(p, USBC_PD_S_HARD_RESET_SEND);
}

/*
 * SRC_NEGOTIATE_CAPABILITIES: evaluate the partner's Request RDO and
 * either Accept (and move to SRC_TRANSITION_SUPPLY) or Reject (and
 * fall back to SRC_SEND_CAPABILITIES).  Minimal policy for now:
 * accept any in-range PDO index.  Per-spec we'd compare requested
 * current/voltage to the advertised PDO and reject mismatches; that
 * comes later.
 */
static void
usbc_pd_state_src_negotiate_caps(struct usbc_pd_policy *p, uint32_t events)
{
	uint32_t rdo;
	uint8_t pos;

	POLICY_ASSERT_LOCKED(p);

	(void)events;	/* entry-driven */

	if (p->prev_state == USBC_PD_S_SRC_NEGOTIATE_CAPABILITIES)
		return;
	p->prev_state = USBC_PD_S_SRC_NEGOTIATE_CAPABILITIES;

	rdo = p->tx.data[0];
	pos = (rdo >> 28) & 0x7;
	if (pos < 1 || pos > p->caps.nr_src_pdos) {
		(void)usbc_pd_send_ctrl(p, USBC_PD_CTRL_REJECT);
		usbc_pd_policy_set_state(p,
		    USBC_PD_S_SRC_SEND_CAPABILITIES);
		return;
	}
	if (usbc_pd_send_ctrl(p, USBC_PD_CTRL_ACCEPT) != 0) {
		usbc_pd_policy_set_state(p, USBC_PD_S_HARD_RESET_SEND);
		return;
	}
	usbc_pd_policy_set_state(p, USBC_PD_S_SRC_TRANSITION_SUPPLY);
}

/*
 * SRC_TRANSITION_SUPPLY: arm tSrcTransition, on timer fire send
 * PS_RDY, then transition to SRC_READY.  In a full implementation
 * the supply rail would actually transition (voltage/current ramp);
 * for the minimal port we just observe the timing contract.
 */
static void
usbc_pd_state_src_transition_supply(struct usbc_pd_policy *p, uint32_t events)
{

	POLICY_ASSERT_LOCKED(p);

	if (p->prev_state != USBC_PD_S_SRC_TRANSITION_SUPPLY) {
		p->prev_state = USBC_PD_S_SRC_TRANSITION_SUPPLY;
		usbc_pd_policy_arm_timer(p, USBC_PD_T_PS_TRANSITION,
		    USBC_PD_T_SRC_TRANSITION_MS);
		return;
	}
	if ((events & USBC_PD_E_TIMER) == 0)
		return;

	if (usbc_pd_send_ctrl(p, USBC_PD_CTRL_PS_RDY) != 0) {
		usbc_pd_policy_set_state(p, USBC_PD_S_HARD_RESET_SEND);
		return;
	}
	usbc_pd_policy_set_state(p, USBC_PD_S_SRC_READY);
}

/*
 * SRC_READY: contract established.  Once we land here, kick off VDM
 * Discover_Identity to learn whether the partner supports DisplayPort
 * Alt Mode.  This matches Linux's tcpm: after the PD contract,
 * immediately discover the partner identity, SVIDs, and modes; if DP
 * is offered, Enter_Mode and start polling DP_Status.  Without this
 * the display sees us as "PD-only" and won't fully wake its DP RX.
 */
static void
usbc_pd_state_src_ready(struct usbc_pd_policy *p, uint32_t events)
{

	POLICY_ASSERT_LOCKED(p);

	(void)events;
	if (p->prev_state == USBC_PD_S_SRC_READY)
		return;
	p->prev_state = USBC_PD_S_SRC_READY;

	/* Reset partner-discovery state on each fresh attach. */
	p->dp_partner_svid_count = 0;
	p->dp_partner_mode_count = 0;
	p->dp_object_position = 0;
	p->dp_status = 0;
	p->dp_ready = false;

	usbc_pd_policy_set_state(p, USBC_PD_S_VDM_SEND_DISCOVER_IDENTITY);
}

/*
 * HARD_RESET_SEND: ask the TCPC to emit a Hard Reset over the wire,
 * then once that's confirmed (HARD_RESET_TX_OK event) transition
 * into SRC_HARD_RESET_VBUS_OFF to drop VBUS for tPSHardReset.
 */
static void
usbc_pd_state_hard_reset_send(struct usbc_pd_policy *p, uint32_t events)
{

	POLICY_ASSERT_LOCKED(p);

	if (p->prev_state != USBC_PD_S_HARD_RESET_SEND) {
		p->prev_state = USBC_PD_S_HARD_RESET_SEND;
		if (p->tcpc->ops->send_hard_reset != NULL)
			(void)p->tcpc->ops->send_hard_reset(p->tcpc);
		return;
	}
	if (events & USBC_PD_E_HARD_RESET_TX_OK) {
		usbc_pd_policy_set_state(p,
		    USBC_PD_S_SRC_HARD_RESET_VBUS_OFF);
	}
}

/*
 * SRC_HARD_RESET_VBUS_OFF: drop VBUS, arm tPSHardReset.  When the
 * timer fires, advance to SRC_HARD_RESET_VBUS_ON to re-enable.
 */
static void
usbc_pd_state_src_hard_reset_vbus_off(struct usbc_pd_policy *p,
    uint32_t events)
{

	POLICY_ASSERT_LOCKED(p);

	if (p->prev_state != USBC_PD_S_SRC_HARD_RESET_VBUS_OFF) {
		p->prev_state = USBC_PD_S_SRC_HARD_RESET_VBUS_OFF;
		if (p->tcpc->ops->set_vbus_source != NULL)
			(void)p->tcpc->ops->set_vbus_source(p->tcpc, false);
		usbc_pd_policy_arm_timer(p, USBC_PD_T_PS_HARD_RESET,
		    USBC_PD_T_VBUS_OFF_MS);
		return;
	}
	if (events & USBC_PD_E_TIMER)
		usbc_pd_policy_set_state(p,
		    USBC_PD_S_SRC_HARD_RESET_VBUS_ON);
}

/*
 * SRC_HARD_RESET_VBUS_ON: bring VBUS back up, arm tVBUSOn for it
 * to settle, then transition back into SRC_STARTUP to redo the
 * full PD bring-up.
 */
static void
usbc_pd_state_src_hard_reset_vbus_on(struct usbc_pd_policy *p,
    uint32_t events)
{

	POLICY_ASSERT_LOCKED(p);

	if (p->prev_state != USBC_PD_S_SRC_HARD_RESET_VBUS_ON) {
		p->prev_state = USBC_PD_S_SRC_HARD_RESET_VBUS_ON;
		if (p->tcpc->ops->set_vbus_source != NULL)
			(void)p->tcpc->ops->set_vbus_source(p->tcpc, true);
		usbc_pd_policy_arm_timer(p, USBC_PD_T_VBUS_ON,
		    USBC_PD_T_VBUS_ON_MS);
		return;
	}
	if (events & USBC_PD_E_TIMER)
		usbc_pd_policy_set_state(p, USBC_PD_S_SRC_STARTUP);
}

/*
 * UNATTACHED_SNK: idle as a sink.  Watch CC for an Rp termination
 * appearing (a source connected); when seen, debounce in
 * ATTACH_WAIT_SNK before committing.
 */
static void
usbc_pd_state_unattached_snk(struct usbc_pd_policy *p, uint32_t events)
{
	enum usbc_tcpc_cc_state cc1, cc2;

	POLICY_ASSERT_LOCKED(p);

	if ((events & USBC_PD_E_CC_CHANGE) == 0)
		return;
	if (p->tcpc->ops->get_cc == NULL)
		return;
	if (p->tcpc->ops->get_cc(p->tcpc, &cc1, &cc2) != 0)
		return;
	if (cc1 == USBC_TCPC_CC_RP_USB || cc1 == USBC_TCPC_CC_RP_1_5A ||
	    cc1 == USBC_TCPC_CC_RP_3_0A ||
	    cc2 == USBC_TCPC_CC_RP_USB || cc2 == USBC_TCPC_CC_RP_1_5A ||
	    cc2 == USBC_TCPC_CC_RP_3_0A)
		usbc_pd_policy_set_state(p, USBC_PD_S_ATTACH_WAIT_SNK);
}

/*
 * ATTACH_WAIT_SNK: tCCDebounce + verify VBUS arrived.  Per spec the
 * sink waits both for a stable CC reading and for VBUS to come up
 * before committing to attach.
 */
static void
usbc_pd_state_attach_wait_snk(struct usbc_pd_policy *p, uint32_t events)
{
	enum usbc_tcpc_cc_state cc1, cc2;
	bool vbus;

	POLICY_ASSERT_LOCKED(p);

	if (p->prev_state != USBC_PD_S_ATTACH_WAIT_SNK ||
	    (events & USBC_PD_E_CC_CHANGE)) {
		usbc_pd_policy_arm_timer(p, USBC_PD_T_CC_DEBOUNCE,
		    USBC_PD_T_CC_DEBOUNCE_MS);
		p->prev_state = USBC_PD_S_ATTACH_WAIT_SNK;
		return;
	}
	if ((events & USBC_PD_E_TIMER) == 0)
		return;

	if (p->tcpc->ops->get_cc == NULL ||
	    p->tcpc->ops->get_cc(p->tcpc, &cc1, &cc2) != 0) {
		usbc_pd_policy_set_state(p, USBC_PD_S_UNATTACHED_SNK);
		return;
	}
	if (cc1 < USBC_TCPC_CC_RP_USB && cc2 < USBC_TCPC_CC_RP_USB) {
		usbc_pd_policy_set_state(p, USBC_PD_S_UNATTACHED_SNK);
		return;
	}
	vbus = false;
	if (p->tcpc->ops->get_vbus_present != NULL)
		(void)p->tcpc->ops->get_vbus_present(p->tcpc, &vbus);
	if (!vbus) {
		/* CC is right but VBUS hasn't come up yet -- keep waiting.
		 * VBUS_CHANGE will re-enter and arrive here through the
		 * timer-fired branch above. */
		usbc_pd_policy_arm_timer(p, USBC_PD_T_CC_DEBOUNCE,
		    USBC_PD_T_CC_DEBOUNCE_MS);
		return;
	}
	usbc_pd_policy_set_state(p, USBC_PD_S_SNK_ATTACHED);
}

/*
 * SNK_ATTACHED: pin polarity to the active CC and chain into
 * SNK_STARTUP for PD bring-up.  Entry-driven; events ignored.
 */
static void
usbc_pd_state_snk_attached(struct usbc_pd_policy *p, uint32_t events)
{
	enum usbc_tcpc_cc_state cc1, cc2;
	enum usbc_pd_polarity polarity;

	POLICY_ASSERT_LOCKED(p);

	(void)events;

	if (p->prev_state == USBC_PD_S_SNK_ATTACHED)
		return;
	p->prev_state = USBC_PD_S_SNK_ATTACHED;

	if (p->tcpc->ops->get_cc == NULL ||
	    p->tcpc->ops->get_cc(p->tcpc, &cc1, &cc2) != 0)
		return;
	polarity = (cc1 >= USBC_TCPC_CC_RP_USB) ? USBC_PD_POLARITY_CC1 :
	    USBC_PD_POLARITY_CC2;
	if (p->tcpc->ops->set_polarity != NULL)
		(void)p->tcpc->ops->set_polarity(p->tcpc, polarity);

	usbc_pd_policy_set_state(p, USBC_PD_S_SNK_STARTUP);
}

/*
 * SNK_STARTUP: reset PD layer state, enable RX, then chain into
 * SNK_DISCOVERY.  Mirrors SRC_STARTUP except no caps to send.
 */
static void
usbc_pd_state_snk_startup(struct usbc_pd_policy *p, uint32_t events)
{

	POLICY_ASSERT_LOCKED(p);
	(void)events;

	p->msg_id = 0;
	p->caps_counter = 0;

	if (p->tcpc->ops->set_rx_enable != NULL)
		(void)p->tcpc->ops->set_rx_enable(p->tcpc, true);

	usbc_pd_policy_set_state(p, USBC_PD_S_SNK_DISCOVERY);
}

/*
 * SNK_DISCOVERY: brief connect debounce gives the source time to
 * start emitting Source_Capabilities.  Just chains through; the
 * actual wait happens in SNK_WAIT_CAPABILITIES under tTypeCSinkWaitCap.
 */
static void
usbc_pd_state_snk_discovery(struct usbc_pd_policy *p, uint32_t events)
{

	POLICY_ASSERT_LOCKED(p);
	(void)events;

	usbc_pd_policy_set_state(p, USBC_PD_S_SNK_WAIT_CAPABILITIES);
}

/*
 * SNK_WAIT_CAPABILITIES: arm tTypeCSinkWaitCap, listen for an
 * incoming Source_Capabilities.  On RX of caps, stash them in tx
 * scratch and move to SNK_NEGOTIATE_CAPABILITIES.  Timer fire means
 * the source isn't PD-capable -- escalate to a hard reset.
 */
static void
usbc_pd_state_snk_wait_caps(struct usbc_pd_policy *p, uint32_t events)
{
	struct usbc_pd_msg rx;
	enum usbc_pd_sop sop;
	uint8_t ndo, type, i;

	POLICY_ASSERT_LOCKED(p);

	if (p->prev_state != USBC_PD_S_SNK_WAIT_CAPABILITIES) {
		p->prev_state = USBC_PD_S_SNK_WAIT_CAPABILITIES;
		usbc_pd_policy_arm_timer(p, USBC_PD_T_TYPEC_SINK_WAIT_CAP,
		    USBC_PD_T_TYPEC_SINK_WAIT_CAP_MS);
		return;
	}
	if (events & USBC_PD_E_RX) {
		if (usbc_pd_drain_rx(p, &rx, &sop) != 0)
			return;
		if (usbc_pd_check_soft_reset_rx(p, &rx, sop))
			return;
		if (sop != USBC_PD_SOP)
			return;
		ndo = USBC_PD_HDR_GET_NDO(rx.hdr);
		type = USBC_PD_HDR_GET_TYPE(rx.hdr);
		if (ndo == 0 || type != USBC_PD_DATA_SRC_CAP)
			return;
		/* Stash the partner PDOs into tx data slots; the negotiate
		 * state reads them back to pick a PDO and build an RDO. */
		if (ndo > 7)
			ndo = 7;
		for (i = 0; i < ndo; i++)
			p->tx.data[i] = rx.data[i];
		p->tx.hdr = (uint16_t)ndo;	/* count stowed in low bits */
		usbc_pd_policy_set_state(p,
		    USBC_PD_S_SNK_NEGOTIATE_CAPABILITIES);
		return;
	}
	if (events & USBC_PD_E_TIMER)
		usbc_pd_policy_set_state(p, USBC_PD_S_HARD_RESET_SEND);
}

/*
 * SNK_NEGOTIATE_CAPABILITIES: pick a PDO from the partner caps and
 * transmit a Request RDO.  Minimal policy: always pick PDO #1
 * (vSafe5V fixed) and request its full advertised current.  After
 * TX_OK, arm tSenderResponse and wait for Accept; on Accept move to
 * SNK_TRANSITION_SINK; on Reject/Wait fall back to SNK_READY (or
 * retry via Wait, simplified here as "stay ready").
 */
static void
usbc_pd_state_snk_negotiate_caps(struct usbc_pd_policy *p, uint32_t events)
{
	struct usbc_pd_msg *msg;
	struct usbc_pd_msg rx;
	enum usbc_pd_sop sop;
	uint32_t pdo, rdo, ma;
	uint8_t type, ndo;
	int err;

	POLICY_ASSERT_LOCKED(p);

	if (p->prev_state != USBC_PD_S_SNK_NEGOTIATE_CAPABILITIES) {
		p->prev_state = USBC_PD_S_SNK_NEGOTIATE_CAPABILITIES;

		ndo = (uint8_t)(p->tx.hdr & 0x7);
		if (ndo == 0) {
			usbc_pd_policy_set_state(p,
			    USBC_PD_S_HARD_RESET_SEND);
			return;
		}
		pdo = p->tx.data[0];
		if ((pdo & USBC_PD_PDO_TYPE_MASK) != USBC_PD_PDO_TYPE_FIXED) {
			usbc_pd_policy_set_state(p,
			    USBC_PD_S_HARD_RESET_SEND);
			return;
		}
		ma = USBC_PD_PDO_FIXED_GET_CURRENT_MA(pdo);
		/*
		 * Build a Fixed and Variable RDO per PD 6.4.2:
		 * bit 31:    reserved
		 * bit 30:28: Object position (1..7)
		 * bit 27:    GiveBack (0)
		 * bit 26:    Capability Mismatch (0)
		 * bit 25:    USB Communications Capable
		 * bit 24:    No USB Suspend (1)
		 * bit 19:10: Operating current in 10mA units
		 * bit  9: 0: Maximum operating current in 10mA units
		 */
		rdo = (1u << 28);				/* PDO #1 */
		rdo |= (1u << 25);				/* USB-comms */
		rdo |= (1u << 24);				/* no-suspend */
		rdo |= ((ma / 10) & 0x3ffu) << 10;		/* op current */
		rdo |= ((ma / 10) & 0x3ffu);			/* max current */

		msg = &p->tx;
		if (!usbc_pd_msg_data(msg, USBC_PD_DATA_REQUEST,
		    &rdo, 1, p->msg_id, USBC_PD_ROLE_SINK,
		    USBC_PD_DATA_UFP, p->caps.advertise_rev)) {
			usbc_pd_policy_set_state(p,
			    USBC_PD_S_HARD_RESET_SEND);
			return;
		}
		if (p->tcpc->ops->transmit == NULL) {
			usbc_pd_policy_set_state(p,
			    USBC_PD_S_HARD_RESET_SEND);
			return;
		}
		err = p->tcpc->ops->transmit(p->tcpc, USBC_PD_SOP, msg);
		if (err != 0)
			usbc_pd_policy_set_state(p,
			    USBC_PD_S_HARD_RESET_SEND);
		return;
	}

	if (events & USBC_PD_E_TX_OK) {
		p->msg_id = (p->msg_id + 1) & 0x7;
		usbc_pd_policy_arm_timer(p, USBC_PD_T_SENDER_RESPONSE,
		    USBC_PD_T_SENDER_RESPONSE_MS);
		return;
	}
	if (events & USBC_PD_E_TX_FAIL) {
		usbc_pd_policy_set_state(p, USBC_PD_S_HARD_RESET_SEND);
		return;
	}
	if (events & USBC_PD_E_RX) {
		if (usbc_pd_drain_rx(p, &rx, &sop) != 0)
			return;
		if (usbc_pd_check_soft_reset_rx(p, &rx, sop))
			return;
		if (sop != USBC_PD_SOP)
			return;
		if (USBC_PD_HDR_GET_NDO(rx.hdr) != 0)
			return;
		type = USBC_PD_HDR_GET_TYPE(rx.hdr);
		if (type == USBC_PD_CTRL_ACCEPT) {
			usbc_pd_policy_set_state(p,
			    USBC_PD_S_SNK_TRANSITION_SINK);
		} else if (type == USBC_PD_CTRL_REJECT ||
		    type == USBC_PD_CTRL_WAIT) {
			/* Source declined; sit in READY at vSafe5V default. */
			usbc_pd_policy_set_state(p, USBC_PD_S_SNK_READY);
		}
		return;
	}
	if (events & USBC_PD_E_TIMER)
		usbc_pd_policy_set_state(p, USBC_PD_S_HARD_RESET_SEND);
}

/*
 * SNK_TRANSITION_SINK: source has Accepted, now waits for PS_RDY.
 * Arm tPSTransition; on RX of PS_RDY → SNK_READY; on timer fire →
 * hard reset.
 */
static void
usbc_pd_state_snk_transition_sink(struct usbc_pd_policy *p, uint32_t events)
{
	struct usbc_pd_msg rx;
	enum usbc_pd_sop sop;
	uint8_t type;

	POLICY_ASSERT_LOCKED(p);

	if (p->prev_state != USBC_PD_S_SNK_TRANSITION_SINK) {
		p->prev_state = USBC_PD_S_SNK_TRANSITION_SINK;
		usbc_pd_policy_arm_timer(p, USBC_PD_T_PS_TRANSITION,
		    USBC_PD_T_PS_TRANSITION_MS);
		return;
	}
	if (events & USBC_PD_E_RX) {
		if (usbc_pd_drain_rx(p, &rx, &sop) != 0)
			return;
		if (usbc_pd_check_soft_reset_rx(p, &rx, sop))
			return;
		if (sop != USBC_PD_SOP)
			return;
		if (USBC_PD_HDR_GET_NDO(rx.hdr) != 0)
			return;
		type = USBC_PD_HDR_GET_TYPE(rx.hdr);
		if (type == USBC_PD_CTRL_PS_RDY)
			usbc_pd_policy_set_state(p, USBC_PD_S_SNK_READY);
		return;
	}
	if (events & USBC_PD_E_TIMER)
		usbc_pd_policy_set_state(p, USBC_PD_S_HARD_RESET_SEND);
}

/*
 * SNK_READY: contract established, sink is drawing the negotiated
 * current.  Stay here until something changes (CC change, hard
 * reset, swap, new caps from source, etc).  Minimal handler.
 */
static void
usbc_pd_state_snk_ready(struct usbc_pd_policy *p, uint32_t events)
{

	POLICY_ASSERT_LOCKED(p);
	(void)events;

	if (p->prev_state != USBC_PD_S_SNK_READY)
		p->prev_state = USBC_PD_S_SNK_READY;
}

/*
 * SNK_HARD_RESET_SINK_OFF: drop our load and arm tPSHardReset.  In
 * a full implementation the sink would gate off its load switch;
 * here we observe the timing and chain.
 */
static void
usbc_pd_state_snk_hard_reset_sink_off(struct usbc_pd_policy *p, uint32_t events)
{

	POLICY_ASSERT_LOCKED(p);

	if (p->prev_state != USBC_PD_S_SNK_HARD_RESET_SINK_OFF) {
		p->prev_state = USBC_PD_S_SNK_HARD_RESET_SINK_OFF;
		usbc_pd_policy_arm_timer(p, USBC_PD_T_PS_HARD_RESET,
		    USBC_PD_T_PS_HARD_RESET_MS);
		return;
	}
	if (events & USBC_PD_E_TIMER)
		usbc_pd_policy_set_state(p, USBC_PD_S_SNK_HARD_RESET_WAIT_VBUS);
}

/*
 * SNK_HARD_RESET_WAIT_VBUS: wait for VBUS to disappear and reappear
 * (source is doing its own VBUS-off / VBUS-on dance).  Driven by
 * VBUS_CHANGE events; tNoResponse caps the wait so we don't get
 * stuck if the source died.
 */
static void
usbc_pd_state_snk_hard_reset_wait_vbus(struct usbc_pd_policy *p,
    uint32_t events)
{
	bool vbus;

	POLICY_ASSERT_LOCKED(p);

	if (p->prev_state != USBC_PD_S_SNK_HARD_RESET_WAIT_VBUS) {
		p->prev_state = USBC_PD_S_SNK_HARD_RESET_WAIT_VBUS;
		usbc_pd_policy_arm_timer(p, USBC_PD_T_NO_RESPONSE,
		    USBC_PD_T_NO_RESPONSE_MS);
		return;
	}
	if (events & USBC_PD_E_VBUS_CHANGE) {
		vbus = false;
		if (p->tcpc->ops->get_vbus_present != NULL)
			(void)p->tcpc->ops->get_vbus_present(p->tcpc, &vbus);
		if (vbus)
			usbc_pd_policy_set_state(p,
			    USBC_PD_S_SNK_HARD_RESET_SINK_ON);
		return;
	}
	if (events & USBC_PD_E_TIMER)
		usbc_pd_policy_set_state(p, USBC_PD_S_UNATTACHED_SNK);
}

/*
 * SNK_HARD_RESET_SINK_ON: VBUS came back, redo PD bring-up by
 * jumping back to SNK_STARTUP.
 */
static void
usbc_pd_state_snk_hard_reset_sink_on(struct usbc_pd_policy *p, uint32_t events)
{

	POLICY_ASSERT_LOCKED(p);
	(void)events;

	usbc_pd_policy_set_state(p, USBC_PD_S_SNK_STARTUP);
}

/*
 * Build and transmit a Structured VDM as a Vendor data message.
 * Always sent SOP (port-to-port).  The chip's TX engine handles
 * GoodCRC; the policy waits for TX_OK / TX_FAIL events.
 */
static int
usbc_pd_send_vdm(struct usbc_pd_policy *p, uint16_t svid, uint8_t cmd,
    uint8_t obj_pos, const uint32_t *vdos, uint8_t vdo_count)
{
	enum usbc_pd_power_role pr;
	enum usbc_pd_data_role dr;

	POLICY_ASSERT_LOCKED(p);

	if (p->tcpc->ops->transmit == NULL)
		return (ENXIO);

	/* DFP/SRC for our typical RP64 flow; if we're SNK, use UFP. */
	pr = (p->state >= USBC_PD_S_SRC_ATTACHED &&
	    p->state <= USBC_PD_S_SRC_WAIT_NEW_CAPABILITIES) ?
	    USBC_PD_ROLE_SOURCE : USBC_PD_ROLE_SINK;
	dr = (pr == USBC_PD_ROLE_SOURCE) ? USBC_PD_DATA_DFP : USBC_PD_DATA_UFP;

	if (!usbc_pd_msg_vdm(&p->tx, svid, cmd, USBC_VDM_CMDTYPE_REQ,
	    obj_pos, vdos, vdo_count, p->msg_id, pr, dr,
	    p->caps.advertise_rev))
		return (EINVAL);
	return (p->tcpc->ops->transmit(p->tcpc, USBC_PD_SOP, &p->tx));
}

/*
 * Handle an unsolicited Attention VDM received outside of any
 * specific Discover/Enter state.  The DP partner uses Attention
 * (cmd=0x06 with a DP_Status VDO payload) to tell us "HPD changed"
 * or "USB/DP preference changed".  We just store the fresh status
 * and stay in DP_READY; consumers (cdn_dp) read dp_status via
 * usbc_pd_policy_dp_status().
 */
static void
usbc_pd_handle_attention(struct usbc_pd_policy *p,
    const struct usbc_pd_msg *rx)
{
	uint16_t svid;
	uint8_t cmd, ndo;

	POLICY_ASSERT_LOCKED(p);

	ndo = USBC_PD_HDR_GET_NDO(rx->hdr);
	if (ndo < 1)
		return;
	svid = USBC_VDM_HDR_SVID(rx->data[0]);
	cmd = USBC_VDM_HDR_GET_CMD(rx->data[0]);
	if (svid != USBC_VDM_SVID_DP || cmd != USBC_VDM_CMD_ATTENTION)
		return;
	if (ndo >= 2)
		p->dp_status = rx->data[1];
}

/*
 * Pick the caps-phase state appropriate for our current role.  Used
 * by both SOFT_RESET branches to decide where to land after the
 * reset exchange completes.  Falls back to HARD_RESET_SEND if the
 * pre-reset state was outside the attached SRC/SNK ranges (which
 * shouldn't happen, but is a safer recovery than picking blindly).
 */
static enum usbc_pd_state
usbc_pd_caps_state_for_role(const struct usbc_pd_policy *p)
{
	enum usbc_pd_state s = p->pre_reset_state;

	if (s >= USBC_PD_S_SRC_ATTACHED &&
	    s <= USBC_PD_S_SRC_WAIT_NEW_CAPABILITIES)
		return (USBC_PD_S_SRC_SEND_CAPABILITIES);
	if (s >= USBC_PD_S_SNK_ATTACHED && s <= USBC_PD_S_SNK_READY)
		return (USBC_PD_S_SNK_WAIT_CAPABILITIES);
	return (USBC_PD_S_HARD_RESET_SEND);
}

/*
 * Detect a SoftReset control message just pulled off the RX queue
 * and route to USBC_PD_S_SOFT_RESET if so.  Caller passes the
 * already-drained message; if this returns true the caller must
 * stop processing and return.  The pre-reset state is captured so
 * the SOFT_RESET handler can return us to the right caps state.
 */
static bool
usbc_pd_check_soft_reset_rx(struct usbc_pd_policy *p,
    const struct usbc_pd_msg *rx, enum usbc_pd_sop sop)
{

	POLICY_ASSERT_LOCKED(p);

	if (sop != USBC_PD_SOP)
		return (false);
	if (USBC_PD_HDR_GET_NDO(rx->hdr) != 0)
		return (false);
	if (USBC_PD_HDR_GET_TYPE(rx->hdr) != USBC_PD_CTRL_SOFT_RESET)
		return (false);

	p->pre_reset_state = p->state;
	usbc_pd_policy_set_state(p, USBC_PD_S_SOFT_RESET);
	return (true);
}

/*
 * SOFT_RESET (received): partner asked us to soft-reset.  Per PD
 * 6.8.2.1: respond with Accept, reset MessageIDCounter, and resume
 * the caps phase appropriate to our current role.  We don't gate
 * VBUS or the contract — only message-level state is reset.
 */
static void
usbc_pd_state_soft_reset(struct usbc_pd_policy *p, uint32_t events)
{

	POLICY_ASSERT_LOCKED(p);

	if (p->prev_state != USBC_PD_S_SOFT_RESET) {
		p->prev_state = USBC_PD_S_SOFT_RESET;
		p->msg_id = 0;
		if (usbc_pd_send_ctrl(p, USBC_PD_CTRL_ACCEPT) != 0) {
			usbc_pd_policy_set_state(p,
			    USBC_PD_S_HARD_RESET_SEND);
			return;
		}
		return;
	}
	if (events & USBC_PD_E_TX_OK) {
		p->msg_id = (p->msg_id + 1) & 0x7;
		usbc_pd_policy_set_state(p,
		    usbc_pd_caps_state_for_role(p));
		return;
	}
	if (events & USBC_PD_E_TX_FAIL)
		usbc_pd_policy_set_state(p, USBC_PD_S_HARD_RESET_SEND);
}

/*
 * SOFT_RESET_SEND (initiated): we want to soft-reset the partner.
 * Send SoftReset, wait for Accept under tSenderResponse.  On
 * Accept, reset MessageIDCounter and resume caps; otherwise
 * escalate to a hard reset.
 */
static void
usbc_pd_state_soft_reset_send(struct usbc_pd_policy *p, uint32_t events)
{
	struct usbc_pd_msg rx;
	enum usbc_pd_sop sop;
	uint8_t type;

	POLICY_ASSERT_LOCKED(p);

	if (p->prev_state != USBC_PD_S_SOFT_RESET_SEND) {
		p->prev_state = USBC_PD_S_SOFT_RESET_SEND;
		if (usbc_pd_send_ctrl(p, USBC_PD_CTRL_SOFT_RESET) != 0) {
			usbc_pd_policy_set_state(p,
			    USBC_PD_S_HARD_RESET_SEND);
			return;
		}
		return;
	}
	if (events & USBC_PD_E_TX_OK) {
		p->msg_id = (p->msg_id + 1) & 0x7;
		usbc_pd_policy_arm_timer(p, USBC_PD_T_SENDER_RESPONSE,
		    USBC_PD_T_SENDER_RESPONSE_MS);
		return;
	}
	if (events & USBC_PD_E_TX_FAIL) {
		usbc_pd_policy_set_state(p, USBC_PD_S_HARD_RESET_SEND);
		return;
	}
	if (events & USBC_PD_E_RX) {
		if (usbc_pd_drain_rx(p, &rx, &sop) != 0)
			return;
		if (sop != USBC_PD_SOP)
			return;
		if (USBC_PD_HDR_GET_NDO(rx.hdr) != 0)
			return;
		type = USBC_PD_HDR_GET_TYPE(rx.hdr);
		if (type == USBC_PD_CTRL_ACCEPT) {
			p->msg_id = 0;
			usbc_pd_policy_set_state(p,
			    usbc_pd_caps_state_for_role(p));
		} else {
			usbc_pd_policy_set_state(p,
			    USBC_PD_S_HARD_RESET_SEND);
		}
		return;
	}
	if (events & USBC_PD_E_TIMER)
		usbc_pd_policy_set_state(p, USBC_PD_S_HARD_RESET_SEND);
}

/*
 * Common VDM-response receive path.  Drains one RX, validates it as
 * the expected (svid, cmd) ACK/NAK/BUSY response.  Returns:
 *   1  = ACK received, vdo_out filled with data[1..ndo-1]
 *   0  = NAK or unexpected/non-VDM message; caller should advance to
 *        a degraded state (DP_READY without DP)
 *  -1  = no message available yet (caller should keep waiting)
 *  -2  = BUSY response; caller may retry after a short delay
 *
 * Attention messages received during a discover sequence are
 * processed (dp_status updated) and reported as "no message yet" so
 * the original wait continues.
 */
static int
usbc_pd_recv_vdm_response(struct usbc_pd_policy *p, uint16_t expect_svid,
    uint8_t expect_cmd, uint32_t *vdo_out, uint8_t *vdo_count_out)
{
	struct usbc_pd_msg rx;
	enum usbc_pd_sop sop;
	uint16_t got_svid;
	uint8_t got_cmd, cmd_type, ndo, i;

	POLICY_ASSERT_LOCKED(p);

	if (usbc_pd_drain_rx(p, &rx, &sop) != 0)
		return (-1);
	if (usbc_pd_check_soft_reset_rx(p, &rx, sop))
		return (-1);
	if (sop != USBC_PD_SOP)
		return (-1);
	ndo = USBC_PD_HDR_GET_NDO(rx.hdr);
	if (ndo < 1)
		return (-1);
	if (USBC_PD_HDR_GET_TYPE(rx.hdr) != USBC_PD_DATA_VENDOR)
		return (-1);

	got_svid = USBC_VDM_HDR_SVID(rx.data[0]);
	got_cmd = USBC_VDM_HDR_GET_CMD(rx.data[0]);
	cmd_type = USBC_VDM_HDR_GET_CMDTYPE(rx.data[0]);

	/* Attention can arrive at any time; handle it inline and keep
	 * waiting for our actual response. */
	if (got_svid == USBC_VDM_SVID_DP &&
	    got_cmd == USBC_VDM_CMD_ATTENTION) {
		usbc_pd_handle_attention(p, &rx);
		return (-1);
	}

	if (got_svid != expect_svid || got_cmd != expect_cmd)
		return (-1);

	if (cmd_type == USBC_VDM_CMDTYPE_BUSY)
		return (-2);
	if (cmd_type != USBC_VDM_CMDTYPE_ACK)
		return (0);

	if (vdo_out != NULL && vdo_count_out != NULL) {
		uint8_t n = (ndo > 1) ? ndo - 1 : 0;
		if (n > 6)
			n = 6;
		for (i = 0; i < n; i++)
			vdo_out[i] = rx.data[i + 1];
		*vdo_count_out = n;
	}
	return (1);
}

/*
 * VDM_SEND_DISCOVER_IDENTITY: send Discover_Identity REQ to SOP, wait
 * for ACK.  On ACK, the response contains 4 VDOs: ID Header, Cert
 * Stat, Product VDO, AMA VDO (or Product Type VDO).  We just store
 * them and advance to DISCOVER_SVIDS.  On NAK or timeout, give up on
 * Alt Mode and sit in DP_READY (without DP) — the partner is PD-only.
 */
static void
usbc_pd_state_vdm_discover_identity(struct usbc_pd_policy *p, uint32_t events)
{
	uint32_t vdos[6];
	uint8_t vdo_count;
	int rc;

	POLICY_ASSERT_LOCKED(p);

	if (p->prev_state != USBC_PD_S_VDM_SEND_DISCOVER_IDENTITY) {
		p->prev_state = USBC_PD_S_VDM_SEND_DISCOVER_IDENTITY;
		if (usbc_pd_send_vdm(p, USBC_VDM_SVID_PD,
		    USBC_VDM_CMD_DISCOVER_IDENTITY, 0, NULL, 0) != 0) {
			usbc_pd_policy_set_state(p, USBC_PD_S_VDM_DP_READY);
			return;
		}
		return;
	}
	if (events & USBC_PD_E_TX_OK) {
		p->msg_id = (p->msg_id + 1) & 0x7;
		usbc_pd_policy_arm_timer(p, USBC_PD_T_VDM_SENDER_RESPONSE,
		    USBC_PD_T_SENDER_RESPONSE_MS);
		return;
	}
	if (events & USBC_PD_E_TX_FAIL) {
		usbc_pd_policy_set_state(p, USBC_PD_S_VDM_DP_READY);
		return;
	}
	if (events & USBC_PD_E_RX) {
		rc = usbc_pd_recv_vdm_response(p, USBC_VDM_SVID_PD,
		    USBC_VDM_CMD_DISCOVER_IDENTITY, vdos, &vdo_count);
		if (rc == -1)
			return;	/* keep waiting */
		if (rc <= 0) {
			usbc_pd_policy_set_state(p,
			    USBC_PD_S_VDM_DP_READY);
			return;
		}
		if (vdo_count >= 1)
			p->dp_partner_id_hdr = vdos[0];
		if (vdo_count >= 2)
			p->dp_partner_cert_stat = vdos[1];
		if (vdo_count >= 3)
			p->dp_partner_product = vdos[2];
		usbc_pd_policy_set_state(p,
		    USBC_PD_S_VDM_SEND_DISCOVER_SVIDS);
		return;
	}
	if (events & USBC_PD_E_TIMER)
		usbc_pd_policy_set_state(p, USBC_PD_S_VDM_DP_READY);
}

/*
 * VDM_SEND_DISCOVER_SVIDS: send Discover_SVIDs REQ.  ACK contains up
 * to 6 VDOs, each holding two 16-bit SVIDs (high then low half).
 * Walk them looking for the DisplayPort SVID 0xff01; when found,
 * remember it and advance to DISCOVER_MODES.  If we get an ACK with
 * no DP SVID present, give up cleanly.
 */
static void
usbc_pd_state_vdm_discover_svids(struct usbc_pd_policy *p, uint32_t events)
{
	uint32_t vdos[6];
	uint8_t vdo_count, i;
	uint16_t hi, lo;
	bool dp_found;
	int rc;

	POLICY_ASSERT_LOCKED(p);

	if (p->prev_state != USBC_PD_S_VDM_SEND_DISCOVER_SVIDS) {
		p->prev_state = USBC_PD_S_VDM_SEND_DISCOVER_SVIDS;
		if (usbc_pd_send_vdm(p, USBC_VDM_SVID_PD,
		    USBC_VDM_CMD_DISCOVER_SVIDS, 0, NULL, 0) != 0) {
			usbc_pd_policy_set_state(p, USBC_PD_S_VDM_DP_READY);
			return;
		}
		return;
	}
	if (events & USBC_PD_E_TX_OK) {
		p->msg_id = (p->msg_id + 1) & 0x7;
		usbc_pd_policy_arm_timer(p, USBC_PD_T_VDM_SENDER_RESPONSE,
		    USBC_PD_T_SENDER_RESPONSE_MS);
		return;
	}
	if (events & USBC_PD_E_TX_FAIL) {
		usbc_pd_policy_set_state(p, USBC_PD_S_VDM_DP_READY);
		return;
	}
	if (events & USBC_PD_E_RX) {
		rc = usbc_pd_recv_vdm_response(p, USBC_VDM_SVID_PD,
		    USBC_VDM_CMD_DISCOVER_SVIDS, vdos, &vdo_count);
		if (rc == -1)
			return;
		if (rc <= 0) {
			usbc_pd_policy_set_state(p,
			    USBC_PD_S_VDM_DP_READY);
			return;
		}
		dp_found = false;
		p->dp_partner_svid_count = 0;
		for (i = 0; i < vdo_count; i++) {
			hi = (uint16_t)(vdos[i] >> 16);
			lo = (uint16_t)(vdos[i] & 0xffff);
			if (hi != 0 && p->dp_partner_svid_count <
			    nitems(p->dp_partner_svids))
				p->dp_partner_svids[p->dp_partner_svid_count++]
				    = hi;
			if (lo != 0 && p->dp_partner_svid_count <
			    nitems(p->dp_partner_svids))
				p->dp_partner_svids[p->dp_partner_svid_count++]
				    = lo;
			if (hi == USBC_VDM_SVID_DP || lo == USBC_VDM_SVID_DP)
				dp_found = true;
		}
		if (!dp_found) {
			usbc_pd_policy_set_state(p,
			    USBC_PD_S_VDM_DP_READY);
			return;
		}
		usbc_pd_policy_set_state(p,
		    USBC_PD_S_VDM_SEND_DISCOVER_MODES);
		return;
	}
	if (events & USBC_PD_E_TIMER)
		usbc_pd_policy_set_state(p, USBC_PD_S_VDM_DP_READY);
}

/*
 * VDM_SEND_DISCOVER_MODES: send Discover_Modes REQ for SVID 0xff01.
 * ACK contains 1..6 32-bit DP Mode VDOs.  We pick the first one (its
 * 1-based object position is index+1) and advance to ENTER_MODE.
 */
static void
usbc_pd_state_vdm_discover_modes(struct usbc_pd_policy *p, uint32_t events)
{
	uint32_t vdos[6];
	uint8_t vdo_count, i;
	int rc;

	POLICY_ASSERT_LOCKED(p);

	if (p->prev_state != USBC_PD_S_VDM_SEND_DISCOVER_MODES) {
		p->prev_state = USBC_PD_S_VDM_SEND_DISCOVER_MODES;
		if (usbc_pd_send_vdm(p, USBC_VDM_SVID_DP,
		    USBC_VDM_CMD_DISCOVER_MODES, 0, NULL, 0) != 0) {
			usbc_pd_policy_set_state(p, USBC_PD_S_VDM_DP_READY);
			return;
		}
		return;
	}
	if (events & USBC_PD_E_TX_OK) {
		p->msg_id = (p->msg_id + 1) & 0x7;
		usbc_pd_policy_arm_timer(p, USBC_PD_T_VDM_SENDER_RESPONSE,
		    USBC_PD_T_SENDER_RESPONSE_MS);
		return;
	}
	if (events & USBC_PD_E_TX_FAIL) {
		usbc_pd_policy_set_state(p, USBC_PD_S_VDM_DP_READY);
		return;
	}
	if (events & USBC_PD_E_RX) {
		rc = usbc_pd_recv_vdm_response(p, USBC_VDM_SVID_DP,
		    USBC_VDM_CMD_DISCOVER_MODES, vdos, &vdo_count);
		if (rc == -1)
			return;
		if (rc <= 0 || vdo_count == 0) {
			usbc_pd_policy_set_state(p,
			    USBC_PD_S_VDM_DP_READY);
			return;
		}
		p->dp_partner_mode_count = vdo_count;
		for (i = 0; i < vdo_count; i++)
			p->dp_partner_modes[i] = vdos[i];
		p->dp_object_position = 1;	/* first mode */
		usbc_pd_policy_set_state(p,
		    USBC_PD_S_VDM_SEND_ENTER_MODE);
		return;
	}
	if (events & USBC_PD_E_TIMER)
		usbc_pd_policy_set_state(p, USBC_PD_S_VDM_DP_READY);
}

/*
 * VDM_SEND_ENTER_MODE: send Enter_Mode REQ for SVID 0xff01 with the
 * chosen object position.  On ACK, the partner has entered DP Alt
 * Mode; advance to DP_STATUS to query the runtime DP state.
 */
static void
usbc_pd_state_vdm_enter_mode(struct usbc_pd_policy *p, uint32_t events)
{
	int rc;

	POLICY_ASSERT_LOCKED(p);

	if (p->prev_state != USBC_PD_S_VDM_SEND_ENTER_MODE) {
		p->prev_state = USBC_PD_S_VDM_SEND_ENTER_MODE;
		if (usbc_pd_send_vdm(p, USBC_VDM_SVID_DP,
		    USBC_VDM_CMD_ENTER_MODE,
		    p->dp_object_position, NULL, 0) != 0) {
			usbc_pd_policy_set_state(p, USBC_PD_S_VDM_DP_READY);
			return;
		}
		return;
	}
	if (events & USBC_PD_E_TX_OK) {
		p->msg_id = (p->msg_id + 1) & 0x7;
		usbc_pd_policy_arm_timer(p, USBC_PD_T_VDM_SENDER_RESPONSE,
		    USBC_PD_T_SENDER_RESPONSE_MS);
		return;
	}
	if (events & USBC_PD_E_TX_FAIL) {
		usbc_pd_policy_set_state(p, USBC_PD_S_VDM_DP_READY);
		return;
	}
	if (events & USBC_PD_E_RX) {
		rc = usbc_pd_recv_vdm_response(p, USBC_VDM_SVID_DP,
		    USBC_VDM_CMD_ENTER_MODE, NULL, NULL);
		if (rc == -1)
			return;
		if (rc <= 0) {
			usbc_pd_policy_set_state(p,
			    USBC_PD_S_VDM_DP_READY);
			return;
		}
		p->dp_ready = true;
		usbc_pd_policy_set_state(p,
		    USBC_PD_S_VDM_SEND_DP_STATUS);
		return;
	}
	if (events & USBC_PD_E_TIMER)
		usbc_pd_policy_set_state(p, USBC_PD_S_VDM_DP_READY);
}

/*
 * VDM_SEND_DP_STATUS: send DP_Status_Update REQ.  ACK carries one
 * DP_Status VDO with the HPD bit and other receiver-state info.  We
 * stash it in p->dp_status and advance to DP_CONFIGURE the first
 * time, or back to DP_READY thereafter.
 */
static void
usbc_pd_state_vdm_dp_status(struct usbc_pd_policy *p, uint32_t events)
{
	uint32_t vdos[6];
	uint8_t vdo_count;
	bool first_time;
	int rc;

	POLICY_ASSERT_LOCKED(p);

	if (p->prev_state != USBC_PD_S_VDM_SEND_DP_STATUS) {
		p->prev_state = USBC_PD_S_VDM_SEND_DP_STATUS;
		/* Status_Update REQ payload: one VDO with our DP-side
		 * status bits.  PD DP Alt Mode 2.0: bits 1,3 = "DFP_D
		 * Connected" + "Enabled" — the source telling the sink
		 * that we're acting as a DP source. */
		vdos[0] = USBC_DP_STATUS_DFP_D_CONN | USBC_DP_STATUS_ENABLED;
		if (usbc_pd_send_vdm(p, USBC_VDM_SVID_DP,
		    USBC_DP_CMD_STATUS_UPDATE, 0, vdos, 1) != 0) {
			usbc_pd_policy_set_state(p, USBC_PD_S_VDM_DP_READY);
			return;
		}
		return;
	}
	if (events & USBC_PD_E_TX_OK) {
		p->msg_id = (p->msg_id + 1) & 0x7;
		usbc_pd_policy_arm_timer(p, USBC_PD_T_VDM_SENDER_RESPONSE,
		    USBC_PD_T_SENDER_RESPONSE_MS);
		return;
	}
	if (events & USBC_PD_E_TX_FAIL) {
		usbc_pd_policy_set_state(p, USBC_PD_S_VDM_DP_READY);
		return;
	}
	if (events & USBC_PD_E_RX) {
		rc = usbc_pd_recv_vdm_response(p, USBC_VDM_SVID_DP,
		    USBC_DP_CMD_STATUS_UPDATE, vdos, &vdo_count);
		if (rc == -1)
			return;
		first_time = (p->dp_status == 0);
		if (rc > 0 && vdo_count >= 1)
			p->dp_status = vdos[0];
		if (first_time) {
			usbc_pd_policy_set_state(p,
			    USBC_PD_S_VDM_SEND_DP_CONFIGURE);
		} else {
			usbc_pd_policy_set_state(p,
			    USBC_PD_S_VDM_DP_READY);
		}
		return;
	}
	if (events & USBC_PD_E_TIMER)
		usbc_pd_policy_set_state(p, USBC_PD_S_VDM_DP_READY);
}

/*
 * VDM_SEND_DP_CONFIGURE: send DP_Configure REQ with our chosen pin
 * assignment.  Default to "C" (4-lane DP, no USB SS) which the RK3399
 * DP TX is wired for; future commits can pick a pin assignment based
 * on partner capabilities + USB-SS preference.
 */
static void
usbc_pd_state_vdm_dp_configure(struct usbc_pd_policy *p, uint32_t events)
{
	uint32_t vdos[1];
	int rc;

	POLICY_ASSERT_LOCKED(p);

	if (p->prev_state != USBC_PD_S_VDM_SEND_DP_CONFIGURE) {
		p->prev_state = USBC_PD_S_VDM_SEND_DP_CONFIGURE;
		/*
		 * DP_Configure VDO bits:
		 *   1:0  Configure (1 = UFP_U as DFP_D, 2 = as UFP_D)
		 *   7:2  reserved
		 *  15:8  DFP_D Pin Assignment (bit set = pin A..F)
		 * 23:16  UFP_D Pin Assignment
		 *  We're DFP_D, requesting Pin C (bit 2 in the DFP_D
		 *  field = 0x04).
		 */
		vdos[0] = (1u << 0) /* UFP_U as DFP_D */ |
		    (0x04u << 8) /* DFP_D Pin C */;
		if (usbc_pd_send_vdm(p, USBC_VDM_SVID_DP,
		    USBC_DP_CMD_CONFIGURE, 0, vdos, 1) != 0) {
			usbc_pd_policy_set_state(p, USBC_PD_S_VDM_DP_READY);
			return;
		}
		return;
	}
	if (events & USBC_PD_E_TX_OK) {
		p->msg_id = (p->msg_id + 1) & 0x7;
		usbc_pd_policy_arm_timer(p, USBC_PD_T_VDM_SENDER_RESPONSE,
		    USBC_PD_T_SENDER_RESPONSE_MS);
		return;
	}
	if (events & USBC_PD_E_TX_FAIL) {
		usbc_pd_policy_set_state(p, USBC_PD_S_VDM_DP_READY);
		return;
	}
	if (events & USBC_PD_E_RX) {
		rc = usbc_pd_recv_vdm_response(p, USBC_VDM_SVID_DP,
		    USBC_DP_CMD_CONFIGURE, NULL, NULL);
		if (rc == -1)
			return;
		usbc_pd_policy_set_state(p, USBC_PD_S_VDM_DP_READY);
		return;
	}
	if (events & USBC_PD_E_TIMER)
		usbc_pd_policy_set_state(p, USBC_PD_S_VDM_DP_READY);
}

/*
 * VDM_DP_READY: terminal state for the DP discovery chain.  Whether
 * we got here cleanly (Enter_Mode + DP_Configure ACK'd, dp_ready=true)
 * or via a degraded path (NAK / partner doesn't speak DP, dp_ready=
 * false), the policy sits here handling Attention messages and
 * occasionally re-polling DP_Status_Update.
 *
 * Re-poll cadence: 1s tick.  Keeps the partner's PD/DP stack alive
 * (Linux's tcpm does similar).
 */
static void
usbc_pd_state_vdm_dp_ready(struct usbc_pd_policy *p, uint32_t events)
{
	struct usbc_pd_msg rx;
	enum usbc_pd_sop sop;

	POLICY_ASSERT_LOCKED(p);

	if (p->prev_state != USBC_PD_S_VDM_DP_READY) {
		p->prev_state = USBC_PD_S_VDM_DP_READY;
		if (p->dp_ready) {
			/* Arm a 1s polling timer for periodic DP_Status. */
			usbc_pd_policy_arm_timer(p,
			    USBC_PD_T_VDM_SENDER_RESPONSE, 1000);
		}
		return;
	}
	if (events & USBC_PD_E_RX) {
		if (usbc_pd_drain_rx(p, &rx, &sop) != 0)
			return;
		if (usbc_pd_check_soft_reset_rx(p, &rx, sop))
			return;
		usbc_pd_handle_attention(p, &rx);
		return;
	}
	if (events & USBC_PD_E_TIMER) {
		if (p->dp_ready) {
			/* Force re-entry into DP_STATUS by clearing
			 * prev_state and bouncing through. */
			usbc_pd_policy_set_state(p,
			    USBC_PD_S_VDM_SEND_DP_STATUS);
		}
	}
}

/* Build and submit a control message of the given type. */
static int
usbc_pd_send_ctrl(struct usbc_pd_policy *p, uint8_t ctrl_type)
{
	enum usbc_pd_power_role pr;

	POLICY_ASSERT_LOCKED(p);

	if (p->tcpc->ops->transmit == NULL)
		return (ENXIO);
	pr = (p->state >= USBC_PD_S_SRC_ATTACHED &&
	    p->state <= USBC_PD_S_SRC_WAIT_NEW_CAPABILITIES) ?
	    USBC_PD_ROLE_SOURCE : USBC_PD_ROLE_SINK;
	if (!usbc_pd_msg_ctrl(&p->tx, ctrl_type, p->msg_id, pr,
	    USBC_PD_DATA_DFP, p->caps.advertise_rev))
		return (EINVAL);
	return (p->tcpc->ops->transmit(p->tcpc, USBC_PD_SOP, &p->tx));
}

/* Pull one message from the TCPC RX queue.  Returns 0 on success. */
static int
usbc_pd_drain_rx(struct usbc_pd_policy *p, struct usbc_pd_msg *out,
    enum usbc_pd_sop *sop_out)
{

	POLICY_ASSERT_LOCKED(p);
	if (p->tcpc->ops->receive == NULL)
		return (ENXIO);
	return (p->tcpc->ops->receive(p->tcpc, sop_out, out));
}

MODULE_VERSION(usbc, 1);
