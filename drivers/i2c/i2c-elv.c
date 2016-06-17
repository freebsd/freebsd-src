/* ------------------------------------------------------------------------- */
/* i2c-elv.c i2c-hw access for philips style parallel port adapters	     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 1995-2000 Simon G. Vogl

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

/* $Id: i2c-elv.c,v 1.17 2001/07/29 02:44:25 mds Exp $ */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/init.h>

#include <asm/uaccess.h>

#include <linux/ioport.h>
#include <asm/io.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

#define DEFAULT_BASE 0x378
static int base=0;
static unsigned char PortData = 0;

/* ----- global defines -----------------------------------------------	*/
#define DEB(x)		/* should be reasonable open, close &c. 	*/
#define DEB2(x) 	/* low level debugging - very slow 		*/
#define DEBE(x)	x	/* error messages 				*/
#define DEBINIT(x) x	/* detection status messages			*/

/* --- Convenience defines for the parallel port:			*/
#define BASE	(unsigned int)(data)
#define DATA	BASE			/* Centronics data port		*/
#define STAT	(BASE+1)		/* Centronics status port	*/
#define CTRL	(BASE+2)		/* Centronics control port	*/


/* ----- local functions ----------------------------------------------	*/


static void bit_elv_setscl(void *data, int state)
{
	if (state) {
		PortData &= 0xfe;
	} else {
		PortData |=1;
	}
	outb(PortData, DATA);
}

static void bit_elv_setsda(void *data, int state)
{
	if (state) {
		PortData &=0xfd;
	} else {
		PortData |=2;
	}
	outb(PortData, DATA);
} 

static int bit_elv_getscl(void *data)
{
	return ( 0 == ( (inb_p(STAT)) & 0x08 ) );
}

static int bit_elv_getsda(void *data)
{
	return ( 0 == ( (inb_p(STAT)) & 0x40 ) );
}

static int bit_elv_init(void)
{
	if (check_region(base,(base == 0x3bc)? 3 : 8) < 0 ) {
		return -ENODEV;	
	} else {
						/* test for ELV adap. 	*/
		if (inb(base+1) & 0x80) {	/* BUSY should be high	*/
			DEBINIT(printk(KERN_DEBUG "i2c-elv.o: Busy was low.\n"));
			return -ENODEV;
		} else {
			outb(0x0c,base+2);	/* SLCT auf low		*/
			udelay(400);
			if ( !(inb(base+1) && 0x10) ) {
				outb(0x04,base+2);
				DEBINIT(printk(KERN_DEBUG "i2c-elv.o: Select was high.\n"));
				return -ENODEV;
			}
		}
		request_region(base,(base == 0x3bc)? 3 : 8,
			"i2c (ELV adapter)");
		PortData = 0;
		bit_elv_setsda((void*)base,1);
		bit_elv_setscl((void*)base,1);
	}
	return 0;
}

static void __exit bit_elv_exit(void)
{
	release_region( base , (base == 0x3bc)? 3 : 8 );
}

static int bit_elv_reg(struct i2c_client *client)
{
	return 0;
}

static int bit_elv_unreg(struct i2c_client *client)
{
	return 0;
}

static void bit_elv_inc_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

static void bit_elv_dec_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

/* ------------------------------------------------------------------------
 * Encapsulate the above functions in the correct operations structure.
 * This is only done when more than one hardware adapter is supported.
 */
static struct i2c_algo_bit_data bit_elv_data = {
	NULL,
	bit_elv_setsda,
	bit_elv_setscl,
	bit_elv_getsda,
	bit_elv_getscl,
	80, 80, 100,		/*	waits, timeout */
};

static struct i2c_adapter bit_elv_ops = {
	"ELV Parallel port adaptor",
	I2C_HW_B_ELV,
	NULL,
	&bit_elv_data,
	bit_elv_inc_use,
	bit_elv_dec_use,
	bit_elv_reg,
	bit_elv_unreg,	
};

int __init i2c_bitelv_init(void)
{
	printk(KERN_INFO "i2c-elv.o: i2c ELV parallel port adapter module version %s (%s)\n", I2C_VERSION, I2C_DATE);
	if (base==0) {
		/* probe some values */
		base=DEFAULT_BASE;
		bit_elv_data.data=(void*)DEFAULT_BASE;
		if (bit_elv_init()==0) {
			if(i2c_bit_add_bus(&bit_elv_ops) < 0)
				return -ENODEV;
		} else {
			return -ENODEV;
		}
	} else {
		bit_elv_ops.data=(void*)base;
		if (bit_elv_init()==0) {
			if(i2c_bit_add_bus(&bit_elv_ops) < 0)
				return -ENODEV;
		} else {
			return -ENODEV;
		}
	}
	printk(KERN_DEBUG "i2c-elv.o: found device at %#x.\n",base);
	return 0;
}


EXPORT_NO_SYMBOLS;

#ifdef MODULE
MODULE_AUTHOR("Simon G. Vogl <simon@tk.uni-linz.ac.at>");
MODULE_DESCRIPTION("I2C-Bus adapter routines for ELV parallel port adapter");
MODULE_LICENSE("GPL");

MODULE_PARM(base, "i");

int init_module(void)
{
	return i2c_bitelv_init();
}

void cleanup_module(void)
{
	i2c_bit_del_bus(&bit_elv_ops);
	bit_elv_exit();
}

#endif
