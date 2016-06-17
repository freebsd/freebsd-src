/*
 *  arch/ppc/kernel/open_pic.h -- OpenPIC Interrupt Handling
 *
 *  Copyright (C) 1997 Geert Uytterhoeven
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive
 *  for more details.
 *  
 */

#ifndef _PPC64_KERNEL_OPEN_PIC_H
#define _PPC64_KERNEL_OPEN_PIC_H

#include <linux/config.h>

#define OPENPIC_SIZE	0x40000

/* OpenPIC IRQ controller structure */
extern struct hw_interrupt_type open_pic;

/* OpenPIC IPI controller structure */
#ifdef CONFIG_SMP
extern struct hw_interrupt_type open_pic_ipi;
#endif /* CONFIG_SMP */

extern u_int OpenPIC_NumInitSenses;
extern u_char *OpenPIC_InitSenses;
extern void* OpenPIC_Addr;

/* Exported functions */
extern void openpic_init(int, int, unsigned char *, int);
extern void openpic_request_IPIs(void);
extern void do_openpic_setup_cpu(void);
extern int openpic_get_irq(struct pt_regs *regs);
extern void openpic_init_processor(u_int cpumask);
extern void openpic_setup_ISU(int isu_num, unsigned long addr);
extern void openpic_cause_IPI(u_int ipi, u_int cpumask);

#endif /* _PPC64_KERNEL_OPEN_PIC_H */
