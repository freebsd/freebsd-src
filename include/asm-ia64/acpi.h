/*
 *  asm-ia64/acpi.h
 *
 *  Copyright (C) 1999 VA Linux Systems
 *  Copyright (C) 1999 Walt Drummond <drummond@valinux.com>
 *  Copyright (C) 2000,2001 J.I. Lee <jung-ik.lee@intel.com>
 *  Copyright (C) 2001,2002 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#ifndef _ASM_ACPI_H
#define _ASM_ACPI_H

#ifdef __KERNEL__

#define COMPILER_DEPENDENT_INT64	long
#define COMPILER_DEPENDENT_UINT64	unsigned long

/*
 * Calling conventions:
 *
 * ACPI_SYSTEM_XFACE        - Interfaces to host OS (handlers, threads)
 * ACPI_EXTERNAL_XFACE      - External ACPI interfaces
 * ACPI_INTERNAL_XFACE      - Internal ACPI interfaces
 * ACPI_INTERNAL_VAR_XFACE  - Internal variable-parameter list interfaces
 */
#define ACPI_SYSTEM_XFACE
#define ACPI_EXTERNAL_XFACE
#define ACPI_INTERNAL_XFACE
#define ACPI_INTERNAL_VAR_XFACE

/* Asm macros */

#define ACPI_ASM_MACROS
#define BREAKPOINT3
#define ACPI_DISABLE_IRQS() __cli()
#define ACPI_ENABLE_IRQS()  __sti()
#define ACPI_FLUSH_CPU_CACHE()

#define ACPI_ACQUIRE_GLOBAL_LOCK(GLptr, Acq) \
	do { \
	__asm__ volatile ("1:  ld4      r29=[%1]\n"  \
		";;\n"                  \
		"mov    ar.ccv=r29\n"   \
		"mov    r2=r29\n"       \
		"shr.u  r30=r29,1\n"    \
		"and    r29=-4,r29\n"   \
		";;\n"                  \
		"add    r29=2,r29\n"    \
		"and    r30=1,r30\n"    \
		";;\n"                  \
		"add    r29=r29,r30\n"  \
		";;\n"                  \
		"cmpxchg4.acq   r30=[%1],r29,ar.ccv\n" \
		";;\n"                  \
		"cmp.eq p6,p7=r2,r30\n" \
		"(p7) br.dpnt.few 1b\n" \
		"cmp.gt p8,p9=3,r29\n"  \
		";;\n"                  \
		"(p8) mov %0=-1\n"      \
		"(p9) mov %0=r0\n"      \
		:"=r"(Acq):"r"(GLptr):"r2","r29","r30","memory"); \
	} while (0)

#define ACPI_RELEASE_GLOBAL_LOCK(GLptr, Acq) \
	do { \
	__asm__ volatile ("1:  ld4      r29=[%1]\n" \
		";;\n"                  \
		"mov    ar.ccv=r29\n"   \
		"mov    r2=r29\n"       \
		"and    r29=-4,r29\n"   \
		";;\n"                  \
		"cmpxchg4.acq   r30=[%1],r29,ar.ccv\n" \
		";;\n"                  \
		"cmp.eq p6,p7=r2,r30\n" \
		"(p7) br.dpnt.few 1b\n" \
		"and    %0=1,r2\n"      \
		";;\n"                  \
		:"=r"(Acq):"r"(GLptr):"r2","r29","r30","memory"); \
	} while (0)

#define acpi_disabled 0	/* ACPI always enabled */
#define acpi_strict 1	/* no ACPI workarounds */
static inline void disable_acpi(void) { }

const char *acpi_get_sysname (void);
int acpi_request_vector (u32 int_type);
int acpi_get_prt (struct pci_vector_struct **vectors, int *count);
int acpi_get_interrupt_model (int *type);
int acpi_irq_to_vector (u32 irq);

#ifdef CONFIG_ACPI_NUMA
#include <asm/numa.h>
/* Proximity bitmap length; _PXM is at most 255 (8 bit)*/
#define MAX_PXM_DOMAINS (256)
extern int __initdata pxm_to_nid_map[MAX_PXM_DOMAINS];
extern int __initdata nid_to_pxm_map[NR_NODES];
#endif

#endif /*__KERNEL__*/

#endif /*_ASM_ACPI_H*/
