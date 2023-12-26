/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Adrian Chadd <adrian@FreeBSD.org>.
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <machine/bus.h>
#include <dev/iicbus/iic.h>
#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mdio/mdio.h>
#include <dev/clk/clk.h>
#include <dev/hwreset/hwreset.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/etherswitch/etherswitch.h>

#include <dev/etherswitch/ar40xx/ar40xx_var.h>
#include <dev/etherswitch/ar40xx/ar40xx_reg.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw.h>

#include <dev/etherswitch/ar40xx/ar40xx_hw_mdio.h>

#include "mdio_if.h"
#include "miibus_if.h"
#include "etherswitch_if.h"

int
ar40xx_hw_phy_dbg_write(struct ar40xx_softc *sc, int phy, uint16_t dbg,
     uint16_t data)
{
	AR40XX_LOCK_ASSERT(sc);
	device_printf(sc->sc_dev, "%s: TODO\n", __func__);
	return (0);
}

int
ar40xx_hw_phy_dbg_read(struct ar40xx_softc *sc, int phy, uint16_t dbg)
{
	AR40XX_LOCK_ASSERT(sc);
	device_printf(sc->sc_dev, "%s: TODO\n", __func__);
	return (-1);
}

int
ar40xx_hw_phy_mmd_write(struct ar40xx_softc *sc, uint32_t phy_id,
    uint16_t mmd_num, uint16_t reg_id, uint16_t reg_val)
{

	AR40XX_LOCK_ASSERT(sc);

	MDIO_WRITEREG(sc->sc_mdio_dev, phy_id, AR40XX_MII_ATH_MMD_ADDR,
	    mmd_num);
	MDIO_WRITEREG(sc->sc_mdio_dev, phy_id, AR40XX_MII_ATH_MMD_DATA,
	    reg_id);
	MDIO_WRITEREG(sc->sc_mdio_dev, phy_id, AR40XX_MII_ATH_MMD_ADDR,
	    0x4000 | mmd_num);
	MDIO_WRITEREG(sc->sc_mdio_dev, phy_id, AR40XX_MII_ATH_MMD_DATA,
	    reg_val);

	return (0);
}

int
ar40xx_hw_phy_mmd_read(struct ar40xx_softc *sc, uint32_t phy_id,
    uint16_t mmd_num, uint16_t reg_id)
{
	uint16_t value;

	AR40XX_LOCK_ASSERT(sc);

	MDIO_WRITEREG(sc->sc_mdio_dev, phy_id, AR40XX_MII_ATH_MMD_ADDR,
	     mmd_num);
	MDIO_WRITEREG(sc->sc_mdio_dev, phy_id, AR40XX_MII_ATH_MMD_DATA,
	    reg_id);
	MDIO_WRITEREG(sc->sc_mdio_dev, phy_id, AR40XX_MII_ATH_MMD_ADDR,
	    0x4000 | mmd_num);

	value = MDIO_READREG(sc->sc_mdio_dev, phy_id,
	    AR40XX_MII_ATH_MMD_DATA);

	return value;
}

