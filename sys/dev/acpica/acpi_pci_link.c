/*-
 * Copyright (c) 2002 Mitsuru IWASAKI <iwasaki@jp.freebsd.org>
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

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>

#include "acpi.h"

#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpi_pcibvar.h>

/*
 * Hooks for the ACPI CA debugging infrastructure
 */
#define _COMPONENT	ACPI_BUS
ACPI_MODULE_NAME("PCI_LINK")

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
};

TAILQ_HEAD(acpi_pci_link_entries, acpi_pci_link_entry);
static struct acpi_pci_link_entries acpi_pci_link_entries;

struct acpi_prt_entry {
	TAILQ_ENTRY(acpi_prt_entry) links;
	device_t	pcidev;
	int		busno;
	ACPI_PCI_ROUTING_TABLE prt;
	struct acpi_pci_link_entry *pci_link;
};

TAILQ_HEAD(acpi_prt_entries, acpi_prt_entry);
static struct acpi_prt_entries acpi_prt_entries;

static int	irq_penalty[MAX_ACPI_INTERRUPTS];

#define ACPI_STA_PRESENT	0x00000001
#define ACPI_STA_ENABLE		0x00000002
#define ACPI_STA_SHOWINUI	0x00000004
#define ACPI_STA_FUNCTIONAL	0x00000008

/*
 * PCI link object management
 */

static void
acpi_pci_link_entry_dump(struct acpi_prt_entry *entry)
{
	UINT8			i;
	ACPI_RESOURCE_IRQ	*Irq;

	if (entry == NULL || entry->pci_link == NULL) {
		return;
	}

	printf("%s irq %3d: ", acpi_name(entry->pci_link->handle),
	    entry->pci_link->current_irq);

	printf("[");
	for (i = 0; i < entry->pci_link->number_of_interrupts; i++) {
		printf("%3d", entry->pci_link->interrupts[i]);
	}
	printf("] ");

	Irq = NULL;
	switch (entry->pci_link->possible_resources.Id) {
	case ACPI_RSTYPE_IRQ:
		Irq = &entry->pci_link->possible_resources.Data.Irq;

		switch (Irq->ActiveHighLow) {
		case ACPI_ACTIVE_HIGH:
			printf("high,");
			break;

		case ACPI_ACTIVE_LOW:
			printf("low,");
			break;

		default:
			printf("unknown,");
			break;
		}
		
		switch (Irq->EdgeLevel) {
		case ACPI_EDGE_SENSITIVE:
			printf("edge,");
			break;

		case ACPI_LEVEL_SENSITIVE:
			printf("level,");
			break;

		default:
			printf("unknown,");
			break;
		}

		switch (Irq->SharedExclusive) {
		case ACPI_EXCLUSIVE:
			printf("exclusive");
			break;

		case ACPI_SHARED:
			printf("sharable");
			break;

		default:
			printf("unknown");
			break;
		}

		break;

	case ACPI_RSTYPE_EXT_IRQ:
		/* TBD */
		break;
	}

	printf(" %d.%d.%d", entry->busno,
	    (int)((entry->prt.Address & 0xffff0000) >> 16),
	    (int)entry->prt.Pin);

	printf("\n");
}

static ACPI_STATUS
acpi_pci_link_get_object_status(ACPI_HANDLE handle, UINT32 *sta)
{
	ACPI_DEVICE_INFO	devinfo;
	ACPI_BUFFER		buf = {sizeof(devinfo), &devinfo};
	ACPI_STATUS		error;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	if (handle == NULL || sta == NULL) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
		    "invalid argument\n"));
		return_ACPI_STATUS (AE_BAD_PARAMETER);
	}

	error = AcpiGetObjectInfo(handle, &buf);
	if (ACPI_FAILURE(error)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
		    "couldn't get object info %s - %s\n",
		    acpi_name(handle), AcpiFormatException(error)));
		return_ACPI_STATUS (error);
	}

	if ((devinfo.Valid & ACPI_VALID_HID) == 0 ||
	    strcmp(devinfo.HardwareId.Value, "PNP0C0F") != 0) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR, "invalid hardware ID - %s\n",
		    acpi_name(handle)));
		return_ACPI_STATUS (AE_TYPE);
	}

	if ((devinfo.Valid & ACPI_VALID_STA) != 0) {
		*sta = devinfo.CurrentStatus;
	} else {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN, "invalid status - %s\n",
		    acpi_name(handle)));
		*sta = 0;
	}

	return_ACPI_STATUS (AE_OK);
}

static ACPI_STATUS
acpi_pci_link_get_irq_resources(ACPI_RESOURCE *resources,
    UINT8 *number_of_interrupts, UINT8 interrupts[])
{
	UINT8			count;
	UINT8			i;
	UINT32			NumberOfInterrupts;
	UINT32			*Interrupts;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	if (resources == NULL || number_of_interrupts == NULL) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR, "invalid argument\n"));
		return_ACPI_STATUS (AE_BAD_PARAMETER);
	}

	*number_of_interrupts = 0;
	NumberOfInterrupts = 0;
	Interrupts = NULL;

	if (resources->Id == ACPI_RSTYPE_START_DPF)
		resources = ACPI_NEXT_RESOURCE(resources);

	if (resources->Id != ACPI_RSTYPE_IRQ &&
	    resources->Id != ACPI_RSTYPE_EXT_IRQ) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
		    "Resource is not an IRQ entry - %d\n", resources->Id));
		return_ACPI_STATUS (AE_TYPE);
	}

	switch (resources->Id) {
	case ACPI_RSTYPE_IRQ:
		NumberOfInterrupts = resources->Data.Irq.NumberOfInterrupts;
		Interrupts = resources->Data.Irq.Interrupts;
		break;

	case ACPI_RSTYPE_EXT_IRQ:
                NumberOfInterrupts = resources->Data.ExtendedIrq.NumberOfInterrupts;
                Interrupts = resources->Data.ExtendedIrq.Interrupts;
		break;
	}
	
	if (NumberOfInterrupts == 0) {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN, "Blank IRQ resource\n"));
		return_ACPI_STATUS (AE_NULL_ENTRY);
	}

	count = 0;
	for (i = 0; i < NumberOfInterrupts; i++) {
		if (i >= MAX_POSSIBLE_INTERRUPTS) {
			ACPI_DEBUG_PRINT((ACPI_DB_WARN, "too many IRQs %d\n", i));
			break;
		}

		if (Interrupts[i] == NULL) {
			ACPI_DEBUG_PRINT((ACPI_DB_WARN, "Invalid IRQ %d\n",
			    Interrupts[i]));
			continue;
		}
		interrupts[count] = Interrupts[i];
		count++;
	}
	*number_of_interrupts = count;

	return_ACPI_STATUS (AE_OK);
}

static ACPI_STATUS
acpi_pci_link_get_current_irq(struct acpi_pci_link_entry *link, UINT8 *irq)
{
	ACPI_STATUS		error;
	ACPI_BUFFER		buf;
	ACPI_RESOURCE		*resources;
	UINT8			number_of_interrupts;
	UINT8			interrupts[MAX_POSSIBLE_INTERRUPTS];;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	if (link == NULL || irq == NULL) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR, "invalid argument\n"));
		return_ACPI_STATUS (AE_BAD_PARAMETER);
	}

	*irq = 0;
	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_BUFFER;
	error = AcpiGetCurrentResources(link->handle, &buf);
	if (ACPI_FAILURE(error)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
		    "couldn't get PCI interrupt link device _CRS %s - %s\n",
		    acpi_name(link->handle), AcpiFormatException(error)));
		return_ACPI_STATUS (error);
	}
	if (buf.Pointer == NULL) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
		    "couldn't allocate memory - %s\n",
		    acpi_name(link->handle)));
		return_ACPI_STATUS (AE_NO_MEMORY);
	}

	resources = (ACPI_RESOURCE *) buf.Pointer;

	number_of_interrupts = 0;
	bzero(interrupts, sizeof(interrupts));
	error = acpi_pci_link_get_irq_resources(resources,
		    &number_of_interrupts, interrupts);
	AcpiOsFree(buf.Pointer);

	if (ACPI_FAILURE(error)) {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN,
		    "couldn't get current IRQ from PCI interrupt link %s - %s\n",
		    acpi_name(link->handle), AcpiFormatException(error)));
		return_ACPI_STATUS (error);
	}

	if (number_of_interrupts == 0) {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN,
		    "PCI interrupt link device _CRS data is corrupted - %s\n",
		    acpi_name(link->handle)));
		return_ACPI_STATUS (AE_NULL_ENTRY);
	}

	*irq = interrupts[0];

	return_ACPI_STATUS (AE_OK);
}

static ACPI_STATUS
acpi_pci_link_add_link(ACPI_HANDLE handle, struct acpi_prt_entry *entry)
{
	ACPI_STATUS		error;
	ACPI_BUFFER		buf;
	ACPI_RESOURCE		*resources;
	struct acpi_pci_link_entry *link;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	entry->pci_link = NULL;
	TAILQ_FOREACH(link, &acpi_pci_link_entries, links) {
		if (link->handle == handle) {
			entry->pci_link = link;
			link->references++;
			return_ACPI_STATUS (AE_OK);
		}
	}

	link = AcpiOsAllocate(sizeof(struct acpi_pci_link_entry));
	if (link == NULL) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
		    "couldn't allocate memory - %s\n", acpi_name(handle)));
		return_ACPI_STATUS (AE_NO_MEMORY);
	}

	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_BUFFER;

	bzero(link, sizeof(struct acpi_pci_link_entry));

	link->handle = handle;

	error = acpi_pci_link_get_current_irq(link, &link->current_irq);
	if (ACPI_FAILURE(error)) {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN,
		    "couldn't get current IRQ from PCI interrupt link %s - %s\n",
		    acpi_name(handle), AcpiFormatException(error)));
	}

	link->initial_irq = link->current_irq;

	error = AcpiGetPossibleResources(handle, &buf);
	if (ACPI_FAILURE(error)) {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN,
		    "couldn't get PCI interrupt link device _PRS data %s - %s\n",
		    acpi_name(handle), AcpiFormatException(error)));
		goto out;
	}

	if (buf.Pointer == NULL) {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN,
		    "_PRS nuffer is empty - %s\n", acpi_name(handle)));
		error = AE_NO_MEMORY;
		goto out;
	}

	resources = (ACPI_RESOURCE *) buf.Pointer;
	bcopy(resources, &link->possible_resources,
	    sizeof(link->possible_resources));

	error = acpi_pci_link_get_irq_resources(resources,
	    &link->number_of_interrupts, link->interrupts);
	if (ACPI_FAILURE(error)) {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN,
		    "couldn't get possible IRQs from PCI interrupt link %s - %s\n",
		    acpi_name(handle), AcpiFormatException(error)));
		goto out;
	}

	if (link->number_of_interrupts == 0) {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN,
		    "PCI interrupt link device _PRS data is corrupted - %s\n",
		    acpi_name(handle)));
		error = AE_NULL_ENTRY;
		goto out;
	}

	link->references++;

	TAILQ_INSERT_TAIL(&acpi_pci_link_entries, link, links);
	entry->pci_link = link;

	error = AE_OK;
out:
	if (buf.Pointer != NULL) {
		AcpiOsFree(buf.Pointer);
	}

	if (error != AE_OK && link != NULL) {
		AcpiOsFree(link);
	}

	return_ACPI_STATUS (error);
}

static ACPI_STATUS
acpi_pci_link_add_prt(device_t pcidev, ACPI_PCI_ROUTING_TABLE *prt, int busno)
{
	ACPI_HANDLE		handle;
	ACPI_STATUS		error;
	UINT32			sta;
	struct acpi_prt_entry	*entry;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	if ((prt == NULL) || (prt->Source == NULL) || (prt->Source[0] == '\0')) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
		    "couldn't handle this routing table - hardwired\n"));
		return_ACPI_STATUS (AE_BAD_PARAMETER);
	}

	error = AcpiGetHandle(acpi_get_handle(pcidev), prt->Source, &handle);
	if (ACPI_FAILURE(error)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
		    "couldn't get acpi handle - %s\n",
		    AcpiFormatException(error)));
		return_ACPI_STATUS (error);
	}

	error = acpi_pci_link_get_object_status(handle, &sta);
	if (ACPI_FAILURE(error)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
		    "couldn't get object status %s - %s\n",
		    acpi_name(handle), AcpiFormatException(error)));
		return_ACPI_STATUS (error);
	}

	if (!(sta & (ACPI_STA_PRESENT | ACPI_STA_FUNCTIONAL))) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
		    "PCI interrupt link is not functional - %s\n",
		    acpi_name(handle)));
		return_ACPI_STATUS (AE_ERROR);
	}

	TAILQ_FOREACH(entry, &acpi_prt_entries, links) {
		if (entry->busno == busno &&
		    entry->prt.Address == prt->Address &&
		    entry->prt.Pin == prt->Pin) {
			ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			    "PCI interrupt link entry already exists - %s\n",
			    acpi_name(handle)));
			return_ACPI_STATUS (AE_ALREADY_EXISTS);
		}
	}

	entry = AcpiOsAllocate(sizeof(struct acpi_prt_entry));
	if (entry == NULL) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
		    "couldn't allocate memory - %s\n", acpi_name(handle)));
		return_ACPI_STATUS (AE_NO_MEMORY);
	}

	bzero(entry, sizeof(struct acpi_prt_entry));

	entry->pcidev = pcidev;
	entry->busno = busno;
	bcopy(prt, &entry->prt, sizeof(entry->prt));

	error = acpi_pci_link_add_link(handle, entry);
	if (ACPI_FAILURE(error)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
		    "couldn't add prt entry to pci link %s - %s\n",
		    acpi_name(handle), AcpiFormatException(error)));
		goto out;
	}

	TAILQ_INSERT_TAIL(&acpi_prt_entries, entry, links);
	error = AE_OK;

out:
	if (error != AE_OK && entry != NULL) {
		AcpiOsFree(entry);
	}

	return_ACPI_STATUS (error);
}

static int
acpi_pci_link_is_valid_irq(struct acpi_pci_link_entry *link, UINT8 irq)
{
	UINT8			i;

	if (irq == 0) {
		return (0);
	}

	for (i = 0; i < link->number_of_interrupts; i++) {
		if (link->interrupts[i] == irq) {
			return (1);
		}
	}

	/* allow initial IRQ as valid one. */
	if (link->initial_irq == irq) {
		return (1);
	}

	return (0);
}

static ACPI_STATUS
acpi_pci_link_set_irq(struct acpi_pci_link_entry *link, UINT8 irq)
{
	ACPI_STATUS		error;
	ACPI_RESOURCE		resbuf;
	ACPI_BUFFER		crsbuf;
	UINT32			sta;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	if (!acpi_pci_link_is_valid_irq(link, irq)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
		    "couldn't set invalid IRQ %d - %s\n", irq,
		    acpi_name(link->handle)));
		return_ACPI_STATUS (AE_BAD_PARAMETER);
	}

	error = acpi_pci_link_get_current_irq(link, &link->current_irq);
	if (ACPI_FAILURE(error)) {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN,
		    "couldn't get current IRQ from PCI interrupt link %s - %s\n",
		    acpi_name(link->handle), AcpiFormatException(error)));
	}

	if (link->current_irq == irq) {
		return_ACPI_STATUS (AE_OK);
	}

	bzero(&resbuf, sizeof(resbuf));
	crsbuf.Pointer = NULL;
	resbuf.Id = ACPI_RSTYPE_IRQ;
	resbuf.Length = ACPI_SIZEOF_RESOURCE(ACPI_RESOURCE_IRQ);

	if (link->possible_resources.Id != ACPI_RSTYPE_IRQ &&
	    link->possible_resources.Id != ACPI_RSTYPE_EXT_IRQ) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
		    "Resource is not an IRQ entry %s - %d\n",
		    acpi_name(link->handle), link->possible_resources.Id));
		return_ACPI_STATUS (AE_TYPE);
	}

	switch (link->possible_resources.Id) {
	case ACPI_RSTYPE_IRQ:
		/* structure copy other fields */
		resbuf.Data.Irq = link->possible_resources.Data.Irq;
		break;

	case ACPI_RSTYPE_EXT_IRQ:
		/* XXX */
		resbuf.Data.Irq.EdgeLevel = ACPI_LEVEL_SENSITIVE;
		resbuf.Data.Irq.ActiveHighLow = ACPI_ACTIVE_LOW;
		resbuf.Data.Irq.SharedExclusive = ACPI_SHARED;
		break;
	}

	resbuf.Data.Irq.NumberOfInterrupts = 1;
	resbuf.Data.Irq.Interrupts[0] = irq;

	error = acpi_AppendBufferResource(&crsbuf, &resbuf);
	if (ACPI_FAILURE(error)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
		    "couldn't setup buffer by acpi_AppendBufferResource - %s\n",
		    acpi_name(link->handle)));
		return_ACPI_STATUS (error);
	}

	if (crsbuf.Pointer == NULL) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
		    "buffer setup by acpi_AppendBufferResource is corrupted - %s\n",
		    acpi_name(link->handle)));
		return_ACPI_STATUS (AE_NO_MEMORY);
	}

	error = AcpiSetCurrentResources(link->handle, &crsbuf);
	if (ACPI_FAILURE(error)) {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN,
		    "couldn't set PCI interrupt link device _SRS %s - %s\n",
		    acpi_name(link->handle), AcpiFormatException(error)));
		return_ACPI_STATUS (error);
	}

	AcpiOsFree(crsbuf.Pointer);
	link->current_irq = 0;

	error = acpi_pci_link_get_object_status(link->handle, &sta);
	if (ACPI_FAILURE(error)) {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN,
		    "couldn't get object status %s - %s\n",
		    acpi_name(link->handle), AcpiFormatException(error)));
		return_ACPI_STATUS (error);
	}

	if (!(sta & ACPI_STA_ENABLE)) {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN,
		    "PCI interrupt link is disabled - %s\n",
		    acpi_name(link->handle)));
		return_ACPI_STATUS (AE_ERROR);
	}

	error = acpi_pci_link_get_current_irq(link, &link->current_irq);
	if (ACPI_FAILURE(error)) {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN,
		    "couldn't get current IRQ from PCI interrupt link %s - %s\n",
		    acpi_name(link->handle), AcpiFormatException(error)));
		return_ACPI_STATUS (error);
	}

	if (link->current_irq == irq) {
		error = AE_OK;
	} else {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN,
		    "couldn't set IRQ %d to PCI interrupt link %d - %s\n",
		    irq, link->current_irq, acpi_name(link->handle)));

		link->current_irq = 0;
		error = AE_ERROR;
	}

	return_ACPI_STATUS (error);
}

/*
 * Auto arbitration for boot-disabled devices
 */

static void
acpi_pci_link_bootdisabled_dump(void)

{
	int			i;
	int			irq;
	struct acpi_pci_link_entry *link;

	TAILQ_FOREACH(link, &acpi_pci_link_entries, links) {
		/* boot-disabled link only. */
		if (link->current_irq != 0) {
			continue;
		}

		printf("%s:\n", acpi_name(link->handle));
		printf("	interrupts:	");
		for (i = 0; i < link->number_of_interrupts; i++) {
			irq = link->sorted_irq[i];
			printf("%6d", irq);
		}
		printf("\n");
		printf("	penalty:	");
		for (i = 0; i < link->number_of_interrupts; i++) {
			irq = link->sorted_irq[i];
			printf("%6d", irq_penalty[irq]);
		}
		printf("\n");
		printf("	references:	%d\n", link->references);
		printf("	priority:	%d\n", link->priority);
	}
}

static void
acpi_pci_link_init_irq_penalty(void)
{
	int			irq;

	bzero(irq_penalty, sizeof(irq_penalty));
	for (irq = 0; irq < MAX_ISA_INTERRUPTS; irq++) {
		/* 0, 1, 2, 8:	timer, keyboard, cascade */
		if (irq == 0 || irq == 1 || irq == 2 || irq == 8) {
			irq_penalty[irq] = 100000;
			continue;
		}

		/* 13, 14, 15:	npx, ATA controllers */
		if (irq == 13 || irq == 14 || irq == 15) {
			irq_penalty[irq] = 10000;
			continue;
		}

		/* 3,4,6,7,12:	typicially used by legacy hardware */
		if (irq == 3 || irq == 4 || irq == 6 || irq == 7 || irq == 12) {
			irq_penalty[irq] = 1000;
			continue;
		}
	}
}

static int
acpi_pci_link_is_irq_exclusive(ACPI_RESOURCE *res)
{
	if (res == NULL) {
		return (0);
	}

	if (res->Id != ACPI_RSTYPE_IRQ &&
	    res->Id != ACPI_RSTYPE_EXT_IRQ) {
		return (0);
	}

	if (res->Id == ACPI_RSTYPE_IRQ &&
	    res->Data.Irq.SharedExclusive == ACPI_EXCLUSIVE) {
		return (1);
	}

	if (res->Id == ACPI_RSTYPE_EXT_IRQ &&
	    res->Data.ExtendedIrq.SharedExclusive == ACPI_EXCLUSIVE) {
		return (1);
	}

	return (0);
}

static void
acpi_pci_link_update_irq_penalty(device_t dev, int busno)
{
	int			i;
	int			irq;
	int			rid;
	struct resource		*res;
	struct acpi_prt_entry	*entry;
	struct acpi_pci_link_entry *link;

	TAILQ_FOREACH(entry, &acpi_prt_entries, links) {
		if (entry->busno != busno) {
			continue;
		}

		link = entry->pci_link;
		if (link == NULL) {
			continue;	/* impossible... */
		}

		if (link->current_irq != 0) {
			/* not boot-disabled link, we will use this IRQ. */
			irq_penalty[link->current_irq] += 100;
			continue;
		}

		/* boot-disabled link */
		for (i = 0; i < link->number_of_interrupts; i++) {
			/* give 10 for each possible IRQs. */
			irq = link->interrupts[i];
			irq_penalty[irq] += 10;

			/* higher penalty if exclusive. */
			if (acpi_pci_link_is_irq_exclusive(&link->possible_resources)) {
				irq_penalty[irq] += 100;
			}

			/* XXX try to get this IRQ in non-sharable mode. */
			rid = 0;
			res = bus_alloc_resource(dev, SYS_RES_IRQ,
						 &rid, irq, irq, 1, 0);
			if (res != NULL) {
				bus_release_resource(dev, SYS_RES_IRQ,
				    rid, res);
			} else {
				/* this is in use, give 100. */
				irq_penalty[irq] += 100;
			}
		}

		/* initialize `sorted' possible IRQs. */
		bcopy(link->interrupts, link->sorted_irq,
		    sizeof(link->sorted_irq));
	}
}

static void
acpi_pci_link_set_bootdisabled_priority(void)
{
	int			sum_penalty;
	int			i;
	int			irq;
	struct acpi_pci_link_entry *link, *link_pri;
	TAILQ_HEAD(, acpi_pci_link_entry) sorted_list;

	if (bootverbose) {
		printf("---- before setting priority for links ------------\n");
		acpi_pci_link_bootdisabled_dump();
	}

	/* reset priority for all links. */
	TAILQ_FOREACH(link, &acpi_pci_link_entries, links) {
		link->priority = 0;
	}

	TAILQ_FOREACH(link, &acpi_pci_link_entries, links) {
		/* not boot-disabled link, give no chance to be arbitrated. */
		if (link->current_irq != 0) {
			link->priority = 0;
			continue;
		}

		/*
		 * Calculate the priority for each boot-disabled links.
		 * o IRQ penalty indicates difficulty to use. 
		 * o #references for devices indicates importance of the link.
		 * o #interrupts indicates flexibility of the link.
		 */
		sum_penalty = 0;
		for (i = 0; i < link->number_of_interrupts; i++) {
			irq = link->interrupts[i];
			sum_penalty += irq_penalty[irq];
		}

		link->priority = (sum_penalty * link->references) / link->number_of_interrupts;
	}

	/*
	 * Sort PCI links based on the priority.
	 * XXX Any other better ways rather than using work list?
	 */
	TAILQ_INIT(&sorted_list);
	while (!TAILQ_EMPTY(&acpi_pci_link_entries)) {
		link = TAILQ_FIRST(&acpi_pci_link_entries);
		/* find an entry which has the highest priority. */
		TAILQ_FOREACH(link_pri, &acpi_pci_link_entries, links) {
			if (link->priority < link_pri->priority) {
				link = link_pri;
			}
		}
		/* move to work list. */
		TAILQ_REMOVE(&acpi_pci_link_entries, link, links);
		TAILQ_INSERT_TAIL(&sorted_list, link, links);
	}

	while (!TAILQ_EMPTY(&sorted_list)) {
		/* move them back to the list, one by one... */
		link = TAILQ_FIRST(&sorted_list);
		TAILQ_REMOVE(&sorted_list, link, links);
		TAILQ_INSERT_TAIL(&acpi_pci_link_entries, link, links);
	}
}

static void
acpi_pci_link_fixup_bootdisabled_link(void)
{
	int			i, j;
	int			irq1, irq2;
	struct acpi_pci_link_entry *link;
	ACPI_STATUS		error;

	if (bootverbose) {
		printf("---- before fixup boot-disabled links -------------\n");
		acpi_pci_link_bootdisabled_dump();
	}

	TAILQ_FOREACH(link, &acpi_pci_link_entries, links) {
		/* ignore non boot-disabled links. */
		if (link->current_irq != 0) {
			continue;
		}

		/* sort IRQs based on their penalty descending. */
		for (i = 0; i < link->number_of_interrupts; i++) {
			irq1 = link->sorted_irq[i];
			for (j = i + 1; j < link->number_of_interrupts; j++) {
				irq2 = link->sorted_irq[j];
				if (irq_penalty[irq1] < irq_penalty[irq2]) {
					continue;
				}
				link->sorted_irq[i] = irq2;
				link->sorted_irq[j] = irq1;
				irq1 = irq2;
			}
		}

		/* try with lower penalty IRQ. */
		for (i = 0; i < link->number_of_interrupts - 1; i++) {
			irq1 = link->sorted_irq[i];
			error = acpi_pci_link_set_irq(link, irq1);
			if (error == AE_OK) {
				/* OK, we use this.  give another penalty. */
				irq_penalty[irq1] += 100 * link->references;
				break;
			}
			/* NG, try next IRQ... */
		}
	}

	if (bootverbose) {
		printf("---- after fixup boot-disabled links --------------\n");
		acpi_pci_link_bootdisabled_dump();
	}
}

/*
 * Public interface
 */

int
acpi_pci_link_config(device_t dev, ACPI_BUFFER *prtbuf, int busno)
{
	struct acpi_prt_entry	*entry;
	ACPI_PCI_ROUTING_TABLE	*prt;
	u_int8_t		*prtp;
	ACPI_STATUS		error;
	static int		first_time =1;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	if (acpi_disabled("pci_link")) {
		return (0);
	}

	if (first_time) {
		TAILQ_INIT(&acpi_prt_entries);
		TAILQ_INIT(&acpi_pci_link_entries);
		acpi_pci_link_init_irq_penalty();
		first_time = 0;
	}

	if (prtbuf == NULL) {
		return (-1);
	}

	prtp = prtbuf->Pointer;
	if (prtp == NULL) {		/* didn't get routing table */
		return (-1);
	}

	/* scan the PCI Routing Table */
	for (;;) {
		prt = (ACPI_PCI_ROUTING_TABLE *)prtp;

		if (prt->Length == 0)	/* end of table */
		    break;

		error = acpi_pci_link_add_prt(dev, prt, busno);
		if (ACPI_FAILURE(error)) {
			ACPI_DEBUG_PRINT((ACPI_DB_WARN,
			    "couldn't add PCI interrupt link entry - %s\n",
			    AcpiFormatException(error)));
		}

		/* skip to next entry */
		prtp += prt->Length;
	}

	if (bootverbose) {
		printf("---- initial configuration ------------------------\n");
		TAILQ_FOREACH(entry, &acpi_prt_entries, links) {
			if (entry->busno != busno) {
				continue;
			}

			acpi_pci_link_entry_dump(entry);
		}
	}

	/* manual configuration. */
	TAILQ_FOREACH(entry, &acpi_prt_entries, links) {
		UINT8			irq;
		char			*irqstr, *op;
		char			prthint[32];

		if (entry->busno != busno) {
			continue;
		}

		snprintf(prthint, sizeof(prthint),
		    "hw.acpi.pci.link.%d.%d.%d.irq", entry->busno,
		    (int)((entry->prt.Address & 0xffff0000) >> 16),
		    (int)entry->prt.Pin);

		irqstr = getenv(prthint);
		if (irqstr == NULL) {
			continue;
		}

		irq = strtoul(irqstr, &op, 0);
		if (*op != '\0') {
			continue;
		}

		if (acpi_pci_link_is_valid_irq(entry->pci_link, irq)) {
			error = acpi_pci_link_set_irq(entry->pci_link, irq);
			if (ACPI_FAILURE(error)) {
				ACPI_DEBUG_PRINT((ACPI_DB_WARN,
				    "couldn't set IRQ to PCI interrupt link entry %s - %s\n",
				    acpi_name(entry->pci_link->handle),
				    AcpiFormatException(error)));
			}
			continue;
		}

		/*
		 * Do auto arbitration for this device's PCI link
		 * if hint value 0 is specified.
		 */
		if (irq == 0) {
			entry->pci_link->current_irq = 0;
		}
	}

	/* auto arbitration */
	acpi_pci_link_update_irq_penalty(dev, busno);
	acpi_pci_link_set_bootdisabled_priority();
	acpi_pci_link_fixup_bootdisabled_link();

	if (bootverbose) {
		printf("---- arbitrated configuration ---------------------\n");
		TAILQ_FOREACH(entry, &acpi_prt_entries, links) {
			if (entry->busno != busno) {
				continue;
			}

			acpi_pci_link_entry_dump(entry);
		}
	}

	return (0);
}

int
acpi_pci_link_resume(device_t dev, ACPI_BUFFER *prtbuf, int busno)
{
	struct acpi_prt_entry	*entry;
	ACPI_STATUS		error;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	if (acpi_disabled("pci_link")) {
		return (0);
	}

	TAILQ_FOREACH(entry, &acpi_prt_entries, links) {
		if (entry->pcidev != dev) {
			continue;
		}

		error = acpi_pci_link_set_irq(entry->pci_link,
			    entry->pci_link->current_irq);
		if (ACPI_FAILURE(error)) {
			ACPI_DEBUG_PRINT((ACPI_DB_WARN,
			    "couldn't set IRQ to PCI interrupt link entry %s - %s\n",
			    acpi_name(entry->pci_link->handle),
			    AcpiFormatException(error)));
		}
	}

	return (0);
}

