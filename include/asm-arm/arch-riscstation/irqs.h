/*
 *  linux/include/asm-arm/arch-rs/irqs.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * (c) 2002 Simtec Electronics / Ben Dooks
 */

/* identical to the arch-rpc implementation (bjd) */

#include <asm/arch-rpc/irqs.h>

#define RTC_IRQ    (27)
#define IRQ_MOUSERX  (40)
#define IRQ_MOUSETX  (41)

#undef IRQ_MOUSERX
#undef IRQ_MOUSETX
#define IRQ_MOUSERX      (40)
#define IRQ_MOUSETX      (41)
