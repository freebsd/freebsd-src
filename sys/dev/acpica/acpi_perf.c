/*-
 * Copyright (c) 2003-2005 Nate Lawson (SDG)
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
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/power.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sbuf.h>
#include <sys/pcpu.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include "acpi.h"
#include <dev/acpica/acpivar.h>

#include "cpufreq_if.h"

/*
 * Support for ACPI processor performance states (Px) according to
 * section x of the ACPI specification.
 */

struct acpi_px {
	uint32_t	 core_freq;
	uint32_t	 power;
	uint32_t	 trans_lat;
	uint32_t	 bm_lat;
	uint32_t	 ctrl_val;
	uint32_t	 sts_val;
};

#define MAX_PX_STATES	 16

struct acpi_perf_softc {
	device_t	 dev;
	ACPI_HANDLE	 handle;
	struct resource	*perf_ctrl;	/* Set new performance state. */
	struct resource	*perf_status;	/* Check that transition succeeded. */
	struct acpi_px	*px_states;	/* ACPI perf states. */
	uint32_t	 px_count;	/* Total number of perf states. */
	uint32_t	 px_max_avail;	/* Lowest index state available. */
	int		 px_curr_state;	/* Active state index. */
	int		 px_rid;
};

#define PX_GET_REG(reg) 				\
	(bus_space_read_4(rman_get_bustag((reg)), 	\
	    rman_get_bushandle((reg)), 0))
#define PX_SET_REG(reg, val)				\
	(bus_space_write_4(rman_get_bustag((reg)), 	\
	    rman_get_bushandle((reg)), 0, (val)))

static void	acpi_perf_identify(driver_t *driver, device_t parent);
static int	acpi_perf_probe(device_t dev);
static int	acpi_perf_attach(device_t dev);
static int	acpi_perf_detach(device_t dev);
static int	acpi_perf_evaluate(device_t dev);
static int	acpi_px_to_set(device_t dev, struct acpi_px *px,
		    struct cf_setting *set);
static void	acpi_px_available(struct acpi_perf_softc *sc);
static void	acpi_px_notify(ACPI_HANDLE h, UINT32 notify, void *context);
static int	acpi_px_settings(device_t dev, struct cf_setting *sets,
		    int *count, int *type);
static int	acpi_px_set(device_t dev, const struct cf_setting *set);
static int	acpi_px_get(device_t dev, struct cf_setting *set);

static device_method_t acpi_perf_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	acpi_perf_identify),
	DEVMETHOD(device_probe,		acpi_perf_probe),
	DEVMETHOD(device_attach,	acpi_perf_attach),
	DEVMETHOD(device_detach,	acpi_perf_detach),

	/* cpufreq interface */
	DEVMETHOD(cpufreq_drv_set,	acpi_px_set),
	DEVMETHOD(cpufreq_drv_get,	acpi_px_get),
	DEVMETHOD(cpufreq_drv_settings,	acpi_px_settings),
	{0, 0}
};

static driver_t acpi_perf_driver = {
	"acpi_perf",
	acpi_perf_methods,
	sizeof(struct acpi_perf_softc),
};

static devclass_t acpi_perf_devclass;
DRIVER_MODULE(acpi_perf, cpu, acpi_perf_driver, acpi_perf_devclass, 0, 0);
MODULE_DEPEND(acpi_perf, acpi, 1, 1, 1);

MALLOC_DEFINE(M_ACPIPERF, "acpi_perf", "ACPI Performance states");

static void
acpi_perf_identify(driver_t *driver, device_t parent)
{
	device_t child;
	ACPI_HANDLE handle;

	/* Make sure we're not being doubly invoked. */
	if (device_find_child(parent, "acpi_perf", 0) != NULL)
		return;

	/* Get the handle for the Processor object and check for perf states. */
	handle = acpi_get_handle(parent);
	if (handle == NULL)
		return;
	if (ACPI_FAILURE(AcpiEvaluateObject(handle, "_PSS", NULL, NULL)))
		return;
	if ((child = BUS_ADD_CHILD(parent, 0, "acpi_perf", 0)) == NULL)
		device_printf(parent, "acpi_perf: add child failed\n");
}

static int
acpi_perf_probe(device_t dev)
{

	device_set_desc(dev, "ACPI CPU Frequency Control");
	return (-10);
}

static int
acpi_perf_attach(device_t dev)
{
	struct acpi_perf_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->handle = acpi_get_handle(dev);
	sc->px_max_avail = 0;
	sc->px_curr_state = CPUFREQ_VAL_UNKNOWN;
	if (acpi_perf_evaluate(dev) != 0)
		return (ENXIO);
	cpufreq_register(dev);

	return (0);
}

static int
acpi_perf_detach(device_t dev)
{
	/* TODO: teardown registers, remove notify handler. */
	return (ENXIO);
}

/* Probe and setup any valid performance states (Px). */
static int
acpi_perf_evaluate(device_t dev)
{
	struct acpi_perf_softc *sc;
	ACPI_BUFFER buf;
	ACPI_OBJECT *pkg, *res;
	ACPI_STATUS status;
	int i, j;
	uint32_t *p;

	/* Get the control values and parameters for each state. */
	sc = device_get_softc(dev);
	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_BUFFER;
	status = AcpiEvaluateObject(sc->handle, "_PSS", NULL, &buf);
	if (ACPI_FAILURE(status))
		return (ENXIO);

	pkg = (ACPI_OBJECT *)buf.Pointer;
	if (!ACPI_PKG_VALID(pkg, 1)) {
		device_printf(dev, "invalid top level _PSS package\n");
		return (ENXIO);
	}
	sc->px_count = pkg->Package.Count;

	sc->px_states = malloc(sc->px_count * sizeof(struct acpi_px),
	    M_ACPIPERF, M_WAITOK | M_ZERO);
	if (sc->px_states == NULL)
		return (ENOMEM);

	/*
	 * Each state is a package of {CoreFreq, Power, TransitionLatency,
	 * BusMasterLatency, ControlVal, StatusVal}, sorted from highest
	 * performance to lowest.
	 */
	for (i = 0; i < sc->px_count; i++) {
		res = &pkg->Package.Elements[i];
		if (!ACPI_PKG_VALID(res, 6)) {
			device_printf(dev, "invalid _PSS package\n");
			continue;
		}
		p = &sc->px_states[i].core_freq;
		for (j = 0; j < 6; j++, p++)
			acpi_PkgInt32(res, j, p);
	}
	AcpiOsFree(buf.Pointer);

	/* Get the control and status registers (one of each). */
	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_BUFFER;
	status = AcpiEvaluateObject(sc->handle, "_PCT", NULL, &buf);
	if (ACPI_FAILURE(status)) {
		free(sc->px_states, M_ACPIPERF);
		return (ENXIO);
	}

	/* Check the package of two registers, each a Buffer in GAS format. */
	pkg = (ACPI_OBJECT *)buf.Pointer;
	if (!ACPI_PKG_VALID(pkg, 2)) {
		device_printf(dev, "invalid perf register package\n");
		return (ENXIO);
	}

	acpi_PkgGas(sc->dev, pkg, 0, &sc->px_rid, &sc->perf_ctrl);
	if (sc->perf_ctrl == NULL) {
		device_printf(dev, "failed to attach PERF_CTL register\n");
		return (ENXIO);
	}
	sc->px_rid++;

	acpi_PkgGas(sc->dev, pkg, 1, &sc->px_rid, &sc->perf_status);
	if (sc->perf_status == NULL) {
		device_printf(dev, "failed to attach PERF_STATUS register\n");
		return (ENXIO);
	}
	sc->px_rid++;
	AcpiOsFree(buf.Pointer);

	/* Get our current limit and register for notifies. */
	acpi_px_available(sc);
	AcpiInstallNotifyHandler(sc->handle, ACPI_DEVICE_NOTIFY,
	    acpi_px_notify, sc);

	return (0);
}

static void
acpi_px_notify(ACPI_HANDLE h, UINT32 notify, void *context)
{
	struct acpi_perf_softc *sc;

	sc = context;
	acpi_px_available(sc);

	/* TODO: Implement notification when frequency changes. */
}

/*
 * Find the highest currently-supported performance state.
 * This can be called at runtime (e.g., due to a docking event) at
 * the request of a Notify on the processor object.
 */
static void
acpi_px_available(struct acpi_perf_softc *sc)
{
	ACPI_STATUS status;
	struct cf_setting set;

	status = acpi_GetInteger(sc->handle, "_PPC", &sc->px_max_avail);

	/* If the old state is too high, set current state to the new max. */
	if (ACPI_SUCCESS(status)) {
		if (sc->px_curr_state != CPUFREQ_VAL_UNKNOWN &&
		    sc->px_curr_state > sc->px_max_avail) {
			acpi_px_to_set(sc->dev,
			    &sc->px_states[sc->px_max_avail], &set);
			acpi_px_set(sc->dev, &set);
		}
	} else
		sc->px_max_avail = 0;
}

static int
acpi_px_to_set(device_t dev, struct acpi_px *px, struct cf_setting *set)
{

	if (px == NULL || set == NULL)
		return (EINVAL);

	set->freq = px->core_freq;
	set->power = px->power;
	/* XXX Include BM latency too? */
	set->lat = px->trans_lat;
	set->volts = CPUFREQ_VAL_UNKNOWN;
	set->dev = dev;

	return (0);
}

static int
acpi_px_settings(device_t dev, struct cf_setting *sets, int *count, int *type)
{
	struct acpi_perf_softc *sc;
	int x, y;

	sc = device_get_softc(dev);
	if (sets == NULL || count == NULL)
		return (EINVAL);
	if (*count < sc->px_count - sc->px_max_avail)
		return (ENOMEM);

	/* Return a list of settings that are currently valid. */
	y = 0;
	for (x = sc->px_max_avail; x < sc->px_count; x++, y++)
		acpi_px_to_set(dev, &sc->px_states[x], &sets[y]);
	*count = sc->px_count - sc->px_max_avail;
	*type = CPUFREQ_TYPE_ABSOLUTE;

	return (0);
}

static int
acpi_px_set(device_t dev, const struct cf_setting *set)
{
	struct acpi_perf_softc *sc;
	int i, status, sts_val, tries;

	if (set == NULL)
		return (EINVAL);
	sc = device_get_softc(dev);

	/* Look up appropriate state, based on frequency. */
	for (i = sc->px_max_avail; i < sc->px_count; i++) {
		if (CPUFREQ_CMP(set->freq, sc->px_states[i].core_freq))
			break;
	}
	if (i == sc->px_count)
		return (EINVAL);

	/* Write the appropriate value to the register. */
	PX_SET_REG(sc->perf_ctrl, sc->px_states[i].ctrl_val);

	/* Try for up to 1 ms to verify the desired state was selected. */
	sts_val = sc->px_states[i].sts_val;
	for (tries = 0; tries < 100; tries++) {
		status = PX_GET_REG(sc->perf_status);
		if (status == sts_val)
			break;
		DELAY(10);
	}
	if (tries == 100) {
		device_printf(dev, "Px transition to %d failed\n",
		    sc->px_states[i].core_freq);
		return (ENXIO);
	}
	sc->px_curr_state = i;

	return (0);
}

static int
acpi_px_get(device_t dev, struct cf_setting *set)
{
	struct acpi_perf_softc *sc;
	uint64_t rate;
	int i;
	struct pcpu *pc;

	if (set == NULL)
		return (EINVAL);
	sc = device_get_softc(dev);

	/* If we've set the rate before, use the cached value. */
	if (sc->px_curr_state != CPUFREQ_VAL_UNKNOWN) {
		acpi_px_to_set(dev, &sc->px_states[sc->px_curr_state], set);
		return (0);
	}

	/* Otherwise, estimate and try to match against our settings. */
	pc = cpu_get_pcpu(dev);
	if (pc == NULL)
		return (ENXIO);
	cpu_est_clockrate(pc->pc_cpuid, &rate);
	rate /= 1000000;
	for (i = 0; i < sc->px_count; i++) {
		if (CPUFREQ_CMP(sc->px_states[i].core_freq, rate)) {
			sc->px_curr_state = i;
			acpi_px_to_set(dev, &sc->px_states[i], set);
			break;
		}
	}

	/* No match, give up. */
	if (i == sc->px_count) {
		sc->px_curr_state = CPUFREQ_VAL_UNKNOWN;
		set->freq = CPUFREQ_VAL_UNKNOWN;
	}

	return (0);
}
