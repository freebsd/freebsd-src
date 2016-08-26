/*-
 * Copyright (c) 2016 Landon Fuller <landonf@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 * 
 * $FreeBSD$
 */

#ifndef	_MIPS_BROADCOM_BCM_MACHDEP_H_
#define	_MIPS_BROADCOM_BCM_MACHDEP_H_

#include <machine/cpufunc.h>
#include <machine/cpuregs.h>

#include <dev/bhnd/bhnd.h>

struct bcm_platform {
	struct bhnd_chipid	id;		/**< chip id */
	struct bhnd_core_info	cc_id;		/**< chipc core info */
	uintptr_t		cc_addr;	/**< chipc core phys address */
	uint32_t		cc_caps;	/**< chipc capabilities */
	uint32_t		cc_caps_ext;	/**< chipc extended capabilies */

	/* On non-AOB devices, the PMU register block is mapped to chipc;
	 * the pmu_id and pmu_addr values will be copied from cc_id
	 * and cc_addr. */
	struct bhnd_core_info	pmu_id;		/**< PMU core info */
	uintptr_t		pmu_addr;	/**< PMU core phys address. */

#ifdef CFE
	int			cfe_console;	/**< Console handle, or -1 */
#endif
};


typedef int (bcm_bus_find_core)(struct bhnd_chipid *chipid,
    bhnd_devclass_t devclass, int unit, struct bhnd_core_info *info,
    uintptr_t *addr);

struct bcm_platform	*bcm_get_platform(void);

bcm_bus_find_core	 bcm_find_core_default;
bcm_bus_find_core	 bcm_find_core_bcma;
bcm_bus_find_core	 bcm_find_core_siba;

#define	BCM_SOC_ADDR(_addr, _offset)	\
	MIPS_PHYS_TO_KSEG1((_addr) + (_offset))

#define	BCM_SOC_READ_4(_addr, _offset)		\
	readl(BCM_SOC_ADDR((_addr), (_offset)))
#define	BCM_SOC_WRITE_4(_addr, _reg, _val)	\
	writel(BCM_SOC_ADDR((_addr), (_offset)), (_val))

#define	BCM_CORE_ADDR(_name, _reg)	\
	BCM_SOC_ADDR(bcm_get_platform()->_name, (_reg))

#define	BCM_CORE_READ_4(_name, _reg)		\
	readl(BCM_CORE_ADDR(_name, (_reg)))
#define	BCM_CORE_WRITE_4(_name, _reg, _val)	\
	writel(BCM_CORE_ADDR(_name, (_reg)), (_val))

#define	BCM_CHIPC_READ_4(_reg)		BCM_CORE_READ_4(cc_addr, (_reg))
#define	BCM_CHIPC_WRITE_4(_reg, _val)	\
	BCM_CORE_WRITE_4(cc_addr, (_reg), (_val))

#define	BCM_PMU_READ_4(_reg)		BCM_CORE_READ_4(pmu_addr, (_reg))
#define	BCM_PMU_WRITE_4(_reg, _val)	\
	BCM_CORE_WRITE_4(pmu_addr, (_reg), (_val))

#endif /* _MIPS_BROADCOM_BCM_MACHDEP_H_ */
