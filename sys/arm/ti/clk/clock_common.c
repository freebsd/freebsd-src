/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Oskar Holmlund <oskar.holmlund@ohdata.se>
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/libkern.h>
#include <sys/types.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <dev/fdt/simplebus.h>

#include <dev/extres/clk/clk_mux.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "clock_common.h"

#if 0
#define DPRINTF(dev, msg...) device_printf(dev, msg)
#else
#define DPRINTF(dev, msg...)
#endif

void
read_clock_cells(device_t dev, struct clock_cell_info *clk) {
	ssize_t numbytes_clocks;
	phandle_t node, parent, *cells;
        int index, ncells, rv;

	node = ofw_bus_get_node(dev);

	/* Get names of parent clocks */
	numbytes_clocks = OF_getproplen(node, "clocks");
	clk->num_clock_cells = numbytes_clocks / sizeof(cell_t);

	/* Allocate space and get clock cells content */
	/* clock_cells / clock_cells_ncells will be freed in
	 * find_parent_clock_names()
	 */
	clk->clock_cells = malloc(numbytes_clocks, M_DEVBUF, M_WAITOK|M_ZERO);
	clk->clock_cells_ncells = malloc(clk->num_clock_cells*sizeof(uint8_t),
		M_DEVBUF, M_WAITOK|M_ZERO);
        OF_getencprop(node, "clocks", clk->clock_cells, numbytes_clocks);

	/* Count number of clocks */
	clk->num_real_clocks = 0;
	for (index = 0; index < clk->num_clock_cells; index++) {
		rv = ofw_bus_parse_xref_list_alloc(node, "clocks", "#clock-cells",
			clk->num_real_clocks, &parent, &ncells, &cells);
		if (rv != 0)
			continue;

		if (cells != NULL)
			OF_prop_free(cells);

		clk->clock_cells_ncells[index] = ncells;
		index += ncells;
		clk->num_real_clocks++;
	}
}

int
find_parent_clock_names(device_t dev, struct clock_cell_info *clk, struct clknode_init_def *def) {
	int	index, clock_index, err;
	bool	found_all = true;
	clk_t	parent;

	/* Figure out names */
	for (index = 0, clock_index = 0; index < clk->num_clock_cells; index++) {
		/* Get name of parent clock */
		err = clk_get_by_ofw_index(dev, 0, clock_index, &parent);
		if (err != 0) {
			clock_index++;
			found_all = false;
			DPRINTF(dev, "Failed to find clock_cells[%d]=0x%x\n",
				index, clk->clock_cells[index]);

			index += clk->clock_cells_ncells[index];
			continue;
		}

		def->parent_names[clock_index] = clk_get_name(parent);
		clk_release(parent);

		DPRINTF(dev, "Found parent clock[%d/%d]: %s\n",
			clock_index, clk->num_real_clocks,
			def->parent_names[clock_index]);

		clock_index++;
		index += clk->clock_cells_ncells[index];
	}

	if (!found_all) {
		return 1;
	}

	free(clk->clock_cells, M_DEVBUF);
	free(clk->clock_cells_ncells, M_DEVBUF);
	return 0;
}

void
create_clkdef(device_t dev, struct clock_cell_info *clk, struct clknode_init_def *def) {
	def->id = 1;

	clk_parse_ofw_clk_name(dev, ofw_bus_get_node(dev), &def->name);

	DPRINTF(dev, "node name: %s\n", def->name);

	def->parent_cnt = clk->num_real_clocks;
	def->parent_names = malloc(clk->num_real_clocks*sizeof(char *),
					 M_OFWPROP, M_WAITOK);
}
void
free_clkdef(struct clknode_init_def *def) {
	OF_prop_free(__DECONST(char *, def->name));
	OF_prop_free(def->parent_names);
}
