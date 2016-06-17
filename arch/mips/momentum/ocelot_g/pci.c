/*
 * Copyright 2002 Momentum Computer
 * Author: Matthew Dharm <mdharm@momenco.com>
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
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <asm/pci.h>
#include <asm/io.h>
#include "gt64240.h"

#include <linux/init.h>

#define SELF 0
#define MASTER_ABORT_BIT 0x100

/*
 * These functions and structures provide the BIOS scan and mapping of the PCI
 * devices.
 */

#define MAX_PCI_DEVS 10

void gt64240_board_pcibios_fixup_bus(struct pci_bus* c);

/*  Functions to implement "pci ops"  */
static int galileo_pcibios_read_config_word(struct pci_dev *dev,
					    int offset, u16 * val);
static int galileo_pcibios_read_config_byte(struct pci_dev *dev,
					    int offset, u8 * val);
static int galileo_pcibios_read_config_dword(struct pci_dev *dev,
					     int offset, u32 * val);
static int galileo_pcibios_write_config_byte(struct pci_dev *dev,
					     int offset, u8 val);
static int galileo_pcibios_write_config_word(struct pci_dev *dev,
					     int offset, u16 val);
static int galileo_pcibios_write_config_dword(struct pci_dev *dev,
					      int offset, u32 val);

/*
 *  General-purpose PCI functions.
 */


/*
 * pci_range_ck -
 *
 * Check if the pci device that are trying to access does really exists
 * on the evaluation board.
 *
 * Inputs :
 * bus - bus number (0 for PCI 0 ; 1 for PCI 1)
 * dev - number of device on the specific pci bus
 *
 * Outpus :
 * 0 - if OK , 1 - if failure
 */
static __inline__ int pci_range_ck(unsigned char bus, unsigned char dev)
{
	/* Accessing device 31 crashes the GT-64240. */
	if (dev < 5)
		return 0;
	return -1;
}

/*
 * galileo_pcibios_(read/write)_config_(dword/word/byte) -
 *
 * reads/write a dword/word/byte register from the configuration space
 * of a device.
 *
 * Note that bus 0 and bus 1 are local, and we assume all other busses are
 * bridged from bus 1.  This is a safe assumption, since any other
 * configuration will require major modifications to the CP7000G
 *
 * Inputs :
 * bus - bus number
 * dev - device number
 * offset - register offset in the configuration space
 * val - value to be written / read
 *
 * Outputs :
 * PCIBIOS_SUCCESSFUL when operation was succesfull
 * PCIBIOS_DEVICE_NOT_FOUND when the bus or dev is errorneous
 * PCIBIOS_BAD_REGISTER_NUMBER when accessing non aligned
 */

static int galileo_pcibios_read_config_dword(struct pci_dev *device,
					      int offset, u32* val)
{
	int dev, bus, func;
	uint32_t address_reg, data_reg;
	uint32_t address;

	bus = device->bus->number;
	dev = PCI_SLOT(device->devfn);
	func = PCI_FUNC(device->devfn);

	/* verify the range */
	if (pci_range_ck(bus, dev))
		return PCIBIOS_DEVICE_NOT_FOUND;

	/* select the GT-64240 registers to communicate with the PCI bus */
	if (bus == 0) {
		address_reg = PCI_0CONFIGURATION_ADDRESS;
		data_reg = PCI_0CONFIGURATION_DATA_VIRTUAL_REGISTER;
		GT_WRITE(PCI_0ERROR_CAUSE, ~MASTER_ABORT_BIT);
	} else {
		address_reg = PCI_1CONFIGURATION_ADDRESS;
		data_reg = PCI_1CONFIGURATION_DATA_VIRTUAL_REGISTER;
		GT_WRITE(PCI_1ERROR_CAUSE, ~MASTER_ABORT_BIT);
		if (bus == 1)
			bus = 0;
	}

	address = (bus << 16) | (dev << 11) | (func << 8) |
		(offset & 0xfc) | 0x80000000;

	/* start the configuration cycle */
	GT_WRITE(address_reg, address);

	/* read the data */
	GT_READ(data_reg, val);

	return PCIBIOS_SUCCESSFUL;
}


static int galileo_pcibios_read_config_word(struct pci_dev *device,
					     int offset, u16* val)
{
	int dev, bus, func;
	uint32_t address_reg, data_reg;
	uint32_t address;

	bus = device->bus->number;
	dev = PCI_SLOT(device->devfn);
	func = PCI_FUNC(device->devfn);

	/* verify the range */
	if (pci_range_ck(bus, dev))
		return PCIBIOS_DEVICE_NOT_FOUND;

	/* select the GT-64240 registers to communicate with the PCI bus */
	if (bus == 0) {
		address_reg = PCI_0CONFIGURATION_ADDRESS;
		data_reg = PCI_0CONFIGURATION_DATA_VIRTUAL_REGISTER;
		GT_WRITE(PCI_0ERROR_CAUSE, ~MASTER_ABORT_BIT);
	} else {
		address_reg = PCI_1CONFIGURATION_ADDRESS;
		data_reg = PCI_1CONFIGURATION_DATA_VIRTUAL_REGISTER;
		GT_WRITE(PCI_1ERROR_CAUSE, ~MASTER_ABORT_BIT);
		if (bus == 1)
			bus = 0;
	}

	address = (bus << 16) | (dev << 11) | (func << 8) |
		(offset & 0xfc) | 0x80000000;

	/* start the configuration cycle */
	GT_WRITE(address_reg, address);

	/* read the data */
	GT_READ_16(data_reg + (offset & 0x3), val);

	return PCIBIOS_SUCCESSFUL;
}

static int galileo_pcibios_read_config_byte(struct pci_dev *device,
					     int offset, u8* val)
{
	int dev, bus, func;
	uint32_t address_reg, data_reg;
	uint32_t address;

	bus = device->bus->number;
	dev = PCI_SLOT(device->devfn);
	func = PCI_FUNC(device->devfn);

	/* verify the range */
	if (pci_range_ck(bus, dev))
		return PCIBIOS_DEVICE_NOT_FOUND;

	/* select the GT-64240 registers to communicate with the PCI bus */
	if (bus == 0) {
		address_reg = PCI_0CONFIGURATION_ADDRESS;
		data_reg = PCI_0CONFIGURATION_DATA_VIRTUAL_REGISTER;
	} else {
		address_reg = PCI_1CONFIGURATION_ADDRESS;
		data_reg = PCI_1CONFIGURATION_DATA_VIRTUAL_REGISTER;
		if (bus == 1)
			bus = 0;
	}

	address = (bus << 16) | (dev << 11) | (func << 8) |
		(offset & 0xfc) | 0x80000000;

	/* start the configuration cycle */
	GT_WRITE(address_reg, address);

	/* write the data */
	GT_READ_8(data_reg + (offset & 0x3), val);

	return PCIBIOS_SUCCESSFUL;
}

static int galileo_pcibios_write_config_dword(struct pci_dev *device,
					      int offset, u32 val)
{
	int dev, bus, func;
	uint32_t address_reg, data_reg;
	uint32_t address;

	bus = device->bus->number;
	dev = PCI_SLOT(device->devfn);
	func = PCI_FUNC(device->devfn);

	/* verify the range */
	if (pci_range_ck(bus, dev))
		return PCIBIOS_DEVICE_NOT_FOUND;

	/* select the GT-64240 registers to communicate with the PCI bus */
	if (bus == 0) {
		address_reg = PCI_0CONFIGURATION_ADDRESS;
		data_reg = PCI_0CONFIGURATION_DATA_VIRTUAL_REGISTER;
	} else {
		address_reg = PCI_1CONFIGURATION_ADDRESS;
		data_reg = PCI_1CONFIGURATION_DATA_VIRTUAL_REGISTER;
		if (bus == 1)
			bus = 0;
	}

	address = (bus << 16) | (dev << 11) | (func << 8) |
		(offset & 0xfc) | 0x80000000;

	/* start the configuration cycle */
	GT_WRITE(address_reg, address);

	/* write the data */
	GT_WRITE(data_reg, val);

	return PCIBIOS_SUCCESSFUL;
}


static int galileo_pcibios_write_config_word(struct pci_dev *device,
					     int offset, u16 val)
{
	int dev, bus, func;
	uint32_t address_reg, data_reg;
	uint32_t address;

	bus = device->bus->number;
	dev = PCI_SLOT(device->devfn);
	func = PCI_FUNC(device->devfn);

	/* verify the range */
	if (pci_range_ck(bus, dev))
		return PCIBIOS_DEVICE_NOT_FOUND;

	/* select the GT-64240 registers to communicate with the PCI bus */
	if (bus == 0) {
		address_reg = PCI_0CONFIGURATION_ADDRESS;
		data_reg = PCI_0CONFIGURATION_DATA_VIRTUAL_REGISTER;
	} else {
		address_reg = PCI_1CONFIGURATION_ADDRESS;
		data_reg = PCI_1CONFIGURATION_DATA_VIRTUAL_REGISTER;
		if (bus == 1)
			bus = 0;
	}

	address = (bus << 16) | (dev << 11) | (func << 8) |
		(offset & 0xfc) | 0x80000000;

	/* start the configuration cycle */
	GT_WRITE(address_reg, address);

	/* write the data */
	GT_WRITE_16(data_reg + (offset & 0x3), val);

	return PCIBIOS_SUCCESSFUL;
}

static int galileo_pcibios_write_config_byte(struct pci_dev *device,
					     int offset, u8 val)
{
	int dev, bus, func;
	uint32_t address_reg, data_reg;
	uint32_t address;

	bus = device->bus->number;
	dev = PCI_SLOT(device->devfn);
	func = PCI_FUNC(device->devfn);

	/* verify the range */
	if (pci_range_ck(bus, dev))
		return PCIBIOS_DEVICE_NOT_FOUND;

	/* select the GT-64240 registers to communicate with the PCI bus */
	if (bus == 0) {
		address_reg = PCI_0CONFIGURATION_ADDRESS;
		data_reg = PCI_0CONFIGURATION_DATA_VIRTUAL_REGISTER;
	} else {
		address_reg = PCI_1CONFIGURATION_ADDRESS;
		data_reg = PCI_1CONFIGURATION_DATA_VIRTUAL_REGISTER;
		if (bus == 1)
			bus = 0;
	}

	address = (bus << 16) | (dev << 11) | (func << 8) |
		(offset & 0xfc) | 0x80000000;

	/* start the configuration cycle */
	GT_WRITE(address_reg, address);

	/* write the data */
	GT_WRITE_8(data_reg + (offset & 0x3), val);

	return PCIBIOS_SUCCESSFUL;
}

struct pci_ops galileo_pci_ops = {
	galileo_pcibios_read_config_byte,
	galileo_pcibios_read_config_word,
	galileo_pcibios_read_config_dword,
	galileo_pcibios_write_config_byte,
	galileo_pcibios_write_config_word,
	galileo_pcibios_write_config_dword
};

struct pci_fixup pcibios_fixups[] = {
	{0}
};

void __init pcibios_fixup_bus(struct pci_bus *c)
{
	gt64240_board_pcibios_fixup_bus(c);
}


/********************************************************************
* pci0P2PConfig - This function set the PCI_0 P2P configurate.
*                 For more information on the P2P read PCI spec.
*
* Inputs:  unsigned int SecondBusLow - Secondery PCI interface Bus Range Lower
*                                      Boundry.
*          unsigned int SecondBusHigh - Secondry PCI interface Bus Range upper
*                                      Boundry.
*          unsigned int busNum - The CPI bus number to which the PCI interface
*                                      is connected.
*          unsigned int devNum - The PCI interface's device number.
*
* Returns:  true.
*/
void pci0P2PConfig(unsigned int SecondBusLow,unsigned int SecondBusHigh,
                   unsigned int busNum,unsigned int devNum)
{
	uint32_t regData;

	regData = (SecondBusLow & 0xff) | ((SecondBusHigh & 0xff) << 8) |
			((busNum & 0xff) << 16) | ((devNum & 0x1f) << 24);
	GT_WRITE(PCI_0P2P_CONFIGURATION, regData);
}

/********************************************************************
* pci1P2PConfig - This function set the PCI_1 P2P configurate.
*                 For more information on the P2P read PCI spec.
*
* Inputs:  unsigned int SecondBusLow - Secondery PCI interface Bus Range Lower
*               Boundry.
*          unsigned int SecondBusHigh - Secondry PCI interface Bus Range upper
*               Boundry.
*          unsigned int busNum - The CPI bus number to which the PCI interface
*               is connected.
*          unsigned int devNum - The PCI interface's device number.
*
* Returns:  true.
*/
void pci1P2PConfig(unsigned int SecondBusLow,unsigned int SecondBusHigh,
                   unsigned int busNum,unsigned int devNum)
{
	uint32_t regData;

	regData = (SecondBusLow & 0xff) | ((SecondBusHigh & 0xff) << 8) |
			((busNum & 0xff) << 16) | ((devNum & 0x1f) << 24);
	GT_WRITE(PCI_1P2P_CONFIGURATION, regData);
}

void __init pcibios_init(void)
{
	/* Reset PCI I/O and PCI MEM values */
	ioport_resource.start = 0xe0000000;
	ioport_resource.end   = 0xe0000000 + 0x20000000 - 1;
	iomem_resource.start  = 0xc0000000;
	iomem_resource.end    = 0xc0000000 + 0x20000000 - 1;

	pci_scan_bus(0, &galileo_pci_ops, NULL);
	pci_scan_bus(1, &galileo_pci_ops, NULL);
}

unsigned __init int pcibios_assign_all_busses(void)
{
	return 1;
}
