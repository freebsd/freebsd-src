/*-
 * Copyright (c) 2009-2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf under sponsorship from
 * the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/limits.h>

#include <machine/fdt.h>
#include <machine/resource.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include "ofw_bus_if.h"

#ifdef DEBUG
#define debugf(fmt, args...) do { printf("%s(): ", __func__);	\
    printf(fmt,##args); } while (0)
#else
#define debugf(fmt, args...)
#endif

#define FDT_COMPAT_LEN	255
#define FDT_TYPE_LEN	64

#define FDT_REG_CELLS	4

vm_paddr_t fdt_immr_pa;
vm_offset_t fdt_immr_va;
vm_offset_t fdt_immr_size;

struct fdt_ic_list fdt_ic_list_head = SLIST_HEAD_INITIALIZER(fdt_ic_list_head);

int
fdt_get_range(phandle_t node, int range_id, u_long *base, u_long *size)
{
	pcell_t ranges[6], *rangesptr;
	pcell_t addr_cells, size_cells, par_addr_cells;
	int len, tuple_size, tuples;

	if ((fdt_addrsize_cells(node, &addr_cells, &size_cells)) != 0)
		return (ENXIO);
	/*
	 * Process 'ranges' property.
	 */
	par_addr_cells = fdt_parent_addr_cells(node);
	if (par_addr_cells > 2)
		return (ERANGE);

	len = OF_getproplen(node, "ranges");
	if (len > sizeof(ranges))
		return (ENOMEM);
	if (len == 0) {
		*base = 0;
		*size = ULONG_MAX;
		return (0);
	}

	if (!(range_id < len))
		return (ERANGE);

	if (OF_getprop(node, "ranges", ranges, sizeof(ranges)) <= 0)
		return (EINVAL);

	tuple_size = sizeof(pcell_t) * (addr_cells + par_addr_cells +
	    size_cells);
	tuples = len / tuple_size;

	if (fdt_ranges_verify(ranges, tuples, par_addr_cells,
	    addr_cells, size_cells)) {
		return (ERANGE);
	}
	*base = 0;
	*size = 0;
	rangesptr = &ranges[range_id];

	*base = fdt_data_get((void *)rangesptr, addr_cells);
	rangesptr += addr_cells;
	*base += fdt_data_get((void *)rangesptr, par_addr_cells);
	rangesptr += par_addr_cells;
	*size = fdt_data_get((void *)rangesptr, size_cells);
	return (0);
}

int
fdt_immr_addr(vm_offset_t immr_va)
{
	phandle_t node;
	u_long base, size;
	int r;

	/*
	 * Try to access the SOC node directly i.e. through /aliases/.
	 */
	if ((node = OF_finddevice("soc")) != 0)
		if (fdt_is_compatible_strict(node, "simple-bus"))
			goto moveon;
	/*
	 * Find the node the long way.
	 */
	if ((node = OF_finddevice("/")) == 0)
		return (ENXIO);

	if ((node = fdt_find_compatible(node, "simple-bus", 1)) == 0)
		return (ENXIO);

moveon:
	if ((r = fdt_get_range(node, 0, &base, &size)) == 0) {
		fdt_immr_pa = base;
		fdt_immr_va = immr_va;
		fdt_immr_size = size;
	}

	return (r);
}

/*
 * This routine is an early-usage version of the ofw_bus_is_compatible() when
 * the ofw_bus I/F is not available (like early console routines and similar).
 * Note the buffer has to be on the stack since malloc() is usually not
 * available in such cases either.
 */
int
fdt_is_compatible(phandle_t node, const char *compatstr)
{
	char buf[FDT_COMPAT_LEN];
	char *compat;
	int len, onelen, l, rv;

	if ((len = OF_getproplen(node, "compatible")) <= 0)
		return (0);

	compat = (char *)&buf;
	bzero(compat, FDT_COMPAT_LEN);

	if (OF_getprop(node, "compatible", compat, FDT_COMPAT_LEN) < 0)
		return (0);

	onelen = strlen(compatstr);
	rv = 0;
	while (len > 0) {
		if (strncasecmp(compat, compatstr, onelen) == 0) {
			/* Found it. */
			rv = 1;
			break;
		}
		/* Slide to the next sub-string. */
		l = strlen(compat) + 1;
		compat += l;
		len -= l;
	}

	return (rv);
}

int
fdt_is_compatible_strict(phandle_t node, const char *compatible)
{
	char compat[FDT_COMPAT_LEN];

	if (OF_getproplen(node, "compatible") <= 0)
		return (0);

	if (OF_getprop(node, "compatible", compat, FDT_COMPAT_LEN) < 0)
		return (0);

	if (strncasecmp(compat, compatible, FDT_COMPAT_LEN) == 0)
		/* This fits. */
		return (1);

	return (0);
}

phandle_t
fdt_find_compatible(phandle_t start, const char *compat, int strict)
{
	phandle_t child;

	/*
	 * Traverse all children of 'start' node, and find first with
	 * matching 'compatible' property.
	 */
	for (child = OF_child(start); child != 0; child = OF_peer(child))
		if (fdt_is_compatible(child, compat)) {
			if (strict)
				if (!fdt_is_compatible_strict(child, compat))
					continue;
			return (child);
		}
	return (0);
}

int
fdt_is_enabled(phandle_t node)
{
	char *stat;
	int ena, len;

	len = OF_getprop_alloc(node, "status", sizeof(char),
	    (void **)&stat);

	if (len <= 0)
		/* It is OK if no 'status' property. */
		return (1);

	/* Anything other than 'okay' means disabled. */
	ena = 0;
	if (strncmp((char *)stat, "okay", len) == 0)
		ena = 1;

	free(stat, M_OFWPROP);
	return (ena);
}

int
fdt_is_type(phandle_t node, const char *typestr)
{
	char type[FDT_TYPE_LEN];

	if (OF_getproplen(node, "device_type") <= 0)
		return (0);

	if (OF_getprop(node, "device_type", type, FDT_TYPE_LEN) < 0)
		return (0);

	if (strncasecmp(type, typestr, FDT_TYPE_LEN) == 0)
		/* This fits. */
		return (1);

	return (0);
}

int
fdt_parent_addr_cells(phandle_t node)
{
	pcell_t addr_cells;

	/* Find out #address-cells of the superior bus. */
	if (OF_searchprop(OF_parent(node), "#address-cells", &addr_cells,
	    sizeof(addr_cells)) <= 0)
		addr_cells = 2;

	return ((int)fdt32_to_cpu(addr_cells));
}

int
fdt_data_verify(void *data, int cells)
{
	uint64_t d64;

	if (cells > 1) {
		d64 = fdt64_to_cpu(*((uint64_t *)data));
		if (((d64 >> 32) & 0xffffffffull) != 0 || cells > 2)
			return (ERANGE);
	}

	return (0);
}

int
fdt_pm_is_enabled(phandle_t node)
{
	int ret;

	ret = 1;

#if defined(SOC_MV_KIRKWOOD) || defined(SOC_MV_DISCOVERY)
	ret = fdt_pm(node);
#endif
	return (ret);
}

u_long
fdt_data_get(void *data, int cells)
{

	if (cells == 1)
		return (fdt32_to_cpu(*((uint32_t *)data)));

	return (fdt64_to_cpu(*((uint64_t *)data)));
}

int
fdt_addrsize_cells(phandle_t node, int *addr_cells, int *size_cells)
{
	pcell_t cell;
	int cell_size;

	/*
	 * Retrieve #{address,size}-cells.
	 */
	cell_size = sizeof(cell);
	if (OF_getprop(node, "#address-cells", &cell, cell_size) < cell_size)
		cell = 2;
	*addr_cells = fdt32_to_cpu((int)cell);

	if (OF_getprop(node, "#size-cells", &cell, cell_size) < cell_size)
		cell = 1;
	*size_cells = fdt32_to_cpu((int)cell);

	if (*addr_cells > 3 || *size_cells > 2)
		return (ERANGE);
	return (0);
}

int
fdt_ranges_verify(pcell_t *ranges, int tuples, int par_addr_cells,
    int this_addr_cells, int this_size_cells)
{
	int i, rv, ulsz;

	if (par_addr_cells > 2 || this_addr_cells > 2 || this_size_cells > 2)
		return (ERANGE);

	/*
	 * This is the max size the resource manager can handle for addresses
	 * and sizes.
	 */
	ulsz = sizeof(u_long);
	if (par_addr_cells <= ulsz && this_addr_cells <= ulsz &&
	    this_size_cells <= ulsz)
		/* We can handle everything */
		return (0);

	rv = 0;
	for (i = 0; i < tuples; i++) {

		if (fdt_data_verify((void *)ranges, par_addr_cells))
			goto err;
		ranges += par_addr_cells;

		if (fdt_data_verify((void *)ranges, this_addr_cells))
			goto err;
		ranges += this_addr_cells;

		if (fdt_data_verify((void *)ranges, this_size_cells))
			goto err;
		ranges += this_size_cells;
	}

	return (0);

err:
	debugf("using address range >%d-bit not supported\n", ulsz * 8);
	return (ERANGE);
}

int
fdt_data_to_res(pcell_t *data, int addr_cells, int size_cells, u_long *start,
    u_long *count)
{

	/* Address portion. */
	if (fdt_data_verify((void *)data, addr_cells))
		return (ERANGE);

	*start = fdt_data_get((void *)data, addr_cells);
	data += addr_cells;

	/* Size portion. */
	if (fdt_data_verify((void *)data, size_cells))
		return (ERANGE);

	*count = fdt_data_get((void *)data, size_cells);
	return (0);
}

int
fdt_regsize(phandle_t node, u_long *base, u_long *size)
{
	pcell_t reg[4];
	int addr_cells, len, size_cells;

	if (fdt_addrsize_cells(OF_parent(node), &addr_cells, &size_cells))
		return (ENXIO);

	if ((sizeof(pcell_t) * (addr_cells + size_cells)) > sizeof(reg))
		return (ENOMEM);

	len = OF_getprop(node, "reg", &reg, sizeof(reg));
	if (len <= 0)
		return (EINVAL);

	*base = fdt_data_get(&reg[0], addr_cells);
	*size = fdt_data_get(&reg[addr_cells], size_cells);
	return (0);
}

int
fdt_reg_to_rl(phandle_t node, struct resource_list *rl)
{
	u_long end, count, start;
	pcell_t *reg, *regptr;
	pcell_t addr_cells, size_cells;
	int tuple_size, tuples;
	int i, rv;
	long busaddr, bussize;

	if (fdt_addrsize_cells(OF_parent(node), &addr_cells, &size_cells) != 0)
		return (ENXIO);
	if (fdt_get_range(OF_parent(node), 0, &busaddr, &bussize)) {
		busaddr = 0;
		bussize = 0;
	}

	tuple_size = sizeof(pcell_t) * (addr_cells + size_cells);
	tuples = OF_getprop_alloc(node, "reg", tuple_size, (void **)&reg);
	debugf("addr_cells = %d, size_cells = %d\n", addr_cells, size_cells);
	debugf("tuples = %d, tuple size = %d\n", tuples, tuple_size);
	if (tuples <= 0)
		/* No 'reg' property in this node. */
		return (0);

	regptr = reg;
	for (i = 0; i < tuples; i++) {

		rv = fdt_data_to_res(reg, addr_cells, size_cells, &start,
		    &count);
		if (rv != 0) {
			resource_list_free(rl);
			goto out;
		}
		reg += addr_cells + size_cells;

		/* Calculate address range relative to base. */
		start += busaddr;
		end = start + count - 1;

		debugf("reg addr start = %lx, end = %lx, count = %lx\n", start,
		    end, count);

		resource_list_add(rl, SYS_RES_MEMORY, i, start, end,
		    count);
	}
	rv = 0;

out:
	free(regptr, M_OFWPROP);
	return (rv);
}

int
fdt_intr_to_rl(device_t dev, phandle_t node, struct resource_list *rl,
    struct fdt_sense_level *intr_sl)
{
	phandle_t iparent;
	uint32_t *intr, icells;
	int nintr, i, k;

	nintr = OF_getencprop_alloc(node, "interrupts",  sizeof(*intr),
	    (void **)&intr);
	if (nintr > 0) {
		iparent = 0;
		OF_searchencprop(node, "interrupt-parent", &iparent,
		    sizeof(iparent));
		OF_searchencprop(OF_xref_phandle(iparent), "#interrupt-cells",
		    &icells, sizeof(icells));
		for (i = 0, k = 0; i < nintr; i += icells, k++) {
			intr[i] = ofw_bus_map_intr(dev, iparent, intr[i]);
			resource_list_add(rl, SYS_RES_IRQ, k, intr[i], intr[i],
			    1);
			if (icells > 1)
				ofw_bus_config_intr(dev, intr[i], intr[i+1]);
		}
		free(intr, M_OFWPROP);
	}

	return (0);
}

int
fdt_get_phyaddr(phandle_t node, device_t dev, int *phy_addr, void **phy_sc)
{
	phandle_t phy_node;
	pcell_t phy_handle, phy_reg;
	uint32_t i;
	device_t parent, child;

	if (OF_getencprop(node, "phy-handle", (void *)&phy_handle,
	    sizeof(phy_handle)) <= 0)
		return (ENXIO);

	phy_node = OF_xref_phandle(phy_handle);

	if (OF_getprop(phy_node, "reg", (void *)&phy_reg,
	    sizeof(phy_reg)) <= 0)
		return (ENXIO);

	*phy_addr = fdt32_to_cpu(phy_reg);

	/*
	 * Search for softc used to communicate with phy.
	 */

	/*
	 * Step 1: Search for ancestor of the phy-node with a "phy-handle"
	 * property set.
	 */
	phy_node = OF_parent(phy_node);
	while (phy_node != 0) {
		if (OF_getprop(phy_node, "phy-handle", (void *)&phy_handle,
		    sizeof(phy_handle)) > 0)
			break;
		phy_node = OF_parent(phy_node);
	}
	if (phy_node == 0)
		return (ENXIO);

	/*
	 * Step 2: For each device with the same parent and name as ours
	 * compare its node with the one found in step 1, ancestor of phy
	 * node (stored in phy_node).
	 */
	parent = device_get_parent(dev);
	i = 0;
	child = device_find_child(parent, device_get_name(dev), i);
	while (child != NULL) {
		if (ofw_bus_get_node(child) == phy_node)
			break;
		i++;
		child = device_find_child(parent, device_get_name(dev), i);
	}
	if (child == NULL)
		return (ENXIO);

	/*
	 * Use softc of the device found.
	 */
	*phy_sc = (void *)device_get_softc(child);

	return (0);
}

int
fdt_get_reserved_regions(struct mem_region *mr, int *mrcnt)
{
	pcell_t reserve[FDT_REG_CELLS * FDT_MEM_REGIONS];
	pcell_t *reservep;
	phandle_t memory, root;
	uint32_t memory_size;
	int addr_cells, size_cells;
	int i, max_size, res_len, rv, tuple_size, tuples;

	max_size = sizeof(reserve);
	root = OF_finddevice("/");
	memory = OF_finddevice("/memory");
	if (memory == -1) {
		rv = ENXIO;
		goto out;
	}

	if ((rv = fdt_addrsize_cells(OF_parent(memory), &addr_cells,
	    &size_cells)) != 0)
		goto out;

	if (addr_cells > 2) {
		rv = ERANGE;
		goto out;
	}

	tuple_size = sizeof(pcell_t) * (addr_cells + size_cells);

	res_len = OF_getproplen(root, "memreserve");
	if (res_len <= 0 || res_len > sizeof(reserve)) {
		rv = ERANGE;
		goto out;
	}

	if (OF_getprop(root, "memreserve", reserve, res_len) <= 0) {
		rv = ENXIO;
		goto out;
	}

	memory_size = 0;
	tuples = res_len / tuple_size;
	reservep = (pcell_t *)&reserve;
	for (i = 0; i < tuples; i++) {

		rv = fdt_data_to_res(reservep, addr_cells, size_cells,
			(u_long *)&mr[i].mr_start, (u_long *)&mr[i].mr_size);

		if (rv != 0)
			goto out;

		reservep += addr_cells + size_cells;
	}

	*mrcnt = i;
	rv = 0;
out:
	return (rv);
}

int
fdt_get_mem_regions(struct mem_region *mr, int *mrcnt, uint32_t *memsize)
{
	pcell_t reg[FDT_REG_CELLS * FDT_MEM_REGIONS];
	pcell_t *regp;
	phandle_t memory;
	uint32_t memory_size;
	int addr_cells, size_cells;
	int i, max_size, reg_len, rv, tuple_size, tuples;

	max_size = sizeof(reg);
	memory = OF_finddevice("/memory");
	if (memory == -1) {
		rv = ENXIO;
		goto out;
	}

	if ((rv = fdt_addrsize_cells(OF_parent(memory), &addr_cells,
	    &size_cells)) != 0)
		goto out;

	if (addr_cells > 2) {
		rv = ERANGE;
		goto out;
	}

	tuple_size = sizeof(pcell_t) * (addr_cells + size_cells);
	reg_len = OF_getproplen(memory, "reg");
	if (reg_len <= 0 || reg_len > sizeof(reg)) {
		rv = ERANGE;
		goto out;
	}

	if (OF_getprop(memory, "reg", reg, reg_len) <= 0) {
		rv = ENXIO;
		goto out;
	}

	memory_size = 0;
	tuples = reg_len / tuple_size;
	regp = (pcell_t *)&reg;
	for (i = 0; i < tuples; i++) {

		rv = fdt_data_to_res(regp, addr_cells, size_cells,
			(u_long *)&mr[i].mr_start, (u_long *)&mr[i].mr_size);

		if (rv != 0)
			goto out;

		regp += addr_cells + size_cells;
		memory_size += mr[i].mr_size;
	}

	if (memory_size == 0) {
		rv = ERANGE;
		goto out;
	}

	*mrcnt = i;
	*memsize = memory_size;
	rv = 0;
out:
	return (rv);
}

int
fdt_get_unit(device_t dev)
{
	const char * name;

	name = ofw_bus_get_name(dev);
	name = strchr(name, '@') + 1;

	return (strtol(name,NULL,0));
}
