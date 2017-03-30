/*-
 * Copyright (c) 2016 Andriy Gapon <avg@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <dev/smbus/smbconf.h>
#include <dev/smbus/smbus.h>

#include "smbus_if.h"


/*
 * SMBus specification defines little-endian byte order,
 * but it seems that the JEDEC devices expect it to
 * be big-endian.
 */
static int
ts_readw_be(device_t dev, uint8_t reg, uint16_t *val)
{
	device_t bus = device_get_parent(dev);
	uint8_t addr = smbus_get_addr(dev);
	int err;

	err = smbus_readw(bus, addr, reg, val);
	if (err != 0)
		return (err);
	*val = be16toh(*val);
	return (0);
}

static int
ts_temp_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = arg1;
	int err;
	int temp;
	uint16_t val;

	err = ts_readw_be(dev, 5, &val);
	if (err != 0)
		return (EIO);

	/*
	 * Convert the reading to temperature in 0.0001 Kelvins.
	 * Three most significant bits are flags, the next
	 * most significant bit is a sign bit.
	 * Each step is 0.0625 degrees.
	 */
	temp = val & 0xfff;
	if ((val & 0x1000) != 0)
		temp = -temp;
	temp = temp * 625 + 2731500;
	err = sysctl_handle_int(oidp, &temp, 0, req);
	return (err);
}

static int
ts_probe(device_t dev)
{
	device_set_desc(dev, "DIMM memory sensor");
	return (BUS_PROBE_DEFAULT);
}

static int
ts_attach(device_t dev)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *tree;
	int err;
	uint16_t vendorid;
	uint16_t devid;
	uint8_t addr;

	addr = smbus_get_addr(dev);
	if ((addr & 0xf0) != 0x30) {
		/* Up to 8 slave devices starting at 0x30. */
		return (ENXIO);
	}

	err = ts_readw_be(dev, 6, &vendorid);
	if (err != 0) {
		device_printf(dev, "failed to read Manufacturer ID\n");
		return (ENXIO);
	}
	err = ts_readw_be(dev, 6, &devid);
	if (err != 0) {
		device_printf(dev, "failed to read Device ID\n");
		return (ENXIO);
	}
	if ((devid & 0xff00) == 0x2200) {
		/*
		 * Defined by JEDEC Standard No. 21-C, Release 26,
		 * Page 4.1.6 – 24
		 */
	} else if (vendorid == 0x104a) {
		/*
		 * STMicroelectronics datasheets say that
		 * device ID and revision can vary.
		 * E.g. STT424E02, Doc ID 13448 Rev 8,
		 * section 4.6, page 26.
		 */
	} else {
		if (bootverbose) {
			device_printf(dev, "Unknown Manufacturer and Device IDs"
			    ", 0x%x and 0x%x\n", vendorid, devid);
		}
		return (ENXIO);
	}

	ctx = device_get_sysctl_ctx(dev);
	tree = SYSCTL_CHILDREN(device_get_sysctl_tree(dev));

	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "temp",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, dev, 0,
	    ts_temp_sysctl, "IK4", "Current temperature");

	return (0);
}

static int
ts_detach(device_t dev)
{
	return (0);
}


static device_method_t jedec_ts_methods[] = {
	/* Methods from the device interface */
	DEVMETHOD(device_probe,		ts_probe),
	DEVMETHOD(device_attach,	ts_attach),
	DEVMETHOD(device_detach,	ts_detach),

	/* Terminate method list */
	{ 0, 0 }
};

static driver_t jedec_ts_driver = {
	"jedec_ts",
	jedec_ts_methods,
	0	/* no softc */
};

static devclass_t jedec_ts_devclass;

DRIVER_MODULE(jedec_ts, smbus, jedec_ts_driver, jedec_ts_devclass, 0, 0);
MODULE_DEPEND(jedec_ts, smbus, SMBUS_MINVER, SMBUS_PREFVER, SMBUS_MAXVER);
MODULE_VERSION(jedec_ts, 1);
