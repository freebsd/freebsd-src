/* orinoco_plx.c 0.13d
 * 
 * Driver for Prism II devices which would usually be driven by orinoco_cs,
 * but are connected to the PCI bus by a PLX9052. 
 *
 * Copyright (C) 2001 Daniel Barlow <dan@telent.net>
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License
 * at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and
 * limitations under the License.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License version 2 (the "GPL"), in
 * which case the provisions of the GPL are applicable instead of the
 * above.  If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the MPL or the GPL.

 * Caution: this is experimental and probably buggy.  For success and
 * failure reports for different cards and adaptors, see
 * orinoco_plx_pci_id_table near the end of the file.  If you have a
 * card we don't have the PCI id for, and looks like it should work,
 * drop me mail with the id and "it works"/"it doesn't work".
 *
 * Note: if everything gets detected fine but it doesn't actually send
 * or receive packets, your first port of call should probably be to   
 * try newer firmware in the card.  Especially if you're doing Ad-Hoc
 * modes
 *
 * The actual driving is done by orinoco.c, this is just resource
 * allocation stuff.  The explanation below is courtesy of Ryan Niemi
 * on the linux-wlan-ng list at
 * http://archives.neohapsis.com/archives/dev/linux-wlan/2001-q1/0026.html

The PLX9052-based cards (WL11000 and several others) are a different
beast than the usual PCMCIA-based PRISM2 configuration expected by
wlan-ng. Here's the general details on how the WL11000 PCI adapter
works:

 - Two PCI I/O address spaces, one 0x80 long which contains the PLX9052
   registers, and one that's 0x40 long mapped to the PCMCIA slot I/O
   address space.

 - One PCI memory address space, mapped to the PCMCIA memory space
   (containing the CIS).

After identifying the I/O and memory space, you can read through the
memory space to confirm the CIS's device ID or manufacturer ID to make
sure it's the expected card. Keep in mind that the PCMCIA spec specifies
the CIS as the lower 8 bits of each word read from the CIS, so to read the
bytes of the CIS, read every other byte (0,2,4,...). Passing that test,
you need to enable the I/O address space on the PCMCIA card via the PCMCIA
COR register. This is the first byte following the CIS. In my case
(which may not have any relation to what's on the PRISM2 cards), COR was
at offset 0x800 within the PCI memory space. Write 0x41 to the COR
register to enable I/O mode and to select level triggered interrupts. To
confirm you actually succeeded, read the COR register back and make sure
it actually got set to 0x41, incase you have an unexpected card inserted.

Following that, you can treat the second PCI I/O address space (the one
that's not 0x80 in length) as the PCMCIA I/O space.

Note that in the Eumitcom's source for their drivers, they register the
interrupt as edge triggered when registering it with the Windows kernel. I
don't recall how to register edge triggered on Linux (if it can be done at
all). But in some experimentation, I don't see much operational
difference between using either interrupt mode. Don't mess with the
interrupt mode in the COR register though, as the PLX9052 wants level
triggers with the way the serial EEPROM configures it on the WL11000.

There's some other little quirks related to timing that I bumped into, but
I don't recall right now. Also, there's two variants of the WL11000 I've
seen, revision A1 and T2. These seem to differ slightly in the timings
configured in the wait-state generator in the PLX9052. There have also
been some comments from Eumitcom that cards shouldn't be hot swapped,
apparently due to risk of cooking the PLX9052. I'm unsure why they
believe this, as I can't see anything in the design that would really
cause a problem, except for crashing drivers not written to expect it. And
having developed drivers for the WL11000, I'd say it's quite tricky to
write code that will successfully deal with a hot unplug. Very odd things
happen on the I/O side of things. But anyway, be warned. Despite that,
I've hot-swapped a number of times during debugging and driver development
for various reasons (stuck WAIT# line after the radio card's firmware
locks up).

Hope this is enough info for someone to add PLX9052 support to the wlan-ng
card. In the case of the WL11000, the PCI ID's are 0x1639/0x0200, with
matching subsystem ID's. Other PLX9052-based manufacturers other than
Eumitcom (or on cards other than the WL11000) may have different PCI ID's.

If anyone needs any more specific info, let me know. I haven't had time
to implement support myself yet, and with the way things are going, might
not have time for a while..

---end of mail---
*/

#include <linux/config.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/ioport.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/system.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/etherdevice.h>
#include <linux/wireless.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/wireless.h>
#include <linux/fcntl.h>

#include <pcmcia/cisreg.h>

#include "hermes.h"
#include "orinoco.h"

static char dev_info[] = "orinoco_plx";

#define COR_OFFSET    (0x3e0 / 2)	/* COR attribute offset of Prism2 PC card */
#define COR_VALUE     (COR_LEVEL_REQ | COR_FUNC_ENA) /* Enable PC card with interrupt in level trigger */

#define PLX_INTCSR       0x4c /* Interrupt Control and Status Register */
#define PLX_INTCSR_INTEN (1<<6) /* Interrupt Enable bit */

static const u16 cis_magic[] = {
	0x0001, 0x0003, 0x0000, 0x0000, 0x00ff, 0x0017, 0x0004, 0x0067
};

static int orinoco_plx_init_one(struct pci_dev *pdev,
				const struct pci_device_id *ent)
{
	int err = 0;
	u16 *attr_mem = NULL;
	u32 reg, addr;
	struct orinoco_private *priv = NULL;
	unsigned long pccard_ioaddr = 0;
	unsigned long pccard_iolen = 0;
	struct net_device *dev = NULL;
	int netdev_registered = 0;
	int i;

	err = pci_enable_device(pdev);
	if (err)
		return -EIO;

	/* Resource 2 is mapped to the PCMCIA space */
	attr_mem = ioremap(pci_resource_start(pdev, 2), PAGE_SIZE);
	if (! attr_mem)
		goto fail;

	printk(KERN_DEBUG "orinoco_plx: CIS: ");
	for (i = 0; i < 16; i++) {
		printk("%02X:", (int)attr_mem[i]);
	}
	printk("\n");

	/* Verify whether PC card is present */
	/* FIXME: we probably need to be smarted about this */
	if (memcmp(attr_mem, cis_magic, sizeof(cis_magic)) != 0) {
		printk(KERN_ERR "orinoco_plx: The CIS value of Prism2 PC card is invalid.\n");
		err = -EIO;
		goto fail;
	}

	/* PCMCIA COR is the first byte following CIS: this write should
	 * enable I/O mode and select level-triggered interrupts */
	attr_mem[COR_OFFSET] = COR_VALUE;
	mdelay(1);
	reg = attr_mem[COR_OFFSET];
	if (reg != COR_VALUE) {
		printk(KERN_ERR "orinoco_plx: Error setting COR value (reg=%x)\n", reg);
		goto fail;
	}			

	iounmap(attr_mem);
	attr_mem = NULL; /* done with this now, it seems */

	/* bjoern: We need to tell the card to enable interrupts, in
	   case the serial eprom didn't do this already. See the
	   PLX9052 data book, p8-1 and 8-24 for reference. */
	addr = pci_resource_start(pdev, 1);
	reg = 0;
	reg = inl(addr+PLX_INTCSR);
	if(reg & PLX_INTCSR_INTEN)
		printk(KERN_DEBUG "orinoco_plx: "
		       "Local Interrupt already enabled\n");
	else {
		reg |= PLX_INTCSR_INTEN;
		outl(reg, addr+PLX_INTCSR);
		reg = inl(addr+PLX_INTCSR);
		if(!(reg & PLX_INTCSR_INTEN)) {
			printk(KERN_ERR "orinoco_plx: "
			       "Couldn't enable Local Interrupts\n");
			goto fail;
		}
	}

	/* and 3 to the PCMCIA slot I/O address space */
	pccard_ioaddr = pci_resource_start(pdev, 3);
	pccard_iolen = pci_resource_len(pdev, 3);
	if (! request_region(pccard_ioaddr, pccard_iolen, dev_info)) {
		printk(KERN_ERR "orinoco_plx: I/O resource 0x%lx @ 0x%lx busy\n",
		       pccard_iolen, pccard_ioaddr);
		pccard_ioaddr = 0;
		err = -EBUSY;
		goto fail;
	}

	dev = alloc_orinocodev(0, NULL);
	if (! dev) {
		err = -ENOMEM;
		goto fail;
	}

	priv = dev->priv;
	dev->base_addr = pccard_ioaddr;
	SET_MODULE_OWNER(dev);

	printk(KERN_DEBUG
	       "Detected Orinoco/Prism2 PLX device at %s irq:%d, io addr:0x%lx\n",
	       pdev->slot_name, pdev->irq, pccard_ioaddr);

	hermes_struct_init(&(priv->hw), dev->base_addr,
			HERMES_IO, HERMES_16BIT_REGSPACING);
	pci_set_drvdata(pdev, dev);

	err = request_irq(pdev->irq, orinoco_interrupt, SA_SHIRQ, dev->name, dev);
	if (err) {
		printk(KERN_ERR "orinoco_plx: Error allocating IRQ %d.\n", pdev->irq);
		err = -EBUSY;
		goto fail;
	}
	dev->irq = pdev->irq;

	err = register_netdev(dev);
	if (err)
		goto fail;
	netdev_registered = 1;

	return 0;		/* succeeded */

 fail:	
	printk(KERN_DEBUG "orinoco_plx: init_one(), FAIL!\n");

	if (priv) {
		if (netdev_registered)
			unregister_netdev(dev);
		
		if (dev->irq)
			free_irq(dev->irq, dev);
		
		kfree(priv);
	}

	if (pccard_ioaddr)
		release_region(pccard_ioaddr, pccard_iolen);

	if (attr_mem)
		iounmap(attr_mem);

	pci_disable_device(pdev);

	return err;
}

static void __devexit orinoco_plx_remove_one(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);

	if (! dev)
		BUG();

	unregister_netdev(dev);
		
	if (dev->irq)
		free_irq(dev->irq, dev);
		
	pci_set_drvdata(pdev, NULL);

	kfree(dev);

	release_region(pci_resource_start(pdev, 3), pci_resource_len(pdev, 3));

	pci_disable_device(pdev);
}


static struct pci_device_id orinoco_plx_pci_id_table[] __devinitdata = {
	{0x111a, 0x1023, PCI_ANY_ID, PCI_ANY_ID,},	/* Siemens SpeedStream SS1023 */
	{0x1385, 0x4100, PCI_ANY_ID, PCI_ANY_ID,},	/* Netgear MA301 */
	{0x15e8, 0x0130, PCI_ANY_ID, PCI_ANY_ID,},	/* Correga  - does this work? */
	{0x1638, 0x1100, PCI_ANY_ID, PCI_ANY_ID,},	/* SMC EZConnect SMC2602W,
							   Eumitcom PCI WL11000,
							   Addtron AWA-100*/
	{0x16ab, 0x1100, PCI_ANY_ID, PCI_ANY_ID,},	/* Global Sun Tech GL24110P */
	{0x16ab, 0x1101, PCI_ANY_ID, PCI_ANY_ID,},	/* Reported working, but unknown */
	{0x16ab, 0x1102, PCI_ANY_ID, PCI_ANY_ID,},	/* Linksys WDT11 */
	{0x16ec, 0x3685, PCI_ANY_ID, PCI_ANY_ID,},	/* USR 2415 */
	{0xec80, 0xec00, PCI_ANY_ID, PCI_ANY_ID,},	/* Belkin F5D6000 tested by
							   Brendan W. McAdams <rit@jacked-in.org> */
	{0x10b7, 0x7770, PCI_ANY_ID, PCI_ANY_ID,},	/* 3Com AirConnect PCI tested by
							   Damien Persohn <damien@persohn.net> */
	{0,},
};

MODULE_DEVICE_TABLE(pci, orinoco_plx_pci_id_table);

static struct pci_driver orinoco_plx_driver = {
	.name		= "orinoco_plx",
	.id_table	= orinoco_plx_pci_id_table,
	.probe		= orinoco_plx_init_one,
	.remove		= __devexit_p(orinoco_plx_remove_one),
	.suspend	= 0,
	.resume		= 0,
};

static char version[] __initdata = "orinoco_plx.c 0.13d (Daniel Barlow <dan@telent.net>, David Gibson <hermes@gibson.dropbear.id.au>)";
MODULE_AUTHOR("Daniel Barlow <dan@telent.net>");
MODULE_DESCRIPTION("Driver for wireless LAN cards using the PLX9052 PCI bridge");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual MPL/GPL");
#endif

static int __init orinoco_plx_init(void)
{
	printk(KERN_DEBUG "%s\n", version);
	return pci_module_init(&orinoco_plx_driver);
}

extern void __exit orinoco_plx_exit(void)
{
	pci_unregister_driver(&orinoco_plx_driver);
	current->state = TASK_UNINTERRUPTIBLE;
	schedule_timeout(HZ);
}

module_init(orinoco_plx_init);
module_exit(orinoco_plx_exit);

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
