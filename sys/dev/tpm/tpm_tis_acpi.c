/*-
 * Copyright (c) 2018 Stormshield.
 * Copyright (c) 2018 Semihalf.
 * Copyright (c) 2023 Juniper Networks, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include "tpm20.h"
#include "tpm_if.h"

static int tpmtis_acpi_probe(device_t dev);

char *tpmtis_ids[] = {"MSFT0101", NULL};

static int
tpmtis_acpi_probe(device_t dev)
{
	int err;
	ACPI_TABLE_TPM23 *tbl;
	ACPI_STATUS status;

	err = ACPI_ID_PROBE(device_get_parent(dev), dev, tpmtis_ids, NULL);
	if (err > 0)
		return (err);
	/*Find TPM2 Header*/
	status = AcpiGetTable(ACPI_SIG_TPM2, 1, (ACPI_TABLE_HEADER **) &tbl);
	if(ACPI_FAILURE(status) ||
	   tbl->StartMethod != TPM2_START_METHOD_TIS)
	    err = ENXIO;

	device_set_desc(dev, "Trusted Platform Module 2.0, FIFO mode");
	return (err);
}

static int
tpmtis_acpi_attach(device_t dev)
{
	struct tpm_sc *sc = device_get_softc(dev);

	sc->mem_rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->mem_rid,
		    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		return (ENXIO);
	}

	/*
	 * If tpmtis_attach() fails, tpmtis_detach() will automatically free
	 * sc->mem_res (not-NULL).
	 */
	return (tpmtis_attach(dev));
}

/* ACPI Driver */
static device_method_t tpmtis_methods[] = {
	DEVMETHOD(device_attach,	tpmtis_acpi_attach),
	DEVMETHOD(device_probe,		tpmtis_acpi_probe),
	DEVMETHOD_END
};

DEFINE_CLASS_2(tpmtis, tpmtis_acpi_driver, tpmtis_methods,
    sizeof(struct tpm_sc), tpmtis_driver, tpm_bus_driver);

DRIVER_MODULE(tpmtis, acpi, tpmtis_acpi_driver, 0, 0);
