/*-
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@jp.freebsd.org>
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
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/sysctl.h>

#include "acpi.h"
#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpiio.h>

MALLOC_DEFINE(M_ACPIBATT, "acpibatt", "ACPI generic battery data");

struct acpi_batteries {
    TAILQ_ENTRY(acpi_batteries)	link;
    struct acpi_battdesc	battdesc;
};

static TAILQ_HEAD(,acpi_batteries) acpi_batteries;
static int			acpi_batteries_initted;
static int			acpi_batteries_units;
static int			acpi_battery_info_expire = 5;
static struct acpi_battinfo	acpi_battery_battinfo;
ACPI_SERIAL_DECL(battery, "ACPI generic battery");

int
acpi_battery_get_info_expire(void)
{
    return (acpi_battery_info_expire);
}

int
acpi_battery_get_units(void)
{
    return (acpi_batteries_units);
}

int
acpi_battery_get_battdesc(int unit, struct acpi_battdesc *battdesc)
{
    struct acpi_batteries *bp;
    int error, i;

    error = ENXIO;
    ACPI_SERIAL_BEGIN(battery);
    if (unit < 0 || unit >= acpi_batteries_units)
	goto out;

    i = 0;
    TAILQ_FOREACH(bp, &acpi_batteries, link) {
	if (unit == i) {
	    battdesc->type = bp->battdesc.type;
	    battdesc->phys_unit = bp->battdesc.phys_unit;
	    error = 0;
	    break;
	}
	i++;
    }

out:
    ACPI_SERIAL_END(battery);
    return (error);
}

int
acpi_battery_get_battinfo(int unit, struct acpi_battinfo *battinfo)
{
    struct acpi_battdesc battdesc;
    int error;

    error = 0;
    if (unit == -1) {
	error = acpi_cmbat_get_battinfo(-1, battinfo);
	goto out;
    } else {
	error = acpi_battery_get_battdesc(unit, &battdesc);
	if (error != 0)
	    goto out;

	switch (battdesc.type) {
	case ACPI_BATT_TYPE_CMBAT:
	    error = acpi_cmbat_get_battinfo(battdesc.phys_unit, battinfo);
	    break;
	default:
	    error = ENXIO;
	    break;
	}
    }

out:
    return (error);
}

static int
acpi_battery_ioctl(u_long cmd, caddr_t addr, void *arg)
{
    union acpi_battery_ioctl_arg *ioctl_arg;
    int error, unit;

    ioctl_arg = (union acpi_battery_ioctl_arg *)addr;
    error = 0;

    /*
     * No security check required: information retrieval only.  If
     * new functions are added here, a check might be required.
     */
    switch (cmd) {
    case ACPIIO_BATT_GET_UNITS:
	*(int *)addr = acpi_battery_get_units();
	break;
    case ACPIIO_BATT_GET_BATTDESC:
	unit = ioctl_arg->unit;
	error = acpi_battery_get_battdesc(unit, &ioctl_arg->battdesc);
	break;
    case ACPIIO_BATT_GET_BATTINFO:
	unit = ioctl_arg->unit;
	error = acpi_battery_get_battinfo(unit, &ioctl_arg->battinfo);
	break;
    default:
	error = EINVAL;
	break;
    }

    return (error);
}

static int
acpi_battery_sysctl(SYSCTL_HANDLER_ARGS)
{
    int val, error;

    acpi_battery_get_battinfo(-1, &acpi_battery_battinfo);
    val = *(u_int *)oidp->oid_arg1;
    error = sysctl_handle_int(oidp, &val, 0, req);
    return (error);
}

static int
acpi_battery_init(void)
{
    struct acpi_softc	*sc;
    device_t		 dev;
    int	 		 error;

    ACPI_SERIAL_ASSERT(battery);

    error = ENXIO;
    dev = devclass_get_device(devclass_find("acpi"), 0);
    if (dev == NULL)
	goto out;
    sc = device_get_softc(dev);

    TAILQ_INIT(&acpi_batteries);

    /* XXX We should back out registered ioctls on error. */
    error = acpi_register_ioctl(ACPIIO_BATT_GET_UNITS, acpi_battery_ioctl,
	NULL);
    if (error != 0)
	goto out;
    error = acpi_register_ioctl(ACPIIO_BATT_GET_BATTDESC, acpi_battery_ioctl,
	NULL);
    if (error != 0)
	goto out;
    error = acpi_register_ioctl(ACPIIO_BATT_GET_BATTINFO, acpi_battery_ioctl,
	NULL);
    if (error != 0)
	goto out;

    sysctl_ctx_init(&sc->acpi_battery_sysctl_ctx);
    sc->acpi_battery_sysctl_tree = SYSCTL_ADD_NODE(&sc->acpi_battery_sysctl_ctx,
	SYSCTL_CHILDREN(sc->acpi_sysctl_tree), OID_AUTO, "battery", CTLFLAG_RD,
	0, "");
    SYSCTL_ADD_PROC(&sc->acpi_battery_sysctl_ctx,
	SYSCTL_CHILDREN(sc->acpi_battery_sysctl_tree),
	OID_AUTO, "life", CTLTYPE_INT | CTLFLAG_RD,
	&acpi_battery_battinfo.cap, 0, acpi_battery_sysctl, "I", "");
    SYSCTL_ADD_PROC(&sc->acpi_battery_sysctl_ctx,
	SYSCTL_CHILDREN(sc->acpi_battery_sysctl_tree),
	OID_AUTO, "time", CTLTYPE_INT | CTLFLAG_RD,
	&acpi_battery_battinfo.min, 0, acpi_battery_sysctl, "I", "");
    SYSCTL_ADD_PROC(&sc->acpi_battery_sysctl_ctx,
	SYSCTL_CHILDREN(sc->acpi_battery_sysctl_tree),
	OID_AUTO, "state", CTLTYPE_INT | CTLFLAG_RD,
	&acpi_battery_battinfo.state, 0, acpi_battery_sysctl, "I", "");
    SYSCTL_ADD_INT(&sc->acpi_battery_sysctl_ctx,
	SYSCTL_CHILDREN(sc->acpi_battery_sysctl_tree),
	OID_AUTO, "units", CTLFLAG_RD, &acpi_batteries_units, 0, "");
    SYSCTL_ADD_INT(&sc->acpi_battery_sysctl_ctx,
	SYSCTL_CHILDREN(sc->acpi_battery_sysctl_tree),
	OID_AUTO, "info_expire", CTLFLAG_RD | CTLFLAG_RW,
	&acpi_battery_info_expire, 0, "");

    acpi_batteries_initted = TRUE;

out:
    return (error);
}

int
acpi_battery_register(int type, int phys_unit)
{
    struct acpi_batteries *bp;
    int error;

    error = 0;
    bp = malloc(sizeof(*bp), M_ACPIBATT, M_NOWAIT);
    if (bp == NULL)
	return (ENOMEM);

    ACPI_SERIAL_BEGIN(battery);
    if (!acpi_batteries_initted && (error = acpi_battery_init()) != 0) {
	printf("acpi_battery_register failed for unit %d\n", phys_unit);
	goto out;
    }
    bp->battdesc.type = type;
    bp->battdesc.phys_unit = phys_unit;
    TAILQ_INSERT_TAIL(&acpi_batteries, bp, link);
    acpi_batteries_units++;

out:
    ACPI_SERIAL_END(battery);
    if (error)
	free(bp, M_ACPIBATT);
    return (error);
}

int
acpi_battery_remove(int type, int phys_unit)
{
    struct acpi_batteries *bp, *tmp;
    int ret;

    ret = ENOENT;
    ACPI_SERIAL_BEGIN(battery);
    TAILQ_FOREACH_SAFE(bp, &acpi_batteries, link, tmp) {
	if (bp->battdesc.type == type && bp->battdesc.phys_unit == phys_unit) {
	    TAILQ_REMOVE(&acpi_batteries, bp, link);
	    acpi_batteries_units--;
	    ret = 0;
	    break;
	}
    }
    ACPI_SERIAL_END(battery);
    if (ret == 0)
	free(bp, M_ACPIBATT);
    return (ret);
}
