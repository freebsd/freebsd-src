/*
 * linux/include/asm-arm/arch-clps711x/keyboard.h
 *
 * Copyright (C) 1998-2001 Russell King
 */
#include <asm/mach-types.h>

#define NR_SCANCODES 128

#define kbd_disable_irq()	do { } while (0)
#define kbd_enable_irq()	do { } while (0)

/*
 * EDB7211 keyboard driver
 */
extern void edb7211_kbd_init_hw(void);
extern void clps711x_kbd_init_hw(void);

static inline void kbd_init_hw(void)
{
	if (machine_is_edb7211())
		edb7211_kbd_init_hw();

	if (machine_is_autcpu12())
		clps711x_kbd_init_hw();
}
