/*
 *
 * BRIEF MODULE DESCRIPTION
 *	Board specific pci fixups.
 *
 * Copyright 2000 MontaVista Software Inc.
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
#include <linux/config.h>

#ifdef CONFIG_PCI

#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/it8172/it8172.h>
#include <asm/it8172/it8172_pci.h>
#include <asm/it8172/it8172_int.h>

void __init pcibios_fixup_resources(struct pci_dev *dev)
{
}

void __init pcibios_fixup(void)
{
}

void __init pcibios_fixup_irqs(void)
{
	unsigned int slot, func;
	unsigned char pin;
	struct pci_dev *dev;
        const int internal_func_irqs[7] = {
            IT8172_AC97_IRQ,
            IT8172_DMA_IRQ,
            IT8172_CDMA_IRQ,
            IT8172_USB_IRQ,
            IT8172_BRIDGE_MASTER_IRQ,
            IT8172_IDE_IRQ,
            IT8172_MC68K_IRQ
        };

	pci_for_each_dev(dev) {
		if (dev->bus->number != 0) {
			return;
		}

		pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
		slot = PCI_SLOT(dev->devfn);
		func = PCI_FUNC(dev->devfn);

		switch (slot) {
			case 0x01:
			    /*
			     * Internal device 1 is actually 7 different
			     * internal devices on the IT8172G (a multi-
			     * function device).
			     */
			    if (func < 7)
				dev->irq = internal_func_irqs[func];
			    break;
			case 0x10:
				switch (pin) {
					case 1: /* pin A */
						dev->irq = IT8172_PCI_INTA_IRQ;
						break;
					case 2: /* pin B */
						dev->irq = IT8172_PCI_INTB_IRQ;
						break;
					case 3: /* pin C */
						dev->irq = IT8172_PCI_INTC_IRQ;
						break;
					case 4: /* pin D */
						dev->irq = IT8172_PCI_INTD_IRQ;
						break;
					default:
						dev->irq = 0xff;
						break;

				}
				break;
			case 0x11:
				switch (pin) {
					case 1: /* pin A */
						dev->irq = IT8172_PCI_INTA_IRQ;
						break;
					case 2: /* pin B */
						dev->irq = IT8172_PCI_INTB_IRQ;
						break;
					case 3: /* pin C */
						dev->irq = IT8172_PCI_INTC_IRQ;
						break;
					case 4: /* pin D */
						dev->irq = IT8172_PCI_INTD_IRQ;
						break;
					default:
						dev->irq = 0xff;
						break;

				}
				break;
			case 0x12:
				switch (pin) {
					case 1: /* pin A */
						dev->irq = IT8172_PCI_INTB_IRQ;
						break;
					case 2: /* pin B */
						dev->irq = IT8172_PCI_INTC_IRQ;
						break;
					case 3: /* pin C */
						dev->irq = IT8172_PCI_INTD_IRQ;
						break;
					case 4: /* pin D */
						dev->irq = IT8172_PCI_INTA_IRQ;
						break;
					default:
						dev->irq = 0xff;
						break;

				}
				break;
			case 0x13:
				switch (pin) {
					case 1: /* pin A */
						dev->irq = IT8172_PCI_INTC_IRQ;
						break;
					case 2: /* pin B */
						dev->irq = IT8172_PCI_INTD_IRQ;
						break;
					case 3: /* pin C */
						dev->irq = IT8172_PCI_INTA_IRQ;
						break;
					case 4: /* pin D */
						dev->irq = IT8172_PCI_INTB_IRQ;
						break;
					default:
						dev->irq = 0xff;
						break;

				}
				break;
			case 0x14:
				switch (pin) {
					case 1: /* pin A */
						dev->irq = IT8172_PCI_INTD_IRQ;
						break;
					case 2: /* pin B */
						dev->irq = IT8172_PCI_INTA_IRQ;
						break;
					case 3: /* pin C */
						dev->irq = IT8172_PCI_INTB_IRQ;
						break;
					case 4: /* pin D */
						dev->irq = IT8172_PCI_INTC_IRQ;
						break;
					default:
						dev->irq = 0xff;
						break;

				}
				break;
			default:
				continue; /* do nothing */
		}
#ifdef DEBUG
		printk("irq fixup: slot %d, int line %d, int number %d\n",
			slot, pin, dev->irq);
#endif
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, dev->irq);
	}
}
#endif
