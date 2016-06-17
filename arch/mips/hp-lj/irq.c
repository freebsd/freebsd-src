/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Code to handle x86 style IRQs plus some generic interrupt stuff.
 *
 * Copyright (C) 1992 Linus Torvalds
 * Copyright (C) 1994 - 2000 Ralf Baechle
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <asm/mipsregs.h>
#include <asm/system.h>
#include <asm/gdb-stub.h>


/* install the handler for exception 0 */
void __init init_IRQ(void)
{
    extern void hpIRQ(void);
    extern void mips_cpu_irq_init(u32 base);
    mips_cpu_irq_init(0);
    set_except_vector(0, hpIRQ);

#ifdef CONFIG_KGDB
    {
       extern void breakpoint(void);
       extern int remote_debug;

       if (remote_debug) {
          set_debug_traps();
          breakpoint();
       }
    }
#endif

}

