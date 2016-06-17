/*
 * Macintosh ADB Mouse driver for Linux
 *
 * 27 Oct 1997 Michael Schmitz
 * logitech fixes by anthony tong
 * further hacking by Paul Mackerras
 *
 * Apple mouse protocol according to:
 *
 * Device code shamelessly stolen from:
 */
/*
 * Atari Mouse Driver for Linux
 * by Robert de Vries (robert@and.nl) 19Jul93
 *
 * 16 Nov 1994 Andreas Schwab
 * Compatibility with busmouse
 * Support for three button mouse (shamelessly stolen from MiNT)
 * third button wired to one of the joystick directions on joystick 1
 *
 * 1996/02/11 Andreas Schwab
 * Module support
 * Allow multiple open's
 *
 * Converted to use new generic busmouse code.  11 July 1998
 *   Russell King <rmk@arm.uk.linux.org>
 */

#include <linux/module.h>

#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/random.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/adb_mouse.h>

#ifdef __powerpc__
#include <asm/processor.h>
#endif
#if defined(__mc68000__) || defined(MODULE)
#include <asm/setup.h>
#endif

#include "busmouse.h"

static int msedev;
static unsigned char adb_mouse_buttons[16];

extern void (*adb_mouse_interrupt_hook)(unsigned char *, int);
extern int adb_emulate_buttons;
extern int adb_button2_keycode;
extern int adb_button3_keycode;

/*
 *    XXX: need to figure out what ADB mouse packets mean ... 
 *      This is the stuff stolen from the Atari driver ...
 */
static void adb_mouse_interrupt(unsigned char *buf, int nb)
{
	int buttons, id;
	char dx, dy;

	/*
	   Handler 1 -- 100cpi original Apple mouse protocol.
	   Handler 2 -- 200cpi original Apple mouse protocol.

	   For Apple's standard one-button mouse protocol the data array will
	   contain the following values:

		       BITS    COMMENTS
	   data[0] = dddd 1100 ADB command: Talk, register 0, for device dddd.
	   data[1] = bxxx xxxx First button and x-axis motion.
	   data[2] = byyy yyyy Second button and y-axis motion.

	   Handler 4 -- Apple Extended mouse protocol.

	   For Apple's 3-button mouse protocol the data array will contain the
	   following values:

		       BITS    COMMENTS
	   data[0] = dddd 1100 ADB command: Talk, register 0, for device dddd.
	   data[1] = bxxx xxxx Left button and x-axis motion.
	   data[2] = byyy yyyy Second button and y-axis motion.
	   data[3] = byyy bxxx Third button and fourth button.  
		   Y is additiona. high bits of y-axis motion.  
		   X is additional high bits of x-axis motion.

	   'buttons' here means 'button down' states!
	   Button 1 (left)  : bit 2, busmouse button 3
	   Button 2 (right) : bit 0, busmouse button 1
	   Button 3 (middle): bit 1, busmouse button 2
	 */

	/* x/y and buttons swapped */

	id = (buf[0] >> 4) & 0xf;

	buttons = adb_mouse_buttons[id];

	/* button 1 (left, bit 2) */
	buttons = (buttons & 3) | (buf[1] & 0x80 ? 4 : 0); /* 1+2 unchanged */

	/* button 2 (middle) */
	buttons = (buttons & 5) | (buf[2] & 0x80 ? 2 : 0); /* 2+3 unchanged */

	/* button 3 (right) present?
	 *  on a logitech mouseman, the right and mid buttons sometimes behave
	 *  strangely until they both have been pressed after booting. */
	/* data valid only if extended mouse format ! */
	if (nb >= 4)
		buttons = (buttons & 6) | (buf[3] & 0x80 ? 1 : 0); /* 1+3 unchanged */

	adb_mouse_buttons[id] = buttons;

	/* a button is down if it is down on any mouse */
	for (id = 0; id < 16; ++id)
		buttons &= adb_mouse_buttons[id];

	dx = ((buf[2] & 0x7f) < 64 ? (buf[2] & 0x7f) : (buf[2] & 0x7f) - 128);
	dy = ((buf[1] & 0x7f) < 64 ? (buf[1] & 0x7f) : (buf[1] & 0x7f) - 128);
	busmouse_add_movementbuttons(msedev, dx, -dy, buttons);

	if (console_loglevel >= 8)
		printk(" %X %X %X dx %d dy %d \n",
		       buf[1], buf[2], buf[3], dx, dy);
}

static int release_mouse(struct inode *inode, struct file *file)
{
	adb_mouse_interrupt_hook = NULL;
	/*
	 *	FIXME?: adb_mouse_interrupt_hook may still be executing
	 *	on another CPU.
	 */
	return 0;
}

static int open_mouse(struct inode *inode, struct file *file)
{
	adb_mouse_interrupt_hook = adb_mouse_interrupt;
	return 0;
}

static struct busmouse adb_mouse =
{
	ADB_MOUSE_MINOR, "adbmouse", THIS_MODULE, open_mouse, release_mouse, 7
};

static int __init adb_mouse_init(void)
{
#ifdef __powerpc__
	if ((_machine != _MACH_chrp) && (_machine != _MACH_Pmac))
		return -ENODEV;
#endif
#ifdef __mc68000__
	if (!MACH_IS_MAC)
		return -ENODEV;
#endif
	/* all buttons up */
	memset(adb_mouse_buttons, 7, sizeof(adb_mouse_buttons));

	msedev = register_busmouse(&adb_mouse);
	if (msedev < 0)
		printk(KERN_WARNING "Unable to register ADB mouse driver.\n");
	else
		printk(KERN_INFO "Macintosh ADB mouse driver installed.\n");

	return msedev < 0 ? msedev : 0;
}

#ifndef MODULE

/*
 * XXX this function is misnamed.
 * It is called if the kernel is booted with the adb_buttons=xxx
 * option, which is about using ADB keyboard buttons to emulate
 * mouse buttons. -- paulus
 */
static int __init adb_mouse_setup(char *str)
{
	int ints[4];

	str = get_options(str, ARRAY_SIZE(ints), ints);
	if (ints[0] >= 1) {
		adb_emulate_buttons = ints[1];
		if (ints[0] >= 2)
			adb_button2_keycode = ints[2];
		if (ints[0] >= 3)
			adb_button3_keycode = ints[3];
	}
	return 1;
}

__setup("adb_buttons=", adb_mouse_setup);

#endif /* !MODULE */

static void __exit adb_mouse_cleanup(void)
{
	unregister_busmouse(msedev);
}

module_init(adb_mouse_init);
module_exit(adb_mouse_cleanup);

MODULE_LICENSE("GPL");
