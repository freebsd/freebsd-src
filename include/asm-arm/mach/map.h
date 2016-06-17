/*
 *  linux/include/asm-arm/map.h
 *
 *  Copyright (C) 1999-2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Page table mapping constructs and function prototypes
 */
struct map_desc {
	unsigned long virtual;
	unsigned long physical;
	unsigned long length;
	int domain:4,
	    prot_read:1,
	    prot_write:1,
	    cacheable:1,
	    bufferable:1,
	    last:1;
};

#define LAST_DESC \
  { last: 1 }

struct meminfo;

extern void create_memmap_holes(struct meminfo *);
extern void memtable_init(struct meminfo *);
extern void iotable_init(struct map_desc *);
extern void setup_io_desc(void);
