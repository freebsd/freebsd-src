/*
 * FILE NAME
 *	arch/mips/vr41xx/nec-eagle/pci_fixup.c
 *
 * BRIEF MODULE DESCRIPTION
 *	The NEC Eagle/Hawk Board specific PCI fixups.
 *
 * Author: Yoichi Yuasa
 *         yyuasa@mvista.com or source@mvista.com
 *
 * Copyright 2001,2002 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
/*
 * Changes:
 *  MontaVista Software Inc. <yyuasa@mvista.com> or <source@mvista.com>
 *  - Moved mips_pci_channels[] to arch/mips/vr41xx/vr4122/eagle/setup.c.
 *  - Added support for NEC Hawk.
 *
 *  Paul Mundt <lethal@chaoticdreams.org>
 *  - Fix empty break statements, remove useless CONFIG_PCI.
 *
 *  MontaVista Software Inc. <yyuasa@mvista.com> or <source@mvista.com>
 *  - New creation, NEC Eagle is supported.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/pci.h>

#include <asm/vr41xx/eagle.h>
#include <asm/vr41xx/vrc4173.h>

void __init pcibios_fixup_resources(struct pci_dev *dev)
{
}

void __init pcibios_fixup(void)
{
}

void __init pcibios_fixup_irqs(void)
{
	struct pci_dev *dev;
	u8 slot, func, pin;

	pci_for_each_dev(dev) {
		slot = PCI_SLOT(dev->devfn);
		func = PCI_FUNC(dev->devfn);
		pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
		dev->irq = 0;

		switch (slot) {
		case 8:
			switch (pin) {
			case 1:
				dev->irq = CP_INTA_IRQ;
				break;
			case 2:
				dev->irq = CP_INTB_IRQ;
				break;
			case 3:
				dev->irq = CP_INTC_IRQ;
				break;
			case 4:
				dev->irq = CP_INTD_IRQ;
				break;
			}
			break;
		case 9:
			switch (pin) {
			case 1:
				dev->irq = CP_INTD_IRQ;
				break;
			case 2:
				dev->irq = CP_INTA_IRQ;
				break;
			case 3:
				dev->irq = CP_INTB_IRQ;
				break;
			case 4:
				dev->irq = CP_INTC_IRQ;
				break;
			}
			break;
		case 10:
			switch (pin) {
			case 1:
				dev->irq = CP_INTC_IRQ;
				break;
			case 2:
				dev->irq = CP_INTD_IRQ;
				break;
			case 3:
				dev->irq = CP_INTA_IRQ;
				break;
			case 4:
				dev->irq = CP_INTB_IRQ;
				break;
			}
			break;
		case 12:
			dev->irq = VRC4173_PCMCIA1_IRQ;
			break;
		case 13:
			dev->irq = VRC4173_PCMCIA2_IRQ;
			break;
		case 28:
			dev->irq = LANINTA_IRQ;
			break;
		case 29:
			switch (pin) {
			case 1:
				dev->irq = PCISLOT_IRQ;
				break;
			case 2:
				dev->irq = CP_INTB_IRQ;
				break;
			case 3:
				dev->irq = CP_INTC_IRQ;
				break;
			case 4:
				dev->irq = CP_INTD_IRQ;
				break;
			}
			break;
		case 30:
			switch (func) {
			case 0:
				dev->irq = VRC4173_CASCADE_IRQ;
				break;
			case 1:
				dev->irq = VRC4173_AC97_IRQ;
				break;
			case 2:
				dev->irq = VRC4173_USB_IRQ;
				break;
			}
			break;
		}

		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, dev->irq);
	}
}

unsigned int pcibios_assign_all_busses(void)
{
	return 0;
}
