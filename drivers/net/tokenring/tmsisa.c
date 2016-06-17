/*
 *  tmsisa.c: A generic network driver for TMS380-based ISA token ring cards.
 *
 *  Based on tmspci written 1999 by Adam Fritzler
 *  
 *  Written 2000 by Jochen Friedrich
 *  Dedicated to my girlfriend Steffi Bopp
 *
 *  This software may be used and distributed according to the terms
 *  of the GNU General Public License, incorporated herein by reference.
 *
 *  This driver module supports the following cards:
 *	- SysKonnect TR4/16(+) ISA	(SK-4190)
 *
 *  Maintainer(s):
 *    AF        Adam Fritzler           mid@auk.cx
 *    JF	Jochen Friedrich	jochen@scram.de
 *
 *  TODO:
 *	1. Add support for Proteon TR ISA adapters (1392, 1392+)
 */
static const char version[] = "tmsisa.c: v1.00 14/01/2001 by Jochen Friedrich\n";

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/pci.h>
#include <asm/dma.h>

#include <linux/netdevice.h>
#include <linux/trdevice.h>
#include "tms380tr.h"

#define TMS_ISA_IO_EXTENT 32

/* A zero-terminated list of I/O addresses to be probed. */
static unsigned int portlist[] __initdata = {
	0x0A20, 0x1A20, 0x0B20, 0x1B20, 0x0980, 0x1980, 0x0900, 0x1900,// SK
	0
};

/* A zero-terminated list of IRQs to be probed. 
 * Used again after initial probe for sktr_chipset_init, called from sktr_open.
 */
static unsigned short irqlist[] = {
	3, 5, 9, 10, 11, 12, 15,
	0
};

/* A zero-terminated list of DMAs to be probed. */
static int dmalist[] __initdata = {
	5, 6, 7,
	0
};

static char isa_cardname[] = "SK NET TR 4/16 ISA\0";

int tms_isa_probe(struct net_device *dev);
static int tms_isa_open(struct net_device *dev);
static int tms_isa_close(struct net_device *dev);
static void tms_isa_read_eeprom(struct net_device *dev);
static unsigned short tms_isa_setnselout_pins(struct net_device *dev);

static unsigned short tms_isa_sifreadb(struct net_device *dev, unsigned short reg)
{
	return inb(dev->base_addr + reg);
}

static unsigned short tms_isa_sifreadw(struct net_device *dev, unsigned short reg)
{
	return inw(dev->base_addr + reg);
}

static void tms_isa_sifwriteb(struct net_device *dev, unsigned short val, unsigned short reg)
{
	outb(val, dev->base_addr + reg);
}

static void tms_isa_sifwritew(struct net_device *dev, unsigned short val, unsigned short reg)
{
	outw(val, dev->base_addr + reg);
}

struct tms_isa_card {
	struct net_device *dev;
	struct tms_isa_card *next;
};

static struct tms_isa_card *tms_isa_card_list;

static int __init tms_isa_probe1(int ioaddr)
{
	unsigned char old, chk1, chk2;

	old = inb(ioaddr + SIFADR);	/* Get the old SIFADR value */

	chk1 = 0;	/* Begin with check value 0 */
	do {
		/* Write new SIFADR value */
		outb(chk1, ioaddr + SIFADR);

		/* Read, invert and write */
		chk2 = inb(ioaddr + SIFADD);
		chk2 ^= 0x0FE;
		outb(chk2, ioaddr + SIFADR);

		/* Read, invert and compare */
		chk2 = inb(ioaddr + SIFADD);
		chk2 ^= 0x0FE;

		if(chk1 != chk2)
			return (-1);	/* No adapter */

		chk1 -= 2;
	} while(chk1 != 0);	/* Repeat 128 times (all byte values) */

    	/* Restore the SIFADR value */
	outb(old, ioaddr + SIFADR);

	return (0);
}

int __init tms_isa_probe(struct net_device *dev)
{
        static int versionprinted;
	struct net_local *tp;
	int j;
	struct tms_isa_card *card;

	if(check_region(dev->base_addr, TMS_ISA_IO_EXTENT))
		return -1;

	if(tms_isa_probe1(dev->base_addr))
		return -1;
   
	if (versionprinted++ == 0)
		printk("%s", version);
 
	/* At this point we have found a valid card. */
    
	if (!request_region(dev->base_addr, TMS_ISA_IO_EXTENT, isa_cardname))
		return -1;

	if (tmsdev_init(dev, ISA_MAX_ADDRESS, NULL))
       	{
		release_region(dev->base_addr, TMS_ISA_IO_EXTENT); 
		return -1;
	}

	dev->base_addr &= ~3; 
		
	tms_isa_read_eeprom(dev);

	printk("%s:    Ring Station Address: ", dev->name);
	printk("%2.2x", dev->dev_addr[0]);
	for (j = 1; j < 6; j++)
		printk(":%2.2x", dev->dev_addr[j]);
	printk("\n");
		
	tp = (struct net_local *)dev->priv;
	tp->setnselout = tms_isa_setnselout_pins;
		
	tp->sifreadb = tms_isa_sifreadb;
	tp->sifreadw = tms_isa_sifreadw;
	tp->sifwriteb = tms_isa_sifwriteb;
	tp->sifwritew = tms_isa_sifwritew;
	
	memcpy(tp->ProductID, isa_cardname, PROD_ID_SIZE + 1);

	tp->tmspriv = NULL;

	dev->open = tms_isa_open;
	dev->stop = tms_isa_close;

	if (dev->irq == 0)
	{
		for(j = 0; irqlist[j] != 0; j++)
		{
			dev->irq = irqlist[j];
			if (!request_irq(dev->irq, tms380tr_interrupt, 0, 
				isa_cardname, dev))
				break;
                }
		
                if(irqlist[j] == 0)
                {
                        printk("%s: AutoSelect no IRQ available\n", dev->name);
			release_region(dev->base_addr, TMS_ISA_IO_EXTENT); 
			tmsdev_term(dev);
			return -1;
		}
	}
	else
	{
		for(j = 0; irqlist[j] != 0; j++)
			if (irqlist[j] == dev->irq)
				break;
		if (irqlist[j] == 0)
		{
			printk("%s: Illegal IRQ %d specified\n",
				dev->name, dev->irq);
			release_region(dev->base_addr, TMS_ISA_IO_EXTENT); 
			tmsdev_term(dev);
			return -1;
		}
		if (request_irq(dev->irq, tms380tr_interrupt, 0, 
			isa_cardname, dev))
		{
                        printk("%s: Selected IRQ %d not available\n", 
				dev->name, dev->irq);
			release_region(dev->base_addr, TMS_ISA_IO_EXTENT); 
			tmsdev_term(dev);
			return -1;
		}
	}

	if (dev->dma == 0)
	{
		for(j = 0; dmalist[j] != 0; j++)
		{
			dev->dma = dmalist[j];
                        if (!request_dma(dev->dma, isa_cardname))
				break;
		}

		if(dmalist[j] == 0)
		{
			printk("%s: AutoSelect no DMA available\n", dev->name);
			release_region(dev->base_addr, TMS_ISA_IO_EXTENT); 
			free_irq(dev->irq, dev);
			tmsdev_term(dev);
			return -1;
		}
	}
	else
	{
		for(j = 0; dmalist[j] != 0; j++)
			if (dmalist[j] == dev->dma)
				break;
		if (dmalist[j] == 0)
		{
                        printk("%s: Illegal DMA %d specified\n", 
				dev->name, dev->dma);
			release_region(dev->base_addr, TMS_ISA_IO_EXTENT); 
			free_irq(dev->irq, dev);
			tmsdev_term(dev);
			return -1;
		}
		if (request_dma(dev->dma, isa_cardname))
		{
                        printk("%s: Selected DMA %d not available\n", 
				dev->name, dev->dma);
			release_region(dev->base_addr, TMS_ISA_IO_EXTENT); 
			free_irq(dev->irq, dev);
			tmsdev_term(dev);
			return -1;
		}
	}

	printk("%s:    IO: %#4lx  IRQ: %d  DMA: %d\n",
	       dev->name, dev->base_addr, dev->irq, dev->dma);
		
	if (register_trdev(dev) == 0) 
	{
		/* Enlist in the card list */
		card = kmalloc(sizeof(struct tms_isa_card), GFP_KERNEL);
		if (!card) {
			unregister_trdev(dev);
			release_region(dev->base_addr, TMS_ISA_IO_EXTENT); 
			free_irq(dev->irq, dev);
			free_dma(dev->dma);
			tmsdev_term(dev);
			return -1;
		}
		card->next = tms_isa_card_list;
		tms_isa_card_list = card;
		card->dev = dev;
	} 
	else 
	{
		printk("%s: register_trdev() returned non-zero.\n", dev->name);
		release_region(dev->base_addr, TMS_ISA_IO_EXTENT); 
		free_irq(dev->irq, dev);
		free_dma(dev->dma);
		tmsdev_term(dev);
		return -1;
	}
	
	return 0;
}

/*
 * Reads MAC address from adapter RAM, which should've read it from
 * the onboard ROM.  
 *
 * Calling this on a board that does not support it can be a very
 * dangerous thing.  The Madge board, for instance, will lock your
 * machine hard when this is called.  Luckily, its supported in a
 * seperate driver.  --ASF
 */
static void tms_isa_read_eeprom(struct net_device *dev)
{
	int i;
	
	/* Address: 0000:0000 */
	tms_isa_sifwritew(dev, 0, SIFADX);
	tms_isa_sifwritew(dev, 0, SIFADR);	
	
	/* Read six byte MAC address data */
	dev->addr_len = 6;
	for(i = 0; i < 6; i++)
		dev->dev_addr[i] = tms_isa_sifreadw(dev, SIFINC) >> 8;
}

unsigned short tms_isa_setnselout_pins(struct net_device *dev)
{
	return 0;
}

static int tms_isa_open(struct net_device *dev)
{  
	struct net_local *tp = (struct net_local *)dev->priv;
	unsigned short val = 0;
	unsigned short oldval;
	int i;

	val = 0;
	for(i = 0; irqlist[i] != 0; i++)
	{
		if(irqlist[i] == dev->irq)
			break;
	}

	val |= CYCLE_TIME << 2;
	val |= i << 4;
	i = dev->dma - 5;
	val |= i;
	if(tp->DataRate == SPEED_4)
		val |= LINE_SPEED_BIT;
	else
		val &= ~LINE_SPEED_BIT;
	oldval = tms_isa_sifreadb(dev, POSREG);
	/* Leave cycle bits alone */
	oldval |= 0xf3;
	val &= oldval;
	tms_isa_sifwriteb(dev, val, POSREG);

	tms380tr_open(dev);
	MOD_INC_USE_COUNT;
	return 0;
}

static int tms_isa_close(struct net_device *dev)
{
	tms380tr_close(dev);
	MOD_DEC_USE_COUNT;
	return 0;
}

#ifdef MODULE

#define ISATR_MAX_ADAPTERS 3

static int io[ISATR_MAX_ADAPTERS];
static int irq[ISATR_MAX_ADAPTERS];
static int dma[ISATR_MAX_ADAPTERS];

MODULE_LICENSE("GPL");

MODULE_PARM(io, "1-" __MODULE_STRING(ISATR_MAX_ADAPTERS) "i");
MODULE_PARM(irq, "1-" __MODULE_STRING(ISATR_MAX_ADAPTERS) "i");
MODULE_PARM(dma, "1-" __MODULE_STRING(ISATR_MAX_ADAPTERS) "i");

int init_module(void)
{
	int i, num;
	struct net_device *dev;

	num = 0;
	if (io[0]) 
	{ /* Only probe addresses from command line */
		for (i = 0; i < ISATR_MAX_ADAPTERS; i++)
	       	{
			if (io[i] == 0)
				continue;

			dev = init_trdev(NULL, 0);
			if (!dev)
				return (-ENOMEM);
		
			dev->base_addr = io[i];
			dev->irq       = irq[i];
			dev->dma       = dma[i];

			if (tms_isa_probe(dev))
			{
				unregister_netdev(dev);
				kfree(dev);
			}
			else
				num++;
		}
	}
       	else
       	{
		for(i = 0; portlist[i]; i++)
		{
			if (num >= ISATR_MAX_ADAPTERS)
				continue;

			dev = init_trdev(NULL, 0);
			if (!dev)
				return (-ENOMEM);
		
			dev->base_addr = portlist[i];
			dev->irq       = irq[num];
			dev->dma       = dma[num];

			if (tms_isa_probe(dev))
			{
				unregister_netdev(dev);
				kfree(dev);
			}
			else
				num++;
		}
	}
	printk(KERN_NOTICE "tmsisa.c: %d cards found.\n", num);
	/* Probe for cards. */
	if (num == 0) {
		printk(KERN_NOTICE "tmsisa.c: No cards found.\n");
	}
	return (0);
}

void cleanup_module(void)
{
	struct net_device *dev;
	struct tms_isa_card *this_card;

	while (tms_isa_card_list) {
		dev = tms_isa_card_list->dev;
		
		unregister_netdev(dev);
		release_region(dev->base_addr, TMS_ISA_IO_EXTENT);
		free_irq(dev->irq, dev);
		free_dma(dev->dma);
		tmsdev_term(dev);
		kfree(dev);
		this_card = tms_isa_card_list;
		tms_isa_card_list = this_card->next;
		kfree(this_card);
	}
}
#endif /* MODULE */


/*
 * Local variables:
 *  compile-command: "gcc -DMODVERSIONS  -DMODULE -D__KERNEL__ -Wall -Wstrict-prototypes -O6 -fomit-frame-pointer -I/usr/src/linux/drivers/net/tokenring/ -c tmsisa.c"
 *  alt-compile-command: "gcc -DMODULE -D__KERNEL__ -Wall -Wstrict-prototypes -O6 -fomit-frame-pointer -I/usr/src/linux/drivers/net/tokenring/ -c tmsisa.c"
 *  c-set-style "K&R"
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
