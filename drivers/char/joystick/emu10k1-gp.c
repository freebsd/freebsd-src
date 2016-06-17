/*
 * $Id: emu10k1-gp.c,v 1.2 2001/04/24 07:48:56 vojtech Exp $
 *
 *  Copyright (c) 2001 Vojtech Pavlik
 *
 *  Sponsored by SuSE
 */

/*
 * EMU10k1 - SB Live! - gameport driver for Linux
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * 
 * Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <vojtech@ucw.cz>, or by paper mail:
 * Vojtech Pavlik, Ucitelska 1576, Prague 8, 182 00 Czech Republic
 */

#include <asm/io.h>

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/config.h>
#include <linux/init.h>
#include <linux/gameport.h>
#include <linux/slab.h>
#include <linux/pci.h>

MODULE_AUTHOR("Vojtech Pavlik <vojtech@ucw.cz>");
MODULE_LICENSE("GPL");

struct emu {
	struct pci_dev *dev;
	struct emu *next;
	struct gameport gameport;
	int size;
};
	
static struct pci_device_id emu_tbl[] __devinitdata = {
	{ 0x1102, 0x7002, PCI_ANY_ID, PCI_ANY_ID }, /* SB Live! gameport */
        { 0x1102, 0x7003, PCI_ANY_ID, PCI_ANY_ID }, /* Audigy! gameport */
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, emu_tbl);

static int __devinit emu_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int ioport, iolen;
	int rc;
	struct emu *port;
        
	rc = pci_enable_device(pdev);
	if (rc) {
		printk(KERN_ERR "emu10k1-gp: Cannot enable emu10k1 gameport (bus %d, devfn %d) error=%d\n",
			pdev->bus->number, pdev->devfn, rc);
		return rc;
	}

	ioport = pci_resource_start(pdev, 0);
	iolen = pci_resource_len(pdev, 0);

	if (!request_region(ioport, iolen, "emu10k1-gp"))
		return -EBUSY;

	if (!(port = kmalloc(sizeof(struct emu), GFP_KERNEL))) {
		printk(KERN_ERR "emu10k1-gp: Memory allocation failed.\n");
		release_region(ioport, iolen);
		return -ENOMEM;
	}
	memset(port, 0, sizeof(struct emu));

	port->gameport.io = ioport;
	port->size = iolen;
	port->dev = pdev;
	pci_set_drvdata(pdev, port);

	gameport_register_port(&port->gameport);

	printk(KERN_INFO "gameport%d: Emu10k1 Gameport at %#x size %d speed %d kHz\n",
		port->gameport.number, port->gameport.io, iolen, port->gameport.speed);

	return 0;
}

static void __devexit emu_remove(struct pci_dev *pdev)
{
	struct emu *port = pci_get_drvdata(pdev);
	gameport_unregister_port(&port->gameport);
	release_region(port->gameport.io, port->size);
	kfree(port);
}

static struct pci_driver emu_driver = {
        name:           "Emu10k1 Gameport",
        id_table:       emu_tbl,
        probe:          emu_probe,
        remove:         __devexit_p(emu_remove),
};

int __init emu_init(void)
{
	return pci_module_init(&emu_driver);
}

void __exit emu_exit(void)
{
	pci_unregister_driver(&emu_driver);
}

module_init(emu_init);
module_exit(emu_exit);
