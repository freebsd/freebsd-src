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
 * $FreeBSD: src/sys/dev/ppbus/lpbb.c,v 1.11.2.1 2000/05/24 00:20:57 n_hibma Exp $
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
#include <sys/uio.h>

#include <machine/clock.h>

#include <dev/ppbus/ppbconf.h>
#include "ppbus_if.h"
#include <dev/ppbus/ppbio.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include "iicbb_if.h"

static int lpbb_detect(device_t dev);

static void
lpbb_identify(driver_t *driver, device_t parent)
{

	BUS_ADD_CHILD(parent, 0, "lpbb", 0);
}

static int
lpbb_probe(device_t dev)
{

	/* Perhaps call this during identify instead? */
	if (!lpbb_detect(dev))
		return (ENXIO);

	device_set_desc(dev, "Parallel I2C bit-banging interface");

	return (0);
}

static int
lpbb_attach(device_t dev)
{
	device_t bitbang, iicbus;
	
	/* add generic bit-banging code */
	bitbang = device_add_child(dev, "iicbb", -1);

	/* add the iicbus to the tree */
	iicbus = iicbus_alloc_bus(bitbang);

	device_probe_and_attach(bitbang);

	/* XXX should be in iicbb_attach! */
	device_probe_and_attach(iicbus);

	return (0);
}

static int
lpbb_callback(device_t dev, int index, caddr_t *data)
{
	device_t ppbus = device_get_parent(dev);
	int error = 0;
	int how;

	switch (index) {
	case IIC_REQUEST_BUS:
		/* request the ppbus */
		how = *(int *)data;
		error = ppb_request_bus(ppbus, dev, how);
		break;

	case IIC_RELEASE_BUS:
		/* release the ppbus */
		error = ppb_release_bus(ppbus, dev);
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

static int getSDA(device_t ppbus)
{
	if((ppb_rstr(ppbus)&SDA_in)==SDA_in)
		return 1;                   
	else                                
		return 0;                   
}

static void setSDA(device_t ppbus, char val)
{
	if(val==0)
		ppb_wdtr(ppbus, (u_char)SDA_out);
	else                            
		ppb_wdtr(ppbus, (u_char)~SDA_out);
}

static void setSCL(device_t ppbus, unsigned char val)
{
	if(val==0)
		ppb_wctr(ppbus, (u_char)(ppb_rctr(ppbus)&~SCL_out));
	else                                               
		ppb_wctr(ppbus, (u_char)(ppb_rctr(ppbus)|SCL_out)); 
}

static int lpbb_detect(device_t dev)
{
	device_t ppbus = device_get_parent(dev);

	if (ppb_request_bus(ppbus, dev, PPB_DONTWAIT)) {
		device_printf(dev, "can't allocate ppbus\n");
		return (0);
	}

	/* reset bus */
	setSDA(ppbus, 1);
	setSCL(ppbus, 1);

	if ((ppb_rstr(ppbus) & I2CKEY) ||
		((ppb_rstr(ppbus) & ALIM) != ALIM)) {

		ppb_release_bus(ppbus, dev);
		return (0);
	}

	ppb_release_bus(ppbus, dev);

	return (1);
}

static int
lpbb_reset(device_t dev, u_char speed, u_char addr, u_char * oldaddr)
{
	device_t ppbus = device_get_parent(dev);

	/* reset bus */
	setSDA(ppbus, 1);
	setSCL(ppbus, 1);

	return (IIC_ENOADDR);
}

static void
lpbb_setlines(device_t dev, int ctrl, int data)
{
	device_t ppbus = device_get_parent(dev);

	setSCL(ppbus, ctrl);
	setSDA(ppbus, data);
}

static int
lpbb_getdataline(device_t dev)
{
	device_t ppbus = device_get_parent(dev);

	return (getSDA(ppbus));
}

static devclass_t lpbb_devclass;

static device_method_t lpbb_methods[] = {
	/* device interface */
	DEVMETHOD(device_identify,	lpbb_identify),
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
	1,
};

DRIVER_MODULE(lpbb, ppbus, lpbb_driver, lpbb_devclass, 0, 0);
