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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
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

static const char *const amdsmu_ip_blocks_names[] = {
    "DISPLAY",
    "CPU",
    "GFX",
    "VDD",
    "ACP",
    "VCN",
    "ISP",
    "NBIO",
    "DF",
    "USB3_0",
    "USB3_1",
    "LAPIC",
    "USB3_2",
    "USB3_3",
    "USB3_4",
    "USB4_0",
    "USB4_1",
    "MPM",
    "JPEG",
    "IPU",
    "UMSCH",
    "VPE",
};

CTASSERT(nitems(amdsmu_ip_blocks_names) <= 32);

struct amdsmu_softc {
	struct sysctl_ctx_list	*sysctlctx;
	struct sysctl_oid	*sysctlnode;

	struct resource		*res;
	bus_space_tag_t 	bus_tag;

	bus_space_handle_t	smu_space;
	bus_space_handle_t	reg_space;

	uint8_t			smu_program;
	uint8_t			smu_maj, smu_min, smu_rev;

	uint32_t		active_ip_blocks;
	struct sysctl_oid	*ip_blocks_sysctlnode;
	size_t			ip_block_count;
	struct sysctl_oid	*ip_block_sysctlnodes[nitems(amdsmu_ip_blocks_names)];
	bool			ip_blocks_active[nitems(amdsmu_ip_blocks_names)];

	bus_space_handle_t	metrics_space;
	struct amdsmu_metrics	metrics;
	uint32_t		idlemask;
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
