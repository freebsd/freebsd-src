/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kevin Bowling <kbowling@FreeBSD.org>
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#ifdef INTRNG
#include <sys/intr.h>
#endif

#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>
#include <dev/tpm/tpm_irq.h>

int
tpm_acpi_configure_irq(device_t dev, struct resource *irq,
    struct tpm_irq_config *config, bool program)
{
	ACPI_BUFFER buf;
	ACPI_HANDLE handle, method;
	ACPI_RESOURCE *res;
	ACPI_STATUS status;
	rman_res_t allocated;
	uint32_t gsi, mode;
	uint8_t trigger, polarity;
	int error, count;
#ifdef INTRNG
	struct intr_map_data_acpi *map;

	/* rman_get_start() is an opaque mapping index on INTRNG systems. */
	map = rman_get_virtual(irq);
	if (map == NULL || map->hdr.type != INTR_MAP_DATA_ACPI)
		return (ENXIO);
	allocated = map->irq;
#else
	allocated = rman_get_start(irq);
#endif
	handle = acpi_get_handle(dev);
	buf.Length = ACPI_ALLOCATE_BUFFER;
	buf.Pointer = NULL;
	status = AcpiGetCurrentResources(handle, &buf);
	if (ACPI_FAILURE(status))
		return (ENXIO);

	error = ENXIO;
	count = 0;
	for (res = buf.Pointer; res->Type != ACPI_RESOURCE_TYPE_END_TAG;
	    res = ACPI_NEXT_RESOURCE(res)) {
		switch (res->Type) {
		case ACPI_RESOURCE_TYPE_IRQ:
			if (res->Data.Irq.InterruptCount != 1 ||
			    res->Data.Irq.Shareable != ACPI_EXCLUSIVE)
				goto out;
			gsi = res->Data.Irq.Interrupts[0];
			trigger = res->Data.Irq.Triggering;
			polarity = res->Data.Irq.Polarity;
			break;
		case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
			if (res->Data.ExtendedIrq.ProducerConsumer !=
			    ACPI_CONSUMER ||
			    res->Data.ExtendedIrq.InterruptCount != 1 ||
			    res->Data.ExtendedIrq.Shareable != ACPI_EXCLUSIVE)
				goto out;
			gsi = res->Data.ExtendedIrq.Interrupts[0];
			trigger = res->Data.ExtendedIrq.Triggering;
			polarity = res->Data.ExtendedIrq.Polarity;
			break;
		default:
			continue;
		}
		/* Reject ambiguous resources or a changed route on resume. */
		if (++count != 1 || gsi != allocated ||
		    (trigger != ACPI_EDGE_SENSITIVE &&
		    trigger != ACPI_LEVEL_SENSITIVE) ||
		    (polarity != ACPI_ACTIVE_HIGH &&
		    polarity != ACPI_ACTIVE_LOW))
			goto out;
#ifdef INTRNG
		if (map->trig != (trigger == ACPI_EDGE_SENSITIVE ?
		    INTR_TRIGGER_EDGE : INTR_TRIGGER_LEVEL) ||
		    map->pol != (polarity == ACPI_ACTIVE_HIGH ?
		    INTR_POLARITY_HIGH : INTR_POLARITY_LOW))
			goto out;
#elif defined(__amd64__) || defined(__i386__)
		/* Override only the local mode; keep _SRS input unchanged. */
		if (gsi < 16 && trigger == ACPI_EDGE_SENSITIVE &&
		    polarity == ACPI_ACTIVE_LOW &&
		    acpi_override_isa_irq_polarity)
			polarity = ACPI_ACTIVE_HIGH;
#endif
		mode = (trigger == ACPI_EDGE_SENSITIVE ?
		    TPM_IRQ_EDGE_RISING : TPM_IRQ_LEVEL_HIGH) |
		    (polarity == ACPI_ACTIVE_LOW ? TPM_IRQ_LEVEL_LOW : 0);
		if (config->valid && config->mode != mode)
			goto out;
	}
	if (count != 1)
		goto out;

	status = AE_OK;
	if (program) {
		/* _SRS can enable IRQs; locality and a handler must exist. */
		status = AcpiGetHandle(handle, "_SRS", &method);
		if (status == AE_NOT_FOUND)
			status = AE_OK;
		else if (ACPI_SUCCESS(status))
			status = AcpiSetCurrentResources(handle, &buf);
	}
	if (ACPI_SUCCESS(status)) {
		config->sirq = TPM_IRQ_SIRQ_UNCHANGED;
		config->mode = mode;
		config->valid = true;
		error = 0;
	}
out:
	AcpiOsFree(buf.Pointer);
	return (error);
}
