/*
 * Microsoft busmouse driver based on Logitech driver (see busmouse.c)
 *
 * Microsoft BusMouse support by Teemu Rantanen (tvr@cs.hut.fi) (02AUG92)
 *
 * Microsoft Bus Mouse support modified by Derrick Cole (cole@concert.net)
 *    8/28/92
 *
 * Microsoft Bus Mouse support folded into 0.97pl4 code
 *    by Peter Cervasio (pete%q106fm.uucp@wupost.wustl.edu) (08SEP92)
 * Changes:  Logitech and Microsoft support in the same kernel.
 *           Defined new constants in busmouse.h for MS mice.
 *           Added int mse_busmouse_type to distinguish busmouse types
 *           Added a couple of new functions to handle differences in using
 *             MS vs. Logitech (where the int variable wasn't appropriate).
 *
 * Modified by Peter Cervasio (address above) (26SEP92)
 * Changes:  Included code to (properly?) detect when a Microsoft mouse is
 *           really attached to the machine.  Don't know what this does to
 *           Logitech bus mice, but all it does is read ports.
 *
 * Modified by Christoph Niemann (niemann@rubdv15.etdv.ruhr-uni-bochum.de)
 * Changes:  Better interrupt-handler (like in busmouse.c).
 *	     Some changes to reduce code-size.
 *	     Changed detection code to use inb_p() instead of doing empty
 *	     loops to delay i/o.
 *
 * Modularised 8-Sep-95 Philip Blundell <pjb27@cam.ac.uk>
 *
 * Converted to use new generic busmouse code.  5 Apr 1998
 *   Russell King <rmk@arm.uk.linux.org>
 *
 * version 0.3b
 */

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/logibusmouse.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/random.h>
#include <linux/poll.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/irq.h>

#include "busmouse.h"

static int msedev;
static int mouse_irq = MOUSE_IRQ;

MODULE_PARM(mouse_irq, "i");

#ifndef MODULE

static int __init msmouse_setup(char *str)
{
        int ints[4];

        str = get_options(str, ARRAY_SIZE(ints), ints);

	if (ints[0] > 0)
		mouse_irq=ints[1];

	return 1;
}

__setup("msmouse=",msmouse_setup);

#endif /* !MODULE */

static void ms_mouse_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
        char dx, dy;
	unsigned char buttons;

	outb(MS_MSE_COMMAND_MODE, MS_MSE_CONTROL_PORT);
	outb((inb(MS_MSE_DATA_PORT) | 0x20), MS_MSE_DATA_PORT);

	outb(MS_MSE_READ_X, MS_MSE_CONTROL_PORT);
	dx = inb(MS_MSE_DATA_PORT);

	outb(MS_MSE_READ_Y, MS_MSE_CONTROL_PORT);
	dy = inb(MS_MSE_DATA_PORT);

	outb(MS_MSE_READ_BUTTONS, MS_MSE_CONTROL_PORT);
	buttons = ~(inb(MS_MSE_DATA_PORT)) & 0x07;

	outb(MS_MSE_COMMAND_MODE, MS_MSE_CONTROL_PORT);
	outb((inb(MS_MSE_DATA_PORT) & 0xdf), MS_MSE_DATA_PORT);

	/* why did the original have:
	 * if (dx != 0 || dy != 0 || buttons != mouse.buttons ||
	 *    ((~buttons) & 0x07))
	 *    ^^^^^^^^^^^^^^^^^^^ this?
	 */
	busmouse_add_movementbuttons(msedev, dx, -dy, buttons);
}

static int release_mouse(struct inode * inode, struct file * file)
{
	MS_MSE_INT_OFF();
	free_irq(mouse_irq, NULL);
	return 0;
}

static int open_mouse(struct inode * inode, struct file * file)
{
	if (request_irq(mouse_irq, ms_mouse_interrupt, 0, "MS Busmouse", NULL))
		return -EBUSY;

	outb(MS_MSE_START, MS_MSE_CONTROL_PORT);
	MS_MSE_INT_ON();	
	return 0;
}

static struct busmouse msbusmouse = {
	MICROSOFT_BUSMOUSE, "msbusmouse", THIS_MODULE, open_mouse, release_mouse, 0
};

static int __init ms_bus_mouse_init(void)
{
	int present = 0;
	int mse_byte, i;

	if (check_region(MS_MSE_CONTROL_PORT, 0x04))
		return -ENODEV;

	if (inb_p(MS_MSE_SIGNATURE_PORT) == 0xde) {

		mse_byte = inb_p(MS_MSE_SIGNATURE_PORT);

		for (i = 0; i < 4; i++) {
			if (inb_p(MS_MSE_SIGNATURE_PORT) == 0xde) {
				if (inb_p(MS_MSE_SIGNATURE_PORT) == mse_byte)
					present = 1;
				else
					present = 0;
			} else
				present = 0;
		}
	}
	if (present == 0)
		return -EIO;
	if (!request_region(MS_MSE_CONTROL_PORT, 0x04, "MS Busmouse"))
		return -EIO;
	
	MS_MSE_INT_OFF();
	msedev = register_busmouse(&msbusmouse);
	if (msedev < 0) {
		printk(KERN_WARNING "Unable to register msbusmouse driver.\n");
		release_region(MS_MSE_CONTROL_PORT, 0x04);
	}
	else
		printk(KERN_INFO "Microsoft BusMouse detected and installed.\n");
	return msedev < 0 ? msedev : 0;
}

static void __exit ms_bus_mouse_exit(void)
{
	unregister_busmouse(msedev);
	release_region(MS_MSE_CONTROL_PORT, 0x04);
}

module_init(ms_bus_mouse_init)
module_exit(ms_bus_mouse_exit)

MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
