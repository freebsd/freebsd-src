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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include "acpi.h"
#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpi_pcibvar.h>

#include <machine/pci_cfgreg.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include "pcib_if.h"

/* Hooks for the ACPI CA debugging infrastructure. */
#define _COMPONENT	ACPI_BUS
ACPI_MODULE_NAME("PCI_LINK")

ACPI_SERIAL_DECL(pci_link, "ACPI PCI link");

#define NUM_ISA_INTERRUPTS	16
#define NUM_ACPI_INTERRUPTS	256

/*
 * An ACPI PCI link device may contain multiple links.  Each link has its
 * own ACPI resource.  _PRT entries specify which link is being used via
 * the Source Index.
 */

struct link;

struct acpi_pci_link_softc {
	int	pl_num_links;
	struct link *pl_links;
};

struct link {
	struct acpi_pci_link_softc *l_sc;
	uint8_t	l_bios_irq;
	uint8_t	l_irq;
	uint8_t	l_initial_irq;
	int	l_res_index;
	int	l_num_irqs;
	int	*l_irqs;
	int	l_references;
	int	l_routed:1;
	int	l_isa_irq:1;
	ACPI_RESOURCE l_prs_template;
};

struct link_res_request {
	struct acpi_pci_link_softc *sc;
	int	count;
};

MALLOC_DEFINE(M_PCI_LINK, "PCI Link", "ACPI PCI Link structures");

static int pci_link_interrupt_weights[NUM_ACPI_INTERRUPTS];
static int pci_link_bios_isa_irqs;

static char *pci_link_ids[] = { "PNP0C0F", NULL };

/*
 * Fetch the short name associated with an ACPI handle and save it in the
 * passed in buffer.
 */
static ACPI_STATUS
acpi_short_name(ACPI_HANDLE handle, char *buffer, size_t buflen)
{
	ACPI_BUFFER buf;

	buf.Length = buflen;
	buf.Pointer = buffer;
	return (AcpiGetName(handle, ACPI_SINGLE_NAME, &buf));
}

static int
acpi_pci_link_probe(device_t dev)
{
	char descr[64], name[10];

	/*
	 * We explicitly do not check _STA since not all systems set it to
	 * sensible values.
	 */
	if (!acpi_disabled("pci_link") &&
	    ACPI_ID_PROBE(device_get_parent(dev), dev, pci_link_ids) != NULL) {
		if (ACPI_FAILURE(acpi_short_name(acpi_get_handle(dev), name,
			    sizeof(name))))
			device_set_desc(dev, "ACPI PCI Link");
		else {
			snprintf(descr, sizeof(descr), "ACPI PCI Link %s",
			    name);
			device_set_desc_copy(dev, descr);
		}
		return (0);
	}
	return (ENXIO);
}

static ACPI_STATUS
acpi_count_irq_resources(ACPI_RESOURCE *res, void *context)
{
	int *count;

	count = (int *)context;
	switch (res->Id) {
	case ACPI_RSTYPE_IRQ:
	case ACPI_RSTYPE_EXT_IRQ:
		(*count)++;
	}
	return (AE_OK);
}

static ACPI_STATUS
link_add_crs(ACPI_RESOURCE *res, void *context)
{
	struct link_res_request *req;
	struct link *link;

	ACPI_SERIAL_ASSERT(pci_link);
	req = (struct link_res_request *)context;
	switch (res->Id) {
	case ACPI_RSTYPE_IRQ:
	case ACPI_RSTYPE_EXT_IRQ:
		KASSERT(req->count < req->sc->pl_num_links,
			("link_add_crs: array boundary violation"));
		link = &req->sc->pl_links[req->count];
		req->count++;
		if (res->Id == ACPI_RSTYPE_IRQ) {
			if (res->Data.Irq.NumberOfInterrupts > 0) {
				KASSERT(res->Data.Irq.NumberOfInterrupts == 1,
				    ("%s: too many interrupts", __func__));
				link->l_irq = res->Data.Irq.Interrupts[0];
			}
		} else if (res->Data.ExtendedIrq.NumberOfInterrupts > 0) {
			KASSERT(res->Data.ExtendedIrq.NumberOfInterrupts == 1,
			    ("%s: too many interrupts", __func__));
			link->l_irq = res->Data.ExtendedIrq.Interrupts[0];
		}

		/*
		 * An IRQ of zero means that the link isn't routed.
		 */
		if (link->l_irq == 0)
			link->l_irq = PCI_INVALID_IRQ;
		break;
	}
	return (AE_OK);
}

/*
 * Populate the set of possible IRQs for each device.
 */
static ACPI_STATUS
link_add_prs(ACPI_RESOURCE *res, void *context)
{
	struct link_res_request *req;
	struct link *link;
	UINT32 *irqs;
	int i;

	ACPI_SERIAL_ASSERT(pci_link);
	req = (struct link_res_request *)context;
	switch (res->Id) {
	case ACPI_RSTYPE_IRQ:
	case ACPI_RSTYPE_EXT_IRQ:
		KASSERT(req->count < req->sc->pl_num_links,
			("link_add_prs: array boundary violation"));
		link = &req->sc->pl_links[req->count];
		req->count++;

		/*
		 * Stash a copy of the resource for later use when doing
		 * _SRS.
		 */
		bcopy(res, &link->l_prs_template, sizeof(ACPI_RESOURCE));
		if (res->Id == ACPI_RSTYPE_IRQ) {
			link->l_num_irqs = res->Data.Irq.NumberOfInterrupts;
			irqs = res->Data.Irq.Interrupts;
		} else {
			link->l_num_irqs =
			    res->Data.ExtendedIrq.NumberOfInterrupts;
			irqs = res->Data.ExtendedIrq.Interrupts;
		}
		if (link->l_num_irqs == 0)
			break;

		/*
		 * Save a list of the valid IRQs.  Also, if all of the
		 * valid IRQs are ISA IRQs, then mark this link as
		 * routed via an ISA interrupt.
		 */
		link->l_isa_irq = 1;
		link->l_irqs = malloc(sizeof(int) * link->l_num_irqs,
		    M_PCI_LINK, M_WAITOK | M_ZERO);
		for (i = 0; i < link->l_num_irqs; i++) {
			link->l_irqs[i] = irqs[i];
			if (irqs[i] >= NUM_ISA_INTERRUPTS)
				link->l_isa_irq = 0;
		}
		break;
	}
	return (AE_OK);
}

static int
link_valid_irq(struct link *link, int irq)
{
	int i;

	ACPI_SERIAL_ASSERT(pci_link);

	/* Invalid interrupts are never valid. */
	if (!PCI_INTERRUPT_VALID(irq))
		return (0);

	/* Any interrupt in the list of possible interrupts is valid. */
	for (i = 0; i < link->l_num_irqs; i++)
		if (link->l_irqs[i] == irq)
			 return (1);

	/*
	 * For links routed via an ISA interrupt, if the SCI is routed via
	 * an ISA interrupt, the SCI is always treated as a valid IRQ.
	 */
	if (link->l_isa_irq && AcpiGbl_FADT->SciInt == irq &&
	    irq < NUM_ISA_INTERRUPTS)
		return (1);

	/* If the interrupt wasn't found in the list it is not valid. */
	return (0);
}

static void
acpi_pci_link_dump(struct acpi_pci_link_softc *sc)
{
	struct link *link;
	int i, j;

	printf("Index  IRQ  Rtd  Ref  IRQs\n");
	for (i = 0; i < sc->pl_num_links; i++) {
		link = &sc->pl_links[i];
		printf("%5d  %3d   %c   %3d ", i, link->l_irq,
		    link->l_routed ? 'Y' : 'N',  link->l_references);
		if (link->l_num_irqs == 0)
			printf(" none");
		else for (j = 0; j < link->l_num_irqs; j++)
			printf(" %d", link->l_irqs[j]);
		printf("\n");
	}
}

static int
acpi_pci_link_attach(device_t dev)
{
	struct acpi_pci_link_softc *sc;
	struct link_res_request req;
	ACPI_STATUS status;
	int i;
	int prslinks;

	sc = device_get_softc(dev);

	/*
	 * Count the number of current resources so we know how big of
	 * a link array to allocate.
	 */
	status = AcpiWalkResources(acpi_get_handle(dev), "_CRS",
	    acpi_count_irq_resources, &sc->pl_num_links);
	if (ACPI_FAILURE(status))
		return (ENXIO);
	if (sc->pl_num_links == 0)
		return (0);

	/*
	 * Try to make the number of resources sufficiently large
	 * for traversal of both _PRS and _CRS.
	 *
	 * XXX Temporary fix for out-of-bounds access in prs_add_links().
	 * We really need to handle these in separate arrays.  -- njl
	 */
	prslinks = 0;
	status = AcpiWalkResources(acpi_get_handle(dev), "_PRS",
	    acpi_count_irq_resources, &prslinks);
	if (prslinks > sc->pl_num_links)
		sc->pl_num_links = prslinks;
	sc->pl_links = malloc(sizeof(struct link) * sc->pl_num_links,
	    M_PCI_LINK, M_WAITOK | M_ZERO);

	/* Initialize the child links. */
	for (i = 0; i < sc->pl_num_links; i++) {
		sc->pl_links[i].l_irq = PCI_INVALID_IRQ;
		sc->pl_links[i].l_bios_irq = PCI_INVALID_IRQ;
		sc->pl_links[i].l_res_index = i;
		sc->pl_links[i].l_sc = sc;
		sc->pl_links[i].l_isa_irq = 0;
	}
	req.count = 0;
	req.sc = sc;
	status = AcpiWalkResources(acpi_get_handle(dev), "_CRS",
	    link_add_crs, &req);
	if (ACPI_FAILURE(status))
		goto fail;
	req.count = 0;
	status = AcpiWalkResources(acpi_get_handle(dev), "_PRS",
	    link_add_prs, &req);
	if (ACPI_FAILURE(status) && status != AE_NOT_FOUND)
		goto fail;
	if (bootverbose) {
		device_printf(dev, "Links after initial probe:\n");
		acpi_pci_link_dump(sc);
	}

	/* Verify initial IRQs if we have _PRS. */
	if (status != AE_NOT_FOUND)
		for (i = 0; i < sc->pl_num_links; i++)
			if (!link_valid_irq(&sc->pl_links[i],
			    sc->pl_links[i].l_irq))
				sc->pl_links[i].l_irq = PCI_INVALID_IRQ;
	if (bootverbose) {
		device_printf(dev, "Links after initial validation:\n");
		acpi_pci_link_dump(sc);
	}

	/* Save initial IRQs. */
	for (i = 0; i < sc->pl_num_links; i++)
		sc->pl_links[i].l_initial_irq = sc->pl_links[i].l_irq;

	/*
	 * Try to disable this link.  If successful, set the current IRQ to
	 * zero and flags to indicate this link is not routed.  If we can't
	 * run _DIS (i.e., the method doesn't exist), assume the initial
	 * IRQ was routed by the BIOS.
	 */
	if (ACPI_SUCCESS(AcpiEvaluateObject(acpi_get_handle(dev), "_DIS", NULL,
		    NULL)))
		for (i = 0; i < sc->pl_num_links; i++)
			sc->pl_links[i].l_irq = PCI_INVALID_IRQ;
	else
		for (i = 0; i < sc->pl_num_links; i++)
			if (PCI_INTERRUPT_VALID(sc->pl_links[i].l_irq))
				sc->pl_links[i].l_routed = 1;
	if (bootverbose) {
		device_printf(dev, "Links after disable:\n");
		acpi_pci_link_dump(sc);
	}
	return (0);

fail:
	for (i = 0; i < sc->pl_num_links; i++)
		if (sc->pl_links[i].l_irqs != NULL)
			free(sc->pl_links[i].l_irqs, M_PCI_LINK);
	free(sc->pl_links, M_PCI_LINK);
	return (ENXIO);
}


/* XXX: Note that this is identical to pci_pir_search_irq(). */
static uint8_t
acpi_pci_link_search_irq(int bus, int device, int pin)
{
	uint32_t value;
	uint8_t func, maxfunc;

	/* See if we have a valid device at function 0. */
	value = pci_cfgregread(bus, device, 0, PCIR_HDRTYPE, 1);
	if ((value & PCIM_HDRTYPE) > PCI_MAXHDRTYPE)
		return (PCI_INVALID_IRQ);
	if (value & PCIM_MFDEV)
		maxfunc = PCI_FUNCMAX;
	else
		maxfunc = 0;

	/* Scan all possible functions at this device. */
	for (func = 0; func <= maxfunc; func++) {
		value = pci_cfgregread(bus, device, func, PCIR_DEVVENDOR, 4);
		if (value == 0xffffffff)
			continue;
		value = pci_cfgregread(bus, device, func, PCIR_INTPIN, 1);

		/*
		 * See if it uses the pin in question.  Note that the passed
		 * in pin uses 0 for A, .. 3 for D whereas the intpin
		 * register uses 0 for no interrupt, 1 for A, .. 4 for D.
		 */
		if (value != pin + 1)
			continue;
		value = pci_cfgregread(bus, device, func, PCIR_INTLINE, 1);
		if (bootverbose)
			printf(
		"ACPI: Found matching pin for %d.%d.INT%c at func %d: %d\n",
			    bus, device, pin + 'A', func, value);
		if (value != PCI_INVALID_IRQ)
			return (value);
	}
	return (PCI_INVALID_IRQ);
}

void
acpi_pci_link_add_reference(device_t dev, int index, device_t pcib, int slot,
	int pin)
{
	struct acpi_pci_link_softc *sc;
	struct link *link;
	uint8_t bios_irq;

	/* Bump the reference count. */
	ACPI_SERIAL_BEGIN(pci_link);
	sc = device_get_softc(dev);
	KASSERT(index >= 0 && index < sc->pl_num_links,
	    ("%s: invalid index %d", __func__, index));
	link = &sc->pl_links[index];
	link->l_references++;
	if (link->l_routed)
		pci_link_interrupt_weights[link->l_irq]++;

	/* Try to find a BIOS IRQ setting from any matching devices. */
	bios_irq = acpi_pci_link_search_irq(pcib_get_bus(pcib), slot, pin);
	if (!PCI_INTERRUPT_VALID(bios_irq)) {
		ACPI_SERIAL_END(pci_link);
		return;
	}

	/* Validate the BIOS IRQ. */
	if (!link_valid_irq(link, bios_irq)) {
		device_printf(dev, "BIOS IRQ %u for %d.%d.INT%c is invalid\n",
		    bios_irq, pcib_get_bus(pcib), slot, pin + 'A');
	} else if (!PCI_INTERRUPT_VALID(link->l_bios_irq)) {
		link->l_bios_irq = bios_irq;
		if (bios_irq < NUM_ISA_INTERRUPTS)
			pci_link_bios_isa_irqs |= (1 << bios_irq);
		if (bios_irq != link->l_initial_irq &&
		    PCI_INTERRUPT_VALID(link->l_initial_irq))
			device_printf(dev,
			    "BIOS IRQ %u does not match initial IRQ %u\n",
			    bios_irq, link->l_initial_irq);
	} else if (bios_irq != link->l_bios_irq)
		device_printf(dev,
	    "BIOS IRQ %u for %d.%d.INT%c does not match previous BIOS IRQ %u\n",
		    bios_irq, pcib_get_bus(pcib), slot, pin + 'A',
		    link->l_bios_irq);
	ACPI_SERIAL_END(pci_link);
}

static ACPI_STATUS
acpi_pci_link_route_irqs(device_t dev)
{
	struct acpi_pci_link_softc *sc;
	ACPI_RESOURCE *resource, *end, newres, *resptr;
	ACPI_BUFFER crsbuf, srsbuf;
	ACPI_STATUS status;
	struct link *link;
	int i;

	/* Fetch the _CRS. */
	ACPI_SERIAL_ASSERT(pci_link);
	sc = device_get_softc(dev);
	crsbuf.Pointer = NULL;
	crsbuf.Length = ACPI_ALLOCATE_BUFFER;
	status = AcpiGetCurrentResources(acpi_get_handle(dev), &crsbuf);
	if (ACPI_SUCCESS(status) && crsbuf.Pointer == NULL)
		status = AE_NO_MEMORY;
	if (ACPI_FAILURE(status)) {
		if (bootverbose)
			device_printf(dev,
			    "Unable to fetch current resources: %s\n",
			    AcpiFormatException(status));
		return (status);
	}

	/* Fill in IRQ resources via link structures. */
	srsbuf.Pointer = NULL;
	link = sc->pl_links;
	i = 0;
	resource = (ACPI_RESOURCE *)crsbuf.Pointer;
	end = (ACPI_RESOURCE *)((char *)crsbuf.Pointer + crsbuf.Length);
	for (;;) {
		switch (resource->Id) {
		case ACPI_RSTYPE_IRQ:
			MPASS(i < sc->pl_num_links);
			MPASS(link->l_prs_template.Id == ACPI_RSTYPE_IRQ);
			newres = link->l_prs_template;
			resptr = &newres;
			resptr->Data.Irq.NumberOfInterrupts = 1;
			if (PCI_INTERRUPT_VALID(link->l_irq))
				resptr->Data.Irq.Interrupts[0] = link->l_irq;
			else
				resptr->Data.Irq.Interrupts[0] = 0;
			break;
		case ACPI_RSTYPE_EXT_IRQ:
			MPASS(i < sc->pl_num_links);
			MPASS(link->l_prs_template.Id == ACPI_RSTYPE_EXT_IRQ);
			newres = link->l_prs_template;
			resptr = &newres;
			resptr->Data.ExtendedIrq.NumberOfInterrupts = 1;
			if (PCI_INTERRUPT_VALID(link->l_irq))
				resource->Data.ExtendedIrq.Interrupts[0] =
				    link->l_irq;
			else
				resource->Data.ExtendedIrq.Interrupts[0] = 0;
			break;
		default:
			resptr = resource;
		}
		status = acpi_AppendBufferResource(&srsbuf, resptr);
		if (ACPI_FAILURE(status)) {
			device_printf(dev, "Unable to build reousrces: %s\n",
			    AcpiFormatException(status));
			if (srsbuf.Pointer != NULL)
				AcpiOsFree(srsbuf.Pointer);
			AcpiOsFree(crsbuf.Pointer);
			return (status);
		}
		if (resource->Id == ACPI_RSTYPE_END_TAG)
			break;
		resource = ACPI_NEXT_RESOURCE(resource);
		link++;
		i++;
		if (resource >= end)
			break;
	}

	/* Write out new resources via _SRS. */
	status = AcpiSetCurrentResources(acpi_get_handle(dev), &srsbuf);
	if (ACPI_FAILURE(status)) {
		device_printf(dev, "Unable to route IRQs: %s\n",
		    AcpiFormatException(status));
		AcpiOsFree(crsbuf.Pointer);
		AcpiOsFree(srsbuf.Pointer);
		return (status);
	}
	AcpiOsFree(crsbuf.Pointer);

	/*
	 * Perform acpi_config_intr() on each IRQ resource if it was just
	 * routed for the first time.
	 */
	link = sc->pl_links;
	i = 0;
	resource = (ACPI_RESOURCE *)srsbuf.Pointer;
	for (;;) {
		if (resource->Id == ACPI_RSTYPE_END_TAG)
			break;
		MPASS(i < sc->pl_num_links);
		if (link->l_routed || !PCI_INTERRUPT_VALID(link->l_irq))
			continue;
		switch (resource->Id) {
		case ACPI_RSTYPE_IRQ:
		case ACPI_RSTYPE_EXT_IRQ:
			link->l_routed = 1;
			acpi_config_intr(dev, resource);
			pci_link_interrupt_weights[link->l_irq] +=
			    link->l_references;
			break;
		}
		resource = ACPI_NEXT_RESOURCE(resource);
		link++;
		i++;
		if (resource >= end)
			break;
	}
	AcpiOsFree(srsbuf.Pointer);
	return (AE_OK);
}

static int
acpi_pci_link_resume(device_t dev)
{
#if 0 /* XXX Disabled temporarily since this hangs resume. */
	ACPI_STATUS status;

	ACPI_SERIAL_BEGIN(pci_link);
	status = acpi_pci_link_route_irqs(dev);
	ACPI_SERIAL_END(pci_link);
	if (ACPI_FAILURE(status))
		return (ENXIO);
	else
#endif
		return (0);
}

/*
 * Pick an IRQ to use for this unrouted link.
 */
static uint8_t
acpi_pci_link_choose_irq(device_t dev, struct link *link)
{
	char tunable_buffer[64], link_name[5];
	u_int8_t best_irq, pos_irq;
	int best_weight, pos_weight, i;

	KASSERT(link->l_routed == 0, ("%s: link already routed", __func__));
	KASSERT(!PCI_INTERRUPT_VALID(link->l_irq),
	    ("%s: link already has an IRQ", __func__));

	/* Check for a tunable override and use it if it is valid. */
	if (ACPI_SUCCESS(acpi_short_name(acpi_get_handle(dev), link_name,
	    sizeof(link_name)))) {
		    snprintf(tunable_buffer, sizeof(tunable_buffer),
			"hw.pci.link.%s.%d.irq", link_name, link->l_res_index);
		    if (getenv_int(tunable_buffer, &i) &&
			PCI_INTERRUPT_VALID(i) && link_valid_irq(link, i))
			    return (i);
		    snprintf(tunable_buffer, sizeof(tunable_buffer),
			"hw.pci.link.%s.irq", link_name);
		    if (getenv_int(tunable_buffer, &i) &&
			PCI_INTERRUPT_VALID(i) && link_valid_irq(link, i))
			    return (i);
	}

	/*
	 * If we have a valid BIOS IRQ, use that.  We trust what the BIOS
	 * says it routed over what _CRS says the link thinks is routed.
	 */
	if (PCI_INTERRUPT_VALID(link->l_bios_irq))
		return (link->l_bios_irq);

	/*
	 * If we don't have a BIOS IRQ but do have a valid IRQ from _CRS,
	 * then use that.
	 */
	if (PCI_INTERRUPT_VALID(link->l_initial_irq))
		return (link->l_initial_irq);

	/*
	 * Ok, we have no useful hints, so we have to pick from the
	 * possible IRQs.  For ISA IRQs we only use interrupts that
	 * have already been used by the BIOS.
	 */
	best_irq = PCI_INVALID_IRQ;
	best_weight = INT_MAX;
	for (i = 0; i < link->l_num_irqs; i++) {
		pos_irq = link->l_irqs[i];
		if (pos_irq < NUM_ISA_INTERRUPTS &&
		    (pci_link_bios_isa_irqs & 1 << pos_irq) == 0)
			continue;
		pos_weight = pci_link_interrupt_weights[pos_irq];
		if (pos_weight < best_weight) {
			best_weight = pos_weight;
			best_irq = pos_irq;
		}
	}

	/*
	 * If this is an ISA IRQ, try using the SCI if it is also an ISA
	 * interrupt as a fallback.
	 */
	if (link->l_isa_irq) {
		pos_irq = AcpiGbl_FADT->SciInt;
		pos_weight = pci_link_interrupt_weights[pos_irq];
		if (pos_weight < best_weight) {
			best_weight = pos_weight;
			best_irq = pos_irq;
		}
	}
		    
	if (bootverbose) {
		if (PCI_INTERRUPT_VALID(best_irq))
			device_printf(dev, "Picked IRQ %u with weight %d\n",
			    best_irq, best_weight);
	} else
		device_printf(dev, "Unable to choose an IRQ\n");
	return (best_irq);
}

int
acpi_pci_link_route_interrupt(device_t dev, int index)
{
	struct acpi_pci_link_softc *sc;
	struct link *link;

	ACPI_SERIAL_BEGIN(pci_link);
	sc = device_get_softc(dev);
	KASSERT(index >= 0 && index < sc->pl_num_links,
	    ("%s: invalid index %d", __func__, index));
	link = &sc->pl_links[index];

	/*
	 * If this link device is already routed to an interrupt, just return
	 * the interrupt it is routed to.
	 */
	if (link->l_routed) {
		KASSERT(PCI_INTERRUPT_VALID(link->l_irq),
		    ("%s: link is routed but has an invalid IRQ", __func__));
		ACPI_SERIAL_END(pci_link);
		return (link->l_irq);
	}

	/* Choose an IRQ if we need one. */
	if (!PCI_INTERRUPT_VALID(link->l_irq)) {
		link->l_irq = acpi_pci_link_choose_irq(dev, link);

		/*
		 * Try to route the interrupt we picked.  If it fails, then
		 * assume the interrupt is not routed.
		 */
		if (PCI_INTERRUPT_VALID(link->l_irq)) {
			acpi_pci_link_route_irqs(dev);
			if (!link->l_routed)
				link->l_irq = PCI_INVALID_IRQ;
		}
	}
	ACPI_SERIAL_END(pci_link);

	return (link->l_irq);
}

/*
 * This is gross, but we abuse the identify routine to perform one-time
 * SYSINIT() style initialization for the driver.
 */
static void
acpi_pci_link_identify(driver_t *driver, device_t parent)
{

	/*
	 * If the SCI is an ISA IRQ, add it to the bitmask of known good
	 * ISA IRQs.
	 *
	 * XXX: If we are using the APIC, the SCI might have been
	 * rerouted to an APIC pin in which case this is invalid.  However,
	 * if we are using the APIC, we also shouldn't be having any PCI
	 * interrupts routed via ISA IRQs, so this is probably ok.
	 */
	if (AcpiGbl_FADT->SciInt < NUM_ISA_INTERRUPTS)
		pci_link_bios_isa_irqs |= (1 << AcpiGbl_FADT->SciInt);
}

static device_method_t acpi_pci_link_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	acpi_pci_link_identify),
	DEVMETHOD(device_probe,		acpi_pci_link_probe),
	DEVMETHOD(device_attach,	acpi_pci_link_attach),
	DEVMETHOD(device_resume,	acpi_pci_link_resume),

	{0, 0}
};

static driver_t acpi_pci_link_driver = {
	"pci_link",
	acpi_pci_link_methods,
	sizeof(struct acpi_pci_link_softc),
};

static devclass_t pci_link_devclass;

DRIVER_MODULE(acpi_pci_link, acpi, acpi_pci_link_driver, pci_link_devclass, 0,
    0);
MODULE_DEPEND(acpi_pci_link, acpi, 1, 1, 1);
