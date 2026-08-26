/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2001-2024, Intel Corporation
 * Copyright (c) 2026 Kevin Bowling <kbowling@FreeBSD.org>
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
 */

#include "if_em.h"

#include <sys/sbuf.h>

#define	IGBV_82576_QUEUES		2
#define	IGBV_I350_QUEUES		1
#define	IGBV_MAX_MAC_FILTERS	3
#define	IGBV_QUEUE_DISABLE_BUSY_RETRIES	10
#define	IGBV_QUEUE_DISABLE_DELAY_US	10
#define	IGBV_QUEUE_DISABLE_PAUSE	(100 * SBT_1US)
#define	IGBV_QUEUE_DISABLE_RETRIES	20
#define	IGBV_QUEUE_SANITIZE_ATTEMPTS	3
#define	IGBV_VLAN_RETRY_BATCH	4
#define	IGBV_VLAN_RETRY_WINDOW	(8 * SBT_1S)

static const struct timeval igbv_queue_log_interval = { 2, 0 };
static const struct timeval igbv_mbx_log_interval = { 60, 0 };
static const sbintime_t igbv_queue_retry_delay[] = {
	100 * SBT_1MS,
	500 * SBT_1MS,
};
static const sbintime_t igbv_mbx_retry_delay[] = {
	250 * SBT_1MS,
	1 * SBT_1S,
	4 * SBT_1S,
	8 * SBT_1S,
};
_Static_assert(nitems(igbv_queue_retry_delay) + 1 ==
    IGBV_QUEUE_SANITIZE_ATTEMPTS, "missing queue retry delay");

struct igb_vf_uc_addr_list {
	struct e1000_softc	*sc;
	u8			addrs[IGBV_MAX_MAC_FILTERS][ETHER_ADDR_LEN];
};

static bool	igbv_tx_pending(struct e1000_softc *);
static bool	igbv_vlan_retry_pending(const struct e1000_softc *);
static void	igbv_vlan_retry_tick(struct e1000_softc *);

static void
igbv_queue_retry_callout(void *arg)
{
	struct e1000_softc *sc;
	if_t ifp;

	sc = arg;
	if (atomic_readandclear_32(&sc->vf_queue_retry_pending) == 0)
		return;
	ifp = iflib_get_ifp(sc->ctx);
	if ((if_getflags(ifp) & IFF_UP) == 0) {
		atomic_set_32(&sc->vf_queue_retry_new_epoch, 1);
		return;
	}
	iflib_request_reset_if_up(sc->ctx);
	iflib_admin_intr_deferred(sc->ctx);
}

/*
 * A missing PF can make the posted reset handshake wait for a full mailbox
 * timeout.  Keep that work out of stopped status paths.  An administratively
 * up VF retries complete initialization with an exponential delay capped at
 * eight seconds, so it recovers without creating a tight mailbox poller.
 */
static void
igbv_mbx_retry_callout(void *arg)
{
	struct e1000_softc *sc;
	if_t ifp;

	sc = arg;
	if (atomic_readandclear_32(&sc->vf_mbx_retry_pending) == 0 ||
	    atomic_load_acq_32(&sc->vf_mbx_ready) != 0 ||
	    iflib_in_detach(sc->ctx))
		return;
	ifp = iflib_get_ifp(sc->ctx);
	if ((if_getflags(ifp) & IFF_UP) == 0)
		return;

	iflib_request_reset_if_up(sc->ctx);
	iflib_admin_intr_deferred(sc->ctx);
}

static const char *
igbv_reset_error_desc(s32 error)
{

	switch (error) {
	case -E1000_ERR_RESET:
		return ("PF reset acknowledgement timed out");
	case -E1000_ERR_MAC_INIT:
		return ("PF returned an invalid VF reset response");
	case -E1000_ERR_MBX:
		return ("PF mailbox reset exchange failed");
	default:
		return ("VF reset handshake failed");
	}
}

void
igbv_log_reset_failure(struct e1000_softc *sc, s32 error, bool attaching)
{

	/* Report each backoff stage, then limit the steady eight-second retry. */
	if (sc->vf_mbx_retry_stage == nitems(igbv_mbx_retry_delay) - 1 &&
	    !ratecheck(&sc->vf_last_mbx_log, &igbv_mbx_log_interval))
		return;
	device_printf(sc->dev, "%s (%d)%s\n", igbv_reset_error_desc(error),
	    error, attaching ? "; continuing attach" : "");
}

void
igbv_mbx_retry_detach(struct e1000_softc *sc)
{

	if (!sc->vf_mbx_retry_initialized)
		return;
	atomic_readandclear_32(&sc->vf_mbx_retry_pending);
	callout_drain(&sc->vf_mbx_retry);
	sc->vf_mbx_retry_initialized = false;
}

void
igbv_mbx_retry_prepare(struct e1000_softc *sc)
{

	if (!sc->vf_mbx_retry_initialized)
		return;
	atomic_readandclear_32(&sc->vf_mbx_retry_pending);
	callout_drain(&sc->vf_mbx_retry);
}

void
igbv_mbx_retry_stop(struct e1000_softc *sc)
{
	if_t ifp;

	if (!sc->vf_mbx_retry_initialized)
		return;
	atomic_readandclear_32(&sc->vf_mbx_retry_pending);
	callout_drain(&sc->vf_mbx_retry);
	ifp = iflib_get_ifp(sc->ctx);
	if ((if_getflags(ifp) & IFF_UP) == 0)
		sc->vf_mbx_retry_stage = 0;
}

void
igbv_mbx_retry_failed(if_ctx_t ctx)
{
	struct e1000_softc *sc;
	if_t ifp;
	sbintime_t delay;
	u_int stage;

	sc = iflib_get_softc(ctx);
	atomic_store_rel_32(&sc->vf_mbx_ready, 0);
	sc->link_speed = 0;
	sc->link_duplex = 0;
	if (sc->link_state != EM_LINK_STATE_DOWN) {
		sc->link_state = EM_LINK_STATE_DOWN;
		iflib_link_state_change(ctx, LINK_STATE_DOWN, 0);
	}
	iflib_init_failed(ctx);

	ifp = iflib_get_ifp(ctx);
	if (!sc->vf_mbx_retry_initialized ||
	    (if_getflags(ifp) & IFF_UP) == 0)
		return;
	stage = sc->vf_mbx_retry_stage;
	if (stage >= nitems(igbv_mbx_retry_delay))
		stage = nitems(igbv_mbx_retry_delay) - 1;
	delay = igbv_mbx_retry_delay[stage];
	if (sc->vf_mbx_retry_stage + 1 < nitems(igbv_mbx_retry_delay))
		sc->vf_mbx_retry_stage++;
	atomic_set_32(&sc->vf_mbx_retry_pending, 1);
	callout_reset_sbt(&sc->vf_mbx_retry, delay, 0,
	    igbv_mbx_retry_callout, sc, C_PREL(1));
}

static void
igbv_mbx_retry_succeeded(struct e1000_softc *sc)
{

	atomic_store_rel_32(&sc->vf_mbx_ready, 1);
	atomic_readandclear_32(&sc->vf_mbx_retry_pending);
	if (sc->vf_mbx_retry_initialized)
		callout_stop(&sc->vf_mbx_retry);
	sc->vf_mbx_retry_stage = 0;
	sc->vf_last_mbx_log.tv_sec = 0;
	sc->vf_last_mbx_log.tv_usec = 0;
}

void
igbv_queue_retry_detach(struct e1000_softc *sc)
{

	if (!sc->vf_queue_retry_initialized)
		return;
	atomic_readandclear_32(&sc->vf_queue_retry_pending);
	callout_drain(&sc->vf_queue_retry);
	sc->vf_queue_retry_initialized = false;
}

void
igbv_queue_retry_stop(struct e1000_softc *sc)
{

	if (!sc->vf_queue_retry_initialized)
		return;
	if (atomic_readandclear_32(&sc->vf_queue_retry_pending) != 0)
		atomic_set_32(&sc->vf_queue_retry_new_epoch, 1);
	callout_stop(&sc->vf_queue_retry);
}

void
igbv_queue_retry_prepare(struct e1000_softc *sc)
{
	bool new_epoch;

	new_epoch =
	    atomic_readandclear_32(&sc->vf_queue_retry_new_epoch) != 0;
	if (!sc->vf_queue_gave_up && !new_epoch)
		return;
	sc->vf_queue_failures = 0;
	sc->vf_queue_gave_up = false;
}

static void
igbv_queue_retry_succeeded(struct e1000_softc *sc)
{

	atomic_readandclear_32(&sc->vf_queue_retry_pending);
	atomic_readandclear_32(&sc->vf_queue_retry_new_epoch);
	if (sc->vf_queue_retry_initialized)
		callout_stop(&sc->vf_queue_retry);
	sc->vf_queue_failures = 0;
	sc->vf_queue_gave_up = false;
}

void
igbv_queue_retry_failed(if_ctx_t ctx)
{
	struct e1000_softc *sc;
	sbintime_t delay;

	sc = iflib_get_softc(ctx);
	KASSERT(sc->vf_ifp, ("%s called for a PF", __func__));

	if (sc->vf_queue_failures < IGBV_QUEUE_SANITIZE_ATTEMPTS)
		sc->vf_queue_failures++;
	if (sc->vf_queue_failures < IGBV_QUEUE_SANITIZE_ATTEMPTS) {
		delay = igbv_queue_retry_delay[sc->vf_queue_failures - 1];
		atomic_set_32(&sc->vf_queue_retry_pending, 1);
		callout_reset_sbt(&sc->vf_queue_retry, delay, 0,
		    igbv_queue_retry_callout, sc, C_PREL(1));
	} else if (!sc->vf_queue_gave_up) {
		atomic_readandclear_32(&sc->vf_queue_retry_pending);
		callout_stop(&sc->vf_queue_retry);
		sc->vf_queue_gave_up = true;
		device_printf(sc->dev,
		    "retained VF queues remained active after %u attempts; "
		    "interface left down; toggle it down/up to retry\n",
		    sc->vf_queue_failures);
	}

	iflib_link_state_change(ctx, LINK_STATE_DOWN, 0);
	iflib_init_failed(ctx);
}

void
igbv_vlan_retry_add(struct e1000_softc *sc, u16 vid)
{
	bool pending;

	KASSERT(sc->vf_ifp, ("%s called for a PF", __func__));
	pending = igbv_vlan_retry_pending(sc);
	sc->vf_vfta_retry[vid >> 5] |= 1U << (vid & 0x1f);
	/* Bound the whole batch from its first failure, not each new VID. */
	if (!pending)
		sc->vf_vlan_retry_deadline =
		    getsbinuptime() + IGBV_VLAN_RETRY_WINDOW;
}

void
igbv_vlan_retry_clear(struct e1000_softc *sc, u16 vid)
{

	KASSERT(sc->vf_ifp, ("%s called for a PF", __func__));
	sc->vf_vfta_retry[vid >> 5] &= ~(1U << (vid & 0x1f));
}

static bool
igbv_vlan_retry_pending(const struct e1000_softc *sc)
{
	int i;

	for (i = 0; i < EM_VFTA_SIZE; i++)
		if (sc->vf_vfta_retry[i] != 0)
			return (true);
	return (false);
}

static void
igbv_vlan_retry_tick(struct e1000_softc *sc)
{
	u32 bit;
	u16 vid;
	int attempts, i, remaining;

	if (!igbv_vlan_retry_pending(sc)) {
		sc->vf_vlan_retry_deadline = 0;
		return;
	}
	if (getsbinuptime() >= sc->vf_vlan_retry_deadline) {
		remaining = 0;
		for (i = 0; i < EM_VFTA_SIZE; i++)
			remaining += bitcount32(sc->vf_vfta_retry[i]);
		memset(sc->vf_vfta_retry, 0, sizeof(sc->vf_vfta_retry));
		sc->vf_vlan_retry_deadline = 0;
		device_printf(sc->dev,
		    "VF VLAN restore retries exhausted for %d VIDs\n",
		    remaining);
		return;
	}

	/*
	 * The mailbox NACK does not distinguish a transient PF rate limit
	 * from permanent VLVF exhaustion.  Retry at the PF's sustained
	 * allowance, but bound the entire recovery window so ENOSPC cannot
	 * create a permanent mailbox poller.
	 */
	for (attempts = 0, i = 0;
	    attempts < IGBV_VLAN_RETRY_BATCH && i < 4096; i++) {
		vid = sc->vf_vlan_retry_cursor;
		sc->vf_vlan_retry_cursor = (vid + 1) & 0xfff;
		bit = 1U << (vid & 0x1f);
		if ((sc->vf_vfta_retry[vid >> 5] & bit) == 0)
			continue;
		attempts++;
		if ((sc->shadow_vfta[vid >> 5] & bit) == 0 ||
		    e1000_vfta_set_vf(&sc->hw, vid, true) ==
		    E1000_SUCCESS)
			sc->vf_vfta_retry[vid >> 5] &= ~bit;
	}
	if (!igbv_vlan_retry_pending(sc))
		sc->vf_vlan_retry_deadline = 0;
}

int
igbv_if_attach_pre(if_ctx_t ctx)
{
	struct e1000_softc *sc;
	device_t dev;
	int error;

	dev = iflib_get_dev(ctx);
	if (pci_msix_count(dev) < 2) {
		device_printf(dev, "VF operation requires two MSI-X vectors\n");
		return (ENXIO);
	}
	error = em_if_attach_pre(ctx);
	if (error != 0)
		return (error);

	sc = iflib_get_softc(ctx);
	callout_init(&sc->vf_queue_retry, 1);
	sc->vf_queue_retry_initialized = true;
	callout_init(&sc->vf_mbx_retry, 1);
	sc->vf_mbx_retry_initialized = true;

	KASSERT(sc->vf_ifp &&
	    (iflib_get_sctx(ctx)->isc_flags & IFLIB_IS_VF) != 0,
	    ("%s: igbv attached without VF policy", __func__));
	return (0);
}

int
igbv_if_attach_post(if_ctx_t ctx)
{
	struct e1000_softc *sc;
	int error;

	sc = iflib_get_softc(ctx);
	KASSERT(sc->vf_ifp, ("%s called for a PF", __func__));
	if (sc->intr_type != IFLIB_INTR_MSIX) {
		device_printf(sc->dev, "VF operation requires MSI-X\n");
		return (ENXIO);
	}
	error = em_if_attach_post(ctx);
	if (error != 0)
		return (error);

	/*
	 * Attach failures can leave the device sysctl tree registered when
	 * hw.bus.disable_failed_devices is set.  Do not publish handlers with
	 * softc arguments until iflib has successfully allocated MSI-X.
	 */
	em_add_device_sysctls(sc);
	return (0);
}

int
igbv_if_media_change(if_ctx_t ctx __unused)
{

	return (EOPNOTSUPP);
}

void
igbv_if_update_admin_status(if_ctx_t ctx)
{
	struct e1000_softc *sc;
	struct e1000_hw *hw;
	device_t dev;
	bool link_check, timer_tick;

	sc = iflib_get_softc(ctx);
	hw = &sc->hw;
	dev = iflib_get_dev(ctx);
	KASSERT(sc->vf_ifp, ("%s called for a PF", __func__));

	if ((if_getdrvflags(iflib_get_ifp(ctx)) & IFF_DRV_RUNNING) == 0 ||
	    !sc->vf_queues_sanitized ||
	    atomic_load_acq_32(&sc->vf_mbx_ready) == 0) {
		if (sc->link_state != EM_LINK_STATE_DOWN) {
			sc->link_speed = 0;
			sc->link_duplex = 0;
			sc->link_state = EM_LINK_STATE_DOWN;
			iflib_link_state_change(ctx, LINK_STATE_DOWN, 0);
		}
		return;
	}

	if (!sc->vf_reset_pending &&
	    atomic_readandclear_32(&sc->promisc_pending) != 0)
		(void)em_if_set_promisc_impl(ctx,
		    if_getflags(iflib_get_ifp(ctx)));

	if (e1000_check_for_link(hw) != E1000_SUCCESS &&
	    !sc->vf_reset_pending) {
		sc->vf_reset_pending = true;
		iflib_request_reset(ctx);
		iflib_admin_intr_deferred(ctx);
	}
	link_check = !hw->mac.get_link_status;

	if (link_check &&
	    (sc->link_state == EM_LINK_STATE_DOWN ||
	    sc->link_state == EM_LINK_STATE_DOWN_RESET_PENDING)) {
		e1000_get_speed_and_duplex(hw, &sc->link_speed,
		    &sc->link_duplex);
		if (bootverbose)
			device_printf(dev, "Link is up %d Mbps %s\n",
			    sc->link_speed,
			    sc->link_duplex == FULL_DUPLEX ?
			    "Full Duplex" : "Half Duplex");
		sc->link_state = EM_LINK_STATE_UP;
		iflib_link_state_change(ctx, LINK_STATE_UP,
		    IF_Mbps(sc->link_speed));
	} else if (!link_check &&
	    (sc->link_state == EM_LINK_STATE_UP ||
	    sc->link_state == EM_LINK_STATE_UP_RESET_PENDING)) {
		sc->link_speed = 0;
		sc->link_duplex = 0;
		sc->link_state = EM_LINK_STATE_DOWN;
		iflib_link_state_change(ctx, LINK_STATE_DOWN, 0);
	}

	/*
	 * A VF stops transmit DMA when its PF reports link down.  Reset if
	 * descriptors remain queued so they cannot be sent stale when carrier
	 * returns, matching the periodic check in Linux igbvf.
	 */
	if (!link_check && !sc->vf_reset_pending && igbv_tx_pending(sc)) {
		sc->vf_reset_pending = true;
		iflib_request_reset(ctx);
		iflib_admin_intr_deferred(ctx);
	}
	/* em_if_init() establishes a new counter baseline after the reset. */
	timer_tick = !sc->vf_reset_pending &&
	    atomic_readandclear_32(&sc->stats_pending) != 0;
	if (timer_tick) {
		em_update_stats_counters(sc);
		/* iflib clears RUNNING before stop; do not replay after reset. */
		if ((if_getdrvflags(iflib_get_ifp(ctx)) &
		    IFF_DRV_RUNNING) != 0)
			igbv_vlan_retry_tick(sc);
	}
}

static bool
igbv_tx_pending(struct e1000_softc *sc)
{
	struct tx_ring *txr;
	u32 head, tail;

	for (int i = 0; i < sc->tx_num_queues; i++) {
		txr = &sc->tx_queues[i].txr;
		head = E1000_READ_REG(&sc->hw, E1000_TDH(txr->me));
		tail = E1000_READ_REG(&sc->hw, E1000_TDT(txr->me));
		if (head != tail)
			return (true);
	}
	return (false);
}

static bool
igbv_sanitize_queues(struct e1000_softc *sc)
{
	struct e1000_hw *hw;
	u32 rxdctl, txdctl;
	int i, nqueues, retry;

	hw = &sc->hw;
	switch (hw->mac.type) {
	case e1000_vfadapt:
		nqueues = IGBV_82576_QUEUES;
		break;
	case e1000_vfadapt_i350:
		nqueues = IGBV_I350_QUEUES;
		break;
	default:
		return (true);
	}

	/*
	 * The 82576 and I350 specification updates, Software Clarification 3,
	 * note that VFLR leaves this queue configuration intact.  Clear it
	 * before programming the new rings so igbv does not depend on its PF
	 * to sanitize state left by a previous VF owner.  igbv uses only queue
	 * zero, but must also clear the unused second 82576 queue.
	 *
	 * Disable every queue first and wait for outstanding DMA activity to
	 * stop before programming TDWBAL/H.  Spin only for the normal fast
	 * transition, then sleep until the bounded deadline.
	 */
	for (i = 0; i < nqueues; i++) {
		E1000_WRITE_REG(hw, E1000_RXDCTL(i), 0);
		E1000_WRITE_REG(hw, E1000_TXDCTL(i), 0);
	}
	E1000_WRITE_FLUSH(hw);
	for (retry = 0; retry < IGBV_QUEUE_DISABLE_RETRIES; retry++) {
		for (i = 0; i < nqueues; i++) {
			rxdctl = E1000_READ_REG(hw, E1000_RXDCTL(i));
			txdctl = E1000_READ_REG(hw, E1000_TXDCTL(i));
			if ((rxdctl & E1000_RXDCTL_QUEUE_ENABLE) != 0 ||
			    (txdctl & E1000_TXDCTL_QUEUE_ENABLE) != 0)
				break;
		}
		if (i == nqueues)
			break;
		if (retry + 1 < IGBV_QUEUE_DISABLE_RETRIES) {
			if (retry < IGBV_QUEUE_DISABLE_BUSY_RETRIES)
				DELAY(IGBV_QUEUE_DISABLE_DELAY_US);
			else
				pause_sbt("igbvqds",
				    IGBV_QUEUE_DISABLE_PAUSE, 0,
				    C_PREL(1));
		}
	}
	if (retry == IGBV_QUEUE_DISABLE_RETRIES) {
		if (ratecheck(&sc->vf_last_queue_log,
		    &igbv_queue_log_interval))
			device_printf(sc->dev,
			    "could not disable retained VF queues; "
			    "reset deferred\n");
		return (false);
	}

	for (i = 0; i < nqueues; i++) {
		E1000_WRITE_REG(hw, E1000_SRRCTL(i), 0);
		E1000_WRITE_REG(hw, E1000_DCA_RXCTRL(i), 0);
		E1000_WRITE_REG(hw, E1000_TDWBAL(i), 0);
		E1000_WRITE_REG(hw, E1000_TDWBAH(i), 0);
		E1000_WRITE_REG(hw, E1000_DCA_TXCTRL(i), 0);
	}
	E1000_WRITE_REG(hw, E1000_VFPSRTYPE, 0);
	E1000_WRITE_FLUSH(hw);
	return (true);
}

bool
igbv_reset(if_ctx_t ctx)
{
	struct e1000_softc *sc;
	struct e1000_hw *hw;
	s32 error;

	sc = iflib_get_softc(ctx);
	hw = &sc->hw;
	KASSERT(sc->vf_ifp, ("%s called for a PF", __func__));

	/*
	 * Receive-buffer allocation and flow control are port resources owned
	 * by the PF.  Zero is an unavailable PBA sentinel, not a per-VF size.
	 */
	sc->pba = 0;
	hw->fc = (struct e1000_fc_info){
		.current_mode = e1000_fc_none,
		.requested_mode = e1000_fc_none,
	};

	error = e1000_reset_hw(hw);
	atomic_store_rel_32(&sc->vf_mbx_ready, 0);
	sc->vf_queues_sanitized = igbv_sanitize_queues(sc);
	if (!sc->vf_queues_sanitized) {
		return (false);
	}
	igbv_queue_retry_succeeded(sc);
	if (error != E1000_SUCCESS) {
		igbv_log_reset_failure(sc, error, false);
		return (false);
	}
	memset(sc->vf_vfta_stale, 0, sizeof(sc->vf_vfta_stale));
	memset(sc->vf_vfta_retry, 0, sizeof(sc->vf_vfta_retry));
	sc->vf_vlan_retry_deadline = 0;
	sc->vf_vlan_retry_cursor = 0;
	if (e1000_init_hw(hw) < 0) {
		device_printf(sc->dev, "Hardware Initialization Failed\n");
		return (false);
	}
	e1000_check_for_link(hw);
	igbv_mbx_retry_succeeded(sc);
	return (true);
}

void
igbv_initialize_transmit_unit(if_ctx_t ctx)
{

	KASSERT(((struct e1000_softc *)iflib_get_softc(ctx))->vf_ifp,
	    ("%s called for a PF", __func__));
	em_initialize_transmit_rings(ctx);
}

void
igbv_initialize_receive_unit(if_ctx_t ctx)
{

	KASSERT(((struct e1000_softc *)iflib_get_softc(ctx))->vf_ifp,
	    ("%s called for a PF", __func__));
	igb_initialize_receive_rings(ctx, true);
}

void
igbv_if_intr_enable(if_ctx_t ctx)
{
	struct e1000_softc *sc;
	struct e1000_hw *hw;
	u32 mask;

	sc = iflib_get_softc(ctx);
	hw = &sc->hw;
	KASSERT(sc->vf_ifp, ("%s called for a PF", __func__));
	if (!sc->vf_queues_sanitized ||
	    atomic_load_acq_32(&sc->vf_mbx_ready) == 0)
		return;
	mask = sc->que_mask | sc->link_mask;

	E1000_WRITE_REG(hw, E1000_EIAC, mask);
	E1000_WRITE_REG(hw, E1000_EIAM, mask);
	E1000_WRITE_REG(hw, E1000_EIMS, mask);
	E1000_WRITE_FLUSH(hw);
}

void
igbv_if_intr_disable(if_ctx_t ctx)
{
	struct e1000_softc *sc;
	struct e1000_hw *hw;

	sc = iflib_get_softc(ctx);
	hw = &sc->hw;
	KASSERT(sc->vf_ifp, ("%s called for a PF", __func__));

	E1000_WRITE_REG(hw, E1000_EIMC, 0xffffffff);
	E1000_WRITE_REG(hw, E1000_EIAC, 0);
	E1000_WRITE_FLUSH(hw);
}

int
igbv_get_regs(SYSCTL_HANDLER_ARGS)
{
	struct e1000_softc *sc;
	struct e1000_hw *hw;
	struct sbuf *sb;
	int error;

	sc = (struct e1000_softc *)arg1;
	hw = &sc->hw;
	KASSERT(sc->vf_ifp, ("%s called for a PF", __func__));

	sb = sbuf_new_for_sysctl(NULL, NULL, 512, req);
	if (sb == NULL)
		return (ENOMEM);

	/*
	 * Limited VF register set:
	 * Don't read EICR here because it is clear-on-read.  The VF register
	 * file exposes its queue pair at index zero, so this diagnostic does
	 * not depend on the narrower lifetime of iflib's queue arrays.
	 */
	sbuf_printf(sb, "VF Registers\n");
	sbuf_printf(sb, "\tVTCTRL\t %08x\n",
	    E1000_READ_REG(hw, E1000_CTRL));
	sbuf_printf(sb, "\tSTATUS\t %08x\n",
	    E1000_READ_REG(hw, E1000_STATUS));
	sbuf_printf(sb, "\tRDLEN\t %08x\n",
	    E1000_READ_REG(hw, E1000_RDLEN(0)));
	sbuf_printf(sb, "\tRDH\t %08x\n",
	    E1000_READ_REG(hw, E1000_RDH(0)));
	sbuf_printf(sb, "\tRDT\t %08x\n",
	    E1000_READ_REG(hw, E1000_RDT(0)));
	sbuf_printf(sb, "\tTDLEN\t %08x\n",
	    E1000_READ_REG(hw, E1000_TDLEN(0)));
	sbuf_printf(sb, "\tTDH\t %08x\n",
	    E1000_READ_REG(hw, E1000_TDH(0)));
	sbuf_printf(sb, "\tTDT\t %08x\n",
	    E1000_READ_REG(hw, E1000_TDT(0)));

	error = sbuf_finish(sb);
	sbuf_delete(sb);
	return (error);
}

static u_int
igbv_copy_uc_addr(void *arg, struct sockaddr_dl *sdl, u_int idx)
{
	struct igb_vf_uc_addr_list *list;
	const u8 *addr;

	list = arg;
	addr = (const u8 *)LLADDR(sdl);
	if (memcmp(addr, list->sc->hw.mac.addr, ETHER_ADDR_LEN) == 0)
		return (0);
	if (idx < IGBV_MAX_MAC_FILTERS)
		memcpy(list->addrs[idx], addr, ETHER_ADDR_LEN);
	return (1);
}

void
igbv_update_uc_addr_list(struct e1000_softc *sc, if_t ifp)
{
	struct igb_vf_uc_addr_list list = {
		.sc = sc,
	};
	u_int count;

	count = if_foreach_lladdr(ifp, igbv_copy_uc_addr, &list);
	if (count > IGBV_MAX_MAC_FILTERS) {
		device_printf(sc->dev,
		    "too many secondary unicast addresses; maximum is %u\n",
		    IGBV_MAX_MAC_FILTERS);
	}
	if (count == 0 && !sc->vf_uc_filters_set)
		return;
	/*
	 * Linux igb PFs validate the address field before dispatching the CLR
	 * subcommand.  Supply the primary address rather than the zero payload
	 * used by igbvf so those PFs actually remove the old filters.  FreeBSD
	 * PFs dispatch CLR before inspecting the otherwise-ignored address.
	 */
	if (e1000_set_uc_addr_vf(&sc->hw, E1000_VF_MAC_FILTER_CLR,
	    sc->hw.mac.addr) != E1000_SUCCESS) {
		device_printf(sc->dev,
		    "VF secondary unicast filter clear request failed\n");
		return;
	}
	sc->vf_uc_filters_set = false;
	if (count > IGBV_MAX_MAC_FILTERS)
		return;

	for (u_int i = 0; i < count; i++) {
		if (e1000_set_uc_addr_vf(&sc->hw, E1000_VF_MAC_FILTER_ADD,
		    list.addrs[i]) != E1000_SUCCESS) {
			device_printf(sc->dev,
			    "VF secondary unicast filter add request failed "
			    "for %6D\n", list.addrs[i], ":");
		} else
			sc->vf_uc_filters_set = true;
		usec_delay(200);
	}
}

void
igbv_reconcile_mac(struct e1000_softc *sc, if_t ifp)
{
	u8 *lladdr;

	if (!em_is_valid_ether_addr(sc->hw.mac.addr))
		return;
	lladdr = (u8 *)if_getlladdr(ifp);
	if (memcmp(lladdr, sc->hw.mac.addr, ETHER_ADDR_LEN) == 0)
		return;

	device_printf(sc->dev,
	    "PF rejected or replaced the requested MAC; using %6D\n",
	    sc->hw.mac.addr, ":");
	/*
	 * if_setlladdr() would re-enter the driver's address-change path.
	 * Initialization already holds the context lock, so update the
	 * storage directly and issue the notification it would have sent.
	 */
	memcpy(lladdr, sc->hw.mac.addr, ETHER_ADDR_LEN);

	CURVNET_SET_QUIET(if_getvnet(ifp));
	EVENTHANDLER_INVOKE(iflladdr_event, ifp);
	CURVNET_RESTORE();
}
