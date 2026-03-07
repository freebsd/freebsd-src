/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Pouria Mousavizadeh Tehrani <pouria@FreeBSD.org>
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
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>
#include <sys/sysctl.h>

#define _COMPONENT ACPI_OEM
ACPI_MODULE_NAME("system76")

static char	*system76_ids[] = { "17761776", NULL };
ACPI_SERIAL_DECL(system76, "System76 ACPI management");

struct acpi_ctrl {
	int	val;
	bool	exists;
};

struct acpi_system76_softc {
	device_t	dev;
	ACPI_HANDLE	handle;

	struct acpi_ctrl	kbb,	/* S76_CTRL_KBB */
				kbc;	/* S76_CTRL_KBC */

	struct sysctl_ctx_list	sysctl_ctx;
	struct sysctl_oid	*sysctl_tree;
};

static int	acpi_system76_probe(device_t);
static int	acpi_system76_attach(device_t);
static int	acpi_system76_detach(device_t);
static void	acpi_system76_init(struct acpi_system76_softc *);
static struct acpi_ctrl *
		acpi_system76_ctrl_map(struct acpi_system76_softc *, int);
static int	acpi_system76_update(struct acpi_system76_softc *, int, bool);
static int	acpi_system76_sysctl_handler(SYSCTL_HANDLER_ARGS);
static void	acpi_system76_notify_handler(ACPI_HANDLE, uint32_t, void *);
static void	acpi_system76_check(struct acpi_system76_softc *);

/* methods */
#define	S76_CTRL_KBB	1	/* Keyboard Brightness */
#define	S76_CTRL_KBC	2	/* Keyboard Color */
#define	S76_CTRL_MAX	3

struct s76_ctrl_table {
	char	*name;
	char	*get_method;
#define S76_CTRL_GKBB	"\\_SB.S76D.GKBB"
#define S76_CTRL_GKBC	"\\_SB.S76D.GKBC"

	char	*set_method;
#define S76_CTRL_SKBB	"\\_SB.S76D.SKBB"
#define S76_CTRL_SKBC	"\\_SB.S76D.SKBC"

	char	*desc;
};

static const struct s76_ctrl_table s76_sysctl_table[] = {
	[S76_CTRL_KBB] = {
		.name = "keyboard_backlight",
		.get_method = S76_CTRL_GKBB,
		.set_method = S76_CTRL_SKBB,
		.desc = "Keyboard Backlight",
	},
	[S76_CTRL_KBC] = {
		.name = "keyboard_color",
		.get_method = S76_CTRL_GKBC,
		.set_method = S76_CTRL_SKBC,
		.desc = "Keyboard Color",
	},
};

static device_method_t acpi_system76_methods[] = {
	DEVMETHOD(device_probe, acpi_system76_probe),
	DEVMETHOD(device_attach, acpi_system76_attach),
	DEVMETHOD(device_detach, acpi_system76_detach),

	DEVMETHOD_END
};

/* Notify event */
#define	ACPI_NOTIFY_BACKLIGHT_CHANGED	0x80
#define	ACPI_NOTIFY_COLOR_TOGGLE	0x81
#define	ACPI_NOTIFY_COLOR_DOWN		0x82
#define	ACPI_NOTIFY_COLOR_UP		0x83
#define	ACPI_NOTIFY_COLOR_CHANGED	0x84

static driver_t acpi_system76_driver = {
	"acpi_system76",
	acpi_system76_methods,
	sizeof(struct acpi_system76_softc)
};

/*
 * Returns corresponding acpi_ctrl of softc from method
 */
static struct acpi_ctrl *
acpi_system76_ctrl_map(struct acpi_system76_softc *sc, int method)
{

	switch (method) {
	case S76_CTRL_KBB:
		return (&sc->kbb);
		break;
	case S76_CTRL_KBC:
		return (&sc->kbc);
		break;
	default:
		device_printf(sc->dev, "Driver received unknown method\n");
		return (NULL);
	}
}

static int
acpi_system76_update(struct acpi_system76_softc *sc, int method, bool set)
{
	struct acpi_ctrl *ctrl;
	ACPI_STATUS status;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);
	ACPI_SERIAL_ASSERT(system76);

	if ((ctrl = acpi_system76_ctrl_map(sc, method)) == NULL)
		return (EINVAL);

	if (set)
		status = acpi_SetInteger(sc->handle, s76_sysctl_table[method].set_method,
		    ctrl->val);
	else
		status = acpi_GetInteger(sc->handle, s76_sysctl_table[method].get_method,
		    &ctrl->val);
	if (ACPI_FAILURE(status)) {
		device_printf(sc->dev, "Couldn't query method (%s)\n",
		    s76_sysctl_table[method].name);
		return (status);
	}

	return (0);
}

static void
acpi_system76_notify_update(void *arg)
{
	struct acpi_system76_softc *sc;
	int method;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	sc = (struct acpi_system76_softc *)device_get_softc(arg);

	ACPI_SERIAL_BEGIN(system76);
	for (method = 1; method < S76_CTRL_MAX; method++)
		acpi_system76_update(sc, method, false);
	ACPI_SERIAL_END(system76);
}

static void
acpi_system76_check(struct acpi_system76_softc *sc)
{
	struct acpi_ctrl *ctrl;
	int method;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);
	ACPI_SERIAL_ASSERT(system76);

	for (method = 1; method < S76_CTRL_MAX; method++) {
		if ((ctrl = acpi_system76_ctrl_map(sc, method)) == NULL)
			continue;

		if (ACPI_FAILURE(acpi_GetInteger(sc->handle,
		    s76_sysctl_table[method].get_method, &ctrl->val))) {
			ctrl->exists = false;
			device_printf(sc->dev, "Driver can't control %s\n",
			    s76_sysctl_table[method].desc);
		} else {
			ctrl->exists = true;
			acpi_system76_update(sc, method, false);
		}
	}
}

static void
acpi_system76_notify_handler(ACPI_HANDLE handle, uint32_t notify, void *ctx)
{

	ACPI_FUNCTION_TRACE_U32((char *)(uintptr_t)__func__, notify);

	switch (notify) {
	case ACPI_NOTIFY_BACKLIGHT_CHANGED:
	case ACPI_NOTIFY_COLOR_TOGGLE:
	case ACPI_NOTIFY_COLOR_DOWN:
	case ACPI_NOTIFY_COLOR_UP:
	case ACPI_NOTIFY_COLOR_CHANGED:
		AcpiOsExecute(OSL_NOTIFY_HANDLER,
		    acpi_system76_notify_update, ctx);
		break;
	default:
		break;
	}
}

static int
acpi_system76_sysctl_handler(SYSCTL_HANDLER_ARGS)
{
	struct acpi_ctrl *ctrl;
	struct acpi_system76_softc *sc;
	int val, method, error;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	sc = (struct acpi_system76_softc *)oidp->oid_arg1;
	method = oidp->oid_arg2;
	if ((ctrl = acpi_system76_ctrl_map(sc, method)) == NULL)
		return (EINVAL);

	val = ctrl->val;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0) {
		device_printf(sc->dev, "Driver query failed\n");
		return (error);
	}
	if (req->newptr == NULL)
		return (error);

	/* Input validation */
	switch (method) {
	case S76_CTRL_KBB:
		if (val > UINT8_MAX || val < 0)
			return (EINVAL);
		break;
	case S76_CTRL_KBC:
		if (val >= (1 << 24) || val < 0)
			return (EINVAL);
		break;
	default:
		break;
	}

	ctrl->val = val;

	ACPI_SERIAL_BEGIN(system76);
	error = acpi_system76_update(sc, method, true);
	ACPI_SERIAL_END(system76);
	return (error);
}

static void
acpi_system76_init(struct acpi_system76_softc *sc)
{
	struct acpi_softc *acpi_sc;
	struct acpi_ctrl *ctrl;
	uint32_t method;

	ACPI_SERIAL_ASSERT(system76);

	acpi_system76_check(sc);
	acpi_sc = acpi_device_get_parent_softc(sc->dev);
	sysctl_ctx_init(&sc->sysctl_ctx);
	sc->sysctl_tree = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
	    SYSCTL_CHILDREN(acpi_sc->acpi_sysctl_tree), OID_AUTO, "s76",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "system76 control");

	for (method = 1; method < S76_CTRL_MAX; method++) {
		if ((ctrl = acpi_system76_ctrl_map(sc, method)) == NULL)
			continue;

		if (!ctrl->exists)
			continue;

		SYSCTL_ADD_PROC(&sc->sysctl_ctx,
		    SYSCTL_CHILDREN(sc->sysctl_tree), OID_AUTO, s76_sysctl_table[method].name,
		    CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_ANYBODY | CTLFLAG_MPSAFE,
		    sc, method, acpi_system76_sysctl_handler, "IU", s76_sysctl_table[method].desc);
	}
}

static int
acpi_system76_attach(device_t dev)
{
	struct acpi_system76_softc *sc;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->handle = acpi_get_handle(dev);

	AcpiInstallNotifyHandler(sc->handle, ACPI_DEVICE_NOTIFY,
	    acpi_system76_notify_handler, dev);

	ACPI_SERIAL_BEGIN(system76);
	acpi_system76_init(sc);
	ACPI_SERIAL_END(system76);

	return (0);
}

static int
acpi_system76_detach(device_t dev)
{
	struct acpi_system76_softc *sc;

	sc = device_get_softc(dev);
	if (sysctl_ctx_free(&sc->sysctl_ctx) != 0)
		return (EBUSY);

	return (0);
}

static int
acpi_system76_probe(device_t dev)
{
	int rv;

	if (acpi_disabled("system76") || device_get_unit(dev) > 1)
		return (ENXIO);
	rv = ACPI_ID_PROBE(device_get_parent(dev), dev, system76_ids, NULL);
	if (rv > 0) {
		return (rv);
	}

	return (BUS_PROBE_VENDOR);
}

DRIVER_MODULE(acpi_system76, acpi, acpi_system76_driver, 0, 0);
MODULE_VERSION(acpi_system76, 1);
MODULE_DEPEND(acpi_system76, acpi, 1, 1, 1);
