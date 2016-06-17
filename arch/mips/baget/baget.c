/*
 * baget.c: Baget low level stuff
 *
 * Copyright (C) 1998 Gleb Raiko & Vladimir Roganov
 *
 */
#include <stdarg.h>

#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/system.h>
#include <asm/bootinfo.h>
#include <asm/mipsregs.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>

#include <asm/baget/baget.h>

/*
 *  Following code is based on routines from 'mm/vmalloc.c'
 *  Additional parameters  ioaddr  is needed to iterate across real I/O address.
 */
static inline int alloc_area_pte(pte_t * pte, unsigned long address,
				 unsigned long size, unsigned long ioaddr)
{
        unsigned long end;

        address &= ~PMD_MASK;
        end = address + size;
        if (end > PMD_SIZE)
                end = PMD_SIZE;
        while (address < end) {
                unsigned long page;
                if (!pte_none(*pte))
                        printk("kseg2_alloc_io: page already exists\n");
		/*
		 *  For MIPS looks pretty to have transparent mapping
		 *  for KSEG2 areas  -- user can't access one, and no
		 *  problems with  virtual <--> physical  translation.
		 */
                page = ioaddr & PAGE_MASK;

                set_pte(pte, __pte(page | pgprot_val(PAGE_USERIO) |
				  _PAGE_GLOBAL | __READABLE | __WRITEABLE));
                address += PAGE_SIZE;
		ioaddr  += PAGE_SIZE;
                pte++;
        }
        return 0;
}

static inline int alloc_area_pmd(pmd_t * pmd, unsigned long address,
				 unsigned long size, unsigned long ioaddr)
{
        unsigned long end;

        address &= ~PGDIR_MASK;
        end = address + size;
        if (end > PGDIR_SIZE)
                end = PGDIR_SIZE;
        while (address < end) {
                pte_t * pte = pte_alloc_kernel(pmd, address);
                if (!pte)
                        return -ENOMEM;
                if (alloc_area_pte(pte, address, end - address, ioaddr))
                        return -ENOMEM;
                address = (address + PMD_SIZE) & PMD_MASK;
		ioaddr  += PMD_SIZE;
                pmd++;
        }
        return 0;
}

int kseg2_alloc_io (unsigned long address, unsigned long size)
{
        pgd_t * dir;
        unsigned long end = address + size;

        dir = pgd_offset_k(address);
        flush_cache_all();
        while (address < end) {
                pmd_t *pmd;
                pgd_t olddir = *dir;

                pmd = pmd_alloc_kernel(dir, address);
                if (!pmd)
                        return -ENOMEM;
                if (alloc_area_pmd(pmd, address, end - address, address))
                        return -ENOMEM;
                if (pgd_val(olddir) != pgd_val(*dir))
                        set_pgdir(address, *dir);
                address = (address + PGDIR_SIZE) & PGDIR_MASK;
                dir++;
        }
        flush_tlb_all();
        return 0;
}
