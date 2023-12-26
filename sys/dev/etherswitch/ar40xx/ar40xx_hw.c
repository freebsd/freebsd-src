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
#include <dev/etherswitch/ar40xx/ar40xx_debug.h>

/*
 * XXX these are here for now; move the code using these
 * into main.c once this is all done!
 */
#include <dev/etherswitch/ar40xx/ar40xx_hw_vtu.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw_port.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw_mirror.h>

#include "mdio_if.h"
#include "miibus_if.h"
#include "etherswitch_if.h"

/*
 * Reset the ESS switch.  This also resets the ESS ethernet
 * and PSGMII block.
 */
int
ar40xx_hw_ess_reset(struct ar40xx_softc *sc)
{
	int ret;

	AR40XX_DPRINTF(sc, AR40XX_DBG_HW_RESET, "%s: called\n", __func__);

	ret = hwreset_assert(sc->sc_ess_rst);
	if (ret != 0) {
		device_printf(sc->sc_dev, "ERROR: failed to assert reset\n");
		return ret;
	}
	DELAY(10*1000);

	ret = hwreset_deassert(sc->sc_ess_rst);
	if (ret != 0) {
		device_printf(sc->sc_dev,
		    "ERROR: failed to deassert reset\n");
		return ret;
	}

	DELAY(10*1000);

	return (0);
}

int
ar40xx_hw_init_globals(struct ar40xx_softc *sc)
{
	uint32_t reg;

	AR40XX_DPRINTF(sc, AR40XX_DBG_HW_INIT, "%s: called\n", __func__);

	/* enable CPU port and disable mirror port */
	reg = AR40XX_FWD_CTRL0_CPU_PORT_EN
	     | AR40XX_FWD_CTRL0_MIRROR_PORT;
	AR40XX_REG_WRITE(sc, AR40XX_REG_FWD_CTRL0, reg);

	/* forward multicast and broadcast frames to CPU */
	reg = (AR40XX_PORTS_ALL << AR40XX_FWD_CTRL1_UC_FLOOD_S)
	    | (AR40XX_PORTS_ALL << AR40XX_FWD_CTRL1_MC_FLOOD_S)
	    | (AR40XX_PORTS_ALL << AR40XX_FWD_CTRL1_BC_FLOOD_S);
	AR40XX_REG_WRITE(sc, AR40XX_REG_FWD_CTRL1, reg);

	/* enable jumbo frames */
	reg = AR40XX_REG_READ(sc, AR40XX_REG_MAX_FRAME_SIZE);
	reg &= ~AR40XX_MAX_FRAME_SIZE_MTU;
	reg |= 9018 + 8 + 2;
	AR40XX_REG_WRITE(sc, AR40XX_REG_MAX_FRAME_SIZE, reg);

	/* Enable MIB counters */
	reg = AR40XX_REG_READ(sc, AR40XX_REG_MODULE_EN);
	reg |= AR40XX_MODULE_EN_MIB;
	AR40XX_REG_WRITE(sc, AR40XX_REG_MODULE_EN, reg);

	/* Disable AZ */
	AR40XX_REG_WRITE(sc, AR40XX_REG_EEE_CTRL, 0);

	/* set flowctrl thershold for cpu port */
	reg = (AR40XX_PORT0_FC_THRESH_ON_DFLT << 16)
	    | AR40XX_PORT0_FC_THRESH_OFF_DFLT;
	AR40XX_REG_WRITE(sc, AR40XX_REG_PORT_FLOWCTRL_THRESH(0), reg);

	AR40XX_REG_BARRIER_WRITE(sc);

	return (0);
}

int
ar40xx_hw_vlan_init(struct ar40xx_softc *sc)
{
	int i;

	AR40XX_DPRINTF(sc, AR40XX_DBG_HW_INIT, "%s: called\n", __func__);

	/* Enable VLANs by default */
	sc->sc_vlan.vlan = 1;

	/* Configure initial LAN/WAN bitmap and include CPU port as tagged */
	sc->sc_vlan.vlan_id[AR40XX_LAN_VLAN] = AR40XX_LAN_VLAN
	    | ETHERSWITCH_VID_VALID;
	sc->sc_vlan.vlan_id[AR40XX_WAN_VLAN] = AR40XX_WAN_VLAN
	    | ETHERSWITCH_VID_VALID;

	sc->sc_vlan.vlan_ports[AR40XX_LAN_VLAN] =
	    sc->sc_config.switch_cpu_bmp | sc->sc_config.switch_lan_bmp;
	sc->sc_vlan.vlan_untagged[AR40XX_LAN_VLAN] =
	    sc->sc_config.switch_lan_bmp;

	sc->sc_vlan.vlan_ports[AR40XX_WAN_VLAN] =
	    sc->sc_config.switch_cpu_bmp | sc->sc_config.switch_wan_bmp;
	sc->sc_vlan.vlan_untagged[AR40XX_WAN_VLAN] =
	    sc->sc_config.switch_wan_bmp;

	/* Populate the per-port PVID - pvid[] is an index into vlan_id[] */
	for (i = 0; i < AR40XX_NUM_PORTS; i++) {
		if (sc->sc_config.switch_lan_bmp & (1U << i))
			sc->sc_vlan.pvid[i] = AR40XX_LAN_VLAN;
		if (sc->sc_config.switch_wan_bmp & (1U << i))
			sc->sc_vlan.pvid[i] = AR40XX_WAN_VLAN;
	}

	return (0);
}

/*
 * Apply the per-port and global configuration from software.
 *
 * This is useful if we ever start doing the linux switch framework
 * thing of updating the config in one hit and pushing it to the
 * hardware.  For now it's just used in the reset path.
 */
int
ar40xx_hw_sw_hw_apply(struct ar40xx_softc *sc)
{
	uint8_t portmask[AR40XX_NUM_PORTS];
	int i, j, ret;

	AR40XX_DPRINTF(sc, AR40XX_DBG_HW_INIT, "%s: called\n", __func__);

	/*
	 * Flush the VTU configuration.
	 */
	ret = ar40xx_hw_vtu_flush(sc);
	if (ret != 0) {
		device_printf(sc->sc_dev,
		    "ERROR: couldn't apply config; vtu flush failed (%d)\n",
		    ret);
		return (ret);
	}

	memset(portmask, 0, sizeof(portmask));

	/*
	 * Configure the ports based on whether it's 802.1q
	 * VLANs, or just straight up per-port VLANs.
	 */
	if (sc->sc_vlan.vlan) {
		device_printf(sc->sc_dev, "%s: configuring 802.1q VLANs\n",
		    __func__);
		for (j = 0; j < AR40XX_NUM_VTU_ENTRIES; j++) {
			uint8_t vp = sc->sc_vlan.vlan_ports[j];

			if (!vp)
				continue;
			if ((sc->sc_vlan.vlan_id[j]
			    & ETHERSWITCH_VID_VALID) == 0)
				continue;

			for (i = 0; i < AR40XX_NUM_PORTS; i++) {
				uint8_t mask = (1U << i);

				if (vp & mask)
					portmask[i] |= vp & ~mask;
			}

			ar40xx_hw_vtu_load_vlan(sc,
			    sc->sc_vlan.vlan_id[j] & ETHERSWITCH_VID_MASK,
			    sc->sc_vlan.vlan_ports[j],
			    sc->sc_vlan.vlan_untagged[j]);
		}
	} else {
		device_printf(sc->sc_dev, "%s: configuring per-port VLANs\n",
		    __func__);
		for (i = 0; i < AR40XX_NUM_PORTS; i++) {
			if (i == AR40XX_PORT_CPU)
				continue;

			portmask[i] = (1U << AR40XX_PORT_CPU);
			portmask[AR40XX_PORT_CPU] |= (1U << i);
		}
	}

	/*
	 * Update per-port destination mask, vlan tag settings
	 */
	for (i = 0; i < AR40XX_NUM_PORTS; i++)
		(void) ar40xx_hw_port_setup(sc, i, portmask[i]);

	/* Set the mirror register config */
	ret = ar40xx_hw_mirror_set_registers(sc);
	if (ret != 0) {
		device_printf(sc->sc_dev,
		    "ERROR: couldn't apply config; mirror config failed"
		    " (%d)\n",
		    ret);
		return (ret);
	}

	return (0);
}

int
ar40xx_hw_wait_bit(struct ar40xx_softc *sc, int reg, uint32_t mask,
    uint32_t val)
{
	int timeout = 20;
	uint32_t t;

	while (true) {
		AR40XX_REG_BARRIER_READ(sc);
		t = AR40XX_REG_READ(sc, reg);
		if ((t & mask) == val)
			return 0;

		if (timeout-- <= 0)
			break;

		DELAY(20);
	}

	device_printf(sc->sc_dev, "ERROR: timeout for reg "
	    "%08x: %08x & %08x != %08x\n",
	    (unsigned int)reg, t, mask, val);
	return (ETIMEDOUT);
}

/*
 * Read the switch MAC address.
 */
int
ar40xx_hw_read_switch_mac_address(struct ar40xx_softc *sc,
    struct ether_addr *ea)
{
	uint32_t ret0, ret1;
	char *s;

	s = (void *) ea;

	AR40XX_LOCK_ASSERT(sc);

	AR40XX_REG_BARRIER_READ(sc);
	ret0 = AR40XX_REG_READ(sc, AR40XX_REG_SW_MAC_ADDR0);
	ret1 = AR40XX_REG_READ(sc, AR40XX_REG_SW_MAC_ADDR1);

	s[5] = MS(ret0, AR40XX_REG_SW_MAC_ADDR0_BYTE5);
	s[4] = MS(ret0, AR40XX_REG_SW_MAC_ADDR0_BYTE4);
	s[3] = MS(ret1, AR40XX_REG_SW_MAC_ADDR1_BYTE3);
	s[2] = MS(ret1, AR40XX_REG_SW_MAC_ADDR1_BYTE2);
	s[1] = MS(ret1, AR40XX_REG_SW_MAC_ADDR1_BYTE1);
	s[0] = MS(ret1, AR40XX_REG_SW_MAC_ADDR1_BYTE0);

	return (0);
}

/*
 * Set the switch MAC address.
 */
int
ar40xx_hw_write_switch_mac_address(struct ar40xx_softc *sc,
    struct ether_addr *ea)
{
	uint32_t ret0 = 0, ret1 = 0;
	char *s;

	s = (void *) ea;

	AR40XX_LOCK_ASSERT(sc);

	ret0 |= SM(s[5], AR40XX_REG_SW_MAC_ADDR0_BYTE5);
	ret0 |= SM(s[4], AR40XX_REG_SW_MAC_ADDR0_BYTE4);

	ret1 |= SM(s[3], AR40XX_REG_SW_MAC_ADDR1_BYTE3);
	ret1 |= SM(s[2], AR40XX_REG_SW_MAC_ADDR1_BYTE2);
	ret1 |= SM(s[1], AR40XX_REG_SW_MAC_ADDR1_BYTE1);
	ret1 |= SM(s[0], AR40XX_REG_SW_MAC_ADDR1_BYTE0);

	AR40XX_REG_WRITE(sc, AR40XX_REG_SW_MAC_ADDR0, ret0);
	AR40XX_REG_WRITE(sc, AR40XX_REG_SW_MAC_ADDR1, ret1);

	AR40XX_REG_BARRIER_WRITE(sc);

	return (0);
}
