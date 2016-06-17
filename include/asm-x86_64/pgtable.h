#ifndef _X86_64_PGTABLE_H
#define _X86_64_PGTABLE_H

/*
 * This file contains the functions and defines necessary to modify and use
 * the x86-64 page table tree.
 * 
 * x86-64 has a 4 level table setup. Generic linux MM only supports
 * three levels. The fourth level is currently a single static page that
 * is shared by everybody and just contains a pointer to the current
 * three level page setup on the beginning and some kernel mappings at 
 * the end. For more details see Documentation/x86_64/mm.txt
 */
#ifndef __ASSEMBLY__
#include <asm/processor.h>
#include <asm/fixmap.h>
#include <asm/bitops.h>
#include <asm/pda.h>
#include <linux/threads.h>
#include <linux/config.h>

extern pgd_t level3_kernel_pgt[512];
extern pgd_t level3_physmem_pgt[512];
extern pgd_t level3_ident_pgt[512];
extern pmd_t level2_kernel_pgt[512];
extern pml4_t init_level4_pgt[];
extern pgd_t boot_vmalloc_pgt[];

extern void paging_init(void);

#define swapper_pg_dir NULL

/* Caches aren't brain-dead on the intel. */
#define flush_cache_all()			do { } while (0)
#define flush_cache_mm(mm)			do { } while (0)
#define flush_cache_range(mm, start, end)	do { } while (0)
#define flush_cache_page(vma, vmaddr)		do { } while (0)
#define flush_page_to_ram(page)			do { } while (0)
#define flush_dcache_page(page)			do { } while (0)
#define flush_icache_range(start, end)		do { } while (0)
#define flush_icache_page(vma,pg)		do { } while (0)
#define flush_icache_user_range(vma,pg,adr,len)	do { } while (0)

#define __flush_tlb()							\
	do {								\
		unsigned long tmpreg;					\
									\
		__asm__ __volatile__(					\
			"movq %%cr3, %0;  # flush TLB \n"		\
			"movq %0, %%cr3;              \n"		\
			: "=r" (tmpreg)					\
			:: "memory");					\
	} while (0)

/*
 * Global pages have to be flushed a bit differently. Not a real
 * performance problem because this does not happen often.
 */
#define __flush_tlb_global()						\
	do {								\
		unsigned long tmpreg;					\
									\
		__asm__ __volatile__(					\
			"movq %1, %%cr4;  # turn off PGE     \n"	\
			"movq %%cr3, %0;  # flush TLB        \n"	\
			"movq %0, %%cr3;                     \n"	\
			"movq %2, %%cr4;  # turn PGE back on \n"	\
			: "=&r" (tmpreg)				\
			: "r" (mmu_cr4_features & ~(u64)X86_CR4_PGE),	\
			  "r" (mmu_cr4_features)			\
			: "memory");					\
	} while (0)

#define __flush_tlb_all() __flush_tlb_global()

#define __flush_tlb_one(addr) __asm__ __volatile__("invlpg %0": :"m" (*(char *) addr))

/*
 * ZERO_PAGE is a global shared page that is always zero: used
 * for zero-mapped memory areas etc..
 */
extern unsigned long empty_zero_page[PAGE_SIZE/sizeof(unsigned long)];
#define ZERO_PAGE(vaddr) (virt_to_page(empty_zero_page))

#endif /* !__ASSEMBLY__ */

#define PML4_SHIFT	39
#define PTRS_PER_PML4	512

/*
 * PGDIR_SHIFT determines what a 3rd level page table entry can map
 */
#define PGDIR_SHIFT	30
#define PTRS_PER_PGD	512

/*
 * PMD_SHIFT determines the size of the area a middle-level
 * page table can map
 */
#define PMD_SHIFT	21
#define PTRS_PER_PMD	512

/*
 * entries per page directory level
 */
#define PTRS_PER_PTE	512

#define pte_ERROR(e) \
	printk("%s:%d: bad pte %p(%016lx).\n", __FILE__, __LINE__, &(e), pte_val(e))
#define pmd_ERROR(e) \
	printk("%s:%d: bad pmd %p(%016lx).\n", __FILE__, __LINE__, &(e), pmd_val(e))
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd %p(%016lx).\n", __FILE__, __LINE__, &(e), pgd_val(e))

#define pml4_none(x)	(!pml4_val(x))
#define pgd_none(x)	(!pgd_val(x))


extern inline int pgd_present(pgd_t pgd)	{ return !pgd_none(pgd); }

static inline void set_pte(pte_t *dst, pte_t val)
{
	pte_val(*dst) = pte_val(val);
} 

static inline void set_pmd(pmd_t *dst, pmd_t val)
{
	pmd_val(*dst) = pmd_val(val);
} 

static inline void set_pgd(pgd_t *dst, pgd_t val)
{
	pgd_val(*dst) = pgd_val(val);
} 

static inline void set_pml4(pml4_t *dst, pml4_t val) 
{ 
	pml4_val(*dst) = pml4_val(val);
} 

extern inline void __pgd_clear (pgd_t * pgd)
{
	set_pgd(pgd, __pgd(0));
}

extern inline void pgd_clear (pgd_t * pgd)
{
	__pgd_clear(pgd);
	__flush_tlb();
}

#define pgd_page(pgd) \
((unsigned long) __va(pgd_val(pgd) & PHYSICAL_PAGE_MASK))
#define __mk_pgd(address,prot) ((pgd_t) { (address) | pgprot_val(prot) })

/* Find an entry in the second-level page table.. */
#define pmd_offset(dir, address) ((pmd_t *) pgd_page(*(dir)) + \
			__pmd_offset(address))
#define __mk_pmd(address,prot) ((pmd_t) { ((address) | pgprot_val(prot)) & __supported_pte_mask})

#define ptep_get_and_clear(xp)	__pte(xchg(&(xp)->pte, 0))
#define pte_same(a, b)		((a).pte == (b).pte)
#define __mk_pte(page_nr,pgprot) \
	__pte(((page_nr) << PAGE_SHIFT) | (pgprot_val(pgprot) & __supported_pte_mask))
#define PML4_SIZE	(1UL << PML4_SHIFT)
#define PML4_MASK	(~(PML4_SIZE-1))
#define PMD_SIZE	(1UL << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE-1))
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))

#define USER_PTRS_PER_PGD	(TASK_SIZE/PGDIR_SIZE)
#define FIRST_USER_PGD_NR	0

#define USER_PGD_PTRS (PAGE_OFFSET >> PGDIR_SHIFT)
#define KERNEL_PGD_PTRS (PTRS_PER_PGD-USER_PGD_PTRS)

#define BOOT_USER_L4_PTRS 1
#define BOOT_KERNEL_L4_PTRS 511	/* But we will do it in 4rd level */



#ifndef __ASSEMBLY__
/* IO mappings are the 509th slot in the PML4. We map them high up to make sure
   they never appear in the node hash table in DISCONTIG configs. */
#define IOMAP_START      0xfffffe8000000000

/* vmalloc space occupies the 510th slot in the PML4. You can have upto 512GB of
   vmalloc/ioremap space. */ 

#define VMALLOC_START	 0xffffff0000000000
#define VMALLOC_END      0xffffff7fffffffff
#define VMALLOC_VMADDR(x) ((unsigned long)(x))

#define MODULES_VADDR    0xffffffffa0000000
#define MODULES_END      0xffffffffafffffff
#define MODULES_LEN   (MODULES_END - MODULES_VADDR)

#define _PAGE_BIT_PRESENT	0
#define _PAGE_BIT_RW		1
#define _PAGE_BIT_USER		2
#define _PAGE_BIT_PWT		3	/* Write Through */
#define _PAGE_BIT_PCD		4	/* Cache disable */
#define _PAGE_BIT_ACCESSED	5
#define _PAGE_BIT_DIRTY		6
#define _PAGE_BIT_PSE		7	/* 2MB page */
#define _PAGE_BIT_GLOBAL	8	/* Global TLB entry PPro+ */
#define _PAGE_BIT_NX           63       /* No execute: only valid after cpuid check */

#define _PAGE_PRESENT	0x001
#define _PAGE_RW	0x002
#define _PAGE_USER	0x004
#define _PAGE_PWT	0x008
#define _PAGE_PCD	0x010
#define _PAGE_ACCESSED	0x020
#define _PAGE_DIRTY	0x040
#define _PAGE_PSE	0x080	/* 2MB page */
#define _PAGE_GLOBAL	0x100	/* Global TLB entry */
#define _PAGE_PGE	_PAGE_GLOBAL
#define _PAGE_NX        (1UL<<_PAGE_BIT_NX)

#define _PAGE_PROTNONE	0x080	/* If not present */

#define _PAGE_TABLE	(_PAGE_PRESENT | _PAGE_RW | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY)
#define _KERNPG_TABLE	(_PAGE_PRESENT | _PAGE_RW | _PAGE_ACCESSED | _PAGE_DIRTY)

#define KERNPG_TABLE	__pgprot(_KERNPG_TABLE)

#define _PAGE_CHG_MASK	(PTE_MASK | _PAGE_ACCESSED | _PAGE_DIRTY)

#define PAGE_NONE	__pgprot(_PAGE_PROTNONE | _PAGE_ACCESSED)
#define PAGE_SHARED	__pgprot(_PAGE_PRESENT | _PAGE_RW | _PAGE_USER | _PAGE_ACCESSED)
#define PAGE_SHARED_NOEXEC	__pgprot(_PAGE_NX | _PAGE_PRESENT | _PAGE_RW | _PAGE_USER | _PAGE_ACCESSED)
#define PAGE_COPY_NOEXEC	__pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_ACCESSED | _PAGE_NX)
#define PAGE_COPY PAGE_COPY_NOEXEC
#define PAGE_COPY_EXEC  \
	__pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_ACCESSED)
#define PAGE_READONLY __pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_ACCESSED | _PAGE_NX)
#define PAGE_READONLY_EXEC \
	__pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_ACCESSED)
#define PAGE_EXECONLY PAGE_READONLY_EXEC

#define PAGE_LARGE (_PAGE_PSE|_PAGE_PRESENT) 

#define __PAGE_KERNEL \
	(_PAGE_PRESENT | _PAGE_RW | _PAGE_DIRTY | _PAGE_ACCESSED | _PAGE_NX)
#define __PAGE_KERNEL_NOCACHE   (__PAGE_KERNEL | _PAGE_PCD)
#define __PAGE_KERNEL_RO        (__PAGE_KERNEL & ~_PAGE_RW)
#define __PAGE_KERNEL_VSYSCALL	(_PAGE_PRESENT | _PAGE_USER | _PAGE_ACCESSED)
#define __PAGE_KERNEL_LARGE     (__PAGE_KERNEL | _PAGE_PSE)
#define __PAGE_KERNEL_LARGE_NOCACHE     (__PAGE_KERNEL_LARGE | _PAGE_PCD)
#define __PAGE_KERNEL_EXECUTABLE (__PAGE_KERNEL & ~_PAGE_NX)
#define __PAGE_USER_NOCACHE_RO	\
	(_PAGE_PRESENT | _PAGE_USER | _PAGE_DIRTY | _PAGE_ACCESSED) 

extern unsigned long __supported_pte_mask; 
#define __PTE_SUPP(x) __pgprot((x) & __supported_pte_mask)

/* _NX is masked away in mk_pmd/pte */

#define PAGE_KERNEL __PTE_SUPP(__PAGE_KERNEL|_PAGE_GLOBAL)
#define PAGE_KERNEL_RO __pgprot(__PAGE_KERNEL_RO|_PAGE_GLOBAL)
#define PAGE_KERNEL_NOCACHE __pgprot(__PAGE_KERNEL_NOCACHE|_PAGE_GLOBAL)
#define PAGE_KERNEL_VSYSCALL __pgprot(__PAGE_KERNEL_VSYSCALL|_PAGE_GLOBAL)
#define PAGE_KERNEL_LARGE __pgprot(__PAGE_KERNEL_LARGE|_PAGE_GLOBAL)
#define PAGE_KERNEL_LARGE_NOCACHE __pgprot(__PAGE_KERNEL_LARGE_NOCACHE|_PAGE_GLOBAL)
#define PAGE_USER_NOCACHE_RO __pgprot(__PAGE_USER_NOCACHE_RO|_PAGE_GLOBAL)
#define PAGE_KERNEL_EXECUTABLE __pgprot(__PAGE_KERNEL_EXECUTABLE|_PAGE_GLOBAL)

/*         xwr */
#define __P000	PAGE_NONE
#define __P001	PAGE_READONLY
#define __P010	PAGE_COPY
#define __P011	PAGE_COPY
#define __P100	PAGE_EXECONLY
#define __P101	PAGE_READONLY_EXEC
#define __P110	PAGE_COPY_EXEC
#define __P111	PAGE_COPY_EXEC

/*         xwr */
#define __S000	PAGE_NONE
#define __S001	PAGE_READONLY
#define __S010	PAGE_SHARED_NOEXEC
#define __S011	PAGE_SHARED_NOEXEC
#define __S100	PAGE_EXECONLY
#define __S101	PAGE_READONLY_EXEC
#define __S110	PAGE_SHARED
#define __S111	PAGE_SHARED

static inline unsigned long pgd_bad(pgd_t pgd) 
{ 
	unsigned long val = pgd_val(pgd);
	val &= ~PAGE_MASK; 
	val &= ~(_PAGE_USER | _PAGE_DIRTY); 
	return val & ~(_PAGE_PRESENT | _PAGE_RW | _PAGE_ACCESSED);  	
} 

/*
 * Handling allocation failures during page table setup.
 */
extern void __handle_bad_pmd(pmd_t * pmd);
extern void __handle_bad_pmd_kernel(pmd_t * pmd);

#define pte_none(x)	(!pte_val(x))
#define pte_present(x)	(pte_val(x) & (_PAGE_PRESENT | _PAGE_PROTNONE))
#define pte_clear(xp)	do { set_pte(xp, __pte(0)); } while (0)

#define pmd_none(x)	(!pmd_val(x))
#define pmd_present(x)	(pmd_val(x) & _PAGE_PRESENT)
#define pmd_clear(xp)	do { set_pmd(xp, __pmd(0)); } while (0)
#define	pmd_bad(x)	\
	((pmd_val(x) & (~PAGE_MASK & (~_PAGE_USER))) != _KERNPG_TABLE )

#define pages_to_mb(x) ((x) >> (20-PAGE_SHIFT))

#ifndef CONFIG_DISCONTIGMEM
#define pte_page(x) (pfn_to_page((pte_val(x) & PHYSICAL_PAGE_MASK) >> PAGE_SHIFT))
#endif
/*
 * The following only work if pte_present() is true.
 * Undefined behaviour if not..
 */
extern inline int pte_read(pte_t pte)		{ return pte_val(pte) & _PAGE_USER; }
extern inline int pte_exec(pte_t pte)		{ return pte_val(pte) & _PAGE_USER; }
extern inline int pte_dirty(pte_t pte)		{ return pte_val(pte) & _PAGE_DIRTY; }
extern inline int pte_young(pte_t pte)		{ return pte_val(pte) & _PAGE_ACCESSED; }
extern inline int pte_write(pte_t pte)		{ return pte_val(pte) & _PAGE_RW; }

extern inline pte_t pte_rdprotect(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) & ~_PAGE_USER)); return pte; }
extern inline pte_t pte_exprotect(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) & ~_PAGE_USER)); return pte; }
extern inline pte_t pte_mkclean(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) & ~_PAGE_DIRTY)); return pte; }
extern inline pte_t pte_mkold(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) & ~_PAGE_ACCESSED)); return pte; }
extern inline pte_t pte_wrprotect(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) & ~_PAGE_RW)); return pte; }
extern inline pte_t pte_mkread(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) | _PAGE_USER)); return pte; }
extern inline pte_t pte_mkexec(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) | _PAGE_USER)); return pte; }
extern inline pte_t pte_mkdirty(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) | _PAGE_DIRTY)); return pte; }
extern inline pte_t pte_mkyoung(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) | _PAGE_ACCESSED)); return pte; }
extern inline pte_t pte_mkwrite(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) | _PAGE_RW)); return pte; }
static inline  int ptep_test_and_clear_dirty(pte_t *ptep)	{ return test_and_clear_bit(_PAGE_BIT_DIRTY, ptep); }
static inline  int ptep_test_and_clear_young(pte_t *ptep)	{ return test_and_clear_bit(_PAGE_BIT_ACCESSED, ptep); }
static inline void ptep_set_wrprotect(pte_t *ptep)		{ clear_bit(_PAGE_BIT_RW, ptep); }
static inline void ptep_mkdirty(pte_t *ptep)			{ set_bit(_PAGE_BIT_DIRTY, ptep); }

/*
 * Conversion functions: convert a page and protection to a page entry,
 * and a page entry and page directory to the page they refer to.
 */

#define mk_pte(page,pgprot) 							 \
({ 										 \
	pte_t __pte; 								 \
	unsigned long __val = page_to_phys(page);  			 \
	__val |= pgprot_val(pgprot);						 \
	__val &= __supported_pte_mask;						\
 	set_pte(&__pte, __pte(__val));						 \
 	__pte;									 \
}) 

/* This takes a physical page address that is used by the remapping functions */
static inline pte_t mk_pte_phys(unsigned long physpage, pgprot_t pgprot)
{ 
	pte_t __pte; 
	set_pte(&__pte, __pte(physpage + (pgprot_val(pgprot) & __supported_pte_mask))); 
	return __pte;
}

extern inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{ 
	set_pte(&pte, 
		__pte(((pte_val(pte) & _PAGE_CHG_MASK) | pgprot_val(newprot))  &
		      __supported_pte_mask));
	return pte; 
}

#define page_pte(page) page_pte_prot(page, __pgprot(0))
#define __pmd_page(pmd) (__va(pmd_val(pmd) & PHYSICAL_PAGE_MASK))

/* to find an entry in a page-table-directory. */
#define pgd_index(address) ((address >> PGDIR_SHIFT) & (PTRS_PER_PGD-1))
#define pgd_offset(mm, address) ((mm)->pgd+pgd_index(address))

#define __pgd_offset_k(pgd, address) ((pgd) + pgd_index(address))

#define current_pgd_offset_k(address) \
	__pgd_offset_k((pgd_t *)read_pda(level4_pgt), address)

/* This accesses the reference page table of the boot cpu. 
   Other CPUs get synced lazily via the page fault handler. */
#define pgd_offset_k(address) \
	__pgd_offset_k( \
       (pgd_t *)__va(pml4_val(init_level4_pgt[pml4_index(address)]) & PHYSICAL_PAGE_MASK), address)

#define __pmd_offset(address) \
		(((address) >> PMD_SHIFT) & (PTRS_PER_PMD-1))

/* Find an entry in the third-level page table.. */
#define __pte_offset(address) \
		((address >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))
#define pte_offset(dir, address) ((pte_t *) __pmd_page(*(dir)) + \
			__pte_offset(address))

/* never use these in the common code */
#define pml4_page(level4) ((unsigned long) __va(pml4_val(level4) & PHYSICAL_PAGE_MASK))
#define pml4_index(address) (((address) >> PML4_SHIFT) & (PTRS_PER_PML4-1))
#define pml4_offset_k(address) ((pml4_t *)read_pda(level4_pgt) + pml4_index(address))
#define level3_offset_k(dir, address) ((pgd_t *) pml4_page(*(dir)) + pgd_index(address))
#define mk_kernel_pml4(address,prot) ((pml4_t){(address) | pgprot_val(prot)})
#define pml4_present(pml4) (pml4_val(pml4) & _PAGE_PRESENT)

/*
 * x86 doesn't have any external MMU info: the kernel page
 * tables contain all the necessary information.
 */
#define update_mmu_cache(vma,address,pte) do { } while (0)

/* Encode and de-code a swap entry */
#define SWP_TYPE(x)			(((x).val >> 1) & 0x3f)
#define SWP_OFFSET(x)			((x).val >> 8)
#define SWP_ENTRY(type, offset)		((swp_entry_t) { ((type) << 1) | ((offset) << 8) })
#define pte_to_swp_entry(pte)		((swp_entry_t) { pte_val(pte) })
#define swp_entry_to_pte(x)		((pte_t) { (x).val })

struct page;
/* 
 * Change attributes of an kernel page.
 */
struct page;
extern int change_page_attr(struct page *page, int numpages, pgprot_t prot);

extern void clear_kernel_mapping(unsigned long addr, unsigned long size);

#endif /* !__ASSEMBLY__ */

/* Needs to be defined here and not in linux/mm.h, as it is arch dependent */
#define PageSkip(page)		(0)
#define kern_addr_valid(kaddr)  ((kaddr)>>PAGE_SHIFT < max_mapnr)

#define io_remap_page_range remap_page_range

#define HAVE_ARCH_UNMAPPED_AREA

#define pgtable_cache_init()   do { } while (0)


#endif /* _X86_64_PGTABLE_H */
