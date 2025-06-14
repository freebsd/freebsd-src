/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 The FreeBSD Foundation
 *
 * This software was developed by Aymeric Wibo <obiwac@freebsd.org>
 * under sponsorship from the FreeBSD Foundation.
 */
#ifndef _AMDSMU_H_
#define	_AMDSMU_H_

#include <sys/types.h>
#include <machine/bus.h>
#include <x86/cputypes.h>

#include <dev/amdsmu/amdsmu_reg.h>

#define SMU_RES_READ_PERIOD_US	50
#define SMU_RES_READ_MAX	20000

static const struct amdsmu_product {
	uint16_t	amdsmu_vendorid;
	uint16_t	amdsmu_deviceid;
} amdsmu_products[] = {
	{ CPU_VENDOR_AMD,	PCI_DEVICEID_AMD_REMBRANDT_ROOT },
	{ CPU_VENDOR_AMD,	PCI_DEVICEID_AMD_PHOENIX_ROOT },
	{ CPU_VENDOR_AMD,	PCI_DEVICEID_AMD_STRIX_POINT_ROOT },
};

struct amdsmu_softc {
	struct resource		*res;
	bus_space_tag_t 	bus_tag;

	bus_space_handle_t	smu_space;
	bus_space_handle_t	reg_space;
};

static inline uint32_t
amdsmu_read4(const struct amdsmu_softc *sc, bus_size_t reg)
{
	return (bus_space_read_4(sc->bus_tag, sc->reg_space, reg));
}

static inline void
amdsmu_write4(const struct amdsmu_softc *sc, bus_size_t reg, uint32_t val)
{
	bus_space_write_4(sc->bus_tag, sc->reg_space, reg, val);
}

#endif /* _AMDSMU_H_ */
