/******************************************************************************
 *
 * Filename: tag_list.c
 *
 * Instantiation of basic routines that create linux-boot tag list.
 *
 * Revision information:
 *
 * 22AUG2004	kb_admin	initial creation
 *
 * BEGIN_KBDD_BLOCK
 * No warranty, expressed or implied, is included with this software.  It is
 * provided "AS IS" and no warranty of any kind including statutory or aspects
 * relating to merchantability or fitness for any purpose is provided.  All
 * intellectual property rights of others is maintained with the respective
 * owners.  This software is not copyrighted and is intended for reference
 * only.
 * END_BLOCK
 *
 * $FreeBSD: src/sys/boot/arm/at91/libat91/tag_list.c,v 1.2 2006/04/21 07:19:22 imp Exp $
 *****************************************************************************/

/******************************* GLOBALS *************************************/

/********************** PRIVATE FUNCTIONS/DATA/DEFINES ***********************/

#define u32 unsigned
#define u16 unsigned short
#define u8  unsigned char

// #include "/usr/src/arm/linux/include/asm/setup.h"
#include <linux/asm/setup.h>
#include "tag_list.h"

#define PAGE_SIZE 	0x1000
#define MEM_SIZE	0x2000000
#define PHYS_OFFSET	0x20000000

/*************************** GLOBAL FUNCTIONS ********************************/

/*
 * .KB_C_FN_DEFINITION_START
 * void InitTagList(char*, void *)
 *  This global function populates a linux-boot style tag list from the
 * string passed in the pointer at the location specified.
 * .KB_C_FN_DEFINITION_END
 */
void InitTagList(char *parms, void *output) {

	char *src, *dst;
	struct tag *tagList = (struct tag*)output;

	tagList->hdr.size  = tag_size(tag_core);
	tagList->hdr.tag   = ATAG_CORE;
	tagList->u.core.flags    = 1;
	tagList->u.core.pagesize = PAGE_SIZE;
	tagList->u.core.rootdev  = 0xff;
	tagList = tag_next(tagList);

	tagList->hdr.size  = tag_size(tag_mem32);
	tagList->hdr.tag   = ATAG_MEM;
	tagList->u.mem.size  = MEM_SIZE;
	tagList->u.mem.start = PHYS_OFFSET;
	tagList = tag_next(tagList);

	tagList->hdr.size  = tag_size(tag_cmdline);
	tagList->hdr.tag   = ATAG_CMDLINE;

	src = parms;
	dst = tagList->u.cmdline.cmdline;
	while (*src) {
		*dst++ = *src++;
	}
	*dst = 0;

	tagList->hdr.size += ((unsigned)(src - parms) + 1) / sizeof(unsigned);
	tagList = tag_next(tagList);

	tagList->hdr.size  = 0;
	tagList->hdr.tag   = ATAG_NONE;
}
