/*
 * Copyright (c) 2023 Juniper Networks, Inc.
 * All rights reserved.
 */
/*-
 * Copyright (c) 2018 Stormshield.
 * Copyright (c) 2018 Semihalf.
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

/* Based *heavily* on the tpm_tis driver. */

#include "opt_platform.h"

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/sx.h>

#include <dev/spibus/spi.h>
#include <dev/tpm/tpm20.h>

#include "spibus_if.h"
#include "tpm_if.h"

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

static struct ofw_compat_data compatible_data[] = {
	{"infineon,slb9670",	true},
	{"tcg,tpm_tis-spi",	true},
	{NULL,			false}
};

static int
tpm_spi_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compatible_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "Trusted Platform Module 2.0, SPI Mode");

	return (BUS_PROBE_DEFAULT);
}

static device_method_t tpm_methods[] = {
	DEVMETHOD(device_probe,		tpm_spi_probe),
	DEVMETHOD_END
};

DEFINE_CLASS_2(tpm, tpm_driver, tpm_methods, sizeof(struct tpm_sc),
    tpmtis_driver, tpm_spi_driver);

#if __FreeBSD_version < 1400067
static devclass_t tpm_devclass;

DRIVER_MODULE(tpm, spibus, tpm_driver, tpm_devclass, NULL, NULL);
#else
DRIVER_MODULE(tpm, spibus, tpm_driver, NULL, NULL);
#endif
MODULE_DEPEND(tpm, spibus, 1, 1, 1);
MODULE_VERSION(tpm, 1);
