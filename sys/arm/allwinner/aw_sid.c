/*-
 * Copyright (c) 2016 Jared McNeill <jmcneill@invisible.ca>
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Allwinner secure ID controller
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/endian.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/allwinner/aw_sid.h>

/* efuse registers */
#define	SID_PRCTL		0x40
#define	 SID_PRCTL_OFFSET_MASK	0xff
#define	 SID_PRCTL_OFFSET(n)	(((n) & SID_PRCTL_OFFSET_MASK) << 16)
#define	 SID_PRCTL_LOCK		(0xac << 8)
#define	 SID_PRCTL_READ		(0x01 << 1)
#define	 SID_PRCTL_WRITE	(0x01 << 0)
#define	SID_PRKEY		0x50
#define	SID_RDKEY		0x60

#define	SID_SRAM		0x200
#define	SID_THERMAL_CALIB0	(SID_SRAM + 0x34)
#define	SID_THERMAL_CALIB1	(SID_SRAM + 0x38)

#define	ROOT_KEY_SIZE		4

struct aw_sid_conf {
	bus_size_t	efuse_size;
	bus_size_t	rootkey_offset;
	bool		has_prctl;
	bool		has_thermal;
	bool		requires_prctl_read;
};

static const struct aw_sid_conf a10_conf = {
	.efuse_size = 0x10,
	.rootkey_offset = 0,
};

static const struct aw_sid_conf a20_conf = {
	.efuse_size = 0x10,
	.rootkey_offset = 0,
};

static const struct aw_sid_conf a64_conf = {
	.efuse_size = 0x100,
	.rootkey_offset = SID_SRAM,
	.has_prctl = true,
	.has_thermal = true,
};

static const struct aw_sid_conf a83t_conf = {
	.efuse_size = 0x100,
	.rootkey_offset = SID_SRAM,
	.has_prctl = true,
	.has_thermal = true,
};

static const struct aw_sid_conf h3_conf = {
	.efuse_size = 0x100,
	.rootkey_offset = SID_SRAM,
	.has_prctl = true,
	.has_thermal = true,
	.requires_prctl_read = true,
};

static struct ofw_compat_data compat_data[] = {
	{ "allwinner,sun4i-a10-sid",		(uintptr_t)&a10_conf},
	{ "allwinner,sun7i-a20-sid",		(uintptr_t)&a20_conf},
	{ "allwinner,sun50i-a64-sid",		(uintptr_t)&a64_conf},
	{ "allwinner,sun8i-a83t-sid",		(uintptr_t)&a83t_conf},
	{ "allwinner,sun8i-h3-sid",		(uintptr_t)&h3_conf},
	{ NULL,					0 }
};

struct aw_sid_softc {
	struct resource		*res;
	struct aw_sid_conf	*sid_conf;
	struct mtx		prctl_mtx;
};

static struct aw_sid_softc *aw_sid_sc;

static struct resource_spec aw_sid_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

enum sid_keys {
	AW_SID_ROOT_KEY,
};

#define	RD4(sc, reg)		bus_read_4((sc)->res, (reg))
#define	WR4(sc, reg, val)	bus_write_4((sc)->res, (reg), (val))

static int aw_sid_sysctl(SYSCTL_HANDLER_ARGS);
static int aw_sid_prctl_read(device_t dev, bus_size_t offset, uint32_t *val);


/*
 * offset here is offset into efuse space, rather than offset into sid register
 * space. This form of read is only an option for newer SoC: A83t, H3, A64
 */
static int
aw_sid_prctl_read(device_t dev, bus_size_t offset, uint32_t *val)
{
	struct aw_sid_softc *sc;
	uint32_t readval;

	sc = device_get_softc(dev);
	if (!sc->sid_conf->has_prctl)
		return (1);

	mtx_lock(&sc->prctl_mtx);
	readval = SID_PRCTL_OFFSET(offset) | SID_PRCTL_LOCK | SID_PRCTL_READ;
	WR4(sc, SID_PRCTL, readval);
	/* Read bit will be cleared once read has concluded */
	while (RD4(sc, SID_PRCTL) & SID_PRCTL_READ)
		continue;
	readval = RD4(sc, SID_RDKEY);
	mtx_unlock(&sc->prctl_mtx);
	*val = readval;

	return (0);
}

static int
aw_sid_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Allwinner Secure ID Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
aw_sid_attach(device_t dev)
{
	struct aw_sid_softc *sc;
	bus_size_t i;
	uint32_t val;

	sc = device_get_softc(dev);

	if (bus_alloc_resources(dev, aw_sid_spec, &sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		return (ENXIO);
	}

	mtx_init(&sc->prctl_mtx, device_get_nameunit(dev), NULL, MTX_DEF);
	sc->sid_conf = (struct aw_sid_conf *)ofw_bus_search_compatible(dev, compat_data)->ocd_data;
	aw_sid_sc = sc;

	/*
	 * This set of reads is solely for working around a silicon bug on some
	 * SoC that require a prctl read in order for direct register access to
	 * return a non-garbled value. Hence, the values we read are simply
	 * ignored.
	 */
	if (sc->sid_conf->requires_prctl_read)
		for (i = 0; i < sc->sid_conf->efuse_size; i += 4)
			if (aw_sid_prctl_read(dev, i, &val) != 0) {
				device_printf(dev, "failed prctl read\n");
				goto fail;
			}

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "rootkey",
	    CTLTYPE_STRING | CTLFLAG_RD,
	    dev, AW_SID_ROOT_KEY, aw_sid_sysctl, "A", "Root Key");

	return (0);

fail:
	bus_release_resources(dev, aw_sid_spec, &sc->res);
	mtx_destroy(&sc->prctl_mtx);
	return (ENXIO);
}

int
aw_sid_read_tscalib(uint32_t *calib0, uint32_t *calib1)
{
	struct aw_sid_softc *sc;

	sc = aw_sid_sc;
	if (sc == NULL)
		return (ENXIO);
	if (!sc->sid_conf->has_thermal)
		return (ENXIO);

	*calib0 = RD4(sc, SID_THERMAL_CALIB0);
	*calib1 = RD4(sc, SID_THERMAL_CALIB1);

	return (0);
}

int
aw_sid_get_rootkey(u_char *out)
{
	struct aw_sid_softc *sc;
	int i;
	bus_size_t root_key_off;
	u_int tmp;

	sc = aw_sid_sc;
	if (sc == NULL)
		return (ENXIO);
	root_key_off = aw_sid_sc->sid_conf->rootkey_offset;
	for (i = 0; i < ROOT_KEY_SIZE ; i++) {
		tmp = RD4(aw_sid_sc, root_key_off + (i * 4));
		be32enc(&out[i * 4], tmp);
	}

	return (0);
}

static int
aw_sid_sysctl(SYSCTL_HANDLER_ARGS)
{
	enum sid_keys key = arg2;
	u_char rootkey[16];
	char out[33];

	if (key != AW_SID_ROOT_KEY)
		return (ENOENT);

	if (aw_sid_get_rootkey(rootkey) != 0)
		return (ENOENT);
	snprintf(out, sizeof(out),
	  "%16D", rootkey, "");

	return sysctl_handle_string(oidp, out, sizeof(out), req);
}

static device_method_t aw_sid_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		aw_sid_probe),
	DEVMETHOD(device_attach,	aw_sid_attach),

	DEVMETHOD_END
};

static driver_t aw_sid_driver = {
	"aw_sid",
	aw_sid_methods,
	sizeof(struct aw_sid_softc),
};

static devclass_t aw_sid_devclass;

EARLY_DRIVER_MODULE(aw_sid, simplebus, aw_sid_driver, aw_sid_devclass, 0, 0,
    BUS_PASS_RESOURCE + BUS_PASS_ORDER_FIRST);
MODULE_VERSION(aw_sid, 1);
