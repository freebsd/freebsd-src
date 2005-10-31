/*-
 * Copyright (c) 2005 Poul-Henning Kamp
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/bus.h>
#include <contrib/dev/acpica/acpi.h>
#include "acpi_if.h"
#include <dev/acpica/acpivar.h>

ACPI_SERIAL_DECL(hpet, "ACPI HPET support");

struct acpi_hpet_softc {
	device_t		dev;
	struct resource		*res[1];
	ACPI_HANDLE		handle;
};

static unsigned hpet_get_timecount(struct timecounter *tc);

struct timecounter hpet_timecounter = {
	.tc_get_timecount =	hpet_get_timecount,
	.tc_counter_mask =	~0u,
	.tc_name =		"HPET",
	.tc_quality =		-200,
};

static char *hpet_ids[] = { "PNP0103", NULL };

static unsigned
hpet_get_timecount(struct timecounter *tc)
{
	struct acpi_hpet_softc *sc;

	sc = tc->tc_priv;
	return (bus_read_4(sc->res[0], 0xf0));
}

static int
acpi_hpet_probe(device_t dev)
{
	if (acpi_disabled("hpet") ||
	    ACPI_ID_PROBE(device_get_parent(dev), dev, hpet_ids) == NULL ||
	    device_get_unit(dev) != 0)
		return (ENXIO);

	device_set_desc(dev, "HPET - High Precision Event Timers");
	return (0);
}

static struct resource_spec hpet_res_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE},
	{ -1, 0, 0}
};

static int
acpi_hpet_attach(device_t dev)
{
	struct acpi_hpet_softc	*sc;
	int error;
	uint32_t u;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t) __func__);

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->handle = acpi_get_handle(dev);

	error = bus_alloc_resources(dev, hpet_res_spec, sc->res);
	if (error)
		return (error);

	u = bus_read_4(sc->res[0], 0x0);
	device_printf(dev, "Vendor: 0x%x\n", u >> 16);
	device_printf(dev, "Leg_Route_Cap: %d\n", (u >> 15) & 1);
	device_printf(dev, "Count_Size_Cap: %d\n", (u >> 13) & 1);
	device_printf(dev, "Num_Tim_Cap: %d\n", (u >> 18) & 0xf);
	device_printf(dev, "Rev_id: 0x%x\n", u & 0xff);

	u = bus_read_4(sc->res[0], 0x4);
	device_printf(dev, "Period: %d fs (%jd Hz)\n",
	    u, (intmax_t)((1000000000000000LL + u / 2) / u));

	hpet_timecounter.tc_frequency = (1000000000000000LL + u / 2) / u;

	bus_write_4(sc->res[0], 0x10, 1);

#if 0
	{
	int i;
	uint32_t u1, u2;
	struct bintime b0, b1, b2;
	struct timespec ts;

	binuptime(&b0);
	binuptime(&b0);
	binuptime(&b1);
	u1 = bus_read_4(sc->res[0], 0xf0);
	for (i = 1; i < 1000; i++)
		u2 = bus_read_4(sc->res[0], 0xf0);
	binuptime(&b2);
	u2 = bus_read_4(sc->res[0], 0xf0);

	bintime_sub(&b2, &b1);
	bintime_sub(&b1, &b0);
	bintime_sub(&b2, &b1);
	bintime2timespec(&b2, &ts);

	device_printf(dev, "%ld.%09ld: %u ... %u = %u\n",
	    (long)ts.tv_sec, ts.tv_nsec, u1, u2, u2 - u1);

	device_printf(dev, "time per call: %ld ns\n", ts.tv_nsec / 1000);
	}
#endif

	device_printf(sc->dev, "HPET attach\n");

	hpet_timecounter.tc_priv = sc;

	tc_init(&hpet_timecounter);

	return (0);
}

static int
acpi_hpet_detach(device_t dev)
{
	ACPI_FUNCTION_TRACE((char *)(uintptr_t) __func__);

#if 1
	return (EBUSY);
#else
	struct acpi_hpet_softc *sc = device_get_softc(dev);
	bus_release_resources(dev, hpet_res_spec, sc->res);

	device_printf(sc->dev, "HPET detach\n");
	return (0);
#endif
}

static device_method_t acpi_hpet_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, acpi_hpet_probe),
	DEVMETHOD(device_attach, acpi_hpet_attach),
	DEVMETHOD(device_detach, acpi_hpet_detach),

	{0, 0}
};

static driver_t	acpi_hpet_driver = {
	"acpi_hpet",
	acpi_hpet_methods,
	sizeof(struct acpi_hpet_softc),
};

static devclass_t acpi_hpet_devclass;

DRIVER_MODULE(acpi_hpet, acpi, acpi_hpet_driver, acpi_hpet_devclass, 0, 0);
MODULE_DEPEND(acpi_hpet, acpi, 1, 1, 1);
