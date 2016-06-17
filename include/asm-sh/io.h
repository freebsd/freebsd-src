#ifndef __ASM_SH_IO_H
#define __ASM_SH_IO_H

/*
 * Convention:
 *    read{b,w,l}/write{b,w,l} are for PCI,
 *    while in{b,w,l}/out{b,w,l} are for ISA
 * These may (will) be platform specific function.
 * In addition we have 'pausing' versions: in{b,w,l}_p/out{b,w,l}_p
 * and 'string' versions: ins{b,w,l}/outs{b,w,l}
 * For read{b,w,l} and write{b,w,l} there are also __raw versions, which
 * do not have a memory barrier after them.
 *
 * In addition, we have 
 *   ctrl_in{b,w,l}/ctrl_out{b,w,l} for SuperH specific I/O.
 *   which are processor specific.
 */

/*
 * We follow the Alpha convention here:
 *  __inb expands to an inline function call (which either calls via the
 *        mach_vec if generic, or a machine specific implementation)
 *  _inb  is a real function call (note ___raw fns are _ version of __raw)
 *  inb   by default expands to _inb, but the machine specific code may
 *        define it to __inb if it chooses.
 */

#include <asm/cache.h>
#include <asm/system.h>
#include <linux/config.h>

/*
 * Depending on which platform we are running on, we need different
 * I/O functions.
 */

#ifdef __KERNEL__
#if defined(CONFIG_SH_GENERIC) || defined(CONFIG_SH_CQREEK) || defined(CONFIG_SH_UNKNOWN)

/* In a generic kernel, we always go through the machine vector.  */

#include <asm/machvec.h>

# define __inb(p)	sh_mv.mv_inb((p))
# define __inw(p)	sh_mv.mv_inw((p))
# define __inl(p)	sh_mv.mv_inl((p))
# define __outb(x,p)	sh_mv.mv_outb((x),(p))
# define __outw(x,p)	sh_mv.mv_outw((x),(p))
# define __outl(x,p)	sh_mv.mv_outl((x),(p))

# define __inb_p(p)	sh_mv.mv_inb_p((p))
# define __inw_p(p)	sh_mv.mv_inw_p((p))
# define __inl_p(p)	sh_mv.mv_inl_p((p))
# define __outb_p(x,p)	sh_mv.mv_outb_p((x),(p))
# define __outw_p(x,p)	sh_mv.mv_outw_p((x),(p))
# define __outl_p(x,p)	sh_mv.mv_outl_p((x),(p))

#define __insb(p,b,c)	sh_mv.mv_insb((p), (b), (c))
#define __insw(p,b,c)	sh_mv.mv_insw((p), (b), (c))
#define __insl(p,b,c)	sh_mv.mv_insl((p), (b), (c))
#define __outsb(p,b,c)	sh_mv.mv_outsb((p), (b), (c))
#define __outsw(p,b,c)	sh_mv.mv_outsw((p), (b), (c))
#define __outsl(p,b,c)	sh_mv.mv_outsl((p), (b), (c))

# define __readb(a)	sh_mv.mv_readb((a))
# define __readw(a)	sh_mv.mv_readw((a))
# define __readl(a)	sh_mv.mv_readl((a))
# define __writeb(v,a)	sh_mv.mv_writeb((v),(a))
# define __writew(v,a)	sh_mv.mv_writew((v),(a))
# define __writel(v,a)	sh_mv.mv_writel((v),(a))

# define __ioremap(a,s)	sh_mv.mv_ioremap((a), (s))
# define __iounmap(a)	sh_mv.mv_iounmap((a))

# define __isa_port2addr(a)	sh_mv.mv_isa_port2addr(a)

# define inb		__inb
# define inw		__inw
# define inl		__inl
# define outb		__outb
# define outw		__outw
# define outl		__outl

# define inb_p		__inb_p
# define inw_p		__inw_p
# define inl_p		__inl_p
# define outb_p		__outb_p
# define outw_p		__outw_p
# define outl_p		__outl_p

# define insb		__insb
# define insw		__insw
# define insl		__insl
# define outsb		__outsb
# define outsw		__outsw
# define outsl		__outsl

# define __raw_readb	__readb
# define __raw_readw	__readw
# define __raw_readl	__readl
# define __raw_writeb	__writeb
# define __raw_writew	__writew
# define __raw_writel	__writel

#else

/* Control operations through platform specific headers */
# define __WANT_IO_DEF

# if defined(CONFIG_SH_HP600)
#  include <asm/io_hd64461.h>
# elif defined(CONFIG_SH_SOLUTION_ENGINE)
#  include <asm/io_se.h>
# elif defined(CONFIG_SH_SH2000)
#  include <asm/io_sh2000.h>
# elif defined(CONFIG_SH_DMIDA) || \
       defined(CONFIG_SH_STB1_HARP) || \
       defined(CONFIG_SH_STB1_OVERDRIVE)
#  include <asm/io_hd64465.h>
# elif defined(CONFIG_SH_EC3104)
#  include <asm/io_ec3104.h>
# elif defined(CONFIG_SH_DREAMCAST)
#  include <asm/io_dc.h>
# elif defined(CONFIG_SH_CAT68701)
#  include <asm/io_cat68701.h>
# elif defined(CONFIG_SH_BIGSUR)
#  include <asm/io_bigsur.h>
# elif defined(CONFIG_SH_HS7729PCI)
#  include <asm/io_hs7729pci.h>
# elif defined(CONFIG_SH_7751_SOLUTION_ENGINE)
#  include <asm/io_7751se.h>
# elif defined(CONFIG_SH_MOBILE_SOLUTION_ENGINE)
#  include <asm/io_shmse.h>
# elif defined(CONFIG_SH_ADX)
#  include <asm/io_adx.h>
# elif defined(CONFIG_SH_SECUREEDGE5410)
#  include <asm/io_snapgear.h>
# elif defined(CONFIG_SH_SH4202_MICRODEV)
#  include <asm/io_microdev.h>
# elif defined(CONFIG_SH_UNKNOWN)
#  include <asm/io_unknown.h>
# else
#  error "What system is this?"
#endif

#undef __WANT_IO_DEF

#endif /* GENERIC */
#endif /* __KERNEL__ */

/* These are always function calls, in both kernel and user space */
extern unsigned char	_inb (unsigned long port);
extern unsigned short	_inw (unsigned long port);
extern unsigned int	_inl (unsigned long port);
extern void		_outb (unsigned char b, unsigned long port);
extern void		_outw (unsigned short w, unsigned long port);
extern void		_outl (unsigned int l, unsigned long port);
extern unsigned char	_inb_p (unsigned long port);
extern unsigned short	_inw_p (unsigned long port);
extern unsigned int	_inl_p (unsigned long port);
extern void		_outb_p (unsigned char b, unsigned long port);
extern void		_outw_p (unsigned short w, unsigned long port);
extern void		_outl_p (unsigned int l, unsigned long port);
extern void		_insb (unsigned long port, void *dst, unsigned long count);
extern void		_insw (unsigned long port, void *dst, unsigned long count);
extern void		_insl (unsigned long port, void *dst, unsigned long count);
extern void		_outsb (unsigned long port, const void *src, unsigned long count);
extern void		_outsw (unsigned long port, const void *src, unsigned long count);
extern void		_outsl (unsigned long port, const void *src, unsigned long count);
extern unsigned char	_readb(unsigned long addr);
extern unsigned short	_readw(unsigned long addr);
extern unsigned int	_readl(unsigned long addr);
extern void		_writeb(unsigned char b, unsigned long addr);
extern void		_writew(unsigned short b, unsigned long addr);
extern void		_writel(unsigned int b, unsigned long addr);

#ifdef __KERNEL__
extern unsigned char	___raw_readb(unsigned long addr);
extern unsigned short	___raw_readw(unsigned long addr);
extern unsigned int	___raw_readl(unsigned long addr);
extern void		___raw_writeb(unsigned char b, unsigned long addr);
extern void		___raw_writew(unsigned short b, unsigned long addr);
extern void		___raw_writel(unsigned int b, unsigned long addr);
#endif

#ifdef __KERNEL__
/*
 * The platform header files may define some of these macros to use
 * the inlined versions where appropriate.  These macros may also be
 * redefined by userlevel programs.
 */
#ifndef inb
# define inb(p)		_inb(p)
#endif
#ifndef inw
# define inw(p)		_inw(p)
#endif
#ifndef inl
# define inl(p)		_inl(p)
#endif

#ifndef outb
# define outb(b,p)	_outb((b),(p))
#endif
#ifndef outw
# define outw(w,p)	_outw((w),(p))
#endif
#ifndef outl
# define outl(l,p)	_outl((l),(p))
#endif

#ifndef inb_p
# define inb_p		_inb_p
#endif
#ifndef inw_p
# define inw_p		_inw_p
#endif
#ifndef inl_p
# define inl_p		_inl_p
#endif

#ifndef outb_p
# define outb_p		_outb_p
#endif
#ifndef outw_p
# define outw_p		_outw_p
#endif
#ifndef outl_p
# define outl_p		_outl_p
#endif

#ifndef insb
# define insb(p,d,c)	_insb((p),(d),(c))
#endif
#ifndef insw
# define insw(p,d,c)	_insw((p),(d),(c))
#endif
#ifndef insl
# define insl(p,d,c)	_insl((p),(d),(c))
#endif
#ifndef outsb
# define outsb(p,s,c)	_outsb((p),(s),(c))
#endif
#ifndef outsw
# define outsw(p,s,c)	_outsw((p),(s),(c))
#endif
#ifndef outsl
# define outsl(p,s,c)	_outsl((p),(s),(c))
#endif

#ifdef __raw_readb
# define readb(a)	({ unsigned long r_ = __raw_readb(a); mb(); r_; })
#endif
#ifdef __raw_readw
# define readw(a)	({ unsigned long r_ = __raw_readw(a); mb(); r_; })
#endif
#ifdef __raw_readl
# define readl(a)	({ unsigned long r_ = __raw_readl(a); mb(); r_; })
#endif

#ifdef __raw_writeb
# define writeb(v,a)	({ __raw_writeb((v),(a)); mb(); })
#endif
#ifdef __raw_writew
# define writew(v,a)	({ __raw_writew((v),(a)); mb(); })
#endif
#ifdef __raw_writel
# define writel(v,a)	({ __raw_writel((v),(a)); mb(); })
#endif

#ifndef __raw_readb
# define __raw_readb(a)	___raw_readb((unsigned long)(a))
#endif
#ifndef __raw_readw
# define __raw_readw(a)	___raw_readw((unsigned long)(a))
#endif
#ifndef __raw_readl
# define __raw_readl(a)	___raw_readl((unsigned long)(a))
#endif

#ifndef __raw_writeb
# define __raw_writeb(v,a)  ___raw_writeb((v),(unsigned long)(a))
#endif
#ifndef __raw_writew
# define __raw_writew(v,a)  ___raw_writew((v),(unsigned long)(a))
#endif
#ifndef __raw_writel
# define __raw_writel(v,a)  ___raw_writel((v),(unsigned long)(a))
#endif

#ifndef readb
# define readb(a)	_readb((unsigned long)(a))
#endif
#ifndef readw
# define readw(a)	_readw((unsigned long)(a))
#endif
#ifndef readl
# define readl(a)	_readl((unsigned long)(a))
#endif

#ifndef writeb
# define writeb(v,a)	_writeb((v),(unsigned long)(a))
#endif
#ifndef writew
# define writew(v,a)	_writew((v),(unsigned long)(a))
#endif
#ifndef writel
# define writel(v,a)	_writel((v),(unsigned long)(a))
#endif

#else 

/* Userspace declarations.  */

extern unsigned char	inb(unsigned long port);
extern unsigned short	inw(unsigned long port);
extern unsigned int	inl(unsigned long port);
extern void		outb(unsigned char b, unsigned long port);
extern void		outw(unsigned short w, unsigned long port);
extern void		outl(unsigned int l, unsigned long port);
extern void		insb(unsigned long port, void *dst, unsigned long count);
extern void		insw(unsigned long port, void *dst, unsigned long count);
extern void		insl(unsigned long port, void *dst, unsigned long count);
extern void		outsb(unsigned long port, const void *src, unsigned long count);
extern void		outsw(unsigned long port, const void *src, unsigned long count);
extern void		outsl(unsigned long port, const void *src, unsigned long count);
extern unsigned char	readb(unsigned long addr);
extern unsigned short	readw(unsigned long addr);
extern unsigned long	readl(unsigned long addr);
extern void		writeb(unsigned char b, unsigned long addr);
extern void		writew(unsigned short b, unsigned long addr);
extern void		writel(unsigned int b, unsigned long addr);

#endif /* __KERNEL__ */

#ifdef __KERNEL__

/*
 * If the platform has PC-like I/O, this function converts the offset into
 * an address.
 */
static __inline__ unsigned long isa_port2addr(unsigned long offset)
{
	return __isa_port2addr(offset);
}

#define isa_readb(a) readb(isa_port2addr(a))
#define isa_readw(a) readw(isa_port2addr(a))
#define isa_readl(a) readl(isa_port2addr(a))
#define isa_writeb(b,a) writeb(b,isa_port2addr(a))
#define isa_writew(w,a) writew(w,isa_port2addr(a))
#define isa_writel(l,a) writel(l,isa_port2addr(a))
#define isa_memset_io(a,b,c) \
  memset((void *)(isa_port2addr((unsigned long)a)),(b),(c))
#define isa_memcpy_fromio(a,b,c) \
  memcpy((a),(void *)(isa_port2addr((unsigned long)(b))),(c))
#define isa_memcpy_toio(a,b,c) \
  memcpy((void *)(isa_port2addr((unsigned long)(a))),(b),(c))

/* We really want to try and get these to memcpy etc */
extern void memcpy_fromio(void *, unsigned long, unsigned long);
extern void memcpy_toio(unsigned long, const void *, unsigned long);
extern void memset_io(unsigned long, int, unsigned long);

/* SuperH on-chip I/O functions */
static __inline__ unsigned char ctrl_inb(unsigned long addr)
{
	return *(volatile unsigned char*)addr;
}

static __inline__ unsigned short ctrl_inw(unsigned long addr)
{
	return *(volatile unsigned short*)addr;
}

static __inline__ unsigned int ctrl_inl(unsigned long addr)
{
	return *(volatile unsigned long*)addr;
}

static __inline__ void ctrl_outb(unsigned char b, unsigned long addr)
{
	*(volatile unsigned char*)addr = b;
}

static __inline__ void ctrl_outw(unsigned short b, unsigned long addr)
{
	*(volatile unsigned short*)addr = b;
}

static __inline__ void ctrl_outl(unsigned int b, unsigned long addr)
{
        *(volatile unsigned long*)addr = b;
}

#define IO_SPACE_LIMIT 0xffffffff

#include <asm/addrspace.h>

/*
 * Change virtual addresses to physical addresses and vv.
 * These are trivial on the 1:1 Linux/SuperH mapping
 */
static __inline__ unsigned long virt_to_phys(volatile void * address)
{
	return PHYSADDR(address);
}

static __inline__ void * phys_to_virt(unsigned long address)
{
	return (void *)P1SEGADDR(address);
}

#define virt_to_bus virt_to_phys
#define bus_to_virt phys_to_virt
#define page_to_bus page_to_phys

/*
 * readX/writeX() are used to access memory mapped devices. On some
 * architectures the memory mapped IO stuff needs to be accessed
 * differently. On the x86 architecture, we just read/write the
 * memory location directly.
 *
 * On SH, we have the whole physical address space mapped at all times
 * (as MIPS does), so "ioremap()" and "iounmap()" do not need to do
 * anything.  (This isn't true for all machines but we still handle
 * these cases with wired TLB entries anyway ...)
 *
 * We cheat a bit and always return uncachable areas until we've fixed
 * the drivers to handle caching properly.  
 */
static __inline__ void * ioremap(unsigned long offset, unsigned long size)
{
	return __ioremap(offset, size);
}

static __inline__ void iounmap(void *addr)
{
	return __iounmap(addr);
}

#define ioremap_nocache(off,size) ioremap(off,size)

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

#define dma_cache_wback_inv(_start,_size) \
    __flush_purge_region(_start,_size)
#define dma_cache_inv(_start,_size) \
    __flush_invalidate_region(_start,_size)
#define dma_cache_wback(_start,_size) \
    __flush_wback_region(_start,_size)

#endif /* __KERNEL__ */
#endif /* __ASM_SH_IO_H */
