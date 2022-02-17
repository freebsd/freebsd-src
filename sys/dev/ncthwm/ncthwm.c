/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016-2022 Stormshield
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/watchdog.h>

#include <dev/superio/superio.h>

#include <machine/bus.h>
#include <machine/resource.h>

#define NCTHWM_FAN_MAX                 5

#define NCTHWM_BANK_SELECT 0x4e
#define NCTHWM_VENDOR_ID   0x4f

#define NCTHWM_VERBOSE_PRINTF(dev, ...)         \
	do {                                        \
		if (__predict_false(bootverbose))       \
			device_printf(dev, __VA_ARGS__);    \
	} while (0)

struct ncthwm_softc {
	device_t              dev;
	struct ncthwm_device *nctdevp;
	struct resource      *iores;
	int                   iorid;
};

struct ncthwm_fan_info
{
	const char *name;
	uint8_t     low_byte_offset;
	uint8_t     high_byte_offset;
};

struct ncthwm_device {
	uint16_t                 devid;
	const char              *descr;
	uint8_t                  base_offset;
	uint8_t                  fan_bank;
	uint8_t                  fan_count;
	struct ncthwm_fan_info   fan_info[NCTHWM_FAN_MAX];
} ncthwm_devices[] = {
	{
		.devid       = 0xc562,
		.descr       = "HWM on Nuvoton NCT6779D",
		.base_offset = 5,
		.fan_bank    = 4,
		.fan_count   = 5,
		.fan_info = {
			{ .name = "SYSFAN",  .low_byte_offset = 0xc1, .high_byte_offset = 0xc0 },
			{ .name = "CPUFAN",  .low_byte_offset = 0xc3, .high_byte_offset = 0xc2 },
			{ .name = "AUXFAN0", .low_byte_offset = 0xc5, .high_byte_offset = 0xc4 },
			{ .name = "AUXFAN1", .low_byte_offset = 0xc7, .high_byte_offset = 0xc6 },
			{ .name = "AUXFAN2", .low_byte_offset = 0xc9, .high_byte_offset = 0xc8 },
		},
	}, {
		.devid       = 0xd42a,
		.descr       = "HWM on Nuvoton NCT6796D-E",
		.base_offset = 5,
		.fan_bank    = 4,
		.fan_count   = 5,
		.fan_info = {
			{ .name = "SYSFAN",  .low_byte_offset = 0xc1, .high_byte_offset = 0xc0 },
			{ .name = "CPUFAN",  .low_byte_offset = 0xc3, .high_byte_offset = 0xc2 },
			{ .name = "AUXFAN0", .low_byte_offset = 0xc5, .high_byte_offset = 0xc4 },
			{ .name = "AUXFAN1", .low_byte_offset = 0xc7, .high_byte_offset = 0xc6 },
			{ .name = "AUXFAN2", .low_byte_offset = 0xc9, .high_byte_offset = 0xc8 },
		},
	}
};

static struct ncthwm_device *
ncthwm_lookup_device(device_t dev)
{
	int      i;
	uint16_t devid;

	devid = superio_devid(dev);
	for (i = 0; i < nitems(ncthwm_devices); i++) {
		if (devid == ncthwm_devices[i].devid)
			return (ncthwm_devices + i);
	}
	return (NULL);
}

static void
ncthwm_write(struct ncthwm_softc *sc, uint8_t reg, uint8_t val)
{
	bus_write_1(sc->iores, 0, reg);
	bus_write_1(sc->iores, 1, val);
}

static uint8_t
ncthwm_read(struct ncthwm_softc *sc, uint8_t reg)
{
	bus_write_1(sc->iores, 0, reg);
	return (bus_read_1(sc->iores, 1));
}

static int
ncthwm_query_fan_speed(SYSCTL_HANDLER_ARGS)
{
	struct ncthwm_softc    *sc;
	struct ncthwm_fan_info *fan;
	uint16_t                val;

	sc = arg1;
	if (sc == NULL)
		return (EINVAL);

	KASSERT(sc->nctdevp != NULL, ("Unreachable"));

	if (sc->nctdevp->fan_count <= arg2)
		return (EINVAL);
	fan = &sc->nctdevp->fan_info[arg2];

	KASSERT(sc->iores != NULL, ("Unreachable"));

	ncthwm_write(sc, NCTHWM_BANK_SELECT, sc->nctdevp->fan_bank);
	val  = ncthwm_read(sc, fan->high_byte_offset) << 8;
	val |= ncthwm_read(sc, fan->low_byte_offset);

	NCTHWM_VERBOSE_PRINTF(sc->dev, "%s: read %u from bank %u offset 0x%x-0x%x\n",
		fan->name, val, sc->nctdevp->fan_bank, fan->high_byte_offset, fan->low_byte_offset);

	return (sysctl_handle_int(oidp, &val, 0, req));
}

static int
ncthwm_probe(device_t dev)
{
	struct ncthwm_device *nctdevp;
	uint8_t               ldn;

	ldn = superio_get_ldn(dev);

	if (superio_vendor(dev) != SUPERIO_VENDOR_NUVOTON) {
		NCTHWM_VERBOSE_PRINTF(dev, "ldn 0x%x not a Nuvoton device\n", ldn);
		return (ENXIO);
	}
	if (superio_get_type(dev) != SUPERIO_DEV_HWM) {
		NCTHWM_VERBOSE_PRINTF(dev, "ldn 0x%x not a HWM device\n", ldn);
		return (ENXIO);
	}

	nctdevp = ncthwm_lookup_device(dev);
	if (nctdevp == NULL) {
		NCTHWM_VERBOSE_PRINTF(dev, "ldn 0x%x not supported\n", ldn);
		return (ENXIO);
	}
	device_set_desc(dev, nctdevp->descr);
	return (BUS_PROBE_DEFAULT);
}

static int
ncthwm_attach(device_t dev)
{
	struct ncthwm_softc *sc;
	int                  i;
	uint16_t             iobase;

	sc      = device_get_softc(dev);
	sc->dev = dev;

	sc->nctdevp = ncthwm_lookup_device(dev);
	if (sc->nctdevp == NULL) {
		device_printf(dev, "device not supported\n");
		return (ENXIO);
	}

	iobase    = superio_get_iobase(dev) + sc->nctdevp->base_offset;
	sc->iorid = 0;
	if (bus_set_resource(dev, SYS_RES_IOPORT, sc->iorid, iobase, 2) != 0) {
		device_printf(dev, "failed to set I/O port resource at 0x%x\n", iobase);
		return (ENXIO);
	}
	sc->iores = bus_alloc_resource_any(dev, SYS_RES_IOPORT,
		&sc->iorid, RF_ACTIVE);
	if (sc->iores == NULL) {
		device_printf(dev, "can't map I/O space at 0x%x\n", iobase);
		return (ENXIO);
	}
	NCTHWM_VERBOSE_PRINTF(dev, "iobase 0x%x iores %p\n", iobase, sc->iores);

	/* Register FAN sysctl */
	for (i = 0; i < sc->nctdevp->fan_count; i++) {
		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
			sc->nctdevp->fan_info[i].name,
			CTLTYPE_U16 | CTLFLAG_RD, sc, i,
			ncthwm_query_fan_speed, "SU", "Fan speed in RPM");
	}

	return (0);
}

static int
ncthwm_detach(device_t dev)
{
	struct ncthwm_softc *sc = device_get_softc(dev);

	if (sc->iores)
		bus_release_resource(dev, SYS_RES_IOPORT, sc->iorid, sc->iores);

	return (0);
}

static device_method_t ncthwm_methods[] = {
	/* Methods from the device interface */
	DEVMETHOD(device_probe,		ncthwm_probe),
	DEVMETHOD(device_attach,	ncthwm_attach),
	DEVMETHOD(device_detach,	ncthwm_detach),

	/* Terminate method list */
	{ 0, 0 }
};

static driver_t ncthwm_driver = {
	"ncthwm",
	ncthwm_methods,
	sizeof (struct ncthwm_softc)
};

DRIVER_MODULE(ncthwm, superio, ncthwm_driver, NULL, NULL);
MODULE_DEPEND(ncthwm, superio, 1, 1, 1);
MODULE_VERSION(ncthwm, 1);
