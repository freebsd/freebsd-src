/*
 * Platform dependent support for SGI SN
 *
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
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
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <asm/current.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/sn/sgi.h>
#include <asm/sn/iograph.h>
#include <asm/sn/invent.h>
#include <linux/devfs_fs_kernel.h>
#include <asm/sn/hcl.h>
#include <asm/sn/types.h>
#include <asm/sn/pci/bridge.h>
#include <asm/sn/pci/pciio.h>
#include <asm/sn/pci/pciio_private.h>
#include <asm/sn/pci/pcibr.h>
#include <asm/sn/pci/pcibr_private.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/io.h>
#include <asm/sn/intr.h>
#include <asm/sn/addrs.h>
#include <asm/sn/driver.h>
#include <asm/sn/arch.h>
#include <asm/sn/pda.h>
#include <asm/processor.h>
#include <asm/system.h>
#include <asm/bitops.h>

int irq_to_bit_pos(int irq);
static void force_interrupt(int irq);
extern void pcibr_force_interrupt(pcibr_intr_t intr);
extern int sn_force_interrupt_flag;

struct pcibr_intr_list_t {
	struct pcibr_intr_list_t *next;
	pcibr_intr_t intr;
};

static struct pcibr_intr_list_t **pcibr_intr_list;



static unsigned int
sn_startup_irq(unsigned int irq)
{
        return(0);
}

static void
sn_shutdown_irq(unsigned int irq)
{
}

static void
sn_disable_irq(unsigned int irq)
{
}

static void
sn_enable_irq(unsigned int irq)
{
}

static void
sn_ack_irq(unsigned int irq)
{
	unsigned long event_occurred, mask = 0;
	int nasid;

	irq = irq & 0xff;
	nasid = smp_physical_node_id();
	event_occurred = HUB_L( (unsigned long *)GLOBAL_MMR_ADDR(nasid,SH_EVENT_OCCURRED) );
	if (event_occurred & SH_EVENT_OCCURRED_UART_INT_MASK) {
		mask |= (1 << SH_EVENT_OCCURRED_UART_INT_SHFT);
	}
	if (event_occurred & SH_EVENT_OCCURRED_IPI_INT_MASK) {
		mask |= (1 << SH_EVENT_OCCURRED_IPI_INT_SHFT);
	}
	if (event_occurred & SH_EVENT_OCCURRED_II_INT0_MASK) {
		mask |= (1 << SH_EVENT_OCCURRED_II_INT0_SHFT);
	}
	if (event_occurred & SH_EVENT_OCCURRED_II_INT1_MASK) {
		mask |= (1 << SH_EVENT_OCCURRED_II_INT1_SHFT);
	}
	HUB_S((unsigned long *)GLOBAL_MMR_ADDR(nasid, SH_EVENT_OCCURRED_ALIAS), mask );
	__set_bit(irq, (volatile void *)pda.sn_in_service_ivecs);
}

static void
sn_end_irq(unsigned int irq)
{
	int nasid;
	int ivec;
	unsigned long event_occurred;

	ivec = irq & 0xff;
	if (ivec == SGI_UART_VECTOR) {
		nasid = smp_physical_node_id();
		event_occurred = HUB_L( (unsigned long *)GLOBAL_MMR_ADDR(nasid,SH_EVENT_OCCURRED) );
		// If the UART bit is set here, we may have received an interrupt from the
		// UART that the driver missed.  To make sure, we IPI ourselves to force us
		// to look again.
		if (event_occurred & SH_EVENT_OCCURRED_UART_INT_MASK) {
				platform_send_ipi(smp_processor_id(), SGI_UART_VECTOR, IA64_IPI_DM_INT, 0);
		}
	}
	clear_bit(ivec, (volatile void *)pda.sn_in_service_ivecs);
	if (sn_force_interrupt_flag)
		force_interrupt(irq);
}

static void
sn_set_affinity_irq(unsigned int irq, unsigned long cpu)
{
	int redir = 0;
	struct pcibr_intr_list_t *p = pcibr_intr_list[irq];
	pcibr_intr_t intr;
	extern void sn_shub_redirect_intr(pcibr_intr_t intr, unsigned long cpu);
	extern void sn_tio_redirect_intr(pcibr_intr_t intr, unsigned long cpu);

	if (p == NULL)
		return;

	intr = p->intr;

	if (intr == NULL)
		return;

	if (IS_PIC_SOFT(intr->bi_soft) ) {
		sn_shub_redirect_intr(intr, cpu);
	// Defer TIO for now.
	// } else if (IS_TIO_SOFT(intr->bi_soft) {
		// sn_tio_redirect_intr(intr, cpu);
	} else {
		return;
	}
	(void) set_irq_affinity_info(irq, cpu_physical_id(cpu), redir);
}


struct hw_interrupt_type irq_type_sn = {
	"SN hub",
	sn_startup_irq,
	sn_shutdown_irq,
	sn_enable_irq,
	sn_disable_irq,
	sn_ack_irq, 
	sn_end_irq,
	sn_set_affinity_irq
};


struct irq_desc *
sn_irq_desc(unsigned int irq) {

	irq = SN_IVEC_FROM_IRQ(irq);

	return(_irq_desc + irq);
}

u8
sn_irq_to_vector(u8 irq) {
	return(irq);
}

unsigned int
sn_local_vector_to_irq(u8 vector) {
	return (CPU_VECTOR_TO_IRQ(smp_processor_id(), vector));
}

void
sn_irq_init (void)
{
	int i;
	irq_desc_t *base_desc = _irq_desc;

	for (i=0; i<NR_IRQS; i++) {
		if (base_desc[i].handler == &no_irq_type) {
			base_desc[i].handler = &irq_type_sn;
		}
	}
}

int
bit_pos_to_irq(int bit) {
#define BIT_TO_IRQ 64
	if (bit > 118) bit = 118;

        return bit + BIT_TO_IRQ;
}

int
irq_to_bit_pos(int irq) {
#define IRQ_TO_BIT 64
	int bit = irq - IRQ_TO_BIT;

        return bit;
}

void
register_pcibr_intr(int irq, pcibr_intr_t intr) {
	struct pcibr_intr_list_t *p = kmalloc(sizeof(struct pcibr_intr_list_t), GFP_KERNEL);
	struct pcibr_intr_list_t *list;
	int cpu = intr->bi_cpu;

	if (pcibr_intr_list == NULL) {
		pcibr_intr_list = kmalloc(sizeof(struct pcibr_intr_list_t *) * NR_IRQS, GFP_KERNEL);
		if (pcibr_intr_list == NULL) 
			pcibr_intr_list = vmalloc(sizeof(struct pcibr_intr_list_t *) * NR_IRQS);
		if (pcibr_intr_list == NULL) panic("Could not allocate memory for pcibr_intr_list\n");
		memset( (void *)pcibr_intr_list, 0, sizeof(struct pcibr_intr_list_t *) * NR_IRQS);
	}
	if (pdacpu(cpu).sn_last_irq < irq) {
		pdacpu(cpu).sn_last_irq = irq;
	}
	if (pdacpu(cpu).sn_first_irq == 0 || pdacpu(cpu).sn_first_irq > irq) pdacpu(cpu).sn_first_irq = irq;
	if (!p) panic("Could not allocate memory for pcibr_intr_list_t\n");
	if ((list = pcibr_intr_list[irq])) {
		while (list->next) list = list->next;
		list->next = p;
		p->next = NULL;
		p->intr = intr;
	} else {
		pcibr_intr_list[irq] = p;
		p->next = NULL;
		p->intr = intr;
	}
}

void
force_polled_int(void) {
	int i;
	struct pcibr_intr_list_t *p;

	for (i=0; i<NR_IRQS;i++) {
		p = pcibr_intr_list[i];
		while (p) {
			if (p->intr){
				pcibr_force_interrupt(p->intr);
			}
			p = p->next;
		}
	}
}

static void
force_interrupt(int irq) {
	struct pcibr_intr_list_t *p = pcibr_intr_list[irq];

	while (p) {
		if (p->intr) {
			pcibr_force_interrupt(p->intr);
		}
		p = p->next;
	}
}

/*
Check for lost interrupts.  If the PIC int_status reg. says that
an interrupt has been sent, but not handled, and the interrupt
is not pending in either the cpu irr regs or in the soft irr regs,
and the interrupt is not in service, then the interrupt may have
been lost.  Force an interrupt on that pin.  It is possible that
the interrupt is in flight, so we may generate a spurious interrupt,
but we should never miss a real lost interrupt.
*/

static void
sn_check_intr(int irq, pcibr_intr_t intr) {
	unsigned long regval;
	int irr_reg_num;
	int irr_bit;
	unsigned long irr_reg;


	regval = intr->bi_soft->bs_base->p_int_status_64;
	irr_reg_num = irq_to_vector(irq) / 64;
	irr_bit = irq_to_vector(irq) % 64;
	switch (irr_reg_num) {
		case 0:
			irr_reg = ia64_get_irr0();
			break;
		case 1:
			irr_reg = ia64_get_irr1();
			break;
		case 2:
			irr_reg = ia64_get_irr2();
			break;
		case 3:
			irr_reg = ia64_get_irr3();
			break;
	}
	if (!test_bit(irr_bit, &irr_reg) ) {
		if (!test_bit(irq, pda.sn_soft_irr) ) {
			if (!test_bit(irq, pda.sn_in_service_ivecs) ) {
				regval &= 0xff;
				if (intr->bi_ibits & regval & intr->bi_last_intr) {
					regval &= ~(intr->bi_ibits & regval);
					pcibr_force_interrupt(intr);
				}
			}
		}
	}
	intr->bi_last_intr = regval;
}

void
sn_lb_int_war_check(void) {
	int i;

	if (pda.sn_first_irq == 0) return;
	for (i=pda.sn_first_irq;
		i <= pda.sn_last_irq; i++) {
			struct pcibr_intr_list_t *p = pcibr_intr_list[i];
			if (p == NULL) {
				continue;
			}
			while (p) {
				sn_check_intr(i, p->intr);
				p = p->next;
			}
	}
}

static inline int
sn_get_next_bit(void) {
	int i;
	int bit;

	for (i = 3; i >= 0; i--) {
		if (pda.sn_soft_irr[i] != 0) {
			bit = (i * 64) +  __ffs(pda.sn_soft_irr[i]);
			__change_bit(bit, (volatile void *)pda.sn_soft_irr);
			return(bit);
		}
	}
	return IA64_SPURIOUS_INT_VECTOR;
}

void
sn_set_tpr(int vector) {
	if (vector > IA64_LAST_DEVICE_VECTOR || vector < IA64_FIRST_DEVICE_VECTOR) {
		ia64_set_tpr(vector);
	} else {
		ia64_set_tpr(IA64_LAST_DEVICE_VECTOR);
	}
}

static inline void
sn_get_all_ivr(void) {
	int vector;

	vector = ia64_get_ivr();
	while (vector != IA64_SPURIOUS_INT_VECTOR) {
		__set_bit(vector, (volatile void *)pda.sn_soft_irr);
		ia64_eoi();
		if (vector > IA64_LAST_DEVICE_VECTOR) return;
		vector = ia64_get_ivr();
	}
}
	
int
sn_get_ivr(void) {
	int vector;

	vector = sn_get_next_bit();
	if (vector == IA64_SPURIOUS_INT_VECTOR) {
		sn_get_all_ivr();
		vector = sn_get_next_bit();
	}
	return vector;
}
