/* 
 * Code to handle x86 style IRQs plus some generic interrupt stuff.
 *
 * Copyright (C) 1992 Linus Torvalds
 * Copyright (C) 1994, 1995, 1996, 1997, 1998 Ralf Baechle
 * Copyright (C) 1999 SuSE GmbH (Philipp Rumpf, prumpf@tux.org)
 * Copyright (C) 1999-2000 Grant Grundler
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/bitops.h>
#include <asm/bitops.h>
#include <linux/config.h>
#include <asm/pdc.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/signal.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/timex.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/irq.h>
#include <linux/spinlock.h>

#include <asm/cache.h>

#undef DEBUG_IRQ
#undef PARISC_IRQ_CR16_COUNTS

extern void timer_interrupt(int, void *, struct pt_regs *);
extern void ipi_interrupt(int, void *, struct pt_regs *);

#ifdef DEBUG_IRQ
#define DBG_IRQ(irq, x)	if ((irq) != TIMER_IRQ) printk x
#else /* DEBUG_IRQ */
#define DBG_IRQ(irq, x)	do { } while (0)
#endif /* DEBUG_IRQ */

#define EIEM_MASK(irq)       (1UL<<(MAX_CPU_IRQ-IRQ_OFFSET(irq)))

/* Bits in EIEM correlate with cpu_irq_action[].
** Numbered *Big Endian*! (ie bit 0 is MSB)
*/
static volatile unsigned long cpu_eiem = 0;

static spinlock_t irq_lock = SPIN_LOCK_UNLOCKED;  /* protect IRQ regions */

#ifdef CONFIG_SMP
static void cpu_set_eiem(void *info)
{
	set_eiem((unsigned long) info);
}
#endif

static inline void disable_cpu_irq(void *unused, int irq)
{
	unsigned long eirr_bit = EIEM_MASK(irq);

	cpu_eiem &= ~eirr_bit;
	set_eiem(cpu_eiem);
        smp_call_function(cpu_set_eiem, (void *) cpu_eiem, 1, 1);
}

static void enable_cpu_irq(void *unused, int irq)
{
	unsigned long eirr_bit = EIEM_MASK(irq);

	mtctl(eirr_bit, 23);	/* clear EIRR bit before unmasking */
	cpu_eiem |= eirr_bit;
        smp_call_function(cpu_set_eiem, (void *) cpu_eiem, 1, 1);
	set_eiem(cpu_eiem);
}

/* mask and disable are the same at the CPU level
** Difference is enable clears pending interrupts
*/
#define mask_cpu_irq	disable_cpu_irq

static inline void unmask_cpu_irq(void *unused, int irq)
{
	unsigned long eirr_bit = EIEM_MASK(irq);
	cpu_eiem |= eirr_bit;
	/* NOTE: sending an IPI will cause do_cpu_irq_mask() to
	** handle *any* unmasked pending interrupts.
	** ie We don't need to check for pending interrupts here.
	*/
        smp_call_function(cpu_set_eiem, (void *) cpu_eiem, 1, 1);
	set_eiem(cpu_eiem);
}

/*
 * XXX cpu_irq_actions[] will become 2 dimensional for per CPU EIR support.
 * correspond changes needed in:
 * 	processor_probe()	initialize additional action arrays
 * 	request_irq()		handle CPU IRQ region specially
 * 	do_cpu_irq_mask()	index into the matching irq_action array.
 */
struct irqaction cpu_irq_actions[IRQ_PER_REGION] = {
	[IRQ_OFFSET(TIMER_IRQ)] { handler: timer_interrupt, name: "timer", },
#ifdef CONFIG_SMP
	[IRQ_OFFSET(IPI_IRQ)]	{ handler: ipi_interrupt,   name: "IPI", },
#endif
};

struct irq_region_ops cpu_irq_ops = {
	disable_cpu_irq, enable_cpu_irq, unmask_cpu_irq, unmask_cpu_irq
};

struct irq_region cpu0_irq_region = {
	ops:	{ disable_cpu_irq, enable_cpu_irq, unmask_cpu_irq, unmask_cpu_irq },
	data:	{ dev: &cpu_data[0],
		  name: "PARISC-CPU",
		  irqbase: IRQ_FROM_REGION(CPU_IRQ_REGION), },
	action:	cpu_irq_actions,
};

struct irq_region *irq_region[NR_IRQ_REGS] = {
	[ 0 ]              NULL, /* reserved for EISA, else causes data page fault (aka code 15) */
	[ CPU_IRQ_REGION ] &cpu0_irq_region,
};


/*
** Generic interfaces that device drivers can use:
**    mask_irq()	block IRQ
**    unmask_irq()	re-enable IRQ and trigger if IRQ is pending
**    disable_irq()	block IRQ
**    enable_irq()	clear pending and re-enable IRQ
*/

void mask_irq(int irq)
{
	struct irq_region *region;

	DBG_IRQ(irq, ("mask_irq(%d) %d+%d eiem 0x%lx\n", irq,
				IRQ_REGION(irq), IRQ_OFFSET(irq), cpu_eiem));
	irq = irq_cannonicalize(irq);
	region = irq_region[IRQ_REGION(irq)];
	if (region->ops.mask_irq)
		region->ops.mask_irq(region->data.dev, IRQ_OFFSET(irq));
}

void unmask_irq(int irq)
{
	struct irq_region *region;

	DBG_IRQ(irq, ("unmask_irq(%d) %d+%d eiem 0x%lx\n", irq,
				IRQ_REGION(irq), IRQ_OFFSET(irq), cpu_eiem));
	irq = irq_cannonicalize(irq);
	region = irq_region[IRQ_REGION(irq)];
	if (region->ops.unmask_irq)
		region->ops.unmask_irq(region->data.dev, IRQ_OFFSET(irq));
}

void disable_irq(int irq)
{
	struct irq_region *region;

	DBG_IRQ(irq, ("disable_irq(%d) %d+%d eiem 0x%lx\n", irq,
				IRQ_REGION(irq), IRQ_OFFSET(irq), cpu_eiem));
	irq = irq_cannonicalize(irq);
	region = irq_region[IRQ_REGION(irq)];
	if (region->ops.disable_irq)
		region->ops.disable_irq(region->data.dev, IRQ_OFFSET(irq));
	else
		BUG();
}

void enable_irq(int irq)
{
	struct irq_region *region;

	DBG_IRQ(irq, ("enable_irq(%d) %d+%d eiem 0x%lx\n", irq,
				IRQ_REGION(irq), IRQ_OFFSET(irq), cpu_eiem));
	irq = irq_cannonicalize(irq);
	region = irq_region[IRQ_REGION(irq)];

	if (region->ops.enable_irq)
		region->ops.enable_irq(region->data.dev, IRQ_OFFSET(irq));
	else
		BUG();
}

int get_irq_list(char *buf)
{
#ifdef CONFIG_PROC_FS
	char *p = buf;
	unsigned int regnr = 0;

	p += sprintf(p, "     ");
#ifdef CONFIG_SMP
	for (regnr = 0; regnr < smp_num_cpus; regnr++)
#endif
		p += sprintf(p, "     CPU%02d ", regnr);


#ifdef PARISC_IRQ_CR16_COUNTS
	p += sprintf(p, "[min/avg/max] (CPU cycle counts)");
#endif
	*p++ = '\n';

	/* We don't need *irqsave lock variants since this is
	** only allowed to change while in the base context.
	*/
	spin_lock(&irq_lock);
	for (regnr = 0; regnr < NR_IRQ_REGS; regnr++) {
	    unsigned int i;
	    struct irq_region *region = irq_region[regnr];
#ifdef CONFIG_SMP
	    unsigned int j;
#endif

            if (!region || !region->action)
		continue;

	    for (i = 0; i <= MAX_CPU_IRQ; i++) {
		struct irqaction *action = &region->action[i];
		unsigned int irq_no = IRQ_FROM_REGION(regnr) + i;

		if (!action->handler)
			continue;

		p += sprintf(p, "%3d: ", irq_no);
#ifndef CONFIG_SMP
		p += sprintf(p, "%10u ", kstat_irqs(irq_no));
#else
		for (j = 0; j < smp_num_cpus; j++)
			p += sprintf(p, "%10u ",
				kstat.irqs[j][regnr][i]);
#endif
		p += sprintf(p, " %14s",
			    region->data.name ? region->data.name : "N/A");

#ifndef PARISC_IRQ_CR16_COUNTS
		p += sprintf(p, "  %s", action->name);

		while ((action = action->next))
			p += sprintf(p, ", %s", action->name);
#else
		for ( ;action; action = action->next) {
			unsigned int i, avg, min, max;

			min = max = action->cr16_hist[0];

			for (avg = i = 0; i < PARISC_CR16_HIST_SIZE; i++) {
				int hist = action->cr16_hist[i];

				if (hist) {
					avg += hist;
				} else
					break;

				if (hist > max) max = hist;
				if (hist < min) min = hist;
			}

			avg /= i;
			p += sprintf(p, " %s[%d/%d/%d]", action->name,
					min,avg,max);
		}
#endif

		*p++ = '\n';
	    }
	}
	spin_unlock(&irq_lock);

	p += sprintf(p, "\n");
	return p - buf;

#else	/* CONFIG_PROC_FS */

	return 0;

#endif	/* CONFIG_PROC_FS */
}



/*
** The following form a "set": Virtual IRQ, Transaction Address, Trans Data.
** Respectively, these map to IRQ region+EIRR, Processor HPA, EIRR bit.
**
** To use txn_XXX() interfaces, get a Virtual IRQ first.
** Then use that to get the Transaction address and data.
*/

int
txn_alloc_irq(void)
{
	int irq;

	/* never return irq 0 cause that's the interval timer */
	for (irq = 1; irq <= MAX_CPU_IRQ; irq++) {
		if (cpu_irq_actions[irq].handler == NULL) {
			return (IRQ_FROM_REGION(CPU_IRQ_REGION) + irq);
		}
	}

	/* unlikely, but be prepared */
	return -1;
}

int
txn_claim_irq(int irq)
{
	if (irq_region[IRQ_REGION(irq)]->action[IRQ_OFFSET(irq)].handler ==NULL)
		return irq;

	/* unlikely, but be prepared */
	return -1;
}

unsigned long
txn_alloc_addr(int virt_irq)
{
	static int next_cpu = -1;

	next_cpu++; /* assign to "next" CPU we want this bugger on */

	/* validate entry */
	while ((next_cpu < NR_CPUS) && !cpu_data[next_cpu].txn_addr)
		next_cpu++;

	if (next_cpu >= NR_CPUS) 
		next_cpu = 0;	/* nothing else, assign monarch */

	return cpu_data[next_cpu].txn_addr;
}


/*
** The alloc process needs to accept a parameter to accomodate limitations
** of the HW/SW which use these bits:
** Legacy PA I/O (GSC/NIO): 5 bits (architected EIM register)
** V-class (EPIC):          6 bits
** N/L-class/A500:          8 bits (iosapic)
** PCI 2.2 MSI:             16 bits (I think)
** Existing PCI devices:    32-bits (all Symbios SCSI/ATM/HyperFabric)
**
** On the service provider side:
** o PA 1.1 (and PA2.0 narrow mode)     5-bits (width of EIR register)
** o PA 2.0 wide mode                   6-bits (per processor)
** o IA64                               8-bits (0-256 total)
**
** So a Legacy PA I/O device on a PA 2.0 box can't use all
** the bits supported by the processor...and the N/L-class
** I/O subsystem supports more bits than PA2.0 has. The first
** case is the problem.
*/
unsigned int
txn_alloc_data(int virt_irq, unsigned int bits_wide)
{
	/* XXX FIXME : bits_wide indicates how wide the transaction
	** data is allowed to be...we may need a different virt_irq
	** if this one won't work. Another reason to index virtual
	** irq's into a table which can manage CPU/IRQ bit seperately.
	*/
	if (IRQ_OFFSET(virt_irq) > (1 << (bits_wide -1)))
	{
		panic("Sorry -- didn't allocate valid IRQ for this device\n");
	}

	return (IRQ_OFFSET(virt_irq));
}

void do_irq(struct irqaction *action, int irq, struct pt_regs * regs)
{
	int cpu = smp_processor_id();

	irq_enter(cpu, irq);
	++kstat.irqs[cpu][IRQ_REGION(irq)][IRQ_OFFSET(irq)];

	DBG_IRQ(irq, ("do_irq(%d) %d+%d\n", irq, IRQ_REGION(irq), IRQ_OFFSET(irq)));

	for (; action; action = action->next) {
#ifdef PARISC_IRQ_CR16_COUNTS
		unsigned long cr_start = mfctl(16);
#endif

		if (action->handler == NULL) {
			if (IRQ_REGION(irq) == EISA_IRQ_REGION && irq_region[EISA_IRQ_REGION]) {
				/* were we called due to autodetecting (E)ISA irqs ? */
				unsigned int *status;
				status = &irq_region[EISA_IRQ_REGION]->data.status[IRQ_OFFSET(irq)];
				if (*status & IRQ_AUTODETECT) {
					*status &= ~IRQ_WAITING;
					continue; 
				}
			}
			printk(KERN_ERR "IRQ:  CPU:%d No handler for IRQ %d !\n", cpu, irq);
			continue;
		}

		action->handler(irq, action->dev_id, regs);

#ifdef PARISC_IRQ_CR16_COUNTS
		{
			unsigned long cr_end = mfctl(16);
			unsigned long tmp = cr_end - cr_start;
			/* check for roll over */
			cr_start = (cr_end < cr_start) ?  -(tmp) : (tmp);
		}
		action->cr16_hist[action->cr16_idx++] = (int) cr_start;
		action->cr16_idx &= PARISC_CR16_HIST_SIZE - 1;
#endif
	}

	irq_exit(cpu, irq);
}


/* ONLY called from entry.S:intr_extint() */
void do_cpu_irq_mask(struct pt_regs *regs)
{
	unsigned long eirr_val;
	unsigned int i=3;	/* limit time in interrupt context */

	/*
	 * PSW_I or EIEM bits cannot be enabled until after the
	 * interrupts are processed.
	 * timer_interrupt() assumes it won't get interrupted when it
	 * holds the xtime_lock...an unmasked interrupt source could
	 * interrupt and deadlock by trying to grab xtime_lock too.
	 * Keeping PSW_I and EIEM disabled avoids this.
	 */
	set_eiem(0UL);	/* disable all extr interrupt for now */

	/* 1) only process IRQs that are enabled/unmasked (cpu_eiem)
	 * 2) We loop here on EIRR contents in order to avoid
	 *    nested interrupts or having to take another interupt
	 *    when we could have just handled it right away.
	 * 3) Limit the number of times we loop to make sure other
	 *    processing can occur.
	 */
	while ((eirr_val = (mfctl(23) & cpu_eiem)) && --i) {
		unsigned long bit = (1UL<<MAX_CPU_IRQ);
		unsigned int irq;

		mtctl(eirr_val, 23); /* reset bits we are going to process */

#ifdef DEBUG_IRQ
		if (eirr_val != (1UL << MAX_CPU_IRQ))
			printk(KERN_DEBUG "do_cpu_irq_mask  %x\n", eirr_val);
#endif

		for (irq = 0; eirr_val && bit; bit>>=1, irq++)
		{
			if (!(bit&eirr_val&cpu_eiem))
				continue;

			/* clear bit in mask - can exit loop sooner */
			eirr_val &= ~bit;

			do_irq(&cpu_irq_actions[irq], TIMER_IRQ+irq, regs);
		}
	}
	set_eiem(cpu_eiem);
}


/* Called from second level IRQ regions: eg dino or iosapic. */
void do_irq_mask(unsigned long mask, struct irq_region *region, struct pt_regs *regs)
{
	unsigned long bit;
	unsigned int irq;

#ifdef DEBUG_IRQ
	if (mask != (1L<<MAX_CPU_IRQ))
	    printk(KERN_DEBUG "do_irq_mask %08lx %p %p\n", mask, region, regs);
#endif

	for (bit = (1L<<MAX_CPU_IRQ), irq = 0; mask && bit; bit>>=1, irq++) {
		unsigned int irq_num;
		if (!(bit&mask))
			continue;

		mask &= ~bit;	/* clear bit in mask - can exit loop sooner */
		irq_num = region->data.irqbase + irq;

		mask_irq(irq_num);
		do_irq(&region->action[irq], irq_num, regs);
		unmask_irq(irq_num);
	}
}


static inline int find_free_region(void)
{
	int irqreg;

	for (irqreg=1; irqreg <= (NR_IRQ_REGS); irqreg++) {
		if (irq_region[irqreg] == NULL)
			return irqreg;
	}

	return 0;
}


/*****
 * alloc_irq_region - allocate/init a new IRQ region
 * @count: number of IRQs in this region.
 * @ops: function table with request/release/mask/unmask/etc.. entries.
 * @name: name of region owner for /proc/interrupts output.
 * @dev: private data to associate with the new IRQ region.
 *
 * Every IRQ must become a MMIO write to the CPU's EIRR in
 * order to get CPU service. The IRQ region represents the
 * number of unique events the region handler can (or must)
 * identify. For PARISC CPU, that's the width of the EIR Register.
 * IRQ regions virtualize IRQs (eg EISA or PCI host bus controllers)
 * for line based devices.
 */
struct irq_region *alloc_irq_region( int count, struct irq_region_ops *ops,
					const char *name, void *dev)
{
	struct irq_region *region;
	int index;

	index = find_free_region();
	if (index == 0) {
		printk(KERN_ERR "Maximum number of irq regions exceeded. Increase NR_IRQ_REGS!\n");
		return NULL;
	}

	if ((IRQ_REGION(count-1)))
		return NULL;

	if (count < IRQ_PER_REGION) {
	    DBG_IRQ(0, ("alloc_irq_region() using minimum of %d irq lines for %s (%d)\n",
			IRQ_PER_REGION, name, count));
	    count = IRQ_PER_REGION;
	}

	/* if either mask *or* unmask is set, both have to be set. */
	if((ops->mask_irq || ops->unmask_irq) &&
		!(ops->mask_irq && ops->unmask_irq))
			return NULL;

	/* ditto for enable/disable */
	if( (ops->disable_irq || ops->enable_irq) &&
		!(ops->disable_irq && ops->enable_irq) )
			return NULL;

	region = kmalloc(sizeof(*region), GFP_ATOMIC);
	if (!region)
		return NULL;
	memset(region, 0, sizeof(*region));

	region->action = kmalloc(count * sizeof(*region->action), GFP_ATOMIC);
	if (!region->action) {
		kfree(region);
		return NULL;
	}
	memset(region->action, 0, count * sizeof(*region->action));

	region->ops = *ops;
	region->data.irqbase = IRQ_FROM_REGION(index);
	region->data.name = name;
	region->data.dev = dev;

	irq_region[index] = region;

	return irq_region[index];
}

/* FIXME: SMP, flags, bottom halves, rest */

int request_irq(unsigned int irq,
		void (*handler)(int, void *, struct pt_regs *),
		unsigned long irqflags,
		const char * devname,
		void *dev_id)
{
	struct irqaction * action;

#if 0
	printk(KERN_INFO "request_irq(%d, %p, 0x%lx, %s, %p)\n",irq, handler, irqflags, devname, dev_id);
#endif

	irq = irq_cannonicalize(irq);
	/* request_irq()/free_irq() may not be called from interrupt context. */
	if (in_interrupt())
		BUG();

	if (!handler) {
		printk(KERN_ERR "request_irq(%d,...): Augh! No handler for irq!\n",
			irq);
		return -EINVAL;
	}

	if (irq_region[IRQ_REGION(irq)] == NULL) {
		/*
		** Bug catcher for drivers which use "char" or u8 for
		** the IRQ number. They lose the region number which
		** is in pcidev->irq (an int).
		*/
		printk(KERN_ERR "%p (%s?) called request_irq with an invalid irq %d\n",
			__builtin_return_address(0), devname, irq);
		return -EINVAL;
	}

	spin_lock(&irq_lock);
	action = &(irq_region[IRQ_REGION(irq)]->action[IRQ_OFFSET(irq)]);

	/* First one is preallocated. */
	if (action->handler) {
		/* But it's in use...find the tail and allocate a new one */
		while (action->next)
			action = action->next;

		action->next = kmalloc(sizeof(*action), GFP_ATOMIC);
		memset(action->next, 0, sizeof(*action));

		action = action->next;
	}

	if (!action) {
		spin_unlock(&irq_lock);
		printk(KERN_ERR "request_irq(): Augh! No action!\n") ;
		return -ENOMEM;
	}

	action->handler = handler;
	action->flags = irqflags;
	action->mask = 0;
	action->name = devname;
	action->next = NULL;
	action->dev_id = dev_id;
	spin_unlock(&irq_lock);

	enable_irq(irq);
	return 0;
}

void free_irq(unsigned int irq, void *dev_id)
{
	struct irqaction *action, **p;

	/* See comments in request_irq() about interrupt context */
	irq = irq_cannonicalize(irq);
	
	if (in_interrupt()) BUG();

	spin_lock(&irq_lock);
	action = &irq_region[IRQ_REGION(irq)]->action[IRQ_OFFSET(irq)];

	if (action->dev_id == dev_id) {
		if (action->next == NULL) {
			action->handler = NULL;
		} else {
			memcpy(action, action->next, sizeof(*action));
		}

		spin_unlock(&irq_lock);
		return;
	}

	p = &action->next;
	action = action->next;

	for (; (action = *p) != NULL; p = &action->next) {
		if (action->dev_id != dev_id)
			continue;

		/* Found it - now free it */
		*p = action->next;
		kfree(action);

		spin_unlock(&irq_lock);
		return;
	}

	spin_unlock(&irq_lock);
	printk(KERN_ERR "Trying to free free IRQ%d\n",irq);
}


/*
 * IRQ autodetection code..
 *
 * This depends on the fact that any interrupt that
 * comes in on to an unassigned handler will get stuck
 * with "IRQ_WAITING" cleared and the interrupt
 * disabled.
 */

static DECLARE_MUTEX(probe_sem);

/**
 *	probe_irq_on	- begin an interrupt autodetect
 *
 *	Commence probing for an interrupt. The interrupts are scanned
 *	and a mask of potential interrupt lines is returned.
 *
 */

/* TODO: spin_lock_irq(desc->lock -> irq_lock) */

unsigned long probe_irq_on(void)
{
	unsigned int i;
	unsigned long val;
	unsigned long delay;
	struct irq_region *region;

	/* support for irq autoprobing is limited to EISA (irq region 0) */
	region = irq_region[EISA_IRQ_REGION];
	if (!EISA_bus || !region)
		return 0;

	down(&probe_sem);

	/*
	 * enable any unassigned irqs
	 * (we must startup again here because if a longstanding irq
	 * happened in the previous stage, it may have masked itself)
	 */
	for (i = EISA_MAX_IRQS-1; i > 0; i--) {
		struct irqaction *action;
		
		spin_lock_irq(&irq_lock);
		action = region->action + i;
		if (!action->handler) {
			region->data.status[i] |= IRQ_AUTODETECT | IRQ_WAITING;
			region->ops.enable_irq(region->data.dev,i);
		}
		spin_unlock_irq(&irq_lock);
	}

	/*
	 * Wait for spurious interrupts to trigger
	 */
	for (delay = jiffies + HZ/10; time_after(delay, jiffies); )
		/* about 100ms delay */ synchronize_irq();

	/*
	 * Now filter out any obviously spurious interrupts
	 */
	val = 0;
	for (i = 0; i < EISA_MAX_IRQS; i++) {
		unsigned int status;

		spin_lock_irq(&irq_lock);
		status = region->data.status[i];

		if (status & IRQ_AUTODETECT) {
			/* It triggered already - consider it spurious. */
			if (!(status & IRQ_WAITING)) {
				region->data.status[i] = status & ~IRQ_AUTODETECT;
				region->ops.disable_irq(region->data.dev,i);
			} else
				if (i < BITS_PER_LONG)
					val |= (1 << i);
		}
		spin_unlock_irq(&irq_lock);
	}

	return val;
}

/*
 * Return the one interrupt that triggered (this can
 * handle any interrupt source).
 */

/**
 *	probe_irq_off	- end an interrupt autodetect
 *	@val: mask of potential interrupts (unused)
 *
 *	Scans the unused interrupt lines and returns the line which
 *	appears to have triggered the interrupt. If no interrupt was
 *	found then zero is returned. If more than one interrupt is
 *	found then minus the first candidate is returned to indicate
 *	their is doubt.
 *
 *	The interrupt probe logic state is returned to its previous
 *	value.
 *
 *	BUGS: When used in a module (which arguably shouldnt happen)
 *	nothing prevents two IRQ probe callers from overlapping. The
 *	results of this are non-optimal.
 */
 
int probe_irq_off(unsigned long val)
{
        struct irq_region *region;
	int i, irq_found, nr_irqs;

        /* support for irq autoprobing is limited to EISA (irq region 0) */
        region = irq_region[EISA_IRQ_REGION];
        if (!EISA_bus || !region)
		return 0;

	nr_irqs = 0;
	irq_found = 0;
	for (i = 0; i < EISA_MAX_IRQS; i++) {
		unsigned int status;
		
		spin_lock_irq(&irq_lock);
		status = region->data.status[i];

                if (status & IRQ_AUTODETECT) {
			if (!(status & IRQ_WAITING)) {
				if (!nr_irqs)
					irq_found = i;
				nr_irqs++;
			}
			region->ops.disable_irq(region->data.dev,i);
			region->data.status[i] = status & ~IRQ_AUTODETECT;
		}
		spin_unlock_irq(&irq_lock);
	}
	up(&probe_sem);

	if (nr_irqs > 1)
		irq_found = -irq_found;
	return irq_found;
}


void __init init_IRQ(void)
{
	local_irq_disable();	/* PARANOID - should already be disabled */
	mtctl(-1L, 23);		/* EIRR : clear all pending external intr */
#ifdef CONFIG_SMP
	if (!cpu_eiem)
		cpu_eiem = EIEM_MASK(IPI_IRQ) | EIEM_MASK(TIMER_IRQ);
#else
	cpu_eiem = EIEM_MASK(TIMER_IRQ);
#endif
        set_eiem(cpu_eiem);	/* EIEM : enable all external intr */

}

#ifdef CONFIG_PROC_FS
/* called from kernel/sysctl.c:sysctl_init() */
void __init init_irq_proc(void)
{
}
#endif
