/*
 *
 * BRIEF MODULE DESCRIPTION
 *	EV96100 Board specific pci fixups.
 *
 * Copyright 2001 MontaVista Software Inc.
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
 */
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci_ids.h>

#include <asm/gt64120/gt64120.h>

extern unsigned short get_gt_devid(void);

void __init pcibios_fixup_resources(struct pci_dev *dev)
{
}

void __init pcibios_fixup(void)
{
}

void __init pcibios_fixup_irqs(void)
{
	struct pci_dev *dev;
	unsigned int slot;
	u32 vendor;
	unsigned short gt_devid = get_gt_devid();

	/*
	** EV96100/A interrupt routing for pci bus 0
	**
	** Note: EV96100A board with irq jumper set on 'VxWorks'
	** for EV96100 compatibility.
	*/

	pci_for_each_dev(dev) {
		if (dev->bus->number != 0)
			return;

		slot = PCI_SLOT(dev->devfn);
		pci_read_config_dword(dev, PCI_SUBSYSTEM_VENDOR_ID, &vendor);

#ifdef DEBUG
		printk("devfn %x, slot %d devid %x\n",
				dev->devfn, slot, gt_devid);
#endif

		/* fixup irq line based on slot # */
		if (slot == 8) {
			dev->irq = 5;
			pci_write_config_byte(dev, PCI_INTERRUPT_LINE,
					dev->irq);
		}
		else if (slot == 9) {
			dev->irq = 2;
			pci_write_config_byte(dev, PCI_INTERRUPT_LINE,
					dev->irq);
		}
	}
}
unsigned int pcibios_assign_all_busses(void)
{
	return 0;
}
