/* $Id: cache-sh3.c,v 1.6 2001/09/10 08:59:59 dwmw2 Exp $
 *
 *  linux/arch/sh/mm/cache-sh3.c
 *
 * Copyright (C) 1999, 2000  Niibe Yutaka
 *
 */

#include <linux/init.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/threads.h>
#include <asm/addrspace.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/cache.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/pgalloc.h>
#include <asm/mmu_context.h>


#define CCR		0xffffffec	/* Address of Cache Control Register */

#define CCR_CACHE_CE	0x01	/* Cache Enable */
#define CCR_CACHE_WT	0x02	/* Write-Through (for P0,U0,P3) (else writeback) */
#define CCR_CACHE_CB	0x04	/* Write-Back (for P1) (else writethrough) */
#define CCR_CACHE_CF	0x08	/* Cache Flush */
#define CCR_CACHE_RA	0x20	/* RAM mode */

#define CCR_CACHE_VAL	(CCR_CACHE_CB|CCR_CACHE_CE)	/* 8k-byte cache, P1-wb, enable */
#define CCR_CACHE_INIT	(CCR_CACHE_CF|CCR_CACHE_VAL)	/* 8k-byte cache, CF, P1-wb, enable */

#define CACHE_OC_ADDRESS_ARRAY 0xf0000000
#define CACHE_VALID	  1
#define CACHE_UPDATED	  2
#define CACHE_PHYSADDR_MASK 0x1ffffc00

/* 7709A/7729 has 16K cache (256-entry), while 7702 has only 2K(direct)
   7702 is not supported (yet) */
struct _cache_system_info {
	int way_shift;
	int entry_mask;
	int num_entries;
};

/* Data at BSS is cleared after setting this variable.
   So, we Should not placed this variable at BSS section.
   Initialize this, it is placed at data section. */
static struct _cache_system_info cache_system_info = {0,};

#define CACHE_OC_WAY_SHIFT	(cache_system_info.way_shift)
#define CACHE_OC_ENTRY_SHIFT    4
#define CACHE_OC_ENTRY_MASK	(cache_system_info.entry_mask)
#define CACHE_OC_NUM_ENTRIES	(cache_system_info.num_entries)
#define CACHE_OC_NUM_WAYS	4
#define CACHE_OC_ASSOC_BIT    (1<<3)


/*
 * Write back all the cache.
 *
 * For SH-4, we only need to flush (write back) Operand Cache,
 * as Instruction Cache doesn't have "updated" data.
 *
 * Assumes that this is called in interrupt disabled context, and P2.
 * Shuld be INLINE function.
 */
static inline void cache_wback_all(void)
{
	unsigned long addr, data, i, j;

	for (i=0; i<CACHE_OC_NUM_ENTRIES; i++) {
		for (j=0; j<CACHE_OC_NUM_WAYS; j++) {
			addr = CACHE_OC_ADDRESS_ARRAY|(j<<CACHE_OC_WAY_SHIFT)|
				(i<<CACHE_OC_ENTRY_SHIFT);
			data = ctrl_inl(addr);
			if ((data & (CACHE_UPDATED|CACHE_VALID))
			    == (CACHE_UPDATED|CACHE_VALID))
				ctrl_outl(data & ~CACHE_UPDATED, addr);
		}
	}
}

static void __init
detect_cpu_and_cache_system(void)
{
	unsigned long addr0, addr1, data0, data1, data2, data3;

	jump_to_P2();
	/*
	 * Check if the entry shadows or not.
	 * When shadowed, it's 128-entry system.
	 * Otherwise, it's 256-entry system.
	 */
	addr0 = CACHE_OC_ADDRESS_ARRAY + (3 << 12);
	addr1 = CACHE_OC_ADDRESS_ARRAY + (1 << 12);

	/* First, write back & invalidate */
	data0  = ctrl_inl(addr0);
	ctrl_outl(data0&~(CACHE_VALID|CACHE_UPDATED), addr0);
	data1  = ctrl_inl(addr1);
	ctrl_outl(data1&~(CACHE_VALID|CACHE_UPDATED), addr1);

	/* Next, check if there's shadow or not */
	data0 = ctrl_inl(addr0);
	data0 ^= CACHE_VALID;
	ctrl_outl(data0, addr0);
	data1 = ctrl_inl(addr1);
	data2 = data1 ^ CACHE_VALID;
	ctrl_outl(data2, addr1);
	data3 = ctrl_inl(addr0);

	/* Lastly, invaliate them. */
	ctrl_outl(data0&~CACHE_VALID, addr0);
	ctrl_outl(data2&~CACHE_VALID, addr1);
	back_to_P1();

	if (data0 == data1 && data2 == data3) {	/* Shadow */
		cache_system_info.way_shift = 11;
		cache_system_info.entry_mask = 0x7f0;
		cache_system_info.num_entries = 128;
		cpu_data->type = CPU_SH7708;
	} else {				/* 7709A or 7729  */
		cache_system_info.way_shift = 12;
		cache_system_info.entry_mask = 0xff0;
		cache_system_info.num_entries = 256;
		cpu_data->type = CPU_SH7729;
	}
}

void __init cache_init(void)
{
	unsigned long ccr;

	detect_cpu_and_cache_system();

	jump_to_P2();
	ccr = ctrl_inl(CCR);
	if (ccr & CCR_CACHE_CE)
		/*
		 * XXX: Should check RA here. 
		 * If RA was 1, we only need to flush the half of the caches.
		 */
		cache_wback_all();

	ctrl_outl(CCR_CACHE_INIT, CCR);
	back_to_P1();
}

/*
 * Write back the dirty D-caches, but not invalidate them.
 *
 * Is this really worth it, or should we just alias this routine
 * to __flush_purge_region too?
 *
 * START: Virtual Address (U0, P1, or P3)
 * SIZE: Size of the region.
 */

void __flush_wback_region(void *start, int size)
{
	unsigned long v, j;
	unsigned long begin, end;
	unsigned long flags;

	begin = (unsigned long)start & ~(L1_CACHE_BYTES-1);
	end = ((unsigned long)start + size + L1_CACHE_BYTES-1)
		& ~(L1_CACHE_BYTES-1);

	for (v = begin; v < end; v+=L1_CACHE_BYTES) {
		for (j=0; j<CACHE_OC_NUM_WAYS; j++) {
			unsigned long data, addr, p;

			p = __pa(v);
			addr = CACHE_OC_ADDRESS_ARRAY|(j<<CACHE_OC_WAY_SHIFT)|
				(v&CACHE_OC_ENTRY_MASK);
			save_and_cli(flags);
			data = ctrl_inl(addr);
			
			if ((data & CACHE_PHYSADDR_MASK) ==
			    (p & CACHE_PHYSADDR_MASK)) {
				data &= ~CACHE_UPDATED;
				ctrl_outl(data, addr);
				restore_flags(flags);
				break;
			}
			restore_flags(flags);
		}
	}
}

/*
 * Write back the dirty D-caches and invalidate them.
 *
 * START: Virtual Address (U0, P1, or P3)
 * SIZE: Size of the region.
 */
void __flush_purge_region(void *start, int size)
{
	unsigned long v;
	unsigned long begin, end;

	begin = (unsigned long)start & ~(L1_CACHE_BYTES-1);
	end = ((unsigned long)start + size + L1_CACHE_BYTES-1)
		& ~(L1_CACHE_BYTES-1);

	for (v = begin; v < end; v+=L1_CACHE_BYTES) {
		unsigned long data, addr;

		data = (v & 0xfffffc00); /* _Virtual_ address, ~U, ~V */
		addr = CACHE_OC_ADDRESS_ARRAY | (v&CACHE_OC_ENTRY_MASK) | 
			CACHE_OC_ASSOC_BIT;
		ctrl_outl(data, addr);
	}
}

/*
 * No write back please
 *
 * Except I don't think there's any way to avoid the writeback. So we
 * just alias it to __flush_purge_region(). dwmw2.
 */
void __flush_invalidate_region(void *start, int size)
	__attribute__((alias("__flush_purge_region")));
