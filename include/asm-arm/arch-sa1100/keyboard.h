/*
 *  linux/include/asm-arm/arch-sa1100/keyboard.h
 *  Created 16 Dec 1999 by Nicolas Pitre <nico@cam.org>
 *  This file contains the SA1100 architecture specific keyboard definitions
 */
#ifndef _SA1100_KEYBOARD_H
#define _SA1100_KEYBOARD_H

#include <linux/config.h>
#include <asm/mach-types.h>
#include <asm/arch/assabet.h>

#define kbd_disable_irq()	do { } while(0)
#define kbd_enable_irq()	do { } while(0)

extern int sa1111_kbd_init_hw(void);
extern void gc_kbd_init_hw(void);
extern void smartio_kbd_init_hw(void);
extern void cerf_kbd_init_hw(void);

static inline void kbd_init_hw(void)
{
	if ((machine_is_assabet() && machine_has_neponset()) ||
	    machine_is_graphicsmaster() || machine_is_adsagc())
		sa1111_kbd_init_hw();
	if (machine_is_graphicsclient())
		gc_kbd_init_hw();
	if (machine_is_adsbitsy() || machine_is_adsbitsyplus())
		smartio_kbd_init_hw();
#ifdef CONFIG_SA1100_CERF_CPLD
	if (machine_is_cerf())
		cerf_kbd_init_hw();
#endif
#ifdef CONFIG_SA1100_PT_SYSTEM3
	/* TODO: add system 3 board specific functions here */
	if (machine_is_pt_system3())
		sa1111_kbd_init_hw();
#endif
}

#endif  /* _SA1100_KEYBOARD_H */
