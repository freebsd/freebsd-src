#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>

#include <arm/s3c2xx0/s3c24x0reg.h>

#include "uart_if.h"

static int uart_s3c2410_probe(device_t dev);

static device_method_t uart_s3c2410_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		uart_s3c2410_probe),
	DEVMETHOD(device_attach,	uart_bus_attach),
	DEVMETHOD(device_detach,	uart_bus_detach),
	{ 0, 0 }
};

static driver_t uart_s3c2410_driver = {
	uart_driver_name,
	uart_s3c2410_methods,
	sizeof(struct uart_softc),
};

extern SLIST_HEAD(uart_devinfo_list, uart_devinfo) uart_sysdevs;
static int
uart_s3c2410_probe(device_t dev)
{
	struct uart_devinfo *sysdev;
	struct uart_softc *sc;
	int unit;

	sc = device_get_softc(dev);
	sc->sc_class = &uart_s3c2410_class;

	unit = device_get_unit(dev);
	sysdev = SLIST_FIRST(&uart_sysdevs);
	if (S3C24X0_UART_BASE(unit) == sysdev->bas.bsh) {
		sc->sc_sysdev = sysdev;
		bcopy(&sc->sc_sysdev->bas, &sc->sc_bas, sizeof(sc->sc_bas));
	}
	return(uart_bus_probe(dev, 0, 0, 0, unit));
}

DRIVER_MODULE(uart, s3c24x0, uart_s3c2410_driver, uart_devclass, 0, 0);
