/*
 * Copyright (C) 2002 Momentum Computer, Inc.
 * Author: Matthew Dharm, mdharm@momenco.com
 *
 * Based on work by:
 *   Copyright (C) 2000 RidgeRun, Inc.
 *   Author: RidgeRun, Inc.
 *   glonnon@ridgerun.com, skranz@ridgerun.com, stevej@ridgerun.com
 *
 *   Copyright 2001 MontaVista Software Inc.
 *   Author: Jun Sun, jsun@mvista.com or jsun@junsun.net
 *
 *   Copyright (C) 2000, 2001 Ralf Baechle (ralf@gnu.org)
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
#include <asm/irq.h>
#include <asm/mipsregs.h>
#include <asm/system.h>

#define read_32bit_cp0_set1_register(source)                    \
({ int __res;                                                   \
        __asm__ __volatile__(                                   \
	".set\tpush\n\t"					\
	".set\treorder\n\t"					\
        "cfc0\t%0,"STR(source)"\n\t"                            \
	".set\tpop"						\
        : "=r" (__res));                                        \
        __res;})

#define write_32bit_cp0_set1_register(register,value)           \
        __asm__ __volatile__(                                   \
        "ctc0\t%0,"STR(register)"\n\t"				\
	"nop"							\
        : : "r" (value));

static spinlock_t irq_lock = SPIN_LOCK_UNLOCKED;

/* Function for careful CP0 interrupt mask access */
static inline void modify_cp0_intmask(unsigned clr_mask_in, unsigned set_mask_in)
{
	unsigned long status;
	unsigned clr_mask;
	unsigned set_mask;

	/* do the low 8 bits first */
	clr_mask = 0xff & clr_mask_in;
	set_mask = 0xff & set_mask_in;
	status = read_c0_status();
	status &= ~((clr_mask & 0xFF) << 8);
	status |= (set_mask & 0xFF) << 8;
	write_c0_status(status);

	/* do the high 8 bits */
	clr_mask = 0xff & (clr_mask_in >> 8);
	set_mask = 0xff & (set_mask_in >> 8);
	status = read_32bit_cp0_set1_register(CP0_S1_INTCONTROL);
	status &= ~((clr_mask & 0xFF) << 8);
	status |= (set_mask & 0xFF) << 8;
	write_32bit_cp0_set1_register(CP0_S1_INTCONTROL, status);
}

static inline void mask_irq(unsigned int irq)
{
	modify_cp0_intmask(irq, 0);
}

static inline void unmask_irq(unsigned int irq)
{
	modify_cp0_intmask(0, irq);
}

static void enable_rm9000_irq(unsigned int irq)
{
	unsigned long flags;

	spin_lock_irqsave(&irq_lock, flags);
	unmask_irq(1 << (irq-1));
	spin_unlock_irqrestore(&irq_lock, flags);
}

static unsigned int startup_rm9000_irq(unsigned int irq)
{
	enable_rm9000_irq(irq);

	return 0;				/* never anything pending */
}

static void disable_rm9000_irq(unsigned int irq)
{
	unsigned long flags;

	spin_lock_irqsave(&irq_lock, flags);
	mask_irq(1 << (irq-1));
	spin_unlock_irqrestore(&irq_lock, flags);
}

#define shutdown_rm9000_irq disable_rm9000_irq

static void mask_and_ack_rm9000_irq(unsigned int irq)
{
	mask_irq(1 << (irq-1));
}

static void end_rm9000_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		unmask_irq(1 << (irq-1));
}

static struct hw_interrupt_type rm9000_hpcdma_irq_type = {
	"RM9000",
	startup_rm9000_irq,
	shutdown_rm9000_irq,
	enable_rm9000_irq,
	disable_rm9000_irq,
	mask_and_ack_rm9000_irq,
	end_rm9000_irq,
	NULL
};

extern void wired_reset(void);
extern void PMON_v2_setup(void);
extern asmlinkage void jaguar_handle_int(void);
extern void mv64340_irq_init(void);

static struct irqaction cascade_mv64340 =
	{ no_action, SA_INTERRUPT, 0, "cascade via MV64340", NULL, NULL };
static struct irqaction unused_irq =
	{ no_action, SA_INTERRUPT, 0, "unused", NULL, NULL };

void __init init_IRQ(void)
{
	int i;

	/*
	 * Clear all of the interrupts while we change the able around a bit.
	 * int-handler is not on bootstrap
	 */
	clear_c0_status(ST0_IM | ST0_BEV);
	__cli();

	/* Sets the first-level interrupt dispatcher. */
	set_except_vector(0, jaguar_handle_int);
	init_generic_irq();

	/* set up handler for first 12 IRQs as the CPU */
	for (i = 0; i < 13; i++) {
		irq_desc[i].status	= IRQ_DISABLED;
		irq_desc[i].action	= 0;
		irq_desc[i].depth	= 1;
		irq_desc[i].handler	= &rm9000_hpcdma_irq_type;
	}

	/* set up the cascading interrupts */
	setup_irq(9, &cascade_mv64340);

	/* mark unconnected IRQs as unconnected */
	setup_irq(10, &unused_irq);

	/* mark un-used IRQ numbers as unconnected */
	setup_irq(13, &unused_irq);
	setup_irq(14, &unused_irq);
	setup_irq(15, &unused_irq);

	mv64340_irq_init();

#ifdef CONFIG_REMOTE_DEBUG
	printk("start kgdb ...\n");
	set_debug_traps();
	breakpoint();	/* you may move this line to whereever you want :-) */
#endif
#ifdef CONFIG_GDB_CONSOLE
	register_gdb_console();
#endif

}
