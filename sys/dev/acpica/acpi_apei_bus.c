/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Alexander Motin <mav@FreeBSD.org>
 * Copyright (c) 2025-2026 Netflix, Inc.
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

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <contrib/dev/acpica/include/acpi.h>

#include <dev/acpica/acpivar.h>
#include <dev/acpica/apeivar.h>

/*
 * Different APEI tables can reuse the same registers (and sometimes
 * different views of the same register, e.g. 32- vs 64-bit mappings
 * of the same register).  To enable this sharing, apei0 acts as a bus
 * device managing a pool of allocated resources and hands out
 * mappings to child devices.  Child devices handle individual tables.
 */

struct apei_register {
	struct resource *res;
	TAILQ_ENTRY(apei_register) link;
};

struct apei_register_mapping {
	struct resource_map map;
	struct apei_register *reg;
	TAILQ_ENTRY(apei_register_mapping) link;
};

struct apei_ivars {
	TAILQ_HEAD(, apei_register_mapping) mappings;
};

TAILQ_HEAD(apei_register_list, apei_register);

struct apei_softc {
	device_t dev;
	struct apei_register_list mem;
	struct apei_register_list io;
};

static char *apei_ids[] = { "PNP0C33", NULL };

static MALLOC_DEFINE(M_APEI, "apei", "ACPI platform error interface");

static ACPI_STATUS
apei_find(ACPI_HANDLE handle, UINT32 level, void *context,
    void **status)
{
	int *found = (int *)status;
	char **ids;

	for (ids = apei_ids; *ids != NULL; ids++) {
		if (acpi_MatchHid(handle, *ids)) {
			*found = 1;
			break;
		}
	}
	return (AE_OK);
}

static void
apei_identify(driver_t *driver, device_t parent)
{
	device_t	child;
	int		found;

	if (acpi_disabled("apei"))
		return;

	/* Only one APEI device can exist. */
	if (device_find_child(parent, "apei", DEVICE_UNIT_ANY) != NULL)
		return;

	/* Search for ACPI error device to be used. */
	found = 0;
	AcpiWalkNamespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
	    100, apei_find, NULL, NULL, (void *)&found);
	if (found)
		return;

	/* If not found - create a fake one. */
	child = BUS_ADD_CHILD(parent, 2, "apei", 0);
	if (child == NULL)
		printf("%s: can't add child\n", __func__);
}

static int
apei_probe(device_t dev)
{
	int rv;

	if (acpi_disabled("apei"))
		return (ENXIO);

	if (acpi_get_handle(dev) != NULL) {
		rv = ACPI_ID_PROBE(device_get_parent(dev), dev, apei_ids, NULL);
		if (rv > 0)
			return (rv);
	}

	device_set_desc(dev, "ACPI Platform Error Interface");
	return (0);
}

static int
apei_attach(device_t dev)
{
	struct apei_softc *sc = device_get_softc(dev);

	sc->dev = dev;
	TAILQ_INIT(&sc->mem);
	TAILQ_INIT(&sc->io);
	bus_identify_children(dev);
	bus_attach_children(dev);
	return (0);
}

static device_t
apei_add_child(device_t dev, u_int order, const char *name, int unit)
{
	struct apei_ivars *ai;
	device_t child;

	ai = malloc(sizeof(*ai), M_APEI, M_WAITOK);
	TAILQ_INIT(&ai->mappings);
	child = device_add_child_ordered(dev, order, name, unit);
	if (child != NULL)
		device_set_ivars(child, ai);
	else
		free(ai, M_APEI);
	return (child);
}

static void
apei_child_deleted(device_t dev, device_t child)
{
	struct apei_ivars *ai = device_get_ivars(child);

	MPASS(TAILQ_EMPTY(&ai->mappings));
	free(ai, M_APEI);
}

static void
apei_child_detached(device_t dev, device_t child)
{
	struct apei_ivars *ai = device_get_ivars(child);

	while (!TAILQ_EMPTY(&ai->mappings)) {
		struct apei_register_mapping *m = TAILQ_FIRST(&ai->mappings);

		device_printf(dev, "child leaked mapping of %s %#jx-%#jx\n",
		    rman_get_type(m->reg->res) == SYS_RES_MEMORY ? "iomem" :
		    "port", rman_get_start(m->reg->res),
		    rman_get_end(m->reg->res));

		(void) apei_unmap_register(child, &m->map);
	}
}

static int
apei_read_ivar(device_t dev, device_t child, int index, uintptr_t *result)
{
	switch (index) {
	case ACPI_IVAR_HANDLE:
		*(ACPI_HANDLE *)result = acpi_get_handle(dev);
		break;
	default:
		return (ENOENT);
	}

	return (0);
}

static struct apei_register *
apei_alloc_register(struct apei_softc *sc, int type, rman_res_t start,
    rman_res_t count)
{
	struct apei_register_list *list;
	struct apei_register *reg, *prev;
	rman_res_t end;
	const char *descr;
	int error, next_rid;

	switch (type) {
	case SYS_RES_MEMORY:
		list = &sc->mem;
		descr = "iomem";
		break;
	case SYS_RES_IOPORT:
		list = &sc->io;
		descr = "port";
		break;
	default:
		return (NULL);
	}

	end = start + (count - 1);

	/* First, look for an existing resource. */
	prev = NULL;
	TAILQ_FOREACH(reg, list, link) {
		/* Does the existing register overlap with this request? */
		if (start > rman_get_end(reg->res) ||
		    end < rman_get_start(reg->res)) {

			/* prev will be the location of the first free rid */
			if (prev == NULL)
				next_rid = 0;
			else
				next_rid = rman_get_rid(prev->res) + 1;
			if (rman_get_rid(reg->res) == next_rid)
				prev = reg;
			continue;
		}

		/* Do we need to extend the existing register? */
		if (start < rman_get_start(reg->res) ||
		    end > rman_get_end(reg->res)) {
			rman_res_t new_end, new_start;

			new_start = rman_get_start(reg->res);
			if (start < new_start)
				new_start = start;
			new_end = rman_get_end(reg->res);
			if (end > new_end)
				new_end = end;
			error = bus_adjust_resource(sc->dev, reg->res,
			    new_start, new_end);
			if (error != 0) {
				if (bootverbose)
					device_printf(sc->dev,
			    "failed to grow %s %#jx-%#jx for %#jx-%#jx: %d\n",
					    descr, rman_get_start(reg->res),
					    rman_get_end(reg->res), start, end,
					    error);
				return (NULL);
			}
		}

		return (reg);
	}

	/* Allocate a new resource. */
	if (prev == NULL)
		next_rid = 0;
	else
		next_rid = rman_get_rid(prev->res) + 1;
	error = bus_set_resource(sc->dev, type, next_rid, start, count);
	if (error != 0) {
		if (bootverbose)
			device_printf(sc->dev,
			    "failed to add %s %#jx-%#jx: %d\n", descr, start,
			    end, error);
		return (NULL);
	}

	reg = malloc(sizeof(*reg), M_APEI, M_WAITOK);
	reg->res = bus_alloc_resource_any(sc->dev, type, next_rid, RF_ACTIVE |
	    RF_UNMAPPED);
	if (reg->res == NULL) {
		if (bootverbose)
			device_printf(sc->dev,
			    "failed to allocate %s %#jx-%#jx\n", descr, start,
			    end);
		free(reg, M_APEI);
		bus_delete_resource(sc->dev, type, next_rid);
		return (NULL);
	}

	if (prev == NULL)
		TAILQ_INSERT_HEAD(list, reg, link);
	else
		TAILQ_INSERT_AFTER(list, prev, reg, link);
	return (reg);
}

static struct resource_map *
apei_map_register_method(device_t dev, device_t child, int type,
    rman_res_t start, rman_res_t count)
{
	struct apei_softc *sc = device_get_softc(dev);
	struct apei_ivars *ai = device_get_ivars(child);
	struct apei_register_mapping *m;
	struct apei_register *reg;
	struct resource_map_request req;
	int error;

	reg = apei_alloc_register(sc, type, start, count);
	if (reg == NULL)
		return (NULL);

	m = malloc(sizeof(*m), M_APEI, M_WAITOK | M_ZERO);
	m->reg = reg;
	resource_init_map_request(&req);
	req.offset = start - rman_get_start(reg->res);
	req.length = count;
	error = bus_map_resource(dev, reg->res, &req, &m->map);
	if (error != 0) {
		if (bootverbose)
			device_printf(dev, "failed to map %s %#jx-%#jx: %d\n",
			    type == SYS_RES_MEMORY ? "iomem" : "port",
			    start,
			    start + (count - 1), error);
		free(m, M_APEI);
		return (NULL);
	}
	TAILQ_INSERT_TAIL(&ai->mappings, m, link);
	return (&m->map);
}

static int
apei_unmap_register_method(device_t dev, device_t child,
    struct resource_map *map)
{
	struct apei_ivars *ai = device_get_ivars(child);
	struct apei_register_mapping *m;
	int error;

	if (map == NULL)
		return (0);

	TAILQ_FOREACH(m, &ai->mappings, link) {
		if (&m->map == map)
			break;
	}

	if (m == NULL)
		return (EINVAL);

	TAILQ_REMOVE(&ai->mappings, m, link);
	error = bus_unmap_resource(dev, m->reg->res, &m->map);
	if (error != 0 && bootverbose)
		device_printf(dev, "failed to unmap %s %#jx-%#jx: %d\n",
		    rman_get_type(m->reg->res) == SYS_RES_MEMORY ? "iomem" :
		    "port", rman_get_start(m->reg->res),
		    rman_get_end(m->reg->res), error);
	free(m, M_APEI);
	return (0);
}

struct resource_map *
apei_map_register(device_t dev, ACPI_GENERIC_ADDRESS *gas)
{
	int type;

	if (gas->Address == 0 || gas->BitWidth == 0 || gas->BitWidth % 8 != 0)
		return (NULL);

	switch (gas->SpaceId) {
	case ACPI_ADR_SPACE_SYSTEM_MEMORY:
		type = SYS_RES_MEMORY;
		break;
	case ACPI_ADR_SPACE_SYSTEM_IO:
		type = SYS_RES_IOPORT;
		break;
	default:
		return (NULL);
	}

	return (APEI_MAP_REGISTER(device_get_parent(dev), dev, type,
	    gas->Address, gas->BitWidth / 8));
}

struct resource_map *
apei_map_memory(device_t dev, rman_res_t start, rman_res_t count)
{
	return (APEI_MAP_REGISTER(device_get_parent(dev), dev, SYS_RES_MEMORY,
	    start, count));
}

int
apei_unmap_register(device_t dev, struct resource_map *map)
{
	return (APEI_UNMAP_REGISTER(device_get_parent(dev), dev, map));
}

static device_method_t apei_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	apei_identify),
	DEVMETHOD(device_probe,		apei_probe),
	DEVMETHOD(device_attach,	apei_attach),

	/* Bus interface */
	DEVMETHOD(bus_add_child,	apei_add_child),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_child_deleted,	apei_child_deleted),
	DEVMETHOD(bus_child_detached,	apei_child_detached),
	DEVMETHOD(bus_read_ivar,	apei_read_ivar),

	/* APEI interface */
	DEVMETHOD(apei_map_register,	apei_map_register_method),
	DEVMETHOD(apei_unmap_register,	apei_unmap_register_method),

	DEVMETHOD_END
};

static driver_t	apei_driver = {
	"apei",
	apei_methods,
	sizeof(struct apei_softc),
};

DRIVER_MODULE(apei, acpi, apei_driver, NULL, NULL);
MODULE_DEPEND(apei, acpi, 1, 1, 1);
