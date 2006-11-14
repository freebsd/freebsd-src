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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

#include <arm/at91/at91rm92reg.h>
#include <arm/at91/at91_twireg.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include "iicbus_if.h"

struct at91_twi_softc
{
	device_t dev;			/* Myself */
	void *intrhand;			/* Interrupt handle */
	struct resource *irq_res;	/* IRQ resource */
	struct resource	*mem_res;	/* Memory resource */
	struct mtx sc_mtx;		/* basically a perimeter lock */
	volatile int flags;
#define RXRDY		4
#define TXRDY		0x10
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

#define AT91_TWI_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	AT91_TWI_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define AT91_TWI_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev), \
	    "twi", MTX_DEF)
#define AT91_TWI_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define AT91_TWI_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define AT91_TWI_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);
#define TWI_DEF_CLK	100000

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

	/*
	 * Activate the interrupt
	 */
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    at91_twi_intr, sc, &sc->intrhand);
	if (err) {
		AT91_TWI_LOCK_DESTROY(sc);
		goto out;
	}
	sc->cwgr = TWI_CWGR_CKDIV(8 * AT91C_MASTER_CLOCK / 90000) |
	    TWI_CWGR_CHDIV(TWI_CWGR_DIV(TWI_DEF_CLK)) |
	    TWI_CWGR_CLDIV(TWI_CWGR_DIV(TWI_DEF_CLK));

	WR4(sc, TWI_CR, TWI_CR_SWRST);
	WR4(sc, TWI_CR, TWI_CR_MSEN | TWI_CR_SVDIS);
	WR4(sc, TWI_CWGR, sc->cwgr);

	WR4(sc, TWI_IER, TWI_SR_RXRDY | TWI_SR_OVRE | TWI_SR_UNRE |
	    TWI_SR_NACK);

	if ((sc->iicbus = device_add_child(dev, "iicbus", -1)) == NULL)
		device_printf(dev, "could not allocate iicbus instance\n");

	/* probe and attach the iicbus */
	bus_generic_attach(dev);

out:;
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
	if (sc->mem_res == NULL)
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
	sc->intrhand = 0;
	bus_generic_detach(sc->dev);
	if (sc->mem_res)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    rman_get_rid(sc->mem_res), sc->mem_res);
	sc->mem_res = 0;
	if (sc->irq_res)
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->irq_res), sc->irq_res);
	sc->irq_res = 0;
	return;
}

static void
at91_twi_intr(void *xsc)
{
	struct at91_twi_softc *sc = xsc;
	uint32_t status;

	/* Reading the status also clears the interrupt */
	status = RD4(sc, TWI_SR);
	printf("status %x\n", status);
	if (status == 0)
		return;
	AT91_TWI_LOCK(sc);
	if (status & TWI_SR_RXRDY)
		sc->flags |= RXRDY;
	if (status & TWI_SR_TXRDY)
		sc->flags |= TXRDY;
	AT91_TWI_UNLOCK(sc);
	wakeup(sc);
	return;
}

static int
at91_twi_wait_stop_done(struct at91_twi_softc *sc)
{
	int err = 0;

	while (!(RD4(sc, TWI_SR) & TWI_SR_TXCOMP))
		continue;
	return (err);
}

/*
 * Stop the transfer by entering a STOP state on the iic bus.  For read
 * operations, we've already entered the STOP state, since we need to do
 * that to read the last character.  For write operations, we need to
 * wait for the TXCOMP bit to turn on before returning.
 */
static int
at91_twi_stop(device_t dev)
{
	struct at91_twi_softc *sc;
	int err = 0;

	sc = device_get_softc(dev);
	if (sc->sc_started) {
		WR4(sc, TWI_CR, TWI_CR_STOP);
		err = at91_twi_wait_stop_done(sc);
	}
	return (err);
}

/*
 * enter a START condition without requiring the device to be in a STOP
 * state.
 */
static int
at91_twi_repeated_start(device_t dev, u_char slave, int timeout)
{
	struct at91_twi_softc *sc;

	sc = device_get_softc(dev);
	WR4(sc, TWI_MMR, TWI_MMR_DADR(slave));
	return (0);
}

/*
 * enter a START condition from an idle state.
 */
static int
at91_twi_start(device_t dev, u_char slave, int timeout)
{
	struct at91_twi_softc *sc;

	sc = device_get_softc(dev);
	WR4(sc, TWI_MMR, TWI_MMR_DADR(slave));
	sc->sc_started = 1;
	return (0);
}

static int
at91_twi_write(device_t dev, char *buf, int len, int *sent, int timeout /* us */)
{
	struct at91_twi_softc *sc;
	uint8_t *walker;
	int err = 0;

	walker = buf;
	sc = device_get_softc(dev);
	AT91_TWI_LOCK(sc);
	WR4(sc, TWI_MMR, ~TWI_MMR_MREAD & RD4(sc, TWI_MMR));
	WR4(sc, TWI_CR, TWI_CR_START);
	sc->sc_started = 1;
	WR4(sc, TWI_IER, TWI_SR_TXRDY);
	while (len--) {
		WR4(sc, TWI_THR, *walker++);
		while (!(sc->flags & TXRDY)) {
			err = msleep(sc, &sc->sc_mtx, PZERO | PCATCH, "twiwr",
			    0);
			if (err)
				goto errout;
		}
	}
errout:;
	WR4(sc, TWI_IDR, TWI_SR_TXRDY);
	AT91_TWI_UNLOCK(sc);
	return (err);
}

static int
at91_twi_read(device_t dev, char *buf, int len, int *read, int last,
	 int delay /* us */)
{
	struct at91_twi_softc *sc;
	char *walker;
	int err = 0;

	walker = buf;
	sc = device_get_softc(dev);
	AT91_TWI_LOCK(sc);
	WR4(sc, TWI_MMR, TWI_MMR_MREAD | RD4(sc, TWI_MMR));
	WR4(sc, TWI_CR, TWI_CR_START);
	sc->sc_started = 1;
	WR4(sc, TWI_IER, TWI_SR_RXRDY);
	while (len-- > 0) {
		err = 0;
		while (!(sc->flags & RXRDY)) {
			err = msleep(sc, &sc->sc_mtx, PZERO | PCATCH, "twird",
			    0);
			if (err)
				goto errout;
		}
		sc->flags &= ~RXRDY;
		*walker++ = RD4(sc, TWI_RHR) & 0xff;
		if (len == 1 && last)
			break;
	}
	if (!last)
		goto errout;
	WR4(sc, TWI_CR, TWI_CR_STOP);
	err = at91_twi_wait_stop_done(sc);
	*walker = RD4(sc, TWI_RHR) & 0xff;
	if (read)
		*read = walker - buf;
errout:;
	WR4(sc, TWI_IDR, TWI_SR_RXRDY);
	AT91_TWI_UNLOCK(sc);
	return (err);
}

static int
at91_twi_rst_card(device_t dev, u_char speed, u_char addr, u_char *oldaddr)
{
	struct at91_twi_softc *sc;
	int ckdiv, rate;

	sc = device_get_softc(dev);
	if (oldaddr)
		*oldaddr = sc->twi_addr;
	if (addr != 0)
		sc->twi_addr = 0;
	else
		sc->twi_addr = addr;

	rate = 1;
	
	/*
	 * 8 * is because "rate == 1" -> 4 clocks down, 4 clocks up.  The
	 * speeds are for 1.5kb/s, 45kb/s and 90kb/s.
	 */
	switch (speed) {
	case IIC_SLOW:
		ckdiv = 8 * AT91C_MASTER_CLOCK / 1500;
		break;

	case IIC_FAST:
		ckdiv = 8 * AT91C_MASTER_CLOCK / 45000;
		break;

	case IIC_UNKNOWN:
	case IIC_FASTEST:
	default:
		ckdiv = 8 * AT91C_MASTER_CLOCK / 90000;
		break;
	}

	sc->cwgr = TWI_CWGR_CKDIV(ckdiv) | TWI_CWGR_CHDIV(TWI_CWGR_DIV(rate)) |
	    TWI_CWGR_CLDIV(TWI_CWGR_DIV(rate));
	WR4(sc, TWI_CR, TWI_CR_SWRST);
	WR4(sc, TWI_CR, TWI_CR_MSEN | TWI_CR_SVDIS);
	WR4(sc, TWI_CWGR, sc->cwgr);

	return EIO;
}

static int
at91_twi_callback(device_t dev, int index, caddr_t *data)
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

static device_method_t at91_twi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		at91_twi_probe),
	DEVMETHOD(device_attach,	at91_twi_attach),
	DEVMETHOD(device_detach,	at91_twi_detach),

	/* iicbus interface */
	DEVMETHOD(iicbus_callback,	at91_twi_callback),
	DEVMETHOD(iicbus_repeated_start, at91_twi_repeated_start),
	DEVMETHOD(iicbus_start,		at91_twi_start),
	DEVMETHOD(iicbus_stop,		at91_twi_stop),
	DEVMETHOD(iicbus_write,		at91_twi_write),
	DEVMETHOD(iicbus_read,		at91_twi_read),
	DEVMETHOD(iicbus_reset,		at91_twi_rst_card),
	{ 0, 0 }
};

static driver_t at91_twi_driver = {
	"at91_twi",
	at91_twi_methods,
	sizeof(struct at91_twi_softc),
};

DRIVER_MODULE(at91_twi, atmelarm, at91_twi_driver, at91_twi_devclass, 0, 0);
DRIVER_MODULE(iicbus, at91_twi, iicbus_driver, iicbus_devclass, 0, 0);
