/*
 * Copyright (c) 1998 Matthew Dillon.  Terms of use and redistribution in all
 * forms are covered by the BSD copyright in the file "/usr/src/COPYRIGHT".
 *
 * Implements bitmap resource lists.
 *
 *	Usage:
 *		blist = blist_create(blocks)
 *		(void)  blist_destroy(blist)
 *		blkno = blist_alloc(blist, count)
 *		(void)  blist_free(blist, blkno, count)
 *		nblks = blist_fill(blist, blkno, count)
 *		(void)  blist_resize(&blist, count, freeextra)
 *		
 *
 *	Notes:
 *		on creation, the entire list is marked reserved.  You should
 *		first blist_free() the sections you want to make available
 *		for allocation before doing general blist_alloc()/free()
 *		ops.
 *
 *		SWAPBLK_NONE is returned on failure.  This module is typically
 *		capable of managing up to (2^31) blocks per blist, though
 *		the memory utilization would be insane if you actually did
 *		that.  Managing something like 512MB worth of 4K blocks 
 *		eats around 32 KBytes of memory. 
 *
 * $FreeBSD$
 */

#ifndef _SYS_BLIST_H_
#define _SYS_BLIST_H_

typedef	u_int32_t	u_daddr_t;	/* unsigned disk address */

/*
 * blmeta and bl_bitmap_t MUST be a power of 2 in size.
 */

typedef struct blmeta {
	union {
	    daddr_t	bmu_avail;	/* space available under us	*/
	    u_daddr_t	bmu_bitmap;	/* bitmap if we are a leaf	*/
	} u;
	daddr_t		bm_bighint;	/* biggest contiguous block hint*/
} blmeta_t;

typedef struct blist {
	daddr_t		bl_blocks;	/* area of coverage		*/
	daddr_t		bl_radix;	/* coverage radix		*/
	daddr_t		bl_skip;	/* starting skip		*/
	daddr_t		bl_free;	/* number of free blocks	*/
	blmeta_t	*bl_root;	/* root of radix tree		*/
	daddr_t		bl_rootblks;	/* daddr_t blks allocated for tree */
} *blist_t;

#define BLIST_META_RADIX	16
#define BLIST_BMAP_RADIX	(sizeof(u_daddr_t)*8)

#define BLIST_MAX_ALLOC		BLIST_BMAP_RADIX

extern blist_t blist_create(daddr_t blocks);
extern void blist_destroy(blist_t blist);
extern daddr_t blist_alloc(blist_t blist, daddr_t count);
extern void blist_free(blist_t blist, daddr_t blkno, daddr_t count);
extern int blist_fill(blist_t bl, daddr_t blkno, daddr_t count);
extern void blist_print(blist_t blist);
extern void blist_resize(blist_t *pblist, daddr_t count, int freenew);

#endif	/* _SYS_BLIST_H_ */

