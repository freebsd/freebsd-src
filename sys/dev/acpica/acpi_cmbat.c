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
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/conf.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <sys/malloc.h>

#include  "acpi.h"

#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpiio.h>

MALLOC_DEFINE(M_ACPICMBAT, "acpicmbat", "ACPI control method battery data");

#define CMBAT_POLLRATE	(60 * hz)

/*
 * Hooks for the ACPI CA debugging infrastructure
 */
#define _COMPONENT	ACPI_BATTERY
MODULE_NAME("BATTERY")

static void	 acpi_cmbat_get_bst(void *);
static void	 acpi_cmbat_get_bif(void *);
static void	 acpi_cmbat_notify_handler(ACPI_HANDLE, UINT32, void *);
static int	 acpi_cmbat_probe(device_t);
static int	 acpi_cmbat_attach(device_t);
static int	 acpi_cmbat_ioctl(u_long, caddr_t, void *);

struct acpi_cmbat_softc {
	device_t	dev;
	struct acpi_bif	bif;
	struct acpi_bst bst;
	ACPI_BUFFER	bif_buffer;
	ACPI_BUFFER	bst_buffer;
	struct timespec	bif_lastupdated;
	struct timespec	bst_lastupdated;

	int		not_present;
	int		cap;
	int		min;
	int		full_charge_time;

	struct callout_handle	cmbat_timeout;
};

static struct timespec	acpi_cmbat_info_lastupdated;

/* XXX: devclass_get_maxunit() don't give us the current allocated units... */
static int acpi_cmbat_units = 0;

#define ACPI_BATTERY_BST_CHANGE 0x80
#define ACPI_BATTERY_BIF_CHANGE 0x81

#define PKG_GETINT(res, tmp, idx, dest, label) do {			\
	tmp = &res->Package.Elements[idx];				\
	if (tmp == NULL) {						\
		device_printf(dev, "%s: PKG_GETINT idx = %d\n.",	\
		    __func__, idx);					\
		goto label;						\
	}								\
	if (tmp->Type != ACPI_TYPE_INTEGER)				\
		goto label;						\
	dest = tmp->Integer.Value;					\
} while(0)

#define PKG_GETSTR(res, tmp, idx, dest, size, label) do {              	\
	size_t	length;							\
	length = size;							\
	tmp = &res->Package.Elements[idx]; 				\
	if (tmp == NULL) {						\
		device_printf(dev, "%s: PKG_GETSTR idx = %d\n.",	\
		    __func__, idx);					\
		goto label;						\
	}								\
	bzero(dest, sizeof(dest));					\
	switch (tmp->Type) {						\
	case ACPI_TYPE_STRING:						\
		if (tmp->String.Length < length) {			\
			length = tmp->String.Length;			\
		}							\
		strncpy(dest, tmp->String.Pointer, length);		\
		break;							\
	case ACPI_TYPE_BUFFER:						\
		if (tmp->Buffer.Length < length) {			\
			length = tmp->Buffer.Length;			\
		}							\
		strncpy(dest, tmp->Buffer.Pointer, length);		\
		break;							\
	default:							\
		goto label;						\
	}								\
	dest[sizeof(dest)-1] = '\0';					\
} while(0)

#define	CMBAT_DPRINT(dev, x...) do {					\
	if (acpi_get_verbose(acpi_device_get_parent_softc(dev)))	\
		device_printf(dev, x);					\
} while (0)

/*
 * Poll the battery info.
 */
static void
acpi_cmbat_timeout(void *context)
{
	device_t	dev;
	struct acpi_cmbat_softc	*sc;

	dev = (device_t)context;
	sc = device_get_softc(dev);

	AcpiOsQueueForExecution(OSD_PRIORITY_LO, acpi_cmbat_get_bif, dev);
	sc->cmbat_timeout = timeout(acpi_cmbat_timeout, dev, CMBAT_POLLRATE);
}

static __inline int
acpi_cmbat_info_expired(struct timespec *lastupdated)
{
	struct timespec	curtime;

	if (lastupdated == NULL) {
		return (1);
	}

	if (!timespecisset(lastupdated)) {
		return (1);
	}

	getnanotime(&curtime);
	timespecsub(&curtime, lastupdated);
	return ((curtime.tv_sec < 0 || curtime.tv_sec > acpi_battery_get_info_expire()));
}


static __inline void
acpi_cmbat_info_updated(struct timespec *lastupdated)
{

	if (lastupdated != NULL) {
		getnanotime(lastupdated);
	}
}

static void
acpi_cmbat_get_bst(void *context)
{
	device_t	dev = context;
	struct acpi_cmbat_softc	*sc = device_get_softc(dev);
	ACPI_STATUS	as;
	ACPI_OBJECT	*res, *tmp;
	ACPI_HANDLE	h = acpi_get_handle(dev);

	if (!acpi_cmbat_info_expired(&sc->bst_lastupdated)) {
		return;
	}

	untimeout(acpi_cmbat_timeout, (caddr_t)dev, sc->cmbat_timeout);
retry:
	if (sc->bst_buffer.Length == 0) {
		if (sc->bst_buffer.Pointer != NULL) {
			free(sc->bst_buffer.Pointer, M_ACPICMBAT);
			sc->bst_buffer.Pointer = NULL;
		}
		as = AcpiEvaluateObject(h, "_BST", NULL, &sc->bst_buffer);
		if (as != AE_BUFFER_OVERFLOW){
			CMBAT_DPRINT(dev, "CANNOT FOUND _BST - %s\n",
			    AcpiFormatException(as));
			goto end;
		}

		sc->bst_buffer.Pointer = malloc(sc->bst_buffer.Length, M_ACPICMBAT, M_NOWAIT);
		if (sc->bst_buffer.Pointer == NULL) {
			device_printf(dev,"malloc failed");
			goto end;
		}
	}

	bzero(sc->bst_buffer.Pointer, sc->bst_buffer.Length);
	as = AcpiEvaluateObject(h, "_BST", NULL, &sc->bst_buffer);

	if (as == AE_BUFFER_OVERFLOW){
		if (sc->bst_buffer.Pointer){
			free(sc->bst_buffer.Pointer, M_ACPICMBAT);
			sc->bst_buffer.Pointer = NULL;
		}
		CMBAT_DPRINT(dev, "bst size changed to %d\n", sc->bst_buffer.Length);
		sc->bst_buffer.Length = 0;
		goto retry;
	} else if (as != AE_OK){
		CMBAT_DPRINT(dev, "CANNOT FOUND _BST - %s\n",
		    AcpiFormatException(as));
		goto end;
	}

	res = (ACPI_OBJECT *)sc->bst_buffer.Pointer;

	if ((res->Type != ACPI_TYPE_PACKAGE) || (res->Package.Count != 4)) {
		CMBAT_DPRINT(dev, "Battery status corrupted\n");
		goto end;
	}

	PKG_GETINT(res, tmp, 0, sc->bst.state, end);
	PKG_GETINT(res, tmp, 1, sc->bst.rate, end);
	PKG_GETINT(res, tmp, 2, sc->bst.cap, end);
	PKG_GETINT(res, tmp, 3, sc->bst.volt, end);
	acpi_cmbat_info_updated(&sc->bst_lastupdated);
end:
	sc->cmbat_timeout = timeout(acpi_cmbat_timeout, dev, CMBAT_POLLRATE);
}

static void
acpi_cmbat_get_bif(void *context)
{
	device_t	dev = context;
	struct acpi_cmbat_softc	*sc = device_get_softc(dev);
	ACPI_STATUS	as;
	ACPI_HANDLE	h = acpi_get_handle(dev);
	ACPI_OBJECT	*res, *tmp;

	if (!acpi_cmbat_info_expired(&sc->bif_lastupdated)) {
		return;
	}

	untimeout(acpi_cmbat_timeout, (caddr_t)dev, sc->cmbat_timeout);
retry:
	if (sc->bif_buffer.Length == 0) {
		if (sc->bif_buffer.Pointer != NULL) {
			free(sc->bif_buffer.Pointer, M_ACPICMBAT);
			sc->bif_buffer.Pointer = NULL;
		}
		as = AcpiEvaluateObject(h, "_BIF", NULL, &sc->bif_buffer);
		if (as != AE_BUFFER_OVERFLOW){
			CMBAT_DPRINT(dev, "CANNOT FOUND _BIF - %s\n",
			    AcpiFormatException(as));
			goto end;
		}

		sc->bif_buffer.Pointer = malloc(sc->bif_buffer.Length, M_ACPICMBAT, M_NOWAIT);
		if (sc->bif_buffer.Pointer == NULL) {
			device_printf(dev,"malloc failed");
			goto end;
		}
	}

	bzero(sc->bif_buffer.Pointer, sc->bif_buffer.Length);
	as = AcpiEvaluateObject(h, "_BIF", NULL, &sc->bif_buffer);

	if (as == AE_BUFFER_OVERFLOW){
		if (sc->bif_buffer.Pointer){
			free(sc->bif_buffer.Pointer, M_ACPICMBAT);
			sc->bif_buffer.Pointer = NULL;
		}
		CMBAT_DPRINT(dev, "bif size changed to %d\n", sc->bif_buffer.Length);
		sc->bif_buffer.Length = 0;
		goto retry;
	} else if (as != AE_OK){
		CMBAT_DPRINT(dev, "CANNOT FOUND _BIF - %s\n",
		    AcpiFormatException(as));
		goto end;
	}

	res = (ACPI_OBJECT *)sc->bif_buffer.Pointer;

	if ((res->Type != ACPI_TYPE_PACKAGE) || (res->Package.Count != 13)) {
		CMBAT_DPRINT(dev, "Battery info corrupted\n");
		goto end;
	}

	PKG_GETINT(res, tmp,  0, sc->bif.unit, end);
	PKG_GETINT(res, tmp,  1, sc->bif.dcap, end);
	PKG_GETINT(res, tmp,  2, sc->bif.lfcap, end);
	PKG_GETINT(res, tmp,  3, sc->bif.btech, end);
	PKG_GETINT(res, tmp,  4, sc->bif.dvol, end);
	PKG_GETINT(res, tmp,  5, sc->bif.wcap, end);
	PKG_GETINT(res, tmp,  6, sc->bif.lcap, end);
	PKG_GETINT(res, tmp,  7, sc->bif.gra1, end);
	PKG_GETINT(res, tmp,  8, sc->bif.gra2, end);
	PKG_GETSTR(res, tmp,  9, sc->bif.model, ACPI_CMBAT_MAXSTRLEN, end);
	PKG_GETSTR(res, tmp, 10, sc->bif.serial, ACPI_CMBAT_MAXSTRLEN, end);
	PKG_GETSTR(res, tmp, 11, sc->bif.type, ACPI_CMBAT_MAXSTRLEN, end);
	PKG_GETSTR(res, tmp, 12, sc->bif.oeminfo, ACPI_CMBAT_MAXSTRLEN, end);
	acpi_cmbat_info_updated(&sc->bif_lastupdated);
end:
	sc->cmbat_timeout = timeout(acpi_cmbat_timeout, dev, CMBAT_POLLRATE);
}

static void
acpi_cmbat_notify_handler(ACPI_HANDLE h, UINT32 notify, void *context)
{
	device_t	dev;
	struct acpi_cmbat_softc	*sc;

	dev = (device_t)context;
	if ((sc = device_get_softc(dev)) == NULL) {
		return;
	}

	switch (notify) {
	case ACPI_BATTERY_BST_CHANGE:
		timespecclear(&sc->bst_lastupdated);
		break;
	case ACPI_BATTERY_BIF_CHANGE:
		timespecclear(&sc->bif_lastupdated);
		AcpiOsQueueForExecution(OSD_PRIORITY_LO, acpi_cmbat_get_bif, dev);
		break;
	default:
		break;
	}
}

static int
acpi_cmbat_probe(device_t dev)
{

	if ((acpi_get_type(dev) == ACPI_TYPE_DEVICE) &&
	    acpi_MatchHid(dev, "PNP0C0A")) {
		/*
		 * Set device description 
		 */
		device_set_desc(dev, "Control method Battery");
		return(0);
	}
	return(ENXIO);
}
  
static int
acpi_cmbat_attach(device_t dev)
{
	int	 error;
	ACPI_HANDLE handle = acpi_get_handle(dev);
	struct acpi_cmbat_softc *sc;

	if ((sc = device_get_softc(dev)) == NULL) {
		return (ENXIO);
	}
	AcpiInstallNotifyHandler(handle, ACPI_DEVICE_NOTIFY,
				 acpi_cmbat_notify_handler, dev);
	bzero(&sc->bif_buffer, sizeof(sc->bif_buffer));
	bzero(&sc->bst_buffer, sizeof(sc->bst_buffer));
	sc->dev = dev;

	timespecclear(&sc->bif_lastupdated);
	timespecclear(&sc->bst_lastupdated);

	if (acpi_cmbat_units == 0) {
		if ((error = acpi_register_ioctl(ACPIIO_CMBAT_GET_BIF,
				acpi_cmbat_ioctl, NULL)) != 0) {
			return (error);
		}
		if ((error = acpi_register_ioctl(ACPIIO_CMBAT_GET_BST,
				acpi_cmbat_ioctl, NULL)) != 0) {
			return (error);
		}
	}

	if ((error = acpi_battery_register(ACPI_BATT_TYPE_CMBAT,
			acpi_cmbat_units)) != 0) {
		return (error);
	}

	acpi_cmbat_units++;
	timespecclear(&acpi_cmbat_info_lastupdated);

	sc->cmbat_timeout = timeout(acpi_cmbat_timeout, dev, CMBAT_POLLRATE);
	return(0);
}

static int
acpi_cmbat_resume(device_t dev)
{

	AcpiOsQueueForExecution(OSD_PRIORITY_LO, acpi_cmbat_get_bif, dev);
	return (0);
}
  
static device_method_t acpi_cmbat_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		acpi_cmbat_probe),
	DEVMETHOD(device_attach,	acpi_cmbat_attach),
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

static int
acpi_cmbat_ioctl(u_long cmd, caddr_t addr, void *arg)
{
	device_t dev;
	union acpi_battery_ioctl_arg	*ioctl_arg;
	struct acpi_cmbat_softc	*sc;
	struct acpi_bif	*bifp;
	struct acpi_bst	*bstp;

	ioctl_arg = (union acpi_battery_ioctl_arg *)addr;
	if ((dev = devclass_get_device(acpi_cmbat_devclass,
			ioctl_arg->unit)) == NULL) {
		return(ENXIO);
	}

	if ((sc = device_get_softc(dev)) == NULL) {
		return(ENXIO);
	}

	switch (cmd) {
	case ACPIIO_CMBAT_GET_BIF:
		acpi_cmbat_get_bif(dev);
		bifp = &ioctl_arg->bif;
		bifp->unit = sc->bif.unit;
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
		acpi_cmbat_get_bst(dev);
		bstp = &ioctl_arg->bst;
		bstp->state = sc->bst.state;
		bstp->rate = sc->bst.rate;
		bstp->cap = sc->bst.cap;
		bstp->volt = sc->bst.volt;
		break;
	}

	return(0);
}

static int
acpi_cmbat_get_total_battinfo(struct acpi_battinfo *battinfo)
{
	int	 i;
	int	 error;
	int	 batt_stat;
	int	 valid_rate, valid_units;
	int	 cap, min;
	int	 total_cap, total_min, total_full;
	device_t dev;
	struct acpi_cmbat_softc	*sc;
	static int	 bat_units = 0;
	static struct acpi_cmbat_softc	**bat = NULL;

	cap = min = -1;
	batt_stat = ACPI_BATT_STAT_NOT_PRESENT;
	error = 0;

	/* Allocate array of softc pointers */
	if (bat_units != acpi_cmbat_units) {
		if (bat != NULL) {
			free(bat, M_ACPICMBAT);
			bat = NULL;
		}
		bat_units = 0;
	}
	if (bat == NULL) {
		bat_units = acpi_cmbat_units;
		bat = malloc(sizeof(struct acpi_cmbat_softc *) * bat_units,
			     M_ACPICMBAT, M_NOWAIT);
		if (bat == NULL) {
			error = ENOMEM;
			goto out;
		}

		/* Collect softc pointers */
		for (i = 0; i < acpi_cmbat_units; i++) { 
			if ((dev = devclass_get_device(acpi_cmbat_devclass, i)) == NULL) {
				error = ENXIO;
				goto out;
			}

			if ((sc = device_get_softc(dev)) == NULL) {
				error = ENXIO;
				goto out;
			}

			bat[i] = sc;
		}
	}

	/* Get battery status, valid rate and valid units */
	batt_stat = valid_rate = valid_units = 0;
	for (i = 0; i < acpi_cmbat_units; i++) {
		acpi_cmbat_get_bst(bat[i]->dev);

		/* If battey not installed, we get strange values */
		if (bat[i]->bst.state >= ACPI_BATT_STAT_MAX ||
		    bat[i]->bst.cap == 0xffffffff ||
		    bat[i]->bst.volt == 0xffffffff ||
		    bat[i]->bif.lfcap == 0) {
			bat[i]->not_present = 1;
			continue;
		} else {
			bat[i]->not_present = 0;
		}

		valid_units++;

		bat[i]->cap = 100 * bat[i]->bst.cap / bat[i]->bif.lfcap;

		batt_stat |= bat[i]->bst.state;

		if (bat[i]->bst.rate > 0) {
			/*
			 * XXX Hack to calculate total battery time.
			 * Systems with 2 or more battries, they may get used
			 * one by one, thus bst.rate is set only to the one
			 * in use. For remaining batteries bst.rate = 0, which
			 * makes it impossible to calculate remaining time.
			 * Some other systems may need sum of bst.rate in 
			 * dis-charging state.
			 * There for we sum up the bst.rate that is valid
			 * (in dis-charging state), and use the sum to
			 * calcutate remaining batteries' time.
			 */
			if (bat[i]->bst.state & ACPI_BATT_STAT_DISCHARG) {
				valid_rate += bat[i]->bst.rate;
			}
		}
	}

	/* Calculate total battery capacity and time */
	total_cap = total_min = total_full = 0;
	for (i = 0; i < acpi_cmbat_units; i++) {
		if (bat[i]->not_present) {
			continue;
		}

		if (valid_rate > 0) {
			/* Use the sum of bst.rate */
			bat[i]->min = 60 * bat[i]->bst.cap / valid_rate;
		} else if (bat[i]->full_charge_time > 0) {
			bat[i]->min = (bat[i]->full_charge_time * bat[i]->cap) / 100;
		} else {
			/* Couldn't find valid rate and full battery time */
			bat[i]->min = 0;
		}
		total_min += bat[i]->min;
		total_cap += bat[i]->cap;
		total_full += bat[i]->full_charge_time;
	}

	/* Battery life */
	if (valid_units == 0) {
		cap = -1;
		batt_stat = ACPI_BATT_STAT_NOT_PRESENT;
	} else {
		cap = total_cap / valid_units;
	}

	/* Battery time */
	if (valid_units == 0) {
		min = -1;
	} else if (valid_rate == 0 || (batt_stat & ACPI_BATT_STAT_CHARGING)) {
		if (total_full == 0) {
			min = -1;
		} else {
			min = (total_full * cap) / 100;
		}
	} else {
		min = total_min;
	}

	acpi_cmbat_info_updated(&acpi_cmbat_info_lastupdated);
out:
	battinfo->cap = cap;
	battinfo->min = min;
	battinfo->state = batt_stat;

	return (error);
}

/*
 * Public interfaces.
 */

int
acpi_cmbat_get_battinfo(int unit, struct acpi_battinfo *battinfo)
{
	int	 error;
	device_t dev;
	struct acpi_cmbat_softc	*sc;

	if (unit == -1) {
		return (acpi_cmbat_get_total_battinfo(battinfo));
	}

	if (acpi_cmbat_info_expired(&acpi_cmbat_info_lastupdated)) {
		error = acpi_cmbat_get_total_battinfo(battinfo);
		if (error) {
			goto out;
		}
	}

	error = 0;
	if (unit >= acpi_cmbat_units) {
		error = ENXIO;
		goto out;
	}

	if ((dev = devclass_get_device(acpi_cmbat_devclass, unit)) == NULL) {
		error = ENXIO;
		goto out;
	}

	if ((sc = device_get_softc(dev)) == NULL) {
		error = ENXIO;
		goto out;
	}

	if (sc->not_present) {
		battinfo->cap = -1;
		battinfo->min = -1;
		battinfo->state = ACPI_BATT_STAT_NOT_PRESENT;
	} else {
		battinfo->cap = sc->cap;
		battinfo->min = sc->min;
		battinfo->state = sc->bst.state;
	}
out:
	return (error);
}

