/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000, 2001 MIPS Technologies, Inc.
 * Copyright (C) 2001 Ralf Baechle
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Routines for generic manipulation of the interrupts found on the MIPS
 * Malta board.
 * The interrupt controller is located in the South Bridge a PIIX4 device
 * with two internal 82C95 interrupt controllers.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/random.h>

#include <asm/i8259.h>
#include <asm/io.h>
#include <asm/mips-boards/malta.h>
#include <asm/mips-boards/maltaint.h>
#include <asm/mips-boards/piix4.h>
#include <asm/gt64120/gt64120.h>
#include <asm/mips-boards/generic.h>
#include <asm/mips-boards/msc01_pci.h>

extern asmlinkage void mipsIRQ(void);
extern int mips_pcibios_iack(void);

#ifdef CONFIG_KGDB
extern void breakpoint(void);
extern void set_debug_traps(void);
extern int remote_debug;
#endif

static spinlock_t mips_irq_lock = SPIN_LOCK_UNLOCKED;

/*
 * Algorithmics Bonito64 system controller register base.
 */
static char * const _bonito = (char *)KSEG1ADDR(BONITO_REG_BASE);

static inline int mips_pcibios_iack(void)
{
	int irq;
        u32 dummy;

	/*
	 * Determine highest priority pending interrupt by performing
	 * a PCI Interrupt Acknowledge cycle.
	 */
	switch(mips_revision_corid) {
	case MIPS_REVISION_CORID_QED_RM5261:
	case MIPS_REVISION_CORID_CORE_LV:
	case MIPS_REVISION_CORID_CORE_FPGA:
	case MIPS_REVISION_CORID_CORE_MSC:
		if (mips_revision_corid == MIPS_REVISION_CORID_CORE_MSC)
			MSC_READ(MSC01_PCI_IACK, irq);
		else
			irq = GT_READ(GT_PCI0_IACK_OFS);
		irq &= 0xff;
		break;
	case MIPS_REVISION_CORID_BONITO64:
	case MIPS_REVISION_CORID_CORE_20K:
		/* The following will generate a PCI IACK cycle on the
		 * Bonito controller. It's a little bit kludgy, but it
		 * was the easiest way to implement it in hardware at
		 * the given time.
		 */
		BONITO_PCIMAP_CFG = 0x20000;

		/* Flush Bonito register block */
		dummy = BONITO_PCIMAP_CFG;
		iob();    /* sync */

		irq = *(volatile u32 *)(KSEG1ADDR(BONITO_PCICFG_BASE));
		iob();    /* sync */
		irq &= 0xff;
		BONITO_PCIMAP_CFG = 0;
		break;
	default:
	        printk("Unknown Core card, don't know the system controller.\n");
		return -1;
	}
	return irq;
}

static inline int get_int(int *irq)
{
	unsigned long flags;

	spin_lock_irqsave(&mips_irq_lock, flags);

	*irq = mips_pcibios_iack();

	/*
	 * IRQ7 is used to detect spurious interrupts.
	 * The interrupt acknowledge cycle returns IRQ7, if no
	 * interrupts is requested.
	 * We can differentiate between this situation and a
	 * "Normal" IRQ7 by reading the ISR.
	 */
	if (*irq == 7)
	{
		outb(PIIX4_OCW3_SEL | PIIX4_OCW3_ISR,
		     PIIX4_ICTLR1_OCW3);
		if (!(inb(PIIX4_ICTLR1_OCW3) & (1 << 7))) {
			spin_unlock_irqrestore(&mips_irq_lock, flags);
			printk("We got a spurious interrupt from PIIX4.\n");
			atomic_inc(&irq_err_count);
			return -1;    /* Spurious interrupt. */
		}
	}

	spin_unlock_irqrestore(&mips_irq_lock, flags);

	return 0;
}

void malta_hw0_irqdispatch(struct pt_regs *regs)
{
	int irq;

	if (get_int(&irq))
	        return;  /* interrupt has already been cleared */

	do_IRQ(irq, regs);
}

void corehi_irqdispatch(struct pt_regs *regs)
{
        unsigned int data,datahi;

	/* Mask out corehi interrupt. */
	clear_c0_status(IE_IRQ3);

        printk("CoreHI interrupt, shouldn't happen, so we die here!!!\n");
        printk("epc   : %08lx\nStatus: %08lx\nCause : %08lx\nbadVaddr : %08lx\n"
, regs->cp0_epc, regs->cp0_status, regs->cp0_cause, regs->cp0_badvaddr);
        switch(mips_revision_corid) {
        case MIPS_REVISION_CORID_CORE_MSC:
                break;
        case MIPS_REVISION_CORID_QED_RM5261:
        case MIPS_REVISION_CORID_CORE_LV:
        case MIPS_REVISION_CORID_CORE_FPGA:
                data = GT_READ(GT_INTRCAUSE_OFS);
                printk("GT_INTRCAUSE = %08x\n", data);
                data = GT_READ(0x70);
                datahi = GT_READ(0x78);
                printk("GT_CPU_ERR_ADDR = %02x%08x\n", datahi, data);
                break;
        case MIPS_REVISION_CORID_BONITO64:
        case MIPS_REVISION_CORID_CORE_20K:
                data = BONITO_INTISR;
                printk("BONITO_INTISR = %08x\n", data);
                data = BONITO_INTEN;
                printk("BONITO_INTEN = %08x\n", data);
                data = BONITO_INTPOL;
                printk("BONITO_INTPOL = %08x\n", data);
                data = BONITO_INTEDGE;
                printk("BONITO_INTEDGE = %08x\n", data);
                data = BONITO_INTSTEER;
                printk("BONITO_INTSTEER = %08x\n", data);
                data = BONITO_PCICMD;
                printk("BONITO_PCICMD = %08x\n", data);
                break;
        }

        /* We die here*/
        die("CoreHi interrupt", regs);
}

void __init init_IRQ(void)
{
	set_except_vector(0, mipsIRQ);
	init_generic_irq();
	init_i8259_irqs();

#ifdef CONFIG_KGDB
	if (remote_debug) {
		set_debug_traps();
		breakpoint();
	}
#endif
}


