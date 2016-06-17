/* obmouse.c -- HP omnibook 600C/CT pop-up mouse driver
 *
 *  Copyright (C) 1999 Olivier Florent
 *  Copyright (C) 1999 Chuck Slivkoff <chuck_slivkoff_AT_hp.com>
 *  Copyright (C) 1999-2004 Grant Grundler <grundler_at_parisc-linux.org>
 *
 * OB600C/CT mouse can be compared to a tablet, as absolute coordinates
 * are given by the hardware.  This driver emulates a basic serial mouse
 * protocol by translating absolute coordinates to relative moves.
 * This works with gpm -t pnp and Xfree86 as a standard Micro$oft mouse.
 *
 * FIXME:  This driver lacks a detection routine.
 *         i.e you must know that you have a 600C/CT before using it.
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *  
 *
 * 0.7	January 2004      grant
 *      convert to /dev/input
 *
 * 0.6	2 January 2004      grant
 *	converted to busmouse
 *
 * 0.5	27 december 2003    grant
 *	fix "built-in" code - added module_* calls
 *
 * 0.4	27 december 2002 grant grundler
 *	Add MODULES_ stuff (author, license, etc)
 *	fix read to return 0 if no change.
 *
 * 0.3	26 november 1999 grant grundler
 *	removed ifdefs for EMULATE_SERIAL_MOUSE  and EMULATE_NCR_PEN.
 *	Only support "basic serial mouse protocol (gpm -t pnp)" which
 *	is also supported directly by Xfree.
 *
 *	Moved delta(x,y) calculations into ob_interrupt.
 *	If we only get interrupts when the mouse moves,
 *	then only need to update delta(x,y) then too.
 *
 * 0.2	22 november 1999, grant grundler
 *	"man 4 mouse" explains really well how PNP mouse works. Read it.
 *
 *	Fixed algorithm which generated delta(x,y) from current-last
 *	position reported. Now handles "roll-over" properly and
 *	speed scales _inversely_ (ie bigger number is slower).
 *	"speed" parameter can be set with "insmod obmouse speed=5" (default).
 *
 * 0.1a	16 november 1999, charles_slivkoff_at_hp.com
 *	File did not compile as received.
 *	Modified ob_write,ob_read parameters to match 2.2.x kernel.
 *	Added uaccess.h. Tried to fix ob_interrupt.
 *
 * 0.1	17 february 1999, olivier_florent_AT_hp.com
 *	original author.
 */

#include <linux/module.h>

#include <linux/errno.h>
#include <linux/kmod.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/ioport.h>

#include <asm/io.h>
#include <asm/irq.h>

#undef OBMOUSE_DEBUG		/* define to enable lots of printf */

#define OBMOUSE_NAME     "HP omnibook 600C/CT pop-up mouse"
#define OBMOUSE_DEV_NAME "obmouse"
#define OBMOUSE_VERSION  "v0.7"


/*
** "speed" of the mouse. Determines how "fast" the mouse "moves".
** Similar (but not the same) as acceleration. Using slower acceleration
** together with lower speed value might result in better mouse control.
** I've only used the default acceleration.
*/
int speed = 4;	/* my personal preference */

/*
** OB 600 mouse doesn't completely act like a tablet.
** The xy-coordinates do in fact roll over which would
** not happen on a tablet.
*/
#define OB_ROLL_LIMIT 0xd00

/*
** obmouse HW specific data
*/
#define OBMOUSE_BASE           (0x238)  /* base address of the hardware */
#define OBMOUSE_EXTENT         (3)      /* from 0x238 to 0x23b          */
#define OBMOUSE_IRQ            (12)     /* irq 12                       */

/* The 4 high bits of OBMOUSE_INTR_CTL */
#define OBMOUSE_BTN_IRQ_MASK   (0x10)    /* WR: enable/disable button irq   */
#define OBMOUSE_MOV_IRQ_MASK   (0x20)    /* WR: enable/disable movement irq */
#define OBMOUSE_BUTTON1_MASK   (0x40)    /* RD: status of button 1 */
#define OBMOUSE_BUTTON2_MASK   (0x80)    /* RD: status of button 2 */

#define OBMOUSE_COORD_ONLY(v)  ((v) & 0xfff)  /* ignore high 4 bits */

#define OBMOUSE_INTR_CTL   (OBMOUSE_BASE+3)
#define OBMOUSE_INTR_BITS  (OBMOUSE_BTN_IRQ_MASK | OBMOUSE_MOV_IRQ_MASK)

/* Enable/disable both button and movement interrupt */
#define OBMOUSE_ENABLE_INTR()  outb(OBMOUSE_INTR_BITS,  OBMOUSE_INTR_CTL)
#define OBMOUSE_DISABLE_INTR() outb(~OBMOUSE_INTR_BITS, OBMOUSE_INTR_CTL)

/* Amount mouse has to move before the hardware launchs a new
** interrupt (I think). 0x08 is the standard value.
** Write value to OBMOUSE_BASE each time an interrupt is handled to
** enable the next one.
*/
#define OBMOUSE_SENSITIVITY    (0x08)

/* reset the sensitivity to enable next interrupt */
#define OBMOUSE_ENABLE_SENSE() outb(OBMOUSE_SENSITIVITY,OBMOUSE_BASE) 
#define OBMOUSE_DISABLE_SENSE() outb(0,OBMOUSE_BASE) 


static unsigned short lastx;	/* last reported normalized coords */
static unsigned short lasty;
static unsigned char ob_opened = 0; /* 0=closed, 1=opened  */
static struct input_dev obdev;


/*
** Omnibook 600 mouse ISR.
** Read the HW state and resets "SENSITIVITY" in order to re-arm
** the interrupt.
*/
static void
ob_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	short dx, dy;
	unsigned short rawx, rawy;

	rawx = inb(OBMOUSE_BASE+0) + (inb(OBMOUSE_BASE+1) << 8);
	rawy = inb(OBMOUSE_BASE+2) + (inb(OBMOUSE_BASE+3) << 8);

#ifdef OBMOUSE_DEBUG
/* This printk is really useful for learning how the mouse actually behaves. */
printk("ob_intr: %4x,%4x\n", rawx, rawy);
#endif

	/* reset the sensitivity */
	OBMOUSE_ENABLE_SENSE();

	/* ------------------------------------
	**  update delta(x,y) values.
	** ------------------------------------
	*/
	dx = (short) rawx - (short) lastx ;
	lastx = rawx;

	{
		register unsigned short coordY = OBMOUSE_COORD_ONLY(rawy);
		dy = (short) coordY - (short) lasty ;
		lasty = coordY;
	}

	/*
	** determine if the reading "rolled" over.
	** Not fool-proof but should be good enough.
	*/
	if (dx > OB_ROLL_LIMIT) {
		/* 0xf80 - 0x80 = 0xf00 (and we want 0x100) */
		dx = 0x1000 - dx;
	} else if (-dx > OB_ROLL_LIMIT) {
		/*
		** 0x80 - 0xf80 = -0xf00 (and we want -0x100)
		** -0x1000 - (-0xf00) = -0x1000 + 0xf00 = -0x100
		*/
		dx = -0x1000 - dx;
	}

	/* Same story with the Y-coordinate */	
	if      ( dy > OB_ROLL_LIMIT)  dy =  0x1000 - dy;
	else if (-dy > OB_ROLL_LIMIT)  dy = -0x1000 - dy;

	dx /= speed;	/* scale */
	dy /= speed;

	rawy = ~rawy;	/* invert mouse buttons */
	input_report_key(&obdev, BTN_LEFT, (rawy & 0x8000));
	input_report_key(&obdev, BTN_RIGHT, (rawy & 0x4000));

	/* obmouse acts like a table *EXCEPT* for the "roll-over". */
	if (dx) input_report_rel(&obdev, REL_X, -dx);
	if (dy) input_report_rel(&obdev, REL_Y, dy);
}


static int ob_open(struct input_dev *dev)
{
	/* device is already opened */
	if (ob_opened)
		return -EBUSY;

#ifdef OBMOUSE_DEBUG
printk(OBMOUSE_DEV_NAME ": attempt request irq %d\n",OBMOUSE_IRQ);
#endif

	/* Try to get the interrupt */
	if (request_irq(OBMOUSE_IRQ,ob_interrupt,0,OBMOUSE_DEV_NAME,NULL)) {
		printk (OBMOUSE_DEV_NAME ": request_irq failed for %d\n",OBMOUSE_IRQ);
		return -EBUSY;
	}

#ifdef OBMOUSE_DEBUG
printk(OBMOUSE_DEV_NAME ": irq %d registered\n",OBMOUSE_IRQ);
#endif

	OBMOUSE_ENABLE_INTR() ;
	OBMOUSE_ENABLE_SENSE() ;

	MOD_INC_USE_COUNT;
	ob_opened = 1 ;
	return 0;
}


void ob_close(struct input_dev *dev)
{
	/* device has never been opened */
	if (!ob_opened)
		return;

	OBMOUSE_DISABLE_INTR() ;
	free_irq(OBMOUSE_IRQ, NULL);

	MOD_DEC_USE_COUNT;
	ob_opened = 0 ;
}


#ifndef MODULE
static int __init obmouse_setup(char *str)
{
	int ints[4];

	str = get_options(str, ARRAY_SIZE(ints), ints);
	if (ints[0] > 0)
		speed=ints[1];

	return 1;
}

__setup("speed=", obmouse_setup);
#endif /* !MODULE */


static int obmouse_init(void)
{
	/* Get the IO Port region first */
	if (request_region(OBMOUSE_BASE,OBMOUSE_EXTENT,OBMOUSE_DEV_NAME) < 0) {
		printk(KERN_ERR OBMOUSE_DEV_NAME ": IO Port 0x%d not available!\n", OBMOUSE_BASE);
		return -ENODEV;

	}

	OBMOUSE_DISABLE_INTR() ;
	OBMOUSE_DISABLE_SENSE() ;

	memset(&obdev, 0, sizeof(obdev));

	obdev.name = OBMOUSE_NAME " " OBMOUSE_VERSION;
	obdev.idbus = BUS_ISA;
	obdev.idvendor = 0x103c;	/* PCI_VENDOR_ID_HP */
	obdev.idproduct = 0x0001;
	obdev.idversion = 0x0100;

	obdev.evbit[0] = BIT(EV_KEY) | BIT(EV_REL);
	obdev.keybit[LONG(BTN_LEFT)] = BIT(BTN_LEFT) | BIT(BTN_RIGHT);
	obdev.relbit[0] = BIT(REL_X) | BIT(REL_Y);
	obdev.open  = ob_open;
	obdev.close = ob_close;

	input_register_device(&obdev);
	printk( OBMOUSE_NAME " " OBMOUSE_VERSION
		" (0x%x, IRQ %d), Grant Grundler\n", OBMOUSE_BASE, OBMOUSE_IRQ);

	return 0;
}

static void obmouse_exit(void)
{
	OBMOUSE_DISABLE_INTR() ;
	OBMOUSE_DISABLE_SENSE() ;

	input_unregister_device(&obdev);
	release_region(OBMOUSE_BASE,OBMOUSE_EXTENT);
	printk(OBMOUSE_DEV_NAME ": closed\n");
}

module_init(obmouse_init);
module_exit(obmouse_exit);

EXPORT_NO_SYMBOLS;
 
MODULE_AUTHOR("Grant Grundler <grundler_at_parisc-linux.org>");
MODULE_DESCRIPTION(OBMOUSE_NAME);
MODULE_PARM(speed, "i");
MODULE_PARM_DESC(speed, "obmouse speed (not accel) control");
MODULE_LICENSE("GPL");
