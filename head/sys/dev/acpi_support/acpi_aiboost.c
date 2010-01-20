/*-
 * Copyright (c) 2006 Takanori Watanabe
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
#include <sys/bus.h>

#include <contrib/dev/acpica/include/acpi.h>

#include "acpi_if.h"
#include <sys/module.h>
#include <dev/acpica/acpivar.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>

#define	_COMPONENT	ACPI_OEM
ACPI_MODULE_NAME("AIBOOST")

#define	DESCSTRLEN	32

struct acpi_aiboost_element {
	uint32_t id;
	char	desc[DESCSTRLEN];
};

ACPI_SERIAL_DECL(aiboost, "ACPI AIBOOST");

struct acpi_aiboost_component {
	u_int	num;
	struct acpi_aiboost_element elem[0];
};

struct acpi_aiboost_softc {
	int	pid;
	struct acpi_aiboost_component *temp;
	struct acpi_aiboost_component *volt;
	struct acpi_aiboost_component *fan;
};

static int	acpi_aiboost_probe(device_t dev);
static int	acpi_aiboost_attach(device_t dev);
static int	acpi_aiboost_detach(device_t dev);

static device_method_t acpi_aiboost_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, acpi_aiboost_probe),
	DEVMETHOD(device_attach, acpi_aiboost_attach),
	DEVMETHOD(device_detach, acpi_aiboost_detach),

	{0, 0}
};

static driver_t	acpi_aiboost_driver = {
	"acpi_aiboost",
	acpi_aiboost_methods,
	sizeof(struct acpi_aiboost_softc),
};

static devclass_t acpi_aiboost_devclass;

DRIVER_MODULE(acpi_aiboost, acpi, acpi_aiboost_driver, acpi_aiboost_devclass,
    0, 0);
MODULE_DEPEND(acpi_aiboost, acpi, 1, 1, 1);

static char    *abs_id[] = {"ATK0110", NULL};

/* VSIF, RVLT, SVLT, TSIF, RTMP, STMP, FSIF, RFAN, SFAN */

static ACPI_STATUS
acpi_aiboost_getcomponent(device_t dev, char *name,
    struct acpi_aiboost_component **comp)
{
	ACPI_BUFFER buf, buf2;
	ACPI_OBJECT *elem, *o, *subobj;
	ACPI_STATUS status;
	struct acpi_aiboost_component *c;
	int i;

	c = NULL;
	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_BUFFER;
	buf2.Pointer = NULL;
	status = AcpiEvaluateObject(acpi_get_handle(dev), name, NULL, &buf);

	if (ACPI_FAILURE(status))
		return (status);

	o = buf.Pointer;
	if (o->Type != ACPI_TYPE_PACKAGE) {
		device_printf(dev, "%s is not a package\n", name);
		goto error;
	}

	elem = o->Package.Elements;
	if (elem->Type != ACPI_TYPE_INTEGER) {
		device_printf(dev, "First item in %s is not a count\n", name);
		goto error;
	}

	if (elem->Integer.Value != o->Package.Count - 1) {
		device_printf(dev, "Device count mismatch in %s\n", name);
		goto error;
	}

	c = malloc(sizeof(struct acpi_aiboost_component) +
	    sizeof(struct acpi_aiboost_element) * elem->Integer.Value, M_DEVBUF,
	    M_WAITOK | M_ZERO);
	*comp = c;
	c->num = elem->Integer.Value;

	for (i = 1; i < o->Package.Count; i++) {
		elem = &o->Package.Elements[i];
		if (elem->Type == ACPI_TYPE_ANY) {
			buf2.Pointer = NULL;
			buf2.Length = ACPI_ALLOCATE_BUFFER;

			status = AcpiEvaluateObject(elem->Reference.Handle,
			    NULL, NULL, &buf2);
			if (ACPI_FAILURE(status)) {
				device_printf(dev,
				    "Failed to fetch object for %s\n", name);
				goto error;
			}
			subobj = buf2.Pointer;
		} else if (elem->Type == ACPI_TYPE_PACKAGE)
			subobj = elem;
		else {
			device_printf(dev,
			    "Subitem %d was not a package for %s\n", i, name);
			goto error;
		}
		status = acpi_PkgInt32(subobj, 0, &c->elem[i - 1].id);
		if (ACPI_FAILURE(status)) {
			device_printf(dev,
			    "Failed to fetch ID for subobject %d in %s\n", i,
			    name);
			goto error;
		}
		status = acpi_PkgStr(subobj, 1, c->elem[i - 1].desc,
		    sizeof(c->elem[i - 1].desc));
		if (ACPI_FAILURE(status)){
			if (status == E2BIG) {
				c->elem[i - 1].desc[DESCSTRLEN-1] = 0;
			} else {
				device_printf(dev,
		    "Failed to fetch description for subobject %d in %s\n",
				    i, name);
				goto error;
			}
		}

		if (buf2.Pointer) {
			AcpiOsFree(buf2.Pointer);
			buf2.Pointer = NULL;
		}
	}

	if (buf.Pointer)
		AcpiOsFree(buf.Pointer);

	return (0);

error:
	if (buf.Pointer)
		AcpiOsFree(buf.Pointer);
	if (buf2.Pointer)
		AcpiOsFree(buf2.Pointer);
	if (c)
		free(c, M_DEVBUF);
	return (AE_BAD_DATA);
}

static int
acpi_aiboost_get_value(ACPI_HANDLE handle, char *path, UINT32 number)
{
	ACPI_OBJECT arg1, *ret;
	ACPI_OBJECT_LIST args;
	ACPI_BUFFER buf;
	int val;

	arg1.Type = ACPI_TYPE_INTEGER;
	arg1.Integer.Value = number;
	args.Count = 1;
	args.Pointer = &arg1;
	buf.Length = ACPI_ALLOCATE_BUFFER;
	buf.Pointer = NULL;
	if (ACPI_FAILURE(AcpiEvaluateObject(handle, path, &args, &buf)))
		return (-1);

	ret = buf.Pointer;
	if (ret->Type == ACPI_TYPE_INTEGER)
		val = ret->Integer.Value;
	else
		val = -1;

	AcpiOsFree(buf.Pointer);
	return (val);
}


static int
acpi_aiboost_temp_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev;
	int error, val;

	dev = arg1;
	ACPI_SERIAL_BEGIN(aiboost);
	val = acpi_aiboost_get_value(acpi_get_handle(dev), "RTMP", arg2);
	error = sysctl_handle_int(oidp, &val, 0, req);
	ACPI_SERIAL_END(aiboost);
	return (error);
}

static int
acpi_aiboost_volt_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev;
	int error, val;

	dev = arg1;
	ACPI_SERIAL_BEGIN(aiboost);
	val = acpi_aiboost_get_value(acpi_get_handle(dev), "RVLT", arg2);
	error = sysctl_handle_int(oidp, &val, 0, req);
	ACPI_SERIAL_END(aiboost);
	return (error);
}

static int
acpi_aiboost_fan_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev;
	int error, val;

	dev = arg1;
	ACPI_SERIAL_BEGIN(aiboost);
	val = acpi_aiboost_get_value(acpi_get_handle(dev), "RFAN", arg2);
	error = sysctl_handle_int(oidp, &val, 0, req);
	ACPI_SERIAL_END(aiboost);
	return (error);
}

static int
acpi_aiboost_probe(device_t dev)
{

	if (ACPI_ID_PROBE(device_get_parent(dev), dev, abs_id) == NULL)
		return (ENXIO);

	device_set_desc(dev, "ASUStek AIBOOSTER");
	return (0);
}

static int
acpi_aiboost_attach(device_t dev)
{
	struct acpi_aiboost_softc *sc;
	char nambuf[16];
	int i;

	sc = device_get_softc(dev);
	if (ACPI_FAILURE(acpi_aiboost_getcomponent(dev, "TSIF", &sc->temp)))
		goto error;
	for (i = 0; i < sc->temp->num; i++) {
		snprintf(nambuf, sizeof(nambuf), "temp%d", i);
		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
		    nambuf, CTLTYPE_INT | CTLFLAG_RD, dev, sc->temp->elem[i].id,
		    acpi_aiboost_temp_sysctl, "I", sc->temp->elem[i].desc);
	}

	if (ACPI_FAILURE(acpi_aiboost_getcomponent(dev, "VSIF", &sc->volt)))
		goto error;
	for (i = 0; i < sc->volt->num; i++) {
		snprintf(nambuf, sizeof(nambuf), "volt%d", i);
		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
		    nambuf, CTLTYPE_INT | CTLFLAG_RD, dev, sc->volt->elem[i].id,
		    acpi_aiboost_volt_sysctl, "I", sc->volt->elem[i].desc);
	}

	if (ACPI_FAILURE(acpi_aiboost_getcomponent(dev, "FSIF", &sc->fan)))
		goto error;
	for (i = 0; i < sc->fan->num; i++) {
		snprintf(nambuf, sizeof(nambuf), "fan%d", i);
		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
		    nambuf, CTLTYPE_INT | CTLFLAG_RD, dev, sc->fan->elem[i].id,
		    acpi_aiboost_fan_sysctl, "I", sc->fan->elem[i].desc);
	}

	return (0);
 error:
	return (EINVAL);
}

static int
acpi_aiboost_detach(device_t dev)
{
	struct acpi_aiboost_softc *sc;

	sc = device_get_softc(dev);
	if (sc->temp)
		free(sc->temp, M_DEVBUF);
	if (sc->volt)
		free(sc->volt, M_DEVBUF);
	if (sc->fan)
		free(sc->fan, M_DEVBUF);
	return (0);
}

#if 0
static int
acpi_aiboost_suspend(device_t dev)
{
	struct acpi_aiboost_softc *sc = device_get_softc(dev);
	return (0);
}

static int
acpi_aiboost_resume(device_t dev)
{
	return (0);
}
#endif
