/*
 * linux/include/asm-ia64/topology.h
 *
 * Copyright (C) 2002, Erich Focht, NEC
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _ASM_IA64_TOPOLOGY_H
#define _ASM_IA64_TOPOLOGY_H

#include <asm/acpi.h>
#include <asm/numa.h>
#include <asm/smp.h>

#ifdef CONFIG_NUMA
/*
 * Returns the number of the node containing CPU 'cpu'
 */
#define __cpu_to_node(cpu) (int)(cpu_to_node_map[cpu])

/*
 * Returns a bitmask of CPUs on Node 'node'.
 */
#define __node_to_cpu_mask(node) (node_to_cpu_mask[node])

#else
#define __cpu_to_node(cpu) (0)
#define __node_to_cpumask(node) (&phys_cpu_present_map)
#endif

/*
 * Returns the number of the node containing MemBlk 'memblk'
 */
#ifdef CONFIG_ACPI_NUMA
#define __memblk_to_node(memblk) (node_memblk[memblk].nid)
#else
#define __memblk_to_node(memblk) (memblk)
#endif

/*
 * Returns the number of the node containing Node 'nid'.
 * Not implemented here. Multi-level hierarchies detected with
 * the help of node_distance().
 */
#define __parent_node(nid) (nid)

/*
 * Returns the number of the first CPU on Node 'node'.
 */
#define __node_to_first_cpu(node) (__ffs(__node_to_cpu_mask(node)))

/*
 * Returns the number of the first MemBlk on Node 'node'
 * Should be fixed when IA64 discontigmem goes in.
 */
#define __node_to_memblk(node) (node)

#endif /* _ASM_IA64_TOPOLOGY_H */
