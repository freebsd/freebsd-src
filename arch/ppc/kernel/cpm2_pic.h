#ifndef _PPC_KERNEL_CPM2_H
#define _PPC_KERNEL_CPM2_H

#include <linux/irq.h>

extern struct hw_interrupt_type cpm2_pic;

void cpm2_pic_init(void);
void cpm2_do_IRQ(struct pt_regs *regs,
                 int            cpu);
int cpm2_get_irq(struct pt_regs *regs);

#endif /* _PPC_KERNEL_CPM2_H */
