/*
 * BRIEF MODULE DESCRIPTION
 *	Board specific pci fixups.
 *
 * Copyright 2001-2003 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
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
 *
 *	CTG 11/18/2003  Added supoprt for Au1550 SOC. The PCI block did not 
 *		   change from Au1500 to the Au1550. However, the boards are now 
 *         using INTB for second PCI slot.
 *         This is reflected in pcibios_fixup_irqs.
 *
 */
#include <linux/config.h>

#ifdef CONFIG_PCI

#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/au1000.h>
#ifdef CONFIG_MIPS_PB1000
#include <asm/pb1000.h>
#endif

#undef	DEBUG
#ifdef 	DEBUG
#define	DBG(x...)	printk(x)
#else
#define	DBG(x...)
#endif

static void fixup_resource(int r_num, struct pci_dev *dev) ;
#if defined( CONFIG_SOC_AU1500 ) || defined( CONFIG_SOC_AU1550 )
static unsigned long virt_io_addr;
#endif

void __init pcibios_fixup_resources(struct pci_dev *dev)
{
	/* will need to fixup IO resources */
}

void __init pcibios_fixup(void)
{
#if defined( CONFIG_SOC_AU1500 ) || defined( CONFIG_SOC_AU1550 )
	int i;
	struct pci_dev *dev;
	
	virt_io_addr = (unsigned long)ioremap(Au1500_PCI_IO_START, 
			Au1500_PCI_IO_END - Au1500_PCI_IO_START + 1);

	if (!virt_io_addr) {
		printk(KERN_ERR "Unable to ioremap pci space\n");
		return;
	}

#ifdef CONFIG_NONCOHERENT_IO
	/* 
	 *  Set the NC bit in controller for pre-AC silicon
	 */
	au_writel( 1<<16 | au_readl(Au1500_PCI_CFG), Au1500_PCI_CFG);
	printk("Non-coherent PCI accesses enabled\n");
#endif

	set_io_port_base(virt_io_addr);
#endif

#ifdef CONFIG_MIPS_PB1000 /* This is truly board specific */
	unsigned long pci_mem_start = (unsigned long) PCI_MEM_START;

	au_writel(0, PCI_BRIDGE_CONFIG); // set extend byte to 0
	au_writel(0, SDRAM_MBAR);        // set mbar to 0
	au_writel(0x2, SDRAM_CMD);       // enable memory accesses
	au_sync_delay(1);

	// set extend byte to mbar of ext slot
	au_writel(((pci_mem_start >> 24) & 0xff) |
	       (1 << 8 | 1 << 9 | 1 << 10 | 1 << 27), PCI_BRIDGE_CONFIG);
	DBG("Set bridge config to %x\n", au_readl(PCI_BRIDGE_CONFIG));
#endif
}

void __init pcibios_fixup_irqs(void)
{
#if defined( CONFIG_SOC_AU1500 ) || defined( CONFIG_SOC_AU1550 )
	unsigned int slot, func;
	unsigned char pin;
	struct pci_dev *dev;

	pci_for_each_dev(dev) {
		if (dev->bus->number != 0)
			return;

		dev->irq = 0xff;
		slot = PCI_SLOT(dev->devfn);
#if defined( CONFIG_SOC_AU1500 )
		switch (slot) {
			case 12:
			case 13:
			default:
				dev->irq = AU1000_PCI_INTA;
				break;
		}
#elif defined( CONFIG_SOC_AU1550 )
		switch (slot) {
			default:
			case 12:
				dev->irq = AU1000_PCI_INTA;
				break;
			case 13:
				dev->irq = AU1000_PCI_INTB;
				break;
		}
#endif

		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, dev->irq);
		DBG("slot %d irq %d\n", slot, dev->irq);
	}
#endif
}
unsigned int pcibios_assign_all_busses(void)
{
	return 0;
}

static void fixup_resource(int r_num, struct pci_dev *dev) 
{
}
#endif
