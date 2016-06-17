/* 
 * arch/ppc/kernel/xics.c
 *
 * Copyright 2000 IBM Corporation.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/threads.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/prom.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/smp.h>
#include <asm/naca.h>
#include <asm/rtas.h>
#include "i8259.h"
#include "xics.h"
#include <asm/ppcdebug.h>

void xics_enable_irq(u_int irq);
void xics_disable_irq(u_int irq);
void xics_mask_and_ack_irq(u_int irq);
void xics_end_irq(u_int irq);
void xics_set_affinity(unsigned int irq_nr, unsigned long cpumask);

struct hw_interrupt_type xics_pic = {
	" XICS     ",
	NULL,
	NULL,
	xics_enable_irq,
	xics_disable_irq,
	xics_mask_and_ack_irq,
	xics_end_irq,
	xics_set_affinity
};

struct hw_interrupt_type xics_8259_pic = {
	" XICS/8259",
	NULL,
	NULL,
	NULL,
	NULL,
	xics_mask_and_ack_irq,
	NULL
};

#define XICS_IPI		2
#define XICS_IRQ_SPURIOUS	0

/* Want a priority other than 0.  Various HW issues require this. */
#define	DEFAULT_PRIORITY	5

struct xics_ipl {
	union {
		u32	word;
		u8	bytes[4];
	} xirr_poll;
	union {
		u32 word;
		u8	bytes[4];
	} xirr;
	u32	dummy;
	union {
		u32	word;
		u8	bytes[4];
	} qirr;
};

struct xics_info {
	volatile struct xics_ipl *	per_cpu[NR_CPUS];
};

struct xics_info	xics_info;

unsigned long long intr_base = 0;
int xics_irq_8259_cascade = 0;
int xics_irq_8259_cascade_real = 0;
unsigned int default_server = 0xFF;
unsigned int default_distrib_server = 0;

/* RTAS service tokens */
int ibm_get_xive;
int ibm_set_xive;
int ibm_int_on;
int ibm_int_off;

struct xics_interrupt_node {
	unsigned long long addr;
	unsigned long long size;
} inodes[NR_CPUS*2];	 

typedef struct {
	int (*xirr_info_get)(int cpu);
	void (*xirr_info_set)(int cpu, int val);
	void (*cppr_info)(int cpu, u8 val);
	void (*qirr_info)(int cpu, u8 val);
} xics_ops;


static int pSeries_xirr_info_get(int n_cpu)
{
	return (xics_info.per_cpu[n_cpu]->xirr.word);
}

static void pSeries_xirr_info_set(int n_cpu, int value)
{
	xics_info.per_cpu[n_cpu]->xirr.word = value;
}

static void pSeries_cppr_info(int n_cpu, u8 value)
{
	xics_info.per_cpu[n_cpu]->xirr.bytes[0] = value;
}

static void pSeries_qirr_info(int n_cpu , u8 value)
{
	xics_info.per_cpu[n_cpu]->qirr.bytes[0] = value;
}

static xics_ops pSeries_ops = {
	pSeries_xirr_info_get,
	pSeries_xirr_info_set,
	pSeries_cppr_info,
	pSeries_qirr_info
};

static xics_ops *ops = &pSeries_ops;
extern xics_ops pSeriesLP_ops;


void
xics_enable_irq(u_int virq)
{
	u_int		irq;
	unsigned long	status;
	long	        call_status;
	unsigned int    interrupt_server = default_server;

	irq = irq_offset_down(virq);
	if (irq == XICS_IPI)
		return;

#ifdef CONFIG_IRQ_ALL_CPUS
	if((smp_num_cpus == systemcfg->processorCount) &&
	   (smp_threads_ready)) {
		interrupt_server = default_distrib_server;
	}
#endif
	call_status = rtas_call(ibm_set_xive, 3, 1, (unsigned long*)&status,
				irq, interrupt_server, DEFAULT_PRIORITY);

	if( call_status != 0 ) {
		printk("xics_enable_irq: irq=%x: rtas_call failed; retn=%lx, status=%lx\n",
		       irq, call_status, status);
		return;
	}
	/* Now unmask the interrupt (often a no-op) */
	call_status = rtas_call(ibm_int_on, 1, 1, (unsigned long*)&status, 
				irq);
	if( call_status != 0 ) {
		printk("xics_disable_irq on: irq=%x: rtas_call failed, retn=%lx\n",
		       irq, call_status);
		return;
	}
}

void
xics_disable_irq(u_int virq)
{
	u_int		irq;
	unsigned long 	status;
	long 	        call_status;

	irq = irq_offset_down(virq);
	call_status = rtas_call(ibm_int_off, 1, 1, NULL, irq);
	if( call_status != 0 ) {
		printk("xics_disable_irq: irq=%x: rtas_call failed, retn=%lx\n",
		       irq, call_status);
		return;
	}
}

void
xics_end_irq(u_int irq)
{
	int cpu = smp_processor_id();

	iosync();
	ops->xirr_info_set(cpu, (0xff<<24) | irq_offset_down(irq));
}

void
xics_mask_and_ack_irq(u_int	irq)
{
	int cpu = smp_processor_id();

	if (irq < irq_offset_value()) {
		i8259_pic.ack(irq);
		iosync();
		ops->xirr_info_set(cpu, ((0xff<<24) | xics_irq_8259_cascade_real));
		iosync();
	}
	else {
		ops->cppr_info(cpu, 0xff);
		iosync();
	}
}

int
xics_get_irq(struct pt_regs *regs)
{
	u_int	cpu = smp_processor_id();
	u_int	vec;
	int irq;

	vec = ops->xirr_info_get(cpu);
	/*  (vec >> 24) == old priority */
	vec &= 0x00ffffff;
	/* for sanity, this had better be < NR_IRQS - 16 */
	if( vec == xics_irq_8259_cascade_real ) {
		irq = i8259_irq(cpu);
		if(irq == -1) {
			/* Spurious cascaded interrupt.  Still must ack xics */
			xics_end_irq(irq_offset_up(xics_irq_8259_cascade));
			irq = -1;
		}
	} else if( vec == XICS_IRQ_SPURIOUS ) {
		irq = -1;
	} else {
		irq = irq_offset_up(vec);
	}
	return irq;
}


#ifdef CONFIG_SMP
void xics_ipi_action(int irq, void *dev_id, struct pt_regs *regs)
{
	extern volatile unsigned long xics_ipi_message[];
	int cpu = smp_processor_id();

	ops->qirr_info(cpu, 0xff);
	while (xics_ipi_message[cpu]) {
		if (test_and_clear_bit(PPC_MSG_CALL_FUNCTION, &xics_ipi_message[cpu])) {
			mb();
			smp_message_recv(PPC_MSG_CALL_FUNCTION, regs);
		}
		if (test_and_clear_bit(PPC_MSG_RESCHEDULE, &xics_ipi_message[cpu])) {
			mb();
			smp_message_recv(PPC_MSG_RESCHEDULE, regs);
		}
#ifdef CONFIG_XMON
		if (test_and_clear_bit(PPC_MSG_XMON_BREAK, &xics_ipi_message[cpu])) {
			mb();
			smp_message_recv(PPC_MSG_XMON_BREAK, regs);
		}
#endif
	}
}

void xics_cause_IPI(int cpu)
{
	ops->qirr_info(cpu,0) ;
}

void xics_setup_cpu(void)
{
	int cpu = smp_processor_id();

	ops->cppr_info(cpu, 0xff);
	iosync();
}
#endif /* CONFIG_SMP */

void xics_init_irq_desc(irq_desc_t *desc)
{
	/* Don't mess with the handler if already set.
	 * This leaves the setup of isa handlers undisturbed.
	 */
	if (!desc->handler)
		desc->handler = &xics_pic;
}

void
xics_init_IRQ( void )
{
	int i;
	unsigned long intr_size = 0;
	struct device_node *np;
	uint *ireg, ilen, indx=0;

	ppc64_boot_msg(0x20, "XICS Init");

	ibm_get_xive = rtas_token("ibm,get-xive");
	ibm_set_xive = rtas_token("ibm,set-xive");
	ibm_int_on  = rtas_token("ibm,int-on");
	ibm_int_off = rtas_token("ibm,int-off");

	np = find_type_devices("PowerPC-External-Interrupt-Presentation");
	if (!np) {
		printk(KERN_WARNING "Can't find Interrupt Presentation\n");
		udbg_printf("Can't find Interrupt Presentation\n");
		while (1);
	}
nextnode:
	ireg = (uint *)get_property(np, "ibm,interrupt-server-ranges", 0);
	if (ireg) {
		/*
		 * set node starting index for this node
		 */
		indx = *ireg;
	}

	ireg = (uint *)get_property(np, "reg", &ilen);
	if (!ireg) {
		printk(KERN_WARNING "Can't find Interrupt Reg Property\n");
		udbg_printf("Can't find Interrupt Reg Property\n");
		while (1);
	}
	
	while (ilen) {
		inodes[indx].addr = (unsigned long long)*ireg++ << 32;
		ilen -= sizeof(uint);
		inodes[indx].addr |= *ireg++;
		ilen -= sizeof(uint);
		inodes[indx].size = (unsigned long long)*ireg++ << 32;
		ilen -= sizeof(uint);
		inodes[indx].size |= *ireg++;
		ilen -= sizeof(uint);
		indx++;
		if (indx >= NR_CPUS) break;
	}

	np = np->next;
	if ((indx < NR_CPUS) && np) goto nextnode;

	/* Find the server numbers for the boot cpu. */
	for (np = find_type_devices("cpu"); np; np = np->next) {
		ireg = (uint *)get_property(np, "reg", &ilen);
		if (ireg && ireg[0] == hard_smp_processor_id()) {
			ireg = (uint *)get_property(np, "ibm,ppc-interrupt-gserver#s", &ilen);
			i = ilen / sizeof(int);
			if (ireg && i > 0) {
				default_server = ireg[0];
				default_distrib_server = ireg[i-1]; /* take last element */
			}
			break;
		}
	}

	intr_base = inodes[0].addr;
	intr_size = (ulong)inodes[0].size;

	np = find_type_devices("interrupt-controller");
	if (!np) {
		printk(KERN_WARNING "xics:  no ISA Interrupt Controller\n");
		xics_irq_8259_cascade_real = -1;
		xics_irq_8259_cascade = -1;
	} else {
		ireg = (uint *) get_property(np, "interrupts", 0);
		if (!ireg) {
			printk(KERN_WARNING "Can't find ISA Interrupts Property\n");
			udbg_printf("Can't find ISA Interrupts Property\n");
			while (1);
		}
		xics_irq_8259_cascade_real = *ireg;
		xics_irq_8259_cascade = xics_irq_8259_cascade_real;
	}

	if (systemcfg->platform == PLATFORM_PSERIES) {
#ifdef CONFIG_SMP
		for (i = 0; i < systemcfg->processorCount; ++i) {
			xics_info.per_cpu[i] =
			  __ioremap((ulong)inodes[get_hard_smp_processor_id(i)].addr, 
				  (ulong)inodes[get_hard_smp_processor_id(i)].size, _PAGE_NO_CACHE);
		}
#else
		xics_info.per_cpu[0] = __ioremap((ulong)intr_base, intr_size, _PAGE_NO_CACHE);
#endif /* CONFIG_SMP */
#ifdef CONFIG_PPC_PSERIES
	/* actually iSeries does not use any of xics...but it has link dependencies
	 * for now, except this new one...
	 */
	} else if (systemcfg->platform == PLATFORM_PSERIES_LPAR) {
		ops = &pSeriesLP_ops;
#endif
	}

	xics_8259_pic.enable = i8259_pic.enable;
	xics_8259_pic.disable = i8259_pic.disable;
	for (i = 0; i < 16; ++i)
		real_irqdesc(i)->handler = &xics_8259_pic;

	ops->cppr_info(0, 0xff);
	iosync();
	if (xics_irq_8259_cascade != -1) {
		if (request_irq(irq_offset_up(xics_irq_8259_cascade),
				no_action, 0, "8259 cascade", 0))
			printk(KERN_ERR "xics_init_IRQ: couldn't get 8259 cascade\n");
		i8259_init();
	}

#ifdef CONFIG_SMP
	request_irq(irq_offset_up(XICS_IPI), xics_ipi_action, 0, "IPI", 0);
	real_irqdesc(irq_offset_up(XICS_IPI))->status |= IRQ_PER_CPU;
#endif
	ppc64_boot_msg(0x21, "XICS Done");
}

/*
 * Find first logical cpu and return its physical cpu number
 */
static inline u32 physmask(u32 cpumask)
{
	int i;

	for (i = 0; i < smp_num_cpus; ++i, cpumask >>= 1) {
		if (cpumask & 1)
			return get_hard_smp_processor_id(i);
	}

	printk(KERN_ERR "xics_set_affinity: invalid irq mask\n");

	return default_distrib_server;
}

void xics_set_affinity(unsigned int irq, unsigned long cpumask)
{
	irq_desc_t *desc = irqdesc(irq);
	unsigned long flags;
	long status;
	unsigned long xics_status[2];
	u32 newmask;

	irq = irq_offset_down(irq);
	if (irq == XICS_IPI)
		return;

	spin_lock_irqsave(&desc->lock, flags);

	status = rtas_call(ibm_get_xive, 1, 3, (void *)&xics_status, irq);

	if (status) {
		printk("xics_set_affinity: irq=%d ibm,get-xive returns %ld\n",
			irq, status);
		goto out;
	}

	/* For the moment only implement delivery to all cpus or one cpu */
	if (cpumask == 0xffffffff)
		newmask = default_distrib_server;
	else
		newmask = physmask(cpumask);

	status = rtas_call(ibm_set_xive, 3, 1, NULL,
				irq, newmask, xics_status[1]);

	if (status) {
		printk("xics_set_affinity irq=%d ibm,set-xive returns %ld\n",
			irq, status);
		goto out;
	}

out:
	spin_unlock_irqrestore(&desc->lock, flags);
}
