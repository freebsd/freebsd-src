/*
 * ip27-irq.c: Highlevel interrupt handling for IP27 architecture.
 *
 * Copyright (C) 1999, 2000 Ralf Baechle (ralf@gnu.org)
 * Copyright (C) 1999, 2000 Silicon Graphics, Inc.
 * Copyright (C) 1999 - 2001 Kanoj Sarcar
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/timex.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/smp_lock.h>
#include <linux/kernel_stat.h>
#include <linux/delay.h>

#include <asm/bitops.h>
#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/mipsregs.h>
#include <asm/system.h>

#include <asm/ptrace.h>
#include <asm/processor.h>
#include <asm/pci/bridge.h>
#include <asm/sn/sn0/hub.h>
#include <asm/sn/sn0/ip27.h>
#include <asm/sn/addrs.h>
#include <asm/sn/agent.h>
#include <asm/sn/arch.h>
#include <asm/sn/intr.h>
#include <asm/sn/intr_public.h>


#undef DEBUG_IRQ
#ifdef DEBUG_IRQ
#define DBG(x...) printk(x)
#else
#define DBG(x...)
#endif

/* These should die */
unsigned char bus_to_wid[256];	/* widget id for linux pci bus */
unsigned char bus_to_nid[256];	/* nasid for linux pci bus */
unsigned char num_bridges;	/* number of bridges in the system */

/*
 * Linux has a controller-independent x86 interrupt architecture.
 * every controller has a 'controller-template', that is used
 * by the main code to do the right thing. Each driver-visible
 * interrupt source is transparently wired to the apropriate
 * controller. Thus drivers need not be aware of the
 * interrupt-controller.
 *
 * Various interrupt controllers we handle: 8259 PIC, SMP IO-APIC,
 * PIIX4's internal 8259 PIC and SGI's Visual Workstation Cobalt (IO-)APIC.
 * (IO-APICs assumed to be messaging to Pentium local-APICs)
 *
 * the code is designed to be easily extended with new/different
 * interrupt controllers, without having to do assembly magic.
 */

extern asmlinkage void ip27_irq(void);

extern int irq_to_bus[], irq_to_slot[], bus_to_cpu[];
int intr_connect_level(int cpu, int bit);
int intr_disconnect_level(int cpu, int bit);

/*
 * There is a single intpend register per node, and we want to have
 * distinct levels for intercpu intrs for both cpus A and B on a node.
 */
int node_level_to_irq[MAX_COMPACT_NODES][PERNODE_LEVELS];

/*
 * use these macros to get the encoded nasid and widget id
 * from the irq value
 */
#define IRQ_TO_BUS(i)			irq_to_bus[(i)]
#define IRQ_TO_CPU(i)			bus_to_cpu[IRQ_TO_BUS(i)]
#define NASID_FROM_PCI_IRQ(i)		bus_to_nid[IRQ_TO_BUS(i)]
#define WID_FROM_PCI_IRQ(i)		bus_to_wid[IRQ_TO_BUS(i)]
#define	SLOT_FROM_PCI_IRQ(i)		irq_to_slot[i]

static inline int alloc_level(cpuid_t cpunum, int irq)
{
	cnodeid_t nodenum = CPUID_TO_COMPACT_NODEID(cpunum);
	int j = LEAST_LEVEL + 3;	/* resched & crosscall entries taken */

	while (++j < PERNODE_LEVELS) {
		if (node_level_to_irq[nodenum][j] == -1) {
			node_level_to_irq[nodenum][j] = irq;
			return j;
		}
	}
	printk("Cpu %ld flooded with devices\n", cpunum);
	while(1);
	return -1;
}

static inline int find_level(cpuid_t *cpunum, int irq)
{
	int j;
	cnodeid_t nodenum = INVALID_CNODEID;

	while (++nodenum < MAX_COMPACT_NODES) {
		j = LEAST_LEVEL + 3;	/* resched & crosscall entries taken */
		while (++j < PERNODE_LEVELS)
			if (node_level_to_irq[nodenum][j] == irq) {
				*cpunum = 0;	/* XXX Fixme */
				return(j);
			}
	}
	printk("Could not identify cpu/level for irq %d\n", irq);
	while(1);
	return(-1);
}

/*
 * Find first bit set
 */
static int ms1bit(unsigned long x)
{
	int b = 0, s;

	s = 16; if (x >> 16 == 0) s = 0; b += s; x >>= s;
	s =  8; if (x >>  8 == 0) s = 0; b += s; x >>= s;
	s =  4; if (x >>  4 == 0) s = 0; b += s; x >>= s;
	s =  2; if (x >>  2 == 0) s = 0; b += s; x >>= s;
	s =  1; if (x >>  1 == 0) s = 0; b += s;

	return b;
}

/*
 * This code is unnecessarily complex, because we do SA_INTERRUPT
 * intr enabling. Basically, once we grab the set of intrs we need
 * to service, we must mask _all_ these interrupts; firstly, to make
 * sure the same intr does not intr again, causing recursion that
 * can lead to stack overflow. Secondly, we can not just mask the
 * one intr we are do_IRQing, because the non-masked intrs in the
 * first set might intr again, causing multiple servicings of the
 * same intr. This effect is mostly seen for intercpu intrs.
 * Kanoj 05.13.00
 */
void ip27_do_irq(struct pt_regs *regs)
{
	int irq, swlevel;
	hubreg_t pend0, mask0;
	cpuid_t thiscpu = smp_processor_id();
	int pi_int_mask0 = ((cputoslice(thiscpu) == 0) ?
					PI_INT_MASK0_A : PI_INT_MASK0_B);

	/* copied from Irix intpend0() */
	while (((pend0 = LOCAL_HUB_L(PI_INT_PEND0)) &
				(mask0 = LOCAL_HUB_L(pi_int_mask0))) != 0) {
		pend0 &= mask0;		/* Pick intrs we should look at */
		if (pend0) {
			/* Prevent any of the picked intrs from recursing */
			LOCAL_HUB_S(pi_int_mask0, mask0 & ~(pend0));
			do {
				swlevel = ms1bit(pend0);
				LOCAL_HUB_CLR_INTR(swlevel);
				/* "map" swlevel to irq */
				irq = LEVEL_TO_IRQ(thiscpu, swlevel);
				do_IRQ(irq, regs);
				/* clear bit in pend0 */
				pend0 ^= 1ULL << swlevel;
			} while(pend0);
			/* Now allow the set of serviced intrs again */
			LOCAL_HUB_S(pi_int_mask0, mask0);
			LOCAL_HUB_L(PI_INT_PEND0);
		}
	}
}


/* Startup one of the (PCI ...) IRQs routes over a bridge.  */
static unsigned int startup_bridge_irq(unsigned int irq)
{
	bridgereg_t device;
	bridge_t *bridge;
	int pin, swlevel;
	cpuid_t cpu;
	nasid_t master = NASID_FROM_PCI_IRQ(irq);

	if (irq < BASE_PCI_IRQ)
		return 0;

        bridge = (bridge_t *) NODE_SWIN_BASE(master, WID_FROM_PCI_IRQ(irq));
	pin = SLOT_FROM_PCI_IRQ(irq);
	cpu = IRQ_TO_CPU(irq);

	DBG("bridge_startup(): irq= 0x%x  pin=%d\n", irq, pin);
	/*
	 * "map" irq to a swlevel greater than 6 since the first 6 bits
	 * of INT_PEND0 are taken
	 */
	swlevel = alloc_level(cpu, irq);
	intr_connect_level(cpu, swlevel);

	bridge->b_int_addr[pin].addr = (0x20000 | swlevel | (master << 8));
	bridge->b_int_enable |= (1 << pin);
	/* more stuff in int_enable reg */
	bridge->b_int_enable |= 0x7ffffe00;

	/*
	 * XXX This only works if b_int_device is initialized to 0!
	 * We program the bridge to have a 1:1 mapping between devices
	 * (slots) and intr pins.
	 */
	device = bridge->b_int_device;
	device |= (pin << (pin*3));
	bridge->b_int_device = device;

        bridge->b_widget.w_tflush;                      /* Flush */

        return 0;       /* Never anything pending.  */
}

/* Shutdown one of the (PCI ...) IRQs routes over a bridge.  */
static unsigned int shutdown_bridge_irq(unsigned int irq)
{
	bridge_t *bridge;
	int pin, swlevel;
	cpuid_t cpu;

	if (irq < BASE_PCI_IRQ)
		return 0;

	bridge = (bridge_t *) NODE_SWIN_BASE(NASID_FROM_PCI_IRQ(irq),
	                                     WID_FROM_PCI_IRQ(irq));
	DBG("bridge_shutdown: irq 0x%x\n", irq);
	pin = SLOT_FROM_PCI_IRQ(irq);

	/*
	 * map irq to a swlevel greater than 6 since the first 6 bits
	 * of INT_PEND0 are taken
	 */
	swlevel = find_level(&cpu, irq);
	intr_disconnect_level(cpu, swlevel);
	LEVEL_TO_IRQ(cpu, swlevel) = -1;

	bridge->b_int_enable &= ~(1 << pin);
	bridge->b_widget.w_tflush;                      /* Flush */

	return 0;       /* Never anything pending.  */
}

static inline void enable_bridge_irq(unsigned int irq)
{
	/* All the braindamage happens magically for us in ip27_do_irq */
}

static void disable_bridge_irq(unsigned int irq)
{
	/* All the braindamage happens magically for us in ip27_do_irq */
}

static void mask_and_ack_bridge_irq(unsigned int irq)
{
	/* All the braindamage happens magically for us in ip27_do_irq */
}

static void end_bridge_irq (unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		enable_bridge_irq(irq);
}

static struct hw_interrupt_type bridge_irq_type = {
	"bridge",
	startup_bridge_irq,
	shutdown_bridge_irq,
	enable_bridge_irq,
	disable_bridge_irq,
	mask_and_ack_bridge_irq,
	end_bridge_irq
};

void irq_debug(void)
{
	bridge_t *bridge = (bridge_t *) 0x9200000008000000;

	printk("bridge->b_int_status = 0x%x\n", bridge->b_int_status);
	printk("bridge->b_int_enable = 0x%x\n", bridge->b_int_enable);
	printk("PI_INT_PEND0   = 0x%lx\n", LOCAL_HUB_L(PI_INT_PEND0));
	printk("PI_INT_MASK0_A = 0x%lx\n", LOCAL_HUB_L(PI_INT_MASK0_A));
}

void __init init_IRQ(void)
{
	int i;

	set_except_vector(0, ip27_irq);

	/*
	 * Right now the bridge irq is our kitchen sink interrupt type
	 */
	for (i = 0; i <= NR_IRQS; i++) {
		irq_desc[i].status	= IRQ_DISABLED;
		irq_desc[i].action	= 0;
		irq_desc[i].depth	= 1;
		irq_desc[i].handler	= &bridge_irq_type;
	}
}

/*
 * Get values that vary depending on which CPU and bit we're operating on.
 */
static hub_intmasks_t *intr_get_ptrs(cpuid_t cpu, int bit, int *new_bit,
				hubreg_t **intpend_masks, int *ip)
{
	hub_intmasks_t *hub_intmasks;

	hub_intmasks = &cpu_data[cpu].p_intmasks;
	if (bit < N_INTPEND_BITS) {
		*intpend_masks = hub_intmasks->intpend0_masks;
		*ip = 0;
		*new_bit = bit;
	} else {
		*intpend_masks = hub_intmasks->intpend1_masks;
		*ip = 1;
		*new_bit = bit - N_INTPEND_BITS;
	}
	return hub_intmasks;
}

int intr_connect_level(int cpu, int bit)
{
	int ip;
	int slice = cputoslice(cpu);
	volatile hubreg_t *mask_reg;
	hubreg_t *intpend_masks;
	nasid_t nasid = COMPACT_TO_NASID_NODEID(cputocnode(cpu));

	(void)intr_get_ptrs(cpu, bit, &bit, &intpend_masks, &ip);

	/* Make sure it's not already pending when we connect it. */
	REMOTE_HUB_CLR_INTR(nasid, bit + ip * N_INTPEND_BITS);

	intpend_masks[0] |= (1ULL << (u64)bit);

	if (ip == 0) {
		mask_reg = REMOTE_HUB_ADDR(nasid, PI_INT_MASK0_A +
				PI_INT_MASK_OFFSET * slice);
	} else {
		mask_reg = REMOTE_HUB_ADDR(nasid, PI_INT_MASK1_A +
				PI_INT_MASK_OFFSET * slice);
	}
	HUB_S(mask_reg, intpend_masks[0]);
	return(0);
}

int intr_disconnect_level(int cpu, int bit)
{
	int ip;
	int slice = cputoslice(cpu);
	volatile hubreg_t *mask_reg;
	hubreg_t *intpend_masks;
	nasid_t nasid = COMPACT_TO_NASID_NODEID(cputocnode(cpu));

	(void)intr_get_ptrs(cpu, bit, &bit, &intpend_masks, &ip);
	intpend_masks[0] &= ~(1ULL << (u64)bit);
	if (ip == 0) {
		mask_reg = REMOTE_HUB_ADDR(nasid, PI_INT_MASK0_A +
				PI_INT_MASK_OFFSET * slice);
	} else {
		mask_reg = REMOTE_HUB_ADDR(nasid, PI_INT_MASK1_A +
				PI_INT_MASK_OFFSET * slice);
	}
	HUB_S(mask_reg, intpend_masks[0]);
	return(0);
}


void handle_resched_intr(int irq, void *dev_id, struct pt_regs *regs)
{
	/* Nothing, the return from intr will work for us */
}

#ifdef CONFIG_SMP

void core_send_ipi(int destid, unsigned int action)
{
	int irq;

#if (CPUS_PER_NODE == 2)
	switch (action) {
		case SMP_RESCHEDULE_YOURSELF:
			irq = CPU_RESCHED_A_IRQ;
			break;
		case SMP_CALL_FUNCTION:
			irq = CPU_CALL_A_IRQ;
			break;
		default:
			panic("sendintr");
	}
	irq += cputoslice(destid);

	/*
	 * Convert the compact hub number to the NASID to get the correct
	 * part of the address space.  Then set the interrupt bit associated
	 * with the CPU we want to send the interrupt to.
	 */
	REMOTE_HUB_SEND_INTR(COMPACT_TO_NASID_NODEID(cputocnode(destid)),
			FAST_IRQ_TO_LEVEL(irq));
#else
	<< Bomb!  Must redefine this for more than 2 CPUS. >>
#endif
}

#endif

extern void smp_call_function_interrupt(void);

void install_cpuintr(int cpu)
{
#ifdef CONFIG_SMP
#if (CPUS_PER_NODE == 2)
	static int done = 0;

	/*
	 * This is a hack till we have a pernode irqlist. Currently,
	 * just have the master cpu set up the handlers for the per
	 * cpu irqs.
	 */
	if (done == 0) {
		int j;

		if (request_irq(CPU_RESCHED_A_IRQ, handle_resched_intr,
							0, "resched", 0))
			panic("intercpu intr unconnectible");
		if (request_irq(CPU_RESCHED_B_IRQ, handle_resched_intr,
							0, "resched", 0))
			panic("intercpu intr unconnectible");
		if (request_irq(CPU_CALL_A_IRQ, smp_call_function_interrupt,
							0, "callfunc", 0))
			panic("intercpu intr unconnectible");
		if (request_irq(CPU_CALL_B_IRQ, smp_call_function_interrupt,
							0, "callfunc", 0))
			panic("intercpu intr unconnectible");

		for (j = 0; j < PERNODE_LEVELS; j++)
			LEVEL_TO_IRQ(0, j) = -1;
		LEVEL_TO_IRQ(0, FAST_IRQ_TO_LEVEL(CPU_RESCHED_A_IRQ)) =
							CPU_RESCHED_A_IRQ;
		LEVEL_TO_IRQ(0, FAST_IRQ_TO_LEVEL(CPU_RESCHED_B_IRQ)) =
							CPU_RESCHED_B_IRQ;
		LEVEL_TO_IRQ(0, FAST_IRQ_TO_LEVEL(CPU_CALL_A_IRQ)) =
							CPU_CALL_A_IRQ;
		LEVEL_TO_IRQ(0, FAST_IRQ_TO_LEVEL(CPU_CALL_B_IRQ)) =
							CPU_CALL_B_IRQ;
		for (j = 1; j < MAX_COMPACT_NODES; j++)
			memcpy(&node_level_to_irq[j][0],
			&node_level_to_irq[0][0],
			sizeof(node_level_to_irq[0][0])*PERNODE_LEVELS);

		done = 1;
	}

	intr_connect_level(cpu, FAST_IRQ_TO_LEVEL(CPU_RESCHED_A_IRQ +
							cputoslice(cpu)));
	intr_connect_level(cpu, FAST_IRQ_TO_LEVEL(CPU_CALL_A_IRQ +
							cputoslice(cpu)));
#else /* CPUS_PER_NODE */
#error Must redefine this for more than 2 CPUS.
#endif /* CPUS_PER_NODE */
#endif /* CONFIG_SMP */
}

void install_tlbintr(int cpu)
{
#if 0
	int intr_bit = N_INTPEND_BITS + TLB_INTR_A + cputoslice(cpu);

	intr_connect_level(cpu, intr_bit);
#endif
}
