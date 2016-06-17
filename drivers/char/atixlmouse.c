/*
 * ATI XL Bus Mouse Driver for Linux
 * by Bob Harris (rth@sparta.com)
 *
 * Uses VFS interface for linux 0.98 (01OCT92)
 *
 * Modified by Chris Colohan (colohan@eecg.toronto.edu)
 * Modularised 8-Sep-95 Philip Blundell <pjb27@cam.ac.uk>
 *
 * Converted to use new generic busmouse code.  5 Apr 1998
 *   Russell King <rmk@arm.uk.linux.org>
 *
 * version 0.3a
 */

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/random.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/ioport.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/irq.h>

#include "busmouse.h"

#define ATIXL_MOUSE_IRQ		5 /* H/W interrupt # set up on ATIXL board */
#define ATIXL_BUSMOUSE		3 /* Minor device # (mknod c 10 3 /dev/bm) */

/* ATI XL Inport Busmouse Definitions */

#define	ATIXL_MSE_DATA_PORT		0x23d
#define	ATIXL_MSE_SIGNATURE_PORT	0x23e
#define	ATIXL_MSE_CONTROL_PORT		0x23c

#define	ATIXL_MSE_READ_BUTTONS		0x00
#define	ATIXL_MSE_READ_X		0x01
#define	ATIXL_MSE_READ_Y		0x02

/* Some nice ATI XL macros */

/* Select IR7, HOLD UPDATES (INT ENABLED), save X,Y */
#define ATIXL_MSE_DISABLE_UPDATE() { outb( 0x07, ATIXL_MSE_CONTROL_PORT ); \
	outb( (0x20 | inb( ATIXL_MSE_DATA_PORT )), ATIXL_MSE_DATA_PORT ); }

/* Select IR7, Enable updates (INT ENABLED) */
#define ATIXL_MSE_ENABLE_UPDATE() { outb( 0x07, ATIXL_MSE_CONTROL_PORT ); \
	 outb( (0xdf & inb( ATIXL_MSE_DATA_PORT )), ATIXL_MSE_DATA_PORT ); }

/* Select IR7 - Mode Register, NO INTERRUPTS */
#define ATIXL_MSE_INT_OFF() { outb( 0x07, ATIXL_MSE_CONTROL_PORT ); \
	outb( (0xe7 & inb( ATIXL_MSE_DATA_PORT )), ATIXL_MSE_DATA_PORT ); }

/* Select IR7 - Mode Register, DATA INTERRUPTS ENABLED */
#define ATIXL_MSE_INT_ON() { outb( 0x07, ATIXL_MSE_CONTROL_PORT ); \
	outb( (0x08 | inb( ATIXL_MSE_DATA_PORT )), ATIXL_MSE_DATA_PORT ); }

/* Same general mouse structure */

static int msedev;

static void mouse_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	char dx, dy, buttons;

	ATIXL_MSE_DISABLE_UPDATE(); /* Note that interrupts are still enabled */
	outb(ATIXL_MSE_READ_X, ATIXL_MSE_CONTROL_PORT); /* Select IR1 - X movement */
	dx = inb( ATIXL_MSE_DATA_PORT);
	outb(ATIXL_MSE_READ_Y, ATIXL_MSE_CONTROL_PORT); /* Select IR2 - Y movement */
	dy = inb( ATIXL_MSE_DATA_PORT);
	outb(ATIXL_MSE_READ_BUTTONS, ATIXL_MSE_CONTROL_PORT); /* Select IR0 - Button Status */
	buttons = inb( ATIXL_MSE_DATA_PORT);
	busmouse_add_movementbuttons(msedev, dx, -dy, buttons);
	ATIXL_MSE_ENABLE_UPDATE();
}

static int release_mouse(struct inode * inode, struct file * file)
{
	ATIXL_MSE_INT_OFF(); /* Interrupts are really shut down here */
	free_irq(ATIXL_MOUSE_IRQ, NULL);
	return 0;
}

static int open_mouse(struct inode * inode, struct file * file)
{
	if (request_irq(ATIXL_MOUSE_IRQ, mouse_interrupt, 0, "ATIXL mouse", NULL))
		return -EBUSY;
	ATIXL_MSE_INT_ON(); /* Interrupts are really enabled here */
	return 0;
}

static struct busmouse atixlmouse = {
	ATIXL_BUSMOUSE, "atixl", THIS_MODULE, open_mouse, release_mouse, 0
};

static int __init atixl_busmouse_init(void)
{
	unsigned char a,b,c;

	/*
	 *	We must request the resource and claim it atomically
	 *	nowdays. We can throw it away on error. Otherwise we
	 *	may race another module load of the same I/O
	 */

	if (!request_region(ATIXL_MSE_DATA_PORT, 3, "atixlmouse"))
		return -EIO;

	a = inb( ATIXL_MSE_SIGNATURE_PORT );	/* Get signature */
	b = inb( ATIXL_MSE_SIGNATURE_PORT );
	c = inb( ATIXL_MSE_SIGNATURE_PORT );
	if (( a != b ) && ( a == c ))
		printk(KERN_INFO "\nATI Inport ");
	else
	{
		release_region(ATIXL_MSE_DATA_PORT,3);
		return -EIO;
	}
	outb(0x80, ATIXL_MSE_CONTROL_PORT);	/* Reset the Inport device */
	outb(0x07, ATIXL_MSE_CONTROL_PORT);	/* Select Internal Register 7 */
	outb(0x0a, ATIXL_MSE_DATA_PORT);	/* Data Interrupts 8+, 1=30hz, 2=50hz, 3=100hz, 4=200hz rate */

	msedev = register_busmouse(&atixlmouse);
	if (msedev < 0)
	{
		printk("Bus mouse initialisation error.\n");
		release_region(ATIXL_MSE_DATA_PORT,3);	/* Was missing */
	}
	else
		printk("Bus mouse detected and installed.\n");
	return msedev < 0 ? msedev : 0;
}

static void __exit atixl_cleanup (void)
{
	release_region(ATIXL_MSE_DATA_PORT, 3);
	unregister_busmouse(msedev);
}

module_init(atixl_busmouse_init);
module_exit(atixl_cleanup);

MODULE_LICENSE("GPL");
