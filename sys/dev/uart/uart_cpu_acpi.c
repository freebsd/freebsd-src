/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016 The FreeBSD Foundation
 * Copyright (c) 2019 Colin Percival
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>
#include <dev/uart/uart_cpu_acpi.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/actables.h>

static struct acpi_uart_compat_data *
uart_cpu_acpi_scan(uint8_t interface_type)
{
	struct acpi_uart_compat_data **cd, *curcd;
	int i;

	SET_FOREACH(cd, uart_acpi_class_and_device_set) {
		curcd = *cd;
		for (i = 0; curcd[i].cd_hid != NULL; i++) {
			if (curcd[i].cd_port_subtype == interface_type)
				return (&curcd[i]);
		}
	}

	SET_FOREACH(cd, uart_acpi_class_set) {
		curcd = *cd;
		for (i = 0; curcd[i].cd_hid != NULL; i++) {
			if (curcd[i].cd_port_subtype == interface_type)
				return (&curcd[i]);
		}
	}

	return (NULL);
}

static int
uart_cpu_acpi_init_devinfo(struct uart_devinfo *di, struct uart_class *class,
    ACPI_GENERIC_ADDRESS *addr)
{
	/* Fill in some fixed details. */
	di->bas.chan = 0;
	di->bas.rclk = 0;
	di->databits = 8;
	di->stopbits = 1;
	di->parity = UART_PARITY_NONE;
	di->ops = uart_getops(class);

	/* Fill in details from SPCR table. */
	switch (addr->SpaceId) {
	case 0:
		di->bas.bst = uart_bus_space_mem;
		break;
	case 1:
		di->bas.bst = uart_bus_space_io;
		break;
	default:
		printf("UART in unrecognized address space: %d!\n",
		    (int)addr->SpaceId);
		return (ENXIO);
	}
	switch (addr->AccessWidth) {
	case 0: /* EFI_ACPI_6_0_UNDEFINED */
		/* FALLTHROUGH */
	case 1: /* EFI_ACPI_6_0_BYTE */
		di->bas.regiowidth = 1;
		break;
	case 2: /* EFI_ACPI_6_0_WORD */
		di->bas.regiowidth = 2;
		break;
	case 3: /* EFI_ACPI_6_0_DWORD */
		di->bas.regiowidth = 4;
		break;
	case 4: /* EFI_ACPI_6_0_QWORD */
		di->bas.regiowidth = 8;
		break;
	default:
		printf("UART unsupported access width: %d!\n",
		    (int)addr->AccessWidth);
		return (ENXIO);
	}
	switch (addr->BitWidth) {
	case 0:
		/* FALLTHROUGH */
	case 8:
		di->bas.regshft = 0;
		break;
	case 16:
		di->bas.regshft = 1;
		break;
	case 32:
		di->bas.regshft = 2;
		break;
	case 64:
		di->bas.regshft = 3;
		break;
	default:
		printf("UART unsupported bit width: %d!\n",
		    (int)addr->BitWidth);
		return (ENXIO);
	}

	return (0);
}

static int
uart_cpu_acpi_spcr(int devtype, struct uart_devinfo *di)
{
	vm_paddr_t spcr_physaddr;
	ACPI_TABLE_SPCR *spcr;
	struct acpi_uart_compat_data *cd;
	struct uart_class *class;
	int error = ENXIO;

	/* Look for the SPCR table. */
	spcr_physaddr = acpi_find_table(ACPI_SIG_SPCR);
	if (spcr_physaddr == 0)
		return (error);
	spcr = acpi_map_table(spcr_physaddr, ACPI_SIG_SPCR);
	if (spcr == NULL) {
		printf("Unable to map the SPCR table!\n");
		return (error);
	}

	/* Search for information about this SPCR interface type. */
	cd = uart_cpu_acpi_scan(spcr->InterfaceType);
	if (cd == NULL)
		goto out;
	class = cd->cd_class;

	error = uart_cpu_acpi_init_devinfo(di, class, &spcr->SerialPort);
	if (error != 0)
		goto out;

	switch (spcr->BaudRate) {
	case 0:
		/* Special value; means "keep current value unchanged". */
		di->baudrate = 0;
		break;
	case 3:
		di->baudrate = 9600;
		break;
	case 4:
		di->baudrate = 19200;
		break;
	case 6:
		di->baudrate = 57600;
		break;
	case 7:
		di->baudrate = 115200;
		break;
	default:
		printf("SPCR has reserved BaudRate value: %d!\n",
		    (int)spcr->BaudRate);
		goto out;
	}
	if (spcr->PciVendorId != PCIV_INVALID &&
	    spcr->PciDeviceId != PCIV_INVALID) {
		di->pci_info.vendor = spcr->PciVendorId;
		di->pci_info.device = spcr->PciDeviceId;
	}

	/* Apply device tweaks. */
	if ((cd->cd_quirks & UART_F_IGNORE_SPCR_REGSHFT) ==
	    UART_F_IGNORE_SPCR_REGSHFT) {
		di->bas.regshft = cd->cd_regshft;
	}

	/* Create a bus space handle. */
	error = bus_space_map(di->bas.bst, spcr->SerialPort.Address,
	    uart_getrange(class), 0, &di->bas.bsh);

out:
	acpi_unmap_table(spcr);
	return (error);
}

static int
uart_cpu_acpi_dbg2(struct uart_devinfo *di)
{
	vm_paddr_t dbg2_physaddr;
	ACPI_TABLE_DBG2 *dbg2;
	ACPI_DBG2_DEVICE *dbg2_dev;
	ACPI_GENERIC_ADDRESS *base_address;
	struct acpi_uart_compat_data *cd;
	struct uart_class *class;
	int error;
	bool found;

	/* Look for the DBG2 table. */
	dbg2_physaddr = acpi_find_table(ACPI_SIG_DBG2);
	if (dbg2_physaddr == 0)
		return (ENXIO);

	dbg2 = acpi_map_table(dbg2_physaddr, ACPI_SIG_DBG2);
	if (dbg2 == NULL) {
		printf("Unable to map the DBG2 table!\n");
		return (ENXIO);
	}

	error = ENXIO;

	dbg2_dev = (ACPI_DBG2_DEVICE *)((uintptr_t)dbg2 + dbg2->InfoOffset);
	found = false;
	while ((uintptr_t)dbg2_dev + dbg2_dev->Length <=
	    (uintptr_t)dbg2 + dbg2->Header.Length) {
		if (dbg2_dev->PortType != ACPI_DBG2_SERIAL_PORT)
			goto next;

		/* XXX: Too restrictive? */
		if (dbg2_dev->RegisterCount != 1)
			goto next;

		cd = uart_cpu_acpi_scan(dbg2_dev->PortSubtype);
		if (cd == NULL)
			goto next;

		class = cd->cd_class;
		base_address = (ACPI_GENERIC_ADDRESS *)
		    ((uintptr_t)dbg2_dev + dbg2_dev->BaseAddressOffset);

		error = uart_cpu_acpi_init_devinfo(di, class, base_address);
		if (error == 0) {
			found = true;
			break;
		}

next:
		dbg2_dev = (ACPI_DBG2_DEVICE *)
		    ((uintptr_t)dbg2_dev + dbg2_dev->Length);
	}
	if (!found)
		goto out;

	/* XXX: Find the correct value */
	di->baudrate = 115200;

	/* Apply device tweaks. */
	if ((cd->cd_quirks & UART_F_IGNORE_SPCR_REGSHFT) ==
	    UART_F_IGNORE_SPCR_REGSHFT) {
		di->bas.regshft = cd->cd_regshft;
	}

	/* Create a bus space handle. */
	error = bus_space_map(di->bas.bst, base_address->Address,
	    uart_getrange(class), 0, &di->bas.bsh);

out:
	acpi_unmap_table(dbg2);
	return (error);
}

int
uart_cpu_acpi_setup(int devtype, struct uart_devinfo *di)
{
	char *cp;

	switch(devtype) {
	case UART_DEV_CONSOLE:
		return (uart_cpu_acpi_spcr(devtype, di));
	case UART_DEV_DBGPORT:
		/* Use the Debug Port Table 2 (DBG2) to find a debug uart */
		cp = kern_getenv("hw.acpi.enable_dbg2");
		if (cp != NULL && strcasecmp(cp, "yes") == 0)
			return (uart_cpu_acpi_dbg2(di));
		break;
	}
	return (ENXIO);
}
