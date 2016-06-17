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
 * Converted to use new generic busmouse code.  5 Apr 1998
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
#include <linux/logibusmouse.h>

#include <asm/setup.h>
#include <asm/atarikb.h>
#include <asm/uaccess.h>

#include "busmouse.h"

static int msedev;
static int mouse_threshold[2] = {2,2};
MODULE_PARM(mouse_threshold, "2i");
extern int atari_mouse_buttons;

static void atari_mouse_interrupt(char *buf)
{
	int buttons;

/*	ikbd_mouse_disable(); */

	buttons = ((buf[0] & 1)
	       | ((buf[0] & 2) << 1)
	       | (atari_mouse_buttons & 2));
	atari_mouse_buttons = buttons;

	busmouse_add_movementbuttons(msedev, buf[1], -buf[2], buttons ^ 7);
/*	ikbd_mouse_rel_pos(); */
}

static int release_mouse(struct inode *inode, struct file *file)
{
	ikbd_mouse_disable();
	atari_mouse_interrupt_hook = NULL;
	return 0;
}

static int open_mouse(struct inode *inode, struct file *file)
{
	atari_mouse_buttons = 0;
	ikbd_mouse_y0_top ();
	ikbd_mouse_thresh (mouse_threshold[0], mouse_threshold[1]);
	ikbd_mouse_rel_pos();
	atari_mouse_interrupt_hook = atari_mouse_interrupt;
	return 0;
}

static struct busmouse atarimouse = {
	ATARIMOUSE_MINOR, "atarimouse", THIS_MODULE, open_mouse, release_mouse, 0
};

static int __init atari_mouse_init(void)
{
	if (!MACH_IS_ATARI)
		return -ENODEV;
	msedev = register_busmouse(&atarimouse);
	if (msedev < 0)
    		printk(KERN_WARNING "Unable to register Atari mouse driver.\n");
	else
		printk(KERN_INFO "Atari mouse installed.\n");
	return msedev < 0 ? msedev : 0;
}


#ifndef MODULE

#define	MIN_THRESHOLD 1
#define	MAX_THRESHOLD 20	/* more seems not reasonable... */

static int __init atari_mouse_setup( char *str )
{
    int ints[8];

    str = get_options(str, ARRAY_SIZE(ints), ints);

    if (ints[0] < 1) {
	printk(KERN_ERR "atari_mouse_setup: no arguments!\n" );
	return 0;
    }
    else if (ints[0] > 2) {
	printk(KERN_WARNING "atari_mouse_setup: too many arguments\n" );
    }

    if (ints[1] < MIN_THRESHOLD || ints[1] > MAX_THRESHOLD)
	printk(KERN_WARNING "atari_mouse_setup: bad threshold value (ignored)\n" );
    else {
	mouse_threshold[0] = ints[1];
	mouse_threshold[1] = ints[1];
	if (ints[0] > 1) {
	    if (ints[2] < MIN_THRESHOLD || ints[2] > MAX_THRESHOLD)
		printk(KERN_WARNING "atari_mouse_setup: bad threshold value (ignored)\n" );
	    else
		mouse_threshold[1] = ints[2];
	}
    }

    return 1;
}

__setup("atarimouse=",atari_mouse_setup);

#endif /* !MODULE */

static void __exit atari_mouse_cleanup (void)
{
	unregister_busmouse(msedev);
}

module_init(atari_mouse_init);
module_exit(atari_mouse_cleanup);

MODULE_LICENSE("GPL");