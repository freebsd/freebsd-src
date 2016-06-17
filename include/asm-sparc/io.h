/*
 * $Id: io.h,v 1.29 2001/11/10 09:28:34 davem Exp $
 */
#ifndef __SPARC_IO_H
#define __SPARC_IO_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ioport.h>  /* struct resource */

#include <asm/page.h>      /* IO address mapping routines need this */
#include <asm/system.h>

#define virt_to_bus virt_to_phys
#define bus_to_virt phys_to_virt
#define page_to_phys(page)     ((((page) - mem_map) << PAGE_SHIFT)+phys_base)

static __inline__ u32 flip_dword (u32 d)
{
	return ((d&0xff)<<24) | (((d>>8)&0xff)<<16) | (((d>>16)&0xff)<<8)| ((d>>24)&0xff);
}

static __inline__ u16 flip_word (u16 d)
{
	return ((d&0xff) << 8) | ((d>>8)&0xff);
}

/*
 * Memory mapped I/O to PCI
 */
static __inline__ u8 readb(unsigned long addr)
{
	return *(volatile u8 *)addr;
}

static __inline__ u16 readw(unsigned long addr)
{
	return flip_word(*(volatile u16 *)addr);
}

static __inline__ u32 readl(unsigned long addr)
{
	return flip_dword(*(volatile u32 *)addr);
}

static __inline__ void writeb(u8 b, unsigned long addr)
{
	*(volatile u8 *)addr = b;
}

static __inline__ void writew(u16 b, unsigned long addr)
{
	*(volatile u16 *)addr = flip_word(b);
}

static __inline__ void writel(u32 b, unsigned long addr)
{
	*(volatile u32 *)addr = flip_dword(b);
}

/* Now the 'raw' versions. */
static __inline__ u8 __raw_readb(unsigned long addr)
{
	return *(volatile u8 *)addr;
}

static __inline__ u16 __raw_readw(unsigned long addr)
{
	return *(volatile u16 *)addr;
}

static __inline__ u32 __raw_readl(unsigned long addr)
{
	return *(volatile u32 *)addr;
}

static __inline__ void __raw_writeb(u8 b, unsigned long addr)
{
	*(volatile u8 *)addr = b;
}

static __inline__ void __raw_writew(u16 b, unsigned long addr)
{
	*(volatile u16 *)addr = b;
}

static __inline__ void __raw_writel(u32 b, unsigned long addr)
{
	*(volatile u32 *)addr = b;
}

/*
 * I/O space operations
 *
 * Arrangement on a Sun is somewhat complicated.
 *
 * First of all, we want to use standard Linux drivers
 * for keyboard, PC serial, etc. These drivers think
 * they access I/O space and use inb/outb.
 * On the other hand, EBus bridge accepts PCI *memory*
 * cycles and converts them into ISA *I/O* cycles.
 * Ergo, we want inb & outb to generate PCI memory cycles.
 *
 * If we want to issue PCI *I/O* cycles, we do this
 * with a low 64K fixed window in PCIC. This window gets
 * mapped somewhere into virtual kernel space and we
 * can use inb/outb again.
 */
#define inb_local(addr)		readb(addr)
#define inb(addr)		readb(addr)
#define inw(addr)		readw(addr)
#define inl(addr)		readl(addr)
#define inb_p(addr)		readb(addr)

#define outb_local(b, addr)	writeb(b, addr)
#define outb(b, addr)		writeb(b, addr)
#define outw(b, addr)		writew(b, addr)
#define outl(b, addr)		writel(b, addr)
#define outb_p(b, addr)		writeb(b, addr)

extern void outsb(unsigned long addr, const void *src, unsigned long cnt);
extern void outsw(unsigned long addr, const void *src, unsigned long cnt);
extern void outsl(unsigned long addr, const void *src, unsigned long cnt);
extern void insb(unsigned long addr, void *dst, unsigned long count);
extern void insw(unsigned long addr, void *dst, unsigned long count);
extern void insl(unsigned long addr, void *dst, unsigned long count);

#define IO_SPACE_LIMIT 0xffffffff

/*
 * SBus accessors.
 *
 * SBus has only one, memory mapped, I/O space.
 * We do not need to flip bytes for SBus of course.
 */
static __inline__ u8 _sbus_readb(unsigned long addr)
{
	return *(volatile u8 *)addr;
}

static __inline__ u16 _sbus_readw(unsigned long addr)
{
	return *(volatile u16 *)addr;
}

static __inline__ u32 _sbus_readl(unsigned long addr)
{
	return *(volatile u32 *)addr;
}

static __inline__ void _sbus_writeb(u8 b, unsigned long addr)
{
	*(volatile u8 *)addr = b;
}

static __inline__ void _sbus_writew(u16 b, unsigned long addr)
{
	*(volatile u16 *)addr = b;
}

static __inline__ void _sbus_writel(u32 b, unsigned long addr)
{
	*(volatile u32 *)addr = b;
}

/*
 * The only reason for #define's is to hide casts to unsigned long.
 * XXX Rewrite drivers without structures for registers.
 */
#define sbus_readb(a)		_sbus_readb((unsigned long)(a))
#define sbus_readw(a)		_sbus_readw((unsigned long)(a))
#define sbus_readl(a)		_sbus_readl((unsigned long)(a))
#define sbus_writeb(v, a)	_sbus_writeb(v, (unsigned long)(a))
#define sbus_writew(v, a)	_sbus_writew(v, (unsigned long)(a))
#define sbus_writel(v, a)	_sbus_writel(v, (unsigned long)(a))

static inline void *sbus_memset_io(void *__dst, int c, __kernel_size_t n)
{
	unsigned long dst = (unsigned long)__dst;

	while(n--) {
		sbus_writeb(c, dst);
		dst++;
	}
	return (void *) dst;
}

#ifdef __KERNEL__

/*
 * Bus number may be embedded in the higher bits of the physical address.
 * This is why we have no bus number argument to ioremap().
 */
extern void *ioremap(unsigned long offset, unsigned long size);
#define ioremap_nocache(X,Y)	ioremap((X),(Y))
extern void iounmap(void *addr);

/* P3: talk davem into dropping "name" argument in favor of res->name */
/*
 * Bus number may be in res->flags... somewhere.
 */
extern unsigned long sbus_ioremap(struct resource *res, unsigned long offset,
    unsigned long size, char *name);
/* XXX Partial deallocations? I think not! */
extern void sbus_iounmap(unsigned long vaddr, unsigned long size);


#define virt_to_phys(x) __pa((unsigned long)(x))
#define phys_to_virt(x) __va((unsigned long)(x))

/*
 * At the moment, we do not use CMOS_READ anywhere outside of rtc.c,
 * so rtc_port is static in it. This should not change unless a new
 * hardware pops up.
 */
#define RTC_PORT(x)   (rtc_port + (x))
#define RTC_ALWAYS_BCD  0

/* Nothing to do */
/* P3: Only IDE DMA may need these. */

#define dma_cache_inv(_start,_size)		do { } while (0)
#define dma_cache_wback(_start,_size)		do { } while (0)
#define dma_cache_wback_inv(_start,_size)	do { } while (0)

#endif

#endif /* !(__SPARC_IO_H) */
