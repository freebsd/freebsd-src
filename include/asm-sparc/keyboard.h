/* $Id: keyboard.h,v 1.7 2001/08/18 09:40:46 davem Exp $
 * linux/include/asm-sparc/keyboard.h
 *
 * sparc64 Created Aug 29 1997 by Eddie C. Dost (ecd@skynet.be)
 */

/*
 *  This file contains the Ultra/PCI architecture specific keyboard definitions
 */

#ifndef _SPARC_KEYBOARD_H
#define _SPARC_KEYBOARD_H 1

#ifdef __KERNEL__

#include <linux/config.h>
#include <linux/ioport.h>
#include <linux/kd.h>
#include <asm/io.h>

#define KEYBOARD_IRQ			13
#define DISABLE_KBD_DURING_INTERRUPTS	0

extern int pcikbd_setkeycode(unsigned int scancode, unsigned int keycode);
extern int pcikbd_getkeycode(unsigned int scancode);
extern int pcikbd_translate(unsigned char scancode, unsigned char *keycode,
			    char raw_mode);
extern char pcikbd_unexpected_up(unsigned char keycode);
extern void pcikbd_leds(unsigned char leds);
extern void pcikbd_init_hw(void);
extern unsigned char pcikbd_sysrq_xlate[128];

#define kbd_setkeycode			pcikbd_setkeycode
#define kbd_getkeycode			pcikbd_getkeycode
#define kbd_translate			pcikbd_translate
#define kbd_unexpected_up		pcikbd_unexpected_up
#define kbd_leds			pcikbd_leds
#define kbd_init_hw			pcikbd_init_hw
#define kbd_sysrq_xlate			pcikbd_sysrq_xlate
#define kbd_init			pcikbd_init

#define compute_shiftstate		pci_compute_shiftstate
#define getkeycode			pci_getkeycode
#define setkeycode			pci_setkeycode
#define getledstate			pci_getledstate
#define setledstate			pci_setledstate
#define register_leds			pci_register_leds

/* #define SYSRQ_KEY 0x54 */	/* sparc64 */
#define SYSRQ_KEY 0x63		/* sparc */

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

#define AUX_IRQ 12

#define aux_request_irq(hand, dev_id)					\
	request_irq(AUX_IRQ, hand, SA_SHIRQ, "PS/2 Mouse", dev_id)

#define aux_free_irq(dev_id) free_irq(AUX_IRQ, dev_id)

#endif /* __KERNEL__ */

#endif /* !(_SPARC_KEYBOARD_H) */
