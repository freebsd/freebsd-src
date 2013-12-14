#-
# Copyright (c) 2009 Nathan Whitehorn
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD: projects/specific_leg/sys/powerpc/powerpc/platform_if.m 255417 2013-09-09 12:49:19Z nwhitehorn $
#

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/smp.h>

#include <machine/platform.h>
#include <machine/platformvar.h>
#include <machine/smp.h>
#include <machine/vmparam.h>

/**
 * @defgroup PLATFORM platform - KObj methods for PowerPC platform
 * implementations
 * @brief A set of methods required by all platform implementations.
 * These are used to bring up secondary CPUs, supply the physical memory
 * map, etc.
 *@{
 */

INTERFACE platform;

#
# Default implementations
#
CODE {
	static void platform_null_attach(platform_t plat)
	{
		return;
	}

	static struct arm32_dma_range *
	platform_null_bus_dma_get_range(platform_t plat)
	{
		return (NULL);
	}

	static int platform_null_bus_dma_get_range_nb(platform_t plat)
	{
		return (0);
	}

#if 0
	static int platform_null_smp_first_cpu(platform_t plat,
	    struct cpuref  *cpuref)
	{
		cpuref->cr_hwref = -1;
		cpuref->cr_cpuid = 0;
		return (0);
	}
	static int platform_null_smp_next_cpu(platform_t plat,
	    struct cpuref  *_cpuref)
	{
		return (ENOENT);
	}
	static struct cpu_group *platform_null_smp_topo(platform_t plat)
	{
#ifdef SMP
		return (smp_topo_none());
#else
		return (NULL);
#endif
	}
#endif
};

/**
 * @brief Probe for whether we are on this platform, returning the standard
 * newbus probe codes. If we have Open Firmware or a flattened device tree,
 * it is guaranteed to be available at this point.
 */
METHOD int probe {
	platform_t	_plat;
};

/**
 * @brief Attach this platform module. This happens before the MMU is online,
 * so the platform module can install its own high-priority MMU module at
 * this point.
 */
METHOD int attach {
	platform_t	_plat;
} DEFAULT platform_null_attach;

/**
 * @brief Called as one of the last steps of early virtual memory
 * initialization, shortly before the new page tables are installed.
 */
METHOD int devmap_init {
	platform_t	_plat;
};

/**
 * @brief Called after devmap_init(), and must return the address of the
 * first byte of unusable KVA space.  This allows a platform to carve out
 * of the top of the KVA space whatever reserves it needs for things like
 * static device mapping, and this is called to get the value before
 * calling pmap_bootstrap() which uses the value to size the available KVA.
 */
METHOD vm_offset_t lastaddr {
	platform_t	_plat;
};

/**
 * @brief Called after the static device mappings are established and just
 * before cninit(). The intention is that the routine can do any hardware
 * setup (such as gpio or pinmux) necessary to make the console functional.
 */
METHOD void gpio_init {
	platform_t	_plat;
};

/**
 * @brief Called just after cninit(). This is the first of the init
 * routines that can use printf() and expect the output to appear on
 * a standard console.
 */
METHOD void late_init {
	platform_t	_plat;
};

/**
 */
METHOD void cpu_reset {
	platform_t	_plat;
};

/**
 */
METHOD int get_next_irq {
	platform_t	_plat;
	int		last;
};

/**
 */
METHOD void mask_irq {
	platform_t	_plat;
	uintptr_t	irq;
};

/**
 */
METHOD void unmask_irq {
	platform_t	_plat;
	uintptr_t	irq;
};

/**
 */
METHOD struct arm32_dma_range * bus_dma_get_range {
	platform_t	_plat;
} DEFAULT platform_null_bus_dma_get_range;

/**
 */
METHOD int bus_dma_get_range_nb {
	platform_t	_plat;
} DEFAULT platform_null_bus_dma_get_range_nb;

/**
 */
METHOD void cpu_initclocks {
	platform_t	_plat;
};

/**
 */
METHOD void delay {
	platform_t	_plat;
	int		usec;
};
