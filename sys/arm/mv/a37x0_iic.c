/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018, 2019 Rubicon Communications, LLC (Netgate)
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
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Driver for Armada 37x0 i2c controller.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/mv/a37x0_iicreg.h>

#include "iicbus_if.h"

struct a37x0_iic_softc {
	boolean_t		sc_fast_mode;
	bus_space_tag_t		sc_bst;
	bus_space_handle_t	sc_bsh;
	device_t		sc_dev;
	device_t		sc_iicbus;
	struct mtx		sc_mtx;
	struct resource		*sc_mem_res;
	struct resource		*sc_irq_res;
	void			*sc_intrhand;
};

#define	A37X0_IIC_WRITE(_sc, _off, _val)			\
    bus_space_write_4((_sc)->sc_bst, (_sc)->sc_bsh, _off, _val)
#define	A37X0_IIC_READ(_sc, _off)				\
    bus_space_read_4((_sc)->sc_bst, (_sc)->sc_bsh, _off)
#define	A37X0_IIC_LOCK(_sc)	mtx_lock(&(_sc)->sc_mtx)
#define	A37X0_IIC_UNLOCK(_sc)	mtx_unlock(&(_sc)->sc_mtx)

static struct ofw_compat_data compat_data[] = {
	{ "marvell,armada-3700-i2c",	1 },
	{ NULL,				0 }
};

#undef A37x0_IIC_DEBUG

static void a37x0_iic_intr(void *);
static int a37x0_iic_detach(device_t);

static void
a37x0_iic_rmw(struct a37x0_iic_softc *sc, uint32_t off, uint32_t mask,
    uint32_t value)
{
	uint32_t reg;

	mtx_assert(&sc->sc_mtx, MA_OWNED);
	reg = A37X0_IIC_READ(sc, off);
	reg &= ~mask;
	reg |= value;
	A37X0_IIC_WRITE(sc, off, reg);
}

static int
a37x0_iic_wait_clear(struct a37x0_iic_softc *sc, uint32_t mask)
{
	int timeout;
	uint32_t status;

	mtx_assert(&sc->sc_mtx, MA_OWNED);
	timeout = 1000;
	do {
		DELAY(10);
		status = A37X0_IIC_READ(sc, A37X0_IIC_ISR);
		if (--timeout == 0)
			return (0);
	} while ((status & mask) != 0);

	return (1);
}

static int
a37x0_iic_wait_set(struct a37x0_iic_softc *sc, uint32_t mask)
{
	int timeout;
	uint32_t status;

	mtx_assert(&sc->sc_mtx, MA_OWNED);
	timeout = 1000;
	do {
		DELAY(10);
		status = A37X0_IIC_READ(sc, A37X0_IIC_ISR);
		if (--timeout == 0)
			return (0);
	} while ((status & mask) != mask);

	return (1);
}

#ifdef A37x0_IIC_DEBUG
static void
a37x0_iic_regdump(struct a37x0_iic_softc *sc)
{

	mtx_assert(&sc->sc_mtx, MA_OWNED);
	printf("%s: IBMR: %#x\n", __func__, A37X0_IIC_READ(sc, A37X0_IIC_IBMR));
	printf("%s: ICR: %#x\n", __func__, A37X0_IIC_READ(sc, A37X0_IIC_ICR));
	printf("%s: ISR: %#x\n", __func__, A37X0_IIC_READ(sc, A37X0_IIC_ISR));
}
#endif

static void
a37x0_iic_reset(struct a37x0_iic_softc *sc)
{
	uint32_t mode, reg;

	mtx_assert(&sc->sc_mtx, MA_OWNED);

	/* Disable the controller. */
	reg = A37X0_IIC_READ(sc, A37X0_IIC_ICR);
	mode = reg & ICR_MODE_MASK;
	A37X0_IIC_WRITE(sc, A37X0_IIC_ICR, reg & ~ICR_IUE);
	A37X0_IIC_WRITE(sc, A37X0_IIC_ICR, reg | ICR_UR);
	DELAY(100);
	A37X0_IIC_WRITE(sc, A37X0_IIC_ICR, reg & ~ICR_IUE);

	/* Enable the controller. */
	reg = A37X0_IIC_READ(sc, A37X0_IIC_ICR);
	reg |= mode | ICR_IUE | ICR_GCD | ICR_SCLE;
	A37X0_IIC_WRITE(sc, A37X0_IIC_ICR, reg);
#ifdef A37x0_IIC_DEBUG
	a37x0_iic_regdump(sc);
#endif
}

static int
a37x0_iic_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Marvell Armada 37x0 IIC controller");

	return (BUS_PROBE_DEFAULT);
}

static int
a37x0_iic_attach(device_t dev)
{
	int rid;
	phandle_t node;
	struct a37x0_iic_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	rid = 0;
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (!sc->sc_mem_res) {
		device_printf(dev, "cannot allocate memory window\n");
		return (ENXIO);
	}

	sc->sc_bst = rman_get_bustag(sc->sc_mem_res);
	sc->sc_bsh = rman_get_bushandle(sc->sc_mem_res);

	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (!sc->sc_irq_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);
		device_printf(dev, "cannot allocate interrupt\n");
		return (ENXIO);
	}

	/* Hook up our interrupt handler. */
	if (bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, a37x0_iic_intr, sc, &sc->sc_intrhand)) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);
		device_printf(dev, "cannot setup the interrupt handler\n");
		return (ENXIO);
	}

	mtx_init(&sc->sc_mtx, "a37x0_iic", NULL, MTX_DEF);

	node = ofw_bus_get_node(dev);
	if (OF_hasprop(node, "mrvl,i2c-fast-mode"))
		sc->sc_fast_mode = true;

	/* Enable the controller. */
	A37X0_IIC_LOCK(sc);
	a37x0_iic_reset(sc);
	A37X0_IIC_UNLOCK(sc);

	sc->sc_iicbus = device_add_child(dev, "iicbus", -1);
	if (sc->sc_iicbus == NULL) {
		a37x0_iic_detach(dev);
		return (ENXIO);
	}

	/* Probe and attach the iicbus. */
	return (bus_generic_attach(dev));
}

static int
a37x0_iic_detach(device_t dev)
{
	struct a37x0_iic_softc *sc;

	bus_generic_detach(dev);

	sc = device_get_softc(dev);
	if (sc->sc_iicbus != NULL)
		device_delete_child(dev, sc->sc_iicbus);
	mtx_destroy(&sc->sc_mtx);
	if (sc->sc_intrhand)
		bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_intrhand);
	if (sc->sc_irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
	if (sc->sc_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);

	return (0);
}

static void
a37x0_iic_intr(void *arg)
{
	struct a37x0_iic_softc *sc;
	uint32_t status;

	/* Not used, the interrupts are not enabled. */
	sc = (struct a37x0_iic_softc *)arg;
	A37X0_IIC_LOCK(sc);
	status = A37X0_IIC_READ(sc, A37X0_IIC_ISR);
#ifdef A37x0_IIC_DEBUG
	a37x0_iic_regdump(sc);
#endif

	/* Clear pending interrrupts. */
	A37X0_IIC_WRITE(sc, A37X0_IIC_ISR, status);
	A37X0_IIC_UNLOCK(sc);
}

static int
a37x0_iic_stop(device_t dev)
{
	struct a37x0_iic_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);
	A37X0_IIC_LOCK(sc);
	/* Clear the STOP condition. */
	reg = A37X0_IIC_READ(sc, A37X0_IIC_ICR);
	if (reg & (ICR_ACKNAK | ICR_STOP)) {
		reg &= ~(ICR_START | ICR_ACKNAK | ICR_STOP);
		A37X0_IIC_WRITE(sc, A37X0_IIC_ICR, reg);
	}
	/* Clear interrupts. */
	reg = A37X0_IIC_READ(sc, A37X0_IIC_ISR);
	A37X0_IIC_WRITE(sc, A37X0_IIC_ISR, reg);
	A37X0_IIC_UNLOCK(sc);

	return (IIC_NOERR);
}

static int
a37x0_iic_start(device_t dev, u_char slave, int timeout)
{
	int rv;
	struct a37x0_iic_softc *sc;
	uint32_t reg, status;

	sc = device_get_softc(dev);
	A37X0_IIC_LOCK(sc);

	/* Wait for the bus to be free before start a transaction. */
	if (a37x0_iic_wait_clear(sc, ISR_IBB) == 0) {
		A37X0_IIC_UNLOCK(sc);
		return (IIC_ETIMEOUT);
	}

	/* Write the slave address. */
	A37X0_IIC_WRITE(sc, A37X0_IIC_IDBR, slave);

	/* Send Start condition (with slave address). */
	reg = A37X0_IIC_READ(sc, A37X0_IIC_ICR);
	reg &= ~(ICR_STOP | ICR_ACKNAK);
	A37X0_IIC_WRITE(sc, A37X0_IIC_ICR, reg | ICR_START | ICR_TB);

	rv = IIC_NOERR;
	if (a37x0_iic_wait_set(sc, ISR_ITE) == 0)
		rv = IIC_ETIMEOUT;
	if (rv == IIC_NOERR) {
		status = A37X0_IIC_READ(sc, A37X0_IIC_ISR);
		A37X0_IIC_WRITE(sc, A37X0_IIC_ISR, status | ISR_ITE);
		if (a37x0_iic_wait_clear(sc, ISR_ACKNAK) == 0)
			rv = IIC_ENOACK;
	}

	A37X0_IIC_UNLOCK(sc);
	if (rv != IIC_NOERR)
		a37x0_iic_stop(dev);

	return (rv);
}

static int
a37x0_iic_bus_reset(device_t dev, u_char speed, u_char addr, u_char *oldaddr)
{
	struct a37x0_iic_softc *sc;
	uint32_t busfreq;

	sc = device_get_softc(dev);
	A37X0_IIC_LOCK(sc);
	a37x0_iic_reset(sc);
	if (sc->sc_iicbus == NULL)
		busfreq = 100000;
	else
		busfreq = IICBUS_GET_FREQUENCY(sc->sc_iicbus, speed);
	a37x0_iic_rmw(sc, A37X0_IIC_ICR, ICR_MODE_MASK,
	    (busfreq > 100000) ? ICR_FAST_MODE : 0);
	A37X0_IIC_UNLOCK(sc);

	return (IIC_ENOADDR);
}

static int
a37x0_iic_read(device_t dev, char *buf, int len, int *read, int last, int delay)
{
	int rv;
	struct a37x0_iic_softc *sc;
	uint32_t reg, status;

	sc = device_get_softc(dev);
	A37X0_IIC_LOCK(sc);
	reg = A37X0_IIC_READ(sc, A37X0_IIC_ISR);
	if ((reg & (ISR_UB | ISR_IBB)) != ISR_UB) {
		A37X0_IIC_UNLOCK(sc);
		return (IIC_EBUSERR);
	}

	*read = 0;
	rv = IIC_NOERR;
	while (*read < len) {
		reg = A37X0_IIC_READ(sc, A37X0_IIC_ICR);
		reg &= ~(ICR_START | ICR_STOP | ICR_ACKNAK);
		if (*read == len - 1)
			reg |= ICR_ACKNAK | ICR_STOP;
		A37X0_IIC_WRITE(sc, A37X0_IIC_ICR, reg | ICR_TB);
		if (a37x0_iic_wait_set(sc, ISR_IRF) == 0) {
			rv = IIC_ETIMEOUT;
			break;
		}
		*buf++ = A37X0_IIC_READ(sc, A37X0_IIC_IDBR);
		(*read)++;
		status = A37X0_IIC_READ(sc, A37X0_IIC_ISR);
		A37X0_IIC_WRITE(sc, A37X0_IIC_ISR, status | ISR_IRF);
	}
	A37X0_IIC_UNLOCK(sc);

	return (rv);
}

static int
a37x0_iic_write(device_t dev, const char *buf, int len, int *sent, int timeout)
{
	int rv;
	struct a37x0_iic_softc *sc;
	uint32_t reg, status;

	sc = device_get_softc(dev);
	A37X0_IIC_LOCK(sc);
	reg = A37X0_IIC_READ(sc, A37X0_IIC_ISR);
	if ((reg & (ISR_UB | ISR_IBB)) != ISR_UB) {
		A37X0_IIC_UNLOCK(sc);
		return (IIC_EBUSERR);
	}

	rv = IIC_NOERR;
	*sent = 0;
	while (*sent < len) {
		A37X0_IIC_WRITE(sc, A37X0_IIC_IDBR, *buf++);
		reg = A37X0_IIC_READ(sc, A37X0_IIC_ICR);
		reg &= ~(ICR_START | ICR_STOP | ICR_ACKNAK);
		if (*sent == len - 1)
			reg |= ICR_STOP;
		A37X0_IIC_WRITE(sc, A37X0_IIC_ICR, reg | ICR_TB);
		if (a37x0_iic_wait_set(sc, ISR_ITE) == 0) {
			rv = IIC_ETIMEOUT;
			break;
		}
		(*sent)++;
		status = A37X0_IIC_READ(sc, A37X0_IIC_ISR);
		A37X0_IIC_WRITE(sc, A37X0_IIC_ISR, status | ISR_ITE);
		if (a37x0_iic_wait_clear(sc, ISR_ACKNAK) == 0) {
			rv = IIC_ENOACK;
			break;
		}
	}
	A37X0_IIC_UNLOCK(sc);

	return (rv);
}

static phandle_t
a37x0_iic_get_node(device_t bus, device_t dev)
{

	return (ofw_bus_get_node(bus));
}

static device_method_t a37x0_iic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		a37x0_iic_probe),
	DEVMETHOD(device_attach,	a37x0_iic_attach),
	DEVMETHOD(device_detach,	a37x0_iic_detach),

	/* iicbus interface */
	DEVMETHOD(iicbus_reset,		a37x0_iic_bus_reset),
	DEVMETHOD(iicbus_callback,	iicbus_null_callback),
	DEVMETHOD(iicbus_transfer,	iicbus_transfer_gen),
	DEVMETHOD(iicbus_repeated_start,	a37x0_iic_start),
	DEVMETHOD(iicbus_start,		a37x0_iic_start),
	DEVMETHOD(iicbus_stop,		a37x0_iic_stop),
	DEVMETHOD(iicbus_read,		a37x0_iic_read),
	DEVMETHOD(iicbus_write,		a37x0_iic_write),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	a37x0_iic_get_node),

	DEVMETHOD_END
};

static driver_t a37x0_iic_driver = {
	"iichb",
	a37x0_iic_methods,
	sizeof(struct a37x0_iic_softc),
};

DRIVER_MODULE(iicbus, a37x0_iic, iicbus_driver, 0, 0);
DRIVER_MODULE(a37x0_iic, simplebus, a37x0_iic_driver, 0, 0);
