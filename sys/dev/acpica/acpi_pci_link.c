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

#include <dev/pci/pcivar.h>
#include "pcib_if.h"

/* Hooks for the ACPI CA debugging infrastructure. */
#define _COMPONENT	ACPI_BUS
ACPI_MODULE_NAME("PCI_LINK")

TAILQ_HEAD(acpi_pci_link_entries, acpi_pci_link_entry);
static struct acpi_pci_link_entries acpi_pci_link_entries;
ACPI_SERIAL_DECL(pci_link, "ACPI PCI link");

TAILQ_HEAD(acpi_prt_entries, acpi_prt_entry);
static struct acpi_prt_entries acpi_prt_entries;

static int	irq_penalty[MAX_ACPI_INTERRUPTS];

static int	acpi_pci_link_is_valid_irq(struct acpi_pci_link_entry *link,
		    UINT8 irq);
static void	acpi_pci_link_update_irq_penalty(device_t dev, int busno);
static void	acpi_pci_link_set_bootdisabled_priority(void);
static void	acpi_pci_link_fixup_bootdisabled_link(void);

/*
 * PCI link object management
 */

static void
acpi_pci_link_dump_polarity(UINT32 ActiveHighLow)
{

	switch (ActiveHighLow) {
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
}

static void
acpi_pci_link_dump_trigger(UINT32 EdgeLevel)
{

	switch (EdgeLevel) {
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
}

static void
acpi_pci_link_dump_sharemode(UINT32 SharedExclusive)
{

	switch (SharedExclusive) {
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
}

static void
acpi_pci_link_entry_dump(struct acpi_prt_entry *entry)
{
	UINT8			i;
	ACPI_RESOURCE_IRQ	*Irq;
	ACPI_RESOURCE_EXT_IRQ	*ExtIrq;
	struct acpi_pci_link_entry *link;

	if (entry == NULL || entry->pci_link == NULL)
		return;
	link = entry->pci_link;

	printf("%s irq%c%2d: ", acpi_name(link->handle),
	    (link->flags & ACPI_LINK_ROUTED) ? '*' : ' ', link->current_irq);

	printf("[");
	if (link->number_of_interrupts)
		printf("%2d", link->interrupts[0]);
	for (i = 1; i < link->number_of_interrupts; i++)
		printf("%3d", link->interrupts[i]);
	printf("] %2d+ ", link->initial_irq);

	switch (link->possible_resources.Id) {
	case ACPI_RSTYPE_IRQ:
		Irq = &link->possible_resources.Data.Irq;
		acpi_pci_link_dump_polarity(Irq->ActiveHighLow);
		acpi_pci_link_dump_trigger(Irq->EdgeLevel);
		acpi_pci_link_dump_sharemode(Irq->SharedExclusive);
		break;
	case ACPI_RSTYPE_EXT_IRQ:
		ExtIrq = &link->possible_resources.Data.ExtendedIrq;
		acpi_pci_link_dump_polarity(ExtIrq->ActiveHighLow);
		acpi_pci_link_dump_trigger(ExtIrq->EdgeLevel);
		acpi_pci_link_dump_sharemode(ExtIrq->SharedExclusive);
		break;
	}

	printf(" %d.%d.%d\n", entry->busno,
	    (int)(ACPI_ADR_PCI_SLOT(entry->prt.Address)),
	    (int)entry->prt.Pin);
}

static ACPI_STATUS
acpi_pci_link_get_object_status(ACPI_HANDLE handle, UINT32 *sta)
{
	ACPI_DEVICE_INFO	*devinfo;
	ACPI_BUFFER		buf;
	ACPI_STATUS		error;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	if (handle == NULL || sta == NULL) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR, "invalid argument\n"));
		return_ACPI_STATUS (AE_BAD_PARAMETER);
	}

	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_BUFFER;
	error = AcpiGetObjectInfo(handle, &buf);
	if (ACPI_FAILURE(error)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
		    "couldn't get object info %s - %s\n",
		    acpi_name(handle), AcpiFormatException(error)));
		return_ACPI_STATUS (error);
	}

	devinfo = (ACPI_DEVICE_INFO *)buf.Pointer;
	if ((devinfo->Valid & ACPI_VALID_HID) == 0 ||
	    strcmp(devinfo->HardwareId.Value, "PNP0C0F") != 0) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR, "invalid hardware ID - %s\n",
		    acpi_name(handle)));
		AcpiOsFree(buf.Pointer);
		return_ACPI_STATUS (AE_TYPE);
	}

	if ((devinfo->Valid & ACPI_VALID_STA) != 0) {
		*sta = devinfo->CurrentStatus;
	} else {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN, "invalid status - %s\n",
		    acpi_name(handle)));
		*sta = 0;
	}

	AcpiOsFree(buf.Pointer);
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
		printf("acpi link get: resource %d is not an IRQ\n",
		    resources->Id);
		return_ACPI_STATUS (AE_TYPE);
	}

	switch (resources->Id) {
	case ACPI_RSTYPE_IRQ:
		NumberOfInterrupts = resources->Data.Irq.NumberOfInterrupts;
		Interrupts = resources->Data.Irq.Interrupts;
		break;
	case ACPI_RSTYPE_EXT_IRQ:
		NumberOfInterrupts =
		    resources->Data.ExtendedIrq.NumberOfInterrupts;
		Interrupts = resources->Data.ExtendedIrq.Interrupts;
		break;
	}

	if (NumberOfInterrupts == 0)
		return_ACPI_STATUS (AE_NULL_ENTRY);

	count = 0;
	for (i = 0; i < NumberOfInterrupts; i++) {
		if (i >= MAX_POSSIBLE_INTERRUPTS) {
			ACPI_DEBUG_PRINT((ACPI_DB_WARN, "too many IRQs (%d)\n",
			    i));
			break;
		}
		if (Interrupts[i] == 0) {
			ACPI_DEBUG_PRINT((ACPI_DB_WARN, "invalid IRQ %d\n",
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
		    "couldn't get current IRQ from interrupt link %s - %s\n",
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
	ACPI_SERIAL_ASSERT(pci_link);

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

	/*
	 * Get the IRQ configured at boot-time.  If successful, set this
	 * as the initial IRQ.
	 */
	error = acpi_pci_link_get_current_irq(link, &link->current_irq);
	if (ACPI_SUCCESS(error)) {
		link->initial_irq = link->current_irq;
	} else {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN,
		    "couldn't get current IRQ from interrupt link %s - %s\n",
		    acpi_name(handle), AcpiFormatException(error)));
		link->initial_irq = 0;
	}

	error = AcpiGetPossibleResources(handle, &buf);
	if (ACPI_FAILURE(error)) {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN,
		    "couldn't get interrupt link device _PRS data %s - %s\n",
		    acpi_name(handle), AcpiFormatException(error)));
		goto out;
	}
	if (buf.Pointer == NULL) {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN,
		    "_PRS buffer is empty - %s\n", acpi_name(handle)));
		error = AE_NO_MEMORY;
		goto out;
	}

	/* Skip any DPF descriptors.  XXX We should centralize this code. */
	resources = (ACPI_RESOURCE *) buf.Pointer;
	if (resources->Id == ACPI_RSTYPE_START_DPF)
		resources = ACPI_NEXT_RESOURCE(resources);

	/* XXX This only handles one resource, ignoring SourceIndex. */
	bcopy(resources, &link->possible_resources,
	    sizeof(link->possible_resources));

	error = acpi_pci_link_get_irq_resources(resources,
	    &link->number_of_interrupts, link->interrupts);
	if (ACPI_FAILURE(error)) {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN,
		    "couldn't get possible IRQs from interrupt link %s - %s\n",
		    acpi_name(handle), AcpiFormatException(error)));
		goto out;
	}

	if (link->number_of_interrupts == 0) {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN,
		    "interrupt link device _PRS data is corrupted - %s\n",
		    acpi_name(handle)));
		error = AE_NULL_ENTRY;
		goto out;
	}

	/*
	 * Try to disable this link.  If successful, set the current IRQ to
	 * zero and flags to indicate this link is not routed.  If we can't
	 * run _DIS (i.e., the method doesn't exist), assume the initial
	 * IRQ was routed by the BIOS.
	 *
	 * XXX Since we detect link devices via _PRT entries but run long
	 * after APIC mode has been enabled, we don't get a chance to
	 * disable links that will be unused (especially in APIC mode).
	 * Leaving them enabled can cause duplicate interrupts for some
	 * devices.  The right fix is to probe links via their PNPID, so we
	 * see them no matter what the _PRT says.
	 */
	if (ACPI_SUCCESS(AcpiEvaluateObject(handle, "_DIS", NULL, NULL))) {
		link->current_irq = 0;
		link->flags = ACPI_LINK_NONE;
	} else
		link->flags = ACPI_LINK_ROUTED;

	/*
	 * If the initial IRQ is invalid (not in _PRS), set it to 0 and
	 * mark this link as not routed.  We won't use it as the preferred
	 * interrupt later when we route.
	 */
	if (!acpi_pci_link_is_valid_irq(link, link->initial_irq) &&
	    link->initial_irq != 0) {
		printf("ACPI link %s has invalid initial irq %d, ignoring\n",
		    acpi_name(handle), link->initial_irq);
		link->initial_irq = 0;
		link->flags = ACPI_LINK_NONE;
	}

	link->references++;

	TAILQ_INSERT_TAIL(&acpi_pci_link_entries, link, links);
	entry->pci_link = link;

	error = AE_OK;
out:
	if (buf.Pointer != NULL)
		AcpiOsFree(buf.Pointer);
	if (error != AE_OK && link != NULL)
		AcpiOsFree(link);

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
	ACPI_SERIAL_ASSERT(pci_link);

	if (prt == NULL) {
		device_printf(pcidev, "NULL PRT entry\n");
		return_ACPI_STATUS (AE_BAD_PARAMETER);
	}

	/* Bail out if attempting to add a duplicate PRT entry. */
	TAILQ_FOREACH(entry, &acpi_prt_entries, links) {
		if (entry->busno == busno &&
		    entry->prt.Address == prt->Address &&
		    entry->prt.Pin == prt->Pin) {
			ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			    "PRT entry already exists\n"));
			return_ACPI_STATUS (AE_ALREADY_EXISTS);
		}
	}

	/* Allocate and initialize our new PRT entry. */
	entry = AcpiOsAllocate(sizeof(struct acpi_prt_entry));
	if (entry == NULL) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR, "can't allocate memory\n"));
		return_ACPI_STATUS (AE_NO_MEMORY);
	}
	bzero(entry, sizeof(struct acpi_prt_entry));

	/*
	 * If the source link is NULL, then this IRQ is hardwired so skip
	 * initializing the link but still add it to the list.
	 */
	if (prt->Source[0] != '\0') {
		/* Get a handle for the link source. */
		error = AcpiGetHandle(acpi_get_handle(pcidev), prt->Source,
		    &handle);
		if (ACPI_FAILURE(error)) {
			device_printf(pcidev, "get handle for %s - %s\n",
			    prt->Source, AcpiFormatException(error));
			goto out;
		}

		error = acpi_pci_link_get_object_status(handle, &sta);
		if (ACPI_FAILURE(error)) {
			device_printf(pcidev, "can't get status for %s - %s\n",
			    acpi_name(handle), AcpiFormatException(error));
			goto out;
		}

		/* Probe/initialize the link. */
		error = acpi_pci_link_add_link(handle, entry);
		if (ACPI_FAILURE(error)) {
			ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			    "couldn't add _PRT entry to link %s - %s\n",
			    acpi_name(handle), AcpiFormatException(error)));
			goto out;
		}
	}

	entry->pcidev = pcidev;
	entry->busno = busno;
	bcopy(prt, &entry->prt, sizeof(entry->prt));

	/*
	 * Make sure the Source value is null-terminated.  It is really a
	 * variable-length string (with a fixed size in the struct) so when
	 * we copy the entire struct, we truncate the string.  Instead of
	 * trying to make a variable-sized PRT object to handle the string,
	 * we store its handle in prt_source.  Callers should use that to
	 * look up the link object.
	 */
	entry->prt.Source[sizeof(prt->Source) - 1] = '\0';
	entry->prt_source = handle;

	TAILQ_INSERT_TAIL(&acpi_prt_entries, entry, links);
	error = AE_OK;

out:
	if (error != AE_OK && entry != NULL)
		AcpiOsFree(entry);

	return_ACPI_STATUS (error);
}

/*
 * Look up the given interrupt in the list of possible settings for
 * this link.  We don't special-case the initial link setting.  Some
 * systems return current settings that are outside the list of valid
 * settings so only allow choices explicitly specified in _PRS.
 */
static int
acpi_pci_link_is_valid_irq(struct acpi_pci_link_entry *link, UINT8 irq)
{
	UINT8			i;

	if (irq == 0)
		return (FALSE);

	/*
	 * Some systems have the initial irq set to the SCI but don't list
	 * it in the valid IRQs.  Add a special case to allow routing to the
	 * SCI if the system really wants to.  This is similar to how
	 * Windows often stacks all PCI IRQs on the SCI (and this is vital
	 * on some systems.)
	 */
	if (irq == AcpiGbl_FADT->SciInt)
		return (TRUE);

	for (i = 0; i < link->number_of_interrupts; i++) {
		if (link->interrupts[i] == irq)
			return (TRUE);
	}

	return (FALSE);
}

static ACPI_STATUS
acpi_pci_link_set_irq(struct acpi_pci_link_entry *link, UINT8 irq)
{
	ACPI_STATUS		error;
	ACPI_RESOURCE		resbuf;
	ACPI_BUFFER		crsbuf;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);
	ACPI_SERIAL_ASSERT(pci_link);

	/* Make sure the new IRQ is valid before routing. */
	if (!acpi_pci_link_is_valid_irq(link, irq)) {
		printf("acpi link set: invalid IRQ %d on %s\n",
		    irq, acpi_name(link->handle));
		return_ACPI_STATUS (AE_BAD_PARAMETER);
	}

	/* If this this link has already been routed, just return. */
	if (link->flags & ACPI_LINK_ROUTED) {
		printf("acpi link set: %s already routed to %d\n",
		    acpi_name(link->handle), link->current_irq);
		return_ACPI_STATUS (AE_OK);
	}

	/* Set up the IRQ resource for _SRS. */
	bzero(&resbuf, sizeof(resbuf));
	crsbuf.Pointer = NULL;

	switch (link->possible_resources.Id) {
	case ACPI_RSTYPE_IRQ:
		resbuf.Id = ACPI_RSTYPE_IRQ;
		resbuf.Length = ACPI_SIZEOF_RESOURCE(ACPI_RESOURCE_IRQ);

		/* structure copy other fields */
		resbuf.Data.Irq = link->possible_resources.Data.Irq;
		resbuf.Data.Irq.NumberOfInterrupts = 1;
		resbuf.Data.Irq.Interrupts[0] = irq;
		break;
	case ACPI_RSTYPE_EXT_IRQ:
		resbuf.Id = ACPI_RSTYPE_EXT_IRQ;
		resbuf.Length = ACPI_SIZEOF_RESOURCE(ACPI_RESOURCE_EXT_IRQ);

		/* structure copy other fields */
		resbuf.Data.ExtendedIrq =
		    link->possible_resources.Data.ExtendedIrq;
		resbuf.Data.ExtendedIrq.NumberOfInterrupts = 1;
		resbuf.Data.ExtendedIrq.Interrupts[0] = irq;
		break;
	default:
		printf("acpi link set: %s resource is not an IRQ (%d)\n",
		    acpi_name(link->handle), link->possible_resources.Id);
		return_ACPI_STATUS (AE_TYPE);
	}

	error = acpi_AppendBufferResource(&crsbuf, &resbuf);
	if (ACPI_FAILURE(error)) {
		printf("acpi link set: AppendBuffer failed for %s\n",
		    acpi_name(link->handle));
		return_ACPI_STATUS (error);
	}
	if (crsbuf.Pointer == NULL) {
		printf("acpi link set: AppendBuffer returned empty for %s\n",
		    acpi_name(link->handle));
		return_ACPI_STATUS (AE_NO_MEMORY);
	}

	/* Make the new IRQ active via the link's _SRS method. */
	error = AcpiSetCurrentResources(link->handle, &crsbuf);
	if (ACPI_FAILURE(error)) {
		printf("acpi link set: _SRS failed for link %s - %s\n",
		    acpi_name(link->handle), AcpiFormatException(error));
		goto out;
	}
	link->flags |= ACPI_LINK_ROUTED;
	link->current_irq = 0;

	/*
	 * Many systems always return invalid values for current settings
	 * (_CRS).  Since we can't trust the value returned, we have to
	 * assume we were successful.
	 */
	error = acpi_pci_link_get_current_irq(link, &link->current_irq);
	if (ACPI_FAILURE(error)) {
		if (bootverbose)
			printf("acpi link set: _CRS failed for link %s - %s\n",
			    acpi_name(link->handle),
			    AcpiFormatException(error));
		error = AE_OK;
	}
	if (link->current_irq != irq) {
		if (bootverbose)
			printf("acpi link set: curr irq %d != %d for %s\n",
			    link->current_irq, irq, acpi_name(link->handle));
		link->current_irq = irq;
	}

out:
	if (crsbuf.Pointer)
		AcpiOsFree(crsbuf.Pointer);
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

	ACPI_SERIAL_ASSERT(pci_link);
	TAILQ_FOREACH(link, &acpi_pci_link_entries, links) {
		/* boot-disabled link only. */
		if (link->current_irq != 0)
			continue;

		printf("%s (references %d, priority %d):\n",
		    acpi_name(link->handle), link->references, link->priority);
		printf("\tinterrupts:\t");
		for (i = 0; i < link->number_of_interrupts; i++) {
			irq = link->sorted_irq[i];
			printf("%6d", irq);
		}
		printf("\n");
		printf("\tpenalty:\t");
		for (i = 0; i < link->number_of_interrupts; i++) {
			irq = link->sorted_irq[i];
			printf("%6d", irq_penalty[irq]);
		}
		printf("\n");
	}
}

/*
 * Heuristics for choosing IRQs.  We start with some static penalties,
 * update them based on what IRQs are currently in use, then sort the
 * result.  This works ok but is not perfect.
 *
 * The PCI BIOS $PIR table offers "preferred PCI interrupts", but ACPI
 * doesn't seem to offer a similar mechanism, so picking a good
 * interrupt here is a difficult task.
 */
static void
acpi_pci_link_init_irq_penalty(void)
{

	bzero(irq_penalty, sizeof(irq_penalty));

	/* 0, 1, 2, 8:  timer, keyboard, cascade, RTC */
	irq_penalty[0] = 100000;
	irq_penalty[1] = 100000;
	irq_penalty[2] = 100000;
	irq_penalty[8] = 100000;

	/* 13, 14, 15:  npx, ATA controllers */
	irq_penalty[13] = 50000;
	irq_penalty[14] = 50000;
	irq_penalty[15] = 50000;

	/* 3, 4, 6, 7, 12:  typically used by legacy hardware */
	irq_penalty[3] =   5000;
	irq_penalty[4] =   5000;
	irq_penalty[6] =   5000;
	irq_penalty[7] =   5000;
	irq_penalty[12] =  5000;

	/* 5:  sometimes legacy sound cards */
	irq_penalty[5] =     50;
}

static int
link_exclusive(ACPI_RESOURCE *res)
{

	if (res == NULL ||
	    (res->Id != ACPI_RSTYPE_IRQ &&
	    res->Id != ACPI_RSTYPE_EXT_IRQ))
		return (FALSE);

	if ((res->Id == ACPI_RSTYPE_IRQ &&
	    res->Data.Irq.SharedExclusive == ACPI_EXCLUSIVE) ||
	    (res->Id == ACPI_RSTYPE_EXT_IRQ &&
	    res->Data.ExtendedIrq.SharedExclusive == ACPI_EXCLUSIVE))
		return (TRUE);

	return (FALSE);
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

	ACPI_SERIAL_ASSERT(pci_link);
	TAILQ_FOREACH(entry, &acpi_prt_entries, links) {
		if (entry->busno != busno)
			continue;

		/* Impossible? */
		link = entry->pci_link;
		if (link == NULL)
			continue;

		/* Update penalties for all possible settings of this link. */
		for (i = 0; i < link->number_of_interrupts; i++) {
			/* give 10 for each possible IRQs. */
			irq = link->interrupts[i];
			irq_penalty[irq] += 10;

			/* higher penalty if exclusive. */
			if (link_exclusive(&link->possible_resources))
				irq_penalty[irq] += 100;

			/* XXX try to get this IRQ in non-sharable mode. */
			rid = 0;
			res = bus_alloc_resource(dev, SYS_RES_IRQ,
						 &rid, irq, irq, 1, 0);
			if (res != NULL) {
				bus_release_resource(dev, SYS_RES_IRQ,
				    rid, res);
			} else {
				/* this is in use, give 10. */
				irq_penalty[irq] += 10;
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

	ACPI_SERIAL_ASSERT(pci_link);

	/* reset priority for all links. */
	TAILQ_FOREACH(link, &acpi_pci_link_entries, links)
		link->priority = 0;

	TAILQ_FOREACH(link, &acpi_pci_link_entries, links) {
		/* If already routed, don't include in arbitration. */
		if (link->flags & ACPI_LINK_ROUTED) {
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

		link->priority = (sum_penalty * link->references) /
		    link->number_of_interrupts;
	}

	/*
	 * Sort PCI links based on the priority.
	 * XXX Any other better ways rather than using work list?
	 */
	TAILQ_INIT(&sorted_list);
	while (!TAILQ_EMPTY(&acpi_pci_link_entries)) {
		link = TAILQ_FIRST(&acpi_pci_link_entries);
		/* find an entry which has the highest priority. */
		TAILQ_FOREACH(link_pri, &acpi_pci_link_entries, links)
			if (link->priority < link_pri->priority)
				link = link_pri;

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

	ACPI_SERIAL_ASSERT(pci_link);

	TAILQ_FOREACH(link, &acpi_pci_link_entries, links) {
		/* Ignore links that have been routed already. */
		if (link->flags & ACPI_LINK_ROUTED)
			continue;

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
	}

	if (bootverbose) {
		printf("ACPI PCI link arbitrated settings:\n");
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
	int			ret;
	static int		first_time = 1;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	if (acpi_disabled("pci_link"))
		return (0);

	ret = -1;
	ACPI_SERIAL_BEGIN(pci_link);
	if (first_time) {
		TAILQ_INIT(&acpi_prt_entries);
		TAILQ_INIT(&acpi_pci_link_entries);
		acpi_pci_link_init_irq_penalty();
		first_time = 0;
	}

	if (prtbuf == NULL)
		goto out;

	prtp = prtbuf->Pointer;
	if (prtp == NULL)		/* didn't get routing table */
		goto out;

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
		printf("ACPI PCI link initial configuration:\n");
		TAILQ_FOREACH(entry, &acpi_prt_entries, links) {
			if (entry->busno != busno)
				continue;
			acpi_pci_link_entry_dump(entry);
		}
	}

	/* manual configuration. */
	TAILQ_FOREACH(entry, &acpi_prt_entries, links) {
		int			irq;
		char			prthint[32];

		if (entry->busno != busno)
			continue;

		snprintf(prthint, sizeof(prthint),
		    "hw.acpi.pci.link.%d.%d.%d.irq", entry->busno,
		    (int)(ACPI_ADR_PCI_SLOT(entry->prt.Address)),
		    (int)entry->prt.Pin);

		if (getenv_int(prthint, &irq) == 0)
			continue;

		if (acpi_pci_link_is_valid_irq(entry->pci_link, irq)) {
			error = acpi_pci_link_set_irq(entry->pci_link, irq);
			if (ACPI_FAILURE(error)) {
				ACPI_DEBUG_PRINT((ACPI_DB_WARN,
				    "couldn't set IRQ to link entry %s - %s\n",
				    acpi_name(entry->pci_link->handle),
				    AcpiFormatException(error)));
			}
			continue;
		}

		/*
		 * Do auto arbitration for this device's PCI link
		 * if hint value 0 is specified.
		 */
		if (irq == 0)
			entry->pci_link->current_irq = 0;
	}
	ret = 0;

out:
	ACPI_SERIAL_END(pci_link);
	return (ret);
}

int
acpi_pci_link_resume(device_t dev)
{
	struct acpi_prt_entry	*entry;
	struct acpi_pci_link_entry *link;
	ACPI_STATUS		error;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	if (acpi_disabled("pci_link"))
		return (0);

	/* Walk through all PRT entries for this PCI bridge. */
	ACPI_SERIAL_BEGIN(pci_link);
	TAILQ_FOREACH(entry, &acpi_prt_entries, links) {
		if (entry->pcidev != dev || entry->pci_link == NULL)
			continue;
		link = entry->pci_link;

		/* If it's not routed, skip re-programming. */
		if ((link->flags & ACPI_LINK_ROUTED) == 0)
			continue;
		link->flags &= ~ACPI_LINK_ROUTED;

		/* Program it to the same setting as before suspend. */
		error = acpi_pci_link_set_irq(link, link->current_irq);
		if (ACPI_FAILURE(error)) {
			ACPI_DEBUG_PRINT((ACPI_DB_WARN,
			    "couldn't set IRQ to link entry %s - %s\n",
			    acpi_name(link->handle),
			    AcpiFormatException(error)));
		}
	}
	ACPI_SERIAL_END(pci_link);

	return (0);
}

/*
 * Look up a PRT entry for the given device.  We match based on the slot
 * number (high word of Address) and pin number (note that ACPI uses 0
 * for INTA).
 *
 * Note that the low word of the Address field (function number) is
 * required by the specification to be 0xffff.  We don't risk checking
 * it here.
 */
struct acpi_prt_entry *
acpi_pci_find_prt(device_t pcibdev, device_t dev, int pin)
{
	struct acpi_prt_entry *entry;
	ACPI_PCI_ROUTING_TABLE *prt;

	ACPI_SERIAL_BEGIN(pci_link);
	TAILQ_FOREACH(entry, &acpi_prt_entries, links) {
		prt = &entry->prt;
		if (entry->busno == pci_get_bus(dev) &&
		    ACPI_ADR_PCI_SLOT(prt->Address) == pci_get_slot(dev) &&
		    prt->Pin == pin)
			break;
	}
	ACPI_SERIAL_END(pci_link);
	return (entry);
}

/*
 * Perform the actual programming for this link.  We attempt to route an
 * IRQ, first the one set by the BIOS, and then a priority-sorted list.
 * Only do the programming once per link.
 */
int
acpi_pci_link_route(device_t dev, struct acpi_prt_entry *prt)
{
	struct acpi_pci_link_entry *link;
	int busno, i, irq;
	ACPI_RESOURCE crsres;
	ACPI_STATUS status;

	busno = pci_get_bus(dev);
	link = prt->pci_link;
	irq = PCI_INVALID_IRQ;
	ACPI_SERIAL_BEGIN(pci_link);
	if (link == NULL || link->number_of_interrupts == 0)
		goto out;

	/* If already routed, just return the current setting. */
	if (link->flags & ACPI_LINK_ROUTED) {
		irq = link->current_irq;
		goto out;
	}

	/* Update all IRQ weights to determine our priority list. */
	acpi_pci_link_update_irq_penalty(prt->pcidev, busno);
	acpi_pci_link_set_bootdisabled_priority();
	acpi_pci_link_fixup_bootdisabled_link();

	/*
	 * First, attempt to route the initial IRQ, if valid, since it was
	 * the one set up by the BIOS.  If this fails, route according to
	 * our priority-sorted list of IRQs.
	 */
	status = AE_NOT_FOUND;
	irq = link->initial_irq;
	if (irq)
		status = acpi_pci_link_set_irq(link, irq);
	for (i = 0; ACPI_FAILURE(status) && i < link->number_of_interrupts;
	    i++) {
		irq = link->sorted_irq[i];
		status = acpi_pci_link_set_irq(link, irq);
		if (ACPI_FAILURE(status)) {
			device_printf(dev, "_SRS failed, irq %d via %s\n",
			    irq, acpi_name(link->handle));
		}
	}
	if (ACPI_FAILURE(status)) {
		irq = PCI_INVALID_IRQ;
		goto out;
	}

	/* Update the penalty now that there's another user for this IRQ. */
	irq_penalty[irq] += 10 * link->references;

	/* Configure trigger/polarity for the new IRQ. */
	bcopy(&link->possible_resources, &crsres, sizeof(crsres));
	if (crsres.Id == ACPI_RSTYPE_IRQ) {
		crsres.Data.Irq.NumberOfInterrupts = 1;
		crsres.Data.Irq.Interrupts[0] = irq;
	} else {
		crsres.Data.ExtendedIrq.NumberOfInterrupts = 1;
		crsres.Data.ExtendedIrq.Interrupts[0] = irq;
	}
	acpi_config_intr(dev, &crsres);

out:
	ACPI_SERIAL_END(pci_link);
	return (irq);
}
