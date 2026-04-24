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
#include <sys/eventhandler.h>
#include <sys/kernel.h>

#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/cputypes.h>
#include <machine/specialreg.h>
#include <x86/cputypes.h>

#include <dev/amdsmu/amdsmu_reg.h>

#define SMU_RES_READ_PERIOD_US	50
#define SMU_RES_READ_MAX	20000

static const char *amdsmu_ip_blocks_names[] = {
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

static const char *amdsmu_ip_blocks_names_v2[] = {
    "DISPLAY",
    "CPU",
    "GFX",
    "VDD",
    "VDD_CCX",
    "ACP",
    "VCN_0",
    "VCN_1",
    "ISP",
    "NBIO",
    "DF",
    "USB3_0",
    "USB3_1",
    "LAPIC",
    "USB3_2",
    "USB4_RT0",
    "USB4_RT1",
    "USB4_0",
    "USB4_1",
    "MPM",
    "JPEG_0",
    "JPEG_1",
    "IPU",
    "UMSCH",
    "VPE",
};

#define IP_MAX_BLOCK_NAMES	32

CTASSERT(nitems(amdsmu_ip_blocks_names) <= IP_MAX_BLOCK_NAMES);
CTASSERT(nitems(amdsmu_ip_blocks_names_v2) <= IP_MAX_BLOCK_NAMES);

static const struct amdsmu_product {
	uint16_t	amdsmu_vendorid;
	uint16_t	amdsmu_deviceid;
	uint32_t	model;
	int16_t		idlemask_reg;
	size_t		ip_block_count;
	const char 	**ip_blocks_names;
	uint32_t	amdsmu_msg;
} amdsmu_products[] = {
	{ CPU_VENDOR_AMD,	PCI_DEVICEID_AMD_CEZANNE_ROOT, 0x00,
	    SMU_REG_IDLEMASK_CEZANNE,	12 , amdsmu_ip_blocks_names,
	    SMU_REG_MSG_CEZANNE},
	{ CPU_VENDOR_AMD,	PCI_DEVICEID_AMD_REMBRANDT_ROOT, 0x00,
	    SMU_REG_IDLEMASK_PHOENIX,	12 , amdsmu_ip_blocks_names,
	    SMU_REG_MSG_CEZANNE},
	{ CPU_VENDOR_AMD,	PCI_DEVICEID_AMD_PHOENIX_ROOT, 0x00,
	    SMU_REG_IDLEMASK_PHOENIX,	21 , amdsmu_ip_blocks_names,
	    SMU_REG_MSG_CEZANNE},
	{ CPU_VENDOR_AMD,	PCI_DEVICEID_AMD_KRACKAN_POINT_ROOT, 0x00,
	    SMU_REG_IDLEMASK_KRACKAN,	22, amdsmu_ip_blocks_names,
	    SMU_REG_MSG_KRACKAN },
	{ CPU_VENDOR_AMD,	PCI_DEVICEID_AMD_KRACKAN_POINT_ROOT, 0x70,
	    SMU_REG_IDLEMASK_KRACKAN,	25, amdsmu_ip_blocks_names_v2,
	    SMU_REG_MSG_KRACKAN },
	/*
	 * XXX Strix Point (PCI_DEVICEID_AMD_STRIX_POINT_ROOT) doesn't support
	 * S0i3 and thus doesn't have an idlemask.  Since our driver doesn't
	 * yet understand this, don't attach to Strix Point for the time being.
	 */
};

struct amdsmu_softc {
	const struct amdsmu_product	*product;

	struct sysctl_ctx_list	*sysctlctx;
	struct sysctl_oid	*sysctlnode;

	struct eventhandler_entry	*eh_suspend;
	struct eventhandler_entry	*eh_resume;

	struct resource		*res;
	bus_space_tag_t 	bus_tag;

	bus_space_handle_t	smu_space;
	bus_space_handle_t	reg_space;

	uint8_t			smu_program;
	uint8_t			smu_maj, smu_min, smu_rev;

	uint32_t		active_ip_blocks;
	struct sysctl_oid	*ip_blocks_sysctlnode;
	struct sysctl_oid	*ip_block_sysctlnodes[IP_MAX_BLOCK_NAMES];
	bool			ip_blocks_active[IP_MAX_BLOCK_NAMES];

	bus_space_handle_t	metrics_space;
	struct amdsmu_metrics	metrics;
	uint32_t		idlemask;
	uint32_t		smu_msg;
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
