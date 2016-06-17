/*
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 * 
 * Copyright (C) 2001-2003 Silicon Graphics, Inc. All rights reserved.
 *
 */

#include <asm/pgalloc.h>

/**
 * sn_flush_all_caches - flush a range of address from all caches (incl. L4)
 * @flush_addr: identity mapped region 7 address to start flushing
 * @bytes: number of bytes to flush
 *
 * Flush a range of addresses from all caches including L4. 
 * All addresses fully or partially contained within 
 * @flush_addr to @flush_addr + @bytes are flushed
 * from the all caches.
 */
void
sn_flush_all_caches(long flush_addr, long bytes)
{
	/*
	 * The following double call to flush_icache_range has
	 * the following effect which is required:
	 *
	 * The first flush_icache_range ensures the fc() address
	 * is visible on the FSB.  The NUMA controller however has
	 * not necessarily forwarded the fc() request to all other
	 * NUMA controllers. The second call will stall
	 * at the associated fc() instruction until the first
	 * has been forwarded to all other NUMA controllers.
	 */
	flush_icache_range(flush_addr, flush_addr+bytes);
	flush_icache_range(flush_addr, flush_addr+bytes);
	mb();
}
