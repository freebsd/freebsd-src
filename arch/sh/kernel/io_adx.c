/* 
 * linux/arch/sh/kernel/io_adx.c
 *
 * Copyright (C) 2001 A&D Co., Ltd.
 *
 * I/O routine and setup routines for A&D ADX Board
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */

#include <asm/io.h>
#include <asm/machvec.h>
#include <linux/module.h>

#define PORT2ADDR(x) (adx_isa_port2addr(x))

static inline void delay(void)
{
	ctrl_inw(0xa0000000);
}

unsigned char adx_inb(unsigned long port)
{
	return *(volatile unsigned char*)PORT2ADDR(port);
}

unsigned short adx_inw(unsigned long port)
{
	return *(volatile unsigned short*)PORT2ADDR(port);
}

unsigned int adx_inl(unsigned long port)
{
	return *(volatile unsigned long*)PORT2ADDR(port);
}

unsigned char adx_inb_p(unsigned long port)
{
	unsigned long v = *(volatile unsigned char*)PORT2ADDR(port);

	delay();
	return v;
}

unsigned short adx_inw_p(unsigned long port)
{
	unsigned long v = *(volatile unsigned short*)PORT2ADDR(port);

	delay();
	return v;
}

unsigned int adx_inl_p(unsigned long port)
{
	unsigned long v = *(volatile unsigned long*)PORT2ADDR(port);

	delay();
	return v;
}

void adx_insb(unsigned long port, void *buffer, unsigned long count)
{
	unsigned char *buf = buffer;
	while(count--) *buf++ = inb(port);
}

void adx_insw(unsigned long port, void *buffer, unsigned long count)
{
	unsigned short *buf = buffer;
	while(count--) *buf++ = inw(port);
}

void adx_insl(unsigned long port, void *buffer, unsigned long count)
{
	unsigned long *buf = buffer;
	while(count--) *buf++ = inl(port);
}

void adx_outb(unsigned char b, unsigned long port)
{
	*(volatile unsigned char*)PORT2ADDR(port) = b;
}

void adx_outw(unsigned short b, unsigned long port)
{
	*(volatile unsigned short*)PORT2ADDR(port) = b;
}

void adx_outl(unsigned int b, unsigned long port)
{
	*(volatile unsigned long*)PORT2ADDR(port) = b;
}

void adx_outb_p(unsigned char b, unsigned long port)
{
	*(volatile unsigned char*)PORT2ADDR(port) = b;
	delay();
}

void adx_outw_p(unsigned short b, unsigned long port)
{
	*(volatile unsigned short*)PORT2ADDR(port) = b;
	delay();
}

void adx_outl_p(unsigned int b, unsigned long port)
{
	*(volatile unsigned long*)PORT2ADDR(port) = b;
	delay();
}

void adx_outsb(unsigned long port, const void *buffer, unsigned long count)
{
	const unsigned char *buf = buffer;
	while(count--) outb(*buf++, port);
}

void adx_outsw(unsigned long port, const void *buffer, unsigned long count)
{
	const unsigned short *buf = buffer;
	while(count--) outw(*buf++, port);
}

void adx_outsl(unsigned long port, const void *buffer, unsigned long count)
{
	const unsigned long *buf = buffer;
	while(count--) outl(*buf++, port);
}

unsigned char adx_readb(unsigned long addr)
{
	return *(volatile unsigned char*)addr;
}

unsigned short adx_readw(unsigned long addr)
{
	return *(volatile unsigned short*)addr;
}

unsigned int adx_readl(unsigned long addr)
{
	return *(volatile unsigned long*)addr;
}

void adx_writeb(unsigned char b, unsigned long addr)
{
	*(volatile unsigned char*)addr = b;
}

void adx_writew(unsigned short b, unsigned long addr)
{
	*(volatile unsigned short*)addr = b;
}

void adx_writel(unsigned int b, unsigned long addr)
{
	*(volatile unsigned long*)addr = b;
}

void *adx_ioremap(unsigned long offset, unsigned long size)
{
	return (void *)P2SEGADDR(offset);
}

EXPORT_SYMBOL (adx_ioremap);

void adx_iounmap(void *addr)
{
}

EXPORT_SYMBOL(adx_iounmap);

#include <linux/vmalloc.h>
extern void *cf_io_base;

unsigned long adx_isa_port2addr(unsigned long offset)
{
  /* CompactFlash (IDE) */
	if (((offset >= 0x1f0) && (offset <= 0x1f7)) || (offset == 0x3f6)) {
		return (unsigned long)cf_io_base + offset;
	}

  /* eth0 */
	if ((offset >= 0x300) && (offset <= 0x30f)) {
		return 0xa5000000 + offset;	/* COMM BOARD (AREA1) */
	}

	return offset + 0xb0000000; /* IOBUS (AREA 4)*/
}
