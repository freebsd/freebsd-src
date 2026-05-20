/*-
 * Copyright (c) 2012 Semihalf.
 * All rights reserved.
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

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/proc.h>
#include <sys/pcpu.h>
#include <sys/rman.h>
#include <sys/sched.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/resource.h>
#include <machine/tlb.h>

#include "portals.h"


int
dpaa_portal_alloc_res(device_t dev, int cpu)
{
	struct dpaa_portal_softc *sc = device_get_softc(dev);

	sc->sc_mres[0] = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    0, RF_ACTIVE);
	if (sc->sc_mres[0] == NULL) {
		device_printf(dev,
		    "Could not allocate cache enabled memory.\n");
		return (ENXIO);
	}
	/* Cache inhibited area */
	sc->sc_mres[1] = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    1, RF_ACTIVE);
	if (sc->sc_mres[1] == NULL) {
		device_printf(dev,
		    "Could not allocate cache inhibited memory.\n");
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mres[0]);
		return (ENXIO);
	}
	sc->sc_dev = dev;
	sc->sc_ce_va = rman_get_bushandle(sc->sc_mres[0]);
	sc->sc_ce_size = rman_get_size(sc->sc_mres[0]);
	sc->sc_ce_pa = pmap_kextract(sc->sc_ce_va);
	sc->sc_ci_va = rman_get_bushandle(sc->sc_mres[1]);
	sc->sc_ci_size = rman_get_size(sc->sc_mres[1]);
	sc->sc_ci_pa = pmap_kextract(sc->sc_ci_va);
	tlb1_set_entry(sc->sc_ce_va, sc->sc_ce_pa, sc->sc_ce_size,
			_TLB_ENTRY_MEM | _TLB_ENTRY_SHARED);
	sc->sc_ires = bus_alloc_resource_any(dev, SYS_RES_IRQ, 0, RF_ACTIVE);

	/* Allocate interrupts */
	if (sc->sc_ires == NULL) {
		device_printf(dev, "Could not allocate irq.\n");
		return (ENXIO);
	}

	return (0);
}
