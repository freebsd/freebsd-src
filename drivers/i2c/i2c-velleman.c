/* ------------------------------------------------------------------------- */
/* i2c-velleman.c i2c-hw access for Velleman K8000 adapters		     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 1995-96, 2000 Simon G. Vogl

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

/* $Id: i2c-velleman.c,v 1.19 2000/01/24 02:06:33 mds Exp $ */

#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/string.h>  /* for 2.0 kernels to get NULL   */
#include <asm/errno.h>     /* for 2.0 kernels to get ENODEV */
#include <asm/io.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

/* ----- global defines -----------------------------------------------	*/
#define DEB(x)		/* should be reasonable open, close &c. 	*/
#define DEB2(x) 	/* low level debugging - very slow 		*/
#define DEBE(x)	x	/* error messages 				*/

					/* Pin Port  Inverted	name	*/
#define I2C_SDA		0x02		/*  ctrl bit 1 	(inv)	*/
#define I2C_SCL		0x08		/*  ctrl bit 3 	(inv)	*/

#define I2C_SDAIN	0x10		/* stat bit 4		*/
#define I2C_SCLIN	0x08		/* ctrl bit 3 (inv)(reads own output)*/

#define I2C_DMASK	0xfd
#define I2C_CMASK	0xf7


/* --- Convenience defines for the parallel port:			*/
#define BASE	(unsigned int)(data)
#define DATA	BASE			/* Centronics data port		*/
#define STAT	(BASE+1)		/* Centronics status port	*/
#define CTRL	(BASE+2)		/* Centronics control port	*/

#define DEFAULT_BASE 0x378
static int base=0;

/* ----- local functions --------------------------------------------------- */

static void bit_velle_setscl(void *data, int state)
{
	if (state) {
		outb(inb(CTRL) & I2C_CMASK,   CTRL);
	} else {
		outb(inb(CTRL) | I2C_SCL, CTRL);
	}
	
}

static void bit_velle_setsda(void *data, int state)
{
	if (state) {
		outb(inb(CTRL) & I2C_DMASK , CTRL);
	} else {
		outb(inb(CTRL) | I2C_SDA, CTRL);
	}
	
} 

static int bit_velle_getscl(void *data)
{
	return ( 0 == ( (inb(CTRL)) & I2C_SCLIN ) );
}

static int bit_velle_getsda(void *data)
{
	return ( 0 != ( (inb(STAT)) & I2C_SDAIN ) );
}

static int bit_velle_init(void)
{
	if (check_region(base,(base == 0x3bc)? 3 : 8) < 0 ) {
		DEBE(printk("i2c-velleman.o: Port %#x already in use.\n",
		     base));
		return -ENODEV;
	} else {
		request_region(base, (base == 0x3bc)? 3 : 8, 
			"i2c (Vellemann adapter)");
		bit_velle_setsda((void*)base,1);
		bit_velle_setscl((void*)base,1);
	}
	return 0;
}

static void __exit bit_velle_exit(void)
{	
	release_region( base , (base == 0x3bc)? 3 : 8 );
}


static int bit_velle_reg(struct i2c_client *client)
{
	return 0;
}

static int bit_velle_unreg(struct i2c_client *client)
{
	return 0;
}

static void bit_velle_inc_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

static void bit_velle_dec_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

/* ------------------------------------------------------------------------
 * Encapsulate the above functions in the correct operations structure.
 * This is only done when more than one hardware adapter is supported.
 */

static struct i2c_algo_bit_data bit_velle_data = {
	NULL,
	bit_velle_setsda,
	bit_velle_setscl,
	bit_velle_getsda,
	bit_velle_getscl,
	10, 10, 100,		/*	waits, timeout */
};

static struct i2c_adapter bit_velle_ops = {
	"Velleman K8000",
	I2C_HW_B_VELLE,
	NULL,
	&bit_velle_data,
	bit_velle_inc_use,
	bit_velle_dec_use,
	bit_velle_reg,
	bit_velle_unreg,
};

int __init  i2c_bitvelle_init(void)
{
	printk(KERN_INFO "i2c-velleman.o: i2c Velleman K8000 adapter module version %s (%s)\n", I2C_VERSION, I2C_DATE);
	if (base==0) {
		/* probe some values */
		base=DEFAULT_BASE;
		bit_velle_data.data=(void*)DEFAULT_BASE;
		if (bit_velle_init()==0) {
			if(i2c_bit_add_bus(&bit_velle_ops) < 0)
				return -ENODEV;
		} else {
			return -ENODEV;
		}
	} else {
		bit_velle_data.data=(void*)base;
		if (bit_velle_init()==0) {
			if(i2c_bit_add_bus(&bit_velle_ops) < 0)
				return -ENODEV;
		} else {
			return -ENODEV;
		}
	}
	printk(KERN_DEBUG "i2c-velleman.o: found device at %#x.\n",base);
	return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE
MODULE_AUTHOR("Simon G. Vogl <simon@tk.uni-linz.ac.at>");
MODULE_DESCRIPTION("I2C-Bus adapter routines for Velleman K8000 adapter");
MODULE_LICENSE("GPL");

MODULE_PARM(base, "i");

int init_module(void) 
{
	return i2c_bitvelle_init();
}

void cleanup_module(void) 
{
	i2c_bit_del_bus(&bit_velle_ops);
	bit_velle_exit();
}

#endif
