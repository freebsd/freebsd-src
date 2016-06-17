
/*
**
**                      P C I  -  C O M M O N  Internal Interface
**
** Module:    This module includes all functions that can be used for all included
**            bridge.
**
** Copyright: This file is subject to the terms and conditions of the GNU General Public
**            License.  See the file "COPYING" in the main directory of this archive
**            for more details.
**
**            Copyright (C) 2001   Roberto Giai Meniet (giai@while1.com)  
**                                 Franco  Ometti      (ometti@while1.com) 
**
** File:      include/asm-sh64/scatterlist.h
**
** Note:      For a good view of this file use TABSTOP=8
**
*/

#ifndef _ASM_SH64_SCATTERLIST_H
#define _ASM_SH64_SCATTERLIST_H

struct scatterlist {
	char *address;		/* Location data is to be transferred to, NULL
    				   for highmem page */
	struct page *page;	/* Location for highmem page, if any */
	unsigned int offset;	/* for highmem, page offset */

	dma_addr_t dma_address;
	unsigned int length;
};

#define ISA_DMA_THRESHOLD (0xffffffff)

#endif /* _ASM_SH64_SCATTERLIST_H */
