/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_IA64_SN_PDA_H
#define _ASM_IA64_SN_PDA_H

#include <linux/config.h>
#include <linux/cache.h>
#include <asm/system.h>
#include <asm/processor.h>
#include <asm/page.h>
#include <linux/mmzone.h>
#include <asm/sn/bte.h>


/*
 * CPU-specific data structure.
 *
 * One of these structures is allocated for each cpu of a NUMA system.
 *
 * This structure provides a convenient way of keeping together 
 * all SN per-cpu data structures. 
 */

typedef struct pda_s {

	/* Having a pointer in the begining of PDA tends to increase
	 * the chance of having this pointer in cache. (Yes something
	 * else gets pushed out). Doing this reduces the number of memory
	 * access to all nodepda variables to be one
	 */
	struct nodepda_s *p_nodepda;		/* Pointer to Per node PDA */
	struct subnodepda_s *p_subnodepda;	/* Pointer to CPU  subnode PDA */

	/*
	 * Support for SN LEDs
	 */
	volatile short	*led_address;
	u8		led_state;
	u8		hb_state;	/* supports blinking heartbeat leds */
	u8		shub_1_1_found;
	unsigned int	hb_count;

	unsigned int	idle_flag;
	
	volatile unsigned long *bedrock_rev_id;
	volatile unsigned long *pio_write_status_addr;
	volatile unsigned long *pio_shub_war_cam_addr;
	volatile unsigned long *mem_write_status_addr;

	struct bteinfo_s *cpu_bte_if[BTES_PER_NODE];	/* cpu interface order */

	unsigned long	sn_soft_irr[4];
	unsigned long	sn_in_service_ivecs[4];
	short		cnodeid_to_nasid_table[NR_NODES];	
	int		sn_lb_int_war_ticks;
	int		sn_last_irq;
	int		sn_first_irq;
} pda_t;


#define CACHE_ALIGN(x)	(((x) + SMP_CACHE_BYTES-1) & ~(SMP_CACHE_BYTES-1))

/*
 * PDA
 * Per-cpu private data area for each cpu. The PDA is located immediately after
 * the IA64 cpu_data area. A full page is allocated for the cp_data area for each
 * cpu but only a small amout of the page is actually used. We put the SNIA PDA
 * in the same page as the cpu_data area. Note that there is a check in the setup
 * code to verify that we don't overflow the page.
 *
 * Seems like we should should cache-line align the pda so that any changes in the
 * size of the cpu_data area don't change cache layout. Should we align to 32, 64, 128
 * or 512 boundary. Each has merits. For now, pick 128 but should be revisited later.
 */
#define CPU_DATA_END	CACHE_ALIGN((long)&(((struct cpuinfo_ia64*)0)->platform_specific))
#define PDAADDR		(PERCPU_ADDR+CPU_DATA_END)

#define pda		(*((pda_t *) PDAADDR))

#define pdacpu(cpu)	(*((pda_t *) ((long)cpu_data(cpu) + CPU_DATA_END)))

/*
 * Use this macro to test if shub 1.1 wars should be enabled
 */
#define enable_shub_wars_1_1()	(pda.shub_1_1_found)


#endif /* _ASM_IA64_SN_PDA_H */
