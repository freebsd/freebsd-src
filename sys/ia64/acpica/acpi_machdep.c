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
#include <sys/bus.h>
#include <machine/md_var.h>
#include <machine/pal.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/actables.h>
#include <dev/acpica/acpivar.h>

int
acpi_machdep_init(device_t dev)
{
	struct	acpi_softc *sc;

        sc = device_get_softc(dev);

	acpi_install_wakeup_handler(sc);

	return (0);
}

int
acpi_machdep_quirks(int *quirks)
{
	return (0);
}

void
acpi_cpu_c1()
{
	ia64_call_pal_static(PAL_HALT_LIGHT, 0, 0, 0);
}

void *
acpi_find_table(const char *sig)
{
	ACPI_PHYSICAL_ADDRESS rsdp_ptr;
	ACPI_TABLE_RSDP *rsdp;
	ACPI_TABLE_XSDT *xsdt;
	ACPI_TABLE_HEADER *table;
	UINT64 addr;
	u_int i, count;

	if ((rsdp_ptr = AcpiOsGetRootPointer()) == 0)
		return (NULL);

	rsdp = (ACPI_TABLE_RSDP *)IA64_PHYS_TO_RR7(rsdp_ptr);
	xsdt = (ACPI_TABLE_XSDT *)IA64_PHYS_TO_RR7(rsdp->XsdtPhysicalAddress);

	count = (UINT64 *)((char *)xsdt + xsdt->Header.Length) -
	    xsdt->TableOffsetEntry;

	for (i = 0; i < count; i++) {
		addr = xsdt->TableOffsetEntry[i];
		table = (ACPI_TABLE_HEADER *)IA64_PHYS_TO_RR7(addr);

		if (strncmp(table->Signature, sig, ACPI_NAME_SIZE) != 0)
			continue;
		if (ACPI_FAILURE(AcpiTbChecksum((void *)table, table->Length)))
			continue;

		return (table);
	}

	return (NULL);
}
