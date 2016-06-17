/*
 * Amiga Mouse Driver for Linux 68k by Michael Rausch
 * based upon:
 *
 * Logitech Bus Mouse Driver for Linux
 * by James Banks
 *
 * Mods by Matthew Dillon
 *   calls verify_area()
 *   tracks better when X is busy or paging
 *
 * Heavily modified by David Giller
 *   changed from queue- to counter- driven
 *   hacked out a (probably incorrect) mouse_poll
 *
 * Modified again by Nathan Laredo to interface with
 *   0.96c-pl1 IRQ handling changes (13JUL92)
 *   didn't bother touching poll code.
 *
 * Modified the poll() code blindly to conform to the VFS
 *   requirements. 92.07.14 - Linus. Somebody should test it out.
 *
 * Modified by Johan Myreen to make room for other mice (9AUG92)
 *   removed assignment chr_fops[10] = &mouse_fops; see mouse.c
 *   renamed mouse_fops => bus_mouse_fops, made bus_mouse_fops public.
 *   renamed this file mouse.c => busmouse.c
 *
 * Modified for use in the 1.3 kernels by Jes Sorensen.
 *
 * Moved the isr-allocation to the mouse_{open,close} calls, as there
 *   is no reason to service the mouse in the vertical blank isr if
 *   the mouse is not in use.             Jes Sorensen
 *
 * Converted to use new generic busmouse code.  5 Apr 1998
 *   Russell King <rmk@arm.uk.linux.org>
 */

#include <linux/module.h>

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/random.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/logibusmouse.h>

#include <asm/setup.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/amigahw.h>
#include <asm/amigaints.h>

#include "busmouse.h"

#if AMIGA_OLD_INT
#define AMI_MSE_INT_ON()	mouseint_allowed = 1
#define AMI_MSE_INT_OFF()	mouseint_allowed = 0
static int mouseint_allowed;
#endif

static int msedev;

static void mouse_interrupt(int irq, void *dummy, struct pt_regs *fp)
{
	static int lastx=0, lasty=0;
	int dx, dy;
	int nx, ny;
	unsigned char buttons;

	unsigned short joy0dat, potgor;

#if AMIGA_OLD_INT
	if(!mouseint_allowed)
		return;
	AMI_MSE_INT_OFF();
#endif

	/*
	 *  This routine assumes, just like Kickstart, that the mouse
	 *  has not moved more than 127 ticks since last VBL.
	 */

	joy0dat = custom.joy0dat;

	nx = joy0dat & 0xff;
	ny = joy0dat >> 8;

	dx = nx - lastx;
	if (dx < - 127)
		dx = (256 + nx) - lastx;

	if (dx > 127)
		dx = (nx - 256) - lastx;

	dy = ny - lasty;
	if (dy < - 127)
		dy = (256 + ny) - lasty;

	if (dy > 127)
		dy = (ny - 256) - lasty;

	lastx = nx;
	lasty = ny;

#if 0
	dx = -lastdx;
	dx += (lastdx = joy0dat & 0xff);
	if (dx < -127)
	    dx = -255-dx;		/* underrun */
	else
	if (dx > 127)
	    dx = 255-dx;		/* overflow */

	dy = -lastdy;
	dy += (lastdy = joy0dat >> 8);
	if (dy < -127)
	    dy = -255-dy;
	else
	if (dy > 127)
	    dy = 255-dy;
#endif


	potgor = custom.potgor;
	buttons = (ciaa.pra & 0x40 ? 4 : 0) |	/* left button; note that the bits are low-active, as are the expected results -> double negation */
#if 1
		  (potgor & 0x0100 ? 2 : 0) |	/* middle button; emulation goes here */
#endif
		  (potgor & 0x0400 ? 1 : 0);	/* right button */


	busmouse_add_movementbuttons(msedev, dx, -dy, buttons);
#if AMIGA_OLD_INT
	AMI_MSE_INT_ON();
#endif
}

/*
 * close access to the mouse
 */

static int release_mouse(struct inode * inode, struct file * file)
{
	free_irq(IRQ_AMIGA_VERTB, mouse_interrupt);
#if AMIGA_OLD_INT
	AMI_MSE_INT_OFF();
#endif
	return 0;
}

/*
 * open access to the mouse, currently only one open is
 * allowed.
 */

static int open_mouse(struct inode * inode, struct file * file)
{
	/*
	 *  use VBL to poll mouse deltas
	 */

	if(request_irq(IRQ_AMIGA_VERTB, mouse_interrupt, 0,
	               "Amiga mouse", mouse_interrupt)) {
		printk(KERN_INFO "Installing Amiga mouse failed.\n");
		return -EIO;
	}

#if AMIGA_OLD_INT
	AMI_MSE_INT_ON();
#endif
	return 0;
}

static struct busmouse amigamouse = {
	AMIGAMOUSE_MINOR, "amigamouse", THIS_MODULE, open_mouse, release_mouse, 7
};

static int __init amiga_mouse_init(void)
{
	if (!MACH_IS_AMIGA || !AMIGAHW_PRESENT(AMI_MOUSE))
		return -ENODEV;
	if (!request_mem_region(CUSTOM_PHYSADDR+10, 2, "amigamouse [Denise]"))
		return -EBUSY;

	custom.joytest = 0;	/* reset counters */
#if AMIGA_OLD_INT
	AMI_MSE_INT_OFF();
#endif
	msedev = register_busmouse(&amigamouse);
	if (msedev < 0)
		printk(KERN_WARNING "Unable to install Amiga mouse driver.\n");
	else
		printk(KERN_INFO "Amiga mouse installed.\n");
	return msedev < 0 ? msedev : 0;
}

static void __exit amiga_mouse_exit(void)
{
	unregister_busmouse(msedev);
	release_mem_region(CUSTOM_PHYSADDR+10, 2);
}

module_init(amiga_mouse_init);
module_exit(amiga_mouse_exit);

MODULE_LICENSE("GPL");
