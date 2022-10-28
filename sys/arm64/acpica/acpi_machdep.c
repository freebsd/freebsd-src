/*-
 * Copyright (c) 2001 Mitsuru IWASAKI
 * Copyright (c) 2015 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
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

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/machdep.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/actables.h>

#include <dev/acpica/acpivar.h>

extern struct bus_space memmap_bus;

int
acpi_machdep_init(device_t dev)
{

	return (0);
}

int
acpi_machdep_quirks(int *quirks)
{

	return (0);
}

static void *
map_table(vm_paddr_t pa, const char *sig)
{
	ACPI_TABLE_HEADER *header;
	vm_size_t length;
	void *table;

	header = pmap_mapbios(pa, sizeof(ACPI_TABLE_HEADER));
	if (strncmp(header->Signature, sig, ACPI_NAMESEG_SIZE) != 0) {
		pmap_unmapbios(header, sizeof(ACPI_TABLE_HEADER));
		return (NULL);
	}
	length = header->Length;
	pmap_unmapbios(header, sizeof(ACPI_TABLE_HEADER));

	table = pmap_mapbios(pa, length);
	if (ACPI_FAILURE(AcpiUtChecksum(table, length))) {
		if (bootverbose)
			printf("ACPI: Failed checksum for table %s\n", sig);
#if (ACPI_CHECKSUM_ABORT)
		pmap_unmapbios(table, length);
		return (NULL);
#endif
	}
	return (table);
}

/*
 * See if a given ACPI table is the requested table.  Returns the
 * length of the table if it matches or zero on failure.
 */
static int
probe_table(vm_paddr_t address, const char *sig)
{
	ACPI_TABLE_HEADER *table;

	table = pmap_mapbios(address, sizeof(ACPI_TABLE_HEADER));
	if (table == NULL) {
		if (bootverbose)
			printf("ACPI: Failed to map table at 0x%jx\n",
			    (uintmax_t)address);
		return (0);
	}

	if (strncmp(table->Signature, sig, ACPI_NAMESEG_SIZE) != 0) {
		pmap_unmapbios(table, sizeof(ACPI_TABLE_HEADER));
		return (0);
	}
	pmap_unmapbios(table, sizeof(ACPI_TABLE_HEADER));
	return (1);
}

/* Unmap a table previously mapped via acpi_map_table(). */
void
acpi_unmap_table(void *table)
{
	ACPI_TABLE_HEADER *header;

	header = (ACPI_TABLE_HEADER *)table;
	pmap_unmapbios(table, header->Length);
}

/*
 * Try to map a table at a given physical address previously returned
 * by acpi_find_table().
 */
void *
acpi_map_table(vm_paddr_t pa, const char *sig)
{

	return (map_table(pa, sig));
}

/*
 * Return the physical address of the requested table or zero if one
 * is not found.
 */
vm_paddr_t
acpi_find_table(const char *sig)
{
	ACPI_PHYSICAL_ADDRESS rsdp_ptr;
	ACPI_TABLE_RSDP *rsdp;
	ACPI_TABLE_XSDT *xsdt;
	ACPI_TABLE_HEADER *table;
	vm_paddr_t addr;
	int i, count;

	if (resource_disabled("acpi", 0))
		return (0);

	/*
	 * Map in the RSDP.  Since ACPI uses AcpiOsMapMemory() which in turn
	 * calls pmap_mapbios() to find the RSDP, we assume that we can use
	 * pmap_mapbios() to map the RSDP.
	 */
	if ((rsdp_ptr = AcpiOsGetRootPointer()) == 0)
		return (0);
	rsdp = pmap_mapbios(rsdp_ptr, sizeof(ACPI_TABLE_RSDP));
	if (rsdp == NULL) {
		printf("ACPI: Failed to map RSDP\n");
		return (0);
	}

	addr = 0;
	if (rsdp->Revision >= 2 && rsdp->XsdtPhysicalAddress != 0) {
		/*
		 * AcpiOsGetRootPointer only verifies the checksum for
		 * the version 1.0 portion of the RSDP.  Version 2.0 has
		 * an additional checksum that we verify first.
		 */
		if (AcpiUtChecksum((UINT8 *)rsdp, ACPI_RSDP_XCHECKSUM_LENGTH)) {
			printf("ACPI: RSDP failed extended checksum\n");
			pmap_unmapbios(rsdp, sizeof(ACPI_TABLE_RSDP));
			return (0);
		}
		xsdt = map_table(rsdp->XsdtPhysicalAddress, ACPI_SIG_XSDT);
		if (xsdt == NULL) {
			printf("ACPI: Failed to map XSDT\n");
			pmap_unmapbios(rsdp, sizeof(ACPI_TABLE_RSDP));
			return (0);
		}
		count = (xsdt->Header.Length - sizeof(ACPI_TABLE_HEADER)) /
		    sizeof(UINT64);
		for (i = 0; i < count; i++)
			if (probe_table(xsdt->TableOffsetEntry[i], sig)) {
				addr = xsdt->TableOffsetEntry[i];
				break;
			}
		acpi_unmap_table(xsdt);
	} else {
		printf("ACPI: Unsupported RSDP version %d and XSDT %#lx\n",
		    rsdp->Revision, rsdp->XsdtPhysicalAddress);
	}
	pmap_unmapbios(rsdp, sizeof(ACPI_TABLE_RSDP));

	if (addr == 0)
		return (0);

	/*
	 * Verify that we can map the full table and that its checksum is
	 * correct, etc.
	 */
	table = map_table(addr, sig);
	if (table == NULL)
		return (0);
	acpi_unmap_table(table);

	return (addr);
}

int
acpi_map_addr(struct acpi_generic_address *addr, bus_space_tag_t *tag,
    bus_space_handle_t *handle, bus_size_t size)
{
	bus_addr_t phys;

	/* Check if the device is Memory mapped */
	if (addr->SpaceId != 0)
		return (ENXIO);

	phys = addr->Address;
	*tag = &memmap_bus;

	return (bus_space_map(*tag, phys, size, 0, handle));
}

#if MAXMEMDOM > 1
static void
parse_pxm_tables(void *dummy)
{
	uint64_t mmfr0, parange;

	/* Only parse ACPI tables when booting via ACPI */
	if (arm64_bus_method != ARM64_BUS_ACPI)
		return;

	if (!get_kernel_reg(ID_AA64MMFR0_EL1, &mmfr0)) {
		/* chosen arbitrarily */
		mmfr0 = ID_AA64MMFR0_PARange_1T;
	}

	switch (ID_AA64MMFR0_PARange_VAL(mmfr0)) {
	case ID_AA64MMFR0_PARange_4G:
		parange = (vm_paddr_t)4 << 30 /* GiB */;
		break;
	case ID_AA64MMFR0_PARange_64G:
		parange = (vm_paddr_t)64 << 30 /* GiB */;
		break;
	case ID_AA64MMFR0_PARange_1T:
		parange = (vm_paddr_t)1 << 40 /* TiB */;
		break;
	case ID_AA64MMFR0_PARange_4T:
		parange = (vm_paddr_t)4 << 40 /* TiB */;
		break;
	case ID_AA64MMFR0_PARange_16T:
		parange = (vm_paddr_t)16 << 40 /* TiB */;
		break;
	case ID_AA64MMFR0_PARange_256T:
		parange = (vm_paddr_t)256 << 40 /* TiB */;
		break;
	case ID_AA64MMFR0_PARange_4P:
		parange = (vm_paddr_t)4 << 50 /* PiB */;
		break;
	default:
		/* chosen arbitrarily */
		parange = (vm_paddr_t)1 << 40 /* TiB */;
		printf("Unknown value for PARange in mmfr0 (%#lx)\n", mmfr0);
		break;
	}

	acpi_pxm_init(MAXCPU, parange);
	acpi_pxm_parse_tables();
	acpi_pxm_set_mem_locality();
}
SYSINIT(parse_pxm_tables, SI_SUB_VM - 1, SI_ORDER_FIRST, parse_pxm_tables,
    NULL);
#endif
