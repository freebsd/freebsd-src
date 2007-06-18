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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/time.h>
#include <sys/timetc.h>

#include <contrib/dev/acpica/acpi.h>
#include <dev/acpica/acpivar.h>

ACPI_SERIAL_DECL(hpet, "ACPI HPET support");

static devclass_t acpi_hpet_devclass;

/* ACPI CA debugging */
#define _COMPONENT	ACPI_TIMER
ACPI_MODULE_NAME("HPET")

struct acpi_hpet_softc {
	device_t		dev;
	struct resource		*mem_res;
	ACPI_HANDLE		handle;
};

static u_int hpet_get_timecount(struct timecounter *tc);
static void acpi_hpet_test(struct acpi_hpet_softc *sc);

static char *hpet_ids[] = { "PNP0103", NULL };

#define HPET_MEM_WIDTH		0x400	/* Expected memory region size */
#define HPET_OFFSET_INFO	0	/* Location of info in region */
#define HPET_OFFSET_PERIOD	4	/* Location of period (1/hz) */
#define HPET_OFFSET_ENABLE	0x10	/* Location of enable word */
#define HPET_OFFSET_VALUE	0xf0	/* Location of actual timer value */

#define DEV_HPET(x)	(acpi_get_magic(x) == (uintptr_t)&acpi_hpet_devclass)

struct timecounter hpet_timecounter = {
	.tc_get_timecount =	hpet_get_timecount,
	.tc_counter_mask =	~0u,
	.tc_name =		"HPET",
	.tc_quality =		2000,
};

static u_int
hpet_get_timecount(struct timecounter *tc)
{
	struct acpi_hpet_softc *sc;

	sc = tc->tc_priv;
	return (bus_read_4(sc->mem_res, HPET_OFFSET_VALUE));
}

/* Temporary define for 6.x */
#define ACPI_SIG_HPET "HPET"

/* Discover the HPET via the ACPI table of the same name. */
void 
acpi_hpet_table_probe(device_t parent)
{
	HPET_TABLE *hpet;
	ACPI_TABLE_HEADER *hdr;
	ACPI_STATUS	status;
	device_t	child;

	/* Currently, ID and minimum clock tick info is unused. */

	status = AcpiGetFirmwareTable(ACPI_SIG_HPET, 1, ACPI_LOGICAL_ADDRESSING,
		&hdr);
	if (ACPI_FAILURE(status))
		return;

	/*
	 * The unit number could be derived from hdr->Sequence but we only
	 * support one HPET device.
	 */
	hpet = (HPET_TABLE *)hdr;
	if (hpet->HpetNumber != 0)
		printf("ACPI HPET table warning: Sequence is non-zero (%d)\n",
		    hpet->HpetNumber);
	child = BUS_ADD_CHILD(parent, 0, "acpi_hpet", 0);
	if (child == NULL) {
		printf("%s: can't add child\n", __func__);
		return;
	}

	/* Record a magic value so we can detect this device later. */
	acpi_set_magic(child, (uintptr_t)&acpi_hpet_devclass);
	bus_set_resource(child, SYS_RES_MEMORY, 0, hpet->BaseAddress.Address,
	    HPET_MEM_WIDTH);
	if (device_probe_and_attach(child) != 0)
		device_delete_child(parent, child);
}

static int
acpi_hpet_probe(device_t dev)
{
	ACPI_FUNCTION_TRACE((char *)(uintptr_t) __func__);

	if (acpi_disabled("hpet"))
		return (ENXIO);
	if (!DEV_HPET(dev) &&
	    (ACPI_ID_PROBE(device_get_parent(dev), dev, hpet_ids) == NULL ||
	    device_get_unit(dev) != 0))
		return (ENXIO);

	device_set_desc(dev, "High Precision Event Timer");
	return (0);
}

static int
acpi_hpet_attach(device_t dev)
{
	struct acpi_hpet_softc *sc;
	int rid;
	uint32_t val;
	uintmax_t freq;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t) __func__);

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->handle = acpi_get_handle(dev);

	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL)
		return (ENOMEM);

	/* Validate that we can access the whole region. */
	if (rman_get_size(sc->mem_res) < HPET_MEM_WIDTH) {
		device_printf(dev, "memory region width %ld too small\n",
		    rman_get_size(sc->mem_res));
		bus_free_resource(dev, SYS_RES_MEMORY, sc->mem_res);
		return (ENXIO);
	}

	/* Read basic statistics about the timer. */
	val = bus_read_4(sc->mem_res, HPET_OFFSET_PERIOD);
	freq = (1000000000000000LL + val / 2) / val;
	if (bootverbose) {
		val = bus_read_4(sc->mem_res, HPET_OFFSET_INFO);
		device_printf(dev,
		    "vend: 0x%x rev: 0x%x num: %d hz: %jd opts:%s%s\n",
		    val >> 16, val & 0xff, (val >> 18) & 0xf, freq,
		    ((val >> 15) & 1) ? " leg_route" : "",
		    ((val >> 13) & 1) ? " count_size" : "");
	}

	/* Be sure it is enabled. */
	bus_write_4(sc->mem_res, HPET_OFFSET_ENABLE, 1);

	if (testenv("debug.acpi.hpet_test"))
		acpi_hpet_test(sc);

	hpet_timecounter.tc_frequency = freq;
	hpet_timecounter.tc_priv = sc;
	tc_init(&hpet_timecounter);

	return (0);
}

static int
acpi_hpet_detach(device_t dev)
{
	ACPI_FUNCTION_TRACE((char *)(uintptr_t) __func__);

	/* XXX Without a tc_remove() function, we can't detach. */
	return (EBUSY);
}

static int
acpi_hpet_resume(device_t dev)
{
	struct acpi_hpet_softc *sc;

	/* Re-enable the timer after a resume to keep the clock advancing. */
	sc = device_get_softc(dev);
	bus_write_4(sc->mem_res, HPET_OFFSET_ENABLE, 1);

	return (0);
}

/* Print some basic latency/rate information to assist in debugging. */
static void
acpi_hpet_test(struct acpi_hpet_softc *sc)
{
	int i;
	uint32_t u1, u2;
	struct bintime b0, b1, b2;
	struct timespec ts;

	binuptime(&b0);
	binuptime(&b0);
	binuptime(&b1);
	u1 = bus_read_4(sc->mem_res, HPET_OFFSET_VALUE);
	for (i = 1; i < 1000; i++)
		u2 = bus_read_4(sc->mem_res, HPET_OFFSET_VALUE);
	binuptime(&b2);
	u2 = bus_read_4(sc->mem_res, HPET_OFFSET_VALUE);

	bintime_sub(&b2, &b1);
	bintime_sub(&b1, &b0);
	bintime_sub(&b2, &b1);
	bintime2timespec(&b2, &ts);

	device_printf(sc->dev, "%ld.%09ld: %u ... %u = %u\n",
	    (long)ts.tv_sec, ts.tv_nsec, u1, u2, u2 - u1);

	device_printf(sc->dev, "time per call: %ld ns\n", ts.tv_nsec / 1000);
}

static device_method_t acpi_hpet_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, acpi_hpet_probe),
	DEVMETHOD(device_attach, acpi_hpet_attach),
	DEVMETHOD(device_detach, acpi_hpet_detach),
	DEVMETHOD(device_resume, acpi_hpet_resume),

	{0, 0}
};

static driver_t	acpi_hpet_driver = {
	"acpi_hpet",
	acpi_hpet_methods,
	sizeof(struct acpi_hpet_softc),
};


DRIVER_MODULE(acpi_hpet, acpi, acpi_hpet_driver, acpi_hpet_devclass, 0, 0);
MODULE_DEPEND(acpi_hpet, acpi, 1, 1, 1);
