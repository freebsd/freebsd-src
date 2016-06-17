/* $Id: iommu.c,v 1.21 2001/02/13 01:16:43 davem Exp $
 * iommu.c:  IOMMU specific routines for memory management.
 *
 * Copyright (C) 1995 David S. Miller  (davem@caip.rutgers.edu)
 * Copyright (C) 1995 Pete Zaitcev
 * Copyright (C) 1996 Eddie C. Dost    (ecd@skynet.be)
 * Copyright (C) 1997,1998 Jakub Jelinek    (jj@sunsite.mff.cuni.cz)
 */
 
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/scatterlist.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/sbus.h>
#include <asm/io.h>
#include <asm/mxcc.h>
#include <asm/mbus.h>

/* srmmu.c */
extern int viking_mxcc_present;
BTFIXUPDEF_CALL(void, flush_page_for_dma, unsigned long)
#define flush_page_for_dma(page) BTFIXUP_CALL(flush_page_for_dma)(page)
extern int flush_page_for_dma_global;
static int viking_flush;
/* viking.S */
extern void viking_flush_page(unsigned long page);
extern void viking_mxcc_flush_page(unsigned long page);

#define IOPERM        (IOPTE_CACHE | IOPTE_WRITE | IOPTE_VALID)
#define MKIOPTE(phys) (((((phys)>>4) & IOPTE_PAGE) | IOPERM) & ~IOPTE_WAZ)

static inline void iommu_map_dvma_pages_for_iommu(struct iommu_struct *iommu)
{
	unsigned long kern_end = (unsigned long) high_memory;
	unsigned long first = PAGE_OFFSET;
	unsigned long last = kern_end;
	iopte_t *iopte = iommu->page_table;

	iopte += ((first - iommu->start) >> PAGE_SHIFT);
	while(first <= last) {
		*iopte++ = __iopte(MKIOPTE(__pa(first)));
		first += PAGE_SIZE;
	}
}

void __init
iommu_init(int iommund, struct sbus_bus *sbus)
{
	unsigned int impl, vers, ptsize;
	unsigned long tmp;
	struct iommu_struct *iommu;
	struct linux_prom_registers iommu_promregs[PROMREG_MAX];
	struct resource r;
	int i;

	iommu = kmalloc(sizeof(struct iommu_struct), GFP_ATOMIC);
	prom_getproperty(iommund, "reg", (void *) iommu_promregs,
			 sizeof(iommu_promregs));
	memset(&r, 0, sizeof(r));
	r.flags = iommu_promregs[0].which_io;
	r.start = iommu_promregs[0].phys_addr;
	iommu->regs = (struct iommu_regs *)
		sbus_ioremap(&r, 0, PAGE_SIZE * 3, "iommu_regs");
	if(!iommu->regs)
		panic("Cannot map IOMMU registers.");
	impl = (iommu->regs->control & IOMMU_CTRL_IMPL) >> 28;
	vers = (iommu->regs->control & IOMMU_CTRL_VERS) >> 24;
	tmp = iommu->regs->control;
	tmp &= ~(IOMMU_CTRL_RNGE);
	switch(PAGE_OFFSET & 0xf0000000) {
	case 0xf0000000:
		tmp |= (IOMMU_RNGE_256MB | IOMMU_CTRL_ENAB);
		iommu->plow = iommu->start = 0xf0000000;
		break;
	case 0xe0000000:
		tmp |= (IOMMU_RNGE_512MB | IOMMU_CTRL_ENAB);
		iommu->plow = iommu->start = 0xe0000000;
		break;
	case 0xd0000000:
	case 0xc0000000:
		tmp |= (IOMMU_RNGE_1GB | IOMMU_CTRL_ENAB);
		iommu->plow = iommu->start = 0xc0000000;
		break;
	case 0xb0000000:
	case 0xa0000000:
	case 0x90000000:
	case 0x80000000:
		tmp |= (IOMMU_RNGE_2GB | IOMMU_CTRL_ENAB);
		iommu->plow = iommu->start = 0x80000000;
		break;
	}
	iommu->regs->control = tmp;
	iommu_invalidate(iommu->regs);
	iommu->end = 0xffffffff;

	/* Allocate IOMMU page table */
	ptsize = iommu->end - iommu->start + 1;
	ptsize = (ptsize >> PAGE_SHIFT) * sizeof(iopte_t);

	/* Stupid alignment constraints give me a headache. 
	   We need 256K or 512K or 1M or 2M area aligned to
           its size and current gfp will fortunately give
           it to us. */
	for (i = 6; i < 9; i++)
		if ((1 << (i + PAGE_SHIFT)) == ptsize)
			break;
        tmp = __get_free_pages(GFP_DMA, i);
	if (!tmp) {
		prom_printf("Could not allocate iopte of size 0x%08x\n", ptsize);
		prom_halt();
	}
	iommu->lowest = iommu->page_table = (iopte_t *)tmp;

	/* Initialize new table. */
	flush_cache_all();
	memset(iommu->page_table, 0, ptsize);
	iommu_map_dvma_pages_for_iommu(iommu);
	if(viking_mxcc_present) {
		unsigned long start = (unsigned long) iommu->page_table;
		unsigned long end = (start + ptsize);
		while(start < end) {
			viking_mxcc_flush_page(start);
			start += PAGE_SIZE;
		}
	} else if (viking_flush) {
		unsigned long start = (unsigned long) iommu->page_table;
		unsigned long end = (start + ptsize);
		while(start < end) {
			viking_flush_page(start);
			start += PAGE_SIZE;
		}
	}
	flush_tlb_all();
	iommu->regs->base = __pa((unsigned long) iommu->page_table) >> 4;
	iommu_invalidate(iommu->regs);

	sbus->iommu = iommu;
	printk("IOMMU: impl %d vers %d page table at %p of size %d bytes\n",
	       impl, vers, iommu->page_table, ptsize);
}

static __u32 iommu_get_scsi_one_noflush(char *vaddr, unsigned long len, struct sbus_bus *sbus)
{
	return (__u32)vaddr;
}

static __u32 iommu_get_scsi_one_gflush(char *vaddr, unsigned long len, struct sbus_bus *sbus)
{
	flush_page_for_dma(0);
	return (__u32)vaddr;
}

static __u32 iommu_get_scsi_one_pflush(char *vaddr, unsigned long len, struct sbus_bus *sbus)
{
	unsigned long page = ((unsigned long) vaddr) & PAGE_MASK;

	while(page < ((unsigned long)(vaddr + len))) {
		flush_page_for_dma(page);
		page += PAGE_SIZE;
	}
	return (__u32)vaddr;
}

static void iommu_get_scsi_sgl_noflush(struct scatterlist *sg, int sz, struct sbus_bus *sbus)
{
	while (sz != 0) {
		sz--;
		sg[sz].dvma_address = (__u32) (sg[sz].address);
		sg[sz].dvma_length = (__u32) (sg[sz].length);
	}
}

static void iommu_get_scsi_sgl_gflush(struct scatterlist *sg, int sz, struct sbus_bus *sbus)
{
	flush_page_for_dma(0);
	while (sz != 0) {
		sz--;
		sg[sz].dvma_address = (__u32) (sg[sz].address);
		sg[sz].dvma_length = (__u32) (sg[sz].length);
	}
}

static void iommu_get_scsi_sgl_pflush(struct scatterlist *sg, int sz, struct sbus_bus *sbus)
{
	unsigned long page, oldpage = 0;

	while(sz != 0) {
		sz--;
		page = ((unsigned long) sg[sz].address) & PAGE_MASK;
		if (oldpage == page)
			page += PAGE_SIZE; /* We flushed that page already */
		while(page < (unsigned long)(sg[sz].address + sg[sz].length)) {
			flush_page_for_dma(page);
			page += PAGE_SIZE;
		}
		sg[sz].dvma_address = (__u32) (sg[sz].address);
		sg[sz].dvma_length = (__u32) (sg[sz].length);
		oldpage = page - PAGE_SIZE;
	}
}

static void iommu_release_scsi_one(__u32 vaddr, unsigned long len, struct sbus_bus *sbus)
{
}

static void iommu_release_scsi_sgl(struct scatterlist *sg, int sz, struct sbus_bus *sbus)
{
}

#ifdef CONFIG_SBUS
static void iommu_map_dma_area(unsigned long va, __u32 addr, int len)
{
	unsigned long page, end, ipte_cache;
	pgprot_t dvma_prot;
	struct iommu_struct *iommu = sbus_root->iommu;
	iopte_t *iopte = iommu->page_table;
	iopte_t *first;

	if(viking_mxcc_present || srmmu_modtype == HyperSparc) {
		dvma_prot = __pgprot(SRMMU_CACHE | SRMMU_ET_PTE | SRMMU_PRIV);
		ipte_cache = 1;
	} else {
		dvma_prot = __pgprot(SRMMU_ET_PTE | SRMMU_PRIV);
		ipte_cache = 0;
	}

	iopte += ((addr - iommu->start) >> PAGE_SHIFT);
	first = iopte;
	end = PAGE_ALIGN((addr + len));
	while(addr < end) {
		page = va;
		{
			pgd_t *pgdp;
			pmd_t *pmdp;
			pte_t *ptep;

			if (viking_mxcc_present)
				viking_mxcc_flush_page(page);
			else if (viking_flush)
				viking_flush_page(page);
			else
				__flush_page_to_ram(page);

			pgdp = pgd_offset(&init_mm, addr);
			pmdp = pmd_offset(pgdp, addr);
			ptep = pte_offset(pmdp, addr);

			set_pte(ptep, mk_pte(virt_to_page(page), dvma_prot));
			if (ipte_cache != 0) {
				iopte_val(*iopte++) = MKIOPTE(__pa(page));
			} else {
				iopte_val(*iopte++) =
					MKIOPTE(__pa(page)) & ~IOPTE_CACHE;
			}
		}
		addr += PAGE_SIZE;
		va += PAGE_SIZE;
	}
	/* P3: why do we need this?
	 *
	 * DAVEM: Because there are several aspects, none of which
	 *        are handled by a single interface.  Some cpus are
	 *        completely not I/O DMA coherent, and some have
	 *        virtually indexed caches.  The driver DMA flushing
	 *        methods handle the former case, but here during
	 *        IOMMU page table modifications, and usage of non-cacheable
	 *        cpu mappings of pages potentially in the cpu caches, we have
	 *        to handle the latter case as well.
	 */
	flush_cache_all();
	if(viking_mxcc_present) {
		unsigned long start = ((unsigned long) first) & PAGE_MASK;
		unsigned long end = PAGE_ALIGN(((unsigned long) iopte));
		while(start < end) {
			viking_mxcc_flush_page(start);
			start += PAGE_SIZE;
		}
	} else if(viking_flush) {
		unsigned long start = ((unsigned long) first) & PAGE_MASK;
		unsigned long end = PAGE_ALIGN(((unsigned long) iopte));
		while(start < end) {
			viking_flush_page(start);
			start += PAGE_SIZE;
		}
	}
	flush_tlb_all();
	iommu_invalidate(iommu->regs);
}

static void iommu_unmap_dma_area(unsigned long busa, int len)
{
	struct iommu_struct *iommu = sbus_root->iommu;
	iopte_t *iopte = iommu->page_table;
	unsigned long end;

	iopte += ((busa - iommu->start) >> PAGE_SHIFT);
	end = PAGE_ALIGN((busa + len));
	while (busa < end) {
		iopte_val(*iopte++) = 0;
		busa += PAGE_SIZE;
	}
	flush_tlb_all();	/* P3: Hmm... it would not hurt. */
	iommu_invalidate(iommu->regs);
}

static unsigned long iommu_translate_dvma(unsigned long busa)
{
	struct iommu_struct *iommu = sbus_root->iommu;
	iopte_t *iopte = iommu->page_table;
	unsigned long pa;

	iopte += ((busa - iommu->start) >> PAGE_SHIFT);
	pa = pte_val(*iopte);
	pa = (pa & 0xFFFFFFF0) << 4;		/* Loose higher bits of 36 */
	return pa + PAGE_OFFSET;
}
#endif

static char *iommu_lockarea(char *vaddr, unsigned long len)
{
	return vaddr;
}

static void iommu_unlockarea(char *vaddr, unsigned long len)
{
}

void __init ld_mmu_iommu(void)
{
	viking_flush = (BTFIXUPVAL_CALL(flush_page_for_dma) == (unsigned long)viking_flush_page);
	BTFIXUPSET_CALL(mmu_lockarea, iommu_lockarea, BTFIXUPCALL_RETO0);
	BTFIXUPSET_CALL(mmu_unlockarea, iommu_unlockarea, BTFIXUPCALL_NOP);

	if (!BTFIXUPVAL_CALL(flush_page_for_dma)) {
		/* IO coherent chip */
		BTFIXUPSET_CALL(mmu_get_scsi_one, iommu_get_scsi_one_noflush, BTFIXUPCALL_RETO0);
		BTFIXUPSET_CALL(mmu_get_scsi_sgl, iommu_get_scsi_sgl_noflush, BTFIXUPCALL_NORM);
	} else if (flush_page_for_dma_global) {
		/* flush_page_for_dma flushes everything, no matter of what page is it */
		BTFIXUPSET_CALL(mmu_get_scsi_one, iommu_get_scsi_one_gflush, BTFIXUPCALL_NORM);
		BTFIXUPSET_CALL(mmu_get_scsi_sgl, iommu_get_scsi_sgl_gflush, BTFIXUPCALL_NORM);
	} else {
		BTFIXUPSET_CALL(mmu_get_scsi_one, iommu_get_scsi_one_pflush, BTFIXUPCALL_NORM);
		BTFIXUPSET_CALL(mmu_get_scsi_sgl, iommu_get_scsi_sgl_pflush, BTFIXUPCALL_NORM);
	}
	BTFIXUPSET_CALL(mmu_release_scsi_one, iommu_release_scsi_one, BTFIXUPCALL_NOP);
	BTFIXUPSET_CALL(mmu_release_scsi_sgl, iommu_release_scsi_sgl, BTFIXUPCALL_NOP);

#ifdef CONFIG_SBUS
	BTFIXUPSET_CALL(mmu_map_dma_area, iommu_map_dma_area, BTFIXUPCALL_NORM);
	BTFIXUPSET_CALL(mmu_unmap_dma_area, iommu_unmap_dma_area, BTFIXUPCALL_NORM);
	BTFIXUPSET_CALL(mmu_translate_dvma, iommu_translate_dvma, BTFIXUPCALL_NORM);
#endif
}
