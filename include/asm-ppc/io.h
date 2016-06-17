#ifdef __KERNEL__
#ifndef _PPC_IO_H
#define _PPC_IO_H

#include <linux/config.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <asm/mmu.h>
#include <asm/page.h>
#include <asm/byteorder.h>

#define SIO_CONFIG_RA	0x398
#define SIO_CONFIG_RD	0x399

#define SLOW_DOWN_IO

#define PMAC_ISA_MEM_BASE 	0
#define PMAC_PCI_DRAM_OFFSET 	0
#define CHRP_ISA_IO_BASE 	0xf8000000
#define CHRP_ISA_MEM_BASE 	0xf7000000
#define CHRP_PCI_DRAM_OFFSET 	0
#define PREP_ISA_IO_BASE 	0x80000000
#define PREP_ISA_MEM_BASE 	0xc0000000
#define PREP_PCI_DRAM_OFFSET 	0x80000000

#if defined(CONFIG_40x)
#include <asm/ibm4xx.h>
#elif defined(CONFIG_8xx)
#include <asm/mpc8xx.h>
#elif defined(CONFIG_8260)
#include <asm/mpc8260.h>
#elif defined(CONFIG_APUS)
#define _IO_BASE	0
#define _ISA_MEM_BASE	0
#define PCI_DRAM_OFFSET 0
#else /* Everyone else */
#define _IO_BASE	isa_io_base
#define _ISA_MEM_BASE	isa_mem_base
#define PCI_DRAM_OFFSET	pci_dram_offset
#endif /* Platform-dependant I/O */

extern unsigned long isa_io_base;
extern unsigned long isa_mem_base;
extern unsigned long pci_dram_offset;

#define readb(addr) in_8((volatile u8 *)(addr))
#define writeb(b,addr) out_8((volatile u8 *)(addr), (b))
#if defined(CONFIG_APUS)
#define readw(addr) (*(volatile u16 *) (addr))
#define readl(addr) (*(volatile u32 *) (addr))
#define writew(b,addr) ((*(volatile u16 *) (addr)) = (b))
#define writel(b,addr) ((*(volatile u32 *) (addr)) = (b))
#else
#define readw(addr) in_le16((volatile u16 *)(addr))
#define readl(addr) in_le32((volatile u32 *)(addr))
#define writew(b,addr) out_le16((volatile u16 *)(addr),(b))
#define writel(b,addr) out_le32((volatile u32 *)(addr),(b))
#endif


#define __raw_readb(addr)	(*(volatile unsigned char *)(addr))
#define __raw_readw(addr)	(*(volatile unsigned short *)(addr))
#define __raw_readl(addr)	(*(volatile unsigned int *)(addr))
#define __raw_writeb(v, addr)	(*(volatile unsigned char *)(addr) = (v))
#define __raw_writew(v, addr)	(*(volatile unsigned short *)(addr) = (v))
#define __raw_writel(v, addr)	(*(volatile unsigned int *)(addr) = (v))

/*
 * The insw/outsw/insl/outsl macros don't do byte-swapping.
 * They are only used in practice for transferring buffers which
 * are arrays of bytes, and byte-swapping is not appropriate in
 * that case.  - paulus
 */
#define insb(port, buf, ns)	_insb((u8 *)((port)+_IO_BASE), (buf), (ns))
#define outsb(port, buf, ns)	_outsb((u8 *)((port)+_IO_BASE), (buf), (ns))
#define insw(port, buf, ns)	_insw_ns((u16 *)((port)+_IO_BASE), (buf), (ns))
#define outsw(port, buf, ns)	_outsw_ns((u16 *)((port)+_IO_BASE), (buf), (ns))
#define insl(port, buf, nl)	_insl_ns((u32 *)((port)+_IO_BASE), (buf), (nl))
#define outsl(port, buf, nl)	_outsl_ns((u32 *)((port)+_IO_BASE), (buf), (nl))

#ifdef CONFIG_ALL_PPC
/*
 * On powermacs, we will get a machine check exception if we
 * try to read data from a non-existent I/O port.  Because the
 * machine check is an asynchronous exception, it isn't
 * well-defined which instruction SRR0 will point to when the
 * exception occurs.
 * With the sequence below (twi; isync; nop), we have found that
 * the machine check occurs on one of the three instructions on
 * all PPC implementations tested so far.  The twi and isync are
 * needed on the 601 (in fact twi; sync works too), the isync and
 * nop are needed on 604[e|r], and any of twi, sync or isync will
 * work on 603[e], 750, 74x0.
 * The twi creates an explicit data dependency on the returned
 * value which seems to be needed to make the 601 wait for the
 * load to finish.
 */

#define __do_in_asm(name, op)				\
extern __inline__ unsigned int name(unsigned int port)	\
{							\
	unsigned int x;					\
	__asm__ __volatile__(				\
			op "	%0,0,%1\n"		\
		"1:	twi	0,%0,0\n"		\
		"2:	isync\n"			\
		"3:	nop\n"				\
		"4:\n"					\
		".section .fixup,\"ax\"\n"		\
		"5:	li	%0,-1\n"		\
		"	b	4b\n"			\
		".previous\n"				\
		".section __ex_table,\"a\"\n"		\
		"	.align	2\n"			\
		"	.long	1b,5b\n"		\
		"	.long	2b,5b\n"		\
		"	.long	3b,5b\n"		\
		".previous"				\
		: "=&r" (x)				\
		: "r" (port + _IO_BASE));		\
	return x;					\
}

#define __do_out_asm(name, op)				\
extern __inline__ void name(unsigned int val, unsigned int port) \
{							\
	__asm__ __volatile__(				\
		op " %0,0,%1\n"				\
		"1:	sync\n"				\
		"2:\n"					\
		".section __ex_table,\"a\"\n"		\
		"	.align	2\n"			\
		"	.long	1b,2b\n"		\
		".previous"				\
		: : "r" (val), "r" (port + _IO_BASE));	\
}

__do_in_asm(inb, "lbzx")
__do_in_asm(inw, "lhbrx")
__do_in_asm(inl, "lwbrx")
__do_out_asm(outb, "stbx")
__do_out_asm(outw, "sthbrx")
__do_out_asm(outl, "stwbrx")

#elif defined(CONFIG_APUS)
#define inb(port)		in_8((u8 *)((port)+_IO_BASE))
#define outb(val, port)		out_8((u8 *)((port)+_IO_BASE), (val))
#define inw(port)		in_be16((u16 *)((port)+_IO_BASE))
#define outw(val, port)		out_be16((u16 *)((port)+_IO_BASE), (val))
#define inl(port)		in_be32((u32 *)((port)+_IO_BASE))
#define outl(val, port)		out_be32((u32 *)((port)+_IO_BASE), (val))

#else /* not APUS or ALL_PPC */
#define inb(port)		in_8((u8 *)((port)+_IO_BASE))
#define outb(val, port)		out_8((u8 *)((port)+_IO_BASE), (val))
#define inw(port)		in_le16((u16 *)((port)+_IO_BASE))
#define outw(val, port)		out_le16((u16 *)((port)+_IO_BASE), (val))
#define inl(port)		in_le32((u32 *)((port)+_IO_BASE))
#define outl(val, port)		out_le32((u32 *)((port)+_IO_BASE), (val))
#endif

#define inb_p(port)		inb((port))
#define outb_p(val, port)	outb((val), (port))
#define inw_p(port)		inw((port))
#define outw_p(val, port)	outw((val), (port))
#define inl_p(port)		inl((port))
#define outl_p(val, port)	outl((val), (port))

extern void _insb(volatile u8 *port, void *buf, int ns);
extern void _outsb(volatile u8 *port, const void *buf, int ns);
extern void _insw(volatile u16 *port, void *buf, int ns);
extern void _outsw(volatile u16 *port, const void *buf, int ns);
extern void _insl(volatile u32 *port, void *buf, int nl);
extern void _outsl(volatile u32 *port, const void *buf, int nl);
extern void _insw_ns(volatile u16 *port, void *buf, int ns);
extern void _outsw_ns(volatile u16 *port, const void *buf, int ns);
extern void _insl_ns(volatile u32 *port, void *buf, int nl);
extern void _outsl_ns(volatile u32 *port, const void *buf, int nl);

/*
 * The *_ns versions below don't do byte-swapping.
 * Neither do the standard versions now, these are just here
 * for older code.
 */
#define insw_ns(port, buf, ns)	_insw_ns((u16 *)((port)+_IO_BASE), (buf), (ns))
#define outsw_ns(port, buf, ns)	_outsw_ns((u16 *)((port)+_IO_BASE), (buf), (ns))
#define insl_ns(port, buf, nl)	_insl_ns((u32 *)((port)+_IO_BASE), (buf), (nl))
#define outsl_ns(port, buf, nl)	_outsl_ns((u32 *)((port)+_IO_BASE), (buf), (nl))


#define IO_SPACE_LIMIT ~0

#define memset_io(a,b,c)       memset((void *)(a),(b),(c))
#define memcpy_fromio(a,b,c)   memcpy((a),(void *)(b),(c))
#define memcpy_toio(a,b,c)	memcpy((void *)(a),(b),(c))

/*
 * Map in an area of physical address space, for accessing
 * I/O devices etc.
 */
extern void *__ioremap(phys_addr_t address, unsigned long size,
		       unsigned long flags);
extern void *ioremap(phys_addr_t address, unsigned long size);
extern void *ioremap64(unsigned long long address, unsigned long size);
#define ioremap_nocache(addr, size)	ioremap((addr), (size))
extern void iounmap(void *addr);
extern unsigned long iopa(unsigned long addr);
extern unsigned long mm_ptov(unsigned long addr) __attribute__ ((const));
extern void io_block_mapping(unsigned long virt, phys_addr_t phys,
			     unsigned int size, int flags);

/*
 * This makes sure that a value has been returned from a device
 * before any subsequent loads or stores are performed.
 */
extern inline void io_flush(int value)
{
	__asm__ __volatile__("twi 0,%0,0; isync" : : "r" (value));
}

/*
 * The PCI bus is inherently Little-Endian.  The PowerPC is being
 * run Big-Endian.  Thus all values which cross the [PCI] barrier
 * must be endian-adjusted.  Also, the local DRAM has a different
 * address from the PCI point of view, thus buffer addresses also
 * have to be modified [mapped] appropriately.
 */
extern inline unsigned long virt_to_bus(volatile void * address)
{
#ifdef CONFIG_APUS
	return (iopa((unsigned long) address) + PCI_DRAM_OFFSET);
#else
	if (address == (void *)0)
		return 0;
	return (unsigned long)address - KERNELBASE + PCI_DRAM_OFFSET;
#endif
}

extern inline void * bus_to_virt(unsigned long address)
{
#ifdef CONFIG_APUS
	return (void*) mm_ptov (address - PCI_DRAM_OFFSET);
#else
	if (address == 0)
		return 0;
	return (void *)(address - PCI_DRAM_OFFSET + KERNELBASE);
#endif
}

/*
 * Change virtual addresses to physical addresses and vv, for
 * addresses in the area where the kernel has the RAM mapped.
 */
extern inline unsigned long virt_to_phys(volatile void * address)
{
#ifdef CONFIG_APUS
	return iopa ((unsigned long) address);
#else
	return (unsigned long) address - KERNELBASE;
#endif
}

extern inline void * phys_to_virt(unsigned long address)
{
#ifdef CONFIG_APUS
	return (void*) mm_ptov (address);
#else
	return (void *) (address + KERNELBASE);
#endif
}

/*
 * Change "struct page" to physical address.
 */
#define page_to_phys(page)	(((page - mem_map) << PAGE_SHIFT) + PPC_MEMSTART)
#define page_to_bus(page)	(page_to_phys(page) + PCI_DRAM_OFFSET)

/*
 * Enforce In-order Execution of I/O:
 * Acts as a barrier to ensure all previous I/O accesses have
 * completed before any further ones are issued.
 */
extern inline void eieio(void)
{
	__asm__ __volatile__ ("eieio" : : : "memory");
}

/* Enforce in-order execution of data I/O.
 * No distinction between read/write on PPC; use eieio for all three.
 */
#define iobarrier_rw() eieio()
#define iobarrier_r()  eieio()
#define iobarrier_w()  eieio()

/*
 * 8, 16 and 32 bit, big and little endian I/O operations, with barrier.
 *
 * Read operations have additional twi & isync to make sure the read
 * is actually performed (i.e. the data has come back) before we start
 * executing any following instructions.
 */
extern inline int in_8(volatile unsigned char *addr)
{
	int ret;

	__asm__ __volatile__(
		"lbz%U1%X1 %0,%1;\n"
		"twi 0,%0,0;\n"
		"isync" : "=r" (ret) : "m" (*addr));
	return ret;
}

extern inline void out_8(volatile unsigned char *addr, int val)
{
	__asm__ __volatile__("stb%U0%X0 %1,%0; eieio" : "=m" (*addr) : "r" (val));
}

extern inline int in_le16(volatile unsigned short *addr)
{
	int ret;

	__asm__ __volatile__("lhbrx %0,0,%1;\n"
			     "twi 0,%0,0;\n"
			     "isync" : "=r" (ret) :
			      "r" (addr), "m" (*addr));
	return ret;
}

extern inline int in_be16(volatile unsigned short *addr)
{
	int ret;

	__asm__ __volatile__("lhz%U1%X1 %0,%1;\n"
			     "twi 0,%0,0;\n"
			     "isync" : "=r" (ret) : "m" (*addr));
	return ret;
}

extern inline void out_le16(volatile unsigned short *addr, int val)
{
	__asm__ __volatile__("sthbrx %1,0,%2; eieio" : "=m" (*addr) :
			      "r" (val), "r" (addr));
}

extern inline void out_be16(volatile unsigned short *addr, int val)
{
	__asm__ __volatile__("sth%U0%X0 %1,%0; eieio" : "=m" (*addr) : "r" (val));
}

extern inline unsigned in_le32(volatile unsigned *addr)
{
	unsigned ret;

	__asm__ __volatile__("lwbrx %0,0,%1;\n"
			     "twi 0,%0,0;\n"
			     "isync" : "=r" (ret) :
			     "r" (addr), "m" (*addr));
	return ret;
}

extern inline unsigned in_be32(volatile unsigned *addr)
{
	unsigned ret;

	__asm__ __volatile__("lwz%U1%X1 %0,%1;\n"
			     "twi 0,%0,0;\n"
			     "isync" : "=r" (ret) : "m" (*addr));
	return ret;
}

extern inline void out_le32(volatile unsigned *addr, int val)
{
	__asm__ __volatile__("stwbrx %1,0,%2; eieio" : "=m" (*addr) :
			     "r" (val), "r" (addr));
}

extern inline void out_be32(volatile unsigned *addr, int val)
{
	__asm__ __volatile__("stw%U0%X0 %1,%0; eieio" : "=m" (*addr) : "r" (val));
}

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

/* Make some pcmcia drivers happy */
static inline int isa_check_signature(unsigned long io_addr,
	const unsigned char *signature, int length)
{
	return 0;
}

#ifdef CONFIG_NOT_COHERENT_CACHE

/*
 * DMA-consistent mapping functions for PowerPCs that don't support
 * cache snooping.  These allocate/free a region of uncached mapped
 * memory space for use with DMA devices.  Alternatively, you could
 * allocate the space "normally" and use the cache management functions
 * to ensure it is consistent.
 */
extern void *consistent_alloc(int gfp, size_t size, dma_addr_t *handle);
extern void consistent_free(void *vaddr);
extern void consistent_sync(void *vaddr, size_t size, int rw);
extern void consistent_sync_page(struct page *page, unsigned long offset,
				 size_t size, int rw);

#define dma_cache_inv(_start,_size) \
	invalidate_dcache_range(_start, (_start + _size))
#define dma_cache_wback(_start,_size) \
	clean_dcache_range(_start, (_start + _size))
#define dma_cache_wback_inv(_start,_size) \
	flush_dcache_range(_start, (_start + _size))

#else /* ! CONFIG_NOT_COHERENT_CACHE */

/*
 * Cache coherent cores.
 */

#define dma_cache_inv(_start,_size)		do { } while (0)
#define dma_cache_wback(_start,_size)		do { } while (0)
#define dma_cache_wback_inv(_start,_size)	do { } while (0)

#define consistent_alloc(gfp, size, handle)	NULL
#define consistent_free(addr, size)		do { } while (0)
#define consistent_sync(addr, size, rw)		do { } while (0)
#define consistent_sync_page(pg, off, sz, rw)	do { } while (0)

#endif /* CONFIG_NOT_COHERENT_CACHE */
#endif /* _PPC_IO_H */
#endif /* __KERNEL__ */
