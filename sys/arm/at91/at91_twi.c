/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <arm/at91/at91_twireg.h>
#include <arm/at91/at91var.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include "iicbus_if.h"

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#define	TWI_SLOW_CLOCK		 1500
#define	TWI_FAST_CLOCK		45000
#define	TWI_FASTEST_CLOCK	90000

struct at91_twi_softc
{
	device_t dev;			/* Myself */
	void *intrhand;			/* Interrupt handle */
	struct resource *irq_res;	/* IRQ resource */
	struct resource	*mem_res;	/* Memory resource */
	struct mtx sc_mtx;		/* basically a perimeter lock */
	volatile uint32_t flags;
	uint32_t cwgr;
	int	sc_started;
	int	twi_addr;
	device_t iicbus;
};

static inline uint32_t
RD4(struct at91_twi_softc *sc, bus_size_t off)
{

	return bus_read_4(sc->mem_res, off);
}

static inline void
WR4(struct at91_twi_softc *sc, bus_size_t off, uint32_t val)
{

	bus_write_4(sc->mem_res, off, val);
}

#define	AT91_TWI_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	AT91_TWI_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define	AT91_TWI_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev), \
	    "twi", MTX_DEF)
#define	AT91_TWI_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define	AT91_TWI_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define	AT91_TWI_ASSERT_UNLOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);
#define	TWI_DEF_CLK	100000

static devclass_t at91_twi_devclass;

/* bus entry points */

static int at91_twi_probe(device_t dev);
static int at91_twi_attach(device_t dev);
static int at91_twi_detach(device_t dev);
static void at91_twi_intr(void *);

/* helper routines */
static int at91_twi_activate(device_t dev);
static void at91_twi_deactivate(device_t dev);

static int
at91_twi_probe(device_t dev)
{
#ifdef FDT
	/* XXXX need a whole list, since there's at least 4 different ones */
	if (!ofw_bus_is_compatible(dev, "atmel,at91sam9g20-i2c"))
		return (ENXIO);
#endif
	device_set_desc(dev, "TWI");
	return (0);
}

static int
at91_twi_attach(device_t dev)
{
	struct at91_twi_softc *sc = device_get_softc(dev);
	int err;

	sc->dev = dev;
	err = at91_twi_activate(dev);
	if (err)
		goto out;

	AT91_TWI_LOCK_INIT(sc);

#ifdef FDT
	/*
	 * Disable devices need to hold their resources, so return now and not attach
	 * the iicbus, setup interrupt handlers, etc.
	 */
	if (!ofw_bus_status_okay(dev))
		return 0;
#endif

	/*
	 * Activate the interrupt
	 */
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, at91_twi_intr, sc, &sc->intrhand);
	if (err) {
		AT91_TWI_LOCK_DESTROY(sc);
		goto out;
	}
	sc->cwgr = TWI_CWGR_CKDIV(8 * at91_master_clock / TWI_FASTEST_CLOCK) |
	    TWI_CWGR_CHDIV(TWI_CWGR_DIV(TWI_DEF_CLK)) |
	    TWI_CWGR_CLDIV(TWI_CWGR_DIV(TWI_DEF_CLK));
	WR4(sc, TWI_CR, TWI_CR_SWRST);
	WR4(sc, TWI_CR, TWI_CR_MSEN | TWI_CR_SVDIS);
	WR4(sc, TWI_CWGR, sc->cwgr);

	if ((sc->iicbus = device_add_child(dev, "iicbus", -1)) == NULL)
		device_printf(dev, "could not allocate iicbus instance\n");
	/* probe and attach the iicbus */
	bus_generic_attach(dev);
out:
	if (err)
		at91_twi_deactivate(dev);
	return (err);
}

static int
at91_twi_detach(device_t dev)
{
	struct at91_twi_softc *sc;
	int rv;

	sc = device_get_softc(dev);
	at91_twi_deactivate(dev);
	if (sc->iicbus && (rv = device_delete_child(dev, sc->iicbus)) != 0)
		return (rv);

	AT91_TWI_LOCK_DESTROY(sc);

	return (0);
}

static int
at91_twi_activate(device_t dev)
{
	struct at91_twi_softc *sc;
	int rid;

	sc = device_get_softc(dev);
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL)
		goto errout;
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res == NULL)
		goto errout;
	return (0);
errout:
	at91_twi_deactivate(dev);
	return (ENOMEM);
}

static void
at91_twi_deactivate(device_t dev)
{
	struct at91_twi_softc *sc;

	sc = device_get_softc(dev);
	if (sc->intrhand)
		bus_teardown_intr(dev, sc->irq_res, sc->intrhand);
	sc->intrhand = NULL;
	bus_generic_detach(sc->dev);
	if (sc->mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->mem_res), sc->mem_res);
	sc->mem_res = NULL;
	if (sc->irq_res)
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->irq_res), sc->irq_res);
	sc->irq_res = NULL;
	return;
}

static void
at91_twi_intr(void *xsc)
{
	struct at91_twi_softc *sc = xsc;
	uint32_t status;

	status = RD4(sc, TWI_SR);
	if (status == 0)
		return;
	AT91_TWI_LOCK(sc);
	sc->flags |= status & (TWI_SR_OVRE | TWI_SR_UNRE | TWI_SR_NACK);
	if (status & TWI_SR_RXRDY)
		sc->flags |= TWI_SR_RXRDY;
	if (status & TWI_SR_TXRDY)
		sc->flags |= TWI_SR_TXRDY;
	if (status & TWI_SR_TXCOMP)
		sc->flags |= TWI_SR_TXCOMP;
	WR4(sc, TWI_IDR, status);
	wakeup(sc);
	AT91_TWI_UNLOCK(sc);
	return;
}

static int
at91_twi_wait(struct at91_twi_softc *sc, uint32_t bit)
{
	int err = 0;
	int counter = 100000;
	uint32_t sr;

	AT91_TWI_ASSERT_LOCKED(sc);
	while (!((sr = RD4(sc, TWI_SR)) & bit) && counter-- > 0 &&
	    !(sr & TWI_SR_NACK))
		continue;
	if (counter <= 0)
		err = EBUSY;
	else if (sr & TWI_SR_NACK)
		err = ENXIO;		// iic nack convention
	return (err);
}

static int
at91_twi_rst_card(device_t dev, u_char speed, u_char addr, u_char *oldaddr)
{
	struct at91_twi_softc *sc;
	int clk;

	sc = device_get_softc(dev);
	AT91_TWI_LOCK(sc);
	if (oldaddr)
		*oldaddr = sc->twi_addr;
	sc->twi_addr = addr;

	/*
	 * speeds are for 1.5kb/s, 45kb/s and 90kb/s.
	 */
	switch (speed) {
	case IIC_SLOW:
		clk = TWI_SLOW_CLOCK;
		break;

	case IIC_FAST:
		clk = TWI_FAST_CLOCK;
		break;

	case IIC_UNKNOWN:
	case IIC_FASTEST:
	default:
		clk = TWI_FASTEST_CLOCK;
		break;
	}
	sc->cwgr = TWI_CWGR_CKDIV(1) | TWI_CWGR_CHDIV(TWI_CWGR_DIV(clk)) |
	    TWI_CWGR_CLDIV(TWI_CWGR_DIV(clk));
	WR4(sc, TWI_CR, TWI_CR_SWRST);
	WR4(sc, TWI_CR, TWI_CR_MSEN | TWI_CR_SVDIS);
	WR4(sc, TWI_CWGR, sc->cwgr);
	AT91_TWI_UNLOCK(sc);

	return 0;
}

static int
at91_twi_callback(device_t dev, int index, caddr_t data)
{
	int error = 0;

	switch (index) {
	case IIC_REQUEST_BUS:
		break;

	case IIC_RELEASE_BUS:
		break;

	default:
		error = EINVAL;
	}

	return (error);
}

static int
at91_twi_transfer(device_t dev, struct iic_msg *msgs, uint32_t nmsgs)
{
	struct at91_twi_softc *sc;
	int i, len, err;
	uint32_t rdwr;
	uint8_t *buf;
	uint32_t sr;

	sc = device_get_softc(dev);
	err = 0;
	AT91_TWI_LOCK(sc);
	for (i = 0; i < nmsgs; i++) {
		/*
		 * The linux atmel driver doesn't use the internal device
		 * address feature of twi.  A separate i2c message needs to
		 * be written to use this.
		 * See http://lists.arm.linux.org.uk/pipermail/linux-arm-kernel/2004-September/024411.html
		 * for details.  Upon reflection, we could use this as an
		 * optimization, but it is unclear the code bloat will
		 * result in faster/better operations.
		 */
		rdwr = (msgs[i].flags & IIC_M_RD) ? TWI_MMR_MREAD : 0;
		WR4(sc, TWI_MMR, TWI_MMR_DADR(msgs[i].slave) | rdwr);
		len = msgs[i].len;
		buf = msgs[i].buf;
		/* zero byte transfers aren't allowed */
		if (len == 0 || buf == NULL) {
			err = EINVAL;
			goto out;
		}
		if (len == 1 && msgs[i].flags & IIC_M_RD)
			WR4(sc, TWI_CR, TWI_CR_START | TWI_CR_STOP);
		else
			WR4(sc, TWI_CR, TWI_CR_START);
		if (msgs[i].flags & IIC_M_RD) {
			sr = RD4(sc, TWI_SR);
			while (!(sr & TWI_SR_TXCOMP)) {
				if ((sr = RD4(sc, TWI_SR)) & TWI_SR_RXRDY) {
					len--;
					*buf++ = RD4(sc, TWI_RHR) & 0xff;
					if (len == 1)
						WR4(sc, TWI_CR, TWI_CR_STOP);
				}
			}
			if (len > 0 || (sr & TWI_SR_NACK)) {
				err = ENXIO;		// iic nack convention
				goto out;
			}
		} else {
			while (len--) {
				if ((err = at91_twi_wait(sc, TWI_SR_TXRDY)))
					goto out;
				WR4(sc, TWI_THR, *buf++);
			}
			WR4(sc, TWI_CR, TWI_CR_STOP);
		}
		if ((err = at91_twi_wait(sc, TWI_SR_TXCOMP)))
			break;
	}
out:
	if (err) {
		WR4(sc, TWI_CR, TWI_CR_SWRST);
		WR4(sc, TWI_CR, TWI_CR_MSEN | TWI_CR_SVDIS);
		WR4(sc, TWI_CWGR, sc->cwgr);
	}
	AT91_TWI_UNLOCK(sc);
	return (err);
}

static device_method_t at91_twi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		at91_twi_probe),
	DEVMETHOD(device_attach,	at91_twi_attach),
	DEVMETHOD(device_detach,	at91_twi_detach),

	/* iicbus interface */
	DEVMETHOD(iicbus_callback,	at91_twi_callback),
	DEVMETHOD(iicbus_reset,		at91_twi_rst_card),
	DEVMETHOD(iicbus_transfer,	at91_twi_transfer),
	DEVMETHOD_END
};

static driver_t at91_twi_driver = {
	"at91_twi",
	at91_twi_methods,
	sizeof(struct at91_twi_softc),
};

#ifdef FDT
DRIVER_MODULE(at91_twi, simplebus, at91_twi_driver, at91_twi_devclass, NULL,
    NULL);
#else
DRIVER_MODULE(at91_twi, atmelarm, at91_twi_driver, at91_twi_devclass, NULL,
    NULL);
#endif
DRIVER_MODULE(iicbus, at91_twi, iicbus_driver, iicbus_devclass, NULL, NULL);
MODULE_DEPEND(at91_twi, iicbus, 1, 1, 1);
