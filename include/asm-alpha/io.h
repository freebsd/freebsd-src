#ifndef __ALPHA_IO_H
#define __ALPHA_IO_H

/* We don't use IO slowdowns on the Alpha, but.. */
#define __SLOW_DOWN_IO	do { } while (0)
#define SLOW_DOWN_IO	do { } while (0)

/*
 * Virtual -> physical identity mapping starts at this offset
 */
#ifdef USE_48_BIT_KSEG
#define IDENT_ADDR     0xffff800000000000
#else
#define IDENT_ADDR     0xfffffc0000000000
#endif

#ifdef __KERNEL__
#include <linux/config.h>
#include <linux/kernel.h>
#include <asm/system.h>
#include <asm/machvec.h>
#include <asm/hwrpb.h>

/*
 * We try to avoid hae updates (thus the cache), but when we
 * do need to update the hae, we need to do it atomically, so
 * that any interrupts wouldn't get confused with the hae
 * register not being up-to-date with respect to the hardware
 * value.
 */
static inline void __set_hae(unsigned long new_hae)
{
	unsigned long flags;
	__save_and_cli(flags);

	alpha_mv.hae_cache = new_hae;
	*alpha_mv.hae_register = new_hae;
	mb();
	/* Re-read to make sure it was written.  */
	new_hae = *alpha_mv.hae_register;

	__restore_flags(flags);
}

static inline void set_hae(unsigned long new_hae)
{
	if (new_hae != alpha_mv.hae_cache)
		__set_hae(new_hae);
}

/*
 * Change virtual addresses to physical addresses and vv.
 */
#ifdef USE_48_BIT_KSEG
static inline unsigned long virt_to_phys(void *address)
{
	return (unsigned long)address - IDENT_ADDR;
}

static inline void * phys_to_virt(unsigned long address)
{
	return (void *) (address + IDENT_ADDR);
}
#else
static inline unsigned long virt_to_phys(void *address)
{
        unsigned long phys = (unsigned long)address;

	/* Sign-extend from bit 41.  */
	phys <<= (64 - 41);
	phys = (long)phys >> (64 - 41);

	/* Crop to the physical address width of the processor.  */
        phys &= (1ul << hwrpb->pa_bits) - 1;

        return phys;
}

static inline void * phys_to_virt(unsigned long address)
{
        return (void *)(IDENT_ADDR + (address & ((1ul << 41) - 1)));
}
#endif

#define page_to_phys(page)	PAGE_TO_PA(page)

/*
 * Change addresses as seen by the kernel (virtual) to addresses as
 * seen by a device (bus), and vice versa.
 *
 * Note that this only works for a limited range of kernel addresses,
 * and very well may not span all memory.  Consider this interface 
 * deprecated in favour of the mapping functions in <asm/pci.h>.
 */
extern unsigned long __direct_map_base;
extern unsigned long __direct_map_size;

static inline unsigned long virt_to_bus(void *address)
{
	unsigned long phys = virt_to_phys(address);
	unsigned long bus = phys + __direct_map_base;
	return phys <= __direct_map_size ? bus : 0;
}

static inline void *bus_to_virt(unsigned long address)
{
	void *virt;

	/* This check is a sanity check but also ensures that bus address 0
	   maps to virtual address 0 which is useful to detect null pointers
	   (the NCR driver is much simpler if NULL pointers are preserved).  */
	address -= __direct_map_base;
	virt = phys_to_virt(address);
	return (long)address <= 0 ? NULL : virt;
}

#else /* !__KERNEL__ */

/*
 * Define actual functions in private name-space so it's easier to
 * accommodate things like XFree or svgalib that like to define their
 * own versions of inb etc.
 */
extern void __sethae (unsigned long addr);	/* syscall */
extern void _sethae (unsigned long addr);	/* cached version */

#endif /* !__KERNEL__ */

/*
 * There are different chipsets to interface the Alpha CPUs to the world.
 */

#ifdef __KERNEL__
#ifdef CONFIG_ALPHA_GENERIC

/* In a generic kernel, we always go through the machine vector.  */

# define __inb(p)	alpha_mv.mv_inb((unsigned long)(p))
# define __inw(p)	alpha_mv.mv_inw((unsigned long)(p))
# define __inl(p)	alpha_mv.mv_inl((unsigned long)(p))
# define __outb(x,p)	alpha_mv.mv_outb((x),(unsigned long)(p))
# define __outw(x,p)	alpha_mv.mv_outw((x),(unsigned long)(p))
# define __outl(x,p)	alpha_mv.mv_outl((x),(unsigned long)(p))

# define __readb(a)	alpha_mv.mv_readb((unsigned long)(a))
# define __readw(a)	alpha_mv.mv_readw((unsigned long)(a))
# define __readl(a)	alpha_mv.mv_readl((unsigned long)(a))
# define __readq(a)	alpha_mv.mv_readq((unsigned long)(a))
# define __writeb(v,a)	alpha_mv.mv_writeb((v),(unsigned long)(a))
# define __writew(v,a)	alpha_mv.mv_writew((v),(unsigned long)(a))
# define __writel(v,a)	alpha_mv.mv_writel((v),(unsigned long)(a))
# define __writeq(v,a)	alpha_mv.mv_writeq((v),(unsigned long)(a))

# define __ioremap(a,s)	alpha_mv.mv_ioremap((unsigned long)(a),(s))
# define __iounmap(a)   alpha_mv.mv_iounmap((unsigned long)(a))
# define __is_ioaddr(a)	alpha_mv.mv_is_ioaddr((unsigned long)(a))

# define inb		__inb
# define inw		__inw
# define inl		__inl
# define outb		__outb
# define outw		__outw
# define outl		__outl

# define __raw_readb	__readb
# define __raw_readw	__readw
# define __raw_readl	__readl
# define __raw_readq	__readq
# define __raw_writeb	__writeb
# define __raw_writew	__writew
# define __raw_writel	__writel
# define __raw_writeq	__writeq

#else

/* Control how and what gets defined within the core logic headers.  */
#define __WANT_IO_DEF

#if defined(CONFIG_ALPHA_APECS)
# include <asm/core_apecs.h>
#elif defined(CONFIG_ALPHA_CIA)
# include <asm/core_cia.h>
#elif defined(CONFIG_ALPHA_IRONGATE)
# include <asm/core_irongate.h>
#elif defined(CONFIG_ALPHA_JENSEN)
# include <asm/jensen.h>
#elif defined(CONFIG_ALPHA_LCA)
# include <asm/core_lca.h>
#elif defined(CONFIG_ALPHA_MARVEL)
# include <asm/core_marvel.h>
#elif defined(CONFIG_ALPHA_MCPCIA)
# include <asm/core_mcpcia.h>
#elif defined(CONFIG_ALPHA_POLARIS)
# include <asm/core_polaris.h>
#elif defined(CONFIG_ALPHA_T2)
# include <asm/core_t2.h>
#elif defined(CONFIG_ALPHA_TSUNAMI)
# include <asm/core_tsunami.h>
#elif defined(CONFIG_ALPHA_TITAN)
# include <asm/core_titan.h>
#elif defined(CONFIG_ALPHA_WILDFIRE)
# include <asm/core_wildfire.h>
#else
#error "What system is this?"
#endif

#undef __WANT_IO_DEF

#endif /* GENERIC */
#endif /* __KERNEL__ */

/*
 * The convention used for inb/outb etc. is that names starting with
 * two underscores are the inline versions, names starting with a
 * single underscore are proper functions, and names starting with a
 * letter are macros that map in some way to inline or proper function
 * versions.  Not all that pretty, but before you change it, be sure
 * to convince yourself that it won't break anything (in particular
 * module support).
 */
extern u8		_inb (unsigned long port);
extern u16		_inw (unsigned long port);
extern u32		_inl (unsigned long port);
extern void		_outb (u8 b,unsigned long port);
extern void		_outw (u16 w,unsigned long port);
extern void		_outl (u32 l,unsigned long port);
extern u8		_readb(unsigned long addr);
extern u16		_readw(unsigned long addr);
extern u32		_readl(unsigned long addr);
extern u64		_readq(unsigned long addr);
extern void		_writeb(u8 b, unsigned long addr);
extern void		_writew(u16 b, unsigned long addr);
extern void		_writel(u32 b, unsigned long addr);
extern void		_writeq(u64 b, unsigned long addr);

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
# define inb_p		inb
#endif
#ifndef inw_p
# define inw_p		inw
#endif
#ifndef inl_p
# define inl_p		inl
#endif

#ifndef outb_p
# define outb_p		outb
#endif
#ifndef outw_p
# define outw_p		outw
#endif
#ifndef outl_p
# define outl_p		outl
#endif

#define IO_SPACE_LIMIT 0xffff

#else 

/* Userspace declarations.  Kill in 2.5. */

extern unsigned int	inb(unsigned long port);
extern unsigned int	inw(unsigned long port);
extern unsigned int	inl(unsigned long port);
extern void		outb(unsigned char b,unsigned long port);
extern void		outw(unsigned short w,unsigned long port);
extern void		outl(unsigned int l,unsigned long port);
extern unsigned long	readb(unsigned long addr);
extern unsigned long	readw(unsigned long addr);
extern unsigned long	readl(unsigned long addr);
extern void		writeb(unsigned char b, unsigned long addr);
extern void		writew(unsigned short b, unsigned long addr);
extern void		writel(unsigned int b, unsigned long addr);

#endif /* __KERNEL__ */

#ifdef __KERNEL__

/*
 * On Alpha, we have the whole of I/O space mapped at all times, but
 * at odd and sometimes discontinuous addresses.  Note that the 
 * discontinuities are all across busses, so we need not care for that
 * for any one device.
 *
 * The DRM drivers need to be able to map contiguously a (potentially)
 * discontiguous set of I/O pages. This set of pages is scatter-gather
 * mapped contiguously from the perspective of the bus, but we can't
 * directly access DMA addresses from the CPU, these addresses need to
 * have a real ioremap. Therefore, iounmap and the size argument to
 * ioremap are needed to give the platforms the ability to fully implement
 * ioremap.
 *
 * Map the I/O space address into the kernel's virtual address space.
 */
static inline void * ioremap(unsigned long offset, unsigned long size)
{
	return (void *) __ioremap(offset, size);
} 

static inline void iounmap(void *addr)
{
	__iounmap(addr);
}

static inline void * ioremap_nocache(unsigned long offset, unsigned long size)
{
	return ioremap(offset, size);
} 

/* Indirect back to the macros provided.  */

extern u8		___raw_readb(unsigned long addr);
extern u16		___raw_readw(unsigned long addr);
extern u32		___raw_readl(unsigned long addr);
extern u64		___raw_readq(unsigned long addr);
extern void		___raw_writeb(u8 b, unsigned long addr);
extern void		___raw_writew(u16 b, unsigned long addr);
extern void		___raw_writel(u32 b, unsigned long addr);
extern void		___raw_writeq(u64 b, unsigned long addr);

#ifdef __raw_readb
# define readb(a)	({ u8 r_ = __raw_readb(a); mb(); r_; })
#endif
#ifdef __raw_readw
# define readw(a)	({ u16 r_ = __raw_readw(a); mb(); r_; })
#endif
#ifdef __raw_readl
# define readl(a)	({ u32 r_ = __raw_readl(a); mb(); r_; })
#endif
#ifdef __raw_readq
# define readq(a)	({ u64 r_ = __raw_readq(a); mb(); r_; })
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
#ifdef __raw_writeq
# define writeq(v,a)	({ __raw_writeq((v),(a)); mb(); })
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
#ifndef __raw_readq
# define __raw_readq(a)	___raw_readq((unsigned long)(a))
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
#ifndef __raw_writeq
# define __raw_writeq(v,a)  ___raw_writeq((v),(unsigned long)(a))
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
#ifndef readq
# define readq(a)	_readq((unsigned long)(a))
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
#ifndef writeq
# define writeq(v,a)	_writeq((v),(unsigned long)(a))
#endif

/*
 * String version of IO memory access ops:
 */
extern void _memcpy_fromio(void *, unsigned long, long);
extern void _memcpy_toio(unsigned long, const void *, long);
extern void _memset_c_io(unsigned long, unsigned long, long);

#define memcpy_fromio(to,from,len) \
  _memcpy_fromio((to),(unsigned long)(from),(len))
#define memcpy_toio(to,from,len) \
  _memcpy_toio((unsigned long)(to),(from),(len))
#define memset_io(addr,c,len) \
  _memset_c_io((unsigned long)(addr),0x0101010101010101UL*(u8)(c),(len))

#define __HAVE_ARCH_MEMSETW_IO
#define memsetw_io(addr,c,len) \
  _memset_c_io((unsigned long)(addr),0x0001000100010001UL*(u16)(c),(len))

/*
 * String versions of in/out ops:
 */
extern void insb (unsigned long port, void *dst, unsigned long count);
extern void insw (unsigned long port, void *dst, unsigned long count);
extern void insl (unsigned long port, void *dst, unsigned long count);
extern void outsb (unsigned long port, const void *src, unsigned long count);
extern void outsw (unsigned long port, const void *src, unsigned long count);
extern void outsl (unsigned long port, const void *src, unsigned long count);

/*
 * XXX - We don't have csum_partial_copy_fromio() yet, so we cheat here and 
 * just copy it. The net code will then do the checksum later. Presently 
 * only used by some shared memory 8390 Ethernet cards anyway.
 */

#define eth_io_copy_and_sum(skb,src,len,unused) \
  memcpy_fromio((skb)->data,(src),(len))
#define isa_eth_io_copy_and_sum(skb,src,len,unused) \
  isa_memcpy_fromio((skb)->data,(src),(len))
 
static inline int
check_signature(unsigned long io_addr, const unsigned char *signature,
		int length)
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
 * ISA space is mapped to some machine-specific location on Alpha.
 * Call into the existing hooks to get the address translated.
 */
#define isa_readb(a)			readb(__ioremap((a),1))
#define isa_readw(a)			readw(__ioremap((a),2))
#define isa_readl(a)			readl(__ioremap((a),4))
#define isa_writeb(b,a)			writeb((b),__ioremap((a),1))
#define isa_writew(w,a)			writew((w),__ioremap((a),2))
#define isa_writel(l,a)			writel((l),__ioremap((a),4))
#define isa_memset_io(a,b,c)		memset_io(__ioremap((a),(c)),(b),(c))
#define isa_memcpy_fromio(a,b,c)	memcpy_fromio((a),__ioremap((b),(c)),(c))
#define isa_memcpy_toio(a,b,c)		memcpy_toio(__ioremap((a),(c)),(b),(c))

static inline int
isa_check_signature(unsigned long io_addr, const unsigned char *signature,
		int length)
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
 * The Alpha Jensen hardware for some rather strange reason puts
 * the RTC clock at 0x170 instead of 0x70. Probably due to some
 * misguided idea about using 0x70 for NMI stuff.
 *
 * These defines will override the defaults when doing RTC queries
 */

#ifdef CONFIG_ALPHA_GENERIC
# define RTC_PORT(x)	((x) + alpha_mv.rtc_port)
#else
# ifdef CONFIG_ALPHA_JENSEN
#  define RTC_PORT(x)	(0x170+(x))
# else
#  define RTC_PORT(x)	(0x70 + (x))
# endif
#endif
#define RTC_ALWAYS_BCD	0

/* Nothing to do */

#define dma_cache_inv(_start,_size)		do { } while (0)
#define dma_cache_wback(_start,_size)		do { } while (0)
#define dma_cache_wback_inv(_start,_size)	do { } while (0)

#endif /* __KERNEL__ */

#endif /* __ALPHA_IO_H */
