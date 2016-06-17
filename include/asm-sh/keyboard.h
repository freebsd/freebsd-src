#ifndef	__ASM_SH_KEYBOARD_H
#define	__ASM_SH_KEYBOARD_H
/*
 *	$Id: keyboard.h,v 1.2 2001/10/15 04:32:05 mrbrown Exp $
 */

#include <linux/kd.h>
#include <linux/config.h>
#include <asm/machvec.h>

#ifdef CONFIG_SH_EC3104
#include <asm/keyboard-ec3104.h>
#elif defined(CONFIG_SH_HS7729PCI)
#include <asm/keyboard-hs7729pci.h>
#else
static __inline__ int kbd_setkeycode(unsigned int scancode,
				     unsigned int keycode)
{
    return -EOPNOTSUPP;
}

static __inline__ int kbd_getkeycode(unsigned int scancode)
{
    return scancode > 127 ? -EINVAL : scancode;
}

#ifdef CONFIG_SH_DREAMCAST
extern int kbd_translate(unsigned char scancode, unsigned char *keycode,
			 char raw_mode);
#else
static __inline__ int kbd_translate(unsigned char scancode,
				    unsigned char *keycode, char raw_mode)
{
    *keycode = scancode;
    return 1;
}
#endif

static __inline__ char kbd_unexpected_up(unsigned char keycode)
{
    return 0200;
}

static __inline__ void kbd_leds(unsigned char leds)
{
}

extern void hp600_kbd_init_hw(void);
extern void dreamcast_kbd_init_hw(void);

static __inline__ void kbd_init_hw(void)
{
	if (MACH_HP600) {
		hp600_kbd_init_hw();
	}
}

#endif
#endif
