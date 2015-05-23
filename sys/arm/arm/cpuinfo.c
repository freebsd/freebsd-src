/*-
 * Copyright 2014 Svatopluk Kraus <onwahe@gmail.com>
 * Copyright 2014 Michal Meloun <meloun@miracle.cz>
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

#include <machine/cpuinfo.h>
#include <machine/cpu-v6.h>

struct cpuinfo cpuinfo;

/* Read and parse CPU id scheme */
void
cpuinfo_init(void)
{

	cpuinfo.midr = cp15_midr_get();	
	/* Test old version id schemes first */
	if ((cpuinfo.midr & CPU_ID_IMPLEMENTOR_MASK) == CPU_ID_ARM_LTD) {
		if (CPU_ID_ISOLD(cpuinfo.midr)) {
			/* obsolete ARMv2 or ARMv3 CPU */
			cpuinfo.midr = 0;
			return;
		}
		if (CPU_ID_IS7(cpuinfo.midr)) {
			if ((cpuinfo.midr & (1 << 23)) == 0) {
				/* obsolete ARMv3 CPU */
				cpuinfo.midr = 0;
				return;
			}
			/* ARMv4T CPU */
			cpuinfo.architecture = 1;
			cpuinfo.revision = (cpuinfo.midr >> 16) & 0x7F;
		} else {
			/* ARM new id scheme */
			cpuinfo.architecture = (cpuinfo.midr >> 16) & 0x0F;
			cpuinfo.revision = (cpuinfo.midr >> 20) & 0x0F;
		}
	} else {
		/* non ARM -> must be new id scheme */
		cpuinfo.architecture = (cpuinfo.midr >> 16) & 0x0F;
		cpuinfo.revision = (cpuinfo.midr >> 20) & 0x0F;
	}	
	/* Parse rest of MIDR  */
	cpuinfo.implementer = (cpuinfo.midr >> 24) & 0xFF;
	cpuinfo.part_number = (cpuinfo.midr >> 4) & 0xFFF;
	cpuinfo.patch = cpuinfo.midr & 0x0F;

	/* CP15 c0,c0 regs 0-7 exist on all CPUs (although aliased with MIDR) */
	cpuinfo.ctr = cp15_ctr_get();
	cpuinfo.tcmtr = cp15_tcmtr_get();
	cpuinfo.tlbtr = cp15_tlbtr_get();
	cpuinfo.mpidr = cp15_mpidr_get();
	cpuinfo.revidr = cp15_revidr_get();
		
	/* if CPU is not v7 cpu id scheme */
	if (cpuinfo.architecture != 0xF)
		return;
		
	cpuinfo.id_pfr0 = cp15_id_pfr0_get();
	cpuinfo.id_pfr1 = cp15_id_pfr1_get();
	cpuinfo.id_dfr0 = cp15_id_dfr0_get();
	cpuinfo.id_afr0 = cp15_id_afr0_get();
	cpuinfo.id_mmfr0 = cp15_id_mmfr0_get();
	cpuinfo.id_mmfr1 = cp15_id_mmfr1_get();
	cpuinfo.id_mmfr2 = cp15_id_mmfr2_get();
	cpuinfo.id_mmfr3 = cp15_id_mmfr3_get();
	cpuinfo.id_isar0 = cp15_id_isar0_get();
	cpuinfo.id_isar1 = cp15_id_isar1_get();
	cpuinfo.id_isar2 = cp15_id_isar2_get();
	cpuinfo.id_isar3 = cp15_id_isar3_get();
	cpuinfo.id_isar4 = cp15_id_isar4_get();
	cpuinfo.id_isar5 = cp15_id_isar5_get();

/* Not yet - CBAR only exist on ARM SMP Cortex A CPUs
	cpuinfo.cbar = cp15_cbar_get();
*/

	/* Test if revidr is implemented */
	if (cpuinfo.revidr == cpuinfo.midr)
		cpuinfo.revidr = 0;

	/* parsed bits of above registers */
	/* id_mmfr0 */
	cpuinfo.outermost_shareability =  (cpuinfo.id_mmfr0 >> 8) & 0xF;
	cpuinfo.shareability_levels = (cpuinfo.id_mmfr0 >> 12) & 0xF;
	cpuinfo.auxiliary_registers = (cpuinfo.id_mmfr0 >> 20) & 0xF;
	cpuinfo.innermost_shareability = (cpuinfo.id_mmfr0 >> 28) & 0xF;
	/* id_mmfr2 */
	cpuinfo.mem_barrier = (cpuinfo.id_mmfr2 >> 20) & 0xF;
	/* id_mmfr3 */
	cpuinfo.coherent_walk = (cpuinfo.id_mmfr3 >> 20) & 0xF;
	cpuinfo.maintenance_broadcast =(cpuinfo.id_mmfr3 >> 12) & 0xF;
	/* id_pfr1 */
	cpuinfo.generic_timer_ext = (cpuinfo.id_pfr1 >> 16) & 0xF;
	cpuinfo.virtualization_ext = (cpuinfo.id_pfr1 >> 12) & 0xF;
	cpuinfo.security_ext = (cpuinfo.id_pfr1 >> 4) & 0xF;
}
