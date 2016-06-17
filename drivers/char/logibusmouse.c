/*
 * Logitech Bus Mouse Driver for Linux
 * by James Banks
 *
 * Mods by Matthew Dillon
 *   calls verify_area()
 *   tracks better when X is busy or paging
 *
 * Heavily modified by David Giller
 *   changed from queue- to counter- driven
 *   hacked out a (probably incorrect) mouse_select
 *
 * Modified again by Nathan Laredo to interface with
 *   0.96c-pl1 IRQ handling changes (13JUL92)
 *   didn't bother touching select code.
 *
 * Modified the select() code blindly to conform to the VFS
 *   requirements. 92.07.14 - Linus. Somebody should test it out.
 *
 * Modified by Johan Myreen to make room for other mice (9AUG92)
 *   removed assignment chr_fops[10] = &mouse_fops; see mouse.c
 *   renamed mouse_fops => bus_mouse_fops, made bus_mouse_fops public.
 *   renamed this file mouse.c => busmouse.c
 *
 * Minor addition by Cliff Matthews
 *   added fasync support
 *
 * Modularised 6-Sep-95 Philip Blundell <pjb27@cam.ac.uk> 
 *
 * Replaced dumb busy loop with udelay()  16 Nov 95
 *   Nathan Laredo <laredo@gnu.ai.mit.edu>
 *
 * Track I/O ports with request_region().  12 Dec 95 Philip Blundell
 *
 * Converted to use new generic busmouse code.  5 Apr 1998
 *   Russell King <rmk@arm.uk.linux.org>
 */

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/logibusmouse.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/ioport.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/irq.h>

#include "busmouse.h"

static int msedev;
static int mouse_irq = MOUSE_IRQ;

MODULE_PARM(mouse_irq, "i");

#ifndef MODULE

static int __init bmouse_setup(char *str)
{
	int ints[4];

	str = get_options(str, ARRAY_SIZE(ints), ints);

	if (ints[0] > 0)
		mouse_irq=ints[1];

	return 1;
}

__setup("logi_busmouse=", bmouse_setup);

#endif /* !MODULE */

static void mouse_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	char dx, dy;
	unsigned char buttons;

	outb(MSE_READ_X_LOW, MSE_CONTROL_PORT);
	dx = (inb(MSE_DATA_PORT) & 0xf);
	outb(MSE_READ_X_HIGH, MSE_CONTROL_PORT);
	dx |= (inb(MSE_DATA_PORT) & 0xf) << 4;
	outb(MSE_READ_Y_LOW, MSE_CONTROL_PORT );
	dy = (inb(MSE_DATA_PORT) & 0xf);
	outb(MSE_READ_Y_HIGH, MSE_CONTROL_PORT);
	buttons = inb(MSE_DATA_PORT);
	dy |= (buttons & 0xf) << 4;
	buttons = ((buttons >> 5) & 0x07);
	busmouse_add_movementbuttons(msedev, dx, -dy, buttons);
	MSE_INT_ON();
}

/*
 * close access to the mouse
 */
static int close_mouse(struct inode * inode, struct file * file)
{
	MSE_INT_OFF();
	free_irq(mouse_irq, NULL);
	return 0;
}

/*
 * open access to the mouse
 */

static int open_mouse(struct inode * inode, struct file * file)
{
	if (request_irq(mouse_irq, mouse_interrupt, 0, "busmouse", NULL))
		return -EBUSY;
	MSE_INT_ON();
	return 0;
}

static struct busmouse busmouse = {
	LOGITECH_BUSMOUSE, "busmouse", THIS_MODULE, open_mouse, close_mouse, 7
};

static int __init logi_busmouse_init(void)
{
	if (!request_region(LOGIBM_BASE, LOGIBM_EXTENT, "busmouse"))
		return -EIO;

	outb(MSE_CONFIG_BYTE, MSE_CONFIG_PORT);
	outb(MSE_SIGNATURE_BYTE, MSE_SIGNATURE_PORT);
	udelay(100L);	/* wait for reply from mouse */
	if (inb(MSE_SIGNATURE_PORT) != MSE_SIGNATURE_BYTE) {
		release_region(LOGIBM_BASE, LOGIBM_EXTENT);
		return -EIO;
	}

	outb(MSE_DEFAULT_MODE, MSE_CONFIG_PORT);
	MSE_INT_OFF();
	
	msedev = register_busmouse(&busmouse);
	if (msedev < 0) {
		release_region(LOGIBM_BASE, LOGIBM_EXTENT);
		printk(KERN_WARNING "Unable to register busmouse driver.\n");
	} 
	else
		printk(KERN_INFO "Logitech busmouse installed.\n");
	
	return msedev < 0 ? msedev : 0;
}

static void __exit logi_busmouse_cleanup (void)
{
	unregister_busmouse(msedev);
	release_region(LOGIBM_BASE, LOGIBM_EXTENT);
}

module_init(logi_busmouse_init);
module_exit(logi_busmouse_cleanup);

MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
