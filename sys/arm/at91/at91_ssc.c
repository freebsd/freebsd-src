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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <arm/at91/at91_sscreg.h>

struct at91_ssc_softc
{
	device_t dev;			/* Myself */
	void *intrhand;			/* Interrupt handle */
	struct resource *irq_res;	/* IRQ resource */
	struct resource	*mem_res;	/* Memory resource */
	struct mtx sc_mtx;		/* basically a perimeter lock */
	struct cdev *cdev;
	int flags;
#define OPENED 1
};

static inline uint32_t
RD4(struct at91_ssc_softc *sc, bus_size_t off)
{
	return bus_read_4(sc->mem_res, off);
}

static inline void
WR4(struct at91_ssc_softc *sc, bus_size_t off, uint32_t val)
{
	bus_write_4(sc->mem_res, off, val);
}

#define AT91_SSC_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	AT91_SSC_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define AT91_SSC_LOCK_INIT(_sc) \
	mtx_init(&(_sc)->sc_mtx, device_get_nameunit((_sc)->dev), \
	    "ssc", MTX_DEF)
#define AT91_SSC_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->sc_mtx);
#define AT91_SSC_ASSERT_LOCKED(_sc)	mtx_assert(&(_sc)->sc_mtx, MA_OWNED);
#define AT91_SSC_ASSERT_UNLOCKED(_sc) mtx_assert(&(_sc)->sc_mtx, MA_NOTOWNED);
#define CDEV2SOFTC(dev)		((dev)->si_drv1)

static devclass_t at91_ssc_devclass;

/* bus entry points */

static int at91_ssc_probe(device_t dev);
static int at91_ssc_attach(device_t dev);
static int at91_ssc_detach(device_t dev);
static void at91_ssc_intr(void *);

/* helper routines */
static int at91_ssc_activate(device_t dev);
static void at91_ssc_deactivate(device_t dev);

/* cdev routines */
static d_open_t at91_ssc_open;
static d_close_t at91_ssc_close;
static d_read_t at91_ssc_read;
static d_write_t at91_ssc_write;

static struct cdevsw at91_ssc_cdevsw =
{
	.d_version = D_VERSION,
	.d_open = at91_ssc_open,
	.d_close = at91_ssc_close,
	.d_read = at91_ssc_read,
	.d_write = at91_ssc_write,
};

static int
at91_ssc_probe(device_t dev)
{
	device_set_desc(dev, "SSC");
	return (0);
}

static int
at91_ssc_attach(device_t dev)
{
	struct at91_ssc_softc *sc = device_get_softc(dev);
	int err;

	sc->dev = dev;
	err = at91_ssc_activate(dev);
	if (err)
		goto out;

	AT91_SSC_LOCK_INIT(sc);

	/*
	 * Activate the interrupt
	 */
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, at91_ssc_intr, sc, &sc->intrhand);
	if (err) {
		AT91_SSC_LOCK_DESTROY(sc);
		goto out;
	}
	sc->cdev = make_dev(&at91_ssc_cdevsw, device_get_unit(dev), UID_ROOT,
	    GID_WHEEL, 0600, "ssc%d", device_get_unit(dev));
	if (sc->cdev == NULL) {
		err = ENOMEM;
		goto out;
	}
	sc->cdev->si_drv1 = sc;

	// Init for TSC needs
	WR4(sc, SSC_CR, SSC_CR_SWRST);
	WR4(sc, SSC_CMR, 0);		// clock divider unused
	WR4(sc, SSC_RCMR,
	    SSC_RCMR_CKS_RK | SSC_RCMR_CKO_NONE | SSC_RCMR_START_FALL_EDGE_RF);
	WR4(sc, SSC_RFMR,
	    0x1f | SSC_RFMR_MSFBF | SSC_RFMR_FSOS_NONE);
	WR4(sc, SSC_TCMR,
	    SSC_TCMR_CKS_TK | SSC_TCMR_CKO_NONE |  SSC_RCMR_START_CONT);
	WR4(sc, SSC_TFMR,
	    0x1f | SSC_TFMR_DATDEF | SSC_TFMR_MSFBF | SSC_TFMR_FSOS_NEG_PULSE);

out:
	if (err)
		at91_ssc_deactivate(dev);
	return (err);
}

static int
at91_ssc_detach(device_t dev)
{
	return (EBUSY);	/* XXX */
}

static int
at91_ssc_activate(device_t dev)
{
	struct at91_ssc_softc *sc;
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
	at91_ssc_deactivate(dev);
	return (ENOMEM);
}

static void
at91_ssc_deactivate(device_t dev)
{
	struct at91_ssc_softc *sc;

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
at91_ssc_intr(void *xsc)
{
	struct at91_ssc_softc *sc = xsc;
	wakeup(sc);
	return;
}

static int
at91_ssc_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct at91_ssc_softc *sc;

	sc = CDEV2SOFTC(dev);
	AT91_SSC_LOCK(sc);
	if (!(sc->flags & OPENED)) {
		sc->flags |= OPENED;
	}
	AT91_SSC_UNLOCK(sc);
    	return (0);
}

static int
at91_ssc_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct at91_ssc_softc *sc;

	sc = CDEV2SOFTC(dev);
	AT91_SSC_LOCK(sc);
	sc->flags &= ~OPENED;
	AT91_SSC_UNLOCK(sc);
	return (0);
}

static int
at91_ssc_read(struct cdev *dev, struct uio *uio, int flag)
{
	return EIO;
}

static int
at91_ssc_write(struct cdev *dev, struct uio *uio, int flag)
{
	return EIO;
}

static device_method_t at91_ssc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		at91_ssc_probe),
	DEVMETHOD(device_attach,	at91_ssc_attach),
	DEVMETHOD(device_detach,	at91_ssc_detach),

	{ 0, 0 }
};

static driver_t at91_ssc_driver = {
	"at91_ssc",
	at91_ssc_methods,
	sizeof(struct at91_ssc_softc),
};

DRIVER_MODULE(at91_ssc, atmelarm, at91_ssc_driver, at91_ssc_devclass, 0, 0);
