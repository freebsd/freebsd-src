/*
 * This file contains the routines setting up the linux page tables.
 *  -- paulus
 *
 *  Derived from arch/ppc/mm/init.c:
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Modifications by Paul Mackerras (PowerMac) (paulus@cs.anu.edu.au)
 *  and Cort Dougan (PReP) (cort@cs.nmt.edu)
 *    Copyright (C) 1996 Paul Mackerras
 *  Amiga/APUS changes by Jesper Skov (jskov@cygnus.co.uk).
 *
 *  Derived from "arch/i386/mm/init.c"
 *    Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/init.h>

#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/io.h>
#include <asm/machdep.h>

#include "mmu_decl.h"

unsigned long ioremap_base;
unsigned long ioremap_bot;
int io_bat_index;

/* The maximum lowmem defaults to 768Mb, but this can be configured to
 * another value.  On SMP, this value will be trimmed down to whatever
 * can be covered by BATs.
 */
#define MAX_LOW_MEM	CONFIG_LOWMEM_SIZE

#ifndef CONFIG_SMP
struct pgtable_cache_struct quicklists;
#endif

#if defined(CONFIG_6xx) || defined(CONFIG_POWER3)
#define HAVE_BATS	1
#endif

#ifdef HAVE_BATS
static unsigned long __bat2, __bat3;
#endif

extern char etext[], _stext[];

#ifdef HAVE_BATS
extern unsigned long v_mapped_by_bats(unsigned long va);
extern unsigned long p_mapped_by_bats(unsigned long pa);
void setbat(int index, unsigned long virt, unsigned long phys,
	    unsigned int size, int flags);

#else /* !HAVE_BATS */
#define v_mapped_by_bats(x)	(0UL)
#define p_mapped_by_bats(x)	(0UL)
#endif /* HAVE_BATS */

#ifdef CONFIG_PTE_64BIT
void *
ioremap(phys_addr_t addr, unsigned long size)
{
	phys_addr_t addr64 = fixup_bigphys_addr(addr, size);;

	return ioremap64(addr64, size);
}

void *
ioremap64(unsigned long long addr, unsigned long size)
{
	return __ioremap(addr, size, _PAGE_NO_CACHE);
}

#else /* !CONFIG_PTE_64BIT */

void *
ioremap(phys_addr_t addr, unsigned long size)
{
	return __ioremap(addr, size, _PAGE_NO_CACHE);
}
#endif /* CONFIG_PTE_64BIT */

void *
__ioremap(phys_addr_t addr, unsigned long size, unsigned long flags)
{
	unsigned long v, i;
	phys_addr_t p;
	int err;

	/*
	 * Choose an address to map it to.
	 * Once the vmalloc system is running, we use it.
	 * Before then, we use space going down from ioremap_base
	 * (ioremap_bot records where we're up to).
	 */
	p = addr & PAGE_MASK;
	size = PAGE_ALIGN(addr + size) - p;

	/*
	 * If the address lies within the first 16 MB, assume it's in ISA
	 * memory space
	 */
	if (p < 16*1024*1024)
		p += _ISA_MEM_BASE;

	/*
	 * Don't allow anybody to remap normal RAM that we're using.
	 * mem_init() sets high_memory so only do the check after that.
	 */
	if ( mem_init_done && (p < virt_to_phys(high_memory)) )
	{
		printk("__ioremap(): phys addr "PTE_FMT" is RAM lr %p\n", p,
		       __builtin_return_address(0));
		return NULL;
	}

	if (size == 0)
		return NULL;

	/*
	 * Is it already mapped?  Perhaps overlapped by a previous
	 * BAT mapping.  If the whole area is mapped then we're done,
	 * otherwise remap it since we want to keep the virt addrs for
	 * each request contiguous.
	 *
	 * We make the assumption here that if the bottom and top
	 * of the range we want are mapped then it's mapped to the
	 * same virt address (and this is contiguous).
	 *  -- Cort
	 */
	if ((v = p_mapped_by_bats(p)) /*&& p_mapped_by_bats(p+size-1)*/ )
		goto out;

	if (mem_init_done) {
		struct vm_struct *area;
		area = get_vm_area(size, VM_IOREMAP);
		if (area == 0)
			return NULL;
		v = VMALLOC_VMADDR(area->addr);
	} else {
		v = (ioremap_bot -= size);
	}

	if ((flags & _PAGE_PRESENT) == 0)
		flags |= _PAGE_KERNEL;
	if (flags & _PAGE_NO_CACHE)
		flags |= _PAGE_GUARDED;

	/*
	 * Should check if it is a candidate for a BAT mapping
	 */

	err = 0;
	for (i = 0; i < size && err == 0; i += PAGE_SIZE)
		err = map_page(v+i, p+i, flags);
	if (err) {
		if (mem_init_done)
			vfree((void *)v);
		return NULL;
	}

out:
	return (void *) (v + ((unsigned long)addr & ~PAGE_MASK));
}

void iounmap(void *addr)
{
	/*
	 * If mapped by BATs then there is nothing to do.
	 * Calling vfree() generates a benign warning.
	 */
	if (v_mapped_by_bats((unsigned long)addr)) return;

	if (addr > high_memory && (unsigned long) addr < ioremap_bot)
		vfree((void *) (PAGE_MASK & (unsigned long) addr));
}

int
map_page(unsigned long va, phys_addr_t pa, int flags)
{
	pmd_t *pd;
	pte_t *pg;
	int err = -ENOMEM;

	spin_lock(&init_mm.page_table_lock);
	/* Use upper 10 bits of VA to index the first level map */
	pd = pmd_offset(pgd_offset_k(va), va);
	/* Use middle 10 bits of VA to index the second-level map */
	pg = pte_alloc(&init_mm, pd, va);
	if (pg != 0) {
		err = 0;
		set_pte(pg, mk_pte_phys(pa & PAGE_MASK, __pgprot(flags)));
		if (mem_init_done)
			flush_HPTE(0, va, pg);
	}
	spin_unlock(&init_mm.page_table_lock);
	return err;
}

void __init
adjust_total_lowmem(void)
{
	unsigned long max_low_mem = MAX_LOW_MEM;

#ifdef HAVE_BATS
	unsigned long bat_max = 0x10000000;
	unsigned long align;
	unsigned long ram;
	int is601 = 0;

	/* 601s have smaller BATs */
	if (PVR_VER(mfspr(PVR)) == 1) {
		bat_max = 0x00800000;
		is601 = 1;
	}

	/* adjust BAT block size to max_low_mem */
	if (max_low_mem < bat_max)
		bat_max = max_low_mem;

	/* adjust lowmem size to max_low_mem */
	if (max_low_mem < total_lowmem)
		ram = max_low_mem;
	else
		ram = total_lowmem;

	/* Make sure we don't map a block larger than the
	   smallest alignment of the physical address. */
	/* alignment of PPC_MEMSTART */
	align = ~(PPC_MEMSTART-1) & PPC_MEMSTART;
	/* set BAT block size to MIN(max_size, align) */
	if (align && align < bat_max)
		bat_max = align;

	/* Calculate BAT values */
	__bat2 = 1UL << __ilog2(ram);
	if (__bat2 > bat_max)
		__bat2 = bat_max;
	ram -= __bat2;
	if (ram) {
		__bat3 = 1UL << __ilog2(ram);
		if (__bat3 > bat_max)
			__bat3 = bat_max;
		ram -= __bat3;
	}

	printk(KERN_INFO "Memory BAT mapping: BAT2=%ldMb, BAT3=%ldMb,"
			" residual: %ldMb\n", __bat2 >> 20, __bat3 >> 20,
			(total_lowmem - (__bat2 + __bat3)) >> 20);

	/* On SMP, we limit the lowmem to the area mapped with BATs.
	 * We also assume nobody will do SMP with 601s
	 */
#ifdef CONFIG_SMP
	if (!is601)
		max_low_mem = __bat2 + __bat3;
#endif /* CONFIG_SMP */

#endif /* HAVE_BATS */
	if (total_lowmem > max_low_mem) {
		total_lowmem = max_low_mem;
#ifndef CONFIG_HIGHMEM
		printk(KERN_INFO "Warning, memory limited to %ld Mb, use "
				"CONFIG_HIGHMEM to reach %ld Mb\n",
				max_low_mem >> 20, total_memory >> 20);
		total_memory = total_lowmem;
#endif /* CONFIG_HIGHMEM */
	}
}

/*
 * Map in all of physical memory starting at KERNELBASE.
 */
void __init mapin_ram(void)
{
	unsigned long v, p, s, f;

#ifdef HAVE_BATS
	if (!__map_without_bats)
		bat_mapin_ram(__bat2, __bat3);
#endif /* HAVE_BATS */

	v = KERNELBASE;
	p = PPC_MEMSTART;
	for (s = 0; s < total_lowmem; s += PAGE_SIZE) {
		/* On the MPC8xx, we want the page shared so we
		 * don't get ASID compares on kernel space.
		 */
		f = _PAGE_PRESENT | _PAGE_ACCESSED | _PAGE_SHARED | _PAGE_HWEXEC;
#if defined(CONFIG_KGDB) || defined(CONFIG_XMON) || defined(CONFIG_BDI_SWITCH)
		/* Allows stub to set breakpoints everywhere */
		f |= _PAGE_WRENABLE;
#else	/* !CONFIG_KGDB && !CONFIG_XMON && !CONFIG_BDI_SWITCH */
		if ((char *) v < _stext || (char *) v >= etext)
			f |= _PAGE_WRENABLE;
#ifdef CONFIG_PPC_STD_MMU
		else
			/* On the powerpc (not all), no user access
			   forces R/W kernel access */
			f |= _PAGE_USER;
#endif /* CONFIG_PPC_STD_MMU */
#endif /* CONFIG_KGDB || CONFIG_XMON */
		map_page(v, p, f);
		v += PAGE_SIZE;
		p += PAGE_SIZE;
	}
	if (ppc_md.progress)
		ppc_md.progress("MMU:mapin_ram done", 0x401);
}

/* is x a power of 2? */
#define is_power_of_2(x)	((x) != 0 && (((x) & ((x) - 1)) == 0))

/*
 * Set up a mapping for a block of I/O.
 * virt, phys, size must all be page-aligned.
 * This should only be called before ioremap is called.
 */
void __init io_block_mapping(unsigned long virt, phys_addr_t phys,
			     unsigned int size, int flags)
{
	int i;

	if (virt > KERNELBASE && virt < ioremap_bot)
		ioremap_bot = ioremap_base = virt;

#ifdef HAVE_BATS
	/*
	 * Use a BAT for this if possible...
	 */
	if (io_bat_index < 2 && is_power_of_2(size)
	    && (virt & (size - 1)) == 0 && (phys & (size - 1)) == 0) {
		setbat(io_bat_index, virt, phys, size, flags);
		++io_bat_index;
		return;
	}
#endif /* HAVE_BATS */

	/* No BATs available, put it in the page tables. */
	for (i = 0; i < size; i += PAGE_SIZE)
		map_page(virt + i, phys + i, flags);
}

/* Scan the real Linux page tables and return a PTE pointer for
 * a virtual address in a context.
 * Returns true (1) if PTE was found, zero otherwise.  The pointer to
 * the PTE pointer is unmodified if PTE is not found.
 */
int
get_pteptr(struct mm_struct *mm, unsigned long addr, pte_t **ptep)
{
        pgd_t	*pgd;
        pmd_t	*pmd;
        pte_t	*pte;
        int     retval = 0;

        pgd = pgd_offset(mm, addr & PAGE_MASK);
        if (pgd) {
                pmd = pmd_offset(pgd, addr & PAGE_MASK);
                if (pmd_present(*pmd)) {
                        pte = pte_offset(pmd, addr & PAGE_MASK);
                        if (pte) {
				retval = 1;
				*ptep = pte;
                        }
                }
        }
        return(retval);
}

/* Find physical address for this virtual address.  Normally used by
 * I/O functions, but anyone can call it.
 */
unsigned long iopa(unsigned long addr)
{
	unsigned long pa;

	/* I don't know why this won't work on PMacs or CHRP.  It
	 * appears there is some bug, or there is some implicit
	 * mapping done not properly represented by BATs or in page
	 * tables.......I am actively working on resolving this, but
	 * can't hold up other stuff.  -- Dan
	 */
	pte_t *pte;
	struct mm_struct *mm;

	/* Check the BATs */
	pa = v_mapped_by_bats(addr);
	if (pa)
		return pa;

	/* Allow mapping of user addresses (within the thread)
	 * for DMA if necessary.
	 */
	if (addr < TASK_SIZE)
		mm = current->mm;
	else
		mm = &init_mm;

	pa = 0;
	if (get_pteptr(mm, addr, &pte))
		pa = (pte_val(*pte) & PAGE_MASK) | (addr & ~PAGE_MASK);

	return(pa);
}

/* This is will find the virtual address for a physical one....
 * Swiped from APUS, could be dangerous :-).
 * This is only a placeholder until I really find a way to make this
 * work.  -- Dan
 */
unsigned long
mm_ptov (unsigned long paddr)
{
	unsigned long ret;
#if 0
	if (paddr < 16*1024*1024)
		ret = ZTWO_VADDR(paddr);
	else {
		int i;

		for (i = 0; i < kmap_chunk_count;){
			unsigned long phys = kmap_chunks[i++];
			unsigned long size = kmap_chunks[i++];
			unsigned long virt = kmap_chunks[i++];
			if (paddr >= phys
			    && paddr < (phys + size)){
				ret = virt + paddr - phys;
				goto exit;
			}
		}

		ret = (unsigned long) __va(paddr);
	}
exit:
#ifdef DEBUGPV
	printk ("PTOV(%lx)=%lx\n", paddr, ret);
#endif
#else
	ret = (unsigned long)paddr + KERNELBASE;
#endif
	return ret;
}

