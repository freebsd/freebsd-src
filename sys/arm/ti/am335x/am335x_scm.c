/*-
 * Copyright (c) 2016 Rubicon Communications, LLC (Netgate)
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <arm/ti/am335x/am335x_scm.h>
#include <arm/ti/ti_cpuid.h>
#include <arm/ti/ti_scm.h>

#include <dev/syscon/syscon.h>
#include "syscon_if.h"

#define	TZ_ZEROC	2731

struct am335x_scm_softc {
	int			sc_last_temp;
	struct sysctl_oid	*sc_temp_oid;
	struct syscon		*syscon;
};

static int
am335x_scm_temp_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev;
	int i, temp;
	struct am335x_scm_softc *sc;
	uint32_t reg;

	dev = (device_t)arg1;
	sc = device_get_softc(dev);

	/* Read the temperature and convert to Kelvin. */
	for(i = 50; i > 0; i--) {
		reg = SYSCON_READ_4(sc->syscon, SCM_BGAP_CTRL);
		if ((reg & SCM_BGAP_EOCZ) == 0)
			break;
		DELAY(50);
	}
	if ((reg & SCM_BGAP_EOCZ) == 0) {
		sc->sc_last_temp =
		    (reg >> SCM_BGAP_TEMP_SHIFT) & SCM_BGAP_TEMP_MASK;
		sc->sc_last_temp *= 10;
	}
	temp = sc->sc_last_temp + TZ_ZEROC;

	return (sysctl_handle_int(oidp, &temp, 0, req));
}

static void
am335x_scm_identify(driver_t *driver, device_t parent)
{
	device_t child;

	/* AM335x only. */
	if (ti_chip() != CHIP_AM335X)
		return;

	/* Make sure we attach only once. */
	if (device_find_child(parent, "am335x_scm", DEVICE_UNIT_ANY) != NULL)
		return;

	child = device_add_child(parent, "am335x_scm", DEVICE_UNIT_ANY);
	if (child == NULL)
		device_printf(parent, "cannot add ti_scm child\n");
}

static int
am335x_scm_probe(device_t dev)
{
	/* Just allow the first one */
	if (strcmp(device_get_nameunit(dev), "am335x_scm0") != 0)
		return (ENXIO);

	device_set_desc(dev, "AM335x Control Module Extension");

	return (BUS_PROBE_DEFAULT);
}

static int
am335x_scm_attach(device_t dev)
{
	struct am335x_scm_softc *sc;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *tree;
	uint32_t reg;
	phandle_t opp_table;
	int err;

	sc = device_get_softc(dev);

	/* FIXME: For now; Go and kidnap syscon from opp-table */
	opp_table = OF_finddevice("/opp-table");
	if (opp_table == -1) {
		device_printf(dev, "Cant find /opp-table\n");
		return (ENXIO);
	}
	if (!OF_hasprop(opp_table, "syscon")) {
		device_printf(dev, "/opp-table missing syscon property\n");
		return (ENXIO);
	}
	err = syscon_get_by_ofw_property(dev, opp_table, "syscon", &sc->syscon);
	if (err) {
		device_printf(dev, "Failed to get syscon\n");
		return (ENXIO);
	}

	/* Reset the digital outputs. */
	SYSCON_WRITE_4(sc->syscon, SCM_BGAP_CTRL, 0);
	reg = SYSCON_READ_4(sc->syscon, SCM_BGAP_CTRL);
	DELAY(500);
	/* Set continuous mode. */
	SYSCON_WRITE_4(sc->syscon, SCM_BGAP_CTRL, SCM_BGAP_CONTCONV);
	reg = SYSCON_READ_4(sc->syscon, SCM_BGAP_CTRL);
	DELAY(500);
	/* Start the ADC conversion. */
	reg = SCM_BGAP_CLRZ | SCM_BGAP_CONTCONV | SCM_BGAP_SOC;
	SYSCON_WRITE_4(sc->syscon, SCM_BGAP_CTRL, reg);

	/* Temperature sysctl. */
        ctx = device_get_sysctl_ctx(dev);
	tree = SYSCTL_CHILDREN(device_get_sysctl_tree(dev));
	sc->sc_temp_oid = SYSCTL_ADD_PROC(ctx, tree, OID_AUTO,
	    "temperature", CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    dev, 0, am335x_scm_temp_sysctl, "IK", "Current temperature");

	return (0);
}

static int
am335x_scm_detach(device_t dev)
{
	struct am335x_scm_softc *sc;

	sc = device_get_softc(dev);

	/* Remove temperature sysctl. */
	if (sc->sc_temp_oid != NULL)
		sysctl_remove_oid(sc->sc_temp_oid, 1, 0);

	/* Stop the bandgap ADC. */
	SYSCON_WRITE_4(sc->syscon, SCM_BGAP_CTRL, SCM_BGAP_BGOFF);

	return (0);
}

static device_method_t am335x_scm_methods[] = {
	DEVMETHOD(device_identify,	am335x_scm_identify),
	DEVMETHOD(device_probe,		am335x_scm_probe),
	DEVMETHOD(device_attach,	am335x_scm_attach),
	DEVMETHOD(device_detach,	am335x_scm_detach),

	DEVMETHOD_END
};

static driver_t am335x_scm_driver = {
	"am335x_scm",
	am335x_scm_methods,
	sizeof(struct am335x_scm_softc),
};

DRIVER_MODULE(am335x_scm, ti_scm, am335x_scm_driver, 0, 0);
MODULE_VERSION(am335x_scm, 1);
MODULE_DEPEND(am335x_scm, ti_scm_syscon, 1, 1, 1);
