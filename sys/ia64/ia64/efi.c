/*-
 * Copyright (c) 2001 Doug Rabson
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <machine/bootinfo.h>
#include <machine/efi.h>
#include <machine/sal.h>

EFI_SYSTEM_TABLE	*ia64_efi_systab;
EFI_RUNTIME_SERVICES	*ia64_efi_runtime;
u_int64_t		ia64_efi_acpi_table;
u_int64_t		ia64_efi_acpi20_table;

extern u_int64_t ia64_call_efi_physical(u_int64_t, u_int64_t, u_int64_t,
					u_int64_t, u_int64_t, u_int64_t);

static EFI_STATUS fake_efi_proc(void);

static EFI_RUNTIME_SERVICES fake_efi = {
	{ EFI_RUNTIME_SERVICES_SIGNATURE,
	  EFI_RUNTIME_SERVICES_REVISION,
	  0, 0, 0 },

	(EFI_GET_TIME)			fake_efi_proc,
	(EFI_SET_TIME)			fake_efi_proc,
	(EFI_GET_WAKEUP_TIME)		fake_efi_proc,
	(EFI_SET_WAKEUP_TIME)		fake_efi_proc,
	
	(EFI_SET_VIRTUAL_ADDRESS_MAP)	fake_efi_proc,
	(EFI_CONVERT_POINTER)		fake_efi_proc,

	(EFI_GET_VARIABLE)		fake_efi_proc,
	(EFI_GET_NEXT_VARIABLE_NAME)	fake_efi_proc,
	(EFI_SET_VARIABLE)		fake_efi_proc,

	(EFI_GET_NEXT_HIGH_MONO_COUNT)	fake_efi_proc,
	(EFI_RESET_SYSTEM)		fake_efi_proc
};

static EFI_STATUS
fake_efi_proc(void)
{
	return EFI_UNSUPPORTED;
}

void
ia64_efi_init(void)
{
	EFI_CONFIGURATION_TABLE *conf;
	struct sal_system_table *saltab = 0;
	EFI_RUNTIME_SERVICES *rs;
	EFI_MEMORY_DESCRIPTOR *md, *mdp;
	int mdcount, i;
	EFI_STATUS status;
	
	ia64_efi_runtime = &fake_efi;

	if (!bootinfo.bi_systab) {
		printf("No system table!\n");
		return;
	}

	mdcount = bootinfo.bi_memmap_size / bootinfo.bi_memdesc_size;
	md = (EFI_MEMORY_DESCRIPTOR *) IA64_PHYS_TO_RR7(bootinfo.bi_memmap);

	for (i = 0, mdp = md; i < mdcount; i++,
		 mdp = NextMemoryDescriptor(mdp, bootinfo.bi_memdesc_size)) {
		/*
		 * Relocate runtime memory segments for firmware.
		 */
		if (mdp->Attribute & EFI_MEMORY_RUNTIME) {
			if (mdp->Attribute & EFI_MEMORY_WB)
				mdp->VirtualStart =
					IA64_PHYS_TO_RR7(mdp->PhysicalStart);
			else if (mdp->Attribute & EFI_MEMORY_UC)
				mdp->VirtualStart =
					IA64_PHYS_TO_RR6(mdp->PhysicalStart);
		}
	}

	ia64_efi_systab = (EFI_SYSTEM_TABLE *)
		IA64_PHYS_TO_RR7(bootinfo.bi_systab);

	rs = (EFI_RUNTIME_SERVICES *)
		IA64_PHYS_TO_RR7((u_int64_t) ia64_efi_systab->RuntimeServices);
	ia64_efi_runtime = rs;

	status = ia64_call_efi_physical
	    ((u_int64_t) rs->SetVirtualAddressMap,
	     bootinfo.bi_memmap_size,
	     bootinfo.bi_memdesc_size,
	     bootinfo.bi_memdesc_version,
	     bootinfo.bi_memmap, 0);

	if (EFI_ERROR(status)) {
		/*
		 * We could wrap EFI in a virtual->physical shim here.
		 */
		printf("SetVirtualAddressMap returned 0x%lx\n", status);
		panic("Can't set firmware into virtual mode");
	}

	conf = (EFI_CONFIGURATION_TABLE *)
		IA64_PHYS_TO_RR7((u_int64_t) ia64_efi_systab->ConfigurationTable);
	for (i = 0; i < ia64_efi_systab->NumberOfTableEntries; i++) {
		static EFI_GUID sal = SAL_SYSTEM_TABLE_GUID;
		static EFI_GUID acpi = ACPI_TABLE_GUID;
		static EFI_GUID acpi20 = ACPI_20_TABLE_GUID;
		if (!memcmp(&conf[i].VendorGuid, &sal, sizeof(EFI_GUID)))
			saltab = (struct sal_system_table *)
			    IA64_PHYS_TO_RR7((u_int64_t) conf[i].VendorTable);
		if (!memcmp(&conf[i].VendorGuid, &acpi, sizeof(EFI_GUID)))
			ia64_efi_acpi_table = (u_int64_t) conf[i].VendorTable;
		if (!memcmp(&conf[i].VendorGuid, &acpi20, sizeof(EFI_GUID)))
			ia64_efi_acpi20_table = (u_int64_t) conf[i].VendorTable;
	}

	if (saltab)
		ia64_sal_init(saltab);

}

