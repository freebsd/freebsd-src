/*
 *  linux/arch/arm/kernel/irq-arch.c
 *
 *  Copyright (C) 1995-2000 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  We contain the architecture-specific parts of interrupt handling
 *  in this file.  In 2.5, it will move into the various arch/arm/mach-*
 *  directories.
 */
#include <linux/ptrace.h>
#include <linux/kernel_stat.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/smp.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/system.h>

#include <asm/mach/irq.h>

/*
 * Get architecture specific interrupt handlers
 * and interrupt initialisation.
 */
#include <asm/arch/irq.h>

void __init genarch_init_irq(void)
{
	irq_init_irq();
}

