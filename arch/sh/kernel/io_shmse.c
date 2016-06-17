/*
 * linux/arch/sh/kernel/io_shmse.c
 *
 * Copyright (C) 2003 YOSHII Takashi <yoshii-takashi@hitachi-ul.co.jp>
 */

#include <linux/kernel.h>
#include <asm/hitachi_shmse.h>
#include <asm/io.h>

#define badio(fn, a) panic("bad i/o operation %s for %08lx.", #fn, a)

struct iop {
	unsigned long start, end;
	unsigned long base;
	struct iop *(*check)(struct iop *p, unsigned long port);
	unsigned char (*inb)(struct iop *p, unsigned long port);
	unsigned short (*inw)(struct iop *p, unsigned long port);
	void (*outb)(struct iop *p, unsigned char value, unsigned long port);
	void (*outw)(struct iop *p, unsigned short value, unsigned long port);
};
 
struct iop *simple_check(struct iop *p, unsigned long port){
	if((p->start <= port) && (port <= p->end))
		return p;
	else
		badio(check, port);
}

unsigned short simple_inw(struct iop *p, unsigned long port){
	return *(unsigned short*)(p->base+port);
}

void simple_outw(struct iop *p, unsigned short value, unsigned long port){
	*(unsigned short*)(p->base+port)=value;
}

unsigned char bad_inb(struct iop *p, unsigned long port)
{
	badio(inb, port);
}

void bad_outb(struct iop *p, unsigned char value, unsigned long port){
	badio(inw, port);
}

/* MSTLANEX01 LAN at 0xb400:0000 */
static struct iop laniop = {
	.start = 0x200,	// device is at 0x300, but start here for probing.
	.end = 0x30f,
	.base = 0xb4000000,
	.check = simple_check,
	.inb = bad_inb,
	.inw = simple_inw,
	.outb = bad_outb,
	.outw = simple_outw,
};

static __inline__ struct iop *port2iop(unsigned long port){
	if(laniop.check(&laniop, port))
		return &laniop;
	else
		badio(check, port); /* XXX: dummy fallback routine? */
}

static inline void delay(void)
{
	ctrl_inw(0xac000000);
	ctrl_inw(0xac000000);
}

unsigned char shmse_inb(unsigned long port)
{
	struct iop *p=port2iop(port);
	return (p->inb)(p, port);
}

unsigned char shmse_inb_p(unsigned long port)
{
	unsigned char v=shmse_inb(port);
	delay();
	return v;
}

unsigned short shmse_inw(unsigned long port)
{
	struct iop *p=port2iop(port);
	return (p->inw)(p, port);
}

unsigned int shmse_inl(unsigned long port)
{
	badio(inl, port);
}

void shmse_outb(unsigned char value, unsigned long port)
{
	struct iop *p=port2iop(port);
	(p->outb)(p, value, port);
}

void shmse_outb_p(unsigned char value, unsigned long port)
{
	shmse_outb(value, port);
	delay();
}

void shmse_outw(unsigned short value, unsigned long port)
{
	struct iop *p=port2iop(port);
	(p->outw)(p, value, port);
}

void shmse_outl(unsigned int value, unsigned long port)
{
	badio(outl, port);
}

void shmse_insb(unsigned long port, void *addr, unsigned long count)
{
	unsigned char *a = addr;
	struct iop *p=port2iop(port);
	while (count--) *a++ = (p->inb)(p, port);
}

void shmse_insw(unsigned long port, void *addr, unsigned long count)
{
	unsigned short *a = addr;
	struct iop *p=port2iop(port);
	while (count--) *a++ = (p->inw)(p, port);
}

void shmse_insl(unsigned long port, void *addr, unsigned long count)
{
	badio(insl, port);
}

void shmse_outsb(unsigned long port, const void *addr, unsigned long count)
{
	unsigned char *a = (unsigned char*)addr;
	struct iop *p=port2iop(port);
	while (count--) (p->outb)(p, *a++, port);
}

void shmse_outsw(unsigned long port, const void *addr, unsigned long count)
{
	unsigned short *a = (unsigned short*)addr;
	struct iop *p=port2iop(port);
	while (count--) (p->outw)(p, *a++, port);
}

void shmse_outsl(unsigned long port, const void *addr, unsigned long count)
{
	badio(outsw, port);
}

unsigned char shmse_readb(unsigned long addr)
{
	badio(readb, addr);
}

unsigned short shmse_readw(unsigned long addr)
{
	badio(readw, addr);
}

unsigned int shmse_readl(unsigned long addr)
{
	badio(readl, addr);
}

void shmse_writeb(unsigned char b, unsigned long addr)
{
	badio(writeb, addr);
}

void shmse_writew(unsigned short b, unsigned long addr)
{
	badio(writew, addr);
}

void shmse_writel(unsigned int b, unsigned long addr)
{
	badio(writel, addr);
}
