/*
 * include/asm-sh/io_bigsur.c
 *
 * By Dustin McIntire (dustin@sensoria.com) (c)2001
 * Derived from io_hd64465.h, which bore the message:
 * By Greg Banks <gbanks@pocketpenguins.com>
 * (c) 2000 PocketPenguins Inc. 
 * and from io_hd64461.h, which bore the message:
 * Copyright 2000 Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * IO functions for a Hitachi Big Sur Evaluation Board.
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/machvec.h>
#include <asm/io.h>
#include <asm/bigsur.h>

//#define BIGSUR_DEBUG 2
#undef BIGSUR_DEBUG

#ifdef BIGSUR_DEBUG
#define DPRINTK(args...)	printk(args)
#define DIPRINTK(n, args...)	if (BIGSUR_DEBUG>(n)) printk(args)
#else
#define DPRINTK(args...)
#define DIPRINTK(n, args...)
#endif


/* Low iomap maps port 0-1K to addresses in 8byte chunks */
#define BIGSUR_IOMAP_LO_THRESH 0x400
#define BIGSUR_IOMAP_LO_SHIFT	3
#define BIGSUR_IOMAP_LO_MASK	((1<<BIGSUR_IOMAP_LO_SHIFT)-1)
#define BIGSUR_IOMAP_LO_NMAP	(BIGSUR_IOMAP_LO_THRESH>>BIGSUR_IOMAP_LO_SHIFT)
static u32 bigsur_iomap_lo[BIGSUR_IOMAP_LO_NMAP];
static u8 bigsur_iomap_lo_shift[BIGSUR_IOMAP_LO_NMAP];

/* High iomap maps port 1K-64K to addresses in 1K chunks */
#define BIGSUR_IOMAP_HI_THRESH 0x10000
#define BIGSUR_IOMAP_HI_SHIFT	10
#define BIGSUR_IOMAP_HI_MASK	((1<<BIGSUR_IOMAP_HI_SHIFT)-1)
#define BIGSUR_IOMAP_HI_NMAP	(BIGSUR_IOMAP_HI_THRESH>>BIGSUR_IOMAP_HI_SHIFT)
static u32 bigsur_iomap_hi[BIGSUR_IOMAP_HI_NMAP];
static u8 bigsur_iomap_hi_shift[BIGSUR_IOMAP_HI_NMAP];

#ifndef MAX
#define MAX(a,b)    ((a)>(b)?(a):(b))
#endif

#define PORT2ADDR(x) (sh_mv.mv_isa_port2addr(x))

void bigsur_port_map(u32 baseport, u32 nports, u32 addr, u8 shift)
{
    u32 port, endport = baseport + nports;

    DPRINTK("bigsur_port_map(base=0x%0x, n=0x%0x, addr=0x%08x)\n",
	    baseport, nports, addr);
	    
	for (port = baseport ;
	     port < endport && port < BIGSUR_IOMAP_LO_THRESH ;
	     port += (1<<BIGSUR_IOMAP_LO_SHIFT)) {
	    	DPRINTK("    maplo[0x%x] = 0x%08x\n", port, addr);
    	    bigsur_iomap_lo[port>>BIGSUR_IOMAP_LO_SHIFT] = addr;
    	    bigsur_iomap_lo_shift[port>>BIGSUR_IOMAP_LO_SHIFT] = shift;
	    	addr += (1<<(BIGSUR_IOMAP_LO_SHIFT));
	}

	for (port = MAX(baseport, BIGSUR_IOMAP_LO_THRESH) ;
	     port < endport && port < BIGSUR_IOMAP_HI_THRESH ;
	     port += (1<<BIGSUR_IOMAP_HI_SHIFT)) {
	    	DPRINTK("    maphi[0x%x] = 0x%08x\n", port, addr);
    	    bigsur_iomap_hi[port>>BIGSUR_IOMAP_HI_SHIFT] = addr;
    	    bigsur_iomap_hi_shift[port>>BIGSUR_IOMAP_HI_SHIFT] = shift;
	    	addr += (1<<(BIGSUR_IOMAP_HI_SHIFT));
	}
}
EXPORT_SYMBOL(bigsur_port_map);

void bigsur_port_unmap(u32 baseport, u32 nports)
{
    u32 port, endport = baseport + nports;
	
    DPRINTK("bigsur_port_unmap(base=0x%0x, n=0x%0x)\n", baseport, nports);

	for (port = baseport ;
	     port < endport && port < BIGSUR_IOMAP_LO_THRESH ;
	     port += (1<<BIGSUR_IOMAP_LO_SHIFT)) {
    	    bigsur_iomap_lo[port>>BIGSUR_IOMAP_LO_SHIFT] = 0;
	}

	for (port = MAX(baseport, BIGSUR_IOMAP_LO_THRESH) ;
	     port < endport && port < BIGSUR_IOMAP_HI_THRESH ;
	     port += (1<<BIGSUR_IOMAP_HI_SHIFT)) {
    	    bigsur_iomap_hi[port>>BIGSUR_IOMAP_HI_SHIFT] = 0;
	}
}
EXPORT_SYMBOL(bigsur_port_unmap);

unsigned long bigsur_isa_port2addr(unsigned long port)
{
    unsigned long addr = 0;
	unsigned char shift;

	/* Physical address not in P0, do nothing */
	if (PXSEG(port)) addr = port;
	/* physical address in P0, map to P2 */
	else if (port >= 0x30000)
	    addr = P2SEGADDR(port);
	/* Big Sur I/O + HD64465 registers 0x10000-0x30000 */
	else if (port >= BIGSUR_IOMAP_HI_THRESH)
	    addr = BIGSUR_INTERNAL_BASE + (port - BIGSUR_IOMAP_HI_THRESH);
	/* Handle remapping of high IO/PCI IO ports */
	else if (port >= BIGSUR_IOMAP_LO_THRESH) {
	    addr = bigsur_iomap_hi[port >> BIGSUR_IOMAP_HI_SHIFT];
	    shift = bigsur_iomap_hi_shift[port >> BIGSUR_IOMAP_HI_SHIFT];
	    if (addr != 0)
		    addr += (port & BIGSUR_IOMAP_HI_MASK) << shift;
	}
	/* Handle remapping of low IO ports */
	else {
	    addr = bigsur_iomap_lo[port >> BIGSUR_IOMAP_LO_SHIFT];
	    shift = bigsur_iomap_lo_shift[port >> BIGSUR_IOMAP_LO_SHIFT];
	    if (addr != 0)
	    	addr += (port & BIGSUR_IOMAP_LO_MASK) << shift;
	}

    DIPRINTK(2, "PORT2ADDR(0x%08lx) = 0x%08lx\n", port, addr);

	return addr;
}

static inline void delay(void)
{
	ctrl_inw(0xa0000000);
}

unsigned char bigsur_inb(unsigned long port)
{
	unsigned long addr = PORT2ADDR(port);
	unsigned long b = (addr == 0 ? 0 : *(volatile unsigned char*)addr);

	DIPRINTK(0, "inb(%08lx) = %02x\n", addr, (unsigned)b);
	return b;
}

unsigned char bigsur_inb_p(unsigned long port)
{
    unsigned long v;
	unsigned long addr = PORT2ADDR(port);

	v = (addr == 0 ? 0 : *(volatile unsigned char*)addr);
	delay();
	DIPRINTK(0, "inb_p(%08lx) = %02x\n", addr, (unsigned)v);
	return v;
}

unsigned short bigsur_inw(unsigned long port)
{
    unsigned long addr = PORT2ADDR(port);
	unsigned long b = (addr == 0 ? 0 : *(volatile unsigned short*)addr);
	DIPRINTK(0, "inw(%08lx) = %04lx\n", addr, b);
	return b;
}

unsigned int bigsur_inl(unsigned long port)
{
    unsigned long addr = PORT2ADDR(port);
	unsigned int b = (addr == 0 ? 0 : *(volatile unsigned long*)addr);
	DIPRINTK(0, "inl(%08lx) = %08x\n", addr, b);
	return b;
}

void bigsur_insb(unsigned long port, void *buffer, unsigned long count)
{
	unsigned char *buf=buffer;
	while(count--) *buf++=inb(port);
}

void bigsur_insw(unsigned long port, void *buffer, unsigned long count)
{
	unsigned short *buf=buffer;
	while(count--) *buf++=inw(port);
}

void bigsur_insl(unsigned long port, void *buffer, unsigned long count)
{
	unsigned long *buf=buffer;
	while(count--) *buf++=inl(port);
}

void bigsur_outb(unsigned char b, unsigned long port)
{
	unsigned long addr = PORT2ADDR(port);

	DIPRINTK(0, "outb(%02x, %08lx)\n", (unsigned)b, addr);
	if (addr != 0)
	    *(volatile unsigned char*)addr = b;
}

void bigsur_outb_p(unsigned char b, unsigned long port)
{
	unsigned long addr = PORT2ADDR(port);

	DIPRINTK(0, "outb_p(%02x, %08lx)\n", (unsigned)b, addr);
    if (addr != 0)
	    *(volatile unsigned char*)addr = b;
	delay();
}

void bigsur_outw(unsigned short b, unsigned long port)
{
	unsigned long addr = PORT2ADDR(port);
	DIPRINTK(0, "outw(%04x, %08lx)\n", (unsigned)b, addr);
	if (addr != 0)
	    *(volatile unsigned short*)addr = b;
}

void bigsur_outl(unsigned int b, unsigned long port)
{
	unsigned long addr = PORT2ADDR(port);
	DIPRINTK(0, "outl(%08x, %08lx)\n", b, addr);
	if (addr != 0)
            *(volatile unsigned long*)addr = b;
}

void bigsur_outsb(unsigned long port, const void *buffer, unsigned long count)
{
	const unsigned char *buf=buffer;
	while(count--) outb(*buf++, port);
}

void bigsur_outsw(unsigned long port, const void *buffer, unsigned long count)
{
	const unsigned short *buf=buffer;
	while(count--) outw(*buf++, port);
}

void bigsur_outsl(unsigned long port, const void *buffer, unsigned long count)
{
	const unsigned long *buf=buffer;
	while(count--) outl(*buf++, port);
}

