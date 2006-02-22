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

#include <arm/at91/at91_spireg.h>
#include <arm/at91/at91_spiio.h>

struct at91_spi_softc
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
};

static inline uint32_t
RD4(struct at91_spi_softc *sc, bus_size_t off)
{
	return bus_read_4(sc->mem_res, off);
}

static inline void
WR4(struct at91_spi_softc *sc, bus_size_t off, uint32_t val)
{
	bus_write_4(sc->mem_res, off, val);
}

#define AT91_SPI_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	AT91_SPI_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define AT91_SPI_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev), \
	    "spi", MTX_DEF)
#define AT91_SPI_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define AT91_SPI_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define AT91_SPI_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);
#define CDEV2SOFTC(dev)		((dev)->si_drv1)

static devclass_t at91_spi_devclass;

/* bus entry points */

static int at91_spi_probe(device_t dev);
static int at91_spi_attach(device_t dev);
static int at91_spi_detach(device_t dev);
static void at91_spi_intr(void *);

/* helper routines */
static int at91_spi_activate(device_t dev);
static void at91_spi_deactivate(device_t dev);

/* cdev routines */
static d_open_t at91_spi_open;
static d_close_t at91_spi_close;
static d_ioctl_t at91_spi_ioctl;

static struct cdevsw at91_spi_cdevsw =
{
	.d_version = D_VERSION,
	.d_open = at91_spi_open,
	.d_close = at91_spi_close,
	.d_ioctl = at91_spi_ioctl
};

static int
at91_spi_probe(device_t dev)
{
	device_set_desc(dev, "SPI");
	return (0);
}

static int
at91_spi_attach(device_t dev)
{
	struct at91_spi_softc *sc = device_get_softc(dev);
	int err;

	sc->dev = dev;
	err = at91_spi_activate(dev);
	if (err)
		goto out;

	AT91_SPI_LOCK_INIT(sc);

	/*
	 * Activate the interrupt
	 */
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    at91_spi_intr, sc, &sc->intrhand);
	if (err) {
		AT91_SPI_LOCK_DESTROY(sc);
		goto out;
	}
	sc->cdev = make_dev(&at91_spi_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
	    "spi%d", device_get_unit(dev));
	if (sc->cdev == NULL) {
		err = ENOMEM;
		goto out;
	}
	sc->cdev->si_drv1 = sc;
#if 0
	/* init */
	sc->cwgr = SPI_CWGR_CKDIV(1) |
	    SPI_CWGR_CHDIV(SPI_CWGR_DIV(SPI_DEF_CLK)) |
	    SPI_CWGR_CLDIV(SPI_CWGR_DIV(SPI_DEF_CLK));

	WR4(sc, SPI_CR, SPI_CR_SWRST);
	WR4(sc, SPI_CR, SPI_CR_MSEN | SPI_CR_SVDIS);
	WR4(sc, SPI_CWGR, sc->cwgr);
#endif
out:;
	if (err)
		at91_spi_deactivate(dev);
	return (err);
}

static int
at91_spi_detach(device_t dev)
{
	return (EBUSY);	/* XXX */
}

static int
at91_spi_activate(device_t dev)
{
	struct at91_spi_softc *sc;
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
	at91_spi_deactivate(dev);
	return (ENOMEM);
}

static void
at91_spi_deactivate(device_t dev)
{
	struct at91_spi_softc *sc;

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
at91_spi_intr(void *xsc)
{
	struct at91_spi_softc *sc = xsc;
#if 0
	uint32_t status;

	/* Reading the status also clears the interrupt */
	status = RD4(sc, SPI_SR);
	if (status == 0)
		return;
	AT91_SPI_LOCK(sc);
	if (status & SPI_SR_RXRDY)
		sc->flags |= RXRDY;
	if (status & SPI_SR_TXCOMP)
		sc->flags |= TXCOMP;
	if (status & SPI_SR_TXRDY)
		sc->flags |= TXRDY;
	AT91_SPI_UNLOCK(sc);
#endif
	wakeup(sc);
	return;
}

static int 
at91_spi_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct at91_spi_softc *sc;

	sc = CDEV2SOFTC(dev);
	AT91_SPI_LOCK(sc);
	if (!(sc->flags & OPENED)) {
		sc->flags |= OPENED;
#if 0
		WR4(sc, SPI_IER, SPI_SR_TXCOMP | SPI_SR_RXRDY | SPI_SR_TXRDY |
		    SPI_SR_OVRE | SPI_SR_UNRE | SPI_SR_NACK);
#endif
	}
	AT91_SPI_UNLOCK(sc);
    	return (0);
}

static int
at91_spi_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct at91_spi_softc *sc;

	sc = CDEV2SOFTC(dev);
	AT91_SPI_LOCK(sc);
	sc->flags &= ~OPENED;
#if 0
	WR4(sc, SPI_IDR, SPI_SR_TXCOMP | SPI_SR_RXRDY | SPI_SR_TXRDY |
	    SPI_SR_OVRE | SPI_SR_UNRE | SPI_SR_NACK);
#endif
	AT91_SPI_UNLOCK(sc);
	return (0);
}

static int
at91_spi_read_master(struct at91_spi_softc *sc, struct at91_spi_io *xfr)
{
#if 1
    return ENOTTY;
#else
	uint8_t *walker;
	uint8_t buffer[256];
	size_t len;
	int err = 0;

	if (xfr->xfer_len > sizeof(buffer))
		return (EINVAL);
	walker = buffer;
	len = xfr->xfer_len;
	RD4(sc, SPI_RHR);
	// Master mode, with the right address and interal addr size
	WR4(sc, SPI_MMR, SPI_MMR_IADRSZ(xfr->iadrsz) | SPI_MMR_MREAD |
	    SPI_MMR_DADR(xfr->dadr));
	WR4(sc, SPI_IADR, xfr->iadr);
	WR4(sc, SPI_CR, SPI_CR_START);
	while (len-- > 1) {
		while (!(sc->flags & RXRDY)) {
			err = msleep(sc, &sc->sc_mtx, PZERO | PCATCH, "spird",
			    0);
			if (err)
				return (err);
		}
		sc->flags &= ~RXRDY;
		*walker++ = RD4(sc, SPI_RHR) & 0xff;
	}
	WR4(sc, SPI_CR, SPI_CR_STOP);
	while (!(sc->flags & TXCOMP)) {
		err = msleep(sc, &sc->sc_mtx, PZERO | PCATCH, "spird2", 0);
		if (err)
			return (err);
	}
	sc->flags &= ~TXCOMP;
	*walker = RD4(sc, SPI_RHR) & 0xff;
	if (xfr->xfer_buf) {
		AT91_SPI_UNLOCK(sc);
		err = copyout(buffer, xfr->xfer_buf, xfr->xfer_len);
		AT91_SPI_LOCK(sc);
	}
	return (err);
#endif
}

static int
at91_spi_write_master(struct at91_spi_softc *sc, struct at91_spi_io *xfr)
{
#if 1
    return ENOTTY;
#else
	uint8_t *walker;
	uint8_t buffer[256];
	size_t len;
	int err;

	if (xfr->xfer_len > sizeof(buffer))
		return (EINVAL);
	walker = buffer;
	len = xfr->xfer_len;
	AT91_SPI_UNLOCK(sc);
	err = copyin(xfr->xfer_buf, buffer, xfr->xfer_len);
	AT91_SPI_LOCK(sc);
	if (err)
		return (err);
	/* Setup the xfr for later readback */
	xfr->xfer_buf = 0;
	xfr->xfer_len = 1;
	while (len--) {
		WR4(sc, SPI_MMR, SPI_MMR_IADRSZ(xfr->iadrsz) | SPI_MMR_MWRITE |
		    SPI_MMR_DADR(xfr->dadr));
		WR4(sc, SPI_IADR, xfr->iadr++);
		WR4(sc, SPI_THR, *walker++);
		WR4(sc, SPI_CR, SPI_CR_START);
		/*
		 * If we get signal while waiting for TXRDY, make sure we
		 * try to stop this device
		 */
		while (!(sc->flags & TXRDY)) {
			err = msleep(sc, &sc->sc_mtx, PZERO | PCATCH, "spiwr",
			    0);
			if (err)
				break;
		}
		WR4(sc, SPI_CR, SPI_CR_STOP);
		if (err)
			return (err);
		while (!(sc->flags & TXCOMP)) {
			err = msleep(sc, &sc->sc_mtx, PZERO | PCATCH, "spiwr2",
			    0);
			if (err)
				return (err);
		}
		/* Readback */
		at91_spi_read_master(sc, xfr);
	}
	return (err);
#endif
}

static int
at91_spi_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	int err = 0;
	struct at91_spi_softc *sc;

	sc = CDEV2SOFTC(dev);
	AT91_SPI_LOCK(sc);
	while (sc->flags & XFER_PENDING) {
		err = msleep(sc, &sc->sc_mtx, PZERO | PCATCH,
		    "spiwait", 0);
		if (err) {
			AT91_SPI_UNLOCK(sc);
			return (err);
		}
	}
	sc->flags |= XFER_PENDING;

	switch (cmd)
	{
	case SPIIOCXFER:
	{
		struct at91_spi_io *xfr = (struct at91_spi_io *)data;
		switch (xfr->type)
		{
		case SPI_IO_READ_MASTER:
			err = at91_spi_read_master(sc, xfr);
			break;
		case SPI_IO_WRITE_MASTER:
			err = at91_spi_write_master(sc, xfr);
			break;
		default:
			err = EINVAL;
			break;
		}
		break;
	}

	case SPIIOCSETCLOCK:
	{
#if 0
		struct at91_spi_clock *spick = (struct at91_spi_clock *)data;

		sc->cwgr = SPI_CWGR_CKDIV(spick->ckdiv) |
		    SPI_CWGR_CHDIV(SPI_CWGR_DIV(spick->high_rate)) |
		    SPI_CWGR_CLDIV(SPI_CWGR_DIV(spick->low_rate));
		WR4(sc, SPI_CR, SPI_CR_SWRST);
		WR4(sc, SPI_CR, SPI_CR_MSEN | SPI_CR_SVDIS);
		WR4(sc, SPI_CWGR, sc->cwgr);
#else
		err = ENOTTY;
#endif
		break;
	}
	default:
		err = ENOTTY;
		break;
	}
	sc->flags &= ~XFER_PENDING;
	AT91_SPI_UNLOCK(sc);
	wakeup(sc);
	return err;
}

static device_method_t at91_spi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		at91_spi_probe),
	DEVMETHOD(device_attach,	at91_spi_attach),
	DEVMETHOD(device_detach,	at91_spi_detach),

	{ 0, 0 }
};

static driver_t at91_spi_driver = {
	"at91_spi",
	at91_spi_methods,
	sizeof(struct at91_spi_softc),
};

DRIVER_MODULE(at91_spi, atmelarm, at91_spi_driver, at91_spi_devclass, 0, 0);
