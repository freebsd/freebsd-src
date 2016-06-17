/* 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2003 Silicon Graphics, Inc. All rights reserved.
 *
 * The generic kernel requires function pointers to these routines, so
 * we wrap the inlines from asm/ia64/sn/sn2/io.h here.
 */

#include <asm/sn/sn2/io.h>

unsigned int
sn_inb (unsigned long port)
{
	return __sn_inb(port);
}

unsigned int
sn_inw (unsigned long port)
{
	return __sn_inw(port);
}

unsigned int
sn_inl (unsigned long port)
{
	return __sn_inl(port);
}

void
sn_outb (unsigned char val, unsigned long port)
{
	__sn_outb(val, port);
}

void
sn_outw (unsigned short val, unsigned long port)
{
	__sn_outw(val, port);
}

void
sn_outl (unsigned int val, unsigned long port)
{
	__sn_outl(val, port);
}

unsigned char
sn_readb (void *addr)
{
	return __sn_readb (addr);
}

unsigned short
sn_readw (void *addr)
{
	return __sn_readw (addr);
}

unsigned int
sn_readl (void *addr)
{
	return __sn_readl (addr);
}

unsigned long
sn_readq (void *addr)
{
	return __sn_readq (addr);
}


/* define aliases: */

asm (".global __sn_inb, __sn_inw, __sn_inl");
asm ("__sn_inb = sn_inb");
asm ("__sn_inw = sn_inw");
asm ("__sn_inl = sn_inl");

asm (".global __sn_outb, __sn_outw, __sn_outl");
asm ("__sn_outb = sn_outb");
asm ("__sn_outw = sn_outw");
asm ("__sn_outl = sn_outl");

asm (".global __sn_readb, __sn_readw, __sn_readl, __sn_readq");
asm ("__sn_readb = sn_readb");
asm ("__sn_readw = sn_readw");
asm ("__sn_readl = sn_readl");
asm ("__sn_readq = sn_readq");
