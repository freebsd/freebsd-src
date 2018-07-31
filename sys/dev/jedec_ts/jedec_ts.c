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
 * General device identification notes.
 *
 * The JEDEC TSE2004av specification defines the device ID that all compliant
 * devices should use, but very few do in practice.  Maybe that's because
 * TSE2002av was rather vague about that.
 * Rare examples are IDT TSE2004GB2B0 and Atmel AT30TSE004A, not sure if
 * they are TSE2004av compliant by design or by accident.
 * Also, the specification mandates that PCI SIG manufacturer IDs are to be
 * used, but in practice the JEDEC manufacturer IDs are often used.
 */

const struct ts_dev {
	uint16_t	vendor_id;
	uint8_t		device_id;
	const char	*description;
} known_devices[] = {
	/*
	 * Analog Devices ADT7408.
	 * http://www.analog.com/media/en/technical-documentation/data-sheets/ADT7408.pdf
	 */
	{ 0x11d4, 0x08, "Analog Devices DIMM temperature sensor" },

	/*
	 * Atmel AT30TSE002B, AT30TSE004A.
	 * http://www.atmel.com/images/doc8711.pdf
	 * http://www.atmel.com/images/atmel-8868-dts-at30tse004a-datasheet.pdf
	 * Note how one chip uses the JEDEC Manufacturer ID while the other
	 * uses the PCI SIG one.
	 */
	{ 0x001f, 0x82, "Atmel DIMM temperature sensor" },
	{ 0x1114, 0x22, "Atmel DIMM temperature sensor" },

	/*
	 * Integrated Device Technology (IDT) TS3000B3A, TSE2002B3C,
	 * TSE2004GB2B0 chips and their variants.
	 * http://www.idt.com/sites/default/files/documents/IDT_TSE2002B3C_DST_20100512_120303152056.pdf
	 * http://www.idt.com/sites/default/files/documents/IDT_TS3000B3A_DST_20101129_120303152013.pdf
	 * https://www.idt.com/document/dst/tse2004gb2b0-datasheet
	 */
	{ 0x00b3, 0x29, "IDT DIMM temperature sensor" },
	{ 0x00b3, 0x22, "IDT DIMM temperature sensor" },

	/*
	 * Maxim Integrated MAX6604.
	 * Different document revisions specify different Device IDs.
	 * Document 19-3837; Rev 0; 10/05 has 0x3e00 while
	 * 19-3837; Rev 3; 10/11 has 0x5400.
	 * http://datasheets.maximintegrated.com/en/ds/MAX6604.pdf
	 */
	{ 0x004d, 0x3e, "Maxim Integrated DIMM temperature sensor" },
	{ 0x004d, 0x54, "Maxim Integrated DIMM temperature sensor" },

	/*
	 * Microchip Technology MCP9805, MCP9843, MCP98242, MCP98243
	 * and their variants.
	 * http://ww1.microchip.com/downloads/en/DeviceDoc/21977b.pdf
	 * Microchip Technology EMC1501.
	 * http://ww1.microchip.com/downloads/en/DeviceDoc/00001605A.pdf
	 */
	{ 0x0054, 0x00, "Microchip DIMM temperature sensor" },
	{ 0x0054, 0x20, "Microchip DIMM temperature sensor" },
	{ 0x0054, 0x21, "Microchip DIMM temperature sensor" },
	{ 0x1055, 0x08, "Microchip DIMM temperature sensor" },

	/*
	 * NXP Semiconductors SE97 and SE98.
	 * http://www.nxp.com/docs/en/data-sheet/SE97B.pdf
	 */
	{ 0x1131, 0xa1, "NXP DIMM temperature sensor" },
	{ 0x1131, 0xa2, "NXP DIMM temperature sensor" },

	/*
	 * ON Semiconductor CAT34TS02 revisions B and C, CAT6095 and compatible.
	 * https://www.onsemi.com/pub/Collateral/CAT34TS02-D.PDF
	 * http://www.onsemi.com/pub/Collateral/CAT6095-D.PDF
	 */
	{ 0x1b09, 0x08, "ON Semiconductor DIMM temperature sensor" },
	{ 0x1b09, 0x0a, "ON Semiconductor DIMM temperature sensor" },

	/*
	 * ST[Microelectronics] STTS424E02, STTS2002 and others.
	 * http://www.st.com/resource/en/datasheet/cd00157558.pdf
	 * http://www.st.com/resource/en/datasheet/stts2002.pdf
	 */
	{ 0x104a, 0x00, "ST DIMM temperature sensor" },
	{ 0x104a, 0x03, "ST DIMM temperature sensor" },
};

static const char *
ts_match_device(uint16_t vid, uint16_t did)
{
	const struct ts_dev *d;
	int i;

	for (i = 0; i < nitems(known_devices); i++) {
		d = &known_devices[i];
		if (vid == d->vendor_id && (did >> 8) == d->device_id)
			return (d->description);
	}

	/*
	 * If no match for a specific device, then check
	 * for a generic TSE2004av compliant device.
	 */
	if ((did >> 8) == 0x22)
		return ("TSE2004av compliant DIMM temperature sensor");
	return (NULL);
}

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
	const char *match;
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
	err = ts_readw_be(dev, 7, &devid);
	if (err != 0) {
		device_printf(dev, "failed to read Device ID\n");
		return (ENXIO);
	}

	match = ts_match_device(vendorid, devid);
	if (match == NULL) {
		if (bootverbose) {
			device_printf(dev, "Unknown Manufacturer and Device IDs"
			    ", 0x%x and 0x%x\n", vendorid, devid);
		}
		return (ENXIO);
	}

	device_set_desc(dev, match);
	return (BUS_PROBE_DEFAULT);
}

static int
ts_attach(device_t dev)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *tree;

	ctx = device_get_sysctl_ctx(dev);
	tree = SYSCTL_CHILDREN(device_get_sysctl_tree(dev));
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "temp",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, dev, 0,
	    ts_temp_sysctl, "IK4", "Current temperature");

	gone_in_dev(dev, 12,
	    "jedec_ts(4) driver; see COMPATIBILITY section of jedec_dimm(4)");

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
