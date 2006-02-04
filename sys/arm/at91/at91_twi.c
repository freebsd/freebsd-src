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
#include <arm/at91/at91_twiio.h>

struct at91_twi_softc
{
	device_t dev;			/* Myself */
	void *intrhand;			/* Interrupt handle */
	struct resource *irq_res;	/* IRQ resource */
	struct resource	*mem_res;	/* Memory resource */
	struct mtx sc_mtx;		/* basically a perimeter lock */
	int flags;
#define XFER_PENDING	1		/* true when transfer taking place */
#define OPENED		2		/* Device opened */
#define RXRDY		4
#define TXCOMP		8
#define TXRDY		0x10
	struct cdev *cdev;
	uint32_t cwgr;
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
#define CDEV2SOFTC(dev)		((dev)->si_drv1)
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

/* cdev routines */
static d_open_t at91_twi_open;
static d_close_t at91_twi_close;
static d_ioctl_t at91_twi_ioctl;

static struct cdevsw at91_twi_cdevsw =
{
	.d_version = D_VERSION,
	.d_open = at91_twi_open,
	.d_close = at91_twi_close,
	.d_ioctl = at91_twi_ioctl
};

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
	sc->cdev = make_dev(&at91_twi_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
	    "twi%d", device_get_unit(dev));
	if (sc->cdev == NULL) {
		err = ENOMEM;
		goto out;
	}
	sc->cdev->si_drv1 = sc;
	sc->cwgr = TWI_CWGR_CKDIV(1) |
	    TWI_CWGR_CHDIV(TWI_CWGR_DIV(TWI_DEF_CLK)) |
	    TWI_CWGR_CLDIV(TWI_CWGR_DIV(TWI_DEF_CLK));

	WR4(sc, TWI_CR, TWI_CR_SWRST);
	WR4(sc, TWI_CR, TWI_CR_MSEN | TWI_CR_SVDIS);
	WR4(sc, TWI_CWGR, sc->cwgr);
out:;
	if (err)
		at91_twi_deactivate(dev);
	return (err);
}

static int
at91_twi_detach(device_t dev)
{
	return (EBUSY);	/* XXX */
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
	if (status == 0)
		return;
	AT91_TWI_LOCK(sc);
	if (status & TWI_SR_RXRDY)
		sc->flags |= RXRDY;
	if (status & TWI_SR_TXCOMP)
		sc->flags |= TXCOMP;
	if (status & TWI_SR_TXRDY)
		sc->flags |= TXRDY;
	AT91_TWI_UNLOCK(sc);
	wakeup(sc);
	return;
}

static int 
at91_twi_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct at91_twi_softc *sc;

	sc = CDEV2SOFTC(dev);
	AT91_TWI_LOCK(sc);
	if (!(sc->flags & OPENED)) {
		sc->flags |= OPENED;
		WR4(sc, TWI_IER, TWI_SR_TXCOMP | TWI_SR_RXRDY | TWI_SR_TXRDY |
		    TWI_SR_OVRE | TWI_SR_UNRE | TWI_SR_NACK);
	}
	AT91_TWI_UNLOCK(sc);
    	return (0);
}

static int
at91_twi_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct at91_twi_softc *sc;

	sc = CDEV2SOFTC(dev);
	AT91_TWI_LOCK(sc);
	sc->flags &= ~OPENED;
	WR4(sc, TWI_IDR, TWI_SR_TXCOMP | TWI_SR_RXRDY | TWI_SR_TXRDY |
	    TWI_SR_OVRE | TWI_SR_UNRE | TWI_SR_NACK);
	AT91_TWI_UNLOCK(sc);
	return (0);
}


static int
at91_twi_read_master(struct at91_twi_softc *sc, struct at91_twi_io *xfr)
{
	uint8_t *walker;
	uint8_t buffer[256];
	size_t len;
	int err = 0;

	if (xfr->xfer_len > sizeof(buffer))
		return (EINVAL);
	walker = buffer;
	len = xfr->xfer_len;
	RD4(sc, TWI_RHR);
	// Master mode, with the right address and interal addr size
	WR4(sc, TWI_MMR, TWI_MMR_IADRSZ(xfr->iadrsz) | TWI_MMR_MREAD |
	    TWI_MMR_DADR(xfr->dadr));
	WR4(sc, TWI_IADR, xfr->iadr);
	WR4(sc, TWI_CR, TWI_CR_START);
	while (len-- > 1) {
		while (!(sc->flags & RXRDY)) {
			err = msleep(sc, &sc->sc_mtx, PZERO | PCATCH, "twird",
			    0);
			if (err)
				return (err);
		}
		sc->flags &= ~RXRDY;
		*walker++ = RD4(sc, TWI_RHR) & 0xff;
	}
	WR4(sc, TWI_CR, TWI_CR_STOP);
	while (!(sc->flags & TXCOMP)) {
		err = msleep(sc, &sc->sc_mtx, PZERO | PCATCH, "twird2", 0);
		if (err)
			return (err);
	}
	sc->flags &= ~TXCOMP;
	*walker = RD4(sc, TWI_RHR) & 0xff;
	if (xfr->xfer_buf) {
		AT91_TWI_UNLOCK(sc);
		err = copyout(buffer, xfr->xfer_buf, xfr->xfer_len);
		AT91_TWI_LOCK(sc);
	}
	return (err);
}

static int
at91_twi_write_master(struct at91_twi_softc *sc, struct at91_twi_io *xfr)
{
	uint8_t *walker;
	uint8_t buffer[256];
	size_t len;
	int err;

	if (xfr->xfer_len > sizeof(buffer))
		return (EINVAL);
	walker = buffer;
	len = xfr->xfer_len;
	AT91_TWI_UNLOCK(sc);
	err = copyin(xfr->xfer_buf, buffer, xfr->xfer_len);
	AT91_TWI_LOCK(sc);
	if (err)
		return (err);
	/* Setup the xfr for later readback */
	xfr->xfer_buf = 0;
	xfr->xfer_len = 1;
	while (len--) {
		WR4(sc, TWI_MMR, TWI_MMR_IADRSZ(xfr->iadrsz) | TWI_MMR_MWRITE |
		    TWI_MMR_DADR(xfr->dadr));
		WR4(sc, TWI_IADR, xfr->iadr++);
		WR4(sc, TWI_THR, *walker++);
		WR4(sc, TWI_CR, TWI_CR_START);
		/*
		 * If we get signal while waiting for TXRDY, make sure we
		 * try to stop this device
		 */
		while (!(sc->flags & TXRDY)) {
			err = msleep(sc, &sc->sc_mtx, PZERO | PCATCH, "twiwr",
			    0);
			if (err)
				break;
		}
		WR4(sc, TWI_CR, TWI_CR_STOP);
		if (err)
			return (err);
		while (!(sc->flags & TXCOMP)) {
			err = msleep(sc, &sc->sc_mtx, PZERO | PCATCH, "twiwr2",
			    0);
			if (err)
				return (err);
		}
		/* Readback */
		at91_twi_read_master(sc, xfr);
	}
	return (err);
}

static int
at91_twi_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	int err = 0;
	struct at91_twi_softc *sc;

	sc = CDEV2SOFTC(dev);
	AT91_TWI_LOCK(sc);
	while (sc->flags & XFER_PENDING) {
		err = msleep(sc, &sc->sc_mtx, PZERO | PCATCH,
		    "twiwait", 0);
		if (err) {
			AT91_TWI_UNLOCK(sc);
			return (err);
		}
	}
	sc->flags |= XFER_PENDING;

	switch (cmd)
	{
	case TWIIOCXFER:
	{
		struct at91_twi_io *xfr = (struct at91_twi_io *)data;
		switch (xfr->type)
		{
		case TWI_IO_READ_MASTER:
			err = at91_twi_read_master(sc, xfr);
			break;
		case TWI_IO_WRITE_MASTER:
			err = at91_twi_write_master(sc, xfr);
			break;
		default:
			err = EINVAL;
			break;
		}
		break;
	}

	case TWIIOCSETCLOCK:
	{
		struct at91_twi_clock *twick = (struct at91_twi_clock *)data;

		sc->cwgr = TWI_CWGR_CKDIV(twick->ckdiv) |
		    TWI_CWGR_CHDIV(TWI_CWGR_DIV(twick->high_rate)) |
		    TWI_CWGR_CLDIV(TWI_CWGR_DIV(twick->low_rate));
		WR4(sc, TWI_CR, TWI_CR_SWRST);
		WR4(sc, TWI_CR, TWI_CR_MSEN | TWI_CR_SVDIS);
		WR4(sc, TWI_CWGR, sc->cwgr);
		break;
	}
	default:
		err = ENOTTY;
		break;
	}
	sc->flags &= ~XFER_PENDING;
	AT91_TWI_UNLOCK(sc);
	wakeup(sc);
	return err;
}

static device_method_t at91_twi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		at91_twi_probe),
	DEVMETHOD(device_attach,	at91_twi_attach),
	DEVMETHOD(device_detach,	at91_twi_detach),

	{ 0, 0 }
};

static driver_t at91_twi_driver = {
	"at91_twi",
	at91_twi_methods,
	sizeof(struct at91_twi_softc),
};

DRIVER_MODULE(at91_twi, atmelarm, at91_twi_driver, at91_twi_devclass, 0, 0);
