/*
 * linux/arch/arm/mm/discontig.c
 *
 * Discontiguous memory support.
 *
 * Initial code: Copyright (C) 1999-2000 Nicolas Pitre
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/bootmem.h>

#if NR_NODES != 4
#error Fix Me Please
#endif

/*
 * Our node_data structure for discontiguous memory.
 */

static bootmem_data_t node_bootmem_data[NR_NODES];

pg_data_t discontig_node_data[NR_NODES] = {
  { bdata: &node_bootmem_data[0] },
  { bdata: &node_bootmem_data[1] },
  { bdata: &node_bootmem_data[2] },
  { bdata: &node_bootmem_data[3] }
};

EXPORT_SYMBOL(discontig_node_data);

