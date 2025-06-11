/*-
 * Copyright (c) 2024 Rubicon Communications, LLC (Netgate)
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

#define _COMPONENT	ACPI_OEM
ACPI_MODULE_NAME("SBL-FW-UPDATE-WMI")
ACPI_SERIAL_DECL(sbl_wmi, "SBL WMI device");

#define ACPI_SBL_FW_UPDATE_WMI_GUID	"44FADEB1-B204-40F2-8581-394BBDC1B651"

struct acpi_sbl_wmi_softc {
	device_t dev;
	device_t wmi_dev;
};

static void
acpi_sbl_wmi_identify(driver_t *driver, device_t parent)
{
	/* Don't do anything if driver is disabled. */
	if (acpi_disabled("sbl_wmi"))
		return;

	/* Add only a single device instance. */
	if (device_find_child(parent, "acpi_sbl_wmi", DEVICE_UNIT_ANY) != NULL)
		return;

	/* Check management GUID to see whether system is compatible. */
	if (!ACPI_WMI_PROVIDES_GUID_STRING(parent,
	    ACPI_SBL_FW_UPDATE_WMI_GUID))
		return;

	if (BUS_ADD_CHILD(parent, 0, "acpi_sbl_wmi", DEVICE_UNIT_ANY) == NULL)
		device_printf(parent, "add acpi_sbl_wmi child failed\n");
}

static int
acpi_sbl_wmi_probe(device_t dev)
{
	if (!ACPI_WMI_PROVIDES_GUID_STRING(device_get_parent(dev),
	    ACPI_SBL_FW_UPDATE_WMI_GUID))
		return (EINVAL);
	device_set_desc(dev, "SBL Firmware Update WMI device");
	return (0);
}

static int
acpi_sbl_wmi_sysctl_get(struct acpi_sbl_wmi_softc *sc, int *val)
{
	ACPI_OBJECT	*obj;
	ACPI_BUFFER	 out = { ACPI_ALLOCATE_BUFFER, NULL };
	int		 error = 0;

	if (ACPI_FAILURE(ACPI_WMI_GET_BLOCK(sc->wmi_dev,
	    ACPI_SBL_FW_UPDATE_WMI_GUID, 0, &out))) {
		error = EINVAL;
		goto out;
	}

	obj = out.Pointer;
	if (obj->Type != ACPI_TYPE_INTEGER) {
		error = EINVAL;
		goto out;
	}

	*val = obj->Integer.Value;

out:
	if (out.Pointer)
		AcpiOsFree(out.Pointer);

	return (error);
}

static int
acpi_sbl_wmi_sysctl_set(struct acpi_sbl_wmi_softc *sc, int in)
{
	ACPI_BUFFER	 input = { ACPI_ALLOCATE_BUFFER, NULL };
	uint32_t	 val;

	val = in;
	input.Length = sizeof(val);
	input.Pointer = &val;

	if (ACPI_FAILURE(ACPI_WMI_SET_BLOCK(sc->wmi_dev,
	    ACPI_SBL_FW_UPDATE_WMI_GUID, 0, &input)))
		return (ENODEV);

	return (0);
}

static int
acpi_sbl_wmi_fw_upgrade_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct acpi_sbl_wmi_softc	*sc;
	int				 arg;
	int				 error = 0;

	ACPI_SERIAL_BEGIN(sbl_wmi);

	sc = (struct acpi_sbl_wmi_softc *)oidp->oid_arg1;
	error = acpi_sbl_wmi_sysctl_get(sc, &arg);
	if (error != 0)
		goto out;

	error = sysctl_handle_int(oidp, &arg, 0, req);
	if (! error && req->newptr != NULL)
		error = acpi_sbl_wmi_sysctl_set(sc, arg);

out:
	ACPI_SERIAL_END(sbl_wmi);

	return (error);
}

static int
acpi_sbl_wmi_attach(device_t dev)
{
	struct acpi_sbl_wmi_softc	*sc;
	struct sysctl_ctx_list		*sysctl_ctx;
	struct sysctl_oid		*sysctl_tree;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->wmi_dev = device_get_parent(dev);

	sysctl_ctx = device_get_sysctl_ctx(dev);
	sysctl_tree = device_get_sysctl_tree(dev);

	SYSCTL_ADD_PROC(sysctl_ctx,
	    SYSCTL_CHILDREN(sysctl_tree), OID_AUTO,
	    "firmware_update_request",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, acpi_sbl_wmi_fw_upgrade_sysctl, "I",
	    "Signal SBL that a firmware update is available");

	return (0);
}

static device_method_t acpi_sbl_wmi_methods[] = {
	DEVMETHOD(device_identify, acpi_sbl_wmi_identify),
	DEVMETHOD(device_probe, acpi_sbl_wmi_probe),
	DEVMETHOD(device_attach, acpi_sbl_wmi_attach),

	DEVMETHOD_END
};

static driver_t	acpi_sbl_wmi_driver = {
	"acpi_sbl_wmi",
	acpi_sbl_wmi_methods,
	sizeof(struct acpi_sbl_wmi_softc),
};

DRIVER_MODULE(acpi_sbl_wmi, acpi_wmi, acpi_sbl_wmi_driver, 0, 0);
MODULE_DEPEND(acpi_sbl_wmi, acpi_wmi, 1, 1, 1);
MODULE_DEPEND(acpi_sbl_wmi, acpi, 1, 1, 1);
