/*
 * linux/include/asm-arm/arch-tbox/keyboard.h
 *
 * Driver definitions for Tbox dummy keyboard.
 *
 * Copyright (C) 1998-2001 Russell King
 * Copyright (C) 1998 Philip Blundell
 */

#define NR_SCANCODES 128

#define kbd_init_hw()		do { } while (0)
#define kbd_disable_irq()	do { } while (0)
#define kbd_enable_irq()	do { } while (0)
