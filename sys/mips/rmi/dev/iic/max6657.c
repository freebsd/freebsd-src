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
__FBSDID("$FreeBSD$");
/*
 * temperature sensor chip sitting on the I2C bus.
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
#include <dev/iicbus/iicbus.h>

#include <mips/rmi/board.h>
#include <mips/rmi/rmi_boot_info.h>
#include "iicbus_if.h"

#define	MAX6657_EXT_TEMP	1	

struct max6657_softc {
	uint32_t	sc_addr;
	device_t	sc_dev;
	int		sc_curtemp;
	int		sc_lastupdate;	/* in ticks */
};

static void max6657_update(struct max6657_softc *);
static int max6657_read(device_t dev, uint32_t addr, int reg) ;

static int
max6657_probe(device_t dev)
{
	device_set_desc(dev, "MAX6657MSA Temperature Sensor");
	return (0);
}

static int
max6657_sysctl_temp(SYSCTL_HANDLER_ARGS)
{
	struct max6657_softc *sc = arg1;
	int temp;

	max6657_update(sc);
	temp = sc->sc_curtemp ;
	return sysctl_handle_int(oidp, &temp, 0, req);
}

static int
max6657_attach(device_t dev)
{
	struct max6657_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(dev);

	if(sc==NULL) {
		printf("max6657_attach device_get_softc failed\n");
		return (0);
	}
	sc->sc_dev = dev;
	sc->sc_addr = iicbus_get_addr(dev);

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		"temp", CTLTYPE_INT | CTLFLAG_RD, sc, 0,
		max6657_sysctl_temp, "I", "operating temperature");

	device_printf(dev, "Chip temperature {%d} Degree Celsius\n",
		max6657_read(sc->sc_dev, sc->sc_addr, MAX6657_EXT_TEMP));

	return (0);
}

static int
max6657_read(device_t dev, uint32_t slave_addr, int reg) 
{
	uint8_t addr = reg;
	uint8_t data[1];
	struct iic_msg msgs[2] = {
	     { slave_addr, IIC_M_WR, 1, &addr },
	     { slave_addr, IIC_M_RD, 1, data },
	};

	return iicbus_transfer(dev, msgs, 2) != 0 ? -1 : data[0];
}


static void
max6657_update(struct max6657_softc *sc)
{
	int v;

	/* NB: no point in updating any faster than the chip */
	if (ticks - sc->sc_lastupdate > hz) {
		v = max6657_read(sc->sc_dev, sc->sc_addr, MAX6657_EXT_TEMP);
		if (v >= 0)
			sc->sc_curtemp = v;
		sc->sc_lastupdate = ticks;
	}
}

static device_method_t max6657_methods[] = {
	DEVMETHOD(device_probe,		max6657_probe),
	DEVMETHOD(device_attach,	max6657_attach),

	{0, 0},
};

static driver_t max6657_driver = {
	"max6657",
	max6657_methods,
	sizeof(struct max6657_softc),
};
static devclass_t max6657_devclass;

DRIVER_MODULE(max6657, iicbus, max6657_driver, max6657_devclass, 0, 0);
MODULE_VERSION(max6657, 1);
MODULE_DEPEND(max6657, iicbus, 1, 1, 1);
