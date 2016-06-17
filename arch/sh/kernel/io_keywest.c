/*
 * linux/arch/sh/io_keywest.c
 *
 * IO functions for a Hitachi KeyWest Evaluation Board.
 *
 * Copyright (C) 2001 Lineo, Japan
 *
 * This file is subject to the terms and conditions of the GNU General Publi
c
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/machvec.h>
#include <asm/io.h>
#include <asm/keywest.h>

//#define KEYWEST_DEBUG 3
#undef KEYWEST_DEBUG

#ifdef KEYWEST_DEBUG
#define DPRINTK(args...)	printk(args)
#define DIPRINTK(n, args...)	if (KEYWEST_DEBUG>(n)) printk(args)
#else
#define DPRINTK(args...)
#define DIPRINTK(n, args...)
#endif


/* Low iomap maps port 0-1K to addresses in 8byte chunks */
#define KEYWEST_IOMAP_LO_THRESH 0x400
#define KEYWEST_IOMAP_LO_SHIFT	3
#define KEYWEST_IOMAP_LO_MASK	((1<<KEYWEST_IOMAP_LO_SHIFT)-1)
#define KEYWEST_IOMAP_LO_NMAP	(KEYWEST_IOMAP_LO_THRESH>>KEYWEST_IOMAP_LO_SHIFT)
static u32 keywest_iomap_lo[KEYWEST_IOMAP_LO_NMAP];
static u8 keywest_iomap_lo_shift[KEYWEST_IOMAP_LO_NMAP];

/* High iomap maps port 1K-64K to addresses in 1K chunks */
#define KEYWEST_IOMAP_HI_THRESH 0x10000
#define KEYWEST_IOMAP_HI_SHIFT	10
#define KEYWEST_IOMAP_HI_MASK	((1<<KEYWEST_IOMAP_HI_SHIFT)-1)
#define KEYWEST_IOMAP_HI_NMAP	(KEYWEST_IOMAP_HI_THRESH>>KEYWEST_IOMAP_HI_SHIFT)
static u32 keywest_iomap_hi[KEYWEST_IOMAP_HI_NMAP];
static u8 keywest_iomap_hi_shift[KEYWEST_IOMAP_HI_NMAP];

#ifndef MAX
#define MAX(a,b)    ((a)>(b)?(a):(b))
#endif

#define PORT2ADDR(x) keywest_isa_port2addr(x)
//#define PORT2ADDR(x) (sh_mv.mv_isa_port2addr(x))
//#define PORT2ADDR(x) (x)

void keywest_port_map(u32 baseport, u32 nports, u32 addr, u8 shift)
{
    u32 port, endport = baseport + nports;

    DPRINTK("keywest_port_map(base=0x%0x, n=0x%0x, addr=0x%08x)\n",
	    baseport, nports, addr);
	    
	for (port = baseport ;
	     port < endport && port < KEYWEST_IOMAP_LO_THRESH ;
	     port += (1<<KEYWEST_IOMAP_LO_SHIFT)) {
	    	DPRINTK("    maplo[0x%x] = 0x%08x\n", port, addr);
    	    keywest_iomap_lo[port>>KEYWEST_IOMAP_LO_SHIFT] = addr;
    	    keywest_iomap_lo_shift[port>>KEYWEST_IOMAP_LO_SHIFT] = shift;
	    	addr += (1<<(KEYWEST_IOMAP_LO_SHIFT));
	}

	for (port = MAX(baseport, KEYWEST_IOMAP_LO_THRESH) ;
	     port < endport && port < KEYWEST_IOMAP_HI_THRESH ;
	     port += (1<<KEYWEST_IOMAP_HI_SHIFT)) {
	    	DPRINTK("    maphi[0x%x] = 0x%08x\n", port, addr);
    	    keywest_iomap_hi[port>>KEYWEST_IOMAP_HI_SHIFT] = addr;
    	    keywest_iomap_hi_shift[port>>KEYWEST_IOMAP_HI_SHIFT] = shift;
	    	addr += (1<<(KEYWEST_IOMAP_HI_SHIFT));
	}
}
EXPORT_SYMBOL(keywest_port_map);

void keywest_port_unmap(u32 baseport, u32 nports)
{
    u32 port, endport = baseport + nports;
	
    DPRINTK("keywest_port_unmap(base=0x%0x, n=0x%0x)\n", baseport, nports);

	for (port = baseport ;
	     port < endport && port < KEYWEST_IOMAP_LO_THRESH ;
	     port += (1<<KEYWEST_IOMAP_LO_SHIFT)) {
    	    keywest_iomap_lo[port>>KEYWEST_IOMAP_LO_SHIFT] = 0;
	}

	for (port = MAX(baseport, KEYWEST_IOMAP_LO_THRESH) ;
	     port < endport && port < KEYWEST_IOMAP_HI_THRESH ;
	     port += (1<<KEYWEST_IOMAP_HI_SHIFT)) {
    	    keywest_iomap_hi[port>>KEYWEST_IOMAP_HI_SHIFT] = 0;
	}
}
EXPORT_SYMBOL(keywest_port_unmap);

unsigned long keywest_isa_port2addr(unsigned long port)
{
    unsigned long addr;
	unsigned char shift;

	/* Physical address not in P0, do nothing */
	if (PXSEG(port))
		return(port);

	addr = 0;
	/* physical address in P0, map to P2 */
	if (port >= 0x30000)
	    addr = P2SEGADDR(port);
	/* Key West I/O + HD64465 registers 0x10000-0x30000 */
	else if (port >= KEYWEST_IOMAP_HI_THRESH)
	    addr = KEYWEST_INTERNAL_BASE + (port - KEYWEST_IOMAP_HI_THRESH);
	/* Handle remapping of high IO/PCI IO ports */
	else if (port >= KEYWEST_IOMAP_LO_THRESH) {
	    addr = keywest_iomap_hi[port >> KEYWEST_IOMAP_HI_SHIFT];
	    shift = keywest_iomap_hi_shift[port >> KEYWEST_IOMAP_HI_SHIFT];
	    if (addr != 0)
		    addr += (port & KEYWEST_IOMAP_HI_MASK) << shift;
	}
	/* Handle remapping of low IO ports */
	else {
	    addr = keywest_iomap_lo[port >> KEYWEST_IOMAP_LO_SHIFT];
	    shift = keywest_iomap_lo_shift[port >> KEYWEST_IOMAP_LO_SHIFT];
	    if (addr != 0)
	    	addr += (port & KEYWEST_IOMAP_LO_MASK) << shift;
	}

    DIPRINTK(2, "PORT2ADDR(0x%08lx) = 0x%08lx\n", port, addr);

	return addr;
}

static inline void delay(void)
{
	ctrl_inw(0xa0000000);
}

unsigned char keywest_inb(unsigned long port)
{
	unsigned long addr = PORT2ADDR(port);
	unsigned long b = (addr == 0 ? 0 : *(volatile unsigned char*)addr);

	DIPRINTK(0, "inb(%08lx) = %02x\n", addr, (unsigned)b);
	return b;
}

unsigned char keywest_inb_p(unsigned long port)
{
    unsigned long v;
	unsigned long addr = PORT2ADDR(port);

	v = (addr == 0 ? 0 : *(volatile unsigned char*)addr);
	delay();
	DIPRINTK(0, "inb_p(%08lx) = %02x\n", addr, (unsigned)v);
	return v;
}

unsigned short keywest_inw(unsigned long port)
{
    unsigned long addr = PORT2ADDR(port);
	unsigned long b = (addr == 0 ? 0 : *(volatile unsigned short*)addr);
	DIPRINTK(0, "inw(%08lx) = %04lx\n", addr, b);
	return b;
}

unsigned int keywest_inl(unsigned long port)
{
    unsigned long addr = PORT2ADDR(port);
	unsigned int b = (addr == 0 ? 0 : *(volatile unsigned long*)addr);
	DIPRINTK(0, "inl(%08lx) = %08x\n", addr, b);
	return b;
}

void keywest_insb(unsigned long port, void *buffer, unsigned long count)
{
	unsigned char *buf=buffer;
	while(count--) *buf++=inb(port);
}

void keywest_insw(unsigned long port, void *buffer, unsigned long count)
{
	unsigned short *buf=buffer;
	while(count--) *buf++=inw(port);
}

void keywest_insl(unsigned long port, void *buffer, unsigned long count)
{
#if 0
	unsigned long *buf=buffer;
	while(count--) *buf++=inl(port);
#else
	union {
		unsigned long l;
		unsigned short s[2];
		unsigned char c[4];
	} align;
	port = (((unsigned long)(port)) & 0xe0000000) ? port :
			keywest_isa_port2addr(port);
	switch (((unsigned long) buffer) & 0x3) {
	case 0: {
		register unsigned long *buf=buffer, *endp = buf + count;
		while (buf < endp)
			*buf++ = * (volatile unsigned long *) port;
		break;
		}
	case 2: {
		register unsigned short *buf=buffer, *endp = buf + (count << 1);
		while (buf < endp) {
			align.l = * (volatile unsigned long *) port;
			*buf++ = align.s[0];
			*buf++ = align.s[1];
		}
		break;
		}
	case 1:
	case 3: {
		register unsigned char *buf=buffer, *endp = buf + (count << 2);
		while (buf < endp) {
			align.l = * (volatile unsigned long *) port;
			*buf++ = align.c[0];
			*buf++ = align.c[1];
			*buf++ = align.c[2];
			*buf++ = align.c[3];
		}
		break;
		}
	}
#endif
}

void keywest_outb(unsigned char b, unsigned long port)
{
	unsigned long addr = PORT2ADDR(port);

	DIPRINTK(0, "outb(%02x, %08lx)\n", (unsigned)b, addr);
	if (addr != 0)
	    *(volatile unsigned char*)addr = b;
}

void keywest_outb_p(unsigned char b, unsigned long port)
{
	unsigned long addr = PORT2ADDR(port);

	DIPRINTK(0, "outb_p(%02x, %08lx)\n", (unsigned)b, addr);
    if (addr != 0)
	    *(volatile unsigned char*)addr = b;
	delay();
}

void keywest_outw(unsigned short b, unsigned long port)
{
	unsigned long addr = PORT2ADDR(port);
	DIPRINTK(0, "outw(%04x, %08lx)\n", (unsigned)b, addr);
	if (addr != 0)
	    *(volatile unsigned short*)addr = b;
}

void keywest_outl(unsigned int b, unsigned long port)
{
	unsigned long addr = PORT2ADDR(port);
	DIPRINTK(0, "outl(%08x, %08lx)\n", b, addr);
	if (addr != 0)
            *(volatile unsigned long*)addr = b;
}

void keywest_outsb(unsigned long port, const void *buffer, unsigned long count)
{
	const unsigned char *buf=buffer;
	while(count--) outb(*buf++, port);
}

void keywest_outsw(unsigned long port, const void *buffer, unsigned long count)
{
	const unsigned short *buf=buffer;
	while(count--) outw(*buf++, port);
}

void keywest_outsl(unsigned long port, const void *buffer, unsigned long count)
{
#if 0
	const unsigned long *buf=buffer;
	while(count--) outl(*buf++, port);
#else
	union {
		unsigned long l;
		unsigned short s[2];
		unsigned char c[4];
	} align;
	port = (((unsigned long)(port)) & 0xe0000000) ? port :
			keywest_isa_port2addr(port);
	switch (((unsigned long) buffer) & 0x3) {
	case 0: {
		register unsigned long *buf=buffer, *endp = buf + count;
		while (buf < endp)
			* (volatile unsigned long *) port = *buf++;
		break;
		}
	case 2: {
		register unsigned short *buf=buffer, *endp = buf + (count << 1);
		while (buf < endp) {
			align.s[0] = *buf++;
			align.s[1] = *buf++;
			* (volatile unsigned long *) port = align.l;
		}
		break;
		}
	case 1:
	case 3: {
		register unsigned char *buf=buffer, *endp = buf + (count << 2);
		while (buf < endp) {
			align.c[0] = *buf++;
			align.c[1] = *buf++;
			align.c[2] = *buf++;
			align.c[3] = *buf++;
			* (volatile unsigned long *) port = align.l;
		}
		break;
		}
	}
#endif
}

