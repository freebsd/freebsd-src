/*-
 * Copyright (c) 1998 Nicolas Souchu, Marc Bouget
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
 *
 *	$Id: lpbb.c,v 1.5 1999/05/08 21:59:06 dfr Exp $
 *
 */

/*
 * I2C Bit-Banging over parallel port
 *
 * See the Official Philips interface description in lpbb(4)
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>

#include <machine/clock.h>

#include <dev/ppbus/ppbconf.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include "iicbb_if.h"

/* iicbus softc */
struct lpbb_softc {

	struct ppb_device lpbb_dev;
};

static int lpbb_detect(struct lpbb_softc *);

static int lpbb_probe(device_t);
static int lpbb_attach(device_t);

static int lpbb_callback(device_t, int, caddr_t *);
static void lpbb_setlines(device_t, int, int);
static int lpbb_getdataline(device_t);
static int lpbb_reset(device_t, u_char, u_char, u_char *);

static devclass_t lpbb_devclass;

static device_method_t lpbb_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		lpbb_probe),
	DEVMETHOD(device_attach,	lpbb_attach),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),

	/* iicbb interface */
	DEVMETHOD(iicbb_callback,	lpbb_callback),
	DEVMETHOD(iicbb_setlines,	lpbb_setlines),
	DEVMETHOD(iicbb_getdataline,	lpbb_getdataline),
	DEVMETHOD(iicbb_reset,		lpbb_reset),

	{ 0, 0 }
};

static driver_t lpbb_driver = {
	"lpbb",
	lpbb_methods,
	sizeof(struct lpbb_softc),
};

/*
 * Make ourselves visible as a ppbus driver
 */
static struct ppb_device	*lpbb_ppb_probe(struct ppb_data *ppb);
static int			lpbb_ppb_attach(struct ppb_device *dev);

#define MAXLPBB	8			/* XXX not much better! */
static struct lpbb_softc *lpbbdata[MAXLPBB];
static int nlpbb = 0;

#ifdef KERNEL

static struct ppb_driver lpbbdriver = {
    lpbb_ppb_probe, lpbb_ppb_attach, "lpbb"
};
DATA_SET(ppbdriver_set, lpbbdriver);

#endif /* KERNEL */

static int
lpbb_probe(device_t dev)
{
	struct lpbb_softc *sc = lpbbdata[device_get_unit(dev)];
	struct lpbb_softc *scdst = (struct lpbb_softc *)device_get_softc(dev);

	/* XXX copy softc. Yet, ppbus device is sc->lpbb_dev, but will be
	 * dev->parent when ppbus will be ported to the new bus architecture */
	bcopy(sc, scdst, sizeof(struct lpbb_softc));

	device_set_desc(dev, "parallel I2C bit-banging interface");

	/* probe done by ppbus initialization */
	return (0);
}

static int
lpbb_attach(device_t dev)
{
	device_t bitbang, iicbus;
	
	/* add generic bit-banging code */
	bitbang = device_add_child(dev, "iicbb", -1, NULL);

	/* add the iicbus to the tree */
	iicbus = iicbus_alloc_bus(bitbang);

	device_probe_and_attach(bitbang);

	/* XXX should be in iicbb_attach! */
	device_probe_and_attach(iicbus);

	return (0);
}

/*
 * lppbb_ppb_probe()
 */
static struct ppb_device *
lpbb_ppb_probe(struct ppb_data *ppb)
{
	struct lpbb_softc *sc;

	sc = (struct lpbb_softc *) malloc(sizeof(struct lpbb_softc),
							M_TEMP, M_NOWAIT);
	if (!sc) {
		printf("lpbb: cannot malloc!\n");
		return (0);
	}
	bzero(sc, sizeof(struct lpbb_softc));

	lpbbdata[nlpbb] = sc;

	/*
	 * ppbus dependent initialisation.
	 */
	sc->lpbb_dev.id_unit = nlpbb;
	sc->lpbb_dev.name = lpbbdriver.name;
	sc->lpbb_dev.ppb = ppb;
	sc->lpbb_dev.intr = 0;

	if (!lpbb_detect(sc)) {
		free(sc, M_TEMP);
		return (NULL);
	}

	/* Ok, go to next device on next probe */
	nlpbb ++;

	/* XXX wrong according to new bus architecture. ppbus needs to be
	 * ported
	 */
	return (&sc->lpbb_dev);
}

static int
lpbb_ppb_attach(struct ppb_device *dev)
{
	/* add the parallel port I2C interface to the bus tree */
	if (!device_add_child(root_bus, "lpbb", dev->id_unit, NULL))
		return (0);

	return (1);
}

static int
lpbb_callback(device_t dev, int index, caddr_t *data)
{
	struct lpbb_softc *sc = (struct lpbb_softc *)device_get_softc(dev);
	int error = 0;
	int how;

	switch (index) {
	case IIC_REQUEST_BUS:
		/* request the ppbus */
		how = *(int *)data;
		error = ppb_request_bus(&sc->lpbb_dev, how);
		break;

	case IIC_RELEASE_BUS:
		/* release the ppbus */
		error = ppb_release_bus(&sc->lpbb_dev);
		break;

	default:
		error = EINVAL;
	}

	return (error);
}

#define SDA_out 0x80
#define SCL_out 0x08
#define SDA_in  0x80
#define SCL_in  0x08
#define ALIM    0x20
#define I2CKEY  0x50

static int getSDA(struct lpbb_softc *sc)
{
if((ppb_rstr(&sc->lpbb_dev)&SDA_in)==SDA_in)
        return 1;                   
else                                
        return 0;                   
}

static void setSDA(struct lpbb_softc *sc, char val)
{
if(val==0)
        ppb_wdtr(&sc->lpbb_dev, (u_char)SDA_out);
else                            
        ppb_wdtr(&sc->lpbb_dev, (u_char)~SDA_out);
}

static void setSCL(struct lpbb_softc *sc, unsigned char val)
{
if(val==0)
        ppb_wctr(&sc->lpbb_dev, (u_char)(ppb_rctr(&sc->lpbb_dev)&~SCL_out));
else                                               
        ppb_wctr(&sc->lpbb_dev, (u_char)(ppb_rctr(&sc->lpbb_dev)|SCL_out)); 
}

static int lpbb_detect(struct lpbb_softc *sc)
{
	if (ppb_request_bus(&sc->lpbb_dev, PPB_DONTWAIT)) {
		printf("lpbb: can't allocate ppbus\n");
		return (0);
	}

	/* reset bus */
	setSDA(sc, 1);
	setSCL(sc, 1);

	if ((ppb_rstr(&sc->lpbb_dev) & I2CKEY) ||
		((ppb_rstr(&sc->lpbb_dev) & ALIM) != ALIM)) {

		ppb_release_bus(&sc->lpbb_dev);
		return (0);
	}

	ppb_release_bus(&sc->lpbb_dev);

	return (1);
}

static int
lpbb_reset(device_t dev, u_char speed, u_char addr, u_char * oldaddr)
{
	struct lpbb_softc *sc = (struct lpbb_softc *)device_get_softc(dev);

	/* reset bus */
	setSDA(sc, 1);
	setSCL(sc, 1);

	return (IIC_ENOADDR);
}

static void
lpbb_setlines(device_t dev, int ctrl, int data)
{
	struct lpbb_softc *sc = (struct lpbb_softc *)device_get_softc(dev);

	setSCL(sc, ctrl);
	setSDA(sc, data);
}

static int
lpbb_getdataline(device_t dev)
{
	struct lpbb_softc *sc = (struct lpbb_softc *)device_get_softc(dev);

	return (getSDA(sc));
}

DRIVER_MODULE(lpbb, root, lpbb_driver, lpbb_devclass, 0, 0);
