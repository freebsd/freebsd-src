/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 KERN_SUCCESS
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/sysctl.h>
#include <machine/cpufunc.h>
#include <machine/specialreg.h>

#define THERMAL_MSR_THERM_STATUS	0x19C
#define THERMAL_MAX_TEMP		100

struct thermal_softc {
	device_t		dev;
	struct cdev		*cdev;
	struct mtx		mtx;
	int			current_temp;
	int			critical_temp;
	int			throttle_count;
	struct sysctl_ctx_list	sysctl_ctx;
	struct sysctl_oid	*sysctl_tree;
};

static struct cdevsw thermal_cdevsw = {
	.d_version = D_VERSION,
	.d_open =	thermal_open,
	.d_close =	thermal_close,
	.d_read =	thermal_read,
	.d_name =	"thermal",
};

static int
thermal_get_temperature(struct thermal_softc *sc, int core)
{
	uint64_t val;

	val = rdmsr(THERMAL_MSR_THERM_STATUS + core);
	if (!(val & 0x80000000))
		return (-1);
	return (THERMAL_MAX_TEMP - ((val >> 16) & 0x7F));
}

static int
thermal_temp_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct thermal_softc *sc = arg1;
	int core = arg2;
	int temp;

	mtx_lock(&sc->mtx);
	temp = thermal_get_temperature(sc, core);
	if (temp >= 0)
		sc->current_temp = temp;
	mtx_unlock(&sc->mtx);
	return (sysctl_handle_int(oidp, &sc->current_temp, 0, req));
}

static int
thermal_open(struct cdev *dev __unused, int oflags __unused,
    int devtype __unused, struct thread *td __unused)
{
	return (0);
}

static int
thermal_close(struct cdev *dev __unused, int fflag __unused,
    int devtype __unused, struct thread *td __unused)
{
	return (0);
}

static int
thermal_read(struct cdev *dev, struct uio *uio, int ioflag __unused)
{
	struct thermal_softc *sc = dev->si_drv1;
	char buf[256];
	int len, temp, i;

	mtx_lock(&sc->mtx);
	len = 0;
	for (i = 0; i < 8; i++) {  // Assuming up to 8 cores
		temp = thermal_get_temperature(sc, i);
		if (temp >= 0)
			len += snprintf(buf + len, sizeof(buf) - len, "Core %d: %d\n", i, temp);
	}
	mtx_unlock(&sc->mtx);
	return (uiomove(buf, min(len, uio->uio_resid), uio));
}

static int
thermal_probe(device_t dev)
{
	/* Only Intel CPUs supported for now. */
	if (cpu_vendor != CPU_VENDOR_INTEL)
		return (ENXIO);
	device_set_desc(dev, "Intel CPU Thermal Monitor");
	return (BUS_PROBE_DEFAULT);
}

static int
thermal_attach(device_t dev)
{
	struct thermal_softc *sc = device_get_softc(dev);

	sc->dev = dev;
	sc->critical_temp = 85;
	sc->throttle_count = 0;
	mtx_init(&sc->mtx, "thermal", NULL, MTX_DEF);

	sysctl_ctx_init(&sc->sysctl_ctx);
	sc->sysctl_tree = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
	    SYSCTL_STATIC_CHILDREN(_hw), OID_AUTO, "thermal",
	    CTLFLAG_RD, 0, "Thermal monitoring");

	for (int i = 0; i < 8; i++) {  // Assuming up to 8 cores
		SYSCTL_ADD_PROC(&sc->sysctl_ctx,
		    SYSCTL_CHILDREN(sc->sysctl_tree), OID_AUTO, "core",
		    CTLTYPE_INT | CTLFLAG_RW, sc, i, thermal_temp_sysctl, "I",
		    "Core %d temperature", i);
	}

	sc->cdev = make_dev(&thermal_cdevsw, 0, UID_ROOT, GID_WHEEL, 0644,
	    "thermal");
	sc->cdev->si_drv1 = sc;
	return (0);
}

static int
thermal_detach(device_t dev)
{
	struct thermal_softc *sc = device_get_softc(dev);

	if (sc->cdev)
		destroy_dev(sc->cdev);
	sysctl_ctx_free(&sc->sysctl_ctx);
	mtx_destroy(&sc->mtx);
	return (0);
}

static device_method_t thermal_methods[] = {
	DEVMETHOD(device_probe,		thermal_probe),
	DEVMETHOD(device_attach,	thermal_attach),
	DEVMETHOD(device_detach,	thermal_detach),
	DEVMETHOD_END
};

DEFINE_CLASS_0(thermal, thermal_driver, thermal_methods,
    sizeof(struct thermal_softc));
static devclass_t thermal_devclass;

DRIVER_MODULE(thermal, nexus, thermal_driver, thermal_devclass, 0, 0);
MODULE_VERSION(thermal, 1);
MODULE_DEPEND(thermal, nexus, 1, 1, 1);
