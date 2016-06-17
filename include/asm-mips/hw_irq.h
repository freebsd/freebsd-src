/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2000, 2001 by Ralf Baechle
 */
#ifndef _ASM_HW_IRQ_H
#define _ASM_HW_IRQ_H

#include <asm/atomic.h>

extern atomic_t irq_err_count;

/* This may not be apropriate for all machines, we'll see ...  */
static inline void hw_resend_irq(struct hw_interrupt_type *h, unsigned int i)
{
}

#endif /* _ASM_HW_IRQ_H */
