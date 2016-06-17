/* 
 * pmc.h
 * Copyright (C) 2001  Dave Engebretsen & Mike Corrigan IBM Corporation.
 *
 * The PPC64 PMC subsystem encompases both the hardware PMC registers and 
 * a set of software event counters.  An interface is provided via the
 * proc filesystem which can be used to access this subsystem.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

/* Start Change Log
 * 2001/06/05 : engebret : Created.
 * End Change Log 
 */

#ifndef _PPC64_TYPES_H
#include        <asm/types.h>
#endif

#ifndef _PMC_H
#define _PMC_H

#define STAB_ENTRY_MAX 64

struct _pmc_hw
{
	u64 mmcr0; 
	u64 mmcr1; 
	u64 mmcra; 

	u64 pmc1; 
	u64 pmc2; 
	u64 pmc3; 
	u64 pmc4; 
	u64 pmc5; 
	u64 pmc6; 
	u64 pmc7; 
	u64 pmc8; 
};

struct _pmc_sw
{
	u64 stab_faults;           /* Count of faults on the stab      */
	u64 stab_capacity_castouts;/* Count of castouts from the stab  */
	u64 stab_invalidations;	   /* Count of invalidations from the  */
                                   /*   stab, not including castouts   */
	u64 stab_entry_use[STAB_ENTRY_MAX]; 

	u64 htab_primary_overflows;
	u64 htab_capacity_castouts;
	u64 htab_read_to_write_fault;
};

#define PMC_HW_TEXT_ENTRY_COUNT (sizeof(struct _pmc_hw) / sizeof(u64))
#define PMC_SW_TEXT_ENTRY_COUNT (sizeof(struct _pmc_sw) / sizeof(u64))
#define PMC_TEXT_ENTRY_SIZE  64

struct _pmc_sw_text {
	char buffer[PMC_SW_TEXT_ENTRY_COUNT * PMC_TEXT_ENTRY_SIZE];
};

struct _pmc_hw_text {
	char buffer[PMC_HW_TEXT_ENTRY_COUNT * PMC_TEXT_ENTRY_SIZE];
};

extern struct _pmc_sw pmc_sw_system;
extern struct _pmc_sw pmc_sw_cpu[];

extern struct _pmc_sw_text pmc_sw_text;
extern struct _pmc_hw_text pmc_hw_text;
extern char *ppc64_pmc_stab(int file);
extern char *ppc64_pmc_htab(int file);
extern char *ppc64_pmc_hw(int file);

void *btmalloc(unsigned long size);
void btfree(void *addr);

#if 1
#define PMC_SW_PROCESSOR(F)      pmc_sw_cpu[smp_processor_id()].F++
#define PMC_SW_PROCESSOR_A(F, E) (pmc_sw_cpu[smp_processor_id()].F[(E)])++
#define PMC_SW_SYSTEM(F)         pmc_sw_system.F++
#else
#define PMC_SW_PROCESSOR(F)   do {;} while (0)
#define PMC_SW_PROCESSOR_A(F) do {;} while (0)
#define PMC_SW_SYSTEM(F)      do {;} while (0)
#endif

#define PMC_CONTROL_CPI 1
#define PMC_CONTROL_TLB 2

/* To find an entry in the bolted page-table-directory */
#define pgd_offset_b(address) (bolted_pgd + pgd_index(address))
#define BTMALLOC_START 0xB000000000000000
#define BTMALLOC_END   0xB0000000ffffffff /* 4 GB Max-more or less arbitrary */

#endif /* _PMC_H */
