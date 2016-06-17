/* $Id: pgtable.h,v 1.109 2001/11/13 00:49:32 davem Exp $ */
#ifndef _SPARC_PGTABLE_H
#define _SPARC_PGTABLE_H

/*  asm-sparc/pgtable.h:  Defines and functions used to work
 *                        with Sparc page tables.
 *
 *  Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 *  Copyright (C) 1998 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 */

#include <linux/config.h>
#include <linux/spinlock.h>
#include <asm/asi.h>
#ifdef CONFIG_SUN4
#include <asm/pgtsun4.h>
#else
#include <asm/pgtsun4c.h>
#endif
#include <asm/pgtsrmmu.h>
#include <asm/vac-ops.h>
#include <asm/oplib.h>
#include <asm/sbus.h>
#include <asm/btfixup.h>
#include <asm/system.h>

#ifndef __ASSEMBLY__

extern void load_mmu(void);
extern unsigned long calc_highpages(void);
			       
BTFIXUPDEF_CALL(void, quick_kernel_fault, unsigned long)

#define quick_kernel_fault(addr) BTFIXUP_CALL(quick_kernel_fault)(addr)

/* Routines for data transfer buffers. */
BTFIXUPDEF_CALL(char *, mmu_lockarea, char *, unsigned long)
BTFIXUPDEF_CALL(void,   mmu_unlockarea, char *, unsigned long)

#define mmu_lockarea(vaddr,len) BTFIXUP_CALL(mmu_lockarea)(vaddr,len)
#define mmu_unlockarea(vaddr,len) BTFIXUP_CALL(mmu_unlockarea)(vaddr,len)

/* These are implementations for sbus_map_sg/sbus_unmap_sg... collapse later */
BTFIXUPDEF_CALL(__u32, mmu_get_scsi_one, char *, unsigned long, struct sbus_bus *sbus)
BTFIXUPDEF_CALL(void,  mmu_get_scsi_sgl, struct scatterlist *, int, struct sbus_bus *sbus)
BTFIXUPDEF_CALL(void,  mmu_release_scsi_one, __u32, unsigned long, struct sbus_bus *sbus)
BTFIXUPDEF_CALL(void,  mmu_release_scsi_sgl, struct scatterlist *, int, struct sbus_bus *sbus)

#define mmu_get_scsi_one(vaddr,len,sbus) BTFIXUP_CALL(mmu_get_scsi_one)(vaddr,len,sbus)
#define mmu_get_scsi_sgl(sg,sz,sbus) BTFIXUP_CALL(mmu_get_scsi_sgl)(sg,sz,sbus)
#define mmu_release_scsi_one(vaddr,len,sbus) BTFIXUP_CALL(mmu_release_scsi_one)(vaddr,len,sbus)
#define mmu_release_scsi_sgl(sg,sz,sbus) BTFIXUP_CALL(mmu_release_scsi_sgl)(sg,sz,sbus)

/*
 * mmu_map/unmap are provided by iommu/iounit; Invalid to call on IIep.
 */
BTFIXUPDEF_CALL(void,  mmu_map_dma_area, unsigned long va, __u32 addr, int len)
BTFIXUPDEF_CALL(unsigned long /*phys*/, mmu_translate_dvma, unsigned long busa)
BTFIXUPDEF_CALL(void,  mmu_unmap_dma_area, unsigned long busa, int len)

#define mmu_map_dma_area(va, ba,len) BTFIXUP_CALL(mmu_map_dma_area)(va,ba,len)
#define mmu_unmap_dma_area(ba,len) BTFIXUP_CALL(mmu_unmap_dma_area)(ba,len)
#define mmu_translate_dvma(ba)     BTFIXUP_CALL(mmu_translate_dvma)(ba)

BTFIXUPDEF_SIMM13(pmd_shift)
BTFIXUPDEF_SETHI(pmd_size)
BTFIXUPDEF_SETHI(pmd_mask)

extern unsigned int pmd_align(unsigned int addr) __attribute__((const));
extern __inline__ unsigned int pmd_align(unsigned int addr)
{
	return ((addr + ~BTFIXUP_SETHI(pmd_mask)) & BTFIXUP_SETHI(pmd_mask));
}

BTFIXUPDEF_SIMM13(pgdir_shift)
BTFIXUPDEF_SETHI(pgdir_size)
BTFIXUPDEF_SETHI(pgdir_mask)

extern unsigned int pgdir_align(unsigned int addr) __attribute__((const));
extern __inline__ unsigned int pgdir_align(unsigned int addr)
{
	return ((addr + ~BTFIXUP_SETHI(pgdir_mask)) & BTFIXUP_SETHI(pgdir_mask));
}

BTFIXUPDEF_SIMM13(ptrs_per_pte)
BTFIXUPDEF_SIMM13(ptrs_per_pmd)
BTFIXUPDEF_SIMM13(ptrs_per_pgd)
BTFIXUPDEF_SIMM13(user_ptrs_per_pgd)

#define VMALLOC_VMADDR(x) ((unsigned long)(x))

#define pte_ERROR(e)   __builtin_trap()
#define pmd_ERROR(e)   __builtin_trap()
#define pgd_ERROR(e)   __builtin_trap()

BTFIXUPDEF_INT(page_none)
BTFIXUPDEF_INT(page_shared)
BTFIXUPDEF_INT(page_copy)
BTFIXUPDEF_INT(page_readonly)
BTFIXUPDEF_INT(page_kernel)

#define PMD_SHIFT       	BTFIXUP_SIMM13(pmd_shift)
#define PMD_SIZE        	BTFIXUP_SETHI(pmd_size)
#define PMD_MASK        	BTFIXUP_SETHI(pmd_mask)
#define PMD_ALIGN(addr) 	pmd_align(addr)
#define PGDIR_SHIFT     	BTFIXUP_SIMM13(pgdir_shift)
#define PGDIR_SIZE      	BTFIXUP_SETHI(pgdir_size)
#define PGDIR_MASK      	BTFIXUP_SETHI(pgdir_mask)
#define PGDIR_ALIGN     	pgdir_align(addr)
#define PTRS_PER_PTE    	BTFIXUP_SIMM13(ptrs_per_pte)
#define PTRS_PER_PMD    	BTFIXUP_SIMM13(ptrs_per_pmd)
#define PTRS_PER_PGD    	BTFIXUP_SIMM13(ptrs_per_pgd)
#define USER_PTRS_PER_PGD	BTFIXUP_SIMM13(user_ptrs_per_pgd)
#define FIRST_USER_PGD_NR	0

#define PAGE_NONE      __pgprot(BTFIXUP_INT(page_none))
#define PAGE_SHARED    __pgprot(BTFIXUP_INT(page_shared))
#define PAGE_COPY      __pgprot(BTFIXUP_INT(page_copy))
#define PAGE_READONLY  __pgprot(BTFIXUP_INT(page_readonly))

extern unsigned long page_kernel;

#ifdef MODULE
#define PAGE_KERNEL	page_kernel
#else
#define PAGE_KERNEL    __pgprot(BTFIXUP_INT(page_kernel))
#endif

/* Top-level page directory */
extern pgd_t swapper_pg_dir[1024];

/* Page table for 0-4MB for everybody, on the Sparc this
 * holds the same as on the i386.
 */
extern pte_t pg0[1024];
extern pte_t pg1[1024];
extern pte_t pg2[1024];
extern pte_t pg3[1024];

extern unsigned long ptr_in_current_pgd;

/* Here is a trick, since mmap.c need the initializer elements for
 * protection_map[] to be constant at compile time, I set the following
 * to all zeros.  I set it to the real values after I link in the
 * appropriate MMU page table routines at boot time.
 */
#define __P000  __pgprot(0)
#define __P001  __pgprot(0)
#define __P010  __pgprot(0)
#define __P011  __pgprot(0)
#define __P100  __pgprot(0)
#define __P101  __pgprot(0)
#define __P110  __pgprot(0)
#define __P111  __pgprot(0)

#define __S000	__pgprot(0)
#define __S001	__pgprot(0)
#define __S010	__pgprot(0)
#define __S011	__pgprot(0)
#define __S100	__pgprot(0)
#define __S101	__pgprot(0)
#define __S110	__pgprot(0)
#define __S111	__pgprot(0)

extern int num_contexts;

/* First physical page can be anywhere, the following is needed so that
 * va-->pa and vice versa conversions work properly without performance
 * hit for all __pa()/__va() operations.
 */
extern unsigned long phys_base;

/*
 * BAD_PAGETABLE is used when we need a bogus page-table, while
 * BAD_PAGE is used for a bogus page.
 *
 * ZERO_PAGE is a global shared page that is always zero: used
 * for zero-mapped memory areas etc..
 */
extern pte_t * __bad_pagetable(void);
extern pte_t __bad_page(void);
extern unsigned long empty_zero_page;

#define BAD_PAGETABLE __bad_pagetable()
#define BAD_PAGE __bad_page()
#define ZERO_PAGE(vaddr) (mem_map + (((unsigned long)&empty_zero_page - PAGE_OFFSET + phys_base) >> PAGE_SHIFT))

/* number of bits that fit into a memory pointer */
#define BITS_PER_PTR      (8*sizeof(unsigned long))

/* to align the pointer to a pointer address */
#define PTR_MASK          (~(sizeof(void*)-1))

#define SIZEOF_PTR_LOG2   2

BTFIXUPDEF_CALL_CONST(unsigned long, pmd_page, pmd_t)
BTFIXUPDEF_CALL_CONST(unsigned long, pgd_page, pgd_t)

#define pmd_page(pmd) BTFIXUP_CALL(pmd_page)(pmd)
#define pgd_page(pgd) BTFIXUP_CALL(pgd_page)(pgd)

BTFIXUPDEF_SETHI(none_mask)
BTFIXUPDEF_CALL_CONST(int, pte_present, pte_t)
BTFIXUPDEF_CALL(void, pte_clear, pte_t *)

extern __inline__ int pte_none(pte_t pte)
{
	return !(pte_val(pte) & ~BTFIXUP_SETHI(none_mask));
}

#define pte_present(pte) BTFIXUP_CALL(pte_present)(pte)
#define pte_clear(pte) BTFIXUP_CALL(pte_clear)(pte)

BTFIXUPDEF_CALL_CONST(int, pmd_bad, pmd_t)
BTFIXUPDEF_CALL_CONST(int, pmd_present, pmd_t)
BTFIXUPDEF_CALL(void, pmd_clear, pmd_t *)

extern __inline__ int pmd_none(pmd_t pmd)
{
	return !(pmd_val(pmd) & ~BTFIXUP_SETHI(none_mask));
}

#define pmd_bad(pmd) BTFIXUP_CALL(pmd_bad)(pmd)
#define pmd_present(pmd) BTFIXUP_CALL(pmd_present)(pmd)
#define pmd_clear(pmd) BTFIXUP_CALL(pmd_clear)(pmd)

BTFIXUPDEF_CALL_CONST(int, pgd_none, pgd_t)
BTFIXUPDEF_CALL_CONST(int, pgd_bad, pgd_t)
BTFIXUPDEF_CALL_CONST(int, pgd_present, pgd_t)
BTFIXUPDEF_CALL(void, pgd_clear, pgd_t *)

#define pgd_none(pgd) BTFIXUP_CALL(pgd_none)(pgd)
#define pgd_bad(pgd) BTFIXUP_CALL(pgd_bad)(pgd)
#define pgd_present(pgd) BTFIXUP_CALL(pgd_present)(pgd)
#define pgd_clear(pgd) BTFIXUP_CALL(pgd_clear)(pgd)

/*
 * The following only work if pte_present() is true.
 * Undefined behaviour if not..
 */
BTFIXUPDEF_HALF(pte_writei)
BTFIXUPDEF_HALF(pte_dirtyi)
BTFIXUPDEF_HALF(pte_youngi)

extern int pte_write(pte_t pte) __attribute__((const));
extern __inline__ int pte_write(pte_t pte)
{
	return pte_val(pte) & BTFIXUP_HALF(pte_writei);
}

extern int pte_dirty(pte_t pte) __attribute__((const));
extern __inline__ int pte_dirty(pte_t pte)
{
	return pte_val(pte) & BTFIXUP_HALF(pte_dirtyi);
}

extern int pte_young(pte_t pte) __attribute__((const));
extern __inline__ int pte_young(pte_t pte)
{
	return pte_val(pte) & BTFIXUP_HALF(pte_youngi);
}

BTFIXUPDEF_HALF(pte_wrprotecti)
BTFIXUPDEF_HALF(pte_mkcleani)
BTFIXUPDEF_HALF(pte_mkoldi)

extern pte_t pte_wrprotect(pte_t pte) __attribute__((const));
extern __inline__ pte_t pte_wrprotect(pte_t pte)
{
	return __pte(pte_val(pte) & ~BTFIXUP_HALF(pte_wrprotecti));
}

extern pte_t pte_mkclean(pte_t pte) __attribute__((const));
extern __inline__ pte_t pte_mkclean(pte_t pte)
{
	return __pte(pte_val(pte) & ~BTFIXUP_HALF(pte_mkcleani));
}

extern pte_t pte_mkold(pte_t pte) __attribute__((const));
extern __inline__ pte_t pte_mkold(pte_t pte)
{
	return __pte(pte_val(pte) & ~BTFIXUP_HALF(pte_mkoldi));
}

BTFIXUPDEF_CALL_CONST(pte_t, pte_mkwrite, pte_t)
BTFIXUPDEF_CALL_CONST(pte_t, pte_mkdirty, pte_t)
BTFIXUPDEF_CALL_CONST(pte_t, pte_mkyoung, pte_t)

#define pte_mkwrite(pte) BTFIXUP_CALL(pte_mkwrite)(pte)
#define pte_mkdirty(pte) BTFIXUP_CALL(pte_mkdirty)(pte)
#define pte_mkyoung(pte) BTFIXUP_CALL(pte_mkyoung)(pte)

#define page_pte_prot(page, prot)	mk_pte(page, prot)
#define page_pte(page)			page_pte_prot(page, __pgprot(0))

BTFIXUPDEF_CALL(struct page *, pte_page, pte_t)
#define pte_page(pte) BTFIXUP_CALL(pte_page)(pte)

/*
 * Conversion functions: convert a page and protection to a page entry,
 * and a page entry and page directory to the page they refer to.
 */
BTFIXUPDEF_CALL_CONST(pte_t, mk_pte, struct page *, pgprot_t)

BTFIXUPDEF_CALL_CONST(pte_t, mk_pte_phys, unsigned long, pgprot_t)
BTFIXUPDEF_CALL_CONST(pte_t, mk_pte_io, unsigned long, pgprot_t, int)

#define mk_pte(page,pgprot) BTFIXUP_CALL(mk_pte)(page,pgprot)
#define mk_pte_phys(page,pgprot) BTFIXUP_CALL(mk_pte_phys)(page,pgprot)
#define mk_pte_io(page,pgprot,space) BTFIXUP_CALL(mk_pte_io)(page,pgprot,space)

BTFIXUPDEF_CALL(void, pgd_set, pgd_t *, pmd_t *)
BTFIXUPDEF_CALL(void, pmd_set, pmd_t *, pte_t *)

#define pgd_set(pgdp,pmdp) BTFIXUP_CALL(pgd_set)(pgdp,pmdp)
#define pmd_set(pmdp,ptep) BTFIXUP_CALL(pmd_set)(pmdp,ptep)

BTFIXUPDEF_INT(pte_modify_mask)

extern pte_t pte_modify(pte_t pte, pgprot_t newprot) __attribute__((const));
extern __inline__ pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	return __pte((pte_val(pte) & BTFIXUP_INT(pte_modify_mask)) |
		pgprot_val(newprot));
}

#define pgd_index(address) ((address) >> PGDIR_SHIFT)

/* to find an entry in a page-table-directory */
#define pgd_offset(mm, address) ((mm)->pgd + pgd_index(address))

/* to find an entry in a kernel page-table-directory */
#define pgd_offset_k(address) pgd_offset(&init_mm, address)

BTFIXUPDEF_CALL(pmd_t *, pmd_offset, pgd_t *, unsigned long)
BTFIXUPDEF_CALL(pte_t *, pte_offset, pmd_t *, unsigned long)

/* Find an entry in the second-level page table.. */
#define pmd_offset(dir,addr) BTFIXUP_CALL(pmd_offset)(dir,addr)

/* Find an entry in the third-level page table.. */ 
#define pte_offset(dir,addr) BTFIXUP_CALL(pte_offset)(dir,addr)

/* The permissions for pgprot_val to make a page mapped on the obio space */
extern unsigned int pg_iobits;

#define flush_icache_page(vma, pg)      do { } while(0)
#define flush_icache_user_range(vma,pg,adr,len)	do { } while (0)

/* Certain architectures need to do special things when pte's
 * within a page table are directly modified.  Thus, the following
 * hook is made available.
 */

BTFIXUPDEF_CALL(void, set_pte, pte_t *, pte_t)

#define set_pte(ptep,pteval) BTFIXUP_CALL(set_pte)(ptep,pteval)

struct seq_file;
BTFIXUPDEF_CALL(void, mmu_info, struct seq_file *)

#define mmu_info(p) BTFIXUP_CALL(mmu_info)(p)

/* Fault handler stuff... */
#define FAULT_CODE_PROT     0x1
#define FAULT_CODE_WRITE    0x2
#define FAULT_CODE_USER     0x4

BTFIXUPDEF_CALL(void, update_mmu_cache, struct vm_area_struct *, unsigned long, pte_t)

#define update_mmu_cache(vma,addr,pte) BTFIXUP_CALL(update_mmu_cache)(vma,addr,pte)

extern int invalid_segment;

/* Encode and de-code a swap entry */
#define SWP_TYPE(x)			(((x).val >> 2) & 0x7f)
#define SWP_OFFSET(x)			(((x).val >> 9) & 0x3ffff)
#define SWP_ENTRY(type,offset)		((swp_entry_t) { (((type) & 0x7f) << 2) | (((offset) & 0x3ffff) << 9) })
#define pte_to_swp_entry(pte)		((swp_entry_t) { pte_val(pte) })
#define swp_entry_to_pte(x)		((pte_t) { (x).val })

struct ctx_list {
	struct ctx_list *next;
	struct ctx_list *prev;
	unsigned int ctx_number;
	struct mm_struct *ctx_mm;
};

extern struct ctx_list *ctx_list_pool;  /* Dynamically allocated */
extern struct ctx_list ctx_free;        /* Head of free list */
extern struct ctx_list ctx_used;        /* Head of used contexts list */

#define NO_CONTEXT     -1

extern __inline__ void remove_from_ctx_list(struct ctx_list *entry)
{
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
}

extern __inline__ void add_to_ctx_list(struct ctx_list *head, struct ctx_list *entry)
{
	entry->next = head;
	(entry->prev = head->prev)->next = entry;
	head->prev = entry;
}
#define add_to_free_ctxlist(entry) add_to_ctx_list(&ctx_free, entry)
#define add_to_used_ctxlist(entry) add_to_ctx_list(&ctx_used, entry)

extern __inline__ unsigned long
__get_phys (unsigned long addr)
{
	switch (sparc_cpu_model){
	case sun4:
	case sun4c:
		return sun4c_get_pte (addr) << PAGE_SHIFT;
	case sun4m:
	case sun4d:
		return ((srmmu_get_pte (addr) & 0xffffff00) << 4);
	default:
		return 0;
	}
}

extern __inline__ int
__get_iospace (unsigned long addr)
{
	switch (sparc_cpu_model){
	case sun4:
	case sun4c:
		return -1; /* Don't check iospace on sun4c */
	case sun4m:
	case sun4d:
		return (srmmu_get_pte (addr) >> 28);
	default:
		return -1;
	}
}

extern unsigned long *sparc_valid_addr_bitmap;

/* Needs to be defined here and not in linux/mm.h, as it is arch dependent */
#define kern_addr_valid(addr) \
	(test_bit(__pa((unsigned long)(addr))>>20, sparc_valid_addr_bitmap))

extern int io_remap_page_range(unsigned long from, unsigned long to,
			       unsigned long size, pgprot_t prot, int space);

#include <asm-generic/pgtable.h>

#endif /* !(__ASSEMBLY__) */

/* We provide our own get_unmapped_area to cope with VA holes for userland */
#define HAVE_ARCH_UNMAPPED_AREA

/*
 * No page table caches to initialise
 */
#define pgtable_cache_init()	do { } while (0)

#endif /* !(_SPARC_PGTABLE_H) */
