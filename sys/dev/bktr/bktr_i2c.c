/*-
 * Copyright (c) 1998 Nicolas Souchu
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
 * $FreeBSD$
 *
 */

/*
 * I2C support for the bti2c chipset.
 *
 * From brooktree848.c <fsmp@freefall.org>
 */

#include "bktr.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/uio.h>
#include <sys/select.h>


#if (__FreeBSD_version >=300000)
#include <machine/bus_memio.h>          /* for bus space */
#include <machine/bus.h>
#include <sys/bus.h>
#endif

#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <machine/ioctl_meteor.h>
#include <machine/ioctl_bt848.h>	/* extensions to ioctl_meteor.h */
#include <dev/bktr/bktr_reg.h>

#include <dev/bktr/bktr_i2c.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include <dev/smbus/smbconf.h>

#include "iicbb_if.h"
#include "smbus_if.h"



#define I2C_DELAY	40

#define BTI2C_DEBUG(x)	if (bti2c_debug) (x)
static int bti2c_debug = 0;

struct bti2c_softc {

	bus_space_tag_t     memt;   /* Bus space register access */
	bus_space_handle_t  memh;   /* Bus space register access */

	int iic_owned;			/* 1 if we own the iicbus */
	int smb_owned;			/* 1 if we own the smbbus */

	device_t smbus;
	device_t iicbus;
};

struct bt_data {
	bus_space_tag_t     memt;
	bus_space_handle_t  memh; 
};
struct bt_data btdata[NBKTR];

static int bti2c_probe(device_t);
static int bti2c_attach(device_t);

static int bti2c_iic_callback(device_t, int, caddr_t *);
static void bti2c_iic_setlines(device_t, int, int);
static int bti2c_iic_getdataline(device_t);
static int bti2c_iic_reset(device_t, u_char, u_char, u_char *);

static int bti2c_smb_callback(device_t, int, caddr_t *);
static int bti2c_smb_writeb(device_t dev, u_char slave, char cmd, char byte);
static int bti2c_smb_writew(device_t dev, u_char slave, char cmd, short word);
static int bti2c_smb_readb(device_t dev, u_char slave, char cmd, char *byte);

static devclass_t bti2c_devclass;

static device_method_t bti2c_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		bti2c_probe),
	DEVMETHOD(device_attach,	bti2c_attach),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),

	/* iicbb interface */
	DEVMETHOD(iicbb_callback,	bti2c_iic_callback),
	DEVMETHOD(iicbb_setlines,	bti2c_iic_setlines),
	DEVMETHOD(iicbb_getdataline,	bti2c_iic_getdataline),
	DEVMETHOD(iicbb_reset,		bti2c_iic_reset),
	
	/* smbus interface */
	DEVMETHOD(smbus_callback,	bti2c_smb_callback),
	DEVMETHOD(smbus_writeb,		bti2c_smb_writeb),
	DEVMETHOD(smbus_writew,		bti2c_smb_writew),
	DEVMETHOD(smbus_readb,		bti2c_smb_readb),
	
	{ 0, 0 }
};

#if (__FreeBSD_version < 400000)
/* FreeBSD 3.x needs DRIVER_TYPE_MISC */
static driver_t bti2c_driver = {
	"bti2c",
	bti2c_methods,
	DRIVER_TYPE_MISC,
	sizeof(struct bti2c_softc),
};
#endif

#if (__FreeBSD_version >=400000)
	static driver_t bti2c_driver = {
	"bti2c",
	bti2c_methods,
	sizeof(struct bti2c_softc),
};      
#endif

/*
 * Call this to pass the address of the bktr device to the
 * bti2c_i2c layer and initialize all the I2C bus architecture
 */
int
bt848_i2c_attach(int unit, struct bktr_softc * bktr, struct bktr_i2c_softc *i2c_sc)
{
	device_t interface;
	device_t bitbang;

	btdata[unit].memh = bktr->memh;
	btdata[unit].memt = bktr->memt;

	/* XXX add the I2C interface to the root_bus until pcibus is ready */
#if (__FreeBSD_version < 400000)
	interface = device_add_child(root_bus, "bti2c", unit, NULL);
#else
	interface = device_add_child(root_bus, "bti2c", unit);
#endif

	/* add bit-banging generic code onto bti2c interface */
#if (__FreeBSD_version < 400000)
	bitbang = device_add_child(interface, "iicbb", -1, NULL);
#else
	bitbang = device_add_child(interface, "iicbb", -1);
#endif

	/* probe and attach the interface, we need it NOW
	 * bit-banging code is also probed and attached */
	device_probe_and_attach(interface);
	device_probe_and_attach(bitbang);

	/* smb and i2c interfaces are available for the bt848 chip
	 * connect bit-banging generic code to an iicbus */
	if ((i2c_sc->iicbus = iicbus_alloc_bus(bitbang)))
		device_probe_and_attach(i2c_sc->iicbus);

	/* hardware i2c is actually smb over the bti2c interface */
	if ((i2c_sc->smbus = smbus_alloc_bus(interface)))
		device_probe_and_attach(i2c_sc->smbus);

	return (0);
};

/*
 * Not a real probe, we know the device exists since the device has
 * been added after the successfull pci probe.
 */
static int
bti2c_probe(device_t dev)
{
	device_set_desc(dev, "bt848 Hard/Soft I2C controller");

	return (0);
}

static int
bti2c_attach(device_t dev)
{
	struct bti2c_softc *sc = (struct bti2c_softc *)device_get_softc(dev);

	/* XXX should use ivars with pcibus or pcibus methods to access
	 * onboard memory */
	sc->memh = btdata[device_get_unit(dev)].memh;
	sc->memt = btdata[device_get_unit(dev)].memt;

	return (0);
}

static int
bti2c_smb_callback(device_t dev, int index, caddr_t *data)
{
	struct bti2c_softc *sc = (struct bti2c_softc *)device_get_softc(dev);
	int error = 0;
	int how;

	/* test each time if we already have/haven't the iicbus
	 * to avoid deadlocks
	 */
	switch (index) {
	case SMB_REQUEST_BUS:
		if (!sc->iic_owned) {
			/* request the iicbus */
			how = *(int *)data;
			error = iicbus_request_bus(sc->iicbus, dev, how);
			if (!error)
				sc->iic_owned = 1;
		}
		break;

	case SMB_RELEASE_BUS:
		if (sc->iic_owned) {
			/* release the iicbus */
			error = iicbus_release_bus(sc->iicbus, dev);
			if (!error)
				sc->iic_owned = 0;
		}
		break;

	default:
		error = EINVAL;
	}

	return (error);
}

static int
bti2c_iic_callback(device_t dev, int index, caddr_t *data)
{
	struct bti2c_softc *sc = (struct bti2c_softc *)device_get_softc(dev);
	int error = 0;
	int how;

	/* test each time if we already have/haven't the smbus
	 * to avoid deadlocks
	 */
	switch (index) {
	case IIC_REQUEST_BUS:
		if (!sc->smb_owned) {
			/* request the smbus */
			how = *(int *)data;
			error = smbus_request_bus(sc->smbus, dev, how);
			if (!error)
				sc->smb_owned = 1;
		}
		break;

	case IIC_RELEASE_BUS:
		if (sc->smb_owned) {
			/* release the smbus */
			error = smbus_release_bus(sc->smbus, dev);
			if (!error)
				sc->smb_owned = 0;
		}
		break;

	default:
		error = EINVAL;
	}

	return (error);
}

static int
bti2c_iic_reset(device_t dev, u_char speed, u_char addr, u_char * oldaddr)
{
	if (oldaddr)
		*oldaddr = 0;			/* XXX */

	return (IIC_ENOADDR);
}

static void
bti2c_iic_setlines(device_t dev, int ctrl, int data)
{
	struct bti2c_softc *sc = (struct bti2c_softc *)device_get_softc(dev);

	OUTL(sc, BKTR_I2C_DATA_CTL, (ctrl << 1) | data);;
	DELAY(I2C_DELAY);

	return;
}

static int
bti2c_iic_getdataline(device_t dev)
{
	struct bti2c_softc *sc = (struct bti2c_softc *)device_get_softc(dev);

	return ( INL(sc,BKTR_I2C_DATA_CTL) & 0x1);
}

static int
bti2c_write(struct bti2c_softc* bti2c_sc, u_long data)
{
	u_long		x;

	/* clear status bits */
	OUTL(bti2c_sc, BKTR_INT_STAT, (BT848_INT_RACK | BT848_INT_I2CDONE));

	BTI2C_DEBUG(printf("w%lx", data));

	/* write the address and data */
	OUTL(bti2c_sc, BKTR_I2C_DATA_CTL, data);

	/* wait for completion */
	for ( x = 0x7fffffff; x; --x ) {	/* safety valve */
		if ( INL(bti2c_sc, BKTR_INT_STAT) & BT848_INT_I2CDONE )
			break;
	}

	/* check for ACK */
	if ( !x || !( INL(bti2c_sc, BKTR_INT_STAT) & BT848_INT_RACK) ) {
		BTI2C_DEBUG(printf("%c%c", (!x)?'+':'-',
			(!( INL(bti2c_sc, BKTR_INT_STAT) & BT848_INT_RACK))?'+':'-'));
		return (SMB_ENOACK);
	}
	BTI2C_DEBUG(printf("+"));

	/* return OK */
	return( 0 );
}

static int
bti2c_smb_writeb(device_t dev, u_char slave, char cmd, char byte)
{
	struct bti2c_softc *sc = (struct bti2c_softc *)device_get_softc(dev);
	u_long data;

	data = ((slave & 0xff) << 24) | ((byte & 0xff) << 16) | (u_char)cmd;

	return (bti2c_write(sc, data));
}

/*
 * byte1 becomes low byte of word
 * byte2 becomes high byte of word
 */
static int
bti2c_smb_writew(device_t dev, u_char slave, char cmd, short word)
{
	struct bti2c_softc *sc = (struct bti2c_softc *)device_get_softc(dev);
	u_long data;
	char low, high;

	low = (char)(word & 0xff);
	high = (char)((word & 0xff00) >> 8);

	data = ((slave & 0xff) << 24) | ((low & 0xff) << 16) |
		((high & 0xff) << 8) | BT848_DATA_CTL_I2CW3B | (u_char)cmd;

	return (bti2c_write(sc, data));
}

/*
 * The Bt878 and Bt879 differed on the treatment of i2c commands
 */
static int
bti2c_smb_readb(device_t dev, u_char slave, char cmd, char *byte)
{
	struct bti2c_softc *sc = (struct bti2c_softc *)device_get_softc(dev);
	u_long		x;

	/* clear status bits */
	OUTL(sc,BKTR_INT_STAT, (BT848_INT_RACK | BT848_INT_I2CDONE));

	OUTL(sc,BKTR_I2C_DATA_CTL, ((slave & 0xff) << 24) | (u_char)cmd);;

	BTI2C_DEBUG(printf("r%lx/", (u_long)(((slave & 0xff) << 24) | (u_char)cmd)));

	/* wait for completion */
	for ( x = 0x7fffffff; x; --x ) {	/* safety valve */
		if ( INL(sc,BKTR_INT_STAT) & BT848_INT_I2CDONE )
			break;
	}

	/* check for ACK */
	if ( !x || !(INL(sc,BKTR_INT_STAT) & BT848_INT_RACK) ) {
		BTI2C_DEBUG(printf("r%c%c", (!x)?'+':'-',
			(!( INL(sc,BKTR_INT_STAT) & BT848_INT_RACK))?'+':'-'));
		return (SMB_ENOACK);
	}

	*byte = (char)((INL(sc,BKTR_I2C_DATA_CTL) >> 8) & 0xff);
	BTI2C_DEBUG(printf("r%x+", *byte));

	return (0);
}

DRIVER_MODULE(bti2c, root, bti2c_driver, bti2c_devclass, 0, 0);
