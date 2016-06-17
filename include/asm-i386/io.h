#ifndef _ASM_IO_H
#define _ASM_IO_H

#include <linux/config.h>

/*
 * This file contains the definitions for the x86 IO instructions
 * inb/inw/inl/outb/outw/outl and the "string versions" of the same
 * (insb/insw/insl/outsb/outsw/outsl). You can also use "pausing"
 * versions of the single-IO instructions (inb_p/inw_p/..).
 *
 * This file is not meant to be obfuscating: it's just complicated
 * to (a) handle it all in a way that makes gcc able to optimize it
 * as well as possible and (b) trying to avoid writing the same thing
 * over and over again with slight variations and possibly making a
 * mistake somewhere.
 */

/*
 * Thanks to James van Artsdalen for a better timing-fix than
 * the two short jumps: using outb's to a nonexistent port seems
 * to guarantee better timings even on fast machines.
 *
 * On the other hand, I'd like to be sure of a non-existent port:
 * I feel a bit unsafe about using 0x80 (should be safe, though)
 *
 *		Linus
 */

 /*
  *  Bit simplified and optimized by Jan Hubicka
  *  Support of BIGMEM added by Gerhard Wichert, Siemens AG, July 1999.
  *
  *  isa_memset_io, isa_memcpy_fromio, isa_memcpy_toio added,
  *  isa_read[wl] and isa_write[wl] fixed
  *  - Arnaldo Carvalho de Melo <acme@conectiva.com.br>
  */

#define IO_SPACE_LIMIT 0xffff

#define XQUAD_PORTIO_BASE 0xfe400000
#define XQUAD_PORTIO_QUAD 0x40000  /* 256k per quad. */
#define XQUAD_PORTIO_LEN  0x80000  /* Only remapping first 2 quads */

#ifdef __KERNEL__

#include <linux/vmalloc.h>

/*
 * Temporary debugging check to catch old code using
 * unmapped ISA addresses. Will be removed in 2.4.
 */
#if CONFIG_DEBUG_IOVIRT
  extern void *__io_virt_debug(unsigned long x, const char *file, int line);
  extern unsigned long __io_phys_debug(unsigned long x, const char *file, int line);
  #define __io_virt(x) __io_virt_debug((unsigned long)(x), __FILE__, __LINE__)
//#define __io_phys(x) __io_phys_debug((unsigned long)(x), __FILE__, __LINE__)
#else
  #define __io_virt(x) ((void *)(x))
//#define __io_phys(x) __pa(x)
#endif

/**
 *	virt_to_phys	-	map virtual addresses to physical
 *	@address: address to remap
 *
 *	The returned physical address is the physical (CPU) mapping for
 *	the memory address given. It is only valid to use this function on
 *	addresses directly mapped or allocated via kmalloc. 
 *
 *	This function does not give bus mappings for DMA transfers. In
 *	almost all conceivable cases a device driver should not be using
 *	this function
 */
 
static inline unsigned long virt_to_phys(volatile void * address)
{
	return __pa(address);
}

/**
 *	phys_to_virt	-	map physical address to virtual
 *	@address: address to remap
 *
 *	The returned virtual address is a current CPU mapping for
 *	the memory address given. It is only valid to use this function on
 *	addresses that have a kernel mapping
 *
 *	This function does not handle bus mappings for DMA transfers. In
 *	almost all conceivable cases a device driver should not be using
 *	this function
 */

static inline void * phys_to_virt(unsigned long address)
{
	return __va(address);
}

/*
 * Change "struct page" to physical address.
 */
#ifdef CONFIG_HIGHMEM64G
#define page_to_phys(page)	((u64)(page - mem_map) << PAGE_SHIFT)
#else
#define page_to_phys(page)	((page - mem_map) << PAGE_SHIFT)
#endif

extern void * __ioremap(unsigned long offset, unsigned long size, unsigned long flags);

/**
 *	ioremap		-	map bus memory into CPU space
 *	@offset:	bus address of the memory
 *	@size:		size of the resource to map
 *
 *	ioremap performs a platform specific sequence of operations to
 *	make bus memory CPU accessible via the readb/readw/readl/writeb/
 *	writew/writel functions and the other mmio helpers. The returned
 *	address is not guaranteed to be usable directly as a virtual
 *	address. 
 */
 
static inline void * ioremap (unsigned long offset, unsigned long size)
{
	return __ioremap(offset, size, 0);
}

/**
 *	ioremap_nocache		-	map bus memory into CPU space
 *	@offset:	bus address of the memory
 *	@size:		size of the resource to map
 *
 *	ioremap_nocache performs a platform specific sequence of operations to
 *	make bus memory CPU accessible via the readb/readw/readl/writeb/
 *	writew/writel functions and the other mmio helpers. The returned
 *	address is not guaranteed to be usable directly as a virtual
 *	address. 
 *
 *	This version of ioremap ensures that the memory is marked uncachable
 *	on the CPU as well as honouring existing caching rules from things like
 *	the PCI bus. Note that there are other caches and buffers on many 
 *	busses. In paticular driver authors should read up on PCI writes
 *
 *	It's useful if some control registers are in such an area and
 *	write combining or read caching is not desirable:
 */
 
static inline void * ioremap_nocache (unsigned long offset, unsigned long size)
{
        return __ioremap(offset, size, _PAGE_PCD);
}

extern void iounmap(void *addr);

/*
 * bt_ioremap() and bt_iounmap() are for temporary early boot-time
 * mappings, before the real ioremap() is functional.
 * A boot-time mapping is currently limited to at most 16 pages.
 */
extern void *bt_ioremap(unsigned long offset, unsigned long size);
extern void bt_iounmap(void *addr, unsigned long size);

/*
 * IO bus memory addresses are also 1:1 with the physical address
 */
#define virt_to_bus virt_to_phys
#define bus_to_virt phys_to_virt
#define page_to_bus page_to_phys

/*
 * readX/writeX() are used to access memory mapped devices. On some
 * architectures the memory mapped IO stuff needs to be accessed
 * differently. On the x86 architecture, we just read/write the
 * memory location directly.
 */

#define readb(addr) (*(volatile unsigned char *) __io_virt(addr))
#define readw(addr) (*(volatile unsigned short *) __io_virt(addr))
#define readl(addr) (*(volatile unsigned int *) __io_virt(addr))
#define __raw_readb readb
#define __raw_readw readw
#define __raw_readl readl

#define writeb(b,addr) (*(volatile unsigned char *) __io_virt(addr) = (b))
#define writew(b,addr) (*(volatile unsigned short *) __io_virt(addr) = (b))
#define writel(b,addr) (*(volatile unsigned int *) __io_virt(addr) = (b))
#define __raw_writeb writeb
#define __raw_writew writew
#define __raw_writel writel

#define memset_io(a,b,c)	__memset(__io_virt(a),(b),(c))
#define memcpy_fromio(a,b,c)	__memcpy((a),__io_virt(b),(c))
#define memcpy_toio(a,b,c)	__memcpy(__io_virt(a),(b),(c))

/*
 * ISA space is 'always mapped' on a typical x86 system, no need to
 * explicitly ioremap() it. The fact that the ISA IO space is mapped
 * to PAGE_OFFSET is pure coincidence - it does not mean ISA values
 * are physical addresses. The following constant pointer can be
 * used as the IO-area pointer (it can be iounmapped as well, so the
 * analogy with PCI is quite large):
 */
#define __ISA_IO_base ((char *)(PAGE_OFFSET))

#define isa_readb(a) readb(__ISA_IO_base + (a))
#define isa_readw(a) readw(__ISA_IO_base + (a))
#define isa_readl(a) readl(__ISA_IO_base + (a))
#define isa_writeb(b,a) writeb(b,__ISA_IO_base + (a))
#define isa_writew(w,a) writew(w,__ISA_IO_base + (a))
#define isa_writel(l,a) writel(l,__ISA_IO_base + (a))
#define isa_memset_io(a,b,c)		memset_io(__ISA_IO_base + (a),(b),(c))
#define isa_memcpy_fromio(a,b,c)	memcpy_fromio((a),__ISA_IO_base + (b),(c))
#define isa_memcpy_toio(a,b,c)		memcpy_toio(__ISA_IO_base + (a),(b),(c))


/*
 * Again, i386 does not require mem IO specific function.
 */

#define eth_io_copy_and_sum(a,b,c,d)		eth_copy_and_sum((a),__io_virt(b),(c),(d))
#define isa_eth_io_copy_and_sum(a,b,c,d)	eth_copy_and_sum((a),__io_virt(__ISA_IO_base + (b)),(c),(d))

/**
 *	check_signature		-	find BIOS signatures
 *	@io_addr: mmio address to check 
 *	@signature:  signature block
 *	@length: length of signature
 *
 *	Perform a signature comparison with the mmio address io_addr. This
 *	address should have been obtained by ioremap.
 *	Returns 1 on a match.
 */
 
static inline int check_signature(unsigned long io_addr,
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

/**
 *	isa_check_signature		-	find BIOS signatures
 *	@io_addr: mmio address to check 
 *	@signature:  signature block
 *	@length: length of signature
 *
 *	Perform a signature comparison with the ISA mmio address io_addr.
 *	Returns 1 on a match.
 *
 *	This function is deprecated. New drivers should use ioremap and
 *	check_signature.
 */
 

static inline int isa_check_signature(unsigned long io_addr,
	const unsigned char *signature, int length)
{
	int retval = 0;
	do {
		if (isa_readb(io_addr) != *signature)
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
 *	Cache management
 *
 *	This needed for two cases
 *	1. Out of order aware processors
 *	2. Accidentally out of order processors (PPro errata #51)
 */
 
#if defined(CONFIG_X86_OOSTORE) || defined(CONFIG_X86_PPRO_FENCE)

static inline void flush_write_buffers(void)
{
	__asm__ __volatile__ ("lock; addl $0,0(%%esp)": : :"memory");
}

#define dma_cache_inv(_start,_size)		flush_write_buffers()
#define dma_cache_wback(_start,_size)		flush_write_buffers()
#define dma_cache_wback_inv(_start,_size)	flush_write_buffers()

#else

/* Nothing to do */

#define dma_cache_inv(_start,_size)		do { } while (0)
#define dma_cache_wback(_start,_size)		do { } while (0)
#define dma_cache_wback_inv(_start,_size)	do { } while (0)
#define flush_write_buffers()

#endif

#endif /* __KERNEL__ */

#ifdef SLOW_IO_BY_JUMPING
#define __SLOW_DOWN_IO "\njmp 1f\n1:\tjmp 1f\n1:"
#else
#define __SLOW_DOWN_IO "\noutb %%al,$0x80"
#endif

#ifdef REALLY_SLOW_IO
#define __FULL_SLOW_DOWN_IO __SLOW_DOWN_IO __SLOW_DOWN_IO __SLOW_DOWN_IO __SLOW_DOWN_IO
#else
#define __FULL_SLOW_DOWN_IO __SLOW_DOWN_IO
#endif

#ifdef CONFIG_MULTIQUAD
extern void *xquad_portio;    /* Where the IO area was mapped */
#endif /* CONFIG_MULTIQUAD */

/*
 * Talk about misusing macros..
 */
#define __OUT1(s,x) \
static inline void out##s(unsigned x value, unsigned short port) {

#define __OUT2(s,s1,s2) \
__asm__ __volatile__ ("out" #s " %" s1 "0,%" s2 "1"

#if defined (CONFIG_MULTIQUAD) && !defined(STANDALONE)
#define __OUTQ(s,ss,x)    /* Do the equivalent of the portio op on quads */ \
static inline void out##ss(unsigned x value, unsigned short port) { \
	if (xquad_portio) \
		write##s(value, (unsigned long) xquad_portio + port); \
	else               /* We're still in early boot, running on quad 0 */ \
		out##ss##_local(value, port); \
} \
static inline void out##ss##_quad(unsigned x value, unsigned short port, int quad) { \
	if (xquad_portio) \
		write##s(value, (unsigned long) xquad_portio + (XQUAD_PORTIO_QUAD*quad)\
			+ port); \
}

#define __INQ(s,ss)       /* Do the equivalent of the portio op on quads */ \
static inline RETURN_TYPE in##ss(unsigned short port) { \
	if (xquad_portio) \
		return read##s((unsigned long) xquad_portio + port); \
	else               /* We're still in early boot, running on quad 0 */ \
		return in##ss##_local(port); \
} \
static inline RETURN_TYPE in##ss##_quad(unsigned short port, int quad) { \
	if (xquad_portio) \
		return read##s((unsigned long) xquad_portio + (XQUAD_PORTIO_QUAD*quad)\
			+ port); \
	else\
		return 0;\
}
#endif /* CONFIG_MULTIQUAD && !STANDALONE */

#if !defined(CONFIG_MULTIQUAD) || defined(STANDALONE)
#define __OUT(s,s1,x) \
__OUT1(s,x) __OUT2(s,s1,"w") : : "a" (value), "Nd" (port)); } \
__OUT1(s##_p,x) __OUT2(s,s1,"w") __FULL_SLOW_DOWN_IO : : "a" (value), "Nd" (port));} 
#else
/* Make the default portio routines operate on quad 0 */
#define __OUT(s,s1,x) \
__OUT1(s##_local,x) __OUT2(s,s1,"w") : : "a" (value), "Nd" (port)); } \
__OUT1(s##_p_local,x) __OUT2(s,s1,"w") __FULL_SLOW_DOWN_IO : : "a" (value), "Nd" (port));} \
__OUTQ(s,s,x) \
__OUTQ(s,s##_p,x) 
#endif /* !CONFIG_MULTIQUAD || STANDALONE */

#define __IN1(s) \
static inline RETURN_TYPE in##s(unsigned short port) { RETURN_TYPE _v;

#define __IN2(s,s1,s2) \
__asm__ __volatile__ ("in" #s " %" s2 "1,%" s1 "0"

#if !defined(CONFIG_MULTIQUAD) || defined(STANDALONE)
#define __IN(s,s1,i...) \
__IN1(s) __IN2(s,s1,"w") : "=a" (_v) : "Nd" (port) ,##i ); return _v; } \
__IN1(s##_p) __IN2(s,s1,"w") __FULL_SLOW_DOWN_IO : "=a" (_v) : "Nd" (port) ,##i ); return _v; } 
#else
/* Make the default portio routines operate on quad 0 */
#define __IN(s,s1,i...) \
__IN1(s##_local) __IN2(s,s1,"w") : "=a" (_v) : "Nd" (port) ,##i ); return _v; } \
__IN1(s##_p_local) __IN2(s,s1,"w") __FULL_SLOW_DOWN_IO : "=a" (_v) : "Nd" (port) ,##i ); return _v; } \
__INQ(s,s) \
__INQ(s,s##_p) 
#endif /* !CONFIG_MULTIQUAD || STANDALONE */

#define __INS(s) \
static inline void ins##s(unsigned short port, void * addr, unsigned long count) \
{ __asm__ __volatile__ ("rep ; ins" #s \
: "=D" (addr), "=c" (count) : "d" (port),"0" (addr),"1" (count)); }

#define __OUTS(s) \
static inline void outs##s(unsigned short port, const void * addr, unsigned long count) \
{ __asm__ __volatile__ ("rep ; outs" #s \
: "=S" (addr), "=c" (count) : "d" (port),"0" (addr),"1" (count)); }

#define RETURN_TYPE unsigned char
__IN(b,"")
#undef RETURN_TYPE
#define RETURN_TYPE unsigned short
__IN(w,"")
#undef RETURN_TYPE
#define RETURN_TYPE unsigned int
__IN(l,"")
#undef RETURN_TYPE

__OUT(b,"b",char)
__OUT(w,"w",short)
__OUT(l,,int)

__INS(b)
__INS(w)
__INS(l)

__OUTS(b)
__OUTS(w)
__OUTS(l)

#endif
