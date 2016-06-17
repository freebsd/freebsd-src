/*
   -------------------------------------------------------------------------
   i2c-adap-ite.c i2c-hw access for the IIC peripheral on the ITE MIPS system
   -------------------------------------------------------------------------
   Hai-Pao Fan, MontaVista Software, Inc.
   hpfan@mvista.com or source@mvista.com

   Copyright 2001 MontaVista Software Inc.

   ----------------------------------------------------------------------------
   This file was highly leveraged from i2c-elektor.c, which was created
   by Simon G. Vogl and Hans Berglund:

 
     Copyright (C) 1995-97 Simon G. Vogl
                   1998-99 Hans Berglund

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.		     */
/* ------------------------------------------------------------------------- */

/* With some changes from Kyösti Mälkki <kmalkki@cc.hut.fi> and even
   Frodo Looijaard <frodol@dds.nl> */

#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-ite.h>
#include <linux/i2c-adap-ite.h>
#include "i2c-ite.h"

#define DEFAULT_BASE  0x14014030
#define ITE_IIC_IO_SIZE	0x40
#define DEFAULT_IRQ   0
#define DEFAULT_CLOCK 0x1b0e	/* default 16MHz/(27+14) = 400KHz */
#define DEFAULT_OWN   0x55

static int base  = 0;
static int irq   = 0;
static int clock = 0;
static int own   = 0;

static int i2c_debug=0;
static struct iic_ite gpi;
static wait_queue_head_t iic_wait;
static int iic_pending;

/* ----- global defines -----------------------------------------------	*/
#define DEB(x)	if (i2c_debug>=1) x
#define DEB2(x) if (i2c_debug>=2) x
#define DEB3(x) if (i2c_debug>=3) x
#define DEBE(x)	x	/* error messages 				*/


/* ----- local functions ----------------------------------------------	*/

static void iic_ite_setiic(void *data, int ctl, short val)
{
        unsigned long j = jiffies + 10;

	DEB3(printk(" Write 0x%02x to 0x%x\n",(unsigned short)val, ctl&0xff));
	DEB3({while (jiffies < j) schedule();}) 
	outw(val,ctl);
}

static short iic_ite_getiic(void *data, int ctl)
{
	short val;

	val = inw(ctl);
	DEB3(printk("Read 0x%02x from 0x%x\n",(unsigned short)val, ctl&0xff));  
	return (val);
}

/* Return our slave address.  This is the address
 * put on the I2C bus when another master on the bus wants to address us
 * as a slave
 */
static int iic_ite_getown(void *data)
{
	return (gpi.iic_own);
}


static int iic_ite_getclock(void *data)
{
	return (gpi.iic_clock);
}


#if 0
static void iic_ite_sleep(unsigned long timeout)
{
	schedule_timeout( timeout * HZ);
}
#endif


/* Put this process to sleep.  We will wake up when the
 * IIC controller interrupts.
 */
static void iic_ite_waitforpin(void) {

   int timeout = 2;

   /* If interrupts are enabled (which they are), then put the process to
    * sleep.  This process will be awakened by two events -- either the
    * the IIC peripheral interrupts or the timeout expires. 
    * If interrupts are not enabled then delay for a reasonable amount 
    * of time and return.
    */
   if (gpi.iic_irq > 0) {
	cli();
	if (iic_pending == 0) {
		interruptible_sleep_on_timeout(&iic_wait, timeout*HZ );
	} else
		iic_pending = 0;
	sti();
   } else {
      udelay(100);
   }
}


static void iic_ite_handler(int this_irq, void *dev_id, struct pt_regs *regs) 
{
	
   iic_pending = 1;

   DEB2(printk("iic_ite_handler: in interrupt handler\n"));
   wake_up_interruptible(&iic_wait);
}


/* Lock the region of memory where I/O registers exist.  Request our
 * interrupt line and register its associated handler.
 */
static int iic_hw_resrc_init(void)
{
  	if (check_region(gpi.iic_base, ITE_IIC_IO_SIZE) < 0 ) {
   	   return -ENODEV;
  	} else {
  	   request_region(gpi.iic_base, ITE_IIC_IO_SIZE, 
		"i2c (i2c bus adapter)");
  	}
	if (gpi.iic_irq > 0) {
	   if (request_irq(gpi.iic_irq, iic_ite_handler, 0, "ITE IIC", 0) < 0) {
	      gpi.iic_irq = 0;
	   } else
	      DEB3(printk("Enabled IIC IRQ %d\n", gpi.iic_irq));
	      enable_irq(gpi.iic_irq);
	}
	return 0;
}


static void iic_ite_release(void)
{
	if (gpi.iic_irq > 0) {
		disable_irq(gpi.iic_irq);
		free_irq(gpi.iic_irq, 0);
	}
	release_region(gpi.iic_base , 2);
}


static int iic_ite_reg(struct i2c_client *client)
{
	return 0;
}


static int iic_ite_unreg(struct i2c_client *client)
{
	return 0;
}


static void iic_ite_inc_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}


static void iic_ite_dec_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}


/* ------------------------------------------------------------------------
 * Encapsulate the above functions in the correct operations structure.
 * This is only done when more than one hardware adapter is supported.
 */
static struct i2c_algo_iic_data iic_ite_data = {
	NULL,
	iic_ite_setiic,
	iic_ite_getiic,
	iic_ite_getown,
	iic_ite_getclock,
	iic_ite_waitforpin,
	80, 80, 100,		/*	waits, timeout */
};

static struct i2c_adapter iic_ite_ops = {
	"ITE IIC adapter",
	I2C_HW_I_IIC,
	NULL,
	&iic_ite_data,
	iic_ite_inc_use,
	iic_ite_dec_use,
	iic_ite_reg,
	iic_ite_unreg,
};

/* Called when the module is loaded.  This function starts the
 * cascade of calls up through the hierarchy of i2c modules (i.e. up to the
 *  algorithm layer and into to the core layer)
 */
static int __init iic_ite_init(void) 
{

	struct iic_ite *piic = &gpi;

	printk(KERN_INFO "Initialize ITE IIC adapter module\n");
	if (base == 0)
		piic->iic_base = DEFAULT_BASE;
	else
		piic->iic_base = base;

	if (irq == 0)
		piic->iic_irq = DEFAULT_IRQ;
	else
		piic->iic_irq = irq;

	if (clock == 0)
		piic->iic_clock = DEFAULT_CLOCK;
	else
		piic->iic_clock = clock;

	if (own == 0)
		piic->iic_own = DEFAULT_OWN;
	else
		piic->iic_own = own;

	iic_ite_data.data = (void *)piic;
	if (iic_hw_resrc_init() == 0) {
		if (i2c_iic_add_bus(&iic_ite_ops) < 0)
			return -ENODEV;
	} else {
		return -ENODEV;
	}
	printk(KERN_INFO " found device at %#x irq %d.\n", 
		piic->iic_base, piic->iic_irq);
	return 0;
}


static void iic_ite_exit(void)
{
	i2c_iic_del_bus(&iic_ite_ops);
        iic_ite_release();
}

EXPORT_NO_SYMBOLS;

/* If modules is NOT defined when this file is compiled, then the MODULE_*
 * macros will resolve to nothing
 */
MODULE_AUTHOR("MontaVista Software <www.mvista.com>");
MODULE_DESCRIPTION("I2C-Bus adapter routines for ITE IIC bus adapter");
MODULE_LICENSE("GPL");

MODULE_PARM(base, "i");
MODULE_PARM(irq, "i");
MODULE_PARM(clock, "i");
MODULE_PARM(own, "i");
MODULE_PARM(i2c_debug,"i");


/* Called when module is loaded or when kernel is intialized.
 * If MODULES is defined when this file is compiled, then this function will
 * resolve to init_module (the function called when insmod is invoked for a
 * module).  Otherwise, this function is called early in the boot, when the
 * kernel is intialized.  Check out /include/init.h to see how this works.
 */
module_init(iic_ite_init);

/* Resolves to module_cleanup when MODULES is defined. */
module_exit(iic_ite_exit); 
