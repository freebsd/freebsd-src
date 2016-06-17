/* ------------------------------------------------------------------------- */
/* i2c-philips-par.c i2c-hw access for philips style parallel port adapters  */
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

/* $Id: i2c-philips-par.c,v 1.18 2000/07/06 19:21:49 frodo Exp $ */

#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/stddef.h>
#include <linux/parport.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

#ifndef __exit
#define __exit __init
#endif

static int type;

struct i2c_par
{
	struct pardevice *pdev;
	struct i2c_adapter adapter;
	struct i2c_algo_bit_data bit_lp_data;
	struct i2c_par *next;
};

static struct i2c_par *adapter_list;


/* ----- global defines -----------------------------------------------	*/
#define DEB(x)		/* should be reasonable open, close &c. 	*/
#define DEB2(x) 	/* low level debugging - very slow 		*/
#define DEBE(x)	x	/* error messages 				*/

/* ----- printer port defines ------------------------------------------*/
					/* Pin Port  Inverted	name	*/
#define I2C_ON		0x20		/* 12 status N	paper		*/
					/* ... only for phil. not used  */
#define I2C_SDA		0x80		/*  9 data   N	data7		*/
#define I2C_SCL		0x08		/* 17 ctrl   N	dsel		*/

#define I2C_SDAIN	0x80		/* 11 stat   Y	busy		*/
#define I2C_SCLIN	0x08		/* 15 stat   Y	enable		*/

#define I2C_DMASK	0x7f
#define I2C_CMASK	0xf7

/* ----- local functions ----------------------------------------------	*/

static void bit_lp_setscl(void *data, int state)
{
	/*be cautious about state of the control register - 
		touch only the one bit needed*/
	if (state) {
		parport_write_control((struct parport *) data,
		      parport_read_control((struct parport *) data)|I2C_SCL);
	} else {
		parport_write_control((struct parport *) data,
		      parport_read_control((struct parport *) data)&I2C_CMASK);
	}
}

static void bit_lp_setsda(void *data, int state)
{
	if (state) {
		parport_write_data((struct parport *) data, I2C_DMASK);
	} else {
		parport_write_data((struct parport *) data, I2C_SDA);
	}
}

static int bit_lp_getscl(void *data)
{
	return parport_read_status((struct parport *) data) & I2C_SCLIN;
}

static int bit_lp_getsda(void *data)
{
	return parport_read_status((struct parport *) data) & I2C_SDAIN;
}

static void bit_lp_setscl2(void *data, int state)
{
	if (state) {
		parport_write_data((struct parport *) data,
		      parport_read_data((struct parport *) data)|0x1);
	} else {
		parport_write_data((struct parport *) data,
		      parport_read_data((struct parport *) data)&0xfe);
	}
}

static void bit_lp_setsda2(void *data, int state)
{
	if (state) {
		parport_write_data((struct parport *) data,
		      parport_read_data((struct parport *) data)|0x2);
	} else {
		parport_write_data((struct parport *) data,
		      parport_read_data((struct parport *) data)&0xfd);
	}
}

static int bit_lp_getsda2(void *data)
{
	return (parport_read_status((struct parport *) data) & 
			             PARPORT_STATUS_BUSY) ? 0 : 1;
}

static int bit_lp_reg(struct i2c_client *client)
{
	return 0;
}

static int bit_lp_unreg(struct i2c_client *client)
{
	return 0;
}

static void bit_lp_inc_use(struct i2c_adapter *adap)
{
	MOD_INC_USE_COUNT;
}

static void bit_lp_dec_use(struct i2c_adapter *adap)
{
	MOD_DEC_USE_COUNT;
}

/* ------------------------------------------------------------------------
 * Encapsulate the above functions in the correct operations structure.
 * This is only done when more than one hardware adapter is supported.
 */
 
static struct i2c_algo_bit_data bit_lp_data = {
	NULL,
	bit_lp_setsda,
	bit_lp_setscl,
	bit_lp_getsda,
	bit_lp_getscl,
	80, 80, 100,		/*	waits, timeout */
}; 

static struct i2c_algo_bit_data bit_lp_data2 = {
	NULL,
	bit_lp_setsda2,
	bit_lp_setscl2,
	bit_lp_getsda2,
	NULL,
	80, 80, 100,		/*	waits, timeout */
}; 

static struct i2c_adapter bit_lp_ops = {
	"Philips Parallel port adapter",
	I2C_HW_B_LP,
	NULL,
	NULL,
	bit_lp_inc_use,
	bit_lp_dec_use,
	bit_lp_reg,

	bit_lp_unreg,
};

static void i2c_parport_attach (struct parport *port)
{
	struct i2c_par *adapter = kmalloc(sizeof(struct i2c_par),
					  GFP_KERNEL);
	if (!adapter) {
		printk(KERN_ERR "i2c-philips-par: Unable to malloc.\n");
		return;
	}

	printk(KERN_DEBUG "i2c-philips-par.o: attaching to %s\n", port->name);

	adapter->pdev = parport_register_device(port, "i2c-philips-par",
						NULL, NULL, NULL, 
						PARPORT_FLAG_EXCL,
						NULL);
	if (!adapter->pdev) {
		printk(KERN_ERR "i2c-philips-par: Unable to register with parport.\n");
		return;
	}

	adapter->adapter = bit_lp_ops;
	adapter->adapter.algo_data = &adapter->bit_lp_data;
	adapter->bit_lp_data = type ? bit_lp_data2 : bit_lp_data;
	adapter->bit_lp_data.data = port;

	/* reset hardware to sane state */
	parport_claim_or_block(adapter->pdev);
	adapter->bit_lp_data.setsda(port, 1);
	adapter->bit_lp_data.setscl(port, 1);
	parport_release(adapter->pdev);

	if (i2c_bit_add_bus(&adapter->adapter) < 0)
	{
		printk(KERN_ERR "i2c-philips-par: Unable to register with I2C.\n");
		parport_unregister_device(adapter->pdev);
		kfree(adapter);
		return;		/* No good */
	}

	adapter->next = adapter_list;
	adapter_list = adapter;
}

static void i2c_parport_detach (struct parport *port)
{
	struct i2c_par *adapter, *prev = NULL;

	for (adapter = adapter_list; adapter; adapter = adapter->next)
	{
		if (adapter->pdev->port == port)
		{
			parport_unregister_device(adapter->pdev);
			i2c_bit_del_bus(&adapter->adapter);
			if (prev)
				prev->next = adapter->next;
			else
				adapter_list = adapter->next;
			kfree(adapter);
			return;
		}
		prev = adapter;
	}
}


static struct parport_driver i2c_driver = {
	"i2c-philips-par",
	i2c_parport_attach,
	i2c_parport_detach,
	NULL
};

int __init i2c_bitlp_init(void)
{
	printk(KERN_INFO "i2c-philips-par.o: i2c Philips parallel port adapter module version %s (%s)\n", I2C_VERSION, I2C_DATE);

	parport_register_driver(&i2c_driver);
	
	return 0;
}

void __exit i2c_bitlp_exit(void)
{
	parport_unregister_driver(&i2c_driver);
}

EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("Simon G. Vogl <simon@tk.uni-linz.ac.at>");
MODULE_DESCRIPTION("I2C-Bus adapter routines for Philips parallel port adapter");
MODULE_LICENSE("GPL");

MODULE_PARM(type, "i");

#ifdef MODULE
int init_module(void)
{
	return i2c_bitlp_init();
}

void cleanup_module(void)
{
	i2c_bitlp_exit();
}
#endif
