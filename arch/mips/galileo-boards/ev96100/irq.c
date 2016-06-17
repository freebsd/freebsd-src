/*
 * Copyright 2000 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
 *
 * This file was derived from Carsten Langgaard's
 * arch/mips/mips-boards/atlas/atlas_int.c.
 *
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/timex.h>
#include <linux/slab.h>
#include <linux/random.h>

#include <asm/bitops.h>
#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/mipsregs.h>
#include <asm/system.h>
#include <asm/galileo-boards/ev96100int.h>

extern void mips_timer_interrupt(int irq, struct pt_regs *regs);
extern asmlinkage void ev96100IRQ(void);

static void disable_ev96100_irq(unsigned int irq_nr)
{
	unsigned long flags;

	save_and_cli(flags);
	clear_c0_status(0x100 << irq_nr);
	restore_flags(flags);
}

static inline void enable_ev96100_irq(unsigned int irq_nr)
{
	unsigned long flags;

	save_and_cli(flags);
	set_c0_status(0x100 << irq_nr);
	restore_flags(flags);
}

static unsigned int startup_ev96100_irq(unsigned int irq)
{
	enable_ev96100_irq(irq);

	return 0;	/* never anything pending */
}

#define shutdown_ev96100_irq		disable_ev96100_irq
#define mask_and_ack_ev96100_irq	disable_ev96100_irq

static void end_ev96100_irq (unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		enable_ev96100_irq(irq);
}

static inline unsigned int ffz8(unsigned int word)
{
	unsigned long k;

	k = 7;
	if (word & 0x0fUL) { k -= 4;  word <<= 4;  }
	if (word & 0x30UL) { k -= 2;  word <<= 2;  }
	if (word & 0x40UL) { k -= 1; }

	return k;
}

asmlinkage void ev96100_cpu_irq(unsigned long cause, struct pt_regs * regs)
{
	if (!(cause & 0xff00))
		return;

	do_IRQ(ffz8((cause >> 8) & 0xff), regs);
}

static struct hw_interrupt_type ev96100_irq_type = {
	"EV96100",
	startup_ev96100_irq,
	shutdown_ev96100_irq,
	enable_ev96100_irq,
	disable_ev96100_irq,
	mask_and_ack_ev96100_irq,
	end_ev96100_irq
};

void __init init_IRQ(void)
{
	int i;

	set_except_vector(0, ev96100IRQ);
	init_generic_irq();

	for (i = 0; i < 8; i++) {
		irq_desc[i].status	= IRQ_DISABLED;
		irq_desc[i].action	= 0;
		irq_desc[i].depth	= 1;
		irq_desc[i].handler	= &ev96100_irq_type;
	}
}
