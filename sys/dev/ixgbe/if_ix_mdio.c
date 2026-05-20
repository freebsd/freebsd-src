/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Adrian Chadd <adrian@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_rss.h"

#include "ixgbe.h"
#include "mdio_if.h"
#include "ixgbe_sriov.h"
#include "ifdi_if.h"
#include "if_ix_mdio_hw.h"
#include "if_ix_mdio.h"

#include <dev/mdio/mdio.h>

/**
 * @brief Return if the given ixgbe chipset supports clause 22 MDIO bus access.
 *
 * Although technically all of the ixgbe chipsets support an MDIO
 * bus interface, there's a bunch of factors controlling whether
 * this should be exposed for external control.
 *
 * This functionr returns true if it supports an MDIO bus and
 * clause 22 transactions.
 */
static bool
ixgbe_has_mdio_bus_clause22(struct ixgbe_hw *hw)
{
	switch (hw->device_id) {
	case IXGBE_DEV_ID_X550EM_A_KR:
	case IXGBE_DEV_ID_X550EM_A_KR_L:
	case IXGBE_DEV_ID_X550EM_A_SFP_N:
	case IXGBE_DEV_ID_X550EM_A_SGMII:
	case IXGBE_DEV_ID_X550EM_A_SGMII_L:
	case IXGBE_DEV_ID_X550EM_A_10G_T:
	case IXGBE_DEV_ID_X550EM_A_SFP:
	case IXGBE_DEV_ID_X550EM_A_1G_T:
	case IXGBE_DEV_ID_X550EM_A_1G_T_L:
		return (true);
	}
	return (false);
}



/**
 * @brief Initiate a clause-22 MDIO read transfer.
 *
 * Note this is only officially supported for a small subset
 * of NICs, notably the X552/X553 devices.  This must not be
 * called for other chipsets.
 */
int
ixgbe_mdio_readreg_c22(device_t dev, int phy, int reg)
{
	if_ctx_t ctx = device_get_softc(dev);
	struct sx *iflib_ctx_lock = iflib_ctx_lock_get(ctx);
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	struct ixgbe_hw *hw = &sc->hw;
	uint16_t val = 0;
	int32_t ret = 0;

	if (! ixgbe_has_mdio_bus_clause22(hw))
		return (-1);

	sx_xlock(iflib_ctx_lock);
	ret = ixgbe_read_mdio_c22(hw, phy, reg, &val);
	if (ret != IXGBE_SUCCESS) {
		device_printf(dev, "%s: read_mdi_22 failed (%d)\n",
		    __func__, ret);
		sx_xunlock(iflib_ctx_lock);
		return (-1);
	}
	sx_xunlock(iflib_ctx_lock);
	return (val);
}

/**
 * @brief Initiate a clause-22 MDIO write transfer.
 *
 * Note this is only officially supported for a small subset
 * of NICs, notably the X552/X553 devices.  This must not be
 * called for other chipsets.
 */
int
ixgbe_mdio_writereg_c22(device_t dev, int phy, int reg, int data)
{
	if_ctx_t ctx = device_get_softc(dev);
	struct sx *iflib_ctx_lock = iflib_ctx_lock_get(ctx);
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	struct ixgbe_hw *hw = &sc->hw;
	int32_t ret;

	if (! ixgbe_has_mdio_bus_clause22(hw))
		return (-1);

	sx_xlock(iflib_ctx_lock);
	ret = ixgbe_write_mdio_c22(hw, phy, reg, data);
	if (ret != IXGBE_SUCCESS) {
		device_printf(dev, "%s: write_mdi_22 failed (%d)\n",
		    __func__, ret);
		sx_xunlock(iflib_ctx_lock);
		return (-1);
	}
	sx_xunlock(iflib_ctx_lock);
	return (0);
}

/**
 * @brief Attach the MDIO bus if one exists.
 */
void
ixgbe_mdio_attach(struct ixgbe_softc *sc)
{
	struct ixgbe_hw *hw = &sc->hw;
	int enable_mdio = 0;

	/*
	 * This explicitly needs to be enabled regardless of whether
	 * the device / instance supports an external MDIO bus.
	 */
	if (resource_int_value(device_get_name(sc->dev),
	    device_get_unit(sc->dev), "enable_mdio", &enable_mdio) == 0) {
		if (enable_mdio == 0)
			return;
	} else
		return;

	if (! ixgbe_has_mdio_bus_clause22(hw))
		return;

	device_add_child(sc->dev, "mdio", DEVICE_UNIT_ANY);
	bus_attach_children(sc->dev);
}
