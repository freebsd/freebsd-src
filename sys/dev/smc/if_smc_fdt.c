/*-
 * Copyright (c) 2008 Benno Rice
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
 *
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>

#include <net/if.h>

#include <dev/smc/if_smcvar.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

static int		smc_fdt_probe(device_t);

static int
smc_fdt_probe(device_t dev)
{
	struct	smc_softc *sc;

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "smsc,lan91c111")) {
		sc = device_get_softc(dev);
		sc->smc_usemem = 1;

		if (smc_probe(dev) != 0) {
			return (ENXIO);
		}

		return (0);
	}

	return (ENXIO);
}

static device_method_t smc_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		smc_fdt_probe),
	{ 0, 0 }
};

DEFINE_CLASS_1(smc, smc_fdt_driver, smc_fdt_methods,
    sizeof(struct smc_softc), smc_driver);

DRIVER_MODULE(smc, simplebus, smc_fdt_driver, 0, 0);
MODULE_DEPEND(smc, fdt, 1, 1, 1);
MODULE_DEPEND(smc, ether, 1, 1, 1);
MODULE_DEPEND(smc, miibus, 1, 1, 1);
