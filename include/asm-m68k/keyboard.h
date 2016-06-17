/*
 *  linux/include/asm-m68k/keyboard.h
 *
 *  Created 3 Nov 1996 by Geert Uytterhoeven
 */

/*
 *  This file contains the m68k architecture specific keyboard definitions
 */

#ifndef __M68K_KEYBOARD_H
#define __M68K_KEYBOARD_H

#ifdef __KERNEL__

#include <linux/config.h>
#include <linux/kd.h>
#include <asm/machdep.h>

#ifdef CONFIG_Q40
#include <asm/q40_keyboard.h>
#endif

static __inline__ int kbd_setkeycode(unsigned int scancode,
				     unsigned int keycode)
{
#ifdef CONFIG_Q40
    if (MACH_IS_Q40)
        return q40kbd_setkeycode(scancode,keycode);
#endif
    return -EOPNOTSUPP;
}

static __inline__ int kbd_getkeycode(unsigned int scancode)
{
#ifdef CONFIG_Q40
    if (MACH_IS_Q40)
        return q40kbd_getkeycode(scancode);
#endif
    return scancode > 127 ? -EINVAL : scancode;
}

static __inline__ char kbd_unexpected_up(unsigned char keycode)
{
#ifdef CONFIG_Q40
    if (MACH_IS_Q40)
        return q40kbd_unexpected_up(keycode);
#endif
    return 0200;
}

static __inline__ void kbd_leds(unsigned char leds)
{
    if (mach_kbd_leds)
	mach_kbd_leds(leds);
}

#define kbd_init_hw		mach_keyb_init
#define kbd_translate		mach_kbd_translate
#define kbd_rate		mach_kbdrate

#define kbd_sysrq_xlate		mach_sysrq_xlate

/* resource allocation */
#define kbd_request_region()
#define kbd_request_irq(handler)

/* How to access the keyboard macros on this platform.  */
#define kbd_read_input() in_8(KBD_DATA_REG)
#define kbd_read_status() in_8(KBD_STATUS_REG)
#define kbd_write_output(val) out_8(KBD_DATA_REG, val)
#define kbd_write_command(val) out_8(KBD_CNTL_REG, val)
extern unsigned int SYSRQ_KEY;

#endif /* __KERNEL__ */

#endif /* __M68K_KEYBOARD_H */
