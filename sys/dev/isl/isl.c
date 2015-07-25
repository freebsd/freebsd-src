/*-
 * Copyright (c) 2015 Michael Gmelin <freebsd@grem.de>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Driver for intersil I2C ISL29018 Digital Ambient Light Sensor and Proximity
 * Sensor with Interrupt Function, only tested connected over SMBus (ig4iic).
 *
 * Datasheet:
 * http://www.intersil.com/en/products/optoelectronics/ambient-light-and-proximity-sensors/light-to-digital-sensors/ISL29018.html
 * http://www.intersil.com/content/dam/Intersil/documents/isl2/isl29018.pdf
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/event.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/lockmgr.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/poll.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include <dev/smbus/smbconf.h>
#include <dev/smbus/smbus.h>
#include <dev/isl/isl.h>

#include "smbus_if.h"
#include "bus_if.h"
#include "device_if.h"

#define ISL_METHOD_ALS		0x10
#define ISL_METHOD_IR		0x11
#define ISL_METHOD_PROX		0x12
#define ISL_METHOD_RESOLUTION	0x13
#define ISL_METHOD_RANGE	0x14

struct isl_softc {
	device_t	dev;
	int		unit;
	int		addr;

	struct sx	isl_sx;
	struct sysctl_ctx_list *sysctl_ctx;
	struct sysctl_oid *sysctl_tree;
};

/* Returns < 0 on problem. */
static int isl_read_sensor(device_t dev, int addr, uint8_t cmd_mask);

/*
 * Initialize the device
 */
static
int
init_device(device_t dev, int addr, int probe)
{
	static char bl_init[] = { 0x00 };

	device_t bus;
	int error;

	bus = device_get_parent(dev);	/* smbus */

	/*
	 * init procedure: send 0x00 to test ref and cmd reg 1
	 */
	error = smbus_trans(bus, addr, REG_TEST,
			    SMB_TRANS_NOCNT | SMB_TRANS_7BIT,
			    bl_init, sizeof(bl_init), NULL, 0, NULL);
	if (error)
		goto done;

	error = smbus_trans(bus, addr, REG_CMD1,
			    SMB_TRANS_NOCNT | SMB_TRANS_7BIT,
			    bl_init, sizeof(bl_init), NULL, 0, NULL);
	if (error)
		goto done;

	pause("islinit", hz/100);

done:
	if (error)
		device_printf(dev, "Unable to initialize\n");
	return (error);
}

static int isl_probe(device_t);
static int isl_attach(device_t);
static int isl_detach(device_t);

static int isl_sysctl(SYSCTL_HANDLER_ARGS);

static devclass_t isl_devclass;

static device_method_t isl_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		isl_probe),
	DEVMETHOD(device_attach,	isl_attach),
	DEVMETHOD(device_detach,	isl_detach),

	DEVMETHOD_END
};

static driver_t isl_driver = {
	"isl",
	isl_methods,
	sizeof(struct isl_softc),
};

static int
isl_probe(device_t dev)
{
	int unit;
	int addr;
	int error;

	addr = smbus_get_addr(dev);

	/*
	 * 0x44 - isl ambient light sensor on the acer c720.
	 * (other devices might use other ids).
	 */
	if (addr != 0x44)
		return (ENXIO);

	unit = device_get_unit(dev);

	error = init_device(dev, addr, 1);
	if (error)
		return (ENXIO);

	device_set_desc(dev, "ISL Digital Ambient Light Sensor");

	return (BUS_PROBE_VENDOR);
}

static int
isl_attach(device_t dev)
{
	struct isl_softc *sc;
	int unit;
	int addr;
	int use_als;
	int use_ir;
	int use_prox;

	sc = device_get_softc(dev);

	if (!sc)
		return (ENOMEM);

	unit = device_get_unit(dev);
	addr = *((unsigned char*) device_get_ivars(dev));

	if (init_device(dev, addr, 0))
		return (ENXIO);

	sx_init(&sc->isl_sx, "ISL read lock");

	sc->dev = dev;
	sc->unit = unit;
	sc->addr = addr;

	sc->sysctl_ctx = device_get_sysctl_ctx(dev);
	sc->sysctl_tree = device_get_sysctl_tree(dev);

	use_als = isl_read_sensor(dev, addr, CMD1_MASK_ALS_ONCE) >= 0;
	use_ir = isl_read_sensor(dev, addr, CMD1_MASK_IR_ONCE) >= 0;
	use_prox = isl_read_sensor(dev, addr, CMD1_MASK_PROX_ONCE) >= 0;

	if (use_als) {
		SYSCTL_ADD_PROC(sc->sysctl_ctx,
	 		SYSCTL_CHILDREN(sc->sysctl_tree), OID_AUTO,
			    "als", CTLTYPE_INT | CTLFLAG_RD,
			    sc, ISL_METHOD_ALS, isl_sysctl, "I",
			    "Current ALS sensor read-out");
	}

	if (use_ir) {
		SYSCTL_ADD_PROC(sc->sysctl_ctx,
			SYSCTL_CHILDREN(sc->sysctl_tree), OID_AUTO,
			    "ir", CTLTYPE_INT | CTLFLAG_RD,
			    sc, ISL_METHOD_IR, isl_sysctl, "I",
			    "Current IR sensor read-out");
	}

	if (use_prox) {
		SYSCTL_ADD_PROC(sc->sysctl_ctx,
			SYSCTL_CHILDREN(sc->sysctl_tree), OID_AUTO,
			    "prox", CTLTYPE_INT | CTLFLAG_RD,
			    sc, ISL_METHOD_PROX, isl_sysctl, "I",
			    "Current proximity sensor read-out");
	}

	SYSCTL_ADD_PROC(sc->sysctl_ctx,
		SYSCTL_CHILDREN(sc->sysctl_tree), OID_AUTO,
		    "resolution", CTLTYPE_INT | CTLFLAG_RD,
		    sc, ISL_METHOD_RESOLUTION, isl_sysctl, "I",
		    "Current proximity sensor resolution");

	SYSCTL_ADD_PROC(sc->sysctl_ctx,
	SYSCTL_CHILDREN(sc->sysctl_tree), OID_AUTO,
	    "range", CTLTYPE_INT | CTLFLAG_RD,
	    sc, ISL_METHOD_RANGE, isl_sysctl, "I",
	    "Current proximity sensor range");

	return (0);
}

static int
isl_detach(device_t dev)
{
	struct isl_softc *sc;

	sc = device_get_softc(dev);
	sx_destroy(&sc->isl_sx);

	return (0);
}

static int
isl_sysctl(SYSCTL_HANDLER_ARGS)
{
	static int resolutions[] = { 16, 12, 8, 4};
	static int ranges[] = { 1000, 4000, 16000, 64000};

	struct isl_softc *sc;
	device_t bus;
	uint8_t rbyte;
	int arg;
	int resolution;
	int range;

	sc = (struct isl_softc *)oidp->oid_arg1;
	arg = -1;

	sx_xlock(&sc->isl_sx);
	bus = device_get_parent(sc->dev);	/* smbus */
	if (smbus_trans(bus, sc->addr, REG_CMD2,
	    SMB_TRANS_NOCNT | SMB_TRANS_7BIT,
	    NULL, 0, &rbyte, sizeof(rbyte), NULL)) {
		sx_xunlock(&sc->isl_sx);
		return (-1);
	}
	resolution = resolutions[(rbyte & CMD2_MASK_RESOLUTION)
			    >> CMD2_SHIFT_RESOLUTION];
	range = ranges[(rbyte & CMD2_MASK_RANGE) >> CMD2_SHIFT_RANGE];

	switch (oidp->oid_arg2) {
	case ISL_METHOD_ALS:
		arg = (isl_read_sensor(sc->dev, sc->addr,
		    CMD1_MASK_ALS_ONCE) * range) >> resolution;
		break;
	case ISL_METHOD_IR:
		arg = isl_read_sensor(sc->dev, sc->addr,
		    CMD1_MASK_IR_ONCE);
		break;
	case ISL_METHOD_PROX:
		arg = isl_read_sensor(sc->dev, sc->addr,
		    CMD1_MASK_PROX_ONCE);
		break;
	case ISL_METHOD_RESOLUTION:
		arg = (1 << resolution);
		break;
	case ISL_METHOD_RANGE:
		arg = range;
		break;
	}
	sx_xunlock(&sc->isl_sx);

	SYSCTL_OUT(req, &arg, sizeof(arg));
	return (0);
}

static int isl_read_sensor(device_t dev, int addr, uint8_t cmd_mask)
{
	device_t bus;
	uint8_t rbyte;
	uint8_t cmd;
	int ret;

	bus = device_get_parent(dev);	/* smbus */

	if (smbus_trans(bus, addr, REG_CMD1,
	    SMB_TRANS_NOCNT | SMB_TRANS_7BIT,
	    NULL, 0, &rbyte, sizeof(rbyte), NULL)) {
		device_printf(dev,
		    "Couldn't read first byte before issuing command %d\n",
		    cmd_mask);
		return (-1);
	}

	cmd = (rbyte & 0x1f) | cmd_mask;
	if (smbus_trans(bus, addr, REG_CMD1,
	    SMB_TRANS_NOCNT | SMB_TRANS_7BIT,
	    &cmd, sizeof(cmd), NULL, 0, NULL)) {
		device_printf(dev, "Couldn't write command %d\n", cmd_mask);
		return (-1);
	}

	pause("islconv", hz/10);

	if (smbus_trans(bus, addr, REG_DATA1,
	    SMB_TRANS_NOCNT | SMB_TRANS_7BIT,
	    NULL, 0, &rbyte, sizeof(rbyte), NULL)) {
		device_printf(dev,
		    "Couldn't read first byte after command %d\n", cmd_mask);
		return (-1);
	}

	ret = rbyte;
	if (smbus_trans(bus, addr, REG_DATA2,
	    SMB_TRANS_NOCNT | SMB_TRANS_7BIT,
	    NULL, 0, &rbyte, sizeof(rbyte), NULL)) {
		device_printf(dev, "Couldn't read second byte after command %d\n", cmd_mask);
		return (-1);
	}
	ret += rbyte << 8;

	return (ret);
}

DRIVER_MODULE(isl, smbus, isl_driver, isl_devclass, NULL, NULL);
MODULE_DEPEND(isl, smbus, SMBUS_MINVER, SMBUS_PREFVER, SMBUS_MAXVER);
MODULE_VERSION(isl, 1);
