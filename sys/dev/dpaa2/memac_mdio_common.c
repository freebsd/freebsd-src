/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2021-2022 Bjoern A. Zeeb
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/endian.h>
#include <sys/socket.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include "memac_mdio.h"
#include "miibus_if.h"

/* #define	MEMAC_MDIO_DEBUG */

/* -------------------------------------------------------------------------- */

int
memacphy_miibus_readreg(device_t dev, int phy, int reg)
{

	return (MIIBUS_READREG(device_get_parent(dev), phy, reg));
}

int
memacphy_miibus_writereg(device_t dev, int phy, int reg, int data)
{

	return (MIIBUS_WRITEREG(device_get_parent(dev), phy, reg, data));
}

void
memacphy_miibus_statchg(struct memacphy_softc_common *sc)
{

	if (sc->dpnidev != NULL)
		MIIBUS_STATCHG(sc->dpnidev);
}

int
memacphy_set_ni_dev(struct memacphy_softc_common *sc, device_t nidev)
{

	if (nidev == NULL)
		return (EINVAL);

#if defined(MEMAC_MDIO_DEBUG)
	if (bootverbose)
		device_printf(sc->dev, "setting nidev %p (%s)\n",
		    nidev, device_get_nameunit(nidev));
#endif

	if (sc->dpnidev != NULL)
		return (EBUSY);

	sc->dpnidev = nidev;
	return (0);
}

int
memacphy_get_phy_loc(struct memacphy_softc_common *sc, int *phy_loc)
{
	int error;

	if (phy_loc == NULL)
		return (EINVAL);

	if (sc->phy == -1) {
		*phy_loc = MII_PHY_ANY;
		error = ENODEV;
	} else {
		*phy_loc = sc->phy;
		error = 0;
	}

#if defined(MEMAC_MDIO_DEBUG)
	if (bootverbose)
		device_printf(sc->dev, "returning phy_loc %d, error %d\n",
		    *phy_loc, error);
#endif

	return (error);
}

/* -------------------------------------------------------------------------- */

/*
 * MDIO Ethernet Management Interface Registers (internal PCS MDIO PHY)
 * 0x0030	MDIO Configuration Register (MDIO_CFG)
 * 0x0034	MDIO Control Register (MDIO_CTL)
 * 0x0038	MDIO Data Register (MDIO_DATA)
 * 0x003c	MDIO Register Address Register (MDIO_ADDR)
 *
 * External MDIO interfaces
 * 0x0030	External MDIO Configuration Register (EMDIO_CFG)
 * 0x0034	External MDIO Control Register (EMDIO_CTL)
 * 0x0038	External MDIO Data Register (EMDIO_DATA)
 * 0x003c	External MDIO Register Address Register (EMDIO_ADDR)
 */
#define	MDIO_CFG			0x00030
#define	MDIO_CFG_MDIO_RD_ER		(1 << 1)
#define	MDIO_CFG_ENC45			(1 << 6)
#define	MDIO_CFG_BUSY			(1 << 31)
#define	MDIO_CTL			0x00034
#define	MDIO_CTL_READ			(1 << 15)
#define	MDIO_CTL_PORT_ADDR(_x)		(((_x) & 0x1f) << 5)
#define	MDIO_CTL_DEV_ADDR(_x)		((_x) & 0x1f)
#define	MDIO_DATA			0x00038
#define	MDIO_ADDR			0x0003c

static uint32_t
memac_read_4(struct memac_mdio_softc_common *sc, uint32_t reg)
{
	uint32_t v, r;

	v = bus_read_4(sc->mem_res, reg);
	if (sc->is_little_endian)
		r = le32toh(v);
	else
		r = be32toh(v);

	return (r);
}

static void
memac_write_4(struct memac_mdio_softc_common *sc, uint32_t reg, uint32_t val)
{
	uint32_t v;

	if (sc->is_little_endian)
		v = htole32(val);
	else
		v = htobe32(val);
	bus_write_4(sc->mem_res, reg, v);
}

static uint32_t
memac_miibus_wait_no_busy(struct memac_mdio_softc_common *sc)
{
	uint32_t count, val;

	for (count = 1000; count > 0; count--) {
		val = memac_read_4(sc, MDIO_CFG);
		if ((val & MDIO_CFG_BUSY) == 0)
			break;
		DELAY(1);
	}

	if (count == 0)
		return (0xffff);

	return (0);
}

int
memac_miibus_readreg(struct memac_mdio_softc_common *sc, int phy, int reg)
{
	uint32_t cfg, ctl, val;

	/* Set proper Clause 45 mode. */
	cfg = memac_read_4(sc, MDIO_CFG);
	/* XXX 45 support? */
	cfg &= ~MDIO_CFG_ENC45;	/* Use Clause 22 */
	memac_write_4(sc, MDIO_CFG, cfg);

	val = memac_miibus_wait_no_busy(sc);
	if (val != 0)
		return (0xffff);

	/* To whom do we want to talk to.. */
	ctl = MDIO_CTL_PORT_ADDR(phy) | MDIO_CTL_DEV_ADDR(reg);
	/* XXX do we need two writes for this to work reliably? */
	memac_write_4(sc, MDIO_CTL, ctl | MDIO_CTL_READ);

	val = memac_miibus_wait_no_busy(sc);
	if (val != 0)
		return (0xffff);

	cfg = memac_read_4(sc, MDIO_CFG);
	if (cfg & MDIO_CFG_MDIO_RD_ER)
		return (0xffff);

	val = memac_read_4(sc, MDIO_DATA);
	val &= 0xffff;

#if defined(MEMAC_MDIO_DEBUG)
	device_printf(sc->dev, "phy read %d:%d = %#06x\n", phy, reg, val);
#endif

        return (val);
}

int
memac_miibus_writereg(struct memac_mdio_softc_common *sc, int phy, int reg, int data)
{
	uint32_t cfg, ctl, val;

#if defined(MEMAC_MDIO_DEBUG)
	device_printf(sc->dev, "phy write %d:%d\n", phy, reg);
#endif

	/* Set proper Clause 45 mode. */
	cfg = memac_read_4(sc, MDIO_CFG);
	/* XXX 45 support? */
	cfg &= ~MDIO_CFG_ENC45;	/* Use Clause 22 */
	memac_write_4(sc, MDIO_CFG, cfg);

	val = memac_miibus_wait_no_busy(sc);
	if (val != 0)
		return (0xffff);

	/* To whom do we want to talk to.. */
	ctl = MDIO_CTL_PORT_ADDR(phy) | MDIO_CTL_DEV_ADDR(reg);
	memac_write_4(sc, MDIO_CTL, ctl);

	memac_write_4(sc, MDIO_DATA, data & 0xffff);

	val = memac_miibus_wait_no_busy(sc);
	if (val != 0)
		return (0xffff);

	return (0);
}

ssize_t
memac_mdio_get_property(device_t dev, device_t child, const char *propname,
    void *propvalue, size_t size, device_property_type_t type)
{

	return (bus_generic_get_property(dev, child, propname, propvalue, size, type));
}

int
memac_mdio_read_ivar(device_t dev, device_t child, int index, uintptr_t *result)
{

	return (BUS_READ_IVAR(device_get_parent(dev), dev, index, result));
}


int
memac_mdio_generic_attach(struct memac_mdio_softc_common *sc)
{
	int rid;

	rid = 0;
	sc->mem_res = bus_alloc_resource_any(sc->dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE | RF_SHAREABLE);
	if (sc->mem_res == NULL) {
		device_printf(sc->dev, "%s: cannot allocate mem resource\n",
		    __func__);
		return (ENXIO);
	}

	sc->is_little_endian = device_has_property(sc->dev, "little-endian");

	return (0);
}

int
memac_mdio_generic_detach(struct memac_mdio_softc_common *sc)
{

	if (sc->mem_res != NULL)
		bus_release_resource(sc->dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->mem_res), sc->mem_res);

	return (0);
}
