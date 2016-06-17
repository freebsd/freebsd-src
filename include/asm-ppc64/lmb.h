#ifndef _PPC64_LMB_H
#define _PPC64_LMB_H

/*
 * Definitions for talking to the Open Firmware PROM on
 * Power Macintosh computers.
 *
 * Copyright (C) 2001 Peter Bergner, IBM Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <asm/prom.h>

extern unsigned long reloc_offset(void);

#define MAX_LMB_REGIONS 64

union lmb_reg_property { 
	struct reg_property32 addr32[MAX_LMB_REGIONS];
	struct reg_property64 addr64[MAX_LMB_REGIONS];
};

#define LMB_MEMORY_AREA	1
#define LMB_IO_AREA	2

#define LMB_ALLOC_ANYWHERE	0
#define LMB_ALLOC_FIRST4GBYTE	(1UL<<32)

struct lmb_property {
	unsigned long base;
	unsigned long physbase;
	unsigned long size;
	unsigned long type;
};

struct lmb_region {
	unsigned long cnt;
	unsigned long size;
	unsigned long iosize;
	unsigned long lcd_size;		/* Least Common Denominator */
	struct lmb_property region[MAX_LMB_REGIONS+1];
};

struct lmb {
	unsigned long debug;
	unsigned long rmo_size;
	struct lmb_region memory;
	struct lmb_region reserved;
};

extern struct lmb lmb;

extern void lmb_init(void);
extern void lmb_analyze(void);
extern long lmb_add(unsigned long, unsigned long);
#ifdef CONFIG_MSCHUNKS
extern long lmb_add_io(unsigned long base, unsigned long size);
#endif /* CONFIG_MSCHUNKS */
extern long lmb_reserve(unsigned long, unsigned long);
extern unsigned long lmb_alloc(unsigned long, unsigned long);
extern unsigned long lmb_alloc_base(unsigned long, unsigned long, unsigned long);
extern unsigned long lmb_phys_mem_size(void);
extern unsigned long lmb_end_of_DRAM(void);
extern unsigned long lmb_abs_to_phys(unsigned long);
extern void lmb_dump(char *);

static inline unsigned long
lmb_addrs_overlap(unsigned long base1, unsigned long size1,
                  unsigned long base2, unsigned long size2)
{
        return ((base1 < (base2+size2)) && (base2 < (base1+size1)));
}

static inline long
lmb_regions_overlap(struct lmb_region *rgn, unsigned long r1, unsigned long r2)
{
	unsigned long base1 = rgn->region[r1].base;
        unsigned long size1 = rgn->region[r1].size;
	unsigned long base2 = rgn->region[r2].base;
        unsigned long size2 = rgn->region[r2].size;

	return lmb_addrs_overlap(base1,size1,base2,size2);
}

static inline long
lmb_addrs_adjacent(unsigned long base1, unsigned long size1,
		   unsigned long base2, unsigned long size2)
{
	if ( base2 == base1 + size1 ) {
		return 1;
	} else if ( base1 == base2 + size2 ) {
		return -1;
	}
	return 0;
}

static inline long
lmb_regions_adjacent(struct lmb_region *rgn, unsigned long r1, unsigned long r2)
{
	unsigned long base1 = rgn->region[r1].base;
        unsigned long size1 = rgn->region[r1].size;
        unsigned long type1 = rgn->region[r1].type;
	unsigned long base2 = rgn->region[r2].base;
        unsigned long size2 = rgn->region[r2].size;
        unsigned long type2 = rgn->region[r2].type;

	return (type1 == type2) && lmb_addrs_adjacent(base1,size1,base2,size2);
}

#endif /* _PPC64_LMB_H */
