/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2010 Hudson River Trading LLC
 * Written by: John H. Baldwin <jhb@FreeBSD.org>
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

#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/smp.h>
#include <sys/vmmeter.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>
#include <vm/vm_phys.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/aclocal.h>
#include <contrib/dev/acpica/include/actables.h>

#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <x86/apicvar.h>

#include <dev/acpica/acpivar.h>

#if MAXMEMDOM > 1
static struct cpu_info {
	int enabled:1;
	int has_memory:1;
	int domain;
} *cpus;

struct mem_affinity mem_info[VM_PHYSSEG_MAX + 1];
int num_mem;

static ACPI_TABLE_SRAT *srat;
static vm_paddr_t srat_physaddr;

static int domain_pxm[MAXMEMDOM];
static int ndomain;

static ACPI_TABLE_SLIT *slit;
static vm_paddr_t slit_physaddr;
static int vm_locality_table[MAXMEMDOM * MAXMEMDOM];

static void	srat_walk_table(acpi_subtable_handler *handler, void *arg);

/*
 * SLIT parsing.
 */

static void
slit_parse_table(ACPI_TABLE_SLIT *s)
{
	int i, j;
	int i_domain, j_domain;
	int offset = 0;
	uint8_t e;

	/*
	 * This maps the SLIT data into the VM-domain centric view.
	 * There may be sparse entries in the PXM namespace, so
	 * remap them to a VM-domain ID and if it doesn't exist,
	 * skip it.
	 *
	 * It should result in a packed 2d array of VM-domain
	 * locality information entries.
	 */

	if (bootverbose)
		printf("SLIT.Localities: %d\n", (int) s->LocalityCount);
	for (i = 0; i < s->LocalityCount; i++) {
		i_domain = acpi_map_pxm_to_vm_domainid(i);
		if (i_domain < 0)
			continue;

		if (bootverbose)
			printf("%d: ", i);
		for (j = 0; j < s->LocalityCount; j++) {
			j_domain = acpi_map_pxm_to_vm_domainid(j);
			if (j_domain < 0)
				continue;
			e = s->Entry[i * s->LocalityCount + j];
			if (bootverbose)
				printf("%d ", (int) e);
			/* 255 == "no locality information" */
			if (e == 255)
				vm_locality_table[offset] = -1;
			else
				vm_locality_table[offset] = e;
			offset++;
		}
		if (bootverbose)
			printf("\n");
	}
}

/*
 * Look for an ACPI System Locality Distance Information Table ("SLIT")
 */
static int
parse_slit(void)
{

	if (resource_disabled("slit", 0)) {
		return (-1);
	}

	slit_physaddr = acpi_find_table(ACPI_SIG_SLIT);
	if (slit_physaddr == 0) {
		return (-1);
	}

	/*
	 * Make a pass over the table to populate the cpus[] and
	 * mem_info[] tables.
	 */
	slit = acpi_map_table(slit_physaddr, ACPI_SIG_SLIT);
	slit_parse_table(slit);
	acpi_unmap_table(slit);
	slit = NULL;

	return (0);
}

/*
 * SRAT parsing.
 */

/*
 * Returns true if a memory range overlaps with at least one range in
 * phys_avail[].
 */
static int
overlaps_phys_avail(vm_paddr_t start, vm_paddr_t end)
{
	int i;

	for (i = 0; phys_avail[i] != 0 && phys_avail[i + 1] != 0; i += 2) {
		if (phys_avail[i + 1] <= start)
			continue;
		if (phys_avail[i] < end)
			return (1);
		break;
	}
	return (0);
	
}

static void
srat_parse_entry(ACPI_SUBTABLE_HEADER *entry, void *arg)
{
	ACPI_SRAT_CPU_AFFINITY *cpu;
	ACPI_SRAT_X2APIC_CPU_AFFINITY *x2apic;
	ACPI_SRAT_MEM_AFFINITY *mem;
	int domain, i, slot;

	switch (entry->Type) {
	case ACPI_SRAT_TYPE_CPU_AFFINITY:
		cpu = (ACPI_SRAT_CPU_AFFINITY *)entry;
		domain = cpu->ProximityDomainLo |
		    cpu->ProximityDomainHi[0] << 8 |
		    cpu->ProximityDomainHi[1] << 16 |
		    cpu->ProximityDomainHi[2] << 24;
		if (bootverbose)
			printf("SRAT: Found CPU APIC ID %u domain %d: %s\n",
			    cpu->ApicId, domain,
			    (cpu->Flags & ACPI_SRAT_CPU_ENABLED) ?
			    "enabled" : "disabled");
		if (!(cpu->Flags & ACPI_SRAT_CPU_ENABLED))
			break;
		if (cpu->ApicId > max_apic_id) {
			printf("SRAT: Ignoring local APIC ID %u (too high)\n",
			    cpu->ApicId);
			break;
		}

		if (cpus[cpu->ApicId].enabled) {
			printf("SRAT: Duplicate local APIC ID %u\n",
			    cpu->ApicId);
			*(int *)arg = ENXIO;
			break;
		}
		cpus[cpu->ApicId].domain = domain;
		cpus[cpu->ApicId].enabled = 1;
		break;
	case ACPI_SRAT_TYPE_X2APIC_CPU_AFFINITY:
		x2apic = (ACPI_SRAT_X2APIC_CPU_AFFINITY *)entry;
		if (bootverbose)
			printf("SRAT: Found CPU APIC ID %u domain %d: %s\n",
			    x2apic->ApicId, x2apic->ProximityDomain,
			    (x2apic->Flags & ACPI_SRAT_CPU_ENABLED) ?
			    "enabled" : "disabled");
		if (!(x2apic->Flags & ACPI_SRAT_CPU_ENABLED))
			break;
		if (x2apic->ApicId > max_apic_id) {
			printf("SRAT: Ignoring local APIC ID %u (too high)\n",
			    x2apic->ApicId);
			break;
		}

		KASSERT(!cpus[x2apic->ApicId].enabled,
		    ("Duplicate local APIC ID %u", x2apic->ApicId));
		cpus[x2apic->ApicId].domain = x2apic->ProximityDomain;
		cpus[x2apic->ApicId].enabled = 1;
		break;
	case ACPI_SRAT_TYPE_MEMORY_AFFINITY:
		mem = (ACPI_SRAT_MEM_AFFINITY *)entry;
		if (bootverbose)
			printf(
		    "SRAT: Found memory domain %d addr 0x%jx len 0x%jx: %s\n",
			    mem->ProximityDomain, (uintmax_t)mem->BaseAddress,
			    (uintmax_t)mem->Length,
			    (mem->Flags & ACPI_SRAT_MEM_ENABLED) ?
			    "enabled" : "disabled");
		if (!(mem->Flags & ACPI_SRAT_MEM_ENABLED))
			break;
		if (mem->BaseAddress >= cpu_getmaxphyaddr() || 
		    !overlaps_phys_avail(mem->BaseAddress,
		    mem->BaseAddress + mem->Length)) {
			printf("SRAT: Ignoring memory at addr 0x%jx\n",
			    (uintmax_t)mem->BaseAddress);
			break;
		}
		if (num_mem == VM_PHYSSEG_MAX) {
			printf("SRAT: Too many memory regions\n");
			*(int *)arg = ENXIO;
			break;
		}
		slot = num_mem;
		for (i = 0; i < num_mem; i++) {
			if (mem_info[i].end <= mem->BaseAddress)
				continue;
			if (mem_info[i].start <
			    (mem->BaseAddress + mem->Length)) {
				printf("SRAT: Overlapping memory entries\n");
				*(int *)arg = ENXIO;
				return;
			}
			slot = i;
		}
		for (i = num_mem; i > slot; i--)
			mem_info[i] = mem_info[i - 1];
		mem_info[slot].start = mem->BaseAddress;
		mem_info[slot].end = mem->BaseAddress + mem->Length;
		mem_info[slot].domain = mem->ProximityDomain;
		num_mem++;
		break;
	}
}

/*
 * Ensure each memory domain has at least one CPU and that each CPU
 * has at least one memory domain.
 */
static int
check_domains(void)
{
	int found, i, j;

	for (i = 0; i < num_mem; i++) {
		found = 0;
		for (j = 0; j <= max_apic_id; j++)
			if (cpus[j].enabled &&
			    cpus[j].domain == mem_info[i].domain) {
				cpus[j].has_memory = 1;
				found++;
			}
		if (!found) {
			printf("SRAT: No CPU found for memory domain %d\n",
			    mem_info[i].domain);
			return (ENXIO);
		}
	}
	for (i = 0; i <= max_apic_id; i++)
		if (cpus[i].enabled && !cpus[i].has_memory) {
			found = 0;
			for (j = 0; j < num_mem && !found; j++) {
				if (mem_info[j].domain == cpus[i].domain)
					found = 1;
			}
			if (!found) {
				if (bootverbose)
					printf("SRAT: mem dom %d is empty\n",
					    cpus[i].domain);
				mem_info[num_mem].start = 0;
				mem_info[num_mem].end = 0;
				mem_info[num_mem].domain = cpus[i].domain;
				num_mem++;
			}
		}
	return (0);
}

/*
 * Check that the SRAT memory regions cover all of the regions in
 * phys_avail[].
 */
static int
check_phys_avail(void)
{
	vm_paddr_t address;
	int i, j;

	/* j is the current offset into phys_avail[]. */
	address = phys_avail[0];
	j = 0;
	for (i = 0; i < num_mem; i++) {
		/*
		 * Consume as many phys_avail[] entries as fit in this
		 * region.
		 */
		while (address >= mem_info[i].start &&
		    address <= mem_info[i].end) {
			/*
			 * If we cover the rest of this phys_avail[] entry,
			 * advance to the next entry.
			 */
			if (phys_avail[j + 1] <= mem_info[i].end) {
				j += 2;
				if (phys_avail[j] == 0 &&
				    phys_avail[j + 1] == 0) {
					return (0);
				}
				address = phys_avail[j];
			} else
				address = mem_info[i].end + 1;
		}
	}
	printf("SRAT: No memory region found for 0x%jx - 0x%jx\n",
	    (uintmax_t)phys_avail[j], (uintmax_t)phys_avail[j + 1]);
	return (ENXIO);
}

/*
 * Renumber the memory domains to be compact and zero-based if not
 * already.  Returns an error if there are too many domains.
 */
static int
renumber_domains(void)
{
	int i, j, slot;

	/* Enumerate all the domains. */
	ndomain = 0;
	for (i = 0; i < num_mem; i++) {
		/* See if this domain is already known. */
		for (j = 0; j < ndomain; j++) {
			if (domain_pxm[j] >= mem_info[i].domain)
				break;
		}
		if (j < ndomain && domain_pxm[j] == mem_info[i].domain)
			continue;

		if (ndomain >= MAXMEMDOM) {
			ndomain = 1;
			printf("SRAT: Too many memory domains\n");
			return (EFBIG);
		}

		/* Insert the new domain at slot 'j'. */
		slot = j;
		for (j = ndomain; j > slot; j--)
			domain_pxm[j] = domain_pxm[j - 1];
		domain_pxm[slot] = mem_info[i].domain;
		ndomain++;
	}

	/* Renumber each domain to its index in the sorted 'domain_pxm' list. */
	for (i = 0; i < ndomain; i++) {
		/*
		 * If the domain is already the right value, no need
		 * to renumber.
		 */
		if (domain_pxm[i] == i)
			continue;

		/* Walk the cpu[] and mem_info[] arrays to renumber. */
		for (j = 0; j < num_mem; j++)
			if (mem_info[j].domain == domain_pxm[i])
				mem_info[j].domain = i;
		for (j = 0; j <= max_apic_id; j++)
			if (cpus[j].enabled && cpus[j].domain == domain_pxm[i])
				cpus[j].domain = i;
	}

	return (0);
}

/*
 * Look for an ACPI System Resource Affinity Table ("SRAT")
 */
static int
parse_srat(void)
{
	unsigned int idx, size;
	vm_paddr_t addr;
	int error;

	if (resource_disabled("srat", 0))
		return (-1);

	srat_physaddr = acpi_find_table(ACPI_SIG_SRAT);
	if (srat_physaddr == 0)
		return (-1);

	/*
	 * Allocate data structure:
	 *
	 * Find the last physical memory region and steal some memory from
	 * it. This is done because at this point in the boot process
	 * malloc is still not usable.
	 */
	for (idx = 0; phys_avail[idx + 1] != 0; idx += 2);
	KASSERT(idx != 0, ("phys_avail is empty!"));
	idx -= 2;

	size =  sizeof(*cpus) * (max_apic_id + 1);
	addr = trunc_page(phys_avail[idx + 1] - size);
	KASSERT(addr >= phys_avail[idx],
	    ("Not enough memory for SRAT table items"));
	phys_avail[idx + 1] = addr - 1;

	/*
	 * We cannot rely on PHYS_TO_DMAP because this code is also used in
	 * i386, so use pmap_mapbios to map the memory, this will end up using
	 * the default memory attribute (WB), and the DMAP when available.
	 */
	cpus = (struct cpu_info *)pmap_mapbios(addr, size);
	bzero(cpus, size);

	/*
	 * Make a pass over the table to populate the cpus[] and
	 * mem_info[] tables.
	 */
	srat = acpi_map_table(srat_physaddr, ACPI_SIG_SRAT);
	error = 0;
	srat_walk_table(srat_parse_entry, &error);
	acpi_unmap_table(srat);
	srat = NULL;
	if (error || check_domains() != 0 || check_phys_avail() != 0 ||
	    renumber_domains() != 0) {
		srat_physaddr = 0;
		return (-1);
	}

	return (0);
}

static void
init_mem_locality(void)
{
	int i;

	/*
	 * For now, assume -1 == "no locality information for
	 * this pairing.
	 */
	for (i = 0; i < MAXMEMDOM * MAXMEMDOM; i++)
		vm_locality_table[i] = -1;
}

static void
parse_acpi_tables(void *dummy)
{

	if (parse_srat() < 0)
		return;
	init_mem_locality();
	(void)parse_slit();
	vm_phys_register_domains(ndomain, mem_info, vm_locality_table);
}
SYSINIT(parse_acpi_tables, SI_SUB_VM - 1, SI_ORDER_FIRST, parse_acpi_tables,
    NULL);

static void
srat_walk_table(acpi_subtable_handler *handler, void *arg)
{

	acpi_walk_subtables(srat + 1, (char *)srat + srat->Header.Length,
	    handler, arg);
}

/*
 * Setup per-CPU domain IDs.
 */
static void
srat_set_cpus(void *dummy)
{
	struct cpu_info *cpu;
	struct pcpu *pc;
	u_int i;

	if (srat_physaddr == 0)
		return;
	for (i = 0; i < MAXCPU; i++) {
		if (CPU_ABSENT(i))
			continue;
		pc = pcpu_find(i);
		KASSERT(pc != NULL, ("no pcpu data for CPU %u", i));
		cpu = &cpus[pc->pc_apic_id];
		if (!cpu->enabled)
			panic("SRAT: CPU with APIC ID %u is not known",
			    pc->pc_apic_id);
		pc->pc_domain = vm_ndomains > 1 ? cpu->domain : 0;
		CPU_SET(i, &cpuset_domain[pc->pc_domain]);
		if (bootverbose)
			printf("SRAT: CPU %u has memory domain %d\n", i,
			    pc->pc_domain);
	}

	/* Last usage of the cpus array, unmap it. */
	pmap_unmapbios((vm_offset_t)cpus, sizeof(*cpus) * (max_apic_id + 1));
	cpus = NULL;
}
SYSINIT(srat_set_cpus, SI_SUB_CPU, SI_ORDER_ANY, srat_set_cpus, NULL);

/*
 * Map a _PXM value to a VM domain ID.
 *
 * Returns the domain ID, or -1 if no domain ID was found.
 */
int
acpi_map_pxm_to_vm_domainid(int pxm)
{
	int i;

	for (i = 0; i < ndomain; i++) {
		if (domain_pxm[i] == pxm)
			return (vm_ndomains > 1 ? i : 0);
	}

	return (-1);
}

#else /* MAXMEMDOM == 1 */

int
acpi_map_pxm_to_vm_domainid(int pxm)
{

	return (-1);
}

#endif /* MAXMEMDOM > 1 */
