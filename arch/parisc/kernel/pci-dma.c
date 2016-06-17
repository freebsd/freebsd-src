/*
** PARISC 1.1 Dynamic DMA mapping support.
** This implementation is for PA-RISC platforms that do not support
** I/O TLBs (aka DMA address translation hardware).
** See Documentation/DMA-mapping.txt for interface definitions.
**
**      (c) Copyright 1999,2000 Hewlett-Packard Company
**      (c) Copyright 2000 Grant Grundler
**	(c) Copyright 2000 Philipp Rumpf <prumpf@tux.org>
**      (c) Copyright 2000 John Marvin
**
** "leveraged" from 2.3.47: arch/ia64/kernel/pci-dma.c.
** (I assume it's from David Mosberger-Tang but there was no Copyright)
**
** AFAIK, all PA7100LC and PA7300LC platforms can use this code.
**
** - ggg
*/

#include <linux/types.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <asm/uaccess.h>
#include <asm/pgalloc.h>

#include <asm/io.h>
#include <asm/page.h>	/* get_order */
#include <asm/dma.h>    /* for DMA_CHUNK_SIZE */

#include <linux/proc_fs.h>

static struct proc_dir_entry * proc_gsc_root = NULL;
static int pcxl_proc_info(char *buffer, char **start, off_t offset, int length);
static unsigned long pcxl_used_bytes = 0;
static unsigned long pcxl_used_pages = 0;

extern unsigned long pcxl_dma_start; /* Start of pcxl dma mapping area */
static spinlock_t   pcxl_res_lock;
static char    *pcxl_res_map;
static int     pcxl_res_hint;
static int     pcxl_res_size;

#ifdef DEBUG_PCXL_RESOURCE
#define DBG_RES(x...)	printk(x)
#else
#define DBG_RES(x...)
#endif


/*
** Dump a hex representation of the resource map.
*/

#ifdef DUMP_RESMAP
static
void dump_resmap(void)
{
	u_long *res_ptr = (unsigned long *)pcxl_res_map;
	u_long i = 0;

	printk("res_map: ");
	for(; i < (pcxl_res_size / sizeof(unsigned long)); ++i, ++res_ptr)
		printk("%08lx ", *res_ptr);

	printk("\n");
}
#else
static inline void dump_resmap(void) {;}
#endif

static int pa11_dma_supported( struct pci_dev *dev, u64 mask)
{
	return 1;
}

static inline int map_pte_uncached(pte_t * pte,
		unsigned long vaddr,
		unsigned long size, unsigned long *paddr_ptr)
{
	unsigned long end;
	unsigned long orig_vaddr = vaddr;

	vaddr &= ~PMD_MASK;
	end = vaddr + size;
	if (end > PMD_SIZE)
		end = PMD_SIZE;
	do {
		if (!pte_none(*pte))
			printk(KERN_ERR "map_pte_uncached: page already exists\n");
		set_pte(pte, __mk_pte(*paddr_ptr, PAGE_KERNEL_UNC));
		pdtlb_kernel(orig_vaddr);
		vaddr += PAGE_SIZE;
		orig_vaddr += PAGE_SIZE;
		(*paddr_ptr) += PAGE_SIZE;
		pte++;
	} while (vaddr < end);
	return 0;
}

static inline int map_pmd_uncached(pmd_t * pmd, unsigned long vaddr,
		unsigned long size, unsigned long *paddr_ptr)
{
	unsigned long end;
	unsigned long orig_vaddr = vaddr;

	vaddr &= ~PGDIR_MASK;
	end = vaddr + size;
	if (end > PGDIR_SIZE)
		end = PGDIR_SIZE;
	do {
		pte_t * pte = pte_alloc(NULL, pmd, vaddr);
		if (!pte)
			return -ENOMEM;
		if (map_pte_uncached(pte, orig_vaddr, end - vaddr, paddr_ptr))
			return -ENOMEM;
		vaddr = (vaddr + PMD_SIZE) & PMD_MASK;
		orig_vaddr += PMD_SIZE;
		pmd++;
	} while (vaddr < end);
	return 0;
}

static inline int map_uncached_pages(unsigned long vaddr, unsigned long size,
		unsigned long paddr)
{
	pgd_t * dir;
	unsigned long end = vaddr + size;

	dir = pgd_offset_k(vaddr);
	do {
		pmd_t *pmd;
		
		pmd = pmd_alloc(NULL, dir, vaddr);
		if (!pmd)
			return -ENOMEM;
		if (map_pmd_uncached(pmd, vaddr, end - vaddr, &paddr))
			return -ENOMEM;
		vaddr = vaddr + PGDIR_SIZE;
		dir++;
	} while (vaddr && (vaddr < end));
	return 0;
}

static inline void unmap_uncached_pte(pmd_t * pmd, unsigned long vaddr,
		unsigned long size)
{
	pte_t * pte;
	unsigned long end;
	unsigned long orig_vaddr = vaddr;

	if (pmd_none(*pmd))
		return;
	if (pmd_bad(*pmd)) {
		pmd_ERROR(*pmd);
		pmd_clear(pmd);
		return;
	}
	pte = pte_offset(pmd, vaddr);
	vaddr &= ~PMD_MASK;
	end = vaddr + size;
	if (end > PMD_SIZE)
		end = PMD_SIZE;
	do {
		pte_t page = *pte;
		pte_clear(pte);
		pdtlb_kernel(orig_vaddr);
		vaddr += PAGE_SIZE;
		orig_vaddr += PAGE_SIZE;
		pte++;
		if (pte_none(page) || pte_present(page))
			continue;
		printk(KERN_CRIT "Whee.. Swapped out page in kernel page table\n");
	} while (vaddr < end);
}

static inline void unmap_uncached_pmd(pgd_t * dir, unsigned long vaddr,
		unsigned long size)
{
	pmd_t * pmd;
	unsigned long end;
	unsigned long orig_vaddr = vaddr;

	if (pgd_none(*dir))
		return;
	if (pgd_bad(*dir)) {
		pgd_ERROR(*dir);
		pgd_clear(dir);
		return;
	}
	pmd = pmd_offset(dir, vaddr);
	vaddr &= ~PGDIR_MASK;
	end = vaddr + size;
	if (end > PGDIR_SIZE)
		end = PGDIR_SIZE;
	do {
		unmap_uncached_pte(pmd, orig_vaddr, end - vaddr);
		vaddr = (vaddr + PMD_SIZE) & PMD_MASK;
		orig_vaddr += PMD_SIZE;
		pmd++;
	} while (vaddr < end);
}

static void unmap_uncached_pages(unsigned long vaddr, unsigned long size)
{
	pgd_t * dir;
	unsigned long end = vaddr + size;

	dir = pgd_offset_k(vaddr);
	do {
		unmap_uncached_pmd(dir, vaddr, end - vaddr);
		vaddr = vaddr + PGDIR_SIZE;
		dir++;
	} while (vaddr && (vaddr < end));
}

#define PCXL_SEARCH_LOOP(idx, mask, size)  \
       for(; res_ptr < res_end; ++res_ptr) \
       { \
               if(0 == ((*res_ptr) & mask)) { \
                       *res_ptr |= mask; \
		       idx = (int)((u_long)res_ptr - (u_long)pcxl_res_map); \
		       pcxl_res_hint = idx + (size >> 3); \
                       goto resource_found; \
               } \
       }

#define PCXL_FIND_FREE_MAPPING(idx, mask, size)  { \
       u##size *res_ptr = (u##size *)&(pcxl_res_map[pcxl_res_hint & ~((size >> 3) - 1)]); \
       u##size *res_end = (u##size *)&pcxl_res_map[pcxl_res_size]; \
       PCXL_SEARCH_LOOP(idx, mask, size); \
       res_ptr = (u##size *)&pcxl_res_map[0]; \
       PCXL_SEARCH_LOOP(idx, mask, size); \
}

unsigned long
pcxl_alloc_range(size_t size)
{
	int res_idx;
	u_long mask, flags;
	unsigned int pages_needed = size >> PAGE_SHIFT;

	ASSERT(pages_needed);
	ASSERT((pages_needed * PAGE_SIZE) < DMA_CHUNK_SIZE);
	ASSERT(pages_needed < (BITS_PER_LONG - PAGE_SHIFT));

	mask = (u_long) -1L;
 	mask >>= BITS_PER_LONG - pages_needed;

	DBG_RES("pcxl_alloc_range() size: %d pages_needed %d pages_mask 0x%08lx\n", 
		size, pages_needed, mask);

	spin_lock_irqsave(&pcxl_res_lock, flags);

	if(pages_needed <= 8) {
		PCXL_FIND_FREE_MAPPING(res_idx, mask, 8);
	} else if(pages_needed <= 16) {
		PCXL_FIND_FREE_MAPPING(res_idx, mask, 16);
	} else if(pages_needed <= 32) {
		PCXL_FIND_FREE_MAPPING(res_idx, mask, 32);
	} else {
		panic(__FILE__ ": pcxl_alloc_range() Too many pages to map.\n");
	}

	dump_resmap();
	panic(__FILE__ ": pcxl_alloc_range() out of dma mapping resources\n");
	
resource_found:
	
	DBG_RES("pcxl_alloc_range() res_idx %d mask 0x%08lx res_hint: %d\n",
		res_idx, mask, pcxl_res_hint);

	pcxl_used_pages += pages_needed;
	pcxl_used_bytes += ((pages_needed >> 3) ? (pages_needed >> 3) : 1);

	spin_unlock_irqrestore(&pcxl_res_lock, flags);

	dump_resmap();

	/* 
	** return the corresponding vaddr in the pcxl dma map
	*/
	return (pcxl_dma_start + (res_idx << (PAGE_SHIFT + 3)));
}

#define PCXL_FREE_MAPPINGS(idx, m, size) \
		u##size *res_ptr = (u##size *)&(pcxl_res_map[(idx) + (((size >> 3) - 1) & (~((size >> 3) - 1)))]); \
		ASSERT((*res_ptr & m) == m); \
		*res_ptr &= ~m;

/*
** clear bits in the pcxl resource map
*/
static void
pcxl_free_range(unsigned long vaddr, size_t size)
{
	u_long mask, flags;
	unsigned int res_idx = (vaddr - pcxl_dma_start) >> (PAGE_SHIFT + 3);
	unsigned int pages_mapped = size >> PAGE_SHIFT;

	ASSERT(pages_mapped);
	ASSERT((pages_mapped * PAGE_SIZE) < DMA_CHUNK_SIZE);
	ASSERT(pages_mapped < (BITS_PER_LONG - PAGE_SHIFT));

	mask = (u_long) -1L;
 	mask >>= BITS_PER_LONG - pages_mapped;

	DBG_RES("pcxl_free_range() res_idx: %d size: %d pages_mapped %d mask 0x%08lx\n", 
		res_idx, size, pages_mapped, mask);

	spin_lock_irqsave(&pcxl_res_lock, flags);

	if(pages_mapped <= 8) {
		PCXL_FREE_MAPPINGS(res_idx, mask, 8);
	} else if(pages_mapped <= 16) {
		PCXL_FREE_MAPPINGS(res_idx, mask, 16);
	} else if(pages_mapped <= 32) {
		PCXL_FREE_MAPPINGS(res_idx, mask, 32);
	} else {
		panic(__FILE__ ": pcxl_free_range() Too many pages to unmap.\n");
	}
	
	pcxl_used_pages -= (pages_mapped ? pages_mapped : 1);
	pcxl_used_bytes -= ((pages_mapped >> 3) ? (pages_mapped >> 3) : 1);

	spin_unlock_irqrestore(&pcxl_res_lock, flags);

	dump_resmap();
}

static int __init
pcxl_dma_init(void)
{
    if (pcxl_dma_start == 0)
	return 0;

    spin_lock_init(&pcxl_res_lock);
    pcxl_res_size = PCXL_DMA_MAP_SIZE >> (PAGE_SHIFT + 3);
    pcxl_res_hint = 0;
    pcxl_res_map = (char *)__get_free_pages(GFP_KERNEL,
					    get_order(pcxl_res_size));

    proc_gsc_root = proc_mkdir("gsc", 0);
    create_proc_info_entry("dino", 0, proc_gsc_root, pcxl_proc_info);
    return 0;
}

__initcall(pcxl_dma_init);

static void * pa11_dma_alloc_consistent (struct pci_dev *hwdev, size_t size, dma_addr_t *dma_handle)
{
	unsigned long vaddr;
	unsigned long paddr;
	int order;

	order = get_order(size);
	size = 1 << (order + PAGE_SHIFT);
	vaddr = pcxl_alloc_range(size);
	paddr = __get_free_pages(GFP_ATOMIC, order);
	flush_kernel_dcache_range(paddr, size);
	paddr = __pa(paddr);
	map_uncached_pages(vaddr, size, paddr);
	*dma_handle = (dma_addr_t) paddr;

#if 0
/* This probably isn't needed to support EISA cards.
** ISA cards will certainly only support 24-bit DMA addressing.
** Not clear if we can, want, or need to support ISA.
*/
	if (!hwdev || hwdev->dma_mask != 0xffffffff)
		gfp |= GFP_DMA;
#endif
	return (void *)vaddr;
}

static void pa11_dma_free_consistent (struct pci_dev *hwdev, size_t size, void *vaddr, dma_addr_t dma_handle)
{
	int order;

	order = get_order(size);
	size = 1 << (order + PAGE_SHIFT);
	unmap_uncached_pages((unsigned long)vaddr, size);
	pcxl_free_range((unsigned long)vaddr, size);
	free_pages((unsigned long)__va(dma_handle), order);
}

static dma_addr_t pa11_dma_map_single(struct pci_dev *dev, void *addr, size_t size, int direction)
{
	if (direction == PCI_DMA_NONE) {
		printk(KERN_ERR "pa11_dma_map_single(PCI_DMA_NONE) called by %p\n", __builtin_return_address(0));
		BUG();
	}

	flush_kernel_dcache_range((unsigned long) addr, size);
	return virt_to_phys(addr);
}

static void pa11_dma_unmap_single(struct pci_dev *dev, dma_addr_t dma_handle, size_t size, int direction)
{
	if (direction == PCI_DMA_NONE) {
		printk(KERN_ERR "pa11_dma_unmap_single(PCI_DMA_NONE) called by %p\n", __builtin_return_address(0));
		BUG();
	}

	if (direction == PCI_DMA_TODEVICE)
	    return;

	/*
	 * For PCI_DMA_FROMDEVICE this flush is not necessary for the
	 * simple map/unmap case. However, it IS necessary if if
	 * pci_dma_sync_single has been called and the buffer reused.
	 */

	flush_kernel_dcache_range((unsigned long) phys_to_virt(dma_handle), size);
	return;
}

static int pa11_dma_map_sg(struct pci_dev *dev, struct scatterlist *sglist, int nents, int direction)
{
	int i;

	if (direction == PCI_DMA_NONE)
	    BUG();

	for (i = 0; i < nents; i++, sglist++ ) {
		sg_dma_address(sglist) = (dma_addr_t) virt_to_phys(sg_virt_addr(sglist));
		sg_dma_len(sglist) = sglist->length;
		flush_kernel_dcache_range((unsigned long)sg_virt_addr(sglist),
				sglist->length);
	}
	return nents;
}

static void pa11_dma_unmap_sg(struct pci_dev *dev, struct scatterlist *sglist, int nents, int direction)
{
	int i;

	if (direction == PCI_DMA_NONE)
	    BUG();

	if (direction == PCI_DMA_TODEVICE)
	    return;

	/* once we do combining we'll need to use phys_to_virt(sg_dma_address(sglist)) */

	for (i = 0; i < nents; i++, sglist++ )
		flush_kernel_dcache_range((unsigned long) sg_virt_addr(sglist), sglist->length);
	return;
}

static void pa11_dma_sync_single(struct pci_dev *dev, dma_addr_t dma_handle, size_t size, int direction)
{
	if (direction == PCI_DMA_NONE)
	    BUG();

	flush_kernel_dcache_range((unsigned long) phys_to_virt(dma_handle), size);
}

static void pa11_dma_sync_sg(struct pci_dev *dev, struct scatterlist *sglist, int nents, int direction)
{
	int i;

	/* once we do combining we'll need to use phys_to_virt(sg_dma_address(sglist)) */

	for (i = 0; i < nents; i++, sglist++ )
		flush_kernel_dcache_range((unsigned long) sg_virt_addr(sglist), sglist->length);
}

struct pci_dma_ops pcxl_dma_ops = {
	pa11_dma_supported,			/* dma_support */
	pa11_dma_alloc_consistent,
	pa11_dma_free_consistent,
	pa11_dma_map_single,			/* map_single */
	pa11_dma_unmap_single,			/* unmap_single */
	pa11_dma_map_sg,			/* map_sg */
	pa11_dma_unmap_sg,			/* unmap_sg */
	pa11_dma_sync_single,			/* dma_sync_single */
	pa11_dma_sync_sg			/* dma_sync_sg */
};

static void *fail_alloc_consistent(struct pci_dev *hwdev, size_t size,
		dma_addr_t *dma_handle)
{
	return NULL;
}

static void fail_free_consistent(struct pci_dev *dev, size_t size,
		void *vaddr, dma_addr_t iova)
{
	return;
}

struct pci_dma_ops pcx_dma_ops = {
	pa11_dma_supported,			/* dma_support */
	fail_alloc_consistent,
	fail_free_consistent,
	pa11_dma_map_single,			/* map_single */
	pa11_dma_unmap_single,			/* unmap_single */
	pa11_dma_map_sg,			/* map_sg */
	pa11_dma_unmap_sg,			/* unmap_sg */
	pa11_dma_sync_single,			/* dma_sync_single */
	pa11_dma_sync_sg			/* dma_sync_sg */
};


static int pcxl_proc_info(char *buf, char **start, off_t offset, int len)
{
	u_long i = 0;
	unsigned long *res_ptr = (u_long *)pcxl_res_map;
	unsigned long total_pages = pcxl_res_size << 3;        /* 8 bits per byte */

	sprintf(buf, "\nDMA Mapping Area size    : %d bytes (%d pages)\n",
		PCXL_DMA_MAP_SIZE,
		(pcxl_res_size << 3) ); /* 1 bit per page */
	
	sprintf(buf, "%sResource bitmap : %d bytes (%d pages)\n", 
		buf, pcxl_res_size, pcxl_res_size << 3);   /* 8 bits per byte */

	strcat(buf,  "     	  total:    free:    used:   % used:\n");
	sprintf(buf, "%sblocks  %8d %8ld %8ld %8ld%%\n", buf, pcxl_res_size,
		pcxl_res_size - pcxl_used_bytes, pcxl_used_bytes,
		(pcxl_used_bytes * 100) / pcxl_res_size);

	sprintf(buf, "%spages   %8ld %8ld %8ld %8ld%%\n", buf, total_pages,
		total_pages - pcxl_used_pages, pcxl_used_pages,
		(pcxl_used_pages * 100 / total_pages));
	
	strcat(buf, "\nResource bitmap:");

	for(; i < (pcxl_res_size / sizeof(u_long)); ++i, ++res_ptr) {
		if ((i & 7) == 0)
		    strcat(buf,"\n   ");
		sprintf(buf, "%s %08lx", buf, *res_ptr);
	}
	strcat(buf, "\n");
	return strlen(buf);
}

