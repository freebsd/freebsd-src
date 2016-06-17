/*
 * SN2 Platform specific SMP Support
 *
 * Copyright (C) 2000-2003 Silicon Graphics, Inc. All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of version 2 of the GNU General Public License 
 * as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * 
 * Further, this software is distributed without any warranty that it is 
 * free of the rightful claim of any third person regarding infringement 
 * or the like.  Any license provided herein, whether implied or 
 * otherwise, applies only to this software file.  Patent licenses, if 
 * any, provided herein do not apply to combinations of this program with 
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pkwy, 
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/NoticeExplan
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/threads.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mmzone.h>

#include <asm/processor.h>
#include <asm/irq.h>
#include <asm/sal.h>
#include <asm/system.h>
#include <asm/delay.h>
#include <asm/io.h>
#include <asm/smp.h>
#include <asm/hw_irq.h>
#include <asm/current.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/addrs.h>
#include <asm/sn/sn2/shub_mmr.h>
#include <asm/sn/nodepda.h>
#include <asm/sn/rw_mmr.h>

void sn2_ptc_deadlock_recovery(unsigned long data0, unsigned long data1);


static spinlock_t sn2_global_ptc_lock __cacheline_aligned = SPIN_LOCK_UNLOCKED;

static unsigned long sn2_ptc_deadlock_count;


static inline unsigned long
wait_piowc(void)
{
	volatile unsigned long *piows;
	unsigned long	ws;

	piows = pda.pio_write_status_addr;
	do {
		__asm__ __volatile__ ("mf.a" ::: "memory");
	} while (((ws = *piows) & SH_PIO_WRITE_STATUS_0_PENDING_WRITE_COUNT_MASK) != 
			SH_PIO_WRITE_STATUS_0_PENDING_WRITE_COUNT_MASK);
	return ws;
}



/**
 * sn2_global_tlb_purge - globally purge translation cache of virtual address range
 * @start: start of virtual address range
 * @end: end of virtual address range
 * @nbits: specifies number of bytes to purge per instruction (num = 1<<(nbits & 0xfc))
 *
 * Purges the translation caches of all processors of the given virtual address
 * range.
 */

void
sn2_global_tlb_purge (unsigned long start, unsigned long end, unsigned long nbits)
{
	int			cnode, mycnode, nasid, flushed=0;
	volatile unsigned	long	*ptc0, *ptc1;
	unsigned long		flags=0, data0, data1;

	data0 = (1UL<<SH_PTC_0_A_SHFT) |
		(nbits<<SH_PTC_0_PS_SHFT) |
		((ia64_get_rr(start)>>8)<<SH_PTC_0_RID_SHFT) |
		(1UL<<SH_PTC_0_START_SHFT);

	ptc0 = (long*)GLOBAL_MMR_PHYS_ADDR(0, SH_PTC_0);
	ptc1 = (long*)GLOBAL_MMR_PHYS_ADDR(0, SH_PTC_1);

	mycnode = numa_node_id();

	spin_lock_irqsave(&sn2_global_ptc_lock, flags);

	do {
		data1 = start | (1UL<<SH_PTC_1_START_SHFT);
		for (cnode = 0; cnode < numnodes; cnode++) {
			if (is_headless_node(cnode))
				continue;
			if (cnode == mycnode) {
				asm volatile ("ptc.ga %0,%1;;srlz.i;;" :: "r"(start), "r"(nbits<<2) : "memory");
			} else if (current->shared_mm || test_bit(cnode, current->node_history)) {
				nasid = cnodeid_to_nasid(cnode);
				ptc0 = CHANGE_NASID(nasid, ptc0);
				ptc1 = CHANGE_NASID(nasid, ptc1);
				pio_atomic_phys_write_mmrs(ptc0, data0, ptc1, data1);
				flushed = 1;
			}
		}

		if (flushed && (wait_piowc() & SH_PIO_WRITE_STATUS_0_WRITE_DEADLOCK_MASK)) {
			sn2_ptc_deadlock_recovery(data0, data1);
		}

		start += (1UL << nbits);

	} while (start < end);

	spin_unlock_irqrestore(&sn2_global_ptc_lock, flags);

}

/*
 * sn2_ptc_deadlock_recovery
 *
 * Recover from PTC deadlocks conditions. Recovery requires stepping thru each 
 * TLB flush transaction.  The recovery sequence is somewhat tricky & is
 * coded in assembly language.
 */
void
sn2_ptc_deadlock_recovery(unsigned long data0, unsigned long data1)
{
	extern void sn2_ptc_deadlock_recovery_core(long*, long, long*, long, long*);
	int	cnode, mycnode, nasid;
	long	*ptc0, *ptc1, *piows;

	sn2_ptc_deadlock_count++;

	ptc0 = (long*)GLOBAL_MMR_PHYS_ADDR(0, SH_PTC_0);
	ptc1 = (long*)GLOBAL_MMR_PHYS_ADDR(0, SH_PTC_1);
	piows = (long*)pda.pio_write_status_addr;

	mycnode = numa_node_id();

	for (cnode = 0; cnode < numnodes; cnode++) {
		if (is_headless_node(cnode) || cnode == mycnode)
			continue;
		nasid = cnodeid_to_nasid(cnode);
		ptc0 = CHANGE_NASID(nasid, ptc0);
		ptc1 = CHANGE_NASID(nasid, ptc1);
		sn2_ptc_deadlock_recovery_core(ptc0, data0, ptc1, data1, piows);
	}
}

/**
 * sn_send_IPI_phys - send an IPI to a Nasid and slice
 * @physid: physical cpuid to receive the interrupt.
 * @vector: command to send
 * @delivery_mode: delivery mechanism
 *
 * Sends an IPI (interprocessor interrupt) to the processor specified by
 * @physid
 *
 * @delivery_mode can be one of the following
 *
 * %IA64_IPI_DM_INT - pend an interrupt
 * %IA64_IPI_DM_PMI - pend a PMI
 * %IA64_IPI_DM_NMI - pend an NMI
 * %IA64_IPI_DM_INIT - pend an INIT interrupt
 */
void
sn_send_IPI_phys(long physid, int vector, int delivery_mode)
{
	long		nasid, slice, val;
	unsigned long	flags=0;
	volatile long	*p;

	nasid = cpu_physical_id_to_nasid(physid);
        slice = cpu_physical_id_to_slice(physid);

	p = (long*)GLOBAL_MMR_PHYS_ADDR(nasid, SH_IPI_INT);
	val =   (1UL<<SH_IPI_INT_SEND_SHFT) | 
		(physid<<SH_IPI_INT_PID_SHFT) | 
	        ((long)delivery_mode<<SH_IPI_INT_TYPE_SHFT) | 
		((long)vector<<SH_IPI_INT_IDX_SHFT) |
		(0x000feeUL<<SH_IPI_INT_BASE_SHFT);

	mb();
	if (enable_shub_wars_1_1() ) {
		spin_lock_irqsave(&sn2_global_ptc_lock, flags);
	}
	pio_phys_write_mmr(p, val);
	if (enable_shub_wars_1_1() ) {
		wait_piowc();
		spin_unlock_irqrestore(&sn2_global_ptc_lock, flags);
	}

}

/**
 * sn2_send_IPI - send an IPI to a processor
 * @cpuid: target of the IPI
 * @vector: command to send
 * @delivery_mode: delivery mechanism
 * @redirect: redirect the IPI?
 *
 * Sends an IPI (InterProcessor Interrupt) to the processor specified by
 * @cpuid.  @vector specifies the command to send, while @delivery_mode can 
 * be one of the following
 *
 * %IA64_IPI_DM_INT - pend an interrupt
 * %IA64_IPI_DM_PMI - pend a PMI
 * %IA64_IPI_DM_NMI - pend an NMI
 * %IA64_IPI_DM_INIT - pend an INIT interrupt
 */
void
sn2_send_IPI(int cpuid, int vector, int delivery_mode, int redirect)
{
	long		physid;

	physid = cpu_physical_id(cpuid);

	sn_send_IPI_phys(physid, vector, delivery_mode);
}
