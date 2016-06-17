/*      National Semiconductor NS87560UBD Super I/O controller used in
 *      HP [BCJ]x000 workstations.
 *
 *      This chip is a horrid piece of engineering, and National
 *      denies any knowledge of its existence. Thus no datasheet is
 *      available off www.national.com. 
 *
 *	(C) Copyright 2000 Linuxcare, Inc.
 * 	(C) Copyright 2000 Linuxcare Canada, Inc.
 *	(C) Copyright 2000 Martin K. Petersen <mkp@linuxcare.com>
 * 	(C) Copyright 2000 Alex deVries <alex@linuxcare.com>
 *      (C) Copyright 2001 John Marvin <jsm@fc.hp.com>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation; either version 2 of
 *	the License, or (at your option) any later version.  
 *
 *	The initial version of this is by Martin Peterson.  Alex deVries
 *	has spent a bit of time trying to coax it into working.
 *
 *      Major changes to get basic interrupt infrastructure working to
 *      hopefully be able to support all SuperIO devices. Currently
 *      works with serial. -- John Marvin <jsm@fc.hp.com>
 */


/* NOTES:
 * 
 * Function 0 is an IDE controller. It is identical to a PC87415 IDE
 * controller (and identifies itself as such).
 *
 * Function 1 is a "Legacy I/O" controller. Under this function is a
 * whole mess of legacy I/O peripherals. Of course, HP hasn't enabled
 * all the functionality in hardware, but the following is available:
 *
 *      Two 16550A compatible serial controllers
 *      An IEEE 1284 compatible parallel port
 *      A floppy disk controller
 *
 * Function 2 is a USB controller.
 *
 * We must be incredibly careful during initialization.  Since all
 * interrupts are routed through function 1 (which is not allowed by
 * the PCI spec), we need to program the PICs on the legacy I/O port
 * *before* we attempt to set up IDE and USB.  @#$!&
 *
 * According to HP, devices are only enabled by firmware if they have
 * a physical device connected.
 *
 * Configuration register bits:
 *     0x5A: FDC, SP1, IDE1, SP2, IDE2, PAR, Reserved, P92
 *     0x5B: RTC, 8259, 8254, DMA1, DMA2, KBC, P61, APM
 *
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/serial.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/parport.h>
#include <linux/parport_pc.h>
#include <linux/serial_reg.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/gsc.h>
#include <asm/irq.h>
#include <asm/superio.h>

static struct superio_device sio_dev = {
	iosapic_irq: -1
};


#undef DEBUG_INIT

void
superio_inform_irq(int irq)
{
    if (sio_dev.iosapic_irq != -1) {
	printk(KERN_ERR "SuperIO: superio_inform_irq called twice! (more than one SuperIO?)\n");
	BUG();
	return;
    }

    sio_dev.iosapic_irq = irq;
}

static void
superio_interrupt(int irq, void *devp, struct pt_regs *regs)
{
	struct superio_device *sio = (struct superio_device *)devp;
	u8 results;
	u8 local_irq;

	/* Poll the 8259 to see if there's an interrupt. */
	outb (OCW3_POLL,IC_PIC1+0);

	results = inb(IC_PIC1+0);

	if ((results & 0x80) == 0) {
#ifndef CONFIG_SMP
		/* HACK: need to investigate why this happens if SMP enabled */
		BUG(); /* This shouldn't happen */
#endif
		return;
	}

	/* Check to see which device is interrupting */

	local_irq = results & 0x0f;

	if (local_irq == 2 || local_irq > 7) {
		printk(KERN_ERR "SuperIO: slave interrupted!\n");
		BUG();
		return;
	}

	if (local_irq == 7) {

		/* Could be spurious. Check in service bits */

		outb(OCW3_ISR,IC_PIC1+0);
		results = inb(IC_PIC1+0);
		if ((results & 0x80) == 0) { /* if ISR7 not set: spurious */
			printk(KERN_WARNING "SuperIO: spurious interrupt!\n");
			return;
		}
	}

	/* Call the appropriate device's interrupt */

	do_irq(&sio->irq_region->action[local_irq],
		sio->irq_region->data.irqbase + local_irq,
		regs);

	/* set EOI */

	outb((OCW2_SEOI|local_irq),IC_PIC1 + 0);
	return;
}

/* Initialize Super I/O device */

static void __devinit
superio_init(struct superio_device *sio)
{
	struct pci_dev *pdev = sio->lio_pdev;
	u16 word;
	u8  i;

	if (!pdev || sio->iosapic_irq == -1) {
		printk(KERN_ERR "All SuperIO functions not found!\n");
		BUG();
		return;
	}

	printk (KERN_INFO "SuperIO: Found NS87560 Legacy I/O device at %s (IRQ %i) \n",
		pdev->slot_name,sio->iosapic_irq);

	/* Find our I/O devices */
	pci_read_config_word (pdev, SIO_SP1BAR, &sio->sp1_base);
	sio->sp1_base &= ~1;
	printk (KERN_INFO "SuperIO: Serial port 1 at 0x%x\n", sio->sp1_base);

	pci_read_config_word (pdev, SIO_SP2BAR, &sio->sp2_base);
	sio->sp2_base &= ~1;
	printk (KERN_INFO "SuperIO: Serial port 2 at 0x%x\n", sio->sp2_base);

	pci_read_config_word (pdev, SIO_PPBAR, &sio->pp_base);
	sio->pp_base &= ~1;
	printk (KERN_INFO "SuperIO: Parallel port at 0x%x\n", sio->pp_base);

	pci_read_config_word (pdev, SIO_FDCBAR, &sio->fdc_base);
	sio->fdc_base &= ~1;
	printk (KERN_INFO "SuperIO: Floppy controller at 0x%x\n", sio->fdc_base);
	pci_read_config_word (pdev, SIO_ACPIBAR, &sio->acpi_base);
	sio->acpi_base &= ~1;
	printk (KERN_INFO "SuperIO: ACPI at 0x%x\n", sio->acpi_base);

	request_region (IC_PIC1, 0x1f, "pic1");
	request_region (IC_PIC2, 0x1f, "pic2");
	request_region (sio->acpi_base, 0x1f, "acpi");

	/* Enable the legacy I/O function */
        pci_read_config_word (pdev, PCI_COMMAND, &word);
	word |= PCI_COMMAND_SERR | PCI_COMMAND_PARITY | PCI_COMMAND_IO;
	pci_write_config_word (pdev, PCI_COMMAND, word);
	pci_set_master (pdev);

	/* Next project is programming the onboard interrupt
	 * controllers.  PDC hasn't done this for us, since it's using
	 * polled I/O.
	 */

	/* Set PIC interrupts to edge triggered */
	pci_write_config_byte (pdev, TRIGGER_1, 0x0);
	pci_write_config_byte (pdev, TRIGGER_2, 0x0);

	/* Disable all interrupt routing */
	for (i = IR_LOW ; i < IR_HIGH ; i++)
		pci_write_config_byte (pdev, i, 0x0);

	/* PIC1 Initialization Command Word register programming */
	outb (0x11,IC_PIC1+0);	/* ICW1: ICW4 write req | ICW1 */
	outb (0x00,IC_PIC1+1);	/* ICW2: N/A */
	outb (0x04,IC_PIC1+1);	/* ICW3: Cascade */
	outb (0x01,IC_PIC1+1);	/* ICW4: x86 mode */

	/* PIC1 Program Operational Control Words */
	outb (0xff,IC_PIC1+1);	/* OCW1: Mask all interrupts */
	outb (0xc2,IC_PIC1+0);  /* OCW2: priority (3-7,0-2) */

	/* PIC2 Initialization Command Word register programming */
	outb (0x11,IC_PIC2+0);	/* ICW1: ICW4 write req | ICW1 */
	outb (0x00,IC_PIC2+1);	/* ICW2: N/A */
	outb (0x02,IC_PIC2+1);	/* ICW3: Slave ID code */
	outb (0x01,IC_PIC2+1);	/* ICW4: x86 mode */
		
	/* Program Operational Control Words */
	outb (0xff,IC_PIC1+1);	/* OCW1: Mask all interrupts */
	outb (0x68,IC_PIC1+0);	/* OCW3: OCW3 select | ESMM | SMM */

	/* Write master mask reg */

	outb (0xff,IC_PIC1+1);

	/* Set up interrupt routing */

	pci_write_config_byte (pdev, IR_USB, 0x10); /* USB on IRQ1 */
	pci_write_config_byte (pdev, IR_SER, 0x43); /* SP1 on IRQ3, SP2 on IRQ4 */
	pci_write_config_byte (pdev, IR_PFD, 0x65); /* PAR on IRQ5, FDC on IRQ6 */
	pci_write_config_byte (pdev, IR_IDE, 0x07); /* IDE1 on IRQ7 */
	
	/* Set USB and IDE to level triggered interrupts, rest to edge */
	pci_write_config_byte (pdev, TRIGGER_1, 0x82); /* IRQ 1 and 7 */

	/* Setup USB power regulation */
	outb(1, sio->acpi_base + USB_REG_CR);
	if (inb(sio->acpi_base + USB_REG_CR) & 1)
		printk(KERN_INFO "SuperIO: USB regulator enabled\n");
	else
		printk(KERN_ERR "USB regulator not initialized!\n");

	pci_enable_device(pdev);

	if (request_irq(sio->iosapic_irq,superio_interrupt,SA_INTERRUPT,
			"SuperIO",(void *)sio)) {

		printk(KERN_ERR "SuperIO: could not get irq\n");
		BUG();
		return;
	}

	sio->iosapic_irq_enabled = 1;
	
}

static void
superio_disable_irq(void *dev, int local_irq)
{
	u8 r8;

	if ((local_irq < 1) || (local_irq == 2) || (local_irq > 7)) {
	    printk(KERN_ERR "SuperIO: Illegal irq number.\n");
	    BUG();
	    return;
	}

	/* Mask interrupt */

	r8 = inb(IC_PIC1+1);
	r8 |= (1 << local_irq);
	outb (r8,IC_PIC1+1);
}

static void
superio_enable_irq(void *dev, int local_irq)
{
	struct superio_device *sio = (struct superio_device *)dev;
	u8 r8;

	if ((local_irq < 1) || (local_irq == 2) || (local_irq > 7)) {
	    printk(KERN_ERR "SuperIO: Illegal irq number.\n");
	    BUG();
	    return;
	}

	/*
	 * It's possible that we haven't initialized the legacy IO
	 * function yet. If not, do it now.
	 */

	if (!sio->iosapic_irq_enabled)
		superio_init(sio);

	/* Unmask interrupt */

	r8 = inb(IC_PIC1+1);
	r8 &= ~(1 << local_irq);
	outb (r8,IC_PIC1+1);
}

static void
superio_mask_irq(void *dev, int local_irq)
{
	BUG();
}

static void
superio_unmask_irq(void *dev, int local_irq)
{
	BUG();
}

static struct irq_region_ops superio_irq_ops = {
	disable_irq:	superio_disable_irq,
	enable_irq:	superio_enable_irq,
	mask_irq:	superio_mask_irq,
	unmask_irq:	superio_unmask_irq
};

#ifdef DEBUG_INIT
static unsigned short expected_device[3] = {
	PCI_DEVICE_ID_NS_87415,
	PCI_DEVICE_ID_NS_87560_LIO,
	PCI_DEVICE_ID_NS_87560_USB
};
#endif

int superio_fixup_irq(struct pci_dev *pcidev)
{
	int local_irq;

#ifdef DEBUG_INIT
	int fn;
	fn = PCI_FUNC(pcidev->devfn);

	/* Verify the function number matches the expected device id. */
	if (expected_device[fn] != pcidev->device) {
		BUG();
		return -1;
	}
	printk("superio_fixup_irq(%s) ven 0x%x dev 0x%x from %p\n",
		pcidev->slot_name,
		pcidev->vendor, pcidev->device,
		__builtin_return_address(0));
#endif

	if (!sio_dev.irq_region) {
		/* Allocate an irq region for SuperIO devices */
		sio_dev.irq_region = alloc_irq_region(SUPERIO_NIRQS,
						&superio_irq_ops,
						"SuperIO", (void *) &sio_dev);
		if (!sio_dev.irq_region) {
			printk(KERN_WARNING "SuperIO: alloc_irq_region failed\n");
			return -1;
		}
	}

	/*
	 * We don't allocate a SuperIO irq for the legacy IO function,
	 * since it is a "bridge". Instead, we will allocate irq's for
	 * each legacy device as they are initialized.
	 */

	switch(pcidev->device) {
	case PCI_DEVICE_ID_NS_87415:		/* Function 0 */
		local_irq = IDE_IRQ;
		break;
	case PCI_DEVICE_ID_NS_87560_LIO:	/* Function 1 */
		sio_dev.lio_pdev = pcidev; /* save for later initialization */
		return -1;
	case PCI_DEVICE_ID_NS_87560_USB:	/* Function 2 */
		local_irq = USB_IRQ;
		break;
	default:
		local_irq = -1;
		BUG();
		break;
	}

	return(sio_dev.irq_region->data.irqbase + local_irq);
}

void __devinit
superio_serial_init(void)
{
#ifdef CONFIG_SERIAL
	struct serial_struct *serial;
	int retval;
	
	if (!sio_dev.irq_region)
		return; /* superio not present */

	if (!sio_dev.iosapic_irq_enabled)
		superio_init(&sio_dev);

	serial = kmalloc(2 * sizeof (struct serial_struct), GFP_KERNEL);

	if (!serial) {
		printk(KERN_WARNING "SuperIO: Could not get memory for serial struct.\n");
		return;
	}

	memset(serial, 0, 2 * sizeof (struct serial_struct));

	serial->type = PORT_16550A;
	serial->line = 0;
	serial->port = sio_dev.sp1_base;
	serial->port_high = 0;
	serial->irq = sio_dev.irq_region->data.irqbase + SP1_IRQ;
	serial->io_type = SERIAL_IO_PORT;
	serial->flags = 0;
	serial->xmit_fifo_size = 16;
	serial->custom_divisor = 0;
	serial->baud_base = 115200;

	retval = register_serial(serial);
	if (retval < 0) {
		printk(KERN_WARNING "SuperIO: Register Serial #0 failed.\n");
		kfree (serial);
		return;
	}

	serial++;

	serial->type = PORT_16550A;
	serial->line = 1;
	serial->port = sio_dev.sp2_base;
	serial->port_high = 0;
	serial->irq = sio_dev.irq_region->data.irqbase + SP2_IRQ;
	serial->io_type = SERIAL_IO_PORT;
	serial->flags = 0;
	serial->xmit_fifo_size = 16;
	serial->custom_divisor = 0;
	serial->baud_base = 115200;

	retval = register_serial(serial);
	if (retval < 0)
		printk(KERN_WARNING "SuperIO: Register Serial #1 failed.\n");
#endif /* CONFIG_SERIAL */
}

EXPORT_SYMBOL(superio_serial_init);


#ifdef CONFIG_PARPORT_PC
void __devinit
superio_parport_init(void)
{
	if (!sio_dev.irq_region)
		return; /* superio not present */

	if (!sio_dev.iosapic_irq_enabled)
		superio_init(&sio_dev);

	if (!parport_pc_probe_port(sio_dev.pp_base, 
		0 /*base_hi*/,
		sio_dev.irq_region->data.irqbase + PAR_IRQ, 
		PARPORT_DMA_NONE /* dma */,
		NULL /*struct pci_dev* */))

		printk(KERN_WARNING "SuperIO: Probing parallel port failed.\n");
}

EXPORT_SYMBOL(superio_parport_init);
#endif	/* CONFIG_PARPORT_PC */


int
superio_get_ide_irq(void)
{
	if (sio_dev.irq_region)
		return sio_dev.irq_region->data.irqbase + IDE_IRQ;
	else
		return 0;
}

EXPORT_SYMBOL(superio_get_ide_irq);

static int __devinit superio_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
#ifdef DEBUG_INIT
	printk("superio_probe(%s) ven 0x%x dev 0x%x sv 0x%x sd 0x%x class 0x%x\n",
		dev->slot_name,
		dev->vendor, dev->device,
		dev->subsystem_vendor, dev->subsystem_device,
		dev->class);
/*
** superio_probe(00:0e.0) ven 0x100b dev 0x2 sv 0x0 sd 0x0 class 0x1018a
** superio_probe(00:0e.1) ven 0x100b dev 0xe sv 0x0 sd 0x0 class 0x68000
** superio_probe(00:0e.2) ven 0x100b dev 0x12 sv 0x0 sd 0x0 class 0xc0310
*/
#endif

	/* superio_fixup_irq(dev); */

	if (dev->device == PCI_DEVICE_ID_NS_87560_LIO) {
#ifdef CONFIG_PARPORT_PC
		superio_parport_init();
#endif
		/* Don't call superio_serial_init() - see rs_init() */
		/* REVISIT : superio_fdc_init() ? */
		return 0;
	}
	else
	{
		/* don't claim this device; let whatever either driver
		 * do it 
		 */ 
		return -1;
	}
}

static struct pci_device_id superio_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_NS, PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ 0, }
};

static struct pci_driver superio_driver = {
	name:		"SuperIO",
	id_table:	superio_tbl,
	probe:		superio_probe,
};

static int __init superio_modinit(void)
{
	return pci_module_init(&superio_driver);
}

static void __exit superio_exit(void)
{
	pci_unregister_driver(&superio_driver);
}

module_init(superio_modinit);
module_exit(superio_exit);
