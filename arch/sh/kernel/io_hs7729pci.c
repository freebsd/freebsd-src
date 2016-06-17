/* $Id: io_hs7729pci.c,v 1.1.2.1 2003/06/24 08:45:47 dwmw2 Exp $
 *
 * linux/arch/sh/kernel/io_hs7729pci.c
 *
 * Copyright (C) 2000  Kazumoto Kojima
 *
 * I/O routine for Hitachi Semiconductor and Devices HS7729PCI.
 *
 */

#include <linux/kernel.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <asm/hitachi_hs7729pci.h>

#include "pci-sd0001.h"

#ifdef CONFIG_PCI
#define io_is_pci(port) (port >= PCIBIOS_MIN_IO)
#else
#define io_is_pci(port) (0)
#endif

/* SH pcmcia io window base, start and end.  */
int sh_pcic_io_wbase = 0xb8400000;
int sh_pcic_io_start;
int sh_pcic_io_stop;
int sh_pcic_io_type;
int sh_pcic_io_dummy;

static inline void delay(void)
{
	ctrl_inw(0xa0000000);
}

/* MS7750 requires special versions of in*, out* routines, since
   PC-like io ports are located at upper half byte of 16-bit word which
   can be accessed only with 16-bit wide.  */

static inline volatile __u16 *
port2adr(unsigned long port)
{
	if (port >= 0x2000)
		return (volatile __u16 *) (PA_MRSHPC + (port - 0x2000));
	else if (sh_pcic_io_start <= port && port <= sh_pcic_io_stop)
		return (volatile __u16 *) (sh_pcic_io_wbase + (port &~ 1));
	
	else
		return (volatile __u16 *) (PA_SUPERIO + (port << 1));
}

static inline int
shifted_port(unsigned long port)
{
	/* For IDE registers, value is not shifted */
	if ((0x1f0 <= port && port < 0x1f8) || port == 0x3f6 || 
	    (0x170 <= port && port < 0x178) || port == 0x376 || 
	    port >= 0x1000)
		return 0;
	else
		return 1;
}

#define maybebadio(name,port) \
  printk("bad PC-like io %s for port 0x%lx at 0x%08x\n", \
	 #name, (port), (__u32) __builtin_return_address(0))


unsigned char hs7729pci_inb(unsigned long port)
{
	if (io_is_pci(port))
		return sd0001_inb(port);
	if (sh_pcic_io_start <= port && port <= sh_pcic_io_stop)
		return *(__u8 *) (sh_pcic_io_wbase + 0x40000 + port); 
	if (shifted_port(port))
		return (*port2adr(port) >> 8); 
#if defined(CONFIG_CF_ENABLER)
	else if ((0x1f0 <= port && port < 0x1f8) || port == 0x3f6)
		return *(__u8 *)(PA_MRSHPC_IO + port + 0x40000);
#endif
	else
		return (*port2adr(port))&0xff; 
}

unsigned char hs7729pci_inb_p(unsigned long port)
{
	unsigned long v;

	if (io_is_pci(port))
		v =  sd0001_inb(port);
	else if (sh_pcic_io_start <= port && port <= sh_pcic_io_stop)
		v = *(__u8 *) (sh_pcic_io_wbase + 0x40000 + port); 
	else if (shifted_port(port))
		v = (*port2adr(port) >> 8); 
#if defined(CONFIG_CF_ENABLER)
	else if ((0x1f0 <= port && port < 0x1f8) || port == 0x3f6)
		v = *(__u8 *)(PA_MRSHPC_IO + port + 0x40000);
#endif
	else
		v = (*port2adr(port))&0xff; 
	delay();
	return v;
}

unsigned short hs7729pci_inw(unsigned long port)
{
	if (io_is_pci(port))
		return sd0001_inw(port);
	if (port >= 0x2000 ||
		 (sh_pcic_io_start <= port && port <= sh_pcic_io_stop))
		return *port2adr(port);

	maybebadio(inw, port);
}

unsigned int hs7729pci_inl(unsigned long port)
{
	if (io_is_pci(port))
		return sd0001_inl(port);
	else 
		maybebadio(inl, port);
}

void hs7729pci_outb(unsigned char value, unsigned long port)
{
	if (io_is_pci(port))
		sd0001_outb(value, port);
	else if (sh_pcic_io_start <= port && port <= sh_pcic_io_stop)
		*(__u8 *)(sh_pcic_io_wbase + port) = value; 
	else if (shifted_port(port))
		*(port2adr(port)) = value << 8;
#if defined(CONFIG_CF_ENABLER)
	else if ((0x1f0 <= port && port < 0x1f8) || port == 0x3f6)
		*(__u8 *)(PA_MRSHPC_IO + port + 0x40000) = value;
#endif
	else
		*(port2adr(port)) = value;
}

void hs7729pci_outb_p(unsigned char value, unsigned long port)
{
	if (io_is_pci(port))
		sd0001_outb(value, port);
	else if (sh_pcic_io_start <= port && port <= sh_pcic_io_stop)
		*(__u8 *)(sh_pcic_io_wbase + port) = value; 
	else if (shifted_port(port))
		*(port2adr(port)) = value << 8;
#if defined(CONFIG_CF_ENABLER)
	else if ((0x1f0 <= port && port < 0x1f8) || port == 0x3f6)
		*(__u8 *)(PA_MRSHPC_IO + port + 0x40000) = value;
#endif
	else
		*(port2adr(port)) = value;
	delay();
}

void hs7729pci_outw(unsigned short value, unsigned long port)
{
	if (io_is_pci(port))
		sd0001_outw(value, port);
	else if (port >= 0x2000 ||
	    (sh_pcic_io_start <= port && port <= sh_pcic_io_stop))
		*port2adr(port) = value;
	else 
		maybebadio(outw, port);
}

void hs7729pci_outl(unsigned int value, unsigned long port)
{
	if (io_is_pci(port))
		sd0001_outl(value, port);
	else
		maybebadio(outl, port);
}

void hs7729pci_insb(unsigned long port, void *addr, unsigned long count)
{
	volatile __u16 *p = port2adr(port);

	if (io_is_pci(port))
		sd0001_insb(port, addr, count);
	else if (sh_pcic_io_start <= port && port <= sh_pcic_io_stop) {
		volatile __u8 *bp = (__u8 *) (sh_pcic_io_wbase + 0x40000 + port); 
		while (count--)
			*((__u8 *) addr)++ = *bp;
	} else if (shifted_port(port)) {
		while (count--)
			*((__u8 *) addr)++ = *p >> 8;
#if defined(CONFIG_CF_ENABLER)
	} else if ((0x1f0 <= port && port < 0x1f8) || port == 0x3f6) {
		volatile __u8 *q = (__u8 *)(PA_MRSHPC_IO + port + 0x40000);
		while (count--) {
			*((__u8 *) addr)++ = *q;
		}
#endif
	} else {
		while (count--)
			*((__u8 *) addr)++ = *p;

	}
}

void hs7729pci_insw(unsigned long port, void *addr, unsigned long count)
{
	volatile __u16 *p;

	 if (io_is_pci(port))
		 return sd0001_insw(port, addr, count);
#if defined(CONFIG_CF_ENABLER)
	 if ((0x1f0 <= port && port < 0x1f8) || port == 0x3f6) {
		p = (__u16 *)(PA_MRSHPC_IO + port);
	 } else
#endif
		 p = port2adr(port);
	 while (count--)
		 *((__u16 *) addr)++ = *p;
}

void hs7729pci_insl(unsigned long port, void *addr, unsigned long count)
{
	if (io_is_pci(port))
		sd0001_insl(port, addr, count);
	else
		maybebadio(insl, port);
}

void hs7729pci_outsb(unsigned long port, const void *addr, unsigned long count)
{
	volatile __u16 *p = port2adr(port);

	if (io_is_pci(port)) {
		sd0001_outsb(port, addr, count);
		return;
	}
	if (sh_pcic_io_start <= port && port <= sh_pcic_io_stop) {
		volatile __u8 *bp = (__u8 *) (sh_pcic_io_wbase + port); 
		while (count--)
			*bp = *((__u8 *) addr)++;
	} else if (shifted_port(port)) {
		while (count--)
			*p = *((__u8 *) addr)++ << 8;
#if defined(CONFIG_CF_ENABLER)
	} else if ((0x1f0 <= port && port < 0x1f8) || port == 0x3f6) {
		volatile __u8 *q = (__u8 *)(PA_MRSHPC_IO + port + 0x40000);
		while (count--) {
			*q = *((__u8 *) addr)++;
		}
#endif
	} else while (count--)
		*p = *((__u8 *) addr)++;
}

void hs7729pci_outsw(unsigned long port, const void *addr, unsigned long count)
{
	volatile __u16 *p;

	if (io_is_pci(port)) {
		sd0001_outsw(port, addr, count);
		return;
	}
#if defined(CONFIG_CF_ENABLER)
	if ((0x1f0 <= port && port < 0x1f8) || port == 0x3f6) {
		p = (__u16 *)(PA_MRSHPC_IO + port);
	} else
#endif
		p = port2adr(port);
	while (count--)
		*p = *((__u16 *) addr)++;
}

void hs7729pci_outsl(unsigned long port, const void *addr, unsigned long count)
{
	if (io_is_pci(port))
		sd0001_outsl(port, addr, count);
	else
		maybebadio(outsl, port);
}

/* Map ISA bus address to the real address. Only for PCMCIA.  */

/* ISA page descriptor.  */
static __u32 sh_isa_memmap[256];

static int
sh_isa_mmap(__u32 start, __u32 length, __u32 offset)
{
	int idx;

	if (start >= 0x100000 || (start & 0xfff) || (length != 0x1000))
		return -1;

	idx = start >> 12;
	sh_isa_memmap[idx] = 0xb8000000 + (offset &~ 0xfff);
#if 0
	printk("sh_isa_mmap: start %x len %x offset %x (idx %x paddr %x)\n",
	       start, length, offset, idx, sh_isa_memmap[idx]);
#endif
	return 0;
}

unsigned long
hs7729pci_isa_port2addr(unsigned long offset)
{
	int idx;

	idx = (offset >> 12) & 0xff;
	offset &= 0xfff;
	return sh_isa_memmap[idx] + offset;
}

void *hs7729pci_ioremap(unsigned long phys_addr, unsigned long size)
{
        /* Don't allow wraparound or zero size */
        if (!size || (phys_addr + size - 1) < phys_addr)
                return NULL;

#ifdef CONFIG_PCI
	if (phys_addr >= PCIBIOS_MIN_MEM &&
	    phys_addr < (PCIBIOS_MIN_MEM&0xfc000000)+0x03000000)
		return sd0001_ioremap(phys_addr, size);
#endif
	   return (void *) P2SEGADDR(phys_addr);
}

void hs7729pci_iounmap(void *addr) 
{
#ifdef CONFIG_PCI
	/* Ideally for cleanliness we'd have a better way of knowing
	   if it came from the PCI controller. */
	if ((unsigned long)addr >= VMALLOC_START && 
	    (unsigned long)addr < VMALLOC_END)
		sd0001_iounmap(addr);
#endif
}
		
