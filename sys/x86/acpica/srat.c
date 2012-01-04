/*-
 * Copyright (c) 2010 Advanced Computing Technologies LLC
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/smp.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>
#include <vm/vm_phys.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/actables.h>

#include <machine/intr_machdep.h>
#include <machine/apicvar.h>

#include <dev/acpica/acpivar.h>

struct cpu_info {
	int enabled:1;
	int has_memory:1;
	int domain;
} cpus[MAX_APIC_ID + 1];

struct mem_affinity mem_info[VM_PHYSSEG_MAX + 1];
int num_mem;

static ACPI_TABLE_SRAT *srat;
static vm_paddr_t srat_physaddr;

static void	srat_walk_table(acpi_subtable_handler *handler, void *arg);

/*
 * Returns true if a memory range overlaps with at least one range in
 * phys_avail[].
 */
static int
overlaps_phys_avail(vm_paddr_t start, vm_paddr_t end)
{
	int i;

	for (i = 0; phys_avail[i] != 0 && phys_avail[i + 1] != 0; i += 2) {
		if (phys_avail[i + 1] < start)
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
		KASSERT(!cpus[cpu->ApicId].enabled,
		    ("Duplicate local APIC ID %u", cpu->ApicId));
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
		KASSERT(!cpus[x2apic->ApicId].enabled,
		    ("Duplicate local APIC ID %u", x2apic->ApicId));
		cpus[x2apic->ApicId].domain = x2apic->ProximityDomain;
		cpus[x2apic->ApicId].enabled = 1;
		break;
	case ACPI_SRAT_TYPE_MEMORY_AFFINITY:
		mem = (ACPI_SRAT_MEM_AFFINITY *)entry;
		if (bootverbose)
			printf(
		    "SRAT: Found memory domain %d addr %jx len %jx: %s\n",
			    mem->ProximityDomain, (uintmax_t)mem->BaseAddress,
			    (uintmax_t)mem->Length,
			    (mem->Flags & ACPI_SRAT_MEM_ENABLED) ?
			    "enabled" : "disabled");
		if (!(mem->Flags & ACPI_SRAT_MEM_ENABLED))
			break;
		if (!overlaps_phys_avail(mem->BaseAddress,
		    mem->BaseAddress + mem->Length)) {
			printf("SRAT: Ignoring memory at addr %jx\n",
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
		for (j = 0; j <= MAX_APIC_ID; j++)
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
	for (i = 0; i <= MAX_APIC_ID; i++)
		if (cpus[i].enabled && !cpus[i].has_memory) {
			printf("SRAT: No memory found for CPU %d\n", i);
			return (ENXIO);
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
	printf("SRAT: No memory region found for %jx - %jx\n",
	    (uintmax_t)phys_avail[j], (uintmax_t)phys_avail[j + 1]);
	return (ENXIO);
}

/*
 * Renumber the memory domains to be compact and zero-based if not
 * already.
 */
static void
renumber_domains(void)
{
	int domains[VM_PHYSSEG_MAX];
	int ndomain, i, j, slot;

	/* Enumerate all the domains. */
	ndomain = 0;
	for (i = 0; i < num_mem; i++) {
		/* See if this domain is already known. */
		for (j = 0; j < ndomain; j++) {
			if (domains[j] >= mem_info[i].domain)
				break;
		}
		if (j < ndomain && domains[j] == mem_info[i].domain)
			continue;

		/* Insert the new domain at slot 'j'. */
		slot = j;
		for (j = ndomain; j > slot; j--)
			domains[j] = domains[j - 1];
		domains[slot] = mem_info[i].domain;
	}

	/* Renumber each domain to its index in the sorted 'domains' list. */
	for (i = 0; i < ndomain; i++) {
		/*
		 * If the domain is already the right value, no need
		 * to renumber.
		 */
		if (domains[i] == i)
			continue;

		/* Walk the cpu[] and mem_info[] arrays to renumber. */
		for (j = 0; j < num_mem; j++)
			if (mem_info[j].domain == domains[i])
				mem_info[j].domain = i;
		for (j = 0; j <= MAX_APIC_ID; j++)
			if (cpus[j].enabled && cpus[j].domain == domains[i])
				cpus[j].domain = i;
	}
}

/*
 * Look for an ACPI System Resource Affinity Table ("SRAT")
 */
static void
parse_srat(void *dummy)
{
	int error;

	if (resource_disabled("srat", 0))
		return;

	srat_physaddr = acpi_find_table(ACPI_SIG_SRAT);
	if (srat_physaddr == 0)
		return;

	/*
	 * Make a pass over the table to populate the cpus[] and
	 * mem_info[] tables.
	 */
	srat = acpi_map_table(srat_physaddr, ACPI_SIG_SRAT);
	error = 0;
	srat_walk_table(srat_parse_entry, &error);
	acpi_unmap_table(srat);
	srat = NULL;
	if (error || check_domains() != 0 || check_phys_avail() != 0) {
		srat_physaddr = 0;
		return;
	}

	renumber_domains();

	/* Point vm_phys at our memory affinity table. */
	mem_affinity = mem_info;
}
SYSINIT(parse_srat, SI_SUB_VM - 1, SI_ORDER_FIRST, parse_srat, NULL);

static void
srat_walk_table(acpi_subtable_handler *handler, void *arg)
{

	acpi_walk_subtables(srat + 1, (char *)srat + srat->Header.Length,
	    handler, arg);
}

/*
 * Setup per-CPU ACPI IDs.
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
		pc->pc_domain = cpu->domain;
		if (bootverbose)
			printf("SRAT: CPU %u has memory domain %d\n", i,
			    cpu->domain);
	}
}
SYSINIT(srat_set_cpus, SI_SUB_CPU, SI_ORDER_ANY, srat_set_cpus, NULL);
