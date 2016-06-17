/*
 * Copyright (C) 2003 PMC-Sierra Inc.
 * Author: Manish Lachwani (lachwani@pmc-sierra.com)
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
 *
 * Second level Interrupt handlers for the PMC-Sierra Titan/Yosemite board
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

/* Hypertransport specific */
#define IRQ_STATUS_REG_CPU0     0xbb001b30 	/* INT# 3 status register on CPU 0*/
#define	IRQ_STATUS_REG_CPU1	0xbb002b30	/* INT# 3 status register on CPU 1*/
#define IRQ_ACK_BITS            0x00000000 	/* Ack bits */
#define IRQ_CLEAR_REG_CPU0      0xbb002b3c 	/* IRQ clear register on CPU 0*/
#define IRQ_CLEAR_REG_CPU0      0xbb002b3c      /* IRQ clear register on CPU 1*/

#define HYPERTRANSPORT_EOI      0xbb0006E0 	/* End of Interrupt */
#define HYPERTRANSPORT_INTA     0x78    	/* INTA# */
#define HYPERTRANSPORT_INTB     0x79    	/* INTB# */
#define HYPERTRANSPORT_INTC     0x7a    	/* INTC# */
#define HYPERTRANSPORT_INTD     0x7b    	/* INTD# */

#define read_32bit_cp0_set1_register(source)                    \
({ int __res;                                                   \
        __asm__ __volatile__(                                   \
        ".set\tpush\n\t"                                        \
        ".set\treorder\n\t"                                     \
        "cfc0\t%0,"STR(source)"\n\t"                            \
        ".set\tpop"                                             \
        : "=r" (__res));                                        \
        __res;})

#define write_32bit_cp0_set1_register(register,value)           \
        __asm__ __volatile__(                                   \
        "ctc0\t%0,"STR(register)"\n\t"                          \
        "nop"                                                   \
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
        status |= (set_mask & 0xFF) << 8 | 0x0000FF00;
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

        return 0;                               /* never anything pending */
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

extern asmlinkage void titan_handle_int(void);
extern void jaguar_mailbox_irq(struct pt_regs *);

/* 
 * Handle hypertransport & SMP interrupts. The interrupt lines are scarce. For interprocessor
 * interrupts, the best thing to do is to use the INTMSG register. We use the same external
 * interrupt line, i.e. INTB3 and monitor another status bit
 */
asmlinkage void ll_ht_smp_irq_handler(int irq, struct pt_regs *regs)
{
        u32 status;
        status = *(volatile u_int32_t *)(IRQ_STATUS_REG_CPU0);

	/* Ack all the bits that correspond to the interrupt sources */
	if (status != 0)
	        *(volatile u_int32_t *)(IRQ_STATUS_REG_CPU0) = IRQ_ACK_BITS;

	status = *(volatile u_int32_t *)(IRQ_STATUS_REG_CPU1);
	if (status != 0)
                *(volatile u_int32_t *)(IRQ_STATUS_REG_CPU1) = IRQ_ACK_BITS;

#ifdef CONFIG_SMP
	if (status == 0x2) {
		/* This is an SMP IPI sent from one core to another */
		jaguar_mailbox_irq(regs);
		goto done;
	}
#endif
	
#ifdef CONFIG_HT_LEVEL_TRIGGER
        /*
         * Level Trigger Mode only. Send the HT EOI message back to the source.
         */
        switch (status) {
                case 0x1000000:
                        *(volatile u_int32_t *)(HYPERTRANSPORT_EOI) = HYPERTRANSPORT_INTA;
                        break;
                case 0x2000000:
                        *(volatile u_int32_t *)(HYPERTRANSPORT_EOI) = HYPERTRANSPORT_INTB;
                        break;
                case 0x4000000:
                        *(volatile u_int32_t *)(HYPERTRANSPORT_EOI) = HYPERTRANSPORT_INTC;
                        break;
                case 0x8000000:
                        *(volatile u_int32_t *)(HYPERTRANSPORT_EOI) = HYPERTRANSPORT_INTD;
                        break;
                case 0x0000001:
                        /* PLX */
                        *(volatile u_int32_t *)(HYPERTRANSPORT_EOI) = 0x20;
                        *(volatile u_int32_t *)(IRQ_CLEAR_REG) = IRQ_ACK_BITS;
                        break;
                case 0xf000000:
                        *(volatile u_int32_t *)(HYPERTRANSPORT_EOI) = HYPERTRANSPORT_INTA;
                        *(volatile u_int32_t *)(HYPERTRANSPORT_EOI) = HYPERTRANSPORT_INTB;
                        *(volatile u_int32_t *)(HYPERTRANSPORT_EOI) = HYPERTRANSPORT_INTC;
                        *(volatile u_int32_t *)(HYPERTRANSPORT_EOI) = HYPERTRANSPORT_INTD;
                        break;
        }
#endif /* CONFIG_HT_LEVEL_TRIGGER */

done:
	if (status != 0x2)
		/* Not for SMP */
		do_IRQ(irq, regs);	
}

/*
 * Initialize the next level interrupt handler
 */
void __init init_IRQ(void)
{
	int	i;

	clear_c0_status(ST0_IM | ST0_BEV);
	__cli();

	set_except_vector(0, titan_handle_int);
	init_generic_irq();

	for (i = 0; i < 13; i++) {
                irq_desc[i].status      = IRQ_DISABLED;
                irq_desc[i].action      = 0;
                irq_desc[i].depth       = 1;
                irq_desc[i].handler     = &rm9000_hpcdma_irq_type;
        }
}

