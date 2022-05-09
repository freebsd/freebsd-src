/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Andriy Gapon
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/sbuf.h>
#include <sys/time.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/stdarg.h>

#include <isa/isavar.h>

#include <dev/superio/superio.h>
#include <dev/superio/superio_io.h>

#include "isa_if.h"

typedef void (*sio_conf_enter_f)(struct resource*, uint16_t);
typedef void (*sio_conf_exit_f)(struct resource*, uint16_t);

struct sio_conf_methods {
	sio_conf_enter_f	enter;
	sio_conf_exit_f		exit;
	superio_vendor_t	vendor;
};

struct sio_device {
	uint8_t			ldn;
	superio_dev_type_t	type;
};

struct superio_devinfo {
	STAILQ_ENTRY(superio_devinfo) link;
	struct resource_list	resources;
	device_t		dev;
	uint8_t			ldn;
	superio_dev_type_t	type;
	uint16_t		iobase;
	uint16_t		iobase2;
	uint8_t			irq;
	uint8_t			dma;
};

struct siosc {
	struct mtx			conf_lock;
	STAILQ_HEAD(, superio_devinfo)	devlist;
	struct resource*		io_res;
	struct cdev			*chardev;
	int				io_rid;
	uint16_t			io_port;
	const struct sio_conf_methods	*methods;
	const struct sio_device		*known_devices;
	superio_vendor_t		vendor;
	uint16_t			devid;
	uint8_t				revid;
	uint8_t				current_ldn;
	uint8_t				ldn_reg;
	uint8_t				enable_reg;
};

static	d_ioctl_t	superio_ioctl;

static struct cdevsw superio_cdevsw = {
	.d_version =	D_VERSION,
	.d_ioctl =	superio_ioctl,
	.d_name =	"superio",
};

#define NUMPORTS	2

static uint8_t
sio_read(struct resource* res, uint8_t reg)
{
	bus_write_1(res, 0, reg);
	return (bus_read_1(res, 1));
}

/* Read a word from two one-byte registers, big endian. */
static uint16_t
sio_readw(struct resource* res, uint8_t reg)
{
	uint16_t v;

	v = sio_read(res, reg);
	v <<= 8;
	v |= sio_read(res, reg + 1);
	return (v);
}

static void
sio_write(struct resource* res, uint8_t reg, uint8_t val)
{
	bus_write_1(res, 0, reg);
	bus_write_1(res, 1, val);
}

static void
sio_ldn_select(struct siosc *sc, uint8_t ldn)
{
	mtx_assert(&sc->conf_lock, MA_OWNED);
	if (ldn == sc->current_ldn)
		return;
	sio_write(sc->io_res, sc->ldn_reg, ldn);
	sc->current_ldn = ldn;
}

static uint8_t
sio_ldn_read(struct siosc *sc, uint8_t ldn, uint8_t reg)
{
	mtx_assert(&sc->conf_lock, MA_OWNED);
	if (reg >= sc->enable_reg) {
		sio_ldn_select(sc, ldn);
		KASSERT(sc->current_ldn == ldn, ("sio_ldn_select failed"));
	}
	return (sio_read(sc->io_res, reg));
}

static uint16_t
sio_ldn_readw(struct siosc *sc, uint8_t ldn, uint8_t reg)
{
	mtx_assert(&sc->conf_lock, MA_OWNED);
	if (reg >= sc->enable_reg) {
		sio_ldn_select(sc, ldn);
		KASSERT(sc->current_ldn == ldn, ("sio_ldn_select failed"));
	}
	return (sio_readw(sc->io_res, reg));
}

static void
sio_ldn_write(struct siosc *sc, uint8_t ldn, uint8_t reg, uint8_t val)
{
	mtx_assert(&sc->conf_lock, MA_OWNED);
	if (reg <= sc->ldn_reg) {
		printf("ignored attempt to write special register 0x%x\n", reg);
		return;
	}
	sio_ldn_select(sc, ldn);
	KASSERT(sc->current_ldn == ldn, ("sio_ldn_select failed"));
	sio_write(sc->io_res, reg, val);
}

static void
sio_conf_enter(struct siosc *sc)
{
	mtx_lock(&sc->conf_lock);
	sc->methods->enter(sc->io_res, sc->io_port);
}

static void
sio_conf_exit(struct siosc *sc)
{
	sc->methods->exit(sc->io_res, sc->io_port);
	sc->current_ldn = 0xff;
	mtx_unlock(&sc->conf_lock);
}

static void
ite_conf_enter(struct resource* res, uint16_t port)
{
	bus_write_1(res, 0, 0x87);
	bus_write_1(res, 0, 0x01);
	bus_write_1(res, 0, 0x55);
	bus_write_1(res, 0, port == 0x2e ? 0x55 : 0xaa);
}

static void
ite_conf_exit(struct resource* res, uint16_t port)
{
	sio_write(res, 0x02, 0x02);
}

static const struct sio_conf_methods ite_conf_methods = {
	.enter = ite_conf_enter,
	.exit = ite_conf_exit,
	.vendor = SUPERIO_VENDOR_ITE
};

static void
nvt_conf_enter(struct resource* res, uint16_t port)
{
	bus_write_1(res, 0, 0x87);
	bus_write_1(res, 0, 0x87);
}

static void
nvt_conf_exit(struct resource* res, uint16_t port)
{
	bus_write_1(res, 0, 0xaa);
}

static const struct sio_conf_methods nvt_conf_methods = {
	.enter = nvt_conf_enter,
	.exit = nvt_conf_exit,
	.vendor = SUPERIO_VENDOR_NUVOTON
};

static void
fintek_conf_enter(struct resource* res, uint16_t port)
{
	bus_write_1(res, 0, 0x87);
	bus_write_1(res, 0, 0x87);
}

static void
fintek_conf_exit(struct resource* res, uint16_t port)
{
	bus_write_1(res, 0, 0xaa);
}

static const struct sio_conf_methods fintek_conf_methods = {
	.enter = fintek_conf_enter,
	.exit = fintek_conf_exit,
	.vendor = SUPERIO_VENDOR_FINTEK
};

static const struct sio_conf_methods * const methods_table[] = {
	&ite_conf_methods,
	&nvt_conf_methods,
	&fintek_conf_methods,
	NULL
};

static const uint16_t ports_table[] = {
	0x2e, 0x4e, 0
};

const struct sio_device ite_devices[] = {
	{ .ldn = 4, .type = SUPERIO_DEV_HWM },
	{ .ldn = 7, .type = SUPERIO_DEV_WDT },
	{ .type = SUPERIO_DEV_NONE },
};

const struct sio_device nvt_devices[] = {
	{ .ldn = 8, .type = SUPERIO_DEV_WDT },
	{ .type = SUPERIO_DEV_NONE },
};

const struct sio_device nct5104_devices[] = {
	{ .ldn = 7, .type = SUPERIO_DEV_GPIO },
	{ .ldn = 8, .type = SUPERIO_DEV_WDT },
	{ .ldn = 15, .type = SUPERIO_DEV_GPIO },
	{ .type = SUPERIO_DEV_NONE },
};

const struct sio_device fintek_devices[] = {
	{ .ldn = 7, .type = SUPERIO_DEV_WDT },
	{ .type = SUPERIO_DEV_NONE },
};

static const struct {
	superio_vendor_t	vendor;
	uint16_t		devid;
	uint16_t		mask;
	const char		*descr;
	const struct sio_device	*devices;
} superio_table[] = {
	{
		.vendor = SUPERIO_VENDOR_ITE, .devid = 0x8712,
		.devices = ite_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_ITE, .devid = 0x8716,
		.devices = ite_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_ITE, .devid = 0x8718,
		.devices = ite_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_ITE, .devid = 0x8720,
		.devices = ite_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_ITE, .devid = 0x8721,
		.devices = ite_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_ITE, .devid = 0x8726,
		.devices = ite_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_ITE, .devid = 0x8728,
		.devices = ite_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_ITE, .devid = 0x8771,
		.devices = ite_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0x1061, .mask = 0x00,
		.descr	= "Nuvoton NCT5104D/NCT6102D/NCT6106D (rev. A)",
		.devices = nct5104_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0x5200, .mask = 0xff,
		.descr = "Winbond 83627HF/F/HG/G",
		.devices = nvt_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0x5900, .mask = 0xff,
		.descr = "Winbond 83627S",
		.devices = nvt_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0x6000, .mask = 0xff,
		.descr = "Winbond 83697HF",
		.devices = nvt_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0x6800, .mask = 0xff,
		.descr = "Winbond 83697UG",
		.devices = nvt_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0x7000, .mask = 0xff,
		.descr = "Winbond 83637HF",
		.devices = nvt_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0x8200, .mask = 0xff,
		.descr = "Winbond 83627THF",
		.devices = nvt_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0x8500, .mask = 0xff,
		.descr = "Winbond 83687THF",
		.devices = nvt_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0x8800, .mask = 0xff,
		.descr = "Winbond 83627EHF",
		.devices = nvt_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0xa000, .mask = 0xff,
		.descr = "Winbond 83627DHG",
		.devices = nvt_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0xa200, .mask = 0xff,
		.descr = "Winbond 83627UHG",
		.devices = nvt_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0xa500, .mask = 0xff,
		.descr = "Winbond 83667HG",
		.devices = nvt_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0xb000, .mask = 0xff,
		.descr = "Winbond 83627DHG-P",
		.devices = nvt_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0xb300, .mask = 0xff,
		.descr = "Winbond 83667HG-B",
		.devices = nvt_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0xb400, .mask = 0xff,
		.descr = "Nuvoton NCT6775",
		.devices = nvt_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0xc300, .mask = 0xff,
		.descr = "Nuvoton NCT6776",
		.devices = nvt_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0xc400, .mask = 0xff,
		.descr = "Nuvoton NCT5104D/NCT6102D/NCT6106D (rev. B+)",
		.devices = nct5104_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0xc500, .mask = 0xff,
		.descr = "Nuvoton NCT6779",
		.devices = nvt_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0xc800, .mask = 0xff,
		.descr = "Nuvoton NCT6791",
		.devices = nvt_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0xc900, .mask = 0xff,
		.descr = "Nuvoton NCT6792",
		.devices = nvt_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0xd100, .mask = 0xff,
		.descr = "Nuvoton NCT6793",
		.devices = nvt_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_NUVOTON, .devid = 0xd300, .mask = 0xff,
		.descr = "Nuvoton NCT6795",
		.devices = nvt_devices,
	},
	{
		.vendor = SUPERIO_VENDOR_FINTEK, .devid = 0x1210, .mask = 0xff,
		.descr = "Fintek F81803",
		.devices = fintek_devices,
	},
	{ 0, 0 }
};

static const char *
devtype_to_str(superio_dev_type_t type)
{
	switch (type) {
	case SUPERIO_DEV_NONE:
		return ("none");
	case SUPERIO_DEV_HWM:
		return ("HWM");
	case SUPERIO_DEV_WDT:
		return ("WDT");
	case SUPERIO_DEV_GPIO:
		return ("GPIO");
	case SUPERIO_DEV_MAX:
		return ("invalid");
	}
	return ("invalid");
}

static int
superio_detect(device_t dev, bool claim, struct siosc *sc)
{
	struct resource *res;
	rman_res_t port;
	rman_res_t count;
	uint16_t devid;
	uint8_t revid;
	int error;
	int rid;
	int i, m;

	error = bus_get_resource(dev, SYS_RES_IOPORT, 0, &port, &count);
	if (error != 0)
		return (error);
	if (port > UINT16_MAX || count < NUMPORTS) {
		device_printf(dev, "unexpected I/O range size\n");
		return (ENXIO);
	}

	/*
	 * Make a temporary resource reservation for hardware probing.
	 * If we can't get the resources we need then
	 * we need to abort.  Possibly this indicates
	 * the resources were used by another device
	 * in which case the probe would have failed anyhow.
	 */
	rid = 0;
	res = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &rid, RF_ACTIVE);
	if (res == NULL) {
		if (claim)
			device_printf(dev, "failed to allocate I/O resource\n");
		return (ENXIO);
	}

	for (m = 0; methods_table[m] != NULL; m++) {
		methods_table[m]->enter(res, port);
		if (methods_table[m]->vendor == SUPERIO_VENDOR_ITE) {
			devid = sio_readw(res, 0x20);
			revid = sio_read(res, 0x22);
		} else if (methods_table[m]->vendor == SUPERIO_VENDOR_NUVOTON) {
			devid = sio_read(res, 0x20);
			revid = sio_read(res, 0x21);
			devid = (devid << 8) | revid;
		} else if (methods_table[m]->vendor == SUPERIO_VENDOR_FINTEK) {
			devid = sio_read(res, 0x20);
			revid = sio_read(res, 0x21);
			devid = (devid << 8) | revid;
		} else {
			continue;
		}
		methods_table[m]->exit(res, port);
		for (i = 0; superio_table[i].vendor != 0; i++) {
			uint16_t mask;

			mask = superio_table[i].mask;
			if (superio_table[i].vendor !=
			    methods_table[m]->vendor)
				continue;
			if ((superio_table[i].devid & ~mask) != (devid & ~mask))
				continue;
			break;
		}

		/* Found a matching SuperIO entry. */
		if (superio_table[i].vendor != 0)
			break;
	}

	if (methods_table[m] == NULL)
		error = ENXIO;
	else
		error = 0;
	if (!claim || error != 0) {
		bus_release_resource(dev, SYS_RES_IOPORT, rid, res);
		return (error);
	}

	sc->methods = methods_table[m];
	sc->vendor = sc->methods->vendor;
	sc->known_devices = superio_table[i].devices;
	sc->io_res = res;
	sc->io_rid = rid;
	sc->io_port = port;
	sc->devid = devid;
	sc->revid = revid;

	KASSERT(sc->vendor == SUPERIO_VENDOR_ITE ||
	    sc->vendor == SUPERIO_VENDOR_NUVOTON,
	    ("Only ITE and Nuvoton SuperIO-s are supported"));
	sc->ldn_reg = 0x07;
	sc->enable_reg = 0x30;
	sc->current_ldn = 0xff;	/* no device should have this */

	if (superio_table[i].descr != NULL) {
		device_set_desc(dev, superio_table[i].descr);
	} else if (sc->vendor == SUPERIO_VENDOR_ITE) {
		char descr[64];

		snprintf(descr, sizeof(descr),
		    "ITE IT%4x SuperIO (revision 0x%02x)",
		    sc->devid, sc->revid);
		device_set_desc_copy(dev, descr);
	}
	return (0);
}

static void
superio_identify(driver_t *driver, device_t parent)
{
	device_t	child;
	int i;

	/*
	 * Don't create child devices if any already exist.
	 * Those could be created via isa hints or if this
	 * driver is loaded, unloaded and then loaded again.
	 */
	if (device_find_child(parent, "superio", -1)) {
		if (bootverbose)
			printf("superio: device(s) already created\n");
		return;
	}

	/*
	 * Create a child for each candidate port.
	 * It would be nice if we could somehow clean up those
	 * that this driver fails to probe.
	 */
	for (i = 0; ports_table[i] != 0; i++) {
		child = BUS_ADD_CHILD(parent, ISA_ORDER_SPECULATIVE,
		    "superio", -1);
		if (child == NULL) {
			device_printf(parent, "failed to add superio child\n");
			continue;
		}
		bus_set_resource(child, SYS_RES_IOPORT,	0, ports_table[i], 2);
		if (superio_detect(child, false, NULL) != 0)
			device_delete_child(parent, child);
	}
}

static int
superio_probe(device_t dev)
{
	struct siosc *sc;
	int error;

	/* Make sure we do not claim some ISA PNP device. */
	if (isa_get_logicalid(dev) != 0)
		return (ENXIO);

	/*
	 * XXX We can populate the softc now only because we return
	 * BUS_PROBE_SPECIFIC
	 */
	sc = device_get_softc(dev);
	error = superio_detect(dev, true, sc);
	if (error != 0)
		return (error);
	return (BUS_PROBE_SPECIFIC);
}

static void
superio_add_known_child(device_t dev, superio_dev_type_t type, uint8_t ldn)
{
	struct siosc *sc = device_get_softc(dev);
	struct superio_devinfo *dinfo;
	device_t child;

	child = BUS_ADD_CHILD(dev, 0, NULL, -1);
	if (child == NULL) {
		device_printf(dev, "failed to add child for ldn %d, type %s\n",
		    ldn, devtype_to_str(type));
		return;
	}
	dinfo = device_get_ivars(child);
	dinfo->ldn = ldn;
	dinfo->type = type;
	sio_conf_enter(sc);
	dinfo->iobase = sio_ldn_readw(sc, ldn, 0x60);
	dinfo->iobase2 = sio_ldn_readw(sc, ldn, 0x62);
	dinfo->irq = sio_ldn_readw(sc, ldn, 0x70);
	dinfo->dma = sio_ldn_readw(sc, ldn, 0x74);
	sio_conf_exit(sc);
	STAILQ_INSERT_TAIL(&sc->devlist, dinfo, link);
}

static int
superio_attach(device_t dev)
{
	struct siosc *sc = device_get_softc(dev);
	int i;

	mtx_init(&sc->conf_lock, device_get_nameunit(dev), "superio", MTX_DEF);
	STAILQ_INIT(&sc->devlist);

	for (i = 0; sc->known_devices[i].type != SUPERIO_DEV_NONE; i++) {
		superio_add_known_child(dev, sc->known_devices[i].type,
		    sc->known_devices[i].ldn);
	}

	bus_generic_probe(dev);
	bus_generic_attach(dev);

	sc->chardev = make_dev(&superio_cdevsw, device_get_unit(dev),
	    UID_ROOT, GID_WHEEL, 0600, "superio%d", device_get_unit(dev));
	if (sc->chardev == NULL)
		device_printf(dev, "failed to create character device\n");
	else
		sc->chardev->si_drv1 = sc;
	return (0);
}

static int
superio_detach(device_t dev)
{
	struct siosc *sc = device_get_softc(dev);
	int error;

	error = bus_generic_detach(dev);
	if (error != 0)
		return (error);
	if (sc->chardev != NULL)
		destroy_dev(sc->chardev);
	device_delete_children(dev);
	bus_release_resource(dev, SYS_RES_IOPORT, sc->io_rid, sc->io_res);
	mtx_destroy(&sc->conf_lock);
	return (0);
}

static device_t
superio_add_child(device_t dev, u_int order, const char *name, int unit)
{
	struct superio_devinfo *dinfo;
	device_t child;

	child = device_add_child_ordered(dev, order, name, unit);
	if (child == NULL)
		return (NULL);
	dinfo = malloc(sizeof(*dinfo), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (dinfo == NULL) {
		device_delete_child(dev, child);
		return (NULL);
	}
	dinfo->ldn = 0xff;
	dinfo->type = SUPERIO_DEV_NONE;
	dinfo->dev = child;
	resource_list_init(&dinfo->resources);
	device_set_ivars(child, dinfo);
	return (child);
}

static int
superio_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct superio_devinfo *dinfo;

	dinfo = device_get_ivars(child);
	switch (which) {
	case SUPERIO_IVAR_LDN:
		*result = dinfo->ldn;
		break;
	case SUPERIO_IVAR_TYPE:
		*result = dinfo->type;
		break;
	case SUPERIO_IVAR_IOBASE:
		*result = dinfo->iobase;
		break;
	case SUPERIO_IVAR_IOBASE2:
		*result = dinfo->iobase2;
		break;
	case SUPERIO_IVAR_IRQ:
		*result = dinfo->irq;
		break;
	case SUPERIO_IVAR_DMA:
		*result = dinfo->dma;
		break;
	default:
		return (ENOENT);
	}
	return (0);
}

static int
superio_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{

	switch (which) {
	case SUPERIO_IVAR_LDN:
	case SUPERIO_IVAR_TYPE:
	case SUPERIO_IVAR_IOBASE:
	case SUPERIO_IVAR_IOBASE2:
	case SUPERIO_IVAR_IRQ:
	case SUPERIO_IVAR_DMA:
		return (EINVAL);
	default:
		return (ENOENT);
	}
}

static struct resource_list *
superio_get_resource_list(device_t dev, device_t child)
{
	struct superio_devinfo *dinfo = device_get_ivars(child);

	return (&dinfo->resources);
}

static int
superio_printf(struct superio_devinfo *dinfo, const char *fmt, ...)
{
	va_list ap;
	int retval;

	retval = printf("superio:%s@ldn%0x2x: ",
	    devtype_to_str(dinfo->type), dinfo->ldn);
	va_start(ap, fmt);
	retval += vprintf(fmt, ap);
	va_end(ap);
	return (retval);
}

static void
superio_child_detached(device_t dev, device_t child)
{
	struct superio_devinfo *dinfo;
	struct resource_list *rl;

	dinfo = device_get_ivars(child);
	rl = &dinfo->resources;

	if (resource_list_release_active(rl, dev, child, SYS_RES_IRQ) != 0)
		superio_printf(dinfo, "Device leaked IRQ resources\n");
	if (resource_list_release_active(rl, dev, child, SYS_RES_MEMORY) != 0)
		superio_printf(dinfo, "Device leaked memory resources\n");
	if (resource_list_release_active(rl, dev, child, SYS_RES_IOPORT) != 0)
		superio_printf(dinfo, "Device leaked I/O resources\n");
}

static int
superio_child_location(device_t parent, device_t child, struct sbuf *sb)
{
	uint8_t ldn;

	ldn = superio_get_ldn(child);
	sbuf_printf(sb, "ldn=0x%02x", ldn);
	return (0);
}

static int
superio_child_pnp(device_t parent, device_t child, struct sbuf *sb)
{
	superio_dev_type_t type;

	type = superio_get_type(child);
	sbuf_printf(sb, "type=%s", devtype_to_str(type));
	return (0);
}

static int
superio_print_child(device_t parent, device_t child)
{
	superio_dev_type_t type;
	uint8_t ldn;
	int retval;

	ldn = superio_get_ldn(child);
	type = superio_get_type(child);

	retval = bus_print_child_header(parent, child);
	retval += printf(" at %s ldn 0x%02x", devtype_to_str(type), ldn);
	retval += bus_print_child_footer(parent, child);

	return (retval);
}

superio_vendor_t
superio_vendor(device_t dev)
{
	device_t sio_dev = device_get_parent(dev);
	struct siosc *sc = device_get_softc(sio_dev);

	return (sc->vendor);
}

uint16_t
superio_devid(device_t dev)
{
	device_t sio_dev = device_get_parent(dev);
	struct siosc *sc = device_get_softc(sio_dev);

	return (sc->devid);
}

uint8_t
superio_revid(device_t dev)
{
	device_t sio_dev = device_get_parent(dev);
	struct siosc *sc = device_get_softc(sio_dev);

	return (sc->revid);
}

uint8_t
superio_read(device_t dev, uint8_t reg)
{
	device_t sio_dev = device_get_parent(dev);
	struct siosc *sc = device_get_softc(sio_dev);
	struct superio_devinfo *dinfo = device_get_ivars(dev);
	uint8_t v;

	sio_conf_enter(sc);
	v = sio_ldn_read(sc, dinfo->ldn, reg);
	sio_conf_exit(sc);
	return (v);
}

void
superio_write(device_t dev, uint8_t reg, uint8_t val)
{
	device_t sio_dev = device_get_parent(dev);
	struct siosc *sc = device_get_softc(sio_dev);
	struct superio_devinfo *dinfo = device_get_ivars(dev);

	sio_conf_enter(sc);
	sio_ldn_write(sc, dinfo->ldn, reg, val);
	sio_conf_exit(sc);
}

bool
superio_dev_enabled(device_t dev, uint8_t mask)
{
	device_t sio_dev = device_get_parent(dev);
	struct siosc *sc = device_get_softc(sio_dev);
	struct superio_devinfo *dinfo = device_get_ivars(dev);
	uint8_t v;

	/* GPIO device is always active in ITE chips. */
	if (sc->vendor == SUPERIO_VENDOR_ITE && dinfo->ldn == 7)
		return (true);

	v = superio_read(dev, sc->enable_reg);
	return ((v & mask) != 0);
}

void
superio_dev_enable(device_t dev, uint8_t mask)
{
	device_t sio_dev = device_get_parent(dev);
	struct siosc *sc = device_get_softc(sio_dev);
	struct superio_devinfo *dinfo = device_get_ivars(dev);
	uint8_t v;

	/* GPIO device is always active in ITE chips. */
	if (sc->vendor == SUPERIO_VENDOR_ITE && dinfo->ldn == 7)
		return;

	sio_conf_enter(sc);
	v = sio_ldn_read(sc, dinfo->ldn, sc->enable_reg);
	v |= mask;
	sio_ldn_write(sc, dinfo->ldn, sc->enable_reg, v);
	sio_conf_exit(sc);
}

void
superio_dev_disable(device_t dev, uint8_t mask)
{
	device_t sio_dev = device_get_parent(dev);
	struct siosc *sc = device_get_softc(sio_dev);
	struct superio_devinfo *dinfo = device_get_ivars(dev);
	uint8_t v;

	/* GPIO device is always active in ITE chips. */
	if (sc->vendor == SUPERIO_VENDOR_ITE && dinfo->ldn == 7)
		return;

	sio_conf_enter(sc);
	v = sio_ldn_read(sc, dinfo->ldn, sc->enable_reg);
	v &= ~mask;
	sio_ldn_write(sc, dinfo->ldn, sc->enable_reg, v);
	sio_conf_exit(sc);
}

device_t
superio_find_dev(device_t superio, superio_dev_type_t type, int ldn)
{
	struct siosc *sc = device_get_softc(superio);
	struct superio_devinfo *dinfo;

	if (ldn < -1 || ldn > UINT8_MAX)
		return (NULL);		/* ERANGE */
	if (type == SUPERIO_DEV_NONE && ldn == -1)
		return (NULL);		/* EINVAL */

	STAILQ_FOREACH(dinfo, &sc->devlist, link) {
		if (ldn != -1 && dinfo->ldn != ldn)
			continue;
		if (type != SUPERIO_DEV_NONE && dinfo->type != type)
			continue;
		return (dinfo->dev);
	}
	return (NULL);
}

static int
superio_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int flags,
    struct thread *td)
{
	struct siosc *sc;
	struct superiocmd *s;

	sc = dev->si_drv1;
	s = (struct superiocmd *)data;
	switch (cmd) {
	case SUPERIO_CR_READ:
		sio_conf_enter(sc);
		s->val = sio_ldn_read(sc, s->ldn, s->cr);
		sio_conf_exit(sc);
		return (0);
	case SUPERIO_CR_WRITE:
		sio_conf_enter(sc);
		sio_ldn_write(sc, s->ldn, s->cr, s->val);
		sio_conf_exit(sc);
		return (0);
	default:
		return (ENOTTY);
	}
}

static device_method_t superio_methods[] = {
	DEVMETHOD(device_identify,	superio_identify),
	DEVMETHOD(device_probe,		superio_probe),
	DEVMETHOD(device_attach,	superio_attach),
	DEVMETHOD(device_detach,	superio_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	DEVMETHOD(bus_add_child,	superio_add_child),
	DEVMETHOD(bus_child_detached,	superio_child_detached),
	DEVMETHOD(bus_child_location,	superio_child_location),
	DEVMETHOD(bus_child_pnpinfo,	superio_child_pnp),
	DEVMETHOD(bus_print_child,	superio_print_child),
	DEVMETHOD(bus_read_ivar,	superio_read_ivar),
	DEVMETHOD(bus_write_ivar,	superio_write_ivar),
	DEVMETHOD(bus_get_resource_list, superio_get_resource_list),
	DEVMETHOD(bus_alloc_resource,	bus_generic_rl_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_rl_release_resource),
	DEVMETHOD(bus_set_resource,	bus_generic_rl_set_resource),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),
	DEVMETHOD(bus_delete_resource,	bus_generic_rl_delete_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	DEVMETHOD_END
};

static driver_t superio_driver = {
	"superio",
	superio_methods,
	sizeof(struct siosc)
};

DRIVER_MODULE(superio, isa, superio_driver, 0, 0);
MODULE_VERSION(superio, 1);
