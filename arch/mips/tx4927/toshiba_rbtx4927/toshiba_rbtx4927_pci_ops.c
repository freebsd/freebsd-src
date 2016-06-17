/*
 * Copyright 2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *              ahennessy@mvista.com       
 *
 * Copyright (C) 2000-2001 Toshiba Corporation 
 *
 * Based on arch/mips/ddb5xxx/ddb5477/pci_ops.c
 *
 *     Define the pci_ops for the Toshiba rbtx4927
 *
 * Much of the code is derived from the original DDB5074 port by 
 * Geert Uytterhoeven <geert@sonycom.com>
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

#include <asm/addrspace.h>
#include <asm/pci_channel.h>
#include <asm/tx4927/tx4927_pci.h>
#include <asm/debug.h>

/* initialize in setup */
struct resource pci_io_resource = {
	"pci IO space",
	(PCIBIOS_MIN_IO),
	((PCIBIOS_MIN_IO) + (TX4927_PCIIO_SIZE)) - 1,
	IORESOURCE_IO
};

/* initialize in setup */
struct resource pci_mem_resource = {
	"pci memory space",
	TX4927_PCIMEM,
	TX4927_PCIMEM + TX4927_PCIMEM_SIZE - 1,
	IORESOURCE_MEM
};

extern struct pci_ops tx4927_pci_ops;

struct pci_channel mips_pci_channels[] = {
	/* h/w only supports devices 0x00 to 0x14 */
	{&tx4927_pci_ops, &pci_io_resource, &pci_mem_resource,
	 PCI_DEVFN(0x00, 0), PCI_DEVFN(0x14, 7)},
	{NULL, NULL, NULL, 0, 0}
};

unsigned int pcibios_assign_all_busses(void)
{
	return 1;
}

static int
mkaddr(unsigned char bus, unsigned char dev_fn, unsigned char where,
       int *flagsp)
{
	if (bus > 0) {
		/* Type 1 configuration */
		tx4927_pcicptr->g2pcfgadrs = ((bus & 0xff) << 0x10) |
		    ((dev_fn & 0xff) << 0x08) | (where & 0xfc) | 1;
	} else {
		if (dev_fn >= PCI_DEVFN(TX4927_PCIC_MAX_DEVNU, 0))
			return -1;

		/* Type 0 configuration */
		tx4927_pcicptr->g2pcfgadrs = ((bus & 0xff) << 0x10) |
		    ((dev_fn & 0xff) << 0x08) | (where & 0xfc);
	}
	/* clear M_ABORT and Disable M_ABORT Int. */
	tx4927_pcicptr->pcistatus =
	    (tx4927_pcicptr->pcistatus & 0x0000ffff) |
	    (PCI_STATUS_REC_MASTER_ABORT << 16);
	tx4927_pcicptr->pcimask &= ~PCI_STATUS_REC_MASTER_ABORT;
	return 0;
}

static int check_abort(int flags)
{
	int code = PCIBIOS_SUCCESSFUL;
	if (tx4927_pcicptr->
	    pcistatus & (PCI_STATUS_REC_MASTER_ABORT << 16)) {
		tx4927_pcicptr->pcistatus =
		    (tx4927_pcicptr->
		     pcistatus & 0x0000ffff) | (PCI_STATUS_REC_MASTER_ABORT
						<< 16);
		tx4927_pcicptr->pcimask |= PCI_STATUS_REC_MASTER_ABORT;
		code = PCIBIOS_DEVICE_NOT_FOUND;
		//      printk("returning PCIBIOS_DEVICE_NOT_FOUND\n");
	}
	return code;
}

/*
 * We can't address 8 and 16 bit words directly.  Instead we have to
 * read/write a 32bit word and mask/modify the data we actually want.
 */
static int tx4927_pcibios_read_config_byte(struct pci_dev *dev,
					   int where, unsigned char *val)
{
	int flags, retval;
	unsigned char bus, func_num;

	db_assert((where & 3) == 0);
	db_assert(where < (1 << 8));

	/* check if the bus is top-level */
	if (dev->bus->parent != NULL) {
		bus = dev->bus->number;
		db_assert(bus != 0);
	} else {
		bus = 0;
	}

	func_num = PCI_FUNC(dev->devfn);
	if (mkaddr(bus, dev->devfn, where, &flags))
		return -1;
#ifdef __BIG_ENDIAN
	*val =
	    *(volatile u8 *) ((ulong) & tx4927_pcicptr->
			      g2pcfgdata | ((where & 3) ^ 3));
#else
	*val =
	    *(volatile u8 *) ((ulong) & tx4927_pcicptr->
			      g2pcfgdata | (where & 3));
#endif
	retval = check_abort(flags);
	if (retval == PCIBIOS_DEVICE_NOT_FOUND)
		*val = 0xff;
//printk("CFG R1 0x%02x 0x%02x 0x%08x\n", dev->devfn, where, *val );
	return retval;
}

static int tx4927_pcibios_read_config_word(struct pci_dev *dev,
					   int where, unsigned short *val)
{
	int flags, retval;
	unsigned char bus, func_num;

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	db_assert((where & 3) == 0);
	db_assert(where < (1 << 8));

	/* check if the bus is top-level */
	if (dev->bus->parent != NULL) {
		bus = dev->bus->number;
		db_assert(bus != 0);
	} else {
		bus = 0;
	}

	func_num = PCI_FUNC(dev->devfn);
	if (mkaddr(bus, dev->devfn, where, &flags))
		return -1;
#ifdef __BIG_ENDIAN
	*val =
	    *(volatile u16 *) ((ulong) & tx4927_pcicptr->
			       g2pcfgdata | ((where & 3) ^ 2));
#else
	*val =
	    *(volatile u16 *) ((ulong) & tx4927_pcicptr->
			       g2pcfgdata | (where & 3));
#endif
	retval = check_abort(flags);
	if (retval == PCIBIOS_DEVICE_NOT_FOUND)
		*val = 0xffff;
//printk("CFG R2 0x%02x 0x%02x 0x%08x\n", dev->devfn, where, *val );
	return retval;
}

static int tx4927_pcibios_read_config_dword(struct pci_dev *dev,
					    int where, unsigned int *val)
{
	int flags, retval;
	unsigned char bus, func_num;

	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	db_assert((where & 3) == 0);
	db_assert(where < (1 << 8));

	/* check if the bus is top-level */
	if (dev->bus->parent != NULL) {
		bus = dev->bus->number;
		db_assert(bus != 0);
	} else {
		bus = 0;
	}

	func_num = PCI_FUNC(dev->devfn);
	if (mkaddr(bus, dev->devfn, where, &flags))
		return -1;
	*val = tx4927_pcicptr->g2pcfgdata;
	retval = check_abort(flags);
	if (retval == PCIBIOS_DEVICE_NOT_FOUND)
		*val = 0xffffffff;

//printk("CFG R4 0x%02x 0x%02x 0x%08x\n", dev->devfn, where, *val );
	return retval;
}

static int tx4927_pcibios_write_config_byte(struct pci_dev *dev,
					    int where, unsigned char val)
{
	int flags;
	unsigned char bus, func_num;

	/* check if the bus is top-level */
	if (dev->bus->parent != NULL) {
		bus = dev->bus->number;
		db_assert(bus != 0);
	} else {
		bus = 0;
	}

	func_num = PCI_FUNC(dev->devfn);
	if (mkaddr(bus, dev->devfn, where, &flags))
		return -1;
#ifdef __BIG_ENDIAN
	*(volatile u8 *) ((ulong) & tx4927_pcicptr->
			  g2pcfgdata | ((where & 3) ^ 3)) = val;
#else
	*(volatile u8 *) ((ulong) & tx4927_pcicptr->
			  g2pcfgdata | (where & 3)) = val;
#endif
//printk("CFG W1 0x%02x 0x%02x 0x%08x\n", dev->devfn, where, val );
	return check_abort(flags);
}

static int tx4927_pcibios_write_config_word(struct pci_dev *dev,
					    int where, unsigned short val)
{
	int flags;
	unsigned char bus, func_num;

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	/* check if the bus is top-level */
	if (dev->bus->parent != NULL) {
		bus = dev->bus->number;
		db_assert(bus != 0);
	} else {
		bus = 0;
	}

	func_num = PCI_FUNC(dev->devfn);
	if (mkaddr(bus, dev->devfn, where, &flags))
		return -1;
#ifdef __BIG_ENDIAN
	*(volatile u16 *) ((ulong) & tx4927_pcicptr->
			   g2pcfgdata | ((where & 3) ^ 2)) = val;
#else
	*(volatile u16 *) ((ulong) & tx4927_pcicptr->
			   g2pcfgdata | (where & 3)) = val;
#endif
//printk("CFG W2 0x%02x 0x%02x 0x%08x\n", dev->devfn, where, val );
	return check_abort(flags);
}

static int tx4927_pcibios_write_config_dword(struct pci_dev *dev,
					     int where, unsigned int val)
{
	int flags;
	unsigned char bus, func_num;

	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	/* check if the bus is top-level */
	if (dev->bus->parent != NULL) {
		bus = dev->bus->number;
		db_assert(bus != 0);
	} else {
		bus = 0;
	}

	func_num = PCI_FUNC(dev->devfn);
	if (mkaddr(bus, dev->devfn, where, &flags))
		return -1;
	tx4927_pcicptr->g2pcfgdata = val;
//printk("CFG W4 0x%02x 0x%02x 0x%08x\n", dev->devfn, where, val );
	return check_abort(flags);
}

struct pci_ops tx4927_pci_ops = {
	tx4927_pcibios_read_config_byte,
	tx4927_pcibios_read_config_word,
	tx4927_pcibios_read_config_dword,
	tx4927_pcibios_write_config_byte,
	tx4927_pcibios_write_config_word,
	tx4927_pcibios_write_config_dword
};
