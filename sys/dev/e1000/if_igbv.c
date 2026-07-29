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

#define	IGBV_MAX_MAC_FILTERS	3

struct igb_vf_uc_addr_list {
	struct e1000_softc	*sc;
	u8			addrs[IGBV_MAX_MAC_FILTERS][ETHER_ADDR_LEN];
};

static bool	igbv_tx_pending(struct e1000_softc *);

int
igbv_if_attach_pre(if_ctx_t ctx)
{
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

	KASSERT(((struct e1000_softc *)iflib_get_softc(ctx))->vf_ifp &&
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
	bool link_check;

	sc = iflib_get_softc(ctx);
	hw = &sc->hw;
	dev = iflib_get_dev(ctx);
	KASSERT(sc->vf_ifp, ("%s called for a PF", __func__));

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
	if (!sc->vf_reset_pending &&
	    atomic_readandclear_32(&sc->stats_pending) != 0)
		em_update_stats_counters(sc);
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

bool
igbv_reset(if_ctx_t ctx)
{
	struct e1000_softc *sc;
	struct e1000_hw *hw;

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

	if (e1000_reset_hw(hw) != E1000_SUCCESS) {
		e1000_check_for_link(hw);
		return (false);
	}
	memset(sc->vf_vfta_stale, 0, sizeof(sc->vf_vfta_stale));
	if (e1000_init_hw(hw) < 0) {
		device_printf(sc->dev, "Hardware Initialization Failed\n");
		return (false);
	}
	e1000_check_for_link(hw);
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
