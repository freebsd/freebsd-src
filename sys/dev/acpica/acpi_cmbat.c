/*-
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
};

/* XXX: devclass_get_maxunit() don't give us the current allocated units... */
static int acpi_cmbat_units = 0;

#define ACPI_BATTERY_BST_CHANGE 0x80
#define ACPI_BATTERY_BIF_CHANGE 0x81

#define PKG_GETINT(res, tmp, idx, dest, label) do {			\
	tmp = &res->Package.Elements[idx];				\
	if (tmp->Type != ACPI_TYPE_INTEGER)				\
		goto label ;						\
	dest = tmp->Integer.Value;					\
} while(0)

#define PKG_GETSTR(res, tmp, idx, dest, size, label) do {              	\
	size_t	length;							\
	length = size;							\
	tmp = &res->Package.Elements[idx]; 				\
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
			length = tmp->String.Length;			\
		}							\
		strncpy(dest, tmp->Buffer.Pointer, length);		\
		break;							\
	default:							\
		goto label;						\
	}								\
	dest[sizeof(dest)-1] = '\0';					\
} while(0)

static void
acpi_cmbat_get_bst(void *context)
{
	device_t	dev = context;
	struct acpi_cmbat_softc	*sc = device_get_softc(dev);
	ACPI_STATUS	as;
	ACPI_OBJECT	*res, *tmp;
	ACPI_HANDLE	h = acpi_get_handle(dev);
	
retry:
	if (sc->bst_buffer.Length == 0) {
		sc->bst_buffer.Pointer = NULL;
		as = AcpiEvaluateObject(h, "_BST", NULL, &sc->bst_buffer);
		if (as != AE_BUFFER_OVERFLOW){
			device_printf(dev, "CANNOT FOUND _BST (%d)\n", as);
			return;
		}

		sc->bst_buffer.Pointer = malloc(sc->bst_buffer.Length, M_DEVBUF, M_NOWAIT);
		if (sc->bst_buffer.Pointer == NULL) {
			device_printf(dev,"malloc failed");
			return;
		}
	}

	as = AcpiEvaluateObject(h, "_BST", NULL, &sc->bst_buffer);

	if (as == AE_BUFFER_OVERFLOW){
		if (sc->bst_buffer.Pointer){
			free(sc->bst_buffer.Pointer, M_DEVBUF);
		}
		device_printf(dev, "bst size changed to %d\n", sc->bst_buffer.Length);
		sc->bst_buffer.Length = 0;
		goto retry;
	} else if (as != AE_OK){
		device_printf(dev, "CANNOT FOUND _BST (%d)\n", as);
		return;
	}

	res = sc->bst_buffer.Pointer;

	if ((res->Type != ACPI_TYPE_PACKAGE) && (res->Package.Count < 4))
		return ;

	PKG_GETINT(res, tmp, 0, sc->bst.state, end);
	PKG_GETINT(res, tmp, 1, sc->bst.rate, end);
	PKG_GETINT(res, tmp, 2, sc->bst.cap, end);
	PKG_GETINT(res, tmp, 3, sc->bst.volt, end);
end:
}

static void
acpi_cmbat_get_bif(void *context)
{
	device_t	dev = context;
	struct acpi_cmbat_softc	*sc = device_get_softc(dev);
	ACPI_STATUS	as;
	ACPI_HANDLE	h = acpi_get_handle(dev);
	ACPI_OBJECT	*res, *tmp;

retry:
	if (sc->bif_buffer.Length == 0) {
		sc->bif_buffer.Pointer = NULL;
		as = AcpiEvaluateObject(h, "_BIF", NULL, &sc->bif_buffer);
		if (as != AE_BUFFER_OVERFLOW){
			device_printf(dev, "CANNOT FOUND _BIF (%d)\n", as);
			return;
		}

		sc->bif_buffer.Pointer = malloc(sc->bif_buffer.Length, M_DEVBUF, M_NOWAIT);
		if (sc->bif_buffer.Pointer == NULL) {
			device_printf(dev,"malloc failed");
			return;
		}
	}

	as = AcpiEvaluateObject(h, "_BIF", NULL, &sc->bif_buffer);

	if (as == AE_BUFFER_OVERFLOW){
		if (sc->bif_buffer.Pointer){
			free(sc->bif_buffer.Pointer, M_DEVBUF);
		}
		device_printf(dev, "bif size changed to %d\n", sc->bif_buffer.Length);
		sc->bif_buffer.Length = 0;
		goto retry;
	} else if (as != AE_OK){
		device_printf(dev, "CANNOT FOUND _BIF (%d)\n", as);
		return;
	}

	res = sc->bif_buffer.Pointer;
	if ((res->Type != ACPI_TYPE_PACKAGE) && (res->Package.Count < 13))
		return ;

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
end:
}

static void
acpi_cmbat_notify_handler(ACPI_HANDLE h, UINT32 notify, void *context)
{
	switch (notify) {
#if 0
	/* XXX
	 * AML method execution is somewhat heavy even using swi.
	 * better to disable them until we fix the problem.
	 */
	case ACPI_BATTERY_BST_CHANGE:
		AcpiOsQueueForExecution(OSD_PRIORITY_LO,
		    acpi_cmbat_get_bst, context);
		break;
	case ACPI_BATTERY_BIF_CHANGE:
		AcpiOsQueueForExecution(OSD_PRIORITY_LO,
		    acpi_cmbat_get_bif, context);
		break;
#endif
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
	sc = device_get_softc(dev);
	AcpiInstallNotifyHandler(handle, ACPI_DEVICE_NOTIFY,
				 acpi_cmbat_notify_handler,dev);
	bzero(&sc->bif_buffer, sizeof(sc->bif_buffer));
	bzero(&sc->bst_buffer, sizeof(sc->bst_buffer));
	
	acpi_cmbat_get_bif(dev);
	acpi_cmbat_get_bst(dev);

	if (acpi_cmbat_units == 0) {
		error = acpi_register_ioctl(ACPIIO_CMBAT_GET_UNITS,
		    acpi_cmbat_ioctl, sc);
		if (error)
			return (error);
		error = acpi_register_ioctl(ACPIIO_CMBAT_GET_BIF,
		    acpi_cmbat_ioctl, sc);
		if (error)
			return (error);
		error = acpi_register_ioctl(ACPIIO_CMBAT_GET_BST,
		    acpi_cmbat_ioctl, sc);
		if (error)
			return (error);
	}

	acpi_cmbat_units++;

	return(0);
}

static device_method_t acpi_cmbat_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		acpi_cmbat_probe),
	DEVMETHOD(device_attach,	acpi_cmbat_attach),

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
	union	 acpi_cmbat_ioctl_arg *ioctl_arg;
	device_t dev;
	struct	 acpi_cmbat_softc *sc;
	struct	 acpi_bif *bifp;
	struct	 acpi_bst *bstp;

	if (cmd == ACPIIO_CMBAT_GET_UNITS) {
		ioctl_arg = NULL;
		dev = NULL;
		sc = NULL;
	} else {
		ioctl_arg = (union acpi_cmbat_ioctl_arg *)addr;
		dev = devclass_get_device(acpi_cmbat_devclass, ioctl_arg->unit);
		if (dev == NULL) {
			return(ENXIO);
		}
		
		sc = device_get_softc(dev);
		if (sc == NULL) {
			return(ENXIO);
		}
	}

	switch (cmd) {
	case ACPIIO_CMBAT_GET_UNITS:
		*(int *)addr = acpi_cmbat_units;
		break;

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
