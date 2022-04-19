/*-
 * Copyright (c) 2005 Nate Lawson
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/ioccom.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/malloc.h>

#include <contrib/dev/acpica/include/acpi.h>

#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpiio.h>

static MALLOC_DEFINE(M_ACPICMBAT, "acpicmbat",
    "ACPI control method battery data");

/* Number of times to retry initialization before giving up. */
#define ACPI_CMBAT_RETRY_MAX	6

/* Check the battery once a minute. */
#define	CMBAT_POLLRATE		(60 * hz)

/* Hooks for the ACPI CA debugging infrastructure */
#define	_COMPONENT	ACPI_BATTERY
ACPI_MODULE_NAME("BATTERY")

#define	ACPI_BATTERY_BST_CHANGE	0x80
#define	ACPI_BATTERY_BIF_CHANGE	0x81
#define	ACPI_BATTERY_BIX_CHANGE	ACPI_BATTERY_BIF_CHANGE

struct acpi_cmbat_softc {
    device_t	    dev;
    int		    flags;

    struct acpi_bix bix;
    struct acpi_bst bst;
    struct timespec bst_lastupdated;
};

ACPI_SERIAL_DECL(cmbat, "ACPI cmbat");

static int		acpi_cmbat_probe(device_t dev);
static int		acpi_cmbat_attach(device_t dev);
static int		acpi_cmbat_detach(device_t dev);
static int		acpi_cmbat_resume(device_t dev);
static void		acpi_cmbat_notify_handler(ACPI_HANDLE h, UINT32 notify,
			    void *context);
static int		acpi_cmbat_info_expired(struct timespec *lastupdated);
static void		acpi_cmbat_info_updated(struct timespec *lastupdated);
static void		acpi_cmbat_get_bst(void *arg);
static void		acpi_cmbat_get_bix_task(void *arg);
static void		acpi_cmbat_get_bix(void *arg);
static int		acpi_cmbat_bst(device_t, struct acpi_bst *);
static int		acpi_cmbat_bix(device_t, void *, size_t);
static void		acpi_cmbat_init_battery(void *arg);

static device_method_t acpi_cmbat_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	acpi_cmbat_probe),
    DEVMETHOD(device_attach,	acpi_cmbat_attach),
    DEVMETHOD(device_detach,	acpi_cmbat_detach),
    DEVMETHOD(device_resume,	acpi_cmbat_resume),

    /* ACPI battery interface */
    DEVMETHOD(acpi_batt_get_info, acpi_cmbat_bix),
    DEVMETHOD(acpi_batt_get_status, acpi_cmbat_bst),

    DEVMETHOD_END
};

static driver_t acpi_cmbat_driver = {
    "battery",
    acpi_cmbat_methods,
    sizeof(struct acpi_cmbat_softc),
};

DRIVER_MODULE(acpi_cmbat, acpi, acpi_cmbat_driver, 0, 0);
MODULE_DEPEND(acpi_cmbat, acpi, 1, 1, 1);

static int
acpi_cmbat_probe(device_t dev)
{
    static char *cmbat_ids[] = { "PNP0C0A", NULL };
    int rv;
    
    if (acpi_disabled("cmbat"))
	return (ENXIO);
    rv = ACPI_ID_PROBE(device_get_parent(dev), dev, cmbat_ids, NULL);
    if (rv <= 0)
	device_set_desc(dev, "ACPI Control Method Battery");
    return (rv);
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

    timespecclear(&sc->bst_lastupdated);

    error = acpi_battery_register(dev);
    if (error != 0) {
    	device_printf(dev, "registering battery failed\n");
	return (error);
    }

    /*
     * Install a system notify handler in addition to the device notify.
     * Toshiba notebook uses this alternate notify for its battery.
     */
    AcpiInstallNotifyHandler(handle, ACPI_ALL_NOTIFY,
	acpi_cmbat_notify_handler, dev);

    AcpiOsExecute(OSL_NOTIFY_HANDLER, acpi_cmbat_init_battery, dev);

    return (0);
}

static int
acpi_cmbat_detach(device_t dev)
{
    ACPI_HANDLE	handle;

    handle = acpi_get_handle(dev);
    AcpiRemoveNotifyHandler(handle, ACPI_ALL_NOTIFY, acpi_cmbat_notify_handler);
    acpi_battery_remove(dev);

    /*
     * Force any pending notification handler calls to complete by
     * requesting cmbat serialisation while freeing and clearing the
     * softc pointer:
     */
    ACPI_SERIAL_BEGIN(cmbat);
    device_set_softc(dev, NULL);
    ACPI_SERIAL_END(cmbat);

    return (0);
}

static int
acpi_cmbat_resume(device_t dev)
{

    AcpiOsExecute(OSL_NOTIFY_HANDLER, acpi_cmbat_init_battery, dev);
    return (0);
}

static void
acpi_cmbat_notify_handler(ACPI_HANDLE h, UINT32 notify, void *context)
{
    struct acpi_cmbat_softc *sc;
    device_t dev;

    dev = (device_t)context;
    sc = device_get_softc(dev);

    switch (notify) {
    case ACPI_NOTIFY_DEVICE_CHECK:
    case ACPI_BATTERY_BST_CHANGE:
	/*
	 * Clear the last updated time.  The next call to retrieve the
	 * battery status will get the new value for us.
	 */
	timespecclear(&sc->bst_lastupdated);
	break;
    case ACPI_NOTIFY_BUS_CHECK:
    case ACPI_BATTERY_BIX_CHANGE:
	/*
	 * Queue a callback to get the current battery info from thread
	 * context.  It's not safe to block in a notify handler.
	 */
	AcpiOsExecute(OSL_NOTIFY_HANDLER, acpi_cmbat_get_bix_task, dev);
	break;
    }

    acpi_UserNotify("CMBAT", h, notify);
}

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
    timespecsub(&curtime, lastupdated, &curtime);
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
acpi_cmbat_get_bst(void *arg)
{
    struct acpi_cmbat_softc *sc;
    ACPI_STATUS	as;
    ACPI_OBJECT	*res;
    ACPI_HANDLE	h;
    ACPI_BUFFER	bst_buffer;
    device_t dev;

    ACPI_SERIAL_ASSERT(cmbat);

    dev = arg;
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

    /* Clear out undefined/extended bits that might be set by hardware. */
    sc->bst.state &= ACPI_BATT_STAT_BST_MASK;
    if ((sc->bst.state & ACPI_BATT_STAT_INVALID) == ACPI_BATT_STAT_INVALID)
	ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
	    "battery reports simultaneous charging and discharging\n");

    /* XXX If all batteries are critical, perhaps we should suspend. */
    if (sc->bst.state & ACPI_BATT_STAT_CRITICAL) {
    	if ((sc->flags & ACPI_BATT_STAT_CRITICAL) == 0) {
	    sc->flags |= ACPI_BATT_STAT_CRITICAL;
	    device_printf(dev, "critically low charge!\n");
	}
    } else
	sc->flags &= ~ACPI_BATT_STAT_CRITICAL;

end:
    AcpiOsFree(bst_buffer.Pointer);
}

/* XXX There should be a cleaner way to do this locking. */
static void
acpi_cmbat_get_bix_task(void *arg)
{

    ACPI_SERIAL_BEGIN(cmbat);
    acpi_cmbat_get_bix(arg);
    ACPI_SERIAL_END(cmbat);
}

static void
acpi_cmbat_get_bix(void *arg)
{
    struct acpi_cmbat_softc *sc;
    ACPI_STATUS	as;
    ACPI_OBJECT	*res;
    ACPI_HANDLE	h;
    ACPI_BUFFER	bix_buffer;
    device_t dev;
    int i, n;
    const struct {
	    enum { _BIX, _BIF } type;
	    char *name;
    } bobjs[] = {
	    { _BIX, "_BIX"},
	    { _BIF, "_BIF"},
    };

    ACPI_SERIAL_ASSERT(cmbat);

    dev = arg;
    sc = device_get_softc(dev);
    h = acpi_get_handle(dev);
    bix_buffer.Pointer = NULL;
    bix_buffer.Length = ACPI_ALLOCATE_BUFFER;

    for (n = 0; n < sizeof(bobjs); n++) {
	as = AcpiEvaluateObject(h, bobjs[n].name, NULL, &bix_buffer);
	if (!ACPI_FAILURE(as)) {
	    res = (ACPI_OBJECT *)bix_buffer.Pointer;
	    break;
	}
	AcpiOsFree(bix_buffer.Pointer);
        bix_buffer.Pointer = NULL;
        bix_buffer.Length = ACPI_ALLOCATE_BUFFER;
    }
    /* Both _BIF and _BIX were not found. */
    if (n == sizeof(bobjs)) {
	ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
	    "error fetching current battery info -- %s\n",
	    AcpiFormatException(as));
	goto end;
    }

    /*
     * ACPI _BIX and _BIF revision mismatch check:
     *
     * 1. _BIF has no revision field.  The number of fields must be 13.
     *
     * 2. _BIX has a revision field.  As of ACPI 6.3 it must be "0" or
     *    "1".  The number of fields will be checked---20 and 21,
     *    respectively.
     *
     *    If the revision number is grater than "1" and the number of
     *    fields is grater than 21, it will be treated as compatible with
     *    ACPI 6.0 _BIX.  If not, it will be ignored.
     */
    i = 0;
    switch (bobjs[n].type) {
    case _BIX:
	if (acpi_PkgInt16(res, i++, &sc->bix.rev) != 0) {
	    ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
		"_BIX revision error\n");
	    goto end;
	}
#define	ACPI_BIX_REV_MISMATCH_ERR(x, r) do {			\
	ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),	\
	    "_BIX revision mismatch (%u != %u)\n", x, r);	\
	goto end;						\
	} while (0)

	if (ACPI_PKG_VALID_EQ(res, 21)) {	/* ACPI 6.0 _BIX */
	    /*
	     * Some models have rev.0 _BIX with 21 members.
	     * In that case, treat the first 20 members as rev.0 _BIX.
	     */
	    if (sc->bix.rev != ACPI_BIX_REV_0 &&
	        sc->bix.rev != ACPI_BIX_REV_1)
		ACPI_BIX_REV_MISMATCH_ERR(sc->bix.rev, ACPI_BIX_REV_1);
	} else if (ACPI_PKG_VALID_EQ(res, 20)) {/* ACPI 4.0 _BIX */
	    if (sc->bix.rev != ACPI_BIX_REV_0)
		ACPI_BIX_REV_MISMATCH_ERR(sc->bix.rev, ACPI_BIX_REV_0);
	} else if (ACPI_PKG_VALID(res, 22)) {
	    /* _BIX with 22 or more members. */
	    if (ACPI_BIX_REV_MIN_CHECK(sc->bix.rev, ACPI_BIX_REV_1 + 1)) {
		/*
		 * Unknown revision number.
		 * Assume 21 members are compatible with 6.0 _BIX.
		 */
		ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
		    "Unknown _BIX revision(%u). "
		    "Assuming compatible with revision %u.\n",
		    sc->bix.rev, ACPI_BIX_REV_1);
	    } else {
		/*
		 * Known revision number.  Ignore the extra members.
		 */
		ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
		    "Extra objects found in _BIX were ignored.\n");
	    }
	} else {
		/* Invalid _BIX.  Ignore it. */
		ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
		    "Invalid _BIX found (rev=%u, count=%u).  Ignored.\n",
		    sc->bix.rev, res->Package.Count);
		goto end;
	}
	break;
#undef	ACPI_BIX_REV_MISMATCH_ERR
    case _BIF:
	if (ACPI_PKG_VALID_EQ(res, 13))	/* _BIF */
	    sc->bix.rev = ACPI_BIX_REV_BIF;
	else {
		ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
		    "Invalid _BIF found (count=%u).  Ignored.\n",
		    res->Package.Count);
		goto end;
	}
	break;
    }

    ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
	"rev = %04x\n", sc->bix.rev);
#define	BIX_GETU32(NAME)	do {			\
    ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),	\
	#NAME " = %u\n", sc->bix.NAME);			\
    if (acpi_PkgInt32(res, i++, &sc->bix.NAME) != 0)	\
	    goto end;					\
    } while (0)

    BIX_GETU32(units);
    BIX_GETU32(dcap);
    BIX_GETU32(lfcap);
    BIX_GETU32(btech);
    BIX_GETU32(dvol);
    BIX_GETU32(wcap);
    BIX_GETU32(lcap);
    if (ACPI_BIX_REV_MIN_CHECK(sc->bix.rev, ACPI_BIX_REV_0)) {
	    BIX_GETU32(cycles);
	    BIX_GETU32(accuracy);
	    BIX_GETU32(stmax);
	    BIX_GETU32(stmin);
	    BIX_GETU32(aimax);
	    BIX_GETU32(aimin);
    }
    BIX_GETU32(gra1);
    BIX_GETU32(gra2);
    if (acpi_PkgStr(res, i++, sc->bix.model, ACPI_CMBAT_MAXSTRLEN) != 0)
	    goto end;
    if (acpi_PkgStr(res, i++, sc->bix.serial, ACPI_CMBAT_MAXSTRLEN) != 0)
	    goto end;
    if (acpi_PkgStr(res, i++, sc->bix.type, ACPI_CMBAT_MAXSTRLEN) != 0)
	    goto end;
    if (acpi_PkgStr(res, i++, sc->bix.oeminfo, ACPI_CMBAT_MAXSTRLEN) != 0)
	    goto end;
    if (ACPI_BIX_REV_MIN_CHECK(sc->bix.rev, ACPI_BIX_REV_1))
	    BIX_GETU32(scap);
#undef	BIX_GETU32
end:
    AcpiOsFree(bix_buffer.Pointer);
}

static int
acpi_cmbat_bix(device_t dev, void *bix, size_t len)
{
    struct acpi_cmbat_softc *sc;

    if (len != sizeof(struct acpi_bix) &&
	len != sizeof(struct acpi_bif))
	    return (-1);

    sc = device_get_softc(dev);

    /*
     * Just copy the data.  The only value that should change is the
     * last-full capacity, so we only update when we get a notify that says
     * the info has changed.  Many systems apparently take a long time to
     * process a _BI[FX] call so we avoid it if possible.
     */
    ACPI_SERIAL_BEGIN(cmbat);
    memcpy(bix, &sc->bix, len);
    ACPI_SERIAL_END(cmbat);

    return (0);
}

static int
acpi_cmbat_bst(device_t dev, struct acpi_bst *bst)
{
    struct acpi_cmbat_softc *sc;

    sc = device_get_softc(dev);

    ACPI_SERIAL_BEGIN(cmbat);
    if (acpi_BatteryIsPresent(dev)) {
	acpi_cmbat_get_bst(dev);
	memcpy(bst, &sc->bst, sizeof(*bst));
    } else
	bst->state = ACPI_BATT_STAT_NOT_PRESENT;
    ACPI_SERIAL_END(cmbat);

    return (0);
}

static void
acpi_cmbat_init_battery(void *arg)
{
    struct acpi_cmbat_softc *sc;
    int		retry, valid;
    device_t	dev;

    dev = (device_t)arg;
    ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
	"battery enitialization start\n");

    /*
     * Try repeatedly to get valid data from the battery.  Since the
     * embedded controller isn't always ready just after boot, we may have
     * to wait a while.
     */
    for (retry = 0; retry < ACPI_CMBAT_RETRY_MAX; retry++, AcpiOsSleep(10000)) {
	/*
	 * Batteries on DOCK can be ejected w/ DOCK during retrying.
	 *
	 * If there is a valid softc pointer the device may be in
	 * attaching, attached or detaching state. If the state is
	 * different from attached retry getting the device state
	 * until it becomes stable. This solves a race if the ACPI
	 * notification handler is called during attach, because
	 * device_is_attached() doesn't return non-zero until after
	 * the attach code has been executed.
	 */
	ACPI_SERIAL_BEGIN(cmbat);
	sc = device_get_softc(dev);
	if (sc == NULL) {
	    ACPI_SERIAL_END(cmbat);
	    return;
	}

	if (!acpi_BatteryIsPresent(dev) || !device_is_attached(dev)) {
	    ACPI_SERIAL_END(cmbat);
	    continue;
	}

	/*
	 * Only query the battery if this is the first try or the specific
	 * type of info is still invalid.
	 */
	if (retry == 0 || !acpi_battery_bst_valid(&sc->bst)) {
	    timespecclear(&sc->bst_lastupdated);
	    acpi_cmbat_get_bst(dev);
	}
	if (retry == 0 || !acpi_battery_bix_valid(&sc->bix))
	    acpi_cmbat_get_bix(dev);

	valid = acpi_battery_bst_valid(&sc->bst) &&
	    acpi_battery_bix_valid(&sc->bix);
	ACPI_SERIAL_END(cmbat);

	if (valid)
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
