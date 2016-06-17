#ifndef __ASM_SH64_IO_H
#define __ASM_SH64_IO_H

/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * include/asm-sh64/io.h
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 * Copyright (C) 2003  Paul Mundt
 *
 */

/*
 * Convention:
 *    read{b,w,l}/write{b,w,l} are for PCI,
 *    while in{b,w,l}/out{b,w,l} are for ISA
 * These may (will) be platform specific function.
 *
 * In addition, we have 
 *   ctrl_in{b,w,l}/ctrl_out{b,w,l} for SuperH specific I/O.
 * which are processor specific. Address should be the result of
 * onchip_remap();
 */

#include <asm/cache.h>
#include <asm/system.h>

#define virt_to_bus virt_to_phys
#define bus_to_virt phys_to_virt
#define page_to_bus page_to_phys

/*
 * Nothing overly special here.. instead of doing the same thing
 * over and over again, we just define a set of sh64_in/out functions
 * with an implicit size. The traditional read{b,w,l}/write{b,w,l}
 * mess is wrapped to this, as are the SH-specific ctrl_in/out routines.
 */
static inline unsigned char sh64_in8(unsigned long addr)
{
	return *(volatile unsigned char *)addr;
}

static inline unsigned short sh64_in16(unsigned long addr)
{
	return *(volatile unsigned short *)addr;
}

static inline unsigned long sh64_in32(unsigned long addr)
{
	return *(volatile unsigned long *)addr;
}

static inline unsigned long long sh64_in64(unsigned long addr)
{
	return *(volatile unsigned long long *)addr;
}

static inline void sh64_out8(unsigned char b, unsigned long addr)
{
	*(volatile unsigned char *)addr = b;
	wmb();
}

static inline void sh64_out16(unsigned short b, unsigned long addr)
{
	*(volatile unsigned short *)addr = b;
	wmb();
}

static inline void sh64_out32(unsigned long b, unsigned long addr)
{
	*(volatile unsigned long *)addr = b;
	wmb();
}

static inline void sh64_out64(unsigned long long b, unsigned long addr)
{
	*(volatile unsigned long long *)addr = b;
	wmb();
}

#define readb(addr)		sh64_in8(addr)
#define readw(addr)		sh64_in16(addr)
#define readl(addr)		sh64_in32(addr)

#define writeb(b, addr)		sh64_out8(b, addr)
#define writew(b, addr)		sh64_out16(b, addr)
#define writel(b, addr)		sh64_out32(b, addr)

#define ctrl_inb(addr)		sh64_in8(addr)
#define ctrl_inw(addr)		sh64_in16(addr)
#define ctrl_inl(addr)		sh64_in32(addr)

#define ctrl_outb(b, addr)	sh64_out8(b, addr)
#define ctrl_outw(b, addr)	sh64_out16(b, addr)
#define ctrl_outl(b, addr)	sh64_out32(b, addr)

unsigned long inb(unsigned long port);
unsigned long inw(unsigned long port);
unsigned long inl(unsigned long port);
void outb(unsigned long value, unsigned long port);
void outw(unsigned long value, unsigned long port);
void outl(unsigned long value, unsigned long port);

#ifdef __KERNEL__

#define IO_SPACE_LIMIT 0xffffffff

/*
 * Change virtual addresses to physical addresses and vv.
 * These are trivial on the 1:1 Linux/SuperH mapping
 */
extern __inline__ unsigned long virt_to_phys(volatile void * address)
{
	return __pa(address);
}

extern __inline__ void * phys_to_virt(unsigned long address)
{
	return __va(address);
}

extern void * __ioremap(unsigned long phys_addr, unsigned long size,
			unsigned long flags);

extern __inline__ void * ioremap(unsigned long phys_addr, unsigned long size)
{
	return __ioremap(phys_addr, size, 1);
}
	
extern __inline__ void * ioremap_nocache (unsigned long phys_addr, unsigned long size)
{
	return __ioremap(phys_addr, size, 0);
}

extern void iounmap(void *addr);

unsigned long onchip_remap(unsigned long addr, unsigned long size, const char* name);
extern void onchip_unmap(unsigned long vaddr);

static __inline__ int check_signature(unsigned long io_addr,
			const unsigned char *signature, int length)
{
	int retval = 0;
	do {
		if (readb(io_addr) != *signature)
			goto out;
		io_addr++;
		signature++;
		length--;
	} while (length);
	retval = 1;
out:
	return retval;
}

/*
 * The caches on some architectures aren't dma-coherent and have need to
 * handle this in software.  There are three types of operations that
 * can be applied to dma buffers.
 *
 *  - dma_cache_wback_inv(start, size) makes caches and RAM coherent by
 *    writing the content of the caches back to memory, if necessary.
 *    The function also invalidates the affected part of the caches as
 *    necessary before DMA transfers from outside to memory.
 *  - dma_cache_inv(start, size) invalidates the affected parts of the
 *    caches.  Dirty lines of the caches may be written back or simply
 *    be discarded.  This operation is necessary before dma operations
 *    to the memory.
 *  - dma_cache_wback(start, size) writes back any dirty lines but does
 *    not invalidate the cache.  This can be used before DMA reads from
 *    memory,
 */

/*
 * Implemented despite DMA is not yet supported on ST50.
 *
 * Also note that PCI DMA is supposed to be cache coherent,
 * therefore these should not be used by PCI device drivers.
 *
 */

static __inline__ void dma_cache_wback_inv (unsigned long start, unsigned long size)
{
	unsigned long s = start & L1_CACHE_ALIGN_MASK;
	unsigned long e = (start + size) & L1_CACHE_ALIGN_MASK;

	for (; s <= e; s += L1_CACHE_BYTES)
		asm volatile ("ocbp	%0, 0" : : "r" (s));
}

static __inline__ void dma_cache_inv (unsigned long start, unsigned long size)
{
	// Note that caller has to be careful with overzealous
	// invalidation should there be partial cache lines at the extremities 
	// of the specified range 
	unsigned long s = start & L1_CACHE_ALIGN_MASK;
	unsigned long e = (start + size) & L1_CACHE_ALIGN_MASK;

	for (; s <= e; s += L1_CACHE_BYTES)
		asm volatile ("ocbi	%0, 0" : : "r" (s));
}

static __inline__ void dma_cache_wback (unsigned long start, unsigned long size)
{
	unsigned long s = start & L1_CACHE_ALIGN_MASK;
	unsigned long e = (start + size) & L1_CACHE_ALIGN_MASK;

	for (; s <= e; s += L1_CACHE_BYTES)
		asm volatile ("ocbwb	%0, 0" : : "r" (s));
}

#endif /* __KERNEL__ */
#endif /* __ASM_SH64_IO_H */
