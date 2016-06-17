/*
 * linux/include/asm-arm/arch-shark/keyboard.h
 * by Alexander Schulz
 * 
 * Derived from linux/include/asm-arm/arch-ebsa285/keyboard.h
 * (C) 1998 Russell King
 * (C) 1998 Phil Blundell
 */
#include <linux/config.h>
#include <linux/ioport.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/system.h>

#define KEYBOARD_IRQ			IRQ_ISA_KEYBOARD
#define NR_SCANCODES			128

#define kbd_disable_irq()		do { } while (0)
#define kbd_enable_irq()		do { } while (0)

extern int pckbd_setkeycode(unsigned int scancode, unsigned int keycode);
extern int pckbd_getkeycode(unsigned int scancode);
extern int pckbd_translate(unsigned char scancode, unsigned char *keycode,
			   char raw_mode);
extern char pckbd_unexpected_up(unsigned char keycode);
extern void pckbd_leds(unsigned char leds);
extern void pckbd_init_hw(void);
extern unsigned char pckbd_sysrq_xlate[128];

static inline void kbd_init_hw(void)
{
		k_setkeycode    = pckbd_setkeycode;
		k_getkeycode    = pckbd_getkeycode;
		k_translate     = pckbd_translate;
		k_unexpected_up = pckbd_unexpected_up;
		k_leds          = pckbd_leds;
#ifdef CONFIG_MAGIC_SYSRQ
		k_sysrq_key     = 0x54;
		k_sysrq_xlate   = pckbd_sysrq_xlate;
#endif
		pckbd_init_hw();
}

/*
 * PC Keyboard specifics
 */

/* resource allocation */
#define kbd_request_region() request_region(0x60, 16, "keyboard")
#define kbd_request_irq(handler) request_irq(KEYBOARD_IRQ, handler, 0, \
                                             "keyboard", NULL)

/* How to access the keyboard macros on this platform.  */
#define kbd_read_input() inb(KBD_DATA_REG)
#define kbd_read_status() inb(KBD_STATUS_REG)
#define kbd_write_output(val) outb(val, KBD_DATA_REG)
#define kbd_write_command(val) outb(val, KBD_CNTL_REG)

/* Some stoneage hardware needs delays after some operations.  */
#define kbd_pause() do { } while(0)

/*
 * Machine specific bits for the PS/2 driver
 */
#define aux_request_irq(hand, dev_id)					\
	request_irq(AUX_IRQ, hand, SA_SHIRQ, "PS/2 Mouse", dev_id)

#define aux_free_irq(dev_id) free_irq(AUX_IRQ, dev_id)
