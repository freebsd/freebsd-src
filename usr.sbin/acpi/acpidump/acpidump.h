/*-
 * Copyright (c) 1999 Doug Rabson
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
 *
 *	$FreeBSD$
 */

#ifndef _ACPIDUMP_H_
#define	_ACPIDUMP_H_

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/acconfig.h>
#include <contrib/dev/acpica/include/actbl1.h>

/* GAS address space ID constants. */
#define	ACPI_GAS_MEMORY		0
#define	ACPI_GAS_IO		1
#define	ACPI_GAS_PCI		2
#define	ACPI_GAS_EMBEDDED	3
#define	ACPI_GAS_SMBUS		4
#define	ACPI_GAS_CMOS		5
#define	ACPI_GAS_PCIBAR		6
#define	ACPI_GAS_DATATABLE	7
#define	ACPI_GAS_FIXED		0x7f

/* Subfields in the HPET Id member. */
#define	ACPI_HPET_ID_HARDWARE_REV_ID	0x000000ff
#define	ACPI_HPET_ID_COMPARATORS	0x00001f00
#define	ACPI_HPET_ID_COUNT_SIZE_CAP	0x00002000
#define	ACPI_HPET_ID_LEGACY_CAPABLE	0x00008000
#define	ACPI_HPET_ID_PCI_VENDOR_ID	0xffff0000

/* Find and map the RSD PTR structure and return it for parsing */
ACPI_TABLE_HEADER *sdt_load_devmem(void);

/*
 * Load the DSDT from a previous save file.  Note that other tables are
 * not saved (i.e. FADT)
 */
ACPI_TABLE_HEADER *dsdt_load_file(char *);

/* Save the DSDT to a file */
void	 dsdt_save_file(char *, ACPI_TABLE_HEADER *, ACPI_TABLE_HEADER *);

/* Print out as many fixed tables as possible, given the RSD PTR */
void	 sdt_print_all(ACPI_TABLE_HEADER *);

/* Disassemble the AML in the DSDT */
void	 aml_disassemble(ACPI_TABLE_HEADER *, ACPI_TABLE_HEADER *);

/* Routines for accessing tables in physical memory */
ACPI_TABLE_RSDP *acpi_find_rsd_ptr(void);
void	*acpi_map_physical(vm_offset_t, size_t);
ACPI_TABLE_HEADER *sdt_from_rsdt(ACPI_TABLE_HEADER *, const char *,
	    ACPI_TABLE_HEADER *);
ACPI_TABLE_HEADER *dsdt_from_fadt(ACPI_TABLE_FADT *);
int	 acpi_checksum(void *, size_t);

/* Command line flags */
extern int	dflag;
extern int	tflag;
extern int	vflag;

#endif	/* !_ACPIDUMP_H_ */
