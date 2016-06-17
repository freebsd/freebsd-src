/* $Id: io.h,v 1.41.2.1 2001/12/11 22:50:52 davem Exp $ */
#ifndef __SPARC64_IO_H
#define __SPARC64_IO_H

#include <linux/kernel.h>
#include <linux/types.h>

#include <asm/page.h>      /* IO address mapping routines need this */
#include <asm/system.h>
#include <asm/asi.h>

/* PC crapola... */
#define __SLOW_DOWN_IO	do { } while (0)
#define SLOW_DOWN_IO	do { } while (0)

extern unsigned long virt_to_bus_not_defined_use_pci_map(volatile void *addr);
#define virt_to_bus virt_to_bus_not_defined_use_pci_map
extern unsigned long bus_to_virt_not_defined_use_pci_map(volatile void *addr);
#define bus_to_virt bus_to_virt_not_defined_use_pci_map

extern unsigned long phys_base;
#define page_to_phys(page)	((((page) - mem_map) << PAGE_SHIFT)+phys_base)

/* Different PCI controllers we support have their PCI MEM space
 * mapped to an either 2GB (Psycho) or 4GB (Sabre) aligned area,
 * so need to chop off the top 33 or 32 bits.
 */
extern unsigned long pci_memspace_mask;

#define bus_dvma_to_mem(__vaddr) ((__vaddr) & pci_memspace_mask)

static __inline__ u8 inb(unsigned long addr)
{
	u8 ret;

	__asm__ __volatile__("lduba\t[%1] %2, %0\t/* pci_inb */"
			     : "=r" (ret)
			     : "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E_L));

	return ret;
}

static __inline__ u16 inw(unsigned long addr)
{
	u16 ret;

	__asm__ __volatile__("lduha\t[%1] %2, %0\t/* pci_inw */"
			     : "=r" (ret)
			     : "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E_L));

	return ret;
}

static __inline__ u32 inl(unsigned long addr)
{
	u32 ret;

	__asm__ __volatile__("lduwa\t[%1] %2, %0\t/* pci_inl */"
			     : "=r" (ret)
			     : "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E_L));

	return ret;
}

static __inline__ void outb(u8 b, unsigned long addr)
{
	__asm__ __volatile__("stba\t%r0, [%1] %2\t/* pci_outb */"
			     : /* no outputs */
			     : "Jr" (b), "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E_L));
}

static __inline__ void outw(u16 w, unsigned long addr)
{
	__asm__ __volatile__("stha\t%r0, [%1] %2\t/* pci_outw */"
			     : /* no outputs */
			     : "Jr" (w), "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E_L));
}

static __inline__ void outl(u32 l, unsigned long addr)
{
	__asm__ __volatile__("stwa\t%r0, [%1] %2\t/* pci_outl */"
			     : /* no outputs */
			     : "Jr" (l), "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E_L));
}

#define inb_p inb
#define outb_p outb
#define inw_p inw
#define outw_p outw
#define inl_p inl
#define outl_p outl

extern void outsb(unsigned long addr, const void *src, unsigned long count);
extern void outsw(unsigned long addr, const void *src, unsigned long count);
extern void outsl(unsigned long addr, const void *src, unsigned long count);
extern void insb(unsigned long addr, void *dst, unsigned long count);
extern void insw(unsigned long addr, void *dst, unsigned long count);
extern void insl(unsigned long addr, void *dst, unsigned long count);

/* Memory functions, same as I/O accesses on Ultra. */
static __inline__ u8 _readb(unsigned long addr)
{
	u8 ret;

	__asm__ __volatile__("lduba\t[%1] %2, %0\t/* pci_readb */"
			     : "=r" (ret)
			     : "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E_L));

	return ret;
}

static __inline__ u16 _readw(unsigned long addr)
{
	u16 ret;

	__asm__ __volatile__("lduha\t[%1] %2, %0\t/* pci_readw */"
			     : "=r" (ret)
			     : "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E_L));

	return ret;
}

static __inline__ u32 _readl(unsigned long addr)
{
	u32 ret;

	__asm__ __volatile__("lduwa\t[%1] %2, %0\t/* pci_readl */"
			     : "=r" (ret)
			     : "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E_L));

	return ret;
}

static __inline__ u64 _readq(unsigned long addr)
{
	u64 ret;

	__asm__ __volatile__("ldxa\t[%1] %2, %0\t/* pci_readq */"
			     : "=r" (ret)
			     : "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E_L));

	return ret;
}

static __inline__ void _writeb(u8 b, unsigned long addr)
{
	__asm__ __volatile__("stba\t%r0, [%1] %2\t/* pci_writeb */"
			     : /* no outputs */
			     : "Jr" (b), "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E_L));
}

static __inline__ void _writew(u16 w, unsigned long addr)
{
	__asm__ __volatile__("stha\t%r0, [%1] %2\t/* pci_writew */"
			     : /* no outputs */
			     : "Jr" (w), "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E_L));
}

static __inline__ void _writel(u32 l, unsigned long addr)
{
	__asm__ __volatile__("stwa\t%r0, [%1] %2\t/* pci_writel */"
			     : /* no outputs */
			     : "Jr" (l), "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E_L));
}

static __inline__ void _writeq(u64 q, unsigned long addr)
{
	__asm__ __volatile__("stxa\t%r0, [%1] %2\t/* pci_writeq */"
			     : /* no outputs */
			     : "Jr" (q), "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E_L));
}

#define readb(__addr)		(_readb((unsigned long)(__addr)))
#define readw(__addr)		(_readw((unsigned long)(__addr)))
#define readl(__addr)		(_readl((unsigned long)(__addr)))
#define readq(__addr)		(_readq((unsigned long)(__addr)))
#define writeb(__b, __addr)	(_writeb((u8)(__b), (unsigned long)(__addr)))
#define writew(__w, __addr)	(_writew((u16)(__w), (unsigned long)(__addr)))
#define writel(__l, __addr)	(_writel((u32)(__l), (unsigned long)(__addr)))
#define writeq(__q, __addr)	(_writeq((u64)(__q), (unsigned long)(__addr)))

/* Now versions without byte-swapping. */
static __inline__ u8 _raw_readb(unsigned long addr)
{
	u8 ret;

	__asm__ __volatile__("lduba\t[%1] %2, %0\t/* pci_raw_readb */"
			     : "=r" (ret)
			     : "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E));

	return ret;
}

static __inline__ u16 _raw_readw(unsigned long addr)
{
	u16 ret;

	__asm__ __volatile__("lduha\t[%1] %2, %0\t/* pci_raw_readw */"
			     : "=r" (ret)
			     : "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E));

	return ret;
}

static __inline__ u32 _raw_readl(unsigned long addr)
{
	u32 ret;

	__asm__ __volatile__("lduwa\t[%1] %2, %0\t/* pci_raw_readl */"
			     : "=r" (ret)
			     : "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E));

	return ret;
}

static __inline__ u64 _raw_readq(unsigned long addr)
{
	u64 ret;

	__asm__ __volatile__("ldxa\t[%1] %2, %0\t/* pci_raw_readq */"
			     : "=r" (ret)
			     : "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E));

	return ret;
}

static __inline__ void _raw_writeb(u8 b, unsigned long addr)
{
	__asm__ __volatile__("stba\t%r0, [%1] %2\t/* pci_raw_writeb */"
			     : /* no outputs */
			     : "Jr" (b), "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E));
}

static __inline__ void _raw_writew(u16 w, unsigned long addr)
{
	__asm__ __volatile__("stha\t%r0, [%1] %2\t/* pci_raw_writew */"
			     : /* no outputs */
			     : "Jr" (w), "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E));
}

static __inline__ void _raw_writel(u32 l, unsigned long addr)
{
	__asm__ __volatile__("stwa\t%r0, [%1] %2\t/* pci_raw_writel */"
			     : /* no outputs */
			     : "Jr" (l), "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E));
}

static __inline__ void _raw_writeq(u64 q, unsigned long addr)
{
	__asm__ __volatile__("stxa\t%r0, [%1] %2\t/* pci_raw_writeq */"
			     : /* no outputs */
			     : "Jr" (q), "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E));
}

#define __raw_readb(__addr)		(_raw_readb((unsigned long)(__addr)))
#define __raw_readw(__addr)		(_raw_readw((unsigned long)(__addr)))
#define __raw_readl(__addr)		(_raw_readl((unsigned long)(__addr)))
#define __raw_readq(__addr)		(_raw_readq((unsigned long)(__addr)))
#define __raw_writeb(__b, __addr)	(_raw_writeb((u8)(__b), (unsigned long)(__addr)))
#define __raw_writew(__w, __addr)	(_raw_writew((u16)(__w), (unsigned long)(__addr)))
#define __raw_writel(__l, __addr)	(_raw_writel((u32)(__l), (unsigned long)(__addr)))
#define __raw_writeq(__q, __addr)	(_raw_writeq((u64)(__q), (unsigned long)(__addr)))

/* Valid I/O Space regions are anywhere, because each PCI bus supported
 * can live in an arbitrary area of the physical address range.
 */
#define IO_SPACE_LIMIT 0xffffffffffffffffUL

/* Now, SBUS variants, only difference from PCI is that we do
 * not use little-endian ASIs.
 */
static __inline__ u8 _sbus_readb(unsigned long addr)
{
	u8 ret;

	__asm__ __volatile__("lduba\t[%1] %2, %0\t/* sbus_readb */"
			     : "=r" (ret)
			     : "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E));

	return ret;
}

static __inline__ u16 _sbus_readw(unsigned long addr)
{
	u16 ret;

	__asm__ __volatile__("lduha\t[%1] %2, %0\t/* sbus_readw */"
			     : "=r" (ret)
			     : "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E));

	return ret;
}

static __inline__ u32 _sbus_readl(unsigned long addr)
{
	u32 ret;

	__asm__ __volatile__("lduwa\t[%1] %2, %0\t/* sbus_readl */"
			     : "=r" (ret)
			     : "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E));

	return ret;
}

static __inline__ void _sbus_writeb(u8 b, unsigned long addr)
{
	__asm__ __volatile__("stba\t%r0, [%1] %2\t/* sbus_writeb */"
			     : /* no outputs */
			     : "Jr" (b), "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E));
}

static __inline__ void _sbus_writew(u16 w, unsigned long addr)
{
	__asm__ __volatile__("stha\t%r0, [%1] %2\t/* sbus_writew */"
			     : /* no outputs */
			     : "Jr" (w), "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E));
}

static __inline__ void _sbus_writel(u32 l, unsigned long addr)
{
	__asm__ __volatile__("stwa\t%r0, [%1] %2\t/* sbus_writel */"
			     : /* no outputs */
			     : "Jr" (l), "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E));
}

#define sbus_readb(__addr)		(_sbus_readb((unsigned long)(__addr)))
#define sbus_readw(__addr)		(_sbus_readw((unsigned long)(__addr)))
#define sbus_readl(__addr)		(_sbus_readl((unsigned long)(__addr)))
#define sbus_writeb(__b, __addr)	(_sbus_writeb((__b), (unsigned long)(__addr)))
#define sbus_writew(__w, __addr)	(_sbus_writew((__w), (unsigned long)(__addr)))
#define sbus_writel(__l, __addr)	(_sbus_writel((__l), (unsigned long)(__addr)))

static inline void *_sbus_memset_io(unsigned long dst, int c, __kernel_size_t n)
{
	while(n--) {
		sbus_writeb(c, dst);
		dst++;
	}
	return (void *) dst;
}

#define sbus_memset_io(d,c,sz)	\
	_sbus_memset_io((unsigned long)d,(int)c,(__kernel_size_t)sz)

static inline void *
_memset_io(void *dst, int c, __kernel_size_t n)
{
	char *d = dst;

	while (n--) {
		writeb(c, d);
		d++;
	}

	return dst;
}

#define memset_io(d,c,sz)	\
	_memset_io((void *)d,(int)c,(__kernel_size_t)sz)

static inline void *
_memcpy_fromio(void *dst, unsigned long src, __kernel_size_t n)
{
	char *d = dst;

	while (n--) {
		char tmp = readb(src);
		*d++ = tmp;
		src++;
	}

	return dst;
}

#define memcpy_fromio(d,s,sz)	\
	_memcpy_fromio((void *)d,(unsigned long)s,(__kernel_size_t)sz)

static inline void *
_memcpy_toio(unsigned long dst, const void *src, __kernel_size_t n)
{
	const char *s = src;
	unsigned long d = dst;

	while (n--) {
		char tmp = *s++;
		writeb(tmp, d);
		d++;
	}
	return (void *)dst;
}

#define memcpy_toio(d,s,sz)	\
	_memcpy_toio((unsigned long)d,(const void *)s,(__kernel_size_t)sz)

static inline int check_signature(unsigned long io_addr,
				  const unsigned char *signature,
				  int length)
{
	int retval = 0;
	do {
		if (readb(io_addr++) != *signature++)
			goto out;
	} while (--length);
	retval = 1;
out:
	return retval;
}

#ifdef __KERNEL__

/* On sparc64 we have the whole physical IO address space accessible
 * using physically addressed loads and stores, so this does nothing.
 */
#define ioremap(__offset, __size)	((void *)(__offset))
#define ioremap_nocache(X,Y)		ioremap((X),(Y))
#define iounmap(__addr)			do { (void)(__addr); } while(0)

/* Similarly for SBUS. */
#define sbus_ioremap(__res, __offset, __size, __name) \
({	unsigned long __ret; \
	__ret  = (__res)->start + (((__res)->flags & 0x1ffUL) << 32UL); \
	__ret += (unsigned long) (__offset); \
	if (! request_region((__ret), (__size), (__name))) \
		__ret = 0UL; \
	__ret; \
})

#define sbus_iounmap(__addr, __size)	\
	release_region((__addr), (__size))

/* Nothing to do */

#define dma_cache_inv(_start,_size)		do { } while (0)
#define dma_cache_wback(_start,_size)		do { } while (0)
#define dma_cache_wback_inv(_start,_size)	do { } while (0)

#endif

#endif /* !(__SPARC64_IO_H) */
