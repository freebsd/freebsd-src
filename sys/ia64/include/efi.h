/*-
 * Copyright (c) 2000 Doug Rabson
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

#ifndef _MACHINE_EFI_H_
#define _MACHINE_EFI_H_

/*
 * Memory types.
 */
#define EFI_RESERVED_MEMORY_TYPE		0
#define EFI_LOADER_CODE				1
#define EFI_LOADER_DATA				2
#define EFI_BOOT_SERVICES_CODE			3
#define EFI_BOOT_SERVICES_DATA			4
#define EFI_RUNTIME_SERVICES_CODE		5
#define EFI_RUNTIME_SERVICES_DATA		6
#define EFI_CONVENTIONAL_MEMORY			7
#define EFI_UNUSABLE_MEMORY			8
#define EFI_ACPI_RECLAIM_MEMORY			9
#define EFI_ACPI_MEMORY_NVS			10
#define EFI_MEMORY_MAPPED_IO			11
#define EFI_MEMORY_MAPPED_IO_PORT_SPACE		12
#define EFI_PAL_CODE				13

struct efi_memory_descriptor {
	u_int32_t	emd_type;
	vm_offset_t	emd_physical_start;
	vm_offset_t	emd_virtul_start;
	u_int64_t	emd_number_of_pages;
	u_int64_t	emd_attribute;
};

/*
 * Values for emd_attribute.
 */
#define EFI_MEMORY_UC		0x0000000000000001
#define EFI_MEMORY_WC		0x0000000000000002
#define EFI_MEMORY_WT		0x0000000000000004
#define EFI_MEMORY_WB		0x0000000000000008
#define EFI_MEMORY_UCE		0x0000000000000010
#define EFI_MEMORY_WP		0x0000000000001000
#define EFI_MEMORY_RP		0x0000000000002000
#define EFI_MEMORY_XP		0x0000000000004000
#define EFI_MEMORY_RUNTIME	0x8000000000000000

#endif /* _MACHINE_EFI_H_ */
