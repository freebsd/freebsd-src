/*
 *  acpi_numa.c - ACPI NUMA support
 *
 *  Copyright (C) 2002 Takayoshi Kochi <t-kochi@bq.jp.nec.com>
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
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/acpi.h>

#define PREFIX			"ACPI: "

extern int __init acpi_table_parse_madt_family (enum acpi_table_id id, unsigned long madt_size, int entry_id, acpi_madt_entry_handler handler);

void __init
acpi_table_print_srat_entry (
	acpi_table_entry_header	*header)
{
	if (!header)
		return;

	switch (header->type) {

	case ACPI_SRAT_PROCESSOR_AFFINITY:
	{
		struct acpi_table_processor_affinity *p =
			(struct acpi_table_processor_affinity*) header;
		printk(KERN_INFO PREFIX "SRAT Processor (id[0x%02x] eid[0x%02x]) in proximity domain %d %s\n",
		       p->apic_id, p->lsapic_eid, p->proximity_domain,
		       p->flags.enabled?"enabled":"disabled");
	}
		break;

	case ACPI_SRAT_MEMORY_AFFINITY:
	{
		struct acpi_table_memory_affinity *p =
			(struct acpi_table_memory_affinity*) header;
		printk(KERN_INFO PREFIX "SRAT Memory (0x%08x%08x length 0x%08x%08x type 0x%x) in proximity domain %d %s%s\n",
		       p->base_addr_hi, p->base_addr_lo, p->length_hi, p->length_lo,
		       p->memory_type, p->proximity_domain,
		       p->flags.enabled ? "enabled" : "disabled",
		       p->flags.hot_pluggable ? " hot-pluggable" : "");
	}
		break;

	default:
		printk(KERN_WARNING PREFIX "Found unsupported SRAT entry (type = 0x%x)\n",
			header->type);
		break;
	}
}


static int __init
acpi_parse_slit (unsigned long phys_addr, unsigned long size)
{
	struct acpi_table_slit	*slit;
	u32			localities;

	if (!phys_addr || !size)
		return -EINVAL;

	slit = (struct acpi_table_slit *) __va(phys_addr);

	/* downcast just for %llu vs %lu for i386/ia64  */
	localities = (u32) slit->localities;

	printk(KERN_INFO PREFIX "SLIT localities %ux%u\n", localities, localities);

	acpi_numa_slit_init(slit);

	return 0;
}


static int __init
acpi_parse_processor_affinity (acpi_table_entry_header *header)
{
	struct acpi_table_processor_affinity *processor_affinity = NULL;

	processor_affinity = (struct acpi_table_processor_affinity*) header;
	if (!processor_affinity)
		return -EINVAL;

	acpi_table_print_srat_entry(header);

	/* let architecture-dependent part to do it */
	acpi_numa_processor_affinity_init(processor_affinity);

	return 0;
}


static int __init
acpi_parse_memory_affinity (acpi_table_entry_header *header)
{
	struct acpi_table_memory_affinity *memory_affinity = NULL;

	memory_affinity = (struct acpi_table_memory_affinity*) header;
	if (!memory_affinity)
		return -EINVAL;

	acpi_table_print_srat_entry(header);

	/* let architecture-dependent part to do it */
	acpi_numa_memory_affinity_init(memory_affinity);

	return 0;
}


static int __init
acpi_parse_srat (unsigned long phys_addr, unsigned long size)
{
	struct acpi_table_srat	*srat = NULL;

	if (!phys_addr || !size)
		return -EINVAL;

	srat = (struct acpi_table_srat *) __va(phys_addr);

	printk(KERN_INFO PREFIX "SRAT revision %d\n", srat->table_revision);

	return 0;
}


int __init
acpi_table_parse_srat (
	enum acpi_srat_entry_id	id,
	acpi_madt_entry_handler	handler)
{
	return acpi_table_parse_madt_family(ACPI_SRAT, sizeof(struct acpi_table_srat),
					    id, handler);
}


int __init
acpi_numa_init()
{
	int			result;

	/* SRAT: Static Resource Affinity Table */
	result = acpi_table_parse(ACPI_SRAT, acpi_parse_srat);

	if (result > 0) {
		result = acpi_table_parse_srat(ACPI_SRAT_PROCESSOR_AFFINITY,
					       acpi_parse_processor_affinity);
		result = acpi_table_parse_srat(ACPI_SRAT_MEMORY_AFFINITY,
					       acpi_parse_memory_affinity);
	} else {
		/* FIXME */
		printk("Warning: acpi_table_parse(ACPI_SRAT) returned %d!\n",result);
	}

	/* SLIT: System Locality Information Table */
	result = acpi_table_parse(ACPI_SLIT, acpi_parse_slit);
	if (result < 1) {
		/* FIXME */
		printk("Warning: acpi_table_parse(ACPI_SLIT) returned %d!\n",result);
	}

	acpi_numa_arch_fixup();
	return 0;
}
