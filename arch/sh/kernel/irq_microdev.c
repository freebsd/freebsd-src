/* 
 * linux/arch/sh/kernel/irq_microdev.c
 *
 * Copyright (C) 2003 Sean McGoogan (Sean.McGoogan@superh.com)
 *
 * SuperH SH4-202 MicroDev board support.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/irq.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq_microdev.h>

#define NUM_EXTERNAL_IRQS 16	/* IRL0 .. IRL15 */


static const struct 
{
	unsigned char fpgaIrq;
	unsigned char mapped;
	const char *name;
}	fpgaIrqTable[NUM_EXTERNAL_IRQS] = 
{
	{ 0,	0,	"unused" },		/* IRQ #0	IRL=15	0x200  */
	{ 0,	0,	"unused" },		/* IRQ #1	IRL=14	0x220  */
	{ 0,	0,	"unused" },		/* IRQ #2	IRL=13	0x240  */
	{ 18,	1,	"Ethernet" },		/* IRQ #3	IRL=12	0x260  */
	{ 0,	0,	"unused" },		/* IRQ #4	IRL=11	0x280  */
	{ 0,	0,	"unused" },		/* IRQ #5	IRL=10	0x2a0  */
	{ 0,	0,	"unused" },		/* IRQ #6	IRL=9	0x2c0  */
	{ 0,	0,	"unused" },		/* IRQ #7	IRL=8	0x2e0  */
	{ 8,	1,	"PCI INTA" },		/* IRQ #8	IRL=7	0x300  */
	{ 9,	1,	"PCI INTB" },		/* IRQ #9	IRL=6	0x320  */
	{ 10,	1,	"PCI INTC" },		/* IRQ #10	IRL=5	0x340  */
	{ 11,	1,	"PCI INTD" },		/* IRQ #11	IRL=4	0x360  */
	{ 0,	0,	"unused" },		/* IRQ #12	IRL=3	0x380  */
	{ 0,	0,	"unused" },		/* IRQ #13	IRL=2	0x3a0  */
	{ 0,	0,	"unused" },		/* IRQ #14	IRL=1	0x3c0  */
	{ 0,	0,	"unused" },		/* IRQ #15	IRL=0	0x3e0  */
};

static void enable_microdev_irq(unsigned int irq);
static void disable_microdev_irq(unsigned int irq);

	/* shutdown is same as "disable" */
#define shutdown_microdev_irq disable_microdev_irq

static void mask_and_ack_microdev(unsigned int);
static void end_microdev_irq(unsigned int irq);

static unsigned int startup_microdev_irq(unsigned int irq)
{
	enable_microdev_irq(irq);
	return 0;		/* never anything pending */
}

static struct hw_interrupt_type microdev_irq_type = {
	"MicroDev-IRQ",
	startup_microdev_irq,
	shutdown_microdev_irq,
	enable_microdev_irq,
	disable_microdev_irq,
	mask_and_ack_microdev,
	end_microdev_irq
};

static void disable_microdev_irq(unsigned int irq)
{
	unsigned int flags; 
	unsigned int fpgaIrq; 

	if (irq >= NUM_EXTERNAL_IRQS) return;
	if (!fpgaIrqTable[irq].mapped) return;

	fpgaIrq = fpgaIrqTable[irq].fpgaIrq;

		/* disable interrupts */
	save_and_cli(flags);

		/* disable interupts on the FPGA INTC register */
	ctrl_outl(MICRODEV_FPGA_INTC_MASK(fpgaIrq), MICRODEV_FPGA_INTDSB_REG);

		/* restore interrupts */
	restore_flags(flags);
}

static void enable_microdev_irq(unsigned int irq)
{
	unsigned long priorityReg, priorities, pri;
	unsigned int flags; 
	unsigned int fpgaIrq; 


	if (irq >= NUM_EXTERNAL_IRQS) return;
	if (!fpgaIrqTable[irq].mapped) return;

	pri = 15 - irq;

	fpgaIrq = fpgaIrqTable[irq].fpgaIrq;
	priorityReg = MICRODEV_FPGA_INTPRI_REG(fpgaIrq);

		/* disable interrupts */
	save_and_cli(flags);

		/* set priority for the interrupt */
	priorities = ctrl_inl(priorityReg);
	priorities &= ~MICRODEV_FPGA_INTPRI_MASK(fpgaIrq);
	priorities |= MICRODEV_FPGA_INTPRI_LEVEL(fpgaIrq, pri);
	ctrl_outl(priorities, priorityReg);
	
		/* enable interupts on the FPGA INTC register */
	ctrl_outl(MICRODEV_FPGA_INTC_MASK(fpgaIrq), MICRODEV_FPGA_INTENB_REG);

		/* restore interrupts */
	restore_flags(flags);
}

	/* This functions sets the desired irq handler to be a MicroDev type */
static void __init make_microdev_irq(unsigned int irq)
{
	disable_irq_nosync(irq);
	irq_desc[irq].handler = &microdev_irq_type;
	disable_microdev_irq(irq);
}

static void mask_and_ack_microdev(unsigned int irq)
{
	disable_microdev_irq(irq);
}

static void end_microdev_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED|IRQ_INPROGRESS)))
	{
		enable_microdev_irq(irq);
	}
}

extern void __init init_microdev_irq(void)
{
	int i;

		/* disable interupts on the FPGA INTC register */
	ctrl_outl(~0ul, MICRODEV_FPGA_INTDSB_REG);

	for (i = 0; i < NUM_EXTERNAL_IRQS; i++)
	{
		make_microdev_irq(i);
	}
}


