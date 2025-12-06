/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Rubicon Communications, LLC (Netgate)
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/ofw_bus.h>

static int
fdt_reserved_memory_probe(device_t dev)
{
	char *name;
	phandle_t node;
	int rv;

	node = ofw_bus_get_node(dev);
	if (node == -1)
		return (ENXIO);

	if (!OF_hasprop(node, "ranges"))
		return (ENXIO);

	rv = OF_getprop_alloc(node, "name", (void **)&name);
	if (rv == -1)
		return (ENXIO);

	rv = strcmp(name, "reserved-memory");
	free(name, M_OFWPROP);
	if (rv != 0)
		return (ENXIO);

	device_set_desc(dev, "Reserved memory pseudo-bus");

	return (BUS_PROBE_GENERIC);
}

static device_method_t fdt_reserved_memory_methods[] = {

	/* Device interface */
	DEVMETHOD(device_probe,		fdt_reserved_memory_probe),

	DEVMETHOD_END
};

DEFINE_CLASS_1(fdt_reserved_memory, fdt_reserved_memory_driver,
    fdt_reserved_memory_methods, 0, simplebus_driver);

EARLY_DRIVER_MODULE(fdt_reserved_memory, simplebus, fdt_reserved_memory_driver,
    0, 0, BUS_PASS_BUS);
MODULE_VERSION(fdt_reserved_memory, 1);
