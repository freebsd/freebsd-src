/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Scott Long
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

#include "opt_acpi.h"
#include "opt_thunderbolt.h"
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/sbuf.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <dev/acpica/acpivar.h>
#include "acpi_wmi_if.h"

ACPI_MODULE_NAME("THUNDERBOLT-NHI-WMI")

#define ACPI_INTEL_THUNDERBOLT_GUID	"86CCFD48-205E-4A77-9C48-2021CBEDE341"

struct nhi_wmi_softc {
	device_t	dev;
	device_t	wmi_dev;
	u_int		state;
	char		*guid;
	struct sysctl_ctx_list	*sysctl_ctx;
	struct sysctl_oid	*sysctl_tree;
};

ACPI_SERIAL_DECL(nhi_wmi, "Thunderbolt NHI WMI device");

static void	nhi_wmi_identify(driver_t *driver, device_t parent);
static int	nhi_wmi_probe(device_t dev);
static int	nhi_wmi_attach(device_t dev);
static int	nhi_wmi_detach(device_t dev);
static int	nhi_wmi_sysctl(SYSCTL_HANDLER_ARGS);
static int	nhi_wmi_evaluate_method(struct nhi_wmi_softc *sc,
    int method, uint32_t arg0, uint32_t *retval);

static device_method_t nhi_wmi_methods[] = {
	DEVMETHOD(device_identify, nhi_wmi_identify),
	DEVMETHOD(device_probe, nhi_wmi_probe),
	DEVMETHOD(device_attach, nhi_wmi_attach),
	DEVMETHOD(device_detach, nhi_wmi_detach),

	DEVMETHOD_END
};

static driver_t nhi_wmi_driver = {
	"nhi_wmi",
	nhi_wmi_methods,
	sizeof(struct nhi_wmi_softc)
};

DRIVER_MODULE(nhi_wmi, acpi_wmi, nhi_wmi_driver,
    NULL, NULL);
MODULE_DEPEND(nhi_wmi, acpi_wmi, 1, 1, 1);
MODULE_DEPEND(nhi_wmi, acpi, 1, 1, 1);

static void
nhi_wmi_identify(driver_t *driver, device_t parent)
{

	if (acpi_disabled("nhi_wmi") != 0)
		return;

	if (device_find_child(parent, "nhi_wmi", -1) != NULL)
		return;

	if (ACPI_WMI_PROVIDES_GUID_STRING(parent,
	    ACPI_INTEL_THUNDERBOLT_GUID) == 0)
		return;

	if (BUS_ADD_CHILD(parent, 0, "nhi_wmi", -1) == NULL)
		device_printf(parent, "failed to add nhi_wmi\n");
}

static int
nhi_wmi_probe(device_t dev)
{

	if (ACPI_WMI_PROVIDES_GUID_STRING(device_get_parent(dev),
	    ACPI_INTEL_THUNDERBOLT_GUID) == 0)
		return (EINVAL);
	device_set_desc(dev, "Thunderbolt WMI Endpoint");
	return (BUS_PROBE_DEFAULT);
}

static int
nhi_wmi_attach(device_t dev)
{
	struct nhi_wmi_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->wmi_dev = device_get_parent(dev);

	sc->sysctl_ctx = device_get_sysctl_ctx(dev);
	sc->sysctl_tree = device_get_sysctl_tree(dev);
	sc->state = 0;
	sc->guid = ACPI_INTEL_THUNDERBOLT_GUID;

	SYSCTL_ADD_STRING(sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "GUID", CTLFLAG_RD, sc->guid, 0, "WMI GUID");
	SYSCTL_ADD_PROC(sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "force_power", CTLTYPE_INT|CTLFLAG_RW|CTLFLAG_MPSAFE,
	    sc, 0, nhi_wmi_sysctl, "I", "Force controller power on");

	return (0);
}

static int
nhi_wmi_detach(device_t dev)
{

	return (0);
}

static int
nhi_wmi_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct nhi_wmi_softc *sc;
	int error, arg;

	sc = (struct nhi_wmi_softc *)arg1;
	arg = !!sc->state;
	error = sysctl_handle_int(oidp, &arg, 0, req);
	if (!error && req->newptr != NULL) {
		ACPI_SERIAL_BEGIN(nhi_wmi);
		error = nhi_wmi_evaluate_method(sc, 1, arg, NULL);
		ACPI_SERIAL_END(nhi_wmi);
		if (error == 0)
			sc->state = arg;
	}
	return (error);
}

static int
nhi_wmi_evaluate_method(struct nhi_wmi_softc *sc, int method, uint32_t arg0,
    uint32_t *retval)
{
	ACPI_OBJECT	*obj;
	ACPI_BUFFER	in, out;
	uint32_t	val, params[1];

	params[0] = arg0;
	in.Pointer = &params;
	in.Length = sizeof(params);
	out.Pointer = NULL;
	out.Length = ACPI_ALLOCATE_BUFFER;

	if (ACPI_FAILURE(ACPI_WMI_EVALUATE_CALL(sc->wmi_dev,
	    ACPI_INTEL_THUNDERBOLT_GUID, 0, method, &in, &out))) {
		AcpiOsFree(out.Pointer);
		return (EINVAL);
	}

	obj = out.Pointer;
	if (obj != NULL && obj->Type == ACPI_TYPE_INTEGER)
		val = (uint32_t)obj->Integer.Value;
	else
		val = 0;

	AcpiOsFree(out.Pointer);
	if (retval)
		*retval = val;

	return (0);
}
