/*-
 * Copyright (c) 2000  Nick Sayer
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

/* spic -- the Sony Programmable I/O Controller
 *
 * This device exists on most recent Sony laptops. It is the means by which
 * you can watch the Jog Dial and some other functions.
 *
 * At the moment, this driver merely tries to turn the jog dial into a
 * device that moused can park on, with the intent of supplying a Z axis
 * and mouse button out of the jog dial. I suspect that this device will
 * end up having to support at least 2 different minor devices: One to be
 * the jog wheel device for moused to camp out on and the other to perform
 * all of the other miscellaneous functions of this device. But for now,
 * the jog wheel is all you get.
 *
 * At the moment, the data sent back by the device is rather primitive.
 * It sends a single character per event:
 * u = up, d = down -- that's the jog button
 * l = left, r = right -- that's the dial.
 * "left" and "right" are rather capricious. They actually represent
 * ccw and cw, respectively
 *
 * What documentation exists is thanks to Andrew Tridge, and his page at
 * http://samba.org/picturebook/ Special thanks also to Ian Dowse, who
 * also provided sample code upon which this driver was based.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <isa/isavar.h>
#include <sys/poll.h>
#include <machine/pci_cfgreg.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <i386/isa/spicreg.h>

static int spic_pollrate;

SYSCTL_INT(_machdep, OID_AUTO, spic_pollrate, CTLFLAG_RW, &spic_pollrate, 0, "")
;

devclass_t spic_devclass;

static d_open_t		spicopen;
static d_close_t	spicclose;
static d_read_t		spicread;
static d_ioctl_t	spicioctl;
static d_poll_t		spicpoll;

static struct cdevsw spic_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	spicopen,
	.d_close =	spicclose,
	.d_read =	spicread,
	.d_ioctl =	spicioctl,
	.d_poll =	spicpoll,
	.d_name =	"spic",
};

#define SCBUFLEN 128

struct spic_softc {
	u_int sc_port_addr;
	u_char sc_intr;
	struct resource *sc_port_res,*sc_intr_res;
	int	sc_port_rid,sc_intr_rid;
	int sc_opened;
	int sc_sleeping;
	int sc_buttonlast;
	struct callout sc_timeout;
	struct mtx sc_lock;
	device_t sc_dev;
	struct cdev *sc_cdev;
	struct selinfo sc_rsel;
	u_char sc_buf[SCBUFLEN];
	int sc_count;
	int sc_model;
};

static void
write_port1(struct spic_softc *sc, u_char val)
{
	DELAY(10);
	outb(sc->sc_port_addr, val);
}

static void
write_port2(struct spic_softc *sc, u_char val)
{
	DELAY(10);
	outb(sc->sc_port_addr + 4, val);
}

static u_char
read_port1(struct spic_softc *sc)
{
	DELAY(10);
	return inb(sc->sc_port_addr);
}

static u_char
read_port2(struct spic_softc *sc)
{
	DELAY(10);
	return inb(sc->sc_port_addr + 4);
}

static u_char
read_port_cst(struct spic_softc *sc)
{
	DELAY(10);
	return inb(SPIC_CST_IOPORT);
}

static void
busy_wait(struct spic_softc *sc)
{
	int i=0;

	while(read_port2(sc) & 2) {
		DELAY(10);
		if (i++>10000) {
			printf("spic busy wait abort\n");
			return;
		}
	}
}

static void
busy_wait_cst(struct spic_softc *sc, int mask)
{
	int i=0;

	while(read_port_cst(sc) & mask) {
		DELAY(10);
		if (i++>10000) {
			printf("spic busy wait abort\n");
			return;
		}
	}
}

static u_char
spic_call1(struct spic_softc *sc, u_char dev) {
	busy_wait(sc);
	write_port2(sc, dev);
	read_port2(sc);
	return read_port1(sc);
}

static u_char
spic_call2(struct spic_softc *sc, u_char dev, u_char fn)
{
	busy_wait(sc);
	write_port2(sc, dev);
	busy_wait(sc);
	write_port1(sc, fn);
	return read_port1(sc);
}

static void
spic_ecrset(struct spic_softc *sc, u_int16_t addr, u_int16_t value)
{
	busy_wait_cst(sc, 3);
	outb(SPIC_CST_IOPORT, 0x81);
	busy_wait_cst(sc, 2);
	outb(SPIC_DATA_IOPORT, addr);
	busy_wait_cst(sc, 2);
	outb(SPIC_DATA_IOPORT, value);
	busy_wait_cst(sc, 2);
}

static void
spic_type2_srs(struct spic_softc *sc)
{
	spic_ecrset(sc, SPIC_SHIB, (sc->sc_port_addr & 0xFF00) >> 8);
	spic_ecrset(sc, SPIC_SLOB,  sc->sc_port_addr & 0x00FF);
	spic_ecrset(sc, SPIC_SIRQ,  0x00); /* using polling mode (IRQ=0)*/
	DELAY(10);
}

static int
spic_probe(device_t dev)
{
	struct spic_softc *sc;
	u_char t, spic_irq;

	sc = device_get_softc(dev);

	/*
	 * We can only have 1 of these. Attempting to probe for a unit 1
	 * will destroy the work we did for unit 0
	 */
	if (device_get_unit(dev))
		return ENXIO;

	bzero(sc, sizeof(struct spic_softc));

	if (!(sc->sc_port_res = bus_alloc_resource_anywhere(dev, SYS_RES_IOPORT,
		&sc->sc_port_rid, 5, RF_ACTIVE))) {
		device_printf(dev,"Couldn't map I/O\n");
		return ENXIO;
	}
	sc->sc_port_addr = (u_short)rman_get_start(sc->sc_port_res);

#ifdef notyet
	if (!(sc->sc_intr_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
		&sc->sc_intr_rid, RF_ACTIVE))) {
		device_printf(dev,"Couldn't map IRQ\n");
		bus_release_resource(dev, SYS_RES_IOPORT,
			sc->sc_port_rid, sc->sc_port_res);
		return ENXIO;
	}
	sc->sc_intr = (u_short)rman_get_start(sc->sc_intr_res);

	switch (sc->sc_intr) {
		case 0: spic_irq = 3; break;
		case 5: spic_irq = 0; break;
		case 0xa: spic_irq = 1; break;
		case 0xb: spic_irq = 2; break;
		default: device_printf(dev,"Invalid IRQ\n");
			bus_release_resource(dev, SYS_RES_IOPORT,
				sc->sc_port_rid, sc->sc_port_res);
			bus_release_resource(dev, SYS_RES_IRQ,
				sc->sc_intr_rid, sc->sc_intr_res);
			return ENXIO;
	}
#else
	spic_irq = 3;
#endif

#if 0
	if (sc->sc_port_addr != 0x10A0) {
		bus_release_resource(dev, SYS_RES_IOPORT,
			sc->sc_port_rid, sc->sc_port_res);
		bus_release_resource(dev, SYS_RES_IRQ,
			sc->sc_intr_rid, sc->sc_intr_res);
		return ENXIO;
	}
#endif

	/* PIIX4 chipset at least? */
	if (pci_cfgregread(PIIX4_BUS, PIIX4_SLOT, PIIX4_FUNC, 0, 4) ==
		PIIX4_DEVID) {
		sc->sc_model = SPIC_DEVICE_MODEL_TYPE1;
	} else {
		/* For newer VAIOs (R505, SRX7, ...) */
		sc->sc_model = SPIC_DEVICE_MODEL_TYPE2;
	}

	/*
	 * This is an ugly hack. It is necessary until ACPI works correctly.
	 *
	 * The SPIC consists of 2 registers. They are mapped onto I/O by the
	 * PIIX4's General Device 10 function. There is also an interrupt
	 * control port at a somewhat magic location, but this first pass is
	 * polled.
	 *
	 * So the first thing we need to do is map the G10 space in.
	 *
	 */

	/* Enable ACPI mode to get Fn key events */
	/* XXX This may slow down your VAIO if ACPI is not supported in the kernel.
	outb(0xb2, 0xf0);
	*/

	device_printf(dev,"device model type = %d\n", sc->sc_model);
	
	if(sc->sc_model == SPIC_DEVICE_MODEL_TYPE1) {
		pci_cfgregwrite(PIIX4_BUS, PIIX4_SLOT, PIIX4_FUNC, G10A,
				sc->sc_port_addr, 2);
		t = pci_cfgregread(PIIX4_BUS, PIIX4_SLOT, PIIX4_FUNC, G10L, 1);
		t &= 0xf0;
		t |= 4;
		pci_cfgregwrite(PIIX4_BUS, PIIX4_SLOT, PIIX4_FUNC, G10L, t, 1);
		outw(SPIC_IRQ_PORT, (inw(SPIC_IRQ_PORT) & ~(0x3 << SPIC_IRQ_SHIFT)) | (spic_irq << SPIC_IRQ_SHIFT));
		t = pci_cfgregread(PIIX4_BUS, PIIX4_SLOT, PIIX4_FUNC, G10L, 1);
		t &= 0x1f;
		t |= 0xc0;
		pci_cfgregwrite(PIIX4_BUS, PIIX4_SLOT, PIIX4_FUNC, G10L, t, 1);
	} else {
		spic_type2_srs(sc);
	}

	/*
	 * XXX: Should try and see if there's anything actually there.
	 */

	device_set_desc(dev, "Sony Programmable I/O Controller");

	return 0;
}

static int
spic_attach(device_t dev)
{
	struct spic_softc *sc;

	sc = device_get_softc(dev);

	sc->sc_dev = dev;
	mtx_init(&sc->sc_lock, "spic", NULL, MTX_DEF);
	callout_init_mtx(&sc->sc_timeout, &sc->sc_lock, 0);
	
	spic_pollrate = (hz/50); /* Every 50th of a second */

	spic_call1(sc, 0x82);
	spic_call2(sc, 0x81, 0xff);
	spic_call1(sc, 0x92);

	/* There can be only one */
	sc->sc_cdev = make_dev(&spic_cdevsw, 0, 0, 0, 0600, "jogdial");
	sc->sc_cdev->si_drv1 = sc;

	return 0;
}

static void
spictimeout(void *arg)
{
	struct spic_softc *sc = arg;
	u_char b, event, param;
	int j;

	mtx_assert(&sc->sc_lock, MA_OWNED);
	if (!sc->sc_opened) {
		device_printf(sc->sc_dev, "timeout called while closed!\n");
		return;
	}

	event = read_port2(sc);
	param = read_port1(sc);

	if ((event != 4) && (!(event & 0x1)))
		switch(event) {
			case 0x10: /* jog wheel event (type1) */
				if (sc->sc_model == SPIC_DEVICE_MODEL_TYPE1) {
					b = !!(param & 0x40);
					if (b != sc->sc_buttonlast) {
						sc->sc_buttonlast = b;
						sc->sc_buf[sc->sc_count++] =
							b?'d':'u';
					}
					j = (param & 0xf) | ((param & 0x10)? ~0xf:0);
					if (j<0)
						while(j++!=0) {
							sc->sc_buf[sc->sc_count++] =
								'l';
						}
					else if (j>0)
						while(j--!=0) {
							sc->sc_buf[sc->sc_count++] =
								'r';
						}
				}
				break;
			case 0x08: /* jog wheel event (type2) */
			case 0x00: 
				/* SPIC_DEVICE_MODEL_TYPE2 returns jog wheel event=0x00 */
				if (sc->sc_model == SPIC_DEVICE_MODEL_TYPE2) {
					b = !!(param & 0x40);
					if (b != sc->sc_buttonlast) {
						sc->sc_buttonlast = b;
						sc->sc_buf[sc->sc_count++] =
							b?'d':'u';
					}
					j = (param & 0xf) | ((param & 0x10)? ~0xf:0);
					if (j<0)
						while(j++!=0) {
							sc->sc_buf[sc->sc_count++] =
								'l';
						}
					else if (j>0)
						while(j--!=0) {
							sc->sc_buf[sc->sc_count++] =
								'r';
						}
				}
				break;
			case 0x60: /* Capture button */
				printf("Capture button event: %x\n",param);
				break;
			case 0x30: /* Lid switch */
				printf("Lid switch event: %x\n",param);
				break;
			case 0x0c: /* We must ignore these type of event for C1VP... */
			case 0x70: /* Closing/Opening the lid on C1VP */
				break;
			default:
				printf("Unknown event: event %02x param %02x\n", event, param);
				break;
		}
	else {
		/* No event. Wait some more */
		callout_reset(&sc->sc_timeout, spic_pollrate, spictimeout, sc);
		return;
	}

	if (sc->sc_count) {
		if (sc->sc_sleeping) {
			sc->sc_sleeping = 0;
			wakeup( sc);
		}
		selwakeuppri(&sc->sc_rsel, PZERO);
	}
	spic_call2(sc, 0x81, 0xff); /* Clear event */

	callout_reset(&sc->sc_timeout, spic_pollrate, spictimeout, sc);
}

static int
spicopen(struct cdev *dev, int flag, int fmt, struct thread *td)
{
	struct spic_softc *sc;

	sc = dev->si_drv1;

	mtx_lock(&sc->sc_lock);
	if (sc->sc_opened) {
		mtx_unlock(&sc->sc_lock);
		return (EBUSY);
	}

	sc->sc_opened++;
	sc->sc_count=0;

	/* Start the polling */
	callout_reset(&sc->sc_timeout, spic_pollrate, spictimeout, sc);
	mtx_unlock(&sc->sc_lock);
	return (0);
}

static int
spicclose(struct cdev *dev, int flag, int fmt, struct thread *td)
{
	struct spic_softc *sc;

	sc = dev->si_drv1;
	mtx_lock(&sc->sc_lock);

	/* Stop polling */
	callout_stop(&sc->sc_timeout);
	sc->sc_opened = 0;
	mtx_unlock(&sc->sc_lock);
	return 0;
}

static int
spicread(struct cdev *dev, struct uio *uio, int flag)
{
	struct spic_softc *sc;
	int l, error;
	u_char buf[SCBUFLEN];

	sc = dev->si_drv1;

	if (uio->uio_resid <= 0) /* What kind of a read is this?! */
		return (0);

	mtx_lock(&sc->sc_lock);
	while (!(sc->sc_count)) {
		sc->sc_sleeping=1;
		error = mtx_sleep(sc, &sc->sc_lock, PZERO | PCATCH, "jogrea", 0);
		sc->sc_sleeping=0;
		if (error) {
			mtx_unlock(&sc->sc_lock);
			return (error);
		}
	}

	l = min(uio->uio_resid, sc->sc_count);
	bcopy(sc->sc_buf, buf, l);
	sc->sc_count -= l;
	bcopy(sc->sc_buf + l, sc->sc_buf, l);
	mtx_unlock(&sc->sc_lock);
	return (uiomove(buf, l, uio));
}

static int
spicioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag, struct thread *td)
{
	struct spic_softc *sc;

	sc = dev->si_drv1;

	return (EIO);
}

static int
spicpoll(struct cdev *dev, int events, struct thread *td)
{
	struct spic_softc *sc;
	int revents = 0;

	sc = dev->si_drv1;
	mtx_lock(&sc->sc_lock);
	if (events & (POLLIN | POLLRDNORM)) {
		if (sc->sc_count)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(td, &sc->sc_rsel); /* Who shall we wake? */
	}
	mtx_unlock(&sc->sc_lock);

	return (revents);
}


static device_method_t spic_methods[] = {
	DEVMETHOD(device_probe,		spic_probe),
	DEVMETHOD(device_attach,	spic_attach),

	{ 0, 0 }
};

static driver_t spic_driver = {
	"spic",
	spic_methods,
	sizeof(struct spic_softc),
};

DRIVER_MODULE(spic, isa, spic_driver, spic_devclass, 0, 0);

