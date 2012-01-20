/*-
 * Copyright (c) 2011
 *	Ben Gray <ben.r.gray@gmail.com>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BEN GRAY ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BEN GRAY BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <machine/intr.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <arm/omap/omapvar.h>
#include <arm/omap/omap4/omap4var.h>
#include <arm/omap/omap4/omap44xx_reg.h>

#include "omap4_if.h"

/*
 * There are a number of ways that interrupt handling is implemented in
 * the various ARM platforms, the PXA has the neatest way, it creates another
 * device driver that handles everything. However IMO this is rather heavy-
 * weight for playing with IRQs which should be quite fast ... so I've
 * gone for something similar to the IXP425, which just directly plays with
 * registers. This assumes that the interrupt control registers are already
 * mapped in virtual memory at a fixed virtual address ... simplies.
 * 
 * The OMAP4xxx processors use the ARM Generic Interrupt Controller (GIC), so
 * this file/driver isn't really specific to the OMAP4 series of processors it
 * is potentially much broader.  However I haven't used any other devices with
 * a GIC ... I guess if other platforms would like to use this driver it could
 * be made more generic (as the name implies ;-)
 *
 *
 */

/* GIC Distributor register map */
#define ARM_GIC_ICDDCR          0x0000
#define ARM_GIC_ICDICTR         0x0004
#define ARM_GIC_ICDIIDR         0x0008
#define ARM_GIC_ICDISR          0x0080
#define ARM_GIC_ICDISER(n)      (0x0100 + ((n) * 4))
#define ARM_GIC_ICDICER(n)      (0x0180 + ((n) * 4))
#define ARM_GIC_ICDISPR(n)      (0x0200 + ((n) * 4))
#define ARM_GIC_ICDICPR(n)      (0x0280 + ((n) * 4))
#define ARM_GIC_ICDABR(n)       (0x0300 + ((n) * 4))
#define ARM_GIC_ICDIPR(n)       (0x0400 + ((n) * 4))
#define ARM_GIC_ICDIPTR(n)      (0x0800 + ((n) * 4))
#define ARM_GIC_ICDICFR(n)      (0x0C00 + ((n) * 4))
#define ARM_GIC_ICDSGIR         0x0F00

/* GIC CPU/Processor register map */
#define ARM_GIC_ICCICR          0x0000
#define ARM_GIC_ICCPMR          0x0004
#define ARM_GIC_ICCBPR          0x0008
#define ARM_GIC_ICCIAR          0x000C
#define ARM_GIC_ICCEOIR         0x0010
#define ARM_GIC_ICCRPR          0x0014
#define ARM_GIC_ICCHPIR         0x0018
#define ARM_GIC_ICCABPR         0x001C
#define ARM_GIC_ICCIIDR         0x00FC

static device_t omap4_dev;

/**
 *	omap4_mask_all_intr - masks all interrupts
 *
 *	Called during initialisation to ensure all interrupts are masked before
 *	globally enabling interrupts. Should only be used at startup time.
 *
 *	RETURNS:
 *	nothing
 */
void
omap4_mask_all_intr(void)
{
	unsigned int i = 0;

	OMAP4_GIC_DIST_WRITE(omap4_dev, ARM_GIC_ICDICER(i), 0xFFFFFFFF);
}

/**
 *	omap4_setup_gic_distributor - configures and enables the distributor part
 *	of the generic interrupt controller (GIC).
 *
 *	Enables and configures the distributor part of the GIC by programming the
 *	interrupt priorities and target CPU's.  It also ensures the enable bit is
 *	set at the end of the process.
 *
 *	RETURNS:
 *	nothing
 */
int
omap4_setup_intr_controller(device_t dev,
    const struct omap4_intr_conf *irqs)
{
	u_int oldirqstate;
	uint32_t reg_off, bit_off;
	uint32_t icdicpr, icdiptr, icdictr;
	uint32_t nirqs, i;

	omap4_dev = dev;
	
	/* Disable interrupts while configuring the controller */
	oldirqstate = disable_interrupts(I32_bit);

	/* Disable interrupt distribution while we are doing the work */
	OMAP4_GIC_DIST_WRITE(omap4_dev, ARM_GIC_ICDDCR, 0x00);

	/* Get the number of interrupts */
	icdictr = OMAP4_GIC_DIST_READ(omap4_dev, ARM_GIC_ICDICTR);
	nirqs = 32 * ((icdictr & 0x1f) + 1);
	 

	/* Set all global interrupts to be level triggered, active low. */
	for (i = 32; i < nirqs; i += 32) {
		OMAP4_GIC_DIST_WRITE(omap4_dev,
		    ARM_GIC_ICDICFR(i >> 5), 0x00000000);
	}

	/* Disable all interrupts. */
	for (i = 32; i < nirqs; i += 32) {
		OMAP4_GIC_DIST_WRITE(omap4_dev,
		    ARM_GIC_ICDICER(i >> 5), 0xffffffff);
	}

	/* Program the interrupt priorites and target CPU(s) */
	for (; irqs->num != -1; irqs++) {
		reg_off = (irqs->num >> 2);
		bit_off = (irqs->num & 0x3) << 3;
	
		/* Read the register, mask out the priority bits and then or in */
		icdicpr = OMAP4_GIC_DIST_READ(omap4_dev,
		                          ARM_GIC_ICDICPR(reg_off));

		icdicpr &= ~(0xFFUL << bit_off);
		icdicpr |= ((irqs->priority & 0xFF) << bit_off);

		OMAP4_GIC_DIST_WRITE(omap4_dev,
		    ARM_GIC_ICDICPR(reg_off), icdicpr);

		/* Read the register, mask out the target cpu bits and then or in */
		icdiptr = OMAP4_GIC_DIST_READ(omap4_dev,
		    ARM_GIC_ICDIPTR(reg_off));

		icdiptr &= ~(0xFFUL << bit_off);
		icdiptr |= ((irqs->target_cpu & 0xFF) << bit_off);

		OMAP4_GIC_DIST_WRITE(omap4_dev,
		    ARM_GIC_ICDIPTR(reg_off), icdiptr);
	}
	
	/* Enable interrupt distribution */
	OMAP4_GIC_DIST_WRITE(omap4_dev, ARM_GIC_ICDDCR, 0x01);

	/* Re-enable interrupts */
	restore_interrupts(oldirqstate);

	return (0);
}

/**
 *	omap4_setup_gic_cpu - configures and enables the per-CPU part of the generic
 *	interrupt controller (GIC).
 *
 *	Enables and configures the per-CPU part of the GIC by enabling interrupt
 *	routing and setting the default interrupt priority filter.
 *
 *	RETURNS:
 *	nothing
 */
int
omap4_setup_gic_cpu(unsigned int prio_mask)
{
	uint32_t dcr;
	uint32_t ictr;
	uint32_t iidr;

	if (omap4_dev == NULL)
		panic("omap4 devices is not set before calling omap4_setup_gic_cpu");

	dcr = OMAP4_GIC_CPU_READ(omap4_dev, ARM_GIC_ICCICR);
	ictr = OMAP4_GIC_CPU_READ(omap4_dev, ARM_GIC_ICCPMR);
	iidr = OMAP4_GIC_CPU_READ(omap4_dev, ARM_GIC_ICCBPR);

	/* Disable interrupt to this CPU while we are doing the work */
	OMAP4_GIC_DIST_WRITE(omap4_dev, ARM_GIC_ICCICR, 0x00);

	/* Enable interrupt to this CPU */
	OMAP4_GIC_DIST_WRITE(omap4_dev, ARM_GIC_ICCICR, 0x01);

	return (0);
}

/**
 *	omap3_post_filter_intr - called after the IRQ has been filtered
 *	@arg: the IRQ number
 *
 *	Called after the interrupt handler has done it's stuff, can be used to
 *	clean up interrupts that haven't been handled properly.
 *
 *
 *	RETURNS:
 *	nothing
 */
void
omap4_post_filter_intr(void *arg)
{
	/* uintptr_t irq = (uintptr_t) arg; */
	
	/* data synchronization barrier */
	cpu_drain_writebuf();
}

/**
 *	arm_mask_irq - masks an IRQ (disables it)
 *	@nb: the number of the IRQ to mask (disable)
 *
 *	Disables the interrupt at the HW level.
 *
 *
 *	RETURNS:
 *	nothing
 */
void
arm_mask_irq(uintptr_t nb)
{
	/* Write the bit corresponding to the IRQ into the "Clear-Enable Register" */
	OMAP4_GIC_DIST_WRITE(omap4_dev, ARM_GIC_ICDICER(nb >> 5),
	    (1UL << (nb & 0x1F)));
}

/**
 *	arm_unmask_irq - unmasks an IRQ (enables it)
 *	@nb: the number of the IRQ to unmask (enable)
 *
 *	Enables the interrupt at the HW level.
 *
 *
 *	RETURNS:
 *	nothing
 */
void
arm_unmask_irq(uintptr_t nb)
{
	/* Write the bit corresponding to the IRQ into the "Set-Enable Register" */
	OMAP4_GIC_DIST_WRITE(omap4_dev, ARM_GIC_ICDISER(nb >> 5),
	    (1UL << (nb & 0x1F)));
}

/**
 *	arm_get_next_irq - gets the next tripped interrupt
 *	@last_irq: the number of the last IRQ processed
 *
 *	This function gets called when an interrupt is tripped, it is initially
 *	passed -1 in the last_irq argument and is suppose to return the IRQ number.
 *	The following is psuedo-code for how this is implemented in the core:
 *
 *		irq_handler() 
 *		{
 *			i = -1;
 *			while((i = arm_get_next_irq(i)) != -1)
 *				process_irq(i);
 *		}
 *
 *
 *	RETURNS:
 *	Returns the IRQ number or -1 if a spurious interrupt was detected
 */
int
arm_get_next_irq(int last_irq)
{
	uint32_t active_irq;
	
	/* clean-up the last IRQ */
	if (last_irq != -1) {
		
		/* write the last IRQ number to the end of interrupt register */
		OMAP4_GIC_CPU_WRITE(omap4_dev, ARM_GIC_ICCEOIR, last_irq);
	}
	
	/* Get the next active interrupt */
	active_irq = OMAP4_GIC_CPU_READ(omap4_dev, ARM_GIC_ICCIAR);
	active_irq &= 0x3FF;
	
	/* Check for spurious interrupt */
	if (active_irq == 0x3FF) {
		if (last_irq == -1)
			printf("Spurious interrupt detected [0x%08x]\n", active_irq);
		return -1;
	}

	/* Return the new IRQ */
	return active_irq;
}
