/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
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

#ifndef	_ACPI_PCIBVAR_H_
#define	_ACPI_PCIBVAR_H_

int	acpi_pcib_attach(device_t bus, ACPI_BUFFER *prt, int busno);
int	acpi_pcib_route_interrupt(device_t pcib, device_t dev, int pin);
int	acpi_pcib_resume(device_t dev);

#define MAX_POSSIBLE_INTERRUPTS	16
#define MAX_ISA_INTERRUPTS	16
#define MAX_ACPI_INTERRUPTS	255

struct acpi_pci_link_entry {
	TAILQ_ENTRY(acpi_pci_link_entry) links;
	ACPI_HANDLE	handle;
	UINT8		current_irq;
	UINT8		initial_irq;
	ACPI_RESOURCE	possible_resources;
	UINT8		number_of_interrupts;
	UINT8		interrupts[MAX_POSSIBLE_INTERRUPTS];
	UINT8		sorted_irq[MAX_POSSIBLE_INTERRUPTS];
	int		references;
	int		priority; 
	int		flags;
#define ACPI_LINK_NONE		0
#define ACPI_LINK_ROUTED	(1 << 0)
};

struct acpi_prt_entry {
	TAILQ_ENTRY(acpi_prt_entry) links;
	device_t	pcidev;
	int		busno;
	ACPI_PCI_ROUTING_TABLE prt;
	ACPI_HANDLE	prt_source;
	struct acpi_pci_link_entry *pci_link;
};

int	acpi_pci_link_config(device_t pcib, ACPI_BUFFER *prt, int busno);
int	acpi_pci_link_resume(device_t pcib);
struct	acpi_prt_entry *acpi_pci_find_prt(device_t pcibdev, device_t dev,
	    int pin);
int	acpi_pci_link_route(device_t dev, struct acpi_prt_entry *prt);

#endif
