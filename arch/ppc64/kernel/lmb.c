/*
 *
 * Procedures for interfacing to Open Firmware.
 *
 * Peter Bergner, IBM Corp.	June 2001.
 * Copyright (C) 2001 Peter Bergner.
 * 
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <asm/types.h>
#include <asm/page.h>
#include <asm/prom.h>
#include <asm/lmb.h>
#include <asm/abs_addr.h>
#include <asm/bitops.h>
#include <asm/udbg.h>

extern unsigned long klimit;
extern unsigned long reloc_offset(void);


static long lmb_add_region(struct lmb_region *, unsigned long, unsigned long, unsigned long);

struct lmb lmb = {
	0, 0,
	{0,0,0,0,{{0,0,0}}},
	{0,0,0,0,{{0,0,0}}}
};


/* Assumption: base addr of region 1 < base addr of region 2 */
static void
lmb_coalesce_regions(struct lmb_region *rgn, unsigned long r1, unsigned long r2)
{
	unsigned long i;

	rgn->region[r1].size += rgn->region[r2].size;
	for (i=r2; i < rgn->cnt-1 ;i++) {
		rgn->region[i].base = rgn->region[i+1].base;
		rgn->region[i].physbase = rgn->region[i+1].physbase;
		rgn->region[i].size = rgn->region[i+1].size;
		rgn->region[i].type = rgn->region[i+1].type;
	}
	rgn->cnt--;
}


/* This routine called with relocation disabled. */
void
lmb_init(void)
{
	unsigned long offset = reloc_offset();
	struct lmb *_lmb = PTRRELOC(&lmb);

	/* Create a dummy zero size LMB which will get coalesced away later.
	 * This simplifies the lmb_add() code below...
	 */
	_lmb->memory.region[0].base = 0;
	_lmb->memory.region[0].size = 0;
	_lmb->memory.region[0].type = LMB_MEMORY_AREA;
	_lmb->memory.cnt = 1;

	/* Ditto. */
	_lmb->reserved.region[0].base = 0;
	_lmb->reserved.region[0].size = 0;
	_lmb->reserved.region[0].type = LMB_MEMORY_AREA;
	_lmb->reserved.cnt = 1;
}

/* This routine called with relocation disabled. */
void
lmb_analyze(void)
{
	unsigned long i;
	unsigned long mem_size = 0;
	unsigned long io_size = 0;
	unsigned long size_mask = 0;
	unsigned long offset = reloc_offset();
	struct lmb *_lmb = PTRRELOC(&lmb);
#ifdef CONFIG_MSCHUNKS
	unsigned long physbase = 0;
#endif

	for (i=0; i < _lmb->memory.cnt ;i++) {
		unsigned long lmb_type = _lmb->memory.region[i].type;
		unsigned long lmb_size;

		if ( lmb_type != LMB_MEMORY_AREA )
			continue;

		lmb_size = _lmb->memory.region[i].size;

#ifdef CONFIG_MSCHUNKS
		_lmb->memory.region[i].physbase = physbase;
		physbase += lmb_size;
#else
		_lmb->memory.region[i].physbase = _lmb->memory.region[i].base;
#endif
		mem_size += lmb_size;
		size_mask |= lmb_size;
	}

#ifdef CONFIG_MSCHUNKS
	for (i=0; i < _lmb->memory.cnt ;i++) {
		unsigned long lmb_type = _lmb->memory.region[i].type;
		unsigned long lmb_size;

		if ( lmb_type != LMB_IO_AREA )
			continue;

		lmb_size = _lmb->memory.region[i].size;

		_lmb->memory.region[i].physbase = physbase;
		physbase += lmb_size;
		io_size += lmb_size;
		size_mask |= lmb_size;
	}
#endif /* CONFIG_MSCHUNKS */

	_lmb->memory.size = mem_size;
	_lmb->memory.iosize = io_size;
	_lmb->memory.lcd_size = (1UL << cnt_trailing_zeros(size_mask));
}

/* This routine called with relocation disabled. */
long
lmb_add(unsigned long base, unsigned long size)
{
	unsigned long offset = reloc_offset();
	struct lmb *_lmb = PTRRELOC(&lmb);
	struct lmb_region *_rgn = &(_lmb->memory);

	/* On pSeries LPAR systems, the first LMB is our RMO region. */
	if ( base == 0 )
		_lmb->rmo_size = size;

	return lmb_add_region(_rgn, base, size, LMB_MEMORY_AREA);

}

#ifdef CONFIG_MSCHUNKS
/* This routine called with relocation disabled. */
long
lmb_add_io(unsigned long base, unsigned long size)
{
	unsigned long offset = reloc_offset();
	struct lmb *_lmb = PTRRELOC(&lmb);
	struct lmb_region *_rgn = &(_lmb->memory);

	return lmb_add_region(_rgn, base, size, LMB_IO_AREA);

}
#endif /* CONFIG_MSCHUNKS */

long
lmb_reserve(unsigned long base, unsigned long size)
{
	unsigned long offset = reloc_offset();
	struct lmb *_lmb = PTRRELOC(&lmb);
	struct lmb_region *_rgn = &(_lmb->reserved);

	return lmb_add_region(_rgn, base, size, LMB_MEMORY_AREA);
}

/* This routine called with relocation disabled. */
static long
lmb_add_region(struct lmb_region *rgn, unsigned long base, unsigned long size,
		unsigned long type)
{
	unsigned long i, coalesced = 0;
	long adjacent;

	/* First try and coalesce this LMB with another. */
	for (i=0; i < rgn->cnt ;i++) {
		unsigned long rgnbase = rgn->region[i].base;
		unsigned long rgnsize = rgn->region[i].size;
		unsigned long rgntype = rgn->region[i].type;

		if ( rgntype != type )
			continue;

		adjacent = lmb_addrs_adjacent(base,size,rgnbase,rgnsize);
		if ( adjacent > 0 ) {
			rgn->region[i].base -= size;
			rgn->region[i].physbase -= size;
			rgn->region[i].size += size;
			coalesced++;
			break;
		}
		else if ( adjacent < 0 ) {
			rgn->region[i].size += size;
			coalesced++;
			break;
		}
	}

	if ((i < rgn->cnt-1) && lmb_regions_adjacent(rgn, i, i+1) ) {
		lmb_coalesce_regions(rgn, i, i+1);
		coalesced++;
	}

	if ( coalesced ) {
		return coalesced;
	} else if ( rgn->cnt >= MAX_LMB_REGIONS ) {
		return -1;
	}

	/* Couldn't coalesce the LMB, so add it to the sorted table. */
	for (i=rgn->cnt-1; i >= 0 ;i--) {
		if (base < rgn->region[i].base) {
			rgn->region[i+1].base = rgn->region[i].base;
			rgn->region[i+1].physbase = rgn->region[i].physbase;
			rgn->region[i+1].size = rgn->region[i].size;
			rgn->region[i+1].type = rgn->region[i].type;
		}  else {
			rgn->region[i+1].base = base;
			rgn->region[i+1].physbase = lmb_abs_to_phys(base);
			rgn->region[i+1].size = size;
			rgn->region[i+1].type = type;
			break;
		}
	}
	rgn->cnt++;

	return 0;
}

long
lmb_overlaps_region(struct lmb_region *rgn, unsigned long base, unsigned long size)
{
	unsigned long i;

	for (i=0; i < rgn->cnt ;i++) {
		unsigned long rgnbase = rgn->region[i].base;
		unsigned long rgnsize = rgn->region[i].size;
		if ( lmb_addrs_overlap(base,size,rgnbase,rgnsize) ) {
			break;
		}
	}

	return (i < rgn->cnt) ? i : -1;
}

unsigned long
lmb_alloc(unsigned long size, unsigned long align)
{
	return lmb_alloc_base(size, align, LMB_ALLOC_ANYWHERE);
}

unsigned long
lmb_alloc_base(unsigned long size, unsigned long align, unsigned long max_addr)
{
	long i, j;
	unsigned long base = 0;
	unsigned long offset = reloc_offset();
	struct lmb *_lmb = PTRRELOC(&lmb);
	struct lmb_region *_mem = &(_lmb->memory);
	struct lmb_region *_rsv = &(_lmb->reserved);

	for (i=_mem->cnt-1; i >= 0 ;i--) {
		unsigned long lmbbase = _mem->region[i].base;
		unsigned long lmbsize = _mem->region[i].size;
		unsigned long lmbtype = _mem->region[i].type;

		if ( lmbtype != LMB_MEMORY_AREA )
			continue;

		if ( max_addr == LMB_ALLOC_ANYWHERE )
			base = _ALIGN_DOWN(lmbbase+lmbsize-size, align);
		else if ( lmbbase < max_addr )
			base = _ALIGN_DOWN(min(lmbbase+lmbsize,max_addr)-size, align);
		else
			continue;

		while ( (lmbbase <= base) &&
			((j = lmb_overlaps_region(_rsv,base,size)) >= 0) ) {
			base = _ALIGN_DOWN(_rsv->region[j].base-size, align);
		}

		if ( (base != 0) && (lmbbase <= base) )
			break;
	}

	if ( i < 0 )
		return 0;

	lmb_add_region(_rsv, base, size, LMB_MEMORY_AREA);

	return base;
}

unsigned long
lmb_phys_mem_size(void)
{
	unsigned long offset = reloc_offset();
	struct lmb *_lmb = PTRRELOC(&lmb);
#ifdef CONFIG_MSCHUNKS
	return _lmb->memory.size;
#else
	struct lmb_region *_mem = &(_lmb->memory);
	unsigned long idx = _mem->cnt-1;
	unsigned long lastbase = _mem->region[idx].physbase;
	unsigned long lastsize = _mem->region[idx].size;
	
	return (lastbase + lastsize);
#endif /* CONFIG_MSCHUNKS */
}

unsigned long
lmb_end_of_DRAM(void)
{
	unsigned long offset = reloc_offset();
	struct lmb *_lmb = PTRRELOC(&lmb);
	struct lmb_region *_mem = &(_lmb->memory);
	unsigned long idx;

	for(idx=_mem->cnt-1; idx >= 0 ;idx--) {
		if ( _mem->region[idx].type != LMB_MEMORY_AREA )
			continue;
#ifdef CONFIG_MSCHUNKS
		return (_mem->region[idx].physbase + _mem->region[idx].size);
#else
		return (_mem->region[idx].base + _mem->region[idx].size);
#endif /* CONFIG_MSCHUNKS */
	}

	return 0;
}


unsigned long
lmb_abs_to_phys(unsigned long aa)
{
	unsigned long i, pa = aa;
	unsigned long offset = reloc_offset();
	struct lmb *_lmb = PTRRELOC(&lmb);
	struct lmb_region *_mem = &(_lmb->memory);

	for (i=0; i < _mem->cnt ;i++) {
		unsigned long lmbbase = _mem->region[i].base;
		unsigned long lmbsize = _mem->region[i].size;
		if ( lmb_addrs_overlap(aa,1,lmbbase,lmbsize) ) {
			pa = _mem->region[i].physbase + (aa - lmbbase);
			break;
		}
	}

	return pa;
}

void
lmb_dump(char *str)
{
	unsigned long i;

	udbg_printf("\nlmb_dump: %s\n", str);
	udbg_printf("    debug                       = %s\n",
		(lmb.debug) ? "TRUE" : "FALSE");
	udbg_printf("    memory.cnt                  = %d\n",
		lmb.memory.cnt);
	udbg_printf("    memory.size                 = 0x%lx\n",
		lmb.memory.size);
	udbg_printf("    memory.lcd_size             = 0x%lx\n",
		lmb.memory.lcd_size);
	for (i=0; i < lmb.memory.cnt ;i++) {
		udbg_printf("    memory.region[%d].base       = 0x%lx\n",
			i, lmb.memory.region[i].base);
		udbg_printf("                      .physbase = 0x%lx\n",
			lmb.memory.region[i].physbase);
		udbg_printf("                      .size     = 0x%lx\n",
			lmb.memory.region[i].size);
		udbg_printf("                      .type     = 0x%lx\n",
			lmb.memory.region[i].type);
	}

	udbg_printf("\n");
	udbg_printf("    reserved.cnt                = %d\n",
		lmb.reserved.cnt);
	udbg_printf("    reserved.size               = 0x%lx\n",
		lmb.reserved.size);
	udbg_printf("    reserved.lcd_size           = 0x%lx\n",
		lmb.reserved.lcd_size);
	for (i=0; i < lmb.reserved.cnt ;i++) {
		udbg_printf("    reserved.region[%d].base     = 0x%lx\n",
			i, lmb.reserved.region[i].base);
		udbg_printf("                      .physbase = 0x%lx\n",
			lmb.reserved.region[i].physbase);
		udbg_printf("                      .size     = 0x%lx\n",
			lmb.reserved.region[i].size);
		udbg_printf("                      .type     = 0x%lx\n",
			lmb.reserved.region[i].type);
	}
}
