/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2015 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef	_LINUX_IO_H_
#define	_LINUX_IO_H_

#include <machine/vm.h>
#include <sys/endian.h>
#include <sys/types.h>

#include <linux/compiler.h>
#include <linux/types.h>

static inline uint32_t
__raw_readl(const volatile void *addr)
{
	return *(const volatile uint32_t *)addr;
}

static inline void
__raw_writel(uint32_t b, volatile void *addr)
{
	*(volatile uint32_t *)addr = b;
}

static inline uint64_t
__raw_readq(const volatile void *addr)
{
	return *(const volatile uint64_t *)addr;
}

static inline void
__raw_writeq(uint64_t b, volatile void *addr)
{
	*(volatile uint64_t *)addr = b;
}

/*
 * XXX This is all x86 specific.  It should be bus space access.
 */
#define	mmiowb()	barrier()

#undef writel
static inline void
writel(uint32_t b, void *addr)
{
	*(volatile uint32_t *)addr = b;
}

#undef writel_relaxed
static inline void
writel_relaxed(uint32_t b, void *addr)
{
	*(volatile uint32_t *)addr = b;
}

#undef writeq
static inline void
writeq(uint64_t b, void *addr)
{
	*(volatile uint64_t *)addr = b;
}

#undef writeb
static inline void
writeb(uint8_t b, void *addr)
{
	*(volatile uint8_t *)addr = b;
}

#undef writew
static inline void
writew(uint16_t b, void *addr)
{
	*(volatile uint16_t *)addr = b;
}

#undef ioread8
static inline uint8_t
ioread8(const volatile void *addr)
{
	return *(const volatile uint8_t *)addr;
}

#undef ioread16
static inline uint16_t
ioread16(const volatile void *addr)
{
	return *(const volatile uint16_t *)addr;
}

#undef ioread16be
static inline uint16_t
ioread16be(const volatile void *addr)
{
	return be16toh(*(const volatile uint16_t *)addr);
}

#undef ioread32
static inline uint32_t
ioread32(const volatile void *addr)
{
	return *(const volatile uint32_t *)addr;
}

#undef ioread32be
static inline uint32_t
ioread32be(const volatile void *addr)
{
	return be32toh(*(const volatile uint32_t *)addr);
}

#undef iowrite8
static inline void
iowrite8(uint8_t v, volatile void *addr)
{
	*(volatile uint8_t *)addr = v;
}

#undef iowrite16
static inline void
iowrite16(uint16_t v, volatile void *addr)
{
	*(volatile uint16_t *)addr = v;
}

#undef iowrite32
static inline void
iowrite32(uint32_t v, volatile void *addr)
{
	*(volatile uint32_t *)addr = v;
}

#undef iowrite32be
static inline void
iowrite32be(uint32_t v, volatile void *addr)
{
	*(volatile uint32_t *)addr = htobe32(v);
}

#undef readb
static inline uint8_t
readb(const volatile void *addr)
{
	return *(const volatile uint8_t *)addr;
}

#undef readw
static inline uint16_t
readw(const volatile void *addr)
{
	return *(const volatile uint16_t *)addr;
}

#undef readl
static inline uint32_t
readl(const volatile void *addr)
{
	return *(const volatile uint32_t *)addr;
}

#if defined(__i386__) || defined(__amd64__)
static inline void
_outb(u_char data, u_int port)
{
	__asm __volatile("outb %0, %w1" : : "a" (data), "Nd" (port));
}
#endif

#if defined(__i386__) || defined(__amd64__) || defined(__powerpc__)
void *_ioremap_attr(vm_paddr_t phys_addr, unsigned long size, int attr);
#else
#define	_ioremap_attr(...) NULL
#endif

#define	ioremap_nocache(addr, size)					\
    _ioremap_attr((addr), (size), VM_MEMATTR_UNCACHEABLE)
#define	ioremap_wc(addr, size)						\
    _ioremap_attr((addr), (size), VM_MEMATTR_WRITE_COMBINING)
#define	ioremap_wb(addr, size)						\
    _ioremap_attr((addr), (size), VM_MEMATTR_WRITE_BACK)
#define	ioremap_wt(addr, size)						\
    _ioremap_attr((addr), (size), VM_MEMATTR_WRITE_THROUGH)
#define	ioremap(addr, size)						\
    _ioremap_attr((addr), (size), VM_MEMATTR_UNCACHEABLE)
void iounmap(void *addr);

#define	memset_io(a, b, c)	memset((a), (b), (c))
#define	memcpy_fromio(a, b, c)	memcpy((a), (b), (c))
#define	memcpy_toio(a, b, c)	memcpy((a), (b), (c))

static inline void
__iowrite32_copy(void *to, void *from, size_t count)
{
	uint32_t *src;
	uint32_t *dst;
	int i;

	for (i = 0, src = from, dst = to; i < count; i++, src++, dst++)
		__raw_writel(*src, dst);
}

static inline void
__iowrite64_copy(void *to, void *from, size_t count)
{
#ifdef __LP64__
	uint64_t *src;
	uint64_t *dst;
	int i;

	for (i = 0, src = from, dst = to; i < count; i++, src++, dst++)
		__raw_writeq(*src, dst);
#else
	__iowrite32_copy(to, from, count * 2);
#endif
}

enum {
	MEMREMAP_WB = 1 << 0,
	MEMREMAP_WT = 1 << 1,
	MEMREMAP_WC = 1 << 2,
};

static inline void *
memremap(resource_size_t offset, size_t size, unsigned long flags)
{
	void *addr = NULL;

	if ((flags & MEMREMAP_WB) &&
	    (addr = ioremap_wb(offset, size)) != NULL)
		goto done;
	if ((flags & MEMREMAP_WT) &&
	    (addr = ioremap_wt(offset, size)) != NULL)
		goto done;
	if ((flags & MEMREMAP_WC) &&
	    (addr = ioremap_wc(offset, size)) != NULL)
		goto done;
done:
	return (addr);
}

static inline void
memunmap(void *addr)
{
	/* XXX May need to check if this is RAM */
	iounmap(addr);
}

#endif	/* _LINUX_IO_H_ */
