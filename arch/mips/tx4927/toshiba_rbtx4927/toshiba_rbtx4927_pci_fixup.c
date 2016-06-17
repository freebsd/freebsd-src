/*
 *
 * BRIEF MODULE DESCRIPTION
 *      Board specific pci fixups for the Toshiba rbtx4927
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *              ppopov@mvista.com or source@mvista.com
 *
 * Copyright (C) 2000-2001 Toshiba Corporation 
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/tx4927/tx4927.h>
#include <asm/tx4927/tx4927_pci.h>

#undef  DEBUG
#ifdef  DEBUG
#define DBG(x...)       printk(x)
#else
#define DBG(x...)
#endif

void __init pcibios_fixup_resources(struct pci_dev *dev)
{
	/* will need to fixup IO resources */
}

void __init pcibios_fixup(void)
{
	/* nothing to do here */
}

/* look up table for backplane pci irq for slots 17-20 by pin # */
static unsigned char backplane_pci_irq[4][4] = {
	/* PJ6 SLOT:  17, PIN: 1 */ {TX4927_IRQ_IOC_PCIA,
				     /* PJ6 SLOT:  17, PIN: 2 */
				     TX4927_IRQ_IOC_PCIB,
				     /* PJ6 SLOT:  17, PIN: 3 */
				     TX4927_IRQ_IOC_PCIC,
				     /* PJ6 SLOT:  17, PIN: 4 */
				     TX4927_IRQ_IOC_PCID},
	/* SB  SLOT:  18, PIN: 1 */ {TX4927_IRQ_IOC_PCIB,
				     /* SB  SLOT:  18, PIN: 2 */
				     TX4927_IRQ_IOC_PCIC,
				     /* SB  SLOT:  18, PIN: 3 */
				     TX4927_IRQ_IOC_PCID,
				     /* SB  SLOT:  18, PIN: 4 */
				     TX4927_IRQ_IOC_PCIA},
	/* PJ5 SLOT:  19, PIN: 1 */ {TX4927_IRQ_IOC_PCIC,
				     /* PJ5 SLOT:  19, PIN: 2 */
				     TX4927_IRQ_IOC_PCID,
				     /* PJ5 SLOT:  19, PIN: 3 */
				     TX4927_IRQ_IOC_PCIA,
				     /* PJ5 SLOT:  19, PIN: 4 */
				     TX4927_IRQ_IOC_PCIB},
	/* PJ4 SLOT:  20, PIN: 1 */ {TX4927_IRQ_IOC_PCID,
				     /* PJ4 SLOT:  20, PIN: 2 */
				     TX4927_IRQ_IOC_PCIA,
				     /* PJ4 SLOT:  20, PIN: 3 */
				     TX4927_IRQ_IOC_PCIB,
				     /* PJ4 SLOT:  20, PIN: 4 */
				     TX4927_IRQ_IOC_PCIC}
};

int pci_get_irq(struct pci_dev *dev, int pin)
{
	unsigned char irq = pin;

	DBG("pci_get_irq: pin is %d\n", pin);
	/* IRQ rotation */
	irq--;			/* 0-3 */
	if (dev->bus->parent == NULL &&
	    PCI_SLOT(dev->devfn) == TX4927_PCIC_IDSEL_AD_TO_SLOT(23)) {
		printk("Onboard PCI_SLOT(dev->devfn) is %d\n",
		       PCI_SLOT(dev->devfn));
		/* IDSEL=A23 is tx4927 onboard pci slot */
		irq = (irq + PCI_SLOT(dev->devfn)) % 4;
		irq++;		/* 1-4 */
		DBG("irq is now %d\n", irq);

		switch (irq) {
		case 1:
			irq = TX4927_IRQ_IOC_PCIA;
			break;
		case 2:
			irq = TX4927_IRQ_IOC_PCIB;
			break;
		case 3:
			irq = TX4927_IRQ_IOC_PCIC;
			break;
		case 4:
			irq = TX4927_IRQ_IOC_PCID;
			break;
		}
	} else {
		/* PCI Backplane */
		DBG("PCI Backplane PCI_SLOT(dev->devfn) is %d\n",
		    PCI_SLOT(dev->devfn));
		irq = backplane_pci_irq[PCI_SLOT(dev->devfn) - 17][irq];
	}
	DBG("assigned irq %d\n", irq);
	return irq;
}


#ifdef  TX4927_SUPPORT_PCI_66
extern int tx4927_pci66;
extern void tx4927_pci66_setup(void);
#endif
extern void tx4927_pci_setup(void);

#ifdef  TX4927_SUPPORT_PCI_66
int tx4927_pci66_check(void)
{
	struct pci_dev *dev;
	unsigned short stat;
	int cap66 = 1;

	if (tx4927_pci66 < 0)
		return 0;

	/* check 66MHz capability */
	pci_for_each_dev(dev) {
		if (cap66) {
			pci_read_config_word(dev, PCI_STATUS, &stat);
			if (!(stat & PCI_STATUS_66MHZ)) {
				printk(KERN_INFO
				       "PCI: %02x:%02x not 66MHz capable.\n",
				       dev->bus->number, dev->devfn);
				cap66 = 0;
			}
		}
	}
	return cap66;
}
#endif

#ifdef DEBUG
void do_it(u32 offset, u32 reg)
{
	volatile u32 a1;
	volatile u32 a2;
	volatile u32 v1;
	volatile u32 v2;

	a1 = 0xff1f0000 + offset + reg;
	a2 = a1 + 4;

	v1 = *(volatile u32 *) a1;
	v2 = *(volatile u32 *) a2;

	if (v1)
		printk("TX4927 0x%08x 0x%08x\n", a1, v1);
	if (v2)
		printk("TX4927 0x%08x 0x%08x\n", a2, v2);
}

void do_it1(u32 base, u32 r)
{
	do_it(base, r);
}

void do_it2(u32 base, u32 start, u32 stop)
{
	u32 r;

	for (r = start; r <= stop; r += 8) {
		do_it(base, r);
	}
}
void dump_config(void)
{
	unsigned long id;
	unsigned long j;
	struct pci_dev *dev;

	printk("----------------------pci\n");
	pci_for_each_dev(dev) {
		for (j = 0; j < 64; j++) {
			pci_read_config_dword(dev, j * 4, &id);
			if (id == 0)
				continue;
			printk
			    ("dev 0x%02x 0x%02x:0x%02x -- 0x%02x-0x%02x 0x%08x\n",
			     dev->devfn, PCI_SLOT(dev->devfn),
			     PCI_FUNC(dev->devfn), (j * 4) + 3, (j * 4),
			     id);
		}
		printk("dev 0x%02x \n", dev->devfn);
	}
	printk("----------------------sdram\n");
	do_it2(0x8000, 0x00, 0x18);
	do_it1(0x8000, 0x40);
	do_it1(0x8000, 0x58);
	printk("----------------------ebus\n");
	do_it2(0x9000, 0x00, 0x38);
	printk("----------------------ecc\n");
	do_it2(0xa000, 0x00, 0x08);
	printk("----------------------dmac\n");
	do_it2(0xb000, 0x00, 0xf8);
	/* b1xx */
	printk("----------------------pci\n");
	/* d */
	printk("----------------------cfg\n");
	do_it2(0xe000, 0x00, 0x20);
	do_it1(0xe000, 0x30);
	do_it1(0xe000, 0x48);
	printk("----------------------timers\n");
	do_it2(0xf000, 0x00, 0xf0);
	do_it2(0xf100, 0x00, 0xf0);
	do_it2(0xf200, 0x00, 0xf0);
	printk("----------------------serial\n");
	do_it2(0xf300, 0x00, 0x20);
	do_it2(0xf400, 0x00, 0x20);
	printk("----------------------parallel\n");
	do_it2(0xf500, 0x00, 0x0c);
	printk("----------------------pic\n");
	do_it2(0xf500, 0x10, 0x24);
	do_it2(0xf600, 0x00, 0x2c);
	do_it1(0xf600, 0x40);
	do_it1(0xf600, 0x60);
	do_it1(0xf600, 0x80);
	do_it1(0xf600, 0xa0);
	printk("----------------------aclink\n");
	do_it2(0xf700, 0x00, 0xfc);
	printk("----------------------done\n");
}
#endif


void __init pcibios_fixup_irqs(void)
{
	unsigned char pin;
	unsigned char irq;
	struct pci_dev *dev;
	unsigned int id;

#ifdef  TX4927_SUPPORT_PCI_66
	{
		if (tx4927_pci66_check()) {
			tx4927_pci66_setup();
			tx4927_pci_setup();	/* Reinitialize PCIC */
		}
	}
#endif

	pci_for_each_dev(dev) {
		DBG("FIXUP:\n");
		DBG(" devfn=0x%02x (0x%02x:0x%02x)\n",
		    dev->devfn, PCI_SLOT(dev->devfn),
		    PCI_FUNC(dev->devfn));

		pci_read_config_dword(dev, PCI_VENDOR_ID, &id);
		DBG(" id=0x%08x\n", id);

		pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq);
		DBG(" line=0x%02x/%d\n", irq, irq);

		pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
		DBG(" pin=%d\n", pin);

#ifdef DEBUG
		{
			unsigned int tmp;
			pci_read_config_dword(dev, 0x10, &tmp);
			DBG(" bar0:0x10=0x%08x\n", tmp);
			pci_read_config_dword(dev, 0x14, &tmp);
			DBG(" bar1:0x14=0x%08x\n", tmp);
			pci_read_config_dword(dev, 0x1c, &tmp);
			DBG(" bar2:0x1c=0x%08x\n", tmp);
			pci_read_config_dword(dev, 0x20, &tmp);
			DBG(" bar3:0x20=0x%08x\n", tmp);
			pci_read_config_dword(dev, 0x24, &tmp);
			DBG(" bar4:0x24=0x%08x\n", tmp);
		}
#endif

		irq = 0;

		if (id == 0x91301055) {	/* ide */
			irq = 14;
		}

		if (pin == 0) {
			DBG(" auto irq (now=%d) -- skipping pin=0\n", irq);
		} else if (irq) {
			DBG(" auto irq (now=%d) -- skipping hardcoded irq\n", irq);
		} else {
			DBG(" auto irq (was=%d)\n", irq);
			irq = pci_get_irq(dev, pin);
			pci_write_config_byte(dev, PCI_INTERRUPT_LINE,
					      irq);
			dev->irq = irq;
			DBG(" auto irq (now=%d)\n", irq);
		}

		pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq);
		printk(KERN_INFO
		       "PCI: 0x%02x:0x%02x(0x%02x,0x%02x) IRQ=%d\n",
		       dev->bus->number, dev->devfn, PCI_SLOT(dev->devfn),
		       PCI_FUNC(dev->devfn), irq);

	}

}
