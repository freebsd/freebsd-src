/*-
 * Copyright (c) 2003-2009 RMI Corporation
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
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * RMI_BSD */

#include <sys/cdefs.h>
/*
 * reading eeprom for the mac address .
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/resource.h>

#include <dev/iicbus/iiconf.h>

#include "iicbus_if.h"

#define	IIC_M_WR	0	/* write operation */
#define	XLR_EEPROM_ADDR	0xa0	/* slave address */
#define	XLR_EEPROM_ETH_MAC_ADDR	0x20	


struct xlr_eeprom_softc {
	device_t	sc_dev;
	struct mtx	sc_mtx;
	uint8_t		mac_address[6];
};

static void xlr_eeprom_read_mac(struct xlr_eeprom_softc *);

static int
xlr_eeprom_probe(device_t dev)
{
	/* XXX really probe? */
	device_set_desc(dev, "reading eeprom for mac address");
	return (0);
}

static int
xlr_eeprom_mac_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct xlr_eeprom_softc *sc = arg1;
	int temp ;

	xlr_eeprom_read_mac(sc);
	return sysctl_handle_int(oidp, &temp, 0, req);
}


static int
xlr_eeprom_attach(device_t dev)
{
	struct xlr_eeprom_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(dev);

	sc->sc_dev = dev;

    
	mtx_init(&sc->sc_mtx, "eeprom", "eeprom", MTX_DEF);

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		"eeprom-mac", CTLTYPE_INT | CTLFLAG_RD, sc, 0,
		xlr_eeprom_mac_sysctl, "I", "mac address");

	return (0);
}

static int
xlr_eeprom_read(device_t dev, int reg) 
{
	uint8_t addr = reg;
	uint8_t data[1];
	struct iic_msg msgs[2] = {
	     { XLR_EEPROM_ADDR, IIC_M_WR, 1, &addr },
	     { XLR_EEPROM_ADDR, IIC_M_RD, 1, data },
	};

	return iicbus_transfer(dev, msgs, 2) != 0 ? -1 : data[0];
}


static void
xlr_eeprom_read_mac(struct xlr_eeprom_softc *sc)
{
	int v; 
    int i;

	mtx_lock(&sc->sc_mtx);
    printf("\nmac address is: \n");
    for(i=0; i<6; i++){
		v = xlr_eeprom_read(sc->sc_dev, XLR_EEPROM_ETH_MAC_ADDR+i);
        sc->mac_address[i] = v;
        if(i != 5)
            printf("%x:", sc->mac_address[i]);
        else
            printf("%x\n", sc->mac_address[i]);
    }
	mtx_unlock(&sc->sc_mtx);
}

static device_method_t xlr_eeprom_methods[] = {
	DEVMETHOD(device_probe,		xlr_eeprom_probe),
	DEVMETHOD(device_attach,	xlr_eeprom_attach),

	{0, 0},
};

static driver_t xlr_eeprom_driver = {
	"xlr_eeprom",
	xlr_eeprom_methods,
	sizeof(struct xlr_eeprom_softc),
};
static devclass_t xlr_eeprom_devclass;

DRIVER_MODULE(xlr_eeprom, iicbus, xlr_eeprom_driver, xlr_eeprom_devclass, 0, 0);
MODULE_VERSION(xlr_eeprom, 1);
MODULE_DEPEND(xlr_eeprom, iicbus, 1, 1, 1);
