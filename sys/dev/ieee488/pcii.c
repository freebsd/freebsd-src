/*-
 * Copyright (c) 2005 Poul-Henning Kamp <phk@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Supported hardware:
 *    PCIIA compatible cards.
 *
 *    Tested and known working:
 *	"B&C Microsystems PC488A-0"
 *
 * A whole lot of wonderful things could be written for GPIB, but for now
 * I have just written it such that it is possible to capture data in the
 * mode known as "unaddressed listen only mode".  This is what many plotters
 * and printers do on GPIB.  This is enough to capture some output from
 * various test instruments.
 *
 * If you are interested in working on this, send me email.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/mutex.h>
#include <sys/uio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

/* ---> upd7210.h at some point. */

struct upd7210 {
	bus_space_handle_t	reg_handle[8];
	bus_space_tag_t		reg_tag[8];
	u_int			reg_offset[8];

	/* private stuff */
	struct mtx		mutex;
	uint8_t			rreg[8];
	uint8_t			wreg[8];

	int			busy;
	u_char			*buf;
	size_t			bufsize;
	u_int			buf_wp;
	u_int			buf_rp;
	struct cdev		*cdev;
};

static void upd7210intr(void *);
static void upd7210attach(struct upd7210 *);


/* ----> pcii.c */

struct pcii_softc {
	int foo;
	struct resource	*port[8];
	struct resource	*irq;
	void *intr_handler;
	struct upd7210	upd7210;
};

#define HERE() printf("pcii HERE %s:%d\n", __FILE__, __LINE__)

static devclass_t pcii_devclass;

static int	pcii_probe(device_t dev);
static int	pcii_attach(device_t dev);

static device_method_t pcii_methods[] = {
	DEVMETHOD(device_probe,		pcii_probe),
	DEVMETHOD(device_attach,	pcii_attach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	{ 0, 0 }
};

static driver_t pcii_driver = {
	"pcii",
	pcii_methods,
	sizeof(struct pcii_softc *),
};

static int
pcii_probe(device_t dev)
{
	struct resource	*port;
	int rid;
	u_long start, count;
	int i, j, error = 0;

	device_set_desc(dev, "PCII IEEE-4888 controller");

	rid = 0;
	if (bus_get_resource(dev, SYS_RES_IOPORT, rid, &start, &count) != 0)
		return ENXIO;
	if ((start & 0x3ff) != 0x2e1)
		return (ENXIO);
	count = 1;
	if (bus_set_resource(dev, SYS_RES_IOPORT, rid, start, count) != 0)
		return ENXIO;
	for (i = 0; i < 8; i++) {
		j = bus_set_resource(dev, SYS_RES_IOPORT, i,
		    start + 0x400 * i, 1);
		if (j) {
			error = ENXIO;
			break;
		}
		rid = i;
		port = bus_alloc_resource_any(dev, SYS_RES_IOPORT,
		    &rid, RF_ACTIVE);
		if (port == NULL)
			return (ENXIO);
		else
			bus_release_resource(dev, SYS_RES_IOPORT, i, port);
	}

	rid = 0;
	port = bus_alloc_resource_any(dev,
	    SYS_RES_IRQ, &rid, RF_SHAREABLE | RF_ACTIVE);
	if (port == NULL)
		return (ENXIO);
	bus_release_resource(dev, SYS_RES_IRQ, rid, port);
					   
	return (error);
}

static int
pcii_attach(device_t dev)
{
	struct pcii_softc *sc;
	int		unit;
	int		rid;
	int i, error = 0;

	unit = device_get_unit(dev);
	sc = device_get_softc(dev);
	memset(sc, 0, sizeof *sc);

	device_set_desc(dev, "PCII IEEE-4888 controller");

	for (rid = 0; rid < 8; rid++) {
		sc->port[rid] = bus_alloc_resource_any(dev,
		    SYS_RES_IOPORT, &rid, RF_ACTIVE);
		if (sc->port[rid] == NULL) {
			error = ENXIO;
			break;
		}
		sc->upd7210.reg_tag[rid] = rman_get_bustag(sc->port[rid]);
		sc->upd7210.reg_handle[rid] = rman_get_bushandle(sc->port[rid]);
	}
	if (!error) {
		rid = 0;
		sc->irq = bus_alloc_resource_any(dev,
		    SYS_RES_IRQ, &rid, RF_SHAREABLE | RF_ACTIVE);
		if (sc->irq == NULL) {
			error = ENXIO;
		} else {
			error = bus_setup_intr(dev, sc->irq,
			    INTR_TYPE_MISC | INTR_MPSAFE,
			    upd7210intr, &sc->upd7210, &sc->intr_handler);
		}
	}
	if (error) {
device_printf(dev, "error = %d\n", error);
		for (i = 0; i < 8; i++) {
			if (sc->port[i] == NULL)
				break;
			bus_release_resource(dev, SYS_RES_IOPORT,
			    0, sc->port[i]);
		}
		if (sc->intr_handler != NULL)
			bus_teardown_intr(dev, sc->irq, sc->intr_handler);
		if (sc->irq != NULL)
			bus_release_resource(dev, SYS_RES_IRQ, i, sc->irq);
	}
	upd7210attach(&sc->upd7210);
	return (error);
}

DRIVER_MODULE(pcii, isa, pcii_driver, pcii_devclass, 0, 0);
DRIVER_MODULE(pcii, acpi, pcii_driver, pcii_devclass, 0, 0);

/* ---> upd7210.c at some point */

enum upd7210_wreg {
	CDOR	= 0,	/* Command/data out	*/
	IMR1	= 1,	/* Interrupt mask 1	*/
	IMR2	= 2,	/* Interrupt mask 2	*/
	SPMR	= 3,	/* Serial poll mode	*/
	ADMR	= 4,	/* Address mode		*/
	AUXMR	= 5,	/* Auxilliary mode	*/
	ADR	= 6,	/* Address		*/
	EOSR	= 7,	/* End-of-string	*/
};

enum upd7210_rreg {
	DIR	= 0,	/* Data in		*/
	ISR1	= 1,	/* Interrupt status 1	*/
	ISR2	= 2,	/* Interrupt status 2	*/
	SPSR	= 3,	/* Serial poll status	*/
	ADSR	= 4,	/* Address status	*/
	CPTR	= 5,	/* Command pass though	*/
	ADR0	= 6,	/* Address 1		*/
	ADR1	= 7,	/* Address 2		*/
};

#define AUXMR_PON        0x00
#define AUXMR_CRST       0x02
#define AUXMR_RFD        0x03
#define AUXMR_SEOI       0x06
#define AUXMR_GTS        0x10
#define AUXMR_TCA        0x11
#define AUXMR_TCS        0x12
#define AUXMR_TCSE       0x1a
#define AUXMR_DSC        0x14
#define AUXMR_CIFC       0x16
#define AUXMR_SIFC       0x1e
#define AUXMR_CREN       0x17
#define AUXMR_SREN       0x1f
#define AUXMR_ICTR       0x20
#define AUXMR_PPR        0x60
#define AUXMR_RA         0x80
#define AUXMR_RB         0xa0
#define AUXMR_RE         0xc0


/* upd7210 generic stuff */

static u_int
read_reg(struct upd7210 *u, enum upd7210_rreg reg)
{
	u_int r;

	r = bus_space_read_1(
	    u->reg_tag[reg],
	    u->reg_handle[reg],
	    u->reg_offset[reg]);
	u->rreg[reg] = r;
	return (r);
}

static void
write_reg(struct upd7210 *u, enum upd7210_wreg reg, u_int val)
{
	bus_space_write_1(
	    u->reg_tag[reg],
	    u->reg_handle[reg],
	    u->reg_offset[reg], val);
	u->wreg[reg] = val;
}

static void
upd7210intr(void *arg)
{
	int i;
	u_int isr1, isr2;
	struct upd7210 *u;

	u = arg;
	mtx_lock(&u->mutex);
	isr1 = read_reg(u, ISR1);
	isr2 = read_reg(u, ISR2);
	if (isr1 & 1) {
		i = read_reg(u, DIR);
		u->buf[u->buf_wp++] = i;
		u->buf_wp &= (u->bufsize - 1);
		i = (u->buf_rp + u->bufsize - u->buf_wp) & (u->bufsize - 1);
		if (i < 8)
			write_reg(u, IMR1, 0);
		wakeup(u->buf);
	} else {
		printf("upd7210intr [%02x %02x %02x",
		    read_reg(u, DIR), isr1, isr2);
		printf(" %02x %02x %02x %02x %02x]\n",
		    read_reg(u, SPSR),
		    read_reg(u, ADSR),
		    read_reg(u, CPTR),
		    read_reg(u, ADR0),
		    read_reg(u, ADR1));
	}
	mtx_unlock(&u->mutex);
}

static int
gpib_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct upd7210 *u;

	u = dev->si_drv1;

	mtx_lock(&u->mutex);
	if (u->busy)
		return (EBUSY);
	u->busy = 1;
	mtx_unlock(&u->mutex);

	u->buf = malloc(PAGE_SIZE, M_DEVBUF, M_WAITOK);
	u->bufsize = PAGE_SIZE;
	u->buf_wp = 0;
	u->buf_rp = 0;

	write_reg(u, AUXMR, AUXMR_CRST);
	DELAY(10000);
	write_reg(u, AUXMR, AUXMR_ICTR | 8);
	DELAY(1000);
	write_reg(u, ADR, 0x60);
	write_reg(u, ADR, 0xe0);
	write_reg(u, ADMR, 0x70);
	write_reg(u, AUXMR, AUXMR_PON);
	write_reg(u, IMR1, 0x01);
	return (0);
}

static int
gpib_close(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct upd7210 *u;

	u = dev->si_drv1;

	mtx_lock(&u->mutex);
	u->busy = 0;
	write_reg(u, AUXMR, AUXMR_CRST);
	DELAY(10000);
	write_reg(u, IMR1, 0x00);
	write_reg(u, IMR2, 0x00);
	free(u->buf, M_DEVBUF);
	u->buf = NULL;
	mtx_unlock(&u->mutex);
	return (0);
}

static int
gpib_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct upd7210 *u;
	int error;
	size_t z;

	u = dev->si_drv1;
	error = 0;

	mtx_lock(&u->mutex);
	while (u->buf_wp == u->buf_rp) {
		error = msleep(u->buf, &u->mutex, PZERO | PCATCH,
		    "gpibrd", hz);
		if (error && error != EWOULDBLOCK) {
			mtx_unlock(&u->mutex);
			return (error);
		}
	}
	while (uio->uio_resid > 0 && u->buf_wp != u->buf_rp) {
		if (u->buf_wp < u->buf_rp)
			z = u->bufsize - u->buf_rp;
		else
			z = u->buf_wp - u->buf_rp;
		if (z > uio->uio_resid)
			z = uio->uio_resid;
		mtx_unlock(&u->mutex);
		error = uiomove(u->buf + u->buf_rp, z, uio);
		mtx_lock(&u->mutex);
		if (error)
			break;
		u->buf_rp += z;
		u->buf_rp &= (u->bufsize - 1);
	}
	if (u->wreg[IMR1] == 0)
		write_reg(u, IMR1, 0x01);
	mtx_unlock(&u->mutex);
	return (error);
}



struct cdevsw gpib_cdevsw = {
	.d_version =	D_VERSION,
	.d_name =	"gpib",
	.d_open	=	gpib_open,
	.d_close =	gpib_close,
	.d_read =	gpib_read,
};

static void
upd7210attach(struct upd7210 *u)
{
	int unit = 0;

	mtx_init(&u->mutex, "gpib", NULL, MTX_DEF);
	u->cdev = make_dev(&gpib_cdevsw, unit,
	    UID_ROOT, GID_WHEEL, 0444,
	    "gpib%ul", unit);
	u->cdev->si_drv1 = u;
}
