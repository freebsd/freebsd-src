/*-
 * Copyright (c) 2000 Munehiro Matsuda
 * Copyright (c) 2000 Takanori Watanabe
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/ioccom.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/malloc.h>

#include "acpi.h"
#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpiio.h>

MALLOC_DEFINE(M_ACPICMBAT, "acpicmbat", "ACPI control method battery data");

/* Number of times to retry initialization before giving up. */
#define ACPI_CMBAT_RETRY_MAX	6

/* Check the battery once a minute. */
#define	CMBAT_POLLRATE		(60 * hz)

/* Hooks for the ACPI CA debugging infrastructure */
#define	_COMPONENT	ACPI_BATTERY
ACPI_MODULE_NAME("BATTERY")

#define	ACPI_BATTERY_BST_CHANGE	0x80
#define	ACPI_BATTERY_BIF_CHANGE	0x81

struct acpi_cmbat_softc {
    device_t	    dev;

    struct acpi_bif bif;
    struct acpi_bst bst;
    struct timespec bif_lastupdated;
    struct timespec bst_lastupdated;

    int		    flags;
    int		    present;
    int		    cap;
    int		    min;
    int		    full_charge_time;
    int		    initializing;
    int		    phys_unit;
};

static struct timespec	acpi_cmbat_info_lastupdated;
ACPI_SERIAL_DECL(cmbat, "ACPI cmbat");

/* XXX: devclass_get_maxunit() don't give us the current allocated units. */
static int		acpi_cmbat_units = 0;

static int		acpi_cmbat_info_expired(struct timespec *);
static void		acpi_cmbat_info_updated(struct timespec *);
static void		acpi_cmbat_get_bst(void *);
static void		acpi_cmbat_get_bif(void *);
static void		acpi_cmbat_notify_handler(ACPI_HANDLE, UINT32, void *);
static int		acpi_cmbat_probe(device_t);
static int		acpi_cmbat_attach(device_t);
static int		acpi_cmbat_detach(device_t);
static int		acpi_cmbat_resume(device_t);
static int		acpi_cmbat_ioctl(u_long, caddr_t, void *);
static int		acpi_cmbat_is_bst_valid(struct acpi_bst*);
static int		acpi_cmbat_is_bif_valid(struct acpi_bif*);
static int		acpi_cmbat_get_total_battinfo(struct acpi_battinfo *);
static void		acpi_cmbat_init_battery(void *);

static device_method_t acpi_cmbat_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	acpi_cmbat_probe),
    DEVMETHOD(device_attach,	acpi_cmbat_attach),
    DEVMETHOD(device_detach,	acpi_cmbat_detach),
    DEVMETHOD(device_resume,	acpi_cmbat_resume),

    {0, 0}
};

static driver_t acpi_cmbat_driver = {
    "acpi_cmbat",
    acpi_cmbat_methods,
    sizeof(struct acpi_cmbat_softc),
};

static devclass_t acpi_cmbat_devclass;
DRIVER_MODULE(acpi_cmbat, acpi, acpi_cmbat_driver, acpi_cmbat_devclass, 0, 0);
MODULE_DEPEND(acpi_cmbat, acpi, 1, 1, 1);

static int
acpi_cmbat_info_expired(struct timespec *lastupdated)
{
    struct timespec	curtime;

    ACPI_SERIAL_ASSERT(cmbat);

    if (lastupdated == NULL)
	return (TRUE);
    if (!timespecisset(lastupdated))
	return (TRUE);

    getnanotime(&curtime);
    timespecsub(&curtime, lastupdated);
    return (curtime.tv_sec < 0 ||
	    curtime.tv_sec > acpi_battery_get_info_expire());
}

static void
acpi_cmbat_info_updated(struct timespec *lastupdated)
{

    ACPI_SERIAL_ASSERT(cmbat);

    if (lastupdated != NULL)
	getnanotime(lastupdated);
}

static void
acpi_cmbat_get_bst(void *context)
{
    device_t	dev;
    struct acpi_cmbat_softc *sc;
    ACPI_STATUS	as;
    ACPI_OBJECT	*res;
    ACPI_HANDLE	h;
    ACPI_BUFFER	bst_buffer;

    ACPI_SERIAL_ASSERT(cmbat);

    dev = context;
    sc = device_get_softc(dev);
    h = acpi_get_handle(dev);
    bst_buffer.Pointer = NULL;
    bst_buffer.Length = ACPI_ALLOCATE_BUFFER;

    if (!acpi_cmbat_info_expired(&sc->bst_lastupdated))
	goto end;

    as = AcpiEvaluateObject(h, "_BST", NULL, &bst_buffer);
    if (ACPI_FAILURE(as)) {
	ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
		    "error fetching current battery status -- %s\n",
		    AcpiFormatException(as));
	goto end;
    }

    res = (ACPI_OBJECT *)bst_buffer.Pointer;
    if (!ACPI_PKG_VALID(res, 4)) {
	ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
		    "battery status corrupted\n");
	goto end;
    }

    if (acpi_PkgInt32(res, 0, &sc->bst.state) != 0)
	goto end;
    if (acpi_PkgInt32(res, 1, &sc->bst.rate) != 0)
	goto end;
    if (acpi_PkgInt32(res, 2, &sc->bst.cap) != 0)
	goto end;
    if (acpi_PkgInt32(res, 3, &sc->bst.volt) != 0)
	goto end;
    acpi_cmbat_info_updated(&sc->bst_lastupdated);

    /* XXX If all batteries are critical, perhaps we should suspend. */
    if (sc->bst.state & ACPI_BATT_STAT_CRITICAL) {
    	if ((sc->flags & ACPI_BATT_STAT_CRITICAL) == 0) {
	    sc->flags |= ACPI_BATT_STAT_CRITICAL;
	    device_printf(dev, "critically low charge!\n");
	}
    } else
	sc->flags &= ~ACPI_BATT_STAT_CRITICAL;

end:
    if (bst_buffer.Pointer != NULL)
	AcpiOsFree(bst_buffer.Pointer);
}

static void
acpi_cmbat_get_bif(void *context)
{
    device_t	dev;
    struct acpi_cmbat_softc *sc;
    ACPI_STATUS	as;
    ACPI_OBJECT	*res;
    ACPI_HANDLE	h;
    ACPI_BUFFER	bif_buffer;

    ACPI_SERIAL_ASSERT(cmbat);

    dev = context;
    sc = device_get_softc(dev);
    h = acpi_get_handle(dev);
    bif_buffer.Pointer = NULL;
    bif_buffer.Length = ACPI_ALLOCATE_BUFFER;

    if (!acpi_cmbat_info_expired(&sc->bif_lastupdated))
	goto end;

    as = AcpiEvaluateObject(h, "_BIF", NULL, &bif_buffer);
    if (ACPI_FAILURE(as)) {
	ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
		    "error fetching current battery info -- %s\n",
		    AcpiFormatException(as));
	goto end;
    }

    res = (ACPI_OBJECT *)bif_buffer.Pointer;
    if (!ACPI_PKG_VALID(res, 13)) {
	ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
		    "battery info corrupted\n");
	goto end;
    }

    if (acpi_PkgInt32(res, 0, &sc->bif.units) != 0)
	goto end;
    if (acpi_PkgInt32(res, 1, &sc->bif.dcap) != 0)
	goto end;
    if (acpi_PkgInt32(res, 2, &sc->bif.lfcap) != 0)
	goto end;
    if (acpi_PkgInt32(res, 3, &sc->bif.btech) != 0)
	goto end;
    if (acpi_PkgInt32(res, 4, &sc->bif.dvol) != 0)
	goto end;
    if (acpi_PkgInt32(res, 5, &sc->bif.wcap) != 0)
	goto end;
    if (acpi_PkgInt32(res, 6, &sc->bif.lcap) != 0)
	goto end;
    if (acpi_PkgInt32(res, 7, &sc->bif.gra1) != 0)
	goto end;
    if (acpi_PkgInt32(res, 8, &sc->bif.gra2) != 0)
	goto end;
    if (acpi_PkgStr(res,  9, sc->bif.model, ACPI_CMBAT_MAXSTRLEN) != 0)
	goto end;
    if (acpi_PkgStr(res, 10, sc->bif.serial, ACPI_CMBAT_MAXSTRLEN) != 0)
	goto end;
    if (acpi_PkgStr(res, 11, sc->bif.type, ACPI_CMBAT_MAXSTRLEN) != 0)
	goto end;
    if (acpi_PkgStr(res, 12, sc->bif.oeminfo, ACPI_CMBAT_MAXSTRLEN) != 0)
	goto end;
    acpi_cmbat_info_updated(&sc->bif_lastupdated);

end:
    if (bif_buffer.Pointer != NULL)
	AcpiOsFree(bif_buffer.Pointer);
}

static void
acpi_cmbat_notify_handler(ACPI_HANDLE h, UINT32 notify, void *context)
{
    device_t dev;
    struct acpi_cmbat_softc	*sc;

    dev = (device_t)context;
    sc = device_get_softc(dev);

    acpi_UserNotify("CMBAT", h, notify);

    /*
     * Clear the appropriate last updated time.  The next call to retrieve
     * the battery status will get the new value for us.  We don't need to
     * acquire a lock since we are only clearing the time stamp and since
     * calling _BST/_BIF can trigger a notify, we could deadlock also.
     */
    switch (notify) {
    case ACPI_NOTIFY_DEVICE_CHECK:
    case ACPI_BATTERY_BST_CHANGE:
	timespecclear(&sc->bst_lastupdated);
	break;
    case ACPI_NOTIFY_BUS_CHECK:
    case ACPI_BATTERY_BIF_CHANGE:
	timespecclear(&sc->bif_lastupdated);
	break;
    default:
	break;
    }
}

static int
acpi_cmbat_probe(device_t dev)
{
    static char *cmbat_ids[] = { "PNP0C0A", NULL };

    if (acpi_disabled("cmbat") ||
	ACPI_ID_PROBE(device_get_parent(dev), dev, cmbat_ids) == NULL)
	return (ENXIO);

    device_set_desc(dev, "Control Method Battery");
    return (0);
}

static int
acpi_cmbat_attach(device_t dev)
{
    int		error;
    ACPI_HANDLE	handle;
    struct acpi_cmbat_softc *sc;

    sc = device_get_softc(dev);
    handle = acpi_get_handle(dev);
    sc->dev = dev;

    /*
     * Install a system notify handler in addition to the device notify.
     * Toshiba notebook uses this alternate notify for its battery.
     */
    AcpiInstallNotifyHandler(handle, ACPI_ALL_NOTIFY,
			     acpi_cmbat_notify_handler, dev);

    ACPI_SERIAL_BEGIN(cmbat);
    timespecclear(&sc->bif_lastupdated);
    timespecclear(&sc->bst_lastupdated);

    if (acpi_cmbat_units == 0) {
	error = acpi_register_ioctl(ACPIIO_CMBAT_GET_BIF,
				    acpi_cmbat_ioctl, NULL);
	if (error != 0) {
	    device_printf(dev, "register bif ioctl failed\n");
	    return (error);
	}
	error = acpi_register_ioctl(ACPIIO_CMBAT_GET_BST,
				    acpi_cmbat_ioctl, NULL);
	if (error != 0) {
	    device_printf(dev, "register bst ioctl failed\n");
	    return (error);
	}
    }

    sc->phys_unit = acpi_cmbat_units;
    error = acpi_battery_register(ACPI_BATT_TYPE_CMBAT, sc->phys_unit);
    if (error != 0) {
    	device_printf(dev, "registering battery %d failed\n", sc->phys_unit);
	return (error);
    }
    acpi_cmbat_units++;
    timespecclear(&acpi_cmbat_info_lastupdated);
    ACPI_SERIAL_END(cmbat);

    AcpiOsQueueForExecution(OSD_PRIORITY_LO, acpi_cmbat_init_battery, dev);

    return (0);
}

static int
acpi_cmbat_detach(device_t dev)
{
    struct acpi_cmbat_softc *sc;

    sc = device_get_softc(dev);
    ACPI_SERIAL_BEGIN(cmbat);
    acpi_battery_remove(ACPI_BATT_TYPE_CMBAT, sc->phys_unit);
    acpi_cmbat_units--;
    ACPI_SERIAL_END(cmbat);
    return (0);
}

static int
acpi_cmbat_resume(device_t dev)
{
    AcpiOsQueueForExecution(OSD_PRIORITY_LO, acpi_cmbat_init_battery, dev);
    return (0);
}

static int
acpi_cmbat_ioctl(u_long cmd, caddr_t addr, void *arg)
{
    device_t dev;
    union acpi_battery_ioctl_arg *ioctl_arg;
    struct acpi_cmbat_softc *sc;
    struct acpi_bif	*bifp;
    struct acpi_bst	*bstp;

    ioctl_arg = (union acpi_battery_ioctl_arg *)addr;
    dev = devclass_get_device(acpi_cmbat_devclass, ioctl_arg->unit);
    if (dev == NULL)
	return (ENXIO);
    sc = device_get_softc(dev);

    /*
     * No security check required: information retrieval only.  If
     * new functions are added here, a check might be required.
     */
    ACPI_SERIAL_BEGIN(cmbat);
    switch (cmd) {
    case ACPIIO_CMBAT_GET_BIF:
	acpi_cmbat_get_bif(dev);
	bifp = &ioctl_arg->bif;
	bifp->units = sc->bif.units;
	bifp->dcap = sc->bif.dcap;
	bifp->lfcap = sc->bif.lfcap;
	bifp->btech = sc->bif.btech;
	bifp->dvol = sc->bif.dvol;
	bifp->wcap = sc->bif.wcap;
	bifp->lcap = sc->bif.lcap;
	bifp->gra1 = sc->bif.gra1;
	bifp->gra2 = sc->bif.gra2;
	strncpy(bifp->model, sc->bif.model, sizeof(sc->bif.model));
	strncpy(bifp->serial, sc->bif.serial, sizeof(sc->bif.serial));
	strncpy(bifp->type, sc->bif.type, sizeof(sc->bif.type));
	strncpy(bifp->oeminfo, sc->bif.oeminfo, sizeof(sc->bif.oeminfo));
	break;
    case ACPIIO_CMBAT_GET_BST:
	bstp = &ioctl_arg->bst;
	if (acpi_BatteryIsPresent(dev)) {
	    acpi_cmbat_get_bst(dev);
	    bstp->state = sc->bst.state;
	    bstp->rate = sc->bst.rate;
	    bstp->cap = sc->bst.cap;
	    bstp->volt = sc->bst.volt;
	} else {
	    bstp->state = ACPI_BATT_STAT_NOT_PRESENT;
	}
	break;
    default:
	break;
    }
    ACPI_SERIAL_END(cmbat);

    return (0);
}

static int
acpi_cmbat_is_bst_valid(struct acpi_bst *bst)
{
    if (bst->state >= ACPI_BATT_STAT_MAX || bst->cap == 0xffffffff ||
	bst->volt == 0xffffffff)
	return (FALSE);
    else
	return (TRUE);
}

static int
acpi_cmbat_is_bif_valid(struct acpi_bif *bif)
{
    if (bif->lfcap == 0)
	return (FALSE);
    else
	return (TRUE);
}

static int
acpi_cmbat_get_total_battinfo(struct acpi_battinfo *battinfo)
{
    int		i;
    int		error;
    int		batt_stat;
    int		valid_rate, valid_units;
    int		cap, min;
    int		total_cap, total_min, total_full;
    struct acpi_cmbat_softc *sc;

    ACPI_SERIAL_ASSERT(cmbat);

    cap = min = -1;
    batt_stat = ACPI_BATT_STAT_NOT_PRESENT;
    error = 0;

    /* Get battery status, valid rate and valid units */
    batt_stat = valid_rate = valid_units = 0;
    for (i = 0; i < acpi_cmbat_units; i++) {
	sc = devclass_get_softc(acpi_cmbat_devclass, i);
	if (sc == NULL)
	    continue;
	sc->present = acpi_BatteryIsPresent(sc->dev);
	if (!sc->present)
	    continue;
	acpi_cmbat_get_bst(sc->dev);

	/* If battery not installed, we get strange values */
	if (!acpi_cmbat_is_bst_valid(&sc->bst) ||
	    !acpi_cmbat_is_bif_valid(&sc->bif)) {
	    sc->present = FALSE;
	    continue;
	}

	valid_units++;
	sc->cap = 100 * sc->bst.cap / sc->bif.lfcap;

	/* 
	 * Some laptops report the "design-capacity" instead of the 
	 * "real-capacity" when the battery is fully charged.
	 * That breaks the above arithmetic as it needs to be 100% maximum.
	 */
	if (sc->cap > 100)
	    sc->cap = 100;

	batt_stat |= sc->bst.state;

	/*
	 * XXX Hack to calculate total battery time.
	 *
	 * On systems with more than one battery, they may get used
	 * sequentially, thus bst.rate may only signify the one in use.
	 * For the remaining batteries, bst.rate will be zero, which
	 * makes it impossible to calculate the remaining time.  Some
	 * other systems may need the sum of all the bst.rate values
	 * when discharging.  Therefore, we sum the bst.rate for valid
	 * batteries (ones in the discharging state) and use the sum
	 * to calculate the total remaining time.
	 */
	if (sc->bst.rate > 0) {
	    if (sc->bst.state & ACPI_BATT_STAT_DISCHARG)
		valid_rate += sc->bst.rate;
	}
    }

    /* Calculate total battery capacity and time */
    total_cap = total_min = total_full = 0;
    for (i = 0; i < acpi_cmbat_units; i++) {
	sc = devclass_get_softc(acpi_cmbat_devclass, i);
	if (!sc->present)
	    continue;

	/*
	 * If any batteries are discharging, use the sum of the bst.rate
	 * values.  Otherwise, use the full charge time to estimate
	 * remaining time.  If neither are available, assume no charge.
	 */
	if (valid_rate > 0)
	    sc->min = 60 * sc->bst.cap / valid_rate;
	else if (sc->full_charge_time > 0)
	    sc->min = (sc->full_charge_time * sc->cap) / 100;
	else
	    sc->min = 0;
	total_min += sc->min;
	total_cap += sc->cap;
	total_full += sc->full_charge_time;
    }

    /* Battery life */
    if (valid_units == 0) {
	cap = -1;
	batt_stat = ACPI_BATT_STAT_NOT_PRESENT;
    } else
	cap = total_cap / valid_units;

    /* Battery time */
    if (valid_units == 0)
	min = -1;
    else if (valid_rate == 0 || (batt_stat & ACPI_BATT_STAT_CHARGING)) {
	if (total_full == 0)
	    min = -1;
	else
	    min = (total_full * cap) / 100;
    } else
	min = total_min;
    acpi_cmbat_info_updated(&acpi_cmbat_info_lastupdated);

    battinfo->cap = cap;
    battinfo->min = min;
    battinfo->state = batt_stat;

    return (error);
}

static void
acpi_cmbat_init_battery(void *arg)
{
    struct acpi_cmbat_softc *sc;
    int		retry;
    device_t	dev;

    dev = (device_t)arg;
    sc = device_get_softc(dev);
    ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
		"battery initialization start\n");

    for (retry = 0; retry < ACPI_CMBAT_RETRY_MAX; retry++, AcpiOsSleep(10000)) {
	sc->present = acpi_BatteryIsPresent(dev);
	if (!sc->present)
	    continue;

	ACPI_SERIAL_BEGIN(cmbat);
	timespecclear(&sc->bst_lastupdated);
	timespecclear(&sc->bif_lastupdated);
	acpi_cmbat_get_bst(dev);
	acpi_cmbat_get_bif(dev);
	ACPI_SERIAL_END(cmbat);

	if (acpi_cmbat_is_bst_valid(&sc->bst) &&
	    acpi_cmbat_is_bif_valid(&sc->bif))
	    break;
    }

    if (retry == ACPI_CMBAT_RETRY_MAX) {
	ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
		    "battery initialization failed, giving up\n");
    } else {
	ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
		    "battery initialization done, tried %d times\n", retry + 1);
    }
}

/*
 * Public interfaces.
 */
int
acpi_cmbat_get_battinfo(int unit, struct acpi_battinfo *battinfo)
{
    int		error;
    struct acpi_cmbat_softc *sc;

    ACPI_SERIAL_BEGIN(cmbat);
    error = acpi_cmbat_get_total_battinfo(battinfo);
    if (unit == -1 || error)
	goto out;

    error = ENXIO;
    if (unit >= acpi_cmbat_units)
	goto out;
    if ((sc = devclass_get_softc(acpi_cmbat_devclass, unit)) == NULL)
	goto out;

    if (!sc->present) {
	battinfo->cap = -1;
	battinfo->min = -1;
	battinfo->state = ACPI_BATT_STAT_NOT_PRESENT;
    } else {
	battinfo->cap = sc->cap;
	battinfo->min = sc->min;
	battinfo->state = sc->bst.state;
    }
    error = 0;

out:
    ACPI_SERIAL_END(cmbat);
    return (error);
}
