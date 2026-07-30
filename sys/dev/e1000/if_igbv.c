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
