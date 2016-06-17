#ifndef _PPC64_PGTABLE_H
#define _PPC64_PGTABLE_H

/*
 * This file contains the functions and defines necessary to modify and use
 * the ppc64 hashed page table.
 */

#ifndef __ASSEMBLY__
#include <asm/processor.h>		/* For TASK_SIZE */
#include <asm/mmu.h>
#include <asm/page.h>
#endif /* __ASSEMBLY__ */

/* PMD_SHIFT determines what a second-level page table entry can map */
#define PMD_SHIFT	(PAGE_SHIFT + PAGE_SHIFT - 3)
#define PMD_SIZE	(1UL << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE-1))

/* PGDIR_SHIFT determines what a third-level page table entry can map */
#define PGDIR_SHIFT	(PAGE_SHIFT + (PAGE_SHIFT - 3) + (PAGE_SHIFT - 2))
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))

/*
 * Entries per page directory level.  The PTE level must use a 64b record
 * for each page table entry.  The PMD and PGD level use a 32b record for 
 * each entry by assuming that each entry is page aligned.
 */
#define PTE_INDEX_SIZE  9
#define PMD_INDEX_SIZE  10
#define PGD_INDEX_SIZE  10

#define PTRS_PER_PTE	(1 << PTE_INDEX_SIZE)
#define PTRS_PER_PMD	(1 << PMD_INDEX_SIZE)
#define PTRS_PER_PGD	(1 << PGD_INDEX_SIZE)

#if 0
/* DRENG / PPPBBB This is a compiler bug!!! */
#define USER_PTRS_PER_PGD	(TASK_SIZE / PGDIR_SIZE)
#else
#define USER_PTRS_PER_PGD	(1024)
#endif
#define FIRST_USER_PGD_NR	0

#define EADDR_SIZE (PTE_INDEX_SIZE + PMD_INDEX_SIZE + \
                    PGD_INDEX_SIZE + PAGE_SHIFT) 

/*
 * Define the address range of the vmalloc VM area.
 */
#define VMALLOC_START (0xD000000000000000)
#define VMALLOC_VMADDR(x) ((unsigned long)(x))

#ifndef CONFIG_SHARED_MEMORY_ADDRESSING
#define VMALLOC_END   (VMALLOC_START + VALID_EA_BITS)
#else
#define VMALLOC_END   (VMALLOC_START + (VALID_EA_BITS >> 1))
#define SMALLOC_START (VMALLOC_START + (VALID_EA_BITS >> 1) + 1)
#define SMALLOC_END   (VMALLOC_START + VALID_EA_BITS)
#define SMALLOC_EA_SHIFT 40
#define SMALLOC_ESID_SHIFT 12
#endif

/*
 * Define the address range of the imalloc VM area.
 * (used for ioremap)
 */
#define IMALLOC_START (ioremap_bot)
#define IMALLOC_VMADDR(x) ((unsigned long)(x))
#define IMALLOC_BASE  (0xE000000000000000)
#define IMALLOC_END   (IMALLOC_BASE + VALID_EA_BITS)

/*
 * Define the address range mapped virt <-> physical
 */
#define KRANGE_START KERNELBASE
#define KRANGE_END   (KRANGE_START + VALID_EA_BITS)

/*
 * Define the user address range
 */
#define USER_START (0UL)
#define USER_END   (USER_START + VALID_EA_BITS)


/*
 * Bits in a linux-style PTE.  These match the bits in the
 * (hardware-defined) PowerPC PTE as closely as possible.
 */
#define _PAGE_PRESENT	0x001UL	/* software: pte contains a translation */
#define _PAGE_USER	0x002UL	/* matches one of the PP bits */
#define _PAGE_RW	0x004UL	/* software: user write access allowed */
#define _PAGE_GUARDED	0x008UL
#define _PAGE_COHERENT	0x010UL	/* M: enforce memory coherence (SMP systems) */
#define _PAGE_NO_CACHE	0x020UL	/* I: cache inhibit */
#define _PAGE_WRITETHRU	0x040UL	/* W: cache write-through */
#define _PAGE_DIRTY	0x080UL	/* C: page changed */
#define _PAGE_ACCESSED	0x100UL	/* R: page referenced */
#define _PAGE_HPTENOIX	0x200UL /* software: pte HPTE slot unknown */
#define _PAGE_HASHPTE	0x400UL	/* software: pte has an associated HPTE */
#define _PAGE_EXEC	0x800UL	/* software: i-cache coherence required */
#define _PAGE_SECONDARY 0x8000UL /* software: HPTE is in secondary group */
#define _PAGE_GROUP_IX  0x7000UL /* software: HPTE index within group */
/* Bits 0x7000 identify the index within an HPT Group */
#define _PAGE_HPTEFLAGS (_PAGE_HASHPTE | _PAGE_HPTENOIX | _PAGE_SECONDARY | _PAGE_GROUP_IX)
/* PAGE_MASK gives the right answer below, but only by accident */
/* It should be preserving the high 48 bits and then specifically */
/* preserving _PAGE_SECONDARY | _PAGE_GROUP_IX */
#define _PAGE_CHG_MASK	(PAGE_MASK | _PAGE_ACCESSED | _PAGE_DIRTY | _PAGE_HPTEFLAGS)

#define _PAGE_BASE	(_PAGE_PRESENT | _PAGE_ACCESSED | _PAGE_COHERENT)

#define _PAGE_WRENABLE	(_PAGE_RW | _PAGE_DIRTY)

/* __pgprot defined in asm-ppc64/page.h */
#define PAGE_NONE	__pgprot(_PAGE_PRESENT | _PAGE_ACCESSED)

#define PAGE_SHARED	__pgprot(_PAGE_BASE | _PAGE_RW | _PAGE_USER)
#define PAGE_SHARED_X	__pgprot(_PAGE_BASE | _PAGE_RW | _PAGE_USER | _PAGE_EXEC)
#define PAGE_COPY	__pgprot(_PAGE_BASE | _PAGE_USER)
#define PAGE_COPY_X	__pgprot(_PAGE_BASE | _PAGE_USER | _PAGE_EXEC)
#define PAGE_READONLY	__pgprot(_PAGE_BASE | _PAGE_USER)
#define PAGE_READONLY_X	__pgprot(_PAGE_BASE | _PAGE_USER | _PAGE_EXEC)
#define PAGE_KERNEL	__pgprot(_PAGE_BASE | _PAGE_WRENABLE)
#define PAGE_KERNEL_CI	__pgprot(_PAGE_PRESENT | _PAGE_ACCESSED | \
			       _PAGE_WRENABLE | _PAGE_NO_CACHE | _PAGE_GUARDED)

/*
 * The PowerPC can only do execute protection on a segment (256MB) basis,
 * not on a page basis.  So we consider execute permission the same as read.
 * Also, write permissions imply read permissions.
 * This is the closest we can get..
 */
#define __P000	PAGE_NONE
#define __P001	PAGE_READONLY_X
#define __P010	PAGE_COPY
#define __P011	PAGE_COPY_X
#define __P100	PAGE_READONLY
#define __P101	PAGE_READONLY_X
#define __P110	PAGE_COPY
#define __P111	PAGE_COPY_X

#define __S000	PAGE_NONE
#define __S001	PAGE_READONLY_X
#define __S010	PAGE_SHARED
#define __S011	PAGE_SHARED_X
#define __S100	PAGE_READONLY
#define __S101	PAGE_READONLY_X
#define __S110	PAGE_SHARED
#define __S111	PAGE_SHARED_X

#ifndef __ASSEMBLY__

/*
 * ZERO_PAGE is a global shared page that is always zero: used
 * for zero-mapped memory areas etc..
 */
extern unsigned long empty_zero_page[PAGE_SIZE/sizeof(unsigned long)];
#define ZERO_PAGE(vaddr) (mem_map + MAP_NR(empty_zero_page))
#endif /* __ASSEMBLY__ */

/* shift to put page number into pte */
#define PTE_SHIFT (16)

#ifndef __ASSEMBLY__

/*
 * Conversion functions: convert a page and protection to a page entry,
 * and a page entry and page directory to the page they refer to.
 *
 * mk_pte_phys takes a physical address as input 
 *
 * mk_pte takes a (struct page *) as input
 */

#define mk_pte_phys(physpage,pgprot)                                      \
({									  \
	pte_t pte;							  \
	pte_val(pte) = (((physpage)<<(PTE_SHIFT-PAGE_SHIFT)) | pgprot_val(pgprot)); \
	pte;							          \
})

#define mk_pte(page,pgprot)                                               \
({									  \
	pte_t pte;							  \
	pte_val(pte) = ((unsigned long)((page) - mem_map) << PTE_SHIFT) |   \
                        pgprot_val(pgprot);                               \
	pte;							          \
})

#define pte_modify(_pte, newprot) \
  (__pte((pte_val(_pte) & _PAGE_CHG_MASK) | pgprot_val(newprot)))

#define pte_none(pte)		((pte_val(pte) & ~_PAGE_HPTEFLAGS) == 0)
#define pte_present(pte)	(pte_val(pte) & _PAGE_PRESENT)

/* pte_clear moved to later in this file */

#define pte_pagenr(x)		((unsigned long)((pte_val(x) >> PTE_SHIFT)))
#define pte_page(x)		(mem_map+pte_pagenr(x))

#define pmd_set(pmdp, ptep) 	(pmd_val(*(pmdp)) = (__ba_to_bpn(ptep)))
#define pmd_none(pmd)		(!pmd_val(pmd))
#define	pmd_bad(pmd)		((pmd_val(pmd)) == 0)
#define	pmd_present(pmd)	((pmd_val(pmd)) != 0)
#define	pmd_clear(pmdp)		(pmd_val(*(pmdp)) = 0)
#define pmd_page(pmd)		(__bpn_to_ba(pmd_val(pmd)))
#define pgd_set(pgdp, pmdp)	(pgd_val(*(pgdp)) = (__ba_to_bpn(pmdp)))
#define pgd_none(pgd)		(!pgd_val(pgd))
#define pgd_bad(pgd)		((pgd_val(pgd)) == 0)
#define pgd_present(pgd)	(pgd_val(pgd) != 0UL)
#define pgd_clear(pgdp)		(pgd_val(*(pgdp)) = 0UL)
#define pgd_page(pgd)		(__bpn_to_ba(pgd_val(pgd))) 

/* 
 * Find an entry in a page-table-directory.  We combine the address region 
 * (the high order N bits) and the pgd portion of the address.
 */
#define pgd_index(address) (((address) >> (PGDIR_SHIFT)) & (PTRS_PER_PGD -1))

#define pgd_offset(mm, address)	 ((mm)->pgd + pgd_index(address))

/* Find an entry in the second-level page table.. */
#define pmd_offset(dir,addr) \
  ((pmd_t *) pgd_page(*(dir)) + (((addr) >> PMD_SHIFT) & (PTRS_PER_PMD - 1)))

/* Find an entry in the third-level page table.. */
#define pte_offset(dir,addr) \
  ((pte_t *) pmd_page(*(dir)) + (((addr) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1)))

/* to find an entry in a kernel page-table-directory */
/* This now only contains the vmalloc pages */
#define pgd_offset_k(address) pgd_offset(&init_mm, address)

/* to find an entry in the ioremap page-table-directory */
#define pgd_offset_i(address) (ioremap_pgd + pgd_index(address))

#define pages_to_mb(x)		((x) >> (20-PAGE_SHIFT))

/*
 * The following only work if pte_present() is true.
 * Undefined behaviour if not..
 */
static inline int pte_read(pte_t pte)  { return pte_val(pte) & _PAGE_USER;}
static inline int pte_write(pte_t pte) { return pte_val(pte) & _PAGE_RW;}
static inline int pte_exec(pte_t pte)  { return pte_val(pte) & _PAGE_EXEC;}
static inline int pte_dirty(pte_t pte) { return pte_val(pte) & _PAGE_DIRTY;}
static inline int pte_young(pte_t pte) { return pte_val(pte) & _PAGE_ACCESSED;}

static inline void pte_uncache(pte_t pte) { pte_val(pte) |= _PAGE_NO_CACHE; }
static inline void pte_cache(pte_t pte)   { pte_val(pte) &= ~_PAGE_NO_CACHE; }

static inline pte_t pte_rdprotect(pte_t pte) {
	pte_val(pte) &= ~_PAGE_USER; return pte; }
static inline pte_t pte_exprotect(pte_t pte) {
	pte_val(pte) &= ~_PAGE_EXEC; return pte; }
static inline pte_t pte_wrprotect(pte_t pte) {
	pte_val(pte) &= ~(_PAGE_RW); return pte; }
static inline pte_t pte_mkclean(pte_t pte) {
	pte_val(pte) &= ~(_PAGE_DIRTY); return pte; }
static inline pte_t pte_mkold(pte_t pte) {
	pte_val(pte) &= ~_PAGE_ACCESSED; return pte; }

static inline pte_t pte_mkread(pte_t pte) {
	pte_val(pte) |= _PAGE_USER; return pte; }
static inline pte_t pte_mkexec(pte_t pte) {
	pte_val(pte) |= _PAGE_USER | _PAGE_EXEC; return pte; }
static inline pte_t pte_mkwrite(pte_t pte) {
	pte_val(pte) |= _PAGE_RW; return pte; }
static inline pte_t pte_mkdirty(pte_t pte) {
	pte_val(pte) |= _PAGE_DIRTY; return pte; }
static inline pte_t pte_mkyoung(pte_t pte) {
	pte_val(pte) |= _PAGE_ACCESSED; return pte; }

/* Atomic PTE updates */

static inline unsigned long pte_update( pte_t *p, unsigned long clr,
					unsigned long set )
{
	unsigned long old, tmp;

	__asm__ __volatile__("\n\
1:	ldarx	%0,0,%3	\n\
	andc	%1,%0,%4 \n\
	or	%1,%1,%5 \n\
	stdcx.	%1,0,%3 \n\
	bne-	1b"
	: "=&r" (old), "=&r" (tmp), "=m" (*p)
	: "r" (p), "r" (clr), "r" (set), "m" (*p)
	: "cc" );
	return old;
}

static inline int ptep_test_and_clear_young(pte_t *ptep)
{
	return (pte_update(ptep, _PAGE_ACCESSED, 0) & _PAGE_ACCESSED) != 0;
}

static inline int ptep_test_and_clear_dirty(pte_t *ptep)
{
	return (pte_update(ptep, _PAGE_DIRTY, 0) & _PAGE_DIRTY) != 0;
}

static inline pte_t ptep_get_and_clear(pte_t *ptep)
{
	return __pte(pte_update(ptep, ~_PAGE_HPTEFLAGS, 0));
}

static inline void ptep_set_wrprotect(pte_t *ptep)
{
	pte_update(ptep, _PAGE_RW, 0);
}

static inline void ptep_mkdirty(pte_t *ptep)
{
	pte_update(ptep, 0, _PAGE_DIRTY);
}

#define pte_same(A,B)	(((pte_val(A) ^ pte_val(B)) & ~_PAGE_HPTEFLAGS) == 0)

/*
 * set_pte stores a linux PTE into the linux page table.
 * On machines which use an MMU hash table we avoid changing the
 * _PAGE_HASHPTE bit.
 */
static inline void set_pte(pte_t *ptep, pte_t pte)
{
	pte_update(ptep, ~_PAGE_HPTEFLAGS, pte_val(pte) & ~_PAGE_HPTEFLAGS);
}

static inline void pte_clear(pte_t * ptep)
{
	pte_update(ptep, ~_PAGE_HPTEFLAGS, 0);
}

struct mm_struct;
struct vm_area_struct;
extern void local_flush_tlb_all(void);
extern void local_flush_tlb_mm(struct mm_struct *mm);
extern void local_flush_tlb_page(struct vm_area_struct *vma, unsigned long vmaddr);
extern void local_flush_tlb_range(struct mm_struct *mm, unsigned long start,
			    unsigned long end);

#define flush_tlb_all local_flush_tlb_all
#define flush_tlb_mm local_flush_tlb_mm
#define flush_tlb_page local_flush_tlb_page
#define flush_tlb_range local_flush_tlb_range

static inline void flush_tlb_pgtables(struct mm_struct *mm,
				      unsigned long start, unsigned long end)
{
	/* PPC has hw page tables. */
}

/*
 * No cache flushing is required when address mappings are
 * changed, because the caches on PowerPCs are physically
 * addressed.
 */
#define flush_cache_all()		do { } while (0)
#define flush_cache_mm(mm)		do { } while (0)
#define flush_cache_range(mm, a, b)	do { } while (0)
#define flush_cache_page(vma, p)	do { } while (0)
#define flush_page_to_ram(page)		do { } while (0)

extern void flush_icache_user_range(struct vm_area_struct *vma,
			struct page *page, unsigned long addr, int len);
extern void flush_icache_range(unsigned long, unsigned long);
extern void __flush_dcache_icache(void *page_va);
extern void flush_dcache_page(struct page *page);
extern void flush_icache_page(struct vm_area_struct *vma, struct page *page);

extern unsigned long va_to_phys(unsigned long address);
extern pte_t *va_to_pte(unsigned long address);
extern unsigned long ioremap_bot, ioremap_base;

#define USER_PGD_PTRS (PAGE_OFFSET >> PGDIR_SHIFT)
#define KERNEL_PGD_PTRS (PTRS_PER_PGD-USER_PGD_PTRS)

#define pte_ERROR(e) \
	printk("%s:%d: bad pte %016lx.\n", __FILE__, __LINE__, pte_val(e))
#define pmd_ERROR(e) \
	printk("%s:%d: bad pmd %08x.\n", __FILE__, __LINE__, pmd_val(e))
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd %08x.\n", __FILE__, __LINE__, pgd_val(e))

extern pgd_t swapper_pg_dir[1024];
extern pgd_t ioremap_dir[1024];

extern void paging_init(void);

/*
 * Page tables may have changed.  We don't need to do anything here
 * as entries are faulted into the hash table by the low-level
 * data/instruction access exception handlers.
 */
/*
 * We won't be able to use update_mmu_cache to update the 
 * hardware page table because we need to update the pte
 * as well, but we don't get the address of the pte, only
 * its value.
 */
#define update_mmu_cache(vma, addr, pte)	do { } while (0)

extern void flush_hash_segments(unsigned low_vsid, unsigned high_vsid);
extern void flush_hash_page(unsigned long context, unsigned long ea, pte_t *ptep);
extern void build_valid_hpte(unsigned long vsid, unsigned long ea, 
			     unsigned long pa, pte_t * ptep, 
			     unsigned hpteflags, unsigned bolted );

/* Encode and de-code a swap entry */
#define SWP_TYPE(entry)			(((entry).val >> 1) & 0x3f)
#define SWP_OFFSET(entry)		((entry).val >> 8)
#define SWP_ENTRY(type, offset)		((swp_entry_t) { ((type) << 1) | ((offset) << 8) })
#define pte_to_swp_entry(pte)		((swp_entry_t) { pte_val(pte) >> PTE_SHIFT })
#define swp_entry_to_pte(x)		((pte_t) { (x).val << PTE_SHIFT })

/*
 * kern_addr_valid is intended to indicate whether an address is a valid
 * kernel address.  Most 32-bit archs define it as always true (like this)
 * but most 64-bit archs actually perform a test.  What should we do here?
 * The only use is in fs/ncpfs/dir.c
 */
#define kern_addr_valid(addr)	(1)

#ifdef CONFIG_PPC_ISERIES
#define io_remap_page_range remap_page_range
#else
extern int io_remap_page_range(unsigned long from, unsigned long to, unsigned long size, pgprot_t prot);
#endif

/*
 * No page table caches to initialise
 */
#define pgtable_cache_init()	do { } while (0)

extern void updateBoltedHptePP(unsigned long newpp, unsigned long ea);
extern void hpte_init_pSeries(void);
extern void hpte_init_iSeries(void);

extern void make_pte(HPTE * htab, unsigned long va, unsigned long pa,
		int mode, unsigned long hash_mask, int large);

#endif /* __ASSEMBLY__ */
#endif /* _PPC64_PGTABLE_H */
