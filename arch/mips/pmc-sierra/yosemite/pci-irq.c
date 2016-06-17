/*
 * Copyright 2003 PMC-Sierra
 * Author: Manish Lachwani (lachwani@pmc-sierra.com)
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
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
#include <asm/pci.h>

/*
 * PCI Bus fixup for the Titan
 * XXX IRQ values need to change based on the board layout
 */
void __init titan_pcibios_fixup_bus(struct pci_bus *bus)
{
        struct pci_bus *current_bus = bus;
        struct pci_dev *devices;
        struct list_head *devices_link;

	list_for_each(devices_link, &(current_bus->devices)) {
                devices = pci_dev_b(devices_link);
                if (devices == NULL)
                        continue;

                if ((current_bus->number == 0) &&
                        (PCI_SLOT(devices->devfn) == 1)) {
                        /* PCI-X A */
                        devices->irq = 3;
                } else if ((current_bus->number == 0) &&
                        (PCI_SLOT(devices->devfn) == 2)) {
                        /* PCI-X B */
                        devices->irq = 4;
                } else if ((current_bus->number == 1) &&
                        (PCI_SLOT(devices->devfn) == 1)) {
                        /* PCI A */
                        devices->irq = 5;
                } else if ((current_bus->number == 1) &&
                        (PCI_SLOT(devices->devfn) == 2)) {
                        /* PCI B */
                        devices->irq = 6;
                }
	}
}
