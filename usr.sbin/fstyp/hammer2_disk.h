/*-
 * Copyright (c) 2011-2018 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 * by Venkatesh Srinivas <vsrinivas@dragonflybsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _HAMMER2_DISK_H_
#define _HAMMER2_DISK_H_

#ifndef _SYS_UUID_H_
#include <sys/uuid.h>
#endif
#ifndef _SYS_DMSG_H_
/*
 * dmsg_hdr must be 64 bytes
 */
struct dmsg_hdr {
	uint16_t	magic;		/* 00 sanity, synchro, endian */
	uint16_t	reserved02;	/* 02 */
	uint32_t	salt;		/* 04 random salt helps w/crypto */

	uint64_t	msgid;		/* 08 message transaction id */
	uint64_t	circuit;	/* 10 circuit id or 0	*/
	uint64_t	reserved18;	/* 18 */

	uint32_t	cmd;		/* 20 flags | cmd | hdr_size / ALIGN */
	uint32_t	aux_crc;	/* 24 auxiliary data crc */
	uint32_t	aux_bytes;	/* 28 auxiliary data length (bytes) */
	uint32_t	error;		/* 2C error code or 0 */
	uint64_t	aux_descr;	/* 30 negotiated OOB data descr */
	uint32_t	reserved38;	/* 38 */
	uint32_t	hdr_crc;	/* 3C (aligned) extended header crc */
};

typedef struct dmsg_hdr dmsg_hdr_t;
#endif

/*
 * The structures below represent the on-disk media structures for the HAMMER2
 * filesystem.  Note that all fields for on-disk structures are naturally
 * aligned.  The host endian format is typically used - compatibility is
 * possible if the implementation detects reversed endian and adjusts accesses
 * accordingly.
 *
 * HAMMER2 primarily revolves around the directory topology:  inodes,
 * directory entries, and block tables.  Block device buffer cache buffers
 * are always 64KB.  Logical file buffers are typically 16KB.  All data
 * references utilize 64-bit byte offsets.
 *
 * Free block management is handled independently using blocks reserved by
 * the media topology.
 */

/*
 * The data at the end of a file or directory may be a fragment in order
 * to optimize storage efficiency.  The minimum fragment size is 1KB.
 * Since allocations are in powers of 2 fragments must also be sized in
 * powers of 2 (1024, 2048, ... 65536).
 *
 * For the moment the maximum allocation size is HAMMER2_PBUFSIZE (64K),
 * which is 2^16.  Larger extents may be supported in the future.  Smaller
 * fragments might be supported in the future (down to 64 bytes is possible),
 * but probably will not be.
 *
 * A full indirect block use supports 512 x 128-byte blockrefs in a 64KB
 * buffer.  Indirect blocks down to 1KB are supported to keep small
 * directories small.
 *
 * A maximally sized file (2^64-1 bytes) requires ~6 indirect block levels
 * using 64KB indirect blocks (128 byte refs, 512 or radix 9 per indblk).
 *
 *	16(datablk) + 9 + 9 + 9 + 9 + 9 + 9 = ~70.
 *	16(datablk) + 7 + 9 + 9 + 9 + 9 + 9 = ~68.  (smaller top level indblk)
 *
 * The actual depth depends on copies redundancy and whether the filesystem
 * has chosen to use a smaller indirect block size at the top level or not.
 */
#define HAMMER2_ALLOC_MIN	1024	/* minimum allocation size */
#define HAMMER2_RADIX_MIN	10	/* minimum allocation size 2^N */
#define HAMMER2_ALLOC_MAX	65536	/* maximum allocation size */
#define HAMMER2_RADIX_MAX	16	/* maximum allocation size 2^N */
#define HAMMER2_RADIX_KEY	64	/* number of bits in key */

/*
 * MINALLOCSIZE		- The minimum allocation size.  This can be smaller
 *		  	  or larger than the minimum physical IO size.
 *
 *			  NOTE: Should not be larger than 1K since inodes
 *				are 1K.
 *
 * MINIOSIZE		- The minimum IO size.  This must be less than
 *			  or equal to HAMMER2_LBUFSIZE.
 *
 * HAMMER2_LBUFSIZE	- Nominal buffer size for I/O rollups.
 *
 * HAMMER2_PBUFSIZE	- Topological block size used by files for all
 *			  blocks except the block straddling EOF.
 *
 * HAMMER2_SEGSIZE	- Allocation map segment size, typically 4MB
 *			  (space represented by a level0 bitmap).
 */

#define HAMMER2_SEGSIZE		(1 << HAMMER2_FREEMAP_LEVEL0_RADIX)
#define HAMMER2_SEGRADIX	HAMMER2_FREEMAP_LEVEL0_RADIX

#define HAMMER2_PBUFRADIX	16	/* physical buf (1<<16) bytes */
#define HAMMER2_PBUFSIZE	65536
#define HAMMER2_LBUFRADIX	14	/* logical buf (1<<14) bytes */
#define HAMMER2_LBUFSIZE	16384

/*
 * Generally speaking we want to use 16K and 64K I/Os
 */
#define HAMMER2_MINIORADIX	HAMMER2_LBUFRADIX
#define HAMMER2_MINIOSIZE	HAMMER2_LBUFSIZE

#define HAMMER2_IND_BYTES_MIN	4096
#define HAMMER2_IND_BYTES_NOM	HAMMER2_LBUFSIZE
#define HAMMER2_IND_BYTES_MAX	HAMMER2_PBUFSIZE
#define HAMMER2_IND_RADIX_MIN	12
#define HAMMER2_IND_RADIX_NOM	HAMMER2_LBUFRADIX
#define HAMMER2_IND_RADIX_MAX	HAMMER2_PBUFRADIX
#define HAMMER2_IND_COUNT_MIN	(HAMMER2_IND_BYTES_MIN / \
				 sizeof(hammer2_blockref_t))
#define HAMMER2_IND_COUNT_MAX	(HAMMER2_IND_BYTES_MAX / \
				 sizeof(hammer2_blockref_t))

/*
 * In HAMMER2, arrays of blockrefs are fully set-associative, meaning that
 * any element can occur at any index and holes can be anywhere.  As a
 * future optimization we will be able to flag that such arrays are sorted
 * and thus optimize lookups, but for now we don't.
 *
 * Inodes embed either 512 bytes of direct data or an array of 4 blockrefs,
 * resulting in highly efficient storage for files <= 512 bytes and for files
 * <= 512KB.  Up to 4 directory entries can be referenced from a directory
 * without requiring an indirect block.
 *
 * Indirect blocks are typically either 4KB (64 blockrefs / ~4MB represented),
 * or 64KB (1024 blockrefs / ~64MB represented).
 */
#define HAMMER2_SET_RADIX		2	/* radix 2 = 4 entries */
#define HAMMER2_SET_COUNT		(1 << HAMMER2_SET_RADIX)
#define HAMMER2_EMBEDDED_BYTES		512	/* inode blockset/dd size */
#define HAMMER2_EMBEDDED_RADIX		9

#define HAMMER2_PBUFMASK	(HAMMER2_PBUFSIZE - 1)
#define HAMMER2_LBUFMASK	(HAMMER2_LBUFSIZE - 1)
#define HAMMER2_SEGMASK		(HAMMER2_SEGSIZE - 1)

#define HAMMER2_LBUFMASK64	((hammer2_off_t)HAMMER2_LBUFMASK)
#define HAMMER2_PBUFSIZE64	((hammer2_off_t)HAMMER2_PBUFSIZE)
#define HAMMER2_PBUFMASK64	((hammer2_off_t)HAMMER2_PBUFMASK)
#define HAMMER2_SEGSIZE64	((hammer2_off_t)HAMMER2_SEGSIZE)
#define HAMMER2_SEGMASK64	((hammer2_off_t)HAMMER2_SEGMASK)

#define HAMMER2_UUID_STRING	"5cbb9ad1-862d-11dc-a94d-01301bb8a9f5"

/*
 * A 4MB segment is reserved at the beginning of each 2GB zone.  This segment
 * contains the volume header (or backup volume header), the free block
 * table, and possibly other information in the future.  A 4MB segment for
 * freemap is reserved at the beginning of every 1GB.
 *
 * 4MB = 64 x 64K blocks.  Each 4MB segment is broken down as follows:
 *
 * ==========
 *  0 volume header (for the first four 2GB zones)
 *  1 freemap00 level1 FREEMAP_LEAF (256 x 128B bitmap data per 1GB)
 *  2           level2 FREEMAP_NODE (256 x 128B indirect block per 256GB)
 *  3           level3 FREEMAP_NODE (256 x 128B indirect block per 64TB)
 *  4           level4 FREEMAP_NODE (256 x 128B indirect block per 16PB)
 *  5           level5 FREEMAP_NODE (256 x 128B indirect block per 4EB)
 *  6 freemap01 level1 (rotation)
 *  7           level2
 *  8           level3
 *  9           level4
 * 10           level5
 * 11 freemap02 level1 (rotation)
 * 12           level2
 * 13           level3
 * 14           level4
 * 15           level5
 * 16 freemap03 level1 (rotation)
 * 17           level2
 * 18           level3
 * 19           level4
 * 20           level5
 * 21 freemap04 level1 (rotation)
 * 22           level2
 * 23           level3
 * 24           level4
 * 25           level5
 * 26 freemap05 level1 (rotation)
 * 27           level2
 * 28           level3
 * 29           level4
 * 30           level5
 * 31 freemap06 level1 (rotation)
 * 32           level2
 * 33           level3
 * 34           level4
 * 35           level5
 * 36 freemap07 level1 (rotation)
 * 37           level2
 * 38           level3
 * 39           level4
 * 40           level5
 * 41 unused
 * .. unused
 * 63 unused
 * ==========
 *
 * The first four 2GB zones contain volume headers and volume header backups.
 * After that the volume header block# is reserved for future use.  Similarly,
 * there are many blocks related to various Freemap levels which are not
 * used in every segment and those are also reserved for future use.
 * Note that each FREEMAP_LEAF or FREEMAP_NODE uses 32KB out of 64KB slot.
 *
 *			Freemap (see the FREEMAP document)
 *
 * The freemap utilizes blocks #1-40 in 8 sets of 5 blocks.  Each block in
 * a set represents a level of depth in the freemap topology.  Eight sets
 * exist to prevent live updates from disturbing the state of the freemap
 * were a crash/reboot to occur.  That is, a live update is not committed
 * until the update's flush reaches the volume root.  There are FOUR volume
 * roots representing the last four synchronization points, so the freemap
 * must be consistent no matter which volume root is chosen by the mount
 * code.
 *
 * Each freemap set is 5 x 64K blocks and represents the 1GB, 256GB, 64TB,
 * 16PB and 4EB indirect map.  The volume header itself has a set of 4 freemap
 * blockrefs representing another 2 bits, giving us a total 64 bits of
 * representable address space.
 *
 * The Level 0 64KB block represents 1GB of storage represented by 32KB
 * (256 x struct hammer2_bmap_data).  Each structure represents 4MB of storage
 * and has a 512 bit bitmap, using 2 bits to represent a 16KB chunk of
 * storage.  These 2 bits represent the following states:
 *
 *	00	Free
 *	01	(reserved) (Possibly partially allocated)
 *	10	Possibly free
 *	11	Allocated
 *
 * One important thing to note here is that the freemap resolution is 16KB,
 * but the minimum storage allocation size is 1KB.  The hammer2 vfs keeps
 * track of sub-allocations in memory, which means that on a unmount or reboot
 * the entire 16KB of a partially allocated block will be considered fully
 * allocated.  It is possible for fragmentation to build up over time, but
 * defragmentation is fairly easy to accomplish since all modifications
 * allocate a new block.
 *
 * The Second thing to note is that due to the way snapshots and inode
 * replication works, deleting a file cannot immediately free the related
 * space.  Furthermore, deletions often do not bother to traverse the
 * block subhierarchy being deleted.  And to go even further, whole
 * sub-directory trees can be deleted simply by deleting the directory inode
 * at the top.  So even though we have a symbol to represent a 'possibly free'
 * block (binary 10), only the bulk free scanning code can actually use it.
 * Normal 'rm's or other deletions do not.
 *
 * WARNING!  ZONE_SEG and VOLUME_ALIGN must be a multiple of 1<<LEVEL0_RADIX
 *	     (i.e. a multiple of 4MB).  VOLUME_ALIGN must be >= ZONE_SEG.
 *
 * In Summary:
 *
 * (1) Modifications to freemap blocks 'allocate' a new copy (aka use a block
 *     from the next set).  The new copy is reused until a flush occurs at
 *     which point the next modification will then rotate to the next set.
 */
#define HAMMER2_VOLUME_ALIGN		(8 * 1024 * 1024)
#define HAMMER2_VOLUME_ALIGN64		((hammer2_off_t)HAMMER2_VOLUME_ALIGN)
#define HAMMER2_VOLUME_ALIGNMASK	(HAMMER2_VOLUME_ALIGN - 1)
#define HAMMER2_VOLUME_ALIGNMASK64     ((hammer2_off_t)HAMMER2_VOLUME_ALIGNMASK)

#define HAMMER2_NEWFS_ALIGN		(HAMMER2_VOLUME_ALIGN)
#define HAMMER2_NEWFS_ALIGN64		((hammer2_off_t)HAMMER2_VOLUME_ALIGN)
#define HAMMER2_NEWFS_ALIGNMASK		(HAMMER2_VOLUME_ALIGN - 1)
#define HAMMER2_NEWFS_ALIGNMASK64	((hammer2_off_t)HAMMER2_NEWFS_ALIGNMASK)

#define HAMMER2_ZONE_BYTES64		(2LLU * 1024 * 1024 * 1024)
#define HAMMER2_ZONE_MASK64		(HAMMER2_ZONE_BYTES64 - 1)
#define HAMMER2_ZONE_SEG		(4 * 1024 * 1024)
#define HAMMER2_ZONE_SEG64		((hammer2_off_t)HAMMER2_ZONE_SEG)
#define HAMMER2_ZONE_BLOCKS_SEG		(HAMMER2_ZONE_SEG / HAMMER2_PBUFSIZE)

#define HAMMER2_ZONE_FREEMAP_INC	5	/* 5 deep */

#define HAMMER2_ZONE_VOLHDR		0	/* volume header or backup */
#define HAMMER2_ZONE_FREEMAP_00		1	/* normal freemap rotation */
#define HAMMER2_ZONE_FREEMAP_01		6	/* normal freemap rotation */
#define HAMMER2_ZONE_FREEMAP_02		11	/* normal freemap rotation */
#define HAMMER2_ZONE_FREEMAP_03		16	/* normal freemap rotation */
#define HAMMER2_ZONE_FREEMAP_04		21	/* normal freemap rotation */
#define HAMMER2_ZONE_FREEMAP_05		26	/* normal freemap rotation */
#define HAMMER2_ZONE_FREEMAP_06		31	/* normal freemap rotation */
#define HAMMER2_ZONE_FREEMAP_07		36	/* normal freemap rotation */
#define HAMMER2_ZONE_FREEMAP_END	41	/* (non-inclusive) */

#define HAMMER2_ZONE_UNUSED41		41
#define HAMMER2_ZONE_UNUSED42		42
#define HAMMER2_ZONE_UNUSED43		43
#define HAMMER2_ZONE_UNUSED44		44
#define HAMMER2_ZONE_UNUSED45		45
#define HAMMER2_ZONE_UNUSED46		46
#define HAMMER2_ZONE_UNUSED47		47
#define HAMMER2_ZONE_UNUSED48		48
#define HAMMER2_ZONE_UNUSED49		49
#define HAMMER2_ZONE_UNUSED50		50
#define HAMMER2_ZONE_UNUSED51		51
#define HAMMER2_ZONE_UNUSED52		52
#define HAMMER2_ZONE_UNUSED53		53
#define HAMMER2_ZONE_UNUSED54		54
#define HAMMER2_ZONE_UNUSED55		55
#define HAMMER2_ZONE_UNUSED56		56
#define HAMMER2_ZONE_UNUSED57		57
#define HAMMER2_ZONE_UNUSED58		58
#define HAMMER2_ZONE_UNUSED59		59
#define HAMMER2_ZONE_UNUSED60		60
#define HAMMER2_ZONE_UNUSED61		61
#define HAMMER2_ZONE_UNUSED62		62
#define HAMMER2_ZONE_UNUSED63		63
#define HAMMER2_ZONE_END		64	/* non-inclusive */

#define HAMMER2_NFREEMAPS		8	/* FREEMAP_00 - FREEMAP_07 */

						/* relative to FREEMAP_x */
#define HAMMER2_ZONEFM_LEVEL1		0	/* 1GB leafmap */
#define HAMMER2_ZONEFM_LEVEL2		1	/* 256GB indmap */
#define HAMMER2_ZONEFM_LEVEL3		2	/* 64TB indmap */
#define HAMMER2_ZONEFM_LEVEL4		3	/* 16PB indmap */
#define HAMMER2_ZONEFM_LEVEL5		4	/* 4EB indmap */
/* LEVEL6 is a set of 4 blockrefs in the volume header 16EB */

/*
 * Freemap radix.  Assumes a set-count of 4, 128-byte blockrefs,
 * 32KB indirect block for freemap (LEVELN_PSIZE below).
 *
 * Leaf entry represents 4MB of storage broken down into a 512-bit
 * bitmap, 2-bits per entry.  So course bitmap item represents 16KB.
 */
#if HAMMER2_SET_COUNT != 4
#error "hammer2_disk.h - freemap assumes SET_COUNT is 4"
#endif
#define HAMMER2_FREEMAP_LEVEL6_RADIX	64	/* 16EB (end) */
#define HAMMER2_FREEMAP_LEVEL5_RADIX	62	/* 4EB */
#define HAMMER2_FREEMAP_LEVEL4_RADIX	54	/* 16PB */
#define HAMMER2_FREEMAP_LEVEL3_RADIX	46	/* 64TB */
#define HAMMER2_FREEMAP_LEVEL2_RADIX	38	/* 256GB */
#define HAMMER2_FREEMAP_LEVEL1_RADIX	30	/* 1GB */
#define HAMMER2_FREEMAP_LEVEL0_RADIX	22	/* 4MB (128by in l-1 leaf) */

#define HAMMER2_FREEMAP_LEVELN_PSIZE	32768	/* physical bytes */

#define HAMMER2_FREEMAP_LEVEL5_SIZE	((hammer2_off_t)1 <<		\
					 HAMMER2_FREEMAP_LEVEL5_RADIX)
#define HAMMER2_FREEMAP_LEVEL4_SIZE	((hammer2_off_t)1 <<		\
					 HAMMER2_FREEMAP_LEVEL4_RADIX)
#define HAMMER2_FREEMAP_LEVEL3_SIZE	((hammer2_off_t)1 <<		\
					 HAMMER2_FREEMAP_LEVEL3_RADIX)
#define HAMMER2_FREEMAP_LEVEL2_SIZE	((hammer2_off_t)1 <<		\
					 HAMMER2_FREEMAP_LEVEL2_RADIX)
#define HAMMER2_FREEMAP_LEVEL1_SIZE	((hammer2_off_t)1 <<		\
					 HAMMER2_FREEMAP_LEVEL1_RADIX)
#define HAMMER2_FREEMAP_LEVEL0_SIZE	((hammer2_off_t)1 <<		\
					 HAMMER2_FREEMAP_LEVEL0_RADIX)

#define HAMMER2_FREEMAP_LEVEL5_MASK	(HAMMER2_FREEMAP_LEVEL5_SIZE - 1)
#define HAMMER2_FREEMAP_LEVEL4_MASK	(HAMMER2_FREEMAP_LEVEL4_SIZE - 1)
#define HAMMER2_FREEMAP_LEVEL3_MASK	(HAMMER2_FREEMAP_LEVEL3_SIZE - 1)
#define HAMMER2_FREEMAP_LEVEL2_MASK	(HAMMER2_FREEMAP_LEVEL2_SIZE - 1)
#define HAMMER2_FREEMAP_LEVEL1_MASK	(HAMMER2_FREEMAP_LEVEL1_SIZE - 1)
#define HAMMER2_FREEMAP_LEVEL0_MASK	(HAMMER2_FREEMAP_LEVEL0_SIZE - 1)

#define HAMMER2_FREEMAP_COUNT		(int)(HAMMER2_FREEMAP_LEVELN_PSIZE / \
					 sizeof(hammer2_bmap_data_t))

/*
 * XXX I made a mistake and made the reserved area begin at each LEVEL1 zone,
 *     which is on a 1GB demark.  This will eat a little more space but for
 *     now we retain compatibility and make FMZONEBASE every 1GB
 */
#define H2FMZONEBASE(key)	((key) & ~HAMMER2_FREEMAP_LEVEL1_MASK)
#define H2FMBASE(key, radix)	((key) & ~(((hammer2_off_t)1 << (radix)) - 1))

/*
 * 16KB bitmap granularity (x2 bits per entry).
 */
#define HAMMER2_FREEMAP_BLOCK_RADIX	14
#define HAMMER2_FREEMAP_BLOCK_SIZE	(1 << HAMMER2_FREEMAP_BLOCK_RADIX)
#define HAMMER2_FREEMAP_BLOCK_MASK	(HAMMER2_FREEMAP_BLOCK_SIZE - 1)

/*
 * bitmap[] structure.  2 bits per HAMMER2_FREEMAP_BLOCK_SIZE.
 *
 * 8 x 64-bit elements, 2 bits per block.
 * 32 blocks (radix 5) per element.
 * representing INDEX_SIZE bytes worth of storage per element.
 */

typedef uint64_t			hammer2_bitmap_t;

#define HAMMER2_BMAP_ALLONES		((hammer2_bitmap_t)-1)
#define HAMMER2_BMAP_ELEMENTS		8
#define HAMMER2_BMAP_BITS_PER_ELEMENT	64
#define HAMMER2_BMAP_INDEX_RADIX	5	/* 32 blocks per element */
#define HAMMER2_BMAP_BLOCKS_PER_ELEMENT	(1 << HAMMER2_BMAP_INDEX_RADIX)

#define HAMMER2_BMAP_INDEX_SIZE		(HAMMER2_FREEMAP_BLOCK_SIZE * \
					 HAMMER2_BMAP_BLOCKS_PER_ELEMENT)
#define HAMMER2_BMAP_INDEX_MASK		(HAMMER2_BMAP_INDEX_SIZE - 1)

#define HAMMER2_BMAP_SIZE		(HAMMER2_BMAP_INDEX_SIZE * \
					 HAMMER2_BMAP_ELEMENTS)
#define HAMMER2_BMAP_MASK		(HAMMER2_BMAP_SIZE - 1)

/*
 * Two linear areas can be reserved after the initial 4MB segment in the base
 * zone (the one starting at offset 0).  These areas are NOT managed by the
 * block allocator and do not fall under HAMMER2 crc checking rules based
 * at the volume header (but can be self-CRCd internally, depending).
 */
#define HAMMER2_BOOT_MIN_BYTES		HAMMER2_VOLUME_ALIGN
#define HAMMER2_BOOT_NOM_BYTES		(64*1024*1024)
#define HAMMER2_BOOT_MAX_BYTES		(256*1024*1024)

#define HAMMER2_REDO_MIN_BYTES		HAMMER2_VOLUME_ALIGN
#define HAMMER2_REDO_NOM_BYTES		(256*1024*1024)
#define HAMMER2_REDO_MAX_BYTES		(1024*1024*1024)

/*
 * Most HAMMER2 types are implemented as unsigned 64-bit integers.
 * Transaction ids are monotonic.
 *
 * We utilize 32-bit iSCSI CRCs.
 */
typedef uint64_t hammer2_tid_t;
typedef uint64_t hammer2_off_t;
typedef uint64_t hammer2_key_t;
typedef uint32_t hammer2_crc32_t;

/*
 * Miscellaneous ranges (all are unsigned).
 */
#define HAMMER2_TID_MIN		1ULL
#define HAMMER2_TID_MAX		0xFFFFFFFFFFFFFFFFULL
#define HAMMER2_KEY_MIN		0ULL
#define HAMMER2_KEY_MAX		0xFFFFFFFFFFFFFFFFULL
#define HAMMER2_OFFSET_MIN	0ULL
#define HAMMER2_OFFSET_MAX	0xFFFFFFFFFFFFFFFFULL

/*
 * HAMMER2 data offset special cases and masking.
 *
 * All HAMMER2 data offsets have to be broken down into a 64K buffer base
 * offset (HAMMER2_OFF_MASK_HI) and a 64K buffer index (HAMMER2_OFF_MASK_LO).
 *
 * Indexes into physical buffers are always 64-byte aligned.  The low 6 bits
 * of the data offset field specifies how large the data chunk being pointed
 * to as a power of 2.  The theoretical minimum radix is thus 6 (The space
 * needed in the low bits of the data offset field).  However, the practical
 * minimum allocation chunk size is 1KB (a radix of 10), so HAMMER2 sets
 * HAMMER2_RADIX_MIN to 10.  The maximum radix is currently 16 (64KB), but
 * we fully intend to support larger extents in the future.
 *
 * WARNING! A radix of 0 (such as when data_off is all 0's) is a special
 *	    case which means no data associated with the blockref, and
 *	    not the '1 byte' it would otherwise calculate to.
 */
#define HAMMER2_OFF_BAD		((hammer2_off_t)-1)
#define HAMMER2_OFF_MASK	0xFFFFFFFFFFFFFFC0ULL
#define HAMMER2_OFF_MASK_LO	(HAMMER2_OFF_MASK & HAMMER2_PBUFMASK64)
#define HAMMER2_OFF_MASK_HI	(~HAMMER2_PBUFMASK64)
#define HAMMER2_OFF_MASK_RADIX	0x000000000000003FULL
#define HAMMER2_MAX_COPIES	6

/*
 * HAMMER2 directory support and pre-defined keys
 */
#define HAMMER2_DIRHASH_VISIBLE	0x8000000000000000ULL
#define HAMMER2_DIRHASH_USERMSK	0x7FFFFFFFFFFFFFFFULL
#define HAMMER2_DIRHASH_LOMASK	0x0000000000007FFFULL
#define HAMMER2_DIRHASH_HIMASK	0xFFFFFFFFFFFF0000ULL
#define HAMMER2_DIRHASH_FORCED	0x0000000000008000ULL	/* bit forced on */

#define HAMMER2_SROOT_KEY	0x0000000000000000ULL	/* volume to sroot */
#define HAMMER2_BOOT_KEY	0xd9b36ce135528000ULL	/* sroot to BOOT PFS */

/************************************************************************
 *				DMSG SUPPORT				*
 ************************************************************************
 * LNK_VOLCONF
 *
 * All HAMMER2 directories directly under the super-root on your local
 * media can be mounted separately, even if they share the same physical
 * device.
 *
 * When you do a HAMMER2 mount you are effectively tying into a HAMMER2
 * cluster via local media.  The local media does not have to participate
 * in the cluster, other than to provide the hammer2_volconf[] array and
 * root inode for the mount.
 *
 * This is important: The mount device path you specify serves to bootstrap
 * your entry into the cluster, but your mount will make active connections
 * to ALL copy elements in the hammer2_volconf[] array which match the
 * PFSID of the directory in the super-root that you specified.  The local
 * media path does not have to be mentioned in this array but becomes part
 * of the cluster based on its type and access rights.  ALL ELEMENTS ARE
 * TREATED ACCORDING TO TYPE NO MATTER WHICH ONE YOU MOUNT FROM.
 *
 * The actual cluster may be far larger than the elements you list in the
 * hammer2_volconf[] array.  You list only the elements you wish to
 * directly connect to and you are able to access the rest of the cluster
 * indirectly through those connections.
 *
 * WARNING!  This structure must be exactly 128 bytes long for its config
 *	     array to fit in the volume header.
 */
struct hammer2_volconf {
	uint8_t	copyid;		/* 00	 copyid 0-255 (must match slot) */
	uint8_t inprog;		/* 01	 operation in progress, or 0 */
	uint8_t chain_to;	/* 02	 operation chaining to, or 0 */
	uint8_t chain_from;	/* 03	 operation chaining from, or 0 */
	uint16_t flags;		/* 04-05 flags field */
	uint8_t error;		/* 06	 last operational error */
	uint8_t priority;	/* 07	 priority and round-robin flag */
	uint8_t remote_pfs_type;/* 08	 probed direct remote PFS type */
	uint8_t reserved08[23];	/* 09-1F */
	uuid_t	pfs_clid;	/* 20-2F copy target must match this uuid */
	uint8_t label[16];	/* 30-3F import/export label */
	uint8_t path[64];	/* 40-7F target specification string or key */
} __packed;

typedef struct hammer2_volconf hammer2_volconf_t;

#define DMSG_VOLF_ENABLED	0x0001
#define DMSG_VOLF_INPROG	0x0002
#define DMSG_VOLF_CONN_RR	0x80	/* round-robin at same priority */
#define DMSG_VOLF_CONN_EF	0x40	/* media errors flagged */
#define DMSG_VOLF_CONN_PRI	0x0F	/* select priority 0-15 (15=best) */

struct dmsg_lnk_hammer2_volconf {
	dmsg_hdr_t		head;
	hammer2_volconf_t	copy;	/* copy spec */
	int32_t			index;
	int32_t			unused01;
	uuid_t			mediaid;
	int64_t			reserved02[32];
} __packed;

typedef struct dmsg_lnk_hammer2_volconf dmsg_lnk_hammer2_volconf_t;

#define DMSG_LNK_HAMMER2_VOLCONF DMSG_LNK(DMSG_LNK_CMD_HAMMER2_VOLCONF, \
					  dmsg_lnk_hammer2_volconf)

#define H2_LNK_VOLCONF(msg)	((dmsg_lnk_hammer2_volconf_t *)(msg)->any.buf)

/*
 * HAMMER2 directory entry header (embedded in blockref)  exactly 16 bytes
 */
struct hammer2_dirent_head {
	hammer2_tid_t		inum;		/* inode number */
	uint16_t		namlen;		/* name length */
	uint8_t			type;		/* OBJTYPE_*	*/
	uint8_t			unused0B;
	uint8_t			unused0C[4];
} __packed;

typedef struct hammer2_dirent_head hammer2_dirent_head_t;

/*
 * The media block reference structure.  This forms the core of the HAMMER2
 * media topology recursion.  This 128-byte data structure is embedded in the
 * volume header, in inodes (which are also directory entries), and in
 * indirect blocks.
 *
 * A blockref references a single media item, which typically can be a
 * directory entry (aka inode), indirect block, or data block.
 *
 * The primary feature a blockref represents is the ability to validate
 * the entire tree underneath it via its check code.  Any modification to
 * anything propagates up the blockref tree all the way to the root, replacing
 * the related blocks and compounding the generated check code.
 *
 * The check code can be a simple 32-bit iscsi code, a 64-bit crc, or as
 * complex as a 512 bit cryptographic hash.  I originally used a 64-byte
 * blockref but later expanded it to 128 bytes to be able to support the
 * larger check code as well as to embed statistics for quota operation.
 *
 * Simple check codes are not sufficient for unverified dedup.  Even with
 * a maximally-sized check code unverified dedup should only be used in
 * subdirectory trees where you do not need 100% data integrity.
 *
 * Unverified dedup is deduping based on meta-data only without verifying
 * that the data blocks are actually identical.  Verified dedup guarantees
 * integrity but is a far more I/O-expensive operation.
 *
 * --
 *
 * mirror_tid - per cluster node modified (propagated upward by flush)
 * modify_tid - clc record modified (not propagated).
 * update_tid - clc record updated (propagated upward on verification)
 *
 * CLC - Stands for 'Cluster Level Change', identifiers which are identical
 *	 within the topology across all cluster nodes (when fully
 *	 synchronized).
 *
 * NOTE: The range of keys represented by the blockref is (key) to
 *	 ((key) + (1LL << keybits) - 1).  HAMMER2 usually populates
 *	 blocks bottom-up, inserting a new root when radix expansion
 *	 is required.
 *
 * leaf_count  - Helps manage leaf collapse calculations when indirect
 *		 blocks become mostly empty.  This value caps out at
 *		 HAMMER2_BLOCKREF_LEAF_MAX (65535).
 *
 *		 Used by the chain code to determine when to pull leafs up
 *		 from nearly empty indirect blocks.  For the purposes of this
 *		 calculation, BREF_TYPE_INODE is considered a leaf, along
 *		 with DIRENT and DATA.
 *
 *				    RESERVED FIELDS
 *
 * A number of blockref fields are reserved and should generally be set to
 * 0 for future compatibility.
 *
 *				FUTURE BLOCKREF EXPANSION
 *
 * CONTENT ADDRESSABLE INDEXING (future) - Using a 256 or 512-bit check code.
 */
struct hammer2_blockref {		/* MUST BE EXACTLY 64 BYTES */
	uint8_t		type;		/* type of underlying item */
	uint8_t		methods;	/* check method & compression method */
	uint8_t		copyid;		/* specify which copy this is */
	uint8_t		keybits;	/* #of keybits masked off 0=leaf */
	uint8_t		vradix;		/* virtual data/meta-data size */
	uint8_t		flags;		/* blockref flags */
	uint16_t	leaf_count;	/* leaf aggregation count */
	hammer2_key_t	key;		/* key specification */
	hammer2_tid_t	mirror_tid;	/* media flush topology & freemap */
	hammer2_tid_t	modify_tid;	/* clc modify (not propagated) */
	hammer2_off_t	data_off;	/* low 6 bits is phys size (radix)*/
	hammer2_tid_t	update_tid;	/* clc modify (propagated upward) */
	union {
		char	buf[16];

		/*
		 * Directory entry header (BREF_TYPE_DIRENT)
		 *
		 * NOTE: check.buf contains filename if <= 64 bytes.  Longer
		 *	 filenames are stored in a data reference of size
		 *	 HAMMER2_ALLOC_MIN (at least 256, typically 1024).
		 *
		 * NOTE: inode structure may contain a copy of a recently
		 *	 associated filename, for recovery purposes.
		 *
		 * NOTE: Superroot entries are INODEs, not DIRENTs.  Code
		 *	 allows both cases.
		 */
		hammer2_dirent_head_t dirent;

		/*
		 * Statistics aggregation (BREF_TYPE_INODE, BREF_TYPE_INDIRECT)
		 */
		struct {
			hammer2_key_t	data_count;
			hammer2_key_t	inode_count;
		} stats;
	} embed;
	union {				/* check info */
		char	buf[64];
		struct {
			uint32_t value;
			uint32_t reserved[15];
		} iscsi32;
		struct {
			uint64_t value;
			uint64_t reserved[7];
		} xxhash64;
		struct {
			char data[24];
			char reserved[40];
		} sha192;
		struct {
			char data[32];
			char reserved[32];
		} sha256;
		struct {
			char data[64];
		} sha512;

		/*
		 * Freemap hints are embedded in addition to the icrc32.
		 *
		 * bigmask - Radixes available for allocation (0-31).
		 *	     Heuristical (may be permissive but not
		 *	     restrictive).  Typically only radix values
		 *	     10-16 are used (i.e. (1<<10) through (1<<16)).
		 *
		 * avail   - Total available space remaining, in bytes
		 */
		struct {
			uint32_t icrc32;
			uint32_t bigmask;	/* available radixes */
			uint64_t avail;		/* total available bytes */
			char reserved[48];
		} freemap;
	} check;
} __packed;

typedef struct hammer2_blockref hammer2_blockref_t;

#define HAMMER2_BLOCKREF_BYTES		128	/* blockref struct in bytes */
#define HAMMER2_BLOCKREF_RADIX		7

#define HAMMER2_BLOCKREF_LEAF_MAX	65535

/*
 * On-media and off-media blockref types.
 *
 * types >= 128 are pseudo values that should never be present on-media.
 */
#define HAMMER2_BREF_TYPE_EMPTY		0
#define HAMMER2_BREF_TYPE_INODE		1
#define HAMMER2_BREF_TYPE_INDIRECT	2
#define HAMMER2_BREF_TYPE_DATA		3
#define HAMMER2_BREF_TYPE_DIRENT	4
#define HAMMER2_BREF_TYPE_FREEMAP_NODE	5
#define HAMMER2_BREF_TYPE_FREEMAP_LEAF	6
#define HAMMER2_BREF_TYPE_FREEMAP	254	/* pseudo-type */
#define HAMMER2_BREF_TYPE_VOLUME	255	/* pseudo-type */

#define HAMMER2_BREF_FLAG_PFSROOT	0x01	/* see also related opflag */
#define HAMMER2_BREF_FLAG_ZERO		0x02

/*
 * Encode/decode check mode and compression mode for
 * bref.methods.  The compression level is not encoded in
 * bref.methods.
 */
#define HAMMER2_ENC_CHECK(n)		(((n) & 15) << 4)
#define HAMMER2_DEC_CHECK(n)		(((n) >> 4) & 15)
#define HAMMER2_ENC_COMP(n)		((n) & 15)
#define HAMMER2_DEC_COMP(n)		((n) & 15)

#define HAMMER2_CHECK_NONE		0
#define HAMMER2_CHECK_DISABLED		1
#define HAMMER2_CHECK_ISCSI32		2
#define HAMMER2_CHECK_XXHASH64		3
#define HAMMER2_CHECK_SHA192		4
#define HAMMER2_CHECK_FREEMAP		5

#define HAMMER2_CHECK_DEFAULT		HAMMER2_CHECK_XXHASH64

/* user-specifiable check modes only */
#define HAMMER2_CHECK_STRINGS		{ "none", "disabled", "crc32", \
					  "xxhash64", "sha192" }
#define HAMMER2_CHECK_STRINGS_COUNT	5

/*
 * Encode/decode check or compression algorithm request in
 * ipdata->meta.check_algo and ipdata->meta.comp_algo.
 */
#define HAMMER2_ENC_ALGO(n)		(n)
#define HAMMER2_DEC_ALGO(n)		((n) & 15)
#define HAMMER2_ENC_LEVEL(n)		((n) << 4)
#define HAMMER2_DEC_LEVEL(n)		(((n) >> 4) & 15)

#define HAMMER2_COMP_NONE		0
#define HAMMER2_COMP_AUTOZERO		1
#define HAMMER2_COMP_LZ4		2
#define HAMMER2_COMP_ZLIB		3

#define HAMMER2_COMP_NEWFS_DEFAULT	HAMMER2_COMP_LZ4
#define HAMMER2_COMP_STRINGS		{ "none", "autozero", "lz4", "zlib" }
#define HAMMER2_COMP_STRINGS_COUNT	4

/*
 * Passed to hammer2_chain_create(), causes methods to be inherited from
 * parent.
 */
#define HAMMER2_METH_DEFAULT		-1

/*
 * HAMMER2 block references are collected into sets of 4 blockrefs.  These
 * sets are fully associative, meaning the elements making up a set are
 * not sorted in any way and may contain duplicate entries, holes, or
 * entries which shortcut multiple levels of indirection.  Sets are used
 * in various ways:
 *
 * (1) When redundancy is desired a set may contain several duplicate
 *     entries pointing to different copies of the same data.  Up to 4 copies
 *     are supported.
 *
 * (2) The blockrefs in a set can shortcut multiple levels of indirections
 *     within the bounds imposed by the parent of set.
 *
 * When a set fills up another level of indirection is inserted, moving
 * some or all of the set's contents into indirect blocks placed under the
 * set.  This is a top-down approach in that indirect blocks are not created
 * until the set actually becomes full (that is, the entries in the set can
 * shortcut the indirect blocks when the set is not full).  Depending on how
 * things are filled multiple indirect blocks will eventually be created.
 *
 * Indirect blocks are typically 4KB (64 entres) or 64KB (1024 entries) and
 * are also treated as fully set-associative.
 */
struct hammer2_blockset {
	hammer2_blockref_t	blockref[HAMMER2_SET_COUNT];
};

typedef struct hammer2_blockset hammer2_blockset_t;

/*
 * Catch programmer snafus
 */
#if (1 << HAMMER2_SET_RADIX) != HAMMER2_SET_COUNT
#error "hammer2 direct radix is incorrect"
#endif
#if (1 << HAMMER2_PBUFRADIX) != HAMMER2_PBUFSIZE
#error "HAMMER2_PBUFRADIX and HAMMER2_PBUFSIZE are inconsistent"
#endif
#if (1 << HAMMER2_RADIX_MIN) != HAMMER2_ALLOC_MIN
#error "HAMMER2_RADIX_MIN and HAMMER2_ALLOC_MIN are inconsistent"
#endif

/*
 * hammer2_bmap_data - A freemap entry in the LEVEL1 block.
 *
 * Each 128-byte entry contains the bitmap and meta-data required to manage
 * a LEVEL0 (4MB) block of storage.  The storage is managed in 256 x 16KB
 * chunks.
 *
 * A smaller allocation granularity is supported via a linear iterator and/or
 * must otherwise be tracked in ram.
 *
 * (data structure must be 128 bytes exactly)
 *
 * linear  - A BYTE linear allocation offset used for sub-16KB allocations
 *	     only.  May contain values between 0 and 4MB.  Must be ignored
 *	     if 16KB-aligned (i.e. force bitmap scan), otherwise may be
 *	     used to sub-allocate within the 16KB block (which is already
 *	     marked as allocated in the bitmap).
 *
 *	     Sub-allocations need only be 1KB-aligned and do not have to be
 *	     size-aligned, and 16KB or larger allocations do not update this
 *	     field, resulting in pretty good packing.
 *
 *	     Please note that file data granularity may be limited by
 *	     other issues such as buffer cache direct-mapping and the
 *	     desire to support sector sizes up to 16KB (so H2 only issues
 *	     I/O's in multiples of 16KB anyway).
 *
 * class   - Clustering class.  Cleared to 0 only if the entire leaf becomes
 *	     free.  Used to cluster device buffers so all elements must have
 *	     the same device block size, but may mix logical sizes.
 *
 *	     Typically integrated with the blockref type in the upper 8 bits
 *	     to localize inodes and indrect blocks, improving bulk free scans
 *	     and directory scans.
 *
 * bitmap  - Two bits per 16KB allocation block arranged in arrays of
 *	     64-bit elements, 256x2 bits representing ~4MB worth of media
 *	     storage.  Bit patterns are as follows:
 *
 *	     00	Unallocated
 *	     01 (reserved)
 *	     10 Possibly free
 *           11 Allocated
 */
struct hammer2_bmap_data {
	int32_t linear;		/* 00 linear sub-granular allocation offset */
	uint16_t class;		/* 04-05 clustering class ((type<<8)|radix) */
	uint8_t reserved06;	/* 06 */
	uint8_t reserved07;	/* 07 */
	uint32_t reserved08;	/* 08 */
	uint32_t reserved0C;	/* 0C */
	uint32_t reserved10;	/* 10 */
	uint32_t reserved14;	/* 14 */
	uint32_t reserved18;	/* 18 */
	uint32_t avail;		/* 1C */
	uint32_t reserved20[8];	/* 20-3F 256 bits manages 128K/1KB/2-bits */
				/* 40-7F 512 bits manages 4MB of storage */
	hammer2_bitmap_t bitmapq[HAMMER2_BMAP_ELEMENTS];
} __packed;

typedef struct hammer2_bmap_data hammer2_bmap_data_t;

/*
 * XXX "Inodes ARE directory entries" is no longer the case.  Hardlinks are
 * dirents which refer to the same inode#, which is how filesystems usually
 * implement hardlink.  The following comments need to be updated.
 *
 * In HAMMER2 inodes ARE directory entries, with a special exception for
 * hardlinks.  The inode number is stored in the inode rather than being
 * based on the location of the inode (since the location moves every time
 * the inode or anything underneath the inode is modified).
 *
 * The inode is 1024 bytes, made up of 256 bytes of meta-data, 256 bytes
 * for the filename, and 512 bytes worth of direct file data OR an embedded
 * blockset.  The in-memory hammer2_inode structure contains only the mostly-
 * node-independent meta-data portion (some flags are node-specific and will
 * not be synchronized).  The rest of the inode is node-specific and chain I/O
 * is required to obtain it.
 *
 * Directories represent one inode per blockref.  Inodes are not laid out
 * as a file but instead are represented by the related blockrefs.  The
 * blockrefs, in turn, are indexed by the 64-bit directory hash key.  Remember
 * that blocksets are fully associative, so a certain degree efficiency is
 * achieved just from that.
 *
 * Up to 512 bytes of direct data can be embedded in an inode, and since
 * inodes are essentially directory entries this also means that small data
 * files end up simply being laid out linearly in the directory, resulting
 * in fewer seeks and highly optimal access.
 *
 * The compression mode can be changed at any time in the inode and is
 * recorded on a blockref-by-blockref basis.
 *
 * Hardlinks are supported via the inode map.  Essentially the way a hardlink
 * works is that all individual directory entries representing the same file
 * are special cased and specify the same inode number.  The actual file
 * is placed in the nearest parent directory that is parent to all instances
 * of the hardlink.  If all hardlinks to a file are in the same directory
 * the actual file will also be placed in that directory.  This file uses
 * the inode number as the directory entry key and is invisible to normal
 * directory scans.  Real directory entry keys are differentiated from the
 * inode number key via bit 63.  Access to the hardlink silently looks up
 * the real file and forwards all operations to that file.  Removal of the
 * last hardlink also removes the real file.
 */
#define HAMMER2_INODE_BYTES		1024	/* (asserted by code) */
#define HAMMER2_INODE_MAXNAME		256	/* maximum name in bytes */
#define HAMMER2_INODE_VERSION_ONE	1

#define HAMMER2_INODE_START		1024	/* dynamically allocated */

struct hammer2_inode_meta {
	uint16_t	version;	/* 0000 inode data version */
	uint8_t		reserved02;	/* 0002 */
	uint8_t		pfs_subtype;	/* 0003 pfs sub-type */

	/*
	 * core inode attributes, inode type, misc flags
	 */
	uint32_t	uflags;		/* 0004 chflags */
	uint32_t	rmajor;		/* 0008 available for device nodes */
	uint32_t	rminor;		/* 000C available for device nodes */
	uint64_t	ctime;		/* 0010 inode change time */
	uint64_t	mtime;		/* 0018 modified time */
	uint64_t	atime;		/* 0020 access time (unsupported) */
	uint64_t	btime;		/* 0028 birth time */
	uuid_t		uid;		/* 0030 uid / degenerate unix uid */
	uuid_t		gid;		/* 0040 gid / degenerate unix gid */

	uint8_t		type;		/* 0050 object type */
	uint8_t		op_flags;	/* 0051 operational flags */
	uint16_t	cap_flags;	/* 0052 capability flags */
	uint32_t	mode;		/* 0054 unix modes (typ low 16 bits) */

	/*
	 * inode size, identification, localized recursive configuration
	 * for compression and backup copies.
	 *
	 * NOTE: Nominal parent inode number (iparent) is only applicable
	 *	 for directories but can also help for files during
	 *	 catastrophic recovery.
	 */
	hammer2_tid_t	inum;		/* 0058 inode number */
	hammer2_off_t	size;		/* 0060 size of file */
	uint64_t	nlinks;		/* 0068 hard links (typ only dirs) */
	hammer2_tid_t	iparent;	/* 0070 nominal parent inum */
	hammer2_key_t	name_key;	/* 0078 full filename key */
	uint16_t	name_len;	/* 0080 filename length */
	uint8_t		ncopies;	/* 0082 ncopies to local media */
	uint8_t		comp_algo;	/* 0083 compression request & algo */

	/*
	 * These fields are currently only applicable to PFSROOTs.
	 *
	 * NOTE: We can't use {volume_data->fsid, pfs_clid} to uniquely
	 *	 identify an instance of a PFS in the cluster because
	 *	 a mount may contain more than one copy of the PFS as
	 *	 a separate node.  {pfs_clid, pfs_fsid} must be used for
	 *	 registration in the cluster.
	 */
	uint8_t		target_type;	/* 0084 hardlink target type */
	uint8_t		check_algo;	/* 0085 check code request & algo */
	uint8_t		pfs_nmasters;	/* 0086 (if PFSROOT) if multi-master */
	uint8_t		pfs_type;	/* 0087 (if PFSROOT) node type */
	uint64_t	pfs_inum;	/* 0088 (if PFSROOT) inum allocator */
	uuid_t		pfs_clid;	/* 0090 (if PFSROOT) cluster uuid */
	uuid_t		pfs_fsid;	/* 00A0 (if PFSROOT) unique uuid */

	/*
	 * Quotas and aggregate sub-tree inode and data counters.  Note that
	 * quotas are not replicated downward, they are explicitly set by
	 * the sysop and in-memory structures keep track of inheritance.
	 */
	hammer2_key_t	data_quota;	/* 00B0 subtree quota in bytes */
	hammer2_key_t	unusedB8;	/* 00B8 subtree byte count */
	hammer2_key_t	inode_quota;	/* 00C0 subtree quota inode count */
	hammer2_key_t	unusedC8;	/* 00C8 subtree inode count */

	/*
	 * The last snapshot tid is tested against modify_tid to determine
	 * when a copy must be made of a data block whos check mode has been
	 * disabled (a disabled check mode allows data blocks to be updated
	 * in place instead of copy-on-write).
	 */
	hammer2_tid_t	pfs_lsnap_tid;	/* 00D0 last snapshot tid */
	hammer2_tid_t	reservedD8;	/* 00D8 (avail) */

	/*
	 * Tracks (possibly degenerate) free areas covering all sub-tree
	 * allocations under inode, not counting the inode itself.
	 * 0/0 indicates empty entry.  fully set-associative.
	 *
	 * (not yet implemented)
	 */
	uint64_t	decrypt_check;	/* 00E0 decryption validator */
	hammer2_off_t	reservedE0[3];	/* 00E8/F0/F8 */
} __packed;

typedef struct hammer2_inode_meta hammer2_inode_meta_t;

struct hammer2_inode_data {
	hammer2_inode_meta_t	meta;	/* 0000-00FF */
	unsigned char	filename[HAMMER2_INODE_MAXNAME];
					/* 0100-01FF (256 char, unterminated) */
	union {				/* 0200-03FF (64x8 = 512 bytes) */
		hammer2_blockset_t blockset;
		char data[HAMMER2_EMBEDDED_BYTES];
	} u;
} __packed;

typedef struct hammer2_inode_data hammer2_inode_data_t;

#define HAMMER2_OPFLAG_DIRECTDATA	0x01
#define HAMMER2_OPFLAG_PFSROOT		0x02	/* (see also bref flag) */
#define HAMMER2_OPFLAG_COPYIDS		0x04	/* copyids override parent */

#define HAMMER2_OBJTYPE_UNKNOWN		0
#define HAMMER2_OBJTYPE_DIRECTORY	1
#define HAMMER2_OBJTYPE_REGFILE		2
#define HAMMER2_OBJTYPE_FIFO		4
#define HAMMER2_OBJTYPE_CDEV		5
#define HAMMER2_OBJTYPE_BDEV		6
#define HAMMER2_OBJTYPE_SOFTLINK	7
#define HAMMER2_OBJTYPE_UNUSED08	8
#define HAMMER2_OBJTYPE_SOCKET		9
#define HAMMER2_OBJTYPE_WHITEOUT	10

#define HAMMER2_COPYID_NONE		0
#define HAMMER2_COPYID_LOCAL		((uint8_t)-1)

#define HAMMER2_COPYID_COUNT		256

/*
 * PFS types identify the role of a PFS within a cluster.  The PFS types
 * is stored on media and in LNK_SPAN messages and used in other places.
 *
 * The low 4 bits specify the current active type while the high 4 bits
 * specify the transition target if the PFS is being upgraded or downgraded,
 * If the upper 4 bits are not zero it may effect how a PFS is used during
 * the transition.
 *
 * Generally speaking, downgrading a MASTER to a SLAVE cannot complete until
 * at least all MASTERs have updated their pfs_nmasters field.  And upgrading
 * a SLAVE to a MASTER cannot complete until the new prospective master has
 * been fully synchronized (though theoretically full synchronization is
 * not required if a (new) quorum of other masters are fully synchronized).
 *
 * It generally does not matter which PFS element you actually mount, you
 * are mounting 'the cluster'.  So, for example, a network mount will mount
 * a DUMMY PFS type on a memory filesystem.  However, there are two exceptions.
 * In order to gain the benefits of a SOFT_MASTER or SOFT_SLAVE, those PFSs
 * must be directly mounted.
 */
#define HAMMER2_PFSTYPE_NONE		0x00
#define HAMMER2_PFSTYPE_CACHE		0x01
#define HAMMER2_PFSTYPE_UNUSED02	0x02
#define HAMMER2_PFSTYPE_SLAVE		0x03
#define HAMMER2_PFSTYPE_SOFT_SLAVE	0x04
#define HAMMER2_PFSTYPE_SOFT_MASTER	0x05
#define HAMMER2_PFSTYPE_MASTER		0x06
#define HAMMER2_PFSTYPE_UNUSED07	0x07
#define HAMMER2_PFSTYPE_SUPROOT		0x08
#define HAMMER2_PFSTYPE_DUMMY		0x09
#define HAMMER2_PFSTYPE_MAX		16

#define HAMMER2_PFSTRAN_NONE		0x00	/* no transition in progress */
#define HAMMER2_PFSTRAN_CACHE		0x10
#define HAMMER2_PFSTRAN_UNMUSED20	0x20
#define HAMMER2_PFSTRAN_SLAVE		0x30
#define HAMMER2_PFSTRAN_SOFT_SLAVE	0x40
#define HAMMER2_PFSTRAN_SOFT_MASTER	0x50
#define HAMMER2_PFSTRAN_MASTER		0x60
#define HAMMER2_PFSTRAN_UNUSED70	0x70
#define HAMMER2_PFSTRAN_SUPROOT		0x80
#define HAMMER2_PFSTRAN_DUMMY		0x90

#define HAMMER2_PFS_DEC(n)		((n) & 0x0F)
#define HAMMER2_PFS_DEC_TRANSITION(n)	(((n) >> 4) & 0x0F)
#define HAMMER2_PFS_ENC_TRANSITION(n)	(((n) & 0x0F) << 4)

#define HAMMER2_PFSSUBTYPE_NONE		0
#define HAMMER2_PFSSUBTYPE_SNAPSHOT	1	/* manual/managed snapshot */
#define HAMMER2_PFSSUBTYPE_AUTOSNAP	2	/* automatic snapshot */

/*
 * PFS mode of operation is a bitmask.  This is typically not stored
 * on-media, but defined here because the field may be used in dmsgs.
 */
#define HAMMER2_PFSMODE_QUORUM		0x01
#define HAMMER2_PFSMODE_RW		0x02

/*
 *				Allocation Table
 *
 */


/*
 * Flags (8 bits) - blockref, for freemap only
 *
 * Note that the minimum chunk size is 1KB so we could theoretically have
 * 10 bits here, but we might have some future extension that allows a
 * chunk size down to 256 bytes and if so we will need bits 8 and 9.
 */
#define HAMMER2_AVF_SELMASK		0x03	/* select group */
#define HAMMER2_AVF_ALL_ALLOC		0x04	/* indicate all allocated */
#define HAMMER2_AVF_ALL_FREE		0x08	/* indicate all free */
#define HAMMER2_AVF_RESERVED10		0x10
#define HAMMER2_AVF_RESERVED20		0x20
#define HAMMER2_AVF_RESERVED40		0x40
#define HAMMER2_AVF_RESERVED80		0x80
#define HAMMER2_AVF_AVMASK32		((uint32_t)0xFFFFFF00LU)
#define HAMMER2_AVF_AVMASK64		((uint64_t)0xFFFFFFFFFFFFFF00LLU)

#define HAMMER2_AV_SELECT_A		0x00
#define HAMMER2_AV_SELECT_B		0x01
#define HAMMER2_AV_SELECT_C		0x02
#define HAMMER2_AV_SELECT_D		0x03

/*
 * The volume header eats a 64K block.  There is currently an issue where
 * we want to try to fit all nominal filesystem updates in a 512-byte section
 * but it may be a lost cause due to the need for a blockset.
 *
 * All information is stored in host byte order.  The volume header's magic
 * number may be checked to determine the byte order.  If you wish to mount
 * between machines w/ different endian modes you'll need filesystem code
 * which acts on the media data consistently (either all one way or all the
 * other).  Our code currently does not do that.
 *
 * A read-write mount may have to recover missing allocations by doing an
 * incremental mirror scan looking for modifications made after alloc_tid.
 * If alloc_tid == last_tid then no recovery operation is needed.  Recovery
 * operations are usually very, very fast.
 *
 * Read-only mounts do not need to do any recovery, access to the filesystem
 * topology is always consistent after a crash (is always consistent, period).
 * However, there may be shortcutted blockref updates present from deep in
 * the tree which are stored in the volumeh eader and must be tracked on
 * the fly.
 *
 * NOTE: The copyinfo[] array contains the configuration for both the
 *	 cluster connections and any local media copies.  The volume
 *	 header will be replicated for each local media copy.
 *
 *	 The mount command may specify multiple medias or just one and
 *	 allow HAMMER2 to pick up the others when it checks the copyinfo[]
 *	 array on mount.
 *
 * NOTE: root_blockref points to the super-root directory, not the root
 *	 directory.  The root directory will be a subdirectory under the
 *	 super-root.
 *
 *	 The super-root directory contains all root directories and all
 *	 snapshots (readonly or writable).  It is possible to do a
 *	 null-mount of the super-root using special path constructions
 *	 relative to your mounted root.
 *
 * NOTE: HAMMER2 allows any subdirectory tree to be managed as if it were
 *	 a PFS, including mirroring and storage quota operations, and this is
 *	 preferred over creating discrete PFSs in the super-root.  Instead
 *	 the super-root is most typically used to create writable snapshots,
 *	 alternative roots, and so forth.  The super-root is also used by
 *	 the automatic snapshotting mechanism.
 */
#define HAMMER2_VOLUME_ID_HBO	0x48414d3205172011LLU
#define HAMMER2_VOLUME_ID_ABO	0x11201705324d4148LLU

struct hammer2_volume_data {
	/*
	 * sector #0 - 512 bytes
	 */
	uint64_t	magic;			/* 0000 Signature */
	hammer2_off_t	boot_beg;		/* 0008 Boot area (future) */
	hammer2_off_t	boot_end;		/* 0010 (size = end - beg) */
	hammer2_off_t	aux_beg;		/* 0018 Aux area (future) */
	hammer2_off_t	aux_end;		/* 0020 (size = end - beg) */
	hammer2_off_t	volu_size;		/* 0028 Volume size, bytes */

	uint32_t	version;		/* 0030 */
	uint32_t	flags;			/* 0034 */
	uint8_t		copyid;			/* 0038 copyid of phys vol */
	uint8_t		freemap_version;	/* 0039 freemap algorithm */
	uint8_t		peer_type;		/* 003A HAMMER2_PEER_xxx */
	uint8_t		reserved003B;		/* 003B */
	uint32_t	reserved003C;		/* 003C */

	uuid_t		fsid;			/* 0040 */
	uuid_t		fstype;			/* 0050 */

	/*
	 * allocator_size is precalculated at newfs time and does not include
	 * reserved blocks, boot, or redo areas.
	 *
	 * Initial non-reserved-area allocations do not use the freemap
	 * but instead adjust alloc_iterator.  Dynamic allocations take
	 * over starting at (allocator_beg).  This makes newfs_hammer2's
	 * job a lot easier and can also serve as a testing jig.
	 */
	hammer2_off_t	allocator_size;		/* 0060 Total data space */
	hammer2_off_t   allocator_free;		/* 0068	Free space */
	hammer2_off_t	allocator_beg;		/* 0070 Initial allocations */

	/*
	 * mirror_tid reflects the highest committed change for this
	 * block device regardless of whether it is to the super-root
	 * or to a PFS or whatever.
	 *
	 * freemap_tid reflects the highest committed freemap change for
	 * this block device.
	 */
	hammer2_tid_t	mirror_tid;		/* 0078 committed tid (vol) */
	hammer2_tid_t	reserved0080;		/* 0080 */
	hammer2_tid_t	reserved0088;		/* 0088 */
	hammer2_tid_t	freemap_tid;		/* 0090 committed tid (fmap) */
	hammer2_tid_t	bulkfree_tid;		/* 0098 bulkfree incremental */
	hammer2_tid_t	reserved00A0[5];	/* 00A0-00C7 */

	/*
	 * Copyids are allocated dynamically from the copyexists bitmap.
	 * An id from the active copies set (up to 8, see copyinfo later on)
	 * may still exist after the copy set has been removed from the
	 * volume header and its bit will remain active in the bitmap and
	 * cannot be reused until it is 100% removed from the hierarchy.
	 */
	uint32_t	copyexists[8];		/* 00C8-00E7 copy exists bmap */
	char		reserved0140[248];	/* 00E8-01DF */

	/*
	 * 32 bit CRC array at the end of the first 512 byte sector.
	 *
	 * icrc_sects[7] - First 512-4 bytes of volume header (including all
	 *		   the other icrc's except this one).
	 *
	 * icrc_sects[6] - Sector 1 (512 bytes) of volume header, which is
	 *		   the blockset for the root.
	 *
	 * icrc_sects[5] - Sector 2
	 * icrc_sects[4] - Sector 3
	 * icrc_sects[3] - Sector 4 (the freemap blockset)
	 */
	hammer2_crc32_t	icrc_sects[8];		/* 01E0-01FF */

	/*
	 * sector #1 - 512 bytes
	 *
	 * The entire sector is used by a blockset.
	 */
	hammer2_blockset_t sroot_blockset;	/* 0200-03FF Superroot dir */

	/*
	 * sector #2-7
	 */
	char	sector2[512];			/* 0400-05FF reserved */
	char	sector3[512];			/* 0600-07FF reserved */
	hammer2_blockset_t freemap_blockset;	/* 0800-09FF freemap  */
	char	sector5[512];			/* 0A00-0BFF reserved */
	char	sector6[512];			/* 0C00-0DFF reserved */
	char	sector7[512];			/* 0E00-0FFF reserved */

	/*
	 * sector #8-71	- 32768 bytes
	 *
	 * Contains the configuration for up to 256 copyinfo targets.  These
	 * specify local and remote copies operating as masters or slaves.
	 * copyid's 0 and 255 are reserved (0 indicates an empty slot and 255
	 * indicates the local media).
	 *
	 * Each inode contains a set of up to 8 copyids, either inherited
	 * from its parent or explicitly specified in the inode, which
	 * indexes into this array.
	 */
						/* 1000-8FFF copyinfo config */
	hammer2_volconf_t copyinfo[HAMMER2_COPYID_COUNT];

	/*
	 * Remaining sections are reserved for future use.
	 */
	char		reserved0400[0x6FFC];	/* 9000-FFFB reserved */

	/*
	 * icrc on entire volume header
	 */
	hammer2_crc32_t	icrc_volheader;		/* FFFC-FFFF full volume icrc*/
} __packed;

typedef struct hammer2_volume_data hammer2_volume_data_t;

/*
 * Various parts of the volume header have their own iCRCs.
 *
 * The first 512 bytes has its own iCRC stored at the end of the 512 bytes
 * and not included the icrc calculation.
 *
 * The second 512 bytes also has its own iCRC but it is stored in the first
 * 512 bytes so it covers the entire second 512 bytes.
 *
 * The whole volume block (64KB) has an iCRC covering all but the last 4 bytes,
 * which is where the iCRC for the whole volume is stored.  This is currently
 * a catch-all for anything not individually iCRCd.
 */
#define HAMMER2_VOL_ICRC_SECT0		7
#define HAMMER2_VOL_ICRC_SECT1		6

#define HAMMER2_VOLUME_BYTES		65536

#define HAMMER2_VOLUME_ICRC0_OFF	0
#define HAMMER2_VOLUME_ICRC1_OFF	512
#define HAMMER2_VOLUME_ICRCVH_OFF	0

#define HAMMER2_VOLUME_ICRC0_SIZE	(512 - 4)
#define HAMMER2_VOLUME_ICRC1_SIZE	(512)
#define HAMMER2_VOLUME_ICRCVH_SIZE	(65536 - 4)

#define HAMMER2_VOL_VERSION_MIN		1
#define HAMMER2_VOL_VERSION_DEFAULT	1
#define HAMMER2_VOL_VERSION_WIP 	2

#define HAMMER2_NUM_VOLHDRS		4

union hammer2_media_data {
	hammer2_volume_data_t	voldata;
        hammer2_inode_data_t    ipdata;
	hammer2_blockset_t	blkset;
	hammer2_blockref_t	npdata[HAMMER2_IND_COUNT_MAX];
	hammer2_bmap_data_t	bmdata[HAMMER2_FREEMAP_COUNT];
	char			buf[HAMMER2_PBUFSIZE];
} __packed;

typedef union hammer2_media_data hammer2_media_data_t;

#endif /* !_HAMMER2_DISK_H_ */
