/*-
 * Copyright (c) 2022 Citrix Systems R&D
 * Copyright (c) 2003-2005 Nate Lawson (SDG)
 * Copyright (c) 2001 Michael Smith
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
#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/pcpu.h>
#include <sys/power.h>
#include <sys/proc.h>
#include <sys/sched.h>

#include <machine/_inttypes.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>

#include <xen/xen-os.h>

#define ACPI_DOMAIN_COORD_TYPE_SW_ALL 0xfc
#define ACPI_DOMAIN_COORD_TYPE_SW_ANY 0xfd
#define ACPI_DOMAIN_COORD_TYPE_HW_ALL 0xfe

#define ACPI_NOTIFY_PERF_STATES 0x80	/* _PSS changed. */
#define ACPI_NOTIFY_CX_STATES	0x81	/* _CST changed. */

static MALLOC_DEFINE(M_XENACPI, "xen_acpi", "Xen CPU ACPI forwarder");

/* Hooks for the ACPI CA debugging infrastructure */
#define _COMPONENT ACPI_PROCESSOR
ACPI_MODULE_NAME("PROCESSOR")

struct xen_acpi_cpu_softc {
	device_t cpu_dev;
	ACPI_HANDLE cpu_handle;
	uint32_t cpu_acpi_id;
	struct xen_processor_cx *cpu_cx_states;
	unsigned int cpu_cx_count;
	struct xen_processor_px *cpu_px_states;
	unsigned int cpu_px_count;
	struct xen_pct_register control_register;
	struct xen_pct_register status_register;
	struct xen_psd_package psd;
};

#define	CPUDEV_DEVICE_ID "ACPI0007"

ACPI_SERIAL_DECL(cpu, "ACPI CPU");

#define device_printf(dev,...) \
	if (!device_is_quiet(dev)) \
		device_printf((dev), __VA_ARGS__)

static int
acpi_get_gas(const ACPI_OBJECT *res, unsigned int idx,
    ACPI_GENERIC_ADDRESS *gas)
{
	const ACPI_OBJECT *obj = &res->Package.Elements[idx];

	if (obj == NULL || obj->Type != ACPI_TYPE_BUFFER ||
	    obj->Buffer.Length < sizeof(ACPI_GENERIC_ADDRESS) + 3)
		return (EINVAL);

	memcpy(gas, obj->Buffer.Pointer + 3, sizeof(*gas));

	return (0);
}

static int
acpi_get_pct(const ACPI_OBJECT *res, unsigned int idx,
    struct xen_pct_register *reg)
{
	struct {
		uint8_t descriptor;
		uint16_t length;
		ACPI_GENERIC_ADDRESS gas;
	} __packed raw;
	const ACPI_OBJECT *obj = &res->Package.Elements[idx];

	if (obj == NULL || obj->Type != ACPI_TYPE_BUFFER ||
	    obj->Buffer.Length < sizeof(raw))
		return (EINVAL);

	memcpy(&raw, obj->Buffer.Pointer, sizeof(raw));
	reg->descriptor = raw.descriptor;
	reg->length = raw.length;
	reg->space_id = raw.gas.SpaceId;
	reg->bit_width = raw.gas.BitWidth;
	reg->bit_offset = raw.gas.BitOffset;
	reg->reserved = raw.gas.AccessWidth;
	reg->address = raw.gas.Address;

	return (0);
}

static int
xen_upload_cx(struct xen_acpi_cpu_softc *sc)
{
	struct xen_platform_op op = {
		.cmd			= XENPF_set_processor_pminfo,
		.interface_version	= XENPF_INTERFACE_VERSION,
		.u.set_pminfo.id	= sc->cpu_acpi_id,
		.u.set_pminfo.type	= XEN_PM_CX,
		.u.set_pminfo.u.power.count = sc->cpu_cx_count,
		.u.set_pminfo.u.power.flags.has_cst = 1,
		/* Ignore bm_check and bm_control, Xen will set those. */
	};
	int error;

	set_xen_guest_handle(op.u.set_pminfo.u.power.states, sc->cpu_cx_states);

	error = HYPERVISOR_platform_op(&op);
	if (error != 0)
		device_printf(sc->cpu_dev,
		    "ACPI ID %u Cx upload failed: %d\n", sc->cpu_acpi_id,
		    error);
	return (error);
}

static int
xen_upload_px(struct xen_acpi_cpu_softc *sc)
{
	struct xen_platform_op op = {
		.cmd = XENPF_set_processor_pminfo,
		.interface_version = XENPF_INTERFACE_VERSION,
		.u.set_pminfo.id = sc->cpu_acpi_id,
		.u.set_pminfo.type = XEN_PM_PX,
		.u.set_pminfo.u.perf.state_count = sc->cpu_px_count,
		.u.set_pminfo.u.perf.control_register = sc->control_register,
		.u.set_pminfo.u.perf.status_register = sc->status_register,
		.u.set_pminfo.u.perf.domain_info = sc->psd,
		.u.set_pminfo.u.perf.flags = XEN_PX_PPC | XEN_PX_PCT |
		    XEN_PX_PSS | XEN_PX_PSD,
	};
	ACPI_STATUS status;
	int error;

	status = acpi_GetInteger(sc->cpu_handle, "_PPC",
	    &op.u.set_pminfo.u.perf.platform_limit);
	if (ACPI_FAILURE(status)) {
		device_printf(sc->cpu_dev, "missing _PPC object\n");
		return (ENXIO);
	}

	set_xen_guest_handle(op.u.set_pminfo.u.perf.states, sc->cpu_px_states);

	/*
	 * NB: it's unclear the exact purpose of the shared_type field, or why
	 * it can't be calculated by Xen itself. Naively set it here to allow
	 * the upload to succeed.
	 */
	switch (sc->psd.coord_type) {
	case ACPI_DOMAIN_COORD_TYPE_SW_ALL:
		op.u.set_pminfo.u.perf.shared_type =
		    XEN_CPUPERF_SHARED_TYPE_ALL;
		break;

	case ACPI_DOMAIN_COORD_TYPE_HW_ALL:
		op.u.set_pminfo.u.perf.shared_type =
		    XEN_CPUPERF_SHARED_TYPE_HW;
		break;

	case ACPI_DOMAIN_COORD_TYPE_SW_ANY:
		op.u.set_pminfo.u.perf.shared_type =
		    XEN_CPUPERF_SHARED_TYPE_ANY;
		break;
	default:
		device_printf(sc->cpu_dev,
		    "unknown coordination type %#" PRIx64 "\n",
		    sc->psd.coord_type);
		return (EINVAL);
	}

	error = HYPERVISOR_platform_op(&op);
	if (error != 0)
	    device_printf(sc->cpu_dev,
		"ACPI ID %u Px upload failed: %d\n", sc->cpu_acpi_id, error);
	return (error);
}

static int
acpi_set_pdc(const struct xen_acpi_cpu_softc *sc)
{
	struct xen_platform_op op = {
		.cmd			= XENPF_set_processor_pminfo,
		.interface_version	= XENPF_INTERFACE_VERSION,
		.u.set_pminfo.id	= -1,
		.u.set_pminfo.type	= XEN_PM_PDC,
	};
	uint32_t pdc[3] = {1, 1};
	ACPI_OBJECT arg = {
		.Buffer.Type = ACPI_TYPE_BUFFER,
		.Buffer.Length = sizeof(pdc),
		.Buffer.Pointer = (uint8_t *)pdc,
	};
	ACPI_OBJECT_LIST arglist = {
		.Pointer = &arg,
		.Count = 1,
	};
	ACPI_STATUS status;
	int error;

	set_xen_guest_handle(op.u.set_pminfo.u.pdc, pdc);
	error = HYPERVISOR_platform_op(&op);
	if (error != 0) {
		device_printf(sc->cpu_dev,
		    "unable to get _PDC features from Xen: %d\n", error);
		return (error);
	}

	status = AcpiEvaluateObject(sc->cpu_handle, "_PDC", &arglist, NULL);
	if (ACPI_FAILURE(status)) {
		device_printf(sc->cpu_dev, "unable to execute _PDC - %s\n",
		    AcpiFormatException(status));
		return (ENXIO);
	}

	return (0);
}

/*
 * Parse a _CST package and set up its Cx states.  Since the _CST object
 * can change dynamically, our notify handler may call this function
 * to clean up and probe the new _CST package.
 */
static int
acpi_fetch_cx(struct xen_acpi_cpu_softc *sc)
{
	ACPI_STATUS status;
	ACPI_BUFFER buf = {
		.Length = ACPI_ALLOCATE_BUFFER,
	};
	ACPI_OBJECT *top;
	uint32_t count;
	unsigned int i;

	status = AcpiEvaluateObject(sc->cpu_handle, "_CST", NULL, &buf);
	if (ACPI_FAILURE(status)) {
		device_printf(sc->cpu_dev, "missing _CST object\n");
		return (ENXIO);
	}

	/* _CST is a package with a count and at least one Cx package. */
	top = (ACPI_OBJECT *)buf.Pointer;
	if (!ACPI_PKG_VALID(top, 2) || acpi_PkgInt32(top, 0, &count) != 0) {
		device_printf(sc->cpu_dev, "invalid _CST package\n");
		AcpiOsFree(buf.Pointer);
		return (ENXIO);
	}
	if (count != top->Package.Count - 1) {
		device_printf(sc->cpu_dev,
		    "invalid _CST state count (%u != %u)\n",
		    count, top->Package.Count - 1);
		count = top->Package.Count - 1;
	}

	sc->cpu_cx_states = mallocarray(count, sizeof(struct xen_processor_cx),
	    M_XENACPI, M_WAITOK | M_ZERO);

	sc->cpu_cx_count = 0;
	for (i = 0; i < count; i++) {
		uint32_t type;
		ACPI_GENERIC_ADDRESS gas;
		ACPI_OBJECT *pkg = &top->Package.Elements[i + 1];
		struct xen_processor_cx *cx_ptr =
		    &sc->cpu_cx_states[sc->cpu_cx_count];

		if (!ACPI_PKG_VALID(pkg, 4) ||
		    acpi_PkgInt32(pkg, 1, &type) != 0 ||
		    acpi_PkgInt32(pkg, 2, &cx_ptr->latency) != 0 ||
		    acpi_PkgInt32(pkg, 3, &cx_ptr->power) != 0 ||
		    acpi_get_gas(pkg, 0, &gas) != 0) {
			device_printf(sc->cpu_dev,
			    "skipping invalid _CST %u package\n",
			    i + 1);
			continue;
		}

		cx_ptr->type = type;
		cx_ptr->reg.space_id = gas.SpaceId;
		cx_ptr->reg.bit_width = gas.BitWidth;
		cx_ptr->reg.bit_offset = gas.BitOffset;
		cx_ptr->reg.access_size = gas.AccessWidth;
		cx_ptr->reg.address = gas.Address;
		sc->cpu_cx_count++;
	}
	AcpiOsFree(buf.Pointer);

	if (sc->cpu_cx_count == 0) {
		device_printf(sc->cpu_dev, "no valid _CST package found\n");
		free(sc->cpu_cx_states, M_XENACPI);
		sc->cpu_cx_states = NULL;
		return (ENXIO);
	}

	return (0);
}

/* Probe and setup any valid performance states (Px). */
static int
acpi_fetch_px(struct xen_acpi_cpu_softc *sc)
{
	ACPI_BUFFER buf = {
		.Length = ACPI_ALLOCATE_BUFFER,
	};
	ACPI_OBJECT *pkg, *res;
	ACPI_STATUS status;
	unsigned int count, i;
	int error;
	uint64_t *p;

	/* _PSS */
	status = AcpiEvaluateObject(sc->cpu_handle, "_PSS", NULL, &buf);
	if (ACPI_FAILURE(status)) {
		device_printf(sc->cpu_dev, "missing _PSS object\n");
		return (ENXIO);
	}

	pkg = (ACPI_OBJECT *)buf.Pointer;
	if (!ACPI_PKG_VALID(pkg, 1)) {
		device_printf(sc->cpu_dev, "invalid top level _PSS package\n");
		goto error;
	}
	count = pkg->Package.Count;

	sc->cpu_px_states = mallocarray(count, sizeof(struct xen_processor_px),
	    M_XENACPI, M_WAITOK | M_ZERO);

	/*
	 * Each state is a package of {CoreFreq, Power, TransitionLatency,
	 * BusMasterLatency, ControlVal, StatusVal}, sorted from highest
	 * performance to lowest.
	 */
	sc->cpu_px_count = 0;
	for (i = 0; i < count; i++) {
		unsigned int j;

		res = &pkg->Package.Elements[i];
		if (!ACPI_PKG_VALID(res, 6)) {
			device_printf(sc->cpu_dev,
			    "invalid _PSS package idx %u\n", i);
			continue;
		}

		/* Parse the rest of the package into the struct. */
		p = (uint64_t *)&sc->cpu_px_states[sc->cpu_px_count++];
		for (j = 0; j < 6; j++, p++)
			acpi_PkgInt(res, j, p);
	}

	/* No valid Px state found so give up. */
	if (sc->cpu_px_count == 0) {
		device_printf(sc->cpu_dev, "no valid _PSS package found\n");
		goto error;
	}
	AcpiOsFree(buf.Pointer);

	/* _PCT */
	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_BUFFER;
	status = AcpiEvaluateObject(sc->cpu_handle, "_PCT", NULL, &buf);
	if (ACPI_FAILURE(status)) {
		device_printf(sc->cpu_dev, "missing _PCT object\n");
		goto error;
	}

	/* Check the package of two registers, each a Buffer in GAS format. */
	pkg = (ACPI_OBJECT *)buf.Pointer;
	if (!ACPI_PKG_VALID(pkg, 2)) {
		device_printf(sc->cpu_dev, "invalid top level _PCT package\n");
		goto error;
	}

	error = acpi_get_pct(pkg, 0, &sc->control_register);
	if (error != 0) {
		device_printf(sc->cpu_dev,
		    "unable to fetch _PCT control register: %d\n", error);
		goto error;
	}
	error = acpi_get_pct(pkg, 1, &sc->status_register);
	if (error != 0) {
		device_printf(sc->cpu_dev,
		    "unable to fetch _PCT status register: %d\n", error);
		goto error;
	}
	AcpiOsFree(buf.Pointer);

	/* _PSD */
	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_BUFFER;
	status = AcpiEvaluateObject(sc->cpu_handle, "_PSD", NULL, &buf);
	if (ACPI_FAILURE(status)) {
		device_printf(sc->cpu_dev, "missing _PSD object\n");
		goto error;
	}

	pkg = (ACPI_OBJECT *)buf.Pointer;
	if (!ACPI_PKG_VALID(pkg, 1)) {
		device_printf(sc->cpu_dev, "invalid top level _PSD package\n");
		goto error;
	}

	res = &pkg->Package.Elements[0];
	if (!ACPI_PKG_VALID(res, 5)) {
		printf("invalid _PSD package\n");
		goto error;
	}

	p = (uint64_t *)&sc->psd;
	for (i = 0; i < 5; i++, p++)
		acpi_PkgInt(res, i, p);
	AcpiOsFree(buf.Pointer);

	return (0);

error:
	if (buf.Pointer != NULL)
		AcpiOsFree(buf.Pointer);
	if (sc->cpu_px_states != NULL) {
		free(sc->cpu_px_states, M_XENACPI);
		sc->cpu_px_states = NULL;
	}
	return (ENXIO);
}

static void
acpi_notify(ACPI_HANDLE h, UINT32 notify, void *context)
{
	struct xen_acpi_cpu_softc *sc = context;

	switch (notify) {
	case ACPI_NOTIFY_PERF_STATES:
		if (acpi_fetch_px(sc) != 0)
			break;
		xen_upload_px(sc);
		free(sc->cpu_px_states, M_XENACPI);
		sc->cpu_px_states = NULL;
		break;

	case ACPI_NOTIFY_CX_STATES:
		if (acpi_fetch_cx(sc) != 0)
			break;
		xen_upload_cx(sc);
		free(sc->cpu_cx_states, M_XENACPI);
		sc->cpu_cx_states = NULL;
		break;
	}
}

static int
xen_acpi_cpu_probe(device_t dev)
{
	static char *cpudev_ids[] = { CPUDEV_DEVICE_ID, NULL };
	ACPI_OBJECT_TYPE type = acpi_get_type(dev);

	if (!xen_initial_domain())
		return (ENXIO);
	if (type != ACPI_TYPE_PROCESSOR && type != ACPI_TYPE_DEVICE)
		return (ENXIO);
	if (type == ACPI_TYPE_DEVICE &&
	    ACPI_ID_PROBE(device_get_parent(dev), dev, cpudev_ids, NULL) >= 0)
		return (ENXIO);

	device_set_desc(dev, "XEN ACPI CPU");
	if (!bootverbose)
		device_quiet(dev);

	/*
	 * Use SPECIFIC because when running as a Xen dom0 the ACPI PROCESSOR
	 * data is the native one, and needs to be forwarded to Xen but not
	 * used by FreeBSD itself.
	 */
	return (BUS_PROBE_SPECIFIC);
}

static bool
is_processor_online(unsigned int acpi_id)
{
	unsigned int i, maxid;
	struct xen_platform_op op = {
		.cmd = XENPF_get_cpuinfo,
	};
	int ret = HYPERVISOR_platform_op(&op);

	if (ret)
		return (false);

	maxid = op.u.pcpu_info.max_present;
	for (i = 0; i <= maxid; i++) {
		op.u.pcpu_info.xen_cpuid = i;
		ret = HYPERVISOR_platform_op(&op);
		if (ret)
			continue;
		if (op.u.pcpu_info.acpi_id == acpi_id)
			return (op.u.pcpu_info.flags & XEN_PCPU_FLAGS_ONLINE);
	}

	return (false);
}

static int
xen_acpi_cpu_attach(device_t dev)
{
	struct xen_acpi_cpu_softc *sc = device_get_softc(dev);
	ACPI_STATUS status;
	int error;

	sc->cpu_dev = dev;
	sc->cpu_handle = acpi_get_handle(dev);

	if (acpi_get_type(dev) == ACPI_TYPE_PROCESSOR) {
		ACPI_BUFFER buf = {
			.Length = ACPI_ALLOCATE_BUFFER,
		};
		ACPI_OBJECT *obj;

		status = AcpiEvaluateObject(sc->cpu_handle, NULL, NULL, &buf);
		if (ACPI_FAILURE(status)) {
			device_printf(dev,
			    "attach failed to get Processor obj - %s\n",
			    AcpiFormatException(status));
			return (ENXIO);
		}
		obj = (ACPI_OBJECT *)buf.Pointer;
		sc->cpu_acpi_id = obj->Processor.ProcId;
		AcpiOsFree(obj);
	} else {
		KASSERT(acpi_get_type(dev) == ACPI_TYPE_DEVICE,
		    ("Unexpected ACPI object"));
		status = acpi_GetInteger(sc->cpu_handle, "_UID",
		    &sc->cpu_acpi_id);
		if (ACPI_FAILURE(status)) {
			device_printf(dev, "device object has bad value - %s\n",
			    AcpiFormatException(status));
			return (ENXIO);
		}
	}

	if (!is_processor_online(sc->cpu_acpi_id))
		/* Processor is not online, attach the driver and ignore it. */
		return (0);

	/*
	 * Install the notify handler now: even if we fail to parse or upload
	 * the states it shouldn't prevent us from attempting to parse further
	 * updates.
	 */
	status = AcpiInstallNotifyHandler(sc->cpu_handle, ACPI_DEVICE_NOTIFY,
	    acpi_notify, sc);
	if (ACPI_FAILURE(status))
		device_printf(dev, "failed to register notify handler - %s\n",
		    AcpiFormatException(status));

	/*
	 * Don't report errors: it's likely there are processor objects
	 * belonging to CPUs that are not online, but the MADT provided to
	 * FreeBSD is crafted to report the number of CPUs available to dom0.
	 *
	 * Parsing or uploading those states could result in errors, just
	 * ignore them in order to avoid pointless noise.
	 */
	error = acpi_set_pdc(sc);
	if (error != 0)
		return (0);

	error = acpi_fetch_px(sc);
	if (error != 0)
		return (0);
	error = xen_upload_px(sc);
	free(sc->cpu_px_states, M_XENACPI);
	sc->cpu_px_states = NULL;
	if (error != 0)
		return (0);

	error = acpi_fetch_cx(sc);
	if (error != 0)
		return (0);
	xen_upload_cx(sc);
	free(sc->cpu_cx_states, M_XENACPI);
	sc->cpu_cx_states = NULL;

	return (0);
}

static device_method_t xen_acpi_cpu_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe, xen_acpi_cpu_probe),
    DEVMETHOD(device_attach, xen_acpi_cpu_attach),

    DEVMETHOD_END
};

static driver_t xen_acpi_cpu_driver = {
    "xen cpu",
    xen_acpi_cpu_methods,
    sizeof(struct xen_acpi_cpu_softc),
};

DRIVER_MODULE(xen_cpu, acpi, xen_acpi_cpu_driver, 0, 0);
MODULE_DEPEND(xen_cpu, acpi, 1, 1, 1);
