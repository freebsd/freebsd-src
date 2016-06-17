#ifndef __ALPHA_POLARIS__H__
#define __ALPHA_POLARIS__H__

#include <linux/types.h>
#include <asm/compiler.h>

/*
 * POLARIS is the internal name for a core logic chipset which provides
 * memory controller and PCI access for the 21164PC chip based systems.
 *
 * This file is based on:
 *
 * Polaris System Controller
 * Device Functional Specification
 * 22-Jan-98
 * Rev. 4.2
 *
 */

/* Polaris memory regions */
#define POLARIS_SPARSE_MEM_BASE		(IDENT_ADDR + 0xf800000000)
#define POLARIS_DENSE_MEM_BASE		(IDENT_ADDR + 0xf900000000)
#define POLARIS_SPARSE_IO_BASE		(IDENT_ADDR + 0xf980000000)
#define POLARIS_SPARSE_CONFIG_BASE	(IDENT_ADDR + 0xf9c0000000)
#define POLARIS_IACK_BASE		(IDENT_ADDR + 0xf9f8000000)
#define POLARIS_DENSE_IO_BASE		(IDENT_ADDR + 0xf9fc000000)
#define POLARIS_DENSE_CONFIG_BASE	(IDENT_ADDR + 0xf9fe000000)

#define POLARIS_IACK_SC			POLARIS_IACK_BASE

/* The Polaris command/status registers live in PCI Config space for
 * bus 0/device 0.  As such, they may be bytes, words, or doublewords.
 */
#define POLARIS_W_VENID		(POLARIS_DENSE_CONFIG_BASE)
#define POLARIS_W_DEVID		(POLARIS_DENSE_CONFIG_BASE+2)
#define POLARIS_W_CMD		(POLARIS_DENSE_CONFIG_BASE+4)
#define POLARIS_W_STATUS	(POLARIS_DENSE_CONFIG_BASE+6)

/*
 * Data structure for handling POLARIS machine checks:
 */
struct el_POLARIS_sysdata_mcheck {
    u_long      psc_status;
    u_long	psc_pcictl0;
    u_long	psc_pcictl1;
    u_long	psc_pcictl2;
};

#ifdef __KERNEL__

#ifndef __EXTERN_INLINE
#define __EXTERN_INLINE extern inline
#define __IO_EXTERN_INLINE
#endif

/*
 * I/O functions:
 *
 * POLARIS, the PCI/memory support chipset for the PCA56 (21164PC)
 * processors, can use either a sparse address  mapping scheme, or the 
 * so-called byte-word PCI address space, to get at PCI memory and I/O.
 *
 * However, we will support only the BWX form.
 */

#define vucp	volatile unsigned char *
#define vusp	volatile unsigned short *
#define vuip	volatile unsigned int  *
#define vulp	volatile unsigned long  *

__EXTERN_INLINE u8 polaris_inb(unsigned long addr)
{
	/* ??? I wish I could get rid of this.  But there's no ioremap
	   equivalent for I/O space.  PCI I/O can be forced into the
	   POLARIS I/O region, but that doesn't take care of legacy
	   ISA crap.  */

	return __kernel_ldbu(*(vucp)(addr + POLARIS_DENSE_IO_BASE));
}

__EXTERN_INLINE void polaris_outb(u8 b, unsigned long addr)
{
	__kernel_stb(b, *(vucp)(addr + POLARIS_DENSE_IO_BASE));
	mb();
}

__EXTERN_INLINE u16 polaris_inw(unsigned long addr)
{
	return __kernel_ldwu(*(vusp)(addr + POLARIS_DENSE_IO_BASE));
}

__EXTERN_INLINE void polaris_outw(u16 b, unsigned long addr)
{
	__kernel_stw(b, *(vusp)(addr + POLARIS_DENSE_IO_BASE));
	mb();
}

__EXTERN_INLINE u32 polaris_inl(unsigned long addr)
{
	return *(vuip)(addr + POLARIS_DENSE_IO_BASE);
}

__EXTERN_INLINE void polaris_outl(u32 b, unsigned long addr)
{
	*(vuip)(addr + POLARIS_DENSE_IO_BASE) = b;
	mb();
}

/*
 * Memory functions.  Polaris allows all accesses (byte/word
 * as well as long/quad) to be done through dense space.
 *
 * We will only support DENSE access via BWX insns.
 */

__EXTERN_INLINE u8 polaris_readb(unsigned long addr)
{
	return __kernel_ldbu(*(vucp)addr);
}

__EXTERN_INLINE u16 polaris_readw(unsigned long addr)
{
	return __kernel_ldwu(*(vusp)addr);
}

__EXTERN_INLINE u32 polaris_readl(unsigned long addr)
{
	return (*(vuip)addr) & 0xffffffff;
}

__EXTERN_INLINE u64 polaris_readq(unsigned long addr)
{
	return *(vulp)addr;
}

__EXTERN_INLINE void polaris_writeb(u8 b, unsigned long addr)
{
	__kernel_stb(b, *(vucp)addr);
}

__EXTERN_INLINE void polaris_writew(u16 b, unsigned long addr)
{
	__kernel_stw(b, *(vusp)addr);
}

__EXTERN_INLINE void polaris_writel(u32 b, unsigned long addr)
{
	*(vuip)addr = b;
}

__EXTERN_INLINE void polaris_writeq(u64 b, unsigned long addr)
{
	*(vulp)addr = b;
}

__EXTERN_INLINE unsigned long polaris_ioremap(unsigned long addr,
					      unsigned long size
					      __attribute__((unused)))
{
	return addr + POLARIS_DENSE_MEM_BASE;
}

__EXTERN_INLINE void polaris_iounmap(unsigned long addr)
{
	return;
}

__EXTERN_INLINE int polaris_is_ioaddr(unsigned long addr)
{
	return addr >= POLARIS_SPARSE_MEM_BASE;
}

#undef vucp
#undef vusp
#undef vuip
#undef vulp

#ifdef __WANT_IO_DEF

#define __inb(p)		polaris_inb((unsigned long)(p))
#define __inw(p)		polaris_inw((unsigned long)(p))
#define __inl(p)		polaris_inl((unsigned long)(p))
#define __outb(x,p)		polaris_outb((x),(unsigned long)(p))
#define __outw(x,p)		polaris_outw((x),(unsigned long)(p))
#define __outl(x,p)		polaris_outl((x),(unsigned long)(p))
#define __readb(a)		polaris_readb((unsigned long)(a))
#define __readw(a)		polaris_readw((unsigned long)(a))
#define __readl(a)		polaris_readl((unsigned long)(a))
#define __readq(a)		polaris_readq((unsigned long)(a))
#define __writeb(x,a)		polaris_writeb((x),(unsigned long)(a))
#define __writew(x,a)		polaris_writew((x),(unsigned long)(a))
#define __writel(x,a)		polaris_writel((x),(unsigned long)(a))
#define __writeq(x,a)		polaris_writeq((x),(unsigned long)(a))
#define __ioremap(a,s)		polaris_ioremap((unsigned long)(a),(s))
#define __iounmap(a)		polaris_iounmap((unsigned long)(a))
#define __is_ioaddr(a)		polaris_is_ioaddr((unsigned long)(a))

#define inb(p)			__inb(p)
#define inw(p)			__inw(p)
#define inl(p)			__inl(p)
#define outb(x,p)		__outb((x),(p))
#define outw(x,p)		__outw((x),(p))
#define outl(x,p)		__outl((x),(p))
#define __raw_readb(a)		__readb(a)
#define __raw_readw(a)		__readw(a)
#define __raw_readl(a)		__readl(a)
#define __raw_readq(a)		__readq(a)
#define __raw_writeb(v,a)	__writeb((v),(a))
#define __raw_writew(v,a)	__writew((v),(a))
#define __raw_writel(v,a)	__writel((v),(a))
#define __raw_writeq(v,a)	__writeq((v),(a))

#endif /* __WANT_IO_DEF */

#ifdef __IO_EXTERN_INLINE
#undef __EXTERN_INLINE
#undef __IO_EXTERN_INLINE
#endif

#endif /* __KERNEL__ */

#endif /* __ALPHA_POLARIS__H__ */
