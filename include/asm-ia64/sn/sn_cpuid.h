/* 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */


#ifndef _ASM_IA64_SN_SN_CPUID_H
#define _ASM_IA64_SN_SN_CPUID_H

#include <linux/config.h>
#include <linux/smp.h>
#include <linux/sched.h>
#include <linux/mmzone.h>
#include <asm/sn/types.h>
#include <asm/current.h>
#include <asm/nodedata.h>
#include <asm/sn/pda.h>


/*
 * Functions for converting between cpuids, nodeids and NASIDs.
 * 
 * These are for SGI platforms only.
 *
 */




/*
 *  Definitions of terms (these definitions are for IA64 ONLY. Other architectures
 *  use cpuid/cpunum quite defferently):
 *
 *	   CPUID - a number in range of 0..NR_CPUS-1 that uniquely identifies
 *		the cpu. The value cpuid has no significance on IA64 other than
 *		the boot cpu is 0.
 *			smp_processor_id() returns the cpuid of the current cpu.
 *
 *	   CPUNUM - On IA64, a cpunum and cpuid are the same. This is NOT true
 *		on other architectures like IA32.
 *
 * 	   CPU_PHYSICAL_ID (also known as HARD_PROCESSOR_ID)
 *		This is the same as 31:24 of the processor LID register
 *			hard_smp_processor_id()- cpu_physical_id of current processor
 *			cpu_physical_id(cpuid) - convert a <cpuid> to a <physical_cpuid>
 *			cpu_logical_id(phy_id) - convert a <physical_cpuid> to a <cpuid> 
 *				* not real efficient - don't use in perf critical code
 *
 *         LID - processor defined register (see PRM V2).
 *
 *           On SN2
 *		31:28 - id   Contains 0-3 to identify the cpu on the node
 *		27:16 - eid  Contains the NASID
 *
 *
 *
 * The following assumes the following mappings for LID register values:
 *
 * The macros convert between cpu physical ids & slice/nasid/cnodeid.
 * These terms are described below:
 *
 *
 * Brick
 *          -----   -----           -----   -----       CPU
 *          | 0 |   | 1 |           | 0 |   | 1 |       SLICE
 *          -----   -----           -----   -----
 *            |       |               |       |
 *            |       |               |       |
 *          0 |       | 2           0 |       | 2       FSB SLOT
 *             -------                 -------  
 *                |                       |
 *                |                       |
 *                |                       |
 *             ------------      -------------
 *             |          |      |           |
 *             |    SHUB  |      |   SHUB    |        NASID   (0..MAX_NASIDS)
 *             |          |----- |           |        CNODEID (0..num_compact_nodes-1)
 *             |          |      |           |
 *             |          |      |           |
 *             ------------      -------------
 *                   |                 |
 *                           
 *
 */

#ifndef CONFIG_SMP
#define cpu_logical_id(cpu)				0
#define cpu_physical_id(cpuid)			((ia64_get_lid() >> 16) & 0xffff)
#endif

/*
 * macros for some of these exist in sn/addrs.h & sn/arch.h, etc. However, 
 * trying #include these files here causes circular dependencies.
 */
#define cpu_physical_id_to_nasid(cpi)		((cpi) &0xfff)
#define cpu_physical_id_to_slice(cpi)		((cpi>>12) & 3)
#define get_nasid()				((ia64_get_lid() >> 16) & 0xfff)
#define get_slice()				((ia64_get_lid() >> 28) & 0xf)
#define get_node_number(addr)			(((unsigned long)(addr)>>38) & 0x7ff)

/*
 * NOTE: id & eid refer to Intel's definitions of the LID register
 * 
 * NOTE: on non-MP systems, only cpuid 0 exists
 */
#define id_eid_to_cpu_physical_id(id,eid)	 	(((id)<<8) | (eid))

#define nasid_slice_to_cpuid(nasid,slice)		(cpu_logical_id(nasid_slice_to_cpu_physical_id((nasid),(slice))))

#define nasid_slice_to_cpu_physical_id(nasid, slice)	(((slice)<<12) | (nasid))

/*
 * The following table/struct  is used for managing PTC coherency domains.
 */
typedef struct {
	u8	domain;
	u8	reserved;
	u16	sapicid;
} sn_sapicid_info_t;

extern sn_sapicid_info_t	sn_sapicid_info[];	/* indexed by cpuid */
extern short physical_node_map[];			/* indexed by nasid to get cnode */


/*
 * cpuid_to_slice  - convert a cpuid to the slice that it resides on
 *  There are 4 cpus per node. This function returns 0 .. 3)
 */
#define cpuid_to_slice(cpuid)		(cpu_physical_id_to_slice(cpu_physical_id(cpuid)))


/*
 * cpuid_to_nasid  - convert a cpuid to the NASID that it resides on
 */
#define cpuid_to_nasid(cpuid)		(cpu_physical_id_to_nasid(cpu_physical_id(cpuid)))


/*
 * cpuid_to_cnodeid  - convert a cpuid to the cnode that it resides on
 */
#define cpuid_to_cnodeid(cpuid)		(physical_node_map[cpuid_to_nasid(cpuid)])


/*
 * cnodeid_to_nasid - convert a cnodeid to a NASID
 *	Macro relies on pg_data for a node being on the node itself.
 *	Just extract the NASID from the pointer.
 *
 */
#define cnodeid_to_nasid(cnodeid)	pda.cnodeid_to_nasid_table[cnodeid]
 

/*
 * nasid_to_cnodeid - convert a NASID to a cnodeid
 */
#define nasid_to_cnodeid(nasid)		(physical_node_map[nasid])


/*
 * cnode_slice_to_cpuid - convert a codeid & slice to a cpuid
 */

#define cnode_slice_to_cpuid(cnodeid,slice) (nasid_slice_to_cpuid(cnodeid_to_nasid(cnodeid),(slice)))
 

/*
 * cpuid_to_subnode - convert a cpuid to the subnode it resides on.
 *   slice 0 & 1 are on subnode 0
 *   slice 2 & 3 are on subnode 1.
 */
#define cpuid_to_subnode(cpuid)		((cpuid_to_slice(cpuid)<2) ? 0 : 1)
 

#define smp_physical_node_id()			(cpuid_to_nasid(smp_processor_id()))


#endif /* _ASM_IA64_SN_SN_CPUID_H */

