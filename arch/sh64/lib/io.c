/* 
 * Copyright (C) 2000 David J. Mckay (david.mckay@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.                            
 *
 * This file contains the I/O routines for use on the overdrive board
 *
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <asm/system.h>
#include <asm/processor.h>
#include <asm/io.h>
#ifdef CONFIG_SH_CAYMAN
#include <asm/cayman.h>
#endif

/*
 * readX/writeX() are used to access memory mapped devices. On some
 * architectures the memory mapped IO stuff needs to be accessed
 * differently. On the SuperH architecture, we just read/write the
 * memory location directly.
 */

#define dprintk(x...)

//#define io_addr(x)  (((unsigned)(x) & 0x000fffff)| PCI_ST50_IO_ADDR )

#ifdef CONFIG_SH_CAYMAN
extern unsigned long smsc_virt;
extern unsigned long pciio_virt;
#define io_addr(x)  ( ((x)<0x400) ? \
			(((x) << 2)|smsc_virt) : \
			((unsigned long)(x)+pciio_virt) )
#else
#define io_addr(x)  ((unsigned long)(x)+pciio_virt)
#endif

unsigned long inb(unsigned long port)
{
	unsigned long r;

	r = ctrl_inb(io_addr(port));
	dprintk("inb(0x%x)=0x%x (0x%x)\n", port, r, io_addr(port));
	return r;
}

unsigned long inw(unsigned long port)
{
	unsigned long r;

	r = ctrl_inw(io_addr(port));
	dprintk("inw(0x%x)=0x%x (0x%x)\n", port, r, io_addr(port));
	return r;
}

unsigned long inl(unsigned long port)
{
	unsigned long r;

	r = ctrl_inl(io_addr(port));
	dprintk("inl(0x%x)=0x%x (0x%x)\n", port, r, io_addr(port));
	return r;
}

void outb(unsigned long value, unsigned long port)
{
	dprintk("outb(0x%x,0x%x) (0x%x)\n", value, port, io_addr(port));
	ctrl_outb(value, io_addr(port));
}

void outw(unsigned long value, unsigned long port)
{
	dprintk("outw(0x%x,0x%x) (0x%x)\n", value, port, io_addr(port));
	ctrl_outw(value, io_addr(port));
}

void outl(unsigned long value, unsigned long port)
{
	dprintk("outw(0x%x,0x%x) (0x%x)\n", value, port, io_addr(port));
	ctrl_outl(value, io_addr(port));
}

/* This is horrible at the moment - needs more work to do something sensible */
#define IO_DELAY()

#define OUT_DELAY(x,type) \
void out##x##_p(unsigned type value,unsigned long port){out##x(value,port);IO_DELAY();}

#define IN_DELAY(x,type) \
unsigned type in##x##_p(unsigned long port) {unsigned type tmp=in##x(port);IO_DELAY();return tmp;}

#if 1
OUT_DELAY(b, long) OUT_DELAY(w, long) OUT_DELAY(l, long)
 IN_DELAY(b, long) IN_DELAY(w, long) IN_DELAY(l, long)
#endif
/*  Now for the string version of these functions */
void outsb(unsigned long port, const void *addr, unsigned long count)
{
	int i;
	unsigned char *p = (unsigned char *) addr;

	for (i = 0; i < count; i++, p++) {
		outb(*p, port);
	}
}

void insb(unsigned long port, void *addr, unsigned long count)
{
	int i;
	unsigned char *p = (unsigned char *) addr;

	for (i = 0; i < count; i++, p++) {
		*p = inb(port);
	}
}

/* For the 16 and 32 bit string functions, we have to worry about alignment.
 * The SH does not do unaligned accesses, so we have to read as bytes and
 * then write as a word or dword. 
 * This can be optimised a lot more, especially in the case where the data
 * is aligned
 */

void outsw(unsigned long port, const void *addr, unsigned long count)
{
	int i;
	unsigned short tmp;
	unsigned char *p = (unsigned char *) addr;

	for (i = 0; i < count; i++, p += 2) {
		tmp = (*p) | ((*(p + 1)) << 8);
		outw(tmp, port);
	}
}

void insw(unsigned long port, void *addr, unsigned long count)
{
	int i;
	unsigned short tmp;
	unsigned char *p = (unsigned char *) addr;

	for (i = 0; i < count; i++, p += 2) {
		tmp = inw(port);
		p[0] = tmp & 0xff;
		p[1] = (tmp >> 8) & 0xff;
	}
}

void outsl(unsigned long port, const void *addr, unsigned long count)
{
	int i;
	unsigned tmp;
	unsigned char *p = (unsigned char *) addr;

	for (i = 0; i < count; i++, p += 4) {
		tmp = (*p) | ((*(p + 1)) << 8) | ((*(p + 2)) << 16) |
		    ((*(p + 3)) << 24);
		outl(tmp, port);
	}
}

void insl(unsigned long port, void *addr, unsigned long count)
{
	int i;
	unsigned tmp;
	unsigned char *p = (unsigned char *) addr;

	for (i = 0; i < count; i++, p += 4) {
		tmp = inl(port);
		p[0] = tmp & 0xff;
		p[1] = (tmp >> 8) & 0xff;
		p[2] = (tmp >> 16) & 0xff;
		p[3] = (tmp >> 24) & 0xff;

	}
}

void memcpy_toio(unsigned long to, const void *from, long count)
{
	unsigned char *p = (unsigned char *) from;

	while (count) {
		count--;
		writeb(*p++, to++);
	}
}

void memcpy_fromio(void *to, unsigned long from, long count)
{
	int i;
	unsigned char *p = (unsigned char *) to;

	for (i = 0; i < count; i++) {
		p[i] = readb(from);
		from++;
	}
}
