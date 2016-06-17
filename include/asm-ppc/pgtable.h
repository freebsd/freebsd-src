#ifdef __KERNEL__
#ifndef _PPC_PGTABLE_H
#define _PPC_PGTABLE_H

#include <linux/config.h>

#ifndef __ASSEMBLY__
#include <linux/sched.h>
#include <linux/threads.h>
#include <asm/processor.h>		/* For TASK_SIZE */
#include <asm/mmu.h>
#include <asm/page.h>

extern void _tlbie(unsigned long address);
extern void _tlbia(void);

#ifdef CONFIG_4xx
#ifdef CONFIG_PIN_TLB
/* When pinning entries on the 4xx, we have to use a software function
 * to ensure we don't remove them since there isn't any hardware support
 * for this.
 */
#define __tlbia()	_tlbia()
#else
#define __tlbia()	asm volatile ("tlbia; sync" : : : "memory")
#endif

static inline void local_flush_tlb_all(void)
	{ __tlbia(); }
static inline void local_flush_tlb_mm(struct mm_struct *mm)
	{ __tlbia(); }
static inline void local_flush_tlb_page(struct vm_area_struct *vma,
				unsigned long vmaddr)
	{ _tlbie(vmaddr); }
static inline void local_flush_tlb_range(struct mm_struct *mm,
				unsigned long start, unsigned long end)
	{ __tlbia(); }
#define update_mmu_cache(vma, addr, pte)	do { } while (0)

#elif defined(CONFIG_8xx)
#define __tlbia()	asm volatile ("tlbia; sync" : : : "memory")

static inline void local_flush_tlb_all(void)
	{ __tlbia(); }
static inline void local_flush_tlb_mm(struct mm_struct *mm)
	{ __tlbia(); }
static inline void local_flush_tlb_page(struct vm_area_struct *vma,
				unsigned long vmaddr)
	{ _tlbie(vmaddr); }
static inline void local_flush_tlb_range(struct mm_struct *mm,
				unsigned long start, unsigned long end)
	{ __tlbia(); }
#define update_mmu_cache(vma, addr, pte)	do { } while (0)

#else	/* 6xx, 7xx, 7xxx cpus */
struct mm_struct;
struct vm_area_struct;
extern void local_flush_tlb_all(void);
extern void local_flush_tlb_mm(struct mm_struct *mm);
extern void local_flush_tlb_page(struct vm_area_struct *vma, unsigned long vmaddr);
extern void local_flush_tlb_range(struct mm_struct *mm, unsigned long start,
			    unsigned long end);

/*
 * This gets called at the end of handling a page fault, when
 * the kernel has put a new PTE into the page table for the process.
 * We use it to put a corresponding HPTE into the hash table
 * ahead of time, instead of waiting for the inevitable extra
 * hash-table miss exception.
 */
extern void update_mmu_cache(struct vm_area_struct *, unsigned long, pte_t);
#endif

#define flush_tlb_all local_flush_tlb_all
#define flush_tlb_mm local_flush_tlb_mm
#define flush_tlb_page local_flush_tlb_page
#define flush_tlb_range local_flush_tlb_range

/*
 * This is called in munmap when we have freed up some page-table
 * pages.  We don't need to do anything here, there's nothing special
 * about our page-table pages.  -- paulus
 */
static inline void flush_tlb_pgtables(struct mm_struct *mm,
				      unsigned long start, unsigned long end)
{
}

/*
 * No cache flushing is required when address mappings are
 * changed, because the caches on PowerPCs are physically
 * addressed.  -- paulus
 * Also, when SMP we use the coherency (M) bit of the
 * BATs and PTEs.  -- Cort
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
extern unsigned long vmalloc_start;

/* Start and end of the vmalloc area. */
#define VMALLOC_START	vmalloc_start
#define VMALLOC_END	ioremap_bot
#define VMALLOC_VMADDR(x) ((unsigned long)(x))

#endif /* __ASSEMBLY__ */

/*
 * The PowerPC MMU uses a hash table containing PTEs, together with
 * a set of 16 segment registers (on 32-bit implementations), to define
 * the virtual to physical address mapping.
 *
 * We use the hash table as an extended TLB, i.e. a cache of currently
 * active mappings.  We maintain a two-level page table tree, much
 * like that used by the i386, for the sake of the Linux memory
 * management code.  Low-level assembler code in hashtable.S
 * (procedure hash_page) is responsible for extracting ptes from the
 * tree and putting them into the hash table when necessary, and
 * updating the accessed and modified bits in the page table tree.
 */

/*
 * The PowerPC MPC8xx uses a TLB with hardware assisted, software tablewalk.
 * We also use the two level tables, but we can put the real bits in them
 * needed for the TLB and tablewalk.  These definitions require Mx_CTR.PPM = 0,
 * Mx_CTR.PPCS = 0, and MD_CTR.TWAM = 1.  The level 2 descriptor has
 * additional page protection (when Mx_CTR.PPCS = 1) that allows TLB hit
 * based upon user/super access.  The TLB does not have accessed nor write
 * protect.  We assume that if the TLB get loaded with an entry it is
 * accessed, and overload the changed bit for write protect.  We use
 * two bits in the software pte that are supposed to be set to zero in
 * the TLB entry (24 and 25) for these indicators.  Although the level 1
 * descriptor contains the guarded and writethrough/copyback bits, we can
 * set these at the page level since they get copied from the Mx_TWC
 * register when the TLB entry is loaded.  We will use bit 27 for guard, since
 * that is where it exists in the MD_TWC, and bit 26 for writethrough.
 * These will get masked from the level 2 descriptor at TLB load time, and
 * copied to the MD_TWC before it gets loaded.
 * Large page sizes added.  We currently support two sizes, 4K and 8M.
 * This also allows a TLB hander optimization because we can directly
 * load the PMD into MD_TWC.  The 8M pages are only used for kernel
 * mapping of well known areas.  The PMD (PGD) entries contain control
 * flags in addition to the address, so care must be taken that the
 * software no longer assumes these are only pointers.
 */

/*
 * At present, all PowerPC 400-class processors share a similar TLB
 * architecture. The instruction and data sides share a unified,
 * 64-entry, fully-associative TLB which is maintained totally under
 * software control. In addition, the instruction side has a
 * hardware-managed, 4-entry, fully-associative TLB which serves as a
 * first level to the shared TLB. These two TLBs are known as the UTLB
 * and ITLB, respectively (see "mmu.h" for definitions).
 */

/*
 * The normal case is that PTEs are 32-bits and we have a 1-page
 * 1024-entry pgdir pointing to 1-page 1024-entry PTE pages.  -- paulus
 *
 * For any >32-bit physical address platform, we can use the following
 * two level page table layout where the pgdir is 8KB and the MS 13 bits
 * are an index to the second level table.  The combined pgdir/pmd first
 * level has 2048 entries and the second level has 512 64-bit PTE entries.
 * -Matt
 */
/* PMD_SHIFT determines the size of the area mapped by the PTE pages */
#define PMD_SHIFT	(PAGE_SHIFT + PTE_SHIFT)
#define PMD_SIZE	(1UL << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE-1))

/* PGDIR_SHIFT determines what a top-level page table entry can map */
#define PGDIR_SHIFT	PMD_SHIFT
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))

/*
 * entries per page directory level: our page-table tree is two-level, so
 * we don't really have any PMD directory.
 */
#define PTRS_PER_PTE	(1 << PTE_SHIFT)
#define PTRS_PER_PMD	1
#define PTRS_PER_PGD	(1 << (32 - PGDIR_SHIFT))

#define USER_PTRS_PER_PGD	(TASK_SIZE / PGDIR_SIZE)
#define FIRST_USER_PGD_NR	0

#define USER_PGD_PTRS (PAGE_OFFSET >> PGDIR_SHIFT)
#define KERNEL_PGD_PTRS (PTRS_PER_PGD-USER_PGD_PTRS)

#define pte_ERROR(e) \
	printk("%s:%d: bad pte "PTE_FMT".\n", __FILE__, __LINE__, pte_val(e))
#define pmd_ERROR(e) \
	printk("%s:%d: bad pmd %08lx.\n", __FILE__, __LINE__, pmd_val(e))
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd %08lx.\n", __FILE__, __LINE__, pgd_val(e))

/*
 * Bits in a linux-style PTE.  These match the bits in the
 * (hardware-defined) PowerPC PTE as closely as possible.
 */

#if defined(CONFIG_40x)

/* There are several potential gotchas here.  The 40x hardware TLBLO
   field looks like this:

   0  1  2  3  4  ... 18 19 20 21 22 23 24 25 26 27 28 29 30 31
   RPN.....................  0  0 EX WR ZSEL.......  W  I  M  G

   Where possible we make the Linux PTE bits match up with this

   - bits 20 and 21 must be cleared, because we use 4k pages (4xx can
     support down to 1k pages), this is done in the TLBMiss exception
     handler.
   - We use only zones 0 (for kernel pages) and 1 (for user pages)
     of the 16 available.  Bit 24-26 of the TLB are cleared in the TLB
     miss handler.  Bit 27 is PAGE_USER, thus selecting the correct
     zone.
   - PRESENT *must* be in the bottom two bits because swap cache
     entries use the top 30 bits.  Because 4xx doesn't support SMP
     anyway, M is irrelevant so we borrow it for PAGE_PRESENT.  Bit 30
     is cleared in the TLB miss handler before the TLB entry is loaded.
   - All other bits of the PTE are loaded into TLBLO without
     modification, leaving us only the bits 20, 21, 24, 25, 26, 30 for
     software PTE bits.  We actually use use bits 21, 24, 25, and
     30 respectively for the software bits: ACCESSED, DIRTY, RW, and
     PRESENT.
*/

/* Definitions for 4xx embedded chips. */
#define	_PAGE_GUARDED	0x001	/* G: page is guarded from prefetch */
#define _PAGE_PRESENT	0x002	/* software: PTE contains a translation */
#define	_PAGE_NO_CACHE	0x004	/* I: caching is inhibited */
#define	_PAGE_WRITETHRU	0x008	/* W: caching is write-through */
#define	_PAGE_USER	0x010	/* matches one of the zone permission bits */
#define	_PAGE_RW	0x040	/* software: Writes permitted */
#define	_PAGE_DIRTY	0x080	/* software: dirty page */
#define _PAGE_HWWRITE	0x100	/* hardware: Dirty & RW, set in exception */
#define _PAGE_HWEXEC	0x200	/* hardware: EX permission */
#define _PAGE_ACCESSED	0x400	/* software: R: page referenced */
#define _PMD_PRESENT	PAGE_MASK

#elif defined(CONFIG_44x)
/*
 * Definitions for PPC44x
 *
 * Because of the 3 word TLB entries to support 36-bit addressing,
 * the attribute are difficult to map in such a fashion that they
 * are easily loaded during exception processing.  I decided to
 * organize the entry so the ERPN is the only portion in the
 * upper word of the PTE and the attribute bits below are packed
 * in as sensibly as they can be in the area below a 4KB page size
 * oriented RPN.  This at least makes it easy to load the RPN and
 * ERPN fields in the TLB. -Matt
 *
 * Note that these bits preclude future use of a page size
 * less than 4KB.
 */
#define _PAGE_PRESENT	0x00000001		/* S: PTE valid */
#define	_PAGE_RW	0x00000002		/* S: Write permission */
#define	_PAGE_DIRTY	0x00000004		/* S: Page dirty */
#define _PAGE_ACCESSED	0x00000008		/* S: Page referenced */
#define _PAGE_HWWRITE	0x00000010		/* H: Dirty & RW */
#define _PAGE_HWEXEC	0x00000020		/* H: Execute permission */
#define	_PAGE_USER	0x00000040		/* S: User page */
#define	_PAGE_ENDIAN	0x00000080		/* H: E bit */
#define	_PAGE_GUARDED	0x00000100		/* H: G bit */
#define	_PAGE_COHERENT	0x00000200		/* H: M bit */
#define _PAGE_FILE	0x00000400		/* S: nonlinear file mapping */
#define	_PAGE_NO_CACHE	0x00000400		/* H: I bit */
#define	_PAGE_WRITETHRU	0x00000800		/* H: W bit */

/* TODO: Add large page lowmem mapping support */
#define _PMD_PRESENT	PAGE_MASK
#define _PMD_PRESENT_MASK (PAGE_MASK)
#define _PMD_BAD	(~PAGE_MASK)

/* ERPN in a PTE never gets cleared, ignore it */
#define _PTE_NONE_MASK 0xffffffff00000000ULL

#elif defined(CONFIG_8xx)
/* Definitions for 8xx embedded chips. */
#define _PAGE_PRESENT	0x0001	/* Page is valid */
#define _PAGE_NO_CACHE	0x0002	/* I: cache inhibit */
#define _PAGE_SHARED	0x0004	/* No ASID (context) compare */

/* These five software bits must be masked out when the entry is loaded
 * into the TLB.
 */
#define _PAGE_EXEC	0x0008	/* software: i-cache coherency required */
#define _PAGE_GUARDED	0x0010	/* software: guarded access */
#define _PAGE_DIRTY	0x0020	/* software: page changed */
#define _PAGE_RW	0x0040	/* software: user write access allowed */
#define _PAGE_ACCESSED	0x0080	/* software: page referenced */

/* Setting any bits in the nibble with the follow two controls will
 * require a TLB exception handler change.  It is assumed unused bits
 * are always zero.
 */
#define _PAGE_HWWRITE	0x0100	/* h/w write enable: never set in Linux PTE */
#define _PAGE_USER	0x0800	/* One of the PP bits, the other is USER&~RW */

#define _PMD_PRESENT	PAGE_MASK
#define _PMD_PAGE_MASK	0x000c
#define _PMD_PAGE_8M	0x000c

/*
 * The 8xx TLB miss handler allegedly sets _PAGE_ACCESSED in the PTE
 * for an address even if _PAGE_PRESENT is not set, as a performance
 * optimization.  This is a bug if you ever want to use swap unless
 * _PAGE_ACCESSED is 2, which it isn't, or unless you have 8xx-specific
 * definitions for __swp_entry etc. below, which would be gross.
 *  -- paulus
 */
#define _PTE_NONE_MASK	_PAGE_ACCESSED

#else /* CONFIG_6xx */
/* Definitions for 60x, 740/750, etc. */
#define _PAGE_PRESENT	0x001	/* software: pte contains a translation */
#define _PAGE_HASHPTE	0x002	/* hash_page has made an HPTE for this pte */
#define _PAGE_USER	0x004	/* usermode access allowed */
#define _PAGE_GUARDED	0x008	/* G: prohibit speculative access */
#define _PAGE_COHERENT	0x010	/* M: enforce memory coherence (SMP systems) */
#define _PAGE_NO_CACHE	0x020	/* I: cache inhibit */
#define _PAGE_WRITETHRU	0x040	/* W: cache write-through */
#define _PAGE_DIRTY	0x080	/* C: page changed */
#define _PAGE_ACCESSED	0x100	/* R: page referenced */
#define _PAGE_EXEC	0x200	/* software: i-cache coherency required */
#define _PAGE_RW	0x400	/* software: user write access allowed */
#define _PMD_PRESENT	PAGE_MASK

#define _PTE_NONE_MASK	_PAGE_HASHPTE

#endif

/*
 * Some bits are only used on some cpu families...
 */
#ifndef _PAGE_HASHPTE
#define _PAGE_HASHPTE	0
#endif
#ifndef _PTE_NONE_MASK
#define _PTE_NONE_MASK	0
#endif
#ifndef _PAGE_SHARED
#define _PAGE_SHARED	0
#endif
#ifndef _PAGE_HWWRITE
#define _PAGE_HWWRITE	0
#endif
#ifndef _PAGE_HWEXEC
#define _PAGE_HWEXEC	0
#endif
#ifndef _PAGE_EXEC
#define _PAGE_EXEC	0
#endif

#define _PAGE_CHG_MASK	(PAGE_MASK | _PAGE_ACCESSED | _PAGE_DIRTY)

/*
 * Note: the _PAGE_COHERENT bit automatically gets set in the hardware
 * PTE if CONFIG_SMP is defined (hash_page does this); there is no need
 * to have it in the Linux PTE, and in fact the bit could be reused for
 * another purpose.  -- paulus.
 */
#define _PAGE_BASE	_PAGE_PRESENT | _PAGE_ACCESSED
#define _PAGE_WRENABLE	_PAGE_RW | _PAGE_DIRTY | _PAGE_HWWRITE

/*
 * 44x wants _PAGE_GUARDED on all kernel pages for various reasons.
 * Allegedly that doesn't hurt performance.  -- paulus
 */
#ifdef CONFIG_44x
#define _PAGE_KERNEL	_PAGE_BASE | _PAGE_WRENABLE | _PAGE_SHARED | _PAGE_HWEXEC | _PAGE_GUARDED
#else
#define _PAGE_KERNEL	_PAGE_BASE | _PAGE_WRENABLE | _PAGE_SHARED | _PAGE_HWEXEC
#endif

#define _PAGE_IO	_PAGE_KERNEL | _PAGE_NO_CACHE | _PAGE_GUARDED

#define PAGE_NONE	__pgprot(_PAGE_BASE)
#define PAGE_READONLY	__pgprot(_PAGE_BASE | _PAGE_USER)
#define PAGE_READONLY_X	__pgprot(_PAGE_BASE | _PAGE_USER | _PAGE_EXEC)
#define PAGE_SHARED	__pgprot(_PAGE_BASE | _PAGE_USER | _PAGE_RW)
#define PAGE_SHARED_X	__pgprot(_PAGE_BASE | _PAGE_USER | _PAGE_RW | _PAGE_EXEC)
#define PAGE_COPY	__pgprot(_PAGE_BASE | _PAGE_USER)
#define PAGE_COPY_X	__pgprot(_PAGE_BASE | _PAGE_USER | _PAGE_EXEC)

#define PAGE_KERNEL	__pgprot(_PAGE_KERNEL)
#define PAGE_KERNEL_RO	__pgprot(_PAGE_BASE | _PAGE_SHARED)
#define PAGE_KERNEL_CI	__pgprot(_PAGE_IO)

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
extern unsigned long empty_zero_page[1024];
#define ZERO_PAGE(vaddr) (virt_to_page(empty_zero_page))

#endif /* __ASSEMBLY__ */

#define pte_none(pte)		((pte_val(pte) & ~_PTE_NONE_MASK) == 0)
#define pte_present(pte)	(pte_val(pte) & _PAGE_PRESENT)
#define pte_clear(ptep)		do { set_pte((ptep), __pte(0)); } while (0)

#define pmd_none(pmd)		(!pmd_val(pmd))
#define	pmd_bad(pmd)		((pmd_val(pmd) & _PMD_PRESENT) == 0)
#define	pmd_present(pmd)	((pmd_val(pmd) & _PMD_PRESENT) != 0)
#define	pmd_clear(pmdp)		do { pmd_val(*(pmdp)) = 0; } while (0)

#define pte_page(x)		(mem_map+(unsigned long)((pte_val(x)-PPC_MEMSTART) >> PAGE_SHIFT))

#ifndef __ASSEMBLY__
/*
 * The "pgd_xxx()" functions here are trivial for a folded two-level
 * setup: the pgd is never bad, and a pmd always exists (as it's folded
 * into the pgd entry)
 */
static inline int pgd_none(pgd_t pgd)		{ return 0; }
static inline int pgd_bad(pgd_t pgd)		{ return 0; }
static inline int pgd_present(pgd_t pgd)	{ return 1; }
#define pgd_clear(xp)				do { } while (0)
#define pgd_page(pgd) \
	((unsigned long) __va(pgd_val(pgd) & PAGE_MASK))

/*
 * The following only work if pte_present() is true.
 * Undefined behaviour if not..
 */
static inline int pte_read(pte_t pte)		{ return pte_val(pte) & _PAGE_USER; }
static inline int pte_write(pte_t pte)		{ return pte_val(pte) & _PAGE_RW; }
static inline int pte_exec(pte_t pte)		{ return pte_val(pte) & _PAGE_EXEC; }
static inline int pte_dirty(pte_t pte)		{ return pte_val(pte) & _PAGE_DIRTY; }
static inline int pte_young(pte_t pte)		{ return pte_val(pte) & _PAGE_ACCESSED; }

static inline void pte_uncache(pte_t pte)       { pte_val(pte) |= _PAGE_NO_CACHE; }
static inline void pte_cache(pte_t pte)         { pte_val(pte) &= ~_PAGE_NO_CACHE; }

static inline pte_t pte_rdprotect(pte_t pte) {
	pte_val(pte) &= ~_PAGE_USER; return pte; }
static inline pte_t pte_wrprotect(pte_t pte) {
	pte_val(pte) &= ~(_PAGE_RW | _PAGE_HWWRITE); return pte; }
static inline pte_t pte_exprotect(pte_t pte) {
	pte_val(pte) &= ~_PAGE_EXEC; return pte; }
static inline pte_t pte_mkclean(pte_t pte) {
	pte_val(pte) &= ~(_PAGE_DIRTY | _PAGE_HWWRITE); return pte; }
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

/*
 * Conversion functions: convert a page and protection to a page entry,
 * and a page entry and page directory to the page they refer to.
 */

static inline pte_t mk_pte_phys(phys_addr_t physpage, pgprot_t pgprot)
{
	pte_t pte;
	pte_val(pte) = physpage | pgprot_val(pgprot);
	return pte;
}

#define mk_pte(page,pgprot) \
({									\
	pte_t pte;							\
	pte_val(pte) = (((page - mem_map) << PAGE_SHIFT) + PPC_MEMSTART) | pgprot_val(pgprot); \
	pte;							\
})

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	pte_val(pte) = (pte_val(pte) & _PAGE_CHG_MASK) | pgprot_val(newprot);
	return pte;
}

/*
 * Atomic PTE updates.
 *
 * pte_update clears and sets bit atomically, and returns
 * the old pte value.
 * The ((unsigned long)(p+1) - 4) hack is to get to the least-significant
 * 32 bits of the PTE regardless of whether PTEs are 32 or 64 bits.
 */
static inline unsigned long pte_update(pte_t *p, unsigned long clr,
				       unsigned long set)
{
	unsigned long old, tmp;

	__asm__ __volatile__("\
1:	lwarx	%0,0,%3\n\
	andc	%1,%0,%4\n\
	or	%1,%1,%5\n"
	PPC405_ERR77(0,%3)
"	stwcx.	%1,0,%3\n\
	bne-	1b"
	: "=&r" (old), "=&r" (tmp), "=m" (*p)
	: "r" ((unsigned long)(p+1) - 4), "r" (clr), "r" (set), "m" (*p)
	: "cc" );
	return old;
}

/*
 * set_pte stores a linux PTE into the linux page table.
 * On machines which use an MMU hash table we avoid changing the
 * _PAGE_HASHPTE bit.
 */
static inline void set_pte(pte_t *ptep, pte_t pte)
{
#if _PAGE_HASHPTE != 0
	pte_update(ptep, ~_PAGE_HASHPTE, pte_val(pte) & ~_PAGE_HASHPTE);
#else
	*ptep = pte;
#endif
}

static inline int ptep_test_and_clear_young(pte_t *ptep)
{
	return (pte_update(ptep, _PAGE_ACCESSED, 0) & _PAGE_ACCESSED) != 0;
}

static inline int ptep_test_and_clear_dirty(pte_t *ptep)
{
	return (pte_update(ptep, (_PAGE_DIRTY | _PAGE_HWWRITE), 0) & _PAGE_DIRTY) != 0;
}

static inline pte_t ptep_get_and_clear(pte_t *ptep)
{
	return __pte(pte_update(ptep, ~_PAGE_HASHPTE, 0));
}

static inline void ptep_set_wrprotect(pte_t *ptep)
{
	pte_update(ptep, (_PAGE_RW | _PAGE_HWWRITE), 0);
}

static inline void ptep_mkdirty(pte_t *ptep)
{
	pte_update(ptep, 0, _PAGE_DIRTY);
}

#define pte_same(A,B)	(((pte_val(A) ^ pte_val(B)) & ~_PAGE_HASHPTE) == 0)

#define pmd_page(pmd)	(pmd_val(pmd) & PAGE_MASK)

/* to find an entry in a kernel page-table-directory */
#define pgd_offset_k(address) pgd_offset(&init_mm, address)

/* to find an entry in a page-table-directory */
#define pgd_index(address)	 ((address) >> PGDIR_SHIFT)
#define pgd_offset(mm, address)	 ((mm)->pgd + pgd_index(address))

/* Find an entry in the second-level page table.. */
static inline pmd_t * pmd_offset(pgd_t * dir, unsigned long address)
{
	return (pmd_t *) dir;
}

/* Find an entry in the third-level page table.. */
static inline pte_t * pte_offset(pmd_t * dir, unsigned long address)
{
	return (pte_t *) pmd_page(*dir) + ((address >> PAGE_SHIFT) & (PTRS_PER_PTE - 1));
}

extern pgd_t swapper_pg_dir[PTRS_PER_PGD];

extern void paging_init(void);

/*
 * When flushing the tlb entry for a page, we also need to flush the hash
 * table entry.  flush_hash_page is assembler (for speed) in hashtable.S.
 */
extern int flush_hash_page(unsigned context, unsigned long va, pte_t *ptep);

/* Add an HPTE to the hash table */
extern void add_hash_page(unsigned context, unsigned long va, pte_t *ptep);

/*
 * Encode and decode a swap entry.
 * Note that the bits we use in a PTE for representing a swap entry
 * must not include the _PAGE_PRESENT bit, or the _PAGE_HASHPTE bit
 * (if used).  -- paulus
 */
#define SWP_TYPE(entry)			((entry).val & 0x3f)
#define SWP_OFFSET(entry)		((entry).val >> 6)
#define SWP_ENTRY(type, offset)		((swp_entry_t) { (type) | ((offset) << 6) })
#define pte_to_swp_entry(pte)		((swp_entry_t) { pte_val(pte) >> 2 })
#define swp_entry_to_pte(x)		((pte_t) { (x).val << 2 })

/* CONFIG_APUS */
/* For virtual address to physical address conversion */
extern void cache_clear(__u32 addr, int length);
extern void cache_push(__u32 addr, int length);
extern int mm_end_of_chunk (unsigned long addr, int len);
extern unsigned long iopa(unsigned long addr);
extern unsigned long mm_ptov(unsigned long addr) __attribute__ ((const));

/* Values for nocacheflag and cmode */
/* These are not used by the APUS kernel_map, but prevents
   compilation errors. */
#define	IOMAP_FULL_CACHING	0
#define	IOMAP_NOCACHE_SER	1
#define	IOMAP_NOCACHE_NONSER	2
#define	IOMAP_NO_COPYBACK	3

/*
 * Map some physical address range into the kernel address space.
 */
extern unsigned long kernel_map(unsigned long paddr, unsigned long size,
				int nocacheflag, unsigned long *memavailp );

/*
 * Set cache mode of (kernel space) address range.
 */
extern void kernel_set_cachemode (unsigned long address, unsigned long size,
                                 unsigned int cmode);

/* Needs to be defined here and not in linux/mm.h, as it is arch dependent */
#define kern_addr_valid(addr)	(1)

#define io_remap_page_range remap_page_range

/*
 * No page table caches to initialise
 */
#define pgtable_cache_init()	do { } while (0)

#endif /* __ASSEMBLY__ */
#endif /* _PPC_PGTABLE_H */
#endif /* __KERNEL__ */
