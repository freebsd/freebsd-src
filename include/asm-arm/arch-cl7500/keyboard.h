/*
 * linux/include/asm-arm/arch-cl7500/keyboard.h
 *  from linux/include/asm-arm/arch-rpc/keyboard.h
 *
 * Keyboard driver definitions for CL7500 architecture
 *
 * Copyright (C) 1998-2001 Russell King
 */
#include <asm/irq.h>
#define NR_SCANCODES 128

extern int ps2kbd_init_hw(void);

#define kbd_disable_irq()	disable_irq(IRQ_KEYBOARDRX)
#define kbd_enable_irq()	enable_irq(IRQ_KEYBOARDRX)
#define kbd_init_hw()		ps2kbd_init_hw()
