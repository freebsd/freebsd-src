/*
 *  linux/include/asm-arm/arch-arc/keyboard.h
 *
 *  Copyright (C) 1998-2001 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Keyboard driver definitions for Acorn Archimedes/A5000
 *  architecture
 */
#include <asm/irq.h>

#define NR_SCANCODES 128

extern void a5kkbd_init_hw(void);

#define kbd_disable_irq()	disable_irq(IRQ_KEYBOARDRX)
#define kbd_enable_irq()	enable_irq(IRQ_KEYBOARDRX)
#define kbd_init_hw()		a5kkbd_init_hw()
