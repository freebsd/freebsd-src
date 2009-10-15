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
#include <mips/xlr/board.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/resource.h>

#include <dev/iicbus/iiconf.h>

#include "iicbus_if.h"

#define	IIC_M_WR	0	/* write operation */
#define	XLR_TEMPSENSOR_ADDR	0x98	/* slave address */
#define	XLR_ATX8_TEMPSENSOR_ADDR	0x9a	/* slave address */
#define	XLR_TEMPSENSOR_EXT_TEMP	1	


struct xlr_temperature_softc {
	device_t	sc_dev;
	struct mtx	sc_mtx;
	int		sc_curtemp;
	int		sc_lastupdate;	/* in ticks */
};

static void xlr_temperature_update(struct xlr_temperature_softc *);

static int
xlr_temperature_probe(device_t dev)
{
	/* XXX really probe? */
	device_set_desc(dev, "temperature sensor on XLR");
	return (0);
}

static int
xlr_temperature_sysctl_temp(SYSCTL_HANDLER_ARGS)
{
	struct xlr_temperature_softc *sc = arg1;
	int temp;

	xlr_temperature_update(sc);
	temp = sc->sc_curtemp ;
	return sysctl_handle_int(oidp, &temp, 0, req);
}


static int
xlr_temperature_attach(device_t dev)
{
	struct xlr_temperature_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(dev);

	sc->sc_dev = dev;
	mtx_init(&sc->sc_mtx, "xlr_temperature", "xlr_temperature", MTX_DEF);

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		"temp", CTLTYPE_INT | CTLFLAG_RD, sc, 0,
		xlr_temperature_sysctl_temp, "I", "operating temperature");

	return (0);
}

static int
xlr_temperature_read(device_t dev, int reg) 
{
	uint8_t addr = reg;
	uint8_t data[1];
	struct iic_msg msgs[2] = {
	     { XLR_TEMPSENSOR_ADDR, IIC_M_WR, 1, &addr },
	     { XLR_TEMPSENSOR_ADDR, IIC_M_RD, 1, data },
	};

    if(xlr_boot1_info.board_major_version == RMI_XLR_BOARD_ARIZONA_VIII)
    {
        msgs[0].slave = XLR_ATX8_TEMPSENSOR_ADDR ;
        msgs[1].slave = XLR_ATX8_TEMPSENSOR_ADDR ;
    }

	return iicbus_transfer(dev, msgs, 2) != 0 ? -1 : data[0];
}


static void
xlr_temperature_update(struct xlr_temperature_softc *sc)
{
	int v;

	mtx_lock(&sc->sc_mtx);
	/* NB: no point in updating any faster than the chip */
	if (ticks - sc->sc_lastupdate > hz) {
		v = xlr_temperature_read(sc->sc_dev, XLR_TEMPSENSOR_EXT_TEMP);
		if (v >= 0)
			sc->sc_curtemp = v;
		sc->sc_lastupdate = ticks;
	}
	mtx_unlock(&sc->sc_mtx);
}

static device_method_t xlr_temperature_methods[] = {
	DEVMETHOD(device_probe,		xlr_temperature_probe),
	DEVMETHOD(device_attach,	xlr_temperature_attach),

	{0, 0},
};

static driver_t xlr_temperature_driver = {
	"xlr_temperature",
	xlr_temperature_methods,
	sizeof(struct xlr_temperature_softc),
};
static devclass_t xlr_temperature_devclass;

DRIVER_MODULE(xlr_temperature, iicbus, xlr_temperature_driver, xlr_temperature_devclass, 0, 0);
MODULE_VERSION(xlr_temperature, 1);
MODULE_DEPEND(xlr_temperature, iicbus, 1, 1, 1);
