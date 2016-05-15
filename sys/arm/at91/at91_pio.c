/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
 * Copyright (C) 2012 Ian Lepore. All rights reserved.
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
#include <sys/poll.h>
#include <sys/rman.h>
#include <sys/selinfo.h>
#include <sys/sx.h>
#include <sys/uio.h>
#include <machine/at91_gpio.h>
#include <machine/bus.h>

#include <arm/at91/at91reg.h>
#include <arm/at91/at91_pioreg.h>
#include <arm/at91/at91_piovar.h>

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#define	MAX_CHANGE	64

struct at91_pio_softc
{
	device_t dev;			/* Myself */
	void *intrhand;			/* Interrupt handle */
	struct resource *irq_res;	/* IRQ resource */
	struct resource	*mem_res;	/* Memory resource */
	struct sx sc_mtx;		/* basically a perimeter lock */
	struct cdev *cdev;
	struct selinfo selp;
	int buflen;
	uint8_t buf[MAX_CHANGE];
	int flags;
#define	OPENED 1
};

static inline uint32_t
RD4(struct at91_pio_softc *sc, bus_size_t off)
{

	return (bus_read_4(sc->mem_res, off));
}

static inline void
WR4(struct at91_pio_softc *sc, bus_size_t off, uint32_t val)
{

	bus_write_4(sc->mem_res, off, val);
}

#define	AT91_PIO_LOCK(_sc)		sx_xlock(&(_sc)->sc_mtx)
#define	AT91_PIO_UNLOCK(_sc)		sx_xunlock(&(_sc)->sc_mtx)
#define	AT91_PIO_LOCK_INIT(_sc) \
	sx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev))
#define	AT91_PIO_LOCK_DESTROY(_sc)	sx_destroy(&_sc->sc_mtx);
#define	AT91_PIO_ASSERT_LOCKED(_sc)	sx_assert(&_sc->sc_mtx, SA_XLOCKED);
#define	AT91_PIO_ASSERT_UNLOCKED(_sc)	sx_assert(&_sc->sc_mtx, SA_UNLOCKED);
#define	CDEV2SOFTC(dev)			((dev)->si_drv1)

static devclass_t at91_pio_devclass;

/* bus entry points */

static int at91_pio_probe(device_t dev);
static int at91_pio_attach(device_t dev);
static int at91_pio_detach(device_t dev);
static void at91_pio_intr(void *);

/* helper routines */
static int at91_pio_activate(device_t dev);
static void at91_pio_deactivate(device_t dev);

/* cdev routines */
static d_open_t at91_pio_open;
static d_close_t at91_pio_close;
static d_read_t at91_pio_read;
static d_poll_t at91_pio_poll;
static d_ioctl_t at91_pio_ioctl;

static struct cdevsw at91_pio_cdevsw =
{
	.d_version = D_VERSION,
	.d_open = at91_pio_open,
	.d_close = at91_pio_close,
	.d_read = at91_pio_read,
	.d_poll = at91_pio_poll,
	.d_ioctl = at91_pio_ioctl
};

static int
at91_pio_probe(device_t dev)
{
	const char *name;
#ifdef FDT
	if (!ofw_bus_is_compatible(dev, "atmel,at91rm9200-gpio"))
		return (ENXIO);
#endif
	switch (device_get_unit(dev)) {
	case 0:
		name = "PIOA";
		break;
	case 1:
		name = "PIOB";
		break;
	case 2:
		name = "PIOC";
		break;
	case 3:
		name = "PIOD";
		break;
	case 4:
		name = "PIOE";
		break;
	case 5:
		name = "PIOF";
		break;
	default:
		name = "PIO";
		break;
	}
	device_set_desc(dev, name);
	return (0);
}

static int
at91_pio_attach(device_t dev)
{
	struct at91_pio_softc *sc;
	int err;

	sc = device_get_softc(dev);
	sc->dev = dev;
	err = at91_pio_activate(dev);
	if (err)
		goto out;

        if (bootverbose)
		device_printf(dev, "ABSR: %#x OSR: %#x PSR:%#x ODSR: %#x\n",
		    RD4(sc, PIO_ABSR), RD4(sc, PIO_OSR), RD4(sc, PIO_PSR),
		    RD4(sc, PIO_ODSR));
	AT91_PIO_LOCK_INIT(sc);

	/*
	 * Activate the interrupt, but disable all interrupts in the hardware.
	 */
	WR4(sc, PIO_IDR, 0xffffffff);
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC,
	    NULL, at91_pio_intr, sc, &sc->intrhand);
	if (err) {
		AT91_PIO_LOCK_DESTROY(sc);
		goto out;
	}
	sc->cdev = make_dev(&at91_pio_cdevsw, device_get_unit(dev), UID_ROOT,
	    GID_WHEEL, 0600, "pio%d", device_get_unit(dev));
	if (sc->cdev == NULL) {
		err = ENOMEM;
		goto out;
	}
	sc->cdev->si_drv1 = sc;
out:
	if (err)
		at91_pio_deactivate(dev);
	return (err);
}

static int
at91_pio_detach(device_t dev)
{

	return (EBUSY);	/* XXX */
}

static int
at91_pio_activate(device_t dev)
{
	struct at91_pio_softc *sc;
	int rid;

	sc = device_get_softc(dev);
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL)
		goto errout;
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->irq_res == NULL)
		goto errout;
	return (0);
errout:
	at91_pio_deactivate(dev);
	return (ENOMEM);
}

static void
at91_pio_deactivate(device_t dev)
{
	struct at91_pio_softc *sc;

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
}

static void
at91_pio_intr(void *xsc)
{
	struct at91_pio_softc *sc = xsc;
	uint32_t status;
	int i;

	/* Reading the status also clears the interrupt. */
	status = RD4(sc, PIO_ISR) & RD4(sc, PIO_IMR);
	if (status != 0) {
		AT91_PIO_LOCK(sc);
		for (i = 0; status != 0 && sc->buflen < MAX_CHANGE; ++i) {
			if (status & 1)
				sc->buf[sc->buflen++] = (uint8_t)i;
			status >>= 1;
		}
		AT91_PIO_UNLOCK(sc);
		wakeup(sc);
		selwakeup(&sc->selp);
	}
}

static int
at91_pio_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct at91_pio_softc *sc;

	sc = CDEV2SOFTC(dev);
	AT91_PIO_LOCK(sc);
	if (!(sc->flags & OPENED)) {
		sc->flags |= OPENED;
	}
	AT91_PIO_UNLOCK(sc);
	return (0);
}

static int
at91_pio_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct at91_pio_softc *sc;

	sc = CDEV2SOFTC(dev);
	AT91_PIO_LOCK(sc);
	sc->flags &= ~OPENED;
	AT91_PIO_UNLOCK(sc);
	return (0);
}

static int
at91_pio_poll(struct cdev *dev, int events, struct thread *td)
{
	struct at91_pio_softc *sc;
	int revents = 0;

	sc = CDEV2SOFTC(dev);
	AT91_PIO_LOCK(sc);
	if (events & (POLLIN | POLLRDNORM)) {
		if (sc->buflen != 0)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(td, &sc->selp);
	}
	AT91_PIO_UNLOCK(sc);

	return (revents);
}

static int
at91_pio_read(struct cdev *dev, struct uio *uio, int flag)
{
	struct at91_pio_softc *sc;
	int err, ret, len;

	sc = CDEV2SOFTC(dev);
	AT91_PIO_LOCK(sc);
	err = 0;
	ret = 0;
	while (uio->uio_resid) {
		while (sc->buflen == 0 && err == 0)
			err = msleep(sc, &sc->sc_mtx, PCATCH | PZERO, "prd", 0);
		if (err != 0)
			break;
		len = MIN(sc->buflen, uio->uio_resid);
		err = uiomove(sc->buf, len, uio);
		if (err != 0)
			break;
		/*
		 * If we read the whole thing no datacopy is needed,
		 * otherwise we move the data down.
		 */
		ret += len;
		if (sc->buflen == len)
			sc->buflen = 0;
		else {
			bcopy(sc->buf + len, sc->buf, sc->buflen - len);
			sc->buflen -= len;
		}
		/* If there's no data left, end the read. */
		if (sc->buflen == 0)
			break;
	}
	AT91_PIO_UNLOCK(sc);
	return (err);
}

static void
at91_pio_bang32(struct at91_pio_softc *sc, uint32_t bits, uint32_t datapin,
    uint32_t clockpin)
{
	int i;

	for (i = 0; i < 32; i++) {
		if (bits & 0x80000000)
			WR4(sc, PIO_SODR, datapin);
		else
			WR4(sc, PIO_CODR, datapin);
		bits <<= 1;
		WR4(sc, PIO_CODR, clockpin);
		WR4(sc, PIO_SODR, clockpin);
	}
}

static void
at91_pio_bang(struct at91_pio_softc *sc, uint8_t bits, uint32_t bitcount, 
              uint32_t datapin, uint32_t clockpin)
{
	int i;

	for (i = 0; i < bitcount; i++) {
		if (bits & 0x80)
			WR4(sc, PIO_SODR, datapin);
		else
			WR4(sc, PIO_CODR, datapin);
		bits <<= 1;
		WR4(sc, PIO_CODR, clockpin);
		WR4(sc, PIO_SODR, clockpin);
	}
}

static int
at91_pio_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct at91_pio_softc *sc;
	struct at91_gpio_cfg *cfg;
	struct at91_gpio_info *info;
	struct at91_gpio_bang *bang;
	struct at91_gpio_bang_many *bangmany;
	uint32_t i, num;
	uint8_t many[1024], *walker;
	int err;
        int bitcount;

	sc = CDEV2SOFTC(dev);
	switch(cmd) {
	case AT91_GPIO_SET:	/* turn bits on */
		WR4(sc, PIO_SODR, *(uint32_t *)data);
		return (0);
	case AT91_GPIO_CLR:	/* turn bits off */
		WR4(sc, PIO_CODR, *(uint32_t *)data);
		return (0);
	case AT91_GPIO_READ:	/* Get the status of input bits */
		*(uint32_t *)data = RD4(sc, PIO_PDSR);
		return (0);
	case AT91_GPIO_CFG:	/* Configure AT91_GPIO pins */
		cfg = (struct at91_gpio_cfg *)data;
		if (cfg->cfgmask & AT91_GPIO_CFG_INPUT) {
			WR4(sc, PIO_OER, cfg->iomask & ~cfg->input);
			WR4(sc, PIO_ODR, cfg->iomask & cfg->input);
		}
		if (cfg->cfgmask & AT91_GPIO_CFG_HI_Z) {
			WR4(sc, PIO_MDDR, cfg->iomask & ~cfg->hi_z);
			WR4(sc, PIO_MDER, cfg->iomask & cfg->hi_z);
		}
		if (cfg->cfgmask & AT91_GPIO_CFG_PULLUP) {
			WR4(sc, PIO_PUDR, cfg->iomask & ~cfg->pullup);
			WR4(sc, PIO_PUER, cfg->iomask & cfg->pullup);
		}
		if (cfg->cfgmask & AT91_GPIO_CFG_GLITCH) {
			WR4(sc, PIO_IFDR, cfg->iomask & ~cfg->glitch);
			WR4(sc, PIO_IFER, cfg->iomask & cfg->glitch);
		}
		if (cfg->cfgmask & AT91_GPIO_CFG_GPIO) {
			WR4(sc, PIO_PDR, cfg->iomask & ~cfg->gpio);
			WR4(sc, PIO_PER, cfg->iomask & cfg->gpio);
		}
		if (cfg->cfgmask & AT91_GPIO_CFG_PERIPH) {
			WR4(sc, PIO_ASR, cfg->iomask & ~cfg->periph);
			WR4(sc, PIO_BSR, cfg->iomask & cfg->periph);
		}
		if (cfg->cfgmask & AT91_GPIO_CFG_INTR) {
			WR4(sc, PIO_IDR, cfg->iomask & ~cfg->intr);
			WR4(sc, PIO_IER, cfg->iomask & cfg->intr);
		}
		return (0);
	case AT91_GPIO_BANG:
		bang = (struct at91_gpio_bang *)data;
		at91_pio_bang32(sc, bang->bits, bang->datapin, bang->clockpin);
		return (0);
	case AT91_GPIO_BANG_MANY:
		bangmany = (struct at91_gpio_bang_many *)data;
		walker = (uint8_t *)bangmany->bits;
		bitcount = bangmany->numbits;
		while (bitcount > 0) {
			num = MIN((bitcount + 7) / 8, sizeof(many));
			err = copyin(walker, many, num);
			if (err)
				return err;
			for (i = 0; i < num && bitcount > 0; i++, bitcount -= 8) 
				if (bitcount >= 8)
					at91_pio_bang(sc, many[i], 8, bangmany->datapin, bangmany->clockpin);
				else
					at91_pio_bang(sc, many[i], bitcount, bangmany->datapin, bangmany->clockpin);
			walker += num;
		}
		return (0);
	case AT91_GPIO_INFO:	/* Learn about this device's AT91_GPIO bits */
		info = (struct at91_gpio_info *)data;
		info->output_status = RD4(sc, PIO_ODSR);
		info->input_status = RD4(sc, PIO_OSR);
		info->highz_status = RD4(sc, PIO_MDSR);
		info->pullup_status = RD4(sc, PIO_PUSR);
		info->glitch_status = RD4(sc, PIO_IFSR);
		info->enabled_status = RD4(sc, PIO_PSR);
		info->periph_status = RD4(sc, PIO_ABSR);
		info->intr_status = RD4(sc, PIO_IMR);
		memset(info->extra_status, 0, sizeof(info->extra_status));
		return (0);
	}
	return (ENOTTY);
}

/*
 * The following functions are called early in the boot process, so
 * don't use bus_space, as that isn't yet available when we need to use
 * them.
 */

void
at91_pio_use_periph_a(uint32_t pio, uint32_t periph_a_mask, int use_pullup)
{
	uint32_t *PIO = (uint32_t *)(AT91_BASE + pio);

	PIO[PIO_ASR / 4] = periph_a_mask;
	PIO[PIO_PDR / 4] = periph_a_mask;
	if (use_pullup)
		PIO[PIO_PUER / 4] = periph_a_mask;
	else
		PIO[PIO_PUDR / 4] = periph_a_mask;
}

void
at91_pio_use_periph_b(uint32_t pio, uint32_t periph_b_mask, int use_pullup)
{
	uint32_t *PIO = (uint32_t *)(AT91_BASE + pio);

	PIO[PIO_BSR / 4] = periph_b_mask;
	PIO[PIO_PDR / 4] = periph_b_mask;
	if (use_pullup)
		PIO[PIO_PUER / 4] = periph_b_mask;
	else
		PIO[PIO_PUDR / 4] = periph_b_mask;
}

void
at91_pio_use_gpio(uint32_t pio, uint32_t gpio_mask)
{
	uint32_t *PIO = (uint32_t *)(AT91_BASE + pio);

	PIO[PIO_PER / 4] = gpio_mask;
}

void
at91_pio_gpio_input(uint32_t pio, uint32_t input_enable_mask)
{
	uint32_t *PIO = (uint32_t *)(AT91_BASE + pio);

	PIO[PIO_ODR / 4] = input_enable_mask;
}

void
at91_pio_gpio_output(uint32_t pio, uint32_t output_enable_mask, int use_pullup)
{
	uint32_t *PIO = (uint32_t *)(AT91_BASE + pio);

	PIO[PIO_OER / 4] = output_enable_mask;
	if (use_pullup)
		PIO[PIO_PUER / 4] = output_enable_mask;
	else
		PIO[PIO_PUDR / 4] = output_enable_mask;
}

void
at91_pio_gpio_high_z(uint32_t pio, uint32_t high_z_mask, int enable)
{
	uint32_t *PIO = (uint32_t *)(AT91_BASE + pio);

	if (enable)
		PIO[PIO_MDER / 4] = high_z_mask;
	else
		PIO[PIO_MDDR / 4] = high_z_mask;
}

void
at91_pio_gpio_set(uint32_t pio, uint32_t data_mask)
{
	uint32_t *PIO = (uint32_t *)(AT91_BASE + pio);

	PIO[PIO_SODR / 4] = data_mask;
}

void
at91_pio_gpio_clear(uint32_t pio, uint32_t data_mask)
{
	uint32_t *PIO = (uint32_t *)(AT91_BASE + pio);

	PIO[PIO_CODR / 4] = data_mask;
}

uint32_t
at91_pio_gpio_get(uint32_t pio, uint32_t data_mask)
{
	uint32_t *PIO = (uint32_t *)(AT91_BASE + pio);

	return (PIO[PIO_PDSR / 4] & data_mask);
}

void
at91_pio_gpio_set_deglitch(uint32_t pio, uint32_t data_mask, int use_deglitch)
{
	uint32_t *PIO = (uint32_t *)(AT91_BASE + pio);

	if (use_deglitch)
		PIO[PIO_IFER / 4] = data_mask;
	else
		PIO[PIO_IFDR / 4] = data_mask;
}

void
at91_pio_gpio_pullup(uint32_t pio, uint32_t data_mask, int do_pullup)
{
	uint32_t *PIO = (uint32_t *)(AT91_BASE + pio);

	if (do_pullup)
		PIO[PIO_PUER / 4] = data_mask;
	else
		PIO[PIO_PUDR / 4] = data_mask;
}

void
at91_pio_gpio_set_interrupt(uint32_t pio, uint32_t data_mask,
	int enable_interrupt)
{
	uint32_t *PIO = (uint32_t *)(AT91_BASE + pio);

	if (enable_interrupt)
		PIO[PIO_IER / 4] = data_mask;
	else
		PIO[PIO_IDR / 4] = data_mask;
}

uint32_t
at91_pio_gpio_clear_interrupt(uint32_t pio)
{
	uint32_t *PIO = (uint32_t *)(AT91_BASE + pio);

	/* Reading this register will clear the interrupts. */
	return (PIO[PIO_ISR / 4]);
}

static void
at91_pio_new_pass(device_t dev)
{

	device_printf(dev, "Pass %d\n", bus_current_pass);
}

static device_method_t at91_pio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		at91_pio_probe),
	DEVMETHOD(device_attach,	at91_pio_attach),
	DEVMETHOD(device_detach,	at91_pio_detach),

	DEVMETHOD(bus_new_pass,		at91_pio_new_pass),

	DEVMETHOD_END
};

static driver_t at91_pio_driver = {
	"at91_pio",
	at91_pio_methods,
	sizeof(struct at91_pio_softc),
};

#ifdef FDT
EARLY_DRIVER_MODULE(at91_pio, at91_pinctrl, at91_pio_driver, at91_pio_devclass,
    NULL, NULL, BUS_PASS_INTERRUPT);
#else
DRIVER_MODULE(at91_pio, atmelarm, at91_pio_driver, at91_pio_devclass, NULL, NULL);
#endif
