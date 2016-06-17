/*
 * IRQ vector handles
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995, 1996, 1997 by Ralf Baechle
 * Copyright (C) 2001 by Liam Davies (ldavies@agile.tv)
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>

#include <asm/bootinfo.h>
#include <asm/i8259.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mipsregs.h>
#include <asm/system.h>

#include <asm/cobalt/cobalt.h>

/* Cobalt Exception handler */
extern void cobalt_handle_int(void);

/* Via masking routines */
extern void unmask_irq(unsigned int irqr);
extern void mask_irq(unsigned int irq);


/*
 * We have two types of interrupts that we handle, ones that come
 *  in through the CPU interrupt lines, and ones that come in on
 *  the via chip. The CPU mappings are:
 *    0,1 - S/W (ignored)
 *    2   - Galileo chip (timer)
 *    3   - Tulip 0 + NCR SCSI
 *    4   - Tulip 1
 *    5   - 16550 UART
 *    6   - VIA southbridge PIC
 *    7   - unused
 *
 * The VIA chip is a master/slave 8259 setup and has the
 *  following interrupts
 *    8   - RTC
 *    9   - PCI
 *    14  - IDE0
 *    15  - IDE1
 *
 * In the table we use a 1 to indicate that we use a VIA interrupt
 *  line, and IE_IRQx to indicate that we use a CPU interrupt line
 *
 * We map all of these onto linux IRQ #s 0-15 and forget the rest
 */
#define NOINT_LINE	0
#define CPUINT_LINE(x)	IE_IRQ##x
#define VIAINT_LINE	1

#define COBALT_IRQS	16

static unsigned short irqnr_to_type[COBALT_IRQS] =
{ CPUINT_LINE(0),  NOINT_LINE,      VIAINT_LINE,  NOINT_LINE,
  CPUINT_LINE(1),  NOINT_LINE,      NOINT_LINE,   CPUINT_LINE(3),
  VIAINT_LINE,     VIAINT_LINE,     NOINT_LINE,   NOINT_LINE,
  NOINT_LINE,      CPUINT_LINE(2),  VIAINT_LINE,  VIAINT_LINE };

/*
 * Cobalt CPU irq
 */

static void enable_cpu_irq(unsigned int irq)
{
	unsigned long flags;

	save_and_cli(flags);
	change_c0_status(irqnr_to_type[irq], irqnr_to_type[irq]);
	restore_flags(flags);
}

static unsigned startup_cpu_irq(unsigned int irq)
{
	enable_cpu_irq(irq);

	return 0;
}

static void disable_cpu_irq(unsigned int irq)
{
	unsigned long flags;

	save_and_cli(flags);
	change_c0_status(irqnr_to_type[irq], ~(irqnr_to_type[irq]));
	restore_flags(flags);
}

#define shutdown_cpu_irq	disable_cpu_irq
#define mask_and_ack_cpu_irq	disable_cpu_irq

static void end_cpu_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		enable_cpu_irq(irq);
}

static struct hw_interrupt_type cobalt_cpu_irq_type = {
	"Cobalt CPU",
	startup_cpu_irq,
	shutdown_cpu_irq,
	enable_cpu_irq,
	disable_cpu_irq,
	mask_and_ack_cpu_irq,
	end_cpu_irq,
	NULL
};

void __init init_IRQ(void)
{
	int i;

	/* Initialise all of the IRQ descriptors */
	init_i8259_irqs();

	/* Map the irqnr to the type int we have */
	for (i=0; i < COBALT_IRQS; i++) {
		if (irqnr_to_type[i] >= CPUINT_LINE(0))
			/* cobalt_cpu_irq_type */
			irq_desc[i].handler = &cobalt_cpu_irq_type;
	}

	/* Mask all cpu interrupts
	    (except IE4, we already masked those at VIA level) */
	clear_c0_status(ST0_IM);
	set_c0_status(IE_IRQ4);

	cli();

	set_except_vector(0, cobalt_handle_int);
}
