/*
 * Copyright (c) 2026 Justin Hibbits
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "dpaa_common.h"

#define	FDT_REG_CELLS	4
int
dpaa_map_private_memory(device_t dev, int idx, const char *compat,
    vm_paddr_t *addrp, size_t *sizep)
{
	phandle_t node;
	pcell_t cells[idx + 1];
	pcell_t *cell_alloc;
	int addr_cells, size_cells;
	uint64_t tmp;
	u_long align, base, size;
	vm_paddr_t alloc_base;
	vm_size_t alloc_range_size;
	ssize_t alloc_size;
	void *reserved;
	int rv;

	node = ofw_bus_get_node(dev);
	if (OF_getencprop(node, "memory-region", cells, sizeof(cells)) <= 0)
		return (ENXIO);

	node = OF_node_from_xref(cells[idx]);
	/* If the memory is already reserved, we just need to return it. */
	if (fdt_regsize(node, &base, &size) == 0)
		goto success;

	rv = fdt_addrsize_cells(OF_parent(node), &addr_cells, &size_cells);
	if (rv != 0)
		return (rv);

	if (OF_getprop(node, "alignment", &tmp, sizeof(tmp)) <= 0)
		return (ENXIO);

	align = fdt_data_get(&tmp, addr_cells);
	if (OF_getprop(node, "size", &tmp, sizeof(tmp)) <= 0)
		return (ENXIO);
	size = fdt_data_get(&tmp, size_cells);

	alloc_size =
	    OF_getencprop_alloc(node, "alloc-ranges", (void **)&cell_alloc);
	if (alloc_size < 0)
		return (ENXIO);

	alloc_size /= sizeof(pcell_t);
	for (int i = 0; i < alloc_size; i += (addr_cells + size_cells)) {
		alloc_base = fdt_data_get(&cell_alloc[i], addr_cells);
		alloc_range_size =
		    fdt_data_get(&cell_alloc[i + addr_cells], size_cells);
		reserved = contigmalloc(size, M_DEVBUF, M_NOWAIT | M_ZERO,
		    alloc_base, alloc_base + alloc_range_size, align, 0);
		if (reserved != NULL)
			break;
	}
	if (reserved == NULL)
		return (ENOMEM);
	/* Flush the cache (zeroed memory) because it won't be touched later. */
	cpu_flush_dcache(reserved, size);
	base = pmap_kextract((vm_offset_t)reserved);

success:
	*addrp = base;
	*sizep = size;

	return (0);
}
