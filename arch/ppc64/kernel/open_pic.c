/*
 *  arch/ppc/kernel/open_pic.c -- OpenPIC Interrupt Handling
 *
 *  Copyright (C) 1997 Geert Uytterhoeven
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive
 *  for more details.
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <asm/ptrace.h>
#include <asm/signal.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/irq.h>
#include <asm/prom.h>
#include <asm/smp.h>
#include <asm/machdep.h>

#include "local_irq.h"
#include "open_pic.h"
#include "open_pic_defs.h"
#include "i8259.h"
#include <asm/ppcdebug.h>

void* OpenPIC_Addr;
static volatile struct OpenPIC *OpenPIC = NULL;
u_int OpenPIC_NumInitSenses __initdata = 0;
u_char *OpenPIC_InitSenses __initdata = NULL;

void find_ISUs(void);

static u_int NumProcessors;
static u_int NumSources;
static int NumISUs;
static int open_pic_irq_offset;
static volatile unsigned char* chrp_int_ack_special;
static int broken_ipi_registers;

OpenPIC_SourcePtr ISU[OPENPIC_MAX_ISU];

static void openpic_end_irq(unsigned int irq_nr);
static void openpic_set_affinity(unsigned int irq_nr, unsigned long cpumask);

struct hw_interrupt_type open_pic = {
	" OpenPIC  ",
	NULL,
	NULL,
	openpic_enable_irq,
	openpic_disable_irq,
	NULL,
	openpic_end_irq,
	openpic_set_affinity
};

#ifdef CONFIG_SMP
static void openpic_end_ipi(unsigned int irq_nr);
static void openpic_enable_ipi(unsigned int irq_nr);
static void openpic_disable_ipi(unsigned int irq_nr);

struct hw_interrupt_type open_pic_ipi = {
	" OpenPIC  ",
	NULL,
	NULL,
	openpic_enable_ipi,
	openpic_disable_ipi,
	NULL,
	openpic_end_ipi,
	NULL
};
#endif /* CONFIG_SMP */

unsigned int openpic_vec_ipi;
unsigned int openpic_vec_timer;
unsigned int openpic_vec_spurious;

/*
 *  Accesses to the current processor's openpic registers
 */
#ifdef CONFIG_SMP
#define THIS_CPU		Processor[cpu]
#define DECL_THIS_CPU		int cpu = hard_smp_processor_id()
#define CHECK_THIS_CPU		check_arg_cpu(cpu)
#else
#define THIS_CPU		Processor[hard_smp_processor_id()]
#define DECL_THIS_CPU
#define CHECK_THIS_CPU
#endif /* CONFIG_SMP */

#if 1
#define check_arg_ipi(ipi) \
    if (ipi < 0 || ipi >= OPENPIC_NUM_IPI) \
	printk(KERN_ERR "open_pic.c:%d: illegal ipi %d\n", __LINE__, ipi);
#define check_arg_timer(timer) \
    if (timer < 0 || timer >= OPENPIC_NUM_TIMERS) \
	printk(KERN_ERR "open_pic.c:%d: illegal timer %d\n", __LINE__, timer);
#define check_arg_vec(vec) \
    if (vec < 0 || vec >= OPENPIC_NUM_VECTORS) \
	printk(KERN_ERR "open_pic.c:%d: illegal vector %d\n", __LINE__, vec);
#define check_arg_pri(pri) \
    if (pri < 0 || pri >= OPENPIC_NUM_PRI) \
	printk(KERN_ERR "open_pic.c:%d: illegal priority %d\n", __LINE__, pri);
/*
 * Print out a backtrace if it's out of range, since if it's larger than NR_IRQ's
 * data has probably been corrupted and we're going to panic or deadlock later
 * anyway --Troy
 */
extern unsigned long* _get_SP(void);
#define check_arg_irq(irq) \
    if (irq < open_pic_irq_offset || irq >= (NumSources+open_pic_irq_offset)){ \
      printk(KERN_ERR "open_pic.c:%d: illegal irq %d\n", __LINE__, irq); \
      print_backtrace(_get_SP()); }
#define check_arg_cpu(cpu) \
    if (cpu < 0 || cpu >= OPENPIC_MAX_PROCESSORS){ \
	printk(KERN_ERR "open_pic.c:%d: illegal cpu %d\n", __LINE__, cpu); \
	print_backtrace(_get_SP()); }
#else
#define check_arg_ipi(ipi)	do {} while (0)
#define check_arg_timer(timer)	do {} while (0)
#define check_arg_vec(vec)	do {} while (0)
#define check_arg_pri(pri)	do {} while (0)
#define check_arg_irq(irq)	do {} while (0)
#define check_arg_cpu(cpu)	do {} while (0)
#endif

#define GET_ISU(source)	ISU[(source) >> 4][(source) & 0xf]

void
openpic_init_irq_desc(irq_desc_t *desc)
{
	/* Don't mess with the handler if already set.
	 * This leaves the setup of isa/ipi handlers undisturbed.
	 */
	if (!desc->handler)
		desc->handler = &open_pic;
}

void __init openpic_init_IRQ(void)
{
        struct device_node *np;
        int i;
        unsigned int *addrp;
        unsigned char* chrp_int_ack_special = 0;
	unsigned char init_senses[NR_IRQS - NUM_ISA_INTERRUPTS];
        int nmi_irq = -1;
#if defined(CONFIG_VT) && defined(CONFIG_ADB_KEYBOARD) && defined(XMON)
        struct device_node *kbd;
#endif

        if (!(np = find_devices("pci"))
            || !(addrp = (unsigned int *)
                 get_property(np, "8259-interrupt-acknowledge", NULL)))
                printk(KERN_ERR "Cannot find pci to get ack address\n");
        else
		chrp_int_ack_special = (unsigned char *)
			__ioremap(addrp[prom_n_addr_cells(np)-1], 1, _PAGE_NO_CACHE);
        /* hydra still sets OpenPIC_InitSenses to a static set of values */
        if (OpenPIC_InitSenses == NULL) {
		prom_get_irq_senses(init_senses, NUM_ISA_INTERRUPTS, NR_IRQS);
		OpenPIC_InitSenses = init_senses;
		OpenPIC_NumInitSenses = NR_IRQS - NUM_ISA_INTERRUPTS;
	}
	openpic_init(1, NUM_ISA_INTERRUPTS, chrp_int_ack_special, nmi_irq);
	for ( i = 0 ; i < NUM_ISA_INTERRUPTS  ; i++ )
		real_irqdesc(i)->handler = &i8259_pic;
        i8259_init();
}

static inline u_int openpic_read(volatile u_int *addr)
{
	u_int val;

	val = in_le32(addr);
	return val;
}

static inline void openpic_write(volatile u_int *addr, u_int val)
{
	out_le32(addr, val);
}

static inline u_int openpic_readfield(volatile u_int *addr, u_int mask)
{
	u_int val = openpic_read(addr);
	return val & mask;
}

static inline void openpic_writefield(volatile u_int *addr, u_int mask,
			       u_int field)
{
	u_int val = openpic_read(addr);
	openpic_write(addr, (val & ~mask) | (field & mask));
}

static inline void openpic_clearfield(volatile u_int *addr, u_int mask)
{
	openpic_writefield(addr, mask, 0);
}

static inline void openpic_setfield(volatile u_int *addr, u_int mask)
{
	openpic_writefield(addr, mask, mask);
}

static void openpic_safe_writefield(volatile u_int *addr, u_int mask,
				    u_int field)
{
	unsigned int loops = 100000;

	openpic_setfield(addr, OPENPIC_MASK);
	while (openpic_read(addr) & OPENPIC_ACTIVITY) {
		if (!loops--) {
			printk(KERN_ERR "openpic_safe_writefield timeout\n");
			break;
		}
	}
	openpic_writefield(addr, mask | OPENPIC_MASK, field | OPENPIC_MASK);
}

#ifdef CONFIG_SMP
static u_int openpic_read_IPI(volatile u_int* addr)
{
        u_int val = 0;

	if (broken_ipi_registers)
		/* yes this is right ... bug, feature, you decide! -- tgall */
		val = in_be32(addr);
	else
		val = in_le32(addr);

        return val;
}

static void openpic_test_broken_IPI(void)
{
	u_int t;

	openpic_write(&OpenPIC->Global.IPI_Vector_Priority(0), OPENPIC_MASK);
	t = openpic_read(&OpenPIC->Global.IPI_Vector_Priority(0));
	if (t == le32_to_cpu(OPENPIC_MASK)) {
		printk(KERN_INFO "OpenPIC reversed IPI registers detected\n");
		broken_ipi_registers = 1;
	}
}

/* because of the power3 be / le above, this is needed */
static inline void openpic_writefield_IPI(volatile u_int* addr, u_int mask, u_int field)
{
        u_int  val = openpic_read_IPI(addr);
        openpic_write(addr, (val & ~mask) | (field & mask));
}

static inline void openpic_clearfield_IPI(volatile u_int *addr, u_int mask)
{
        openpic_writefield_IPI(addr, mask, 0);
}

static inline void openpic_setfield_IPI(volatile u_int *addr, u_int mask)
{
        openpic_writefield_IPI(addr, mask, mask);
}

static void openpic_safe_writefield_IPI(volatile u_int *addr, u_int mask, u_int field)
{
	unsigned int loops = 100000;

        openpic_setfield_IPI(addr, OPENPIC_MASK);

        /* wait until it's not in use */
        /* BenH: Is this code really enough ? I would rather check the result
         *       and eventually retry ...
         */
        while(openpic_read_IPI(addr) & OPENPIC_ACTIVITY) {
		if (!loops--) {
			printk(KERN_ERR "openpic_safe_writefield timeout\n");
			break;
		}
	}

        openpic_writefield_IPI(addr, mask, field | OPENPIC_MASK);
}
#endif /* CONFIG_SMP */

void __init openpic_init(int main_pic, int offset, unsigned char* chrp_ack,
			 int programmer_switch_irq)
{
	u_int t, i;
	u_int timerfreq;
	const char *version;

	if (!OpenPIC_Addr) {
		printk(KERN_INFO "No OpenPIC found !\n");
		return;
	}
	OpenPIC = (volatile struct OpenPIC *)OpenPIC_Addr;

	ppc64_boot_msg(0x20, "OpenPic Init");

	t = openpic_read(&OpenPIC->Global.Feature_Reporting0);
	switch (t & OPENPIC_FEATURE_VERSION_MASK) {
	case 1:
		version = "1.0";
		break;
	case 2:
		version = "1.2";
		break;
	case 3:
		version = "1.3";
		break;
	default:
		version = "?";
		break;
	}
	NumProcessors = ((t & OPENPIC_FEATURE_LAST_PROCESSOR_MASK) >>
			 OPENPIC_FEATURE_LAST_PROCESSOR_SHIFT) + 1;
	NumSources = ((t & OPENPIC_FEATURE_LAST_SOURCE_MASK) >>
		      OPENPIC_FEATURE_LAST_SOURCE_SHIFT) + 1;
	printk(KERN_INFO "OpenPIC Version %s (%d CPUs and %d IRQ sources) at %p\n",
	       version, NumProcessors, NumSources, OpenPIC);
	timerfreq = openpic_read(&OpenPIC->Global.Timer_Frequency);
	if (timerfreq)
		printk(KERN_INFO "OpenPIC timer frequency is %d.%06d MHz\n",
		       timerfreq / 1000000, timerfreq % 1000000);

	if (!main_pic)
		return;

	open_pic_irq_offset = offset;
	chrp_int_ack_special = (volatile unsigned char*)chrp_ack;

	find_ISUs();

	/* Initialize timer interrupts */
	ppc64_boot_msg(0x21, "OpenPic Timer");
	for (i = 0; i < OPENPIC_NUM_TIMERS; i++) {
		/* Disabled, Priority 0 */
		openpic_inittimer(i, 0, openpic_vec_timer+i);
		/* No processor */
		openpic_maptimer(i, 0);
	}

#ifdef CONFIG_SMP
	/* Initialize IPI interrupts */
	ppc64_boot_msg(0x22, "OpenPic IPI");
	openpic_test_broken_IPI();
	for (i = 0; i < OPENPIC_NUM_IPI; i++) {
		/* Disabled, Priority 10..13 */
		openpic_initipi(i, 10+i, openpic_vec_ipi+i);
		/* IPIs are per-CPU */
		real_irqdesc(openpic_vec_ipi+i)->status |= IRQ_PER_CPU;
		real_irqdesc(openpic_vec_ipi+i)->handler = &open_pic_ipi;
	}
#endif

	/* Initialize external interrupts */
	ppc64_boot_msg(0x23, "OpenPic Ext");

	openpic_set_priority(0xf);

	/* SIOint (8259 cascade) is special */
	if (offset) {
		openpic_initirq(0, 8, offset, 1, 1);
		openpic_mapirq(0, 1<<get_hard_smp_processor_id(0));
	}

	/* Init all external sources */
	for (i = 1; i < NumSources; i++) {
		int pri, sense;

		/* the bootloader may have left it enabled (bad !) */
		openpic_disable_irq(i+offset);

		pri = (i == programmer_switch_irq)? 9: 8;
		sense = (i < OpenPIC_NumInitSenses)? OpenPIC_InitSenses[i]: 1;
		if (sense)
			real_irqdesc(i+offset)->status = IRQ_LEVEL;

		/* Enabled, Priority 8 or 9 */
		openpic_initirq(i, pri, i+offset, !sense, sense);
		/* Processor 0 */
		openpic_mapirq(i, 1<<get_hard_smp_processor_id(0));
	}

	/* Initialize the spurious interrupt */
	ppc64_boot_msg(0x24, "OpenPic Spurious");
	openpic_set_spurious(openpic_vec_spurious);

	/* Initialize the cascade */
	if (offset) {
		if (request_irq(offset, no_action, SA_INTERRUPT,
				"82c59 cascade", NULL))
			printk(KERN_ERR "Unable to get OpenPIC IRQ 0 for cascade\n");
	}
	openpic_set_priority(0);
	openpic_disable_8259_pass_through();

	ppc64_boot_msg(0x25, "OpenPic Done");
}

void openpic_setup_ISU(int isu_num, unsigned long addr)
{
	if (isu_num >= OPENPIC_MAX_ISU)
		return;
	ISU[isu_num] = (OpenPIC_SourcePtr) __ioremap(addr, 0x400, _PAGE_NO_CACHE);
	if (isu_num >= NumISUs)
		NumISUs = isu_num + 1;
}

void find_ISUs(void)
{
        /* Use /interrupt-controller/reg and
         * /interrupt-controller/interrupt-ranges from OF device tree
	 * the ISU array is setup in chrp_pci.c in ibm_add_bridges
	 * as a result
	 * -- tgall
         */

	/* basically each ISU is a bus, and this assumes that
	 * open_pic_isu_count interrupts per bus are possible 
	 * ISU == Interrupt Source
	 */
	NumSources = NumISUs * 0x10;
	openpic_vec_ipi = NumSources + open_pic_irq_offset;
	openpic_vec_timer = openpic_vec_ipi + OPENPIC_NUM_IPI; 
	openpic_vec_spurious = openpic_vec_timer + OPENPIC_NUM_TIMERS;
}

static inline void openpic_reset(void)
{
	openpic_setfield(&OpenPIC->Global.Global_Configuration0,
			 OPENPIC_CONFIG_RESET);
}

static inline void openpic_enable_8259_pass_through(void)
{
	openpic_clearfield(&OpenPIC->Global.Global_Configuration0,
			   OPENPIC_CONFIG_8259_PASSTHROUGH_DISABLE);
}

static void openpic_disable_8259_pass_through(void)
{
	openpic_setfield(&OpenPIC->Global.Global_Configuration0,
			 OPENPIC_CONFIG_8259_PASSTHROUGH_DISABLE);
}

/*
 *  Find out the current interrupt
 */
static u_int openpic_irq(void)
{
	u_int vec;
	DECL_THIS_CPU;

	CHECK_THIS_CPU;
	vec = openpic_readfield(&OpenPIC->THIS_CPU.Interrupt_Acknowledge,
				OPENPIC_VECTOR_MASK);
	return vec;
}

static void openpic_eoi(void)
{
	DECL_THIS_CPU;

	CHECK_THIS_CPU;
	openpic_write(&OpenPIC->THIS_CPU.EOI, 0);
	/* Handle PCI write posting */
	(void)openpic_read(&OpenPIC->THIS_CPU.EOI);
}


static inline u_int openpic_get_priority(void)
{
	DECL_THIS_CPU;

	CHECK_THIS_CPU;
	return openpic_readfield(&OpenPIC->THIS_CPU.Current_Task_Priority,
				 OPENPIC_CURRENT_TASK_PRIORITY_MASK);
}

static void openpic_set_priority(u_int pri)
{
	DECL_THIS_CPU;

	CHECK_THIS_CPU;
	check_arg_pri(pri);
	openpic_writefield(&OpenPIC->THIS_CPU.Current_Task_Priority,
			   OPENPIC_CURRENT_TASK_PRIORITY_MASK, pri);
}

/*
 *  Get/set the spurious vector
 */
static inline u_int openpic_get_spurious(void)
{
	return openpic_readfield(&OpenPIC->Global.Spurious_Vector,
				 OPENPIC_VECTOR_MASK);
}

static void openpic_set_spurious(u_int vec)
{
	check_arg_vec(vec);
	openpic_writefield(&OpenPIC->Global.Spurious_Vector, OPENPIC_VECTOR_MASK,
			   vec);
}

/*
 * Convert a cpu mask from logical to physical cpu numbers.
 */
static inline u32 physmask(u32 cpumask)
{
	int i;
	u32 mask = 0;

	for (i = 0; i < smp_num_cpus; ++i, cpumask >>= 1)
		mask |= (cpumask & 1) << get_hard_smp_processor_id(i);
	return mask;
}

void openpic_init_processor(u_int cpumask)
{
	openpic_write(&OpenPIC->Global.Processor_Initialization,
		      physmask(cpumask));
}

#ifdef CONFIG_SMP
/*
 *  Initialize an interprocessor interrupt (and disable it)
 *
 *  ipi: OpenPIC interprocessor interrupt number
 *  pri: interrupt source priority
 *  vec: the vector it will produce
 */
static void __init openpic_initipi(u_int ipi, u_int pri, u_int vec)
{
	check_arg_ipi(ipi);
	check_arg_pri(pri);
	check_arg_vec(vec);
	openpic_safe_writefield_IPI(&OpenPIC->Global.IPI_Vector_Priority(ipi),
				OPENPIC_PRIORITY_MASK | OPENPIC_VECTOR_MASK,
				(pri << OPENPIC_PRIORITY_SHIFT) | vec);
}

/*
 *  Send an IPI to one or more CPUs
 *  
 *  Externally called, however, it takes an IPI number (0...OPENPIC_NUM_IPI)
 *  and not a system-wide interrupt number
 */
void openpic_cause_IPI(u_int ipi, u_int cpumask)
{
	DECL_THIS_CPU;

	CHECK_THIS_CPU;
	check_arg_ipi(ipi);
	openpic_write(&OpenPIC->THIS_CPU.IPI_Dispatch(ipi),
		      physmask(cpumask));
}

void openpic_request_IPIs(void)
{
	int i;
	
	/*
	 * Make sure this matches what is defined in smp.c for 
	 * smp_message_{pass|recv}() or what shows up in 
	 * /proc/interrupts will be wrong!!! --Troy */
	
	if (OpenPIC == NULL)
		return;

	request_irq(openpic_vec_ipi,
		    openpic_ipi_action, 0, "IPI0 (call function)", 0);
	request_irq(openpic_vec_ipi+1,
		    openpic_ipi_action, 0, "IPI1 (reschedule)", 0);
	request_irq(openpic_vec_ipi+2,
		    openpic_ipi_action, 0, "IPI2 (invalidate tlb)", 0);
	request_irq(openpic_vec_ipi+3,
		    openpic_ipi_action, 0, "IPI3 (xmon break)", 0);

	for ( i = 0; i < OPENPIC_NUM_IPI ; i++ )
		openpic_enable_ipi(openpic_vec_ipi+i);
}

/*
 * Do per-cpu setup for SMP systems.
 *
 * Get IPI's working and start taking interrupts.
 *   -- Cort
 */
static spinlock_t openpic_setup_lock __initdata = SPIN_LOCK_UNLOCKED;

void __init do_openpic_setup_cpu(void)
{
#ifdef CONFIG_IRQ_ALL_CPUS
 	int i;
	u32 msk = 1 << hard_smp_processor_id();
#endif

	spin_lock(&openpic_setup_lock);

#ifdef CONFIG_IRQ_ALL_CPUS
 	/* let the openpic know we want intrs. default affinity
 	 * is 0xffffffff until changed via /proc
 	 * That's how it's done on x86. If we want it differently, then
 	 * we should make sure we also change the default values of irq_affinity
 	 * in irq.c.
 	 */
 	for (i = 0; i < NumSources ; i++)
		openpic_mapirq(i, openpic_read(&GET_ISU(i).Destination) | msk);
#endif /* CONFIG_IRQ_ALL_CPUS */
 	openpic_set_priority(0);

	spin_unlock(&openpic_setup_lock);
}
#endif /* CONFIG_SMP */

/*
 *  Initialize a timer interrupt (and disable it)
 *
 *  timer: OpenPIC timer number
 *  pri: interrupt source priority
 *  vec: the vector it will produce
 */
static void __init openpic_inittimer(u_int timer, u_int pri, u_int vec)
{
	check_arg_timer(timer);
	check_arg_pri(pri);
	check_arg_vec(vec);
	openpic_safe_writefield(&OpenPIC->Global.Timer[timer].Vector_Priority,
				OPENPIC_PRIORITY_MASK | OPENPIC_VECTOR_MASK,
				(pri << OPENPIC_PRIORITY_SHIFT) | vec);
}

/*
 *  Map a timer interrupt to one or more CPUs
 */
static void __init openpic_maptimer(u_int timer, u_int cpumask)
{
	check_arg_timer(timer);
	openpic_write(&OpenPIC->Global.Timer[timer].Destination,
		      physmask(cpumask));
}


/*
 *
 * All functions below take an offset'ed irq argument
 *
 */


/*
 *  Enable/disable an external interrupt source
 *
 *  Externally called, irq is an offseted system-wide interrupt number
 */
static void openpic_enable_irq(u_int irq)
{
	unsigned int loops = 100000;
	check_arg_irq(irq);

	openpic_clearfield(&GET_ISU(irq - open_pic_irq_offset).Vector_Priority, OPENPIC_MASK);
	/* make sure mask gets to controller before we return to user */
	do {
		if (!loops--) {
			printk(KERN_ERR "openpic_enable_irq timeout\n");
			break;
		}

		mb(); /* sync is probably useless here */
	} while(openpic_readfield(&GET_ISU(irq - open_pic_irq_offset).Vector_Priority,
			OPENPIC_MASK));
}

static void openpic_disable_irq(u_int irq)
{
	u32 vp;
	unsigned int loops = 100000;
	
	check_arg_irq(irq);

	openpic_setfield(&GET_ISU(irq - open_pic_irq_offset).Vector_Priority, OPENPIC_MASK);
	/* make sure mask gets to controller before we return to user */
	do {
		if (!loops--) {
			printk(KERN_ERR "openpic_disable_irq timeout\n");
			break;
		}

		mb();  /* sync is probably useless here */
		vp = openpic_readfield(&GET_ISU(irq - open_pic_irq_offset).Vector_Priority,
    			OPENPIC_MASK | OPENPIC_ACTIVITY);
	} while((vp & OPENPIC_ACTIVITY) && !(vp & OPENPIC_MASK));
}

#ifdef CONFIG_SMP
/*
 *  Enable/disable an IPI interrupt source
 *  
 *  Externally called, irq is an offseted system-wide interrupt number
 */
void openpic_enable_ipi(u_int irq)
{
	irq -= openpic_vec_ipi;
	check_arg_ipi(irq);
	openpic_clearfield_IPI(&OpenPIC->Global.IPI_Vector_Priority(irq), OPENPIC_MASK);

}
void openpic_disable_ipi(u_int irq)
{
   /* NEVER disable an IPI... that's just plain wrong! */
}

#endif

/*
 *  Initialize an interrupt source (and disable it!)
 *
 *  irq: OpenPIC interrupt number
 *  pri: interrupt source priority
 *  vec: the vector it will produce
 *  pol: polarity (1 for positive, 0 for negative)
 *  sense: 1 for level, 0 for edge
 */
static void openpic_initirq(u_int irq, u_int pri, u_int vec, int pol, int sense)
{
	openpic_safe_writefield(&GET_ISU(irq).Vector_Priority,
				OPENPIC_PRIORITY_MASK | OPENPIC_VECTOR_MASK |
				OPENPIC_SENSE_MASK | OPENPIC_POLARITY_MASK,
				(pri << OPENPIC_PRIORITY_SHIFT) | vec |
				(pol ? OPENPIC_POLARITY_POSITIVE :
			    		OPENPIC_POLARITY_NEGATIVE) |
				(sense ? OPENPIC_SENSE_LEVEL : OPENPIC_SENSE_EDGE));
}

/*
 *  Map an interrupt source to one or more CPUs
 */
static void openpic_mapirq(u_int irq, u_int physmask)
{
	openpic_write(&GET_ISU(irq).Destination, physmask);
}

/*
 *  Set the sense for an interrupt source (and disable it!)
 *
 *  sense: 1 for level, 0 for edge
 */
static inline void openpic_set_sense(u_int irq, int sense)
{
	openpic_safe_writefield(&GET_ISU(irq).Vector_Priority,
				OPENPIC_SENSE_LEVEL,
				(sense ? OPENPIC_SENSE_LEVEL : 0));
}

static void openpic_end_irq(unsigned int irq_nr)
{
	if ((irqdesc(irq_nr)->status & IRQ_LEVEL) != 0)
		openpic_eoi();
}

static void openpic_set_affinity(unsigned int irq_nr, unsigned long cpumask)
{
	openpic_mapirq(irq_nr - open_pic_irq_offset, physmask(cpumask));
}

#ifdef CONFIG_SMP
static void openpic_end_ipi(unsigned int irq_nr)
{
	/* IPIs are marked IRQ_PER_CPU. This has the side effect of
	 * preventing the IRQ_PENDING/IRQ_INPROGRESS logic from
	 * applying to them. We EOI them late to avoid re-entering.
	 * however, I'm wondering if we could simply let them have the
	 * SA_INTERRUPT flag and let them execute with all interrupts OFF.
	 * This would have the side effect of either running cross-CPU
	 * functions with interrupts off, or we can re-enable them explicitely
	 * with a __sti() in smp_call_function_interrupt(), since
	 * smp_call_function() is protected by a spinlock.
	 * Or maybe we shouldn't set the IRQ_PER_CPU flag on cross-CPU
	 * function calls IPI at all but that would make a special case.
	 */
	openpic_eoi();
}

static void openpic_ipi_action(int cpl, void *dev_id, struct pt_regs *regs)
{
	smp_message_recv(cpl-openpic_vec_ipi, regs);
}

#endif /* CONFIG_SMP */

int openpic_get_irq(struct pt_regs *regs)
{
	extern int i8259_irq(int cpu);

	int irq = openpic_irq();

	/* Management of the cascade should be moved out of here */
        if (open_pic_irq_offset && irq == open_pic_irq_offset)
        {
                /*
                 * This magic address generates a PCI IACK cycle.
                 */
		if ( chrp_int_ack_special )
			irq = *chrp_int_ack_special;
		else
			irq = i8259_irq( smp_processor_id() );
		openpic_eoi();
        }
	if (irq == openpic_vec_spurious)
		irq = -1;
	return irq;
}
