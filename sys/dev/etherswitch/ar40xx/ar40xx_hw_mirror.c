/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/etherswitch/etherswitch.h>

#include <dev/etherswitch/ar40xx/ar40xx_var.h>
#include <dev/etherswitch/ar40xx/ar40xx_reg.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw_mirror.h>

#include "mdio_if.h"
#include "miibus_if.h"
#include "etherswitch_if.h"


int
ar40xx_hw_mirror_set_registers(struct ar40xx_softc *sc)
{
	uint32_t reg;
	int port;

	/* Reset the mirror registers before configuring */
	reg = AR40XX_REG_READ(sc, AR40XX_REG_FWD_CTRL0);
	reg &= ~(AR40XX_FWD_CTRL0_MIRROR_PORT);
	reg |= (0xF << AR40XX_FWD_CTRL0_MIRROR_PORT_S);
	AR40XX_REG_WRITE(sc, AR40XX_REG_FWD_CTRL0, reg);
	AR40XX_REG_BARRIER_WRITE(sc);

	for (port = 0; port < AR40XX_NUM_PORTS; port++) {
		reg = AR40XX_REG_READ(sc, AR40XX_REG_PORT_LOOKUP(port));
		reg &= ~AR40XX_PORT_LOOKUP_ING_MIRROR_EN;
		AR40XX_REG_WRITE(sc, AR40XX_REG_PORT_LOOKUP(port), reg);

		reg = AR40XX_REG_READ(sc, AR40XX_REG_PORT_HOL_CTRL1(port));
		reg &= ~AR40XX_PORT_HOL_CTRL1_EG_MIRROR_EN;
		AR40XX_REG_WRITE(sc, AR40XX_REG_PORT_HOL_CTRL1(port), reg);

		AR40XX_REG_BARRIER_WRITE(sc);
	}

	/* Now, enable mirroring if requested */
	if (sc->sc_monitor.source_port >= AR40XX_NUM_PORTS
	    || sc->sc_monitor.monitor_port >= AR40XX_NUM_PORTS
	    || sc->sc_monitor.source_port == sc->sc_monitor.monitor_port) {
		return (0);
	}

	reg = AR40XX_REG_READ(sc, AR40XX_REG_FWD_CTRL0);
	reg &= ~AR40XX_FWD_CTRL0_MIRROR_PORT;
	reg |=
	    (sc->sc_monitor.monitor_port << AR40XX_FWD_CTRL0_MIRROR_PORT_S);
	AR40XX_REG_WRITE(sc, AR40XX_REG_FWD_CTRL0, reg);

	if (sc->sc_monitor.mirror_rx) {
		reg = AR40XX_REG_READ(sc,
		    AR40XX_REG_PORT_LOOKUP(sc->sc_monitor.source_port));
		reg |= AR40XX_PORT_LOOKUP_ING_MIRROR_EN;
		AR40XX_REG_WRITE(sc,
		    AR40XX_REG_PORT_LOOKUP(sc->sc_monitor.source_port),
		    reg);
		AR40XX_REG_BARRIER_WRITE(sc);
	}

	if (sc->sc_monitor.mirror_tx) {
		reg = AR40XX_REG_READ(sc,
		    AR40XX_REG_PORT_HOL_CTRL1(sc->sc_monitor.source_port));
		reg |= AR40XX_PORT_HOL_CTRL1_EG_MIRROR_EN;
		AR40XX_REG_WRITE(sc,
		    AR40XX_REG_PORT_HOL_CTRL1(sc->sc_monitor.source_port),
		    reg);
		AR40XX_REG_BARRIER_WRITE(sc);
	}

	return (0);
}
