/*-
 * Copyright (c) 2012 Olivier Houchard.  All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <arm/ti/ti_smc.h>
#include <arm/ti/omap4/omap4_smc.h>
#include <machine/bus.h>
#include <machine/pl310.h>

void
platform_pl310_init(struct pl310_softc *sc)
{
	uint32_t aux, prefetch;

	aux = pl310_read4(sc, PL310_AUX_CTRL);
	prefetch = pl310_read4(sc, PL310_PREFETCH_CTRL);

	if (bootverbose) {
		device_printf(sc->sc_dev, "Early BRESP response: %s\n",
			(aux & AUX_CTRL_EARLY_BRESP) ? "enabled" : "disabled");
		device_printf(sc->sc_dev, "Instruction prefetch: %s\n",
			(aux & AUX_CTRL_INSTR_PREFETCH) ? "enabled" : "disabled");
		device_printf(sc->sc_dev, "Data prefetch: %s\n",
			(aux & AUX_CTRL_DATA_PREFETCH) ? "enabled" : "disabled");
		device_printf(sc->sc_dev, "Non-secure interrupt control: %s\n",
			(aux & AUX_CTRL_NS_INT_CTRL) ? "enabled" : "disabled");
		device_printf(sc->sc_dev, "Non-secure lockdown: %s\n",
			(aux & AUX_CTRL_NS_LOCKDOWN) ? "enabled" : "disabled");
		device_printf(sc->sc_dev, "Share override: %s\n",
			(aux & AUX_CTRL_SHARE_OVERRIDE) ? "enabled" : "disabled");

		device_printf(sc->sc_dev, "Double linefil: %s\n",
			(prefetch & PREFETCH_CTRL_DL) ? "enabled" : "disabled");
		device_printf(sc->sc_dev, "Instruction prefetch: %s\n",
			(prefetch & PREFETCH_CTRL_INSTR_PREFETCH) ? "enabled" : "disabled");
		device_printf(sc->sc_dev, "Data prefetch: %s\n",
			(prefetch & PREFETCH_CTRL_DATA_PREFETCH) ? "enabled" : "disabled");
		device_printf(sc->sc_dev, "Double linefill on WRAP request: %s\n",
			(prefetch & PREFETCH_CTRL_DL_ON_WRAP) ? "enabled" : "disabled");
		device_printf(sc->sc_dev, "Prefetch drop: %s\n",
			(prefetch & PREFETCH_CTRL_PREFETCH_DROP) ? "enabled" : "disabled");
		device_printf(sc->sc_dev, "Incr double Linefill: %s\n",
			(prefetch & PREFETCH_CTRL_INCR_DL) ? "enabled" : "disabled");
		device_printf(sc->sc_dev, "Not same ID on exclusive sequence: %s\n",
			(prefetch & PREFETCH_CTRL_NOTSAMEID) ? "enabled" : "disabled");
		device_printf(sc->sc_dev, "Prefetch offset: %d\n",
			(prefetch & PREFETCH_CTRL_OFFSET_MASK));
	}

	/*
	 * Disable instruction prefetch
	 */
	prefetch &= ~PREFETCH_CTRL_INSTR_PREFETCH;
	aux &= ~AUX_CTRL_INSTR_PREFETCH;

	// prefetch &= ~PREFETCH_CTRL_DATA_PREFETCH;
	// aux &= ~AUX_CTRL_DATA_PREFETCH;

	/*
	 * Make sure data prefetch is on
	 */
	prefetch |= PREFETCH_CTRL_DATA_PREFETCH;
	aux |= AUX_CTRL_DATA_PREFETCH;

	/*
	 * TODO: add tunable for prefetch offset
	 * and experiment with performance
	 */

	ti_smc0(aux, 0, WRITE_AUXCTRL_REG);
	ti_smc0(prefetch, 0, WRITE_PREFETCH_CTRL_REG);
}

void
platform_pl310_write_ctrl(struct pl310_softc *sc, uint32_t val)
{
	ti_smc0(val, 0, L2CACHE_WRITE_CTRL_REG);
}

void
platform_pl310_write_debug(struct pl310_softc *sc, uint32_t val)
{
	ti_smc0(val, 0, L2CACHE_WRITE_DEBUG_REG);
}
