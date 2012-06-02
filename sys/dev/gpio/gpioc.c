/*-
 * Copyright (c) 2009 Oleksandr Tymoshenko <gonzo@freebsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>

#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <sys/gpio.h>
#include "gpio_if.h"

#undef GPIOC_DEBUG
#ifdef GPIOC_DEBUG
#define dprintf printf
#else
#define dprintf(x, arg...)
#endif

static int gpioc_probe(device_t dev);
static int gpioc_attach(device_t dev);
static int gpioc_detach(device_t dev);

static d_ioctl_t	gpioc_ioctl;

static struct cdevsw gpioc_cdevsw = {
	.d_version	= D_VERSION,
	.d_ioctl	= gpioc_ioctl,
	.d_name		= "gpioc",
};

struct gpioc_softc {
	device_t	sc_dev;		/* gpiocX dev */
	device_t	sc_pdev;	/* gpioX dev */
	struct cdev	*sc_ctl_dev;	/* controller device */
	int		sc_unit;
};

static int
gpioc_probe(device_t dev)
{
	device_set_desc(dev, "GPIO controller");
	return (0);
}

static int
gpioc_attach(device_t dev)
{
	struct gpioc_softc *sc = device_get_softc(dev);

	sc->sc_dev = dev;
	sc->sc_pdev = device_get_parent(dev);
	sc->sc_unit = device_get_unit(dev);
	sc->sc_ctl_dev = make_dev(&gpioc_cdevsw, sc->sc_unit,
	    UID_ROOT, GID_WHEEL, 0600, "gpioc%d", sc->sc_unit);
	if (!sc->sc_ctl_dev) {
		printf("Failed to create gpioc%d", sc->sc_unit);
		return (ENXIO);
	}
	sc->sc_ctl_dev->si_drv1 = sc;

	return (0);
}

static int
gpioc_detach(device_t dev)
{
	struct gpioc_softc *sc = device_get_softc(dev);
	int err;

	if (sc->sc_ctl_dev)
		destroy_dev(sc->sc_ctl_dev);

	if ((err = bus_generic_detach(dev)) != 0)
		return (err);

	return (0);
}

static int 
gpioc_ioctl(struct cdev *cdev, u_long cmd, caddr_t arg, int fflag, 
    struct thread *td)
{
	int max_pin, res;
	struct gpioc_softc *sc = cdev->si_drv1;
	struct gpio_pin pin;
	struct gpio_req req;

	switch (cmd) {
		case GPIOMAXPIN:
			max_pin = -1;
			res = GPIO_PIN_MAX(sc->sc_pdev, &max_pin);
			bcopy(&max_pin, arg, sizeof(max_pin));
			break;
		case GPIOGETCONFIG:
			bcopy(arg, &pin, sizeof(pin));
			dprintf("get config pin %d\n", pin.gp_pin);
			res = GPIO_PIN_GETFLAGS(sc->sc_pdev, pin.gp_pin,
			    &pin.gp_flags);
			/* Fail early */
			if (res)
				break;
			GPIO_PIN_GETCAPS(sc->sc_pdev, pin.gp_pin, &pin.gp_caps);
			GPIO_PIN_GETNAME(sc->sc_pdev, pin.gp_pin, pin.gp_name);
			bcopy(&pin, arg, sizeof(pin));
			break;
		case GPIOSETCONFIG:
			bcopy(arg, &pin, sizeof(pin));
			dprintf("set config pin %d\n", pin.gp_pin);
			res = GPIO_PIN_SETFLAGS(sc->sc_pdev, pin.gp_pin,
			    pin.gp_flags);
			break;
		case GPIOGET:
			bcopy(arg, &req, sizeof(req));
			res = GPIO_PIN_GET(sc->sc_pdev, req.gp_pin,
			    &req.gp_value);
			dprintf("read pin %d -> %d\n", 
			    req.gp_pin, req.gp_value);
			bcopy(&req, arg, sizeof(req));
			break;
		case GPIOSET:
			bcopy(arg, &req, sizeof(req));
			res = GPIO_PIN_SET(sc->sc_pdev, req.gp_pin, 
			    req.gp_value);
			dprintf("write pin %d -> %d\n", 
			    req.gp_pin, req.gp_value);
			break;
		case GPIOTOGGLE:
			bcopy(arg, &req, sizeof(req));
			dprintf("toggle pin %d\n", 
			    req.gp_pin);
			res = GPIO_PIN_TOGGLE(sc->sc_pdev, req.gp_pin);
			break;
		default:
			return (ENOTTY);
			break;
	}

	return (res);
}

static device_method_t gpioc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		gpioc_probe),
	DEVMETHOD(device_attach,	gpioc_attach),
	DEVMETHOD(device_detach,	gpioc_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	{ 0, 0 }
};

driver_t gpioc_driver = {
	"gpioc",
	gpioc_methods,
	sizeof(struct gpioc_softc)
};

devclass_t	gpioc_devclass;

DRIVER_MODULE(gpioc, gpio, gpioc_driver, gpioc_devclass, 0, 0);
MODULE_VERSION(gpioc, 1);
