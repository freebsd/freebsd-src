/*-
 * Copyright (c) 2012 Olivier Houchard.  All rights reserved.
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
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/smp.h>
#include <machine/fdt.h>
#include <machine/intr.h>

#include <arm/ti/ti_smc.h>
#include <arm/ti/omap4/omap4_smc.h>

void
platform_mp_init_secondary(void)
{
	arm_pic_init_secondary();
}

void
platform_mp_setmaxid(void)
{

	mp_maxid = 1;
	mp_ncpus = 2;
}

int
platform_mp_probe(void)
{

	return (1);
}

void    
platform_mp_start_ap(void)
{
	bus_addr_t scu_addr;

	if (bus_space_map(fdtbus_bs_tag, 0x48240000, 0x1000, 0, &scu_addr) != 0)
		panic("Couldn't map the SCU\n");
	/* Enable the SCU */
	*(volatile unsigned int *)scu_addr |= 1;
	//*(volatile unsigned int *)(scu_addr + 0x30) |= 1;
	cpu_idcache_wbinv_all();
	cpu_l2cache_wbinv_all();
	ti_smc0(0x200, 0xfffffdff, MODIFY_AUX_CORE_0);
	ti_smc0(pmap_kextract((vm_offset_t)mpentry), 0, WRITE_AUX_CORE_1);
	armv7_sev();
	bus_space_unmap(fdtbus_bs_tag, scu_addr, 0x1000);
}

void
platform_ipi_send(cpuset_t cpus, u_int ipi)
{
	pic_ipi_send(cpus, ipi);
}
