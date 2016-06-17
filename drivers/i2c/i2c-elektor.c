/* ------------------------------------------------------------------------- */
/* i2c-elektor.c i2c-hw access for PCF8584 style isa bus adaptes             */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 1995-97 Simon G. Vogl
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

/* Partialy rewriten by Oleg I. Vdovikin for mmapped support of 
   for Alpha Processor Inc. UP-2000(+) boards */

#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-pcf.h>
#include <linux/i2c-elektor.h>
#include "i2c-pcf8584.h"

#define DEFAULT_BASE 0x330

static int base   = 0;
static int irq    = 0;
static int clock  = 0x1c;
static int own    = 0x55;
static int mmapped = 0;
static int i2c_debug = 0;

/* vdovikin: removed static struct i2c_pcf_isa gpi; code - 
  this module in real supports only one device, due to missing arguments
  in some functions, called from the algo-pcf module. Sometimes it's
  need to be rewriten - but for now just remove this for simpler reading */

static wait_queue_head_t pcf_wait;
static int pcf_pending;

/* ----- global defines -----------------------------------------------	*/
#define DEB(x)	if (i2c_debug>=1) x
#define DEB2(x) if (i2c_debug>=2) x
#define DEB3(x) if (i2c_debug>=3) x
#define DEBE(x)	x	/* error messages 				*/

/* ----- local functions ----------------------------------------------	*/

static void pcf_isa_setbyte(void *data, int ctl, int val)
{
	int address = ctl ? (base + 1) : base;

	if (ctl && irq) {
		val |= I2C_PCF_ENI;
	}

	DEB3(printk(KERN_DEBUG "i2c-elektor.o: Write 0x%X 0x%02X\n", address, val & 255));

	switch (mmapped) {
	case 0: /* regular I/O */
		outb(val, address);
		break;
	case 2: /* double mapped I/O needed for UP2000 board,
                   I don't know why this... */
		writeb(val, address);
		/* fall */
	case 1: /* memory mapped I/O */
		writeb(val, address);
		break;
	}
}

static int pcf_isa_getbyte(void *data, int ctl)
{
	int address = ctl ? (base + 1) : base;
	int val = mmapped ? readb(address) : inb(address);

	DEB3(printk(KERN_DEBUG "i2c-elektor.o: Read 0x%X 0x%02X\n", address, val));

	return (val);
}

static int pcf_isa_getown(void *data)
{
	return (own);
}


static int pcf_isa_getclock(void *data)
{
	return (clock);
}

static void pcf_isa_waitforpin(void) {

	int timeout = 2;

	if (irq > 0) {
		cli();
		if (pcf_pending == 0) {
			interruptible_sleep_on_timeout(&pcf_wait, timeout*HZ );
		} else
			pcf_pending = 0;
		sti();
	} else {
		udelay(100);
	}
}


static void pcf_isa_handler(int this_irq, void *dev_id, struct pt_regs *regs) {
	pcf_pending = 1;
	wake_up_interruptible(&pcf_wait);
}


static int pcf_isa_init(void)
{
	if (!mmapped) {
		if (check_region(base, 2) < 0 ) {
			printk(KERN_ERR
			       "i2c-elektor.o: requested I/O region (0x%X:2) "
			       "is in use.\n", base);
			return -ENODEV;
		} else {
			request_region(base, 2, "i2c (isa bus adapter)");
		}
	}
	if (irq > 0) {
		if (request_irq(irq, pcf_isa_handler, 0, "PCF8584", 0) < 0) {
			printk(KERN_ERR "i2c-elektor.o: Request irq%d failed\n", irq);
			irq = 0;
		} else
			enable_irq(irq);
	}
	return 0;
}


static void __exit pcf_isa_exit(void)
{
	if (irq > 0) {
		disable_irq(irq);
		free_irq(irq, 0);
	}
	if (!mmapped) {
		release_region(base , 2);
	}
}


static int pcf_isa_reg(struct i2c_client *client)
{
	return 0;
}


static int pcf_isa_unreg(struct i2c_client *client)
{
	return 0;
}

static void pcf_isa_inc_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

static void pcf_isa_dec_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}


/* ------------------------------------------------------------------------
 * Encapsulate the above functions in the correct operations structure.
 * This is only done when more than one hardware adapter is supported.
 */
static struct i2c_algo_pcf_data pcf_isa_data = {
	NULL,
	pcf_isa_setbyte,
	pcf_isa_getbyte,
	pcf_isa_getown,
	pcf_isa_getclock,
	pcf_isa_waitforpin,
	10, 10, 100,		/*	waits, timeout */
};

static struct i2c_adapter pcf_isa_ops = {
	"PCF8584 ISA adapter",
	I2C_HW_P_ELEK,
	NULL,
	&pcf_isa_data,
	pcf_isa_inc_use,
	pcf_isa_dec_use,
	pcf_isa_reg,
	pcf_isa_unreg,
};

int __init i2c_pcfisa_init(void) 
{
#ifdef __alpha__
	/* check to see we have memory mapped PCF8584 connected to the 
	Cypress cy82c693 PCI-ISA bridge as on UP2000 board */
	if ((base == 0) && pci_present()) {
		
		struct pci_dev *cy693_dev =
                    pci_find_device(PCI_VENDOR_ID_CONTAQ, 
		                    PCI_DEVICE_ID_CONTAQ_82C693, NULL);

		if (cy693_dev) {
			char config;
			/* yeap, we've found cypress, let's check config */
			if (!pci_read_config_byte(cy693_dev, 0x47, &config)) {
				
				DEB3(printk(KERN_DEBUG "i2c-elektor.o: found cy82c693, config register 0x47 = 0x%02x.\n", config));

				/* UP2000 board has this register set to 0xe1,
                                   but the most significant bit as seems can be 
				   reset during the proper initialisation
                                   sequence if guys from API decides to do that
                                   (so, we can even enable Tsunami Pchip
                                   window for the upper 1 Gb) */

				/* so just check for ROMCS at 0xe0000,
                                   ROMCS enabled for writes
				   and external XD Bus buffer in use. */
				if ((config & 0x7f) == 0x61) {
					/* seems to be UP2000 like board */
					base = 0xe0000;
                                        /* I don't know why we need to
                                           write twice */
					mmapped = 2;
                                        /* UP2000 drives ISA with
					   8.25 MHz (PCI/4) clock
					   (this can be read from cypress) */
					clock = I2C_PCF_CLK | I2C_PCF_TRNS90;
					printk(KERN_INFO "i2c-elektor.o: found API UP2000 like board, will probe PCF8584 later.\n");
				}
			}
		}
	}
#endif

	/* sanity checks for mmapped I/O */
	if (mmapped && base < 0xc8000) {
		printk(KERN_ERR "i2c-elektor.o: incorrect base address (0x%0X) specified for mmapped I/O.\n", base);
		return -ENODEV;
	}

	printk(KERN_INFO "i2c-elektor.o: i2c pcf8584-isa adapter module version %s (%s)\n", I2C_VERSION, I2C_DATE);

	if (base == 0) {
		base = DEFAULT_BASE;
	}

	init_waitqueue_head(&pcf_wait);
	if (pcf_isa_init() == 0) {
		if (i2c_pcf_add_bus(&pcf_isa_ops) < 0)
			return -ENODEV;
	} else {
		return -ENODEV;
	}
	
	printk(KERN_DEBUG "i2c-elektor.o: found device at %#x.\n", base);

	return 0;
}


EXPORT_NO_SYMBOLS;

#ifdef MODULE
MODULE_AUTHOR("Hans Berglund <hb@spacetec.no>");
MODULE_DESCRIPTION("I2C-Bus adapter routines for PCF8584 ISA bus adapter");
MODULE_LICENSE("GPL");

MODULE_PARM(base, "i");
MODULE_PARM(irq, "i");
MODULE_PARM(clock, "i");
MODULE_PARM(own, "i");
MODULE_PARM(mmapped, "i");
MODULE_PARM(i2c_debug, "i");

int init_module(void) 
{
	return i2c_pcfisa_init();
}

void cleanup_module(void) 
{
	i2c_pcf_del_bus(&pcf_isa_ops);
	pcf_isa_exit();
}

#endif
