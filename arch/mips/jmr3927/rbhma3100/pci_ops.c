/***********************************************************************
 * Copyright 2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *              ahennessy@mvista.com
 *
 * Copyright (C) 2000-2001 Toshiba Corporation
 *
 * Based on arch/mips/ddb5xxx/ddb5477/pci_ops.c
 *
 *     Define the pci_ops for JMR3927.
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
 ***********************************************************************
 */
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/addrspace.h>
#include <asm/pci_channel.h>
#include <asm/jmr3927/jmr3927.h>
#include <asm/debug.h>

struct resource pci_io_resource = {
	"pci IO space",
	0x1000,  /* reserve regacy I/O space */
	0x1000 + JMR3927_PCIIO_SIZE -1,
	IORESOURCE_IO};

struct resource pci_mem_resource = {
	"pci memory space",
	JMR3927_PCIMEM,
	JMR3927_PCIMEM + JMR3927_PCIMEM_SIZE -1,
	IORESOURCE_MEM};

extern struct pci_ops jmr3927_pci_ops;

struct pci_channel mips_pci_channels[] = {
	{ &jmr3927_pci_ops, &pci_io_resource, &pci_mem_resource, 0, 0xff },
	{ NULL, NULL, NULL, NULL, NULL}
};

unsigned int pcibios_assign_all_busses(void)
{
	return 1;
}

static int
mkaddr(unsigned char bus, unsigned char dev_fn, unsigned char where, int *flagsp)
{
	if (bus == 0 && dev_fn >= PCI_DEVFN(TX3927_PCIC_MAX_DEVNU, 0))
		return -1;

	tx3927_pcicptr->ica = ((bus    & 0xff) << 0x10) |
	                      ((dev_fn & 0xff) << 0x08) |
	                      (where  & 0xfc);
	/* clear M_ABORT and Disable M_ABORT Int. */
	tx3927_pcicptr->pcistat |= PCI_STATUS_REC_MASTER_ABORT;
	tx3927_pcicptr->pcistatim &= ~PCI_STATUS_REC_MASTER_ABORT;
	return 0;
}

static int
check_abort(int flags)
{
	int code = PCIBIOS_SUCCESSFUL;
	if (tx3927_pcicptr->pcistat & PCI_STATUS_REC_MASTER_ABORT) {
		tx3927_pcicptr->pcistat |= PCI_STATUS_REC_MASTER_ABORT;
		tx3927_pcicptr->pcistatim |= PCI_STATUS_REC_MASTER_ABORT;
		code =PCIBIOS_DEVICE_NOT_FOUND;
	}
	return code;
}

/*
 * We can't address 8 and 16 bit words directly.  Instead we have to
 * read/write a 32bit word and mask/modify the data we actually want.
 */
static int jmr3927_pcibios_read_config_byte (struct pci_dev *dev,
					     int where,
					     unsigned char *val)
{
	int flags;
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
	*val = *(volatile u8 *)((ulong)&tx3927_pcicptr->icd | (where&3));
	return check_abort(flags);
}

static int jmr3927_pcibios_read_config_word (struct pci_dev *dev,
					     int where,
					     unsigned short *val)
{
	int flags;
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
	*val = le16_to_cpu(*(volatile u16 *)((ulong)&tx3927_pcicptr->icd | (where&3)));
	return check_abort(flags);
}

static int jmr3927_pcibios_read_config_dword (struct pci_dev *dev,
					      int where,
					      unsigned int *val)
{
	int flags;
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
	*val = le32_to_cpu(tx3927_pcicptr->icd);
	return check_abort(flags);
}

static int jmr3927_pcibios_write_config_byte (struct pci_dev *dev,
					      int where,
					      unsigned char val)
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
	*(volatile u8 *)((ulong)&tx3927_pcicptr->icd | (where&3)) = val;
	return check_abort(flags);
}

static int jmr3927_pcibios_write_config_word (struct pci_dev *dev,
					      int where,
					      unsigned short val)
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
	*(volatile u16 *)((ulong)&tx3927_pcicptr->icd | (where&3)) = cpu_to_le16(val);
	return check_abort(flags);
}

static int jmr3927_pcibios_write_config_dword (struct pci_dev *dev,
					       int where,
					       unsigned int val)
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
	tx3927_pcicptr->icd = cpu_to_le32(val);
	return check_abort(flags);
}
struct pci_ops jmr3927_pci_ops = {
	jmr3927_pcibios_read_config_byte,
	jmr3927_pcibios_read_config_word,
	jmr3927_pcibios_read_config_dword,
	jmr3927_pcibios_write_config_byte,
	jmr3927_pcibios_write_config_word,
	jmr3927_pcibios_write_config_dword
};

#ifndef JMR3927_INIT_INDIRECT_PCI
inline unsigned long tc_readl(volatile __u32 *addr)
{
	return readl(addr);
}
inline void tc_writel(unsigned long data, volatile __u32 *addr)
{
	writel(data, addr);
}
#else
unsigned long tc_readl(volatile __u32 *addr)
{
	unsigned long val;

	addr = PHYSADDR(addr);
	*(volatile u32 *)(ulong)&tx3927_pcicptr->ipciaddr =  (unsigned long)addr;
	*(volatile u32 *)(ulong)&tx3927_pcicptr->ipcibe =
	    (PCI_IPCIBE_ICMD_MEMREAD << PCI_IPCIBE_ICMD_SHIFT) | PCI_IPCIBE_IBE_LONG;
	while (!(tx3927_pcicptr->istat & PCI_ISTAT_IDICC)) ;
	val = le32_to_cpu(*(volatile u32 *)(ulong)&tx3927_pcicptr->ipcidata);
	/* clear by setting */
	tx3927_pcicptr->istat |= PCI_ISTAT_IDICC;
	return val;
}
void tc_writel(unsigned long data, volatile __u32 *addr)
{
	addr = PHYSADDR(addr);
	*(volatile u32 *)(ulong)&tx3927_pcicptr->ipcidata = cpu_to_le32(data);
	*(volatile u32 *)(ulong)&tx3927_pcicptr->ipciaddr =  (unsigned long)addr;
	*(volatile u32 *)(ulong)&tx3927_pcicptr->ipcibe =
	    (PCI_IPCIBE_ICMD_MEMWRITE << PCI_IPCIBE_ICMD_SHIFT)  | PCI_IPCIBE_IBE_LONG;
	while (!(tx3927_pcicptr->istat & PCI_ISTAT_IDICC)) ;
	/* clear by setting */
	tx3927_pcicptr->istat |= PCI_ISTAT_IDICC;
}
unsigned char tx_ioinb(unsigned char *addr)
{
	unsigned long val;
	__u32 ioaddr;
	int offset;
	int byte;

	ioaddr = (unsigned long)addr;
        offset = ioaddr & 0x3;
	if (offset == 0)
		byte = 0x7;
	else if (offset == 1)
		byte = 0xb;
	else if (offset == 2)
		byte = 0xd;
	else if (offset == 3)
		byte = 0xe;
	*(volatile u32 *)(ulong)&tx3927_pcicptr->ipciaddr =  (unsigned long)ioaddr;
	*(volatile u32 *)(ulong)&tx3927_pcicptr->ipcibe =
	    (PCI_IPCIBE_ICMD_IOREAD << PCI_IPCIBE_ICMD_SHIFT) | byte;
	while (!(tx3927_pcicptr->istat & PCI_ISTAT_IDICC)) ;
	val = le32_to_cpu(*(volatile u32 *)(ulong)&tx3927_pcicptr->ipcidata);
	val = val & 0xff;
	/* clear by setting */
	tx3927_pcicptr->istat |= PCI_ISTAT_IDICC;
	return val;
}
void tx_iooutb(unsigned long data, unsigned char *addr)
{
	__u32 ioaddr;
	int offset;
	int byte;

	data = data | (data << 8) | (data << 16) | (data << 24);
	ioaddr = (unsigned long)addr;
        offset = ioaddr & 0x3;
	if (offset == 0)
		byte = 0x7;
	else if (offset == 1)
		byte = 0xb;
	else if (offset == 2)
		byte = 0xd;
	else if (offset == 3)
		byte = 0xe;
	*(volatile u32 *)(ulong)&tx3927_pcicptr->ipcidata = data;
	*(volatile u32 *)(ulong)&tx3927_pcicptr->ipciaddr =  (unsigned long)ioaddr;
	*(volatile u32 *)(ulong)&tx3927_pcicptr->ipcibe =
	    (PCI_IPCIBE_ICMD_IOWRITE << PCI_IPCIBE_ICMD_SHIFT)  | byte;
	while (!(tx3927_pcicptr->istat & PCI_ISTAT_IDICC)) ;
	/* clear by setting */
	tx3927_pcicptr->istat |= PCI_ISTAT_IDICC;
}
unsigned short tx_ioinw(unsigned short *addr)
{
	unsigned long val;
	__u32 ioaddr;
	int offset;
	int byte;

	ioaddr = (unsigned long)addr;
        offset = ioaddr & 0x3;
	if (offset == 0)
		byte = 0x3;
	else if (offset == 2)
		byte = 0xc;
	*(volatile u32 *)(ulong)&tx3927_pcicptr->ipciaddr =  (unsigned long)ioaddr;
	*(volatile u32 *)(ulong)&tx3927_pcicptr->ipcibe =
	    (PCI_IPCIBE_ICMD_IOREAD << PCI_IPCIBE_ICMD_SHIFT) | byte;
	while (!(tx3927_pcicptr->istat & PCI_ISTAT_IDICC)) ;
	val = le32_to_cpu(*(volatile u32 *)(ulong)&tx3927_pcicptr->ipcidata);
	val = val & 0xffff;
	/* clear by setting */
	tx3927_pcicptr->istat |= PCI_ISTAT_IDICC;
	return val;

}
void tx_iooutw(unsigned long data, unsigned short *addr)
{
	__u32 ioaddr;
	int offset;
	int byte;

	data = data | (data << 16);
	ioaddr = (unsigned long)addr;
        offset = ioaddr & 0x3;
	if (offset == 0)
		byte = 0x3;
	else if (offset == 2)
		byte = 0xc;
	*(volatile u32 *)(ulong)&tx3927_pcicptr->ipcidata = data;
	*(volatile u32 *)(ulong)&tx3927_pcicptr->ipciaddr =  (unsigned long)ioaddr;
	*(volatile u32 *)(ulong)&tx3927_pcicptr->ipcibe =
	    (PCI_IPCIBE_ICMD_IOWRITE << PCI_IPCIBE_ICMD_SHIFT)  | byte;
	while (!(tx3927_pcicptr->istat & PCI_ISTAT_IDICC)) ;
	/* clear by setting */
	tx3927_pcicptr->istat |= PCI_ISTAT_IDICC;
}
unsigned long tx_ioinl(unsigned int *addr)
{
	unsigned long val;
	__u32 ioaddr;

	ioaddr = (unsigned long)addr;
	*(volatile u32 *)(ulong)&tx3927_pcicptr->ipciaddr =  (unsigned long)ioaddr;
	*(volatile u32 *)(ulong)&tx3927_pcicptr->ipcibe =
	    (PCI_IPCIBE_ICMD_IOREAD << PCI_IPCIBE_ICMD_SHIFT) | PCI_IPCIBE_IBE_LONG;
	while (!(tx3927_pcicptr->istat & PCI_ISTAT_IDICC)) ;
	val = le32_to_cpu(*(volatile u32 *)(ulong)&tx3927_pcicptr->ipcidata);
	/* clear by setting */
	tx3927_pcicptr->istat |= PCI_ISTAT_IDICC;
	return val;
}
void tx_iooutl(unsigned long data, unsigned int *addr)
{
	__u32 ioaddr;

	ioaddr = (unsigned long)addr;
	*(volatile u32 *)(ulong)&tx3927_pcicptr->ipcidata = cpu_to_le32(data);
	*(volatile u32 *)(ulong)&tx3927_pcicptr->ipciaddr =  (unsigned long)ioaddr;
	*(volatile u32 *)(ulong)&tx3927_pcicptr->ipcibe =
	    (PCI_IPCIBE_ICMD_IOWRITE << PCI_IPCIBE_ICMD_SHIFT)  | PCI_IPCIBE_IBE_LONG;
	while (!(tx3927_pcicptr->istat & PCI_ISTAT_IDICC)) ;
	/* clear by setting */
	tx3927_pcicptr->istat |= PCI_ISTAT_IDICC;
}
void tx_insbyte(unsigned char *addr,void *buffer,unsigned int count)
{
	unsigned char *ptr = (unsigned char *) buffer;

	while (count--) {
		*ptr++ = tx_ioinb(addr);
	}
}
void tx_insword(unsigned short *addr,void *buffer,unsigned int count)
{
	unsigned short *ptr = (unsigned short *) buffer;

	while (count--) {
		*ptr++ = tx_ioinw(addr);
	}
}
void tx_inslong(unsigned int *addr,void *buffer,unsigned int count)
{
	unsigned long *ptr = (unsigned long *) buffer;

	while (count--) {
		*ptr++ = tx_ioinl(addr);
	}
}
void tx_outsbyte(unsigned char *addr,void *buffer,unsigned int count)
{
	unsigned char *ptr = (unsigned char *) buffer;

	while (count--) {
		tx_iooutb(*ptr++,addr);
	}
}
void tx_outsword(unsigned short *addr,void *buffer,unsigned int count)
{
	unsigned short *ptr = (unsigned short *) buffer;

	while (count--) {
		tx_iooutw(*ptr++,addr);
	}
}
void tx_outslong(unsigned int *addr,void *buffer,unsigned int count)
{
	unsigned long *ptr = (unsigned long *) buffer;

	while (count--) {
		tx_iooutl(*ptr++,addr);
	}
}
#endif
