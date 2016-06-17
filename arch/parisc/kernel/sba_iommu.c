/*
**  System Bus Adapter (SBA) I/O MMU manager
**
**	(c) Copyright 2000 Grant Grundler
**	(c) Copyright 2000 Hewlett-Packard Company
**
**	Portions (c) 1999 Dave S. Miller (from sparc64 I/O MMU code)
**
**	This program is free software; you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**      the Free Software Foundation; either version 2 of the License, or
**      (at your option) any later version.
**
**
** This module initializes the IOC (I/O Controller) found on B1000/C3000/
** J5000/J7000/N-class/L-class machines and their successors.
**
** FIXME: add DMA hint support programming in both sba and lba modules.
*/

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/init.h>

#include <linux/mm.h>
#include <linux/string.h>
#undef PCI_DEBUG		/* for ASSERT */
#include <linux/pci.h>
#undef PCI_DEBUG

#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/dma.h>		/* for DMA_CHUNK_SIZE */

#include <asm/hardware.h>	/* for register_parisc_driver() stuff */
#include <asm/gsc.h>		/* FIXME: for gsc_read/gsc_write */

#include <linux/proc_fs.h>
#include <asm/runway.h>		/* for proc_runway_root */
#include <asm/pdc.h>		/* for PDC_MODEL_* */

#define MODULE_NAME "SBA"

/*
** The number of debug flags is a clue - this code is fragile.
** Don't even think about messing with it unless you have
** plenty of 710's to sacrifice to the computer gods. :^)
*/
#undef DEBUG_SBA_INIT
#undef DEBUG_SBA_RUN
#undef DEBUG_SBA_RUN_SG
#undef DEBUG_SBA_RESOURCE
#undef ASSERT_PDIR_SANITY
#undef DEBUG_LARGE_SG_ENTRIES
#undef DEBUG_DMB_TRAP

#define SBA_INLINE	__inline__

#ifdef DEBUG_SBA_INIT
#define DBG_INIT(x...)	printk(x)
#else
#define DBG_INIT(x...)
#endif

#ifdef DEBUG_SBA_RUN
#define DBG_RUN(x...)	printk(x)
#else
#define DBG_RUN(x...)
#endif

#ifdef DEBUG_SBA_RUN_SG
#define DBG_RUN_SG(x...)	printk(x)
#else
#define DBG_RUN_SG(x...)
#endif


#ifdef DEBUG_SBA_RESOURCE
#define DBG_RES(x...)	printk(x)
#else
#define DBG_RES(x...)
#endif

/*
** The number of pdir entries to "free" before issueing
** a read to PCOM register to flush out PCOM writes.
** Interacts with allocation granularity (ie 4 or 8 entries
** allocated and free'd/purged at a time might make this
** less interesting).
*/
#define DELAYED_RESOURCE_CNT	16

#define DEFAULT_DMA_HINT_REG	0

#define ASTRO_RUNWAY_PORT	0x582
#define ASTRO_ROPES_PORT	0x780

#define IKE_MERCED_PORT		0x803
#define IKE_ROPES_PORT		0x781

#define REO_MERCED_PORT		0x804
#define REO_ROPES_PORT		0x782

#define REOG_MERCED_PORT	0x805
#define REOG_ROPES_PORT		0x783

#define SBA_FUNC_ID	0x0000	/* function id */
#define SBA_FCLASS	0x0008	/* function class, bist, header, rev... */

#define IS_ASTRO(id) \
(((id)->hversion == ASTRO_RUNWAY_PORT) || ((id)->hversion == ASTRO_ROPES_PORT))

#define IS_IKE(id) \
(((id)->hversion == IKE_MERCED_PORT) || ((id)->hversion == IKE_ROPES_PORT))

#define SBA_FUNC_SIZE 4096   /* SBA configuration function reg set */

#define ASTRO_IOC_OFFSET 0x20000
/* Ike's IOC's occupy functions 2 and 3 (not 0 and 1) */
#define IKE_IOC_OFFSET(p) ((p+2)*SBA_FUNC_SIZE)

#define IOC_CTRL          0x8	/* IOC_CTRL offset */
#define IOC_CTRL_TC       (1 << 0) /* TOC Enable */
#define IOC_CTRL_CE       (1 << 1) /* Coalesce Enable */
#define IOC_CTRL_DE       (1 << 2) /* Dillon Enable */
#define IOC_CTRL_RM       (1 << 8) /* Real Mode */
#define IOC_CTRL_NC       (1 << 9) /* Non Coherent Mode */

#define MAX_IOC		2	/* per Ike. Astro only has 1 */


/*
** Offsets into MBIB (Function 0 on Ike and hopefully Astro)
** Firmware programs this stuff. Don't touch it.
*/
#define IOS_DIST_BASE	0x390
#define IOS_DIST_MASK	0x398
#define IOS_DIST_ROUTE	0x3A0

#define IOS_DIRECT_BASE	0x3C0
#define IOS_DIRECT_MASK	0x3C8
#define IOS_DIRECT_ROUTE 0x3D0

/*
** Offsets into I/O TLB (Function 2 and 3 on Ike)
*/
#define ROPE0_CTL	0x200  /* "regbus pci0" */
#define ROPE1_CTL	0x208
#define ROPE2_CTL	0x210
#define ROPE3_CTL	0x218
#define ROPE4_CTL	0x220
#define ROPE5_CTL	0x228
#define ROPE6_CTL	0x230
#define ROPE7_CTL	0x238

#define HF_ENABLE	0x40


#define IOC_IBASE	0x300	/* IO TLB */
#define IOC_IMASK	0x308
#define IOC_PCOM	0x310
#define IOC_TCNFG	0x318
#define IOC_PDIR_BASE	0x320

#define IOC_IOVA_SPACE_BASE	0	/* IOVA ranges start at 0 */

/*
** IOC supports 4/8/16/64KB page sizes (see TCNFG register)
** It's safer (avoid memory corruption) to keep DMA page mappings
** equivalently sized to VM PAGE_SIZE.
**
** We really can't avoid generating a new mapping for each
** page since the Virtual Coherence Index has to be generated
** and updated for each page.
**
** IOVP_SIZE could only be greater than PAGE_SIZE if we are
** confident the drivers really only touch the next physical
** page iff that driver instance owns it.
*/
#define IOVP_SIZE	PAGE_SIZE
#define IOVP_SHIFT	PAGE_SHIFT
#define IOVP_MASK	PAGE_MASK

#define SBA_PERF_CFG	0x708	/* Performance Counter stuff */
#define SBA_PERF_MASK1	0x718
#define SBA_PERF_MASK2	0x730


/*
** Offsets into PCI Performance Counters (functions 12 and 13)
** Controlled by PERF registers in function 2 & 3 respectively.
*/
#define SBA_PERF_CNT1	0x200
#define SBA_PERF_CNT2	0x208
#define SBA_PERF_CNT3	0x210


struct ioc {
	unsigned long	ioc_hpa;	/* I/O MMU base address */
	char	*res_map;	/* resource map, bit == pdir entry */
	u64	*pdir_base;	/* physical base address */

	unsigned long	*res_hint;	/* next avail IOVP - circular search */
	spinlock_t	res_lock;
	unsigned long	hint_mask_pdir;	/* bits used for DMA hints */
	unsigned int	res_bitshift;	/* from the LEFT! */
	unsigned int	res_size;	/* size of resource map in bytes */
	unsigned int	hint_shift_pdir;
#if DELAYED_RESOURCE_CNT > 0
	int saved_cnt;
	struct sba_dma_pair {
		dma_addr_t	iova;
		size_t		size;
	} saved[DELAYED_RESOURCE_CNT];
#endif

#ifdef CONFIG_PROC_FS
#define SBA_SEARCH_SAMPLE	0x100
	unsigned long avg_search[SBA_SEARCH_SAMPLE];
	unsigned long avg_idx;	/* current index into avg_search */
	unsigned long used_pages;
	unsigned long msingle_calls;
	unsigned long msingle_pages;
	unsigned long msg_calls;
	unsigned long msg_pages;
	unsigned long usingle_calls;
	unsigned long usingle_pages;
	unsigned long usg_calls;
	unsigned long usg_pages;
#endif

	/* STUFF We don't need in performance path */
	unsigned int	pdir_size;	/* in bytes, determined by IOV Space size */
	unsigned long	ibase;		/* pdir IOV Space base - shared w/lba_pci */
	unsigned long	imask;		/* pdir IOV Space mask - shared w/lba_pci */
};

struct sba_device {
	struct sba_device	*next;	/* list of SBA's in system */
	struct parisc_device	*dev;	/* dev found in bus walk */
	struct parisc_device_id	*iodc;	/* data about dev from firmware */
	const char 		*name;
	unsigned long		sba_hpa; /* base address */
	spinlock_t		sba_lock;
	unsigned int		flags;  /* state/functionality enabled */
	unsigned int		hw_rev;  /* HW revision of chip */

	unsigned int		num_ioc;  /* number of on-board IOC's */
	struct ioc		ioc[MAX_IOC];
};


static struct sba_device *sba_list;

static unsigned long ioc_needs_fdc = 0;

/* Ratio of Host MEM to IOV Space size */
static unsigned long sba_mem_ratio = 8;

/* global count of IOMMUs in the system */
static unsigned int global_ioc_cnt = 0;

/* PA8700 (Piranha 2.2) bug workaround */
static unsigned long piranha_bad_128k = 0;

/* Looks nice and keeps the compiler happy */
#define SBA_DEV(d) ((struct sba_device *) (d))


#define ROUNDUP(x,y) ((x + ((y)-1)) & ~((y)-1))


/************************************
** SBA register read and write support
**
** BE WARNED: register writes are posted.
**  (ie follow writes which must reach HW with a read)
**
** Superdome (in particular, REO) allows only 64-bit CSR accesses.
*/
#define READ_REG32(addr)	 le32_to_cpu(__raw_readl(addr))
#define READ_REG64(addr)	 le64_to_cpu(__raw_readq(addr))
#define WRITE_REG32(val, addr) __raw_writel(cpu_to_le32(val), addr)
#define WRITE_REG64(val, addr) __raw_writeq(cpu_to_le64(val), addr)

#ifdef __LP64__
#define READ_REG(addr)		READ_REG64(addr)
#define WRITE_REG(value, addr)	WRITE_REG64(value, addr)
#else
#define READ_REG(addr)		READ_REG32(addr)
#define WRITE_REG(value, addr)	WRITE_REG32(value, addr)
#endif

#ifdef DEBUG_SBA_INIT

/* NOTE: When __LP64__ isn't defined, READ_REG64() is two 32-bit reads */

/**
 * sba_dump_ranges - debugging only - print ranges assigned to this IOA
 * @hpa: base address of the sba
 *
 * Print the MMIO and IO Port address ranges forwarded by an Astro/Ike/RIO
 * IO Adapter (aka Bus Converter).
 */
static void
sba_dump_ranges(unsigned long hpa)
{
	DBG_INIT("SBA at 0x%lx\n", hpa);
	DBG_INIT("IOS_DIST_BASE   : %Lx\n", READ_REG64(hpa+IOS_DIST_BASE));
	DBG_INIT("IOS_DIST_MASK   : %Lx\n", READ_REG64(hpa+IOS_DIST_MASK));
	DBG_INIT("IOS_DIST_ROUTE  : %Lx\n", READ_REG64(hpa+IOS_DIST_ROUTE));
	DBG_INIT("\n");
	DBG_INIT("IOS_DIRECT_BASE : %Lx\n", READ_REG64(hpa+IOS_DIRECT_BASE));
	DBG_INIT("IOS_DIRECT_MASK : %Lx\n", READ_REG64(hpa+IOS_DIRECT_MASK));
	DBG_INIT("IOS_DIRECT_ROUTE: %Lx\n", READ_REG64(hpa+IOS_DIRECT_ROUTE));
}

/**
 * sba_dump_tlb - debugging only - print IOMMU operating parameters
 * @hpa: base address of the IOMMU
 *
 * Print the size/location of the IO MMU PDIR.
 */
static void
sba_dump_tlb(unsigned long hpa)
{
	DBG_INIT("IO TLB at 0x%lx\n", hpa);
	DBG_INIT("IOC_IBASE    : %Lx\n", READ_REG64(hpa+IOC_IBASE));
	DBG_INIT("IOC_IMASK    : %Lx\n", READ_REG64(hpa+IOC_IMASK));
	DBG_INIT("IOC_TCNFG    : %Lx\n", READ_REG64(hpa+IOC_TCNFG));
	DBG_INIT("IOC_PDIR_BASE: %Lx\n", READ_REG64(hpa+IOC_PDIR_BASE));
	DBG_INIT("\n");
}
#endif


#ifdef ASSERT_PDIR_SANITY

/**
 * sba_dump_pdir_entry - debugging only - print one IOMMU PDIR entry
 * @ioc: IO MMU structure which owns the pdir we are interested in.
 * @msg: text to print ont the output line.
 * @pide: pdir index.
 *
 * Print one entry of the IO MMU PDIR in human readable form.
 */
static void
sba_dump_pdir_entry(struct ioc *ioc, char *msg, uint pide)
{
	/* start printing from lowest pde in rval */
	u64 *ptr = &(ioc->pdir_base[pide & (~0U * BITS_PER_LONG)]);
	unsigned long *rptr = (unsigned long *) &(ioc->res_map[(pide >>3) & ~(sizeof(unsigned long) - 1)]);
	uint rcnt;

	printk(KERN_DEBUG "SBA: %s rp %p bit %d rval 0x%lx\n",
		 msg,
		 rptr, pide & (BITS_PER_LONG - 1), *rptr);

	rcnt = 0;
	while (rcnt < BITS_PER_LONG) {
		printk(KERN_DEBUG "%s %2d %p %016Lx\n",
			(rcnt == (pide & (BITS_PER_LONG - 1)))
				? "    -->" : "       ",
			rcnt, ptr, *ptr );
		rcnt++;
		ptr++;
	}
	printk(KERN_DEBUG "%s", msg);
}


/**
 * sba_check_pdir - debugging only - consistency checker
 * @ioc: IO MMU structure which owns the pdir we are interested in.
 * @msg: text to print ont the output line.
 *
 * Verify the resource map and pdir state is consistent
 */
static int
sba_check_pdir(struct ioc *ioc, char *msg)
{
	u32 *rptr_end = (u32 *) &(ioc->res_map[ioc->res_size]);
	u32 *rptr = (u32 *) ioc->res_map;	/* resource map ptr */
	u64 *pptr = ioc->pdir_base;	/* pdir ptr */
	uint pide = 0;

	while (rptr < rptr_end) {
		u32 rval = *rptr;
		int rcnt = 32;	/* number of bits we might check */

		while (rcnt) {
			/* Get last byte and highest bit from that */
			u32 pde = ((u32) (((char *)pptr)[7])) << 24;
			if ((rval ^ pde) & 0x80000000)
			{
				/*
				** BUMMER!  -- res_map != pdir --
				** Dump rval and matching pdir entries
				*/
				sba_dump_pdir_entry(ioc, msg, pide);
				return(1);
			}
			rcnt--;
			rval <<= 1;	/* try the next bit */
			pptr++;
			pide++;
		}
		rptr++;	/* look at next word of res_map */
	}
	/* It'd be nice if we always got here :^) */
	return 0;
}


/**
 * sba_dump_sg - debugging only - print Scatter-Gather list
 * @ioc: IO MMU structure which owns the pdir we are interested in.
 * @startsg: head of the SG list
 * @nents: number of entries in SG list
 *
 * print the SG list so we can verify it's correct by hand.
 */
static void
sba_dump_sg( struct ioc *ioc, struct scatterlist *startsg, int nents)
{
	while (nents-- > 0) {
		printk(KERN_DEBUG " %d : %08lx/%05x %p/%05x\n",
				nents,
				(unsigned long) sg_dma_address(startsg),
				sg_dma_len(startsg),
				sg_virt_addr(startsg), startsg->length);
		startsg++;
	}
}

#endif /* ASSERT_PDIR_SANITY */




/**************************************************************
*
*   I/O Pdir Resource Management
*
*   Bits set in the resource map are in use.
*   Each bit can represent a number of pages.
*   LSbs represent lower addresses (IOVA's).
*
***************************************************************/
#define PAGES_PER_RANGE 1	/* could increase this to 4 or 8 if needed */

/* Convert from IOVP to IOVA and vice versa. */
#define SBA_IOVA(ioc,iovp,offset,hint_reg) ((iovp) | (offset) | ((hint_reg)<<(ioc->hint_shift_pdir)))
#define SBA_IOVP(ioc,iova) ((iova) & ioc->hint_mask_pdir)

/* FIXME : review these macros to verify correctness and usage */
#define PDIR_INDEX(iovp)   ((iovp)>>IOVP_SHIFT)
#define MKIOVP(dma_hint,pide)  (dma_addr_t)((long)(dma_hint) | ((long)(pide) << IOVP_SHIFT))
#define MKIOVA(iovp,offset) (dma_addr_t)((long)iovp | (long)offset)

#define RESMAP_MASK(n)    (~0UL << (BITS_PER_LONG - (n)))
#define RESMAP_IDX_MASK   (sizeof(unsigned long) - 1)


/**
 * sba_search_bitmap - find free space in IO PDIR resource bitmap
 * @ioc: IO MMU structure which owns the pdir we are interested in.
 * @bits_wanted: number of entries we need.
 *
 * Find consecutive free bits in resource bitmap.
 * Each bit represents one entry in the IO Pdir.
 * Cool perf optimization: search for log2(size) bits at a time.
 */
static SBA_INLINE unsigned long
sba_search_bitmap(struct ioc *ioc, unsigned long bits_wanted)
{
	unsigned long *res_ptr = ioc->res_hint;
	unsigned long *res_end = (unsigned long *) &(ioc->res_map[ioc->res_size]);
	unsigned long pide = ~0UL;

	ASSERT(((unsigned long) ioc->res_hint & (sizeof(unsigned long) - 1UL)) == 0);
	ASSERT(res_ptr < res_end);
	if (bits_wanted > (BITS_PER_LONG/2)) {
		/* Search word at a time - no mask needed */
		for(; res_ptr < res_end; ++res_ptr) {
			if (*res_ptr == 0) {
				*res_ptr = RESMAP_MASK(bits_wanted);
				pide = ((unsigned long)res_ptr - (unsigned long)ioc->res_map);
				pide <<= 3;	/* convert to bit address */
				break;
			}
		}
		/* point to the next word on next pass */
		res_ptr++;
		ioc->res_bitshift = 0;
	} else {
		/*
		** Search the resource bit map on well-aligned values.
		** "o" is the alignment.
		** We need the alignment to invalidate I/O TLB using
		** SBA HW features in the unmap path.
		*/
		unsigned long o = 1 << get_order(bits_wanted << PAGE_SHIFT);
		uint bitshiftcnt = ROUNDUP(ioc->res_bitshift, o);
		unsigned long mask;

		if (bitshiftcnt >= BITS_PER_LONG) {
			bitshiftcnt = 0;
			res_ptr++;
		}
		mask = RESMAP_MASK(bits_wanted) >> bitshiftcnt;

		DBG_RES("%s() o %ld %p", __FUNCTION__, o, res_ptr);
		while(res_ptr < res_end)
		{ 
			DBG_RES("    %p %lx %lx\n", res_ptr, mask, *res_ptr);
			ASSERT(0 != mask);
			if(0 == ((*res_ptr) & mask)) {
				*res_ptr |= mask;     /* mark resources busy! */
				pide = ((unsigned long)res_ptr - (unsigned long)ioc->res_map);
				pide <<= 3;	/* convert to bit address */
				pide += bitshiftcnt;
				break;
			}
			mask >>= o;
			bitshiftcnt += o;
			if (0 == mask) {
				mask = RESMAP_MASK(bits_wanted);
				bitshiftcnt=0;
				res_ptr++;
			}
		}
		/* look in the same word on the next pass */
		ioc->res_bitshift = bitshiftcnt + bits_wanted;
	}

	/* wrapped ? */
	if (res_end <= res_ptr) {
		ioc->res_hint = (unsigned long *) ioc->res_map;
		ioc->res_bitshift = 0;
	} else {
		ioc->res_hint = res_ptr;
	}
	return (pide);
}


/**
 * sba_alloc_range - find free bits and mark them in IO PDIR resource bitmap
 * @ioc: IO MMU structure which owns the pdir we are interested in.
 * @size: number of bytes to create a mapping for
 *
 * Given a size, find consecutive unmarked and then mark those bits in the
 * resource bit map.
 */
static int
sba_alloc_range(struct ioc *ioc, size_t size)
{
	unsigned int pages_needed = size >> IOVP_SHIFT;
#ifdef CONFIG_PROC_FS
	unsigned long cr_start = mfctl(16);
#endif
	unsigned long pide;

	ASSERT(pages_needed);
	ASSERT((pages_needed * IOVP_SIZE) <= DMA_CHUNK_SIZE);
	ASSERT(pages_needed <= BITS_PER_LONG);
	ASSERT(0 == (size & ~IOVP_MASK));

	/*
	** "seek and ye shall find"...praying never hurts either...
	** ggg sacrifices another 710 to the computer gods.
	*/

	pide = sba_search_bitmap(ioc, pages_needed);
	if (pide >= (ioc->res_size << 3)) {
		pide = sba_search_bitmap(ioc, pages_needed);
		if (pide >= (ioc->res_size << 3))
			panic(__FILE__ ": I/O MMU @ %lx is out of mapping resources\n", ioc->ioc_hpa);
	}

#ifdef ASSERT_PDIR_SANITY
	/* verify the first enable bit is clear */
	if(0x00 != ((u8 *) ioc->pdir_base)[pide*sizeof(u64) + 7]) {
		sba_dump_pdir_entry(ioc, "sba_search_bitmap() botched it?", pide);
	}
#endif

	DBG_RES("%s(%x) %d -> %lx hint %x/%x\n",
		__FUNCTION__, size, pages_needed, pide,
		(uint) ((unsigned long) ioc->res_hint - (unsigned long) ioc->res_map),
		ioc->res_bitshift );

#ifdef CONFIG_PROC_FS
	{
		unsigned long cr_end = mfctl(16);
		unsigned long tmp = cr_end - cr_start;
		/* check for roll over */
		cr_start = (cr_end < cr_start) ?  -(tmp) : (tmp);
	}
	ioc->avg_search[ioc->avg_idx++] = cr_start;
	ioc->avg_idx &= SBA_SEARCH_SAMPLE - 1;

	ioc->used_pages += pages_needed;
#endif

	return (pide);
}


/**
 * sba_free_range - unmark bits in IO PDIR resource bitmap
 * @ioc: IO MMU structure which owns the pdir we are interested in.
 * @iova: IO virtual address which was previously allocated.
 * @size: number of bytes to create a mapping for
 *
 * clear bits in the ioc's resource map
 */
static SBA_INLINE void
sba_free_range(struct ioc *ioc, dma_addr_t iova, size_t size)
{
	unsigned long iovp = SBA_IOVP(ioc, iova);
	unsigned int pide = PDIR_INDEX(iovp);
	unsigned int ridx = pide >> 3;	/* convert bit to byte address */
	unsigned long *res_ptr = (unsigned long *) &((ioc)->res_map[ridx & ~RESMAP_IDX_MASK]);

	int bits_not_wanted = size >> IOVP_SHIFT;

	/* 3-bits "bit" address plus 2 (or 3) bits for "byte" == bit in word */
	unsigned long m = RESMAP_MASK(bits_not_wanted) >> (pide & (BITS_PER_LONG - 1));

	DBG_RES("%s( ,%x,%x) %x/%lx %x %p %lx\n",
		__FUNCTION__, (uint) iova, size,
		bits_not_wanted, m, pide, res_ptr, *res_ptr);

#ifdef CONFIG_PROC_FS
	ioc->used_pages -= bits_not_wanted;
#endif

	ASSERT(m != 0);
	ASSERT(bits_not_wanted);
	ASSERT((bits_not_wanted * IOVP_SIZE) <= DMA_CHUNK_SIZE);
	ASSERT(bits_not_wanted <= BITS_PER_LONG);
	ASSERT((*res_ptr & m) == m); /* verify same bits are set */
	*res_ptr &= ~m;
}


/**************************************************************
*
*   "Dynamic DMA Mapping" support (aka "Coherent I/O")
*
***************************************************************/

#define SBA_DMA_HINT(ioc, val) ((val) << (ioc)->hint_shift_pdir)


typedef unsigned long space_t;
#define KERNEL_SPACE 0

/**
 * sba_io_pdir_entry - fill in one IO PDIR entry
 * @pdir_ptr:  pointer to IO PDIR entry
 * @sid: process Space ID
 * @vba: Virtual CPU address of buffer to map
 *
 * SBA Mapping Routine
 *
 * Given a virtual address (vba, arg2) and space id, (sid, arg1)
 * sba_io_pdir_entry() loads the I/O PDIR entry pointed to by
 * pdir_ptr (arg0). Each IO Pdir entry consists of 8 bytes as
 * shown below (MSB == bit 0):
 *
 *  0                    19                                 51   55       63
 * +-+---------------------+----------------------------------+----+--------+
 * |V|        U            |            PPN[43:12]            | U  |   VI   |
 * +-+---------------------+----------------------------------+----+--------+
 *
 *  V  == Valid Bit
 *  U  == Unused
 * PPN == Physical Page Number
 * VI  == Virtual Index (aka Coherent Index)
 *
 * The physical address fields are filled with the results of the LPA
 * instruction.  The virtual index field is filled with the results of
 * of the LCI (Load Coherence Index) instruction.  The 8 bits used for
 * the virtual index are bits 12:19 of the value returned by LCI.
 *
 * We need to pre-swap the bytes since PCX-W is Big Endian.
 */


void SBA_INLINE
sba_io_pdir_entry(u64 *pdir_ptr, space_t sid, unsigned long vba)
{
	u64 pa; /* physical address */
	register unsigned ci; /* coherent index */

	/* We currently only support kernel addresses.
	 * fdc instr below will need to reload sr1 with KERNEL_SPACE
	 * once we try to support direct DMA to user space.
	 */
	ASSERT(sid == KERNEL_SPACE);

	pa = virt_to_phys(vba);
	pa &= ~4095ULL;			/* clear out offset bits */

	mtsp(sid,1);
	asm("lci 0(%%sr1, %1), %0" : "=r" (ci) : "r" (vba));
	pa |= (ci >> 12) & 0xff;  /* move CI (8 bits) into lowest byte */

	pa |= 0x8000000000000000ULL;	/* set "valid" bit */
	*pdir_ptr = cpu_to_le64(pa);	/* swap and store into I/O Pdir */

	/*
	 * If the PDC_MODEL capabilities has Non-coherent IO-PDIR bit set
	 * (bit #61, big endian), we have to flush and sync every time
	 * IO-PDIR is changed in Ike/Astro.
	 */
	if (ioc_needs_fdc) {
		asm volatile("fdc 0(%%sr1,%0)\n\tsync" : : "r" (pdir_ptr));
	}
}


/**
 * sba_mark_invalid - invalidate one or more IO PDIR entries
 * @ioc: IO MMU structure which owns the pdir we are interested in.
 * @iova:  IO Virtual Address mapped earlier
 * @byte_cnt:  number of bytes this mapping covers.
 *
 * Marking the IO PDIR entry(ies) as Invalid and invalidate
 * corresponding IO TLB entry. The Ike PCOM (Purge Command Register)
 * is to purge stale entries in the IO TLB when unmapping entries.
 *
 * The PCOM register supports purging of multiple pages, with a minium
 * of 1 page and a maximum of 2GB. Hardware requires the address be
 * aligned to the size of the range being purged. The size of the range
 * must be a power of 2. The "Cool perf optimization" in the
 * allocation routine helps keep that true.
 */
static SBA_INLINE void
sba_mark_invalid(struct ioc *ioc, dma_addr_t iova, size_t byte_cnt)
{
	u32 iovp = (u32) SBA_IOVP(ioc,iova);

	/* Even though this is a big-endian machine, the entries
	** in the iopdir are little endian. That's why we clear the byte
	** at +7 instead of at +0.
	*/
	int off = PDIR_INDEX(iovp)*sizeof(u64)+7;

	/* Must be non-zero and rounded up */
	ASSERT(byte_cnt > 0);
	ASSERT(0 == (byte_cnt & ~IOVP_MASK));

#ifdef ASSERT_PDIR_SANITY
	/* Assert first pdir entry is set */
	if (0x80 != (((u8 *) ioc->pdir_base)[off])) {
		sba_dump_pdir_entry(ioc,"sba_mark_invalid()", PDIR_INDEX(iovp));
	}
#endif

	if (byte_cnt <= IOVP_SIZE)
	{
		ASSERT( off < ioc->pdir_size);

		iovp |= IOVP_SHIFT;     /* set "size" field for PCOM */

		/*
		** clear I/O PDIR entry "valid" bit
		** Do NOT clear the rest - save it for debugging.
		** We should only clear bits that have previously
		** been enabled.
		*/
		((u8 *)(ioc->pdir_base))[off] = 0;
	} else {
		u32 t = get_order(byte_cnt) + PAGE_SHIFT;

		iovp |= t;
		ASSERT(t <= 31);   /* 2GB! Max value of "size" field */

		do {
			/* verify this pdir entry is enabled */
			ASSERT(0x80 == (((u8 *) ioc->pdir_base)[off] & 0x80));
			/* clear I/O Pdir entry "valid" bit first */
			((u8 *)(ioc->pdir_base))[off] = 0;
			off += sizeof(u64);
			byte_cnt -= IOVP_SIZE;
		} while (byte_cnt > 0);
	}

	WRITE_REG(iovp, ioc->ioc_hpa+IOC_PCOM);
}

/**
 * sba_dma_supported - PCI driver can query DMA support
 * @dev: instance of PCI owned by the driver that's asking
 * @mask:  number of address bits this PCI device can handle
 *
 * See Documentation/DMA-mapping.txt
 */
static int
sba_dma_supported( struct pci_dev *dev, u64 mask)
{
	if (dev == NULL) {
		printk(KERN_ERR MODULE_NAME ": EISA/ISA/et al not supported\n");
		BUG();
		return(0);
	}

	/* only support 32-bit PCI devices - no DAC support (yet) */
	return((int) (mask == 0xffffffffUL));
}


/**
 * sba_map_single - map one buffer and return IOVA for DMA
 * @dev: instance of PCI owned by the driver that's asking.
 * @addr:  driver buffer to map.
 * @size:  number of bytes to map in driver buffer.
 * @direction:  R/W or both.
 *
 * See Documentation/DMA-mapping.txt
 */
static dma_addr_t
sba_map_single(struct pci_dev *dev, void *addr, size_t size, int direction)
{
	struct ioc *ioc;
	unsigned long flags; 
	dma_addr_t iovp;
	dma_addr_t offset;
	u64 *pdir_start;
	int pide;

	ASSERT(size > 0);
	ASSERT(size <= DMA_CHUNK_SIZE);

	ASSERT(dev->sysdata);
	ioc = GET_IOC(dev);
	ASSERT(ioc);

	/* save offset bits */
	offset = ((dma_addr_t) (long) addr) & ~IOVP_MASK;

	/* round up to nearest IOVP_SIZE */
	size = (size + offset + ~IOVP_MASK) & IOVP_MASK;

	spin_lock_irqsave(&ioc->res_lock, flags);
#ifdef ASSERT_PDIR_SANITY
	sba_check_pdir(ioc,"Check before sba_map_single()");
#endif

#ifdef CONFIG_PROC_FS
	ioc->msingle_calls++;
	ioc->msingle_pages += size >> IOVP_SHIFT;
#endif
	pide = sba_alloc_range(ioc, size);
	iovp = (dma_addr_t) pide << IOVP_SHIFT;

	DBG_RUN("%s() 0x%p -> 0x%lx",
		__FUNCTION__, addr, (long) iovp | offset);

	pdir_start = &(ioc->pdir_base[pide]);

	while (size > 0) {
		ASSERT(((u8 *)pdir_start)[7] == 0); /* verify availability */
		sba_io_pdir_entry(pdir_start, KERNEL_SPACE, (unsigned long) addr);

		DBG_RUN(" pdir 0x%p %02x%02x%02x%02x%02x%02x%02x%02x\n",
			pdir_start,
			(u8) (((u8 *) pdir_start)[7]),
			(u8) (((u8 *) pdir_start)[6]),
			(u8) (((u8 *) pdir_start)[5]),
			(u8) (((u8 *) pdir_start)[4]),
			(u8) (((u8 *) pdir_start)[3]),
			(u8) (((u8 *) pdir_start)[2]),
			(u8) (((u8 *) pdir_start)[1]),
			(u8) (((u8 *) pdir_start)[0])
			);

		addr += IOVP_SIZE;
		size -= IOVP_SIZE;
		pdir_start++;
	}
	/* form complete address */
#ifdef ASSERT_PDIR_SANITY
	sba_check_pdir(ioc,"Check after sba_map_single()");
#endif
	spin_unlock_irqrestore(&ioc->res_lock, flags);
	return SBA_IOVA(ioc, iovp, offset, DEFAULT_DMA_HINT_REG);
}


/**
 * sba_unmap_single - unmap one IOVA and free resources
 * @dev: instance of PCI owned by the driver that's asking.
 * @iova:  IOVA of driver buffer previously mapped.
 * @size:  number of bytes mapped in driver buffer.
 * @direction:  R/W or both.
 *
 * See Documentation/DMA-mapping.txt
 */
static void
sba_unmap_single(struct pci_dev *dev, dma_addr_t iova, size_t size, int direction)
{
	struct ioc *ioc;
#if DELAYED_RESOURCE_CNT > 0
	struct sba_dma_pair *d;
#endif
	unsigned long flags; 
	dma_addr_t offset;

	ASSERT(dev->sysdata);
	ioc = GET_IOC(dev);
	ASSERT(ioc);

	offset = iova & ~IOVP_MASK;

	DBG_RUN("%s() iovp 0x%lx/%x\n",
		__FUNCTION__, (long) iova, size);

	iova ^= offset;        /* clear offset bits */
	size += offset;
	size = ROUNDUP(size, IOVP_SIZE);

	spin_lock_irqsave(&ioc->res_lock, flags);

#ifdef CONFIG_PROC_FS
	ioc->usingle_calls++;
	ioc->usingle_pages += size >> IOVP_SHIFT;
#endif

#if DELAYED_RESOURCE_CNT > 0
	d = &(ioc->saved[ioc->saved_cnt]);
	d->iova = iova;
	d->size = size;
	if (++(ioc->saved_cnt) >= DELAYED_RESOURCE_CNT) {
		int cnt = ioc->saved_cnt;
		while (cnt--) {
			sba_mark_invalid(ioc, d->iova, d->size);
			sba_free_range(ioc, d->iova, d->size);
			d--;
		}
		ioc->saved_cnt = 0;
		READ_REG(ioc->ioc_hpa+IOC_PCOM);	/* flush purges */
	}
#else /* DELAYED_RESOURCE_CNT == 0 */
	sba_mark_invalid(ioc, iova, size);
	sba_free_range(ioc, iova, size);
	READ_REG(ioc->ioc_hpa+IOC_PCOM);	/* flush purges */
#endif /* DELAYED_RESOURCE_CNT == 0 */
	spin_unlock_irqrestore(&ioc->res_lock, flags);

	/* XXX REVISIT for 2.5 Linux - need syncdma for zero-copy support.
	** For Astro based systems this isn't a big deal WRT performance.
	** As long as 2.4 kernels copyin/copyout data from/to userspace,
	** we don't need the syncdma. The issue here is I/O MMU cachelines
	** are *not* coherent in all cases.  May be hwrev dependent.
	** Need to investigate more.
	asm volatile("syncdma");	
	*/
}


/**
 * sba_alloc_consistent - allocate/map shared mem for DMA
 * @hwdev: instance of PCI owned by the driver that's asking.
 * @size:  number of bytes mapped in driver buffer.
 * @dma_handle:  IOVA of new buffer.
 *
 * See Documentation/DMA-mapping.txt
 */
static void *
sba_alloc_consistent(struct pci_dev *hwdev, size_t size, dma_addr_t *dma_handle)
{
	void *ret;

	if (!hwdev) {
		/* only support PCI */
		*dma_handle = 0;
		return 0;
	}

        ret = (void *) __get_free_pages(GFP_ATOMIC, get_order(size));

	if (ret) {
		memset(ret, 0, size);
		*dma_handle = sba_map_single(hwdev, ret, size, 0);
	}

	return ret;
}


/**
 * sba_free_consistent - free/unmap shared mem for DMA
 * @hwdev: instance of PCI owned by the driver that's asking.
 * @size:  number of bytes mapped in driver buffer.
 * @vaddr:  virtual address IOVA of "consistent" buffer.
 * @dma_handler:  IO virtual address of "consistent" buffer.
 *
 * See Documentation/DMA-mapping.txt
 */
static void
sba_free_consistent(struct pci_dev *hwdev, size_t size, void *vaddr, dma_addr_t dma_handle)
{
	sba_unmap_single(hwdev, dma_handle, size, 0);
	free_pages((unsigned long) vaddr, get_order(size));
}


/*
** Since 0 is a valid pdir_base index value, can't use that
** to determine if a value is valid or not. Use a flag to indicate
** the SG list entry contains a valid pdir index.
*/
#define PIDE_FLAG 0x80000000UL

#ifdef DEBUG_LARGE_SG_ENTRIES
int dump_run_sg = 0;
#endif


/**
 * sba_fill_pdir - write allocated SG entries into IO PDIR
 * @ioc: IO MMU structure which owns the pdir we are interested in.
 * @startsg:  list of IOVA/size pairs
 * @nents: number of entries in startsg list
 *
 * Take preprocessed SG list and write corresponding entries
 * in the IO PDIR.
 */

static SBA_INLINE int
sba_fill_pdir(
	struct ioc *ioc,
	struct scatterlist *startsg,
	int nents)
{
	struct scatterlist *dma_sg = startsg;	/* pointer to current DMA */
	int n_mappings = 0;
	u64 *pdirp = 0;
	unsigned long dma_offset = 0;

	dma_sg--;
	while (nents-- > 0) {
		int     cnt = sg_dma_len(startsg);
		sg_dma_len(startsg) = 0;

#ifdef DEBUG_LARGE_SG_ENTRIES
		if (dump_run_sg)
			printk(KERN_DEBUG " %2d : %08lx/%05x %p/%05x\n",
				nents,
				(unsigned long) sg_dma_address(startsg), cnt,
				sg_virt_addr(startsg), startsg->length
		);
#else
		DBG_RUN_SG(" %d : %08lx/%05x %p/%05x\n",
				nents,
				(unsigned long) sg_dma_address(startsg), cnt,
				sg_virt_addr(startsg), startsg->length
		);
#endif
		/*
		** Look for the start of a new DMA stream
		*/
		if (sg_dma_address(startsg) & PIDE_FLAG) {
			u32 pide = sg_dma_address(startsg) & ~PIDE_FLAG;
			dma_offset = (unsigned long) pide & ~IOVP_MASK;
			sg_dma_address(startsg) = 0;
			dma_sg++;
			sg_dma_address(dma_sg) = pide;
			pdirp = &(ioc->pdir_base[pide >> IOVP_SHIFT]);
			n_mappings++;
		}

		/*
		** Look for a VCONTIG chunk
		*/
		if (cnt) {
			unsigned long vaddr = (unsigned long) sg_virt_addr(startsg);
			ASSERT(pdirp);

			/* Since multiple Vcontig blocks could make up
			** one DMA stream, *add* cnt to dma_len.
			*/
			sg_dma_len(dma_sg) += cnt;
			cnt += dma_offset;
			dma_offset=0;	/* only want offset on first chunk */
			cnt = ROUNDUP(cnt, IOVP_SIZE);
#ifdef CONFIG_PROC_FS
			ioc->msg_pages += cnt >> IOVP_SHIFT;
#endif
			do {
				sba_io_pdir_entry(pdirp, KERNEL_SPACE, vaddr);
				vaddr += IOVP_SIZE;
				cnt -= IOVP_SIZE;
				pdirp++;
			} while (cnt > 0);
		}
		startsg++;
	}
#ifdef DEBUG_LARGE_SG_ENTRIES
	dump_run_sg = 0;
#endif
	return(n_mappings);
}


/*
** Two address ranges are DMA contiguous *iff* "end of prev" and
** "start of next" are both on a page boundry.
**
** (shift left is a quick trick to mask off upper bits)
*/
#define DMA_CONTIG(__X, __Y) \
	(((((unsigned long) __X) | ((unsigned long) __Y)) << (BITS_PER_LONG - PAGE_SHIFT)) == 0UL)


/**
 * sba_coalesce_chunks - preprocess the SG list
 * @ioc: IO MMU structure which owns the pdir we are interested in.
 * @startsg:  list of IOVA/size pairs
 * @nents: number of entries in startsg list
 *
 * First pass is to walk the SG list and determine where the breaks are
 * in the DMA stream. Allocates PDIR entries but does not fill them.
 * Returns the number of DMA chunks.
 *
 * Doing the fill seperate from the coalescing/allocation keeps the
 * code simpler. Future enhancement could make one pass through
 * the sglist do both.
 */
static SBA_INLINE int
sba_coalesce_chunks( struct ioc *ioc,
	struct scatterlist *startsg,
	int nents)
{
	struct scatterlist *vcontig_sg;    /* VCONTIG chunk head */
	unsigned long vcontig_len;         /* len of VCONTIG chunk */
	unsigned long vcontig_end;
	struct scatterlist *dma_sg;        /* next DMA stream head */
	unsigned long dma_offset, dma_len; /* start/len of DMA stream */
	int n_mappings = 0;

	while (nents > 0) {
		unsigned long vaddr = (unsigned long) sg_virt_addr(startsg); 

		/*
		** Prepare for first/next DMA stream
		*/
		dma_sg = vcontig_sg = startsg;
		dma_len = vcontig_len = vcontig_end = startsg->length;
		vcontig_end +=  vaddr;
		dma_offset = vaddr & ~IOVP_MASK;

		/* PARANOID: clear entries */
		sg_dma_address(startsg) = 0;
		sg_dma_len(startsg) = 0;

		/*
		** This loop terminates one iteration "early" since
		** it's always looking one "ahead".
		*/
		while (--nents > 0) {
			unsigned long vaddr;	/* tmp */

			startsg++;

			/* PARANOID: clear entries */
			sg_dma_address(startsg) = 0;
			sg_dma_len(startsg) = 0;

			/* catch brokenness in SCSI layer */
			ASSERT(startsg->length <= DMA_CHUNK_SIZE);

			/*
			** First make sure current dma stream won't
			** exceed DMA_CHUNK_SIZE if we coalesce the
			** next entry.
			*/
			if (((dma_len + dma_offset + startsg->length + ~IOVP_MASK) & IOVP_MASK) > DMA_CHUNK_SIZE)
				break;

			/*
			** Then look for virtually contiguous blocks.
			** PARISC needs to associate a virtual address
			** with each IO address mapped. The CPU cache is
			** virtually tagged and the IOMMU uses part
			** of the virtual address to participate in
			** CPU cache coherency.
			**
			** append the next transaction?
			*/
			vaddr = (unsigned long) sg_virt_addr(startsg);
			if  (vcontig_end == vaddr)
			{
				vcontig_len += startsg->length;
				vcontig_end += startsg->length;
				dma_len     += startsg->length;
				continue;
			}

#ifdef DEBUG_LARGE_SG_ENTRIES
			dump_run_sg = (vcontig_len > IOVP_SIZE);
#endif

			/*
			** Not virtually contigous.
			** Terminate prev chunk.
			** Start a new chunk.
			**
			** Once we start a new VCONTIG chunk, dma_offset
			** can't change. And we need the offset from the first
			** chunk - not the last one. Ergo Successive chunks
			** must start on page boundaries and dove tail
			** with it's predecessor.
			*/
			sg_dma_len(vcontig_sg) = vcontig_len;

			vcontig_sg = startsg;
			vcontig_len = startsg->length;

			/*
			** 3) do the entries end/start on page boundaries?
			**    Don't update vcontig_end until we've checked.
			*/
			if (DMA_CONTIG(vcontig_end, vaddr))
			{
				vcontig_end = vcontig_len + vaddr;
				dma_len += vcontig_len;
				continue;
			} else {
				break;
			}
		}

		/*
		** End of DMA Stream
		** Terminate last VCONTIG block.
		** Allocate space for DMA stream.
		*/
		sg_dma_len(vcontig_sg) = vcontig_len;
		dma_len = (dma_len + dma_offset + ~IOVP_MASK) & IOVP_MASK;
		ASSERT(dma_len <= DMA_CHUNK_SIZE);
		sg_dma_address(dma_sg) =
			PIDE_FLAG 
			| (sba_alloc_range(ioc, dma_len) << IOVP_SHIFT)
			| dma_offset;
		n_mappings++;
	}

	return n_mappings;
}


/**
 * sba_map_sg - map Scatter/Gather list
 * @dev: instance of PCI owned by the driver that's asking.
 * @sglist:  array of buffer/length pairs
 * @nents:  number of entries in list
 * @direction:  R/W or both.
 *
 * See Documentation/DMA-mapping.txt
 */
static int
sba_map_sg(struct pci_dev *dev, struct scatterlist *sglist, int nents, int direction)
{
	struct ioc *ioc;
	int coalesced, filled = 0;
	unsigned long flags;

	DBG_RUN_SG("%s() START %d entries\n", __FUNCTION__, nents);

	ASSERT(dev->sysdata);
	ioc = GET_IOC(dev);
	ASSERT(ioc);

	/* Fast path single entry scatterlists. */
	if (nents == 1) {
		sg_dma_address(sglist) = sba_map_single(dev,
						sg_virt_addr(sglist),
						sglist->length, direction);
		sg_dma_len(sglist)     = sglist->length;
		return 1;
	}

	spin_lock_irqsave(&ioc->res_lock, flags);

#ifdef ASSERT_PDIR_SANITY
	if (sba_check_pdir(ioc,"Check before sba_map_sg()"))
	{
		sba_dump_sg(ioc, sglist, nents);
		panic("Check before sba_map_sg()");
	}
#endif

#ifdef CONFIG_PROC_FS
	ioc->msg_calls++;
#endif

	/*
	** First coalesce the chunks and allocate I/O pdir space
	**
	** If this is one DMA stream, we can properly map using the
	** correct virtual address associated with each DMA page.
	** w/o this association, we wouldn't have coherent DMA!
	** Access to the virtual address is what forces a two pass algorithm.
	*/
	coalesced = sba_coalesce_chunks(ioc, sglist, nents);

	/*
	** Program the I/O Pdir
	**
	** map the virtual addresses to the I/O Pdir
	** o dma_address will contain the pdir index
	** o dma_len will contain the number of bytes to map 
	** o address contains the virtual address.
	*/
	filled = sba_fill_pdir(ioc, sglist, nents);

#ifdef ASSERT_PDIR_SANITY
	if (sba_check_pdir(ioc,"Check after sba_map_sg()"))
	{
		sba_dump_sg(ioc, sglist, nents);
		panic("Check after sba_map_sg()\n");
	}
#endif

	spin_unlock_irqrestore(&ioc->res_lock, flags);

	ASSERT(coalesced == filled);
	DBG_RUN_SG("%s() DONE %d mappings\n", __FUNCTION__, filled);

	return filled;
}


/**
 * sba_unmap_sg - unmap Scatter/Gather list
 * @dev: instance of PCI owned by the driver that's asking.
 * @sglist:  array of buffer/length pairs
 * @nents:  number of entries in list
 * @direction:  R/W or both.
 *
 * See Documentation/DMA-mapping.txt
 */
static void 
sba_unmap_sg(struct pci_dev *dev, struct scatterlist *sglist, int nents, int direction)
{
	struct ioc *ioc;
#ifdef ASSERT_PDIR_SANITY
	unsigned long flags;
#endif

	DBG_RUN_SG("%s() START %d entries,  %p,%x\n",
		__FUNCTION__, nents, sg_virt_addr(sglist), sglist->length);

	ASSERT(dev->sysdata);
	ioc = GET_IOC(dev);
	ASSERT(ioc);

#ifdef CONFIG_PROC_FS
	ioc->usg_calls++;
#endif

#ifdef ASSERT_PDIR_SANITY
	spin_lock_irqsave(&ioc->res_lock, flags);
	sba_check_pdir(ioc,"Check before sba_unmap_sg()");
	spin_unlock_irqrestore(&ioc->res_lock, flags);
#endif

	while (sg_dma_len(sglist) && nents--) {

		sba_unmap_single(dev, sg_dma_address(sglist), sg_dma_len(sglist), direction);
#ifdef CONFIG_PROC_FS
		ioc->usg_pages += ((sg_dma_address(sglist) & ~IOVP_MASK) + sg_dma_len(sglist) + IOVP_SIZE - 1) >> PAGE_SHIFT;
		ioc->usingle_calls--;	/* kluge since call is unmap_sg() */
#endif
		++sglist;
	}

	DBG_RUN_SG("%s() DONE (nents %d)\n", __FUNCTION__,  nents);

#ifdef ASSERT_PDIR_SANITY
	spin_lock_irqsave(&ioc->res_lock, flags);
	sba_check_pdir(ioc,"Check after sba_unmap_sg()");
	spin_unlock_irqrestore(&ioc->res_lock, flags);
#endif

}

static struct pci_dma_ops sba_ops = {
	sba_dma_supported,
	sba_alloc_consistent,	/* allocate cacheable host mem */
	sba_free_consistent,	/* release cacheable host mem */
	sba_map_single,
	sba_unmap_single,
	sba_map_sg,
	sba_unmap_sg,
	NULL,			/* dma_sync_single */
	NULL			/* dma_sync_sg */
};


/**************************************************************************
**
**   SBA PAT PDC support
**
**   o call pdc_pat_cell_module()
**   o store ranges in PCI "resource" structures
**
**************************************************************************/

static void
sba_get_pat_resources(struct sba_device *sba_dev)
{
#if 0
/*
** TODO/REVISIT/FIXME: support for directed ranges requires calls to
**      PAT PDC to program the SBA/LBA directed range registers...this
**      burden may fall on the LBA code since it directly supports the
**      PCI subsystem. It's not clear yet. - ggg
*/
PAT_MOD(mod)->mod_info.mod_pages   = PAT_GET_MOD_PAGES(temp);
	FIXME : ???
PAT_MOD(mod)->mod_info.dvi         = PAT_GET_DVI(temp);
	Tells where the dvi bits are located in the address.
PAT_MOD(mod)->mod_info.ioc         = PAT_GET_IOC(temp);
	FIXME : ???
#endif
}


/**************************************************************
*
*   Initialization and claim
*
***************************************************************/
#define PIRANHA_ADDR_MASK	0x00160000UL /* bit 17,18,20 */
#define PIRANHA_ADDR_VAL	0x00060000UL /* bit 17,18 on */
static void *
sba_alloc_pdir(unsigned int pdir_size)
{
        unsigned long pdir_base;
	unsigned long pdir_order = get_order(pdir_size);

	pdir_base = __get_free_pages(GFP_KERNEL, pdir_order);
	if (NULL == (void *) pdir_base)
		panic("sba_ioc_init() could not allocate I/O Page Table\n");

	/* If this is not PA8700 (PCX-W2)
	**	OR newer than ver 2.2
	**	OR in a system that doesn't need VINDEX bits from SBA,
	**
	** then we aren't exposed to the HW bug.
	*/
	if ( ((boot_cpu_data.pdc.cpuid >> 5) & 0x7f) != 0x13
			|| (boot_cpu_data.pdc.versions > 0x202)
			|| (boot_cpu_data.pdc.capabilities & 0x08L) )
		return (void *) pdir_base;

	/*
	 * PA8700 (PCX-W2, aka piranha) silent data corruption fix
	 *
	 * An interaction between PA8700 CPU (Ver 2.2 or older) and
	 * Ike/Astro can cause silent data corruption. This is only
	 * a problem if the I/O PDIR is located in memory such that
	 * (little-endian)  bits 17 and 18 are on and bit 20 is off.
	 *
	 * Since the max IO Pdir size is 2MB, by cleverly allocating the
	 * right physical address, we can either avoid (IOPDIR <= 1MB)
	 * or minimize (2MB IO Pdir) the problem if we restrict the
	 * IO Pdir to a maximum size of 2MB-128K (1902K).
	 *
	 * Because we always allocate 2^N sized IO pdirs, either of the
	 * "bad" regions will be the last 128K if at all. That's easy
	 * to test for.
	 * 
	 */
	if (pdir_order <= (19-12)) {
		if (((virt_to_phys(pdir_base)+pdir_size-1) & PIRANHA_ADDR_MASK) == PIRANHA_ADDR_VAL) {
			/* allocate a new one on 512k alignment */
			unsigned long new_pdir = __get_free_pages(GFP_KERNEL, (19-12));
			/* release original */
			free_pages(pdir_base, pdir_order);

			pdir_base = new_pdir;

			/* release excess */
			while (pdir_order < (19-12)) {
				new_pdir += pdir_size;
				free_pages(new_pdir, pdir_order);
				pdir_order +=1;
				pdir_size <<=1;
			}
		}
	} else {
		/*
		** 1MB or 2MB Pdir
		** Needs to be aligned on an "odd" 1MB boundary.
		*/
		unsigned long new_pdir = __get_free_pages(GFP_KERNEL, pdir_order+1); /* 2 or 4MB */

		/* release original */
		free_pages( pdir_base, pdir_order);

		/* release first 1MB */
		free_pages(new_pdir, 20-12);

		pdir_base = new_pdir + 1024*1024;

		if (pdir_order > (20-12)) {
			/*
			** 2MB Pdir.
			**
			** Flag tells init_bitmap() to mark bad 128k as used
			** and to reduce the size by 128k.
			*/
			piranha_bad_128k = 1;

			new_pdir += 3*1024*1024;
			/* release last 1MB */
			free_pages(new_pdir, 20-12);

			/* release unusable 128KB */
			free_pages(new_pdir - 128*1024 , 17-12);

			pdir_size -= 128*1024;
		}
	}

	memset((void *) pdir_base, 0, pdir_size);
	return (void *) pdir_base;
}


static void
sba_ioc_init(struct parisc_device *sba, struct ioc *ioc, int ioc_num)
{
	/* lba_set_iregs() is in arch/parisc/kernel/lba_pci.c */
	extern void lba_set_iregs(struct parisc_device *, u32, u32);

	u32 iova_space_size, iova_space_mask;
	int pdir_size, iov_order;
	unsigned long physmem;
	struct parisc_device *lba;

	/*
	** Determine IOVA Space size from memory size.
	**
	** Ideally, PCI drivers would register the maximum number
	** of DMA they can have outstanding for each device they
	** own.  Next best thing would be to guess how much DMA
	** can be outstanding based on PCI Class/sub-class. Both
	** methods still require some "extra" to support PCI
	** Hot-Plug/Removal of PCI cards. (aka PCI OLARD).
	**
	** While we have 32-bits "IOVA" space, top two 2 bits are used
	** for DMA hints - ergo only 30 bits max.
	*/

	physmem = num_physpages << PAGE_SHIFT;
	iova_space_size = (u32) (physmem/(sba_mem_ratio*global_ioc_cnt));

	/* limit IOVA space size to 1MB-1GB */
	if (iova_space_size < 1024*1024) {
		iova_space_size = 1024*1024;
	}
#ifdef __LP64__
	else if (iova_space_size > 512*1024*1024) {
		iova_space_size = 512*1024*1024;
	}
#endif

	/*
	** iova space must be log2() in size.
	** thus, pdir/res_map will also be log2().
	** PIRANHA BUG: Exception is when IO Pdir is 2MB (gets reduced)
	*/
	iov_order = get_order(iova_space_size >> (IOVP_SHIFT-PAGE_SHIFT));
	ASSERT(iov_order <= (30 - IOVP_SHIFT));   /* iova_space_size <= 1GB */
	ASSERT(iov_order >= (20 - IOVP_SHIFT));   /* iova_space_size >= 1MB */
	iova_space_size = 1 << (iov_order + IOVP_SHIFT);

	ioc->pdir_size = pdir_size = (iova_space_size/IOVP_SIZE) * sizeof(u64);

	ASSERT(pdir_size < 4*1024*1024);   /* max pdir size == 2MB */

	/* Verify it's a power of two */
	ASSERT((1 << get_order(pdir_size)) == (pdir_size >> PAGE_SHIFT));

	DBG_INIT("%s() hpa 0x%lx mem %dMB IOV %dMB (%d bits) PDIR size 0x%0x\n",
		__FUNCTION__, ioc->ioc_hpa, (int) (physmem>>20),
		iova_space_size>>20, iov_order + PAGE_SHIFT, pdir_size);

	/* FIXME : DMA HINTs not used */
	ioc->hint_shift_pdir = iov_order + PAGE_SHIFT;
	ioc->hint_mask_pdir = ~(0x3 << (iov_order + PAGE_SHIFT));

	ioc->pdir_base = sba_alloc_pdir(pdir_size);

	DBG_INIT("%s() pdir %p size %x hint_shift_pdir %x hint_mask_pdir %lx\n",
		__FUNCTION__, ioc->pdir_base, pdir_size,
		ioc->hint_shift_pdir, ioc->hint_mask_pdir);

	ASSERT((((unsigned long) ioc->pdir_base) & PAGE_MASK) == (unsigned long) ioc->pdir_base);
	WRITE_REG64(virt_to_phys(ioc->pdir_base), ioc->ioc_hpa + IOC_PDIR_BASE);

	/* build IMASK for IOC and Elroy */
	iova_space_mask =  0xffffffff;
	iova_space_mask <<= (iov_order + PAGE_SHIFT);

	/*
	** On C3000 w/512MB mem, HP-UX 10.20 reports:
	**     ibase=0, imask=0xFE000000, size=0x2000000.
	*/
	ioc->ibase = IOC_IOVA_SPACE_BASE | 1;	/* bit 0 == enable bit */
	ioc->imask = iova_space_mask;	/* save it */

	DBG_INIT("%s() IOV base 0x%lx mask 0x%0lx\n",
		__FUNCTION__, ioc->ibase, ioc->imask);

	/*
	** FIXME: Hint registers are programmed with default hint
	** values during boot, so hints should be sane even if we
	** can't reprogram them the way drivers want.
	*/

	/*
	** setup Elroy IBASE/IMASK registers as well.
	*/
	for (lba = sba->child; lba; lba = lba->sibling) {
		int rope_num = (lba->hpa >> 13) & 0xf;
		if (rope_num >> 3 == ioc_num)
			lba_set_iregs(lba, ioc->ibase, ioc->imask);
	}

	/*
	** Program the IOC's ibase and enable IOVA translation
	*/
	WRITE_REG(ioc->ibase, ioc->ioc_hpa+IOC_IBASE);
	WRITE_REG(ioc->imask, ioc->ioc_hpa+IOC_IMASK);

	/* Set I/O PDIR Page size to 4K */
	WRITE_REG(0, ioc->ioc_hpa+IOC_TCNFG);

	/*
	** Clear I/O TLB of any possible entries.
	** (Yes. This is a bit paranoid...but so what)
	*/
	WRITE_REG(0 | 31, ioc->ioc_hpa+IOC_PCOM);

	DBG_INIT("%s() DONE\n", __FUNCTION__);
}



/**************************************************************************
**
**   SBA initialization code (HW and SW)
**
**   o identify SBA chip itself
**   o initialize SBA chip modes (HardFail)
**   o initialize SBA chip modes (HardFail)
**   o FIXME: initialize DMA hints for reasonable defaults
**
**************************************************************************/

static void
sba_hw_init(struct sba_device *sba_dev)
{ 
	int i;
	int num_ioc;
	u64 ioc_ctl;

	ioc_ctl = READ_REG(sba_dev->sba_hpa+IOC_CTRL);
	DBG_INIT("%s() hpa 0x%lx ioc_ctl 0x%Lx ->",
		__FUNCTION__, sba_dev->sba_hpa, ioc_ctl);
	ioc_ctl &= ~(IOC_CTRL_RM | IOC_CTRL_NC | IOC_CTRL_CE);
	ioc_ctl |= IOC_CTRL_TC;	/* Astro: firmware enables this */

	WRITE_REG(ioc_ctl, sba_dev->sba_hpa+IOC_CTRL);

#ifdef DEBUG_SBA_INIT
	ioc_ctl = READ_REG64(sba_dev->sba_hpa+IOC_CTRL);
	DBG_INIT(" 0x%Lx\n", ioc_ctl);
#endif

	if (IS_ASTRO(sba_dev->iodc)) {
		/* PAT_PDC (L-class) also reports the same goofy base */
		sba_dev->ioc[0].ioc_hpa = ASTRO_IOC_OFFSET;
		num_ioc = 1;
	} else {
		sba_dev->ioc[0].ioc_hpa = sba_dev->ioc[1].ioc_hpa = 0;
		num_ioc = 2;
	}

	sba_dev->num_ioc = num_ioc;
	for (i = 0; i < num_ioc; i++) {
		sba_dev->ioc[i].ioc_hpa += sba_dev->sba_hpa + IKE_IOC_OFFSET(i);

		/*
		** Make sure the box crashes if we get any errors on a rope.
		*/
		WRITE_REG(HF_ENABLE, sba_dev->ioc[i].ioc_hpa + ROPE0_CTL);
		WRITE_REG(HF_ENABLE, sba_dev->ioc[i].ioc_hpa + ROPE1_CTL);
		WRITE_REG(HF_ENABLE, sba_dev->ioc[i].ioc_hpa + ROPE2_CTL);
		WRITE_REG(HF_ENABLE, sba_dev->ioc[i].ioc_hpa + ROPE3_CTL);
		WRITE_REG(HF_ENABLE, sba_dev->ioc[i].ioc_hpa + ROPE4_CTL);
		WRITE_REG(HF_ENABLE, sba_dev->ioc[i].ioc_hpa + ROPE5_CTL);
		WRITE_REG(HF_ENABLE, sba_dev->ioc[i].ioc_hpa + ROPE6_CTL);
		WRITE_REG(HF_ENABLE, sba_dev->ioc[i].ioc_hpa + ROPE7_CTL);

		/* flush out the writes */
		READ_REG(sba_dev->ioc[i].ioc_hpa + ROPE7_CTL);

		sba_ioc_init(sba_dev->dev, &(sba_dev->ioc[i]), i);
	}
}

static void
sba_common_init(struct sba_device *sba_dev)
{
	int i;

	/* add this one to the head of the list (order doesn't matter)
	** This will be useful for debugging - especially if we get coredumps
	*/
	sba_dev->next = sba_list;
	sba_list = sba_dev;

	for(i=0; i< sba_dev->num_ioc; i++) {
		int res_size;
#ifdef DEBUG_DMB_TRAP
		extern void iterate_pages(unsigned long , unsigned long ,
					  void (*)(pte_t * , unsigned long),
					  unsigned long );
		void set_data_memory_break(pte_t * , unsigned long);
#endif
		/* resource map size dictated by pdir_size */
		res_size = sba_dev->ioc[i].pdir_size/sizeof(u64); /* entries */

		/* Second part of PIRANHA BUG */
		if (piranha_bad_128k) {
			res_size -= (128*1024)/sizeof(u64);
		}

		res_size >>= 3;  /* convert bit count to byte count */
		DBG_INIT("%s() res_size 0x%x\n",
			__FUNCTION__, res_size);

		sba_dev->ioc[i].res_size = res_size;
		sba_dev->ioc[i].res_map = (char *) __get_free_pages(GFP_KERNEL, get_order(res_size));

#ifdef DEBUG_DMB_TRAP
		iterate_pages( sba_dev->ioc[i].res_map, res_size,
				set_data_memory_break, 0);
#endif

		if (NULL == sba_dev->ioc[i].res_map)
		{
			panic(__FILE__ ":%s() could not allocate resource map\n", __FUNCTION__ );
		}

		memset(sba_dev->ioc[i].res_map, 0, res_size);
		/* next available IOVP - circular search */
		sba_dev->ioc[i].res_hint = (unsigned long *)
				&(sba_dev->ioc[i].res_map[L1_CACHE_BYTES]);

#ifdef ASSERT_PDIR_SANITY
		/* Mark first bit busy - ie no IOVA 0 */
		sba_dev->ioc[i].res_map[0] = 0x80;
		sba_dev->ioc[i].pdir_base[0] = 0xeeffc0addbba0080ULL;
#endif

		/* Third (and last) part of PIRANHA BUG */
		if (piranha_bad_128k) {
			/* region from +1408K to +1536 is un-usable. */

			int idx_start = (1408*1024/sizeof(u64)) >> 3;
			int idx_end   = (1536*1024/sizeof(u64)) >> 3;
			long *p_start = (long *) &(sba_dev->ioc[i].res_map[idx_start]);
			long *p_end   = (long *) &(sba_dev->ioc[i].res_map[idx_end]);

			/* mark that part of the io pdir busy */
			while (p_start < p_end)
				*p_start++ = -1;
				
		}

#ifdef DEBUG_DMB_TRAP
		iterate_pages( sba_dev->ioc[i].res_map, res_size,
				set_data_memory_break, 0);
		iterate_pages( sba_dev->ioc[i].pdir_base, sba_dev->ioc[i].pdir_size,
				set_data_memory_break, 0);
#endif

		DBG_INIT("%s() %d res_map %x %p\n",
			__FUNCTION__, i, res_size, sba_dev->ioc[i].res_map);
	}

	sba_dev->sba_lock = SPIN_LOCK_UNLOCKED;
	ioc_needs_fdc = boot_cpu_data.pdc.capabilities & PDC_MODEL_IOPDIR_FDC;

#ifdef DEBUG_SBA_INIT
	/*
	 * If the PDC_MODEL capabilities has Non-coherent IO-PDIR bit set
	 * (bit #61, big endian), we have to flush and sync every time
	 * IO-PDIR is changed in Ike/Astro.
	 */
	if (boot_cpu_data.pdc.capabilities & PDC_MODEL_IOPDIR_FDC) {
		printk(KERN_INFO MODULE_NAME " FDC/SYNC required.\n");
	} else {
		printk(KERN_INFO MODULE_NAME " IOC has cache coherent PDIR.\n");
	}
#endif
}

#ifdef CONFIG_PROC_FS
static int sba_proc_info(char *buf, char **start, off_t offset, int len)
{
	struct sba_device *sba_dev = sba_list;
	struct ioc *ioc = &sba_dev->ioc[0];	/* FIXME: Multi-IOC support! */
	int total_pages = (int) (ioc->res_size << 3); /* 8 bits per byte */
	unsigned long i = 0, avg = 0, min, max;

	sprintf(buf, "%s rev %d.%d\n",
		sba_dev->name,
		(sba_dev->hw_rev & 0x7) + 1,
		(sba_dev->hw_rev & 0x18) >> 3
		);
	sprintf(buf, "%sIO PDIR size    : %d bytes (%d entries)\n",
		buf,
		(int) ((ioc->res_size << 3) * sizeof(u64)), /* 8 bits/byte */
		total_pages);

	sprintf(buf, "%sIO PDIR entries : %ld free  %ld used (%d%%)\n", buf,
		total_pages - ioc->used_pages, ioc->used_pages,
		(int) (ioc->used_pages * 100 / total_pages));
	
	sprintf(buf, "%sResource bitmap : %d bytes (%d pages)\n", 
		buf, ioc->res_size, ioc->res_size << 3);   /* 8 bits per byte */

	min = max = ioc->avg_search[0];
	for (i = 0; i < SBA_SEARCH_SAMPLE; i++) {
		avg += ioc->avg_search[i];
		if (ioc->avg_search[i] > max) max = ioc->avg_search[i];
		if (ioc->avg_search[i] < min) min = ioc->avg_search[i];
	}
	avg /= SBA_SEARCH_SAMPLE;
	sprintf(buf, "%s  Bitmap search : %ld/%ld/%ld (min/avg/max CPU Cycles)\n",
		buf, min, avg, max);

	sprintf(buf, "%spci_map_single(): %12ld calls  %12ld pages (avg %d/1000)\n",
		buf, ioc->msingle_calls, ioc->msingle_pages,
		(int) ((ioc->msingle_pages * 1000)/ioc->msingle_calls));

	/* KLUGE - unmap_sg calls unmap_single for each mapped page */
	min = ioc->usingle_calls;
	max = ioc->usingle_pages - ioc->usg_pages;
	sprintf(buf, "%spci_unmap_single: %12ld calls  %12ld pages (avg %d/1000)\n",
		buf, min, max,
		(int) ((max * 1000)/min));

	sprintf(buf, "%spci_map_sg()    : %12ld calls  %12ld pages (avg %d/1000)\n",
		buf, ioc->msg_calls, ioc->msg_pages,
		(int) ((ioc->msg_pages * 1000)/ioc->msg_calls));

	sprintf(buf, "%spci_unmap_sg()  : %12ld calls  %12ld pages (avg %d/1000)\n",
		buf, ioc->usg_calls, ioc->usg_pages,
		(int) ((ioc->usg_pages * 1000)/ioc->usg_calls));

	return strlen(buf);
}

#if 0
/* XXX too much output - exceeds 4k limit and needs to be re-written */
static int
sba_resource_map(char *buf, char **start, off_t offset, int len)
{
	struct sba_device *sba_dev = sba_list;
	struct ioc *ioc = &sba_dev->ioc[0];	/* FIXME: Mutli-IOC suppoer! */
	unsigned int *res_ptr = (unsigned int *)ioc->res_map;
	int i;

	buf[0] = '\0';
	for(i = 0; i < (ioc->res_size / sizeof(unsigned int)); ++i, ++res_ptr) {
		if ((i & 7) == 0)
		    strcat(buf,"\n   ");
		sprintf(buf, "%s %08x", buf, *res_ptr);
	}
	strcat(buf, "\n");

	return strlen(buf);
}
#endif /* 0 */
#endif /* CONFIG_PROC_FS */

static struct parisc_device_id sba_tbl[] = {
	{ HPHW_IOA, HVERSION_REV_ANY_ID, ASTRO_RUNWAY_PORT, 0xb },
	{ HPHW_BCPORT, HVERSION_REV_ANY_ID, IKE_MERCED_PORT, 0xc },
	{ HPHW_BCPORT, HVERSION_REV_ANY_ID, REO_MERCED_PORT, 0xc },
	{ HPHW_BCPORT, HVERSION_REV_ANY_ID, REOG_MERCED_PORT, 0xc },
/* These two entries commented out because we don't find them in a
 * buswalk yet.  If/when we do, they would cause us to think we had
 * many more SBAs then we really do.
 *	{ HPHW_BCPORT, HVERSION_REV_ANY_ID, ASTRO_ROPES_PORT, 0xc },
 *	{ HPHW_BCPORT, HVERSION_REV_ANY_ID, IKE_ROPES_PORT, 0xc },
 */
	{ 0, }
};

int sba_driver_callback(struct parisc_device *);

static struct parisc_driver sba_driver = {
	name:		MODULE_NAME,
	id_table:	sba_tbl,
	probe:		sba_driver_callback,
};

/*
** Determine if lba should claim this chip (return 0) or not (return 1).
** If so, initialize the chip and tell other partners in crime they
** have work to do.
*/
int
sba_driver_callback(struct parisc_device *dev)
{
	struct sba_device *sba_dev;
	u32 func_class;
	int i;
	char *version;

#ifdef DEBUG_SBA_INIT
	sba_dump_ranges(dev->hpa);
#endif

	/* Read HW Rev First */
	func_class = READ_REG(dev->hpa + SBA_FCLASS);

	if (IS_ASTRO(&dev->id)) {
		unsigned long fclass;
		static char astro_rev[]="Astro ?.?";

		/* Astro is broken...Read HW Rev First */
		fclass = READ_REG(dev->hpa);

		astro_rev[6] = '1' + (char) (fclass & 0x7);
		astro_rev[8] = '0' + (char) ((fclass & 0x18) >> 3);
		version = astro_rev;

	} else if (IS_IKE(&dev->id)) {
		static char ike_rev[]="Ike rev ?";

		ike_rev[8] = '0' + (char) (func_class & 0xff);
		version = ike_rev;
	} else {
		static char reo_rev[]="REO rev ?";

		reo_rev[8] = '0' + (char) (func_class & 0xff);
		version = reo_rev;
	}

	if (!global_ioc_cnt) {
		global_ioc_cnt = count_parisc_driver(&sba_driver);

		/* Only Astro has one IOC per SBA */
		if (!IS_ASTRO(&dev->id))
			global_ioc_cnt *= 2;
	}

	printk(KERN_INFO "%s found %s at 0x%lx\n",
		MODULE_NAME, version, dev->hpa);

#ifdef DEBUG_SBA_INIT
	sba_dump_tlb(dev->hpa);
#endif

	sba_dev = kmalloc(sizeof(struct sba_device), GFP_KERNEL);
	if (NULL == sba_dev) {
		printk(KERN_ERR MODULE_NAME " - couldn't alloc sba_device\n");
		return(1);
	}

	dev->sysdata = (void *) sba_dev;
	memset(sba_dev, 0, sizeof(struct sba_device));

	for(i=0; i<MAX_IOC; i++)
		spin_lock_init(&(sba_dev->ioc[i].res_lock));

	sba_dev->dev = dev;
	sba_dev->hw_rev = func_class;
	sba_dev->iodc = &dev->id;
	sba_dev->name = dev->name;
	sba_dev->sba_hpa = dev->hpa;  /* faster access */

	sba_get_pat_resources(sba_dev);
	sba_hw_init(sba_dev);
	sba_common_init(sba_dev);

	hppa_dma_ops = &sba_ops;

#ifdef CONFIG_PROC_FS
	if (IS_ASTRO(&dev->id)) {
		create_proc_info_entry("Astro", 0, proc_runway_root, sba_proc_info);
	} else if (IS_IKE(&dev->id)) {
		create_proc_info_entry("Ike", 0, proc_runway_root, sba_proc_info);
	} else {
		create_proc_info_entry("Reo", 0, proc_runway_root, sba_proc_info);
	}
#if 0
	create_proc_info_entry("bitmap", 0, proc_runway_root, sba_resource_map);
#endif
#endif
	return 0;
}

/*
** One time initialization to let the world know the SBA was found.
** This is the only routine which is NOT static.
** Must be called exactly once before pci_init().
*/
void __init sba_init(void)
{
	register_parisc_driver(&sba_driver);
}


/**
 * sba_get_iommu - Assign the iommu pointer for the pci bus controller.
 * @dev: The parisc device.
 *
 * This function searches through the registerd IOMMU's and returns the
 * appropriate IOMMU data for the given parisc PCI controller.
 */
void * sba_get_iommu(struct parisc_device *pci_hba)
{
	struct sba_device *sba = (struct sba_device *) pci_hba->parent->sysdata;
	char t = pci_hba->parent->id.hw_type;
	int iocnum = (pci_hba->hw_path >> 3);	/* rope # */

	if ((t!=HPHW_IOA) && (t!=HPHW_BCPORT))
		BUG();

	return &(sba->ioc[iocnum]);
}
