/*
 *	Find I2O capable controllers on the PCI bus, and register/install
 *	them with the I2O layer
 *
 *	(C) Copyright 1999   Red Hat Software
 *	
 *	Written by Alan Cox, Building Number Three Ltd
 * 	Modified by Deepak Saxena <deepak@plexity.net>
 * 	Modified by Boji T Kannanthanam <boji.t.kannanthanam@intel.com>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 * 	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	TODO:
 *		Support polled I2O PCI controllers. 
 *		2.4 hotplug support
 *		Finish verifying 64bit/bigendian clean
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/i2o.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <asm/io.h>

#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif // CONFIG_MTRR

static int dpt = 0;

extern void i2o_sys_init(void);

/**
 *	i2o_pci_dispose		-	Free bus specific resources
 *	@c: I2O controller
 *
 *	Disable interrupts and then free interrupt, I/O and mtrr resources 
 *	used by this controller. Called by the I2O core on unload.
 */
 
static void i2o_pci_dispose(struct i2o_controller *c)
{
	I2O_IRQ_WRITE32(c,0xFFFFFFFF);
	if(c->bus.pci.irq > 0)
		free_irq(c->bus.pci.irq, c);
	iounmap(((u8 *)c->post_port)-0x40);

#ifdef CONFIG_MTRR
	if(c->bus.pci.mtrr_reg0 > 0)
		mtrr_del(c->bus.pci.mtrr_reg0, 0, 0);
	if(c->bus.pci.mtrr_reg1 > 0)
		mtrr_del(c->bus.pci.mtrr_reg1, 0, 0);
#endif
}

/**
 *	i2o_pci_bind		-	Bind controller and devices
 *	@c: i2o controller
 *	@dev: i2o device
 *
 *	Bind a device driver to a controller. In the case of PCI all we need to do
 *	is module housekeeping.
 */
 
static int i2o_pci_bind(struct i2o_controller *c, struct i2o_device *dev)
{
	MOD_INC_USE_COUNT;
	return 0;
}

/**
 *	i2o_pci_unbind		-	Bind controller and devices
 *	@c: i2o controller
 *	@dev: i2o device
 *
 *	Unbind a device driver from a controller. In the case of PCI all we need to do
 *	is module housekeeping.
 */
 

static int i2o_pci_unbind(struct i2o_controller *c, struct i2o_device *dev)
{
	MOD_DEC_USE_COUNT;
	return 0;
}

/**
 *	i2o_pci_enable		-	Enable controller
 *	@c: controller
 *
 *	Called by the I2O core code in order to enable bus specific
 *	resources for this controller. In our case that means unmasking the 
 *	interrupt line.
 */

static void i2o_pci_enable(struct i2o_controller *c)
{
	I2O_IRQ_WRITE32(c, 0);
	c->enabled = 1;
}

/**
 *	i2o_pci_disable		-	Enable controller
 *	@c: controller
 *
 *	Called by the I2O core code in order to enable bus specific
 *	resources for this controller. In our case that means masking the 
 *	interrupt line.
 */

static void i2o_pci_disable(struct i2o_controller *c)
{
	I2O_IRQ_WRITE32(c, 0xFFFFFFFF);
	c->enabled = 0;
}

/**
 *	i2o_pci_interrupt	-	Bus specific interrupt handler
 *	@irq: interrupt line
 *	@dev_id: cookie
 *
 *	Handle an interrupt from a PCI based I2O controller. This turns out
 *	to be rather simple. We keep the controller pointer in the cookie.
 */
 
static void i2o_pci_interrupt(int irq, void *dev_id, struct pt_regs *r)
{
	struct i2o_controller *c = dev_id;
	i2o_run_queue(c);
}	

/**
 *	i2o_pci_install		-	Install a PCI i2o controller
 *	@dev: PCI device of the I2O controller
 *
 *	Install a PCI (or in theory AGP) i2o controller. Devices are
 *	initialized, configured and registered with the i2o core subsystem. Be
 *	very careful with ordering. There may be pending interrupts.
 *
 *	To Do: Add support for polled controllers
 */

int __init i2o_pci_install(struct pci_dev *dev)
{
	struct i2o_controller *c=kmalloc(sizeof(struct i2o_controller),
						GFP_KERNEL);
	unsigned long mem;
	u32 memptr = 0;
	u32 size;
	
	int i;

	if(c==NULL)
	{
		printk(KERN_ERR "i2o: Insufficient memory to add controller.\n");
		return -ENOMEM;
	}
	memset(c, 0, sizeof(*c));

	for(i=0; i<6; i++)
	{
		/* Skip I/O spaces */
		if(!(pci_resource_flags(dev, i) & IORESOURCE_IO))
		{
			memptr = pci_resource_start(dev, i);
			break;
		}
	}
	
	if(i==6)
	{
		printk(KERN_ERR "i2o: I2O controller has no memory regions defined.\n");
		kfree(c);
		return -EINVAL;
	}
	
	size = dev->resource[i].end-dev->resource[i].start+1;	
	/* Map the I2O controller */
	
	printk(KERN_INFO "i2o: PCI I2O controller at 0x%08X size=%d\n", memptr, size);
	mem = (unsigned long)ioremap(memptr, size);
	if(mem==0)
	{
		printk(KERN_ERR "i2o: Unable to map controller.\n");
		kfree(c);
		return -EINVAL;
	}

	c->bus.pci.irq = -1;
	c->bus.pci.dpt = 0;
	c->bus.pci.short_req = 0;
	c->pdev = dev;

	c->irq_mask = mem+0x34;
	c->post_port = mem+0x40;
	c->reply_port = mem+0x44;

	c->mem_phys = memptr;
	c->mem_offset = mem;
	c->destructor = i2o_pci_dispose;
	
	c->bind = i2o_pci_bind;
	c->unbind = i2o_pci_unbind;
	c->bus_enable = i2o_pci_enable;
	c->bus_disable = i2o_pci_disable;
	
	c->type = I2O_TYPE_PCI;

	/*
	 *	Cards that fall apart if you hit them with large I/O
	 *	loads...
	 */
	 
	if(dev->vendor == PCI_VENDOR_ID_NCR && dev->device == 0x0630)
	{
		c->bus.pci.short_req = 1;
		printk(KERN_INFO "I2O: Symbios FC920 workarounds activated.\n");
	}
	if(dev->subsystem_vendor == PCI_VENDOR_ID_PROMISE)
	{
		c->bus.pci.promise = 1;
		printk(KERN_INFO "I2O: Promise workarounds activated.\n");
	}

	/*
	 *	Cards that go bananas if you quiesce them before you reset
	 *	them
	 */
	 
	if(dev->vendor == PCI_VENDOR_ID_DPT)
		c->bus.pci.dpt=1;
	
	/* 
	 * Enable Write Combining MTRR for IOP's memory region
	 */
#ifdef CONFIG_MTRR
	c->bus.pci.mtrr_reg0 =
		mtrr_add(c->mem_phys, size, MTRR_TYPE_WRCOMB, 1);
	/*
	 * If it is an INTEL i960 I/O processor then set the first 64K to
	 * Uncacheable since the region contains the Messaging unit which
	 * shouldn't be cached.
	 */
	c->bus.pci.mtrr_reg1 = -1;
	if(dev->vendor == PCI_VENDOR_ID_INTEL || dev->vendor == PCI_VENDOR_ID_DPT)
	{
		printk(KERN_INFO "I2O: MTRR workaround for Intel i960 processor\n"); 
		c->bus.pci.mtrr_reg1 =	mtrr_add(c->mem_phys, 65536, MTRR_TYPE_UNCACHABLE, 1);
		if(c->bus.pci.mtrr_reg1< 0)
		{
			printk(KERN_INFO "i2o_pci: Error in setting MTRR_TYPE_UNCACHABLE\n");
			mtrr_del(c->bus.pci.mtrr_reg0, c->mem_phys, size);
			c->bus.pci.mtrr_reg0 = -1;
		}
	}

#endif

	I2O_IRQ_WRITE32(c,0xFFFFFFFF);

	i = i2o_install_controller(c);
	
	if(i<0)
	{
		printk(KERN_ERR "i2o: Unable to install controller.\n");
		kfree(c);
		iounmap((void *)mem);
		return i;
	}

	c->bus.pci.irq = dev->irq;
	if(c->bus.pci.irq)
	{
		i=request_irq(dev->irq, i2o_pci_interrupt, SA_SHIRQ,
			c->name, c);
		if(i<0)
		{
			printk(KERN_ERR "%s: unable to allocate interrupt %d.\n",
				c->name, dev->irq);
			c->bus.pci.irq = -1;
			i2o_delete_controller(c);
			iounmap((void *)mem);
			return -EBUSY;
		}
	}

	printk(KERN_INFO "%s: Installed at IRQ%d\n", c->name, dev->irq);
	I2O_IRQ_WRITE32(c,0x0);
	c->enabled = 1;
	return 0;	
}

/**
 *	i2o_pci_scan	-	Scan the pci bus for controllers
 *	
 *	Scan the PCI devices on the system looking for any device which is a 
 *	memory of the Intelligent, I2O class. We attempt to set up each such device
 *	and register it with the core.
 *
 *	Returns the number of controllers registered
 */
 
int __init i2o_pci_scan(void)
{
	struct pci_dev *dev;
	int count=0;
	
	printk(KERN_INFO "i2o: Checking for PCI I2O controllers...\n");

	pci_for_each_dev(dev)	
	{
		if((dev->class>>8)!=PCI_CLASS_INTELLIGENT_I2O)
			continue;
		if(dev->vendor == PCI_VENDOR_ID_DPT && !dpt)
		{
			if(dev->device == 0xA501 || dev->device == 0xA511)
			{
				printk(KERN_INFO "i2o: Skipping Adaptec/DPT I2O raid with preferred native driver.\n");
				continue;
			}
		}
		if((dev->class&0xFF)>1)
		{
			printk(KERN_INFO "i2o: I2O Controller found but does not support I2O 1.5 (skipping).\n");
			continue;
		}
		if (pci_enable_device(dev))
			continue;
		printk(KERN_INFO "i2o: I2O controller on bus %d at %d.\n",
			dev->bus->number, dev->devfn);
		if(pci_set_dma_mask(dev, 0xffffffff))
		{
			printk(KERN_WARNING "I2O controller on bus %d at %d : No suitable DMA available\n", dev->bus->number, dev->devfn);
		 	continue;
		}
		pci_set_master(dev);
		if(i2o_pci_install(dev)==0)
			count++;
	}
	if(count)
		printk(KERN_INFO "i2o: %d I2O controller%s found and installed.\n", count,
			count==1?"":"s");
	return count?count:-ENODEV;
}


/**
 *	i2o_pci_core_attach	-	PCI initialisation for I2O
 *
 *	Find any I2O controllers and if present initialise them and bring up
 *	the I2O subsystem.
 *
 *	Returns 0 on success or an error code
 */
 
static int i2o_pci_core_attach(void)
{
	printk(KERN_INFO "Linux I2O PCI support (c) 1999 Red Hat Software.\n");
	if(i2o_pci_scan()>0)
	{
		i2o_sys_init();
		return 0;
	}
	return -ENODEV;
}

/**
 *	i2o_pci_core_detach	-	PCI unload for I2O
 *
 *	Free up any resources not released when the controllers themselves were
 *	shutdown and unbound from the bus and drivers
 */
 
static void i2o_pci_core_detach(void)
{
}

MODULE_AUTHOR("Red Hat Software");
MODULE_DESCRIPTION("I2O PCI Interface");
MODULE_LICENSE("GPL");

MODULE_PARM(dpt, "i");
MODULE_PARM_DESC(dpt, "Set this if you want to drive DPT cards normally handled by dpt_i2o");
module_init(i2o_pci_core_attach);
module_exit(i2o_pci_core_detach);
 